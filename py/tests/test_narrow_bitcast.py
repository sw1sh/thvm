"""A bitcast to or from a 16- or 8-bit float keeps its bits, as tinygrad's (mixin/gradient.py:105, a
BITCAST is a reinterpret): the UOp walker converted the value, so a lazy uint16 0x3c00 bitcast to
float16 read 15360.0, and the C kernels held the float16 in a float, so the C renderer declines such
a kernel to the walker, as for a narrow CAST.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_narrow_bitcast.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, dtypes

U16 = np.array([0x3c00, 0x4000, 0xc500, 0x0001, 0x7bff, 0x3555], np.uint16)


class TestNarrowBitcast(unittest.TestCase):
    def test_u16_to_f16(self):
        x = (Tensor(U16.astype(np.int32)) + 0).cast(dtypes.uint16)
        np.testing.assert_array_equal(x.bitcast(dtypes.float16).numpy(), U16.view(np.float16))

    def test_f16_to_u16(self):
        f = Tensor(U16.view(np.float16).astype(np.float32)).cast(dtypes.float16) * 1
        np.testing.assert_array_equal(f.bitcast(dtypes.uint16).numpy(), U16)

    def test_bf16_from_u16(self):
        b = np.array([0x3f80, 0x4040, 0xbf00, 0x0080], np.uint16)
        x = (Tensor(b.astype(np.int32)) + 0).cast(dtypes.uint16).bitcast(dtypes.bfloat16)
        np.testing.assert_array_equal(x.float().numpy(), np.array([1.0, 3.0, -0.5, 2.0 ** -126], np.float32))


if __name__ == "__main__":
    unittest.main()
