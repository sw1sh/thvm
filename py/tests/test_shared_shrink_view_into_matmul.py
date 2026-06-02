"""Minimal repro: an N-D matmul (or any MUL) consuming an un-realized
SHRINK/getitem view of a computed source that is ALSO read via a second,
structurally-different movement leg DROPS the stride at fusion time.

Root cause (rangeify_unified.c): a movement op (SHRINK) shared by 2+
consumers with divergent index swizzles is force-shared via the
`uop_is_movement -> all_all_same=1` override (line ~856), then a SINGLE
RU_SUBST minted against consumer 0 is re-expressed for consumer 1 by a
POSITIONAL axis-id substitution that assumes both consumers' RESHAPE
swizzles have the identical tree shape (ru_subtree_align, ~line 1689).
The two reshape legs `q.reshape(M,N,1)` vs `q.reshape(M,1,N)` do NOT, so
consumer 1 silently inherits consumer 0's stride -> wrong result.

tinygrad (schedule/indexing.py:196-215) has NO movement-share override:
divergent consumers of a movement op partial-realize (fresh range +
buffer boundary, line 213-215), so each consumer reads a correct
ShapeTracker.

Controls that PASS prove it is fusion-time movement-op index dropping,
not a math error: elementwise (q*q) / reduce (q.sum) of the SAME view, a
realize() barrier on q, and two SEPARATE q subgraphs all match numpy.
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))


class TestSharedShrinkViewIntoMatmul(unittest.TestCase):
    def setUp(self):
        rng = np.random.default_rng(0)
        self.M, self.K, self.G, self.N = 4, 3, 3, 5
        self.an = rng.standard_normal((self.M, self.K)).astype(np.float32)
        self.bn = rng.standard_normal((self.K, self.G * self.N)).astype(np.float32)
        cn = (self.an @ self.bn).reshape(self.M, self.G, self.N)
        self.qn = cn[:, 0]                                # (M, N)
        self.gt = np.einsum("mi,mj->mij", self.qn, self.qn).sum(-1)

    def _q(self):
        import thvm
        a = thvm.Tensor(self.an.copy())
        b = thvm.Tensor(self.bn.copy())
        c = (a @ b).reshape(self.M, self.G, self.N)
        return c[:, 0]                                    # SHRINK/getitem view

    def _dot(self, q):
        M, N = self.M, self.N
        x = q.reshape(M, N, 1).expand(M, N, N)
        y = q.reshape(M, 1, N).expand(M, N, N)
        return (x * y).sum(-1)

    def test_shared_shrink_two_legs(self):
        """The bug: shared SHRINK consumed by two divergent reshape legs."""
        out = self._dot(self._q()).numpy()
        md = float(np.abs(out - self.gt).max())
        self.assertLess(md, 1e-3, f"shared-shrink-into-mul maxdiff={md:.4g}")

    def test_realize_barrier_control(self):
        """Realize barrier on q -> correct (PASSES today)."""
        import thvm
        q = self._q()
        q.realize()
        out = self._dot(q).numpy()
        self.assertLess(float(np.abs(out - self.gt).max()), 1e-3)

    def test_elementwise_control(self):
        """Same view fed to elementwise q*q -> correct (PASSES today)."""
        q = self._q()
        out = (q * q).numpy()
        self.assertLess(float(np.abs(out - self.qn * self.qn).max()), 1e-3)

    def test_separate_subgraph_control(self):
        """Two SEPARATE q subgraphs (no sharing) -> correct (PASSES today)."""
        M, N = self.M, self.N
        q1, q2 = self._q(), self._q()
        a = q1.reshape(M, N, 1).expand(M, N, N)
        b = q2.reshape(M, 1, N).expand(M, N, N)
        out = (a * b).sum(-1).numpy()
        self.assertLess(float(np.abs(out - self.gt).max()), 1e-3)


if __name__ == "__main__":
    unittest.main()
