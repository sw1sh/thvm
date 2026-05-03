# Memory And Lifetime Model

This document is the current reference for thvm buffer lifetime,
memory diagnostics, and the safety boundary around memory planning.
It supersedes the older LeNet-only notes that treated memory as a CPU
buffer-pool problem.

The practical rule is simple: do not run large autotune or batch-size
sweeps until the small memory invariants here pass and repeated-step
Metal profiles show bounded retained memory.

## Runtime Objects

thvm separates tensor descriptors from backend storage:

- `TenDesc` records shape, dtype, producer kernel, backend id, and
  backend `buf_id`.
- CPU buffers live in `CPU_BUFS`.
- Metal buffers live in `METAL_BUFS`.
- A view-only tensor may share a buffer with another tensor and raise
  that buffer's refcount.
- A materialized kernel output usually gets a new `TenDesc` and a new
  backend buffer unless a view/alias fast path applies.

The heap owns graph structure; the backend tables own bytes.  Correct
memory planning has to respect both: a buffer is reusable only when no
live heap path, tensor descriptor, alias, pending command buffer, or
future replay can read it.

## CPU Accounting

`TTotalBufBytes[]` reports live CPU bytes: the sum of
`CPU_BUFS[i].nbytes` where `refcount > 0`.

`TCpuBufTable[]` returns one row per CPU buffer:

```text
{nbytes, refcount, preserved, freeable, owns_data}
```

CPU also has a per-realize pool boundary:

- `cpu_buf_pool_begin()`
- `cpu_buf_pool_rollback_with_preserve(wm)`
- `mark_gc_preserve(res)`

This rollback is intentionally conservative.  It preserves buffers
reachable from heap roots and result chains, then releases only what is
proven unreachable.  That keeps correctness ahead of memory savings.

`THVM_REUSE_BUFS=1` enables the experimental schedule planner for CPU
only.  It can push proven-dead same-pass outputs onto the CPU freelist,
then drains unused pushes at the end of the pass so reuse cannot leak
into a later WNF/materialize pass.

## Metal Accounting

Metal has a different lifetime problem: command buffers can still be
using temporary storage after the C dispatcher has finished encoding.
The Metal backend therefore distinguishes four byte classes:

- `LiveBytes`: buffers with `refcount > 0`; these are visible to live
  tensor descriptors.
- `RetainedBytes`: all non-nil `MTLBuffer` storage still held by the
  backend, including freelist buffers with `refcount == 0`.
- `DeferredBytes`: bytes queued for decref after the current Metal
  batch boundary.
- `FreelistBytes`: `RetainedBytes - LiveBytes`; storage held for reuse,
  not currently referenced by tensors.

`TMetalBufTable[]` returns one row per Metal buffer:

```text
{nbytes, refcount}
```

`TMetalBufSummary[]` returns:

```text
<|"LiveBytes" -> ...,
  "RetainedBytes" -> ...,
  "DeferredBytes" -> ...,
  "DeferredCount" -> ...,
  "FreelistCount" -> ...,
  "PeakLiveBytes" -> ...,
  "PeakRetainedBytes" -> ...,
  "PeakDeferredBytes" -> ...|>
```

`TMetalMemoryProfile[]` derives a flatter profile from the summary and
table:

```text
<|"LiveBytes", "RetainedBytes", "DeferredBytes",
  "DeferredCount", "FreelistCount",
  "PeakLiveBytes", "PeakRetainedBytes", "PeakDeferredBytes",
  "BufferCount", "LiveBuffers", "RetainedBuffers",
  "FreelistBytes", "LargestLiveBytes",
  "LargestRetainedBytes"|>
```

Use `RetainedBytes` and `PeakRetainedBytes` to diagnose OS memory
pressure.  `LiveBytes` can look healthy while the backend still holds a
large freelist.

Like CPU, Metal now has a per-realize rollback boundary:

