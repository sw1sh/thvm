"""TinyJit with several returned tensors refills every one on replay.

The capture's liveness pass (jit_capture_finalize) replays only the
dispatches its declared result roots need; TinyJit used to declare the
first returned tensor alone, so a replay left the others stale.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_jit_multi_output.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, TinyJit


class TestJitMultiOutput(unittest.TestCase):
    def test_every_output_refreshes_on_replay(self):
        @TinyJit
        def f(x):
            return x * 2, x + 1

        # Call 0 warms up, call 1 captures, calls 2-4 replay.
        for i in range(5):
            x = Tensor(np.full(4, float(i), np.float32)).realize()
            a, b = f(x)
            np.testing.assert_array_equal(a.numpy(), np.full(4, 2.0 * i, np.float32))
            np.testing.assert_array_equal(b.numpy(), np.full(4, i + 1.0, np.float32))


if __name__ == "__main__":
    unittest.main()
