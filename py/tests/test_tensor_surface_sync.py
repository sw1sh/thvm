"""Tensor methods synced to tinygrad 365f87a8a.

clone (tensor.py:344-351, uop/ops.py:887-893), to(device, force)
(tensor.py:353-367) and var / std with Bessel's correction
(mixin/op.py:523-607), checked against numpy and, when importable,
against tinygrad itself.
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor, dtypes
from thvm.tensor import _TH


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

    def test_clone_passes_gradient_through(self):
        # A lazy assign into a new buffer passes the gradient to its source
        # (mixin/gradient.py:98-99).
        x = Tensor([1., 2, 3])
        (x.clone() * 2).sum().backward()
        np.testing.assert_array_equal(x.grad.numpy(), [2., 2., 2.])

    def test_clone_copies_narrow_dtypes_bit_exact(self):
        # NaN payloads, subnormals and -0 survive only a byte copy.
        cases = [
            (dtypes.float16, np.uint16, [0x7C01, 0xFE01, 0x0001, 0x8000]),
            (dtypes.bfloat16, np.uint16, [0x7F81, 0xFFC1, 0x0001, 0x8000]),
            (dtypes.fp8e5m2, np.uint8, [0x7D, 0xFE, 0x01, 0x80]),
            (dtypes.int8, np.uint8, [0x80, 0xFF, 0x01, 0x7F]),
        ]
        for dt, raw, bits in cases:
            data = np.array(bits, raw).tobytes()
            src = _TH.ten_create(dt.thvm_id, [len(bits)])
            _TH.ten_write(src, data)
            t = Tensor._from_term(src, dt, (len(bits),))
            c = t.clone().realize()
            self.assertEqual(c.dtype, dt)
            self.assertNotEqual(_TH.ten_get_buf_id(c.term), _TH.ten_get_buf_id(t.term))
            self.assertEqual(_TH.ten_read(c.term, len(data)), data, dt.name)


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


class TestVarStd(unittest.TestCase):
    X = np.array([[1., 2, 3], [4, 5, 7]], np.float32)

    def test_var_of_int_input_is_float(self):
        v = Tensor(np.array([1, 2, 3, 4], np.int32)).var()
        self.assertEqual(v.dtype, dtypes.float32)
        np.testing.assert_allclose(v.item(), 5 / 3, rtol=1e-6)

    def test_std_defaults_to_bessel_correction(self):
        np.testing.assert_allclose(Tensor(self.X).std(axis=1).numpy(),
                                   np.std(self.X, axis=1, ddof=1), rtol=1e-6)

    def test_var_correction_zero_is_the_population_variance(self):
        np.testing.assert_allclose(Tensor(self.X).var(correction=0).numpy(),
                                   np.var(self.X), rtol=1e-6)

    def test_var_axes_and_keepdim(self):
        v = Tensor(self.X).var(axis=(0, 1), keepdim=True)
        self.assertEqual(v.shape, (1, 1))
        np.testing.assert_allclose(v.numpy(), np.var(self.X, ddof=1, keepdims=True),
                                   rtol=1e-6)
        np.testing.assert_allclose(Tensor(self.X).var(axis=-1).numpy(),
                                   np.var(self.X, axis=-1, ddof=1), rtol=1e-6)

    def test_std_of_int_input_is_float(self):
        x = np.array([[1, 2], [4, 8]], np.int32)
        s = Tensor(x).std(axis=0)
        self.assertEqual(s.dtype, dtypes.float32)
        np.testing.assert_allclose(s.numpy(), np.std(x, axis=0, ddof=1), rtol=1e-6)

    def test_var_without_degrees_of_freedom_is_nan(self):
        self.assertTrue(np.isnan(Tensor([3.0]).var().item()))

    def test_var_mean_and_std_mean(self):
        v, m = Tensor(self.X).var_mean(axis=0)
        np.testing.assert_allclose(v.numpy(), np.var(self.X, axis=0, ddof=1), rtol=1e-6)
        np.testing.assert_allclose(m.numpy(), np.mean(self.X, axis=0), rtol=1e-6)
        s, m = Tensor(self.X).std_mean(axis=1, keepdim=True, correction=0)
        np.testing.assert_allclose(s.numpy(), np.std(self.X, axis=1, keepdims=True),
                                   rtol=1e-6)
        np.testing.assert_allclose(m.numpy(), np.mean(self.X, axis=1, keepdims=True),
                                   rtol=1e-6)

    def test_mean_matches_numpy(self):
        for axis in (None, 0, 1, -1, (0, 1), (-2, 1)):
            for keepdim in (False, True):
                got = Tensor(self.X).mean(axis=axis, keepdim=keepdim)
                want = np.mean(self.X, axis=axis, keepdims=keepdim)
                self.assertEqual(got.shape, want.shape)
                np.testing.assert_allclose(got.numpy(), want, rtol=1e-6)
        m = Tensor(np.array([1, 2, 4], np.int32)).mean()
        self.assertEqual(m.dtype, dtypes.float32)
        np.testing.assert_allclose(m.item(), 7 / 3, rtol=1e-6)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_var_std_match_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        for kw in ({}, {"axis": 0}, {"axis": -1, "keepdim": True},
                   {"axis": (0, 1), "correction": 0}):
            np.testing.assert_allclose(Tensor(self.X).var(**kw).numpy(),
                                       TgTensor(self.X).var(**kw).numpy(), rtol=1e-6)
            np.testing.assert_allclose(Tensor(self.X).std(**kw).numpy(),
                                       TgTensor(self.X).std(**kw).numpy(), rtol=1e-6)
        xi = np.array([1, 2, 3, 4], np.int32)
        np.testing.assert_allclose(Tensor(xi).var().numpy(),
                                   TgTensor(xi).var().numpy(), rtol=1e-6)


class TestGelu(unittest.TestCase):
    """gelu's approximate modes (mixin/elementwise.py:780-795)."""

    def test_none_is_the_erf_form(self):
        import math
        x = np.linspace(-3, 3, 13).astype(np.float32)
        want = np.array([v * 0.5 * (1.0 + math.erf(v / math.sqrt(2))) for v in x], np.float32)
        np.testing.assert_allclose(Tensor(x).gelu(approximate="none").numpy(), want, atol=2e-6)
        self.assertRaises(RuntimeError, Tensor(x).gelu, approximate="bogus")

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not importable")
    def test_none_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        x = np.linspace(-3, 3, 13).astype(np.float32)
        np.testing.assert_allclose(Tensor(x).gelu(approximate="none").numpy(),
                                   TgTensor(x).gelu(approximate="none").numpy(), rtol=1e-6, atol=1e-7)