- `thvm_metal_buf_pool_begin()`
- `thvm_metal_buf_pool_rollback_with_preserve(wm)`
- `mark_gc_preserve(res)`

The root marker is backend-aware: every reachable `TAG_TEN` preserves
its owning backend buffer.  Metal rollback first flushes outstanding
dispatch, then drops or freelists unpreserved buffers allocated after
the boundary.  This is still conservative: it only reclaims storage
that is no longer rooted by the heap/result chain, and it does not
enable speculative planner reuse for Metal.

## Metal Batch Retention

Metal dispatch batching is enabled by default:

```text
THVM_METAL_BATCH=1
```

Temporary buffers released while a batch is open are queued through
`metal_buf_decref_after_batch`.  The queue is drained by
`metal_dispatch_flush()` when:

- `backend_dispatch_end_all()` closes the outermost batch;
- a host read/write needs synchronized contents;
- an allocation would exceed `THVM_METAL_DEFER_BYTES`;
- the deferred queue reaches its hard cap.

`THVM_METAL_DEFER_BYTES` defaults to `1073741824` bytes.  Set it lower
for debugging:

```bash
THVM_METAL_DEFER_BYTES=134217728 ...
```

Set it to `0` to flush as soon as a deferred decref is queued.  That is
useful for lifetime tests, but it usually hurts throughput.

Metal also caps retained freelist storage:

```text
THVM_METAL_FREELIST_BYTES=1073741824
```

The freelist stores `MTLBuffer` objects after their refcount reaches
zero so exact-size future allocations can reuse them.  When freelist
bytes exceed the cap, the backend drops the largest free buffers first.
Set this to `0` to disable retention of dead Metal buffers while
debugging memory pressure.

Alias-only kernels need special care.  A same-numel reshape can alias
the input buffer and release the preallocated output buffer without
encoding a command buffer at all.  The output buffer was allocated
speculatively and has never been submitted to the GPU, so the Metal
backend drops it directly to the freelist/free path instead of queuing
a deferred decref.  During TJit replay the captured output id can be
stale because the output tensor is already bound to the input buffer;
the backend therefore checks the output tensor's current buffer before
touching refcounts.  This is covered by
`metal-real/alias-reshape-drops-unused-output-immediately`.

## TJit Replay Ownership

TJit capture is also a memory root.  During the first call, every
recorded dispatch buffer is retained and marked preserved so the
post-realize rollback cannot free buffers that a future replay will
read.  At capture end, the replay sequence is walked backward:

- assignment records are always live, because they mutate optimizer
  state or weights;
- a dispatch is live only when its output feeds a later live record or
  the first-call return tensor;
- dead dispatches are marked `ReplaySkip` and their speculative output
  buffers are dropped;
- live buffers remain retained until `TJitDrop` or runtime reset.

`TJitCaptureSummary` reports `ReplayLiveDispatches` and
`ReplaySkipped` so benchmark output separates the captured IR size
from the records actually replayed.  It also reports
`ReplayPackedDispatches`: rootless Metal tile outputs whose replay
buffer id was rewritten to an earlier same-size temporary slot after
capture finalization.

Replay slot packing is deliberately conservative:

- it runs only for rootless TJit captures;
- it only packs Metal tile dispatch outputs, not alias or generic
  Metal JIT routes;
- it skips outputs whose tensor descriptor was retargeted to a
  persistent assignment destination;
- it skips any output read by a later explicit ASSIGN, because ASSIGN
  records still refer to tensor ids rather than captured buffer ids;
- it reuses only exact-size slots whose last captured dispatch read is
  before the candidate producer.

Set `THVM_JIT_REPLAY_PACK=0` to disable this rewrite when bisecting a
memory or correctness issue.

Large multi-consumer `EXPAND` relaxation is a separate schedule
classification rule for Metal tile graphs.  It avoids materializing a
global buffer for an expanded broadcast view when the expanded view is
at least 8x larger than its source and the source subtree has no
reduction.  Set `THVM_INLINE_MULTI_CONSUMER_EXPAND=0` to disable it.

