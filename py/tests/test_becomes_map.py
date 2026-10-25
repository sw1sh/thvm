"""A realize is tinygrad's becomes_map (tensor.py:22-36 _apply_map_to_tensors): every live tensor that
reads a node realize() gave a buffer reads that buffer afterwards, so a later write to the node's inputs
does not reach it, and an assign that fired, by any realize, never fires again.  contiguous() maps only
assigns, as tinygrad's realizes a CONTIGUOUS of the node; a node that reads no buffer is never mapped, as
equal fulls share it.  clone() is a lazy assign into a new buffer, which passes the gradient to its
source.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_becomes_map.py -q
"""
import unittest

import helpers  # noqa: F401
from thvm import Tensor


class TestBecomesMap(unittest.TestCase):
    def test_realized_node_is_its_buffer(self):
        a = Tensor([1.0, 2.0]).realize()
        z = a + 1
        w = z * 2
        z.realize()
        a.assign(Tensor([10.0, 10.0])).realize()
        self.assertEqual(w.tolist(), [4.0, 6.0])

    def test_contiguous_leaves_the_node(self):
        a = Tensor([1.0, 2.0]).realize()
        z = a + 1
        w = z * 2
        z.contiguous()
        a.assign(Tensor([10.0, 10.0])).realize()
        self.assertEqual(w.tolist(), [22.0, 22.0])
        self.assertEqual(z.tolist(), [2.0, 3.0])

    def test_fired_clone_does_not_fire_again(self):
        x = Tensor([1.0, 2.0]).realize()
        y = x.clone()
        v = y * 2
        y.realize()
        x.assign(Tensor([5.0, 5.0])).realize()
        self.assertEqual(v.tolist(), [2.0, 4.0])
        self.assertEqual(y.tolist(), [1.0, 2.0])

    def test_equal_fulls_keep_their_own_buffers(self):
        a, b = Tensor.zeros(3), Tensor.zeros(3)
        a.contiguous()
        a[1] = 5
        self.assertEqual(a.tolist(), [0.0, 5.0, 0.0])
        self.assertEqual(b.tolist(), [0.0, 0.0, 0.0])

    def test_clone_passes_gradient(self):
        # tinygrad's test_flip.py test_partial_store_gradient.
        x = Tensor([1.0, 2.0, 3.0, 4.0])
        y, value = x.clone(), Tensor([10.0, 20.0])
        y.flip(0)[:2].assign(value)
        gx, gv = (y * Tensor([1.0, 2.0, 3.0, 4.0])).sum().gradient(x, value)
        self.assertEqual(gx.tolist(), [1.0, 2.0, 0.0, 0.0])
        self.assertEqual(gv.tolist(), [4.0, 3.0])
        self.assertEqual(y.tolist(), [1.0, 2.0, 20.0, 10.0])


if __name__ == "__main__":
    unittest.main()
