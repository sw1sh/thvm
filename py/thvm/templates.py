"""Reusable UOp DAG builders for canonical kernel patterns.

Each builder takes a `Thvm` plus shape parameters and returns
`(root, out_buf, in_bufs)` ready for `Thvm.render` and dispatch via
`Metal.compile_msl`. Designed so agents start from a working DAG and
spend iterations on geometry (axis types, OPT annotations) rather
than re-deriving the canonical shape each time.

Patterns:
  matmul       -- C = A @ B, shapes (M,K) @ (K,N) -> (M,N)
  vector_sum   -- out[r] = sum over c of x[r,c]
  vector_max   -- out[r] = max over c of x[r,c]
  softmax      -- out[r,c] = exp(x[r,c] - max) / sum_exp
  layernorm    -- out[r,c] = ((x[r,c] - mean) / sqrt(var + eps)) * gamma[c] + beta[c]
  rmsnorm      -- out[r,c] = (x[r,c] / sqrt(mean(x^2) + eps)) * gamma[c]

All defaults: fp32, GLOBAL scope, instance-disambiguated buffers
(out=0, inputs=1..n in argument order).

Default axis_types are LOOP -- the agent applies KOP_GLOBAL/LOCAL/etc
via Thvm.kernel_apply_opt or by reconstructing with different
axis_types to parallelize.
"""
from __future__ import annotations

from .thvm import Thvm, Term, K


# -------------------- matmul ----------------------------------------
def matmul(h: Thvm, M: int, N: int, K_dim: int, *, with_tc: bool = False):
    """C = A @ B, shapes (M,K_dim) @ (K_dim,N) -> (M,N).

    Returns (root, out_buf, [a_buf, b_buf]).
    With `with_tc=True`, wraps the inner REDUCE with OPT(_, TC, 0)
    -- the renderer's simdgroup_matrix template fires when K_dim is
    a multiple of 8.
    """
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
    a_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, K_dim), instance=1)
    b_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(K_dim, N), instance=2)

    m_axis = h.range(0, K.AXIS_LOOP, M)
    n_axis = h.range(1, K.AXIS_LOOP, N)
    k_axis = h.range(2, K.AXIS_REDUCE, K_dim)

    K_c = h.iconst(K_dim)
    N_c = h.iconst(N)

    a_addr = h.iadd(h.imul(m_axis, K_c), k_axis)
    b_addr = h.iadd(h.imul(k_axis, N_c), n_axis)
    c_addr = h.iadd(h.imul(m_axis, N_c), n_axis)

    prod = h.mul(h.index_e(a_buf, a_addr), h.index_e(b_buf, b_addr))
    reduced = h.reduce(K.REDUCE_SUM, axis=2, src=prod)
    if with_tc:
        reduced = h.opt(reduced, K.OPT_TC, 0)
    return h.store(out_buf, c_addr, reduced), out_buf, [a_buf, b_buf]


# -------------------- vector_sum / vector_max -----------------------
def vector_sum(h: Thvm, R: int, C: int):
    """out[r] = sum over c of x[r, c].  Shapes: in (R, C), out (R,)."""
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R,), instance=0)
    in_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)

    r_axis = h.range(0, K.AXIS_LOOP, R)
    c_axis = h.range(1, K.AXIS_REDUCE, C)
    C_c = h.iconst(C)

    in_addr = h.iadd(h.imul(r_axis, C_c), c_axis)
    reduced = h.reduce(K.REDUCE_SUM, axis=1, src=h.index_e(in_buf, in_addr))
    return h.store(out_buf, r_axis, reduced), out_buf, [in_buf]


def vector_max(h: Thvm, R: int, C: int):
    """out[r] = max over c of x[r, c]."""
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R,), instance=0)
    in_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)

    r_axis = h.range(0, K.AXIS_LOOP, R)
    c_axis = h.range(1, K.AXIS_REDUCE, C)
    C_c = h.iconst(C)

    in_addr = h.iadd(h.imul(r_axis, C_c), c_axis)
    reduced = h.reduce(K.REDUCE_MAX, axis=1, src=h.index_e(in_buf, in_addr))
    return h.store(out_buf, r_axis, reduced), out_buf, [in_buf]


