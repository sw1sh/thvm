# NN-layer cross-validation on CUDA: two renderer bugs

The CUDA cross-validation (`docs/cuda_xval.md`) measured single ops --
matmul, softmax, row-reduce.  Extending it to whole neural-net *layers*
(`py/examples/cuda_nn_xval.py`) -- with scaled-dot-product attention as
the headline -- surfaced two genuine renderer bugs in `render_uop.c`.
Both are now fixed; this page records the root cause, the fix, and the
V100 cross-validation that closed each out.

The two failing cases were first captured as standalone reproductions in
`py/examples/cuda_render_bugs.py` (each builds a minimal UOp DAG that
isolates the bug, renders CUDA, nvrtc-compiles, dispatches, and checks
against numpy).  `render_uop.c` is shared by the Metal, C, and CUDA
targets, so both bugs reproduced on all three -- the fixes are
renderer-wide, not CUDA-only.

## Bug 1: nested reduce emitted at the wrong loop depth

### Symptom

Scaled-dot-product attention would not compile on CUDA: nvrtc rejected
the rendered kernel with `identifier "aN" is undefined`.

### Root cause

A `REDUCE` whose body wraps an *inner* `REDUCE` over a different axis,
where the inner reduce indexes by the outer reduce's axis variable, was
emitted at the wrong loop nesting depth.  Minimal shape:

```
out[i] = sum_j( in[i] * sum_k m[j,k] )
```

The inner reduce `sum_k m[j,k]` indexes `m` by `j` -- the *outer*
reduce's axis.  The renderer has a chain-reduce path, but it handles
only directly-nested `REDUCE(REDUCE(...))`; here the inner reduce is
wrapped in an elementwise `MUL`, so the value fell to the generic
post-order hoist.  That hoist's `required_pos` model tracks only
*output*-axis loop depth -- it has no notion of one reduce depending on
another reduce's axis.  Both reduces were assigned the same emission
depth and emitted side by side, so the inner reduce's loop body
referenced the outer axis variable `aN` before its declaration.

Scaled-dot-product attention hits this directly: a softmax over the
score row nests a per-row reduce (the row max / row sum-of-exp) inside
the per-key reduce that forms the output.

### Fix

Each `REDUCE` is now emitted at the loop depth its axis dependencies
require.  A new helper, `rmu_term_uses_axis`, detects when a reduce's
body references another reduce's axis variable as a *free* variable;
`parent_idx[i]` records the innermost enclosing reduce that
`reduces[i]` depends on.  The reduce-emission macro became the recursive
`rmu_emit_one_reduce`: after opening a reduce's accumulator loop it
recursively emits every child reduce that depends on that axis -- inside
the loop, before the combine.  The Pass-0 / output-loop interleave now
emits only *root* reduces; a non-root reduce is placed by its parent.

