# Changelog

Human-readable log of meaningful changes. Newest first. Group entries
under `## Unreleased` until we cut a tagged version, then roll into a
dated section.

## Unreleased

### Added: fold scalar valid-mask helpers

Rangeify now centralizes scalar valid-mask emission through helpers
that fold `S_IAND` identities and constant-condition `S_IWHERE`
nodes.  Existing PAD/RESHAPE valid-mask wrappers route through the
helpers, reducing redundant mask nodes before CSE/DCE.

### Added: canonicalize identity movement in UOp graph simplification

`uop_graph_simplify` now drops no-op movement nodes for identity
`RESHAPE`/`EXPAND`, identity `PERMUTE`, zero `PAD`, full-range
`SHRINK`, and zero-axis `FLIP`.  These folds run as the named
`movement-identity` rule and are covered by raw-node tests that bypass
the constructors.

### Added: fold cast and bitcast through UOp graph simplification

`uop_graph_simplify` now includes a `symbolic-cast` rule for proven
identity casts, nested bitcasts, and const bitcasts.  The rule avoids
recreating equivalent non-hash-consed cast nodes, so it can run safely
inside the bottom-up graph rewrite loop.

### Added: gate UOp graph simplification by shape and dtype

`uop_graph_simplify_checked` now accepts graph rewrites only when
shape and dtype inference prove the rewritten term matches the
original.  The materializer has a default-off
`THVM_UOP_GRAPH_SIMPLIFY=1` hook for this checked pass, giving us a
safe place to test symbolic UOp rewrites before they become part of
normal scheduling.

### Added: add UOp view helpers and graph simplification

`uop_view` now gives graph rewrite callbacks a stable op/source-slot
view, and `uop_graph_simplify` runs the first named symbolic UOp rules
over `uop_graph_rewrite`.  The initial graph rules reuse the existing
safe constructor-time unary, binary, and reshape/expand-chain
simplifications so later range/index rewrites have a concrete pass to
extend.

### Added: add a UOp graph rewrite skeleton

`src/uop/graph_rewrite.c` now provides a named bottom-up rewrite pass
over UOp DAGs with memoization, canonical parent rebuilds, replacement
callbacks, and hit counters exposed through `DUMP_UOP_REWRITE=1`.  The
rewrite-fusion plan now records this as the first UOp-level
implementation slice toward tinygrad-style graph rewrites.

### Changed: clarify rewrite-fusion as one goal

`docs/plans/rewrite_fusion.md` now frames the work as a single goal:
build a tinygrad-style UOp rewrite pipeline that lowers high-level
tensor graphs into legal, autotunable scalar/tile kernels for Metal
beautiful-mnist parity.  The tinygrad rule inventory is now a parity
checklist, and the coverage order is framed as implementation phases
under that goal.

### Changed: inventory tinygrad rewrite rule families

The rewrite-fusion plan now tracks the declarative tinygrad
`PatternMatcher` families that matter for fusion parity: symbolic
simplification, valid-mask movement, rangeify, bufferize insertion and
removal, reduce/range simplification, late load/store lowering,
GPU-dim lowering, compile/JIT rewrites, and memory planning.  It also
marks which families THVM already covers and which gaps block
beautiful-mnist parity.

### Changed: name realization-boundary fusion rewrites

`realize_classify` now seeds conservative boundaries and then runs a
named realize-map rewrite table for fusion policy.  Existing
relaxations such as shared constants, adjacent reduce chains,
softmax-style reduce broadcast, large Metal `EXPAND` fanout, the
default-off pure fanout probe, and the Metal tile fan-in cap now report
rule hit counters via `DUMP_REWRITE=1` / `DUMP_FUSION_REWRITE=1`.
This starts moving the scheduler toward tinygrad-style rewrite-driven
fusion instead of hidden classifier branches.

### Changed: inline constants and gate broad Metal fanout recompute

Shared `UOP_CONST` nodes now stay inline instead of becoming tiny
multi-consumer materialization boundaries, and a root constant
materializes directly to a one-element tensor rather than a replay
kernel.  The broader large multi-consumer movement/ALU recompute rule
is present as an explicit probe only: set
`THVM_INLINE_MULTI_CONSUMER_PURE=1` plus optional
`THVM_INLINE_MULTI_CONSUMER_PURE_MIN_NUMEL` to test it.  The first
BS=32 `beautiful_mnist` probe showed the broad rule reduces dispatch
count only slightly while creating unsupported fat kernels, so it
stays default-off.

### Changed: raise Metal graph replay chunk default

Metal TJit graph replay now defaults to `THVM_METAL_GRAPH_MAX_DISPATCHES=256`
instead of 128, matching the fixed replay record array cap.  The env
override remains bounded to `2..256`, and the WL
`TJitCaptureGraphRuns` simulator uses the same default.  On the bounded
BS=32 `beautiful_mnist` canary after large-EXPAND inlining, the chunk
count drops from 18 to 9 and steady replay improves to about `172ms`
with the same `1.69GB` live Metal footprint.

### Changed: inline large multi-consumer Metal expands

Metal tile scheduling now keeps large multi-consumer `EXPAND` views
inside their consumers when the expanded view is at least 8x larger
than its source and the inlined source subtree has no reduction.  The
existing Metal fan-in cap still splits the graph if duplicated inputs
would exceed the backend argument budget; set
`THVM_INLINE_MULTI_CONSUMER_EXPAND=0` to disable the relaxation while
bisecting.  The `beautiful_mnist` benchmark can now print
producer/consumer IR for the largest live memory-plan buffers with
`DUMP_MEMORY_IR=1`.  On the bounded BS=32 canary, this removes the
`1.31GB` expanded conv-backward buffer and leaves live Metal storage
around `1.69GB`, with memory-plan peak/total around `627MB` / `1.93GB`.

### Changed: pack rootless TJit replay temporaries on Metal

Rootless TJit capture finalization now rewrites non-overlapping Metal
tile temporaries onto reusable same-size replay slots after liveness
and ASSIGN sinking.  `TJitCaptureOps` exposes a `ReplayPacked` bit, and
`TJitCaptureSummary` reports `ReplayPackedDispatches` so memory
profiles can distinguish true persistent outputs from packed scratch
slots; set `THVM_JIT_REPLAY_PACK=0` to disable the rewrite while
bisecting.  On the bounded BS=32 `beautiful_mnist` canary, steady
replay is about `179ms`; live Metal storage drops to about `3.32GB`,
and the memory-plan estimate drops to about `1.99GB` peak / `3.56GB`
total.  The remaining largest buffer is still a `1.31GB`
multi-consumer conv-backward movement/add producer, so the next gap is
fusion rather than another custom backend kernel.

### Changed: include conv tile templates in Metal graph replay

Metal TJit graph replay now binds the small conv2d-flat template
configuration buffer through the indirect-command-buffer path, so
normal conv tile kernels no longer split replay chunks just because
they need `cfg`.  The graph cache owns one config buffer per cached
ICB and includes the config values in the cache key.  Generated
`metal-jit` shaders now bake their logical output size into MSL, but
graph replay still treats `metal-jit` as a blocker until command-order
correctness for producer chains is proven.  `DUMP_JIT_CAPTURE=1` now
prints the simulated graph chunks and their blockers.  Dead
`ReplaySkip` records are consumed through graph replay without
splitting ICB chunks, and capture dumps include the concrete blocker
kernel metadata for each top graph chunk.  Metal tile codegen now
supports `GLOBAL`/`LOCAL` bindings with remaining serial `LOOP` or
`UPCAST` axes by decoding `_tgid` across non-local axes.  Rootless
optimizer-style TJit captures can sink safe ASSIGN copies into the
immediately preceding Metal tile producer, and
`THVM_METAL_GRAPH_MAX_DISPATCHES` bounds ICB chunk length for graph
replay autotuning.

### Changed: make Metal graph replay the default TJit path

Metal TJit replay now defaults to indirect-command-buffer graph
replay; set `THVM_METAL_GRAPH_REPLAY=0` to force the old per-kernel
replay path.  HotCounters now include `JitGraphRuns` and
`JitGraphDispatches`, and the `beautiful_mnist` bench prints graph
replay mode plus applied axis opts for hot program shapes.  Bounded
BS=32 `beautiful_mnist` with Metal+tile+autotune now reaches roughly
`190ms` per replay with graph replay enabled, versus about `420ms`
without graph replay and about `760ms` before the new GROUP reduce
proposal coverage.

### Changed: expose GROUP candidates for tile-reduced conv-like graphs

The Metal proposer now offers `GROUP` candidates for tile-reduced
scalar graphs, including conv2d-flat-shaped reductions.  Applying
`GROUP` intentionally bypasses the conv2d-flat template and routes
through the generic scalar/tile group-reduce renderer so autotune can
compare primitive schedules rather than only template-specific
LOCAL/UPCAST/UNROLL variants.  Metal graph replay uses the same gate,
so GROUP-routed conv-like kernels can be packed into ICB replay runs.

### Fixed: make TJit replay own live Metal buffers

TJit capture now retains and preserves backend buffers needed by
future replay, finalizes the capture with a backward liveness pass,
and marks dead dispatch records as `ReplaySkip`.  Replay now counts
only successful backend dispatches, and `TJitCaptureSummary` reports
live versus skipped dispatch records.  The Metal backend also has an
ICB replay path for consecutive `metal-tile` chunks; pipeline states
are built with indirect-command-buffer support and graph capture
rejects dead buffers before encoding.

The bounded BS=32 `beautiful_mnist` audit changed the trustworthy
baseline: non-graph replay is about `751.5ms` with `2067` live
dispatches and `5.26GB` live Metal storage, while opt-in graph replay
is about `711.1ms`.  This supersedes the earlier faster number that
counted backend dispatch failures for freed captured buffers.

### Added: comparable IR dumps for tinygrad and TJit replay

`tools/bench_tinygrad_beautiful_mnist.py` can now print captured
tinygrad graph/leaf-program summaries with `DUMP_IR=1`, including
program shape counts, launch geometry, applied opts, op/dtype
histograms, and optional source/linear heads.  THVM now exposes
`TJitCaptureOps`, `TJitCaptureRuns`, and `TJitCaptureSummary`, and
`wl/Examples/beautiful-mnist/bench-train.wls` prints the captured
dispatch sequence with `DUMP_JIT_CAPTURE=1`.  The new comparison shows
the current BS=32 gap as graph replay granularity: tinygrad replays
three Metal graph submissions over about 110 leaf kernels, while THVM
still walks thousands of captured dispatch records split by optimizer
assignments.

### Added: profile tinygrad beautiful_mnist from THVM

`tools/bench_tinygrad_beautiful_mnist.py` now imports the sibling
tinygrad checkout read-only and profiles the same beautiful_mnist
training loop on a fixed synthetic batch.  The harness separates
no-JIT warmup, TinyJit capture, and synchronized replay, and reports
captured graph calls, leaf calls, kernel counters, memory, and beam
settings.  The benchmark history now treats this harness as the live
tinygrad comparison source: BS=32 with `JITBEAM=1` replays in about
`14ms`, while BS=512 without JIT beam replays in about `97ms` on the
current machine.

### Changed: fuse contiguous private reduce chains

The scheduler can now collapse same-kind private `UOP_REDUCE` chains
over a contiguous axis span into one KProg `REDUCE` while preserving
SUM traversal order.  Materialize re-packs the chain through the
existing `(kind << 24) | inner` encoding, and rangeify can carry a
flattened coordinate context backward through reshapes.  Focused
reduce tests now assert trailing SUM/MAX chains materialize as one
kernel.  The classifier currently fuses only direct/boundary reduce
sources so movement-heavy backward programs stay split until tile
lowering can represent their flattened coordinates.  On the bounded
BS=32 `beautiful_mnist` Metal+tile training canary this cuts replay
dispatches from about `4137` to `2201` and JIT ops from about `4179`
to `2243`, with no `metal-jit` routes; wall time is still not improved
yet.

### Fixed: avoid Metal alias-reshape refcount growth on replay

Metal alias-only reshape dispatch now looks at the output tensor's
current buffer before releasing the speculative output buffer.  This
prevents TJit replay from repeatedly increfing the already-aliased
input buffer or freelisting a stale captured output id.  On the
bounded BS=32 `beautiful_mnist` Metal+tile probe, timed-window live
Metal storage stays near `20MB` after replay instead of growing to
about `249MB`, while steady wall time remains about `389ms`.  The
dispatch profiler now reports these metadata-only routes as
`metal-alias` instead of `metal-op`.

### Changed: expose TJit replay counters in benchmark profiles

Hot counters now include TJit replay calls, replayed dispatches, and
captured tensor assignments.  The `beautiful_mnist` training benchmark
can print these counters with `SHOW_HOT_COUNTERS=1`, making the
remaining small-batch gap visible as host replay/dispatch granularity
after the hot conv-backward path has moved to generated Metal tile
kernels.

### Changed: rangeify PAD-adjusted reshape chains into tile graphs

Rangeify now lets rank-mismatched `S_RESHAPE_V` use the edge-local
coordinate context recorded by the backward walk, including explicit
output extents for PAD-adjusted axes via `S_IMOD(expr, extent)`.
Scalar DCE now prunes stale movement wrappers before tile/proposal
analysis.  On the bounded BS=32 `beautiful_mnist` Metal+tile probe,
the hot `[32,1,12800] -> [32,32,20,20]` conv-backward reshape group
moves from `not-rangeified`/`metal-jit` to generated `metal-tile`,
and the timed window reports no `metal-jit` dispatches.

### Fixed: rollback transient Metal buffers after each realize

Metal buffers allocated after a realize boundary are now rolled back
with the same root-preservation discipline as CPU buffers.  GC root
marking preserves tensor storage through the owning backend, and the
Metal backend can drop or freelist unrooted post-boundary buffers after
flushing outstanding command buffers.  A bounded BS=32
`beautiful_mnist` Metal+tile probe improved from about `2.6s` per step
with `~9.8GB` live Metal storage to about `417ms` per step with
`~249MB` live storage after the timed window; larger sweeps remain
blocked until the remaining fusion gaps are closed and memory profiles
stay bounded.

### Changed: make Metal memory pressure observable before sweeping

`TMetalBufSummary[]` now reports Metal live bytes, retained bytes,
deferred-decref bytes/count, freelist count, and peak live/retained/
deferred bytes so benchmark runs can distinguish live tensors from
buffers retained for reuse.  `TMetalMemoryProfile[]` adds derived
buffer counts and freelist bytes, and `bench-train.wls` reports
before/after timed-window Metal memory snapshots.  The opt-in
`THVM_REUSE_BUFS=1` schedule planner is CPU-only for now; Metal keeps
ordinary refcount-driven reuse, but speculative planner reuse is
blocked until command-buffer and deferred-decref lifetimes have tighter
proof coverage.  Added small Metal invariants for freelist accounting
and alias-only batch flushes instead of running large sweeps.

`TProfileFusionGaps[]` now annotates hot `TProfileProgramGroups[]`
rows with rangeify/tile/proposer status so fusion work can be triaged
against the same timed window as the Metal memory profile.

Metal freelist retention is capped by `THVM_METAL_FREELIST_BYTES`
(default `1073741824`); the backend drops the largest dead buffers
first when the cap is exceeded.  The Metal tile proposer now offers
`LOCAL` candidates for multi-axis tile-lowered elementwise kernels
instead of only rank-1 outputs, removing a common small-batch
`tile-no-proposals` gap.

Metal+tile materialization now splits over-wide ADD/MUL expression
trees before they exceed the 30-input direct Metal buffer limit
(`THVM_METAL_FUSION_MAX_INPUTS`, default 30), so wide elementwise
graphs can stay on generated tile kernels instead of falling to the
per-op Metal interpreter.  Alias-only reshape dispatch also drops its
unused speculative output buffer immediately instead of sending it
through the deferred-decref queue.

### Changed: bounded Metal batch retention and grouped profile deltas

Metal deferred temporary-buffer releases are now byte-capped by
`THVM_METAL_DEFER_BYTES` (default `1073741824`) and are drained even
when an alias-only batch did not create a command buffer.  This avoids
holding an entire `beautiful_mnist` training replay's dead temporaries
inside one Metal batch.  `TProfileDelta` and `TProfileProgramGroups`
promote the benchmark-local timed-window view into the public WL
profile API, and `bench-train.wls` reports top repeated program shapes,
optional lowered scalar/tile plans, and the active defer cap.

### Changed: broaden rangeify coverage for conv-backward reductions

Rangeify now lowers movement-heavy reduce chains through the
backward-walk coordinate context for `PERMUTE`, falls back to
`S_INDEX_E` when packed `S_INDEX` strides overflow, and allows the
tile opt proposer to see scalar/tile-reduced graphs that originated
from PAD/SHRINK KProg chains.  On BS=32 `beautiful_mnist` no-batch
training this moves the remaining hot conv-backward reductions from
`metal-jit` to `metal-tile`; with a 128MB defer cap the full replay
smoke is about `2.5s` and leaves `total_buf_bytes=0`.  BS=512 is not
rerun until the memory-pressure fix is validated at larger batch sizes.

### Changed: Metal expression JIT for backward movement graphs

