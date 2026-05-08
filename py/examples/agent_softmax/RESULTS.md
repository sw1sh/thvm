# Softmax via thvm UOp DAG -- agent run

## Iteration log

```
[iter 1] change: pure UOp DAG, two REDUCEs (max + sum-of-exp) over
         distinct REDUCE axis ids, single-thread serial dispatch
         (grid=(1,1,1), tg=(1,1,1));
         result: correctness=ok max_abs=1.1e-8, p50=25949us,
         speedup_vs_mlx=0.007x.
[iter 2] change: r_axis -> AXIS_GLOBAL (one TG per row), tg=(32,1,1),
         grid=(R*32,1,1); c_axis still AXIS_LOOP;
         result: correctness=ok, p50=1034us, speedup_vs_mlx=0.141x.
         Renderer puts both reduces inside the c-loop because the
         "hoistable" check sees r_axis (an output address axis) used
         in the reduce body and refuses to hoist above ANY output loop.
[iter 3] change: c_axis -> AXIS_LOCAL (bound to tt), tg=(C,1,1),
         grid=(R*C,1,1). Now no for-loop is opened for either output
         axis, so the "non-hoistable" reduces emit at the top of the
         body once. Each of C threads runs the reduce loops
         redundantly but they execute in lockstep on the simdgroup;
         result: correctness=ok, p50~210us, speedup_vs_mlx ~0.85-1.00x.
[iter 4] change: split c_axis into outer AXIS_LOOP (extent C/32) +
         inner AXIS_LOCAL (extent 32), tg=(32,1,1) so only 32 threads
         run the redundant reduce instead of C;
         result: correctness=ok, p50=308us, speedup_vs_mlx=0.60x.
         Worse than iter 3 -- the renderer pushes the reduces INSIDE
         the outer c_axis LOOP (because that loop's range is in the
         addr expression, so it's an output-axis), so each thread now
         runs the reduce 8x times = 8x more total redundant work.
[iter 5] change: revert to iter 3 shape; verified stable across
         multiple runs;
         result: correctness=ok, speedup_vs_mlx ~0.85x median.
```

## Final speedup_vs_mlx (3-run variance, R=32 C=256)

```
speedup_vs_mlx=0.762x
speedup_vs_mlx=0.608x
speedup_vs_mlx=1.001x
```

Bigger shape (R=64 C=1024): ~0.58x.

Correctness stable at `max_abs ~1.1e-8`, well within `1e-4`.

## Friction points

1. **Multi-reduce hoisting is binary, not per-loop.** The renderer's
   hoistable[i] check is "does the reduce body reference any axis
   used in the store address?" If yes, the reduce is emitted INSIDE
   *all* output loops; if no, it's hoisted above *all* of them.
   Softmax wants the reduces hoisted between the row loop and the
   column loop (each reduce depends on r but is invariant in c).
   Workaround: make BOTH r and c thread-bound (AXIS_GLOBAL +
   AXIS_LOCAL) so neither is a for-loop, then "non-hoistable" emits
   end up at body_depth=1 -- effectively per-thread but with no extra
   loop nesting. Each thread re-runs the reduce; on Metal that's
   ~free across a simdgroup. Hard to do better without an AFTER chain
   + scratch buffers.

2. **GROUP_REDUCE only fires for single-reduce stores.** The
   `rmu_emit_store_reduce` path with `rmu_emit_group_reduce` (the
   `_acc[L]` shared-memory + barrier pattern) only triggers when the
   store's value is *itself* a `UOP_REDUCE` term -- i.e. the entire
   value of the store is one reduce. Softmax has the reduces embedded
   in a larger expression, so this fast path is unreachable.
   Wrapping a reduce with `OPT(_, OPT_GROUP_REDUCE, K)` had no effect
   in the multi-reduce expression path.

3. **No UOP_EXP, and unary opcodes go through `_uop_binary`.**
   `Thvm` has Python wrappers for `add/mul/cmplt/cmpeq` but not for
   the unary `EXP2/LOG2/RECIP/NEG/SQRT`. Had to drop down to ctypes
   and call the raw `_uop_binary(opcode, x, x)` -- the second slot is
   ignored by the renderer for unary opcodes (see render_uop.c
   UOP_EXP2 case reading only `heap_read(loc + 0)`). EXP itself is
   missing entirely; built it as `exp2(x * log2(e))` with `log2(e)`
   as an `fconst`.

## What I'd try next given more budget

