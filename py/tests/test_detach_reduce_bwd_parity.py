"""Backward parity for reduce-over-broadcast cotangents containing a DETACH.

These guard two distinct findings from the eager-vs-JIT boundary bisect
(RANK 4 / RANK 7 gap-discovery):

  1. DETACH must be a hard backward cut (tinygrad gradient.py:89 drops
     Ops.DETACH from the backward walk).  thvm's wnf eagerly unwraps DETACH
     to its forward child before the grad-cell dispatch, which used to make
     .detach() a backward no-op (gradient flowed through it).  Fixed in
     wnf/_.c (pin the grad cell's y to the DETACH term so interact_grad's
     UOP_DETACH case fires grad_zero_at) + interact/uop_grad.c (DETACH is a
     diff-sink in the prewalk fanin count).  test_detach_cut_* lock this in.

  2. OPEN BUG (size-1-axis nested-reduce root): a multi-axis
     reduce-over-broadcast in the backward pass whose cotangent body
     contains a NESTED reduce (the detached batch-mean) mis-computes when
     ONE of the multi-axis reduce's axes has extent 1 (it collapses to
     CONST(0) per ru_new_range and the live mean's adjoint vanishes --
     thvm returns EXACTLY the detached per-channel mean broadcast).
     PROVEN a thvm SCHEDULER bug (not the tape): scheduling tinygrad's own
     correct backward graph through thvm via from_tinygrad gives 0.519, not
     0.  PROVEN dropped-TERM (not roundoff): the wrong value == the channel
     mean, and N=2/4 cancel to ~3e-8 while N=1 (or any reduce with a
     mixed size-1 + size>1 axis set, e.g. shape (2,1,2,2)) leaks.  A
     `.contiguous()` barrier on the detached factor (its own BUFFERIZE
     boundary) fixes it (oracle).  test_detach_reduce_bcast_n1_multiaxis
     reproduces it and FAILS today.  Same family as
     project_multiaxis_reduce_over_expand_zero / the source-comment
     "nested-reduce-iter handler (task #21)" in bufferize_classify.c.  The
     fix is in the CPU walker's nested-reduce-iter path / the materialize
     size-1-collapse handling; NOT boundary classification (matmul-protect,
     softmax-unmark, reduce-chain-unmark, REDUCE_UNMARK_CAP, and the
     broadcast-body repair are ALL ruled out empirically).  Do NOT mask by
     forcing JIT.  This is DISTINCT from the structurally-zero
     reduction-order roundoff in test_composite_reduce_bwd_parity (and in
     test_batchnorm_block_bwd_n1 below) -- that one's true grad is 0 and
     thvm fails to cancel; THIS one drops a real nonzero adjoint term.
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.environ.get("THVM_PY", os.path.join(
    os.path.dirname(__file__), "..")))


def _have_tg():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


def _thvm_grad(build):
    import thvm
    x = build(thvm.Tensor, lambda t: t.requires_grad_(True),
              lambda t: t.detach())
    return np.asarray(x.grad.numpy(), dtype=np.float32).reshape(-1)


def _tg_grad(build):
    from tinygrad import Tensor
    x = build(Tensor, lambda t: t.requires_grad_(), lambda t: t.detach())
    return np.asarray(x.grad.numpy(), dtype=np.float32).reshape(-1)


@unittest.skipUnless(_have_tg(), "tinygrad not on path")
class TestDetachReduceBwdParity(unittest.TestCase):
    def _assert_parity(self, build, tol=1e-4):
        g_thvm = _thvm_grad(build)
        g_tg = _tg_grad(build)
        self.assertEqual(g_thvm.shape, g_tg.shape)
        self.assertLess(float(np.abs(g_thvm - g_tg).max()), tol,
                        f"thvm {g_thvm} vs tinygrad {g_tg}")

    # --- DETACH is a backward cut (these PASS with the wnf/uop_grad fix) ---
    def test_detach_cut_scalar_mean(self):
        # loss = ((x - mean) * mean.detach()).sum(); scalar mean over all.
        X = np.random.default_rng(0).standard_normal((4,)).astype(np.float32)

        def build(T, rg, detach):
            x = rg(T(X)); m = x.mean(); a = x - m
            d = detach(x.mean())
            (a * d).sum().backward()
            return x
        self._assert_parity(build)

    def test_detach_cut_over_reduce(self):
        # loss = (x * s.detach()).sum() where s = x.sum() (a REDUCE under the
        # detach).  grad must be s (the detached value), NOT 2*s.  Before the
        # wnf/uop_grad fix, the detached REDUCE leaked its gradient (clean HEAD
        # returned 2x): wnf unwrapped DETACH(REDUCE) to the live REDUCE for the
        # grad-cell dispatch, so the s-path propagated.  The simple x*x.detach()
        # was unaffected (detach's child is a bare TEN leaf), so this composite
        # case is the regression guard that actually moves.
        X = np.random.default_rng(2).standard_normal((3,)).astype(np.float32)

        def build(T, rg, detach):
            x = rg(T(X)); s = x.sum()
            (x * detach(s)).sum().backward()
            return x
        self._assert_parity(build)

    def test_detach_cut_single_bcast_axis(self):
        # 2D, reduce over axis 0 only (one broadcast axis) -- DETACH cut.
        X = np.random.default_rng(0).standard_normal((3, 2)).astype(np.float32)

        def build(T, rg, detach):
            x = rg(T(X)); m = x.mean(axis=0); a = x - m.reshape(1, 2)
            d = detach(x.mean(axis=0)).reshape(1, 2)
            (a * d).sum().backward()
            return x
        self._assert_parity(build)

    def test_detach_cut_multiaxis_n2(self):
        # 4D, reduce over (0,2,3), batch N=2 (no size-1 reduce axis).
        X = np.random.default_rng(0).standard_normal(
            (2, 3, 2, 2)).astype(np.float32)

        def build(T, rg, detach):
            x = rg(T(X)); m = x.mean(axis=(0, 2, 3)); a = x - m.reshape(1, 3, 1, 1)
            d = detach(x.mean(axis=(0, 2, 3))).reshape(1, 3, 1, 1)
            (a * d).sum().backward()
            return x
        self._assert_parity(build)

    # --- OPEN: N==1 reduce-over-broadcast with a nested detached reduce ---
    def test_detach_reduce_bcast_n1_multiaxis(self):
        # 4D, reduce over (0,2,3), batch N=1 -> size-1 leading reduce axis.
        # grad wrt x should be ~0 (loss is identically 0); thvm returns the
        # detached per-channel mean broadcast (the live-mean adjoint is lost).
        X = np.random.default_rng(0).standard_normal(
            (1, 2, 2, 2)).astype(np.float32)

        def build(T, rg, detach):
            x = rg(T(X)); m = x.mean(axis=(0, 2, 3)); a = x - m.reshape(1, 2, 1, 1)
            d = detach(x.mean(axis=(0, 2, 3))).reshape(1, 2, 1, 1)
            (a * d).sum().backward()
            return x
        self._assert_parity(build)

    def test_batchnorm_block_bwd_n1(self):
        # conv->relu->BatchNorm(train)->sum, N=1.  The conv-weight grad here is
        # STRUCTURALLY ZERO (fp64 finite-diff: |grad|.sum() ~1.8e-7, base loss
        # ~1e-14 -- sum(BN_train(.)) is a constant since each channel's
        # normalized values sum to 0).  This is NOT the dropped-live-mean-adjoint
        # bug above (that one returns EXACTLY the per-channel mean == sum(invstd)
        # scale, ~26000 for this shape; thvm returns 377, four orders of
        # magnitude smaller).  It is the SAME class as the three
        # test_composite_reduce_bwd_parity cases: a reduction-ORDER divergence
        # that fails to cancel to roundoff on a cancellation-to-zero, AND it is
        # invariant to N (thvm 377/648/1218 at N=1/2/4 vs tinygrad ~1e-5), so it
        # is not N==1-specific.  The assertion is absolute (true grad is 0).
        # Lives here so the detach/N=1 file documents the boundary between the
        # two distinct roots; the actual fix is the composite-reduce reduction-
        # order arc, not the size-1-axis nested-reduce arc.
        import thvm
        import thvm.nn as THN
        import tinygrad
        import tinygrad.nn as TGN
        rng = np.random.default_rng(0)
        Xn = rng.standard_normal((1, 3, 8, 8)).astype(np.float32)
        Wn = rng.standard_normal((4, 3, 3, 3)).astype(np.float32)

        x = thvm.Tensor(Xn); w = thvm.Tensor(Wn).requires_grad_(True)
        THN.BatchNorm(4)(x.conv2d(w).relu()).sum().backward()
        g_thvm = float(np.abs(w.grad.numpy()).sum())

        tinygrad.Tensor.training = True
        tx = tinygrad.Tensor(Xn); tw = tinygrad.Tensor(Wn, requires_grad=True)
        TGN.BatchNorm(4)(tx.conv2d(tw).relu()).sum().backward()
        g_tg = float(np.abs(tw.grad.numpy()).sum())
        tinygrad.Tensor.training = False

        # True grad is 0 (fp64-proven); thvm must cancel to tinygrad's roundoff
        # floor.  Currently FAILS (thvm 377 >> tinygrad 1e-5).
        self.assertLess(g_thvm, 100.0 * max(g_tg, 1e-4),
                        f"conv1 weight-grad (true=0): thvm {g_thvm} vs "
                        f"tinygrad {g_tg} -- reduction-order roundoff on a "
                        f"structurally-zero grad")

    # --- OPEN: maxpool MAX-grad /count nested reduce fused into conv-weight ---
    def test_relu_bn_maxpool_sum_bwd_n1(self):
        # N=1 relu -> BatchNorm(train) -> maxpool2d(2) -> sum, conv-weight grad.
        # The maxpool MAX vjp is (mask/count)*gy (gradient.py:11-14), where
        # count = mask.sum(window-axes) broadcast back over the window.  When
        # that nested SUM-reduce + its broadcast-back + the recip are FUSED
        # into the conv-weight backward chain (which itself reduces the N=1
        # batch axis), the walker's nested-reduce-iter handler
        # (uop_walk.c:806-811 cext==0 identity bail) lets count read 0 at some
        # fused positions -> recip(0)=inf -> mask*inf = NaN.  Without the
        # /count divide (clean HEAD) this was silently all-0.  fp64 truth and
        # tinygrad both give a finite grad (|g|.max ~1.57); thvm is NaN.
        import thvm
        import thvm.nn as THN
        import tinygrad
        import tinygrad.nn as TGN
        rng = np.random.default_rng(0)
        Xn = rng.standard_normal((1, 3, 8, 8)).astype(np.float32)
        Wn = rng.standard_normal((4, 3, 3, 3)).astype(np.float32)

        # Both sides run BN in TRAIN mode (batch statistics); without the
        # thvm training flag BN reads its init running_mean=0/var=1 and the
        # two sides normalize differently.
        thvm.Tensor.training = True
        x = thvm.Tensor(Xn); w = thvm.Tensor(Wn).requires_grad_(True)
        THN.BatchNorm(4)(x.conv2d(w).relu()).max_pool2d((2, 2)).sum().backward()
        g_thvm = w.grad.numpy()
        thvm.Tensor.training = False

        tinygrad.Tensor.training = True
        tx = tinygrad.Tensor(Xn); tw = tinygrad.Tensor(Wn, requires_grad=True)
        TGN.BatchNorm(4)(tx.conv2d(tw).relu()).max_pool2d((2, 2)).sum() \
            .backward()
        g_tg = tw.grad.numpy()
        tinygrad.Tensor.training = False

        self.assertFalse(bool(np.isnan(g_thvm).any()),
                         "relu->BN->maxpool->sum conv-weight grad is NaN "
                         "(maxpool /count nested reduce fused into the N=1 "
                         "conv-weight reduce)")
        self.assertLess(float(np.abs(g_thvm - g_tg).max()), 1e-3,
                        f"thvm {g_thvm.reshape(-1)[:4]} vs "
                        f"tinygrad {g_tg.reshape(-1)[:4]}")

    def test_relu_maxpool_sum_bwd_n1(self):
        # Minimal isolation of the maxpool /count fusion NaN: N=1 relu ->
        # maxpool2d(2) -> sum, conv-weight grad (no normalization layer).  This
        # NaNs too (sparse: some grad entries finite ~2.6, others NaN), proving
        # the root is the maxpool /count nested-reduce fusion itself, NOT a
        # BatchNorm/GroupNorm interaction.  N=2 of the same graph is finite, so
        # the trigger is the size-1 batch axis in the fused conv-weight reduce.
        import thvm
        import tinygrad
        rng = np.random.default_rng(0)
        Xn = rng.standard_normal((1, 3, 8, 8)).astype(np.float32)
        Wn = rng.standard_normal((4, 3, 3, 3)).astype(np.float32)

        x = thvm.Tensor(Xn); w = thvm.Tensor(Wn).requires_grad_(True)
        x.conv2d(w).relu().max_pool2d((2, 2)).sum().backward()
        g_thvm = w.grad.numpy()

        tx = tinygrad.Tensor(Xn); tw = tinygrad.Tensor(Wn, requires_grad=True)
        tx.conv2d(tw).relu().max_pool2d((2, 2)).sum().backward()
        g_tg = tw.grad.numpy()

        self.assertFalse(bool(np.isnan(g_thvm).any()),
                         "relu->maxpool->sum conv-weight grad is NaN")
        self.assertLess(float(np.abs(g_thvm - g_tg).max()), 1e-3,
                        f"thvm {g_thvm.reshape(-1)[:4]} vs "
                        f"tinygrad {g_tg.reshape(-1)[:4]}")

    def test_relu_groupnorm_maxpool_sum_bwd_n2(self):
        # N=2 relu -> GroupNorm(2) -> maxpool2d(2) -> sum, conv-weight grad.
        # Same maxpool /count fusion NaN, but with a per-group mean/var reduce
        # ahead of the pool (groupnorm reduces the per-group channel+spatial
        # axes).  fp64 truth and tinygrad both give a finite grad (|g|.max
        # ~3.78); thvm is NaN.  GroupNorm is built from primitives because
        # thvm.nn has no GroupNorm class.
        import thvm
        import tinygrad
        rng = np.random.default_rng(0)
        Xn = rng.standard_normal((2, 3, 8, 8)).astype(np.float32)
        Wn = rng.standard_normal((4, 3, 3, 3)).astype(np.float32)
        G = 2

        def gn(y, T):
            n, c, h, ww = y.shape
            yg = y.reshape(n, T, c // T, h, ww)
            mu = yg.mean(axis=(2, 3, 4), keepdim=True)
            var = ((yg - mu) * (yg - mu)).mean(axis=(2, 3, 4), keepdim=True)
            yn = (yg - mu) * ((var + 1e-5).rsqrt())
            return yn.reshape(n, c, h, ww)

        x = thvm.Tensor(Xn); w = thvm.Tensor(Wn).requires_grad_(True)
        gn(x.conv2d(w).relu(), G).max_pool2d((2, 2)).sum().backward()
        g_thvm = w.grad.numpy()

        tx = tinygrad.Tensor(Xn); tw = tinygrad.Tensor(Wn, requires_grad=True)
        gn(tx.conv2d(tw).relu(), G).max_pool2d((2, 2)).sum().backward()
        g_tg = tw.grad.numpy()

        self.assertFalse(bool(np.isnan(g_thvm).any()),
                         "relu->groupnorm(2)->maxpool->sum conv-weight grad "
                         "is NaN")
        self.assertLess(float(np.abs(g_thvm - g_tg).max()), 1e-3,
                        f"thvm {g_thvm.reshape(-1)[:4]} vs "
                        f"tinygrad {g_tg.reshape(-1)[:4]}")


if __name__ == "__main__":
    unittest.main()
