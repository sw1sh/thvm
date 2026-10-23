"""The flat C ABI a foreign host (an FFI, not Python) uses: a CPU tensor over the host's own
bytes (py_ten_external) with its release callback, the buffer pointer back out (py_ten_data),
and the term a pin handle holds (py_pin_get)."""
import ctypes
import unittest
from ctypes import POINTER, c_int32, c_uint32, c_uint64, c_void_p

import numpy as np

import helpers  # noqa: F401 - puts py/ on the path
from thvm import Tensor, dtypes
from thvm.thvm import K, Term, _lib

_RELEASE = ctypes.CFUNCTYPE(None, c_void_p)
_lib.py_ten_external.restype = c_uint64
_lib.py_ten_external.argtypes = [c_uint32, c_uint32, POINTER(c_uint32), c_void_p, c_uint64,
                                 _RELEASE, c_void_p]
_lib.py_ten_data.restype = c_void_p
_lib.py_ten_data.argtypes = [c_uint64, POINTER(c_int32), POINTER(c_int32)]
_lib.py_pin_get.restype = c_uint64
_lib.py_pin_get.argtypes = [c_uint64]

# thvm may release an adopted buffer after the test returns, so the host bytes and the callback
# live as long as the module.
_HOST = np.arange(6, dtype=np.float32).reshape(2, 3)
_ON_RELEASE = _RELEASE(lambda handle: None)


class TestFlatAbi(unittest.TestCase):
    def test_external_buffer_in_and_out(self):
        dims = (c_uint32 * 2)(2, 3)
        t = _lib.py_ten_external(K.FP32, 2, dims, _HOST.ctypes.data, _HOST.nbytes, _ON_RELEASE, 7)
        self.assertNotEqual(t, 0)
        off, strides = c_int32(), (c_int32 * K.MAX_DIM)()
        self.assertEqual(_lib.py_ten_data(t, ctypes.byref(off), strides), _HOST.ctypes.data)
        self.assertEqual((off.value, strides[0], strides[1]), (0, 3, 1))
        x = Tensor._from_term(Term(t), dtypes.float32, (2, 3))
        np.testing.assert_array_equal((x * 2).numpy(), _HOST * 2)
        # A buffer shorter than the shape is refused.
        self.assertEqual(_lib.py_ten_external(K.FP32, 2, dims, _HOST.ctypes.data, 8,
                                              _ON_RELEASE, 7), 0)

    def test_pin_get_reads_the_pinned_term(self):
        x = (Tensor(np.ones((4,), dtype=np.float32)) + 1).realize()
        self.assertEqual(_lib.py_pin_get(x._pin_id), int(x.term))
        self.assertEqual(_lib.py_pin_get(1 << 40), 0)


if __name__ == "__main__":
    unittest.main()