Large pure movement/elementwise producer relaxation is an experimental
fusion probe for Metal tile graphs.  When explicitly enabled, a
multi-consumer producer can be recomputed inside each consumer instead
of materialized when:

- its output has at least `THVM_INLINE_MULTI_CONSUMER_PURE_MIN_NUMEL`
  elements, default `65536`;
- the subtree contains at least one movement op;
- the subtree contains no reductions or side-effecting ops.

Set `THVM_INLINE_MULTI_CONSUMER_PURE=1` to enable this relaxation.
It targets im2col-style PAD/RESHAPE/ADD producers where writing a
large global movement buffer may be worse than duplicating the index
expression inside the consumers.  It is default-off for now: the broad
BS=32 beautiful-mnist probe reduced replay dispatch count only
slightly but produced unsupported fat kernels that fell back to
`metal-jit`/`metal-op` and caused a large memory regression.  The
Metal fan-in cap still runs afterward, so over-wide ADD/MUL trees are
split before codegen hits the direct buffer-argument limit.

After replay slot packing, large multi-consumer `EXPAND` relaxation,
and the conservative Metal fan-in cap, the current BS=32
`beautiful_mnist` training replay retains about `1.63GB` of live Metal
buffers and no longer falls back to `metal-op` for the two 34-input
gradient-accumulation kernels.  That is much better than the earlier
`5.3GB` audited replay set, but it is still too large for broad
autotune sweeps.  Use bounded one-step canaries until the remaining
conv/backward movement producers are fused or recomputed inside their
consumers.  The current bounded canary packs about `1112` dispatch
outputs.

`DUMP_MEMORY_PLAN=1` on `wl/Examples/beautiful-mnist/bench-train.wls`
prints the largest live Metal buffers with producer-kernel metadata.
Add `DUMP_MEMORY_IR=1` to include the producer program, scalar/tile
lowering summaries, and consumer kernel briefs for those top buffers.
For `metal-alias` reshape outputs the dump now prints both
`producer` and `origin`: `producer` is the alias dispatch that owns
the output TenDesc, while `origin` follows the alias chain to the
first real producer of the bytes.  Use `origin_ir` when choosing the
next fusion rule; the alias reshape itself does not allocate the large
buffer.
With replay slot packing and large `EXPAND` relaxation, the current
BS=32 profile is dominated by smaller conv/backward movement
intermediates:

- many `10,240,000` element f32 buffers (`40.96MB`) whose immediate
  producer is a one-op `metal-alias` RESHAPE, but whose `origin`
  producer is a large REDUCE tile kernel.  The common origin shapes
  are `MUL/PERMUTE/RESHAPE/EXPAND/REDUCE` and
  `RESHAPE/MUL/EXPAND/ADD/PERMUTE/REDUCE`; each feeds a 24-way
  PAD/ADD consumer plus a smaller reduce consumer.
- the former hot untuned tile gap was the 24-input movement fan-in:
  `RESHAPE=24, PAD=24, ADD=23`, `out_numel=460800`, with default
  axes `{LOOP, LOOP, LOOP}` and shape `{1, 25, 18432}`.  The proposer
  now skips the unsplittable leading axes and offers `LOCAL`
  candidates on the inner loop axis, so the bounded BS=32 profile no
  longer reports timed fusion gaps.  This makes the shape autotunable;
  it does not by itself reduce dispatch count or live bytes.

That points to three separate memory jobs:

- **Fusion job:** avoid materializing shared im2col-like
  movement/add producers when consumers can inline the scalar index
  expression cheaply enough, or fuse the consumers around the shared
  producer so the huge tensor is never a global buffer.
  Do not solve this by merely turning same-pass movement wrappers into
  input-slot aliases: current Metal lowering treats input slots as
  flat contiguous buffers, and a bounded probe of that approach made
  the first warmup step stall badly.  The correct fix needs scalar
  index fusion or a view-aware input contract, not a silent alias
  substitution.
