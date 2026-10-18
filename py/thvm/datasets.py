"""tinygrad's nn.datasets.mnist (nn/datasets.py:4-8): images (N, 1, 28, 28) and labels
(N,), here float32 of the raw 0-255 values and int32, as py/thvm has no uint8."""
from __future__ import annotations

import gzip

import numpy as np

from .helpers import fetch
from .tensor import Tensor

_MNIST = "https://storage.googleapis.com/cvdf-datasets/mnist/"
_FASHION = "http://fashion-mnist.s3-website.eu-central-1.amazonaws.com/"


def _fetch(url: str, fname: str) -> bytes:
    return fetch(url + fname).read_bytes()


def _parse_idx(raw: bytes) -> np.ndarray:
    data = gzip.decompress(raw)
    # IDX header: magic (4) where byte 3 = ndim; then ndim big-endian
    # uint32 dimensions; then the payload as uint8.
    ndim = data[3]
    dims = [int.from_bytes(data[4 + 4 * i:8 + 4 * i], "big") for i in range(ndim)]
    payload = np.frombuffer(data, dtype=np.uint8, offset=4 + 4 * ndim)
    return payload.reshape(dims)


def mnist(fashion: bool = False):
    base = _FASHION if fashion else _MNIST
    xtr = _parse_idx(_fetch(base, "train-images-idx3-ubyte.gz"))
    ytr = _parse_idx(_fetch(base, "train-labels-idx1-ubyte.gz"))
    xte = _parse_idx(_fetch(base, "t10k-images-idx3-ubyte.gz"))
    yte = _parse_idx(_fetch(base, "t10k-labels-idx1-ubyte.gz"))
    X_train = Tensor(xtr.reshape(-1, 1, 28, 28).astype(np.float32))
    Y_train = Tensor(ytr.astype(np.int32))
    X_test = Tensor(xte.reshape(-1, 1, 28, 28).astype(np.float32))
    Y_test = Tensor(yte.astype(np.int32))
    return X_train, Y_train, X_test, Y_test
