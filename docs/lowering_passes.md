# Lowering Passes

How a high-level UOp DAG gets compiled into GPU/CPU dispatches.

## At a glance

```
   user / WL
      |
      v
   UOp DAG (TAG_UOP heap cells)                  <- src/uop/*.c
      |   pure-functional, hash-consed.  Tile-level concepts
      |   (BUFFER scope, STORE addr/value, AFTER ordering, OPT
      |   annotations) live here as opcodes.
      |
      |  pass 1: simplify                        <- src/uop/graph_simplify.c
      |     algebraic folds; movement-as-INDEX        src/uop/movement_index.c
      |     (PERMUTE/RESHAPE/EXPAND/PAD/SHRINK/        src/uop/index_simplify.c
      |     FLIP rewrite to symbolic INDEX trees);
      |     range-bound-aware simplifier folds.
      |
      |  pass 2: schedule                        <- src/schedule/bufferize.c
      |     walk DAG, mark ROOT/MULTI/REDUCE
      |     boundaries.  Each boundary becomes
      |     a B_BUFFERIZE node; rewrite rules
      |     promote / remove buffers based on
      |     cost.
      v
   for each boundary in topo order:               <- src/schedule/materialize.c
      |
      |  pass 3: lift to UOp DAG kernel root     <- src/schedule/kernel_lift.c
      |     kernel_lift_to_uop synthesises a
      |     UOP_STORE root from the boundary's
      |     compute subtree.  Covers every shape
      |     (matmul via TileGemmInfo, conv2d_flat
      |     via TileConv2DInfo including im2col
      |     multi-input, elementwise/reduce/
      |     movement-fused, BN-grad chain reduces).
      v
   pass 4: render                                <- src/codegen/render_uop.c
      |   cg_render_uop_kernel walks UOp DAG
      |   rooted at UOP_STORE / UOP_AFTER and
      |   emits MSL.  Pattern-matches:
      |     OPT(REDUCE(MUL(LOAD,LOAD), SUM, k), TC, _)
      |       -> 8x8 simdgroup_matrix template
      |     generic shapes
      |       -> for-loop nest with hoisted
      |          accumulator for nested REDUCE.
      v
   one Metal dispatch path                       <- src/backend/metal/_.m
                                                    one MTLComputePipelineState
                                                    per kernel; ICB replay
                                                    amortises encoder cost.
```

## Pass 1: UOp graph simplify

- File: [src/uop/graph_simplify.c](../src/uop/graph_simplify.c)
- Movement: [src/uop/movement_index.c](../src/uop/movement_index.c)
- Index simplifier: [src/uop/index_simplify.c](../src/uop/index_simplify.c)

A peephole rewriter on the heap-allocated UOp DAG.  Each rule
matches a small structural pattern (e.g.
`REDUCE_SUM(MUL(x, CONST))` -> `MUL(REDUCE_SUM(x), CONST)`) and
returns a rewritten Term.  The driver loops until no rule fires.

Movement ops (`UOP_PERMUTE` / `UOP_RESHAPE` / `UOP_EXPAND` /
`UOP_PAD` / `UOP_SHRINK` / `UOP_FLIP`) rewrite to symbolic INDEX
expressions over `UOP_RANGE` leaves -- a direct port of tinygrad's
`apply_movement_op`.  This lets the consumer's address arithmetic
absorb the movement, eliminating intermediate buffers.

The index simplifier folds the resulting trees: range-bound-aware
`r % N -> r` when `r ∈ [0, N)`, divandmod recombination, gcd-aware
IDIV, affine normalisation, IWHERE-of-IWHERE collapse, etc.  Without
these folds the addresses in the rendered MSL are giant ASTs the
shader compiler can't reduce.

## Pass 2: Schedule (bufferize)

- File: [src/schedule/bufferize.c](../src/schedule/bufferize.c)

The bufferize pass walks the simplified DAG and decides which UOp
nodes get a backing buffer (kernel boundaries).  ROOT (the value the
user asked to realize), MULTI (consumed by more than one downstream),
and REDUCE (output of a reduce that can't fuse into the consumer's
loop) are seeds.  Named rewrite rules then promote or remove buffer
boundaries based on a cost model (op count, output bytes, recompute
budget).

