"""Tensor methods mirrored from tinygrad's public surface.

softmin (mixin/op.py:709-724) and max_shape / max_numel
(mixin/movement.py:49-57), checked against numpy and, when importable,
against tinygrad itself.
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor


def _np_softmin(x, axis):
    e = np.exp(-(x - x.min(axis=axis, keepdims=True)))
    return e / e.sum(axis=axis, keepdims=True)


class TestTensorSurface(unittest.TestCase):
    def setUp(self):
        self.X = np.random.default_rng(0).standard_normal((3, 5)).astype(np.float32)

    def test_softmin_matches_numpy(self):
        for axis in (-1, 0):
            got = Tensor(self.X).softmin(axis=axis).numpy()
            np.testing.assert_allclose(got, _np_softmin(self.X, axis), atol=1e-6)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_softmin_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        got = Tensor(self.X).softmin().numpy()
        np.testing.assert_allclose(got, TgTensor(self.X).softmin().numpy(), atol=1e-6)

    def test_max_shape_is_the_concrete_shape(self):
        t = Tensor(self.X).reshape(5, 3)
        self.assertEqual(t.max_shape, (5, 3))
        self.assertEqual(t.max_numel(), 15)


if __name__ == "__main__":
    unittest.main()