`rmu_term_uses_axis` stops at every `UOP_REDUCE` boundary: a nested
reduce binds its own axis (and uses its body's other axes privately),
so its `_accN` result is an opaque value in the enclosing scope.
Without the boundary stop, a sibling reduce's private axis would be
reported as "used" -- e.g. softmax's sum-of-exp body references the max
reduce's `_accN`, but the max's reduce-axis is bound, not free, so the
two reduces are siblings, not nested.

This is a port of tinygrad's reduce-loop placement.  In tinygrad each
`REDUCE` carries every `RANGE` its body transitively depends on in
`.ranges` (`tinygrad/uop/ops.py:352-370`), with a reduce's
`ended_ranges` popped out of its *result's* range set -- the reduce-axis
stops flowing at the reduce, exactly the boundary `rmu_term_uses_axis`
enforces.  The linearizer (`tinygrad/codegen/late/linearizer.py:56-90`)
then schedules each `RANGE`/`END` so a `RANGE` that another `RANGE`
depends on is opened first -- an inner reduce-loop nests inside every
reduce-loop it depends on.

### Composes with the reduce-feeding-broadcast hoist

The gap-closing loop (commit `bb24c96b`) added a reduce-feeding-
broadcast hoist in the same emission area.  The two are orthogonal: the
broadcast hoist decides whether an output axis is promoted to a grid
thread (so a row reduce runs once per row, not once per output element);
the reduce-loop nesting decides at what loop depth each reduce's
accumulator loop is emitted.  Attention exercises both -- the row max
and row sum-of-exp are hoisted (depend only on the row), and the inner
reduce nests correctly within the outer.

## Bug 2: integer expressions emitted unsigned, breaking signed ISUB

### Symptom

A guard such as `-1 < (i-1)` was always false, masking off every output
element it should have kept live; a conv2d dX shifted index relied on
unsigned wraparound to mask out-of-range reads.

### Root cause

`UOP_ISUB` is documented a *signed* integer subtract (`src/thvm.h`), but
the renderer declared every `RANGE` variable as `uint` and emitted the
`UOP_I*` binaries with no cast, so the entire integer-expression tree
was unsigned.  A negative `ISUB` result wrapped to `~UINT_MAX`:

- a comparison like `-1 < (i-1)` promotes `-1` to `UINT_MAX`, so the
  guard is always false;
- a negative array index wraps far out of bounds.

### Fix

Each `UOP_I*` binary (`IADD`/`ISUB`/`IMUL`/`IDIV`/`IMOD`/`ILT`/`IAND`)
is now emitted with both operands cast to `int`, so the whole integer
subtree -- and any `RANGE` leaf inside it -- is computed signed.
Subtraction, comparison and idiv/imod are then signed-correct, and a
signed array subscript is valid on every target while the `IWHERE`/`ILT`
bounds guard masks it correctly.  The change is localized to
`rmu_emit_term`'s `UOP_I*` case; the `RANGE` declaration stays `uint`
(it is a loop counter) and the GPU thread-builtin decode
(`tid`/`tg`/`tt`) is untouched, since those carry no `UOP_I*` nodes.

This matches tinygrad, which renders `RANGE` and index dtype as a signed
`int` (`tinygrad/renderer/cstyle.py:18-19,150`) rather than leaning on
unsigned wraparound.

## V100 cross-validation

Verified on the Tesla V100-SXM2-16GB pod (`sm_70`, CUDA 12 toolkit):

| harness | result |
|---|---|
| `bin/test_cuda_backend` | 46/46 |
| `cuda_render_bugs.py` -- `nested_reduce_axis_dep` | OK, max_abs 2.1e-7 |
| `cuda_render_bugs.py` -- `signed_index_isub` | OK, max_abs 0.0 |
| `cuda_nn_xval.py` -- `attention/forward` | OK, max_abs 1.0e-7 |
| `cuda_nn_xval.py` -- `attention/backward` | OK, max_abs 4.3e-8 |
| `cuda_nn_xval.py` -- `softmax/forward` | OK, max_abs 2.3e-8 |

Attention forward (`O[i,d] = sum_j exp(S[i,j]-m_i) V[j,d] / Z_i`, with
`m_i` and `Z_i` nested reduces over the key axis) and attention backward
(the softmax-Jacobian `dS[i,j] = P[i,j](G[i,j] - sum_k P[i,k]G[i,k])`,
the correction term a nested reduce) both compile on nvrtc and
cross-validate against the numpy fp64 reference.  Before bug 1's fix the
forward kernel did not compile at all.

Render/regression on macOS held: `test_render_uop` 238/238,
`test_render_uop_metal` 8/8, `test_render_uop_cuda` 83/83,
`test_cpu_jit_via_uop` 320/320, and `cuda_xval.py --quick` (matmul,
softmax, row-reduce) still cross-validates correct.

## Reproducing

```sh
# render-bug isolation harness (both bugs)
python3 py/examples/cuda_render_bugs.py

# NN-layer cross-validation
python3 py/examples/cuda_nn_xval.py --only attention
python3 py/examples/cuda_nn_xval.py            # attention + softmax
```

Both run only on a Linux+CUDA host; on a machine without the CUDA bridge
they print a `SKIP` line and exit 0.
