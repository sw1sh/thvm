"""Reduce-kind-chain fusion parity (a correctness guardrail).

A REDUCE feeding directly into another REDUCE of a DIFFERENT kind (a max_pool
MAX-reduce feeding a global SUM/MEAN) must NOT fuse into one kernel: the inner
reduce's accumulator semantics are lost when its body is inlined into the outer
reduce.  tinygrad keeps the bufferize between such reduce OPs unconditionally
(schedule/rangeify.py:88-96 remove_bufferize, buffer_in_reduce -> keep).  thvm's bufferize-classify reduce-chain unmark used to fuse
ANY inner-reduce -> consumer-reduce regardless of kind, corrupting the forward
value (e.g. x.max_pool2d().sum() returned ~2% of the correct total).  That wrong
forward then poisoned the whole backward tape (the RANK-4 conv-weight grad).

Ground truth is numpy.  Same-kind chains (sum-then-sum) are exercised too as a
guard against over-fixing (they legitimately fold).

    make py && python -m pytest py/tests/test_reduce_kind_chain.py
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

from thvm import Context, Tensor, nn  # noqa: E402

TOL = 1e-3


def maxpool_ref(a, k=2):
    # non-overlapping (stride==kernel) window max over the last two axes.
    n, c, h, w = a.shape
    a = a.reshape(n, c, h // k, k, w // k, k)
    return a.max(axis=3).max(axis=4)


class TestReduceKindChain(unittest.TestCase):
    def setUp(self):
        self.rng = np.random.default_rng(3)

    def _rand(self, shp):
        return self.rng.standard_normal(shp).astype(np.float32)

    # --- forward: MAX-reduce (pool) feeding a SUM/MEAN reduce ---
    def test_maxpool_then_global_sum(self):
        a = self._rand((2, 3, 8, 8))
        got = float(Tensor(a).max_pool2d().sum().numpy())
        ref = float(maxpool_ref(a).sum())
        self.assertLessEqual(abs(got - ref) / max(abs(ref), 1.0), TOL,
                             f"max_pool2d().sum(): got {got} ref {ref}")

    def test_maxpool_then_global_mean(self):
        a = self._rand((2, 3, 8, 8))
        got = float(Tensor(a).max_pool2d().mean().numpy())
        ref = float(maxpool_ref(a).mean())
        self.assertLessEqual(abs(got - ref) / max(abs(ref), 1.0), TOL,
                             f"max_pool2d().mean(): got {got} ref {ref}")

    def test_max_axis_then_sum(self):
        a = self._rand((2, 3, 8, 8))
        got = float(Tensor(a).max(axis=3).sum().numpy())
        ref = float(a.max(axis=3).sum())
        self.assertLessEqual(abs(got - ref) / max(abs(ref), 1.0), TOL,
                             f"max(3).sum(): got {got} ref {ref}")

    # --- guard: same-kind chain still folds correctly ---
    def test_sum_axis_then_sum(self):
        a = self._rand((2, 3, 8, 8))
        got = float(Tensor(a).sum(axis=3).sum().numpy())
        ref = float(a.sum(axis=3).sum())
        self.assertLessEqual(abs(got - ref) / max(abs(ref), 1.0), TOL,
                             f"sum(3).sum(): got {got} ref {ref}")

    # --- backward: conv -> relu -> BN(train) -> max_pool -> sum ---
    # The wrong forward (above) corrupted this conv-weight grad (RANK 4).
    # Reference: a one-conv pool-sum whose forward is now correct, so the
    # autograd grad must be finite + match a finite-difference probe.
    @Context(TRAINING=1)
    def test_conv_relu_pool_sum_bwd_matches_fd(self):
        x = self._rand((2, 1, 12, 12))
        w = (self._rand((8, 1, 5, 5))) * 0.1

        def loss_of(weight_np):
            c = nn.Conv2d(1, 8, 5, bias=False)
            c.weight = Tensor(weight_np.copy())
            return float(c(Tensor(x.copy())).relu().max_pool2d().sum().numpy())

        c = nn.Conv2d(1, 8, 5, bias=False)
        c.weight = Tensor(w.copy())
        c(Tensor(x.copy())).relu().max_pool2d().sum().backward()
        ag = c.weight.grad.numpy()

        # central finite-difference on a few coords
        eps = 1e-2
        flat = ag.flatten()
        idxs = [0, 17, 99, 150]
        for fi in idxs:
            mi = np.unravel_index(fi, w.shape)
            wp = w.copy(); wp[mi] += eps
            wm = w.copy(); wm[mi] -= eps
            fd = (loss_of(wp) - loss_of(wm)) / (2 * eps)
            self.assertLessEqual(
                abs(flat[fi] - fd) / max(abs(fd), 1.0), 5e-2,
                f"conv-pool bwd coord {mi}: ag {flat[fi]} fd {fd}")
        self.assertGreater(float(np.abs(ag).sum()), 1.0,
                           "conv-pool grad collapsed to ~0 (severed tape)")


if __name__ == "__main__":
    unittest.main()
