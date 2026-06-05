"""Pin the thvm scheduler bug: a MUL+REDUCE (matmul) whose operand is a
movement-op VIEW (reshape/shrink/permute) over another op's UNREALIZED
output mis-fuses -- the view's offset/strides are dropped, producing wrong
results.  This is the GPT2 head-split pattern
(`qkv[:, :, i].transpose(1, 2) @ ...`, a getitem-view of the c_attn
matmul+bias output).

Tensor.__matmul__ currently inserts a contiguity barrier
(`_matmul_contiguous_operand`) that realizes such operands, so the PUBLIC
`@` path is correct (test_public_matmul_is_correct).  The UNDERLYING
scheduler bug is still live: test_raw_fused_matmul_over_computed_view_is_wrong
reaches it by building the same graph WITHOUT the barrier (operating on the
raw shrink view) and asserts the wrong answer that proves the bug is unfixed.
When the C scheduler is fixed, the `xfail` flips to a pass and the barrier
in Tensor.__matmul__ can be removed.
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

import thvm  # noqa: E402
from thvm import Tensor  # noqa: E402


def _setup():
    np.random.seed(0)
    B, S, H, Dh = 1, 4, 12, 64
    D = H * Dh
    base = np.random.randn(B, S, 3 * D).astype("float32")
    arr = base * 2.0  # a COMPUTED (unrealized) source
    qkv_np = arr.reshape(B, S, 3, H, Dh)
    qn = qkv_np[:, :, 0].transpose(0, 2, 1, 3)
    kn = qkv_np[:, :, 1].transpose(0, 2, 1, 3)
    scn = np.einsum("bhid,bhjd->bhij", qn, kn)
    return base, scn, (B, S, H, Dh)


def _build_views():
    base, scn, (B, S, H, Dh) = _setup()
    bt = Tensor(base.copy()) * 2.0
    qkv = bt.reshape(B, S, 3, H, Dh)
    q = qkv[:, :, 0, :, :].transpose(1, 2)   # view of computed source
    k = qkv[:, :, 1, :, :].transpose(1, 2)
    return q, k, scn


def _relmax(a, b):
    return float(np.abs(a - b).max() / (np.abs(b).max() + 1e-6))


class TestMatmulOverComputedView(unittest.TestCase):

    def test_public_matmul_is_correct(self):
        """The public `@` (with the contiguity barrier) matches numpy."""
        q, k, scn = _build_views()
        sc = (q @ k.transpose(-2, -1)).numpy()
        self.assertLess(_relmax(sc, scn), 1e-4,
                        f"public matmul relmax={_relmax(sc, scn):.4g}")

    @unittest.expectedFailure
    def test_raw_fused_matmul_over_computed_view_is_wrong(self):
        """Bypass the barrier: feed the RAW (unrealized) views straight
        into the fused MUL+REDUCE.  EXPECTED FAIL at HEAD -- the scheduler
        drops the view offset/strides (relmax ~1+).  When the C fix lands,
        this passes and the Tensor.__matmul__ barrier can be removed."""
        q, k, scn = _build_views()
        kt = k.transpose(-2, -1)
        # Replicate __matmul__'s N-D contraction WITHOUT the barrier:
        # reshape+expand+mul+sum over the contraction axis, on the raw
        # (unrealized) views.
        x = q                                    # (B,H,S,Dh)
        w = kt                                   # (B,H,Dh,S)
        B, H, S, Dh = x.shape
        Sk = w.shape[-1]
        xb = x.reshape(B, H, S, 1, Dh).expand(B, H, S, Sk, Dh)
        wb = w.transpose(-1, -2).reshape(B, H, 1, Sk, Dh).expand(B, H, S, Sk, Dh)
        sc = (xb * wb).sum(axis=-1).numpy()
        self.assertLess(_relmax(sc, scn), 1e-4,
                        f"raw fused matmul relmax={_relmax(sc, scn):.4g}")


if __name__ == "__main__":
    unittest.main()
