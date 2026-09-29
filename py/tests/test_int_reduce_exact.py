"""An int32 SUM or MAX is exact on both CPU paths.

tinygrad keeps an int reduce in its dtype: reduce_ranges_to_acc allocates the
accumulator like the reduce (tinygrad/codegen/__init__.py:197-204) and starts
it at identity_element (uop/ops.py:61), 0 for SUM and dt.min for MAX
(dtype.py:70-72).  thvm's C renderer declared every reduce accumulator
`float`, so on the CPU JIT an int32 past 2^24 rounded: the max of
[16777217, 1, 2, 3] came back as 16777216.

Every case runs in a child process twice: on the default path, where the CPU
JIT compiles each kernel, and on a walker path, where the JIT never compiles
and the UOp walker executes every kernel.  The values are odd and past 2^24,
which an f32 accumulator cannot hold; numpy's int64 sum and max are the
reference.  The 37-wide rows are not unrolled, so hand_opts UPCASTs their
output axis into parallel accumulators.  When tinygrad is importable it must
agree with the reference too.

An int32 true division stays int32 in thvm (tinygrad promotes it to float), so
`(a / b).sum()` reduces a float body into an int accumulator.  The rendered C
adds each quotient in float and truncates the sum back into the accumulator,
and a store truncates the quotient; the walker must compute the same values.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_int_reduce_exact.py -q
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


def _cases():
    rng = np.random.default_rng(0)
    wide = (16777217 + 2 * rng.integers(0, 1 << 20, size=(4, 37))) \
        * np.where(rng.random((4, 37)) < 0.3, -1, 1)
    grid = np.array([[-100000001 - 16 * r - 3 * c for c in range(4)]
                     for r in range(4)])
    past = np.array([16777217, 1, 2, 3])
    return [
        ("max-past-2p24", past, "max", None),
        ("sum-past-2p24", past, "sum", None),
        ("4x4-max-axis1", grid, "max", 1),
        ("4x37-max-axis1", wide, "max", 1),
        ("4x37-sum-axis1", wide, "sum", 1),
        ("37x4-max-axis0", np.ascontiguousarray(wide.T), "max", 0),
        ("max-of-int32-min", np.full(5, -2**31), "max", None),
    ]


CASES = _cases()

_A = np.array([[-7, 5, 7, 8], [7, 9, 11, 13], [100, -3, 17, 2], [1, 2, 3, 4]])
_B = np.array([[2, 2, 2, 4], [2, 2, 2, 4], [8, 2, 4, 2], [4, 4, 4, 4]])
FLOAT_BODY_CASES = ["div-sum", "div", "sqrt-sum"]


def run_float_body(name):
    """thvm's int32 result for an int reduce or store of a float body."""
    a, b = Tensor(_A.astype(np.int32)), Tensor(_B.astype(np.int32))
    if name == "div-sum":
        return (a / b).sum(axis=1).numpy()
    if name == "div":
        return (a / b).numpy()
    return Tensor(np.abs(_A).astype(np.int32)).sqrt().sum(axis=1).numpy()


def float_body_reference(name):
    """`acc = acc + v` per element, truncated toward zero, as the C runs it."""
    if name == "div":
        return np.trunc(_A / _B).astype(np.int32)
    rows = _A / _B if name == "div-sum" else np.sqrt(np.abs(_A))
    out = []
    for row in rows:
        acc = 0
        for v in row:
            acc = int(acc + v)
        out.append(acc)
    return np.array(out, dtype=np.int32)


def reference(arr, op, axis):
    a = arr.astype(np.int64)
    return (a.max(axis=axis) if op == "max" else a.sum(axis=axis)).astype(np.int32)


def run_case(arr, op, axis):
    """thvm's int32 result for the reduce."""
    t = Tensor(arr.astype(np.int32))
    return (t.max(axis=axis) if op == "max" else t.sum(axis=axis)).numpy()


_CHILD = r"""
import os, sys
sys.path.insert(0, os.environ["THVM_PY"])
sys.path.insert(0, os.environ["THVM_TESTS"])
import numpy as np
import test_int_reduce_exact as t
for name, arr, op, axis in t.CASES:
    got = np.asarray(t.run_case(arr, op, axis))
    want = t.reference(arr, op, axis)
    ok = got.dtype == np.int32 and got.shape == want.shape and bool((got == want).all())
    print(f"CASE {name} ok={ok} got={got.tolist()} want={want.tolist()}", flush=True)
for name in t.FLOAT_BODY_CASES:
    got = np.asarray(t.run_float_body(name))
    want = t.float_body_reference(name)
    ok = got.dtype == np.int32 and got.shape == want.shape and bool((got == want).all())
    print(f"CASE {name} ok={ok} got={got.tolist()} want={want.tolist()}", flush=True)
"""


def _run_child(extra_env):
    env = dict(os.environ, DEV="cpu",
               THVM_PY=os.environ.get("THVM_PY", str(ROOT / "py")),
               THVM_TESTS=str(pathlib.Path(__file__).resolve().parent),
               THVM_CPU_UOP_WALK_TRACE="1", **extra_env)
    return subprocess.run([sys.executable, "-c", _CHILD], env=env,
                          capture_output=True, text=True, timeout=600)


class TestIntReduceExact(unittest.TestCase):
    def _check_child(self, out):
        lines = [l for l in out.stdout.splitlines() if l.startswith("CASE ")]
        self.assertEqual(len(lines), len(CASES) + len(FLOAT_BODY_CASES),
                         f"child failed:\n{out.stdout}\n{out.stderr[-4000:]}")
        bad = [l for l in lines if "ok=False" in l]
        self.assertEqual(bad, [], "an int32 reduce is not exact")

    def test_default_path(self):
        out = _run_child({})
        self._check_child(out)
        walked = re.findall(r"^uop_walk: ", out.stderr, re.M)
        self.assertEqual(walked, [], "a kernel fell to the walker: the JIT did not run")

    def test_uop_walker(self):
        # A warmup the JIT never reaches, no batched precompile, and an opt
        # tag no cached dylib carries: every kernel falls to the UOp walker.
        out = _run_child({"THVM_CPU_JIT_WARMUP": "1000000000",
                          "THVM_CPU_JIT_PARALLEL": "0",
                          "THVM_CPU_JIT_OPT": "O0"})
        self._check_child(out)
        walked = re.findall(r"^uop_walk: n_inputs=\d+ root_op=\d+ rc=(\d)$",
                            out.stderr, re.M)
        self.assertGreaterEqual(len(walked), len(CASES),
                                "the walker did not execute the kernels")
        self.assertEqual(set(walked), {"1"})

    def test_tinygrad_agrees(self):
        try:
            from tinygrad import Tensor as TgTensor, dtypes as tg_dtypes
        except Exception:
            self.skipTest("tinygrad is not importable")
        for name, arr, op, axis in CASES:
            t = TgTensor(arr.astype(np.int32), dtype=tg_dtypes.int32)
            got = (t.max(axis=axis) if op == "max" else t.sum(axis=axis)).numpy()
            np.testing.assert_array_equal(got.astype(np.int64),
                                          reference(arr, op, axis).astype(np.int64),
                                          err_msg=name)


if __name__ == "__main__":
    unittest.main()
