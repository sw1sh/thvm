"""Tensor.rand is tinygrad's threefry2x32 (uop/decompositions.py:308-320, mixin/rand.py): the bits and
the seeded draw equal tinygrad's jax-generated references (test/unit/test_randomness.py
test_threefry_against_reference and _full), on both CPU routes, where a uint32 counter word wraps before it
widens; a 16-bit float draw stays in [0, 1)
(its kernels are the walker's, as for any narrow-float CAST).

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_threefry.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor, dtypes

BITS = [2221762175, 1752107825, 653745012, 1967534793, 1395205442, 3840423848, 2159346757, 603508235,
        3319473678, 3363866483, 3544324138, 1436466838, 2169858556, 2570072943, 2387150698, 3678370550,
        2911697663, 403244401, 2560861638, 1692360114]
RAND = [0.45735931396484375, 0.6311527490615845, 0.15571284294128418, 0.8149417638778687, 0.7862188816070557,
        0.8008807897567749, 0.568588376045227, 0.9852620363235474, 0.42314577102661133, 0.9811755418777466]


def case_bits():
    counts0, counts1 = Tensor.arange(20, dtype=dtypes.uint32).chunk(2)
    r = Tensor._threefry_random_bits(Tensor([0, 1337], dtype=dtypes.uint32), counts0, counts1).numpy()
    np.testing.assert_array_equal(r, np.array(BITS, np.uint32))


def case_counter_word_wraps():
    # rand's counter combine: a uint32 sum past 2^32 wraps before it widens, as the C kernels compute it;
    # the walker kept the carry, which landed in the high word.
    x = Tensor([0xFFFFFFFF, 0xFFFFFFFE, 7], dtype=dtypes.uint32).realize()
    w = ((x + 4).cast(dtypes.uint64) << 32) | (x + 2).cast(dtypes.uint64)
    np.testing.assert_array_equal(w.numpy(), np.array([(3 << 32) | 1, (2 << 32) | 0, (11 << 32) | 9], np.uint64))


def case_seeded_rand():
    Tensor.manual_seed(1337)
    np.testing.assert_allclose(Tensor.rand(20).numpy()[:10], RAND, atol=1e-5, rtol=1e-5)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_threefry"
    ROUTE_CASES = ROUTE_CASES


class TestNarrowRand(unittest.TestCase):
    def test_stays_below_one(self):
        Tensor.manual_seed(0)
        for dt in (dtypes.float16, dtypes.bfloat16, dtypes.float64):
            x = Tensor.rand(2, 64, 64, dtype=dt).float().numpy()
            self.assertTrue(x.min() >= 0 and x.max() < 1, dt)
            self.assertGreater(len(np.unique(x)), min(1000, 2 ** dtypes.finfo(dt)[1] // 4), dt)


if __name__ == "__main__":
    unittest.main()
