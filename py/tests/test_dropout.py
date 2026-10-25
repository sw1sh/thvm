"""Tensor.dropout is tinygrad's (mixin/rand.py): under TRAINING each element is zeroed with probability
p by an in-graph rand mask and the rest scaled by 1 / (1 - p), the gradient through the same mask;
outside TRAINING it is the identity.  scaled_dot_product_attention drops out its softmax under
TRAINING, where thvm raised for want of a dropout.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_dropout.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor
from thvm.helpers import Context

X = np.random.default_rng(0).uniform(1, 2, (64, 64)).astype(np.float32)


class TestDropout(unittest.TestCase):
    def test_identity_outside_training(self):
        x = Tensor(X)
        self.assertIs(x.dropout(0.5), x)

    def test_rate_scale_and_grad(self):
        with Context(TRAINING=1):
            x = Tensor(X)
            y = x.dropout(0.25)
            y.sum().backward()
            out, g = y.numpy(), x.grad.numpy()
        kept = out != 0
        self.assertAlmostEqual(1 - kept.mean(), 0.25, delta=0.03)
        np.testing.assert_allclose(out[kept], X[kept] / 0.75, rtol=1e-6)
        np.testing.assert_allclose(g, kept / 0.75, rtol=1e-6)

    def test_attention_dropout(self):
        q, k, v = (Tensor(np.random.default_rng(i).standard_normal((2, 3, 5, 8)).astype(np.float32)) for i in range(3))
        want = q.scaled_dot_product_attention(k, v, is_causal=True).numpy()
        with Context(TRAINING=1):
            same = q.scaled_dot_product_attention(k, v, is_causal=True, dropout_p=1e-9).numpy()
            dropped = q.scaled_dot_product_attention(k, v, is_causal=True, dropout_p=0.5).numpy()
        np.testing.assert_allclose(same, want, rtol=1e-5, atol=1e-6)
        self.assertEqual(dropped.shape, want.shape)
        self.assertFalse(np.allclose(dropped, want))


if __name__ == "__main__":
    unittest.main()
