"""A reduce broadcast back into a second reduce must match numpy on both CPU paths.

`(W - W.sum((0,2,3)).reshape(1,2,1,1)).sum((0,2,3)) / 36` fuses both reduces
into one kernel.  hand_opts then fully UNROLLs the inner reduce's two extent-3
axes.  uop_dag_apply_split leaves REDUCE.axes naming only the extent-1 outer
half of a split axis, and the unrolled inner half sits inside BOTH reduces'
bodies.  The renderer opened its loop in both, so the outer SUM re-accumulated
the inner reduce once per unrolled lane: 9x the true value.  The UOp walker
never folded an inner half into a reduce at all; it iterated it as a store
loop and kept the last write.  That walker path is how BatchNorm-train
gradients (reduce + scalar-epilogue kernels the JIT declines) came out at the
~1e4 dropped-term scale in test_composite_reduce_bwd_parity.

The grid {NEG before/after the EXPAND} x {scalar-const / buffer / no epilogue}
x {final broadcast or not} runs through the default CPU JIT, and again in a
child process whose JIT never compiles, so the walker executes every kernel.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_nested_reduce_broadcast.py -q
"""
import itertools
import unittest

import numpy as np

import helpers
from thvm import K, Tensor
from thvm.dtypes import dtypes
from thvm.tensor import _TH
from thvm.thvm import Term

SHAPE = (4, 2, 3, 3)
CASES = list(itertools.product((True, False), ("const", "buffer", "none"),
                               (True, False)))


def _red(t):
    """Sum over axes (0, 2, 3) of a (4, 2, 3, 3) term -> (2,)."""
    return _TH.reshape(_TH.reduce(K.REDUCE_SUM, (0, 2, 3), t), (2,))


def run_case(neg_before, scale, final_expand):
    """Build one variant from raw UOps; return (thvm result, numpy reference)."""
    rng = np.random.default_rng(0)
    w = rng.standard_normal(SHAPE).astype(np.float32)
    s = rng.standard_normal((2,)).astype(np.float32)
    # The leaf Tensors stay alive while their raw terms are in use: a Tensor
    # allocation may reclaim the buffer of a Tensor already gone.
    tw, ts = Tensor(w), Tensor(s)
    x = int(tw.term)
    mean = _TH.reshape(_red(x), (1, 2, 1, 1))
    if neg_before:
        m = _TH.expand(_TH.neg(mean), SHAPE)
    else:
        m = _TH.neg(_TH.expand(mean, SHAPE))
    r = _red(_TH.add(x, m))
    want = (w - w.sum(axis=(0, 2, 3)).reshape(1, 2, 1, 1)).sum(axis=(0, 2, 3))
    if scale == "const":
        r = _TH.mul(r, _TH.expand(_TH.reshape(_TH.fconst(1 / 36), (1,)), (2,)))
        want = want * np.float32(1 / 36)
    elif scale == "buffer":
        r = _TH.mul(r, int(ts.term))
        want = want * s
    shape = (2,)
    if final_expand:
        r = _TH.expand(_TH.reshape(r, (1, 2, 1, 1)), SHAPE)
        want = np.broadcast_to(want.reshape(1, 2, 1, 1), SHAPE)
        shape = SHAPE
    got = Tensor._from_term(Term(r), dtypes.float32, shape).numpy()
    return got, want


class TestNestedReduceBroadcast(unittest.TestCase):
    def test_grid_default_path(self):
        for case in CASES:
            with self.subTest(case=case):
                got, want = run_case(*case)
                np.testing.assert_allclose(got, want, rtol=1e-4, atol=1e-4,
                                           err_msg=f"default path {case}")


_CHILD = r"""
import numpy as np
import test_nested_reduce_broadcast as t
for case in t.CASES:
    got, want = t.run_case(*case)
    err = float(np.abs(np.asarray(got) - want).max())
    ok = bool(np.allclose(got, want, rtol=1e-4, atol=1e-4))
    print(f"CASE {case} ok={ok} maxerr={err:.6g}", flush=True)
"""


class TestNestedReduceBroadcastWalker(unittest.TestCase):
    def test_grid_uop_walker(self):
        # A warmup the JIT never reaches, no batched precompile, and an opt
        # tag no cached dylib carries: every kernel falls to the UOp walker.
        out = helpers.run_child(_CHILD, 300, DEV="cpu", THVM_CPU_JIT_WARMUP="1000000000",
                                THVM_CPU_JIT_PARALLEL="0", THVM_CPU_JIT_OPT="O0",
                                THVM_CPU_UOP_WALK_TRACE="1")
        lines = [l for l in out.stdout.splitlines() if l.startswith("CASE ")]
        self.assertEqual(len(lines), len(CASES),
                         f"child failed:\n{out.stdout}\n{out.stderr}")
        walked = [l for l in out.stderr.splitlines() if l.startswith("uop_walk:")]
        self.assertGreaterEqual(len(walked), len(CASES),
                                "the walker did not execute the kernels")
        self.assertTrue(all(l.endswith("rc=1") for l in walked), "\n".join(walked))
        bad = [l for l in lines if "ok=False" in l]
        self.assertEqual(bad, [], "walker path diverges from numpy")


if __name__ == "__main__":
    unittest.main()
