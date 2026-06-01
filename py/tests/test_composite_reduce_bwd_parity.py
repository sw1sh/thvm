"""Composite-reduce BACKWARD parity vs tinygrad (the spec).

WHAT IS / ISN'T A BUG (established by bisection -- see the docstring on each
test). thvm's scheduler is NUMERICALLY CORRECT on every genuinely-nonzero
composite-reduce backward (conv->relu->BN(train)->maxpool->sum matches tinygrad
to 1e-5; conv->relu->BN->sigmoid->sum matches to 1e-5). The ONE remaining
divergence is a floating-point REDUCTION-ORDER difference on a STRUCTURALLY-ZERO
gradient: `sum(BN_train(.))` is a constant (the per-channel centered sum is 0),
so its gradient is exactly 0, and both backends return only cancellation
roundoff. thvm's reduce-kernel ordering amplifies that invstd-scaled roundoff
~40-65x more than tinygrad's single-reduce kernels.

The structural-zero claim is fp64-PROVEN, not asserted: a numpy fp64
finite-difference of sum(BN_train(relu(conv(x,w)))) wrt w gives |grad|.sum()
~3e-8 (base loss ~1e-14), and the "dropped-adjoint" alternative (if thvm were
losing the live mean/var adjoint TERM) would give |grad|.sum() ~= sum(invstd)
which is ~25000 for the (32,8,10,10) shape -- four orders of magnitude above
thvm's 0.32-0.58.  So thvm is NOT dropping a term here; it is failing to cancel
to roundoff.  The assertion is absolute-magnitude (the true grad is 0, so thvm
must come within tinygrad's roundoff floor), framed as `rel` only because
tinygrad's ~1e-5 IS that floor.  Contrast test_detach_reduce_bwd_parity's N==1
case, which IS a dropped-term bug (thvm returns exactly the per-channel mean,
== sum(invstd)-scale -- a different, fixable root).

Proof the bug is thvm's SCHEDULER (not the autograd tape): test (a)+(c) build
the gradient graph in tinygrad (correct by construction) then schedule it
through thvm via from_tinygrad. tinygrad's own .numpy() is the reference; they
diverge => thvm mis-schedules a CORRECT backward graph into a less
numerically-stable kernel set.

These are expected to FAIL at HEAD. They are normal regression tests left red
for whoever tightens the composite-reduce-backward reduction order to match
tinygrad's single-reduce-per-kernel shape (do not skip / quarantine). Run:

    make py && python -m pytest py/tests/test_composite_reduce_bwd_parity.py
"""
import os
import pathlib
import subprocess
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
PY = str(ROOT / "py")


def _have_tg():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


