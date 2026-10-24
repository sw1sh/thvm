"""b ** e is tinygrad's POW: a node lowered at emit, with tinygrad's own gradient rule.

uop/sym.c lowers UOP_POW as tinygrad's symbolic does (uop/symbolic.py:194-197, 451): simplify_pow
for a constant exponent, exp2(e log2 c) for a positive constant base, xpow_int for an int pair (a
square and multiply over the exponent's bits that wraps, codegen/decomp/transcendental.py:267-283)
and xpow for a float one.  The gradient is tinygrad's POW rule (mixin/gradient.py:75-76), not the
lowering's: b gets e * b ** (e - 1), e gets y * log2(b) * ln 2, with tinygrad's values at a zero
base or exponent.  Each case runs on both CPU routes (the C JIT and the UOp walker) in child
processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_pow.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor, dtypes


def _eq(got, want):
    np.testing.assert_array_equal(got, want)


def case_int_pow_wraps():
    # tinygrad test_pow_int; the values are torch's.
    b = Tensor([11, 3, 2, -3, 7], dtype=dtypes.int32)
    e = Tensor([7, 40, 31, 21, 2**30], dtype=dtypes.int32)
    _eq((b ** e).numpy(), [19487171, 689956897, -2147483648, -1870418611, 1])
    neg = Tensor([0, 1, -1, 1, -1], dtype=dtypes.int32) ** Tensor([-1, -5, -5, -6, -6], dtype=dtypes.int32)
    _eq(neg.numpy(), [0, 1, -1, 1, 1])


def case_float_pow_values():
    b = np.array([2.0, -2.0, -2.0, 0.0, 9.0, -np.inf], np.float32)
    e = np.array([3.0, 3.0, 0.5, 0.0, 0.5, 0.3], np.float32)
    got = (Tensor(b) ** Tensor(e)).numpy()
    np.testing.assert_allclose(got[[0, 1, 3, 4]], [8.0, -8.0, 1.0, 3.0], rtol=1e-6)
    assert np.isnan(got[2]) and got[5] == np.inf


def case_pow_grad_rule():
    b0 = np.array([0.5, 2.0, 3.0], np.float32)
    e0 = np.array([1.5, -1.0, 2.0], np.float32)
    b, e = Tensor(b0), Tensor(e0)
    (b ** e).sum().backward()
    np.testing.assert_allclose(b.grad.numpy(), e0 * b0 ** (e0 - 1), rtol=1e-5)
    np.testing.assert_allclose(e.grad.numpy(), b0 ** e0 * np.log(b0), rtol=1e-5)


def case_pow_grad_at_zero():
    # tinygrad's rule: d/db is inf for 0 ** 0.3, 0 at e == 0; d/de is 0 at b == 0, -inf under e < 0.
    x = Tensor(np.array([0.0], np.float32))
    (x ** 0.3).sum().backward()
    assert x.grad.numpy()[0] == np.inf
    b, e = Tensor(np.array([0.0, 0.0, 2.0], np.float32)), Tensor(np.array([0.0, -1.0, 0.0], np.float32))
    (b ** e).sum().backward()
    _eq(b.grad.numpy()[[0, 2]], [0.0, 0.0])
    _eq(e.grad.numpy()[:2], [0.0, -np.inf])


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_pow"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
