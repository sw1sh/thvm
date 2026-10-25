"""Tensor.arange is tinygrad's lazy ramp (mixin/op.py:189-218): a running sum of a constant `step`, which
codegen folds to index math, where thvm wrote a host buffer that every consumer loaded.  The fold of a
running sum of a float constant multiplies as a float: as an integer multiply, (int)0.1 * count, the
cumsum of full(0.1) and arange(0, 1, 0.1) read 0.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_lazy_arange.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import GlobalCounters, Tensor, dtypes


class TestLazyArange(unittest.TestCase):
    def test_values(self):
        for args, dt in [((5,), None), ((5, 10), None), ((5, 10, 2), None), ((5.5, 10, 2), None),
                         ((10, 0, -3), None), ((0, 1, 0.1), None), ((2049,), None),
                         ((3,), dtypes.float16), ((-4, 4), dtypes.int64), ((7,), dtypes.uint8), ((0,), None)]:
            got = Tensor.arange(*args, dtype=dt).numpy() if dt else Tensor.arange(*args).numpy()
            np.testing.assert_allclose(got.astype(np.float64), np.arange(*args), atol=1e-6, err_msg=str(args))

    def test_long_ramps(self):
        # Past 2^15 the ramp is two short ones: one pool over n would index ~2n^2 positions, past int32,
        # and rand's counter ramp over a 128000-element draw read wrong.
        for n in (32769, 70001, 131072):
            np.testing.assert_array_equal(Tensor.arange(n, dtype=dtypes.uint32).numpy(), np.arange(n), err_msg=str(n))
        np.testing.assert_allclose(Tensor.arange(0, 7000.0, 0.1).numpy(), np.arange(0, 7000.0, 0.1, dtype=np.float32),
                                   rtol=1e-6, atol=1e-3)
        Tensor.manual_seed(42)
        r = Tensor.rand(128000).numpy()
        self.assertTrue(r.min() < 0.001 and r.max() > 0.999 and len(np.unique(r)) > 100000)

    def test_wide_int_bounds(self):
        np.testing.assert_array_equal(Tensor.arange(2**33, 2**33 + 3).numpy(), np.arange(2**33, 2**33 + 3))

    def test_float_running_sum_of_a_constant(self):
        np.testing.assert_allclose(Tensor.full((6,), 0.1).cumsum().numpy(), np.cumsum(np.full(6, 0.1, np.float32)),
                                   rtol=1e-6)

    def test_one_kernel_linear_ops(self):
        GlobalCounters.reset()
        Tensor.arange(4096).realize()
        self.assertEqual(GlobalCounters.kernel_count, 1)
        self.assertLess(GlobalCounters.global_ops, 16 * 4096)


if __name__ == "__main__":
    unittest.main()