@unittest.skipUnless(_have_tg(), "tinygrad not on path")
class TestCompositeReduceBwdParity(unittest.TestCase):

    def test_conv_relu_bn_train_sum_conv_weight_grad_vs_tinygrad(self):
        """(a) conv->relu->BN(train)->sum, conv-weight grad, scheduled by thvm
        from tinygrad's own (correct) backward graph, must match tinygrad's
        value. The true grad is structurally 0 (sum(BN) is constant); both
        backends return invstd-scaled cancellation roundoff, but thvm's reduce
        ordering amplifies it ~50x. Currently FAILS (rel ~55)."""
        import tinygrad
        import tinygrad.nn as TGN
        from thvm.from_tinygrad import from_tinygrad

        rng = np.random.default_rng(0)
        X = rng.standard_normal((32, 1, 12, 12)).astype(np.float32)
        CW = rng.standard_normal((8, 1, 3, 3)).astype(np.float32)

        tinygrad.Tensor.training = True
        w = tinygrad.Tensor(CW, requires_grad=True)
        y = tinygrad.Tensor(X).conv2d(w).relu()
        y = TGN.BatchNorm(8)(y)
        loss = y.sum()
        loss.backward()
        tinygrad.Tensor.training = False

        g_tg = np.abs(w.grad.numpy()).sum()
        g_th = np.abs(from_tinygrad(w.grad).numpy()).sum()
        rel = abs(g_th - g_tg) / (abs(g_tg) + 1e-12)
        self.assertLess(rel, 0.02,
                        f"conv->relu->BN(train)->sum conv-weight |grad| "
                        f"thvm={g_th:.6g} vs tinygrad={g_tg:.6g} rel={rel:.4g} "
                        f"(reduction-order roundoff on a structurally-0 grad)")

    def test_bn_train_grad_per_element_thvm_native_vs_tinygrad(self):
        """(b) thvm's OWN autograd+scheduler vs tinygrad's, per element, on
        conv->relu->BN(train)->sum. Distinct from (a)/(c): no from_tinygrad
        import, so this pins thvm's end-to-end native path (its gradient.py +
        scheduler) against tinygrad's. The per-element absmax diverges ~65x
        (thvm absmax ~0.023 vs tinygrad ~0.00035 on the structurally-0 grad).

        NOTE: a default-vs-faithful seed comparison does NOT fail here -- the
        per-tensor eager realize of a single grad picks the same reduce
        structure under both seeds (verified: the seed knob does not move this
        value). The RANK-7 default-vs-faithful convergence gap is a multi-step
        TRAINING phenomenon, not a single-grad-tensor seed difference."""
        import thvm
        import thvm.nn as THN
        import tinygrad
        import tinygrad.nn as TGN

        rng = np.random.default_rng(0)
        X = rng.standard_normal((32, 1, 12, 12)).astype(np.float32)
        CW = rng.standard_normal((8, 1, 3, 3)).astype(np.float32)

        tinygrad.Tensor.training = True
        wt = tinygrad.Tensor(CW, requires_grad=True)
        TGN.BatchNorm(8)(tinygrad.Tensor(X).conv2d(wt).relu()).sum().backward()
        g_tg = wt.grad.numpy()
        tinygrad.Tensor.training = False

        thvm.Tensor.training = True
        wh = thvm.Tensor(CW).requires_grad_(True)
        THN.BatchNorm(8)(thvm.Tensor(X).conv2d(wh).relu()).sum().backward()
        g_th = wh.grad.numpy().reshape(g_tg.shape)
        thvm.Tensor.training = False

        rel = np.abs(g_th - g_tg).max() / (np.abs(g_tg).max() + 1e-12)
        self.assertLess(rel, 0.05,
                        f"conv->relu->BN(train)->sum per-element grad: thvm "
                        f"absmax={np.abs(g_th).max():.6g} vs tinygrad "
                        f"absmax={np.abs(g_tg).max():.6g} max-rel-diff={rel:.4g}")

    def test_bn_train_only_sum_grad_vs_tinygrad(self):
        """(c) Minimal isolation: x->BN(train)->sum, scheduled by thvm from
        tinygrad's correct backward graph, must match tinygrad. Removes conv +
        relu, leaving the bare BatchNorm-train statistics backward (the SUM
        reduces over (0,2,3) that compute mean/var grad). The true grad is
        structurally 0; thvm's reduce ordering amplifies the cancellation
        roundoff ~100x. Currently FAILS."""
        import tinygrad
        import tinygrad.nn as TGN
        from thvm.from_tinygrad import from_tinygrad

        rng = np.random.default_rng(0)
        W = rng.standard_normal((32, 8, 10, 10)).astype(np.float32)

        tinygrad.Tensor.training = True
        w = tinygrad.Tensor(W, requires_grad=True)
        loss = TGN.BatchNorm(8)(w).sum()
        loss.backward()
        tinygrad.Tensor.training = False

        g_tg = np.abs(w.grad.numpy()).sum()
        g_th = np.abs(from_tinygrad(w.grad).numpy()).sum()
        rel = abs(g_th - g_tg) / (abs(g_tg) + 1e-12)
        self.assertLess(rel, 0.02,
                        f"x->BN(train)->sum |grad| thvm={g_th:.6g} vs "
                        f"tinygrad={g_tg:.6g} rel={rel:.4g}")


if __name__ == "__main__":
    unittest.main()
