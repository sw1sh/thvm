"""TinyJit as tinygrad's class (engine/jit.py:39-183): `captured._linear.src` holds the ops a replay
runs, reset() captures afresh, a method jits through __get__, a call whose inputs alias, mismatch the
capture's shape, dtype or view, return a non-Tensor or capture nothing raises JitError, a TinyJit
capturing inside another raises RuntimeError, every op the capture ran replays (a kernel no result
reads too) but CAPTURING=0 keeps a realize out of the capture, and prune=True runs the ops that read
no input once.  A view of a realized buffer is an input as it stands: the capture reads its base
through the view, and a later call's other view raises JitError.  A pruned op's output is read through a copy, as a
later intermediate may take its arena slot.  A jit returning one of its inputs returns, on a replay,
that call's tensor, where it returned the capture call's stale one.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_jit_api.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Context, Tensor, TinyJit, dtypes
from thvm.engine.jit import JitError


def ops(f):
    return len(f.captured._linear.src) if f.captured is not None else 0


class TestJitApi(unittest.TestCase):
    def test_captured_reset_and_method(self):
        @TinyJit
        def add(a, b):
            return a + b

        for n in (10, 20):
            for _ in range(4):
                a, b = Tensor.randn(n, n), Tensor.randn(n, n)
                np.testing.assert_allclose(add(a, b).numpy(), a.numpy() + b.numpy(), atol=1e-6)
            self.assertEqual(ops(add), 1)
            add.reset()
            self.assertIsNone(add.captured)

        class Fun:
            def __init__(self):
                self.a = Tensor.randn(4).realize()

            @TinyJit
            def __call__(self, b):
                return (self.a + b).realize()

        fun = Fun()
        for _ in range(4):
            b = Tensor.randn(4)
            np.testing.assert_allclose(fun(b).numpy(), fun.a.numpy() + b.numpy(), atol=1e-6)
        self.assertEqual(ops(fun.__call__.func.__self__), 1)

    def test_jit_errors(self):
        @TinyJit
        def add(a, b):
            return (a + b).realize()

        a = Tensor.randn(3, 3)
        with self.assertRaises(JitError):
            add(a, a)
        for _ in range(3):
            add(Tensor.randn(3, 3), Tensor.randn(3, 3))
        with self.assertRaises(JitError):
            add(Tensor.randn(3, 3), Tensor.randn(4, 4))
        with self.assertRaises(JitError):
            add(Tensor.randn(3, 3), Tensor.randn(3, 3).cast(dtypes.int32))
        with self.assertRaises(JitError):
            add(a=Tensor.randn(3, 3), b=Tensor.randn(3, 3))

        @TinyJit
        def nothing(a):
            return None

        with self.assertRaises(JitError):
            for _ in range(3):
                nothing(Tensor.randn(2))

        @TinyJit
        def pair(a, i):
            return (a + 1).realize(), i

        with self.assertRaises(JitError):
            for i in range(3):
                pair(Tensor.randn(2), i)

    def test_returns_its_input(self):
        @TinyJit
        def f(a, b):
            return b.assign(a + 1)

        for i in range(5):
            b = Tensor.empty(1)
            c = f(Tensor([float(i)]), b)
            self.assertIs(c, b)
            self.assertEqual(c.item(), i + 1)

    def test_view_input_mismatch(self):
        @TinyJit
        def inc(a):
            return (a + 1).realize()

        base = Tensor.arange(20).float().reshape(4, 5).realize()
        with self.assertRaises(JitError):
            for i in range(4):
                np.testing.assert_array_equal(inc(base[:, i:i + 2]).numpy(), base.numpy()[:, i:i + 2] + 1)

    def test_view_input_replays_on_its_base(self):
        @TinyJit
        def inc(a):
            return (a + 1).realize()

        for i in range(5):
            base = (Tensor.arange(20).float().reshape(4, 5) * (i + 1)).realize()
            np.testing.assert_array_equal(inc(base[:, 1:3]).numpy(), base.numpy()[:, 1:3] + 1)
        self.assertEqual(ops(inc), 1)

    def test_jit_inside_jit(self):
        @TinyJit
        def f(t):
            return t + 1

        @TinyJit
        def g(t):
            return f(t) * 3

        self.assertEqual(g(Tensor([1])).realize().item(), 6)
        with self.assertRaisesRegex(RuntimeError, "having TinyJit inside another TinyJit"):
            g(Tensor([2])).realize()

    def test_not_capturing(self):
        @TinyJit
        def add_dead(a, b):
            Tensor.zeros(4, 4).contiguous().realize()
            return (a + b).realize()

        for i in range(4):
            np.testing.assert_array_equal(add_dead(Tensor([i]), Tensor([i])).numpy(), [2 * i])
        self.assertEqual(ops(add_dead), 2)

        @TinyJit
        def add(a, b):
            with Context(CAPTURING=0):
                (Tensor.arange(4) * 2).realize()
            return (a + b).realize()

        for i in range(4):
            np.testing.assert_array_equal(add(Tensor([i]), Tensor([i])).numpy(), [2 * i])
        self.assertEqual(ops(add), 1)

    def test_prune(self):
        w = Tensor.rand(16).realize()

        def f(x):
            return ((w * 2).realize() + x).realize()

        plain, pruned = TinyJit(f), TinyJit(f, prune=True)
        for jf, n in ((plain, 2), (pruned, 1)):
            for _ in range(4):
                x = Tensor.rand(16).realize()
                np.testing.assert_allclose(jf(x).numpy(), w.numpy() * 2 + x.numpy(), rtol=1e-6)
            self.assertEqual(ops(jf), n)

    def test_prune_once_output_outlives_its_arena_slot(self):
        # s1 reads no input; s3's slot may reuse s1's once s2 has read it.
        w = Tensor.rand(16).realize()

        @TinyJit(prune=True)
        def f(x):
            return x + (x * (x + (w * 2).sum()).sum()).sum()

        def ref(x):
            s1 = (w.numpy() * 2).sum()
            return x + (x * (x + s1).sum()).sum()

        for _ in range(5):
            x = Tensor.rand(16).realize()
            np.testing.assert_allclose(f(x).numpy(), ref(x.numpy()), rtol=1e-5)


if __name__ == "__main__":
    unittest.main()
