"""Regression: a MUL+REDUCE (matmul) whose operand is a movement-op VIEW
(reshape/shrink/permute) over another op's UNREALIZED output must fuse
correctly -- the view's offset/strides must be composed into the reduce's
index, not dropped.  This is the GPT2 head-split pattern
(`qkv[:, :, i].transpose(1, 2) @ ...`, a getitem-view of the c_attn
matmul+bias output).

The C scheduler bug (a `realized_full` view boundary with a leading unit
axis was wrongly inlined, dropping the SHRINK begin offset) was fixed in
topo_sort_boundaries (src/schedule/materialize.c): a full-realize node
whose closed-range count dropped below its rank (a unit axis collapsed)
and that is read by a consumer SHRINK with a non-zero begin is now
realized as a boundary the consumer indexes at its own offset, instead of
being inlined and re-decoded (which dropped the offset), matching
tinygrad/schedule/indexing.py:63.  Both the public `@` path and the raw
fused MUL+REDUCE over the unrealized view now match numpy.
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

    def test_raw_fused_matmul_over_computed_view(self):
        """Feed the RAW (unrealized) views straight into the fused
        MUL+REDUCE, bypassing any contiguity barrier.  The scheduler must
        compose the view offset/strides into the reduce index and match
        numpy (relmax < 1e-4)."""
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
