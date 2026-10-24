"""tinygrad's composite Tensor ops (py/thvm/mixin.py) and the primitives under them (tensor.py):
bool compares, div / mod / trunc, pad modes, getitem, the cumulative, pool, sort and loss ops,
sin / cos, against numpy. tinygrad's own test/runtime/test_ops.py runs them on the pods.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_op_mixin.py -q
"""
import unittest

import numpy as np

from thvm import Tensor, dtypes


class TestOpMixin(unittest.TestCase):
    def setUp(self):
        self.a = np.random.default_rng(0).uniform(-3, 3, (3, 4)).astype(np.float32)
        self.x = Tensor(self.a)

    def check(self, got, want, atol=1e-5):
        np.testing.assert_allclose(np.asarray(got.numpy(), np.float64), np.asarray(want, np.float64),
                                   atol=atol, equal_nan=True)

    def test_compares_are_bool(self):
        a, x = self.a, self.x
        for got, want in [(x < 0.5, a < 0.5), (x <= 0.5, a <= 0.5), (x != 0.5, a != 0.5),
                          ((x < 1) & (x > -1), (a < 1) & (a > -1)), ((x < -1) | (x > 1), (a < -1) | (a > 1))]:
            self.assertEqual(got.dtype, dtypes.bool)
            np.testing.assert_array_equal(got.numpy(), want)

    def test_div_mod_round(self):
        a, x = self.a, self.x
        self.check(x.floor(), np.floor(a))
        self.check(x.ceil(), np.ceil(a))
        self.check(Tensor([0.5, 1.5, 2.5, -0.5, -1.5, 1.4]).round(), np.round([0.5, 1.5, 2.5, -0.5, -1.5, 1.4]))
        self.check(Tensor([7, -7, 9]) / Tensor([2, 2, 4]), [3.5, -3.5, 2.25])
        self.check(Tensor([7, -7, 9]) // Tensor([2, 2, 4]), [3, -4, 2])
        self.check(Tensor([7, -7, 9]) % 4, np.array([7, -7, 9]) % 4)
        self.check(Tensor([1.0, -1.0]) / 0, [np.inf, -np.inf])

    def test_pad_modes(self):
        a, x = self.a, self.x
        self.check(x.pad((2, 1)), np.pad(a, ((0, 0), (2, 1))))
        self.check(x.pad(((1, 1), None), value=7.0), np.pad(a, ((1, 1), (0, 0)), constant_values=7.0))
        for mode, np_mode in (("reflect", "reflect"), ("replicate", "edge"), ("circular", "wrap")):
            self.check(x.pad((2, 1), mode=mode), np.pad(a, ((0, 0), (2, 1)), mode=np_mode))

    def test_getitem(self):
        a, x = self.a, self.x
        for idx in [np.s_[::2, 1::2], np.s_[::-1, ::-2], np.s_[None, ..., 1], np.s_[-2:-5]]:
            self.check(x[idx], a[idx])
        self.check(x[[0, 2], [1, 3]], a[[0, 2], [1, 3]])
        self.check(x[:, [3, 0, 1]], a[:, [3, 0, 1]])

    def test_reduce_and_cumulative(self):
        a, x = self.a, self.x
        self.check(x.cumsum(1), np.cumsum(a, 1))
        self.check(Tensor(np.ones(600, np.float32)).cumsum(), np.arange(1, 601))
        self.check(x.cummax(1)[0], np.maximum.accumulate(a, 1))
        self.check(x.min(1), a.min(1))
        self.check(x.argmin(1), a.argmin(1))
        self.check(x.logsumexp(1), np.log(np.exp(a).sum(1)))
        np.testing.assert_array_equal((x > 2).any(1).numpy(), (a > 2).any(1))
        np.testing.assert_array_equal((x > -2).all(1).numpy(), (a > -2).all(1))

    def test_pool_sort_einsum(self):
        a, x = self.a, self.x
        b = np.arange(16, dtype=np.float32).reshape(1, 1, 4, 4)
        self.check(Tensor(b).avg_pool2d(), b.reshape(1, 1, 2, 2, 2, 2).mean((3, 5)))
        self.check(Tensor(-np.ones((1, 1, 2, 2), np.float32)).max_pool2d(kernel_size=2, stride=1, padding=1),
                   -np.ones((1, 1, 3, 3)))
        self.check(x.sort(1)[0], np.sort(a, 1))
        self.check(x.argsort(1), np.argsort(a, 1))
        self.check(x.topk(2)[0], -np.sort(-a, 1)[:, :2])
        self.check(Tensor.einsum("ij,kj->ik", x, x), a @ a.T, atol=1e-4)

    def test_losses(self):
        a, x, y = self.a, self.x, [1, 0, 3]
        logp = np.log(np.exp(a) / np.exp(a).sum(1, keepdims=True))
        self.check(x.cross_entropy(Tensor(y)), -logp[[0, 1, 2], y].mean())
        self.check(x.log_softmax().nll_loss(Tensor(y)), -logp[[0, 1, 2], y].mean())
        self.check(Tensor([0, 2, 1]).one_hot(3), np.eye(3)[[0, 2, 1]])

    def test_empty_results(self):
        # An empty realize has nothing to compute (thvm_realize): the zero-size cummax read a
        # chained view the CPU dispatch could not gather.
        v, i = Tensor(np.ones((4, 0, 3), np.float32)).cummax(1)
        self.assertEqual((v.numpy().shape, i.numpy().shape), ((4, 0, 3), (4, 0, 3)))
        self.assertEqual(Tensor(np.ones((3, 3), np.float32)).unfold(1, 0, 8).numpy().shape, (3, 1, 0))

    def test_sin_cos_and_grad(self):
        a, x = self.a, self.x
        self.check(x.sin(), np.sin(a))
        self.check(x.cos(), np.cos(a))
        self.check(x.tan(), np.tan(a), atol=1e-3)
        self.check(x.sin().sum().gradient(x)[0], np.cos(a))


if __name__ == "__main__":
    unittest.main()
