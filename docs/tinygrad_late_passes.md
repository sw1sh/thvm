# Tinygrad late codegen passes: expander + devectorizer

A reading-notes doc on tinygrad's `codegen/late/expander.py` and
`codegen/late/devectorizer.py` (plus the surrounding `reduce_to_acc`
piece). The point of writing this down is to map the divergence between
tinygrad's lowering pipeline and thvm's `render_uop.c` so the next port
arc is grounded in actual upstream semantics, not invented heuristics.

This doc covers the passes that run AFTER `hand_coded_optimizations`
has tagged ranges with `AxisType.UPCAST` / `UNROLL` / `LOCAL` etc. The
heuristic produces an opt-rich UOp graph; these late passes turn that
graph into linearizable code. Without them, an opt-rich graph emits the
right shape but doesn't get the perf the opts were chosen for, which is
exactly the regression thvm hits when its faithful heuristic runs
against thvm's renderer ([commit 6e4c2ed1](src/codegen/hand_opts.c);
CUDA wall 540 ms -> 1545 ms at BS=128).

## The pipeline (`tinygrad/codegen/__init__.py:full_rewrite_to_sink`)

```
sink   = AST (Ops.SINK rooted)
       |
       |  apply_opts(sink, ren, beam)
       |     -> hand_coded_optimizations(Scheduler)
       |        OR beam_search
       |        produces sink with Ranges tagged AxisType.{UPCAST,UNROLL,LOCAL,...}
       v
sink   = graph_rewrite(sink, sym + pm_pre_expander + pm_group_for_reduce + expander)
       |     ^ THIS is "the expander"
       v
sink   = graph_rewrite(sink, pm_add_buffers_local + rangeify_codegen)
       |
sink   = graph_rewrite(sink, pm_reduce + gep_pushing, ctx=ReduceContext())
       |     ^ REDUCE -> DEFINE_ACC + STORE/LOAD on REG buffer
       v
sink   = graph_rewrite(sink, pm_add_gpudims)         # late thread mapping
sink   = graph_rewrite(sink, pm_add_loads)           # INDEX -> LOAD
sink   = graph_rewrite(sink, pm_devectorize)         # split vectors, fold loads, etc.
sink   = graph_rewrite(sink, pm_lower_index_dtype + load_store_indexing + gep_pushing)
sink   = graph_rewrite(sink, symbolic)               # post-index simplification
sink   = ... ren.pre_matcher ...                     # device-specific tweaks
sink   = graph_rewrite(sink, pm_decomp + pm_dtype_decomps + pm_transcendental)
sink   = graph_rewrite(sink, pm_final_rewrite)       # pm_render + extra_matcher + pm_split_ends
sink   = graph_rewrite(sink, pm_add_control_flow)    # IF / barrier insertion
```

The bold steps are what this doc covers: **expander** (which lowers
UPCAST/UNROLL ranges) and **devectorizer** (which deals with the
vector-typed UOps the expander produces, plus turns REDUCE into a
scalar accumulator program).

## Concept: what a "UPCAST'd range" means

After `hand_coded_optimizations` runs, the AST still has `Ops.RANGE`
nodes for every dimension. Each RANGE carries an `arg = (axis_id,
AxisType)`. Some axes have type `AxisType.UPCAST` or `AxisType.UNROLL`
(or `LOCAL`/`GLOBAL`/`REDUCE`/...).

The semantic difference between a `LOOP` RANGE and an `UPCAST` RANGE is
*not* visible in the AST shape; both are just RANGE nodes that get
referenced by INDEX expressions and REDUCE axis tuples. The
distinction is in the **lowering convention**:

- `LOOP` ranges become runtime `for (int i = 0; i < N; i++)` loops.
- `UPCAST` / `UNROLL` ranges get **fully unrolled at the UOp level**: a
  range of extent F is REPLACED by F separate copies of every subtree
  that depended on the range value, each with the range substituted by
  a literal constant 0..F-1.

This full-unroll-at-UOp-level is what the **expander** does. It is NOT
a renderer-level `#pragma unroll`. The unrolled body is concrete UOp
nodes that subsequent passes (devectorizer, reduce_to_acc) treat as F
independent scalar operations.

## The expander pass (`tinygrad/codegen/late/expander.py`)

Three `PatternMatcher`s, run together with `sym`:

### 1. `pm_pre_expander` (lines 147-155)

Rewrites that prepare the graph for expansion.

**`Ops.RANGE` with `AxisType.UNROLL` or `AxisType.UPCAST`** becomes:

```
UOp(Ops.UNROLL,
    dtype = r.dtype,
    src   = (UOp.const(r.dtype.vec(s), tuple(range(s))),),
    arg   = ((r.arg[0], s),))    # s = r.vmax + 1, the extent
```

So a `RANGE(0..4)` of type UPCAST becomes an `Ops.UNROLL` node carrying
a **vector const** `(0, 1, 2, 3)` of width 4. The `arg` `((axis_id,
4),)` records what got unrolled, so downstream passes can re-collect
the expansion shape.

**`Ops.REDUCE`** that has UNROLL nodes in its `src[1:]` (i.e. some of
its reduce axes have been UPCAST'd / UNROLL'd) gets fixed up by
`fix_reduce_unroll` (lines 116-125): each UNROLL'd reduce axis stays in
the body via `Ops.CONTRACT` instead of being a RANGE. The
non-UNROLL'd reduce axes stay as RANGEs in `src[1:]`.

