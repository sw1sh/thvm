"""Python-scalar promotion on the float32/int32 surface.

tinygrad's rule (mixin/elementwise.py:21-31 + the dtype.py lattice): a
Python scalar is a weak value that adopts the tensor operand's kind, except a
float scalar combined with an int tensor promotes the op to float.  The old
_wrap_other truncated the float scalar to the int dtype (1.5 -> 1).

Run:
  cd <repo> && PYTHONPATH=py DEV=cpu python3 -m pytest py/tests/test_scalar_promotion.py -q
"""
import unittest
from thvm import Tensor, dtypes


class TestScalarPromotion(unittest.TestCase):
    def test_int_tensor_times_float_scalar_promotes(self):
        out = Tensor([1, 2, 3], dtype=dtypes.int32) * 1.5
        self.assertEqual(out.dtype, dtypes.float32)
        self.assertListEqual(out.tolist(), [1.5, 3.0, 4.5])

    def test_float_scalar_reverse_op_promotes(self):
        out = 0.5 + Tensor([1, 2], dtype=dtypes.int32)
        self.assertEqual(out.dtype, dtypes.float32)
        self.assertListEqual(out.tolist(), [1.5, 2.5])

    def test_int_tensor_plus_int_scalar_stays_int(self):
        out = Tensor([1, 2, 3], dtype=dtypes.int32) + 1
        self.assertEqual(out.dtype, dtypes.int32)
        self.assertListEqual(out.tolist(), [2, 3, 4])

    def test_float_tensor_plus_int_scalar_stays_float(self):
        out = Tensor([1.0, 2.0]) + 1
        self.assertEqual(out.dtype, dtypes.float32)
        self.assertListEqual(out.tolist(), [2.0, 3.0])

    def test_mixed_tensor_binop_computes_in_float(self):
        out = Tensor([1, 2], dtype=dtypes.int32) + Tensor([0.5, 0.5])
        self.assertEqual(out.dtype, dtypes.float32)
        self.assertListEqual(out.tolist(), [1.5, 2.5])


if __name__ == "__main__":
    unittest.main()
