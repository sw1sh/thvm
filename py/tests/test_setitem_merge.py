"""Tensor.__setitem__ is tinygrad's (tensor.py:474-498, mixin/op.py:97-186): v goes where the
index's view ops took self's values from, by a where over per-axis masks (advanced indices by a
masked merge, the last repeated index winning); a buffer or a view of one keeps its storage.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_setitem_merge.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, dtypes


def _both(shape, index, value):
    n = np.arange(int(np.prod(shape)), dtype=np.float32).reshape(shape)
    t = Tensor(n).contiguous().realize()
    n[index] = value
    t[index] = Tensor(np.asarray(value, np.float32)) if not np.isscalar(value) else value
    return t.numpy(), n


class TestSetitemMerge(unittest.TestCase):
    def test_basic_slices_ints_and_strides(self):
        for index, value in [((1,), -1.0), ((slice(None), 2), -2.0), ((slice(1, 3), slice(0, 4, 2)), -3.0),
                             ((slice(None, None, -2), 1), -4.0), ((Ellipsis, slice(1, 2)), -5.0)]:
            got, want = _both((4, 5), index, value)
            np.testing.assert_array_equal(got, want, err_msg=str(index))

    def test_broadcast_tensor_value(self):
        got, want = _both((3, 4), (slice(None), slice(1, 3)), np.array([7.0, 8.0], np.float32))
        np.testing.assert_array_equal(got, want)

    def test_advanced_index_last_write_wins(self):
        got, want = _both((5, 2), ([0, 3, 0],), np.array([[1, 1], [2, 2], [3, 3]], np.float32))
        np.testing.assert_array_equal(got, want)

    def test_view_writes_through_to_base(self):
        base = Tensor.zeros(3, 4).contiguous().realize()
        view = base[1:3]
        view[:, 1] = 9.0
        want = np.zeros((3, 4), np.float32)
        want[1:3, 1] = 9.0
        np.testing.assert_array_equal(base.numpy(), want)

    def test_lazy_setitem_keeps_gradient(self):
        x = Tensor([2.0, 3.0])
        t = Tensor.zeros(4)
        t[1:3] = x * x
        t.sum().backward()
        np.testing.assert_array_equal(x.grad.numpy(), [4.0, 6.0])

    def test_dtype_mismatch_raises(self):
        t = Tensor.zeros(3).contiguous().realize()
        with self.assertRaises(RuntimeError):
            t[0] = Tensor([1], dtype=dtypes.int32)


if __name__ == "__main__":
    unittest.main()
