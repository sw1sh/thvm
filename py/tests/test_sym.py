"""Values past float32 range stay finite where tinygrad's rewrites keep them so.

A float32 exp2 is tinygrad's xexp2, inf past 128, so a sigmoid's gradient e * (1 / (1 + e))^2
read inf * 0 = nan far from 0; tinygrad's sym rules (uop/symbolic.py:466-468, src/uop/sym.c)
rewrite x * (1 / (1 + x)) to 1 - 1 / (1 + x) on every kernel, and the RECIP and EXP2 gradients
take tinygrad's shapes so the rule reads them.  A constant exponent is tinygrad's simplify_pow
(uop/symbolic.py:16-21): an int x ** 7 multiplies, exact where exp2(7 log2 x) in float32 is not.
Each case runs on both CPU routes (the C JIT and the UOp walker) in child processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_sym.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor

FAR = np.array([-400.0, -350.0, -300.0, 300.0, 350.0, 400.0], np.float32)


def _grad(f):
    x = Tensor(FAR)
    f(x).sum().backward()
    return x.grad.numpy()


def case_sigmoid_far_grad_is_zero():
    np.testing.assert_array_equal(_grad(lambda x: x.sigmoid()), np.zeros(6, np.float32))


def case_sigmoid_alt_far_grad_is_zero():
    # tinygrad test_sigmoid_alt_extreme: exp / (1 + exp), x * (1 / (1 + x)) itself.
    np.testing.assert_array_equal(_grad(lambda x: x.exp() / (1 + x.exp())), np.zeros(6, np.float32))


def case_tanh_far_grad_is_zero():
    np.testing.assert_array_equal(_grad(lambda x: x.tanh()), np.zeros(6, np.float32))


def case_sigmoid_near_grad():
    a = np.linspace(-6, 6, 25, dtype=np.float32)
    x = Tensor(a)
    x.sigmoid().sum().backward()
    s = 1 / (1 + np.exp(-a.astype(np.float64)))
    np.testing.assert_allclose(x.grad.numpy(), s * (1 - s), rtol=2e-6, atol=1e-7)


def case_int_pow_const_is_exact():
    a = np.array([11, 12, 13, -7], np.int32)
    np.testing.assert_array_equal((Tensor(a) ** 7).numpy(), a.astype(np.int64) ** 7)
    np.testing.assert_array_equal((Tensor(a) ** 2).numpy(), a * a)
    np.testing.assert_array_equal((Tensor(a) ** 0).numpy(), np.ones(4, np.int32))


def case_float_pow_const():
    a = np.array([0.5, 2.0, 3.0, 9.0], np.float32)
    for c in (3, 2.5, 0.5, -2):
        np.testing.assert_allclose((Tensor(a) ** c).numpy(), a.astype(np.float64) ** c, rtol=2e-6)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_sym"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