# -------------------- softmax ---------------------------------------
def softmax(h: Thvm, R: int, C: int, *,
            r_axis_type: int = K.AXIS_GLOBAL,
            c_axis_type: int = K.AXIS_LOCAL):
    """out[r, c] = exp(x[r, c] - max_r) / sum_r where reductions are
    over the col axis.  Two reductions fused inline.

    Defaults to the parallel form: r->threadgroup, c->thread-position
    (lockstep simdgroup execution).  The renderer's per-axis hoist
    logic places each reduce above the c-axis (where applicable),
    avoiding redundant per-output recompute when c is a for-loop.

    Override via `r_axis_type` / `c_axis_type` kwargs (e.g.
    AXIS_LOOP / AXIS_GLOBAL / AXIS_LOCAL / AXIS_GROUP_REDUCE).

    Uses distinct REDUCE axis ids (2, 3) so the renderer hoists each
    reduce into its own _accN accumulator.

    Dispatch convention for the default (r=GLOBAL, c=LOCAL):
      grid=(R*C, 1, 1), threadgroup=(C, 1, 1).  R threadgroups, C
      threads each.
    """
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=0)
    in_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)

    r_axis = h.range(0, r_axis_type, R)
    c_axis = h.range(1, c_axis_type, C)
    cmax_axis = h.range(2, K.AXIS_REDUCE, C)
    csum_axis = h.range(3, K.AXIS_REDUCE, C)

    C_c = h.iconst(C)

    # max over c
    max_addr = h.iadd(h.imul(r_axis, C_c), cmax_axis)
    max_r = h.reduce(K.REDUCE_MAX, axis=2, src=h.index_e(in_buf, max_addr))

    # sum over c of exp(x - max)
    sum_addr = h.iadd(h.imul(r_axis, C_c), csum_axis)
    sum_e = h.exp(h.add(h.index_e(in_buf, sum_addr), h.neg(max_r)))
    sum_r = h.reduce(K.REDUCE_SUM, axis=3, src=sum_e)

    # output: exp(x - max) / sum
    out_addr = h.iadd(h.imul(r_axis, C_c), c_axis)
    out_e = h.exp(h.add(h.index_e(in_buf, out_addr), h.neg(max_r)))
    out_val = h.mul(out_e, h.recip(sum_r))
    return h.store(out_buf, out_addr, out_val), out_buf, [in_buf]


# -------------------- layernorm -------------------------------------
def layernorm(h: Thvm, R: int, C: int, *, eps: float = 1e-5):
    """out[r,c] = ((x[r,c] - mean_r) / sqrt(var_r + eps)) * gamma[c] + beta[c].

    Two reductions (mean + variance) over the col axis.  Uses two
    distinct REDUCE axis ids so each gets its own _accN accumulator.

    Buffers: out=0, in_x=1, gamma=2, beta=3.
    """
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=0)
    in_x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)
    gamma = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(C,), instance=2)
    beta = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(C,), instance=3)

    r_axis = h.range(0, K.AXIS_LOOP, R)
    c_axis = h.range(1, K.AXIS_LOOP, C)
    cmean = h.range(2, K.AXIS_REDUCE, C)
    cvar = h.range(3, K.AXIS_REDUCE, C)

    C_c = h.iconst(C)
    inv_C = h.fconst(1.0 / float(C))
    eps_c = h.fconst(float(eps))

    # mean = sum(x) / C
    mean_addr = h.iadd(h.imul(r_axis, C_c), cmean)
    mean_sum = h.reduce(K.REDUCE_SUM, axis=2, src=h.index_e(in_x, mean_addr))
    mean_r = h.mul(mean_sum, inv_C)

    # var = sum((x - mean)^2) / C
    var_addr = h.iadd(h.imul(r_axis, C_c), cvar)
    var_x = h.index_e(in_x, var_addr)
    var_centered = h.add(var_x, h.neg(mean_r))
    var_sq = h.mul(var_centered, var_centered)
    var_sum = h.reduce(K.REDUCE_SUM, axis=3, src=var_sq)
    var_r = h.mul(var_sum, inv_C)

    inv_std = h.recip(h.sqrt(h.add(var_r, eps_c)))

    out_addr = h.iadd(h.imul(r_axis, C_c), c_axis)
    out_centered = h.add(h.index_e(in_x, out_addr), h.neg(mean_r))
    normalized = h.mul(out_centered, inv_std)
    scaled = h.mul(normalized, h.index_e(gamma, c_axis))
    out_val = h.add(scaled, h.index_e(beta, c_axis))
    return h.store(out_buf, out_addr, out_val), out_buf, [in_x, gamma, beta]


# -------------------- rmsnorm ---------------------------------------
def rmsnorm(h: Thvm, R: int, C: int, *, eps: float = 1e-5):
    """out[r,c] = (x[r,c] / sqrt(mean(x^2) + eps)) * gamma[c].

    Single reduction (mean of squares).  Buffers: out=0, in_x=1, gamma=2.
    """
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=0)
    in_x = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(R, C), instance=1)
    gamma = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(C,), instance=2)

    r_axis = h.range(0, K.AXIS_LOOP, R)
    c_axis = h.range(1, K.AXIS_LOOP, C)
    cred = h.range(2, K.AXIS_REDUCE, C)

    C_c = h.iconst(C)
    inv_C = h.fconst(1.0 / float(C))
    eps_c = h.fconst(float(eps))

    sq_addr = h.iadd(h.imul(r_axis, C_c), cred)
    sq_x = h.index_e(in_x, sq_addr)
    sq_v = h.mul(sq_x, sq_x)
    sq_sum = h.reduce(K.REDUCE_SUM, axis=2, src=sq_v)
    ms_r = h.mul(sq_sum, inv_C)
    inv_rms = h.recip(h.sqrt(h.add(ms_r, eps_c)))

    out_addr = h.iadd(h.imul(r_axis, C_c), c_axis)
    norm = h.mul(h.index_e(in_x, out_addr), inv_rms)
    out_val = h.mul(norm, h.index_e(gamma, c_axis))
    return h.store(out_buf, out_addr, out_val), out_buf, [in_x, gamma]
