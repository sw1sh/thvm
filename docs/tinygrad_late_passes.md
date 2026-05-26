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
- **CUDA gate STILL Metal-only** (`hand_opts.c::kernel_hand_coded_opts`
  narrows to `b->id == 2`). V100 BS=128 STEPS=10 after path #2 +
  gate-widened still runs at 3682 ms warm vs 1464 ms narrow-gate
  baseline. Loss stays monotone 2.81 -> 1.73, correctness preserved.
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

## References

- `/Users/swish/src/tinygrad/tinygrad/codegen/__init__.py:full_rewrite_to_sink` -- the pipeline
- `/Users/swish/src/tinygrad/tinygrad/codegen/late/expander.py` -- `do_expand`, `pm_pre_expander`
- `/Users/swish/src/tinygrad/tinygrad/codegen/late/devectorizer.py` -- `reduce_to_acc`, `load_store_folding`, `devectorize`
- `/Users/swish/src/tinygrad/tinygrad/codegen/opt/heuristic.py` -- `hand_coded_optimizations`
- thvm: [src/codegen/render_uop.c](src/codegen/render_uop.c) -- `rmu_emit_one_reduce`, `rmu_emit_store`
- thvm: [src/codegen/hand_opts.c](src/codegen/hand_opts.c) -- the heuristic port (commit eaf29a45)
- thvm: [ba6252c9](src/codegen/render_uop.c) -- partial fix for K-axis UPCAST (inner half stays inside reduce)
