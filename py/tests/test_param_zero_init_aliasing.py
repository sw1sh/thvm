"""Zero-initialized parameters must own distinct buffers.

In tinygrad 0.11 two parameters built as `Tensor.zeros(shape).contiguous()`
hash-consed to one UOp and shared a buffer, which also aliased Adam's
zero-initialized m / v state: training went NaN in epoch 0.  py/thvm builds
zeros and full from numpy, one buffer per call; this pins that every
zero-init spelling trains independently under Adam.

Run:
  cd <repo> && PYTHONPATH=py DEV=cpu python3 -m pytest py/tests/test_param_zero_init_aliasing.py -q
"""
import unittest

import numpy as np

from thvm import Context, Tensor, nn


def _np_adam(target, steps, lr=0.1, b1=0.9, b2=0.999, eps=1e-8):
    p = np.zeros_like(target, dtype=np.float64)
    m = np.zeros_like(p)
    v = np.zeros_like(p)
    for t in range(1, steps + 1):
        g = 2.0 * (p - target)
        m = b1 * m + (1 - b1) * g
        v = b2 * v + (1 - b2) * g * g
        p = p - lr * (m / (1 - b1 ** t)) / (np.sqrt(v / (1 - b2 ** t)) + eps)
    return p


class TestParamZeroInitAliasing(unittest.TestCase):
    SPELLINGS = {
        "zeros": lambda: Tensor.zeros(3),
        "zeros.contiguous": lambda: Tensor.zeros(3).contiguous(),
        "full": lambda: Tensor.full((3,), 0.0),
        "zeros_like": lambda: Tensor.zeros_like(Tensor([7.0, 8.0, 9.0])),
    }

    def test_zero_params_train_independently(self):
        ta = np.array([1.0, 2.0, 3.0], np.float32)
        steps = 5
        want = _np_adam(ta.astype(np.float64), steps)
        for name, make in self.SPELLINGS.items():
            with self.subTest(spelling=name):
                a, b = make(), make()
                opt = nn.optim.Adam([a, b], lr=0.1)
                with Context(TRAINING=1):
                    for _ in range(steps):
                        opt.zero_grad()
                        loss = (((a - Tensor(ta)) * (a - Tensor(ta))).sum()
                                + ((b + Tensor(ta)) * (b + Tensor(ta))).sum())
                        loss.backward()
                        opt.step()
                an, bn = a.numpy(), b.numpy()
                self.assertTrue(np.isfinite(an).all() and np.isfinite(bn).all())
                np.testing.assert_array_equal(an, -bn)
                np.testing.assert_allclose(an, want, rtol=1e-5)

    def test_assign_to_one_leaves_the_other(self):
        for name, make in self.SPELLINGS.items():
            with self.subTest(spelling=name):
                a, b = make(), make()
                a.assign(Tensor([4.0, 5.0, 6.0])).realize()
                np.testing.assert_array_equal(b.numpy(), np.zeros(3, np.float32))


if __name__ == "__main__":
    unittest.main()
