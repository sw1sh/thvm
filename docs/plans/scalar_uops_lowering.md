# Scalar-UOp lowering: the architectural gap

## Why

Measured 2026-04-30 on LeNet:

| | thvm | tinygrad |
|---|---|---|
| unique programs / Adam step | 130 | 57 |
| kernel slots / Adam step | 5,023 | 57 |

The slot count (5023 vs 57, ~88x) is bookkeeping waste — same
program reallocated as fresh `KernelEntry` per boundary.  Fixable
without architectural change.

The program count (130 vs 57, ~2.3x) is the **fusion gap**.
Tinygrad fuses entire elementwise+reduce chains into one
"BUFFERIZE point" per kernel; we emit one kernel per
`UOP_REDUCE` and one per multi-consumer `UOP_ADD`/`UOP_MUL`/etc.
boundary.  This gap is structural -- thvm has no abstraction
between the tensor-level UOps and the kernel program.

## Architectural reference (tinygrad)

The tinygrad pipeline (researched in detail; see Section F file
map below):

1. **User-level Tensor ops** -- `a + b`, `a.relu()`, etc.
   Build a high-level UOp graph (ADD, MUL, REDUCE, RESHAPE,
   PERMUTE, ...).  Same as our `TUOpAdd` etc.
2. **Rangeify** (`schedule/indexing.py::run_rangeify`):
   - Walk the high-level UOp DAG.
   - Attach `RANGE(extent, axis_id, AxisType)` UOps to each
     output dimension.
   - Push `INDEX` through ALU ops: `INDEX(ADD(a, b), r0, r1)`
     becomes `ADD(INDEX(a, r0, r1), INDEX(b, r0, r1))`.
   - Movement ops (PERMUTE / RESHAPE / EXPAND / PAD / SHRINK /
     FLIP) DON'T MATERIALIZE -- they rewrite the index
     expressions: PERMUTE swaps range order, EXPAND substitutes
     0 for the broadcast axis, etc.
   - REDUCE creates fresh RANGEs with `AxisType.REDUCE`.
3. **Realize map** (`pm_generate_realize_map`): decide which
   nodes become `BUFFERIZE` boundaries:
   - Always: `COPY`, `CONTIGUOUS`, `STORE`.
   - Multi-consumer with diverging ranges.
   - REDUCE with ending ranges.
   - Buffer-count cap (>N reads -> insert BUFFERIZE).
4. **BUFFERIZE -> STORE**: each BUFFERIZE becomes
   `STORE(INDEX(buffer, idx), expr).end(*ranges)`.
5. **Linearize**: topological sort of scalar UOps within each
   kernel; `for r0 { for r1 { LOAD; ALU; STORE } }`.
6. **Codegen**: walk the scalar UOps, emit C / Metal source.

The whole thing is `PatternMatcher` rewrites on a single graph.
No separate "lower" pass -- rangeify IS the lowering.

## Where this slots into thvm

Today's pipeline:

```
WL TTerm
  -> Term (TAG_UOP at tensor level, e.g. UOP_ADD over heap loc)
  -> wnf+materialize loop
       -> realize_classify (decide boundaries)
       -> emit_kernel_for_boundary (one KProgOp[] per boundary)
  -> kernel_fire_by_id
       -> backend dispatch (cpu_interpret OR jit'd C)
```

Today's kernel program (`KProgOp[]`) is scalar-ish but coarse:
each op produces a whole-tensor output.  No explicit RANGEs,
indices, loads, or stores.

Proposed new path:

```
WL TTerm
  -> Term (TAG_UOP, tensor level, UNCHANGED)
  -> wnf+materialize loop (UNCHANGED)
       -> realize_classify (UNCHANGED, but renamed semantically)
       -> rangeify_boundary  (NEW: lowering pass)
            * walk boundary subgraph
            * emit RANGE per output dim
            * push INDEX through ALU
            * absorb movement ops into indices
            * emit BUFFERIZE/STORE at the root
       -> linearize_to_kprog (NEW: scalar UOps -> KProgOp[])
  -> kernel_fire_by_id (UNCHANGED)
  -> backend dispatch (UNCHANGED -- KProgOp[] format the same)
```

