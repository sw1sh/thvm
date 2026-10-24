"""A float32 exp2 on the CPU is tinygrad's xexp2 (codegen/decomp/transcendental.py:193-217).

tinygrad decomposes EXP2 where the device has no exp2 instruction, as the CPU has none: a
degree-6 polynomial on the fraction of d, scaled by 2^q through the exponent bits.  thvm's C
kernels and its UOp walker both run that one function (src/codegen/xexp2f.c), so the bits must
equal xexp2's, written here in numpy float32 op by op, on both CPU routes (the C JIT and the
walker) in child processes; Tensor.exp and Tensor.exp2 read it, and the values stay within 2 ulp
of numpy's exp2.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_xexp2.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor

F = np.float32
C = [0.1535920892e-3, 0.1339262701e-2, 0.9618384764e-2, 0.5550347269e-1, 0.2402264476e+0,
     0.6931471825e+0, 1.0]


def xexp2(d):
    """tinygrad's xexp2 for float32, every op rounded to float32."""
    d = np.asarray(d, F)
    with np.errstate(all="ignore"):
        x = np.where(np.isfinite(d), d, F(0))
        q = (x + np.where(x < 0, F(-0.5), F(0.5))).astype(np.int32)
        s = x - q.astype(F)
        u = F(C[0])
        for c in C[1:]:
            u = u * s + F(c)
        h = q >> 1
        p0 = ((h + 127).astype(np.uint32) << 23).view(F)
        p1 = ((q - h + 127).astype(np.uint32) << 23).view(F)
        u = (u * p0) * p1
        u = np.where(d < F(128), u, F(np.inf))
        u = np.where(d < F(-150), F(0), u)
        return np.where(d == d, u, F(np.nan)).astype(F)


X = np.concatenate([np.linspace(-160, 140, 4001, dtype=F),
                    np.array([0, -0.0, 0.5, -0.5, 1e-30, 127.9, 128, -149.5, -150, -150.5,
                              np.inf, -np.inf, np.nan], F)])


def _bits_equal(got, want):
    np.testing.assert_array_equal(np.asarray(got, F).view(np.uint32), want.view(np.uint32))


def case_exp2_bits():
    _bits_equal(Tensor(X).exp2().numpy(), xexp2(X))


def case_exp_bits():
    log2e = F(1.4426950408889634)
    _bits_equal(Tensor(X).exp().numpy(), xexp2(X * log2e))


def case_exp2_of_exp2():
    # Two kernels: the second reads the first's output.
    small = np.linspace(-6, 6, 257, dtype=F)
    _bits_equal(Tensor(small).exp2().contiguous().exp2().numpy(), xexp2(xexp2(small)))


def case_exp2_close_to_numpy():
    fin = X[np.isfinite(X) & (X > -126) & (X < 127)]
    np.testing.assert_allclose(Tensor(fin).exp2().numpy(), np.exp2(fin.astype(np.float64)), rtol=2.5e-7)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_xexp2"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
