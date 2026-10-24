"""Reads through shrink and pad views fused into one kernel, and the gradients through them.

Several views of one buffer share the first view's input slot.  A one-element BUFFER is read at
CONST(0), since the unified address can run past the element; a slot bound to an offset view's
raw buffer instead reads the view's own address with its ranges at 0, or x[2:3] * 3 read x[0] and
x[0:1] + x[2:3] read x[0] twice.  A partly realized producer (one closed axis) read at a bare
RANGE binds its closed range through the per-axis side table: the address also spans the open
axes, and L[0:1] + L[1:2] of a lazy 2-D L bound the row to the column range.  A realized PAD
stages its guarded value, so a consumer that inlines it reads 0 past the kept window rather than
the folded INVALID address.  And tinygrad's broadcast of 1 against 0 is 0, the gradient of a
leaf with respect to itself is the seed, and an EXPAND from 1 to 0 sums to zeros.  A replicate
pad's gradient sums broadcast edges whose reduce axis the inline drops: the repair after it is
tinygrad's reduce_unparented, a SUM times its extent; and a no-op SHRINK passes its gradient
through, where its zero-width PAD inverse hit that same reduce.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_view_reads.py -q
"""
import unittest

import numpy as np

from thvm import Tensor


class TestViewReads(unittest.TestCase):
    def check(self, got, want):
        np.testing.assert_allclose(got.numpy(), np.asarray(want, np.float32), rtol=1e-6)

    def test_one_element_offset_views(self):
        a = np.array([1.0, 2.0, 4.0], np.float32)
        x = Tensor(a)
        self.check(x[2:3] * 3.0, a[2:3] * 3)
        self.check(x[0:1] + x[2:3], a[0:1] + a[2:3])
        self.check(x[1:2] * x[2:3], a[1:2] * a[2:3])
        y = np.arange(12, dtype=np.float32).reshape(3, 4)
        self.check(Tensor(y)[2:3, 1:2] * 3.0, y[2:3, 1:2] * 3)
        self.check(Tensor(y)[0:1, 0:1] + Tensor(y)[0:1, 2:3], y[0:1, 0:1] + y[0:1, 2:3])

    def test_partly_realized_rows(self):
        a = np.array([[1.0, 3.0], [5.0, 7.0]], np.float32)
        L = Tensor(a) * 1.5
        self.check(L[0:1] + L[1:2], (a[0:1] + a[1:2]) * 1.5)

    def test_shrinks_of_a_pad(self):
        a = np.array([[1.0, 3.0], [5.0, 7.0]], np.float32)
        p = np.pad(a, ((0, 2), (0, 0)))
        P = Tensor(a).pad(((0, 2), None))
        self.check(P[0:2] + P[2:4], p[0:2] + p[2:4])
        self.check(P[1:3] + P[2:4], p[1:3] + p[2:4])
        q = np.pad(a[:1], ((0, 1), (0, 0)))
        Q = Tensor(a[:1]).pad(((0, 1), None))
        self.check(Q[0:1] + Q[1:2], q[0:1] + q[1:2])
        self.check(Q[1:2] * Q[0:1], q[1:2] * q[0:1])

    def test_broadcast_one_against_zero(self):
        self.assertEqual((Tensor(np.ones((5, 0, 3), np.float32)) + Tensor([[1.0, 2.0, 3.0]])).shape, (5, 0, 3))
        self.assertEqual(Tensor(np.ones((5, 0, 3), np.float32)).tril().shape, (5, 0, 3))

    def grad(self, x, f):
        f(x).sum().backward()
        return x.grad.numpy()

    def test_gradients(self):
        np.testing.assert_equal(self.grad(Tensor(np.float32(2.0)), lambda x: x), 1.0)
        np.testing.assert_equal(self.grad(Tensor(np.ones((3, 1, 4), np.float32)), lambda x: x.expand(3, 0, 4)),
                                np.zeros((3, 1, 4)))
        np.testing.assert_equal(self.grad(Tensor(np.float32(2.0)), lambda x: x.reshape(1, 1).expand(2, 3)), 6.0)
        np.testing.assert_equal(self.grad(Tensor(np.ones(4, np.float32)), lambda x: Tensor.stack(x, x, x)[0]),
                                np.ones(4))

    def test_replicate_gradient(self):
        x = Tensor(np.arange(25, dtype=np.float32).reshape(1, 1, 5, 5))
        xb = x.shrink(((0, 1), (0, 1), (0, 1), (0, 5))).expand(1, 1, 3, 5)
        xa = x.shrink(((0, 1), (0, 1), (4, 5), (0, 5))).expand(1, 1, 2, 5)
        y = xb.cat(x, xa, dim=2)
        y.cat(y.shrink(((0, 1), (0, 1), (0, 10), (4, 5))).expand(1, 1, 10, 2), dim=3).sum().backward()
        want = np.ones((5, 5), np.float32)
        want[0] *= 4
        want[4] *= 3
        want[:, 4] *= 3
        np.testing.assert_equal(x.grad.numpy()[0, 0], want)

    def test_noop_shrink_gradient(self):
        from thvm.tensor import _TH
        x = Tensor(np.ones((1, 1, 5, 5), np.float32))
        c = x.cat(x.shrink(((0, 1), (0, 1), (0, 5), (4, 5))).expand(1, 1, 5, 2), dim=3)
        Tensor._from_term(_TH.shrink(c.term, [0, 1, 0, 1, 0, 5, 0, 7]), c.dtype, c.shape).sum().backward()
        np.testing.assert_equal(x.grad.numpy()[0, 0, 0], [1, 1, 1, 1, 3])


if __name__ == "__main__":
    unittest.main()
