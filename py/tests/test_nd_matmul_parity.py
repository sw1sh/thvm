"""N-D (batched) matmul + attention-block parity vs tinygrad (the spec).

GAP (gap-discovery rank 11): thvm's Tensor.__matmul__ hard-raises
NotImplementedError for ndim != 2 ("Phase 2A"), so batched matmuls
(transformers, multi-head attention: Q@K^T, attn@V) cannot run. tinygrad
supports N-D matmul via its dot() rule (mixin/__init__.py:322): broadcast the
leading batch dims, reshape/expand, elementwise-mul, then sum the trailing
contraction axis.

These tests pin thvm's (future) N-D matmul + attention forward+backward against
tinygrad. They are expected to FAIL at HEAD (NotImplementedError) and stay red
for whoever ports the N-D dot rule. The 2-D path is unchanged, so the existing
Linear/matmul stays byte-clean -- only ndim>2 / broadcast cases are new.

    make py && python -m pytest py/tests/test_nd_matmul_parity.py
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
PY = str(ROOT / "py")
sys.path.insert(0, os.environ.get("THVM_PY", PY))


def _have_tg():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


@unittest.skipUnless(_have_tg(), "tinygrad not on path")
class TestNDMatmulParity(unittest.TestCase):

    def _matmul_fwd_bwd(self, ashape, bshape):
        """Run a@b fwd + sum().backward() in thvm and tinygrad, return
        (out_th, out_tg, ga_th, ga_tg, gb_th, gb_tg)."""
        import thvm
        import tinygrad

        rng = np.random.default_rng(0)
        A = rng.standard_normal(ashape).astype(np.float32)
        B = rng.standard_normal(bshape).astype(np.float32)

        ah = thvm.Tensor(A.copy()).requires_grad_(True)
        bh = thvm.Tensor(B.copy()).requires_grad_(True)
        oh = ah @ bh
        oh.sum().backward()
        out_th = oh.numpy()
        ga_th = ah.grad.numpy().reshape(A.shape)
        gb_th = bh.grad.numpy().reshape(B.shape)

        at = tinygrad.Tensor(A.copy(), requires_grad=True)
        bt = tinygrad.Tensor(B.copy(), requires_grad=True)
        ot = at @ bt
        ot.sum().backward()
        out_tg = ot.numpy()
        ga_tg = at.grad.numpy()
        gb_tg = bt.grad.numpy()
        return out_th, out_tg, ga_th, ga_tg, gb_th, gb_tg

    def _assert_close(self, label, out_th, out_tg, ga_th, ga_tg, gb_th, gb_tg):
        self.assertEqual(out_th.shape, out_tg.shape,
                         f"{label} fwd shape {out_th.shape} vs {out_tg.shape}")
        for name, th, tg in (("fwd", out_th, out_tg),
                             ("ga", ga_th, ga_tg),
                             ("gb", gb_th, gb_tg)):
            rel = np.abs(th - tg).max() / (np.abs(tg).max() + 1e-6)
            self.assertLess(rel, 1e-4,
                            f"{label} {name} relmax={rel:.4g} "
                            f"(thvm absmax {np.abs(th).max():.6g} "
                            f"tg absmax {np.abs(tg).max():.6g})")

    def test_3d_batched_matmul_2_8_16_x_2_16_4(self):
        """(2,8,16) @ (2,16,4) -- a clean 3-D batched matmul (one batch
        dim, no broadcasting). fwd + both grads vs tinygrad."""
        self._assert_close("(2,8,16)@(2,16,4)",
                           *self._matmul_fwd_bwd((2, 8, 16), (2, 16, 4)))

    def test_4d_batched_matmul_4_2_8_16_x_4_2_16_4(self):
        """(4,2,8,16) @ (4,2,16,4) -- two batch dims, no broadcasting."""
        self._assert_close("(4,2,8,16)@(4,2,16,4)",
                           *self._matmul_fwd_bwd((4, 2, 8, 16), (4, 2, 16, 4)))

    def test_broadcast_2d_x_3d_batched_8_16_x_2_16_4(self):
        """(8,16) @ (2,16,4) -- the lhs has no batch dim; tinygrad's dot
        broadcasts it across the rhs's batch dim. Exercises the leading-
        dim broadcast path."""
        self._assert_close("(8,16)@(2,16,4)",
                           *self._matmul_fwd_bwd((8, 16), (2, 16, 4)))

    def test_broadcast_3d_x_2d_batched_2_8_16_x_16_4(self):
        """(2,8,16) @ (16,4) -- the rhs is a shared 2-D weight broadcast
        across the lhs batch dim (a batched Linear)."""
        self._assert_close("(2,8,16)@(16,4)",
                           *self._matmul_fwd_bwd((2, 8, 16), (16, 4)))


@unittest.skipUnless(_have_tg(), "tinygrad not on path")
class TestAttentionBlockParity(unittest.TestCase):
    """A real attention block: scores = Q@K^T / sqrt(d) -> softmax(rank-1
    fix, 6a313307) -> attn @ V. Both a 2-D single-head and a batched
    multi-head case, fwd + bwd, vs tinygrad. This is the integration the
    N-D matmul unblocks; it MUST compose with the rank-1 softmax fix."""

    def _attn_thvm(self, Q, K, V):
        import thvm
        d = Q.shape[-1]
        qh = thvm.Tensor(Q.copy()).requires_grad_(True)
        kh = thvm.Tensor(K.copy()).requires_grad_(True)
        vh = thvm.Tensor(V.copy()).requires_grad_(True)
        scores = (qh @ kh.transpose(-2, -1)) * (1.0 / float(np.sqrt(d)))
        attn = scores.softmax(axis=-1)
        out = attn @ vh
        out.sum().backward()
        return (out.numpy(), qh.grad.numpy().reshape(Q.shape),
                kh.grad.numpy().reshape(K.shape),
                vh.grad.numpy().reshape(V.shape))

    def _attn_tg(self, Q, K, V):
        import tinygrad
        d = Q.shape[-1]
        qt = tinygrad.Tensor(Q.copy(), requires_grad=True)
        kt = tinygrad.Tensor(K.copy(), requires_grad=True)
        vt = tinygrad.Tensor(V.copy(), requires_grad=True)
        scores = (qt @ kt.transpose(-2, -1)) * (1.0 / float(np.sqrt(d)))
        attn = scores.softmax(axis=-1)
        out = attn @ vt
        out.sum().backward()
        return (out.numpy(), qt.grad.numpy(), kt.grad.numpy(), vt.grad.numpy())

    def _check(self, label, Q, K, V):
        oh, gq_h, gk_h, gv_h = self._attn_thvm(Q, K, V)
        ot, gq_t, gk_t, gv_t = self._attn_tg(Q, K, V)
        self.assertEqual(oh.shape, ot.shape, f"{label} fwd shape")
        for name, th, tg in (("out", oh, ot), ("gQ", gq_h, gq_t),
                             ("gK", gk_h, gk_t), ("gV", gv_h, gv_t)):
            rel = np.abs(th - tg).max() / (np.abs(tg).max() + 1e-6)
            self.assertLess(rel, 1e-4,
                            f"{label} {name} relmax={rel:.4g}")

    def test_single_head_2d_attention(self):
        """2-D single-head attention: Q,K,V are (seq, d). All matmuls are
        2-D (uses the existing path) but validates the softmax-rank-1 fix
        composes end-to-end with the matmul-based block."""
        rng = np.random.default_rng(1)
        seq, d = 6, 8
        self._check("2d-attn",
                    rng.standard_normal((seq, d)).astype(np.float32),
                    rng.standard_normal((seq, d)).astype(np.float32),
                    rng.standard_normal((seq, d)).astype(np.float32))

    def test_batched_multihead_attention(self):
        """Batched multi-head attention: Q,K,V are (batch, heads, seq, d).
        Q@K^T and attn@V are 4-D batched matmuls -> requires N-D matmul."""
        rng = np.random.default_rng(2)
        B, Hd, seq, d = 2, 3, 5, 8
        self._check("mha",
                    rng.standard_normal((B, Hd, seq, d)).astype(np.float32),
                    rng.standard_normal((B, Hd, seq, d)).astype(np.float32),
                    rng.standard_normal((B, Hd, seq, d)).astype(np.float32))


if __name__ == "__main__":
    unittest.main()
