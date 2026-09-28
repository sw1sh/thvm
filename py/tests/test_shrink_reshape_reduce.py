"""A reduce over a RESHAPE of a SHRINK must read the shrunk elements on both CPU paths.

`x.shrink(((0,2),(0,3),(1,3))).reshape(2,6).sum(1)` fuses the movement chain
into the reduce kernel.  The unified rangeify pass composes SHRINK + RESHAPE
into the load address over x's raw buffer, while visit() binds the kernel
input to the view the same chain resolves to.  The input_chain_composed slot
flag ("the address is already composed, bind the raw buffer") has to be set
whenever that kernel runs.  Withheld (a diagnostic gate trips whenever a
composed address runs past the view's element count, as a shrunk base's
always does), the dispatcher gathers the view into a contiguous temp and the
kernel applies the chain a second time (the CPU JIT read 12 elements with a
24-element index), and the UOp walker biases the pointer by the view offset,
so on that path even `x.shrink(...) * 2` read shifted elements.

The grid {shrink axis} x {merge it with its outer / inner neighbour} x
{sum the merged / the other axis} x {head / interior shrink} x {two shapes}
runs through the default CPU JIT, and again in a child process whose JIT
never compiles, so the walker executes every kernel.

With BEAM=2 each kernel is first benched on scratch buffers.  A composed
slot is read up to its base's extent, so its scratch must cover that
extent, not the view's numel.  The BEAM child runs under macOS Guard Malloc
when it loads, where a read past a scratch buffer faults.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_shrink_reshape_reduce.py -q
"""
import itertools
import os
import pathlib
import subprocess
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

from thvm import Tensor  # noqa: E402

SHAPES = ((2, 3, 4), (3, 5, 7))
# (shrunk axis, merged pair): the outer neighbour merge is (axis-1, axis),
# the inner one is (axis, axis+1).
MERGES = ((0, (0, 1)), (1, (0, 1)), (1, (1, 2)), (2, (1, 2)))
# An interior shrink needs at least 3 elements on the shrunk axis to keep one.
GRID = [(shape, merge, reduce_on, kind)
        for shape, merge, reduce_on, kind in itertools.product(
            SHAPES, MERGES, ("merged", "other"), ("head", "interior"))
        if kind == "head" or shape[merge[0]] >= 3]


def _data(shape):
    return np.random.default_rng(1).standard_normal(shape).astype(np.float32)


def _bounds(shape, axis, kind):
    """(begin, end) per axis: drop the first element of `axis` ("head"), or
    the first and the last ("interior")."""
    hi = shape[axis] - 1 if kind == "interior" else shape[axis]
    return tuple((1, hi) if a == axis else (0, d) for a, d in enumerate(shape))


def _merged_shape(shape, pair):
    lo, hi = pair
    return shape[:lo] + (shape[lo] * shape[hi],) + shape[hi + 1:]


def run_grid_case(shape, merge, reduce_on, kind):
    axis, pair = merge
    xa = _data(shape)
    bounds = _bounds(shape, axis, kind)
    ref = xa[tuple(slice(b, e) for b, e in bounds)]
    new_shape = _merged_shape(ref.shape, pair)
    red = pair[0] if reduce_on == "merged" else 1 - pair[0]
    got = Tensor(xa).shrink(bounds).reshape(new_shape).sum(axis=red).numpy()
    return got, ref.reshape(new_shape).sum(axis=red)


def _center(t):
    return t - t.sum(axis=1, keepdim=True)


def _named_cases():
    xa = _data((2, 3, 4))
    ref = xa[:, :, 1:3]
    inner = ((0, 2), (0, 3), (1, 3))
    x = lambda: Tensor(xa)  # noqa: E731
    return {
        "merge-sum-last": (
            lambda: x().shrink(inner).reshape(2, 6).sum(axis=1),
            ref.reshape(2, 6).sum(1)),
        "merge-sum-first": (
            lambda: x().shrink(inner).reshape(2, 6).sum(axis=0),
            ref.reshape(2, 6).sum(0)),
        "merge-mul-sum": (
            lambda: (x().shrink(inner).reshape(2, 6) * 2).sum(axis=1),
            (ref.reshape(2, 6) * 2).sum(1)),
        "merge-center": (
            lambda: _center(x().shrink(inner).reshape(2, 6)),
            ref.reshape(2, 6) - ref.reshape(2, 6).sum(1, keepdims=True)),
        "merge-no-reduce": (
            lambda: x().shrink(inner).reshape(2, 6), ref.reshape(2, 6)),
        "shrink-sum": (
            lambda: x().shrink(inner).sum((1, 2)), ref.sum((1, 2))),
        "shrink-mul": (lambda: x().shrink(inner) * 2, ref * 2),
        "shrink-middle-merge-sum": (
            lambda: x().shrink(((0, 2), (1, 3), (0, 4))).reshape(2, 8).sum(1),
            xa[:, 1:3, :].reshape(2, 8).sum(1)),
        "reshape-sum-no-shrink": (
            lambda: x().reshape(2, 12).sum(1), xa.reshape(2, 12).sum(1)),
        "contiguous-merge-sum": (
            lambda: x().shrink(inner).contiguous().reshape(2, 6).sum(1),
            ref.reshape(2, 6).sum(1)),
    }


def run_named_case(name):
    build, want = _named_cases()[name]
    return build().numpy(), want


NAMED = list(_named_cases())


