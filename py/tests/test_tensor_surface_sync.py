"""Tensor methods synced to tinygrad 365f87a8a.

clone (tensor.py:344-351, uop/ops.py:887-893) and to(device, force)
(tensor.py:353-367), checked against numpy.
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

from thvm import Tensor, dtypes  # noqa: E402
from thvm.dtypes import DType  # noqa: E402
from thvm.tensor import _TH  # noqa: E402


class TestClone(unittest.TestCase):

    def test_clone_keeps_value_dtype_shape(self):
        a = Tensor([1., 2, 3])
        b = a.clone()
        self.assertIsNot(b, a)
        self.assertEqual(b.tolist(), [1., 2., 3.])
        self.assertEqual(b.dtype, a.dtype)
        self.assertEqual(b.shape, a.shape)

    def test_clone_owns_its_storage(self):
        a = Tensor([1., 2, 3])
        b = a.clone()
        self.assertNotEqual(_TH.ten_get_buf_id(b.term), _TH.ten_get_buf_id(a.term))
        b.assign(Tensor([7., 8, 9])).realize()
        self.assertEqual(a.tolist(), [1., 2., 3.])
        self.assertEqual(b.tolist(), [7., 8., 9.])

    def test_clone_of_views_and_lazy_values(self):
        x = np.arange(12.).reshape(3, 4).astype(np.float32)
        a2 = Tensor(x)
        cases = [
            (a2.permute(1, 0), x.T),
            (a2[1:, 1:], x[1:, 1:]),
            (a2[:1].expand(3, 4), np.broadcast_to(x[:1], (3, 4))),
            (a2 * 2 + 1, x * 2 + 1),
        ]
        for src, want in cases:
            got = src.clone()
            self.assertEqual(got.shape, want.shape)
            np.testing.assert_array_equal(got.numpy(), want)

    def test_clone_int32(self):
        a = Tensor(np.array([1, -2, 3], np.int32))
        b = a.clone()
        self.assertEqual(b.dtype, dtypes.int32)
        self.assertEqual(b.tolist(), [1, -2, 3])

    def test_clone_0d_and_zero_size(self):
        z = Tensor(5.0).clone()
        self.assertEqual(z.shape, ())
        self.assertEqual(z.item(), 5.0)
        e = Tensor(np.zeros((2, 0), np.float32)).clone()
        self.assertEqual(e.shape, (2, 0))
        self.assertEqual(e.numpy().shape, (2, 0))

    def test_clone_keeps_is_param(self):
        self.assertTrue(Tensor([1., 2]).clone().is_param)
        self.assertFalse(Tensor([1., 2]).is_param_(False).clone().is_param)

    def test_clone_clones_grad(self):
        x = Tensor([1., 2, 3])
        (x * 2).sum().backward()
        c = x.clone()
        self.assertIsNotNone(c.grad)
        self.assertIsNot(c.grad, x.grad)
        self.assertEqual(c.grad.tolist(), [2., 2., 2.])
        self.assertNotEqual(_TH.ten_get_buf_id(c.grad.term),
                            _TH.ten_get_buf_id(x.grad.realize().term))

    def test_clone_does_not_pass_gradient_through(self):
        # tinygrad's clone passes the gradient to the source
        # (mixin/gradient.py:100-101); thvm's clone realizes eagerly, so
        # the source falls out of the loss's scope and its grad stays None.
        x = Tensor([1., 2, 3])
        (x.clone() * 2).sum().backward()
        self.assertIsNone(x.grad)

    def test_clone_refuses_a_dtype_assign_cannot_copy(self):
        t = Tensor([1., 2])
        t._dtype = DType("float16", 2, np.float16, 11)
        with self.assertRaises(NotImplementedError):
            t.clone()


class TestTo(unittest.TestCase):

    def test_to_force_is_the_identity(self):
        t = Tensor([1., 2.])
        self.assertEqual(t.to("CPU", force=True).tolist(), [1., 2.])
        self.assertIs(t.to("CPU", True), t)
        self.assertEqual(t.to(None).tolist(), [1., 2.])

    def test_to_force_keeps_a_lazy_tensor(self):
        u = Tensor([1., 2.]) + 1
        self.assertIs(u.to("CPU", force=True), u)
        self.assertEqual(u.tolist(), [2., 3.])

    def test_to_force_keeps_grad(self):
        x = Tensor([1., 2.])
        (x * 3).sum().backward()
        y = x.to("CPU", force=True)
        self.assertIs(y.grad, x.grad)
        self.assertEqual(y.grad.tolist(), [3., 3.])


if __name__ == "__main__":
    unittest.main()
