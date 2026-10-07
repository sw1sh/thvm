"""Optimizer steps match tinygrad master at every heap-GC threshold.

Two repros of one bug class, where a realize boundary left a step reading
bytes it no longer owned:

  - Muon (nn/optim.py:83-93 LARS with Newton-Schulz): the momentum buffers
    realize first, and the step then reads them back.  It used to capture
    the momentum ASSIGN before that realize, so realizing the param fired
    it a second time, from a kernel the kernel GC had already stripped.
    And a heap collection at a realize boundary moved every lazy graph a
    Python Tensor still held (the step's grads, the loss).
  - Bp + gather(X @ W, idx) with Bp and W both trained: Bp's grad is a
    root of the grad realize AND an input of W's grad kernel, so the
    arena planner gave it a recycled slot, and the realize returned a
    buffer it freed (SIGSEGV in the update).

The GC threshold (THVM_GC_KB) and the CPU route (THVM_CPU_JIT_WARMUP) are
read once per process, so each setting runs in a child process; the
tinygrad reference runs here.

    make py && DEV=cpu PYTHONPATH=py:$HOME/tinygrad python -m pytest py/tests/test_optim_step_gc.py -q
"""
import os
import tempfile
import unittest

import numpy as np

import helpers
from thvm import Device

NATIVE = Device.DEFAULT == "CPU"

MUON1 = dict(lr=0.02, momentum=0.95, weight_decay=0.1, nesterov=True)
MUON6 = dict(lr=0.005, momentum=0.9, weight_decay=0.0, nesterov=True)
LARS5 = dict(lr=0.01, momentum=0.9, weight_decay=1e-4, tcoef=0.001, nesterov=False)
STEPS2 = 3


def muon_lars_runs(run, optim):
    """Every Muon / LARS trajectory, through `run` (_thvm_run or _tg_run)."""
    from test_muon_lars_parity import _linreg_problem
    p1 = _linreg_problem(7, 5, 3, 8)
    p6 = _linreg_problem(11, 16, 8, 32)
    return {
        "muon1": run(lambda p: optim.Muon(p, **MUON1), *p1, 1),
        "muon6": run(lambda p: optim.Muon(p, **MUON6), *p6, 6, mean=True),
        "lars5": run(lambda p: optim.LARS(p, **LARS5), *p1, 5),
    }


def gather_step_runs(Tensor, nn, TinyJit, Context):
    """W and Bp after STEPS2 steps of Bp + gather(X @ W, idx), per optimizer
    and with / without TinyJit, written once for thvm and tinygrad."""
    out = {}
    for opt_name in ("SGD", "Adam"):
        for jit in (False, True):
            rng = np.random.default_rng(3)
            W = Tensor((rng.standard_normal((5, 4)) * 0.5).astype(np.float32)).realize()
            Bp = Tensor((rng.standard_normal((9, 4)) * 0.1).astype(np.float32)).realize()
            Xn = rng.standard_normal((9, 5)).astype(np.float32)
            idx = np.repeat(np.arange(9)[:, None], 4, 1).astype(np.int32)
            with Context(TRAINING=1):
                opt = getattr(nn.optim, opt_name)([W, Bp], lr=0.05)

                def step(x, dst):
                    opt.zero_grad()
                    o = Bp + (x @ W).gather(0, dst)
                    loss = (o * o).mean()
                    loss.backward()
                    return loss.realize(*opt.schedule_step())

                f = TinyJit(step) if jit else step
                for _ in range(STEPS2):
                    f(Tensor(Xn).realize(), Tensor(idx).realize())
            key = f"{opt_name}-{'jit' if jit else 'eager'}"
            out[key + "-W"] = W.numpy()
            out[key + "-Bp"] = Bp.numpy()
    return out