After this pass the DAG is annotated with `B_BUFFERIZE` /
`B_STORE` / `B_INDEX` UOps that name kernel boundaries and per-
boundary input edges.

See `docs/plans/bufferize.md` for the data layer + cost model.

## Pass 3: Lift kernel boundary to UOp DAG root

- File: [src/schedule/kernel_lift.c](../src/schedule/kernel_lift.c)

For each `B_BUFFERIZE` boundary,
[materialize.c](../src/schedule/materialize.c) calls
`emit_kernel_for_boundary` which reserves a `KernelEntry` slot.
`kernel_lift_to_uop` then synthesises the `UOP_STORE` root that
represents the kernel's compute:

- **Elementwise / reduce / movement-fused**: walks the boundary's
  scalar arena (built by rangeify) -- supports `S_CONST`, `S_LOAD`,
  `S_LOAD_RAW`, `S_INDEX`, `S_INDEX_E`, `S_ADD`, `S_MUL`,
  `S_NEG`/`RECIP`/`EXP2`/`LOG2`/`SQRT`, `S_CMPLT`/`CMPEQ`, `S_CAST`,
  `S_REDUCE_SUM`/`MAX`, `S_STORE`, `S_BUFFERIZE`, `S_RANGE`,
  `S_FLIP`, `S_SHRINK`, `S_PAD` (with `UOP_IWHERE` bounds-check
  guard), `S_RESHAPE_V` (factor-decomposition flat-roundtrip), plus
  the `S_ICONST` / `S_I*` index-arithmetic ops via `scalar_to_uop`.

- **Matmul**: when the boundary lacks a scalar arena but
  `tile_collect_mma_plan` recognises the matmul shape, synthesises
  the canonical `STORE(C, m*N+n, OPT(REDUCE(MUL(A[m*K+k],
  B[k*N+n]), SUM, k), TC, 0))` UOp DAG.  The renderer's
  `OPT(_, TC)` pattern-matcher fires the simdgroup template.

- **Conv2D flat (incl. multi-input X)**: when
  `tile_analyze_conv2d_flat` matches, decomposes the output index
  into `(co, bi, oh, ow)` and the reduce index into
  `(ci, kh_v, kw_v)` via `UOP_IDIV`/`UOP_IMOD`.  Multi-input X
  (im2col split) emits a nested `UOP_IWHERE` chain selecting the
  right input slot based on q.

The lifter populates `KernelUopLift { store_root, out_buf, in_bufs,
n_inputs }` ready for the renderer; `in_bufs` is a heap array with one
entry per kernel input, so a kernel takes any number of inputs (CPU and
CUDA have no per-kernel buffer cap; Metal's 31 is enforced before the
lift).  `uop_buffer_inst`'s slot disambiguator keeps each kernel-arg
buffer distinct even when shapes coincide.

A kernel root may also be an `UOP_AFTER` chain of `UOP_STORE`s: a
kernel that writes its output in ordered passes (a scatter's init,
accumulate and finish stores).  `uop_arity(UOP_AFTER)` stays 0 because
the devectorizer's AFTER chains rely on it, so every materialize-time
pass over a kernel root goes through `uop_root_map_stores` /
`uop_root_each_store` ([src/uop/root_map_stores.c](../src/uop/root_map_stores.c)):
the arange collapse, the walker rewrite with its REDUCE repair, the
chain-fold scan and the resid / stranded / broadcast gates each see every
store; `uop_dag_renumber_axes` gives all stores one dense axis-id space;
and `out_buf` is the buffer the stores write (`uop_root_store_buf`).
Emit sets `KernelEntry.no_opts` for such a root, and for any kernel whose
address holds a load (`uop_dag_has_dynamic_index`, a gather or scatter
index); hand_opts, BEAM/autotune and `kernel_apply_opt` then leave the
kernel as a serial loop nest.