Generated Metal source can now inline f32 movement/ALU expression
graphs, including `PAD`/`SHRINK`/`FLIP` and direct view indexing,
instead of forcing large backward materializers through the per-op
Metal fallback.  The dispatcher preserves generated tile kernels as
the first choice, uses expression JIT for heavy non-reduce movement
graphs, and avoids direct MSL JIT for more than 30 input buffers
(Metal's direct buffer-index limit).  On the BS=512
`beautiful_mnist` benchmark this cuts `grad-3` from roughly `10.6s`
to `2.6s`, `grad-7` to about `127ms`, and full Adam replay to about
`41-46s`; the remaining tinygrad gap is repeated early-conv backward
lowering, not first-sample overhead.

### Fixed: materialized beautiful_mnist gradients replay through kernels

`TAdam` now materializes the loss before `TGradMany`, and the C
gradient path can backprop through materialized `KernelEntry` programs
for elementwise, movement, and reduction ops.  Per-realize gradient and
kernel-fire memo scopes now survive the whole training step while
assignment invalidates fired-kernel memo entries after buffer mutation.
This turns the early beautiful_mnist W1 gradient from a non-finishing
capture into a completed Metal replay path and cuts redundant TJit
kernel replays in the full train loop.

### Changed: broaden Metal tile rangeify coverage

Rangeify now lowers `PERMUTE` through edge-local coordinate contexts
instead of treating axis swaps as flat identities, and Metal tile
codegen can consume `S_RESHAPE_V` scalar wrappers while rejecting
unsubstituted virtual ranges cleanly.  The Metal backend now skips
host-side pre-materialization for generated tile kernels that can
consume view indexing directly.

### Bench: beautiful_mnist forward reaches tinygrad-class replay

With `THVM_BACKEND=metal THVM_TILE=1 THVM_AUTOTUNE=1` and first
capture/autotune outside the measured window, BS=512
`BENCH_MODE=forward` replays at `29.7ms` mean over eight steady
samples with no `metal-op` fallbacks.  Full training parity is still
blocked by early conv-weight gradients: `BENCH_MODE=grad-1 BS=1`
does not finish first capture in a useful time window, while
`grad-7` replays at `105.6ms` with 127 generic `metal-op` fallbacks.

### Added: beautiful_mnist full-loop benchmark path

`wl/Examples/beautiful-mnist/bench-train.wls` now builds the full
tinygrad-style beautiful_mnist architecture with rank-4 batched inputs
and separates warmup from steady TJit replay timing.  The benchmark can
run forward-only, a single target gradient, or the full Adam path so the
training gap is measured at the loop that matters instead of the old
single-sample forward canary.

### Changed: rank-4 NN lowering for beautiful_mnist

`TConv2D`, `TMaxPool2d`, and batch norm lowering now handle rank-4
channels-first tensors.  Batched Conv2D lowers to one im2col-shaped
matmul over `B * Hout * Wout`, rank-4 maxpool preserves the batch axis,
and `TBatchNormTrain` computes per-channel statistics over batch and
spatial axes.

### Fixed: target-pruned gradient walks

Target-aware `TGrad` now skips child subgraphs that cannot reach the
current target tensor.  This avoids walking the whole forward graph for
final-layer gradients and turns the beautiful_mnist final-linear
gradient from a >30s first capture into a sub-second capture with
steady TJit replay.

### Fixed: TJit capture for high-input gradient kernels

TJit capture records now spill large input lists to per-op heap storage
instead of dropping kernels whose input count exceeds the inline record
capacity.  This covers movement-heavy gradient kernels with more than
64 input buffers.

### Added: rank-4 Metal tile Conv2D templates

The generated Metal tile Conv2D recognizer now accepts rank-4
`{B,C,H,W}` inputs and the single-channel im2col patch-input form used
by the first beautiful_mnist convolution.  Batched forward replay no
longer falls back to generic `metal-op` for those convolution kernels.

### Added: Conv2D tile reduce-unroll candidates

The Metal Conv2D tile template now records a `KOP_UNROLL` factor from
the reduction axis and emits a flattened constant-trip reduction loop
with an optional MSL unroll pragma.  The Conv2D proposer includes
reduce-axis `UNROLL` candidates alongside `LOCAL` and `UPCAST`, giving
autotune another lowered schedule knob without adding a backend-private
kernel recognizer.

### Removed: diagnostic Metal GEMV shortcut

The old opt-in direct Metal GEMV shader has been removed from dispatch.
Rank-1 `TMatVec` already lowers through the shared `TILE_MMA` analysis
and routes via `metal-gemm`, so even `THVM_METAL_SPECIALIZED=1` no
longer bypasses the generic tile/MMA path for matrix-vector kernels.

### Changed: tile JIT hashes ignore mutation counters

CPU and Metal tile JIT cache keys no longer include
`tile_axes_version`, which is only an internal invalidation counter.
The keys still include the generated scalar/tile graph, dtypes, shapes,
and applied opts, so equivalent autotune variants can reuse compiled
tile kernels across reset/apply cycles instead of recompiling because
the version counter changed.

### Added: autotune opt-sequence search

`TKernelAutotune` can now expand the best single candidates into short
opt sequences, controlled by `THVM_AUTOTUNE_DEPTH` and
`THVM_AUTOTUNE_BEAM`.  Cached winners now store the full sequence, so
Metal tile Conv2D can discover combined schedules like
`UPCAST + LOCAL/GLOBAL` instead of choosing only one knob at a time.

### Fixed: Metal tile PAD-heavy Conv2D fallback

Metal tile codegen now treats scalar integer expressions as signed and
declines generic Metal tile dispatch for KProg graphs that still
contain PAD movement.  This keeps single-channel im2col Conv2D on the
correct Metal fallback path until the generic tile renderer can prove
the PAD-heavy scalar expression directly.

### Added: Conv2D tile output-per-thread candidates

Generated Metal Conv2D tile kernels can now compute multiple output
elements per thread via a `KOP_UPCAST`-backed `outputs_per_thread`
schedule knob.  The Conv2D proposer emits `UPCAST` candidates alongside
`LOCAL` threadgroup-size candidates, giving autotune a second schedule
dimension without adding another backend-private recognizer.

### Changed: consolidated bench phase notes

The old `docs/bench/phase6.md` through `phase16.md` notes and the
beautiful-mnist trend file are now folded into one cleaned
`docs/bench/history.md` page.  Stale source comments and plan links now
point at the consolidated benchmark history instead of deleted phase
files.

### Changed: Conv2D tile autotune scans every loop axis

The Metal tile Conv2D proposer now emits `LOCAL` threadgroup-size
candidates from any loop axis that can legally split, instead of only
using the first output axis.  This exposes larger valid threadgroup
choices for im2col-shaped convolution kernels while keeping the
existing `TOpt["LOCAL", axis, factor]` semantics.

### Added: generic Metal tile Conv2D template

The tile analyzer now recognizes the im2col-fused Conv2D reduce shape
as a reusable `TileConv2DInfo` template.  `THVM_TILE=1` lowers that
template through the generated Metal tile path, binds the original
strided inputs directly instead of pre-materializing them, and exposes
`LOCAL` threadgroup-size candidates for autotune.  The old
`THVM_METAL_SPECIALIZED=1` conv shader remains a diagnostic oracle, but
the default Metal tile path now reaches the same steady-state
beautiful-mnist forward class without a backend-private recognizer.

### Added: scalar reduce axes feed Metal GROUP autotune

Rangeified scalar kernels now append the scalar `S_REDUCE_*` range to
`KernelAxes` when the legacy KProg program is not a tail-REDUCE.  The
Metal proposer can now see those non-tail scalar reductions and emit
`GROUP` candidates for the existing generated `GROUP_REDUCE` tile
renderer.  The Metal tile renderer also substitutes the generated
serial or threadgroup reduction accumulator into post-reduce scalar
expressions after a `GROUP` opt is applied, while the no-opt baseline
keeps the old Metal route for autotune comparison.

### Added: scalar-UOp structural CSE before tile planning

Rangeify now runs a structural dedup pass over safe scalar expression
nodes before schedule-key sharing and TileUop planning.  The pass keeps
`S_RANGE`, `S_STORE`, and `S_BUFFERIZE` identity intact, remaps sources,
and deduplicates repeated constants, index expressions, loads, casts,
and ALU nodes.  On the `beautiful-mnist` forward canary, the two large
conv-like generated Metal tile graphs shrink from `1615 -> 590` and
`1415 -> 563` scalar nodes; steady runtime is still dominated by the
remaining generated conv/reduce schedule quality.

### Added: persistent autotune cache

`TKernelAutotune` now stores the winning `TOpt` decision on disk under
`$XDG_CACHE_HOME/thvm/autotune` (or `$HOME/.cache/thvm/autotune`, or
`THVM_AUTOTUNE_CACHE_DIR`) using a key derived from backend, structural
program shape, tensor shapes/dtypes, candidate list, and autotune run
count.  Later runtime sessions replay the cached winner without
rerunning benchmark dispatches.  Set `THVM_AUTOTUNE_CACHE=0` or
`THVM_AUTOTUNE_DISABLE_CACHE=1` to force fresh benchmarking.

### Added: generated Metal TileUop reductions

`THVM_TILE=1` can now dispatch f32 scalar `TILE_REDUCE` plans through
generated Metal source.  The renderer maps default loop axes to a flat
Metal grid, emits a serial per-output reduction from the scalar body,
emits threadgroup reductions for `GROUP` / `GROUP_REDUCE` opts, and
preserves reduce-axis `UNROLL` candidates so movement-heavy lowered
reductions can start participating in autotune without custom backend
recognizers.

### Added: rank-1 TMatVec promotes to generic TILE_MMA

The tile analyzer now recognizes `EXPAND(vector) -> MUL(matrix, vector)
-> REDUCE_SUM` as a GEMV-shaped `TILE_MMA` plan with `N=1`.  Metal
therefore dispatches rank-1 `TMatVec` through the generic tile-driven
GEMM path and exposes the usual `TC` autotune candidates.

### Changed: specialized Metal paths are diagnostic only

The direct Metal Conv2D recognizer is gated behind
`THVM_METAL_SPECIALIZED=1` and disabled by default.  It remains as a
correctness/performance oracle, but the default path stays on the
lowered scalar/tile graph so future speedups must come from tile-plan
recognition, beam search, and autotuning.

### Added: direct Metal conv2d dispatch

Metal now recognizes the im2col-fused scalar graph produced by
`TConv2D` over an already-produced activation and dispatches a direct
f32 convolution shader instead of the generic scalar `metal-op`
renderer.  This targets the second `beautiful-mnist` convolution,
which was the dominant steady forward cost.

### Fixed: autotune first-capture producer replay

Fire-time autotune now runs after producer kernels have populated the
current kernel's inputs, and variant benchmarks dispatch only the
current kernel directly.  Benchmark fires no longer perturb the real
fire-generation memo or cause the first `TJit` capture to skip
upstream producer kernels, so `beautiful-mnist` replays fresh outputs
after inputs change.  Added a Metal WL regression for the stale
producer-capture case under `THVM_AUTOTUNE=1`.

### Fixed: high-input TJit capture

`TJit` now records fused kernels with up to 64 input buffers instead
of dropping dispatches above the old 16-input inline cap.  This keeps
full forward-pass captures, including `beautiful-mnist`, from falling
back to partial replay when a fused scalar graph has many tensor
inputs.  Added a WL regression that captures and replays a 20-input
fused elementwise kernel.

### Changed: beautiful-mnist forward canary uses replay

`wl/Examples/beautiful-mnist/forward.wls` now builds weights and the
input slot once, captures the full forward pass with `TJit`, and
replays it for each sample.  The script reports per-sample wall time,
captured op count, and output delta, making it a useful architecture
canary instead of measuring runtime reinitialization and weight
reconstruction.

### Fixed: autotune benchmark capture pollution

Fire-time autotune now pauses `TJit` capture while it benchmarks
candidate variants.  The user's actual kernel dispatch is still
captured after tuning, but the internal warmup/timed benchmark fires
no longer inflate replay sequences.  This keeps `THVM_AUTOTUNE=1`
from turning a compact training replay into hundreds or thousands of
recorded benchmark dispatches.

### Changed: configurable autotune sample count

`TKernelVariants` and `TKernelAutotune` now respect
`THVM_AUTOTUNE_RUNS` for the number of timed dispatches per candidate,
defaulting to the previous five-run min.  The focused Metal GEMM
autotune example defaults that knob to nine runs so TC tile-size
comparisons are less sensitive to GPU dispatch jitter.

### Added: focused Metal GEMM autotune smoke

Added `wl/Examples/metal-gemm-autotune.wls`, a narrow benchmark
harness for the new Metal `TILE_MMA` path.  It builds one f32 GEMM,
checks that dispatch routes through `metal-gemm`, prints baseline and
`TC` tile-size variant timings, applies `TKernelAutotune`, reruns the
matmul, and verifies both runs against a host reference.

### Changed: generic GEMM plan analysis

Moved the f32 Metal GEMM recognizer out of the Metal backend and into
the tile/scheduling layer as `tile_analyze_gemm`.  CPU BLAS GEMM and
the tile planner now consume the same `TileGemmInfo` analysis for
`MUL + REDUCE_SUM` matmul-shaped kernels, including view-stride
detection for transposed inputs and ambiguous square matmul cases
where buffer sizes alone cannot identify A vs B.  The same analysis
seeds an introspectable `TILE_MMA` root with M/N/K axes and layout
metadata; the direct Metal GEMM shader now dispatches from that
validated `TILE_MMA` plan and uses a fixed 16x16 threadgroup-memory
tiled shader for f32 matmul while CPU still uses the existing
BLAS/scalar fallbacks until generated MMA renderers are added.
The existing `TC` opt is now kernel-aware metadata for f32 GEMM/MMA:
the Metal proposer offers 32/16/8 tile-size candidates, `TKernelApplyOpt`
records the selected tile size on recognized GEMM kernels, and
`metal-gemm` compiles/caches one tiled shader per tile size.
Fixed WL context bookkeeping so `TContextNew["metal"]` contexts are
treated as initialized instead of being reinitialized through the
ambient `THVM_BACKEND` on first tensor creation.  Kernel GC now skips
non-CPU output kernels because its liveness signal is CPU-buffer
refcounts, preserving Metal `TILE_MMA` plans for post-realize
inspection and autotune.

### Added: Metal dispatch batching and direct GEMM

Added backend dispatch scopes so `TRealize` and `TJit` replay can batch
Metal kernel encodes into one command buffer, with
`THVM_METAL_BATCH=0` as an opt-out for A/B measurement.  Metal
host reads/writes and autotune timing now flush pending command
buffers explicitly.  Added backend-native `buf_copy`, implemented as
a Metal blit on the Metal backend, so `ASSIGN`/`TSet` no longer has
to round-trip through host memory when a backend can copy directly.
Metal also recognizes f32 `MUL + REDUCE_SUM` matmul kernels and
routes them through a direct `metal-gemm` shader over the original
unexpanded input buffers.

### Fixed: Metal training buffer reuse and tile autotune gates

Metal buffers whose refcount drops to zero now recycle through the
freelist, and the allocator can reuse empty table slots before growing
`METAL_BUFS_NEXT`.  Four-step LeNet/Adam training with
`THVM_BACKEND=metal THVM_TILE=1` now completes without
`metal_buf_alloc -- buffer table full` warnings.  Metal autotune also
has a real `LOCAL` proposal path for supported rank-1 f32 tile kernels:
the autotune loop expands `LOCAL` candidates to `LOCAL + GLOBAL`, and
rejects tile variants with more than 30 MSL buffer bindings before
compilation.

### Changed: refreshed LeNet autotune bench plan

Revised the Phase 16 benchmark plan around the current state:
CPU LeNet/Adam training and the old Conv2D/ReLU/Pool gradient repro
now pass, while Metal 4-step training still needs buffer-table
cleanup before performance numbers are trustworthy.  `TKernelProgramKey`
now also covers shared rangeified/axes-only schedule slots, and Metal
constant rendering emits raw-bit `as_type<float>` literals instead of
invalid forms such as `-1f`.

### Added: program-shape autotune de-duplication

Added `TKernelProgramKey` and `TKernelAutotuneUnique` so training
benchmarks can tune one representative per shared KProgOp program
shape instead of benchmarking every live kernel id.  The LeNet
autotune scripts now report live candidate count vs representative
tuning count and use representative kernels for bounded tuning, while
keeping zero-key kernels separate because they do not share a KProgOp
cache slot.

### Fixed: autotune benchmark fires

Fixed `TKernelVariants` and `TKernelAutotune` benchmarking so each
timed run advances the kernel-fire generation.  Variant benchmarks
now actually dispatch kernels after a prior `TRealize` instead of
being skipped by the per-realize fire memo and reporting zero-work
measurements.  The CPU JIT cache is now reset with the runtime
lifecycle, and generated C emits exact f32 constants via raw-bit
unions instead of invalid literals such as `2f`.

### Added: LeNet training benchmark

Added `wl/Examples/lenet-mnist/bench-train.wls`, a training benchmark
that separates warmup, optional bounded `TKernelAutotune`, and timed
LeNet/Adam steps.  Closed the current `TOpt` gap by adding a real
`GLOBAL` opt for `GLOBAL x LOCAL` tile plans and rejecting reserved
no-op opts (`PADTO`, `NOLOCALS`, `TC`) until renderers consume them.

### Added: LeNet autotune example

Added `wl/Examples/lenet-mnist/autotune.wls`, a bounded LeNet
autotuning walkthrough that materializes the forward pass, lists
`TKernelProposed` candidates, measures sample `TKernelVariants`,
applies `TKernelAutotune`, and reruns the forward pass to check the
softmax probabilities remain stable.  Fixed `TKernelVariants`
decoding so `UPCAST` candidate rows render as opts instead of
baseline rows, and tightened the elementwise proposer so ranked
outputs only advertise `UPCAST` factors that divide the selected
axis.

### Added: explicit tile reduction nodes

Tile plans now represent scalar `S_REDUCE_SUM`/`S_REDUCE_MAX` values
as `TILE_REDUCE(TILE_SCALAR_BODY(...))` instead of only carrying
reduction axes as renderer metadata.  `TKernelTilePlan` exposes the
reduce tile and reducer scalar ids for introspection.  Focused
coverage includes SUM and MAX reduce plans plus both `GLOBAL`/`LOCAL`
axis orders for Metal tile dispatch.

### Added: Metal tile dispatch

Added an opt-in `THVM_TILE=1` Metal tile path for f32
`LOCAL`/`GLOBAL` elementwise plans.  The backend now compiles
`cg_emit_tile_metal`, caches the pipeline state, dispatches
`GLOBAL` axes as threadgroups and `LOCAL` axes as threads per
threadgroup, and records dispatch kind `"metal-tile"`.

### Added: generated tile C reductions

Generated CPU tile C now accepts reduction tile plans when the scalar
graph contains `S_REDUCE_SUM` or `S_REDUCE_MAX`, treating
`REDUCE`/`UNROLL`/`GROUP_REDUCE` axes as schedule metadata while the
scalar expression emits the accumulator loop.

### Added: generated tile C movement and cast coverage

The generated CPU tile renderer now has focused execution tests for
movement wrappers and mixed f32/f64 casts, proving that tile C reuses
the broader scalar expression emitter instead of only covering plain
elementwise arithmetic.

### Added: richer tile axis consumption

Tile plans now sync `LOCAL` and `GROUP`/`GROUP_REDUCE` opts into the
validated tile-plan view.  The generated CPU tile renderer accepts
`LOCAL` and `GLOBAL` axes as loop-like output bindings while keeping
`GROUP_REDUCE` on the tile-interpreter fallback path until explicit
tile reductions land.

### Added: scalar C expression-index rendering

The scalar-UOp C renderer now accepts `S_INDEX_E` address expressions
and the `S_I*` integer expression family for f32/f64 kernels.  This
lets generated scalar and tile C paths cover rangeify graphs that use
symbolic movement-derived addresses instead of only packed-stride
`S_INDEX`.

### Added: scalar C reduction rendering

The scalar-UOp C renderer now emits generated loops for f32/f64
`S_REDUCE_SUM` and `S_REDUCE_MAX`, with focused C JIT tests covering
both reducer kinds.

### Added: scalar C f32/f64 cast rendering

The scalar-UOp C renderer now accepts mixed f32/f64 kernels when the
mix is represented by `S_CAST`, emitting per-input and output pointer
types instead of requiring one uniform kernel dtype.

### Added: scalar C movement wrapper rendering

The scalar-UOp C renderer now emits f32/f64 `S_SHRINK`, `S_PAD`,
`S_FLIP`, `S_RESHAPE`, and `S_RESHAPE_V` wrappers by saving and
rewriting generated loop coordinates around the wrapped scalar body.

### Added: generated C tile renderer

`THVM_TILE=1` now tries a generated C tile renderer for simple
elementwise f32/f64 `TileUop` plans before falling back to the tile
interpreter.  The renderer lowers `TILE_LOOP_NEST` axes to nested C
loops, emits direct scalar expressions, and maps `UPCAST` axes to clang
unroll pragmas.

### Added: opt-in CPU tile dispatch

CPU dispatch now has a `THVM_TILE=1` path that executes validated
`TileUop` plans over scalar UOps and records dispatch kind `"tile"`.
Focused WL tests cover elementwise tile dispatch and a reduce kernel
whose tile plan carries an `UNROLL` split.

### Added: tile plan sync with KernelAxes opts

Tile plans now track the `KernelAxes` version they were built against
and rebuild from scalar UOps after `TKernelApplyOpt`, autotune resets,
or lazy WL tile introspection.  Focused WL tests cover `UPCAST`,
`UNROLL`, and `SWAP` showing up in `TKernelTilePlan`.

### Fixed: rangeify movement-chain gaps

Rangeify now re-emits scalar subgraphs under consumer-edge coordinate
contexts for rank-changing reshapes and PAD chains.  This closes the
grad reshape/reduce-max fallback, the LeNet leading-1 PAD fanout
fallback, and the attention pre-INDEX mismatch while preserving the
focused grad/NN numerics.

### Added: WL tile-UOp introspection

LibraryLink now exposes raw `TileUop` snapshots and validated tile
plan summaries to WL as `TKernelTileUops[kid]` and
`TKernelTilePlan[kid]`, with tests covering the seeded loop/store/body
plan over a rangeified elementwise kernel.

### Added: tile-UOp plan info extraction

Tile plans now expose `tile_collect_plan_info`, a validated compact
view of the root/store/body ids, scalar store/index/value ids, dtype,
and axis metadata.  The existing axis query helpers now read through
that validated view instead of indexing directly into possibly
malformed tile refs.

### Added: tile-UOp root, store boundary, and validation helpers

Tile plans now record an explicit `KernelEntry.tile_root`, validate
their root/store/body/axis structure, and expose root-loop axis count/
type/extent helpers so future renderers and autotuners do not depend on
emission order.  Seeded plans now make the scalar `S_STORE` boundary
explicit with `TILE_STORE(TILE_SCALAR_BODY(value))`.

### Fixed: stale C test contracts

Refreshed the C tests around the current dup-style grad projection
layout, `KernelEntry.fire_gen` dispatch memoization, PAD heap layout,
frame-only tag constants, and host-gated Metal real-backend coverage.
The memory notes now describe `consumer_count` as structural metadata
rather than a dispatch-time decref path.

### Fixed: rangeify per-use PAD chain lowering

Rangeify now emits use-local input loads for safe PAD chains using the
consumer edge's own `RngsCtx`, including chains through `LOAD`,
`EXPAND`, `BITCAST`, `SHRINK`, and conservative RESHAPE cases.  This
removes the conv-im2col and grad PAD rank-promotion bails while keeping
the leading-1 LeNet fanout on the fallback path until its view
addressing is modeled safely.

### Added: tile-UOp schedule scaffold

Added a non-dispatching `TileUop` arena on `KernelEntry` as the next
optimization layer above scalar UOps.  The initial builder seeds a
`TILE_LOOP_NEST(TILE_STORE(TILE_SCALAR_BODY), TILE_AXIS...)` plan from
`scalar_uops` plus `KernelAxes` when present, materialize runs it after
successful rangeify, and lifecycle cleanup goes through
`kernel_free_arrays`.  Added a focused C test and a plan doc for the
CPU/Metal tiled renderer path.

### Added: fusion_count.wlt + realize loop fixed-point (g3d)

`wl/THVMLink/Tests/fusion_count.wlt` (4/4 green) pins the plan's
kernel-count claims:

- Linear + MSE forward = 2 kernels (matmul-as-REDUCE-tail + L2-loss-
  as-REDUCE).
- Linear + MSE forward+backward = 4 kernels (the headline number
  from the rewrite arc).
- Softmax forward = 3 kernels (exp, sum-of-exp, divide; the shared
  exp can't fuse without unsafe re-compute).
- `(a + b) * c` = 1 fused kernel.

`thvm_realize` loop now runs to fixed point: the iteration body is
`res = nf(wnf(res)); mat = thvm_materialize(res)`, and exits when
both KERNELS_NEXT and ITRS are unchanged across a full pass.
Either delta alone isn't enough -- wnf can fire kernels without
growing KERNELS_NEXT, and materialize can emit a kernel without
producing new redexes (the kernel fires next iteration via wnf).
A 64-iteration safety cap remains because there's a subtle
materialize re-emission bug (kernel exhaustion at 16k slots on
some grad chains under unbounded loop); the cap converges in 2-3
iterations for working tests.

`nf` excludes TAG_REF / TAG_ALO from eager firing -- recursive
named definitions like `sgd_loop` would non-terminatingly unfold;
wnf handles them lazily at head position.

### Added: explicit nf() reducer + worklist-driven loop

`wnf` is WHNF-only -- it surfaces the head and stops at plain UOPs.
That leaves redexes nested inside (e.g. the UOPs an `interact_grad`
chain rule produces) unfired -- they're redexes by IC semantics but
WHNF never visits them.

`nf` (`src/wnf/nf.c`) is a worklist-driven full-NF reducer:

1. Seed worklist via `redex_enumerate` from root + global heap scan.
2. Pop a redex, call `redex_fire`.
3. If the fired redex was the root, track the new root (heap_replace
   inside redex_fire can't update Terms held off-heap).
4. Push (a) the result if it's itself a redex, (b) any cells the
   fire allocated that contain redexes.
5. Loop until the worklist is empty.

Per-fire cost is O(redexes-locally-created), not O(heap-size) per
sweep -- a long chain of grad + elementwise interactions runs in
linear interactions, not quadratic in heap_size * sweeps.

Pure IC machinery -- no opcode is privileged.  GRAD reduces because
`interact_grad` is wired into `redex_fire`, the same way APP-LAM
and KERNEL do.  Future combinators wired through `is_redex` /
`redex_fire` get nf coverage automatically.

`thvm_realize` swapped its first/last `wnf` calls for `nf` so the
materialize+wnf loop is now an `nf -> materialize -> nf` loop.

Renamed `docs/wnf.md` -> `docs/normal_form.md` and added an `nf`
section + the composition with materialize.

WL recovery on top of g3c: per-file run goes from "60 fail in nn"
when bypassing nf to grad 35/41 + nn 33/41 + tensors 15/15 +
beautiful_mnist 0/2 + uop_load segfault, with all other 24 files
green.  `make test` stays 166/166.

### Refactored: eager-grad-in-materialize -> wnf-first loop (g3c)

User clarified that eager-vs-lazy GRAD is a user-facing choice, not
a hardcoded model.  The materialize path no longer auto-unrolls
GRAD; thvm_realize now drives a wnf-first loop.

Removed (eager-grad code from g2d/g3b):
- The while-loop unroll at top of `thvm_materialize`.
- The GRAD branch in `visit()` that recursively unrolled deep.
- The UOP_GRAD shape rule in `term_shape_in`.

GRAD now hits an explicit `return VISIT_BAIL` (in visit) or
`return term` (at the materialize entry); the caller drives the
unroll via wnf.

Refactored `thvm_realize` (src/schedule/realize.c):
- Old: one `materialize -> kernel_compute_consumer_counts -> wnf`.
- New: `wnf(expr)`, then up to 16 iterations of
  `materialize -> kernel_compute_consumer_counts -> wnf`, exiting
  when materialize emits no fresh kernel (KERNELS_NEXT unchanged).

Users wanting Order A (forward-first, lazy backward) call
`TMaterialize` and `TWnf` themselves; `TRealize` provides the
fully-converged Order B form via the loop.

Recovery: per-file WL run goes from 73 fails + 1 segfault down to
2 fails (beautiful_mnist) + 1 segfault (uop_load).  Both handed to
g3d.  `make test` stays 166/166.

### Added: WL grad chain coverage (g3b)

Per g3a's triage memo, the bulk of WL test failures (60+) all
shared one root cause: the grad chain bottoms out at
`UOP_EXPAND(UOP_CONST(0|1), target.shape)` (interact_grad's
expand_to_target leaf rule), and the new scheduler had no path
through it.  Three fixes:

1. New `const_to_tendesc(const_loc)` in materialize.c:
   allocates a 1-element TenDesc, writes the const bits.
   `view_resolve` calls it when the chain ends in `UOP_CONST`.
2. `term_shape_in` (uop_meta.c) learns UOP_GRAD: the output
   shape equals the target's shape (heap layout [y, gy, NUM(n=1),
   target]; unary case only -- multi-target lowers to TAG_CTR
   before shape inference is queried).
3. `visit()` (build_kernel) handles UOP_GRAD anywhere in the
   tree by lazy-unrolling: loop `interact_grad` until the term
   is no longer UOP_GRAD, then recurse.
4. `visit()`'s movement-op branch (RESHAPE/EXPAND/PERMUTE/
   SHRINK/FLIP) falls through to a kernel-op emit when
   `view_resolve` fails (e.g., EXPAND wrapping a MUL from
   interact_grad's chain rule).  Populates the `src0_ndim/dims`
   + `out_ndim/dims` + `axis_perm` / `pad_widths` metadata that
   `cpu_op_*` and the Metal shaders need.

WL test recovery (per-file pass count): grad 1 -> 35 (+34),
nn 22 -> 31 (+9), tensors 13 -> 13 (unchanged but now matches
g3a's expectation).  Still red and handed to g3c: sgd 1/5,
optim 5/8, beautiful_mnist 0/2, reduce 1/4, uop_load segfault.

`make test` stays 166/166 green.

### Added: GRAD unroll + TAG_CTR descent in thvm_materialize (g2d)

`thvm_materialize` now handles UOP_GRAD sinks directly instead of
deferring entirely to wnf.  Two new branches at the top:

1. While the sink is UOP_GRAD, call `interact_grad` (one-step lazy
   unroll) until either the sink is no longer UOP_GRAD or
   `interact_grad` returns the input unchanged (chain stuck on a
   free VAR).
2. If the post-unroll sink is TAG_CTR (multi-target GRAD lowered to
   a CTR of unary GRADs), materialize each child independently and
   rebuild the CTR via `term_new_ctr`.

Then the existing flow runs (movement-op root -> realize_classify +
emit kernels per boundary).

`make test` stays 166/166 green.

### Added: PAD as kernel emit (g2c2)

`visit()` now dispatches UOP_PAD instead of bailing.  Recurses into
the source, computes output shape (`out.dim[i] = src.dim[i] + b_i +
e_i`), and populates `src0_ndim`, `src0_dims`, `out_ndim`,
`out_dims`, `pad_widths` on the KProgOp so `cpu_op_pad` and the
Metal pad shader can index correctly.  `materialize_uop_in_env(PAD)`
falls through to `thvm_materialize` (PAD isn't in
`op_is_view_movement`), which emits the kernel via the standard
boundary loop.

`make test` is 166/166 green.  `wl-test` remains red (60+ failures
across nn / shape / heap_snapshot / extern_pin / core), but those
are pre-g2 expectations from the round-1/2 WL bridge -- g3 fixes
them as part of the WL surface clean-up.

### Added: view-only path for movement ops (g2c1)

`thvm_materialize` now correctly handles RESHAPE/EXPAND/PERMUTE/
SHRINK/FLIP without allocating kernels.  New helpers:

- `view_apply_{reshape,expand,permute,shrink,flip}` compute a target
  `View` (shape, strides, offset, contiguous flag) from a source
  `View`, mirroring the round-1/2 view arithmetic but as standalone
  pure functions.  `view_apply_expand` additionally handles rank-up
  EXPAND (src.ndim < target ndim) by treating the new trailing
  axes as broadcast (stride 0).
- `view_resolve(t)` recursively walks a movement-op chain rooted at
  a TAG_TEN (or UOP_KERNEL output), allocating an alias TenDesc per
  layer via `tensor_view_of`.  Returns the final tid.
- `materialize_root_alias(t)` flattens a non-contig TenDesc into a
  contiguous copy via `view_strided_index`, used when
  `thvm_materialize` is called with a movement-op root and the
  caller (wnf) needs flat-buffer reads.

`materialize_uop_in_env(t, 0)` returns a `TAG_TEN` aliasing the
source for the 5 view-only ops; `visit()` (build_kernel) treats
movement-op children the same way, so the kernel input slot
aliases the upstream buf with the movement-rewritten View.

Metal regression fixed: `metal_dispatch_kernel` was reading
`program[ke->n_inputs]` (the old LOAD-prefix layout's "main op"
slot).  With g2b's no-prefix design that points past the program
end (opcode 0 -> "no pipeline").  Switched to
`program[ke->n_ops - 1]`.  test_metal_real: 80/166 -> 158/166 pass.

Test status:
- test_view_shrink 24/24, test_view_permute 32/32, test_view_flip
  35/35, test_expand_axis 14/14: all green.
- test_view_pad 8/15 fail, test_metal_real PAD parity 8 fail:
  exactly g2c2's scope (PAD intentionally takes the kernel-emit
  path -- a view-only PAD would have to read out-of-bounds bytes).

### Added: build_kernel for elementwise + REDUCE-as-tail roots (g2b)

`thvm_materialize` now actually emits kernels.  For each boundary in
topo order, `emit_kernel_for_boundary`:

1. Reconstructs the root UOp Term from `REALIZE_INFO` + boundary loc.
2. Infers output shape/dtype via `term_shape_in` / `term_dtype_in`.
3. Allocates an output TenDesc + buf via `tensor_alloc`, links
   `producer_kid`.
4. Calls `visit(root)`, a recursive walker that dedups inputs and
   emits one KProgOp per op (CONST / unary / binary elementwise /
   REDUCE-as-tail-when-root).  Inputs return `KSRC_AS_INPUT(slot)`
   directly -- no LOAD prefix, since the interpreter's per-step
   LOAD-skip path leaves `regs[load]` NULL and downstream refs would
   segfault.
5. Wraps the KernelEntry id in a UOP_KERNEL Term, records it in
   `BOUNDARY_TERM[bi]` so downstream boundaries can wire it as input.

Movement ops (RESHAPE/EXPAND/PERMUTE/PAD/SHRINK/FLIP) and non-tail
REDUCE return VISIT_BAIL; on bail `thvm_materialize` returns the
input term unchanged.  Movement-op support lands in g2c.

`make test` failures: 99 -> 80.  All forward + GRAD tests green
(test_grad 92/92, test_mat_op2 9/9, test_consumer_count 17/17,
test_realize_classify 22/22, test_collapse 17/17, test_decref_hook
16/16, test_materialize_v2 10/10).  Remaining 80 fails are
movement-op-only: test_expand_axis (14), test_view_shrink (3),
test_view_permute (4), test_view_pad (7), test_view_flip (4),
test_metal_real (34).

### Added: scheduler skeleton + topo-sort over realize boundaries (g2a)

`thvm_materialize` now runs the first half of the tinygrad-style
scheduler: `realize_classify` populates `REALIZE_INFO`, then a new
`topo_sort_boundaries` pass walks the marked boundary UOps and
sorts them by producer-to-consumer depth.  The order is exposed via
`materialize_boundary_count()` / `materialize_boundary_at(i)` for
g2b's kernel-emit pass.

Restructure: `uop_arity`, `uop_is_*_elementwise`, `term_shape_in`,
`term_dtype_in` moved out of `materialize.c` into a new
`src/schedule/uop_meta.c` so the include order is `uop_meta ->
consumer_count -> realize_classify -> materialize` (materialize now
consumes `REALIZE_INFO` from `realize_classify.c`).

`tests/test_materialize_v2.c` covers shared-subexpression, linear
chain, REDUCE-as-root, and chain-of-two-REDUCEs.  10/10 green; no
regression on the 99/166 stub-baseline fail count from g1.

### Removed: rounds 1-2 fusion scaffolding (g1)

Purged the dual-path fusion code that gated OFF in production:

- Deleted `src/schedule/{materialize_inlined,materialize_memo,materialize_in_env,walk,shape_env}.c` (~1430 LOC).
- Reduced `src/schedule/materialize.c` from 666 LOC to a 116-LOC stub
  keeping just `uop_arity`, `uop_is_unary_elementwise`,
  `uop_is_binary_elementwise`, `term_shape_in`, `term_dtype_in`, and
  no-op `thvm_materialize` / `materialize_uop_in_env` so the build
  links until the g2 rewrite lands.
- Removed `MATERIALIZE_USE_REALIZE_INFO` toggle (global + extern +
  WL bridge `thvm_wl_set_use_realize_info` + `TSetUseRealizeInfo`).
- Removed `MAT_STATS_*` diagnostic counters + `MAT_STATS_LABEL` +
  `THVM_MAT_STATS` env hook + WL bridge `thvm_wl_mat_stats_label` +
  `TMatStatsLabel`.
- Removed `ShapeBinding` + `SHAPE_ENV` from TContext.
- Deleted `tests/{test_materialize,test_materialize_inlined,test_splice,test_use_realize}.c`
  and `wl/THVMLink/Tests/use_realize.wlt` (all targeted the deleted
  scaffolding).

`make test` fails 99/166 because most surviving tests call
`thvm_materialize` / `materialize_uop_in_env` and now get a no-op.
Per the approved plan: this is the expected post-purge state;
the green count comes back as the g2 rewrite + g3 WL fixups land.

### Added: Wolfram-axiom bench-twee comparison (stage 10c)

`docs/plans/corpus_expansion_findings.md` gains a "Stage 10c
update" section with the new `wolfram_axiom_literal` row:

| Fixture | thvm | Twee | Gap |
|---|---|---|---|
| `wolfram_axiom_literal` | PROVED 0.012 ms | PROVED 19.740 ms | thvm wins |

Updated score: 10/13 thvm-wins, 3/13 thvm-fails (still
`comm_monoid_swap` QUEUE_EMPTY + the two group TIMEOUTs;
Wolfram-axiom shape did not introduce new failures).

The memo also documents the IC-rewrite scaling finding
discovered while attempting the Sheffer-commutativity stress
fixture: the IC-routed rewrite path on depth-4 NAND nesting
exhausts HEAP_CAP at ~13M cells worst-case under
`step_cap=32`. Lists three follow-on options (HEAP_CAP bump,
widened 9.3 heap reset, in-place rewrite compaction) any of
which would unblock the stress-fixture row.

Closes 10 (parent + 10a/b/c all `[x]`). Documentation +
bench-twee snapshot regenerated; tests stay green
(166/166 C, 323 WL).

### Added: Wolfram-axiom literal fixture (stage 10b)

`tests/data/atp/wolfram_axiom_literal.pr` (+ `.expect`):
literal instance of the Wolfram axiom with concrete
constants p, q, r as the conjecture.  Predicted PROVED @ 0;
observed PROVED @ 0 across all 4 (cp-gen x rewrite) modes.
Bench-atp jumps from 98/98 to 102/102 sub-checks.

**Stress fixture dropped:** the planned
`wolfram_sheffer_commutativity` (`nand(a, b) = nand(b, a)`)
crashes the bench under IC-routed rewrite modes (ci, ii) --
the IC-rewrite path on the axiom's depth-4 NAND nesting
exhausts the 16M-cell HEAP_CAP before the 32-step budget is
reached.  Under cc-mode the run completes as predicted
TIMEOUT @ 32 in 47 ms (32 rules, 1133 trace entries).
Per the design memo's stop condition, 10b ships with only
the literal fixture; the full Sheffer-commutativity test
is deferred pending a HEAP_CAP bump, widened heap reset
beyond 9.3's joined-CP branch, or a less-extreme stress
conjecture.  Memo's prediction table updated with the
DROPPED row + cell-budget analysis.

166/166 C, 323 WL.

### Added: Wolfram-axiom design memo (stage 10a)

`docs/plans/wolfram_axiom_design.md` (~150 lines) picks two
fixtures based on Wolfram's 2000 single-equation axiomatisation
of Boolean algebra in NAND form (proven equivalent to standard
Boolean algebra by McCune et al., JAR 2002):

    ((x NAND y) NAND z) NAND (x NAND ((x NAND z) NAND x)) = z

| Fixture | Predicted | Step bound | Notes |
|---|---|---|---|
| `wolfram_axiom_literal`         | PROVED  | 0      | Literal axiom instance with constants |
| `wolfram_sheffer_commutativity` | TIMEOUT | 32 cap | `nand(a, b) = nand(b, a)`; lemma-discovery hard |

One PROVED + one TIMEOUT, brings corpus from 12 to 14 fixtures.
KBO/LPO orient the axiom identically (lhs depth 4 vs rhs
variable; no weight-vs-precedence flip).  10b implements the
fixtures, 10c reruns the bench-twee comparison.

Documentation-only.  Tests stay green (166/166 C, 323 WL).

### Added: corpus expansion findings memo (stage 9.4c)

`docs/plans/corpus_expansion_findings.md` (~150 lines) captures
the IC-vs-Twee bench comparison on the 12-fixture enlarged
corpus.

**Score: 9 thvm-wins, 3 thvm-fails** (against Twee 12/12).
The 9 wins are dominated by goal-rewrite shortcutting at
step 0; thvm's wall-clock advantage is real but biased toward
"answer is one rewrite away" cases (Twee always pays ~20 ms
saturation startup).

**The 3 fails cluster on AC redundancy + lemma discovery**:
- `comm_monoid_swap` -- QUEUE_EMPTY @ 2 (commutativity rewrite
  cycle, needs AC-aware joinability)
- `group_commutative_inverse` -- TIMEOUT @ 32 (left-inverse
  lemma discovery costs ~10-15 CPs)
- `group_left_id_from_assoc` -- TIMEOUT @ 32 (same root cause
  as above; same per-fail counters: 32/20/116/95/51)

8.5d's "KBO and LPO agree on our corpus" claim survives the
expansion: every fixture's axioms orient identically under
both. All 4 (cp-gen x rewrite) modes report byte-identical
counters per fixture, re-confirming 8.1e-ii / 8.3e-ii parity.

Closes 9.4 (parent + 9.4a/b/c all `[x]`). Documentation +
bench snapshots; tests stay green (166/166 C, 323 WL).

### Added: corpus expansion fixtures (stage 9.4b)

Five new bench fixtures (`.pr` + `.expect`) under
`tests/data/atp/`:

| Fixture | Division | Predicted | Observed |
|---|---|---|---|
| `comm_monoid_swap`         | GRP | PROVED  | QUEUE_EMPTY @ 2 |
| `lattice_absorb_simple`    | LAT | PROVED  | PROVED @ 0      |
| `ring_distrib_zero`        | RNG | PROVED  | PROVED @ 0      |
| `comb_K`                   | LCL | PROVED  | PROVED @ 0      |
| `group_left_id_from_assoc` | GRP | TIMEOUT | TIMEOUT @ 32    |

Bench-atp jumps from 78/78 to 98/98 sub-checks (5 new files x
4 modes); all 4 (cp-gen x rewrite) modes agree on status per
fixture, preserving the parity property from 8.1e-ii / 8.3e-ii.

**Surprise:** `comm_monoid_swap` returns QUEUE_EMPTY rather
than PROVED.  The commutativity axiom is unorientable, so the
unfailing 2-way fallback installs both `f(x, y) -> f(y, x)`
and the reverse; the goal-rewrite path then cycles between
`f(a, b)` and `f(b, a)` without canonicalising.  This is the
textbook AC-redundancy gap; flagged in
`docs/plans/corpus_expansion_design.md` as a clean motivation
for future AC-aware joinability work.

`ring_distrib_zero` and `comb_K` PROVED faster than predicted
(step 0 vs predicted 1-2 / 1) -- the goal-rewrite check
shortcuts before any saturation step fires.  Memo's prediction
table updated with the observed column.

166/166 C, 323 WL.

### Added: corpus expansion design memo (stage 9.4a)

`docs/plans/corpus_expansion_design.md` (~200 lines) picks the
candidates for 9.4b to implement.

Five hand-encoded textbook UEQ fixtures spanning GRP / RNG /
LCL / LAT divisions:

| Fixture | Division | Predicted | Step bound |
|---|---|---|---|
| `comm_monoid_swap`         | GRP | PROVED  | 0-1     |
| `lattice_absorb_simple`    | LAT | PROVED  | 0       |
| `ring_distrib_zero`        | RNG | PROVED  | 1-2     |
| `comb_K`                   | LCL | PROVED  | 1       |
| `group_left_id_from_assoc` | GRP | TIMEOUT | 32 cap  |

Hits the >=1 PROVED + >=1 TIMEOUT mix.  Brings corpus from 7 to
12 fixtures.  Each candidate is axiomatized inline, with KBO-vs-
LPO orientation notes (every axiom is `lhs > rhs` under both
orderings except commutativity in #1, which is unorientable
under both -- closes via the unfailing 2-way fallback).

Documentation-only.  Tests stay green (166/166 C, 323 WL).

### Added: heap checkpoint/reset for saturation hygiene (stage 9.3)

`thvm_atp_heap_checkpoint()` snapshots `HEAP_NEXT`;
`thvm_atp_heap_reset(c)` pops it back, reclaiming the cells
in between. The saturation step (`thvm_atp_step`) now
checkpoints just before the per-CP rewrite-normalize block
and resets on the trivially-joined branch, where neither `l`
nor `r` is referenced downstream. Reset is a silent no-op if
the requested checkpoint is past the current `HEAP_NEXT`
(API safe to sprinkle in step paths).

Most CPs end up trivially joined, so this should materially
reduce intra-saturation cell accumulation under
`use_ic_rewrite = 1`. 8.3e-iii's `BENCH_STEP_BUDGET = 32`
was set to dodge `HEAP_CAP` overflow at 256; this change
gives back headroom on that bound.

Tests (4 new cases in `tests/test_atp.c`):
- `heap-checkpoint/reads-heap-next`
- `heap-reset/pops-back`
- `heap-reset/refuses-to-advance` (silent no-op invariant)
- `heap-reset/joined-cp-reclaims-cells` (functional check on
  a trivially-joined axiom: HEAP_NEXT after a step stays
  within a few cells of the pre-step checkpoint)

166/166 C, 323 WL.

### Added: file-driven TATP runner (stage 9.2)

`TATP[File["path.pr"]]` parses a Waldmeister .pr spec via
`wald_parse_file` and runs the saturator directly, returning
the same `<|"Status", "Steps", "Rules", "QueueSize"|>`
Association as the expression form.

- New LibraryLink entry `thvm_wl_atp_run_file` (UTF8String,
  Integer) -> NumericArray[4]: parses, builds KBO/LPO config
  from `spec->symbols[i].prec_rank`, honours EXISTS sections
  (switches to `set_goal_existential`), runs `thvm_atp_run`,
  packs `[status, n_rules, n_trace, n_cps]`. Parse failures
  return ATP_RUNNING (0) as a sentinel.
- New TATP downvalue `TATP[File[path_String], OptionsPattern[
  {MaxSteps -> 64}]]` dispatched ahead of the expression form.
  Witness bindings are not surfaced in v0 (witness-name to
  WL-symbol mapping would duplicate the encoder state); callers
  that need them keep using the expression form.

Tests (4 new VerificationTests in `wl/THVMLink/Tests/atp.wlt`):
- `monoid-right-id-proves` (universal-goal fixture)
- `exists-inverse-proves` (EXISTS-section fixture)
- `keys-shape-matches-expression-form`
- `missing-path-yields-running-sentinel`

166/166 C, 323 WL.

### Added: multi-witness `.pr` fixture (stage 9.1d)

`tests/data/atp/exists_multi.pr` (+ `.expect`): existential
goal `f(x, x) = a` with axioms `f(b, b) = a` and `f(c, c) = a`
under LPO `f > b > c > a`.  Both rules unify with the goal at
the top, binding x to a different constant; full enumeration
via `narrow_all` recovers x=b and x=c.

The bench harness still runs the single-witness narrow path
(`thvm_atp_run` with `set_goal_existential`), so it reports
PROVED on the first witness.  All 4 (cp-gen x rewrite) modes
agree on status -- parity holds for the multi-witness setup.
Multi-witness enumeration is exercised separately by 9.1b's C
tests and 9.1c's WL `AllWitnesses -> True` tests.

166/166 C, 319 WL.

### Added: WL multi-witness surface (stage 9.1c)

`TATP[axioms, conjecture, Witness -> {x_, ...}, AllWitnesses
-> True]` enumerates all witnesses found by `narrow_all`.

- New LibraryLink entry `thvm_wl_atp_run_all_witnesses`
  (in `wl/THVMLink/CSource/thvmlink.c`) takes the existential
  args plus `max_depth`, `max_witnesses`. Saturates first with
  no goal set so `thvm_atp_run` does not early-exit, then
  calls `thvm_atp_narrow_all` on the original goal. Output:
  `[status, n_rules, n_trace, n_cps, n_found,
   w_0_id_0, ..., w_(max_witnesses-1)_id_(n_witness-1)]`,
  zero-padded for unused witness rows.
- New TATP options `AllWitnesses -> False` (default),
  `MaxDepth -> 8`, `MaxWitnesses -> 16`. With
  `AllWitnesses -> True` the result Association swaps the
  singular `"Witness" -> <|...|>` for `"Witnesses" -> {<|x ->
  t1|>, <|x -> t2|>, ...}`. Default behaviour is unchanged
  (singular `Witness` key) so existing callers keep working.

Tests (5 new VerificationTests in `wl/THVMLink/Tests/atp.wlt`):
- `witnesses-key-present`, `witnesses-is-list`
- `two-rules-yield-two-witnesses` (the multi-witness happy
  path: axioms `f(a, e) == a` and `f(e, e) == a` both unify
  with goal `f(x_, e) == a`)
- `max-witnesses-caps-list` (cap honored)
- `default-stays-singular` (backwards-compat regression)

166/166 C, 319 WL.

### Added: bounded DFS multi-witness narrowing (stage 9.1b)

`thvm_atp_narrow_all(s, lhs, rhs, max_depth, max_witnesses,
witnesses[])` enumerates witness substitutions by recursively
trying every `(position, rule)` choice on each side. Leaf
condition: `kbo_eq(lhs, rhs)` emits the accumulated subst.
Stateless w.r.t. `s->witness_subst` -- writes only the
caller's array.

Implementation (~110 LOC in `src/atp/_.c`):
- `NarrowAllCtx` carries the witnesses array + caps + counter.
- `NarrowAllVisitor` is the per-frame closure for
  `cp_walk_positions`; on each successful unification it
  composes the new subst into a fresh accumulator copy and
  recurses with the sigma-applied terms.
- `narrow_all_dfs` is the recursive driver; siblings start
  from the same parent acc, so DFS branches stay independent.

Tests (7 new cases in `tests/test_atp.c`):
- `no-rules-returns-zero`
- `already-equal-emits-empty-witness` (depth-0 short-circuit)
- `depth-zero-no-narrow` (bound enforcement)
- `single-witness-binds-x` (parity with 8.9b's narrow_step)
- `two-rules-two-witnesses` (multi-witness happy path; both
  `x=a` and `x=e` recovered)
- `max-witnesses-caps-count` (cap honored)
- `state-untouched` (sentinel in `s->witness_subst` survives)

Tests stay green (166/166 C, 314 WL).

### Added: multi-witness narrowing design memo (stage 9.1a)

`docs/plans/multi_witness_design.md` (~150 lines) specifies
the bounded DFS extension of stage 8.9's first-witness
narrow into a multi-witness enumerator.

- Algorithm: bounded DFS over `(position, rule)` choices on
  the `(lhs, rhs)` state, accumulating substitutions, leaf
  on `kbo_eq` / depth cap / witness cap.
- API:
  ```c
  fn u32 thvm_atp_narrow_all(AtpState *s,
                             Term lhs, Term rhs,
                             u32 max_depth,
                             u32 max_witnesses,
                             RewriteSubst *witnesses);
  ```
  Stateless w.r.t. `s->witness_subst` (writes only the
  caller's array, leaves `s` unchanged).
- Bounds: `max_depth=8` (mirrors `ATP_NARROW_BUDGET`),
  `max_witnesses=16`, `step_cap=0` (v0 keeps `R` fixed -- no
  saturation interleaved with narrow).
- Distinctness: v0 returns raw witnesses; caller post-filters.
- WL surface plan: `TATP[..., AllWitnesses -> True]` returns
  `<|"Witnesses" -> {<|x -> t1|>, ...}|>`; default
  (`AllWitnesses -> False`) is backwards-compatible.
- Note: multi-witness is the small-scale form of trace-level
  SupGen (8.10 deferred research vector); 9.1 is the seed.

Documentation-only. Tests stay green (166/166).

### Added: IC-native ATP arc closing memo (stage 8.10c)

`docs/plans/atp_arc_summary.md` (~250 lines) closes the arc.
Recaps stages 1-8.10 (79 `feat:` commits + a dozen `task:`
commits across the arc), tracks deferred items with their
preconditions, and frames the natural follow-on stages.

**What shipped**:
- Stages 1-4: foundations (KBO, rewrite, CP, unify)
- Stage 5: saturation loop with priority-aware INC selection
- Stage 6: `.pr` parser + PCL trace serializer
- Stage 7: 5 redundancy criteria + Twee comparison harness
- Stage 8: 10 sub-stages of full-fledged ATP iteration --
  IC-routed paths via `TAG_PRI`, multi-sort sigs, LPO ordering,
  WL bridge (`TATP[]`), `--mix` heuristic, narrowing for
  existential goals, top-K CP peek

**What's deferred** (forward-looking blocks, not failures):
- 8.2d full pure-IC `thvm_kbo` -- awaiting SupGen use case
- 8.6 unordered SUP/DUP -- awaiting HVM4 upstream

**Empirical findings worth preserving**:
- BDP connectedness is dominated by 7.1's trivial-joinability
- Rule-subsumption similarly dominated
- KBO and LPO orient identically on canonical group/monoid
  axioms (consistent with classical KB literature)
- IC-routed paths produce byte-identical counters to C paths
- Twee proves cases we time out on (heuristic gap, not
  unification gap)

**Natural follow-on stages**: TPTP corpus expansion, AC
matching, full pure-IC KBO, sort-aware KBO, AVATAR-style
clause splitting, TPTP file parsing in WL, multi-witness
narrowing, heap-resetting mechanism, trace-level SupGen.

**Verification snapshot at arc close**: 58 C test files (~14k
sub-checks), 314 WL verification tests, both suites green at
every commit.

The arc accomplished its primary goal: a working IC-native
equational theorem prover reachable from Wolfram notebooks via
`TATP[...]`.  Further development happens via discrete
follow-on stages with their own design memos.

Stage 8.10 closes; the IC-native ATP arc is complete.

### Added: thvm_atp_peek_top_k CP lookahead (stage 8.10b)

New API in `src/atp/_.c`:

```c
u32 thvm_atp_peek_top_k(AtpState *s, u32 k,
                        Term *out_lhs, Term *out_rhs);
```

Reuses the same INC-priority + SUP-tree + collapse_ordered
pipeline as `thvm_atp_select_cp` but does NOT pop -- the queue
stays unchanged.  Writes the top `k` cheapest CPs (or `n_cps`
if fewer) into the caller's buffers in priority order; returns
the actual count peeked.

Same priority logic as `select_cp` (8.5c's KBO/LPO dispatch +
8.8's `--mix` heuristic if enabled), so peek and pop agree on
what comes first.

`tests/test_atp.c` adds 6 cases (8444 sub-checks, was 8426):
- `peek/empty-queue-returns-zero`
- `peek/k-zero-returns-zero` (queue unchanged)
- `peek/singleton`: 1-CP fast path
- `peek/orders-by-priority`: 3 mixed-size CPs returned
  cheapest-first
- `peek/k-greater-than-n-clamps`
- `peek/then-pop-stays-consistent`: select_cp pops what
  peek[0] showed

Designed for future research: branchless lookahead, multi-CP
batch heuristics, debugging the priority ordering.  Stage
8.10c writes the IC-native ATP arc closing memo.

### Added: SupGen-style search design memo (stage 8.10a)

`docs/plans/supgen_search_design.md` (~120 lines) closes the
question of "what additional superposition adds value" given
the IC-native ATP arc's existing SupGen-flavored mechanisms:

**Starting position recap**: CP-priority queue (5.3) +
SUP-encoded CP enumeration (8.1) + --mix heuristic (8.8) all
already use the SupGen pattern.  The CP queue specifically
wraps each CP in `INC^k` and collapses via
`thvm_collapse_ordered` -- this *is* SupGen-style search at
the CP-selection level.

**Surveyed extensions**:
1. **A. Superpose unfailing-orient direction** -- breaks
   completeness; rejected.
2. **B. Superpose KBO vs LPO** -- empirically redundant on the
   v0 corpus (8.5d showed identical orientations); defer to
   TPTP-UEQ corpus expansion.
3. **C. Superpose --add vs --mix** -- trivially redundant
   (`min(add, mix) = add` always).
4. **D. Trace-level superposition** (the genuine SupGen
   vision) -- research-grade, multi-firing, requires
   backtracking machinery.

**Decision**: 8.10b ships a small demonstrative
`thvm_atp_peek_top_k` API that exposes the existing INC-priority
collapse pipeline as a non-popping peek; 8.10c writes the
arc-closing memo as the substantive deliverable.

**Honest mid-arc finding**: trace-level SupGen is more research
than engineering for this codebase today.  The CP-priority
queue is the SupGen mechanism that DID find a home; deeper
integration is left for follow-on when a use case (e.g., a
TPTP family that benefits from multi-trace exploration)
emerges.

### Added: TATP[..., Witness -> {x_}] WL surface (stage 8.9e)

`TATP[]` gains a `Witness -> {x_, y_, ...}` option (default
`{}`).  When non-empty:
- Each entry must match `Verbatim[Pattern][_Symbol, Blank[]]`
  (the `x_` syntax) -- enforced at runtime, otherwise
  `Failure["TATPParseError"]`.
- Names are looked up against the encoder's var-id state from
  the axioms / conjecture pass; missing names yield Failure.
- Calls a new `$atpRunExistFn` LibraryLink entry that runs the
  saturator in narrow mode and returns witness Term values
  trailing the stats array.

```mathematica
TATP[{f[a, e] == a}, f[Pattern[x, Blank[]], e] == a,
     Witness -> {Pattern[x, Blank[]]}]
(* -> <|"Status" -> "PROVED", "Steps" -> ..., "Rules" -> ...,
        "QueueSize" -> ..., "Witness" -> <|x -> 36028...|>|> *)
```

The witness-Term values are returned as raw Int64 packed Term
values for now -- the inverse encoder (decode Term back to a
WL expression) is left for future work; users with the encoder
state can decode manually.

New LibraryLink entry `thvm_wl_atp_run_existential` in
`wl/THVMLink/CSource/thvmlink.c`: same packed-NumericArray
input as `thvm_wl_atp_run` plus a witness-id MTensor; output
NumericArray gains `n_witness` trailing Term values.  WL-side
loader `$atpRunExistFn`.

`wl/THVMLink/Tests/atp.wlt` adds 5 cases (314 WL tests, was
309):
- `TATP/witness/proves-with-narrow`: narrowing closes
- `TATP/witness/witness-key-present`: result has `"Witness"`
- `TATP/witness/key-is-x`: `Witness` keyed by `x` symbol
- `TATP/witness/missing-name-yields-failure`: var not in
  axioms/conjecture
- `TATP/witness/empty-runs-universal-path`: empty Witness
  list takes the existing path

Stage 8.9 is now complete (a-e shipped).  Existential equational
queries are reachable from Wolfram notebooks via
`TATP[..., Witness -> {x_}]`.

### Added: EXISTS .pr section + bench narrow dispatch (stage 8.9d)

New `EXISTS` section in the `.pr` grammar lists existing
variable names (declared earlier in `VARIABLES`) that should be
treated as existentially quantified in the conjecture:

```
VARIABLES       x : ANY
EQUATIONS       f(a, e) = a
EXISTS          x
CONCLUSION      f(x, e) = a
```

Implementation:
- New `WSEC_EXISTS = 9` enum + recognized in
  `wald_section_from_ident`.
- New `wald_parse_exists(spec, lex)` walks idents, looks them up
  against `spec->vars[]`, stores FVR ids in
  `spec->existential_var_ids[]`.  Permissive: unknown names
  silently skipped.
- New `WaldSpec` fields `existential_var_ids[REWRITE_MAX_VAR]` +
  `n_existential` (cap matches `RewriteSubst`).

Bench harness `tests/test_bench_atp.c` now dispatches: when
`spec->n_existential > 0`, calls `thvm_atp_set_goal_existential`
instead of `thvm_atp_set_goal` -- saturator runs in narrow mode
per 8.9c.

New fixture `tests/data/atp/exists_inverse.pr`: rule
`f(a, e) -> a`, EXISTS `x`, CONCLUSION `f(x, e) = a`.  All four
bench modes prove in 0 saturation steps (narrow closes at the
first `goal_check`).  Companion `.expect`.

`tests/test_wald.c` adds 4 cases (5541 sub-checks, was 5527):
- `wald/exists/section-recognized`
- `wald/exists/missing-section-defaults-to-zero`
- `wald/exists/multiple-vars`: `EXISTS x, y` populates both
- `wald/exists/end-to-end-narrow-binds-witness`: full pipeline
  parse -> KBO config -> set_goal_existential -> run -> PROVED
  with witness `x = a`

8.9e adds the WL `TATP[..., Witness -> {x_}]` surface.

### Added: existential-goal narrowing in goal_check (stage 8.9c)

`AtpState` gains `u8 goal_existential` (default 0).  When set,
`thvm_atp_goal_check` dispatches from the rewrite-and-compare
path to a narrow-and-extract path:

- Iterates `thvm_atp_narrow_step` up to `ATP_NARROW_BUDGET = 8`
  per call (outer saturation calls `goal_check` again with a
  larger R if budget exhausts).
- After each narrow, structural-equality check: PROVED iff
  `kbo_eq(lhs, rhs)`.
- Witness substitution accumulates in `s->witness_subst`.
- Goal slots are updated with the narrowed terms so successive
  `goal_check` calls see the post-σ state.

New API:
- `u8 thvm_atp_set_goal_existential(s, lhs, rhs)` -- mirrors
  `thvm_atp_set_goal` but flips `s->goal_existential = 1`.
  Honors the 8.4d sort-check gate.  `lhs == 0` clears (sets
  flag back to 0).

`tests/test_atp.c` adds 5 cases (8426 sub-checks, was 8415):
- `exist/default-off`
- `exist/set-flips-flag` (with clear via `lhs == 0`)
- `exist/narrow-proves-with-witness`: rule `f(a, e) -> a`,
  goal `f(x, e) = a` -> PROVED, witness `x = a`
- `exist/no-narrow-returns-running`: head-mismatched rule
  doesn't apply
- `exist/already-equal-proves-no-narrow`: structural identity
  short-circuits

8.9d adds `.pr` `EXISTS` syntax + bench fixture; 8.9e adds the
WL `TATP[..., Witness -> {x_}]` surface.

### Added: thvm_atp_narrow_step + get_witness primitives (stage 8.9b)

`AtpState` gains a `RewriteSubst witness_subst` field
populated during narrowing.  Two new public APIs in
`src/atp/_.c`:

- `u8 thvm_atp_narrow_step(s, lhs, rhs, *out_lhs, *out_rhs,
  *witness)` -- one narrowing step.  Walks every non-variable
  position of `lhs` then `rhs` (left-to-right), tries unifying
  each subterm with each rule's LHS.  On first success: applies
  σ to both sides, accumulates the binding into `witness->
  bindings[]`, returns 1.  Returns 0 if no narrow step applies.
- `Term thvm_atp_get_witness(s, var_id)` -- reads
  `s->witness_subst.bindings[var_id]`; returns 0 if unbound or
  out of range.

Implementation: reuses `cp_walk_positions`, `cp_subterm_at`,
`cp_replace_at` from `src/cp/_.c` (visible in the single-TU
build).  No σ-composition step needed because each iteration's
σ is applied to both sides before the next narrow attempt --
new bindings already live in the post-σ universe.

`tests/test_atp.c` adds 6 cases (8415 sub-checks, was 8401):
- `narrow/no-rules-returns-zero`
- `narrow/top-position-binds-witness`: rule `f(a, e) -> a`,
  goal `f(x, e) = a` -> binds `x = a`, both sides become `a`
- `narrow/no-unifier-returns-zero`: head-mismatched rule and
  goal -> no narrow step applies
- `narrow/get-witness-empty`
- `narrow/get-witness-out-of-range`
- `narrow/get-witness-roundtrip`

Stage 8.9c integrates this into `thvm_atp_goal_check` via the
`s->goal_existential` flag.

### Added: narrowing design memo (stage 8.9a)

`docs/plans/narrowing_design.md` (~180 lines) lays out the
design for stage 8.9 -- narrowing for existential goals
(Waldmeister's *NormaleZiele.c* "normal goals" + *Zielverwaltung.c*
"goal management").

**Key distinction**: rewriting matches LHS against a subterm to
substitute RHS; narrowing UNIFIES, accumulating a witness
substitution σ.  FVRs in the goal are existentially quantified
and σ binds them.

**API decision**:
- Explicit witness-var-id list: `thvm_atp_set_goal_existential
  (s, lhs, rhs, witness_var_ids[], n_witness)`.  Sets
  `s->goal_existential = 1`.  Other FVRs in the goal stay
  treated as opaque.
- Witness output via `thvm_atp_get_witness(s, var_id)` reading
  from `s->witness_subst[REWRITE_MAX_VAR]`.

**Saturation-loop divergence**: only step 7 (`goal_check`)
changes -- narrow at every non-variable position; on σ-success
substitute and continue narrowing.  Step 1-6 unchanged.

**Termination**: bounded by step_cap (existing) + a narrow-budget
parameter for per-iteration depth.

**Migration plan**:
- 8.9b: `thvm_atp_narrow_step` helper + tests
- 8.9c: integrate via `goal_check` flag-dispatch
- 8.9d: `.pr` `EXISTS` syntax + bench fixture
- 8.9e: WL `TATP[..., Witness -> {x_}]` surface

Out of scope: multi-witness enumeration, conditional narrowing,
higher-order narrowing.

### Added: --mix CP-priority heuristic (stage 8.8)

`AtpState` gains `use_mix_heuristic` (default 0).  When set, the
CP queue's priority weight in `thvm_atp_select_cp` adds a
penalty of `MIX_UNORIENTED_PENALTY = 4` to CPs that fail to
orient cleanly (KBO_UN or KBO_EQ under the active KBO/LPO
config).  Mirrors Waldmeister's `--mix` heuristic in
`ClasHeuristics.c` ("classification heuristics") -- biases
toward CPs whose orientation is unambiguous, typically a small
win on hard problems.

Implementation:
- New static helper `atp_cp_priority(s, lhs, rhs)` picks
  `--add` (size-only) or `--mix` (size + penalty) based on the
  flag.  Calls `atp_compare` (8.5c) so it respects KBO vs LPO.
- `thvm_atp_select_cp` calls `atp_cp_priority` instead of the
  inline `atp_symbol_count` sum.
- Soundness preserved: priority changes pop ORDER, not the
  closure of the saturation; the saturator explores the same
  rule set under either heuristic.

`tests/test_atp.c` adds 4 cases (8401 sub-checks, was 8394):
- `atp/mix-heuristic-default-off`
- `atp/mix-heuristic-changes-pop-order`: hand-built KBO_UN
  pair shows `mix_prio - add_prio == MIX_UNORIENTED_PENALTY`
- `atp/mix-heuristic-no-penalty-on-clean-orient`: KBO_GT pair
  shows identical priorities under add and mix
- `atp/mix-heuristic-saturation-still-correct`: full group-
  axiom saturation under both heuristics produces the same
  final status

### Added: TATP[] WL surface form (stage 8.7d)

`TATP[{lhs == rhs, ...}, conjecture]` lands as a public WL
function (declared via `TATP::usage` in the package's public
section).  Wires 8.7c (encoder) into 8.7b (LibraryLink runner)
and decodes the stats array into a notebook-friendly
Association.

```mathematica
TATP[{f[Pattern[x, Blank[]], e] == Pattern[x, Blank[]]},
     f[a, e] == a]
(* -> <|"Status" -> "PROVED", "Steps" -> 0, "Rules" -> 0,
        "QueueSize" -> 0|> *)
```

Implementation notes:
- `SetAttributes[TATP, HoldAll]`: WL evaluates `a == a` to
  `True` before reaching us, so we hold both axioms list and
  conjecture and destructure via `Extract[..., HoldComplete]`.
- `Catch[..., "TATPError"]` at the boundary; parse errors
  Throw a `Failure[]` early without unwinding the threaded
  encoder state piecewise.
- `MaxSteps -> 64` option (default).
- Status decoded from the `AtpStatus` enum:
  `0=RUNNING, 1=PROVED, 2=REFUTED, 3=TIMEOUT, 4=QUEUE_EMPTY`.

`wl/THVMLink/Tests/atp.wlt` adds 5 cases (309 WL tests, was
304):
- `ATP/TATP/trivial-reflexive-proves`: `a == a` derives `a == a`
- `ATP/TATP/direct-rewrite-proves`: `f[x_, e] == x` rewrite
- `ATP/TATP/return-keys`: Association keys are
  `{QueueSize, Rules, Status, Steps}`
- `ATP/TATP/bad-axiom-yields-failure`: missing `==` -> Failure
- `ATP/TATP/bad-conjecture-yields-failure`

Stage 8.7 is now complete (a-d shipped).  The IC-native ATP is
reachable from Wolfram notebooks for hand-written equational
problems.

### Added: WL-expression-to-Term encoder (stage 8.7c)

New LibraryLink helper `thvm_wl_term_new_ctr` in
`wl/THVMLink/CSource/thvmlink.c` builds a `TAG_CTR` Term from a
label + list of child Term integers (uses `MTensor` for clean
empty-list handling).  WL-side loader `$termNewCtrFn`.

WL-side encoder `encodeAtpTerm[expr, state]` in
`wl/THVMLink/Kernel/THVMLink.wl` walks an expression and produces
`{term, state'}`:

- `Verbatim[Pattern][name_Symbol, Blank[]]` (the `x_` syntax)
  -> `term_new_fvr(var_id)` via raw `thvm_wl_term_new`.  Variable
  ids are stable across occurrences of the same name within a
  single encode.
- Bare `Symbol` -> nullary CTR.
- Compound `head[args...]` -> CTR with recursively-encoded
  children.
- Symbol labels start at 1 and increment per fresh symbol; var
  ids start at 0.

State threaded explicitly via `Association` (Module-local mutation
on Associations doesn't work in WL; use `ReplacePart` to thread).
The pattern-matched dispatch via `Verbatim[Pattern]` correctly
distinguishes the `x_` form from a literal `Pattern[]` head.

`wl/THVMLink/Tests/atp.wlt` adds 5 cases (304 WL tests total --
was 299):
- `ATP/encoder/symbol-becomes-nullary-ctr`
- `ATP/encoder/distinct-symbols-get-distinct-labels`
- `ATP/encoder/compound-head-becomes-ctr-with-children`:
  `f[zero, succ[zero]]` -> CTR with arity 2
- `ATP/encoder/pattern-becomes-fvr`
- `ATP/encoder/pattern-var-stable-across-occurrences`

8.7d wires this into the `TATP[]` user-facing surface.

### Added: thvm_wl_atp_run LibraryLink helper (stage 8.7b)

New entry point in `wl/THVMLink/CSource/thvmlink.c`:

```c
EXTERN_C DLLEXPORT int thvm_wl_atp_run(...)
```

Inputs:
- `args[0]`: shared `Int64` `MNumericArray` of packed Term values
  `[n_axioms, lhs_0, rhs_0, ..., goal_lhs, goal_rhs]`.
- `args[1]`: max_steps (mint).
- `args[2]`: max_label (mint; sizes a v0 trivial precedence /
  weights table).

Output: `Int64` `MNumericArray` of `[status, n_rules, n_trace,
n_cps]`.

The trivial KboConfig (uniform weights = 1, precedence = label
+ 1) gives KBO_UN for most comparisons -- saturation falls into
unfailing fallback.  Future stages can pass a real precedence
+ weights array.  Trace serialization deferred to 8.7d.

WL-side loader added to `wl/THVMLink/Kernel/THVMLink.wl` as
`$atpRunFn`.

`wl/THVMLink/Tests/atp.wlt` (4 cases, 299 WL tests total -- was
295):
- `ATP/init`
- `ATP/runner/trivial-self-equation-proves`: x = x with goal
  x = x -> PROVED with n_trace=1 (axiom only)
- `ATP/runner/no-axioms-distinct-goals-empties-queue`: x vs y
  goal -> QUEUE_EMPTY
- `ATP/runner/return-shape`: stats array has shape [4]

8.7c-d add the WL-side encoder + `TATP[]` surface.

### Added: WL ATP bridge design memo (stage 8.7a)

`docs/plans/wl_atp_bridge.md` (~190 lines) lays out the design
for `TATP[axioms, conjecture]`.

**WL surface**:
```mathematica
TATP[
  { f[x_, e] == x, f[x_, i[x_]] == e,
    f[f[x_, y_], z_] == f[x_, f[y_, z_]] },
  f[a, i[a]] == e
]
(* -> Association["Status" -> "PROVED", "Steps" -> 1, ... ] *)
```

**Encoding**:
- `Symbol[s]` (e.g. `e`) -> nullary `term_new_ctr`
- `head[args...]` -> `term_new_ctr` with encoded children
- `Pattern[var, Blank[]]` (`x_`) -> `term_new_fvr`
- `Equal[lhs, rhs]` -> equation pair fed to
  `thvm_atp_add_equation`

**Two-layer LibraryLink plumbing**:
- 8.7b: pre-encoded Term entry point `thvm_wl_atp_run`
  isolates the saturator-from-LibraryLink plumbing; tested
  with manually-built Terms.
- 8.7c-d: WL-side encoder + wrapper.  Encoder is pure WL (no
  new C); calls existing `thvm_wl_term_new` primitives.

Out of scope (deferred): TPTP-UEQ file parsing from WL,
proof-tree-as-graphics, typed pattern handling.  Documented
stop conditions for 8.7b-d.

### Added: ORDERING-kind capture + bench dispatch (stage 8.5d)

`WaldSpec` gains a `u8 ordering_kind` field (default
`WALD_ORDER_KBO = 0`); the parser captures `WALD_ORDER_LPO = 1`
when the `.pr` file's ORDERING section starts with `LPO`,
otherwise stays at KBO.  The leading-letter discriminator is
sufficient (only KBO and LPO are recognized by Waldmeister's
.pr format).

Bench harness `tests/test_bench_atp.c` now builds an `LpoConfig`
from the same precedence array and calls `thvm_atp_set_lpo` when
`spec->ordering_kind == WALD_ORDER_LPO`.  All 5 fixtures in
`tests/data/atp/` declare `ORDERING LPO`, so they now actually
run under LPO instead of being silently mapped to KBO.

**Empirical finding**: bench numbers are byte-identical between
KBO and LPO on these axioms.  KBO-with-weights and LPO-with-
precedence happen to orient the canonical group / monoid /
list-length axioms the same way.  This is consistent with the
classical KB-completion literature (both orderings agree when
the "obvious" precedence and weight functions are aligned).

`tests/test_wald.c` adds 3 cases (5527 sub-checks, was 5522):
- `wald/ordering/lpo-captured`
- `wald/ordering/kbo-captured`
- `wald/ordering/missing-defaults-to-kbo`

`tests/test_atp.c` adds 1 case (8394 sub-checks, was 8392):
- `atp/lpo-vs-kbo-parity-on-group-axioms`: full saturation under
  both orderings produces the same final status and rule count

Stage 8.5 is now complete.

### Added: LPO wired into saturation (stage 8.5c)

`AtpState` gains a `const LpoConfig *lpo` field alongside the
existing `kbo`.  Per `docs/plans/lpo_design.md`'s Choice C: when
non-NULL, takes precedence over KBO; when both are NULL, every
comparison is incomparable (KBO_UN) and saturation falls into
unfailing fallback.

New API:
- `void thvm_atp_set_lpo(AtpState *s, const LpoConfig *lpo);`
  -- attach an LpoConfig (or NULL to clear).

New static helper `atp_compare(s, lhs, rhs)` picks LPO (if
attached) or KBO and returns a unified `KboCmp`-shaped result
(both enums share numeric values: EQ=0, GT=1, LT=-1, UN=2;
direct cast is safe).

`thvm_atp_orient_and_add` now calls `atp_compare` instead of
`thvm_kbo` directly.  All other AtpState callsites (joinability
filter, connectedness counter, rule subsumption check) still
use the rewriter rather than the comparator, so they're
unaffected.

`tests/test_atp.c` adds 3 cases (8392 sub-checks, was 8386):
- `atp/lpo-default-off`: fresh AtpState has `lpo == NULL`
- `atp/lpo-orient-prefers-lpo-when-attached`: orienting
  `f(x, e) -> x` succeeds via LPO subterm-dominance with a
  precedence-only LpoConfig
- `atp/lpo-set-clear-roundtrip`: setter accepts and clears
  cleanly

Stage 8.5d adds an `.pr` fixture wired to actually use LPO.

### Added: thvm_lpo Lexicographic Path Ordering (stage 8.5b)

`src/lpo/_.c` (~150 lines) implements the LPO comparator
following Dershowitz (1982).  Mirrors `src/kbo/_.c`'s structure:

- `lpo_eq` -- structural equality on Terms (local copy of the
  KBO pattern; static-but-TU-visible from kbo would also work)
- `lpo_var_occurs_in` -- "does FVR var_id appear anywhere in
  t?", used for the variable subterm-occurrence cases
- `lpo_some_arg_dominates` -- case (1) check (subterm
  domination)
- `lpo_dominates_all_args` -- case (2)/(3) condition
- `lpo_lex` -- lexicographic argument comparison
- `lpo_rec` -- top-level recursion implementing all three
  Dershowitz cases plus variable handling

New types in `src/thvm.h`:
- `typedef enum { LPO_EQ, LPO_GT, LPO_LT, LPO_UN } LpoCmp;`
- `typedef struct { const u32 *precedence; u32 n_labels; }
  LpoConfig;` (no weights or var_weight -- LPO is precedence-
  only).
- `fn LpoCmp thvm_lpo(Term s, Term t, const LpoConfig *cfg);`

`tests/test_lpo.c` (12 sub-checks, 9 cases):
- `eq-on-identical-terms`
- `gt-via-precedence`: `h(b) > f(a)` when `h >_F f`
- `gt-via-subterm-domination`: `f(a, b) > a`
- `gt-via-lex-on-equal-heads`: `f(a, c) > f(a, b)` when
  `c >_F b`
- `un-on-distinct-vars`
- `var-occurs-as-strict-subterm`: `f(x) > x` and `x < f(x)`
- `var-not-in-term-incomparable`
- `eq-fvr-same-id`
- `group-axiom-orient-gt`: `f(x, a) > x` (right-id-style rule)

Stage 8.5c will wire LPO into `AtpState` + dispatch from
`thvm_atp_orient_and_add`.

### Added: LPO ordering design memo (stage 8.5a)

`docs/plans/lpo_design.md` (~150 lines) lays out the design for
porting LPO (Dershowitz, "Orderings for term-rewriting systems",
1982) alongside KBO.

**Algorithm summary**: recursive comparator with three GT cases
on `f(s_1...s_m) >_lpo g(t_1...t_n)`: subterm domination,
precedence-based with arg verification, and lex-on-equal-heads.
Variable cases handled via subterm-occurrence check.

**LpoConfig**: separate struct (`precedence` + `n_labels`); no
weights or `var_weight` -- LPO is precedence-only.

**Selector pattern** (KBO vs LPO on AtpState): surveyed three
options:
1. Sum type / discriminated union: type-safe, lots of churn
2. Two parallel inits: clean signatures, AtpState gets two
   parallel fields
3. **Add LPO field alongside KBO; selector by non-NULL**
   (chosen): minimal churn, mirrors `use_ic_cp_gen` /
   `use_ic_rewrite` direct-poke pattern; existing KBO callers
   untouched

**Migration plan**:
- 8.5b: `thvm_lpo` in `src/lpo/_.c` mirroring `src/kbo/_.c`
- 8.5c: extend AtpState with `const LpoConfig *lpo`; dispatch
  from `thvm_atp_orient_and_add`
- 8.5d: at least one bench fixture wired to use LPO directly
  (today most `.pr` files declare `ORDERING LPO` but the
  saturator silently maps to KBO -- the gap has been
  outstanding through stages 5-7)

Verification + stop conditions documented in the memo.

### Added: ICC sort dispatch resolution memo (stage 8.3d)

`docs/plans/icc_sort_dispatch.md` (~110 lines) closes 8.3d with
**no IC code changes**.  Analysis: the "wrong-sort dispatch"
intent is already satisfied by 8.4's infrastructure:

1. Entry-point gating (8.4d) rejects ill-sorted equations /
   goals before they reach saturation.
2. Closed-world inheritance: well-sorted entry + well-typed
   rules guarantees well-sorted CPs through the rewriting
   chain.
3. Head-symbol dispatch in `prim_rewrite_step` already
   discriminates correctly when signatures aren't overloaded.

ICC `TAG_BRI` / `TAG_ANN` (about lambda-calculus dependent
types) would only buy something with **operator overloading**
(same name, multiple sort signatures) or **SupGen-style sort
superposition** (stage 8.10) -- neither of which is in our v0
scope.  Both documented as follow-up research items.

The 8.3d task description allowed "rolling up under 8.4
(multi-sort) instead", which is what this resolution does.

Stage 8.3 is now fully closed (a-e, no more pending sub-items).

### Added: multi-sort `.pr` test fixture (stage 8.4e)

`tests/data/atp/list_length.pr` lands the first real multi-sort
fixture: two sorts (nat, list); five symbols (zero, succ, nil,
cons, len) with sort-discriminating signatures (`cons : nat list
-> list`, `len : list -> nat`); two FVR variables of distinct
sorts.  The conjecture `len(cons(zero, nil)) = succ(zero)`
proves in 1 step under the bench's saturator.

Companion `list_length.expect`: `status=PROVED, max_step=2,
max_rules=3`.

`build/bench-atp.csv` (16 rows -> 20 rows; 4 modes per fixture
across 5 fixtures now): the new fixture proves in 1 step under
all four CP-gen / rewrite paths with byte-identical counters,
adding additional coverage of the parser + saturation pipeline
on a non-homogeneous signature.

Note: 8.4d's gate is OFF in the bench harness (the harness
doesn't call `thvm_atp_set_spec`), so this fixture exercises
the parser + saturation pipeline on a multi-sort signature
without sort policing.  Wiring spec attachment into the bench
harness is a possible follow-up but not required by 8.4e's
"bench picks it up automatically" criterion.

Stage 8.4 is now complete (a-e shipped).  Closing it unblocks
**8.3d** (ICC TAG_BRI / TAG_ANN integration) per its design
memo.  Future perf work: sort-aware KBO and early CP-pair sort
precheck.

### Added: sort-check gating in saturation entry points (stage 8.4d)

`AtpState` gains a `const struct WaldSpec *spec` field (default
NULL) plus setter `thvm_atp_set_spec(s, spec)`.  When attached,
sort-check fires on the two saturation entry points:

- `thvm_atp_add_equation(s, lhs, rhs)`: rejects if either side
  is ill-sorted or `sort(lhs) != sort(rhs)`.  Returns 0 without
  mutating state.
- `thvm_atp_set_goal(s, lhs, rhs)`: same gate.  Special case:
  `lhs == 0` clears the goal and is always accepted (completion-
  mode idiom).  Return type changed from `void` to `u8` (1 =
  set, 0 = rejected).  Existing callers that ignore the return
  continue to work.

CPs are NOT prechecked per the design memo: well-sortedness
inherits from the source rules' LHSs through unification.

The gate is a no-op when no spec is attached (NULL) or when the
spec has `n_sorts == 0` (homogeneous mode), preserving the
previous behavior for tests that don't use sorts.

`tests/test_wald.c` adds 3 cases (5522 sub-checks, was 5507):
- `add-equation-rejects-mismatch`: nat-vs-list equation
  rejected; well-sorted nat-vs-nat accepted; n_cps unchanged
  on rejection
- `set-goal-rejects-mismatch`: nat-vs-list goal rejected; clear
  (lhs == 0) always accepted; previous goal preserved on
  rejection
- `no-spec-attached-passes-everything`: gate is a no-op when
  spec is NULL even on terms with unregistered labels / var ids

Header change: `WaldSpec` typedef now uses a tagged struct name
(`typedef struct WaldSpec { ... } WaldSpec`) so AtpState can
forward-reference it.

### Added: wald_term_sort / wald_sort_check (stage 8.4c)

Two new helpers in `src/wald/_.c`:

- `wald_term_sort(spec, t)`: top-down sort inference; returns the
  sort id of `t` if well-sorted, or `WALD_MAX_SORTS` (sentinel)
  on mismatch.  FVR looked up by var_id in `spec->vars[]`; CTR
  looked up by label in `spec->symbols[]`, arity must match,
  each child's inferred sort must equal the symbol's
  `arg_sorts[i]`.
- `wald_sort_check(spec, t)`: convenience predicate -- returns
  1 if well-sorted, 0 otherwise.

Homogeneous-mode shortcut: `spec == NULL` or `spec->n_sorts == 0`
returns sort 0 unconditionally so the existing single-sort
fixtures (which auto-register an "ANY" sort or have no SORTS
section) keep passing without sort policing.

`tests/test_wald.c` adds 5 cases (5507 sub-checks, was 5492):
- `homogeneous-mode-passes-everything`: n_sorts==0 returns 0
- `well-sorted-multi-sort`: nat/list signature; verifies
  `cons(zero, nil)` infers sort `list`; FVR `n` infers `nat`
- `sort-mismatch-detected`: `cons(nil, zero)` (args swapped)
  is rejected
- `unknown-symbol-fails`: CTR with unregistered label
- `unknown-fvr-fails`: FVR with unregistered var_id

8.4d will wire this into the saturation loop's add_equation
and set_goal entry points; CPs aren't pre-checked since
well-sortedness inherits from the source rules.

### Added: sort metadata on WaldSpec (stage 8.4b)

`src/thvm.h` extends WaldSpec with a sort table per
`docs/plans/multi_sort.md`'s Choice C:

- `WaldSpec.sorts[WALD_MAX_SORTS][WALD_NAME_LEN]` (16 sorts max,
  32 chars each) + `n_sorts` count.
- `WaldSym.arg_sorts[WALD_MAX_ARITY]` + `WaldSym.result_sort` --
  per-symbol sort indices, populated from SIGNATURE.
- `WaldVar.sort` -- per-variable sort index, populated from
  VARIABLES with comma-batch sharing (`x, y, z : nat` -> all
  three get sort `nat`).

New helper `wald_sort_id_or_register(spec, name, len)` looks up
a sort by name or registers a new entry if one isn't present.
Returns `WALD_MAX_SORTS` (sentinel) on overflow.  Used by
SORTS / SIGNATURE / VARIABLES parsers; sort ids stay stable
across sections.

Parser updates in `src/wald/_.c`:
- `wald_parse_sorts` now stores sort names (was discarding)
- `wald_parse_signature` captures arg/result sort ids per symbol
- `wald_parse_variables` tracks a comma-batch start so a single
  `: <sort>` post-comma applies to all variables in the batch

`tests/test_wald.c` adds 3 cases (5492 sub-checks, was 5458):
- `wald/sorts/empty-spec-has-no-sorts`
- `wald/sorts/register-and-lookup` (idempotent lookup)
- `wald/sorts/multi-sort-pr-fixture`: full nat/list signature
  with comma-batched variables; verifies all sort metadata

Backwards-compat path: a `.pr` file without an explicit SORTS
section still parses fine -- sorts get auto-registered lazily
during SIGNATURE / VARIABLES parsing.  `n_sorts == 0` after
parsing means "no sorts referenced anywhere," which 8.4d will
treat as homogeneous mode (no sort checking).

### Added: multi-sort signatures design memo (stage 8.4a)

`docs/plans/multi_sort.md` (~150 lines) lays out the design for
stage 8.4 (lifting the homogeneous-sort assumption that's held
through stages 1-7).

**Where do sorts live?**  Surveys three placements:
1. Parallel arrays on `WaldSpec` -- minimal struct churn, awkward
   to keep in sync
2. Embedded sort fields in `WaldSym` / `WaldVar` -- self-contained
   metadata, ~2 KB extra
3. **Hybrid** (chosen): sort name table on `WaldSpec`; per-symbol
   / per-variable sort *indices* embedded in metadata.  Mirrors
   how labels / var ids work today.

**Where does sort-checking fire?**  Surveys:
1. **Precheck via `wald_sort_check(spec, term)`** (chosen): top-
   down walk before equations / goals reach saturation.  Clean
   separation; saturation engine sees only well-sorted terms.
2. Threading sort logic through `thvm_match` / `thvm_unify`:
   significant API change with implications for every caller.
   Rejected.

**KBO / CP impact**: deferred.  Single-sort KBO is sound (just
less complete) on multi-sort signatures; CP enumeration's
unifier fails-late on sort-mismatched pairs without an explicit
precheck.  v0 accepts the conservative cost; sort-aware KBO is
future work.

**Migration plan**:
- 8.4b: implement choice C on data structures + parser
- 8.4c: `wald_sort_check` helper
- 8.4d: gate `thvm_atp_add_equation` / `thvm_atp_set_goal`;
  CPs not prechecked (well-sortedness inherits from source
  rules' LHSs)
- 8.4e: `nat_list.pr` fixture for the bench corpus

**Unblocks**: 8.3d (ICC integration) per its design memo;
sort-aware KBO / CP-precheck as follow-up perf items.

### Added: IC-rewrite vs C-rewrite bench cross-product (stage 8.3e-iii)

`tests/test_bench_atp.c` extended to a 2x2 cross product over
`(use_ic_cp_gen, use_ic_rewrite)`.  Mode label is two letters
(`cc`, `ci`, `ic`, `ii`) -- first character is cp-gen path, second
is rewrite path.  4 modes x 4 fixtures = 16 rows in
`build/bench-atp.csv`.

`thvm_free` + `thvm_init` between modes resets the heap so all
16 runs fit in the 16M-cell `HEAP_CAP`.

**Budget reduced from 256 to 32**: IC-rewrite allocates ~6 heap
cells per APP-PRI chain; 256 steps on the harder TIMEOUT
fixture overflows `HEAP_CAP`.  This is a real architectural
finding documented in `BENCH_STEP_BUDGET`'s comment:
production-scale IC-rewrite saturations need a heap-resetting
mechanism (today the heap is bump-allocated and only reclaimed
on `thvm_free`).

Results:

- All 4 modes produce **identical counters** per file (parity
  across both axes; confirms 8.1e-ii and 8.3e-ii's parity
  tests on the same corpus).
- IC-rewrite is **~2x slower** than C-rewrite on the TIMEOUT
  case (5.5 ms vs 2.4 ms); IC-cp-gen alone shows no
  significant overhead.  Within the 2x target.

**Decision**: both `use_ic_cp_gen` and `use_ic_rewrite` default
off.  IC paths are production-viable opt-in for SupGen-style
search (8.10) within their budget envelope.  A future
optimization (bump-pointer reset between saturation steps, or
a more compact PRI encoding) is the prerequisite for switching
defaults.

`docs/bench-atp.md` updated with the cross-product table and
analysis under a new "IC-rewrite vs C-rewrite" subsection.

Stage 8.3 is now complete (8.3a-c-e shipped; 8.3d blocked
pending 8.4 sorts).

### Added: IC-routed rewrite normalization (stage 8.3e-ii)

`atp_rewrite_normalize_ic` now actually routes per-rule matching
through the TAG_PRI machinery instead of delegating to the C
path:

- New helper `atp_ic_rewrite_try_top(t, lhs, rhs, n, &fired)`
  builds the saturated APP chain
  `APP(APP(APP(PRI(REWRITE_STEP), lhs_i), rhs_i), t)` per rule
  and reduces via `wnf`.  Returns the rewritten term on the
  first non-ERA result.
- New helper `atp_ic_rewrite_step` mirrors `thvm_rewrite_step`
  (try top, else descend left-to-right into CTR children) but
  uses `atp_ic_rewrite_try_top` for the top-position match.
- `atp_rewrite_normalize_ic` iterates `atp_ic_rewrite_step`
  until fixpoint, mirroring `thvm_rewrite_normalize`.

Same outermost-leftmost strategy as the C path; structurally
identical outputs verified by parity tests.

`tests/test_atp.c` (8386 sub-checks, was 8384):
- `atp/rewrite-flag-toggle-preserves-output` updated to assert
  the IC path now actually runs (not delegating)
- `atp/rewrite-ic-parity-on-group-axioms` new: full saturation
  on group axioms under both rewrite paths must agree on rst
  and n_rules

Stage 8.3e-iii will benchmark the IC overhead and decide on
the default.

### Added: `use_ic_rewrite` feature flag (stage 8.3e-i)

New `u8 use_ic_rewrite` field on `AtpState` (default 0) selects
between the C-direct and IC-routed rewrite-normalize paths.
A new shim `atp_rewrite_normalize(s, t, lhs, rhs, n, cap)`
dispatches:

- `use_ic_rewrite == 0`: `thvm_rewrite_normalize` (the existing
  C path)
- `use_ic_rewrite == 1`: `atp_rewrite_normalize_ic` (currently
  a no-op wrapper that delegates to the C path; 8.3e-ii will
  replace its body with PRI-routed dispatch via
  `prim_rewrite_step`)

All AtpState-internal direct callers of
`thvm_rewrite_normalize` updated to use the shim:
- `thvm_atp_step` (CP popping + normalize phase, 2 calls)
- `thvm_atp_goal_check` (goal lhs/rhs normalize, 2 calls)
- `thvm_atp_interreduce` (old-rule LHS reduction)
- `atp_cp_trivially_joinable` (7.1 filter, 2 calls)
- `atp_cp_source_disjoint_connected` (7.2b counter, 2 calls)

Tests in `tests/test_atp.c` (8384 sub-checks, was 8378):
- `atp/rewrite-flag-default-off`: fresh AtpState has flag 0
- `atp/rewrite-flag-toggle-preserves-output`: enabling the flag
  produces identical n_cps / n_rules / n_cps_dropped_joinable
  to the default path on the same input (the IC path is
  currently a delegate)

Stage 8.3e-ii will replace the IC path's body with PRI-routed
rewrite via `prim_rewrite_step`.

### Added: SUP-of-rules dispatch demo (stage 8.3c)

`tests/test_sup_rewrite.c` (23 sub-checks, 4 cases) demonstrates
that a SUP of `prim_rewrite_step` calls reduces to the same
rewrite outcomes as direct C-side `thvm_match` +
`thvm_subst_apply` calls would produce per rule.

Cases:
- `sup-rewrite/two-rules-one-applies`: rules `f(x, e) -> x`
  and `g(x) -> a`; target `f(b, e)`.  Rule 0 matches yielding
  `b`; rule 1 yields ERA.  SUP children's wnf outputs match
  direct C-side reference per rule.
- `sup-rewrite/two-rules-both-apply`: bare-FVR rules
  `x -> a` and `x -> b`.  Both match anything; outcomes are
  the rules' RHS unchanged.
- `sup-rewrite/three-rules-mixed`: nested SUP `&L_outer{r0,
  &L_inner{r1, r2}}` covers >2 rules.  Ref-checked per
  branch.
- `sup-rewrite/app-sup-fan-out-with-num-args`: exercises
  APP-SUP commutation directly with NUM args (DUP-friendly)
  to confirm the fan-out machinery operates with PRI children.

CP-shaped tests use the "fully-applied PRI inside the SUP"
encoding (`&L{APP(APP(APP(PRI(REWRITE), lhs_i), rhs_i),
target), ...}`) because APP-SUP cannot fan out CTR-shaped
args today (DUP-CTR not yet implemented).  The NUM test
confirms APP-SUP itself works on DUP-friendly args.  Caveat
documented in the test header; mirrors 8.1d-ii's pattern.

### Added: prim_rewrite_step IC dispatch primitive (stage 8.3b)

`prim_rewrite_step` (arity 3) registered at
`ATP_PRIM_REWRITE_STEP = 4` during `thvm_atp_init`.  Per
`docs/plans/ic_rule_dispatch.md`'s Strategy B: takes
`(lhs, rhs, target)`; runs `thvm_match`; on success returns
`thvm_subst_apply(rhs, &subst)`; on failure returns ERA.
Equivalent to one step of `thvm_rewrite_step` at the top
position, dispatched via APP-PRI evaluation.

`tests/test_rewrite_pri.c` (16 sub-checks, 6 cases):
- `rewrite-pri/direct-match`: `f(x, e) -> x` applied to
  `f(a, e)` -> CTR `a`
- `rewrite-pri/no-match-different-head`: target `g(a)` against
  `f(_, _)` LHS -> ERA
- `rewrite-pri/no-match-second-arg-mismatch`: target `f(a, b)`
  against `f(_, e)` LHS -> ERA
- `rewrite-pri/fvr-only-lhs-binds-anything`: bare `x` LHS
  matches any term
- `rewrite-pri/nested-ctr-binds-multiple-vars`:
  `f(g(x), y) -> g(y)` on `f(g(a), b)` -> `g(b)`
- `rewrite-pri/repeated-var-must-match-consistently`:
  `f(x, x)` matches `f(a, a)` but not `f(a, b)`

Combined with APP-SUP fan-out (8.3c) this lets a SUP of
partial-PRI rules dispatch in parallel against a single target
term.

### Added: IC-native rule dispatch design memo (stage 8.3a)

`docs/plans/ic_rule_dispatch.md` (~150 lines) lays out the
design for stage 8.3.  Identifies the **FVR-vs-VAR translation
problem**: our pattern variables are `TAG_FVR` atoms with
explicit ids, but `LAM` uses `TAG_VAR` binder slots that point
at heap cells.  Surveys three encoding strategies:

1. **Strategy A (literal LAM port)**: alpha-convert FVR to VAR
   via per-id binder cells, wrap in LAM chain.  Fails for nested
   patterns since the IC reducer doesn't naturally peel nested
   CTR.
2. **Strategy B (PRI dispatch)**: keep FVR, register a
   `prim_rewrite_step` (arity 3) at `ATP_PRIM_REWRITE_STEP = 4`
   that does `thvm_match` + `thvm_subst_apply`; SUP of partial
   PRIs handles fan-out via APP-SUP.  Faithful, simple, parity
   story is clear.
3. **Strategy C (hybrid)**: LAM only on outermost args; defer
   nested patterns to a primitive.  Fiddly, unclear win.

**Decision**: Strategy B for 8.3b-c.  Reinterprets the literal
"rule as LAM-binder" phrasing as "rule as a callable IC entity"
-- the LAM-vs-PRI choice is a means (IC dispatch), not an end.

8.3d (ICC TAG_BRI / TAG_ANN integration) deferred until 8.4
lands multi-sort signatures.  Without sorts, every rule applies
to any term and BRI/ANN wrapping is ceremony.

8.3e mirrors 8.1e: feature flag `use_ic_rewrite` swap of
`thvm_rewrite_step` under bench harness validation.

### Added: pure-IC kbo_eq via prim_kbo_eq_ic (stage 8.2c)

`prim_kbo_eq_ic` (arity 2) registered at `ATP_PRIM_KBO_EQ_IC = 3`
during `thvm_atp_init`.  Returns `NUM(0)` or `NUM(1)` depending
on structural equality of the two argument terms.  Implementation
splits on tag:

- Tag mismatch / ext mismatch -> immediate `NUM(0)`
- `TAG_FVR` with same ext -> `NUM(1)` (same variable id)
- `TAG_CTR` with arity 0 -> `NUM(1)`
- `TAG_CTR` with arity n > 0 -> **builds** an AND chain of n
  self-recursive APP-PRI calls and returns the unfired chain.
  The wnf reducer then evaluates the AND, firing each child
  comparison through APP-PRI saturation, short-circuiting on
  the first NUM(0).
- Other tags -> compare `term_val` directly

This is "IC-driven structural recursion with C base cases" --
the design memo's option (2) at minimum scope, demonstrating
that recursive structural code runs through our reducer
end-to-end.

`tests/test_kbo_pri.c` adds 9 cases (35 sub-checks total, was
17): leaf FVR same-id / different-id / tag mismatch, nullary
CTR same-label / different-label, binary CTR (equal, first
child differs, second child differs), nested CTR 3-level
recursion.

Stage 8.2c closes; 8.2d (full pure-IC port of `thvm_kbo`)
remains deferred until SupGen-style search (8.10) creates a
concrete use case.

### Added: thvm_kbo as TAG_PRI primitive (stage 8.2b)

`prim_kbo` (arity 3) registered at `ATP_PRIM_KBO = 2` during
`thvm_atp_init`.  Takes `(s, t, cfg_id_NUM)`; resolves cfg_id
via the new process-global `KBO_CFG_TABLE` (cap 16); calls
`thvm_kbo(s, t, cfg)`; returns `NUM(KboCmp)` (0=EQ, 1=GT, 2=LT,
3=UN per the existing enum), or `ERA` if cfg_id is bogus / no
config registered / cid arg is not a NUM.

New API in `src/thvm.h`:
- `u32 kbo_cfg_register(u32 cfg_id, const KboConfig *cfg);`
- `const KboConfig *kbo_cfg_get(u32 cfg_id);`
- `#define KBO_CFG_TABLE_CAP 16`
- `#define ATP_PRIM_KBO 2u`

`tests/test_kbo_pri.c` (17 sub-checks, 6 cases):
- `kbo-pri/registry-roundtrip`: register/get + out-of-range
- `kbo-pri/eq-outcome`: identical terms -> NUM(KBO_EQ),
  parity-checked vs direct C call
- `kbo-pri/gt-outcome`: `f(x, e) > x` -> NUM(KBO_GT)
- `kbo-pri/lt-outcome`: mirror image -> NUM(KBO_LT)
- `kbo-pri/un-outcome`: `f(x, y)` vs `f(y, x)` -> NUM(KBO_UN)
- `kbo-pri/unregistered-cfg-falls-through-to-ERA`: bogus cfg_id

This unblocks 8.10 (SupGen-style search) to invoke KBO from
inside an APP-PRI evaluation chain -- the minimum useful
increment per `docs/plans/kbo_ic_design.md`.

### Added: KBO-as-IC encoding design memo (stage 8.2a)

`docs/plans/kbo_ic_design.md` (~150 lines) lands the design
sketch for stage 8.2.  Surveys three encoding options:

1. **TAG_PRI wrapper** (~50 LOC): registers `thvm_kbo` as a
   primitive callable from IC; mirrors 8.1c's
   `prim_unify_apply`.  Unblocks 8.10 to invoke KBO from inside
   a SUP-encoded search.  Minimum useful increment.
2. **Hybrid IC structural recursion + C arithmetic primitives**
   (~200 LOC): structural recursion in IC, weights and counts in
   C as TAG_PRI callbacks.  Lets 8.10 superpose alternative
   ordering structures without porting arithmetic to IC.
3. **Full pure IC** (~500-1000 LOC): everything in IC, including
   Church-numeral or TAG_NUM weights and IC-encoded variable
   counts.  Research target; lets 8.10 superpose KboConfigs
   themselves.

Decision:
- 8.2b implements (1) immediately -- bounded scope, ports cleanly
  from `prim_unify_apply`.
- 8.2c implements a sliver of (2): pure-IC `kbo_eq` (the
  structural-equality sub-routine) as a proof point that IC-
  driven recursion is viable in our codebase.
- 8.2d (full pure IC) deferred until SupGen-style search (8.10)
  materializes and creates a concrete use case that pays for
  the engineering cost.

The memo also documents:
- The KboConfig registry pattern: `KBO_CFG_TABLE[16]` keyed by
  a u32 id, since `KboConfig*` doesn't fit cleanly in a Term's
  `val` field.
- An `prim_kbo` sketch (arity 3: `(s, t, cfg_id_NUM)` -> NUM
  encoding of `KboCmp`).
- Parity-test verification plans for each subtask.

### Added: IC vs C path bench comparison (stage 8.1e-iii)

`tests/test_bench_atp.c` now runs each `.pr` fixture under both
CP-gen modes (C-direct + IC-routed) and emits one CSV row per
`(file, mode)` pair into `build/bench-atp.csv`.  New `mode`
column joins the existing schema.

Results on the 4-fixture corpus, darwin/arm64, single run:

- All counters (step, n_rules, n_trace, all four
  `n_cps_dropped_*`) are **byte-identical** between C and IC
  paths.  Empirically confirms 8.1e-ii's parity claim.
- Wall-clock: IC is within run-to-run noise of C on every
  fixture.  On the TIMEOUT case (`group_commutative_inverse.pr`)
  C=132 ms, IC=119 ms; both well within the 2x target.

Hypothesis: CP-gen time is dominated by the position walk +
unification itself; the IC wrapper (APP-PRI accumulation, wnf
reduction) adds only a small constant per call.  σ is
recomputed twice per CP through `prim_unify_apply3`, which is
wasteful but cheap on small problems.

**Decision**: `use_ic_cp_gen` default stays off (C path is more-
tested).  IC is production-viable opt-in for SupGen-style search
(8.10).  If larger TPTP-UEQ problems show >2x slowdown, the
single-σ primitive idea (return CTR-pair of (σ(replaced),
σ(ri))) is the obvious mitigation.

`docs/bench-atp.md` updated with the full comparison table and
analysis under a new "IC path vs C path" subsection.

Stage 8.1 -- SUP-encoded CP enumeration via TAG_PRI unify --
is now complete.

### Added: IC-routed CP enumeration (stage 8.1e-ii)

`thvm_atp_generate_cps_ic` now actually routes the per-position
unify+apply step through the TAG_PRI machinery instead of
delegating to the C path:

- New primitive `prim_unify_apply3` (arity 3) registered at id
  `ATP_PRIM_UNIFY_APPLY3 = 1`.  Takes `(s, t, target)`; returns
  `thvm_unify_apply(target, &σ)` where `σ = mgu(s, t)`, or `ERA`
  on unify failure.
- New helper `ic_unify_apply3(s, t, target)` builds the saturated
  APP chain `APP(APP(APP(PRI(1), s), t), target)` and reduces it
  via `wnf`.  Each invocation flows through APP-PRI accumulation
  and saturated-call dispatch.
- New visitor `cp_visit_ic` (mirrors `cp_visit` from
  `src/cp/_.c`) routes both `σ(replaced)` and `σ(ri)` calls
  through `ic_unify_apply3`.  Recomputes σ once per side
  (wasteful but correct -- 8.1e-iii will measure).
- `thvm_atp_generate_cps_ic` reuses the C-side
  `cp_walk_positions` for the (i, j, position) enumeration but
  feeds it the IC-routed visitor.

Same iteration pattern as the C path; structurally identical
output verified by parity tests.

`tests/test_atp.c` (8378 sub-checks, was 8376):
- `cp-gen-flag-toggle-preserves-output` updated to assert the
  IC path now actually runs (not delegating)
- `cp-gen-ic-parity-on-group-axioms` new: full saturation on
  the group axioms under both paths must agree on rst and
  n_rules

Stage 8.1e-iii will benchmark the IC overhead and decide on
the default.

### Added: `use_ic_cp_gen` feature flag (stage 8.1e-i)

New `u8 use_ic_cp_gen` field on `AtpState` (default 0) selects
between the C-direct and IC-routed critical-pair enumerators.
`thvm_atp_generate_cps` now dispatches:

- `use_ic_cp_gen == 0`: `thvm_atp_generate_cps_c` (renamed body
  of the previous implementation; the C-direct path)
- `use_ic_cp_gen == 1`: `thvm_atp_generate_cps_ic` (currently a
  no-op wrapper that delegates to the C path; 8.1e-ii will land
  the actual SUP+PRI routing)

Tests in `tests/test_atp.c`:
- `atp/cp-gen-flag-default-off`: fresh AtpState has flag 0
- `atp/cp-gen-flag-toggle-preserves-output`: enabling the flag
  must produce identical n_cps / n_rules / n_cps_dropped_joinable
  to the default path on the same input (since the IC path is
  currently a delegate)

`tests/test_atp.c`: 8376 sub-checks (was 8370).  Stage 8.1e-ii
will replace the IC path's body with PRI-routed unification.

### Added: SUP-encoded CP fan-out demo (stage 8.1d-ii)

`tests/test_sup_cps.c` (21 sub-checks, 4 cases) demonstrates that
a SUP of `prim_unify_apply` calls reduces to the same terms as
direct C-side `thvm_unify_apply` would produce for each pair --
the structural parity check from `docs/plans/sup_encoded_cps.md`:

- `sup-cps/two-positions-both-unify`: `&L{(f(x), f(a)),
  (g(y), g(b))}` -- both branches unify; child wnfs match
  reference `thvm_unify_apply` outputs term-by-term
- `sup-cps/one-unifies-one-fails`: mixed -- one branch yields
  CTR, the other ERA
- `sup-cps/three-positions-mixed`: nested SUP `&L_outer{p1,
  &L_inner{p2, p3}}` -- demonstrates the encoding scales beyond
  binary
- `sup-cps/app-sup-fan-out-with-num-args`: exercises APP-SUP
  commutation directly with NUM args (DUP-NUM is implemented;
  DUP-CTR isn't yet) -- a `&L{PRI(40), PRI(40)}` applied to
  NUM(11) fans out and each branch's identity primitive
  returns 11

The CP-shaped tests use the "fully-applied PRI inside the SUP"
encoding (`&L{APP(APP(PRI_unify, s_i), t_i), ...}`) because
APP-SUP cannot fan out CTR-shaped args today (no DUP-CTR).
The 4th test confirms APP-SUP itself works on DUP-friendly args.

Stage 8.1d closes; the design memo's parity claim is now
empirically verified at the 2-3 position scope.

### Added: APP-SUP commutation (stage 8.1d-i)

`src/interact/app_sup.c` lands the standard HVM4 rule:

```
APP(&L{f, g}, arg)
------------------ APP-SUP
&L{ APP(f, arg_0), APP(g, arg_1) }   where ! &L{arg_0, arg_1} = arg
```

Allocates a 7-cell block: shared DUP body for `arg`, two APP slots
referencing the DUP via DP0/DP1, two SUP children pointing at the
APPs.  Wired into the WNF dispatch in `src/wnf/_.c` and
`src/wnf/redex.c` next to APP-LAM / APP-BRI / APP-PRI.

Foundational interaction; not 8.1-specific but blocks 8.1d-ii
(SUP-encoded CP fan-out).

`tests/test_app_sup.c` (16 sub-checks, 5 cases): single-fanout
with PRI children, label preservation, asymmetric children
(PRI vs ERA), ERA arg fan-out, ITRS counter increment.

Caveat documented in the test header: APP-SUP shares the arg
via a DUP, which fires only for tags with DUP-* interactions.
Today our IC has `DUP-{ERA, LAM, NUM, SUP, BRI, ANY}`; CTR and
FVR remain passive.  8.1d-ii will route CP enumeration around
this (pass the rule pair through the SUP rather than as the
APP arg) or land DUP-CTR as a separate task.

### Added: ATP unification as a TAG_PRI primitive (stage 8.1c)

`src/atp/_.c` registers `prim_unify_apply` (arity 2) at id
`ATP_PRIM_UNIFY_APPLY = 0` during `thvm_atp_init`.  The
primitive takes two terms `(s, t)`, runs `thvm_unify`; on
success returns `thvm_unify_apply(s, &subst)` (the unified
term), on failure returns `ERA` so the surrounding APP-PRI
structure short-circuits cleanly via APP-ERA when consumed by
SUP-encoded CP enumeration in 8.1d.

`tests/test_pri.c` adds 3 round-trip cases (now 25 sub-checks
total):
- `pri/unify-apply/var-ctr`: `(f(x), f(a))` -> `f(a)`
- `pri/unify-apply/incompatible-ctrs-give-ERA`:
  `(f(x), g(y))` -> `ERA`
- `pri/unify-apply/identical-vars-trivial-success`:
  `(x, x)` -> `x`

`atp_register_primitives` is idempotent (registry overwrites
with the same fn pointer), so multiple `thvm_atp_init` calls
do not double-register.

### Added: TAG_PRI primitive function call (stage 8.1b)

New IC tag `TAG_PRI = 25` (HVM4 port) lands a "primitive function
call" mechanism: a PRI carries a `prim_id` (u32, in EXT) into a
process-global registry mapping id -> `(PrimFn, arity)`.  APP-PRI
accumulates args into a heap cell `[NUM(count), arg_0, ...]` until
`count == arity`, at which point the registered C function is
called and its return Term replaces the redex.

New API in `src/thvm.h`:
- `typedef Term (*PrimFn)(Term *args);`
- `Term term_new_pri(u32 prim_id);`
- `u32 prim_register(u32 prim_id, PrimFn func, u32 arity);`
- `PrimFn prim_fun(u32 prim_id);`
- `u32 prim_arity(u32 prim_id);`
- `#define PRIM_TABLE_CAP 64`

`TAG_COUNT` bumped to 26.  `tests/test_tensor.c` updated.

New files:
- `src/term/new_pri.c` -- constructor + registry storage
- `src/interact/app_pri.c` -- APP-PRI accumulation + saturation

WNF dispatch wired in `src/wnf/_.c` and `src/wnf/redex.c`.

`tests/test_pri.c` (18 sub-checks) covers:
- Tag + ext layout on a fresh PRI
- Registry roundtrip + out-of-range cleanup
- Arity-1 immediate fire (identity)
- Arity-2 partial-then-saturate (pair-CTR builder)
- Arity-3 saturation across 3 APPs
- Unregistered prim_id falls through to ERA defensively

Stage 8.1c will register `thvm_unify` as the first real primitive.

### Added: SUP-encoded CP enumeration design memo (stage 8.1a)

`docs/plans/sup_encoded_cps.md` (~200 lines) lands the design
sketch for stage 8.1.  Surveys HVM4's `TAG_PRI` reference
implementation (registry table mapping `id -> (PrimFn, arity)`,
APP-PRI partial-application interaction); spells out the
SUP-cross-product encoding for the
`outer_rule x inner_rule x overlap_position` triple via three
labeled SUPs and APP-SUP commutation; analyzes feasibility
and concludes labeled SUPs suffice (8.6 unordered SUPs are an
optimization, not a prerequisite).

Migration target documented: `thvm_unify`, `thvm_match`,
`thvm_subst_apply`, `kbo_eq` stay in C as `TAG_PRI` callbacks;
`thvm_critical_pairs_range` and `atp_push_cps_traced`'s loop
move to IC.

Decision: 8.1 unblocks 8.10 (SupGen-style search) -- 8.10
needs CPs reified as SUP entries to superpose the
"which next CP" choice.  Implementation order: 8.1 then 8.10.

Stop conditions for the implementation subtasks: revert to
8.4 / 8.5 if `TAG_PRI` integration runs into the existing
stack-machine reducer; treat 8.1 as research infrastructure
(not a perf win) if IC enumeration is structurally correct
but asymptotically slower.

### Added: Twee comparison harness `tools/bench_twee.c` (stage 7.4d)

`tools/bench_twee.c` parses each `.pr` in `tests/data/atp/` via
`wald_parse_file`, emits a TPTP-CNF representation
(`cnf(eqn<i>, axiom, lhs = rhs).` for axioms,
`cnf(goal, negated_conjecture, lhs != rhs).` for the goal) into
`build/bench_twee_<i>.tptp`, then invokes
`twee --quiet --no-proof --max-cps 256 <tptp>` matching our own
budget.  Wall-clock + Twee's status are written to
`build/bench-twee.csv`.

`make bench-twee` builds and runs the comparison; not part of
`make test` (Twee is an external dependency, install via
`cabal install twee`).

Twee 2.6.1 was successfully installed via `cabal install twee`
on this host (darwin/arm64 + GHC 9.12.1; cabal warned about
GHC version but compile succeeded for all 20 transitive
dependencies including `twee-lib`, `jukebox`, `minisat`).

First comparison numbers, 2026-04-26:

| File | Twee | thvm |
|---|---|---|
| `group_commutative_inverse.pr` | PROVED 26.1 ms | TIMEOUT 132.7 ms |
| `group_right_inverse_to_e.pr` | PROVED 26.7 ms | PROVED 0.006 ms |
| `idempotent_nested.pr` | PROVED 24.2 ms | PROVED 0.001 ms |
| `monoid_right_id.pr` | PROVED 30.0 ms | PROVED 0.001 ms |

Twee proves all four; we prove three out of four.  The harder
group-commutativity goal isolates a real gap (LPO + better
heuristic).  For the easy goals our IC-native ATP wins on
latency by 3-4 orders of magnitude (no process spawn, no TPTP
parse, no warm-up), but Twee wins on hard-saturation
throughput.  `docs/bench-atp.md` records the full table and
observations.

Stage 7.4 complete; stage 7 (Twee-class redundancy criteria
and benchmarking) closed.

### Added: ATP bench harness `test_bench_atp` (stage 7.4c)

`tests/test_bench_atp.c` walks `tests/data/atp/*.pr`, runs our
ATP on each with a fixed step budget (256), times via
`clock_gettime(CLOCK_MONOTONIC)`, and writes per-file rows to
`build/bench-atp.csv` with columns:
`file,status,wall_ms,step,n_rules,n_trace,drop_joinable,
drop_connected,drop_rule_subsumed,drop_queue_subsumed`.

Soft regression: only the final ATP `status` is asserted against
the matching `.expect` file (so PROVED <-> TIMEOUT swaps fail
the test); step / rule / counter values are recorded but not
gated -- the bench is a measurement, not a regression.

Wired into the standard `TESTS` list so `make test` rebuilds and
runs it; `make` exit code stays 0 on numeric drift.  CSV is
regenerated on every run.

`docs/bench-atp.md` now points at the harness for re-runs and
records the full 4-row results table sourced from
`build/bench-atp.csv`.

### Added: `.pr` test corpus for ATP bench (stage 7.4b)

Four small group-flavored `.pr` fixtures land under
`tests/data/atp/`, each paired with a `.expect` companion that
records the empirically-observed outcome (status + advisory
upper bounds on step / rule count):

- `group_right_inverse_to_e.pr` -- group axioms, conclude
  `f(a, i(a)) = e` (direct rewrite). Expected: PROVED in 1
  step, 2 rules.
- `group_commutative_inverse.pr` -- group axioms, conclude
  `f(a, i(a)) = f(i(a), a)` (commutativity-of-inverse-on-
  element; same as `waldmeister/documents/example.pr`).
  Expected: TIMEOUT at 256 steps under the current KBO config.
- `monoid_right_id.pr` -- monoid (assoc + right-id, no
  inverse), conclude `f(a, e) = a`. Expected: PROVED in 0
  steps (closes via direct goal-rewrite).
- `idempotent_nested.pr` -- pure idempotent rule
  `f(x, x) = x`, conclude `f(a, f(a, a)) = a`. Expected:
  PROVED in 0 steps via the recursive rewriter.

`.expect` format: simple `key=value` pairs with `%`-prefixed
comments (matching the `.pr` lexer's syntax).  Recognized keys:
`status`, `max_step`, `max_rules`. Stage 7.4c will add a bench
harness that consumes these.

### Added: ATP benchmark log skeleton (stage 7.4a)

`docs/bench-atp.md` lands the methodology + first results table
for our IC-native ATP. Records 8 metrics per run (status,
wall-clock ms, saturation step count, rule-set size, trace
length, and the four `n_cps_dropped_*` counters from 7.1/7.2b/
7.3a/7.3b) on two starting cases:

| File | Status | Wall (ms) | Steps |
|---|---|---|---|
| simple goal `f(a, i(a)) = e` | PROVED | 0.007 | 1 |
| `waldmeister/documents/example.pr` (`f(a, i(a)) = f(i(a), a)`) | TIMEOUT | 130.765 | 256 |

Confirms (a) the simple goal closes in step 1 via direct rewrite,
(b) the harder commutativity-of-inverse goal needs more than 256
steps under the current KBO config (~52% of generated CPs are
trivially joinable; 231 rules accumulated without proving),
(c) the domination invariants from 7.2b / 7.3a hold empirically
(`drop_connected` 697 <= `drop_joinable` 743;
`drop_rule_subsumed` 212 <= 743), and (d) 7.3b's queue-
subsumption fires rarely (2 hits) on the group example.

Twee comparison deferred to 7.4d (Twee not installed locally).
Bench harness deferred to 7.4c.

### Added: queue-subsumption filter (stage 7.3b)

`src/atp/_.c` gains `atp_cp_queue_subsumed(s, lhs, rhs)`: returns 1
if the candidate `(lhs, rhs)` is a substitution instance of some
already-queued CP `(s->cp_lhs[k], s->cp_rhs[k])` -- i.e., there is
σ such that `(lhs, rhs) = (σs', σt')` (forward) or `(σt', σs')`
(symmetric).

Genuinely orthogonal to 7.1: the queue does not participate in
`thvm_rewrite_normalize`, so the queue-subsumption check can fire
on CPs that 7.1 misses (and vice versa).  Wired as a real FILTER
in `atp_push_cps_traced`: candidate is dropped, `n_cps_dropped_
queue_subsumed` ticks, queue does not grow.

`tests/test_atp.c` adds 5 cases:
- `cp-queue-subsumed-direct-instance`: forward direction fires
- `cp-queue-subsumed-symmetric-instance`: symmetric direction fires
- `cp-queue-subsumed-empty-queue-no-fire`: nothing to subsume
  against
- `cp-queue-subsumed-non-instance-no-fire`: non-instance does not
  fire
- `cp-queue-subsumed-filter-drops-instance`: end-to-end filter
  test via `atp_push_cps_traced`

Stage 7.3 is now complete.

### Added: rule-subsumption counter (stage 7.3a)

`src/atp/_.c` gains `atp_cp_rule_subsumed(s, lhs, rhs)`: returns 1
if there exist `(l, r) ∈ R` and substitution σ such that
`(lhs, rhs) = (σl, σr)` (forward) or `(σr, σl)` (symmetric).
Equational subsumption: σ is consistent across both sides
(extended through both `thvm_match` calls on the same
`RewriteSubst`).

Per the same domination argument as 7.2b: rule-subsumption fires
only when an existing rule rewrites lhs to rhs in one step under σ,
which 7.1's full-R normalize also catches.  The counter
`n_cps_dropped_rule_subsumed` ticks unconditionally for empirical
measurement and is bounded above by `n_cps_dropped_joinable`.

`tests/test_atp.c` adds 4 cases:
- `cp-rule-subsumed-direct-instance`: forward direction fires
- `cp-rule-subsumed-symmetric-instance`: symmetric direction fires
- `cp-rule-subsumed-non-instance-no-fire`: non-instance does not fire
- `cp-rule-subsumed-domination-on-saturation`: invariant holds on the
  group example

Stage 7.3b will add queue subsumption -- which IS orthogonal to
7.1 and adds genuine new pruning.

### Added: source-rule-disjoint connectedness counter (stage 7.2b)

`src/atp/_.c` gains `atp_cp_source_disjoint_connected(s, lhs, rhs,
rule_a, rule_b)`: returns 1 if `(lhs, rhs)` is joinable under
`R \ {rule_a, rule_b}` (the two rules that birthed the CP via
overlap unification).  Implementation: builds a filtered rule
array excluding `rule_a` and `rule_b`, normalizes both sides
under it, compares via `kbo_eq`.

`atp_push_cps_traced` signature extended with `(rule_a, rule_b)`;
calls 7.2b's check alongside 7.1's.  New `n_cps_dropped_connected`
field on `AtpState` ticks unconditionally for measurement (does
not gate dropping -- that remains 7.1's job).  Per the domination
lemma in `connectedness_design.md`, the connected count is bounded
above by the joinable count.

Sentinel: passing `ATP_MAX_RULES` for either `rule_a` or `rule_b`
means "exclude no rule," making the function fall through to
trivial-joinability semantics.

`tests/test_atp.c` adds 4 cases (all in the same TEST_BEGIN
group as 7.1's filter tests):
- `cp-connectedness-counter-on-self-overlap`: domination
  invariant holds on self-overlap
- `cp-connectedness-genuine-CP-not-dropped`: hand-constructed
  CP `(a, e)` survives both filters when neither parent rule
  helps the join
- `cp-connectedness-empty-filter-falls-through`: sentinel
  exclusion makes the connectedness check equivalent to
  trivial-joinability
- `cp-connectedness-domination-on-saturation`: empirical
  confirmation on the group example: connected count <= joinable
  count throughout

Stage 7.2 is now complete.

### Added: connectedness redundancy design memo (stage 7.2a)

`docs/plans/connectedness_design.md` (~150 lines): surveys three
candidate Bachmair-Dershowitz-Plaisted (BDP) connectedness
criteria -- subsumption-connected, source-rule-disjoint connected,
"connected below c" -- and proves a *domination lemma*: any rule
subset `R' ⊆ R` cannot find joins that `R` itself cannot find, so
all three candidates are strictly dominated by 7.1's trivial-
joinability filter (which uses full R).

Decision: implement criterion (2), source-rule-disjoint
connectedness, in 7.2b *as an empirical demonstration* of the
domination relationship rather than as a new pruning mechanism.
The resulting `n_cps_dropped_connected` counter is expected to be
a strict lower bound on `n_cps_dropped_joinable`; this is useful
infrastructure for stage 7.4+ when AC theories or extended
joinability tests can break the domination.

The memo also recommends prioritizing stage 7.3 (subsumption)
since it is genuinely orthogonal to 7.1 -- subsumption can fire
on CPs that are not joinable, e.g. instances of an existing rule
that have not been reduced.

### Added: trivial-joinability CP filter (stage 7.1)

Drops critical pairs that are joinable-by-current-R at generate time
rather than letting them flow through the queue and orient pipeline.
This is the simplest version of Waldmeister's `Grundzusammenfuehrung`
("ground-merging") criterion -- equivalent to Twee's
"joinable-by-current-R" pruning.

Implementation:
- `static u8 atp_cp_trivially_joinable(s, lhs, rhs)` in
  `src/atp/_.c` -- normalizes both sides under R via
  `thvm_rewrite_normalize` (NORM_CAP=64) and returns
  `kbo_eq(l, r)`.
- `atp_push_cps_traced` calls it before pushing; on hit, bumps
  `n_cps_dropped_joinable` and skips both the trace push and the
  queue push.
- New `u32 n_cps_dropped_joinable` field in `AtpState` records
  the count for benchmarking.

Behavior change: a single rule's self-overlap CP is always
trivially joinable, so `generate_cps` now returns 0 in that case
(previously it pushed the CP to the queue, where step would drop
it after popping).  Updated tests:
- `atp/generate-cps-single-rule-self-overlap`: pushed == 0, counter
  ticks
- `atp/generate-cps-old-times-new-direction`: pushed == 0, counter
  ticks (assoc + left-id overlaps are all joinable)
- `atp/trace-cp-records-source-rules-as-parents`: rebuilt with two
  non-confluent rules so a non-joinable CP survives
- `atp/trace-serialize-orient-with-parent`: drops the now-absent
  "(cp from N, M):" assertion

New test cases:
- `atp/cp-joinability-filter-self-overlap-counter`
- `atp/cp-joinability-filter-survives-non-joinable`
- `atp/cp-joinability-filter-counter-on-saturation`

Stronger criteria (ground-joinability over a sample of
substitutions, AC-aware joinability) are deferred to 7.2+.

### Added: PCL DAG well-formedness cross-check (stage 6.4c)

`tests/test_wald.c` adds `wald/example.pr/pcl-dag-well-formed`,
which structurally cross-checks our trace against Waldmeister's
PCL format (sources/INF/pcl.c).

Documented format mapping:
- `tes-eqn  : <l> = <r> : initial`             <-> our `(axiom)`
- `tes-rule : <l> -> <r> : orient(<src>,<d>)`  <-> our `(orient from N)`
- `tes-eqn  : <l> = <r> : cp(<a>,<pa>,<b>,<pb>)` <-> our `(cp from A, B)`

Known gaps (deferred):
- We render `tes-rule` with `=` rather than `->`
- We don't carry CP overlap positions in the trace
- No `tes-final` line on proof close (implicit in run-status)

What we DO match (verified by walking `atp->trace[]` directly):
- First `n_eqns` trace entries are `TRACE_AXIOM` with no parents
- Every subsequent orient/cp entry has every parent id strictly
  less than its own id (DAG well-formed -- Waldmeister relies on
  the same invariant for PCL replay)
- Total entry count = axioms + orients + cps (unaccounted reasons
  fail the test)

`tests/test_wald.c`: 2420 sub-checks.

### Added: end-to-end .pr -> saturation -> PCL trace test (stage 6.4b)

`tests/test_wald.c` adds a `wald/example.pr/end-to-end-pcl-trace`
case (248 sub-checks total) that closes the loop:

1. `wald_parse_file("waldmeister/documents/example.pr", spec)`
2. Build `KboConfig` from `spec->symbols[i].prec_rank`
3. `thvm_atp_init`, push 3 axioms, set goal `f(a, i(a)) = f(i(a), a)`
4. `thvm_atp_run` (256-step budget)
5. `thvm_atp_trace_serialize` into 8 KB buffer

Structural assertions (hold whether the goal is proved or the
budget is exhausted -- the example.pr conclusion needs left-inverse
derived from right-inverse + associativity + identity, which is
beyond what we can guarantee in a small budget):
- `n_trace >= n_eqns` (each axiom is recorded)
- trace text contains "0 (axiom): ", "1 (axiom): ", "2 (axiom): "
- at least one "(orient from " line exists
- final `AtpStatus` is one of PROVED / TIMEOUT / QUEUE_EMPTY

The test silently passes if the `waldmeister/` symlink isn't
present (matches the 6.4a fallback).

### Added: `wald_parse_file` -- file-loader convenience wrapper (stage 6.4a)

Thin wrapper in `src/wald/_.c`: opens `path`, slurps the bytes,
calls `wald_parse`, frees the buffer.  New error code
`WALD_ERR_FILE = 3` covers open/read/alloc failure.

API: `WaldErr wald_parse_file(const char *path, WaldSpec *spec)`.

`tests/test_wald.c` adds 4 cases (236 sub-checks total):
- `wald/parse-file/null-path` -> `WALD_ERR_NULL`
- `wald/parse-file/null-spec` -> `WALD_ERR_NULL`
- `wald/parse-file/missing-file` -> `WALD_ERR_FILE`
- `wald/parse-file/example.pr-from-disk` -- loads
  `waldmeister/documents/example.pr` via the vendored-tree
  symlink.  Asserts spec identity (`name == "group"`,
  `mode_proof == 1`, 4 symbols, 3 vars, 3 axioms, goal
  populated).  If the symlink is absent (`WALD_ERR_FILE`),
  the test still passes -- this is a research fixture, not a
  regression test.

### Added: Waldmeister .pr parser feeds saturation end-to-end (stage 6.3g)

Two new test cases in `tests/test_wald.c` close the loop between the
`.pr` parser (stages 6.3a-f) and the saturation engine + KBO comparator:

- `wald/parsed-axioms-kbo-orient-correctly` -- parses the full group
  spec, builds a `KboConfig` from `spec->symbols[i].prec_rank` (with a
  +1 shift so `prec_rank == 0` still gets a positive precedence), and
  asserts each of the 3 parsed axioms orients `KBO_GT` under that
  config.
- `wald/parsed-spec-feeds-saturation-and-proves` -- end-to-end pipeline:
  parse `.pr` source, build `KboConfig` from parsed precedences,
  `thvm_atp_init`, push parsed axioms via `thvm_atp_add_equation`,
  `thvm_atp_set_goal` from parsed conclusion, `thvm_atp_run`, and
  assert `ATP_PROVED` within a small step budget.

This verifies that the parser output is structurally compatible with
the saturation engine without an explicit conversion layer -- the
`Term` values it produces (TAG_CTR / TAG_FVR with the right symbol IDs)
flow straight into the engine.

`tests/test_wald.c`: 226 sub-checks (was 214).

### Added: top-level Waldmeister .pr parser driver (stage 6.3f)

`wald_parse(src, spec) -> WaldErr` lands in `src/wald/_.c`.
Lexes the source, finds the first section keyword via
`wald_skip_to_section`, then dispatches each section to its
parser (NAME / MODE / SORTS / SIGNATURE / VARIABLES / ORDERING /
EQUATIONS / CONCLUSION).  Each parser returns the next section's
enum so the driver just chains.

Sections are accepted in any order -- Waldmeister's grammar
specifies a fixed order, but the parser is permissive (matching
upstream behavior and easing test fixture writing).  Per-section
parse errors don't bail the driver: the section parser falls
through to `wald_skip_to_section` so we still consume the rest
of the file and produce a partial-but-coherent spec.

`WaldErr` enum: `WALD_OK`, `WALD_ERR_NULL`, `WALD_ERR_NO_SECTION`.

Tests in `tests/test_wald.c` (214 sub-checks) include:
- NULL args -> `WALD_ERR_NULL`
- empty source -> `WALD_ERR_NO_SECTION`
- "foo bar baz" with no recognized keyword -> `WALD_ERR_NO_SECTION`
- the full group-axiom `.pr` file from
  `waldmeister/documents/example.pr` parses end-to-end:
  - `name == "group"`, `mode_proof == 1`
  - 4 symbols (e/i/f/a) with arities 0/1/2/0 and the
    monotonic CTR labels
  - precedence ranks i=3, f=2, e=1, a=0 from the LPO section
  - 3 variables (x/y/z) with sequential FVR ids
  - 3 axioms in `eqn_lhs/rhs[]`
  - goal_lhs is `f(...)` (label of f), goal_rhs is `e`

This unblocks 6.3g (named end-to-end test) and 6.4 (run
saturation on the parsed spec, emit a PCL trace).

### Added: EQUATIONS + CONCLUSION parsers (stage 6.3e)

`wald_parse_equations` and `wald_parse_conclusion` land in
`src/wald/_.c`.  Both share a small `wald_parse_equation_pair`
helper that reads `term "=" term`, returning 1 on success.

EQUATIONS appends each pair to `spec->eqn_lhs/rhs[]` (capped at
`WALD_MAX_EQNS`).  CONCLUSION writes only the FIRST pair into
`goal_lhs/goal_rhs`; subsequent pairs in the same section are
parsed (so the section terminates correctly) but discarded.
This matches the proof-mode constraint of one conjecture per
spec; multi-conclusion is a 8.x revisit.

Both share the same recovery pattern as 6.3c2..c5: peek for
end-of-section keyword, fall through to `wald_skip_to_section`
on parse error so downstream parsers still get the next
section's keyword.

Tests in `tests/test_wald.c` (183 sub-checks) cover:
- three-axiom EQUATIONS (`f(x, e) = x  f(x, i(x)) = e
  f(f(x, y), z) = f(x, f(y, z))  CONCLUSION ...`) with cross-
  check on the first pair's CTR/FVR structure
- empty EQUATIONS section (immediate `CONCLUSION` keyword)
- single CONCLUSION storing the goal
- reject-multiple: `a = e  i(a) = a` keeps only `a = e`

### Added: Waldmeister .pr term parser (stage 6.3d)

`wald_parse_term(spec, lex) -> Term` lands in `src/wald/_.c`.
Grammar:

  term ::= ident                            -- var (FVR) or
                                               zero-arity sym
        |  ident "(" term ("," term)* ")"   -- application

Variable-vs-symbol dispatch: an ident is a variable iff it
appears in `spec->vars[]` (returns `term_new_fvr(var_id)`);
otherwise it's a signature symbol that becomes a TAG_CTR with
the registered label.  Application arity must match the
signature -- mismatched calls return 0.

Recursive-descent; arg list capped at `REWRITE_MAX_ARITY`.
Returns 0 on any parse error: unknown ident, missing close
paren, arity mismatch, or argument list on a 0-arity constant.

Tests in `tests/test_wald.c` (166 sub-checks) cover variable
lookup, zero-arity constant, two-arg application, nested
`f(i(x), e)`, unknown-ident, arity mismatch, and
constant-with-args paths.

### Added: ORDERING section parser (stage 6.3c5)

`wald_parse_ordering(spec, lex)` lands in `src/wald/_.c` with a
new `prec_rank` field on `WaldSym`.  Grammar:

  "KBO" weight_list precedence
  "LPO" precedence

Where `weight_list = name = number, name = number, ...` and
`precedence = f1 > f2 > ... > fN` (left = greatest).

The parser reads everything as a token stream, tracking the most
recently seen ident; on `>` the previous ident becomes the next
chain entry.  KBO weight lists are consumed and discarded (the
saturation engine's `KboConfig` stays caller-supplied; the .pr
file only contributes the precedence ordering).  After the
chain is gathered, ranks are assigned so chain[0] gets
`prec_rank = N - 1` (greatest) and chain[N-1] gets
`prec_rank = 0` (smallest).

Stage 6.3c (a/b/c1/c2/c3/c4/c5) now complete.  Next: 6.3d term
parser, then 6.3e/f/g.

Tests in `tests/test_wald.c` (147 sub-checks) cover:
- LPO chain `i > f > e > a` -> ranks i=3, f=2, e=1, a=0
- KBO weights `i=0, f=1, e=1, a=1` followed by the same chain
  -> identical ranks (weights ignored)
- empty ORDERING (immediate next-section keyword) -> ranks
  stay at calloc'd 0
- lone ident with no `>` -> not added to the chain, ranks 0

### Added: VARIABLES section parser (stage 6.3c4)

`wald_parse_variables(spec, lex)` lands in `src/wald/_.c`.
Grammar:

  { ident { "," ident } ":" sort_ident }

Each ident gets registered into `spec->vars[]` with a sequential
FVR id (`var_id = spec->n_vars` at registration time).  Sort
names are consumed and discarded under the homogeneous-signature
assumption.  Multiple `name : sort` decl groups in one section
accumulate ids monotonically.

Section ends at the next section keyword via the same peek +
`wald_skip_to_section` recovery pattern as 6.3c2/c3.  EOF
mid-list returns `WSEC_NONE`; whatever names were registered
stay registered.

Tests in `tests/test_wald.c` (131 sub-checks) cover:
- `x,y,z : ANY EQUATIONS foo` -> 3 vars (ids 0/1/2), returns
  EQUATIONS, lexer past `EQUATIONS` at `foo`.
- empty VARIABLES (immediate `EQUATIONS`) -> 0 vars.
- multi-decl `x,y : ANY  z,w : ANY1  EQUATIONS` -> 4 vars
  (ids 0/1/2/3) flowing across both decl groups.
- truncated `x,y` -> WSEC_NONE, 2 vars registered.

### Added: SIGNATURE section parser (stage 6.3c3)

`wald_parse_signature(spec, lex)` lands in `src/wald/_.c`.  Per
entry parses `name : arg_sort1 ... argN -> result_sort` and
registers a `WaldSym { name, label = next_label++, arity = N }`
in `spec->symbols[]`.  The result sort is consumed and discarded
under the homogeneous-signature assumption for stages 5-7.

Section ends at the next section keyword via the same peek +
`wald_skip_to_section` recovery pattern as 6.3c2.  Capacity
overflow (`n_symbols >= WALD_MAX_SYMBOLS`) and parse errors fall
through to `wald_skip_to_section` so downstream parsers still
see the next section keyword.

Tests in `tests/test_wald.c` (109 sub-checks) cover:
- single zero-arity entry: `e: -> ANY ORDERING ...` -> e at
  label 1, arity 0; lexer past `ORDERING`.
- three entries with monotonic labels: e=1, i=2, f=3; arities
  0, 1, 2.
- empty SIGNATURE (immediate `ORDERING` keyword): no entries.
- truncated mid-entry (`f: ANY ANY ->` without result sort):
  returns WSEC_NONE, half-parsed entry not committed.

### Added: NAME / MODE / SORTS section parsers (stage 6.3c2)

`src/wald/_.c` lands three parsers with the same shape:

- `wald_parse_name`  -- one ident -> `spec->name`
- `wald_parse_mode`  -- one ident; `"COMPLETION"` -> `mode_proof
  = 0`, anything else (or empty) -> `mode_proof = 1`
- `wald_parse_sorts` -- ident list consumed and discarded
  (homogeneous-signature assumption for stages 5-7)

Each peeks for an immediate section keyword (empty section ->
return next section's enum without touching `spec`), otherwise
consumes its content + falls through to `wald_skip_to_section`.
Internal helper `wald_consume_if_section` factors the empty
check.

Tests in `tests/test_wald.c` (88 sub-checks) cover the populated
path (`name == "group"`, `mode_proof = 1` for PROOF / 0 for
COMPLETION, sorts consumed leaving the lexer at the next
section), the empty-section path (immediate next keyword leaves
defaults intact), and the EOF path (returns `WSEC_NONE`).

### Added: section-detect infrastructure for the .pr parser (stage 6.3c1)

`src/thvm.h`:

- `WaldSection` enum: `WSEC_NONE` plus `NAME / MODE / SORTS /
  SIGNATURE / VARIABLES / ORDERING / EQUATIONS / CONCLUSION`.
- `WaldLex` gains a 1-token peek (`have_peek` flag +
  `peeked_kind` + `peeked_text` + `peeked_len`).
- `wald_section_from_ident(name) -> WaldSection`,
  `wald_lex_peek(lex)`, `wald_skip_to_section(lex)`.

`src/wald/_.c`:

- `wald_lex_init` clears the peek slot.
- `wald_lex_next` consumes the peek if armed (copies
  peeked_text into tok_text), otherwise scans normally.
- `wald_lex_peek` populates the peek using the regular scan
  path; idempotent until the next consume.
- `wald_section_from_ident` is a flat strcmp dispatch,
  case-sensitive (".pr" files use uppercase keywords).
- `wald_skip_to_section` eats tokens until it sees a known
  section keyword; returns the matching enum, or WSEC_NONE on
  EOF.

This is the shared dependency for 6.3c2..c5: each section parser
falls back to `wald_skip_to_section` on unrecognized content and
uses `wald_lex_peek` to detect end-of-section without consuming
the next section's keyword.

Tests in `tests/test_wald.c` (76 sub-checks) cover all eight
section keywords, the unknown / case-sensitive / empty-string
paths for `wald_section_from_ident`, peek-then-next consistency,
peek-at-EOF idempotence, `wald_skip_to_section` landing past the
keyword, and the EOF + empty-source paths.

### Added: Waldmeister .pr lexer (stage 6.3b)

`src/wald/_.c` gains the lexer half of the parser: `WaldLex`
cursor over a NUL-terminated source buffer plus
`wald_lex_next(lex) -> WaldTokKind`.

Token kinds: `WT_END`, `WT_IDENT` (ident text in
`lex.tok_text[]`, truncated to `WALD_NAME_LEN - 1`), `WT_COLON`,
`WT_ARROW` (`->`), `WT_EQ`, `WT_LPAREN`, `WT_RPAREN`,
`WT_COMMA`, `WT_GT`, `WT_ERR`.  Skips whitespace and
`%`-to-end-of-line comments; section keywords (NAME, MODE,
SORTS, SIGNATURE, ORDERING, VARIABLES, EQUATIONS, CONCLUSION,
LPO, KBO) come back as plain `WT_IDENT` -- the section drivers
(6.3c) compare `tok_text`.

Tests in `tests/test_wald.c` (49 sub-checks) cover empty input,
whitespace-only input, `%` comment skipping, ident chars
including digits + underscore, `->` vs bare `-` (errors), the
full punctuation set, an `f(x, e) = x` token stream, long-ident
truncation (`tok_len == NAME_LEN - 1`, NUL-terminated), and the
unknown-char error path.

### Added: WaldSpec data model for the .pr parser (stage 6.3a)

`src/wald/_.c` lands the Waldmeister-spec data container plus
`wald_init` (heap-allocated, defaults to `mode_proof = 1` and
`next_label = 1`) and `wald_free` (NULL-safe).  Public types in
`src/thvm.h`: `WaldSym`, `WaldVar`, `WaldSpec`, plus caps
`WALD_MAX_SYMBOLS = 64`, `WALD_MAX_VARS = 32`, `WALD_MAX_EQNS =
64`, `WALD_NAME_LEN = 32`.  The struct holds the parsed
signature (each symbol gets a monotonic CTR label starting at 1
to skip the anonymous-tuple label), variable table (sequential
FVR ids), parallel `eqn_lhs/rhs[]` axiom arrays, and a single
`(goal_lhs, goal_rhs)` for proof-mode CONCLUSION.

Lexer / section drivers / term parser / equations / driver land
in 6.3b..f; end-to-end test against the group example follows in
6.3g.

### Added: PCL-shaped trace serializer (stage 6.2)

`thvm_atp_trace_serialize(s, buf, cap)` walks `s->trace[]` and
emits Waldmeister-PCL-style text into `buf`.  Each entry becomes
one line:

  <idx> (<reason> [from <p_a>[, <p_b>]]): <lhs> = <rhs>

Internal `atp_pretty_term` recursively prints CTR / FVR / NUM /
ERA terms with a "?T<tag>" fallback for the rest.  Truncates
silently on buffer overflow; the returned byte count gives the
caller a way to detect truncation against `cap - 1`.

Mirrors the role of Waldmeister's `pcl.c`
(*Proof Construction Language*) output -- a flat per-step record
that downstream tools can re-render into LaTeX / ASCII / Prolog
proofs.

Tests in `tests/test_atp.c` (8342 sub-checks) cover:
- empty trace yields zero bytes + null-terminated buf
- single axiom prints `0 (axiom): C... = C...`
- f(x, e) renders as `C3(x_0, C1)`; rhs as `x_0`
- post-step trace contains `1 (orient from 0):` and
  `... (cp from 1, 1): ...` lines
- 16-byte buffer truncation stays null-terminated

### Added: trace-walk verification on the headline demo (stage 6.1d)

`atp/headline-trace-shape-and-walk-to-axiom` in
`tests/test_atp.c` runs the same group-axiom proof as the stage
5.5 headline test and asserts the trace produced is sane:

- exactly 3 `TRACE_AXIOM` entries (the three axioms we pushed)
- at least 1 `TRACE_ORIENT` entry (the rule(s) added to R)
- the latest `TRACE_ORIENT`'s `parent_a` chain walks back
  through orient/CP entries to a `TRACE_AXIOM`, capped at 100
  hops to defend against pointer corruption

The walk-to-axiom invariant is the proof of correctness for the
trace plumbing: every rule in R can be tracked back to the
axioms that produced it, end-to-end, with no broken links.

Stage 6.1 (a/b/c/d) complete.  Next: 6.2 PCL-shaped serializer.

### Added: TRACE_CP entries from generate_cps (stage 6.1c)

`thvm_atp_generate_cps` rewritten to iterate `(i, j)` pairs
explicitly so each emitted CP knows the trace indices of the two
source rules.  Survivors push onto the queue with
`TRACE_CP(parent_a = r_trace[i], parent_b = r_trace[j])` -- a
new helper `atp_push_cps_traced` does the trace+queue push.

`AtpState` gains `u32 r_trace[ATP_MAX_RULES]` tracking each rule's
TRACE_ORIENT entry index.  Initialized to `ATP_TRACE_NONE` so
test code that pre-populates `s->lhs/rhs` directly produces
TRACE_CP entries with NONE parents (clearly marking them as
"source rule unknown" rather than mis-aliasing trace index 0).

`thvm_atp_step` now stashes each newly-added rule's TRACE_ORIENT
index in `r_trace[added.first + k]` so subsequent generate_cps
calls have the provenance.  `thvm_atp_interreduce` shifts
`r_trace[]` in lockstep with the `lhs/rhs` arrays when dropping
subsumed older rules.

Tests in `tests/test_atp.c` (8323 sub-checks) verify that after
adding one axiom and running one step, `s->trace[2]` is a
TRACE_CP entry whose `parent_a == parent_b == 1` (the orient
entry index of the new rule, which self-overlaps to produce the
trivial top-position CP).

### Added: trace wired into add_equation + atp_step orient (stage 6.1b)

`thvm_atp_add_equation` now pushes a TRACE_AXIOM entry (parents =
ATP_TRACE_NONE) and stashes its index in
`AtpState.cp_trace[i]` alongside the lhs/rhs slot.

`thvm_atp_select_cp` shifts the new `cp_trace[]` array in lockstep
with the lhs/rhs queue and writes the popped CP's trace index
into a transient `s->last_popped_trace` field for the saturation
step to consume.

`thvm_atp_step` reads `last_popped_trace` after `select_cp` and,
following a successful `orient_and_add`, pushes one TRACE_ORIENT
entry per added rule (the unfailing 2-way fallback gets two
entries, both linking back to the same source CP).

Tests in `tests/test_atp.c` verify:
- `add_equation` populates the trace with a TRACE_AXIOM whose
  parents are NONE; `cp_trace[0]` points back at it.
- After `thvm_atp_step` orients an axiom, the new trace entry is
  TRACE_ORIENT with `parent_a` pointing at the original axiom.

CP-generation traces (TRACE_CP) still pending in 6.1c.

### Added: trace storage + push helper for AtpState (stage 6.1a)

`src/thvm.h` gains `TRACE_AXIOM/ORIENT/CP` reason labels,
`ATP_TRACE_NONE = 0xFFFFFFFFu` parent-index sentinel,
`ATP_MAX_TRACE = 4096` cap, and a new `Term trace[]` + `u32
n_trace` pair on `AtpState`.

`src/atp/_.c` gains the internal `atp_trace_push(s, reason, p_a,
p_b, lhs, rhs)` helper which packs a trace entry as
`TAG_CTR(label = reason, children = [NUM(p_a), NUM(p_b), lhs,
rhs])` -- IC-native shape so 6.2's PCL serializer can walk the
trace as plain heap terms.  Returns the index of the new entry,
or `ATP_TRACE_NONE` on overflow.

The helper is not yet wired into `add_equation` /
`orient_and_add` / `generate_cps` -- those land in 6.1b/c.
Storage is zero-init via `thvm_atp_init`'s calloc.

Tests in `tests/test_atp.c` cover entry decoding (AXIOM with both
parents NONE), parent threading (ORIENT pointing back at an
earlier AXIOM), and the overflow path (push #4097 yields
ATP_TRACE_NONE).

### Added: stage-5 headline saturation demo green (stage 5.5)

`atp/headline-prove-f-a-ia-equals-e-from-group-axioms` in
`tests/test_atp.c`: under the standard group-axiom KBO config
(weights `i=0, f=1, e=1, a=1`; precedence `i > f > e > a`;
`w0 = 1`), `thvm_atp_run` proves the conjecture
`f(a, i(a)) == e` from the three axioms

  right-id:  f(x, e)        = x
  right-inv: f(x, i(x))     = e
  assoc:     f(f(x, y), z)  = f(x, f(y, z))

in <= 20 saturation steps.  Stage 5 of
`docs/plans/waldmeister_ic_atp.md` complete: the IC-native ATP
saturation engine -- INC-priority CP selection (5.3),
KBO orientation with unfailing fallback (5.2b), interreduction
(5.2c), CP generation (5.2d), goal check (5.2e), and recursive-
descent rewriting (5.4) -- proves a real group-theory lemma
end-to-end.

### Changed: thvm_rewrite_step now recursive-descent (stage 5.4)

`thvm_rewrite_step` upgrades from top-only to outermost-leftmost
descent: it tries the top first; on no top-match, it walks the
TAG_CTR children left-to-right, recursing into the first child
that yields a rewrite, and returns the rebuilt term.  One step
still fires exactly one redex; `thvm_rewrite_normalize`'s
fixpoint loop drives multi-level reductions.

Effect on the saturation pipeline (stage 5):

  thvm_atp_interreduce   (5.2c) -- now drops rules whose LHS has
                                   a reducible sub-position, not
                                   just rules whose top reduces
  thvm_atp_goal_check    (5.2e) -- compound goals normalize fully
  thvm_atp_step          (5.2f) -- the per-step normalize covers
                                   all positions in the picked CP

No external API changes; the saturation pipeline inherits the
wider coverage automatically.  All existing top-only test cases
still pass (top is tried first, so top-position rewrites don't
get pre-empted by deeper ones).

Three new cases in `tests/test_rewrite.c` exercise sub-position
firing (`i(f(a, e))` -> `i(a)` under `f(x, e) -> x`),
multi-level (`i(i(f(a, e)))` -> `i(i(a))`), and top-tried-before-
children precedence.

### Changed: thvm_atp_select_cp now priority-aware via INC + collapse_ordered (stage 5.3)

Replaces the FIFO pop with the SupGen-style priority encoding from
the design memo: each queued CP becomes
`INC^k(CTR_label=idx [lhs, rhs])` where
`k = symbol_count(lhs) + symbol_count(rhs)` (the `--add`
heuristic from Waldmeister's `ClasHeuristics.c`
"classification heuristics").  All wrapped CPs are folded into a
SUP tree and run through `thvm_collapse_ordered`; the cheapest
leaf comes out first, its CTR label decodes back to the original
queue index, and we pop that index.

Singleton case skips the SUP/INC plumbing.  The pre-existing
saturation pipeline (5.2a..5.2f) consumes the upgraded selector
unchanged -- the `atp/run-one-step-prove` headline still passes.

The existing FIFO test in `tests/test_atp.c` is upgraded to
`select-cp-priority-order`: under k_1=4, k_2=k_3=2 the pop order
is l2 (k=2, dfs=1) -> l3 (k=2, dfs=2) -> l1 (k=4) rather than
the original l1 -> l2 -> l3 FIFO sequence.

### Fixed: TMemoryPlanGantt y-axis -- "BarHeight"->"Log" actually works

The `"BarHeight"->"Log"` option was documented in
`TMemoryPlanGantt::usage` but unwired: `linearScanPack` hardcoded
raw nbytes for slot height, so sub-1-KiB bufs in linear-train
all stacked at y=0 and a single preserved buffer dominated the
whole page.  Threaded the option through (default "Log") +
made y-axis tick labels mode-aware (Log mode shows
`(2^y - 1)/1024` KiB).  Each buf now gets a distinct y-stripe,
proportional to log2 of its size.

### Added: thvm_atp_step + thvm_atp_run -- saturation loop driver (stage 5.2f)

`src/atp/_.c` glues 5.2a..5.2e into the full step.  Order from
`docs/plans/saturation_loop.md` sec.2:

  goal_check -> step_cap -> select_cp -> normalize ->
  trivialize -> orient_and_add -> interreduce (with post-
  interreduce range adjustment so generate_cps targets the
  correct, possibly-shifted slots) -> generate_cps -> goal_check.

`thvm_atp_step` returns one of ATP_PROVED / ATP_TIMEOUT /
ATP_QUEUE_EMPTY / ATP_RUNNING.  `thvm_atp_run` is the trivial
loop wrapper.

The interreduce-shifts-new-rule-indices coordination matters: when
`thvm_atp_interreduce` drops `dropped` older rules, the freshly-
added rules' indices each move down by `dropped`, so
`thvm_atp_generate_cps` is called with `post.first =
added.first - dropped` to re-derive the right (new x R) and
(old x new) sweeps.

Tests cover empty queue (returns QUEUE_EMPTY), trivial `e == e`
goal (PROVED at the top-of-step check, no work done),
`step_cap == 0` with non-trivial goal (TIMEOUT), the headline
one-step prove (`f(a, e) == a` from axiom `f(x, e) = x` runs to
ATP_PROVED via `thvm_atp_run`), and completion-mode saturation
that exhausts the queue and returns QUEUE_EMPTY.

### Added: thvm_atp_goal_check (stage 5.2e)

`thvm_atp_goal_check(s)` normalizes both sides of `s->goal_{lhs,
rhs}` under R via `thvm_rewrite_normalize` (NORM_CAP = 64) and
returns ATP_PROVED on a `kbo_eq` hit, ATP_RUNNING otherwise.
Skips cleanly (returns ATP_RUNNING) when `goal_lhs == 0` --
the completion-mode case where there's no conjecture to prove.

Top-only rewriting today; 5.4's recursive descent will widen
coverage to compound goal terms automatically.

Tests cover the no-goal pass-through, the trivial `e == e` case
that proves under empty R, the close-under-one-rule case
(`f(a, e) == a` under `f(x, e) -> x`), and the doesn't-close
case (`a == e`, no applicable rule).

### Added: thvm_atp_interreduce (stage 5.2c)

`thvm_atp_interreduce(s, added)` walks the older rules in `R`
(indices `[0, added.first)`), normalizes each LHS under the
freshly-added rule(s), and drops any rule whose LHS simplifies.
The dropped rule's `(reduced, old_rhs)` equation is pushed back
onto the CP queue for the saturation loop to re-orient under the
smaller `R`.

Top-only rewriting today via the existing
`thvm_rewrite_normalize`; stage 5.4's recursive descent widens
coverage to sub-positions without changing this function.

The new rules are copied out by Term value before the loop so the
array can be compacted without invalidating the dispatch.  Mirrors
Waldmeister's `Interreduktion.c` ("interreduction") cleanup phase.

Tests cover empty-added, drop-on-specialization (an `f(a, e) ->
f(a, a)` rule disappears under a fresh `f(x, e) -> x`),
keep-on-irreducible (a rule with a different top symbol survives),
and the underflow guard for the first-ever rule add (`added.first
== 0`).

### Added: thvm_atp_generate_cps + thvm_critical_pairs_range (stage 5.2d)

`src/cp/_.c` gains `thvm_critical_pairs_range(lhs, rhs, n,
start_i, end_i, start_j, end_j, out, cap)` -- generates CPs
restricted to a sub-rectangle of the rule index space.  The
existing `thvm_critical_pairs` becomes a thin wrapper passing the
full extents.

`src/atp/_.c` gains `thvm_atp_generate_cps(s, added)` which uses
the range version twice -- (new x all_R) then (old x new) --
to compute exactly the freshly-required CPs after a rule add,
skipping the (old x old) work that's already in the queue.
Temp buffer `ATP_CP_BATCH = 1024` CPs; survivors pushed onto the
queue, overflow dropped silently (matches Waldmeister's
*Kritische-Paare-Verwaltung* "critical-pair management" in
`KPVerwaltung.c`).

Tests cover empty-added no-op, single-rule self-overlap producing
at least one CP, an old-times-new sweep over (assoc + left-id),
and equivalence between the full and range versions.

### Added: thvm_atp_orient_and_add (stage 5.2b)

KBO-orient an equation and push the resulting rule(s) onto `R`.
Returns `AtpAddedRange { first, count }` so the next saturation
phase (5.2d generate-CPs) can target only the new rules.

- `KBO_GT`: push `lhs -> rhs`                                 (count = 1)
- `KBO_LT`: push the swap `rhs -> lhs`                         (count = 1)
- `KBO_UN`: unfailing fallback -- push both orientations,
  atomic on capacity (skip both if there's room for only one)   (count = 2)
- `KBO_EQ` or `R` full: no-op                                  (count = 0)

The unfailing variant of Knuth-Bendix completion (Bachmair-
Dershowitz-Plaisted) keeps unorientable equations as 2-way rules
so rewriting can try either direction.  Mirrors Waldmeister's
behavior; see `waldmeister/sources/INF/Hauptkomponenten.c`
(*Hauptkomponenten* = "main components").

### Added: thvm_atp_select_cp FIFO pop (stage 5.2a)

`thvm_atp_select_cp(s, &lhs_out, &rhs_out)` lands in
`src/atp/_.c` -- pops the front CP, shifts the tail down to
keep the array dense.  Returns 1 on success / 0 on empty.
Stage 5.3 will replace the FIFO with priority-collapse over
INC-wrapped CPs (the `--add` heuristic from
`waldmeister/sources/CLAS/ClasHeuristics.c`).  Tests cover
empty queue, FIFO order across three pushes, and tail
densification after a pop.

### Added: AtpState struct + init/free helpers (stage 5.1)

`src/atp/_.c` lands the saturation-loop state container plus
`thvm_atp_init` (heap-allocates, stores cfg + step_cap),
`thvm_atp_free` (NULL-safe), `thvm_atp_add_equation` (push CP),
`thvm_atp_set_goal`.  Public types in `src/thvm.h`: AtpStatus
enum, ATP_MAX_RULES (256), ATP_MAX_CPS (4096), AtpState struct
(rules R, CP queue, goal, KboConfig, step counter).  Tests in
`tests/test_atp.c` cover init/free symmetry, NULL-free safety,
queue-full rejection, goal set/clear.  Step + run drivers are
5.2.

### Changed: f1d helper accepts REDUCE-as-tail-op (CPU)

`materialize_kernel_inlined` (CPU-only via the existing backend
gate) now accepts `root_op == UOP_REDUCE` when the source is a
fully-inlinable elementwise chain.  Tinygrad's "local reduction"
pattern: one kernel runs N-1 elementwise ops into a register and
the final REDUCE writes the output buffer.

Linear-train forward+loss with toggle ON: 16 -> 8 kernels.  Both
the Softmax-normalization REDUCE_SUM(EXP(x)) and the CE-loss
REDUCE_SUM(MUL(target, LOG(p))) chains now collapse into one
kernel each instead of one kernel per UOp.

### Added: saturation-loop design sketch (stage 5.0)

[docs/plans/saturation_loop.md](docs/plans/saturation_loop.md)
designs the AtpState struct, the 10-step saturation algorithm,
fairness mitigations (step_cap + round-robin escape), termination
conditions, and the mapping from existing C-side primitives
(`thvm_match`, `thvm_unify`, `thvm_critical_pairs`,
`thvm_rewrite_normalize`, `thvm_kbo`, `thvm_collapse_ordered`)
into the loop body.  The implementation lands in 5.1-5.4; demo
(prove `f(a, i(a)) = e` from group axioms) is 5.5.

### Verified: ATP arc baseline green (stage 0 sanity)

`make test` (48 C executables, 166 sub-checks) and `make wl-test`
(295 WL VerificationTests) both green at HEAD `f49f267`.  First
firing of cron `757c483c` driving
[docs/plans/waldmeister_ic_atp_tasks.md](docs/plans/waldmeister_ic_atp_tasks.md)
through stages 5-8+.

### Added: ICC type-flow primitives (TAG_BRI + TAG_ANN, real ICC rules)

`TAG_BRI = 23` (Bridge / Val: θx.body) and `TAG_ANN = 24`
(Annotation: {val : typ}) land with the actual ICC reduction rules
from `TinyHVM/resources/gists/icc_spec.md`, not the LAM-alias that
TinyHVM shipped with:

  APP (θx.body) arg = θx (APP body[x ← λ$k.x]  (ANN $k arg))
  ANN val (λx.body) = λx (ANN (APP val $k) body[x ← θ$k.x])
  ANN val (θx.body) = body[x ← val]                          (type erasure)

Plus DUP-BRI commutation (mirror of DUP-LAM) so bridges duplicate
correctly under SUP search.

Files: `src/term/new_bri.c`, `src/term/new_ann.c`,
`src/interact/{app_bri,ann_lam,ann_bri,dup_bri}.c`.  TAG_ANN
reduction is inline in `src/wnf/_.c` (mirrors TAG_OP2's strict-
on-typ-then-dispatch pattern).

Tests (`tests/test_icc.c`, 11 sub-checks):
- ANN-BRI type erasure on θx.x consumes the bridge, leaves the val
- ANN-BRI on a bridge whose body is its own bound y returns y[y ← val]
- APP-BRI on θx.x with NUM(7) fires and the head is again BRI
  (the inner structure changes; ICC is type-flow, not value-flow)
- ANN-LAM fires and the head wraps in a new λ
- DUP-BRI commutes !&7{F0,F1} = θx.x into two bridges
- ANN with a non-LAM/BRI typ stays stuck

These are the ICC primitives the IC-native ATP plan can fall back
on for closed-form encodings of equations + dependent-type proofs.
FVR-based open-form remains the active path for stages 2-4 of the
plan; BRI/ANN are now ready when stages 5-7 motivate them.

### Added: stage 4 -- unification + critical-pair enumeration

`src/unify/_.c` lands the Robinson MGU on TAG_CTR + TAG_FVR with
the standard occurs check.  Result lives in the same RewriteSubst
struct used by stage 3's matcher; `unify_walk` follows FVR -> FVR
chains, and `thvm_unify_apply` realizes a chained substitution
into a fully-instantiated term.  `thvm_rename_vars(t, offset)`
shifts every FVR id by `offset` so two rules can be unified
without variable-name collisions.

`src/cp/_.c` enumerates critical pairs.  Walks every non-variable
position of `rule_i.lhs`, tries unifying with `rule_j.lhs`
(renamed apart by `REWRITE_MAX_VAR/2`), and emits
`(σ(l_i[p ← r_j]), σ(r_i))` on success.

Demo (`tests/test_cp.c`): rule set `{ f(e, x) = x ; f(f(x, y), z)
= f(x, f(y, z)) }` produces the expected CP `(f(y, z), f(e, f(y,
z)))` from the [0]-overlap of left-id into assoc.

C-side only for now; SUP-encoded CP enumeration via a `TAG_PRI`
unify primitive (stage 4.5) is optional and deferred.

### Added: TMatStatsLabel for per-realize THVM_MAT_STATS attribution

`TMatStatsLabel["fwd_conv1"]` tags the next `thvm_realize` call's
`THVM_MAT_STATS=<path>` log line with the given string; the buffer
clears after one realize.  Bridges:
`thvm_wl_mat_stats_label(UTF8String)` in `wl/THVMLink/CSource/thvmlink.c`,
backed by a 64-byte `MAT_STATS_LABEL` global in `materialize_memo.c`.

Lets probes attribute kernel counts to specific layers / grad
chains.  Sample LeNet breakdown: forward+loss=231 kernels;
grad_b1..b3=20 each; grad_w4=40; grad_b4=36 -- forward Conv2D-
lowered chain dominates and is the next fusion target.

### Changed: lenet bench + verify use TGradMany; materialize descends into TAG_CTR

`lenetStep` (baseline.wls) and `stepGrads` (verify.wls) now build a
single multi-target `UOP_GRAD` via `TGradMany[loss, weights]`.
`materialize_expr` gained a `TAG_CTR` case that recursively
materializes each child within the same realize, so all n backward
kernels emit in ONE materialize pass with shared memo.

Bench result is NEGATIVE for the kernel-count metric (427 -> 426
on lenet) and 0% for peak.  Cause: each per-target chain rule
allocates fresh cotangent UOp cells with new heap locs, so the
memo can only dedup the forward leaf references.  Detail +
follow-up options in `docs/bench-results.md` "k0e" section.

### Added: equational rewriter -- stage 3 (one-shot, top-position only)

`src/rewrite/_.c` exports a small C-side equational rewriter for
TAG_CTR + TAG_FVR terms:

- `thvm_match(pattern, term, subst)` -- one-way matching with
  linearity check (a variable seen multiple times must bind to
  the same sub-term, verified via `kbo_eq`).
- `thvm_subst_apply(t, subst)` -- substitution-with-rebuild: TAG_FVR
  becomes its bound sub-term; TAG_CTR is rebuilt with substituted
  children; everything else passes through.
- `thvm_rewrite_step(t, lhs, rhs, n_rules)` -- try each rule in
  order at the *top* position; first match wins, RHS returned with
  substitution applied.
- `thvm_rewrite_normalize(t, lhs, rhs, n_rules, step_cap)` -- iterate
  rewrite_step to a fixpoint or until step_cap exhausts.

Recursive descent into sub-terms is not yet wired -- that's part of
the saturation loop in stage 5.

The headline demo from `docs/plans/waldmeister_ic_atp.md` sec.5
runs in `tests/test_rewrite.c`: under the full group axioms

  f(x, e)        = x
  f(x, i(x))     = e
  f(f(x,y), z)   = f(x, f(y, z))

`f(a, e)` normalizes to `a` (one rewrite_step fires; the second
step is a fixpoint).  Plus 8 supporting cases for matching,
non-linear consistency, substitution, no-applicable-rule, and the
inverse rule firing.

### Added: TAG_FVR + thvm_kbo -- stage 2 (term encoding + KBO ordering)

`TAG_FVR = 22` is an atomic first-order variable: `EXT = var_id`,
no heap cells.  Distinct from `TAG_VAR` (the IC's bound variable
tied to a binder).  Used by the IC-as-ATP layer to encode the
universally / existentially quantified variables of equational
logic.

`thvm_kbo(s, t, cfg)` (`src/kbo/_.c`) implements the Knuth-Bendix
ordering on TAG_CTR + TAG_FVR terms.  KboConfig holds per-symbol
weights, total precedence, and the scalar variable weight w0.
Returns KBO_EQ / KBO_GT / KBO_LT / KBO_UN.  Algorithm: Baader-
Nipkow (variable-domination check, weight comparison, top-symbol
precedence tiebreak, lexicographic on args).

The headline demo from `docs/plans/waldmeister_ic_atp.md` sec.5
runs in `tests/test_kbo.c`: under Waldmeister's default group-
axiom KBO (weights `i=0, f=1, e=1, a=1`; precedence `i > f > e > a`;
`w0 = 1`), `f(x, e) > x` orients correctly.

C-side only for now -- the IC-as-pure-program port (stage 2.4) is
optional and deferred.

### Added: TGradMany WL bridge

`TGradMany[y, {x_1, ..., x_n}]` in `wl/THVMLink/Kernel/Tensor.wl`
builds a single `UOP_GRAD` and realizes once; the resulting
`TAG_CTR` of n cotangents is unpacked into a List of TTerm
wrappers via `thvm_wl_term_ctr_at`.  3 new tests in `grad.wlt`
assert equality with the per-target `TGrad` results.

### Added: multi-target chain rule for UOP_GRAD

`interact_grad` now handles `n>1` by lowering to a `TAG_CTR` of `n`
unary `uop_grad(y, gy, x_i)` terms.  Each unary grad walks the
chain rule independently; the forward DAG (y and its descendants)
lives at shared heap locs so materialize's per-realize memo dedups
every kernel emitted from those forward UOps across all `n`
targets.  `n=1` keeps the scalar return for backward compat.

### Changed: UOP_GRAD heap layout is now multi-target (k0b)

`uop_grad` heap is now `[y, gy, NUM(n), x_1, ..., x_n]` (was
`[y, gy, target]`).  New `uop_grad_multi(y, gy, targets, n)` is
the primary constructor; the legacy unary `uop_grad(y, gy, x)`
is a thin wrapper with `n=1`.  `interact_grad` bails on `n>1`
for now -- the multi-target chain rule lands in k0c.

The change cascades through every site that knows GRAD's heap
arity: `wnf/redex.c` `term_arity` reads `NUM(n)` to compute
`3+n`; `alo/realize.c` `alo_node_arity` and
`book/from_dynamic.c` `dyn_arity` take a `val` argument so they
can `book_read` / `heap_read` the count when cloning UOP_GRAD
templates (TOptim's recursive lambdas embed it).  Accessors
`uop_grad_n` / `uop_grad_target` provide read-side parity.

### Added: TAG_WHEN boolean filter -- closes the stage-1 e2e demo

`TAG_WHEN = 21` is the IC-side primitive for "collapse to the
matching one":

  WHEN(NUM(0), _)        -> ERA               (failed branch erases)
  WHEN(NUM(n != 0), b)   -> wnf(b)
  WHEN(ERA, _)           -> ERA
  WHEN(&L{c0,c1}, b)     -> &L{WHEN(c0, B0), WHEN(c1, B1)}, !&L{B0,B1}=b

The end-to-end demo from `docs/plans/waldmeister_ic_atp.md` now
runs in one IC reduction + one collapse:

```
cands = &L{NUM(2), NUM(3)}
t     = WHEN(EQL(cands, NUM(3)), cands_dup)
collapse(t) -> [NUM(3)]    -- only the matching candidate
```

Failed candidates collapse to ERA via WHEN-NUM-zero, and
`thvm_collapse` drops ERA branches.  This is stage 1.7 revised:
constructors+MAT deferred to stage 2 (term encoding) where they
are motivated by encoding equations.

Constructor: `term_new_when(cond, body)`.  Tests:
`tests/test_when.c` covers all rules + the e2e demo.

### Added: TAG_INC priority wrapper + thvm_collapse_ordered

`TAG_INC = 19` is a one-cell priority wrapper.  The reducer treats
it as a WNF atom (default fall-through; no interactions), so the
INC layer survives reduction and becomes visible to collapse.

`thvm_collapse_ordered(t, out, cap)` performs the same shallow
SUP-tree walk as `thvm_collapse`, but counts INC wrappers along
the path to each leaf and emits the leaves sorted by INC-depth
ascending (ties broken by DFS order).  Lower INC count = higher
priority = enumerated first.  Implementation collects (Term, pri,
idx) into a heap-allocated buffer, qsorts, writes Terms back.

This is the IC encoding of Waldmeister's `--mix` CP-selection
heuristic: wrap each candidate with INC^k where k is its weighted
cost, and `thvm_collapse_ordered` enumerates cheapest first.

Constructor: `term_new_inc(body)`.  Tests: `tests/test_inc.c`.

### Added: TAG_ANY wildcard

`TAG_ANY = 18` is an atomic wildcard.  Two interactions:

  EQL(ANY, x) -> NUM(1)        (matches anything, on either port)
  ! &L{x0,x1} = ANY  ->  x0 <- ANY, x1 <- ANY

Constructor: `term_new_any()`.  Used as the IC encoding of
existential / Skolem variables in the ATP plan.  Tests:
[tests/test_any.c](tests/test_any.c).

### Added: TAG_AND, TAG_OR with short-circuit + SUP commutation

`TAG_AND = 16` and `TAG_OR = 17` land as short-circuit boolean
nodes; both are strict on the left operand and lazy on the right:

  AND(NUM(0), _)        -> NUM(0)        (right stays unreduced)
  AND(NUM(n != 0), b)   -> wnf(b)
  AND(ERA, _)           -> ERA
  AND(&L{a0,a1}, b)     -> &L{AND(a0,B0), AND(a1,B1)}, !&L{B0,B1}=b

  OR(NUM(0), b)         -> wnf(b)
  OR(NUM(n != 0), _)    -> NUM(1)        (right stays unreduced)
  OR(ERA, _)            -> ERA
  OR(&L{a0,a1}, b)      -> &L{OR(a0,B0), OR(a1,B1)}, !&L{B0,B1}=b

The SUP commutation routes a superposed left operand through both
branches with the right operand DUPed, mirroring EQL-SUP.  This
enables the SupGen-style filter pattern `AND(EQL(cand, expected),
cand)`: the matching candidate survives, the rest become NUM(0).
Full ERA-propagating filter (collapse to *only* the matching
candidate) needs the MAT/constructor work in stage 1.7.

Constructors: `term_new_and(a, b)`, `term_new_or(a, b)` in
`src/term/`.  Tests: `tests/test_and_or.c`.

### Added: EQL-SUP commutation + DUP-NUM annihilation

The `EQL` reducer now commutes through `SUP` on either port:

  EQL(&L{a0,a1}, b)  ->  &L{EQL(a0, B0), EQL(a1, B1)}, !&L{B0,B1}=b
  EQL(a, &L{b0,b1})  ->  &L{EQL(A0, b0), EQL(A1, b1)}, !&L{A0,A1}=a

The DUPed b (resp. a) propagates correctly because `DUP-NUM`
annihilates atomically, copying the Term value into both
projections.  New file `src/interact/dup_num.c`.

End-to-end: `EQL(&L{NUM(2), NUM(3)}, NUM(3))` now reduces to
`&L{NUM(0), NUM(1)}`, and `thvm_collapse` enumerates `[NUM(0),
NUM(1)]` -- the SupGen-style search-as-superposition pattern is
working for the first time on thvm.

### Added: TAG_EQL (structural equality) -- minimal cut

`TAG_EQL = 15` lands as a strict equality node with heap layout
`[a, b]`.  The wnf reducer walks both ports to WNF and dispatches:

- `EQL(NUM(x), NUM(y))` -> `NUM(1)` if x==y else `NUM(0)`
- `EQL(ERA, _)` / `EQL(_, ERA)` -> `ERA` (failed branches collapse out)
- otherwise stuck

SUP commutation (the rule that pushes a SUP at either port up to
the head) lands separately in stage 1.3b alongside DUP-NUM.

Constructor: `term_new_eql(a, b)` ([src/term/new_eql.c](src/term/new_eql.c)).
Tests: [tests/test_eql.c](tests/test_eql.c).

### Added: glossary section on equational reasoning and the IC-as-ATP layer

[docs/glossary.md](docs/glossary.md) gains an *Equational reasoning
and the IC-as-ATP layer* table, explicitly distinguishing **HVM-SUP**
(the runtime data primitive `&L{a, b}`) from **ATP-superposition**
(the logical inference rule, refined paramodulation), plus
companion entries: collapse, label, substitution, **cosubstitution
and bisubstitution** (Wolfram's framing -- bisubstitution = paramodulation),
unification, matching, paramodulation, critical pair, Knuth-Bendix
completion, unfailing completion, reduction ordering, joinability,
saturation, subsumption, PCL.  The plan memo
[docs/plans/waldmeister_ic_atp.md](docs/plans/waldmeister_ic_atp.md)
gets a terminology warning at the top cross-referencing the new
section, and [docs/README.md](docs/README.md) lists the plan in its
plans-and-references index.

### Added: thvm_collapse -- shallow SUP-tree enumeration

`src/collapse/_.c` exposes `thvm_collapse(t, out, cap)` which walks
the head of `t` via WNF and recurses on TAG_SUP, dropping TAG_ERA
branches.  Caller-supplied buffer + cap; returns count.  This is
the "shallow" version: deeper enumeration through APP / OP2 / EQL /
... lands as those tags get SUP-commutation interactions.
Tests in `tests/test_collapse.c` cover single-leaf, single-SUP,
nested SUP, ERA-pruned branch, and cap-truncation.

### Added: docs/plans/waldmeister_ic_atp.md -- IC-native ATP design memo

Research-and-design memo summarizing Waldmeister's unfailing
Knuth-Bendix completion algorithm, surveying prior art on
interaction-net + ATP work (April 2026), and sketching how the same
proof procedure could be expressed as IC graph rewrites in thvm
using SupGen / NeoGen-style superposition over rule sets and
overlap spaces.  Includes a 7-stage build trajectory.

### Added: DUP-SUP cross-label commutation

`interact_dup_sup` now handles the commuting case
`!&L{x0,x1} = &R{a,b}` (L != R) by allocating a 6-cell block of
two new dup bodies (for `a` and `b`) plus four DP0/DP1 leaves, and
returning two fresh `&R`-labeled SUPs.  Previously the cross-label
case was stuck.  This unblocks any future tag whose interactions
need SUPs to flow through DUPs (EQL, AND/OR, MAT, INC, ...).
Tests in `tests/test_dup_sup.c` exercise head shape, inner
structure, and both-projection consistency.

### Changed: lazy GRAD + lazy materialize via shared term_resolve

`interact_grad` and `materialize_expr` no longer call `wnf` to
expose their inputs.  Both now route through a new shared
`term_resolve` (in `src/term/resolve.c`) that does the minimum
work needed to surface the outermost layer:

- TAG_VAR: take the SUB-bit cell (single-step deref); chase the
  chain if it cascades.
- TAG_ALO: force one realisation layer via `alo_force` (which is
  itself memoised, so repeated walks are cheap).
- everything else: return unchanged.

That's the entire resolver -- it does NOT fire materialize, kernel,
or grad reductions.  Anything `interact_grad` can't structurally
pattern-match (e.g., a free VAR that hasn't been bound yet) is
returned unchanged; `wnf`'s UOP_GRAD case got a fixed-point check
so the term sits as WHNF rather than re-fires.  `materialize_expr`
follows the same pattern, with a single `wnf` step retained for
the LAM/APP/REF case (where actual beta / unfolding is required
before any UOp shape is visible).

The SGD demo in `wl/THVMLink/Tests/sgd.wlt` was rewritten to drop
the per-iteration `TUOpMaterialize` wrapper from the loop body.
The recursive call now passes the symbolic `step(w)` UOp graph as
the new w; `TRealize` at the end fires one materialize over the
deeply-nested expression.  That's both cleaner and side-steps the
"shared TUOpMaterialize wrapper produces distinct fresh TENs per
fire so grad's leaf check breaks" issue we papered over with the
materialize cache: now every reference to `w` in the body is the
same UOp Term value, and the leaf check just works.

### Added: phase 3 -- SGD optimizer as a recursive lambda term

Tying phases 1 + 2 together to demonstrate the original use case
the user laid out: a lambda term that takes a "net with loss at
root" plus a parameter and adds GRAD nodes inside a recursive
training loop expressed as `TDef`/`TRef`.

Three runtime fixes were needed to make `materialize(step(w))`
compose multiple times without breaking grad's leaf check:

1. **Lazy GRAD chain rule.**  `interact_grad` used to recursively
   compute the entire chain rule expansion in one fire (eager).
   Now each fire does a single structural step on `y`'s outermost
   UOp, deferring sub-positions as fresh `UOP_GRAD` nodes that
   wnf re-enters on demand.  `wnf` is called ONCE on `y` and
   `target` to expose the outermost layer (so a `GRAD[APP(loss_fn,
   w), w]` body can beta-reduce before pattern-matching).  Existing
   numerics preserved (9/9 grad + 17/17 nn end-to-end tests still
   pass); test_grad.c structural assertions updated to expect the
   one-layer form.

2. **Materialize follows VAR substitutions.**  After APP-LAM beta,
   a UOP body's cells hold VARs pointing at the binder's
   substituted heap slot.  `materialize_expr` now wnfs each input
   first so VAR (and ALO and the active-path UOPs the wnf reducer
   knows about) resolve to a concrete TEN/UOP_KERNEL.

3. **Materialize result memoization.**  The same `MATERIALIZE`
   wrapper inside a single graph is often referenced from N
   slots (e.g. a recursive `step(w)` body uses `w` in both the
   loss and the weight update).  Each fire used to allocate a
   fresh kernel + TenDesc, so the resulting TAG_TEN ids differed
   per use; `interact_grad`'s `y == target` pointer-equality
   leaf check then failed and the gradient collapsed to zero.
   `thvm_materialize` now caches the realised result back into the
   wrapper's heap cell so subsequent fires return the SAME
   TAG_TEN id.

4. **ALO_force memoization.**  The companion fix for #3.  Each
   ALO fire used to re-realize from the book template, allocating
   fresh dyn cells.  In a recursive REF body that references the
   bound `w` multiple times, the multiple references then mapped
   to distinct fresh wrappers and #3 didn't help.  `alo_force`
   now writes the realised term back into the ALO cell and marks
   the second slot non-NUM as a "cached" sentinel.  Subsequent
   fires hit the cache.

WL example, in `wl/THVMLink/Tests/sgd.wlt`:

```
sgd_loop = TLam[w |->
  TLam[n |->
    TIfZero[n, w,
      TApp[
        TApp[TRef["sgd_loop"],
             TUOpMaterialize[
                w + (-lr) * grad(L2(w - target), w)]],
        TOp2["-", n, TNum[1]]
      ]
    ]
  ]
]
TDef["sgd_loop", sgd_loop]
TWnf @ TApp[TApp[TRef["sgd_loop"], w0], TNum[2]]
  -> {0.36, 0.72, 1.08}    (* w_2 = 0.8 w_1 + 0.2 target *)
```

4/4 SGD cases pass (one-step lambda + 0/1/2 recursive iters).
Compute scales steeply (kernels ~3-4x per iteration without DUP
sharing for tensors); training-scale runs need that next.

### Added: phase 2 -- MAT (numeric switch) + OP2 (binary ops on NUMs)

Two more term tags so a recursive REF body can hit a base case and
manipulate its iteration counter (precondition for the SGD-as-lambda
optimizer in phase 3):

- `TAG_OP2` (val = heap loc -> [x, y], ext = OP_*) -- strict on x
  then y; both must reduce to TAG_NUM for the op to fire.  Opcodes
  `OP_ADD` / `OP_SUB` / `OP_MUL` / `OP_EQ` / `OP_LT`.  Stuck if
  either operand stays non-NUM.
- `TAG_MAT` (val = heap loc -> [handler, fallback], ext = match)
  -- numeric-switch atom.  In wnf's `apply` phase, when an APP frame
  pops with a MAT head, the arg is forced via a recursive `wnf()`
  call: NUM matching `ext` reduces to `handler`, otherwise the
  result is `APP(fallback, arg)`.  Mirrors HVM4's APP-MAT-NUM.

`book/from_dynamic.c` and `alo/realize.c` learned the two new
fixed-arity-2 nodes so REF unfolding handles them.  `wnf/_.c` got
`case TAG_OP2` in enter and a `case TAG_MAT` branch inside the APP
apply switch.

WL surface in `wl/THVMLink/Kernel/Switch.wl`:
- `TNum[i]` / `TNum[i, dtype]` -- a TAG_NUM atom (defaults to i32).
- `TOp2["+"|"-"|"*"|"=="|"<", x, y]` -- a TAG_OP2 term.
- `TMatNum[matchVal, handler, fallback]` -- a TAG_MAT atom.
- `TIfZero[counter, then, else]` -- sugar that wraps the else branch
  in a discarding lambda so MAT's miss-path looks like a plain
  conditional.

Tests:
- `tests/test_mat_op2.c` (9 cases) -- OP2 arithmetic + MAT match /
  miss + an end-to-end **recursive countdown** built from
  REF + ALO + LAM/APP + MAT + OP2 (`@count 0 5 -> NUM(5)`).
- `wl/THVMLink/Tests/switch.wlt` (9 cases) -- the WL surface plus
  recursive countdown + sumto via `TDef`/`TRef`.  All pass.

All 331 C cases + 39 WL cases pass.

### Added: phase 1 of REF / ALO -- lazy named definitions

Two new term tags layered on top of the IC + UOP graph so users can
register named definitions and unfold them lazily during reduction
(precondition for the recursive SGD-as-lambda optimizer described in
PLAN.md):

- `TAG_REF` (val = name slot) -- a one-cell pointer into a fresh
  `DEFS[]` table holding the registered definition's *static
  template*.  Reducing a REF wraps the template in an empty-state
  ALO and re-enters; the body itself isn't expanded.
- `TAG_ALO` (val = dyn loc -> [book_term, NUM(state_id)]) -- the
  HVM4-style allocator.  Each fire walks one layer of the static
  template into a fresh dynamic heap region, threading an
  `AloState` chain that rebinds binders (LAM -> VAR) through the
  new dyn locs so multiple unfoldings of the same def don't alias
  each other's bound variables.

New runtime infrastructure:
- `BOOK_HEAP[]` (256K cells, parallel to `HEAP`) -- immutable
  per-def template cells.
- `DEFS[256]` -- root book term per registered name.
- `ALO_STATES[]` -- linked substitution chain for ALO descents.
- `book/{alloc,read,set,from_dynamic}.c` -- allocator + the
  recursive snapshot that lifts a dynamic term tree into the book
  heap (handles LAM / APP / VAR / fixed-arity UOP families; SUP /
  DUP / variable-arity movement ops are a follow-up).
- `alo/{state,realize,force}.c` -- the substitution chain plus
  `alo_realize` (one book-layer -> dyn) and `alo_force` (force a
  TAG_ALO term into its dyn shape).
- `term/{new_ref,new_alo}.c` -- term constructors.

`wnf/_.c` gained `case TAG_REF` / `case TAG_ALO` cases that fire
the unfolding; both bump `ITRS`.

WL surface in `wl/THVMLink/Kernel/Ref.wl`:
- `TDef[name, body]` -- snapshots `body` into the book heap and
  registers it under an integer slot (`name` may be a string -- it
  gets interned to a stable slot via `$defNames`).
- `TRef[name]` -- returns a TTerm wrapping a TAG_REF cell.
- `TDefName[name]` -- expose the slot mapping for tests.

Tests: `tests/test_ref.c` (5 cases) covers identity-via-REF + fresh
allocation per call + lazy self-reference; `wl/THVMLink/Tests/ref.wlt`
(4 cases) covers the WL surface end-to-end.  All 322 C cases + 30+
WL cases still pass.

Known scope: REF unfolds forever for self-referential defs without a
termination construct.  Phase 2 adds `MAT` (pattern match / numeric
switch) + `OP2` (SUB on NUMs for counter decrement) so a recursive
`train_step` lambda can hit a base case at iteration 0.

### Added: NN training-step numerics + per-render TimeConstrained budget

`nn.wlt` grew five training-flavoured cases on top of the layer
helpers:
- two-head square loss `(w.x + v.x)^2`, gradient sums across both
  paths to the same target;
- MSE through a dot product checked w.r.t. both `w` and `x`;
- one SGD step on `(w.x - t)^2` confirms the gradient direction
  reduces the loss;
- three-step gradient descent verifies loss is monotonically
  non-increasing;
- polynomial-regression-ish `(a x^2 + b x - t)^2` checks both
  partials.

`wl/Examples/run.wls` wraps each render in `TimeConstrained` (30 s
budget per heap-graph / IC-diagram render).  Dense tensor graphs
(NN-style compositions) sometimes blow the IC layout up by 100x and
hung the whole batch; now the over-budget render is skipped and
logged with `[skip] ... (over 30s)` so the rest of the examples
keep going.

### Added: NN.wl -- Wolfram NeuralNetworks layer -> TUOp graph converter

`wl/THVMLink/Kernel/NN.wl` lets users build the UOp graph by feeding
in built-in layers (`LinearLayer`, `ElementwiseLayer`, `NetChain`,
...) instead of inventing parallel layer constructors.  Tinygrad's
"Tensor + thin layer wrappers" model: a layer is a snapshot of
weights, the converter lifts them to TTensors and emits the same
TUOp* combinators users would write by hand.

Public surface (all in the THVMLink` context):
- `TFromNet[net, x]` / `TFromLayer[layer, x]` -- entry points,
  dispatch on `Head[layer]`.
- `TLayerWeights[layer]` / `TLayerToTensors[layer]` -- read a
  layer's NumericArrays / wrap them as TTensors.
- Tensor-method helpers: `TSum`, `TSquare`, `TDot`, `TMatVec`,
  `TL2Loss`, `TMSELoss`.

Currently supported layers:
- `LinearLayer[out, "Input" -> in]` -- forward via TMatVec
  (W @ x + b through EXPAND-broadcast + REDUCE_SUM).  Backward
  through W is a TODO until interact_grad gains an EXPAND rule.
- `ElementwiseLayer[#*# &]` -- maps to TSquare.  Adding more
  functions is a one-line entry in `$elementwiseDispatch`.
- `NetChain[{...}]` -- folds layers in declaration order.

Stubbed / out-of-scope:
- `ConvolutionLayer` -- raises a Message; needs movement-op
  support in materialize/interpret + the matching grad rules
  (step 14).

`wl/THVMLink/Tests/nn.wlt` covers the helpers (12 cases): forward
of LinearLayer / ElementwiseLayer / NetChain, plus end-to-end
gradient chains (TDot, TL2Loss, TMSELoss, polynomial, square of
dot product) -- all 12 pass.

### Fixed: shared-wire spiders for non-CONST UOP multi-reference

`wireFor` used to give every cell its own `w<loc>` wire name, so
when a non-CONST UOP fed N consumer slots only the principal cell
matched up; the other N-1 consumers dangled (visible in
`MUL[x, x]` where x is itself a UOP -- the second src wire had a
fresh name and stayed unconnected).

Fix: TAG_UOP cells (excluding CONST, which we render per-reference
as leaves) now key the wire on the producer's base
(`uop<val>` instead of `w<loc>`), so all consumer slots and the
producer's output share one wire.  DC then draws a spider where
the producer fans out to all the consumers -- same idiom we
already use for VAR / DP0 / DP1.

`plainUopDiagram` and `gradDiagram` updated their synthetic-fallback
pWire (used when the seed is heapless) to match the new naming
(`uop<base>` instead of `p<base>`).

### Fixed: grad chain rule allocates fresh EXPAND per branch + single-line node headers

`grad_rec` previously lifted `gy` to target.shape ONCE in
`interact_grad`, which was correct numerically but produced a heap
where multiple chain-rule consumers all referenced the same EXPAND
node.  In any visualisation that doesn't fan out via DUP, all but
one of those consumer wires dangled (visible in `grad-x-times-x`:
the second branch's MUL had a missing CONST input).

Fix: each branching chain-rule node (`UOP_MUL`, `UOP_ADD`,
`UOP_NEG`, `UOP_REDUCE`) now allocates a *fresh* EXPAND of `gy`
per branch.  A new `gy_lifted` flag threaded through `grad_rec`
prevents redundant outer EXPANDs at deeper leaf positions when
the cotangent is already target-shaped.  Test structure update
in `tests/test_grad.c`; numerics unchanged (9/9 WL grad cases
still pass).

### Changed: single-line node headers carry heap loc + handle id

Diagram + heap-graph labels now use `OPCODE@<heap-loc>(#<id>)` on
one line instead of stacking opcode and base across two lines.
The `#<id>` suffix only appears when the opcode carries an extra
handle: `KERNEL@10#2` (kernel id from the `NUM(kid)` cell),
`GRAD@3#1` (target tensor id from cell base+2), `TEN@10#1` (cell
loc + tensor id).  Plain compute UOPs stay terse: `MUL@8`,
`ADD@14`.  Shape (when known) and CONST scalar value remain on
follow-up lines.

### Changed: WL kernel split into per-concern files; shape inference centralised

`wl/THVMLink/Kernel/` now uses one BeginPackage["THVMLink`"] +
Begin["`Private`"] block per file, all sharing the same private
context.  Cross-file references resolve directly without
THVMLink`Private`-qualified calls.

Two new files separate concerns that used to be inlined in the
renderers:
- `Shape.wl` -- shape arithmetic (`broadcastShape`, `dropAxis`,
  `shapeText`), tensor-id shape lookup (`tenShapeOf`), and the
  manual IEEE 754 single-precision decoder (`bitsToReal32`,
  `bitsToInt32`, `scalarTextFromCell`).
- `Uop.wl` -- per-opcode metadata in one place: `uopArity`,
  `uopName`, plus an inferred-output-shape walker (`uopShapeOf`,
  `cellShape`, `uopSrcShape`) that mirrors the rules in
  `src/schedule/materialize.c`.

`Visualization.wl` and `Diagram.wl` now read these helpers
directly, dropping their duplicated tables.  UOP labels gained the
inferred output shape (e.g. "MUL\n@8\n{3}") and CONST keeps both
its heap base and its scalar value.

`THVMLink.wl` no longer hard-codes the load order -- after its
own EndPackage it Get's every other `*.wl` in the Kernel directory
in alphabetical order.  Adding a new sibling file means dropping
it in; no edits to the loader.

### Changed: shape-aware grad_rec drops the MUL(target, CONST(0)) wrapper

`interact_grad` no longer post-wraps the chain-rule output in
`ADD[raw, MUL(target, CONST(0))]` to coax materialize into producing
target-shaped gradients.  Instead, every leaf-level emission inside
`grad_rec` is wrapped in `EXPAND(_, target.shape)`:

- leaf match (`y === target`)        -> `EXPAND(gy, target.shape)`
- independent leaf / NUM             -> `EXPAND(CONST(0), target.shape)`
- `UOP_CONST`                        -> `EXPAND(CONST(0), target.shape)`
- `default`                          -> `EXPAND(CONST(0), target.shape)`

This required minimal materialize + interpret support for `UOP_EXPAND`
(previously a step-14 placeholder): `op_output_shape` now reads the
heap NUM cells for EXPAND's target dims (using the source view's rank
to know how many cells to read -- tinygrad EXPAND preserves rank), and
a new `cpu_op_expand` fans the source buffer out to the larger numel.
Sufficient for the autograd path (scalar -> 1-D); per-axis broadcast
in higher ranks lands with view tracking in step 14.

`tests/test_grad.c` was rewritten to expect the EXPAND wrapping
(replacing the old `unwrap` helper that stripped the dead `MUL` wrapper).
WL `grad.wlt` end-to-end numerics still pass (9/9).

### Added: shape on TEN labels, scalar value on CONST labels

`THeapDiagram`'s leaf labels now carry the data the user actually wants
to see:
- `TEN#<id>` -> reads `TENS[id].view.shape` and shows e.g. `{3}` on a
  third line.
- `CONST` -> decodes the NUM cell's bits via manual IEEE 754 (so a
  CONST(1.0) renders as `CONST\n1.` instead of a mystery `CONST\n@2`).

### Added: tensor-aware THeapDiagram (IC string-diagram path)

Diagram.wl now renders TAG_UOP / TAG_TEN terms via Wolfram`Diagrammatic`-
`Computation`, so `THeapDiagram[term]` produces a proper IC string
diagram for tensor compute graphs (it previously returned an empty
network for anything that wasn't pure IC).

UOP rendering uses an opcode-driven shape/style:
- Plain compute UOPs (ADD/MUL/...) -- apex-down blue triangle, N
  inputs at top (one per `uopArity[opcode]`), 1 output at bottom.
- GRAD -- DUP-shaped (apex-up orange triangle), 1 input at top
  apex (the y branch), 2 outputs at the flat bottom (forward
  passthrough + backward gradient).  `gy` and `target` cells are
  hidden; the target tensor id is surfaced as `#<tid>` in the
  GRAD label.

TEN handles render as cyan apex-down triangles, one leaf per
referencing slot (no DUP needed for multi-reference -- each ref
gets its own `TEN#<id>` triangle).

CONST UOPs (zero-arity) are rendered the same way: per-reference
leaves labeled `CONST@<base>`, so a constant referenced from N
slots draws N triangles instead of forcing a shared agent (which
would require DUPs to fan out).

Reachability filter walks UOPs/TENs forward from the seed term
so post-`TWnf` heaps don't surface their pre-rewrite cells.
`principalCellOf` was tightened to consider only cells inside
reachable agents' slot ranges, so dead heap can't grab a UOP's
output wire.

`run.wls` no longer skips IC diagrams for `grad-` examples; both
pre-reduce (`diagram.png`) and post-WNF (`diagram-wnf.png`) IC
diagrams are now rendered alongside the heap graphs.

New plain-UOP examples (no grad rewrite):
- `wl/Examples/uop-add` -- `TUOpAdd[a, b]`
- `wl/Examples/uop-mul` -- `TUOpMul[a, b]`
- `wl/Examples/uop-mul-add` -- `(a*b)+c`

Existing grad-`*` examples now use lazy `TTensor[{3}]` allocations
instead of `TTensorCreate @ NumericArray[...]`; the visualization
doesn't need real numerics and the lazy form is shorter.

### Added: tensor-aware heap graph + grad- visualization examples

`Visualization.wl` got a major extension to render tensor compute
graphs (it previously only knew about IC tags LAM/APP/SUP/DUP/ERA,
so any `TAG_UOP` / `TAG_TEN` term came out blank).

New vertex-id convention prefixes the kind:
- `a<base>` -- IC compound at args base `<base>`
- `e<loc>`  -- ERA cell at heap loc
- `u<loc>`  -- TAG_UOP at heap loc
- `t<id>`   -- TAG_TEN at tensor id

Per-tag rendering:
- `TAG_TEN` -- cyan square labeled `TEN\n#<id>`
- `TAG_UOP` -- blue rectangle labeled `<OPCODE>\n@<loc>`
- Edge labels follow `src<N>` using a per-opcode `uopComputeArity`
  table (NUM-only cells stay implicit).

Single-vertex default size bumped (0.18 -> 0.45) so identity-only
terms don't render as a pinhead.

Three new `wl/Examples/grad-*` folders, each with `term.wl` plus
pre-reduce (`term.png`) and post-`TWnf` (`term-wnf.png`) heap
renderings:
- `grad-add` -- gradient of `a + b` w.r.t. `a` -> ones_like(a)
- `grad-mul` -- product rule `d(ab)/da` -> b
- `grad-x-times-x` -- `d(a*a)/da` -> 2a

`run.wls` detects `grad-`-prefixed folders, skips the IC string
diagram (tensor graphs aren't IC nets), and renders both the
pre-reduce graph and the post-`TWnf` rewritten graph using the
`TWnf` result as the discovery seed.

### Added: PLAN.md step 13 (partial) -- UOP_GRAD reverse-mode autograd

`UOP_GRAD` is the 18th UOp opcode and a pure rewrite rule (not a
graph node that survives reduction).  Reducing
`UOP_GRAD[y, gy_seed, target]` under `TWnf` recursively applies the
chain rule until no `UOP_GRAD` nodes remain, then wraps the result
in a `target * 0` summand so the broadcast machinery in materialize
projects it onto target's shape.

Step-13 chain-rule coverage: leaf cases (target match, other tensor,
NUM, CONST), `UOP_ADD`, `UOP_MUL` (product rule), `UOP_NEG`, and
`UOP_REDUCE` (SUM only -- MAX needs an indicator one-hot, deferred
to step 14).  Anything else returns `CONST(0)` and warns.

WL surface:
- `TUOpGrad[y, gy, target]` -- explicit cotangent.
- `TGrad[y, target]` -- top-level VJP shortcut with `gy = CONST(1)`.

`materialize_expr` recognises `UOP_GRAD` and reduces it inline before
kernelizing, so `TMaterialize[TGrad[...]]` works without a separate
TWnf pass.

Tests:
- `tests/test_grad.c` (16 checks): structural pin-downs of the
  rewrite output for each handled opcode.
- `wl/THVMLink/Tests/grad.wlt` (9 checks): end-to-end f32 numerics
  including identity, independent leaf, ADD, MUL product rule,
  NEG, REDUCE_SUM broadcast-back, `x*x = 2x`, and `2x + 3 = 2`.

### Removed: `Function[t_TTerm]` UpValue

The `(f_Function)[t_TTerm] -> TApp[TLam[f], t]` IC sugar was a
footgun -- it silently rewrote any pure-function map over TTerms
into a beta-redex (which surfaced as a crash when our numeric
Plus/Times UpValues used `& /@`).  Removed alongside the
`$inTLamBinder` guard that only existed to break the resulting
recursion.  `TTerm[id_Integer][arg]` sugar (forming
TApp[TTerm[id], arg]) stays.

### Added: PLAN.md step 12 -- TTensor + TUOp + materialize + dispatch

End-to-end tensor pipeline.  WL-built UOp graphs reduce naturally
through schedule + kernelize + linearize + interpreter dispatch to
concrete `TAG_TEN` results, all under one `TWnf` call.  See
`docs/tensors.md` and `docs/glossary.md`.

Six commits across the step:

- **tensor foundation** (139af93)
  - Three new term tags: `TAG_TEN` (8) atom for tensor handles,
    `TAG_UOP` (9) heap-backed for graph nodes, `TAG_NUM` (10) atom
    for inline scalars.
  - `TenDesc` side table (`TENS[]`) with refcount, View
    (shape/strides/offset), buffer id, and Backend pointer.
  - CPU `Backend` vtable: alloc/free/incref/decref + buf_read/write,
    parallel `CPU_BUFS[]` table with its own refcount for view
    aliasing.
  - View aliasing (`tensor_view_of`) bumps the buffer refcount so
    reshape/permute can share storage zero-copy in step 14.

- **UOp vocabulary + WL surface** (719ac4a)
  - 18 opcodes covering CONST, six movement ops
    (RESHAPE/PERMUTE/EXPAND/PAD/SHRINK/FLIP), eight elementwise ops
    (ADD/MUL/NEG/RECIP/EXP2/LOG2/SQRT/CMPLT), REDUCE, plus the
    rewrite triggers MATERIALIZE and KERNEL.
  - One `src/uop/<op>.c` per opcode emitting the documented heap
    layout.
  - WL surface in the new `Tensor.wl` sibling: `TTensor`,
    `TUOpAdd/Mul/.../Reduce`, `TUOpMaterialize`, plus inspection
    helpers.

- **TRealize + TTensorCreate + zero-copy NumericArray I/O** (862887e)
  - `TRealize[expr] := TWnf[TUOpMaterialize[expr]]`.
  - `TTensorCreate[data]` shares a `NumericArray`'s buffer on the
    CPU backend (Shared passing mode + per-buffer cleanup
    callback).  PackedArrays / nested Lists lift to NumericArray
    first.
  - `TTensorData` returns a `NumericArray` whose type matches the
    dtype (single memcpy in the f32 fast path; no f32 -> f64
    conversion).
  - CpuBuf gains `owns_data` + `on_release` callback so the same
    slot can hold either malloc'd or borrowed bytes.

- **materialize pipeline** (8ffd333)
  - New `KERNELS[]` side table with linearized `KProgOp` programs;
    the same SSA-over-indices shape tinygrad's PYTHON device
    consumes.
  - `src/schedule/materialize.c` rewrites a UOp graph into a tree
    of `UOP_KERNEL[output_buf, NUM(kid)]` terms; recursively
    materializes children, dedups identical inputs.
  - `TMaterialize` WL helper for inspecting the scheduled DAG
    *before* kernel firing; `TKernelInfo[kid]` returns the
    linearized program as an Association.

- **CPU interpreter + interact_kernel** (3e071bd)
  - Per-op CPU files under `src/backend/cpu/op/` (one per opcode,
    matching the project's file = function name convention).
  - `cpu_interpret` walks `KernelEntry.program[]`, allocates one
    scratch per intermediate, dispatches via switch on opcode.
  - `interact_kernel` recursively fires producer kernels first
    (via the new `TenDesc.producer_kid` field), then invokes
    `Backend.dispatch_kernel` for the current kernel.  Increments
    `ITRS` once per firing, the same way HVM4 counts an OP2-NUM-NUM
    collapse.
  - `wnf` extension: `TAG_UOP/UOP_MATERIALIZE` -> direct rewrite,
    `TAG_UOP/UOP_KERNEL` -> fire, anything else -> WNF.

- **PLAN.md** (9b5a4db)
  - Step 12 marked done.

Numerical UpValues on `TTerm` (Plus / Times / Minus / Power[1/2] /
Less) rewrite ordinary WL arithmetic against tensor-shaped TTerms
into UOp graphs.  Scalars lift to UOP_CONST with the seed tensor's
dtype.

Removed: the `Function[t_TTerm]` UpValue that converted `f[t]` to
`TApp[TLam[f], t]`.  It was dumb, surprised the Plus/Times rewrite
that maps over tensors, and the matching `TLam[$inTLamBinder] guard`
went with it.

End-to-end:
```mathematica
a   = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
b   = TTensor[{4}, {10.0, 20.0, 30.0, 40.0}];
out = TRealize[2.0 * (a + b) + 1.0];
Normal @ TTensorData[out]
(* {23.0, 45.0, 67.0, 89.0} *)
```

### Added: THeapDiagram (Wolfram`DiagrammaticComputation` backend)

- New `wl/THVMLink/Kernel/Diagram.wl` subpackage at context
  `THVMLink`Diagram` exporting `THeapDiagram[term]`, which builds a
  `DiagramNetwork` from the current heap using the
  `Wolfram/DiagrammaticComputation` paclet (assumed installed).
- The subpackage lives in its own context so its `BeginPackage`
  imports can pull in `Wolfram`DiagrammaticComputation` and its
  `Diagram` subcontext without shadowing names in the main
  `THVMLink` context.
- Wire-name strings are unique per heap location: `w<loc>` for
  cells, with `VAR` cells collapsed to their binder's wire and
  `DP0`/`DP1` cells expanded to `dup<base>_dp{0,1}_lab<ext>`.
- `wl/Examples/run.wls` now writes `diagram.png` next to `term.png`
  for each example's input `term.wl` (skipped for reference
  variants like `term-reduced.wl`).

### Added: TTermExpr / TTermTree, TReduce, .hvm refs, restructured examples

- `wl/Examples/` folders no longer carry numeric prefixes; the
  reduced variants merge into their parent (`02-id-app-era` +
  `03-id-app-era-reduced` -> `id-app-era/` with both `term.wl` and
  `term-reduced.wl`).  `13-church-2-applied` becomes its own
  top-level `church-2-applied/` (the lambda lives in `church-2/`).
- `term.wl` is the input term construction.
- `term-reduced.wl` (optional) constructs the *expected* WHNF
  directly -- no `TWnf` / `TReduce` inside it.  The reduction test
  runner compares `TTermExpr[TWnf[term.wl]]` against
  `TTermExpr[term-reduced.wl]`.
- `term.hvm` (optional) carries the HVM4 surface-syntax reference
  with the expected output as a `//` comment line.  Documentation
  only; we have no parser yet.
- `wl/Examples/run.wls` now scans `term*.wl` per folder but only
  renders the input `term.wl` (skips `term-reduced.wl`, which is
  reference data).
- `wl/Examples/test_reductions.wls` is the reduction-comparison test
  driver, wired up as `make wl-examples-test`.
- New WL helpers in the paclet:
  - `TReduce[t]` = `(TWnf[t]; t)` -- reduces in place and returns the
    original root.  Useful as a `THeapGraph` seed when you want to
    visualise the post-reduction state.
  - `TTermExpr[t]` walks the heap from `t` and returns a nested
    expression with tag-name string heads (`"LAM"`, `"APP"`, `"SUP"`,
    `"DUP"`, `"DP0"`, `"DP1"`, `"VAR"`, `"ERA"`).  Cycles produce
    `"Cycle"[loc]` leaves.
  - `TTermTree[t]` = `ExpressionTree[TTermExpr[t]]` for visual
    rendering as a Wolfram `Tree`.
- README catalogue rewritten with the new folder names + an
  "Expected WHNF" column pointing at `term-reduced.wl`.

### Added: dark export + auto-fit labels + sugar

- WL `THeapGraph` accepts trailing `Graph` options via
  `OptionsPattern[]` (per the GUIDE) so callers can override
  `GraphLayout`, `VertexSize`, `PlotRange`, `Background`, etc.
- Vertex labels now render INSIDE each shape via `Inset[Pane[label,
  {pixelW, pixelH}, ImageSizeAction -> "ShrinkToFit"]]`.  Labels
  auto-shrink so the same `LAM @0` text fits cleanly in any vertex
  size.
- `VertexShapeFunction` honours the `size` argument throughout,
  including the ERA stroked Circle, so `VertexSize -> Tiny | Small |
  Large | Scaled[...]` all behave.  Removed the manual
  `singleVertexLoopFn` hack -- the default Wolfram self-loop renderer
  works once the shape sizes are scaled correctly and the plot range
  has room for the loop (single-vertex case explicitly widens
  `PlotRange` and shrinks the vertex).
- Examples export onto a dark `GrayLevel[0.12]` background with
  `Style[..., "DarkScheme"]` so `LightDarkSwitched` picks the
  dark-mode arm (white labels, darker fills, white outlines).
  Generated PNGs now read cleanly on dark READMEs and notebooks.

### Added: TTerm sugar (call as function, lambda literal)

- `TTerm[id_Integer][arg_]` desugars to `TApp[TTerm[id], arg]` so
  users can write `id[era]` instead of `TApp[id, era]`.
- `(var |-> body)[t_TTerm]` desugars to `TApp[TLam[var |-> body], t]`
  via a tagged UpValue on `TTerm`.  Lets you write a literal
  beta-redex without spelling out `TLam` / `TApp`.
- The `Function` UpValue is guarded by `$inTLamBinder` so `TLam`'s
  own internal call `builder[TVarFor[loc]]` does not trigger it
  (which would recurse infinitely).
- Two new VerificationTests cover both forms.

### Added: DUP-LAM + church-numeral examples

- `src/interact/dup_lam.c`: real DUP-LAM rule.  Allocates one
  five-cell block holding the new pair of bound vars (as a SUP
  inside the original binder) and the new pair of body projections
  (as a fresh DUP over the original body).  No body cloning happens
  eagerly -- only when a future projection inspects part of the
  body does it descend lazily.  This is the rule that gives Church
  numerals (and similarly Lamping / optimal-reduction style
  workloads) their non-exponential cloning behaviour.
- `tests/test_dup_lam.c`: two C tests; clone an identity lambda and
  confirm DUP-LAM fires once, then end-to-end apply one of the
  cloned copies to ERA.
- `wl/Examples/10-k-combinator/`, `11-church-1/`, `12-church-2/`,
  `13-church-2-applied/`: four new runnable examples.  The Church 2
  family exercises the DUP machinery; the applied form reduces
  end-to-end and the resulting graph (in `13-...-applied/graph.png`)
  shows the post-firing heap including the cloned lambdas and the
  substituted DUP cell.
- Two new VerificationTests in `wl/THVMLink/Tests/core.wlt`: a
  direct DUP-LAM clone, and Church-2-applied reducing to the
  identity-applied result.
- `docs/interact/dup_lam.md` documents the rule, the C, the cost,
  and why the lazy-cloning shape matters.

### Added: visualization renderer split + theme-aware colors

- `wl/THVMLink/Kernel/Visualization.wl`: extracts the heap-graph
  renderer into a dedicated kernel sibling.  THVMLink.wl now `Get`s
  it after declaring public symbols.
- Theme-aware colors throughout: `LightDarkSwitched[Black, White]`
  for foreground; `Lighter[StandardX, 0.55]` / `Darker[StandardX,
  0.45]` per-tag agent fills (green LAM, blue APP, orange SUP,
  purple DUP); ERA stays as a plain foreground-stroked Circle.
- Vertex labels now render in column form: `TAG\n@<base>` for
  arity-1 agents, `TAG\n@<base>..<base+1>` for arity-2.
- Triangles are real triangles via `Triangle[]` (not trapezoids)
  with apex orientation matching IC convention: LAM/DUP point down,
  APP/SUP point up.
- VertexShapeFunction now respects the size argument so
  `VertexSize -> Tiny | Small | Large | Scaled[...]` actually take
  effect.
- Single-vertex self-loop is drawn explicitly via
  EdgeShapeFunction; the identity lambda's loop is now visible.
- Pink "background" mystery solved: `Dashing[{Small, Small}]` was
  invalid (Small is not a numeric Dashing arg) which silently put
  Wolfram into an error-overlay state.  Replaced with the proper
  `Dashed` directive.
- Context-shadowing fix: switched `wl/Examples/run.wls` and
  `wl/THVMLink/Tests/run.wls` from `Needs["THVMLink`"]` to
  `Get["THVMLink`"]` so user code resolves to package symbols
  rather than auto-created `Global`*` placeholders.
- `wl/GUIDE.md` gains a Dark-mode + Standard colors section and an
  OptionsPattern[] section.

### Added: TTerm atomic wrapper + ensureInit

- TTerm[id_Integer] is the canonical wrapper around a packed Term;
  TLam / TApp / TSup / TDup / TEra / TVarFor return TTerm-wrapped
  values; TTermTag/Ext/Val/Sub accept either a TTerm or a raw
  Integer.  Old TTermInfo is gone (folded into TTerm[id]["info"]);
  TTermNew is no longer in the public API (private packTerm helper).
- TTerm[id]["tag" | "ext" | "val" | "sub" | "tagName" | "raw" |
  "info"] forwards to the bridge.  Format.wl gives TTerm a summary
  box keyed off the structural pattern (QuantumFramework style).
- ensureInit[]: heap-touching ops auto-call TInit if the runtime is
  not initialised yet.  TFree clears the flag.

### Added: wl/Examples/ runnable example database

- New `wl/Examples/` directory: one folder per example term, each
  holding a minimal `term.wl` (no `Needs`, no `TInit`, just the
  expression to construct the term) plus the rendered
  `graph.png` produced by the runner.
- 9 examples covering every interaction we currently fire: identity
  lambda, (id ERA) before / after `TWnf`, (ERA lam) before / after
  `TWnf`, bare `TSup[ERA, ERA]`, DUP-SUP same-label annihilation
  before / after, and nested APPs.
- `wl/Examples/run.wls`: single CLI for both bulk and per-example
  runs.  Loads the paclet, calls `TInit` per example, evaluates the
  `term.wl`, exports the resulting `THeapGraph[term]` as a PNG
  alongside the source.  Supports a positional example id and a
  `--eval` flag to skip the PNG export.
- `wl/Examples/README.md` catalogues every example and documents how
  to add new ones.
- `make wl-examples` (regenerate every PNG) and
  `make wl-examples EXAMPLE=<id>` (just one).
- `docs/heap_graph.md` now embeds two of those PNGs directly from
  `wl/Examples/<id>/graph.png` so the doc and the runnable example
  stay in sync.  The previous one-off `docs/images/` directory is
  removed.
- `wl/GUIDE.md` gains a rule for multi-line `If`: leading space after
  the bracket so the test argument lines up with the branches
  (`If[ cond, then, else]`).

### Added: heap graph rendering (PLAN.md step 10)

- `THeapGraph[]` and `THeapGraph[term]` (or `THeapGraph[{t1, t2,
  ...}]`) render the runtime as an IC string-diagram Wolfram
  `Graph[]`: compound terms (LAM/APP/SUP/DUP) are agent vertices
  keyed by their args base, VAR cells collapse into wires labelled
  `var`, and ERA cells render as small black dots.  Optional seed
  terms add agents that are heapless (held only as WL return values).
- `THeap[]` now returns an atomic `THeap[<|nextLoc, cells, Graph|>]`
  with the rendered graph at the `"Graph"` key (capitalized).
- `TTermInfo[t]` now returns an atomic `TTermInfo[<|...|>]` with the
  same payload shape.
- Both atomic objects expose Association-style indexing via DownValues
  and forward `KeyExistsQ`/`Keys`/`Values`/`Normal` via UpValues so
  callers see the same access shape as before.
- `wl/THVMLink/Kernel/Format.wl` defines the `MakeBoxes` UpValues
  (QuantumFramework-style: structural Q-test guarded by `Unevaluated`,
  `BoxForm`ArrangeSummaryBox` for the visual).  Loaded from
  `THVMLink.wl` after the public symbols are declared.
- `TFreshLabel[]` returns a fresh integer from a monotonic counter
  (reset by `TReset[]`).  `TSup[a, b]` and `TDup[body, k]` now
  auto-label via `TFreshLabel[]`; the existing 3-arg
  `TSup[label, a, b]` / `TDup[label, body, k]` forms remain for
  tests that need explicit label matching.
- `wl/THVMLink/Tests/core.wlt` gains six new VerificationTests:
  fresh-label monotonicity + TReset rewind, auto-label distinctness
  for both SUP and DUP, identity-lambda `THeapGraph` shape, seeded
  vs unseeded graph for `TApp[id, ERA]` and `TDup[TSup[ERA, ERA], k]`.
- `docs/heap_graph.md` is now the permanent reference for the model
  (agent-as-vertex, VAR-as-wire, ERA-as-dot) with six worked
  snapshots, mermaid diagrams, and live `THeapGraph` PNGs for
  examples 2 and 4 (regenerated by `docs/images/generate.wls`).
- `docs/term.md` gains a glossary table pinning down term / cell /
  loc / slot / agent / args base / port / node / wire and links
  forward to `docs/heap_graph.md`.
- `docs/wl.md` documents the `Format.wl` summary-box layer and the
  layout convention.
- `docs/images/generate.wls` produces the PNGs embedded in the doc;
  `docs/images/` is the canonical location for generated diagrams.

`make test`    -> 91 C checks pass.
`make wl-test` -> 23 WL VerificationTests pass.

### Added: architecture docs (PLAN.md steps 8-9)

- `docs/` with a self-contained markdown per piece, indexed by
  `docs/README.md`:
  - `docs/term.md`: bit layout + tag table + worked examples.
  - `docs/heap.md`: bump allocator + the substitution model
    (`heap_subst_var`, `heap_subst_cop`).
  - `docs/wnf.md`: enter/apply state machine, frame protocol, and
    the dispatch table for current interactions.
  - `docs/interact/_.md`: index of active-pair rules + tracking of
    which active pairs are stuck (deferred).
  - `docs/interact/{app_lam,app_era,dup_sup,dup_era}.md`: one page
    per interaction with the sequent rule, the C, a worked example,
    and a cost summary.
  - `docs/wl.md`: WL paclet design (scalar bridge + WL-side
    constructors) and usage.
- `README.md`: top of the file points at `docs/README.md` and the
  layout block now includes `docs/`.
- `AGENTS.md`: workflow step 4 clarifies that the `docs/interact/`
  page is the source of truth when it disagrees with the C file's
  header comment.

### Added: minimal reducer + interactions (PLAN.md steps 5-6)

- `src/wnf/_.c`: real two-phase stack-machine reducer (enter/apply)
  modeled on HVM4's clang/wnf/_.c.  Pushes APP / DP0 / DP1 frames at
  enter, dispatches active-pair interactions at apply, rebuilds stuck
  nodes by writing the reduced head back into the heap cell.
- `src/interact/app_lam.c`: real APP-LAM beta (`(lam x.body) arg`
  substitutes `arg` at the binder loc and continues into `body`).
- `src/interact/app_era.c`: APP-ERA (erased function yields ERA).
- `src/interact/dup_sup.c`: DUP-SUP same-label annihilation.  The
  commuting (different-label) case is left stuck for now; a test will
  drive the implementation when needed.
- `src/interact/dup_era.c`: DUP-ERA (both projections receive ERA via
  `heap_subst_cop`).
- `src/heap/subst_cop.c`: pair-substitution helper used by both
  DUP-style interactions; substitutes one side and returns the other.
- `src/thvm.h`: declares the new `interact_*` and `heap_subst_cop`
  signatures.
- `tests/test_app_lam.c`, `tests/test_era.c`, `tests/test_dup_sup.c`:
  removed the `PENDING(...)` gates and added ITRS-counter assertions
  so each test verifies the specific interaction fires (and only
  fires once).
- `wl/THVMLink/Tests/core.wlt`: three new VerificationTests covering
  APP-LAM, APP-ERA, and same-label DUP-SUP through the LibraryLink
  bridge.
- `Makefile`: moved `SRC :=` definition above the `$(WL_LIB)` rule so
  `make wl` correctly retriggers when any C runtime file changes.

`make test` -> 91 C checks pass.  `make wl-test` -> 14 WL tests pass.

### Added: WL paclet (PLAN.md step 4)

- `wl/THVMLink/` paclet that exposes the C runtime to Wolfram
  Language, with the LibraryLink bridge in `wl/THVMLink/CSource/`,
  the package in `wl/THVMLink/Kernel/THVMLink.wl`, and tests in
  `wl/THVMLink/Tests/`.
- `wl/THVMLink/CSource/thvmlink.c` exports 14 scalar `EXTERN_C
  DLLEXPORT` functions covering lifecycle (init/free/reset), term
  packing/unpacking (`thvm_wl_term_*`), heap access
  (`thvm_wl_heap_pos/alloc/read/set`), the WNF entry point, and the
  interaction counter. Every function is scalar-in / scalar-out (no
  arrays, no opaque handles).
- `wl/THVMLink/Kernel/THVMLink.wl` synthesizes higher-level term
  constructors (`TLam`, `TApp`, `TSup`, `TDup`) from the scalar
  primitives via shared `heapWith` / `heapTerm` helpers, plus the
  inspector `TTermInfo` and the heap snapshot `THeap[]`.
- `wl/THVMLink/Tests/core.wlt` defines 11 `VerificationTest` specs
  covering term packing roundtrip, heap primitives, the four
  high-level constructors, the heap snapshot, and the WNF stub
  passthrough.
- `wl/THVMLink/Tests/run.wls` is the test runner. It loads the
  paclet, invokes `TestReport` on every `*.wlt` file, prints
  `wl tests: N passed, M failed` to stdout, lists each failed test,
  and exits non-zero on any failure.
- `wl/GUIDE.md` records WL style rules: no `Print` (use a local
  `debugPrint` wrapping `WriteString`), no em dashes, no Unicode
  box-drawing characters, no decorative arrows in source.
- `Makefile` gains `make wl` (build the dylib at
  `wl/THVMLink/LibraryResources/$(WL_PLATFORM)/THVMLink.dylib`) and
  `make wl-test` (run `run.wls`). Auto-detects the newest
  `/Applications/Wolfram*.app`; override with
  `WOLFRAM_APP=/Applications/Wolfram\ X.Y.app`.

### Added: scaffold (PLAN.md steps 0-3)

- `AGENTS.md` with conventions (path-is-the-function-name, single TU,
  one-interaction-per-file), build/test instructions, and a code map.
- `.gitignore` covering `bin/`, `*.o`, `*.dylib`, macOS `.DS_Store`,
  the local-only `.claude/` settings dir, and the `TinyHVM` reference
  symlink.
- `Makefile` with `make` (build all), `make test` (build + run tests),
  `make clean`. Tests are independent C programs that include
  `src/thvm.c`.
- `src/thvm.h` declaring the term bit layout (SUB:1 / TAG:7 / EXT:18 /
  VAL:38), the minimal tag set (APP, LAM, VAR, ERA, DP0, DP1, SUP,
  DUP), heap globals, and function signatures for the
  term/heap/wnf/interact modules.
- `src/thvm.c` single-TU hub that `#include`s all `.c` files in build
  order.
- `src/term/{new,tag,ext,val}.c` and `src/term/sub/{get,set}.c`:
  full implementations of term packing/unpacking. Trivial
  bit-twiddling.
- `src/heap/{alloc,read,set,take,subst_var}.c`: flat single-threaded
  bump-allocated heap with substitution helper.
- `src/wnf/_.c`: WNF stack machine **stub** that returns its input
  unchanged. Step 6 will replace this with the real reducer.
- `src/interact/app_lam.c`: APP-LAM beta reduction **stub**. Step 6
  fills it in.
- `tests/test_term.c`: round-trip test for `term_new` and
  `term_tag/ext/val/sub_get`. 73 checks pass.
- `tests/test_heap.c`: alloc-then-read-back, set-then-read-back. 9
  checks pass.
- `tests/test_app_lam.c`, `tests/test_era.c`, `tests/test_dup_sup.c`:
  carry the spec for APP-LAM beta, ERA propagation, and DUP-SUP
  collapse/commute. Bodies are gated by `PENDING(...)` until step 6
  lands `wnf` and the interactions, so they exit 0 today and report
  `pend` in `make test` output.
- `README.md` describing what works today and what is stubbed.
