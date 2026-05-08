"""Build a matmul UOp DAG in Python, render to MSL via thvm.

  C[m, n] = sum_k A[m, k] * B[k, n]

Shape (M, K) @ (K, N) -> (M, N), fp32.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from py.thvm import Thvm, K


def build_matmul(h: Thvm, M: int, N: int, K_dim: int, *, with_tc: bool = False):
    """Build the canonical matmul UOp:
        STORE(C, m*N+n, REDUCE(MUL(A[m*K+k], B[k*N+n]), SUM, axis=k))
    Returns the STORE root term.
    """
    # Buffers (instance disambiguates output vs inputs in the renderer)
    out_buf = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
    a_buf   = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, K_dim), instance=1)
    b_buf   = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(K_dim, N), instance=2)

    # Axis ranges
    m_axis = h.range(axis_id=0, axis_type=K.AXIS_LOOP,   extent=M)
    n_axis = h.range(axis_id=1, axis_type=K.AXIS_LOOP,   extent=N)
    k_axis = h.range(axis_id=2, axis_type=K.AXIS_REDUCE, extent=K_dim)

    # Integer constants for stride arithmetic
    K_c = h.iconst(K_dim)
    N_c = h.iconst(N)

    # a[m, k] address: m*K + k
    a_addr = h.iadd(h.imul(m_axis, K_c), k_axis)
    # b[k, n] address: k*N + n
    b_addr = h.iadd(h.imul(k_axis, N_c), n_axis)
    # c[m, n] address: m*N + n
    c_addr = h.iadd(h.imul(m_axis, N_c), n_axis)

    # Indexed loads
    a_load = h.index_e(a_buf, a_addr)
    b_load = h.index_e(b_buf, b_addr)

    # FP multiply + reduce-sum across k
    prod = h.mul(a_load, b_load)
    reduced = h.reduce(K.REDUCE_SUM, axis=2, src=prod)

    # Optional: wrap reduce with OPT(_, TC) to fire the simdgroup_matrix template
    if with_tc:
        reduced = h.opt(reduced, K.OPT_TC, 0)

    return h.store(out_buf, c_addr, reduced)


def main() -> int:
    h = Thvm()

    # Print plain (LOOP-only, no OPT) and TC-annotated variants
    M, N, K_dim = 64, 64, 64

    plain = build_matmul(h, M, N, K_dim, with_tc=False)
    msl_plain = h.render(plain, name="matmul_plain")
    print("=" * 78)
    print("PLAIN (no OPT)")
    print("=" * 78)
    print(msl_plain)

    tc = build_matmul(h, M, N, K_dim, with_tc=True)
    msl_tc = h.render(tc, name="matmul_tc")
    print("=" * 78)
    print("OPT(_, TC) -- expect simdgroup_matrix template")
    print("=" * 78)
    print(msl_tc)
    return 0


if __name__ == "__main__":
    sys.exit(main())