- **Replay allocator job:** once a TJit capture is finalized, pack
  live temporary outputs into reusable replay slots by captured op
  lifetime.  The first rootless Metal pass reduced the BS=32 memory
  plan from about `3.62GB` peak / `5.26GB` total live bytes to about
  `1.99GB` peak / `3.56GB` total live bytes.  The remaining headroom is
  mostly blocked by true multi-consumer producers rather than simple
  non-overlapping temporaries.
- **Broadcast inlining job:** avoid materializing expanded broadcast
  views when every consumer can carry the index expression.  The first
  targeted Metal rule reduced the BS=32 memory plan further to about
  `627MB` peak / `1.93GB` total live bytes by removing the largest
  `1.31GB` expanded conv-backward buffer.

## Safety Boundary

Current policy:

- CPU speculative schedule reuse is opt-in via `THVM_REUSE_BUFS=1`.
- Metal speculative schedule reuse is disabled.
- Metal still performs normal refcount-driven reuse after real decrefs.
- Large performance sweeps stay blocked until Metal profiles prove
  retained memory is bounded under repeated steps.

Why Metal planner reuse is disabled: a schedule planner can decide an
output is dead in graph order, but Metal also has command-buffer order
and a deferred-decref queue.  Until the planner has a Metal-specific
drain/proof equivalent to CPU's end-of-pass freelist drain, pushing
planner-dead Metal buffers to the freelist risks reusing storage too
early.

## Fusion And Memory

Fusion is the largest real memory win, because an unfused chain
allocates every intermediate output.  The current direction is:

1. Let `realize_classify` pick larger legal boundaries.
2. Let `rangeify` lower movement, elementwise, and reduction chains
   into one scalar graph.
3. Let tile/scalar renderers consume that graph.
4. Let `kernel_opts_propose` and autotune choose axis transforms.

This avoids backend-specific custom kernels as the main solution.
Metal GEMM and conv fast paths may exist as compatibility or parity
bridges, but the target path is lowered primitives plus search.

Boundary fusion policy now runs through a named realize-map rewrite
harness in `src/schedule/realize_rewrite.c`.  `realize_classify`
seeds conservative boundaries, then applies rules such as
`inline-constants`, `inline-large-expand-fanout`,
`inline-pure-fanout-probe`, and `metal-tile-fanin-cap`.  Set
`DUMP_REWRITE=1` or `DUMP_FUSION_REWRITE=1` to print the rule hit
summary for a materialization.  This is the first slice of the
tinygrad-style approach: keep fusion decisions as explicit rewrite
rules with legality/cost guards, rather than as untracked classifier
branches or backend-specific custom kernels.

Metal direct MSL kernels can bind at most 30 input buffers without
argument buffers.  With `THVM_BACKEND=metal THVM_TILE=1`,
`realize_classify` therefore splits over-wide ADD/MUL expression
trees before materialization.  The default cap is:

```text
THVM_METAL_FUSION_MAX_INPUTS=24
```

The default is below the hard 30-buffer Metal binding limit because
real kernels often need extra scalar/tile/runtime buffers after
classification.  Lower it only for focused tests.  Raising it above 30
does not make direct Metal buffer binding legal; it should wait for
argument-buffer support.

Memory diagnostics should be read with fusion diagnostics:

- `TProfileProgramGroups[TProfileDelta[before, after]]` tells which
  repeated program shapes dominate time.
- `TProfileFusionGaps[TProfileDelta[before, after]]` filters those
  groups to hot shapes that are not yet tile-tunable.
- `TKernelScalarUops[kid]` shows whether rangeify fused the boundary.
- `TKernelTileUops[kid]` and `TKernelTilePlan[kid]` show whether the
  tile path can consume it.
- `TMetalMemoryProfile[]` shows whether that fused/tiled path keeps
  retained Metal memory bounded.

A good fusion change should usually reduce one or more of:

