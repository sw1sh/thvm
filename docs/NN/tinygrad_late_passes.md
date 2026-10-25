# UPCAST and UNROLL lowering: tinygrad's late passes, thvm's renderer

`hand_coded_optimizations` (or BEAM) tags some ranges UPCAST or UNROLL.
Something then has to turn those tags into code: one accumulator per
lane, loads shared across lanes, straight-line unrolled bodies. tinygrad
does it with graph rewrites over the UOp graph before rendering. thvm
does it while it emits source, in
[render_uop.c](../src/codegen/render_uop.c).

## tinygrad at 365f87a8a

`full_rewrite_to_sink` (`tinygrad/codegen/__init__.py:275-383`) lowers
an optimized kernel in a sequence of `graph_rewrite` passes. The passes
that handle UPCAST and UNROLL are shape-based: the lanes of an UPCAST
axis are an extra axis of the UOp's shape.

- `expander` (`__init__.py:78-84`, run at 308): an UPCAST or UNROLL
  RANGE becomes `const(tuple(range(n)))` reshaped onto its own axis
  (`build_range_map`, 42-47, numbers those axes), so everything
  computed from it gains a lane axis. `expand_reduce` (49-63) moves
  the lane axes of a REDUCE source out of its reduce set, and
  `expand_wmma` (72-76) contracts and unrolls a tensor-core op.
- `pm_reduce_local` (217-223, run at 313): `reduce_ranges_to_acc`
  (197-204) turns a REDUCE into a register accumulator: an init STORE
  of the identity, an accumulate STORE inside the reduce ranges, and
  an END. `fix_group_for_reduce` (160-175) stages a GROUP reduce
  through a LOCAL buffer.
- `pm_add_gpudims` (`codegen/gpudims.py`, run at 319) maps global and
  local ranges onto thread ids.
- `pm_expand_broadcast` and `pm_add_loads` (112-115 and 228-232, run
  at 323) broadcast each ALU or STORE's operands to one shape and add
  the LOADs.
- `devectorizer2` (137-158, run at 326): `do_devectorize` (117-124)
  splits an op of shape S into one scalar op per element, stacked back
  with `UOp.stack` (grouped, for a STORE).
- The linearizer pass is gone ("this was the linearizer", 376):
  `pm_add_control_flow` orders the graph, and `linearize`
  (`codegen/late/linearizer.py:8-52`) emits the list.

## thvm

thvm runs none of these passes on the UOp graph. The renderer's emit
walk reads the post-heuristic graph directly and produces the same
effects in the source it writes:

- **Path #1, parallel accumulators** (`rmu_emit_store_reduce`): when an
  UPCAST output axis encloses a reduce, the kernel declares one
  accumulator per lane and the lanes share one reduce loop, which is
  what `reduce_ranges_to_acc` over an expanded REDUCE produces.
- **Path #2, shared-load hoist** (`rmu_hoist_shared_loads`,
  `rmu_hoist_lane_invariant`): a load or a subexpression that no UPCAST
  axis reaches is emitted once per reduce iteration into a local and
  reused by every lane.
- **Full-extent UNROLL**: an unrolled reduce axis renders as
  straight-line accumulation under `#pragma unroll(extent)`.
- **GROUP reduce** (`rmu_emit_group_reduce`): a cooperative reduce
  through threadgroup / shared memory on Metal and CUDA.

The faithful port is lever L1 of the size plan: the shape-based
expander and devectorizer above, `pm_add_gpudims`, and the linearizer,
replacing the emit-time versions.

## Removed: the UOp-level late-pass port (2026-10)

From 2026-05 to 2026-10 thvm carried a port of tinygrad's earlier
late-pass design: `uop/expander.c` (UNROLL, CONTRACT, VCONST and GEP
opcodes), `uop/devectorize.c` (STACK, PLACEHOLDER and END, its
`reduce_to_acc`, load-store folding), `uop/symbolic_rewrite.c`,
`uop/linearize.c`, and two renderers over the linearized list,
`codegen/render_linearized.c` (C, MSL, CUDA) and `codegen/render_ptx.c`
(CUDA PTX, opt-in through `THVM_CUDA_PTX`). The three render entries
tried it first on every kernel with an UPCAST or UNROLL range and fell
back to the emit walk when a stage declined. It was deleted, about
3,500 lines of code and 2,400 of tests, because:

- It rendered no production kernel. The linearized renderer needs a
  STORE through INDEX_E, and every STORE producer passes a bare
  address, so it declined every kernel: 33,062 attempts and no render
  across the C, Metal and CUDA entries.
- tinygrad deleted the design it ports before 365f87a8a: VCONST
  (55515747b), GEP (3fd6f3d28), UNROLL and CONTRACT (dd6aa77fd),
  `pm_render` (43c1bbceb), and the old devectorizer, whose remains
  moved to `codegen/late/coalesce.py` (c92a4c044). The port was not a
  base to grow toward the spec.
- Nothing set `THVM_CUDA_PTX`, and the PTX renderer's scalar stores
  used an element index as a byte address.

Opcodes 44-50 stay unassigned, so the other opcodes keep their numbers.
The port's design notes, its V100 measurements, and the 2026-05 CUDA
performance log that shared this page are in
`git show 3c2f2c96:docs/tinygrad_late_passes.md`.