class TestMaxPoolCeilMode(unittest.TestCase):
    """ceil_mode (mixin/op.py:1294-1305, 1356-1399): a no-op when the floor sizes cover the
    input; a window past it reads the dtype's min pad, so it takes the max of what it covers."""

    def test_covered_input_matches_floor(self):
        x = np.arange(32.).reshape(1, 2, 4, 4).astype(np.float32)
        np.testing.assert_array_equal(Tensor(x).max_pool2d(ceil_mode=True).numpy(),
                                      Tensor(x).max_pool2d().numpy())

    def test_window_past_the_input(self):
        x = np.arange(50.).reshape(1, 2, 5, 5).astype(np.float32)
        want = np.pad(x, ((0, 0), (0, 0), (0, 1), (0, 1)), constant_values=-np.inf)
        want = want.reshape(1, 2, 3, 2, 3, 2).max((3, 5))
        np.testing.assert_array_equal(Tensor(x).max_pool2d(ceil_mode=True).numpy(), want)


class TestTranspose(unittest.TestCase):
    """transpose() and .T swap dims 0 and 1 (mixin/movement.py:323-343)."""

    def test_default_swaps_dims_0_and_1(self):
        x = np.arange(24.).reshape(2, 3, 4).astype(np.float32)
        np.testing.assert_array_equal(Tensor(x).transpose().numpy(), x.transpose(1, 0, 2))
        np.testing.assert_array_equal(Tensor(x).T.numpy(), x.transpose(1, 0, 2))
        np.testing.assert_array_equal(Tensor(x).transpose(-1, -2).numpy(), x.transpose(0, 2, 1))

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not importable")
    def test_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        x = np.arange(24.).reshape(2, 3, 4).astype(np.float32)
        np.testing.assert_array_equal(Tensor(x).T.numpy(), TgTensor(x).T.numpy())


if __name__ == "__main__":
    unittest.main()