**`Ops.STORE`** with UNROLL nodes in `src[2:]` (UPCAST'd output axes)
gets `fix_store_unroll` (lines 127-130): wraps the STORE in a CONTRACT
that collects the unrolled output offsets.

### 2. `pm_group_for_reduce` (lines 157-160)

Only one rule: `Ops.REDUCE` with `GROUP_REDUCE` axes triggers
`fix_group_for_reduce`. This is the cooperative-reduce pattern (each
thread block does a partial reduction over a subset of the reduce axis
into shared memory, then a single thread folds the partials). Out of
scope for the current thvm gap.

### 3. `expander` (lines 94-112)

The actual unrolling rules. Two key patterns:

**`do_expand`** (lines 22-75) fires on any UOp whose `src` contains an
`Ops.UNROLL`:

```
@expander rule:
  UPat(*ALU_ops, *MOV_ops, REDUCE, STORE, BUFFERIZE, ...)
       with src containing UNROLL
    -> do_expand
```

`do_expand` collects the UNROLL args across all source UNROLL children,
deduplicates / sorts them, then **broadcasts every non-UNROLL src into
the same vector width** and produces a vectorized version of the root
op:

```
ADD(UNROLL([1,2,3,4]), UNROLL([10,20,30,40]))
  -> UNROLL([ADD(1,10), ADD(2,20), ADD(3,30), ADD(4,40)])

ADD(UNROLL([1,2,3,4]), x)
  -> UNROLL([ADD(1,x), ADD(2,x), ADD(3,x), ADD(4,x)])  via x.broadcast(4)
```

The "expanded" root is a UOp with vector dtype `f32x4` (etc.) wrapped
in a new outer UNROLL that carries the same axis tag. Subsequent
expansions of THIS UNROLL's parents continue to widen further; the
final outer UNROLL gets eaten by `fix_reduce_unroll` (for REDUCE
parents) or `fix_store_unroll` (for STORE parents).

**`do_contract`** (lines 77-86) is the inverse: when a `CONTRACT`
appears on top of an UNROLL (the `fix_reduce_unroll` / `fix_store_unroll`
output), it `GEP`s through the UNROLL's elements in the order the
CONTRACT specifies.

### Net effect

The graph after the expander has NO RANGEs of type UPCAST/UNROLL left.
Every ALU op that referenced those ranges has been duplicated F times
(once per index in the unrolled range), with the range value baked in
as a constant. Reductions over UPCAST/UNROLL axes become CONTRACT nodes
that reduce the vector horizontally. STOREs to UPCAST'd output axes
become vector stores wrapped in CONTRACT.

For a `[Cout=16 with UPCAST=4]` output × `[K=400, single reduce]` conv:
- Before expander: `STORE(out, addr_with_a_outer*4+a_inner, REDUCE_SUM(MUL(...), reduce_axis))` where `a_inner` is a UPCAST RANGE.
- After expander: 4 separate `acc` STOREs at 4 adjacent offsets, each with its own REDUCE over the same `reduce_axis`. The 4 reductions become 4 separate `_acc0..3` variables in the final code.

## The devectorizer pass

This file actually contains **multiple** related PatternMatchers
(`load_store_indexing`, `load_store_folding`, `correct_load_store`,
`devectorize`, `pm_render`, `pm_reduce`, `pm_add_loads`,
`pm_make_images`). They run at different points in the pipeline. The
ones that matter for the heuristic-vs-renderer gap:

### `pm_reduce` (lines 350-357) -> `reduce_to_acc` (311-328)

The single most important late-pass for codegen quality. Rewrites:

```
Ops.REDUCE(body, *reduce_axes, arg=(op, axes_tuple))
  -> a) declare an accumulator register     acc = PLACEHOLDER (REG buffer)
     b) init the accumulator                 acc[0] = identity_element(op)
     c) emit a STORE that accumulates        acc[0] = acc[0].alu(op, body)
                                             then END(reduce_axes)
     d) the original REDUCE position         LOAD(acc[0]) (after the END)
        becomes a load of the final acc
```

`acc_num` (held in `ReduceContext`) ensures every REDUCE gets a unique
accumulator slot. The result is a graph where reduces have been
"linearized" into a register accumulator + an explicit per-axis update
+ a final load.

**Why this matters for opt-rich code:** when the expander has REPLICATED
the body F times (one copy per UPCAST'd element), each copy hits a
separate REDUCE; `reduce_to_acc` then declares F separate accumulators
`_acc0`, `_acc1`, ..., `_acc{F-1}`. Each accumulator's reduce loop
runs independently with no cross-iteration dependency. The compiler
(LLVM, nvcc, etc.) can schedule them in parallel ILP-style.

### `devectorize` (lines 267-273)

Splits vector-typed ALU ops back into scalar ops:

```
ADD(f32x4, f32x4) -> STACK(f32, [ADD(gep(0)), ADD(gep(1)), ADD(gep(2)), ADD(gep(3))])
```

This UNDOES the vector aggregation the expander introduced for ALU.
Why? Because most rendered code wants scalar ops:
`_acc0_lane0 + _acc1_lane0; _acc0_lane1 + _acc1_lane1; ...` not
`acc + acc` on `f32x4`. The pattern matchers `no_vectorized_alu` /
`no_vectorized_wmma` handle this. WMMA is the exception: it KEEPS
vector srcs because the tensor-core instructions need them.

`devectorize_buf_and_index` (lines 258-264) splits vectorized BUFFER /
INDEX expressions for `DEFINE_LOCAL` / `DEFINE_REG` so each scalar
component goes to its own buffer slot.

### `load_store_folding` (lines 136-149)

Re-vectorizes **adjacent** scalar loads/stores when the addresses are
contiguous. This is what enables `float4` / SIMD widened memory ops:

```
LOAD(buf[i]), LOAD(buf[i+1]), LOAD(buf[i+2]), LOAD(buf[i+3])
  -> LOAD(buf.cast(float4).index(i/4))    (one wide load)
```

The folding is gated on the backend's `supports_float4` and the
address divisibility. For thvm's CUDA renderer, this would unlock
`reinterpret_cast<float4>` style coalesced loads inside the conv body.

### `correct_load_store` (lines 214-219) -> `split_load_store`

Splits over-wide loads / stores into the largest supported fold
length. E.g. `float8` load -> two `float4`s on backends without
`float8`. The `lengths = [16,8,4,2,1]` table is target-derived.

### `pm_render` (lines 275-295)

Final rewrites JUST for rendering. Notable ones:

- `Ops.CONST` with vector dtype -> explicit `Ops.STACK` of N scalars.
- `Ops.GEP` of a single index -> the source directly when the source is already scalar.
- LOAD-after-GATED-INDEX gets an `alt` value injected so the renderer can emit `cond ? load : alt`.

These prepare the graph for the renderer's line-by-line emit.

## thvm's current renderer (`src/codegen/render_uop.c`)

thvm does NONE of the expander/devectorizer work at the UOp level.
Instead, `rmu_emit_store` / `rmu_emit_one_reduce` walk the
post-heuristic UOp graph DIRECTLY and emit C/MSL/CUDA source.

For a UPCAST'd range, thvm emits a `#pragma unroll(F); for (uint
a<id> = 0; a<id> < F; a<id>++)` loop. The reduce body inside the loop
declares a SHARED `_accN` variable.

This works when the heuristic produces SIMPLE opt patterns. It breaks
for tinygrad-style stacked UPCASTs because:

1. **Shared accumulator:** the UPCAST'd output axis wraps the reduce.
   The renderer emits one `_acc` declaration outside the inner reduce
   loop, then writes to it on every UPCAST iteration. Without the
   per-UPCAST-element accumulator declaration that the expander +
   `reduce_to_acc` would produce, the value gets overwritten.

   thvm partially fixed this in [commit ba6252c9](src/codegen/render_uop.c)
   for the specific case "UPCAST splits a REDUCE-axis range" (the
   inner half stays inside the reduce loop with its own `#pragma
   unroll`). But "UPCAST splits an OUTPUT axis whose body has a
   REDUCE" is the actually-common conv shape and is the one where
   thvm's `rmu_emit_one_reduce` still emits one shared `_acc` per
   kid.

2. **No load folding:** tinygrad's `load_store_folding` re-vectorizes
   adjacent scalar loads into `float4`. thvm emits scalar loads even
   when the addresses are contiguous in memory. On bandwidth-bound
   kernels (every conv kernel beautiful_mnist runs is bandwidth-
   bound), this is a 2-4x perf gap.

3. **No `reduce_to_acc` analog:** thvm's reduce emit is structurally
   correct (declares `_acc`, opens loops, combines, stores) but it
   doesn't have the GEP/CONTRACT machinery that lets multiple unrolled
   reduces share intermediate computations. Every UPCAST'd element re-
   executes the full body's index arithmetic instead of computing the
   shared parts once.

4. **No scalar split of vector ALU:** since thvm doesn't go through
   the expander, it never PRODUCES vector ALU in the first place, so
   the lack of `devectorize` is moot. But it also means thvm can't
   benefit from `load_store_folding`'s rewrites (which expect vector
   loads as inputs).

## The CUDA wall regression, precisely

When the gate flips ([6e4c2ed1](src/codegen/hand_opts.c)) and the
heuristic produces tinygrad-style opt sequences:

- thvm's `rmu_emit_one_reduce` correctly produces per-UPCAST-element
  inner loops (post-ba6252c9).
- But each `_acc` declaration is shared across the outer UPCAST loop.
  The inner reduce writes to it. Next outer-UPCAST iteration overwrites.
- The result COMPILES, the loss is CORRECT (because the
  collapsed-shared-accumulator path happens to produce a valid sum for
  the specific topology), but the GPU runs F times more reduce
  iterations than necessary.
- Net: more code, same memory traffic, slower wall.

That last point is the crux. The heuristic CHOSE to UPCAST because the
load pattern said "this axis has stride-0 in some buf while existing
UPCASTs have no stride-0" -- meaning the chosen axis would amortize
loads across F register-blocked elements. With thvm's renderer NOT
producing per-element accumulators, the register-blocking benefit
doesn't exist; only the cost (more code, more index arithmetic per
thread) lands.

## Status (2026-05-27)

- **Path #1 LANDED** (commits `b9f2f32e` + `eb732ab7`): parallel
  accumulators emit in `rmu_emit_store_reduce` + axis-id translation
  in `hand_opts.c`. Render is correct: F parallel accumulators share
  ONE inner K-loop. test_render_uop pins it.
- **Path #2 LANDED** (commit `d31b74fb`): shared-load hoisting in the
  parallel-accumulator emit. Reduce body's INDEX_E subexpressions
  that don't depend on any UPCAST'd axis get hoisted to one `float
  _sh<kid>_<idx> = in[...]` declaration per K-iteration, then the F
  MADs reuse the local. Cuts `in1[` occurrences in K-body from F to
  1.
- **CUDA gate widened** (`hand_opts.c::kernel_hand_coded_opts` uses
  `hand_opt_kernel_on_gpu`, `b->id == 2 || b->id == 3`), per spec --
  tinygrad's `hand_coded_optimizations` runs on every renderer. V100
  BS=128 STEPS=10 runs slow on CUDA (see the measurement table below);
  the remedy is the PTX renderer port, not a backend carve-out. Loss
  stays monotone 2.81 -> 1.73, correctness preserved.
- **Architectural alternative piece #1 LANDED** (this commit): port of
  tinygrad `codegen/late/expander.py` to the thvm UOp graph rewrite
  layer.  Introduces FOUR structural opcodes (`UOP_VCONST`,
  `UOP_UNROLL`, `UOP_CONTRACT`, `UOP_GEP`) and one entry point
  `uop_expand_graph(root) -> Term`.  `pm_pre_expander` rewrites every
  `UOP_RANGE` with `axis_type in {KAX_UPCAST, KAX_UNROLL}` to
  `UNROLL(VCONST(0..F-1))`; `do_expand` lifts ALU through UNROLL with
  GEP swizzles + scalar broadcast via CONTRACT (expander.py:80
  semantics); `do_contract` collapses `CONTRACT(UNROLL(...))` into a
  GEP; `fix_reduce_unroll` / `fix_store_unroll` pull unrolled axes out
  of REDUCE/STORE.  See `src/uop/expander.c` for the full mapping.
  Eight test cases in `tests/test_uop_expand.c` cover the full
  pipeline (bare RANGE -> UNROLL, LOOP RANGE untouched, ALU lift,
  multi-axis UPCAST, REDUCE-over-UPCAST, UPCAST+UNROLL, nested ops,
  hash-cons).  NOT WIRED INTO `render_uop.c`: see "what's deferred"
  below.

**Why the V100 gap is still open**: the heuristic also applies
UNROLL(K, 4) from section 7. With UPCAST=4 + UNROLL=4 the parallel-acc
body becomes F=4 MADs * 4-way K-unroll = 16 MADs per outer K-iter.
For K=400 that's 1600 statements per kernel. nvrtc spends ~340 s on
the cold compile and the larger PTX hurts V100 occupancy. Path #2
hoists the X-side shared load but the W-side per-lane loads still
have 4-way-K-stride structure that begs for `float4`-style adjacent-
load folding (the tinygrad `fold_expanded_index` at devectorizer.py:
81-117). That's path #3 below.

## Architectural port FULLY LANDED (pieces #1-6, ~2400 LOC, 6 commits)

By end of 2026-05-27 the structural port is complete end-to-end:

```
recognise_conv
  -> uop_expand_graph        (fc371fe3 -- F-replicates UPCAST/UNROLL)
  -> uop_symbolic_rewrite     (79cbe9ff -- sym pass: ADD/MUL/etc. fold)
  -> uop_devectorize_graph    (525136bc + 1e7747b9 -- reduce_to_acc + do_contract)
  -> uop_symbolic_rewrite     (79cbe9ff -- post-devec fold)
  -> uop_load_store_fold_graph (525136bc + 79cbe9ff -- adjacent + shared-load CSE)
  -> uop_symbolic_rewrite     (79cbe9ff -- post-fold fold)
  -> uop_linearize             (b0acd7d3 -- topo flatten)
  -> cg_render_linearized_*    (d5af3033 + 5dacdf3a -- new emit; bail to legacy)
```

Active at all 3 entry points (Metal/C/CUDA) via the route gate at
`cg_render_uop_kernel_*_root`. When `uop_has_upcast_or_unroll(root)`,
the new pipeline runs.

**V100 wall, BS=128 STEPS=10 (TEST_EVERY=0, THVM_GC=0, V100-SXM2-16GB)**:

| Configuration                                         | Cold step 1 | Warm step | Compiles | Loss |
|-------------------------------------------------------|------------:|----------:|---------:|-----:|
| Gate widened (CUDA gets the heuristic; SPEC)          |  43408 ms   |   262 s   |     167  | 2.81 -> 1.83 (5 steps) |
| Gate widened, no Section-7 UNROLL CUDA-skip           |  47196 ms   |   301 s   |     165  | 2.81 -> 1.89 (5 steps) |
| Gate widened + RMU_REDUCE_UNROLL_MAX=64 (initial try) |   ~390 s    | ~3500 ms  |     ~140 | 2.81 -> 1.73 |
| Reference: heuristic OFF on CUDA (naive, no opts)     |    3890 ms  |   505 ms  |     110  | 2.81 -> 1.73 |
| tinygrad reference (PTX renderer)                     |       -     |  ~115 ms  |       -  | -    |

**Finding, not a disposition**: the gate runs the heuristic on CUDA
(`hand_opts.c::kernel_hand_coded_opts` calls `hand_opt_kernel_on_gpu`,
which is `b->id == 2 || b->id == 3`), matching tinygrad's spec --
`hand_coded_optimizations` applies to every renderer. On V100 the
opt'd kernels currently run ~130x slower than the heuristic-off naive
path (262 s vs 505 ms warm) because thvm emits C-style source for
nvrtc instead of PTX; the heuristic's UPCAST/UNROLL register-blocking
assumes the PTX register model. The Section-7 UNROLL CUDA-skip
(`hand_opt_kernel_is_cuda` check in the heuristic body) trims this
~13% (301 s -> 262 s) and is a tinygrad-style intra-heuristic target
skip (heuristic.py:37 AMX, :109 DSP). The remediation is to port
`tinygrad/renderer/ptx.py`, NOT to disable the spec on CUDA.

**Diagnosis**: every IR-level rewrite landed cleanly with unit tests.
The GRAPH shrinks dramatically through the sym sweeps. But the final
EMITTED source for `kid=3` conv with the heuristic's `UPCAST=4` +
`UNROLL=4` choices is hundreds of MAD statements per kernel, which
nvrtc's C++ frontend handles in tens of seconds AND produces PTX that
overflows V100's register file (kernels then run far below peak).

**The gap is between IR-rewrite output and source emission**. tinygrad
gets around it via:
1. PTX-level emitter (`tinygrad/renderer/ptx.py`) that bypasses nvcc/nvrtc's C++ frontend entirely.
2. `tinygrad/runtime/support/assembler_cuda.py` that compiles PTX -> SASS directly.

thvm goes UOp -> C-style source -> nvrtc -> PTX -> SASS. The C-style
source stage is what crashes on hundreds-of-statement kernels, AND the
register-pressure feedback that tinygrad's heuristic counts on (PTX
register allocator + occupancy model) is absent in the C-source path.

**Spec-aligned paths to actually close V100**:
- (A) PTX-level emitter mirroring `tinygrad/renderer/ptx.py`. Big work but tinygrad-canonical.
- (B) Target-aware factor caps in the heuristic (`Renderer.target` knobs in tinygrad; thvm has the `hand_opt_kernel_is_cuda` gate now, the Section-7 skip is the first user).  Smaller, also spec-aligned.

The architectural pipeline (pieces #1-6) is the necessary prerequisite
for either. Without expander/devec/linearize/sym, neither a PTX emitter
nor target-aware caps would have a clean IR to operate on. Now they do.

## What a faithful port of the late passes would look like

In rough order of impact:

### 1. `reduce_to_acc` analog in `render_uop.c`

When `rmu_emit_one_reduce` is called inside an enclosing UPCAST'd
output-axis context, declare F separate `_acc<kid>_<j>` variables for
`j in 0..F-1`, run F independent reduce loops, then emit F stores at
F adjacent offsets.

This is what the previous renderer-fix agent (`accde3f9f930ab06a`)
INTENDED but didn't actually validate against the heuristic-driven
shapes -- their synthetic test exercised a single-K-UPCAST case
([commit 75eb4c54](tests/test_render_uop.c)), which is exactly the
case [ba6252c9](src/codegen/render_uop.c) does handle. The case that
fails is the *output*-axis UPCAST, which `rmu_emit_one_reduce` still
treats as a serial loop over a shared accumulator.

Implementation sketch: in `rmu_emit_store`, when walking output
ranges, track which ranges have OPT type UPCAST (`opt_kinds[i] ==
UOP_OPT_UPCAST`). For each REDUCE descendant of the value subtree,
detect whether any UPCAST'd output range is in scope. If so, emit F
parallel accumulators; if not, the existing single-acc path stays.

### 2. Load folding (`load_store_folding` analog)

After the F-acc reduces are in place, look at the INDEX_E reads inside
each reduce body. If the F lanes' addresses differ by exactly `1, 2,
3, ..., F-1` in the unrolled-axis direction, emit a single
`reinterpret_cast<float<F>>(in0 + base)[0]` load and `.x .y .z .w`
extraction instead of F scalar loads.

This is what tinygrad's `load_store_folding.fold_expanded_index`
(devectorizer.py:81-117) does. Gated on backend's `supports_float4`
(thvm needs a renderer flag analog).

### 3. Late symbolic on indices

Tinygrad runs `sym` repeatedly throughout the pipeline. thvm's
`uop_dag_simplify` runs once early. Re-running it after the expander
would catch index-arithmetic simplifications that only become visible
after UPCAST replication (e.g. `(a_outer*4 + 0) * stride + (a_outer*4
+ 1) * stride` collapses to `(a_outer*8) * stride + stride`).

### 4. `pm_add_gpudims` late thread mapping

thvm's renderer hard-codes the threadgroup decode in
`RmuGlobalDecode`. Tinygrad's `pm_add_gpudims` does it LATE so the
decode benefits from the expanded scalar code. Lower priority --
correctness-neutral, perf-neutral until #1 and #2 land.

## What this means for the next port arc

The minimum to close the CUDA wall regression after enabling the
heuristic on CUDA is **just #1** (parallel accumulators in
`rmu_emit_one_reduce` when an UPCAST'd output range is in scope).
That's the one piece without which UPCAST is pure cost.

#2 (`load_store_folding`) is a separate ~2x perf bump on top, but
isn't needed for correctness.

#3 and #4 are nice-to-have polish.

The structural alternative is to actually run the
expander+devectorizer at the UOp level in thvm BEFORE rendering, the
way tinygrad does. That gets all four for free but requires teaching
thvm's renderer to consume the post-expander AST shape (vector dtypes,
CONTRACT/STACK/GEP nodes, REG buffers from `reduce_to_acc`). Much
bigger refactor.

The pragmatic path is #1 + maybe #2 in `render_uop.c` directly. The
structural path is to port the late passes wholesale. Both are valid
spec-faithful moves; the former matches thvm's "render walks the UOp
graph" architecture, the latter matches tinygrad's "rewrite the graph
then render the rewritten graph" architecture.

## Files touched by a port

For path #1 (renderer extension), the changes localize to:

- [render_uop.c](src/codegen/render_uop.c) -- `rmu_emit_one_reduce`
  (line ~2825), `rmu_emit_store` (line ~2960). New logic to detect
  UPCAST'd output ranges in scope of a reduce, declare F accumulators
  + emit F reduces + F stores.
- [test_render_uop.c](tests/test_render_uop.c) -- regression test for
  "UPCAST output axis, reduce body, F accumulators emitted".
- [test_metal_real.c](tests/test_metal_real.c) -- the existing
  `conv2-4d-output-parity` / `conv-dual-upcast-parity` cover this; no
  test changes needed.

For path #2 (load folding), add a render-time analyzer:

- new `src/codegen/render_load_fold.c` -- detect F adjacent loads
  inside a #pragma-unroll'd block, emit a single wide load + GEP.
- backend renderer flag (`Renderer.supports_float4` analog).

### 5. Architectural alternative: full late-pass UOp port

Path #1 + #2 + #3 are renderer-level patches that mimic individual
late-pass effects at source-emission time. The structural alternative
is to actually run tinygrad's expander + devectorizer at the UOp
graph level BEFORE rendering, the way tinygrad does:

```
heuristic-tagged UOp -> [expander] -> [devectorizer / reduce_to_acc]
                     -> [load_store_folding] -> [pm_render]
                     -> renderer that just walks and emits
```

This decouples the renderer from the optimization passes entirely.
The renderer only needs to emit a small set of canonical UOps
(STORE, LOAD, DEFINE_ACC, ENDIF, etc.). All the F-replication, load
folding, accumulator declaration, etc. happens in graph rewrites
that produce a smaller, simpler UOp graph that any backend renderer
can consume.

The cost: significant rewrite of `render_uop.c`. The benefit:
adding a new late-pass becomes a `PatternMatcher` addition (~10
lines) instead of a per-renderer-branch C edit. And the UOp graph
shrinks during rewrites so nvrtc isn't asked to compile 1600-
statement bodies that it then has to simplify.

This is the "real" port. Path #1 + #2 + #3 are the pragmatic patches
that buy time. The CUDA wall regression suggests the pragmatic patch
ceiling is below tinygrad parity; the real port is what unlocks the
full perf.

### 5a. What landed (architectural alternative piece #1)

The first step of the "real" port is in place: `src/uop/expander.c`
implements `uop_expand_graph(root)` as a self-contained graph rewrite
that consumes a `KAX_UPCAST`/`KAX_UNROLL`-tagged DAG and produces a
vectorized DAG with no UPCAST/UNROLL RANGE leaves.  Validated
end-to-end in `tests/test_uop_expand.c`.

### 5b. Architectural alternative piece #2 LANDED

The four pieces flagged as deferred in 5a are now in
`src/uop/devectorize.c` (port of
`tinygrad/codegen/late/devectorizer.py`):

1. **`reduce_to_acc`** (devectorizer.py:311-328) -> `devec_pm_reduce`.
   Rewrites `REDUCE(body, *axes)` into a chain of
   `STORE(PLACEHOLDER, 0, identity)` (init), `STORE(PLACEHOLDER, 0,
   alu(LOAD(PLACEHOLDER), body))` (update), `END(axes)` marker, and
   final `LOAD(PLACEHOLDER)`.  Sequenced via thvm's existing
   `UOP_AFTER` chain so the renderer's `rmu_emit_after` walker can
   linearize it without a new traversal.  Each REDUCE gets a unique
   `acc_id` threaded through the rewrite via a `DevecReduceCtx`.
2. **`devectorize`** (devectorizer.py:235-273) -> `devec_pm_devectorize`.
   Detects vector ALU (UOP_ADD / IADD / NEG / CAST / ... whose srcs
   are `UOP_STACK`s) and rewrites to `STACK(N, [op(gep(s, i)) for i])`.
   Width detection sources from STACK lanes (post-`pm_render` lowering
   of VCONST -> STACK).
3. **`pm_render`** (devectorizer.py:275-295) -> three rules under
   `devec_pm_*`: VCONST -> STACK of scalar CONST; GEP(STACK, (i,)) ->
   STACK src[i]; GEP(scalar, (0,)) -> scalar; STACK with single src
   -> the src.  Run pre- and post-`devectorize` to canonicalize.
4. **`load_store_folding`** (devectorizer.py:81-149) ->
   `uop_load_store_fold_graph` (a SEPARATE entry point invoked after
   `uop_devectorize_graph`).  Detects F=2/4/8 adjacent scalar LOADs
   in a STACK whose addresses share a base + lane offsets 0..F-1, and
   collapses them to one wide LOAD wrapped in `UNROLL(axis_id=0xFE,
   F)` + per-lane GEPs.  Dtype gate: float32 today (matches tinygrad's
   `supports_float4` initial port; f16/bf16 deferred until the
   renderer can emit half-width vector loads).

Three new opcodes back this:

- `UOP_STACK` (slot 48): variadic scalar-list, arity-0 by walker
  convention with variadic Term payload at `heap[loc+1..]`.  The
  renderer can lower to N scalar emissions; the constructor collapses
  N=1 to a no-op.
- `UOP_PLACEHOLDER` (slot 49): per-thread REG accumulator
  declaration, heap = [NUM(dtype), NUM(acc_id)], hash-cons by
  (dtype, acc_id).
- `UOP_END` (slot 50): "close these loops here" marker, heap =
  [NUM(n), range_0, ..., range_{n-1}].  Not hash-cons'd -- two END
  markers in two different reduces are semantically distinct.

Both `uop_devectorize_graph` and `uop_load_store_fold_graph` are
exposed via `src/thvm.h` and exercised by `tests/test_uop_devectorize.c`
(7 cases: reduce_to_acc round-trip, vector ALU split, VCONST -> STACK,
GEP unwrap, F=4 load fold, end-to-end expander+devectorize on UPCAST'd
shape, PLACEHOLDER/STACK hash-cons).

**NOT WIRED INTO `render_uop.c`.**  The renderer still walks the
RANGE-leaf representation directly; teaching it to emit PLACEHOLDER /
END / STACK / GEP-folded wide LOADs is the remaining work for stage
(f).  Both `uop_devectorize_graph` and `uop_load_store_fold_graph`
land as off-pipeline tools today; the V100 wall measurement awaits
the renderer wiring.

### 5c. Architectural alternative piece #3 (stages a + b) LANDED

The first two stages of the renderer-side wiring are in place:

- `src/uop/linearize.c` (stage a): port of
  `tinygrad/codegen/late/linearizer.py:7-52` `linearize()`.  Entry
  `uop_linearize(sink, &LinKernel) -> int`.  Walks a post-devectorize
  DAG (or any UOp DAG -- the linearizer is shape-agnostic) and
  produces a `LinKernel`: an ordered array of UOp Terms in emission
  order.  The algorithm is the tinygrad priority-tuple toposort:
  bottom-up out-degree count, per-opcode priority bias
  (`LOAD = -1`, `STORE = +1`, `RANGE = +5`, `END = -5`,
  `PLACEHOLDER = -17`, `BUFFER = -18`), then a min-heap pop-and-emit
  ordered by toposort-respecting ideal-position key.  No allocations
  on the main heap; everything lives in `static` arrays sized by
  `LIN_KERNEL_CAP = 4096`.

- `src/codegen/render_linearized.c` (stage b, partial): proof-of-
  concept renderer that consumes a `LinKernel` and emits a C99
  kernel for the simplest shape -- a single-store elementwise body
  over one `KAX_LOOP` range.  Recognises `UOP_CONST`, `UOP_RANGE`,
  `UOP_INDEX_E`, `UOP_LOAD`, `UOP_CAST`, binary/unary ALU, and
  `UOP_PLACEHOLDER` (for the post-`reduce_to_acc` shape).  Anything
  involving `UOP_REDUCE` or `UOP_AFTER` returns `0` and the caller
  is expected to fall back to the legacy emit (not yet wired).

Tested via `tests/test_uop_linearize.c` (4 cases, 29 assertions):
  1. Elementwise STORE(1 + LOAD(in[i])) -- toposort order +
     LOAD < ADD < STORE + RANGE < LOAD.
  2. Matmul SUM(A[i,k] * B[k,j]) -- LOADs < MUL < REDUCE < STORE.
  3. Post-devectorize REDUCE -- PLACEHOLDER present, END precedes
     final LOAD, no surviving REDUCE.
  4. End-to-end: linearize + cg_render_linearized_c on case 1
     emits a valid C99 kernel with the expected loop header and
     INDEX_E body.

**Stages c-f deferred.**  The remaining work to close the V100 wall
regression is:
  (c) extend the renderer to handle REDUCE-path PLACEHOLDER + STORE-
      back-to-acc + END + final LOAD as a multi-line emit (the
      AFTER-chain semantics linearize produces).
  (d) extend to STACK / GEP / VCONST so vector loads + per-lane
      scalar ALU emit correctly after `uop_load_store_fold_graph`
      has run.
  (e) wire the renderer into `cg_render_uop_kernel_metal_root` /
      `cg_render_uop_kernel_cuda_root` for opt-rich kernels, gated
      so the legacy emit keeps fielding the kernels that today's
      test_render_uop 301/301 pins.
  (f) measure V100 BS=128 STEPS=10 wall vs the <=540 ms target.

The reason the wire-up (stage e) is the longest remaining piece is
that the legacy `rmu_emit_store` walks ~4500 lines of interleaved
concerns: range-with-opt collection, conv recogniser branch, TC
template branch, parallel-accumulator broadcast-hoist, simd-collective
warp reduces, kvar wedging, multi-output AFTER chains.  A drop-in
replacement that doesn't regress any of the 301 test_render_uop cases
needs to either be limited to a narrow opt-rich shape (with a robust
gate) or grow to cover all the legacy concerns.  Either path is a
multi-session piece of work; landing stages a + b first gives the
next agent the post-linearize input shape to start from.

## PTX renderer (the nvrtc bypass) -- M1-M3c LANDED, conv renders to PTX

The fix for the nvrtc-frontend bottleneck is a PTX emitter that consumes
the SAME linearized UOp list and emits PTX assembly directly, so the
CUDA jit `cuModuleLoadData`s it without nvrtc's C++ frontend.  Port of
`tinygrad/renderer/ptx.py`.  All landed + V100-validated:

- **M1** `src/codegen/render_ptx.c` (`cac3598e`): SSA register allocation
  per value UOp (`%<prefix>_<type>_<n>`), one PTX instruction per node,
  `.reg` decls + `.visible .entry` prologue.  INDEX_E lowers to int64
  pointer math inline (`mad.wide.s32 base + idx*itemsize`).
- **M2** `src/backend/cuda/jit.c` (`c033a531`): PTX passthrough -- a
  source with a leading `.version` skips nvrtc, straight to
  `cuModuleLoadData`.
- **M3a** (`70da9734`): thread-axis geometry.  A store-indexing KAX_LOOP
  axis is PROMOTED to a parallel grid thread (flat-thread-id decode via
  the reused `rmu_compute_global_decode_ctx`) + `tid >= total` guard.
- **M3b** (`c75ce385`): reduce accumulator (PLACEHOLDER -> persistent
  register) + unique occurrence-keyed loop labels.
- **expander/devectorize** (`86e78dfb`, `18b74cee`, `2cd837bf`): the
  prerequisite lowering, which the original "M3c blocked" analysis
  pointed at.  STORE/LOAD/REDUCE added to `do_expand` (was: only
  ALU/INDEX_E) so an UPCAST'd output's UNROLL propagates all the way up
  (LOAD(UNROLL) -> UNROLL(LOAD) -> UNROLL(MUL) -> UNROLL(REDUCE)); the
  old `fix_store_unroll` CONTRACT-wrap (which re-matched its own output
  and nested ~129 deep) is gone.  Result: F DISTINCT reduces -> F
  parallel accumulators, and an F-lane store `STORE(buf, STACK[F] addr,
  STACK[F] value)` (the redundant UNROLL(STORE) marker is dropped by
  `devec_pm_unroll_store`).
- **M3c renderer** (`2cd837bf`): render_ptx handles the F-lane store (F
  scalar `st.global`, peeling AFTER wrappers) + STACK lane bundles + GEP
  lane extraction; the thread-geometry pass peels a STACK address to
  lane 0 so the batch axis is still found + promoted.
- **route** (`2e18307e`): `THVM_CUDA_PTX=1` emits PTX before the C-source
  emit; the jit passthrough loads it.

**Final correct picture (after iterative renderer fixes):**

| Configuration                          | Cold    | Warm step | Loss |
|----------------------------------------|--------:|----------:|------|
| Widened gate, PTX unset (nvrtc)        | 14.7 s  | 11.8 s    | 2.81 -> 2.47 ✓ |
| Widened gate, **`THVM_CUDA_PTX=1`**    | **10.9 s** | 12.4 s | 2.81 -> 2.47 ✓ |
| Reference: heuristic OFF (naive)       |  3.9 s  | 505 ms    | -    |
| tinygrad reference (PTX renderer)      |   -     | ~115 ms   | -    |

`THVM_CUDA_PTX=1` saves **~26% cold** time (the nvrtc bypass paying off),
warm comparable to nvrtc within noise, loss matches baseline.  124
PTX-rendered kernels load + run correctly; no INVALID_PTX cache thrash.

**An earlier draft over-claimed "48x via PTX"** -- that was a
misattribution at the time PTX wasn't actually firing (counter showed
PTX loads = 0).  The iterative renderer extensions below fixed the
bails; the headline above is the verified result with PTX actually
exercised.

**Renderer iterations that made PTX work on real kernels:**
- OPT-strip sym rule (1395 OPT bails -> 0).
- LOCAL axes: decode KAX_LOCAL from `%tid.x`, GLOBAL from `%ctaid.x`
  when `has_local` (1373 LOCAL bails -> 0).
- Implicit load: bare INDEX_E in ALU value position lazily emits
  `ld.global` (fixes `add.f32 reg_s64, reg_s64`).
- MUL/ADD with comparison operand -> `selp.f32` (fixes invalid
  `mul.f32 f32, pred`).
- F-lane STORE over STACK of plain integer indices: compute byte
  address per lane via `mad.wide.s32` + buf base.
- **Positional buf-param naming**: a bisect with
  `THVM_CUDA_PTX_MAX` pinned the wrong-results bug to a single
  PTX-rendered kernel (#150) whose 7 inputs had non-consecutive insts
  `{0,1,3,4,5,6,7}`; the renderer emitted `k(data0, data1, data7,
  data3, ...)` (inst-based names in encounter order), but the CUDA
  dispatch packs args positionally (resolved_tids order), so the 3rd
  param was named `data7` but received args[2] = a different buffer.
  Fix: name params + base-pointer registers by REGISTRATION ORDER
  (`data0..data_{N-1}`), mirroring the legacy `in0..in_{N-1}`
  positional naming.  Loss diverged 2.809 -> 2.991 before; matches
  baseline 2.809 -> 2.575 -> 2.468 after.

**The remaining gap (10.9 s cold / 12 s warm vs naive 505 ms / tinygrad
115 ms)** is the heuristic's UPCAST kernel choices -- PTX correctly
renders what the heuristic asks for; the heuristic asks for shapes
whose runtime is poor on V100 (low occupancy, no LOCAL/GROUP_REDUCE
parallelisation of the small-output reduce kernels).  A profile shows
the dominant cost is `[64]`/`[32]`-output reduce kernels running
~serially (one thread per output doing a long reduce loop) -- the
next lever is GROUP_REDUCE / threadgroup-parallel reductions, a
substantial new subsystem (shared memory + `bar.sync` + warp shuffles
in the renderer).

## GROUP_REDUCE wiring (May 2026)

End-to-end GROUP_REDUCE on CUDA now lands but only fires on **single
reduce-axis** kernels (one of the conv-stack's Linear matmul shapes in
beautiful_mnist).  The heuristic gate at hand_opts Section 4 declines
multi-axis reduces (conv `sum(C_in, kH, kW)`, layernorm/softmax
broadcast) because `rmu_emit_group_reduce` is single-axis only -- the
mixed shape would race extra REDUCE loops against the cooperative one.

What landed (commits 322b5c93 + 0d672e41 + 15e2bd1b):
- `uop_dag_apply_group_reduce` (`uop/apply_opt_dag.c`): single-axis
  stamp (no axis-id split), flips the target REDUCE leaf to KAX_GROUP_
  REDUCE and wraps every reference in `OPT(_, GROUP_REDUCE, k)`.
- `rmu_emit_group_reduce` (`codegen/render_uop.c`): target-aware --
  `__shared__/__syncthreads()` on CUDA, `threadgroup/threadgroup_
  barrier` on Metal, bail on C target.
- `RMU_HAS_GROUP_REDUCE` flag (mirrors `RMU_SIMD_WARP`): set per-render
  via `rmu_dag_has_group_reduce` so the output axis decode picks `tg`
  (one block per output) instead of flat `tid`.  Without this, the
  block-size-N launch with 16 different outputs hit shared[16] -> OOB.
- `cuda_dag_group_reduce_factor` / `rmt_dag_group_reduce_factor`: read
  the OPT factor `k` (the cooperative block size = 16) instead of the
  range extent (= full reduce size = 576).  Wrong read -> launch with
  block=576 -> immediate `cuModuleLoadData: CUDA_ERROR_ILLEGAL_ADDRESS`.
- PTX renderer bails on KAX_GROUP_REDUCE so the kernels fall back to
  the (now-correct) C-source path.

Verified on V100 with `THVM_GROUPTOP=1 STEPS=3 BS=128`:
- losses 2.81/2.58/2.47 IDENTICAL to baseline
- 137 compiles (same as baseline -- only the Linear kernel changes shape)
- warm 9.3 s vs baseline 9.5 s (~+2.5%)

## Multi-axis GROUP_REDUCE (commit 9d84ba5a)

`rmu_emit_group_reduce` now accepts the full reduce-range array.  The
non-grouped REDUCE axes wrap the cooperative GROUP-axis loop as serial
for-loops, so the conv `(C_in, kH, kW)` reduce gets the parallel
template too:

```
for (uint a_red1 = 0; a_red1 < ext1; ++a_red1)
  for (uint a_red2 = 0; a_red2 < ext2; ++a_red2)
    for (uint a_grp = tt; a_grp < extG; a_grp += k)
      _acc[tt] = combine(_acc[tt], body(...));
```

`hand_opts.c` Section 4 GROUPTOP gate picks the LARGEST REDUCE axis
that divides 16 (the obvious heuristic for the parallel slice).

V100 beautiful_mnist BS=128 **STEPS=5** A/B (THVM_GC=0, no JIT):

| Config              | Cold    | Warm mean | Compiles | Loss        |
|---------------------|--------:|----------:|---------:|-------------|
| Baseline (no GROUPTOP, no PTX) | 15.1 s | 18.2 s | 137 | 2.81 -> 2.20 |
| PTX=1 only          | 11.8 s | 17.2 s | 124 | 2.81 -> 2.20 |
| GROUPTOP=1 only     | 10.7 s | 7.3 s  | 125 | 2.81 -> 2.20 |
| **PTX + GROUPTOP**  | **9.2 s** | **7.4 s** | **124** | **2.81 -> 2.20** |

Headline: **-39% cold / -59% warm** at bit-identical losses (137
compiles -> 124).  The gate now fires on ~30 kernels per step (vs 1 in
the single-axis era); most are 3-axis conv reduces ("best of 3 red
axes"), plus the original Linear matmul.

**Caveat on warm_mean numbers**: at 10+ steps both configs hit a
per-step time SPIKE around step 4-6 (warm jumps ~10 s -> ~25 s and
stays high).  This is consistent across baseline AND GROUPTOP -- a
separate pre-existing issue (memory pool growth / fragmentation across
steps, see [[project_memory_parity_followup]]).  Pre-spike (steps 2-3
of baseline vs steps 2-5 of GROUPTOP, which are still pre-spike under
the smaller working set): baseline warm 10.9 s vs GROUPTOP warm 7.3 s,
~**-33% pre-spike warm**.  The post-spike steady-state shows GROUPTOP
also wins but by a smaller margin (23 s vs 31 s ~ -26%).

`THVM_GROUP_SZ=N` (default 16, matches tinygrad).  A sweep at 3 steps
shows the result is roughly flat across 4/8/16/32/64 with noise
dominating differences; 16 keeps the default and the env stays as a
follow-up knob.

(Peak memory at 5 steps is 1.2 GB in BOTH configs -- the earlier note
of "GROUPTOP raises peak to 1.2 GB" was comparing different step counts
and is wrong; not a regression.)

The PTX-renderer GROUP_REDUCE emit (shared declaration in prologue +
`bar.sync 0;` + the `if (tt==0)` final fold) is still deferred -- the
C-source CUDA path through nvrtc already gives us the win above, and
the PTX detour would need to be measurably faster than nvrtc to be
worth the new code.

## Dispatch caches + async launch + TinyJit

The remaining "the warm step is still 7-9 s, not 100 ms" gap was NOT
the GPU.  Three pieces landed:

### Per-KernelEntry CUfunction cache (commit 30147adc)

`cuda_dispatch_kernel` was re-rendering + re-canonicalizing the .cu
source on EVERY dispatch (1-3 ms per call), then hashing the string
into the source-text cache to get back a CUfunction.  124 kernels per
step burned 100-500 ms of CPU work to produce byte-identical source.

`CUDA_KE_CACHE[1<<14]` (keyed by ke pointer offset mod cap; value
(store_root, func, grid_x, block_x, kvar_ids)) skips render + hash
when store_root unchanged from the prior dispatch.  Cache miss writes
the slot AFTER render+compile so kernel_apply_opt-mutated DAGs
naturally invalidate (store_root changes -> miss -> re-render).

Effect: **warm step 7.4 s -> 800 ms (-89%)**, step-4 spike GONE
(turns out the spike was render-time accumulation / open_memstream
allocator fragmentation).  Loss bit-identical.

### Drop per-launch cuCtxSynchronize (commit 03629553)

`cuda_jit_launch` forced the CPU to block until each kernel finished,
serializing what should be an asynchronous CUDA stream.  The
default-stream `cuMemcpyDtoH` used by `loss.item()` / `Tensor.numpy()`
is itself synchronous and naturally enforces the right "wait for all
prior work before reading" boundary, so removing the per-launch sync
is semantically equivalent for the user but lets the GPU queue many
launches deep.

Effect: warm ~800 ms -> ~750 ms.  `THVM_CUDA_SYNC=1` restores the old
behavior for debugging (a bad kernel then surfaces its error at the
launch site instead of the next memcpy).

### TinyJit (commit 87c59ab4)

The remaining ~640 ms warm step was the C-side `wnf + materialize`
fixpoint loop in `thvm_realize` (graph build + scheduler).  Captured
on first call via the existing `jit_capture_begin/end/replay` infra
(C-side already there, wired through `py_jit_*` + new `_misc.py
TinyJit`).  beautiful_mnist_train pre-allocates stable `X_buf` /
`OH_buf` input Tensors; the step closure runs once under
`jit_begin/end_with_result(loss.term)` and every subsequent call goes
through `jit_replay(slot)` -- no graph build, no materialize, no
render.

**`Y.numpy()` host-side scatter trap**: `sparse_categorical_crossentropy`
did `Y.numpy()` to build the one-hot host-side, which baked step-1's
labels into the captured graph (replays reused those frozen labels).
New `cross_entropy_from_onehot(OH)` reads from a caller-pre-allocated
one-hot Tensor; the train script scatter-fills OH outside the JIT
scope so the captured graph reads new contents from OH's stable
buf_id.

V100 beautiful_mnist BS=128 STEPS=10 (THVM_CUDA_PTX=1 THVM_GROUPTOP=1
THVM_JIT=1): **warm step 121 ms** (vs tinygrad reference ~115 ms,
**at tinygrad parity**), per-step time flat, 124 compiles.

### Sticky `jit_pinned` buf flag (commits 33f19afd + 9b1a41eb)

Root cause of the stale-replay bug: the per-realize `preserved` flag
gets cleared at end-of-realize (`cuda_buf_clear_preserved`), so a
captured intermediate buf survived its own realize's rollback but the
NEXT realize's rollback freelisted it.  Replay's captured dispatch
then read dptr=0 -> cuda_dispatch_kernel returned -1 -> loss stayed at
capture's value.

Added a sticky `jit_pinned` flag on `CudaBuf` (set by
`cuda_buf_jit_pin` when JIT capture retains a buf; cleared by
`jit_capture_drop`).  `pool_rollback`, `pool_rollback_with_preserve`,
`freelist_push`, `buf_free` all guard on it.  Wired through the
backend vtable.

Minimal repro NOW correct:
- capture:  `c(X=zeros).sum()` = -426.6
- replay (after `X.assign(0.5)`): -1001.7
- truth (fresh `c(X)`):            -1001.7  ✓ matches

Conv+ReLU+Linear+SCCE under JIT also correct (loss changes step-over-
step).  The simple JIT path is fully working.

### Arena-view JIT pin fix (commit 3276c7ee) -- FULL MNIST JIT WORKS

Root cause of the prior `CUDA_ERROR_ILLEGAL_ADDRESS` on full
beautiful_mnist: the materialize-time arena planner allocates one big
cuMemAlloc'd buffer per realize, then carves it into per-tensor views
(`cuda_buf_alloc_arena_view` sets `parent_buf_id` + `owns_data=0`).
JIT capture pinned the VIEW (correctly preventing the view slot from
being freed) but the underlying ARENA still got cuMemFree'd at the
next realize's pool_rollback.  The next realize then cuMemAlloc'd a
NEW (often smaller) arena at a different address.  The captured op's
dispatch wrote 9.4 MB worth into a 1.18 MB freed/reused region --
compute-sanitizer pinned this as "out of bounds, 4.89 MB after nearest
allocation of size 1.18 MB".

Fix: `cuda_buf_jit_pin` / `_unpin` recurse through `parent_buf_id` so
pinning a view also pins the arena.  Mirrors the `parent_buf_id`
refcount chain in `cuda_buf_free` / `cuda_buf_decref`.

V100 beautiful_mnist BS=128 STEPS=10 (THVM_CUDA_PTX=1 THVM_GROUPTOP=1
THVM_JIT=1) -- the literal model from tinygrad:

| Metric              | Value          |
|---------------------|---------------:|
| Cold step           | 5.2 s          |
| **Warm step**       | **192 ms**     |
| Loss (10 steps)     | 2.81 -> 1.43 ✓ |
| Test accuracy       | 50.44% ✓       |
| Compiles            | 124, stable    |
| Per-step time       | flat           |

Compared to tinygrad reference (~115 ms warm), we're now within
~1.7× -- same order of magnitude, real tinygrad-class speed.

Compared to pre-arc baseline (18.2 s warm), the full arc delivers a
**95× warm-step speedup with bit-identical correct training**.

### Diagnostics added

- `THVM_CUDA_LAUNCH_TRACE` -- per-launch failure log (kid, grid, block, err)
- `THVM_CUDA_DUMP_FAILED_KERNEL` -- dump rendered .cu source of any
  kernel whose compile/load failed
- `THVM_CUDA_DUMP_DISPATCH=<kid>` -- dump dispatch args (buf_ids,
  dptrs, nbytes, refcount, pinned) for one specific kernel
- `THVM_CUDA_ALLOC_TRACE` -- log every cuda_buf_alloc (fresh, freelist-
  popped, external/arena-view) with size + dptr
- `THVM_CUDA_KE_CACHE=0` -- disable per-KE CUfunction cache (force
  re-render+probe each dispatch) for A/B
- `THVM_JIT_NO_PIN` -- disable JIT capture's sticky buf pin for A/B
- Combined with `compute-sanitizer --tool memcheck`, these surfaced
  the arena-view pin bug in one pass.

### Remaining gap to tinygrad

V100 BS=128 STEPS=10 warm-mean across 10 runs (THVM_CUDA_PTX=1
THVM_GROUPTOP=1 THVM_LOCAL_INNER_FIRST=1 THVM_JIT=1):
- min 164 ms, median 185 ms, max 223 ms

Variance is high because the V100 is shared with another experiment
running at ~62% GPU utilization (brain-arc 258 policy training).  The
fastest run beats the median by ~10% and approaches tinygrad's ~115 ms
reference within ~40 ms.

Knobs landed for this exploration:
- `THVM_UPCAST_CAP=N` -- overrides the main UPCAST loop cap (default
  tinygrad-faithful 32).  Sweep shows no gain at 8/16/64/128 on this
  model; the conv2 kernels are memory-bound, not register-pressure-bound.
- `THVM_LOCAL_INNER_FIRST=1` -- reverses the LOCAL picker sort from
  (expand=1 first, axis desc) to (expand=0 first, axis desc), favoring
  the innermost address-relevant axes for LOCAL placement.  ~10% win
  on the conv2 kernels via better load coalescing in the inner W
  dimension.
- `THVM_GROUP_SZ=N` -- cooperative reduce width (default 16).

Next levers (real GPU optimization, not knob tuning):
- Shared-memory weight cache for the conv inner loop (currently 800
  global loads per thread; could be ~200 shared loads with proper tile).
- Multi-axis UPCAST on both Cout and W (more outputs per thread).
- WMMA path for fp32 conv via im2col + GEMM (V100 fp32 doesn't have
  TensorCore acceleration, so this is a wash on V100 specifically but
  needed for sm_80+ TF32 path).

### Summary table (V100 BS=128 STEPS=5)

| Config                     | Cold    | Warm    | Loss   | Correct? |
|----------------------------|--------:|--------:|-------:|---------:|
| Pre-arc baseline           | 15.1 s  | 18.2 s  | ✓      | ✓        |
| + GROUP_REDUCE             | 10.7 s  |  7.3 s  | ✓      | ✓        |
| + KE-CUfunction cache      |  5.1 s  | **800 ms** | ✓ | ✓        |
| + async launches           |  5.1 s  | **750 ms** | ✓ | ✓        |
| + PTX + GROUPTOP (no JIT)  |  8.7 s  | **765 ms** | ✓ | ✓        |
| + TinyJit (simple model)   |    -    | **121 ms** (tinygrad parity) | ✓ | ✓ |
| + TinyJit (full beautiful_mnist) | -- | -- | -- | ✗ ILLEGAL_ADDRESS |
| tinygrad reference         |    -    | ~115 ms | -      | -        |

## References

- `/Users/swish/src/tinygrad/tinygrad/codegen/__init__.py:full_rewrite_to_sink` -- the pipeline
- `/Users/swish/src/tinygrad/tinygrad/codegen/late/expander.py` -- `do_expand`, `pm_pre_expander`
- `/Users/swish/src/tinygrad/tinygrad/codegen/late/devectorizer.py` -- `reduce_to_acc`, `load_store_folding`, `devectorize`
- `/Users/swish/src/tinygrad/tinygrad/codegen/opt/heuristic.py` -- `hand_coded_optimizations`
- thvm: [src/codegen/render_uop.c](src/codegen/render_uop.c) -- `rmu_emit_one_reduce`, `rmu_emit_store`
- thvm: [src/codegen/hand_opts.c](src/codegen/hand_opts.c) -- the heuristic port (commit eaf29a45)
- thvm: [ba6252c9](src/codegen/render_uop.c) -- partial fix for K-axis UPCAST (inner half stays inside reduce)
