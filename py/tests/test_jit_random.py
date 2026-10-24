"""A TinyJit replay redraws the host random tensors its capture drew.

thvm's random factories draw on the host from numpy's RNG, where tinygrad's draw in-graph, so a
replay of the captured kernels read the capture's draw again: beautiful_mnist's
`Tensor.randint(BS, high=N)` batch never changed after the second step, and training overfit it.
Each draw a capture makes is recorded with its generator, and a replay writes a fresh draw into
the same buffer first.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_jit_random.py -q
"""
import unittest

import numpy as np

from thvm import Tensor, TinyJit


class TestJitRandom(unittest.TestCase):
    def test_replays_redraw(self):
        x = Tensor(np.arange(100, dtype=np.float32))

        @TinyJit
        def pick(x):
            return x[Tensor.randint(16, high=100)].realize()

        draws = [tuple(pick(x).numpy().tolist()) for _ in range(5)]
        self.assertEqual(len(set(draws[2:])), 3, "replays returned the captured draw")
        for d in draws:
            self.assertTrue(all(0 <= v < 100 for v in d))

    def test_rand_and_seed(self):
        @TinyJit
        def noise(y):
            return (y + Tensor.rand(8)).realize()

        y = Tensor(np.zeros(8, np.float32))
        Tensor.manual_seed(3)
        first = [noise(y).numpy() for _ in range(4)]
        Tensor.manual_seed(3)
        again = [noise(y).numpy() for _ in range(4)]
        self.assertFalse(np.array_equal(first[2], first[3]))
        np.testing.assert_array_equal(first[3], again[3])


if __name__ == "__main__":
    unittest.main()
