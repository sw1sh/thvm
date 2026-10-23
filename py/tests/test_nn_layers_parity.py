"""The nn layers ported from tinygrad (nn/__init__.py) against tinygrad itself, forward and the
input gradient: Conv1d, Conv2d with per-side and "same" padding, ConvTranspose1d / 2d,
GroupNorm, InstanceNorm, RMSNorm and LSTMCell.  Each layer's parameters are copied from the
tinygrad layer, so both compute from the same values."""
import unittest

import numpy as np

import helpers


def _copy_params(src, dst):
    """dst's Tensor attributes set to src's values (both layers of one class)."""
    import thvm
    for k, v in vars(src).items():
        if v is not None and hasattr(v, "numpy") and hasattr(dst, k):
            setattr(dst, k, thvm.Tensor(v.numpy()))


# (name, layer constructor args, input shape)
CASES = [
    ("conv1d", ("Conv1d", (3, 4, 3), {"stride": 2, "padding": 1}), (2, 3, 9)),
    ("conv2d_per_side", ("Conv2d", (2, 3, (2, 3)), {"padding": (2, 1, 1, 0)}), (1, 2, 5, 6)),
    ("conv2d_same", ("Conv2d", (2, 2, 3), {"padding": "same", "dilation": 2}), (1, 2, 7, 7)),
    ("conv_transpose2d", ("ConvTranspose2d", (4, 3, 3),
                          {"stride": 2, "padding": 1, "output_padding": 1}), (1, 4, 3, 3)),
    ("conv_transpose2d_groups", ("ConvTranspose2d", (4, 2, 2), {"groups": 2, "dilation": 2}),
     (2, 4, 3, 4)),
    ("conv_transpose1d", ("ConvTranspose1d", (2, 3, 3), {"stride": 3}), (2, 2, 5)),
    ("groupnorm", ("GroupNorm", (2, 6), {}), (2, 6, 3, 3)),
    ("instancenorm", ("InstanceNorm", (3,), {}), (2, 3, 4, 4)),
    ("rmsnorm", ("RMSNorm", (5,), {}), (3, 5)),
]


@unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
class TestNnLayersParity(unittest.TestCase):
    def test_abs_sign(self):
        # abs is x * sign(x), as tinygrad's: the gradient at 0 is 0, where sqrt(x * x) gave NaN.
        import thvm
        import tinygrad
        X = np.array([-2.0, -0.5, 0.0, 0.25, 3.0], np.float32)
        xt, xm = tinygrad.Tensor(X), thvm.Tensor(X)
        xt.abs().sum().backward()
        xm.abs().sum().backward()
        np.testing.assert_array_equal(xm.grad.numpy(), xt.grad.numpy())
        np.testing.assert_array_equal(thvm.Tensor(X).abs().numpy(), np.abs(X))
        np.testing.assert_array_equal(thvm.Tensor(X).sign().numpy(), np.sign(X))

    def test_lstm_cell(self):
        import thvm
        import tinygrad
        rng = np.random.default_rng(1)
        X = rng.standard_normal((2, 3)).astype(np.float32)
        H = rng.standard_normal((2, 4)).astype(np.float32)
        C = rng.standard_normal((2, 4)).astype(np.float32)
        ref = tinygrad.nn.LSTMCell(3, 4)
        ref.bias_ih = tinygrad.Tensor(rng.standard_normal(16).astype(np.float32))
        mine = thvm.nn.LSTMCell(3, 4)
        _copy_params(ref, mine)
        h_t, c_t = ref(tinygrad.Tensor(X), (tinygrad.Tensor(H), tinygrad.Tensor(C)))
        h_m, c_m = mine(thvm.Tensor(X), (thvm.Tensor(H), thvm.Tensor(C)))
        np.testing.assert_allclose(h_m.numpy(), h_t.numpy(), rtol=1e-5, atol=1e-6)
        np.testing.assert_allclose(c_m.numpy(), c_t.numpy(), rtol=1e-5, atol=1e-6)
        h0, c0 = mine(thvm.Tensor(X))
        h0_t, c0_t = ref(tinygrad.Tensor(X))
        np.testing.assert_allclose(h0.numpy(), h0_t.numpy(), rtol=1e-5, atol=1e-6)
        np.testing.assert_allclose(c0.numpy(), c0_t.numpy(), rtol=1e-5, atol=1e-6)


def _make(name, spec, shape):
    cls_name, args, kw = spec

    def test(self):
        import thvm
        import tinygrad
        rng = np.random.default_rng(0)
        X = rng.standard_normal(shape).astype(np.float32)
        ref = getattr(tinygrad.nn, cls_name)(*args, **kw)
        mine = getattr(thvm.nn, cls_name)(*args, **kw)
        # Norm layers start at weight 1, bias 0: make the affine part visible.
        for layer in (ref,):
            for k in ("weight", "bias"):
                v = getattr(layer, k, None)
                if v is not None and cls_name in ("GroupNorm", "InstanceNorm", "RMSNorm"):
                    setattr(layer, k, tinygrad.Tensor(
                        rng.standard_normal(v.shape).astype(np.float32)))
        _copy_params(ref, mine)
        xt = tinygrad.Tensor(X)
        yt = ref(xt)
        yt.square().sum().backward()
        xm = thvm.Tensor(X)
        ym = mine(xm)
        ym.square().sum().backward()
        self.assertEqual(tuple(ym.shape), tuple(yt.shape))
        np.testing.assert_allclose(ym.numpy(), yt.numpy(), rtol=1e-4, atol=1e-5)
        np.testing.assert_allclose(xm.grad.numpy(), xt.grad.numpy(), rtol=1e-4, atol=1e-4)

    return test


for _name, _spec, _shape in CASES:
    setattr(TestNnLayersParity, f"test_{_name}", _make(_name, _spec, _shape))


if __name__ == "__main__":
    unittest.main()
