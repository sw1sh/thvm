"""A TinyJit replay writes an assign into this call's input, as tinygrad's input_replace rebinds every
use of an input (engine/jit.py:124-126): the replay copied into the capture call's buffer by tensor
id, so from the third call on t.assign(t + a) left the caller's t untouched (tinygrad's
test_simple_jit_setitem).

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_jit_assign_input.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, TinyJit


class TestJitAssignInput(unittest.TestCase):
    def test_assign_into_input(self):
        @TinyJit
        def f(t, a):
            t.assign(t + a)
            return t

        for i in range(1, 6):
            t = Tensor.zeros(6, 6).contiguous().realize()
            f(t, Tensor.full((6, 6), float(i)).contiguous())
            np.testing.assert_array_equal(t.numpy(), np.full((6, 6), i), err_msg=f"call {i}")

    def test_setitem_into_input(self):
        @TinyJit
        def f(t, a):
            t[2:4, 3:5] = a
            return t

        for i in range(1, 6):
            t = Tensor.zeros(6, 6).contiguous().realize()
            f(t, Tensor.full((2, 2), float(i)).contiguous())
            want = np.zeros((6, 6))
            want[2:4, 3:5] = i
            np.testing.assert_array_equal(t.numpy(), want, err_msg=f"call {i}")

    def test_assign_from_input(self):
        @TinyJit
        def f(t, a):
            t.assign(a)
            return t

        t = Tensor.zeros(4).contiguous().realize()
        for i in range(1, 6):
            f(t, Tensor.full((4,), float(i)).contiguous())
            np.testing.assert_array_equal(t.numpy(), np.full(4, i), err_msg=f"call {i}")


if __name__ == "__main__":
    unittest.main()
