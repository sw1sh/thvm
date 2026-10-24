"""An int pair divides as ints, tinygrad's CDIV, FLOORDIV, CMOD and FLOORMOD (mixin/elementwise.py).

UOP_IDIV and UOP_IMOD, the index layer's truncating divide and remainder, are also value ops in
their operands' dtype, so a uint64 2**64 - 1 // 1 is exact where the float32 divide rounded it;
floor division and modulo are tinygrad's floordiv_to_idiv and floormod_to_mod
(codegen/decomp/op.py:54-62).  A zero divisor gives 0 on both routes, where tinygrad's C divide
traps.  Each case runs against numpy on both CPU routes (the C JIT and the UOp walker) in child
processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_int_div.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor, dtypes

A = np.array([-7, 7, 5, 4, -4, -8, 0, 2147483647], np.int32)
B = np.array([2, -3, 8, -2, 3, 5, 7, 3], np.int32)


def _trunc(a, b):
    return (np.sign(a) * np.sign(b) * (np.abs(a.astype(np.int64)) // np.abs(b))).astype(a.dtype)


def _eq(got, want):
    np.testing.assert_array_equal(got, want)


def case_trunc_and_floor():
    _eq(Tensor(A).div(Tensor(B), rounding_mode="trunc").numpy(), _trunc(A, B))
    _eq((Tensor(A) // Tensor(B)).numpy(), A // B)
    _eq((Tensor(A) // 3).numpy(), A // 3)


def case_mod_and_fmod():
    _eq((Tensor(A) % Tensor(B)).numpy(), np.mod(A, B))
    _eq(Tensor(A).fmod(Tensor(B)).numpy(), np.fmod(A, B))
    _eq((Tensor(A) % 5).numpy(), np.mod(A, 5))


def case_wide_dtypes_exact():
    x = Tensor(2**64 - 1, dtype=dtypes.uint64).div(1, rounding_mode="trunc")
    _eq(x.numpy(), np.uint64(2**64 - 1))
    u = np.array([4294967295, 4000000000, 7], np.uint32)
    _eq((Tensor(u) // Tensor(np.array([2, 3, 7], np.uint32))).numpy(), u // np.array([2, 3, 7], np.uint32))
    w = np.array([2**62 + 3, -(2**61) - 5, 10**18], np.int64)
    _eq((Tensor(w) // Tensor(np.array([7, 3, -9], np.int64))).numpy(), w // np.array([7, 3, -9], np.int64))


def case_zero_divisor_is_zero():
    z = np.array([0, 0, 2], np.int32)
    a = np.array([5, -5, 4], np.int32)
    _eq(Tensor(a).div(Tensor(z), rounding_mode="trunc").numpy(), np.array([0, 0, 2], np.int32))
    _eq(Tensor(a).fmod(Tensor(z)).numpy(), np.array([0, 0, 0], np.int32))


def case_div_of_div():
    # Two kernels: the second divides the first's output.
    _eq(((Tensor(A) // Tensor(B)).contiguous() // 2).numpy(), (A // B) // 2)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_int_div"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
