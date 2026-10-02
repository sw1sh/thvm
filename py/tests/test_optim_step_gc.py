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
import pathlib
import subprocess
import sys
import tempfile
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from thvm import Device  # noqa: E402

NATIVE = Device.DEFAULT == "CPU"

MUON1 = dict(lr=0.02, momentum=0.95, weight_decay=0.1, nesterov=True)
MUON6 = dict(lr=0.005, momentum=0.9, weight_decay=0.0, nesterov=True)
LARS5 = dict(lr=0.01, momentum=0.9, weight_decay=1e-4, tcoef=0.001, nesterov=False)
STEPS2 = 3


def _have_tg():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


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


_CHILD = r"""
import os, sys
import numpy as np
sys.path.insert(0, os.environ["THVM_PY"])
sys.path.insert(0, os.environ["THVM_TESTS"])
import test_optim_step_gc as t
from test_muon_lars_parity import _thvm_run
from thvm import Context, Tensor, TinyJit, nn, optim
res = t.muon_lars_runs(_thvm_run, optim)
res.update(t.gather_step_runs(Tensor, nn, TinyJit, Context))
np.savez(os.environ["OUT"], **res)
print("DONE", flush=True)
"""


def _child(gc_kb, walker, out):
    env = dict(os.environ, THVM_PY=str(ROOT / "py"),
               THVM_TESTS=str(pathlib.Path(__file__).resolve().parent), OUT=out,
               THVM_CPU_JIT_WARMUP="1000000000" if walker else "0")
    env.pop("THVM_GC_KB", None)
    if gc_kb is not None:
        env["THVM_GC_KB"] = str(gc_kb)
    return subprocess.run([sys.executable, "-c", _CHILD], env=env,
                          capture_output=True, text=True, timeout=1800)


@unittest.skipUnless(NATIVE and _have_tg(), "tinygrad parity on the CPU backend")
class TestOptimStepGC(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from test_muon_lars_parity import _tg_run
        from tinygrad import Context, Tensor, TinyJit, nn
        from tinygrad.nn import optim
        from _tinygrad_compat import tg_muon_decays
        cls.want = muon_lars_runs(_tg_run, optim)
        cls.want.update(gather_step_runs(Tensor, nn, TinyJit, Context))
        cls.muon_decays = tg_muon_decays()

    def _check(self, gc_kb, walker):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "res.npz")
            out = _child(gc_kb, walker, path)
            self.assertIn("DONE", out.stdout, f"child failed:\n{out.stdout}\n{out.stderr[-4000:]}")
            got = dict(np.load(path))
        self.assertNotIn("ran no dispatch rung", out.stderr)
        tol = {"muon1": 1e-4, "muon6": 1e-3, "lars5": 1e-3}
        for key, want in self.want.items():
            if key == "muon1" and not self.muon_decays:
                continue
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
