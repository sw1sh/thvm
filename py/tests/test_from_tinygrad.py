"""Cross-validation: tinygrad builds the lazy graph, thvm executes it.

For each case we build a tinygrad Tensor, rebuild its UOp graph in thvm
via `from_tinygrad`, and assert the thvm result matches tinygrad's own
`.numpy()` within 1e-4.

Run:
  cd <repo> && PYTHONPATH=py:/Users/swish/src/tinygrad DEV=cpu \\
    python3 -m pytest py/tests/test_from_tinygrad.py -x -q
"""
import os

import numpy as np

# Force the CPU backend (this is a macOS build; cpu is the safe path).
os.environ.setdefault("DEV", "cpu")

import pytest

tinygrad = pytest.importorskip("tinygrad")
from tinygrad import Tensor as TgTensor

from thvm.from_tinygrad import from_tinygrad


def _check(tg_tensor, atol=1e-4):
    expected = tg_tensor.numpy()
    got = from_tinygrad(tg_tensor).numpy()
    assert got.shape == expected.shape, f"shape {got.shape} != {expected.shape}"
    np.testing.assert_allclose(got, expected, atol=atol, rtol=1e-4)
    return got


def test_add_sum():
    t = (TgTensor([1., 2, 3]) + TgTensor([4., 5, 6])).sum()
    _check(t)


def test_matmul():
    np.random.seed(0)
    a = np.random.randn(4, 8).astype(np.float32)
    b = np.random.randn(8, 3).astype(np.float32)
    t = TgTensor(a) @ TgTensor(b)
    _check(t)


def test_relu_sum_axis1():
    np.random.seed(1)
    x = np.random.randn(2, 5).astype(np.float32)
    t = TgTensor(x).relu().sum(axis=1)
    _check(t)


def test_elementwise_mul_add():
    np.random.seed(2)
    a = np.random.randn(3, 4).astype(np.float32)
    b = np.random.randn(3, 4).astype(np.float32)
    t = TgTensor(a) * TgTensor(b) + TgTensor(a)
    _check(t)


def test_reduce_max_axis0():
    np.random.seed(3)
    x = np.random.randn(4, 6).astype(np.float32)
    t = TgTensor(x).max(axis=0)
    _check(t)


def test_movement_reshape_permute():
    np.random.seed(4)
    x = np.random.randn(2, 3, 4).astype(np.float32)
    t = TgTensor(x).permute(2, 0, 1).reshape(4, 6).sum(axis=1)
    _check(t)


def test_exp_log_mean():
    np.random.seed(5)
    x = np.abs(np.random.randn(3, 5).astype(np.float32)) + 0.5
    t = TgTensor(x).log().exp().mean(axis=1)
    _check(t)


if __name__ == "__main__":
    import sys
    sys.exit(pytest.main([__file__, "-x", "-q"]))
