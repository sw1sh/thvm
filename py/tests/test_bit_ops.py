"""Tensor's &, |, ^, << and >> on int tensors are value ops, tinygrad's Ops.AND/OR/XOR/SHL/SHR.

A bool tensor's and, or and xor stay where-based; an int one's are the int bit ops a kernel
computes elementwise in its operands' dtype, so a uint32 shift right is logical, an int32 one
arithmetic, and an int64 one keeps its high bits.  A Python int beside an int tensor wraps to
that dtype's width (int32 | 0xFFFFFFFF is -1), and the index simplifier folds x & 1 and x | 1
only for a 0/1 x, as tinygrad does for a bool (uop/symbolic.py:127-128).  Each case runs against
numpy on both CPU routes (the C JIT and the UOp walker) in child processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_bit_ops.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor, dtypes

A = np.array([[1, -8, 1], [32, 1, 6]], np.int32)
B = np.array([[3, 5, -1], [7, 2, 1]], np.int32)
U = np.array([[0, 1, 2], [1 << 8, 1 << 16, 1 << 30]], np.uint32)
S = np.array([[-1, -3, 1, 7], [0, -2147483648, 2147483647, -1]], np.int32)
SH = np.array([0, 2, 8, 31], np.int32)


def _eq(got, want):
    np.testing.assert_array_equal(got, want)


def case_and_or_xor():
    _eq((Tensor(A) & Tensor(B)).numpy(), A & B)
    _eq((Tensor(A) | Tensor(B)).numpy(), A | B)
    _eq((Tensor(A) ^ Tensor(B)).numpy(), A ^ B)


def case_value_const_keeps_bits():
    _eq((Tensor(A) | 0x1337).numpy(), A | 0x1337)
    _eq((0x1337 & Tensor(A)).numpy(), A & 0x1337)
    _eq((Tensor(A) & 1).numpy(), A & 1)
    _eq((Tensor(A) | 1).numpy(), A | 1)


def case_const_wraps_to_dtype():
    _eq((Tensor([0, 5], dtype=dtypes.int32) | 0xFFFFFFFF).numpy(), np.array([-1, -1], np.int32))
    w = np.array([0x1234567890, 0xFFFFFFFF, 7], np.uint64)
    _eq((Tensor(w, dtype=dtypes.uint64) & 0xFFFFFFFF).numpy(), w & np.uint64(0xFFFFFFFF))


def case_shift_int32():
    _eq((Tensor(S) >> 31).numpy(), S >> 31)
    _eq((Tensor(S) << 8).numpy(), (S.astype(np.int64) << 8).astype(np.int32))
    _eq((Tensor(S) << Tensor(SH)).numpy(), (S.astype(np.int64) << SH).astype(np.int32))
    _eq((Tensor(S) >> Tensor(SH)).numpy(), S >> SH)


def case_shift_uint32_is_logical():
    _eq((Tensor(U, dtype=dtypes.uint32) >> 2).numpy(), U >> np.uint32(2))
    _eq((Tensor(U, dtype=dtypes.uint32) << 31).numpy(), U << np.uint32(31))


def case_shift_int64_keeps_high_bits():
    w = np.array([1, -3, 0x7FFF], np.int64)
    _eq((Tensor(w, dtype=dtypes.int64) << 40).numpy(), w << 40)
    _eq(((Tensor(w, dtype=dtypes.int64) << 40) >> 36).numpy(), (w << 40) >> 36)


def case_bool_ops():
    p, q = np.array([True, True, False]), np.array([True, False, False])
    _eq((Tensor(p) ^ Tensor(q)).numpy(), p ^ q)
    _eq((Tensor(p) & Tensor(q)).numpy(), p & q)
    _eq((Tensor(p) | Tensor(q)).numpy(), p | q)


def case_bit_op_of_bit_op():
    # Two kernels: the second shifts the first's output.
    _eq(((Tensor(A) & Tensor(B)).contiguous() << 2).numpy(), (A & B) << 2)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_bit_ops"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
