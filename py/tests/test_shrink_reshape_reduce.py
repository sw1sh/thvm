"""A reduce over a RESHAPE of a SHRINK must read the shrunk elements on both CPU paths.

`x.shrink(((0,2),(0,3),(1,3))).reshape(2,6).sum(1)` fuses the movement chain
into the reduce kernel.  The unified rangeify pass composes SHRINK + RESHAPE
into the load address over x's raw buffer, while visit() binds the kernel
input to the view the same chain resolves to.  The input_chain_composed slot
flag ("the address is already composed, bind the raw buffer") has to be set
whenever that kernel runs.  Withheld, the dispatcher gathers the view into a
contiguous temp and the kernel applies the chain a second time (the CPU JIT
read 12 elements with a 24-element index), and the UOp walker biases the
pointer by the view offset, so on that path even `x.shrink(...) * 2` read
shifted elements.

The grid {shrink axis} x {merge it with its outer / inner neighbour} x
{sum the merged / the other axis} x {head / interior shrink} x {two shapes}
runs through the default CPU JIT, and again in a child process whose JIT
never compiles, so the walker executes every kernel.

With BEAM=2 each kernel is first benched on scratch buffers.  A composed
slot is read up to its base's extent, and a kernel that reads two views of
one buffer (x[:, :64] + x[:, 64:], x[0] + x[1], a GLU split) loads both
through the first view's slot, whether that slot is composed or, like x[0],
a contiguous view at offset 0.  So that slot's scratch must hold the whole
buffer, not the view's numel.  The BEAM child reports each scratch it
allocates (THVM_AUTOTUNE_TRACE); the test checks that some slot's scratch
holds the whole leaf buffer, that the search ran and every candidate passed
its correctness gate, and that BEAM=2 leaves each result as the default
path computes it, bit for bit where no opt can reorder a sum.  On macOS the
child also runs under Guard Malloc, where a read past a scratch buffer
faults.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_shrink_reshape_reduce.py -q
"""
import itertools
import os
import pathlib
import re
import subprocess
import sys
import tempfile
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


def _on(xa, build):
    return lambda: build(Tensor(xa))


def _beam_cases():
    """name -> (build, numpy reference, element count of the leaf buffer
    the kernel reads)."""
    xm = _data((2, 128))
    xs = _data((2, 4, 8))
    xd = _data((4, 33))
    xg = _data((4, 64))
    xr = _data((4, 2, 32))
    xf = _data((33,))
    return {
        # The view is (2,64) at offset 64: the kernel reads xm up to its
        # last element, twice the view's numel.
        "shrink-mul": (
            _on(xm, lambda x: x.shrink(((0, 2), (64, 128))) * 2),
            xm[:, 64:] * 2, xm.size),
        # A two-view chain whose address spans all 64 elements of xs.
        "shrink-merge-sum": (
            _on(xs, lambda x: x.shrink(((0, 2), (0, 4), (4, 8)))
                .reshape(2, 16).sum(axis=1)),
            xs[:, :, 4:].reshape(2, 16).sum(1), xs.size),
        # Two views of one buffer load through the first operand's slot.
        # With the lower view first, that slot's own view stops short of
        # the buffer's end.
        "halves-add": (
            _on(xm, lambda x: x[:, :64] + x[:, 64:]),
            xm[:, :64] + xm[:, 64:], xm.size),
        "halves-add-rev": (
            _on(xm, lambda x: x[:, 64:] + x[:, :64]),
            xm[:, 64:] + xm[:, :64], xm.size),
        "diff": (
            _on(xd, lambda x: x[:, :-1] - x[:, 1:]),
            xd[:, :-1] - xd[:, 1:], xd.size),
        "diff-rev": (
            _on(xd, lambda x: x[:, 1:] - x[:, :-1]),
            xd[:, 1:] - xd[:, :-1], xd.size),
        "glu-halves": (
            _on(xg, lambda x: x[:, :32] * x[:, 32:]),
            xg[:, :32] * xg[:, 32:], xg.size),
        "glu-halves-rev": (
            _on(xg, lambda x: x[:, 32:] * x[:, :32]),
            xg[:, 32:] * xg[:, :32], xg.size),
        # x[0] is contiguous at offset 0, so its slot is not composed, and
        # x[1]'s loads land on it too.
        "rows-add": (
            _on(xm, lambda x: x[0] + x[1]), xm[0] + xm[1], xm.size),
        "rows-add-rev": (
            _on(xm, lambda x: x[1] + x[0]), xm[1] + xm[0], xm.size),
        "rows-mul": (
            _on(xr, lambda x: x[0] * x[1]), xr[0] * xr[1], xr.size),
        "rows-dot": (
            _on(xm, lambda x: (x[0] * x[1]).sum()), (xm[0] * xm[1]).sum(),
            xm.size),
        # The reshape makes x[:32] contiguous at offset 0, one element short
        # of the buffer.
        "diff-flat": (
            _on(xf, lambda x: x[:32].reshape(4, 8) - x[1:].reshape(4, 8)),
            xf[:32].reshape(4, 8) - xf[1:].reshape(4, 8), xf.size),
    }


