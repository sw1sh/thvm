"""Deep fused chains must materialize correctly.

materialize.c's graph walks/rewrites were bounded by `if (depth > 256)`
guards that returned a WRONG result past the budget (the term unchanged, 0,
or an empty list) instead of failing, so any graph deeper than 256 was
silently mis-materialized -- a 300-deep `a+b+c+...` chain summed to 0.0
instead of its true value.  The bug was masked for years because clang's own
256-bracket parser limit rejected the generated kernel, so such chains fell
back to the interpreter.

Run:
  cd <repo> && PYTHONPATH=py DEV=cpu python3 -m pytest py/tests/test_deep_chain_depth.py -q
"""
import unittest
import numpy as np
from thvm import Tensor


class TestDeepChainDepth(unittest.TestCase):
    def _chain(self, depth, nbuf, seed=7):
        rng = np.random.default_rng(seed)
        arrs = [rng.random(32).astype(np.float32) for _ in range(nbuf)]
        ts = [Tensor(a) for a in arrs]
        acc_t, acc_n = ts[0], arrs[0].copy()
        for i in range(depth):
            acc_t = acc_t + ts[i % nbuf]
            acc_n = acc_n + arrs[i % nbuf]
        return float(acc_t.numpy().sum()), float(acc_n.sum())

    def test_depth_below_old_guard(self):
        got, ref = self._chain(200, 8)
        self.assertAlmostEqual(got, ref, delta=max(1e-2, abs(ref) * 1e-5))

    def test_depth_at_old_guard(self):
        # 255 was the first depth that rendered an unsound kernel
        got, ref = self._chain(255, 8)
        self.assertAlmostEqual(got, ref, delta=max(1e-2, abs(ref) * 1e-5))

    def test_depth_past_old_guard(self):
        # 1000 and 2000 also cross the UOp rewrite passes' old 256 cap and
        # stress their recursion budget (uop/graph_rewrite.c)
        for depth in (300, 500, 1000, 2000):
            with self.subTest(depth=depth):
                got, ref = self._chain(depth, 8)
                self.assertAlmostEqual(got, ref, delta=max(1e-2, abs(ref) * 1e-5))

    def test_single_buffer_deep_chain(self):
        # nbuf=1 previously returned exactly 0.0 past the guard
        got, ref = self._chain(300, 1)
        self.assertAlmostEqual(got, ref, delta=max(1e-2, abs(ref) * 1e-5))

    def test_in_graph_sum_over_deep_const_chain(self):
        # The reduce runs INSIDE thvm, over a chain of broadcast-CONST adds.
        # rangeify's "does the reduce body still use its axis" walk gave up
        # at depth 64 and answered no, so the broadcast repair rewrote
        # SUM(body) into body*extent (depth 62 exact, 63+ off by 1.36).
        from thvm import K
        from thvm.dtypes import dtypes
        from thvm.tensor import _TH
        from thvm.thvm import Term
        rng = np.random.default_rng(0)
        x = rng.standard_normal((3, 4)).astype(np.float32)
        for depth in (62, 64, 100, 300):
            with self.subTest(depth=depth):
                v = int(Tensor(x).term)
                c = _TH.expand(_TH.reshape(_TH.fconst(0.001), (1, 1)), (3, 4))
                for _ in range(depth):
                    v = _TH.add(v, c)
                r = _TH.reduce(K.REDUCE_SUM, (1,), v)
                got = Tensor._from_term(Term(r), dtypes.float32, (3,)).numpy()
                want = (x.astype(np.float64) + 0.001 * depth).sum(axis=1)
                np.testing.assert_allclose(got, want, atol=1e-4)

if __name__ == "__main__":
    unittest.main()
