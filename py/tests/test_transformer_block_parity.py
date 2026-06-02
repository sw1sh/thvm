"""End-to-end transformer-block parity vs tinygrad (the spec).

Validates the integration that this session's three fixes enabled:
  * 6a313307 -- equal-extent softmax/attention (positional-axis fix)
  * 19badcc0 -- N-D batched matmul + grouped conv (Q@K^T and attn@V)
  * f1919062 -- Muon optimizer (Newton-Schulz)

A minimal transformer encoder block is built TWICE -- once from thvm's
Tensor + nn primitives, once from tinygrad -- with byte-identical weights
copied via .numpy(), and forward/backward are compared.

Block (batch=2, seq=8, d_model=16, n_heads=2, d_ff=32):
  multi-head self-attention (fused QKV proj -> head-split -> Q@K^T/sqrt(d)
  -> softmax -> @V -> output proj) -> residual -> LayerNorm -> 2-layer MLP
  (Linear-relu-Linear) -> residual -> LayerNorm.

thvm.nn supplies Linear + LayerNorm. Multi-head attention has no nn layer,
so it is built from Tensor ops (reshape/getitem/permute for head-splitting,
N-D matmul for Q@K^T and attn@V, softmax, transpose). NO primitive is
missing -- every op the block needs exists.

VALIDATION RESULT (this is a *validation* workflow; bugs are reported, not
fixed here):

  A NEW integration bug is pinned by test_forward_parity /
  test_backward_parity. It is *not* in any single committed fix -- the
  standalone attention test (test_nd_matmul_parity) passes. The bug only
  triggers when a batched matmul consumes an *un-realized SHRINK/getitem
  view of another matmul's output*:

      linear (matmul) -> reshape -> getitem[slice] (-> reshape -> permute)
                      -> batched matmul              # WRONG (fused)

  The QKV projection is a matmul; head-splitting slices it with getitem;
  Q@K^T is a batched matmul that reads those slices. Fused, thvm drops the
  shrink offset/stride and computes the wrong scores (rel ~4 vs numpy
  ground truth; tinygrad is exact). Inserting a realize barrier on q/k/v
  (test_realize_barrier_fixes_scores) makes the forward match ground truth
  exactly -- proving it is a fusion-time movement-op bug, NOT a math error
  in matmul/softmax/layernorm themselves.

  Pinned minimal repro (no attention, no softmax needed):
      x@w -> reshape(B,S,3,H,Dh) -> [:, :, 0] -> reshape -> permute -> @self
  is wrong; a plain elementwise or sum-reduce over the same getitem view is
  CORRECT, so the corruption is in the N-D-matmul index path specifically.

  Family: SHRINK-over-computed-source offset drop (cf.
  project_shrink_over_pad_offset_dropped, but the source is a matmul/reshape
  rather than a PAD).

  Backward inherits the wrong forward (grads finite but ~2x off; rel fails).

These tests are EXPECTED TO FAIL at HEAD and stay red for the fix workflow.

    make -C <root> py && python -m pytest py/tests/test_transformer_block_parity.py
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


# ---- block dims (small, as specified) --------------------------------
B, SEQ, D_MODEL, N_HEADS, D_FF = 2, 8, 16, 2, 32
D_HEAD = D_MODEL // N_HEADS


def _make_weights(seed=0):
    """Full transformer-block weights as numpy, shared by both builds.
    Linear weights are (out, in); applied as x @ W^T + b (tinygrad/thvm)."""
    rng = np.random.default_rng(seed)

    def lin(out_f, in_f):
        bound = 1.0 / np.sqrt(in_f)
        w = rng.uniform(-bound, bound, (out_f, in_f)).astype(np.float32)
        b = rng.uniform(-bound, bound, (out_f,)).astype(np.float32)
        return w, b

    w = {}
    w["qkv_w"], w["qkv_b"] = lin(3 * D_MODEL, D_MODEL)   # fused QKV proj
    w["o_w"], w["o_b"] = lin(D_MODEL, D_MODEL)            # attn output proj
    w["ff1_w"], w["ff1_b"] = lin(D_FF, D_MODEL)           # MLP up
    w["ff2_w"], w["ff2_b"] = lin(D_MODEL, D_FF)           # MLP down
    w["ln1_w"] = rng.uniform(0.5, 1.5, (D_MODEL,)).astype(np.float32)
    w["ln1_b"] = rng.uniform(-0.2, 0.2, (D_MODEL,)).astype(np.float32)
    w["ln2_w"] = rng.uniform(0.5, 1.5, (D_MODEL,)).astype(np.float32)
    w["ln2_b"] = rng.uniform(-0.2, 0.2, (D_MODEL,)).astype(np.float32)
    return w


def _block(Tn, leaf, W, X, realize_qkv=False):
    """Build + run the transformer block with backend Tn (constructed leaves
    via `leaf`). Returns (out, param_dict, x_leaf). If realize_qkv, force a
    realize barrier on q/k/v -- which sidesteps the fusion bug (forward then
    matches ground truth) but detaches autograd in thvm, so it is only used
    to *demonstrate* the bug, never on the grad path."""
    p = {k: leaf(v) for k, v in W.items()}
    x = leaf(X)

    def linear(t, wn, bn):
        return t.linear(p[wn].transpose(), p[bn])

    def layernorm(t, wn, bn):
        return t.layernorm(axis=-1, eps=1e-5) * p[wn] + p[bn]

    qkv = linear(x, "qkv_w", "qkv_b").reshape(B, SEQ, 3, N_HEADS, D_HEAD)
    q = qkv[:, :, 0].reshape(B, SEQ, N_HEADS, D_HEAD).permute(0, 2, 1, 3)
    k = qkv[:, :, 1].reshape(B, SEQ, N_HEADS, D_HEAD).permute(0, 2, 1, 3)
    v = qkv[:, :, 2].reshape(B, SEQ, N_HEADS, D_HEAD).permute(0, 2, 1, 3)
    if realize_qkv:
        q.realize()
        k.realize()
        v.realize()
    scores = (q @ k.transpose(-2, -1)) * (1.0 / float(np.sqrt(D_HEAD)))
    ctx = scores.softmax(axis=-1) @ v
    ctx = ctx.permute(0, 2, 1, 3).reshape(B, SEQ, D_MODEL)
    h = layernorm(x + linear(ctx, "o_w", "o_b"), "ln1_w", "ln1_b")
    ff = linear(linear(h, "ff1_w", "ff1_b").relu(), "ff2_w", "ff2_b")
    out = layernorm(h + ff, "ln2_w", "ln2_b")
    return out, p, x


def _np_block(W, X):
    """numpy ground-truth forward (no autograd) for the block output."""
    def lin(t, wn, bn):
        return t @ W[wn].T + W[bn]

    def ln(t, wn, bn):
        mu = t.mean(-1, keepdims=True)
        var = t.var(-1, keepdims=True)
        return (t - mu) / np.sqrt(var + 1e-5) * W[wn] + W[bn]

    qkv = lin(X, "qkv_w", "qkv_b").reshape(B, SEQ, 3, N_HEADS, D_HEAD)
    q = qkv[:, :, 0].transpose(0, 2, 1, 3)
    k = qkv[:, :, 1].transpose(0, 2, 1, 3)
    v = qkv[:, :, 2].transpose(0, 2, 1, 3)
    scores = np.einsum("bhid,bhjd->bhij", q, k) / np.sqrt(D_HEAD)
    scores = scores - scores.max(-1, keepdims=True)
    e = np.exp(scores)
    attn = e / e.sum(-1, keepdims=True)
    ctx = np.einsum("bhij,bhjd->bhid", attn, v)
    ctx = ctx.transpose(0, 2, 1, 3).reshape(B, SEQ, D_MODEL)
    h = ln(X + lin(ctx, "o_w", "o_b"), "ln1_w", "ln1_b")
    ff = lin(np.maximum(lin(h, "ff1_w", "ff1_b"), 0.0), "ff2_w", "ff2_b")
    return ln(h + ff, "ln2_w", "ln2_b")


def _relmax(th, tg):
    return float(np.abs(th - tg).max() / (np.abs(tg).max() + 1e-6))


@unittest.skipUnless(_have_tg(), "tinygrad not on path")
class TestTransformerBlockParity(unittest.TestCase):

    def setUp(self):
        self.W = _make_weights(0)
        self.X = (np.random.default_rng(7)
                  .standard_normal((B, SEQ, D_MODEL)).astype(np.float32))

    def _thvm(self, realize_qkv=False):
        import thvm
        return _block(thvm.Tensor,
                      lambda a: thvm.Tensor(a.copy()).requires_grad_(True),
                      self.W, self.X, realize_qkv=realize_qkv)

    def _tg(self):
        import tinygrad
        return _block(tinygrad.Tensor,
                      lambda a: tinygrad.Tensor(a.copy(), requires_grad=True),
                      self.W, self.X)

    def test_forward_parity(self):
        """FORWARD: thvm block output == tinygrad within fp tol.

        EXPECTED FAIL at HEAD: fused batched-matmul over a getitem-view of a
        matmul output drops the shrink offset/stride (see module docstring).
        thvm scores diverge from tinygrad by rel ~0.17 at the block output."""
        oth, _, _ = self._thvm()
        otg, _, _ = self._tg()
        oth_n, otg_n = oth.numpy(), otg.numpy()
        self.assertEqual(oth_n.shape, otg_n.shape)
        self.assertLess(_relmax(oth_n, otg_n), 1e-4,
                        f"forward relmax={_relmax(oth_n, otg_n):.4g}")

    def test_realize_barrier_fixes_scores(self):
        """Diagnostic: with a realize barrier on q/k/v, the thvm forward
        matches numpy ground truth exactly -- proving the bug is fusion-time
        movement-op index dropping, NOT a math error in matmul/softmax/ln.
        (PASSES today; documents the root cause for the fix workflow.)"""
        oth, _, _ = self._thvm(realize_qkv=True)
        gt = _np_block(self.W, self.X)
        self.assertLess(_relmax(oth.numpy(), gt), 1e-4,
                        "realize-barrier forward should match numpy GT")

    def test_backward_parity(self):
        """BACKWARD: d/d(every weight) AND d/d(input) match tinygrad.

        EXPECTED FAIL at HEAD: inherits the wrong fused forward (grads are
        finite but ~2x off). Pinned for the same fix as forward."""
        oth, pth, xth = self._thvm()
        otg, ptg, xtg = self._tg()
        oth.sum().backward()
        otg.sum().backward()

        gx_th = xth.grad.numpy().reshape(self.X.shape)
        gx_tg = xtg.grad.numpy()
        self.assertLess(_relmax(gx_th, gx_tg), 1e-4,
                        f"d/d(input) relmax={_relmax(gx_th, gx_tg):.4g}")
        for name in self.W:
            g_th = pth[name].grad.numpy().reshape(self.W[name].shape)
            g_tg = ptg[name].grad.numpy()
            self.assertLess(_relmax(g_th, g_tg), 1e-4,
                            f"d/d({name}) relmax={_relmax(g_th, g_tg):.4g}")


def _mlp_block(p, x, ln1, ln2):
    """The forward-CORRECT sub-block of the transformer: residual ->
    LayerNorm -> 2-layer MLP (Linear-relu-Linear) -> residual -> LayerNorm.
    No attention (no batched-matmul-over-getitem), so this exercises the
    SAME optim + layernorm-bwd + matmul-bwd integration the session enabled
    but WITHOUT the fused-attention forward bug.  Used by the positive
    train tests below to prove the optimizers genuinely descend on a thvm
    transformer-style graph, matching tinygrad."""
    def L(t, wn, bn):
        return t.linear(p[wn].transpose(), p[bn])

    def LN(t, wn, bn):
        return t.layernorm(axis=-1, eps=1e-5) * p[wn] + p[bn]

    h = LN(x, *ln1)
    ff = L(L(h, "ff1_w", "ff1_b").relu(), "ff2_w", "ff2_b")
    return LN(h + ff, *ln2)


@unittest.skipUnless(_have_tg(), "tinygrad not on path")
class TestTransformerTrain(unittest.TestCase):
    """TRAIN: descent on the transformer sub-block that has a CORRECT
    forward (residual + LayerNorm + 2-layer MLP), with both Adam and Muon,
    vs tinygrad.  Because the FULL block's attention forward is wrong (the
    pinned fused batched-matmul-over-getitem bug), full-block training goes
    NaN by step 1 (see test_full_block_train_goes_nan_step1) -- so a
    tinygrad-comparable *full*-block descent is impossible at HEAD and is
    NOT what these tests assert.  Instead they validate the Adam/Muon +
    layernorm-bwd + matmul-bwd machinery the session enabled descends a
    multi-step loss with no NaN and tracks tinygrad's descent on the
    correct-forward sub-block.

    Backward MUST precede any loss read: thvm frees the autograd graph on
    realize, so reading loss.numpy() before backward() yields None grads
    (canonical thvm idiom, see test_adam_jit_bias_correction).

    CORRECTION to a prior report: the full-block step time does NOT
    'explode after step 0 / fail to finish in minutes'.  It is a CONSTANT
    ~16s/step (the buggy fused-attention grad graph is just expensive to
    realize); step 1 produces NaN, it does not hang.  No per-step
    graph-growth pathology exists -- the correct-forward sub-block below
    realizes in ~0.07s/step (Adam) after a one-time compile."""

    def setUp(self):
        self.W = _make_weights(0)
        self.X = (np.random.default_rng(7)
                  .standard_normal((B, SEQ, D_MODEL)).astype(np.float32))
        self.target = (np.random.default_rng(11)
                       .standard_normal((B, SEQ, D_MODEL)).astype(np.float32))

    # The MLP sub-block reads only these leaves; pass exactly them to the
    # optimizer (tinygrad's optim asserts on a param with a None grad).
    _MLP_PARAMS = ("ff1_w", "ff1_b", "ff2_w", "ff2_b",
                   "ln1_w", "ln1_b", "ln2_w", "ln2_b")

    def _train_thvm_mlp(self, opt_name, n_steps=6):
        import thvm
        p = {k: thvm.Tensor(self.W[k].copy()).requires_grad_(True)
             for k in self._MLP_PARAMS}
        x = thvm.Tensor(self.X.copy())
        tgt = thvm.Tensor(self.target.copy())
        opt = ((thvm.optim.Adam if opt_name == "adam" else thvm.optim.Muon)
               (list(p.values()), lr=1e-2))
        ln1, ln2 = ("ln1_w", "ln1_b"), ("ln2_w", "ln2_b")
        losses = []
        with thvm.Tensor.train():
            for _ in range(n_steps):
                opt.zero_grad()
                d = _mlp_block(p, x, ln1, ln2) - tgt
                loss = (d * d).mean()
                loss.backward()                 # before any loss read
                # Read the PRE-update loss (tinygrad reads loss.numpy()
                # of the same forward, which reflects the params BEFORE
                # the in-place ASSIGN this step applies).  Bundling loss
                # into Tensor.realize(loss, *sched) instead lets the
                # in-place param ASSIGN mutate the buffers loss's forward
                # reads -> loss lags one step.  Realize+read loss first,
                # then apply the step.
                loss.realize()
                losses.append(float(loss.numpy()))
                thvm.Tensor.realize(*opt.schedule_step())
        return losses

    def _train_tg_mlp(self, opt_name, n_steps=6):
        import tinygrad
        p = {k: tinygrad.Tensor(self.W[k].copy(), requires_grad=True)
             for k in self._MLP_PARAMS}
        x = tinygrad.Tensor(self.X.copy())
        tgt = tinygrad.Tensor(self.target.copy())
        from tinygrad.nn.optim import Adam, Muon
        opt = (Adam if opt_name == "adam" else Muon)(list(p.values()), lr=1e-2)
        ln1, ln2 = ("ln1_w", "ln1_b"), ("ln2_w", "ln2_b")
        losses = []
        with tinygrad.Tensor.train():
            for _ in range(n_steps):
                opt.zero_grad()
                d = _mlp_block(p, x, ln1, ln2) - tgt
                loss = (d * d).mean()
                loss.backward()
                # Read the PRE-update loss: tinygrad's in-place param ASSIGN
                # in opt.step() mutates the buffers loss's lazy forward
                # reads, so a post-step loss.numpy() would be one update
                # ahead.  Realize+read loss, then step (matches the thvm
                # helper above).
                loss.realize()
                losses.append(float(loss.numpy()))
                opt.step()
        return losses

    def _assert_descends(self, losses, opt_name):
        self.assertFalse(any(np.isnan(losses)),
                         f"NaN in {opt_name} loss {losses}")
        self.assertLess(losses[-1], losses[0],
                        f"{opt_name} did not descend: {losses}")

    def test_train_adam_descends(self):
        """Adam descends the correct-forward sub-block with no NaN, and
        tracks tinygrad's descent (final-loss within fp tol)."""
        th = self._train_thvm_mlp("adam")
        tg = self._train_tg_mlp("adam")
        self._assert_descends(th, "thvm-adam")
        self._assert_descends(tg, "tg-adam")
        self.assertLess(_relmax(np.array(th), np.array(tg)), 1e-3,
                        f"adam descent thvm {th} vs tg {tg}")

    def test_train_muon_descends(self):
        """Muon (Newton-Schulz) descends the correct-forward sub-block with
        no NaN, and tracks tinygrad's descent."""
        th = self._train_thvm_mlp("muon")
        tg = self._train_tg_mlp("muon")
        self._assert_descends(th, "thvm-muon")
        self._assert_descends(tg, "tg-muon")
        self.assertLess(_relmax(np.array(th), np.array(tg)), 1e-3,
                        f"muon descent thvm {th} vs tg {tg}")

    def test_full_block_train_goes_nan_step1(self):
        """GUARDRAIL for the fix workflow: the FULL block (with attention)
        trains to a FINITE loss at step 0 but NaN at step 1 -- the wrong
        fused-attention forward produces wrong/exploding grads.  tinygrad's
        identical full block descends cleanly (no NaN) for 8 steps.  This
        test pins the CURRENT broken behavior (step0 finite, step1 NaN) so
        the fusion fix can flip it; it is EXPECTED to assert NaN at HEAD.

        Bounded to 2 steps: each step is ~16s on the buggy graph."""
        import thvm
        p = {k: thvm.Tensor(v.copy()).requires_grad_(True)
             for k, v in self.W.items()}
        x = thvm.Tensor(self.X.copy())
        tgt = thvm.Tensor(self.target.copy())
        opt = thvm.optim.Adam(list(p.values()), lr=1e-2)

        def fwd():
            def L(t, wn, bn):
                return t.linear(p[wn].transpose(), p[bn])

            def LN(t, wn, bn):
                return t.layernorm(axis=-1, eps=1e-5) * p[wn] + p[bn]

            qkv = L(x, "qkv_w", "qkv_b").reshape(B, SEQ, 3, N_HEADS, D_HEAD)
            q = qkv[:, :, 0].reshape(
                B, SEQ, N_HEADS, D_HEAD).permute(0, 2, 1, 3)
            k = qkv[:, :, 1].reshape(
                B, SEQ, N_HEADS, D_HEAD).permute(0, 2, 1, 3)
            vv = qkv[:, :, 2].reshape(
                B, SEQ, N_HEADS, D_HEAD).permute(0, 2, 1, 3)
            sc = (q @ k.transpose(-2, -1)) * (1.0 / float(np.sqrt(D_HEAD)))
            ctx = (sc.softmax(axis=-1) @ vv)
            ctx = ctx.permute(0, 2, 1, 3).reshape(B, SEQ, D_MODEL)
            h = LN(x + L(ctx, "o_w", "o_b"), "ln1_w", "ln1_b")
            ff = L(L(h, "ff1_w", "ff1_b").relu(), "ff2_w", "ff2_b")
            return LN(h + ff, "ln2_w", "ln2_b")

        losses = []
        with thvm.Tensor.train():
            for _ in range(2):
                opt.zero_grad()
                d = fwd() - tgt
                loss = (d * d).mean()
                loss.backward()
                sched = opt.schedule_step()
                thvm.Tensor.realize(loss, *sched)
                losses.append(float(loss.numpy()))
        self.assertFalse(np.isnan(losses[0]),
                         f"step0 should be finite, got {losses}")
        self.assertTrue(np.isnan(losses[1]),
                        f"EXPECTED step1 NaN (forward bug); got {losses} -- "
                        "if this is finite the fusion bug may be fixed")


if __name__ == "__main__":
    unittest.main()
