# SIMD_REDUCE wedge report

## Files changed

| File | LOC added | Purpose |
|---|---|---|
| `src/thvm.h` | +2 | `UOP_OPT_SIMD_REDUCE = 7`, `KOP_SIMD_REDUCE = 12` |
| `src/uop/apply_opt_dag.c` | ~95 | `uop_dag_apply_simd_reduce(root)` walker + dispatch |
| `src/codegen/render_uop.c` | ~70 | `rmu_collect_reduces_with_simd` collector + per-reduce SIMD branch in `RMU_EMIT_ONE_REDUCE`; `thread_index_in_simdgroup` builtin in kernel signature |
| `py/csource/thvm_py.c` | +2 | `py_const_UOP_OPT_SIMD_REDUCE` / `py_const_KOP_SIMD_REDUCE` |
| `py/thvm/thvm.py` | +2 | `K.OPT_SIMD_REDUCE` / `K.KOP_SIMD_REDUCE` |
| `tests/test_apply_opt_dag.c` | ~100 | 6 SIMD_REDUCE tests (wrap, idempotent, bail, dispatch, MSL render SUM, MSL render MAX) |

Total: ~270 LOC.

## Emitted MSL diff

Vector_sum (extent 64). Before:

```msl
float _acc0 = 0.0f;
for (uint a0 = 0; a0 < 64; a0++) /*reduce*/ {
  _acc0 = _acc0 + in0[a0];
}
out[0] = _acc0;
```

After `KOP_SIMD_REDUCE` (SUM):

```msl
float _acc0 = 0.0f;
for (uint a0 = thread_index_in_simdgroup; a0 < 64; a0 += 32u) {
  _acc0 = _acc0 + in0[a0];
}
_acc0 = simd_sum(_acc0);
out[0] = _acc0;
```

After `KOP_SIMD_REDUCE` (MAX):

```msl
float _acc0 = -INFINITY;
for (uint a0 = thread_index_in_simdgroup; a0 < 64; a0 += 32u) {
  _acc0 = fmax(_acc0, in0[a0]);
}
_acc0 = simd_max(_acc0);
out[0] = _acc0;
```

## Snags

1. **Finding the OPT-parent of a REDUCE in the value tree.** Solved with the parallel-array approach: `rmu_collect_reduces_with_simd` carries a `parent_is_simd` bool through the tree; on hitting `UOP_OPT(_, SIMD_REDUCE, _)` it flips to 1 for the recursion into target, then writes `simd_flags[i] = 1` when the inner REDUCE is collected. Returns alongside `reduces[]`.
2. **Idempotency.** Naive walker double-wrapped: hitting `OPT(REDUCE, SR, _)` recursed into the OPT child, found `REDUCE`, re-wrapped it. Fixed by short-circuiting on `op == UOP_OPT && uop_opt_kind == SIMD_REDUCE` -- recurse into REDUCE's children and re-wrap, never re-firing the wrap rule on the inner REDUCE.
3. **Kernel signature.** Body now references `thread_index_in_simdgroup`; added it as a builtin attribute in both `cg_render_uop_kernel` and `cg_render_uop_kernel_root`. Existing 906/906 regression tests still pass (signature-grep tests use prefix matches not exact line counts).

## Follow-ups

1. **Two-stage TG reduce (Annotation 2)**: feature 2 from the doc. The current SIMD_REDUCE is correct only when one threadgroup == one simdgroup (extent ≤ 32 lanes worth of data). For longer rows, need `local[]` shared-mem + a `simd_<op>(local[lane])` second stage.
2. **kernel_opts_propose**: BEAM doesn't yet generate `KOP_SIMD_REDUCE` candidates. Add a propose rule: any kernel containing a UOP_REDUCE node whose extent is small and whose body has no output-axis dependence (the classic softmax max/sum pattern) -> propose KOP_SIMD_REDUCE. ~15 LOC in `src/codegen/propose.c`.
