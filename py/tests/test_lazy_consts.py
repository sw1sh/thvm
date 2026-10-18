"""Constants and the one-hot loss stay in the graph, as tinygrad's do.

const_like is a lazy CONST (tinygrad mixin/op.py const_like), backward's
default seed is self.const_like(1.0), empty allocates without writing, and
sparse_categorical_crossentropy builds its one-hot with an in-graph compare
(mixin/op.py:1005-1008, 1723-1746), so a TinyJit replay reads each step's
labels instead of the first step's.

Run:
  cd <repo> && PYTHONPATH=py DEV=cpu python3 -m pytest py/tests/test_lazy_consts.py -q
"""
import unittest

import numpy as np

from thvm import Tensor, TinyJit, dtypes


def _ce_ref(logits, labels):
    z = logits - logits.max(axis=-1, keepdims=True)
    logp = z - np.log(np.exp(z).sum(axis=-1, keepdims=True))
    return -logp[np.arange(len(labels)), labels].mean()


def _ce_grad_ref(logits, labels):
    z = logits - logits.max(axis=-1, keepdims=True)
    p = np.exp(z) / np.exp(z).sum(axis=-1, keepdims=True)
    p[np.arange(len(labels)), labels] -= 1.0
    return p / len(labels)


class TestLazyConsts(unittest.TestCase):
    def test_const_like_is_lazy(self):
        x = Tensor(np.zeros((2, 3), np.float32))
        c = x.const_like(2.5)
        self.assertEqual(c.shape, (2, 3))
        self.assertEqual(c.dtype, dtypes.float32)
        np.testing.assert_array_equal(c.numpy(), np.full((2, 3), 2.5, np.float32))

    def test_empty_shape_and_dtype(self):
        e = Tensor.empty(4, 5, dtype=dtypes.int32)
        self.assertEqual(e.shape, (4, 5))
        self.assertEqual(e.dtype, dtypes.int32)

    def test_tril_triu(self):
        a = np.arange(12, dtype=np.float32).reshape(3, 4)
        np.testing.assert_array_equal(Tensor(a).tril().numpy(), np.tril(a))
        np.testing.assert_array_equal(Tensor(a).triu(1).numpy(), np.triu(a, 1))

    def test_backward_default_seed(self):
        x = Tensor(np.array([1.0, -2.0, 3.0], np.float32))
        (x * x).sum().backward()
        np.testing.assert_allclose(x.grad.numpy(), [2.0, -4.0, 6.0], rtol=1e-6)

    def test_sparse_crossentropy_value_and_grad(self):
        rng = np.random.default_rng(3)
        logits = rng.standard_normal((5, 7)).astype(np.float32)
        labels = rng.integers(0, 7, 5).astype(np.int32)
        x = Tensor(logits)
        loss = x.sparse_categorical_crossentropy(Tensor(labels))
        loss.backward()
        np.testing.assert_allclose(float(loss.numpy()), _ce_ref(logits, labels), rtol=1e-5)
        np.testing.assert_allclose(x.grad.numpy(), _ce_grad_ref(logits, labels), atol=1e-6)

    def test_sparse_crossentropy_jit_reads_new_labels(self):
        rng = np.random.default_rng(4)
        logits = rng.standard_normal((4, 6)).astype(np.float32)
        x = Tensor(logits)
        y_buf = Tensor(np.zeros(4, np.int32))
        y_buf.realize()

        @TinyJit
        def step():
            return x.sparse_categorical_crossentropy(y_buf).realize()

        for _ in range(4):
            labels = rng.integers(0, 6, 4).astype(np.int32)
            y_buf.assign(Tensor(labels))
            y_buf.realize()
            np.testing.assert_allclose(float(step().numpy()), _ce_ref(logits, labels), rtol=1e-5)


if __name__ == "__main__":
    unittest.main()
