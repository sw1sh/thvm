"""A cast to a 16- or 8-bit float rounds inside a kernel, as tinygrad's does; arange takes its dtypes.

C holds fp16, bf16 and the fp8s as a float, so x.cast(fp8).float() fused into one kernel read x
back unrounded; the walker now rounds such a CAST through the dtype's storage, as its stores do,
and the C renderer declines a kernel that holds one (src/uop/dag_has_narrow_cast.c).  Tensor.arange
picks tinygrad's int dtype ladder (dtype.py:171-174) and raises OverflowError past its dtype
(mixin/op.py:186-195), as tinygrad's test_arange and test_arange_low_precision_dtype check.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_narrow_cast.py -q
"""
import unittest

import numpy as np

from thvm import Tensor, dtypes

NARROW = (dtypes.float16, dtypes.bfloat16, *dtypes.fp8s)


class TestNarrowCast(unittest.TestCase):
    def test_cast_chain_rounds(self):
        for dt in NARROW:
            with self.subTest(dt=dt):
                x = Tensor.arange(0, 200, 1)
                fused = x.cast(dt).float().tolist()
                stored = x.cast(dt).contiguous().realize().float().tolist()
                self.assertEqual(fused, stored)

    def test_float_cast_chain_rounds(self):
        a = np.linspace(-3, 3, 97, dtype=np.float32)
        for dt in NARROW:
            with self.subTest(dt=dt):
                fused = (Tensor(a) * 1.0).cast(dt).float().numpy()
                stored = (Tensor(a) * 1.0).cast(dt).contiguous().realize().float().numpy()
                np.testing.assert_array_equal(fused, stored)

    def test_arange_low_precision(self):
        # tinygrad test_arange_low_precision_dtype.
        for dt in NARROW:
            for args in ((0, 10, 3), (0, 200, 1), (5.5, 175.5, 2.5)):
                with self.subTest(dt=dt, args=args):
                    self.assertEqual(Tensor.arange(*args, dtype=dt).float().tolist(),
                                     Tensor.arange(*args).cast(dt).float().tolist())

    def test_arange_dtypes(self):
        self.assertEqual(Tensor.arange(10).dtype, dtypes.int32)
        self.assertEqual(Tensor.arange(2**31, 2**31 + 3).dtype, dtypes.int64)
        self.assertEqual(Tensor.arange(2**31, 2**31 + 3).tolist(), [2**31, 2**31 + 1, 2**31 + 2])
        np.testing.assert_array_equal(Tensor.arange(-128, 128, dtype=dtypes.int8).numpy(),
                                      np.arange(-128, 128, dtype=np.int8))
        for args, kw in (((2**33,), {"dtype": dtypes.int32}), ((129,), {"dtype": dtypes.int8}),
                         ((128, 0, -1), {"dtype": dtypes.int8}), ((127, -130, -1), {"dtype": dtypes.int8})):
            with self.subTest(args=args), self.assertRaises(OverflowError):
                Tensor.arange(*args, **kw)


if __name__ == "__main__":
    unittest.main()
