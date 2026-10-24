"""Tensor.pad with negative pads against tinygrad master (365f87a8a).

tinygrad's pad shrinks a side whose pad is negative, then pads the rest
with zeros (mixin/op.py:282-288, _pad_constant), so pad(((0, n - k), None))
cuts a k-row tensor to its first n rows when n < k.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_pad_parity.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor


def np_pad(x, pairs):
    """numpy's pad after slicing away the negative sides."""
    sl = tuple(slice(-min(b, 0), s + min(e, 0)) for (b, e), s in zip(pairs, x.shape))
    return np.pad(x[sl], [(max(b, 0), max(e, 0)) for b, e in pairs])


CASES = [
    ((5, 3), ((0, -2), None)),
    ((5, 3), ((-1, 2), (1, -1))),
    ((4, 6), ((-2, -1), (-3, 2))),
    ((2, 3, 4), (None, (-1, 0), (2, -3))),
]


class TestNegativePad(unittest.TestCase):
    def test_values_and_grad(self):
        rng = np.random.default_rng(0)
        for shape, pads in CASES:
            with self.subTest(shape=shape, pads=pads):
                pairs = [(0, 0) if p is None else p for p in pads]
                x0 = rng.standard_normal(shape).astype(np.float32)
                want = np_pad(x0, pairs)
                w0 = rng.standard_normal(want.shape).astype(np.float32)
                x = Tensor(x0)
                out = x.pad(pads)
                self.assertEqual(out.shape, want.shape)
                (out * Tensor(w0)).sum().backward()
                np.testing.assert_array_equal(out.numpy(), want)
                # the gradient is w at the kept positions, 0 where cut away
                g = np.zeros(shape, np.float32)
                sl = tuple(slice(-min(b, 0), s + min(e, 0)) for (b, e), s in zip(pairs, shape))
                g[sl] = w0[tuple(slice(max(b, 0), max(b, 0) + (s.stop - s.start))
                                 for (b, _), s in zip(pairs, sl))]
                np.testing.assert_array_equal(x.grad.numpy(), g)

    def test_flat_form(self):
        # torch's flat order, last axis first (tinygrad helpers.py:58 flat_to_grouped).
        x0 = np.arange(12, dtype=np.float32).reshape(4, 3)
        got = Tensor(x0).pad([0, -1, -1, 2]).numpy()
        np.testing.assert_array_equal(got, np_pad(x0, [(-1, 2), (0, -1)]))

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        rng = np.random.default_rng(1)
        for shape, pads in CASES:
            with self.subTest(shape=shape, pads=pads):
                x0 = rng.standard_normal(shape).astype(np.float32)
                x, tx = Tensor(x0), TgTensor(x0)
                out, tout = x.pad(pads), tx.pad(pads)
                w0 = rng.standard_normal(tout.shape).astype(np.float32)
                (out * Tensor(w0)).sum().backward()
                (tout * TgTensor(w0)).sum().backward()
                np.testing.assert_array_equal(out.numpy(), tout.numpy())
                np.testing.assert_array_equal(x.grad.numpy(), tx.grad.numpy())


if __name__ == "__main__":
    unittest.main()
