"""JIT replay: an assign sunk into its producer must not strand a gather.

jit_capture_sink_assigns (src/jit/capture.c) lets a dispatch write straight
into an ASSIGN's destination when no later captured op reads the dispatch's
output buffer.  A replayable gather (a realized view of that output) is such
a reader.  When the check missed it, the dispatch moved to the assign's
buffer and the gather replayed the bytes left over from the capture step.

The sink only fires for Metal tile kernels (jit_assign_sink_safe), so the
sink itself is exercised with DEV=metal; on CPU this pins the same
DISPATCH / ASSIGN / GATHER / ASSIGN replay sequence.

Run:
  cd <repo> && PYTHONPATH=py DEV=cpu python3 -m pytest py/tests/test_jit_sink_assign_gather.py -q
"""
import unittest

import numpy as np

from thvm import Tensor, TinyJit


class TestJitSinkAssignGather(unittest.TestCase):
    def test_gather_after_sunk_assign_sees_fresh_output(self):
        w = Tensor(np.zeros((3, 4), np.float32)).realize()
        v = Tensor(np.zeros((4, 3), np.float32)).realize()

        @TinyJit
        def step(x):
            # Captured ops: DISPATCH y, ASSIGN w <- y, GATHER y.T (a realize
            # root that is a view of a kernel output), ASSIGN v <- y.T.
            y = (x * 2 + 1).realize()
            w.assign(y).realize()
            yt = y.permute(1, 0).realize()
            v.assign(yt).realize()

        rng = np.random.default_rng(0)
        for call in range(5):
            xn = rng.standard_normal((3, 4)).astype(np.float32)
            step(Tensor(xn).realize())
            want = xn * 2 + 1
            np.testing.assert_allclose(w.numpy(), want, rtol=1e-6, err_msg=f"w, call {call}")
            np.testing.assert_allclose(v.numpy(), want.T, rtol=1e-6, err_msg=f"v, call {call}")


if __name__ == "__main__":
    unittest.main()
