"""Tensor.prod and cumprod are tinygrad's MUL reduce (mixin/reduce.py:47-71, mixin/op.py:752-796).

REDUCE_PROD folds from 1 by MUL in the C kernels and the UOp walker; a bool prod is an AND,
which Tensor.all reduces by, and an int one wraps.  A range the body never reads raises the body
to its extent, tinygrad's reduce_unparented (codegen/simplify.py:89-90), as an expanded tensor's
prod does; cumprod past 512 runs tinygrad's two-stage split.  The gradient is tinygrad's
(mixin/gradient.py:14-18): y / x away from a zero, the product of the others at the only zero,
0 past one.  Each case runs against numpy on both CPU routes (the C JIT and the UOp walker) in
child processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_prod.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor

R = np.random.default_rng(3)
X = R.uniform(-2, 2, (3, 4, 5, 6)).astype(np.float32)


def _close(got, want, rtol=1e-6):
    np.testing.assert_allclose(got, want, rtol=rtol, atol=1e-30)


def case_prod_axes():
    _close(Tensor(X).prod().numpy(), X.astype(np.float64).prod(), rtol=1e-4)
    _close(Tensor(X).prod(3).numpy(), X.prod(3))
    _close(Tensor(X).prod(1, keepdim=True).numpy(), X.prod(1, keepdims=True))
    _close(Tensor(np.float32(2.5)).prod().numpy(), np.float32(2.5))


def case_prod_zeros():
    for v in ([1.0, 2.0, 3.0], [0.0, 2.0, 3.0], [0.0, 0.0, 3.0]):
        a = np.array(v, np.float32)
        _close(Tensor(a).prod().numpy(), a.prod())


def case_prod_int_wraps_and_bool_all():
    a = np.array([[7, -3, 100000], [65536, 65536, 1]], np.int32)
    np.testing.assert_array_equal(Tensor(a).prod(1).numpy(), a.prod(1, dtype=np.int32))
    p = np.array([[True, True, False], [True, True, True]])
    np.testing.assert_array_equal(Tensor(p).all(1).numpy(), p.all(1))
    np.testing.assert_array_equal(Tensor(p).all().numpy(), p.all())


def case_prod_of_expand_is_a_power():
    a = np.array([1.5, -2.0, 0.5], np.float32)
    got = Tensor(a).reshape(3, 1).expand(3, 7).prod(1).numpy()
    _close(got, a.astype(np.float64) ** 7)
    _close(Tensor(a).full_like(2.0).reshape(3, 1).expand(3, 3).prod().numpy(), 512.0)


def case_cumprod():
    a = R.uniform(-2, 2, (20, 30)).astype(np.float32)
    _close(Tensor(a).cumprod(0).numpy(), np.cumprod(a, 0), rtol=1e-5)
    _close(Tensor(a).cumprod(1).numpy(), np.cumprod(a, 1), rtol=1e-5)


def case_cumprod_split():
    # Past 2 * 256 the cumulative op runs per chunk, then over the chunk ends.
    a = R.uniform(0.99, 1.01, 1022).astype(np.float32)
    _close(Tensor(a).cumprod(0).numpy(), np.cumprod(a.astype(np.float64)), rtol=1e-4)


def case_prod_grad():
    a = np.array([[1, 2, 3], [0, 2, 3], [0, 0, 3]], np.float32)
    x = Tensor(a)
    x.prod(1).sum().backward()
    np.testing.assert_array_equal(x.grad.numpy(), [[6, 3, 2], [6, 0, 0], [0, 0, 0]])


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_prod"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