The walker rewrite composes an input's view (a SHRINK offset, a permute,
a prior-view chain) into the addresses over its underlying buffer, and the
chain-fold scan marks those slots `input_chain_composed` so the backends
bind that buffer as is: no pre-materialization, and no view-offset bias on
the UOp walker's pointer.  The marks commit with the rewritten root
whatever the resid / stranded / broadcast gates would report (the gates
only count, under `THVM_DEBUG_BYPASS_LAST` / `THVM_DUMP_LIFT_COVERAGE`).
Such a slot reads past its view's numel (row `t` of `W` reaches `W`'s
element `t*D+D-1`), so the BEAM / AUTOTUNE / JITBEAM isolated search gives
a raw-buffer slot a scratch buffer of the whole buffer behind it, as
tinygrad sizes each beam buffer to its whole PARAM
(codegen/opt/postrange.py:256-258).

A `UOP_GATHER(src, idx, dim)` ([src/uop/gather.c](../src/uop/gather.c))
lowers in `pm_apply_rangeify` (`ru_lower_gather`) straight to the form
tinygrad's `pm_reduce_load_collapse` leaves once the one-hot sum of
`Tensor.gather` collapses: idx loads at the node's own ranges, and src
loads at those ranges with axis `dim` replaced by the loaded index,
clamped to 0 and gated `INVALID` (read as 0) when the index is out of
range.  No REDUCE is created, so a gather costs one load per output
element and fuses into its consumer like an elementwise op.  src is read
at an address the gather builds itself, so `ru_index_leaf` walks its views
down to storage, and the gather hands its ranges to idx only
(`ru_consumer_passes_rngs`).  A src that is not a view of a TEN, VAR,
KERNEL output or CONST (`uop_indexed_operand_is_structural`) realizes its
compute source first (`BUFFERIZE_REASON_INDEXED_SRC`), and `visit` binds
the storage itself as the kernel input, never a view alias.  idx is read
at the node's ranges (`ru_read_operand_at_ranges`): a structural idx
through its storage the same way, so a view-alias TEN's offset and
strides or a lazy SHRINK / FLIP compose into the address exactly, and a
computed idx (`idx + 1`, a cast) fuses into the gather kernel.  Gather and
scatter kernels run on the CPU backend only; emit declines them on a GPU
backend.

A `UOP_SCATTER` ([src/uop/scatter.c](../src/uop/scatter.c)) is always its
own kernel (`BUFFERIZE_REASON_SCATTER`): `ru_lower_scatter` builds an AFTER
chain of stores over the output buffer instead of an expression.  base is
read at the node's ranges, as a gather's idx is (through its storage when
structural; a computed base fuses into the stores); idx and src at fresh
loop ranges over idx's shape, both through `ru_index_leaf` (a computed idx
or src realizes first).  The accumulate store is a serial
read-modify-write, `OUT[t] = valid ? OUT[t] + src : OUT[t]`, where `t`
puts the clamped loaded index on axis `dim`, so an invalid index writes
the old value back.  Positions that meet at one output differ only on
`dim` and are visited in ascending order: tinygrad's reduce order, and
REPLACE's last write wins.  SUM with
include_self stores the identity, accumulates, then adds base (S + self,
as tinygrad; the last store drops when base is the identity CONST);
without include_self it stores base, resets every touched position to the
identity, then accumulates; REPLACE stores base, then writes.  The
scatter's BUFFERIZE wraps the SCATTER itself, so no consumer inlines it,
and every store of a gather or scatter kernel is checked for unbound
leaves after the walker rewrite.  PROD, MAX and MIN have no lowering yet:
emit declines their kernel with a stderr line.  The ops' semantics,
gradients and py surface are in [gather_scatter.md](gather_scatter.md).

## Pass 4: Render

- File: [src/codegen/render_uop.c](../src/codegen/render_uop.c)
- Entry: `cg_render_uop_kernel(root, name, out_buf, in_bufs, n_inputs, fp)`

Walks the UOp DAG rooted at a `UOP_STORE` (or `UOP_AFTER` chain for
multi-store kernels) and emits MSL.  Coverage:

- **Kernel signature**: typed `device <T> *out [[ buffer(0) ]]` +
  `device const <T> *inN [[ buffer(1+N) ]]`, plus the standard
  thread-position attributes.