The key insight: **we keep `KProgOp[]` as the dispatch format**.
The change is what FILLS it.  Today `visit()` walks one tensor
UOp per KProgOp slot; tomorrow `linearize_to_kprog` walks a
scalar UOp graph and emits one KProgOp slot per scalar op.
This means the cpu/jit backends don't change -- they already
read `KProgOp[]` and execute it.

For multi-consumer / reduce / explicit-CONTIGUOUS boundaries,
the rangeify pass terminates and emits one BUFFERIZE per
boundary.  We then RUN rangeify again for the next boundary.
Net: one KProgOp[] per BUFFERIZE point, but the program now
contains a fused chain of all elementwise ops between
boundaries.

## Scope discipline

**In scope (this plan):**
- Scalar UOp opcodes live in a parallel `ScalarUop[]` arena
  (not new TAG_UOP ext bits at the Term level), but the arena
  is **observable from WL** via introspection APIs (see
  "Introspection surface" below).  Internal-by-default for the
  hot path (no extra Term overhead) but inspectable for tests
  and debugging.
- `rangeify_boundary(root_term, boundary_locs)` lowering pass.
- `linearize_to_kprog(scalar_graph)` -> existing `KProgOp[]`.
- Movement-op absorption into INDEX expressions (PERMUTE,
  RESHAPE, EXPAND, PAD, SHRINK, FLIP).
- REDUCE lowering with REDUCE-typed RANGE.
- One BUFFERIZE per `realize_classify` boundary.
- Tests: long elementwise chains compile to one kernel via the
  new path (already pass via the current path, but the new
  path should match); reduce chains (softmax, BatchNorm) fuse
  to fewer kernels than today.

### Introspection surface (WL-facing)

The lowered scalar graph is transient (per-realize, calloc'd
+ freed) but a per-kernel snapshot is retained on the
`KernelEntry` for inspection.  Mirrors the existing
`TKernelInfo` / `TKernelSourceC` introspection surface.

C-side, on each `KernelEntry`:
- `ScalarUop *scalar_uops` (heap-alloc'd snapshot of the
  lowered graph for THIS kernel; NULL when rangeify path
  was bypassed)
- `u32 n_scalar_uops`

WL-side, new functions in `Kernel.wl`:
- `TKernelScalarUops[kid]` -> Association list, one per
  scalar op: `<|"op" -> "S_RANGE", "dtype" -> "f32", "src" ->
  {ids...}, "extra" -> ...|>`.  Returns `Missing[]` for
  kernels emitted via the legacy path.
- `TKernelScalarGraph[kid]` -> Graph[] visualization (uses
  the existing `Graph` -based UOp visualizer in
  `Visualization.wl` as the renderer for cross-consistency).
- `TKernelLoweringTrace[kid]` (optional, Phase B+): list
  of `{stage, before_uops, after_uops}` snapshots taken
  during rangeify -- useful for "why did this NOT fuse?"
  debugging.  Gated on `THVM_LOWER_TRACE=1` to avoid
  retaining intermediate snapshots when not needed.

Test surface that uses introspection:
- `TKernelScalarUops[kid]` count + kind assertions in
  `wl/THVMLink/Tests/lowering.wlt` (NEW).
- e.g. `(a + b) * c` -> exactly one `S_STORE`, two
  `S_RANGE` (rank-2 inputs), three `S_LOAD`, one `S_ADD`,
  one `S_MUL`.
- Movement-op-absorbed: `Transpose[a + b]` lowers to the
  same scalar op count as `a + b`, with `S_INDEX`
  expressions reflecting the swapped range order.

This makes the gap between "what we computed should fuse"
and "what actually fused" testable in WL, not just
inferable from `TKernelCount[]`.

