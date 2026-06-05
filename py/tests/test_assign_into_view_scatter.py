"""assign() into a shrink-slice VIEW must scatter into the base buffer.

The GPT2 kv-cache write `cache[:, :, p:p+n, :, :].assign(stack(xk, xv))`
relies on this: each decode step writes one position's keys/values into a
persistent cache, then reads the whole `[:p+1]` slice back.  thvm's eager
UOP_ASSIGN copies whole buffers and ignores the view offset/strides, so
Tensor.assign special-cases a view dst (tagged by shrink/__getitem__) and
scatters host-side into the base tensor's buffer.
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

from thvm import Tensor  # noqa: E402


class TestAssignIntoViewScatter(unittest.TestCase):

    def test_slice_assign_writes_base(self):
        cache = Tensor.zeros(2, 1, 8, 3, 4).contiguous().realize()
        src = Tensor(np.arange(2 * 1 * 2 * 3 * 4)
                     .reshape(2, 1, 2, 3, 4).astype("float32"))
        cache[:, :, 2:4, :, :].assign(src).realize()
        got = cache.numpy()
        self.assertTrue(np.allclose(got[:, :, 2:4], src.numpy()))
        self.assertTrue(np.allclose(got[:, :, 0:2], 0))
        self.assertTrue(np.allclose(got[:, :, 4:], 0))

    def test_sequential_slice_assigns_accumulate(self):
        cache = Tensor.zeros(1, 5, 4).contiguous().realize()
        for p in range(5):
            row = Tensor(np.full((1, 1, 4), float(p + 1), dtype="float32"))
            cache[:, p:p + 1, :].assign(row).realize()
        got = cache.numpy()
        for p in range(5):
            self.assertTrue(np.allclose(got[:, p, :], p + 1),
                            f"row {p} = {got[:, p, :]}")


if __name__ == "__main__":
    unittest.main()
