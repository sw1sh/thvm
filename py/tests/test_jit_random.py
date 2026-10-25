"""A TinyJit replay draws fresh random values, as tinygrad's (test/unit/test_jit_footguns.py
test_random_regenerates): the draws are threefry over a counter buffer that each call advances, and
the replay advances it too, so beautiful_mnist's `Tensor.randint(BS, high=N)` batch changes every
step.  manual_seed repeats an eager sequence.

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

    def test_random_regenerates(self):
        @TinyJit
        def f(x):
            return (x + Tensor.rand(3)).realize()

        f(Tensor([0.0, 0.0, 0.0]))
        f(Tensor([0.0, 0.0, 0.0]))
        results = {tuple(f(Tensor([0.0, 0.0, 0.0])).numpy().tolist()) for _ in range(5)}
        self.assertEqual(len(results), 5)

    def test_seed_repeats(self):
        Tensor.manual_seed(3)
        first = [Tensor.rand(8).numpy() for _ in range(3)]
        Tensor.manual_seed(3)
        again = [Tensor.rand(8).numpy() for _ in range(3)]
        for a, b in zip(first, again):
            np.testing.assert_array_equal(a, b)
        self.assertFalse(np.array_equal(first[0], first[1]))


if __name__ == "__main__":
    unittest.main()
