"""A pending assign realizes with every realize: one fired inside another tensor's realize stayed its
tensor's term, a UOP_ASSIGN over a kernel the kernel GC then stripped, so x.assign(x + 1); (x * 2).numpy();
x.numpy() raised "realize fired kernel ..., which the kernel GC stripped".  Reads still come before
the write (tinygrad's WAR order), and a TinyJit capture starts with no pending assign from outside it.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_pending_assign.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, TinyJit


class TestPendingAssign(unittest.TestCase):
    def test_read_through_another_tensor_first(self):
        x = Tensor([1.0, 2.0]).clone()
        x.assign(x + 1)
        np.testing.assert_array_equal((x * 2).numpy(), [4.0, 6.0])
        np.testing.assert_array_equal(x.numpy(), [2.0, 3.0])

    def test_earlier_read_sees_the_old_value(self):
        t = Tensor([1.0, 2.0]).clone()
        s0 = t.sum()
        t.assign(t * 10)
        self.assertEqual(s0.item(), 3.0)
        np.testing.assert_array_equal(t.numpy(), [10.0, 20.0])

    def test_outside_assign_is_not_captured(self):
        x = Tensor([1.0, 2.0]).clone()
        x.assign(x + 1)

        @TinyJit
        def f(y):
            return (y * 2).realize()

        for _ in range(4):
            f(Tensor([1.0, 1.0]))
        np.testing.assert_array_equal(x.numpy(), [2.0, 3.0])


if __name__ == "__main__":
    unittest.main()
