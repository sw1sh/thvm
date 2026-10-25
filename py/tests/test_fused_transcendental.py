"""A fused float32 log2, sqrt or sin is the stored value: the C kernels called math.h's double
functions and the UOp walker evaluated them in double, so a consumer in the same kernel compared or
sorted the unrounded result, and x.log2() == x.log2().realize() read False everywhere.  sort's
index pass compares the input to the sorted values, so a lazy log2 sorted to all-zero indices and
multinomial without replacement drew index 0 every time.  Each case runs on both CPU routes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_fused_transcendental.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor

X = np.random.default_rng(5).uniform(0.01, 4, (2, 33)).astype(np.float32)


def _fused_eq_stored(op):
    x = Tensor(X)
    stored = getattr(x, op)().realize()
    np.testing.assert_array_equal(getattr(x, op)().eq(stored).numpy(), True)


def case_log2():
    _fused_eq_stored("log2")


def case_sqrt():
    _fused_eq_stored("sqrt")


def case_sin():
    _fused_eq_stored("sin")


def case_log():
    _fused_eq_stored("log")


def case_lazy_sort_indices():
    want = np.argsort(-np.log2(X), axis=1, kind="stable")[:, :5]
    np.testing.assert_array_equal(Tensor(X).log2().topk(5, dim=1)[1].numpy(), want)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_fused_transcendental"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