def _beam_cases():
    xm = _data((2, 128))
    xs = _data((2, 4, 8))
    return {
        # The view is (2,64) at offset 64: the kernel reads xm up to its
        # last element, twice the view's numel.
        "shrink-mul": (
            lambda: Tensor(xm).shrink(((0, 2), (64, 128))) * 2,
            xm[:, 64:] * 2),
        # A two-view chain whose address spans all 64 elements of xs.
        "shrink-merge-sum": (
            lambda: Tensor(xs).shrink(((0, 2), (0, 4), (4, 8)))
            .reshape(2, 16).sum(axis=1),
            xs[:, :, 4:].reshape(2, 16).sum(1)),
    }


def run_beam_case(name):
    build, want = _beam_cases()[name]
    return build().numpy(), want


BEAM = list(_beam_cases())


class TestShrinkReshapeReduce(unittest.TestCase):
    def test_named_default_path(self):
        for name in NAMED:
            with self.subTest(case=name):
                got, want = run_named_case(name)
                np.testing.assert_allclose(got, want, rtol=1e-5, atol=1e-5)

    def test_grid_default_path(self):
        for case in GRID:
            with self.subTest(case=case):
                got, want = run_grid_case(*case)
                np.testing.assert_allclose(got, want, rtol=1e-5, atol=1e-5)


_CHILD = r"""
import os, sys
sys.path.insert(0, os.environ["THVM_PY"])
sys.path.insert(0, os.environ["THVM_TESTS"])
import numpy as np
import test_shrink_reshape_reduce as t
cases = [("named", n) for n in t.NAMED] + [("grid", c) for c in t.GRID]
for kind, case in cases:
    got, want = t.run_named_case(case) if kind == "named" else t.run_grid_case(*case)
    err = float(np.abs(np.asarray(got) - want).max())
    ok = bool(np.allclose(got, want, rtol=1e-5, atol=1e-5))
    print(f"CASE {case} ok={ok} maxerr={err:.6g}", flush=True)
"""


class TestShrinkReshapeReduceWalker(unittest.TestCase):
    def test_uop_walker(self):
        # A warmup the JIT never reaches, no batched precompile, and an opt
        # tag no cached dylib carries: every kernel falls to the UOp walker.
        env = dict(os.environ, DEV="cpu",
                   THVM_PY=os.environ.get("THVM_PY", str(ROOT / "py")),
                   THVM_TESTS=str(pathlib.Path(__file__).resolve().parent),
                   THVM_CPU_JIT_WARMUP="1000000000", THVM_CPU_JIT_PARALLEL="0",
                   THVM_CPU_JIT_OPT="O0", THVM_CPU_UOP_WALK_TRACE="1")
        out = subprocess.run([sys.executable, "-c", _CHILD], env=env,
                             capture_output=True, text=True, timeout=300)
        lines = [l for l in out.stdout.splitlines() if l.startswith("CASE ")]
        self.assertEqual(len(lines), len(NAMED) + len(GRID),
                         f"child failed:\n{out.stdout}\n{out.stderr}")
        walked = [l for l in out.stderr.splitlines() if l.startswith("uop_walk:")]
        self.assertGreaterEqual(len(walked), len(GRID),
                                "the walker did not execute the kernels")
        self.assertTrue(all(l.endswith("rc=1") for l in walked), "\n".join(walked))
        bad = [l for l in lines if "ok=False" in l]
        self.assertEqual(bad, [], "walker path diverges from numpy")


_BEAM_CHILD = r"""
import os, sys
sys.path.insert(0, os.environ["THVM_PY"])
sys.path.insert(0, os.environ["THVM_TESTS"])
import numpy as np
import test_shrink_reshape_reduce as t
for name in t.BEAM:
    got, want = t.run_beam_case(name)
    err = float(np.abs(np.asarray(got) - want).max())
    ok = bool(np.allclose(got, want, rtol=1e-5, atol=1e-5))
    print(f"CASE {name} ok={ok} maxerr={err:.6g}", flush=True)
"""


def _guard_malloc(env):
    """`env` with macOS Guard Malloc inserted, which faults on a read past
    the end of a malloc'd block; `env` unchanged where it does not load."""
    if sys.platform != "darwin":
        return env
    guarded = dict(env, DYLD_INSERT_LIBRARIES="/usr/lib/libgmalloc.dylib")
    probe = subprocess.run([sys.executable, "-c", "pass"], env=guarded,
                           capture_output=True, timeout=120)
    return guarded if probe.returncode == 0 else env


class TestShrinkBeamIsolatedBench(unittest.TestCase):
    def test_beam_bench_stays_inside_scratch(self):
        # AUTOTUNE_CACHE=0: no cached winner may skip the search.
        env = _guard_malloc(dict(
            os.environ, DEV="cpu", BEAM="2", AUTOTUNE_CACHE="0",
            THVM_AUTOTUNE_TRACE="1",
            THVM_PY=os.environ.get("THVM_PY", str(ROOT / "py")),
            THVM_TESTS=str(pathlib.Path(__file__).resolve().parent)))
        out = subprocess.run([sys.executable, "-c", _BEAM_CHILD], env=env,
                             capture_output=True, text=True, timeout=600)
        self.assertEqual(out.returncode, 0,
                         f"child failed:\n{out.stdout}\n{out.stderr[-4000:]}")
        lines = [l for l in out.stdout.splitlines() if l.startswith("CASE ")]
        self.assertEqual(len(lines), len(BEAM), out.stdout)
        self.assertEqual([l for l in lines if "ok=False" in l], [])
        cands = [l for l in out.stderr.splitlines() if l.startswith("[cand]")]
        self.assertGreaterEqual(len(cands), len(BEAM),
                                "the isolated search declined the kernels")
        # The scratch holds finite values, so a non-finite baseline means the
        # bench read past it.
        self.assertNotIn("finite=0 /", out.stderr)


if __name__ == "__main__":
    unittest.main()
