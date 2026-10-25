"""GlobalCounters counts as tinygrad's (helpers.py:296-304, engine/realize.py:65-67): kernel_count, and
global_ops and global_mem by tinygrad's Estimates (renderer/__init__.py:26-61), grow at every dispatch,
a TinyJit replay's included, until reset(); mem_used is the bytes of every live buffer.  thvm's never
counted.  TinyJit's captured.free_intermediates() releases the capture's buffers, and the next call
captures again.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_global_counters.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import GlobalCounters, Tensor, TinyJit


class TestGlobalCounters(unittest.TestCase):
    def test_elementwise(self):
        n = 64
        a, b = Tensor.rand(n).realize(), Tensor.rand(n).realize()
        GlobalCounters.reset()
        (a + b).realize()
        self.assertEqual(GlobalCounters.kernel_count, 1)
        # One add per element, and the index arithmetic, which tinygrad counts too.
        self.assertGreaterEqual(GlobalCounters.global_ops, n)
        self.assertLessEqual(GlobalCounters.global_ops, 3 * n)
        self.assertEqual(GlobalCounters.global_mem, 3 * n * 4)
        GlobalCounters.reset()
        self.assertEqual((GlobalCounters.kernel_count, GlobalCounters.global_ops, GlobalCounters.global_mem), (0, 0, 0))

    def test_reduce_charges_its_iterations(self):
        a = Tensor.rand(32, 16).realize()
        GlobalCounters.reset()
        a.sum(axis=1).realize()
        self.assertEqual(GlobalCounters.kernel_count, 1)
        self.assertGreaterEqual(GlobalCounters.global_ops, 32 * 16)
        self.assertEqual(GlobalCounters.global_mem, 32 * 16 * 4 + 32 * 4)

    def test_matmul_ops(self):
        m = k = n = 16
        a, b = Tensor.rand(m, k).realize(), Tensor.rand(k, n).realize()
        GlobalCounters.reset()
        (a @ b).realize()
        self.assertGreaterEqual(GlobalCounters.global_ops, 2 * m * n * k)
        self.assertLessEqual(GlobalCounters.global_mem, (m * k + k * n + m * n) * 4)

    def test_jit_replay_counts(self):
        @TinyJit
        def f(a, b):
            c = (a + b).realize()
            d = (c * 2).realize()
            return (d - a).realize()

        a, b = Tensor.rand(64, 64).realize(), Tensor.rand(64, 64).realize()
        for _ in range(4):
            GlobalCounters.reset()
            f(a, b)
            self.assertEqual(GlobalCounters.kernel_count, 3)
            self.assertGreater(GlobalCounters.global_ops, 0)
            self.assertGreater(GlobalCounters.global_mem, 0)

    def test_mem_used_and_free_intermediates(self):
        before = GlobalCounters.mem_used
        t = Tensor.empty(4096).realize()
        self.assertGreaterEqual(GlobalCounters.mem_used - before, 4096 * 4)
        del t

        ext = Tensor([1.0, 24.0, 23.0, 45.0, 1.0]).realize()

        @TinyJit
        def f(x):
            t1 = (x * 2).realize()
            t2 = (t1 + ext).realize()
            return t2.sum().realize()

        for i in range(4):
            self.assertEqual(f(Tensor([float(i), 1, 2, 3, 4])).item(), 114 + 2 * i)
        pre = GlobalCounters.mem_used
        f.captured.free_intermediates()
        self.assertGreaterEqual(pre - GlobalCounters.mem_used, 2 * 5 * 4)
        for i in range(3):
            np.testing.assert_allclose(f(Tensor([float(i + 11), 1, 2, 3, 4])).item(), 136 + 2 * i)


if __name__ == "__main__":
    unittest.main()
