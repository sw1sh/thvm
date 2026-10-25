"""realize() runs only what needs storage, as tinygrad's (tensor.py:217-222, uop/ops.py:747): a graph that
reads no buffer (an arange, a full, a setitem into either) stays lazy, with no kernel and no memory, until
its data is read; contiguous() gives it a buffer.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_realize_lazy.py -q
"""
import unittest

import helpers  # noqa: F401
from thvm import GlobalCounters, Tensor, dtypes


class TestRealizeLazy(unittest.TestCase):
    def test_bufferless_graph_stays_lazy(self):
        # tinygrad's test_setitem_into_unrealized.
        GlobalCounters.reset()
        t = Tensor.arange(4, dtype=dtypes.int32).reshape(2, 2)
        t[1] = 5
        t.realize()
        Tensor.ones(3).realize()
        self.assertEqual(GlobalCounters.kernel_count, 0)
        self.assertEqual(GlobalCounters.global_mem, 0)
        self.assertEqual(t.tolist(), [[0, 1], [5, 5]])

    def test_scalar_is_a_lazy_const(self):
        # tinygrad's Tensor(True) is a CONST: masked_select by it keeps every element (its index is
        # the cumsum of a lazy mask, which the gated-load fold leaves to the reduce).
        x = Tensor([[0.0, 1.0, 2.0], [3.0, 4.0, 5.0]])
        self.assertEqual(x.masked_select(Tensor(True)).tolist(), [0.0, 1.0, 2.0, 3.0, 4.0, 5.0])
        self.assertEqual((Tensor(2.0) * 3).item(), 6.0)

    def test_contiguous_and_buffer_reads_realize(self):
        GlobalCounters.reset()
        a = Tensor.ones(4).contiguous()
        self.assertEqual(GlobalCounters.kernel_count, 1)
        b = Tensor.rand(4).realize() + a
        b.realize()
        self.assertEqual(b.numpy().shape, (4,))


if __name__ == "__main__":
    unittest.main()
