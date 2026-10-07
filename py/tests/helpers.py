"""Shared py-test setup, after tinygrad's test/helpers.py.

Importing it puts THVM_PY (default: this repo's py/) first on sys.path, so a
test runs without PYTHONPATH.  run_child runs a snippet in a fresh interpreter
that can import thvm and the test modules.
"""
import os
import re
import subprocess
import sys

TESTS = os.path.dirname(os.path.abspath(__file__))
PY = os.environ.get("THVM_PY", os.path.dirname(TESTS))
sys.path.insert(0, PY)


def have_tinygrad():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


def run_child(code, timeout, *argv, **env):
    """Run `code` in a fresh interpreter; an env value of None unsets it."""
    full = dict(os.environ, PYTHONPATH=os.pathsep.join(
        [PY, TESTS] + [p for p in [os.environ.get("PYTHONPATH")] if p]))
    for k, v in env.items():
        if v is None:
            full.pop(k, None)
        else:
            full[k] = v
    return subprocess.run([sys.executable, "-c", code, *argv], env=full,
                          capture_output=True, text=True, timeout=timeout)


_ROUTE_CHILD = r"""
import importlib, sys, traceback
t = importlib.import_module(sys.argv[1])
for name, fn in t.ROUTE_CASES:
    try:
        fn()
        print(f"CASE {name} ok=True", flush=True)
    except Exception:
        print(f"CASE {name} ok=False " + traceback.format_exc().replace(chr(10), " | "), flush=True)
"""


class BothRoutes:
    """Mixin for a unittest.TestCase: run the module MODULE's ROUTE_CASES in a
    child process on the C JIT and again on the UOp walker."""

    def _run_routes(self, warmup):
        out = run_child(_ROUTE_CHILD, 1800, self.MODULE, DEV="cpu",
                        THVM_CPU_UOP_WALK_TRACE="1", THVM_CPU_JIT_WARMUP=warmup)
        lines = [ln for ln in out.stdout.splitlines() if ln.startswith("CASE ")]
        self.assertEqual(len(lines), len(self.ROUTE_CASES),
                         f"child failed:\n{out.stdout}\n{out.stderr[-4000:]}")
        self.assertEqual([ln for ln in lines if "ok=False" in ln], [])
        return out

    def test_c_jit_route(self):
        # Warmup 0 compiles each kernel on its first fire.
        out = self._run_routes("0")
        walked = re.findall(r"^uop_walk: ", out.stderr, re.M)
        self.assertEqual(walked, [], "a kernel fell to the walker: the JIT did not run")

    def test_uop_walker_route(self):
        # A warmup the JIT never reaches: every kernel runs on the UOp walker.
        out = self._run_routes("1000000000")
        walked = re.findall(r"^uop_walk: n_inputs=\d+ root_op=\d+ rc=(\d)$", out.stderr, re.M)
        self.assertGreater(len(walked), len(self.ROUTE_CASES), "the walker did not execute the kernels")
        self.assertEqual(set(walked), {"1"})
