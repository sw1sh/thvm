"""Tensors of up to 16 axes, as tinygrad's 3-D conv, pool and fancy indexing build (MAX_DIM 16).

tinygrad's _pool for three spatial axes and its multi-tensor indexing reach 9 to 12 axes, past
thvm's former 8; MAX_DIM is 16, MAX_AXES 32 and the walker holds 32 ranges.  Tensor.repeat is
tinygrad's (mixin/movement.py:576-595): fewer repeats than axes fail its reshape.  Each case runs
against numpy on both CPU routes (the C JIT and the UOp walker) in child processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_high_rank.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor

R = np.random.default_rng(1)


def _close(got, want, rtol=1e-5):
    np.testing.assert_allclose(got, want, rtol=rtol, atol=1e-5)


def case_twelve_axes():
    a = R.standard_normal((2, 1, 2, 1, 2, 3, 1, 2, 1, 2, 1, 2)).astype(np.float32)
    t = Tensor(a)
    _close(t.sum((1, 4, 7, 10)).numpy(), a.sum((1, 4, 7, 10)))
    perm = tuple(reversed(range(12)))
    _close((t.permute(perm) * 2).numpy(), a.transpose(perm) * 2)


def case_conv3d():
    x = R.standard_normal((1, 2, 5, 5, 5)).astype(np.float32)
    w = R.standard_normal((3, 2, 3, 3, 3)).astype(np.float32)
    want = np.zeros((1, 3, 3, 3, 3), np.float32)
    for o in range(3):
        for i, j, k in np.ndindex(3, 3, 3):
            want[0, o, i, j, k] = (x[0, :, i:i + 3, j:j + 3, k:k + 3] * w[o]).sum()
    _close(Tensor(x).conv2d(Tensor(w)).numpy(), want, rtol=1e-4)


def case_avg_pool3d():
    x = R.standard_normal((1, 2, 4, 4, 4)).astype(np.float32)
    want = x.reshape(1, 2, 2, 2, 2, 2, 2, 2).mean((3, 5, 7))
    _close(Tensor(x).avg_pool2d(kernel_size=(2, 2, 2)).numpy(), want)


def case_repeat():
    a = R.standard_normal((4, 6, 3)).astype(np.float32)
    _close(Tensor(a).repeat(2, 4, 3, 2).numpy(), np.tile(a, (2, 4, 3, 2)))
    try:
        Tensor(a).repeat((2, 4))
    except ValueError:
        return
    raise AssertionError("repeat with fewer repeats than axes did not raise")


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_high_rank"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
