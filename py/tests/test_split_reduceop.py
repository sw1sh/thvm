"""A large reduce takes tinygrad's split_reduceop (schedule/prepare.py:110-132).

A SUM or MAX of at least 32768 elements into at most 2**19 outputs becomes two
reduces: the first sums contiguous chunks of one reduced, unexpanded axis into
up to 256 partials per output, the second reduces the partials.  The split
reaches the first reduce again, so a 4096x2048 sum takes three kernels.  thvm
splits on the CPU backend (src/schedule/split_reduceop.c); a GEMM or GEMV,
whose operands broadcast, keeps its matmul path.

The cases cover a full and a partial sum, a max, a keepdim sum, an int32 sum,
a reduce over a permuted, a broadcast and a padded view, an unbroadcast
product, a GEMV, and the gradients of a sum and of a max, against float64
numpy, on both CPU routes (the C JIT and the UOp walker) in child processes.
test_schedule_parity.py checks the kernel counts against tinygrad.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_split_reduceop.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor

R = np.random.default_rng(0)


def _f(*s):
    return R.standard_normal(s).astype(np.float32)


X = _f(256, 256)
P = _f(8, 65536)
C = _f(64, 16, 1024)
W = _f(65536)
I = R.integers(-1000, 1000, (256, 256)).astype(np.int32)


def _close(got, want, rtol=2e-5, atol=1e-3):
    np.testing.assert_allclose(np.asarray(got, np.float64), want, rtol=rtol, atol=atol)


def case_full_sum():
    _close(Tensor(X).sum().numpy(), X.astype(np.float64).sum())


def case_full_max():
    np.testing.assert_array_equal(Tensor(X).max().numpy(), X.max())


def case_partial_sum():
    _close(Tensor(P).sum(axis=1).numpy(), P.astype(np.float64).sum(axis=1))


def case_keepdim_sum():
    got = Tensor(P).sum(axis=1, keepdim=True).numpy()
    assert got.shape == (8, 1), got.shape
    _close(got, P.astype(np.float64).sum(axis=1, keepdims=True))


def case_two_axes():
    _close(Tensor(C).sum(axis=(0, 2)).numpy(), C.astype(np.float64).sum(axis=(0, 2)))


def case_int_sum():
    got = Tensor(I).sum().numpy()
    assert got == I.astype(np.int64).sum(), (got, I.astype(np.int64).sum())


def case_permuted():
    _close(Tensor(P).T.sum(axis=0).numpy(), P.astype(np.float64).sum(axis=1))


def case_broadcast():
    col = X[:, :1]
    _close(Tensor(col).expand(256, 256).sum().numpy(), 256 * col.astype(np.float64).sum())


def case_padded():
    got = Tensor(X).pad(((0, 0), (1, 3))).sum().numpy()
    _close(got, X.astype(np.float64).sum())


def case_product():
    got = (Tensor(P[0]) * Tensor(P[1])).sum().numpy()
    _close(got, (P[0].astype(np.float64) * P[1]).sum())


def case_gemv():
    _close((Tensor(P) @ Tensor(W)).numpy(), P.astype(np.float64) @ W, atol=1e-2)


def case_sum_grad():
    x, w = Tensor(P[0]), Tensor(P[1])
    (x * w).sum().backward()
    _close(x.grad.numpy(), P[1].astype(np.float64), atol=1e-6)


def case_max_grad():
    x = Tensor(X)
    x.max().backward()
    want = (X == X.max()).astype(np.float64)
    _close(x.grad.numpy(), want / want.sum(), atol=1e-6)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_split_reduceop"
    ROUTE_CASES = ROUTE_CASES


class TestSplitOff(unittest.TestCase):
    def test_same_values_unsplit(self):
        # SPLIT_REDUCEOP=0 keeps one reduce; the values agree with the split.
        out = helpers.run_child(
            "import test_split_reduceop as t\n"
            "for n, f in t.ROUTE_CASES:\n"
            "    f(); print('CASE', n, flush=True)\n",
            1800, DEV="cpu", SPLIT_REDUCEOP="0")
        lines = [ln for ln in out.stdout.splitlines() if ln.startswith("CASE ")]
        self.assertEqual(len(lines), len(ROUTE_CASES), f"child failed:\n{out.stdout}\n{out.stderr[-4000:]}")


if __name__ == "__main__":
    unittest.main()
