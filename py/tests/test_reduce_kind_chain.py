"""Reduce-kind-chain fusion parity (a correctness guardrail).

A REDUCE feeding directly into another REDUCE of a DIFFERENT kind (a max_pool
MAX-reduce feeding a global SUM/MEAN) must NOT fuse into one kernel: the inner
reduce's accumulator semantics are lost when its body is inlined into the outer
reduce.  tinygrad keeps the bufferize between such reduce OPs unconditionally
(schedule/rangeify.py:269-293 remove_bufferize, buffer_in_reduce -> keep,
PCONTIG default 0).  thvm's bufferize-classify reduce-chain unmark used to fuse
ANY inner-reduce -> consumer-reduce regardless of kind, corrupting the forward
value (e.g. x.max_pool2d().sum() returned ~2% of the correct total).  That wrong
forward then poisoned the whole backward tape (the RANK-4 conv-weight grad).

Ground truth is numpy.  Same-kind chains (sum-then-sum) are exercised too as a
guard against over-fixing (they legitimately fold).

    make py && python -m pytest py/tests/test_reduce_kind_chain.py
"""
import os
import pathlib
import subprocess
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

from thvm import Tensor, nn  # noqa: E402

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
    def test_conv_relu_pool_sum_bwd_matches_fd(self):
        Tensor.training = True
        x = self._rand((2, 1, 12, 12))
        w = (self._rand((8, 1, 5, 5))) * 0.1

        def loss_of(weight_np):
            c = nn.Conv2d(1, 8, 5, bias=False)
            c.weight = Tensor(weight_np.copy())
            return float(c(Tensor(x.copy())).relu().max_pool2d().sum().numpy())

        c = nn.Conv2d(1, 8, 5, bias=False)
        c.weight = Tensor(w.copy())
        c.weight.requires_grad_()
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


# Seed-mode parity on a max_pool-bearing forward+backward graph: default and
# faithful realize-seed must agree (the env is read once per process, so each
# mode runs in its own subprocess -- mirrors test_faithful_parity).
_CHILD = r"""
import os, sys
import numpy as np
sys.path.insert(0, os.environ["THVM_PY"])
from thvm import Tensor, nn
np.random.seed(0)
Tensor.training = True
bs = 4
c1 = nn.Conv2d(1, 8, 5); bn = nn.BatchNorm(8)
nn.optim.Adam(nn.state.get_parameters([c1]))
x = Tensor(np.random.randn(bs, 1, 16, 16).astype(np.float32)); x.realize()
out = bn(c1(x).relu()).max_pool2d().sum()
out.backward(); out.realize()
gssq = float((c1.weight.grad * c1.weight.grad).sum().numpy())
print(f"OUT={float(out.numpy()):.7g} GSSQ={gssq:.7g}")
"""


class TestSeedModePoolParity(unittest.TestCase):
    def _run(self, faithful):
        env = dict(os.environ, THVM_PY=str(ROOT / "py"), DEV="cpu")
        if faithful:
            env["THVM_RU_FAITHFUL_SEED"] = "1"
        else:
            env.pop("THVM_RU_FAITHFUL_SEED", None)
        out = subprocess.run([sys.executable, "-c", _CHILD], env=env,
                             capture_output=True, text=True, timeout=300)
        line = next((l for l in out.stdout.splitlines()
                     if l.startswith("OUT=")), None)
        assert line is not None, f"child failed:\n{out.stdout}\n{out.stderr}"
        o = float(line.split("OUT=")[1].split()[0])
        g = float(line.split("GSSQ=")[1])
        return o, g

    def test_pool_forward_and_grad_seed_parity(self):
        d_o, d_g = self._run(faithful=False)
        f_o, f_g = self._run(faithful=True)
        self.assertAlmostEqual(f_o, d_o, delta=1e-3,
                               msg=f"pool out: faithful {f_o} vs default {d_o}")
        self.assertLessEqual(abs(f_g - d_g) / max(abs(d_g), 1.0), 1e-3,
                             f"pool grad ssq: faithful {f_g} vs default {d_g}")


if __name__ == "__main__":
    unittest.main()
