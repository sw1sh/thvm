"""assign() into a view of a buffer must write the buffer.

The GPT2 kv-cache write `cache[:, :, p:p+n, :, :].assign(stack(xk, xv))`
relies on this: each decode step writes one position's keys/values into a
persistent cache, then reads the whole `[:p+1]` slice back.  thvm's eager
UOP_ASSIGN writes one contiguous run at the view's offset, so a view of a
buffer (a shrink, reshape, permute, flip or detach of it, or of a view of it)
hands x to its base, which takes it where the view reads it, down to the
buffer, on the device, as tinygrad's assign through a view.
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, dtypes


class TestAssignIntoViewScatter(unittest.TestCase):

    def test_slice_assign_writes_base(self):
        cache = Tensor.zeros(2, 1, 8, 3, 4).contiguous().realize()
        src = Tensor(np.arange(2 * 1 * 2 * 3 * 4)
                     .reshape(2, 1, 2, 3, 4).astype("float32"))
        view = cache[:, :, 2:4, :, :].assign(src).realize()
        got = cache.numpy()
        self.assertTrue(np.allclose(got[:, :, 2:4], src.numpy()))
        self.assertTrue(np.allclose(got[:, :, 0:2], 0))
        self.assertTrue(np.allclose(got[:, :, 4:], 0))
        self.assertEqual(view.shape, (2, 1, 2, 3, 4))
        self.assertTrue(np.allclose(view.numpy(), src.numpy()))

    def test_sequential_slice_assigns_accumulate(self):
        cache = Tensor.zeros(1, 5, 4).contiguous().realize()
        for p in range(5):
            row = Tensor(np.full((1, 1, 4), float(p + 1), dtype="float32"))
            cache[:, p:p + 1, :].assign(row).realize()
        got = cache.numpy()
        for p in range(5):
            self.assertTrue(np.allclose(got[:, p, :], p + 1),
                            f"row {p} = {got[:, p, :]}")

    def test_assign_into_view_of_view(self):
        base = Tensor(np.arange(24, dtype=np.float32).reshape(2, 3, 4)).contiguous().realize()
        base[1][:, 1:3].assign(Tensor.full((3, 2), -1.0)).realize()
        want = np.arange(24, dtype=np.float32).reshape(2, 3, 4)
        want[1][:, 1:3] = -1.0
        np.testing.assert_array_equal(base.numpy(), want)

    def test_broadcast_value_int(self):
        cache = Tensor.zeros(3, 4, dtype=dtypes.int32).contiguous().realize()
        cache[1:3, 2:4].assign(Tensor([7, 8], dtype=dtypes.int32)).realize()
        want = np.zeros((3, 4), np.int32)
        want[1:3, 2:4] = [7, 8]
        np.testing.assert_array_equal(cache.numpy(), want)

    def test_writes_through_permute_flip_detach(self):
        t = Tensor.zeros(2, 3).contiguous().realize()
        t.T[1, 0] = 5.0
        t[::-1][0] += 1.0
        t.detach()[1, 2] = 7.0
        np.testing.assert_array_equal(t.numpy(), [[0, 5, 0], [1, 1, 7]])

    def test_chained_index_equals_tuple_index(self):
        a = Tensor.zeros(3, 3).contiguous().realize()
        b = Tensor.zeros(3, 3).contiguous().realize()
        a[1, 2] = 5.0
        b[1][2] = 5.0
        np.testing.assert_array_equal(a.numpy(), b.numpy())

    def test_inplace_ops_keep_storage(self):
        x = Tensor([1.0, 2.0]).clone()
        view = x[:1]
        x += 3
        np.testing.assert_array_equal(x.numpy(), [4.0, 5.0])
        np.testing.assert_array_equal(view.numpy(), [4.0])
        view *= 2
        np.testing.assert_array_equal(x.numpy(), [8.0, 5.0])

    def test_assign_into_pending_value_takes_it(self):
        a = Tensor([1.0, 2.0]) * 2
        a.assign(a + 1)
        np.testing.assert_array_equal(a.numpy(), [3.0, 5.0])


if __name__ == "__main__":
    unittest.main()