- kernel count;
- repeated program-group count;
- live intermediate bytes;
- peak retained Metal bytes;
- per-step dispatch time.

Do not accept a speed win that causes unbounded `PeakRetainedBytes`.

## Diagnostic Workflow

Use small correctness and lifetime tests first:

```bash
make bin/test_metal_real
bin/test_metal_real
make wl
wolframscript -code 'PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"]; r=TestReport[{"wl/THVMLink/Tests/memory_plan_bridge.wlt","wl/THVMLink/Tests/metal_dtypes.wlt","wl/THVMLink/Tests/kernel_profile.wlt"}]; Print[r["TestsSucceededCount"], " passed, ", r["TestsFailedCount"], " failed"]; Exit[If[r["TestsFailedCount"] > 0, 1, 0]]'
```

Then use a small repeated-step probe.  Keep batch size and step count
small until retained memory is proven bounded:

```bash
BS=32 WARMUP_STEPS=1 N_STEPS=2 \
THVM_BACKEND=metal THVM_TILE=1 \
THVM_METAL_DEFER_BYTES=134217728 \
THVM_METAL_FREELIST_BYTES=1073741824 \
SHOW_MEMORY_PROFILE=1 SHOW_PROGRAM_PROFILE=1 SHOW_FUSION_GAPS=1 \
wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls
```

To tune only a known hot kernel without running a broad sweep, pass a
comma-separated kid list:

```bash
POST_AUTOTUNE_KIDS=1 \
BS=32 WARMUP_STEPS=1 N_STEPS=1 \
THVM_BACKEND=metal THVM_TILE=1 \
wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls
```

In the current bounded profile, kid 1 is the former 24-way
movement-fan-in gap.  `TKernelVariants[1]` shows `LOCAL(axis=2,
factor=64)` improving that kernel from about `328us` to `275us`, but
full-loop time remains around `800ms` because the replay still fires
about `2118` dispatch records.

Read the two benchmark memory lines:

```text
metal_memory before_timed: ...
metal_memory after_timed:  ...
```

Expected properties for a healthy small run:

- `DeferredBytes` is `0` after the timed window.
- `DeferredCount` is `0` after the timed window.
- `RetainedBytes` may exceed `LiveBytes`, but should stabilize across
  repeated steps.
- `PeakRetainedBytes` should not grow without bound when the same TJit
  replay runs repeatedly.
- BS=32 `beautiful_mnist` with one warmup and two timed steps should
  finish with live Metal storage in the hundreds of MB, not many GB.

Only after that should larger batch tests be considered.  A single
large run that spikes memory pressure is not a valid tuning result.

## Historical LeNet Baseline

The older LeNet probe measured one forward plus one backward target on
CPU:

```text
TenDescs:      511
Buf bytes:     about 15.6 MiB
KernelEntries: 330 before view/fusion cleanup, about 280 after
```

Those numbers were useful for proving that conv partials and
elementwise chains dominated CPU allocation.  They are not sufficient
for Metal training today because Metal pressure is driven by retained
`MTLBuffer` storage, deferred batch releases, and fused/tiled dispatch
coverage.

The probe script still exists:

```bash
wolframscript -f wl/Examples/lenet-mnist/memory-probe.wls
```

Use it for regression context, not as proof that Metal training memory
is safe.

## Open Work

The remaining memory work is ordered:

1. Add a small repeated-step memory regression that asserts
   `DeferredBytes == 0` after every dispatch boundary and checks that
   `PeakRetainedBytes` stays under a conservative cap.
2. Add Metal-specific planner proof/drain machinery before enabling
   `THVM_REUSE_BUFS` for Metal.
3. Continue rangeify/tile fusion coverage so movement-heavy gradients
   and reductions stop materializing large intermediate chains.
4. Feed `TProfileProgramGroups` plus `TMetalMemoryProfile` into
   autotune triage so search optimizes time without hiding memory
   regressions.
