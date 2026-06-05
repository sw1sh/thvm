"""Keepdim-reduce broadcast into a binary op on SHARED-EXTENT shapes.

A keepdim reduce (`x.max(axis,keepdim=True)`, `x.sum(...)`, `x.mean(...)`)
realized into a 1-axis BUFFERIZE and then broadcast back into a binary op
(softmax `f - s.max(kd)`; layer-norm `t - mean`) must read the producer's row
stride POSITIONALLY. The materialize.c BUFFERIZE-inline rewriter used to
disambiguate the closed_range by EXTENT equality, so whenever two consumer axes
shared an extent (e.g. (16,16), (10,10), the MNIST cross-entropy when
BS==n_classes) the inline bailed and the keepdim-reduce value silently collapsed
to 0 -> the whole softmax / layer-norm / log_softmax output was garbage
(rowsum_err==1.0). The fix binds closed_range[i] -> consumer in_rng at the
producer's positional realized axis i (tinygrad/schedule/indexing.py:66,78), so
shared extents are unambiguous.

Ground truth is numpy; distinct-extent controls guard against over-fixing the
common path. All run under THVM_PY on sys.path.

    make py && python -m pytest py/tests/test_shared_extent_broadcast.py
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

from thvm import Tensor  # noqa: E402

try:
    import tinygrad  # noqa: F401
    from tinygrad import Tensor as TgTensor  # noqa: E402

    HAVE_TINYGRAD = True
except Exception:  # pragma: no cover - oracle optional
    HAVE_TINYGRAD = False

TOL = 1e-4

# rank-3+ backward shapes the keepdim-reduce broadcast must handle.  The
# regressing set: 2+ OUTER (non-reduced) axes on the backward MUL consumer.
# (3,3,3)/(7,7,7) are also the pre-existing shared-extent rank-3+ cases.
BWD_SHAPES = [(3, 4, 5), (2, 3, 4), (7, 7, 7), (2, 3, 4, 5), (3, 3, 3)]
# Finite-difference tolerance: looser than the forward bit-exact TOL because
# central differences on f32 carry O(eps^2) + roundoff error.
FD_REL_TOL = 2e-2


def softmax_ref(a, axis=-1):
    m = a.max(axis=axis, keepdims=True)
    e = np.exp(a - m)
    return e / e.sum(axis=axis, keepdims=True)


def logsoftmax_ref(a, axis=-1):
    m = a.max(axis=axis, keepdims=True)
    s = a - m
    return s - np.log(np.exp(s).sum(axis=axis, keepdims=True))


class TestSharedExtentBroadcast(unittest.TestCase):
    def setUp(self):
        self.rng = np.random.default_rng(0)

    def _rand(self, shp):
        return self.rng.standard_normal(shp).astype(np.float32)

    def _assert_close(self, got, ref, msg):
        err = float(np.max(np.abs(got - ref)))
        self.assertLessEqual(err, TOL, f"{msg}: maxerr={err:.3e}")

    # --- softmax: shared-extent shapes (the headline bug) ---
    def test_softmax_shared_extent(self):
        for shp in [(16, 16), (10, 10), (4, 16, 16, 8), (4, 8, 6, 8), (3, 3, 5, 7),
                    (128, 128)]:
            a = self._rand(shp)
            o = Tensor(a).softmax().numpy()
            self._assert_close(o, softmax_ref(a), f"softmax {shp}")
            rs = float(np.max(np.abs(o.sum(-1) - 1.0)))
            self.assertLessEqual(rs, TOL, f"softmax {shp} rowsum_err={rs:.3e}")

    # --- softmax: distinct-extent control (must stay correct) ---
    def test_softmax_distinct_extent(self):
        for shp in [(16, 32), (16, 17), (128, 10), (10, 11)]:
            a = self._rand(shp)
            o = Tensor(a).softmax().numpy()
            self._assert_close(o, softmax_ref(a), f"softmax {shp}")

    # --- log_softmax (the MNIST CE-loss-when-BS==n_classes corruption) ---
    def test_log_softmax_shared_extent(self):
        for shp in [(10, 10), (16, 16)]:
            a = self._rand(shp)
            o = Tensor(a).log_softmax().numpy()
            self._assert_close(o, logsoftmax_ref(a), f"log_softmax {shp}")

    # --- minimal isolate: keepdim-max broadcast subtract, no exp/recip ---
    def test_reduce_keepdim_subtract(self):
        for shp in [(4, 4), (8, 8), (16, 16)]:
            a = self._rand(shp)
            o = (Tensor(a) - Tensor(a).max(axis=-1, keepdim=True)).numpy()
            self._assert_close(o, a - a.max(-1, keepdims=True), f"sub {shp}")

    def test_reduce_keepdim_subtract_distinct(self):
        a = self._rand((4, 5))
        o = (Tensor(a) - Tensor(a).max(axis=-1, keepdim=True)).numpy()
        self._assert_close(o, a - a.max(-1, keepdims=True), "sub (4,5)")

    # --- mul-keepdim-sum on the OTHER combiner (proves MUL path stays ok) ---
    def test_mul_keepdim_sum(self):
        a = np.arange(16).reshape(4, 4).astype(np.float32)
        o = (Tensor(a) * Tensor(a).sum(axis=-1, keepdim=True)).numpy()
        self._assert_close(o, a * a.sum(-1, keepdims=True), "mul-sum (4,4)")

    # --- layer-norm on a square shape ---
    def test_layernorm_square(self):
        for shp in [(16, 16), (10, 10)]:
            a = self._rand(shp)
            t = Tensor(a)
            mean = t.mean(axis=-1, keepdim=True)
            xc = t - mean
            var = (xc * xc).mean(axis=-1, keepdim=True)
            out = (xc * (var + 1e-5).rsqrt()).numpy()
            m = a.mean(-1, keepdims=True)
            xcr = a - m
            vr = (xcr * xcr).mean(-1, keepdims=True)
            ref = xcr / np.sqrt(vr + 1e-5)
            self._assert_close(out, ref, f"layernorm {shp}")


def _fd_grad(f, a, eps=1e-3):
    """Central-difference gradient of scalar f wrt every element of a."""
    g = np.zeros_like(a)
    it = np.nditer(a, flags=["multi_index"])
    while not it.finished:
        idx = it.multi_index
        old = a[idx]
        a[idx] = old + eps
        fp = f(a)
        a[idx] = old - eps
        fm = f(a)
        a[idx] = old
        g[idx] = (fp - fm) / (2 * eps)
        it.iternext()
    return g


def _rel(got, ref):
    return float(np.max(np.abs(got - ref)) / (np.max(np.abs(ref)) + 1e-8))


class TestRank3BackwardBroadcast(unittest.TestCase):
    """rank-3+ keepdim-reduce broadcast into a binary op, BACKWARD.

    The headline regression: with 2+ OUTER (non-reduced) axes on the backward
    MUL consumer (rank>=3), the keepdim-reduce broadcast value was bound at the
    wrong consumer iteration axis, producing a wrong gradient.  Forward of every
    one of these shapes is bit-exact; this class isolates the backward defect.

    Ground truth #1: central finite differences (model-agnostic).
    Ground truth #2: tinygrad CPU autodiff (the spec), when importable.
    """

    def setUp(self):
        self.rng = np.random.default_rng(0)

    def _rand(self, shp):
        return self.rng.standard_normal(shp).astype(np.float32)

    # --- (x * x.sum(-1,kd).rsqrt()).sum() backward (the minimal isolate) ---
    @staticmethod
    def _rms_loss_np(a):
        return float((a * (a * a).sum(-1, keepdims=True) ** -0.5).sum())

    @staticmethod
    def _rms_loss_thvm(t):
        return (t * (t * t).sum(axis=-1, keepdim=True).rsqrt()).sum()

    @staticmethod
    def _rms_loss_tg(t):
        return (t * (t * t).sum(axis=-1, keepdim=True).rsqrt()).sum()

    def test_rmsnorm_sum_backward_vs_fd(self):
        for shp in BWD_SHAPES:
            a = self._rand(shp)
            t = Tensor(a.copy())
            self._rms_loss_thvm(t).backward()
            g = t.grad.numpy()
            g_fd = _fd_grad(self._rms_loss_np, a.copy())
            self.assertLessEqual(
                _rel(g, g_fd), FD_REL_TOL, f"rms-bwd {shp} vs fd rel={_rel(g, g_fd):.4e}")

    @unittest.skipUnless(HAVE_TINYGRAD, "tinygrad oracle unavailable")
    def test_rmsnorm_sum_backward_vs_tinygrad(self):
        for shp in BWD_SHAPES:
            a = self._rand(shp)
            t = Tensor(a.copy())
            self._rms_loss_thvm(t).backward()
            g = t.grad.numpy()
            tg = TgTensor(a.copy())
            self._rms_loss_tg(tg).backward()
            g_ref = tg.grad.numpy()
            self.assertLessEqual(
                _rel(g, g_ref), TOL, f"rms-bwd {shp} vs tinygrad rel={_rel(g, g_ref):.4e}")

    # --- softmax backward (weighted-sum scalar loss for a nontrivial grad) ---
    @staticmethod
    def _softmax_np(a):
        m = a.max(-1, keepdims=True)
        e = np.exp(a - m)
        return e / e.sum(-1, keepdims=True)

    def test_softmax_backward_vs_fd(self):
        for shp in BWD_SHAPES:
            a = self._rand(shp)
            w = self._rand(shp)
            t = Tensor(a.copy())
            (t.softmax() * Tensor(w)).sum().backward()
            g = t.grad.numpy()
            g_fd = _fd_grad(lambda x: float((self._softmax_np(x) * w).sum()), a.copy())
            self.assertLessEqual(
                _rel(g, g_fd), FD_REL_TOL, f"softmax-bwd {shp} vs fd rel={_rel(g, g_fd):.4e}")

    @unittest.skipUnless(HAVE_TINYGRAD, "tinygrad oracle unavailable")
    def test_softmax_backward_vs_tinygrad(self):
        for shp in BWD_SHAPES:
            a = self._rand(shp)
            w = self._rand(shp)
            t = Tensor(a.copy())
            (t.softmax() * Tensor(w)).sum().backward()
            g = t.grad.numpy()
            tg = TgTensor(a.copy())
            (tg.softmax() * TgTensor(w)).sum().backward()
            g_ref = tg.grad.numpy()
            self.assertLessEqual(
                _rel(g, g_ref), TOL, f"softmax-bwd {shp} vs tinygrad rel={_rel(g, g_ref):.4e}")

    # --- layer-norm backward (mean + var, both keepdim reduces) ---
    @staticmethod
    def _ln_np(a, w):
        m = a.mean(-1, keepdims=True)
        xc = a - m
        v = (xc * xc).mean(-1, keepdims=True)
        return float((xc * np.sqrt(1.0 / (v + 1e-5)) * w).sum())

    @staticmethod
    def _ln_thvm(t, w):
        mean = t.mean(axis=-1, keepdim=True)
        xc = t - mean
        var = (xc * xc).mean(axis=-1, keepdim=True)
        return (xc * (var + 1e-5).rsqrt() * Tensor(w)).sum()

    @staticmethod
    def _ln_tg(t, w):
        mean = t.mean(axis=-1, keepdim=True)
        xc = t - mean
        var = (xc * xc).mean(axis=-1, keepdim=True)
        return (xc * (var + 1e-5).rsqrt() * TgTensor(w)).sum()

    def test_layernorm_backward_vs_fd(self):
        for shp in BWD_SHAPES:
            a = self._rand(shp)
            w = self._rand(shp)
            t = Tensor(a.copy())
            self._ln_thvm(t, w).backward()
            g = t.grad.numpy()
            g_fd = _fd_grad(lambda x: self._ln_np(x, w), a.copy())
            self.assertLessEqual(
                _rel(g, g_fd), FD_REL_TOL, f"layernorm-bwd {shp} vs fd rel={_rel(g, g_fd):.4e}")

    @unittest.skipUnless(HAVE_TINYGRAD, "tinygrad oracle unavailable")
    def test_layernorm_backward_vs_tinygrad(self):
        for shp in BWD_SHAPES:
            a = self._rand(shp)
            w = self._rand(shp)
            t = Tensor(a.copy())
            self._ln_thvm(t, w).backward()
            g = t.grad.numpy()
            tg = TgTensor(a.copy())
            self._ln_tg(tg, w).backward()
            g_ref = tg.grad.numpy()
            self.assertLessEqual(
                _rel(g, g_ref), TOL, f"layernorm-bwd {shp} vs tinygrad rel={_rel(g, g_ref):.4e}")


if __name__ == "__main__":
    unittest.main()
