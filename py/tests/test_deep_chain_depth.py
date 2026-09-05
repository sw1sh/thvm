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
        for depth in (300, 500):
            with self.subTest(depth=depth):
                got, ref = self._chain(depth, 8)
                self.assertAlmostEqual(got, ref, delta=max(1e-2, abs(ref) * 1e-5))

    def test_single_buffer_deep_chain(self):
        # nbuf=1 previously returned exactly 0.0 past the guard
        got, ref = self._chain(300, 1)
        self.assertAlmostEqual(got, ref, delta=max(1e-2, abs(ref) * 1e-5))


if __name__ == "__main__":
    unittest.main()
