"""Embedding forward and backward stay linear in the index count.

On the CPU backend Tensor.embedding is one native gather (O(N*D)) and its
weight gradient one scatter-add (O(V*D + N*D)), where tinygrad's one-hot
formulation costs O(N*V*D).  With N = 1e5 ids of width D = 64, growing the
table from V = 1e3 to 1e5 rows adds only the V*D gradient init: a one-hot
path would slow down 100x, the native one stays well under 3x.  Values are
checked against numpy (np.add.at for the gradient) at every size.
"""
import time
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Device, Tensor

N, D = 100_000, 64


def _step(w, idx, g):
    out = w.embedding(idx)
    (out * g).sum().backward()
    Tensor.realize(out, w.grad)
    return out, w.grad


def _inputs(w0, ids, g):
    # Fresh leaves per step (backward accumulates into a leaf's gradient),
    # realized before the timer starts: copying the table in is not the
    # embedding's cost.
    return Tensor(w0).realize(), Tensor(ids).realize(), Tensor(g).realize()


@unittest.skipUnless(Device.DEFAULT == "CPU", "native gather runs on the CPU backend")
class TestGatherScale(unittest.TestCase):
    def test_linear_in_index_count(self):
        rng = np.random.default_rng(0)
        g = rng.standard_normal((N, D)).astype(np.float32)
        times = {}
        for V in (1_000, 10_000, 100_000):
            w0 = rng.standard_normal((V, D)).astype(np.float32)
            ids = rng.integers(0, V, N).astype(np.int32)
            out, grad = _step(*_inputs(w0, ids, g))
            np.testing.assert_array_equal(out.numpy(), w0[ids])
            want = np.zeros((V, D), np.float32)
            np.add.at(want, ids, g)
            np.testing.assert_allclose(grad.numpy(), want, rtol=1e-5, atol=1e-5)
            best = float("inf")
            for _ in range(3):
                args = _inputs(w0, ids, g)
                t0 = time.perf_counter()
                _step(*args)
                best = min(best, time.perf_counter() - t0)
            times[V] = best
        print(f"\n  embedding fwd+bwd N={N} D={D}: "
              + ", ".join(f"V={v}: {t * 1e3:.1f} ms" for v, t in times.items()))
        self.assertLess(times[100_000] / times[1_000], 3.0)


if __name__ == "__main__":
    unittest.main()
