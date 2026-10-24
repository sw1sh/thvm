"""A reduce under an epilogue runs once per output on the UOp walker, as at the store root.

The walker took a top-level REDUCE's axes out of its output loops only when the REDUCE was the
stored value, so under -x.max() or x.max() == 0 the reduce axis also looped around the store and
reran the reduce at each of its own iterations: -(ones(2**14).max()) took 4.7 s on the walker,
where it now takes 3 ms.  2**14 stays one kernel; split_reduceop splits 2**15 and up.
Each case runs on both CPU routes (the C JIT and the UOp walker) in child processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_walk_reduce_epilogue.py -q
"""
import time
import unittest

import numpy as np

import helpers
from thvm import Tensor

X = np.random.default_rng(3).uniform(-2, 2, (2**14,)).astype(np.float32)
R = np.random.default_rng(4).uniform(-2, 2, (8, 4096)).astype(np.float32)


def _fast(f):
    t = time.perf_counter()
    out = f()
    assert time.perf_counter() - t < 0.5, "a reduce under an epilogue reran per reduce iteration"
    return out


def case_neg_max_of_ones():
    # One kernel: no buffer read makes the reduce its own.
    np.testing.assert_equal(_fast(lambda: (-Tensor.ones(2**14).max()).numpy()), -1)


def case_max_of_ones_eq_zero():
    np.testing.assert_equal(_fast(lambda: (Tensor.ones(2**14).max() == 0).numpy()), False)


def case_neg_max():
    np.testing.assert_equal(_fast(lambda: (-Tensor(X).max()).numpy()), -X.max())


def case_row_sum_epilogue():
    np.testing.assert_allclose(_fast(lambda: (Tensor(R).sum(axis=1) * 2 + 1).numpy()),
                               R.sum(axis=1) * 2 + 1, rtol=1e-5)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_walk_reduce_epilogue"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