def run_beam_case(name):
    build, want, _ = _beam_cases()[name]
    return build().numpy(), want


BEAM = list(_beam_cases())
# A winning opt may sum a reduce in another order; the rest compute each
# element the same way under every opt.
BEAM_REORDERS = {"shrink-merge-sum", "rows-dot"}


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
    print(f"BEGIN {name}", file=sys.stderr, flush=True)
    got, want = t.run_beam_case(name)
    np.save(os.path.join(os.environ["THVM_BEAM_OUT"], name + ".npy"), got)
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


def _trace_by_case(stderr):
    """The child's stderr lines grouped under the case whose BEGIN marker
    precedes them."""
    sections, name = {}, None
    for line in stderr.splitlines():
        if line.startswith("BEGIN "):
            name = line[len("BEGIN "):]
            sections[name] = []
        elif name is not None:
            sections[name].append(line)
    return sections


def _iso_slots(lines):
    """The fields of each `[iso]` line that reports an allocated scratch."""
    return [{k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", l)}
            for l in lines if l.startswith("[iso] ") and " scratch=" in l]


class TestShrinkBeamIsolatedBench(unittest.TestCase):
    def test_beam_bench_stays_inside_scratch(self):
        # AUTOTUNE_CACHE=0: no cached winner may skip the search.
        with tempfile.TemporaryDirectory() as out_dir:
            env = _guard_malloc(dict(
                os.environ, DEV="cpu", BEAM="2", AUTOTUNE_CACHE="0",
                THVM_AUTOTUNE_TRACE="1", THVM_BEAM_OUT=out_dir,
                THVM_PY=os.environ.get("THVM_PY", str(ROOT / "py")),
                THVM_TESTS=str(pathlib.Path(__file__).resolve().parent)))
            out = subprocess.run([sys.executable, "-c", _BEAM_CHILD], env=env,
                                 capture_output=True, text=True, timeout=600)
            self.assertEqual(out.returncode, 0,
                             f"child failed:\n{out.stdout}\n{out.stderr[-4000:]}")
            lines = [l for l in out.stdout.splitlines() if l.startswith("CASE ")]
            self.assertEqual(len(lines), len(BEAM), out.stdout)
            self.assertEqual([l for l in lines if "ok=False" in l], [])
            beam = {name: np.load(os.path.join(out_dir, name + ".npy"))
                    for name in BEAM}
        cases, traces, problems = _beam_cases(), _trace_by_case(out.stderr), []
        for name in BEAM:
            trace = traces.get(name, [])
            # The default path (no BEAM in this process) computes the same
            # values, bit for bit where no opt can reorder a sum.
            default = run_beam_case(name)[0]
            same = (np.allclose(beam[name], default, rtol=1e-5, atol=1e-5)
                    if name in BEAM_REORDERS
                    else np.array_equal(beam[name], default))
            if not same:
                problems.append(f"{name}: BEAM=2 changed the result")
            if not [l for l in trace if l.startswith("[cand]")]:
                problems.append(f"{name}: the isolated search declined")
            if [l for l in trace if "NAN-REJECT" in l]:
                problems.append(f"{name}: a candidate failed the gate")
            # The slot the leaf's loads land on, composed or not, holds the
            # whole leaf buffer; the other slots hold broadcast constants.
            whole = cases[name][2]
            scratch = [s["scratch"] for s in _iso_slots(trace)]
            if not scratch or max(scratch) < whole:
                problems.append(f"{name}: scratch {scratch} is short of the "
                                f"{whole}-element leaf buffer")
        self.assertEqual(problems, [])
        # The scratch holds finite values, so a non-finite baseline means the
        # bench read past it.
        self.assertNotIn("finite=0 /", out.stderr)


if __name__ == "__main__":
    unittest.main()
