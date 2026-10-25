"""Tensor.gradient differentiates with respect to any tensor of the graph, as tinygrad's
compute_gradient does (mixin/op.py:473-488): an unrealized target is a node of the graph, and the
chain rule stops there instead of running through it to the buffers.  Before, gradient() realized
the target and differentiated with respect to its new buffer, which the graph did not read, so
dy/dx of y = (x * x).sum(), x = a * 2 read 0 where tinygrad reads 2x.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_gradient_targets.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor

A = np.array([1.0, -2.0, 3.0], np.float32)


class TestGradientTargets(unittest.TestCase):
    def test_intermediate_target(self):
        a = Tensor(A)
        x = a * 2
        y = (x * x).sum()
        dx, da = y.gradient(x, a)
        np.testing.assert_allclose(dx.numpy(), 2 * (2 * A), rtol=1e-6)
        np.testing.assert_allclose(da.numpy(), 8 * A, rtol=1e-6)

    def test_target_is_self(self):
        y = (Tensor(A) * 3).sum()
        np.testing.assert_allclose(y.gradient(y)[0].numpy(), 1.0)

    def test_target_off_the_graph(self):
        y = (Tensor(A) * 3).sum()
        z = Tensor(A) + 1
        np.testing.assert_allclose(y.gradient(z)[0].numpy(), np.zeros(3))

    def test_shared_intermediate(self):
        # x feeds y twice, through exp and directly: dy/dx = exp(x) + 1.
        a = Tensor(A)
        x = a.sigmoid()
        y = (x.exp() + x).sum()
        xs = 1 / (1 + np.exp(-A.astype(np.float64)))
        np.testing.assert_allclose(y.gradient(x)[0].numpy(), np.exp(xs) + 1, rtol=1e-5)


if __name__ == "__main__":
    unittest.main()
