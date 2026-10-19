"""Small tinygrad behaviours of nn and helpers: BatchNorm counts its training
batches (nn/__init__.py:58), and fetch(gunzip=True) stores the decompressed
body under a .gunzip name (helpers.py:476-484)."""
import gzip
import os
import tempfile
import unittest

import numpy as np

import helpers  # noqa: F401 - puts py/ on the path
from thvm import Context, Tensor, nn
from thvm.helpers import fetch


class TestNnBookkeeping(unittest.TestCase):
    def test_batchnorm_counts_training_batches(self):
        bn = nn.BatchNorm(3)
        x = Tensor(np.arange(24, dtype=np.float32).reshape(2, 3, 2, 2))
        with Context(TRAINING=1):
            for _ in range(3):
                bn(x).realize()
                Tensor.realize(bn.running_mean, bn.running_var, bn.num_batches_tracked)
        self.assertEqual(float(bn.num_batches_tracked.numpy()), 3.0)

    def test_fetch_gunzip_decompresses(self):
        body = b"thvm fetch gunzip\n" * 100
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "blob.gz")
            with open(src, "wb") as f:
                f.write(gzip.compress(body))
            fp = fetch("file://" + src, name=os.path.join(d, "out"), gunzip=True)
            self.assertEqual(fp.read_bytes(), body)


if __name__ == "__main__":
    unittest.main()
