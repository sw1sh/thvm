"""Self-contained transformer-attention training + grad regression guard.

Locks in two fixes (no tinygrad dependency -- runs anywhere, unlike the
tinygrad-parity test_transformer_block_parity):

- a02aad17 (arena lifetimes in fire order): a leaf consumed via multiple
  views (attention's q/k/v slices of one qkv weight) gets a CORRECT gradient.
  Before the fix, a recycled arena offset clobbered a live grad buffer and the
  V (or K) slice's grad came back ~0.  Guarded here by a numpy finite-difference
  check on the shared qkv leaf.

- 07532c22 (optim step realizes the lazy assigns): a multi-step eager training
  loop with opt.step() advances (loss descends, no hang).  Before the fix the
  un-sunk assign made wnf_n re-expand without bound (hang) or the update never
  landed (freeze).

    make py && python -m pytest py/tests/test_transformer_train.py
"""
import os
import sys
import unittest

import numpy as np

PY = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, os.environ.get("THVM_PY", PY))

import thvm  # noqa: E402
import thvm.optim as optim  # noqa: E402
from thvm import Tensor as T  # noqa: E402

B, S, H, Dh = 2, 4, 2, 3
D = H * Dh
SCALE = 1.0 / np.sqrt(Dh)


def _attn_np(QKV, O):
    q = QKV[:, :, 0].transpose(0, 2, 1, 3)
    k = QKV[:, :, 1].transpose(0, 2, 1, 3)
    v = QKV[:, :, 2].transpose(0, 2, 1, 3)
    s = np.einsum("bhid,bhjd->bhij", q, k) * SCALE
    s = s - s.max(-1, keepdims=True)
    e = np.exp(s)
    a = e / e.sum(-1, keepdims=True)
    ctx = np.einsum("bhij,bhjd->bhid", a, v).transpose(0, 2, 1, 3).reshape(B, S, D)
    return float((ctx @ O).sum())


def _attn_thvm(qkv, o):
    q = qkv[:, :, 0].reshape(B, S, H, Dh).permute(0, 2, 1, 3)
    k = qkv[:, :, 1].reshape(B, S, H, Dh).permute(0, 2, 1, 3)
    v = qkv[:, :, 2].reshape(B, S, H, Dh).permute(0, 2, 1, 3)
    a = ((q @ k.transpose(-2, -1)) * SCALE).softmax(axis=-1)
    ctx = (a @ v).permute(0, 2, 1, 3).reshape(B, S, D)
    return (ctx @ o).sum()


class TransformerTrainTest(unittest.TestCase):
    def test_qkv_grad_matches_finite_diff(self):
        """a02aad17: shared-qkv-leaf grad correct (no slice clobbered to ~0)."""
        rng = np.random.default_rng(0)
        QKV = rng.standard_normal((B, S, 3, H, Dh)).astype(np.float32)
        O = rng.standard_normal((D, D)).astype(np.float32)
        g = np.zeros_like(QKV)
        eps = 1e-3
        for i in range(QKV.size):
            b = QKV.flat[i]
            QKV.flat[i] = b + eps; lp = _attn_np(QKV, O)
            QKV.flat[i] = b - eps; lm = _attn_np(QKV, O)
            QKV.flat[i] = b
            g.flat[i] = (lp - lm) / (2 * eps)
        qkv = T(QKV.copy()).requires_grad_(True)
        _attn_thvm(qkv, T(O.copy())).backward()
        gt = qkv.grad.numpy().reshape(QKV.shape)
        for slot in range(3):
            relmax = np.abs(gt[:, :, slot] - g[:, :, slot]).max() / (np.abs(g[:, :, slot]).max() + 1e-6)
            self.assertLess(relmax, 1e-3, f"qkv slot {slot} grad relmax={relmax:.4g}")

    def test_attention_block_trains(self):
        """07532c22: multi-step eager opt.step() training descends (no hang/freeze)."""
        T.training = True
        rng = np.random.default_rng(0)
        X = rng.standard_normal((4, 6, D)).astype(np.float32)
        Y = (X @ (rng.standard_normal((D, D)).astype(np.float32) * 0.3)).astype(np.float32)
        Bx, Sx = 4, 6
        xt = T(X.copy()); yt = T(Y.copy())
        Wqkv = T((rng.standard_normal((D, 3 * D)) * 0.1).astype(np.float32)).requires_grad_(True)
        Wo = T((rng.standard_normal((D, D)) * 0.1).astype(np.float32)).requires_grad_(True)
        opt = optim.Adam([Wqkv, Wo], lr=1e-2)

        def loss():
            qkv = (xt @ Wqkv).reshape(Bx, Sx, 3, H, Dh)
            q = qkv[:, :, 0].permute(0, 2, 1, 3)
            k = qkv[:, :, 1].permute(0, 2, 1, 3)
            v = qkv[:, :, 2].permute(0, 2, 1, 3)
            a = ((q @ k.transpose(-2, -1)) * SCALE).softmax(axis=-1)
            ctx = (a @ v).permute(0, 2, 1, 3).reshape(Bx, Sx, D)
            d = (ctx @ Wo) - yt
            return (d * d).sum()

        losses = []
        for _ in range(20):
            opt.zero_grad(); l = loss(); l.backward(); opt.step()
            losses.append(float(l.numpy()))
        self.assertTrue(all(np.isfinite(x) for x in losses), f"non-finite loss: {losses}")
        self.assertLess(losses[-1], 0.85 * losses[0], f"loss did not descend: {losses[0]:.3f} -> {losses[-1]:.3f}")


if __name__ == "__main__":
    unittest.main()