**Out of scope (deferred):**
- Replacing `KProgOp[]`.  We keep the existing dispatch format.
- Conv2D / MatMul as scalar UOps.  These keep their existing
  BLAS-dispatch path; rangeify treats them as opaque
  "compound" ops with materialized inputs/outputs.  A later
  phase can lower them.
- Tinygrad's local-memory / WMMA / parallel-RANGE optimizations.
  We use AxisType.LOOP only; no GLOBAL / UPCAST / UNROLL.
- Replacing realize_classify with `pm_generate_realize_map`.
  We keep the existing classifier; only the kernel-body
  emission changes.
- Multi-output kernels.  One STORE per kernel for now; a
  future pass can fuse co-aligned outputs into one kernel.

## Phased plan

### Phase A: scaffolding (1-2 days)

- New file `src/schedule/rangeify.c`.
- Define `ScalarUop` struct (op, dtype, src_count, src[4], extra)
  and `ScalarGraph` arena (per-realize, calloc'd at sweep).
- Scalar opcodes: `S_RANGE`, `S_INDEX`, `S_LOAD`, `S_STORE`,
  `S_CONST`, `S_ADD`, `S_MUL`, `S_NEG`, `S_RECIP`, `S_EXP2`,
  `S_LOG2`, `S_SQRT`, `S_CMPLT`, `S_CMPEQ`, `S_REDUCE_SUM`,
  `S_REDUCE_MAX`, `S_BUFFERIZE`.
- Add `scalar_uops` + `n_scalar_uops` fields to `KernelEntry`
  (NULL when rangeify path was bypassed).
- WL introspection surface:
  - C-side: `thvm_wl_kernel_scalar_uops(kid)` returns a packed
    Integer MTensor (ScalarUop fields encoded inline, one row
    per uop).
  - WL-side: `TKernelScalarUops[kid]` in `Kernel.wl` decodes
    into an Association list with `"op"`, `"dtype"`, `"src"`,
    `"extra"` keys; symbolic op names so tests read naturally.
- Unit test (`test_scalar_graph.c`): construct a tiny graph by
  hand; assert structure.
- WL test (`lowering.wlt`, NEW): smoke test that
  `TKernelScalarUops[1]` returns Missing when rangeify off
  and a real list when on.

### Phase B: rangeify for elementwise chains (2-3 days)

- `rangeify_root(root_term)` walks a single boundary subgraph.
- Implement INDEX-through-ALU push.
- Implement movement-op absorption (PERMUTE / RESHAPE / EXPAND
  first; PAD / SHRINK / FLIP next).
- Emit S_BUFFERIZE at the root.
- `linearize_to_kprog`: topo sort + emit one KProgOp per
  scalar op.
- New tests in `lowering.wlt`:
  - `(a + b) * c` lowered: assert exact scalar-op counts via
    `TKernelScalarUops[kid]` (1 STORE, 2 RANGE, 3 LOAD, 1 ADD,
    1 MUL).
  - `Transpose[a + b]` lowered: same scalar-op count as
    `a + b`, with INDEX exprs reflecting swapped range order
    (no extra LOAD/STORE for the permute).
  - Cross-check correctness: cpu_interpret on the rangeified
    KProgOp matches the legacy path's output bitwise.
- Wire as opt-in: `THVM_RANGEIFY=1` routes through the new
  path; default off.  WL-side `TConfig["Rangeify"]` toggles
  it without restarting the kernel.

### Phase C: REDUCE + softmax fusion (2-3 days)

- REDUCE lowering: emit `S_RANGE(axis, REDUCE)` and an
  accumulator scalar.
- Verify: TSoftmax, TSum, TMean fuse to one kernel via
  the new path.
- New tests: softmax-eq-1 (was 2 in current), TBatchNorm-fuses-
  to-1.
- Adjust `realize_classify` to NOT force-realize a REDUCE when
  rangeify is on AND the consumer is elementwise+broadcast (the
  existing softmax-relaxation pass becomes unnecessary).

### Phase D: backward-pass coverage (1-2 days)

- Run the rangeify path on TGrad outputs.  Confirm chain-rule
  outputs fuse the same way forward graphs do.
- Adjust `realize_classify` rules for backward-only patterns
  (the leaf-zero CONST handling).
- New tests: linear-mse-grad-eq-1 (was 3), softmax-grad-eq-1.

### Phase E: turn it on by default (1-2 days)

- Flip `THVM_RANGEIFY=1` to default-on.
- Re-run beautiful-mnist canary at N_STEPS=5; confirm loss
  curve and per-step kernel-program count drop toward
  tinygrad's ceiling.
- Update fusion_count.wlt baselines (some existing ceilings
  will tighten -- e.g. softmax-forward 2 -> 1, two-layer-mlp
  3 -> 2).
- Retire the old per-tensor-UOp emit path (`visit()` in
  materialize.c) once all tests pass on the new path.

### Phase F (optional, follow-up): Conv2D / MatMul lowering

- Lower these via tinygrad-style nested RANGEs (im2col +
  matmul as scalar UOps).
- Compare to BLAS dispatch perf; keep BLAS as the default,
  rangeify path as opt-in for Conv where the compiler can
  generate competitive code.

## Risk + rollback

Each phase lands behind `THVM_RANGEIFY=1`.  Default-off until
Phase E.  Tests on the existing path keep passing throughout.
If Phase E surfaces correctness regressions, set the flag back
to off; the new path is dead code, no integration risk.

The scalar UOp arena is per-realize -- it gets calloc'd at
realize entry and freed at exit.  No persistent state in the
scalar layer.

## Expected outcome

- LeNet unique programs / step: **130 -> ~50-60** (matches
  tinygrad).
- LeNet kernel slots / step: **5023 -> ~50-60** as a side
  effect of "one BUFFERIZE = one slot" (separate from the
  slot-inflation fix; this naturally collapses when the
  emit path stops creating per-tensor-UOp slots).
- TBatchNorm: 5 kernels -> 1.
- TSoftmax: 2 kernels -> 1.

The slot-inflation fix listed in the M4 status table becomes
**moot** once Phase E lands -- there's no slot inflation if
each kernel covers a whole BUFFERIZE'd subgraph.

## Critical files (current path, to read before starting)

- `src/schedule/realize_classify.c` -- boundary decision (rules
  a/b/c).  Stays as-is; rangeify reads its output.
- `src/schedule/materialize.c::emit_kernel_for_boundary` lines
  ~1118-1175 -- current per-boundary emission; the new
  rangeify+linearize path replaces just this function's body.
- `src/schedule/materialize.c::visit` lines ~200-700 -- per-UOP
  emit-into-program walker; new path bypasses this.
- `src/codegen/cg.c`, `src/codegen/render_c.c` -- KProgOp ->
  C source.  No changes; new path emits the same KProgOp[].
- `src/backend/cpu/_.c` and `cpu_interpret` -- KProgOp[]
  executor.  No changes.

## Tinygrad reference files (for cross-check during impl)

- `/Users/swish/src/tinygrad/tinygrad/uop/__init__.py` -- Ops
  enum (~80 scalar opcodes).
- `/Users/swish/src/tinygrad/tinygrad/schedule/indexing.py` --
  `run_rangeify` (lines 148-269), `apply_movement_op` (lines
  128-145), `pm_generate_realize_map` (lines 28-35).
- `/Users/swish/src/tinygrad/tinygrad/schedule/rangeify.py` --
  `bufferize_to_store` (lines 384-415), `split_kernels` (lines
  545-586), `get_kernel_graph` (lines 570-602).
- `/Users/swish/src/tinygrad/tinygrad/codegen/late/linearizer.py`
  -- topological linearization with priority.
- `/Users/swish/src/tinygrad/tinygrad/renderer/cstyle.py` -- C
  rendering rewrite rules (`base_rewrite`, lines 12-61).