def head_step_runs(Tensor, nn, TinyJit, Context):
    """W1 / W2 / Bp after STEPS2 steps of o = Bp + gather(x @ W1, idx) over
    repeated random ids, read by a head out = o @ W2 (three params) or
    out = o.sum(1, keepdim=True) (Bp's grad is then a broadcast of the
    loss grad), per optimizer and with / without TinyJit."""
    out = {}
    B, D, H = 16, 32, 64
    for head in ("W2", "sum"):
        for opt_name in ("SGD", "Adam"):
            for jit in (False, True):
                rng = np.random.default_rng(0)
                W1 = Tensor((rng.standard_normal((D, H)) * 0.1).astype(np.float32)).realize()
                W2 = Tensor((rng.standard_normal((H, 1)) * 0.1).astype(np.float32)).realize()
                Bp = Tensor((rng.standard_normal((B, H)) * 0.1).astype(np.float32)).realize()
                Wt = rng.standard_normal((D, 1)).astype(np.float32)
                params = [W1, W2, Bp] if head == "W2" else [W1, Bp]
                with Context(TRAINING=1):
                    opt = getattr(nn.optim, opt_name)(params, lr=0.01)

                    def step(x, y, dst):
                        opt.zero_grad()
                        o = Bp + (x @ W1).gather(0, dst)
                        o = o @ W2 if head == "W2" else o.sum(1, keepdim=True)
                        d = o - y
                        loss = (d * d).mean()
                        loss.backward()
                        return loss.realize(*opt.schedule_step())

                    f = TinyJit(step) if jit else step
                    for _ in range(STEPS2):
                        xn = rng.standard_normal((B, D)).astype(np.float32)
                        ids = rng.integers(0, B, B)
                        idx = np.repeat(ids[:, None], H, 1).astype(np.int32)
                        f(Tensor(xn).realize(), Tensor(xn @ Wt).realize(), Tensor(idx).realize())
                key = f"{head}-{opt_name}-{'jit' if jit else 'eager'}"
                for name, p in (("W1", W1), ("W2", W2), ("Bp", Bp)):
                    out[f"{key}-{name}"] = p.numpy()
    return out


_CHILD = r"""
import os
import numpy as np
import test_optim_step_gc as t
from test_muon_lars_parity import _thvm_run
from thvm import Context, Tensor, TinyJit, nn, optim
res = t.muon_lars_runs(_thvm_run, optim)
res.update(t.gather_step_runs(Tensor, nn, TinyJit, Context))
res.update(t.head_step_runs(Tensor, nn, TinyJit, Context))
np.savez(os.environ["OUT"], **res)
print("DONE", flush=True)
"""


def _child(gc_kb, walker, out):
    return helpers.run_child(_CHILD, 1800, OUT=out, THVM_GC_KB=None if gc_kb is None else str(gc_kb),
                             THVM_CPU_JIT_WARMUP="1000000000" if walker else "0")


@unittest.skipUnless(NATIVE and helpers.have_tinygrad(), "tinygrad parity on the CPU backend")
class TestOptimStepGC(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from test_muon_lars_parity import _tg_run
        from tinygrad import Context, Tensor, TinyJit, nn
        from tinygrad.nn import optim
        cls.want = muon_lars_runs(_tg_run, optim)
        cls.want.update(gather_step_runs(Tensor, nn, TinyJit, Context))
        cls.want.update(head_step_runs(Tensor, nn, TinyJit, Context))

    def _check(self, gc_kb, walker):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "res.npz")
            out = _child(gc_kb, walker, path)
            self.assertIn("DONE", out.stdout, f"child failed:\n{out.stdout}\n{out.stderr[-4000:]}")
            got = dict(np.load(path))
        self.assertNotIn("ran no dispatch rung", out.stderr)
        tol = {"muon1": 1e-4, "muon6": 1e-3, "lars5": 1e-3}
        for key, want in self.want.items():
            diff = float(np.abs(got[key] - want).max())
            self.assertLess(diff, tol.get(key, 1e-5),
                            f"{key} at THVM_GC_KB={gc_kb} walker={walker}: maxdiff {diff:.3e}")

    def test_gc_kb_1_default(self):
        self._check(1, False)

    def test_gc_kb_1_walker(self):
        self._check(1, True)

    def test_gc_kb_64_default(self):
        self._check(64, False)

    def test_gc_kb_64_walker(self):
        self._check(64, True)

    def test_gc_default_threshold_default(self):
        self._check(None, False)

    def test_gc_default_threshold_walker(self):
        self._check(None, True)


if __name__ == "__main__":
    unittest.main()
