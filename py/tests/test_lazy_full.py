"""Tensor.full, zeros and ones are lazy constants, as tinygrad's Tensor(v).reshape(1s).expand(shape)
(mixin/creation.py full), not host buffers filled in numpy.  backward() makes a float one a leaf, as
tinygrad's backward sees every in-scope float tensor (tensor.py:457-460), so a Tensor.ones weight
gets its .grad; an assign into one initializes it (tensor.py:253-256).

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_lazy_full.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, dtypes

X = np.array([[1.0, -2.0, 3.0], [0.5, 4.0, -1.0]], np.float32)


class TestLazyFull(unittest.TestCase):
    def test_ones_weight_gets_grad(self):
        w = Tensor.ones(3)
        (Tensor(X) * w).sum().backward()
        np.testing.assert_allclose(w.grad.numpy(), X.sum(0), rtol=1e-6)

    def test_zeros_bias_gets_grad(self):
        b = Tensor.zeros(3)
        ((Tensor(X) + b) ** 2).sum().backward()
        np.testing.assert_allclose(b.grad.numpy(), (2 * X).sum(0), rtol=1e-6)

    def test_assign_initializes(self):
        m = Tensor.zeros(2, 3)
        m.assign(Tensor(X) * 2)
        np.testing.assert_array_equal(m.numpy(), X * 2)
        m.assign(m + 1)
        np.testing.assert_array_equal(m.numpy(), X * 2 + 1)

    def test_values_and_dtypes(self):
        np.testing.assert_array_equal(Tensor.full((2, 2), 7).numpy(), np.full((2, 2), 7, np.int32))
        self.assertEqual(Tensor.full((2,), True).dtype, dtypes.bool)
        self.assertEqual(Tensor.ones(2).dtype, dtypes.float32)
        big = Tensor.full((2,), 2**40, dtype=dtypes.int64).numpy()
        np.testing.assert_array_equal(big, np.full(2, 2**40, np.int64))


if __name__ == "__main__":
    unittest.main()
