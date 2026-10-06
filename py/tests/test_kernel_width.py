"""A kernel that reads more than 64 distinct buffers must compute on CPU.

tinygrad's CPU takes any number of kernel buffers: DEVICE_MAX_BUFS lists only
WEBGPU (tinygrad/schedule/rangeify.py:170) and the CPU runtime passes one
pointer per buffer (tinygrad/runtime/ops_cpu.py:72).  thvm lifted at most 64
inputs into a kernel, so a sum of 65 or more distinct tensors lifted nothing:
every CPU dispatch rung declined and the output kept its zero fill.

Every sum here fuses into one kernel with one input per tensor.  Each mode
runs in a child process with the walker-decline trace on: the default path,
where the CPU JIT compiles the kernel, and a walker path, where the JIT
never compiles and the UOp walker executes every kernel.  The parent checks
the values, that the widest kernel really took every tensor as an input,
and that no trace reports a declined lift or a dispatch that ran nothing.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_kernel_width.py -q
"""
import os
import pathlib
import re
import subprocess
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

from thvm import Tensor  # noqa: E402

LEFT_DEEP = (60, 63, 64, 65, 66, 70, 130, 300)
BALANCED = (70, 130)
CASES = [("left", n) for n in LEFT_DEEP] + [("tree", n) for n in BALANCED]
WIDEST = max(LEFT_DEEP)

# Traces that mean a kernel ran without its lift, or ran nothing at all.
FORBIDDEN = ("lift_declined", "ran no dispatch rung")


def _arrays(n):
    rng = np.random.default_rng(n)
    return [rng.standard_normal(16).astype(np.float32) for _ in range(n)]


def run_case(kind, n):
    """(thvm result, float64 numpy reference) for the sum of n tensors."""
    arrs = _arrays(n)
    level = [Tensor(a) for a in arrs]
    if kind == "left":
        acc = level[0]
        for t in level[1:]:
            acc = acc + t
    else:
        while len(level) > 1:
            pairs = [level[i] + level[i + 1] for i in range(0, len(level) - 1, 2)]
            level = pairs + level[len(pairs) * 2:]
        acc = level[0]
    return acc.numpy(), np.sum(np.asarray(arrs, dtype=np.float64), axis=0)


_CHILD = r"""
import os, sys
sys.path.insert(0, os.environ["THVM_PY"])
sys.path.insert(0, os.environ["THVM_TESTS"])
import numpy as np
import test_kernel_width as t
for kind, n in t.CASES:
    got, want = t.run_case(kind, n)
    err = float(np.abs(np.asarray(got, dtype=np.float64) - want).max())
    ok = bool(err <= 1e-4 * n)
    print(f"CASE {kind} {n} ok={ok} maxerr={err:.6g}", flush=True)
"""


def _run_child(extra_env):
    env = dict(os.environ, DEV="cpu",
               THVM_PY=os.environ.get("THVM_PY", str(ROOT / "py")),
               THVM_TESTS=str(pathlib.Path(__file__).resolve().parent),
               THVM_TRACE_UOP_WALK_DECLINE="1", **extra_env)
    return subprocess.run([sys.executable, "-c", _CHILD], env=env,
                          capture_output=True, text=True, timeout=600)


class TestKernelWidth(unittest.TestCase):
    def _check_child(self, out):
        lines = [l for l in out.stdout.splitlines() if l.startswith("CASE ")]
        self.assertEqual(len(lines), len(CASES),
                         f"child failed:\n{out.stdout}\n{out.stderr[-4000:]}")
        bad = [l for l in lines if "ok=False" in l]
        self.assertEqual(bad, [], "a wide sum diverges from numpy")
        hits = [l for l in out.stderr.splitlines()
                if any(marker in l for marker in FORBIDDEN)]
        self.assertEqual(hits[:10], [], "a kernel ran without its lift")

    def test_default_path(self):
        # THVM_DUMP_KERNEL_SRC=0 prints one line per JIT-rendered kernel
        # with its input count.
        out = _run_child({"THVM_DUMP_KERNEL_SRC": "0"})
        self._check_child(out)
        widths = [int(m.group(1)) for m in
                  re.finditer(r"^\[kdump\] kid=\d+ key=\w+ n_in=(\d+)", out.stderr, re.M)]
        self.assertIn(WIDEST, widths, "the widest sum was not one JIT kernel")

    def test_uop_walker(self):
        # A warmup the JIT never reaches, no batched precompile, and an opt
        # tag no cached dylib carries: every kernel falls to the UOp walker.
        out = _run_child({"THVM_CPU_JIT_WARMUP": "1000000000",
                          "THVM_CPU_JIT_PARALLEL": "0",
                          "THVM_CPU_JIT_OPT": "O0",
                          "THVM_CPU_UOP_WALK_TRACE": "1"})
        self._check_child(out)
        walked = re.findall(r"^uop_walk: n_inputs=(\d+) root_op=\d+ rc=(\d)$",
                            out.stderr, re.M)
        self.assertIn((str(WIDEST), "1"), walked,
                      "the walker did not execute the widest sum")


if __name__ == "__main__":
    unittest.main()
