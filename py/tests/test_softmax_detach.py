"""softmax, log_softmax and softmin detach their max, as tinygrad's _softmax does (mixin/op.py:681).

The gradient through the max is 0 but for rounding, which left ~1e-7 at each row's argmax where
torch reads -0.0 (tinygrad's test_softmin, grad_atol 1e-7).  The gradient of softmin(x).sum() is 0
analytically: within 2e-8 with the detach (7.5e-9 at most), where the residue read 7e-8 to
1.1e-7.  Each case runs on both CPU routes (the C JIT and the UOp walker) in child processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_softmax_detach.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor

X = np.random.default_rng(7).uniform(-2, 2, (45, 65)).astype(np.float32)


def _grad_of_sum(f):
    x = Tensor(X)
    f(x).sum().backward()
    return x.grad.numpy()


def case_softmin_sum_grad_is_zero():
    np.testing.assert_allclose(_grad_of_sum(lambda x: x.softmin(1)), 0, atol=2e-8)


def case_softmax_sum_grad_is_zero():
    np.testing.assert_allclose(_grad_of_sum(lambda x: x.softmax(1)), 0, atol=2e-8)


def case_log_softmax_grad():
    w = np.random.default_rng(8).uniform(-1, 1, X.shape).astype(np.float32)
    x = Tensor(X)
    (x.log_softmax(1) * Tensor(w)).sum().backward()
    x64 = X.astype(np.float64)
    s = np.exp(x64 - x64.max(1, keepdims=True))
    s /= s.sum(1, keepdims=True)
    np.testing.assert_allclose(x.grad.numpy(), w - s * w.sum(1, keepdims=True), rtol=1e-5, atol=1e-6)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_softmax_detach"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