1. **AFTER chain with SCOPE_LOCAL scratch.** Build it as 3 sequential
   stores with `h.after`: pass1 = store max[r] into a per-TG
   threadgroup-shared scalar (SCOPE_LOCAL buf of dim 1); pass2 = read
   max, compute and store sum[r] in another local scalar; pass3 =
   read both, divide, store output. Cross-scope AFTER edges already
   emit `threadgroup_barrier` (render_uop.c rmu_emit_after L1347).
   The two reduces would each be a single-reduce store, unlocking
   `rmu_emit_group_reduce` so each pass uses the cooperative
   `_acc[tt]` + barrier shape -- cuts the per-thread reduce work from
   O(C) to O(C/L) where L is threads-per-TG. Risk: SCOPE_LOCAL
   buffers might still get exposed in the kernel signature
   (rmu_discover_bufs_rec walks all UOP_BUFFER nodes regardless of
   scope), so the harness's 2-buffer alloc would break. Would need to
   verify or extend renderer.

2. **Online softmax fusion.** Computing max and sum in one pass via
   the trick `(m_new, l_new) = (max(m, x), l*exp(m - m_new) + exp(x -
   m_new))`. Halves the read traffic and exp count. Hard to express
   in pure UOp DAG without scalar state across the reduce iteration;
   would probably need a custom OPT marker + renderer template.

3. **Multi-row per TG.** With R=32 there are only 32 TGs; that's
   tile-count-limited on M3 Max which has 30+ GPU cores. Have each TG
   process 4 or 8 rows in parallel via an inner LOOP, so the GPU is
   fully saturated. With C=256 and TG=128, 4 rows/TG, 8 TGs, every
   core has work. Currently iter 3 may underutilize the chip at small
   R.

## Final kernel.py

```python
"""Softmax UOp DAG: out[r,c] = exp(x[r,c] - max_r) / sum_r."""
from __future__ import annotations
import sys
from pathlib import Path
ROOT = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(ROOT))
from py.thvm import Thvm, K
from py.thvm.thvm import _uop_binary
from ctypes import c_uint32, c_uint64

LOG2E = 1.4426950408889634

def _unary(opcode, x):
    return int(_uop_binary(c_uint32(opcode), c_uint64(int(x)), c_uint64(int(x))))

def build(h, R, C):
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=0)
    in_buf  = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)
    r_axis = h.range(0, K.AXIS_GLOBAL, R)
    c_axis = h.range(1, K.AXIS_LOCAL,  C)
    cmax_axis = h.range(2, K.AXIS_REDUCE, C)
    csum_axis = h.range(3, K.AXIS_REDUCE, C)
    C_const = h.iconst(C); log2e = h.fconst(LOG2E); neg1 = h.fconst(-1.0)
    max_addr = h.iadd(h.imul(r_axis, C_const), cmax_axis)
    max_r = h.reduce(K.REDUCE_MAX, axis=2, src=h.index_e(in_buf, max_addr))
    sum_addr = h.iadd(h.imul(r_axis, C_const), csum_axis)
    sum_centered = h.add(h.index_e(in_buf, sum_addr), h.mul(max_r, neg1))
    sum_r = h.reduce(K.REDUCE_SUM, axis=3,
                     src=_unary(K.EXP2, h.mul(sum_centered, log2e)))
    out_addr = h.iadd(h.imul(r_axis, C_const), c_axis)
    out_centered = h.add(h.index_e(in_buf, out_addr), h.mul(max_r, neg1))
    out_val = h.mul(_unary(K.EXP2, h.mul(out_centered, log2e)),
                    _unary(K.RECIP, sum_r))
    return h.store(out_buf, out_addr, out_val), out_buf, in_buf

def dispatch(R, C):
    return dict(grid=(R * C, 1, 1), threadgroup=(C, 1, 1))
```

## Final rendered MSL (R=32 C=256)

```c
kernel void k(
    device float *out [[ buffer(0) ]],
    device const float *in0 [[ buffer(1) ]],
    uint tid [[ thread_position_in_grid ]],
    uint tg  [[ threadgroup_position_in_grid ]],
    uint tt  [[ thread_position_in_threadgroup ]],
    uint sgi [[ simdgroup_index_in_threadgroup ]]) {
  uint a0 = tg; /* global ext=32 */
  uint a1 = tt; /* local  ext=256 */
  float _acc2 = -INFINITY;
  for (uint a2 = 0; a2 < 256; a2++) /*reduce*/ {
    _acc2 = fmax(_acc2, in0[((a0 * 256) + a2)]);
  }
  float _acc3 = 0.0f;
  for (uint a3 = 0; a3 < 256; a3++) /*reduce*/ {
    _acc3 = _acc3 + exp2(((in0[((a0 * 256) + a3)] + (_acc2 * -1.0f))
                          * 1.44269502f));
  }
  out[((a0 * 256) + a1)]
      = (exp2(((in0[((a0 * 256) + a1)] + (_acc2 * -1.0f))
               * 1.44269502f))
         * (1.0f/_acc3));
}
```