- **Loop nest**: `UOP_RANGE` leaves with `axis_type=LOOP` open
  for-loops; `LOCAL` axes bind to `tt`
  (`thread_position_in_threadgroup`); `GLOBAL` to `tg`
  (`threadgroup_position_in_grid`); `REDUCE` opens a for-loop with
  `/*reduce*/` marker.
- **Address arithmetic**: row-major `_idx = a0 * stride0 + a1 *
  stride1 + ...` from output `UOP_RANGE` leaves; `UOP_IADD`/
  `IMUL`/`IDIV`/`IMOD`/`ILT`/`IAND` emit parenthesised infix.
- **Elementwise**: `UOP_ADD`/`MUL`/`CMPLT`/`CMPEQ` -> infix;
  `UOP_NEG`/`RECIP` -> prefix `(-x)` / `(1.0f/x)`;
  `UOP_EXP2`/`LOG2`/`SQRT` -> MSL builtins;
  `UOP_CAST`/`BITCAST` -> `((T)x)` / `as_type<T>(x)`.
- **REDUCE**: when `UOP_REDUCE` appears in an expression, the
  renderer pre-walks the value tree, hoists each reduce to an
  accumulator preamble (`float _accK = init; for (...) _accK =
  combine(_accK, body); }`), and substitutes `_acc<axis>` in the
  expression.
- **OPT pattern-match**: `OPT(_, UNROLL/UPCAST, factor)` -> `#pragma
  unroll(N)`; `OPT(_, LOCAL, _)` -> bind to `tt`;
  `OPT(REDUCE(MUL(LOAD, LOAD), SUM, k), TC, _)` with K divisible by
  8 -> 8x8 `simdgroup_matrix<float, 8, 8>` template emitting
  `simdgroup_load`/`multiply_accumulate`/`store`.
- **AFTER**: emits `threadgroup_barrier(mem_flags::mem_threadgroup)`
  when `UOP_AFTER` crosses a `UOP_BUFFER(scope=LOCAL)` boundary.

The output is validated through `xcrun metal -c` in
[tests/test_render_uop_metal.c](../tests/test_render_uop_metal.c)
and through real WL workloads (cmpeq/cast/reduce/bitcast/bn_grad/
grad_edge/assign/flip/core/conv_im2col).

## Dispatch

- File: [src/backend/metal/_.m](../src/backend/metal/_.m)

The Metal backend maintains an MTLLibrary cache keyed on the
`cg_emit_tile_metal` source string.  At first dispatch:
`cg_emit_tile_metal` -> `cg_emit_via_uop` -> `kernel_lift_to_uop`
-> `cg_render_uop_kernel` -> MSL string ->
`[MTLDevice newLibraryWithSource:]` -> `MTLComputePipelineState`.
Subsequent dispatches reuse the PSO.

`cg_tile_metal_dispatch_shape` derives `(groups_x, threads_x)` from
the kernel's axis structure (FLAT_GRID / LOCAL_GLOBAL /
GROUP_REDUCE modes).  Threads-per-threadgroup are configured from
`KAX_LOCAL` extents; groups from `KAX_GLOBAL` * other axes.

## Where to look in the code

| Concern | File |
|---|---|
| Movement-as-INDEX rewrites | [src/uop/movement_index.c](../src/uop/movement_index.c) |
| Symbolic index folds | [src/uop/index_simplify.c](../src/uop/index_simplify.c) |
| New UOp opcodes | [src/uop/buffer.c](../src/uop/buffer.c), [store.c](../src/uop/store.c), [opt.c](../src/uop/opt.c), [index.c](../src/uop/index.c) |
| Bufferize / schedule | [src/schedule/bufferize.c](../src/schedule/bufferize.c) |
| Kernel boundary -> UOp DAG | [src/schedule/kernel_lift.c](../src/schedule/kernel_lift.c) |
| MSL renderer | [src/codegen/render_uop.c](../src/codegen/render_uop.c) |
| Metal dispatch | [src/backend/metal/_.m](../src/backend/metal/_.m) |
