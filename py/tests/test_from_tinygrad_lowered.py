"""Cross-validate thvm's codegen against tinygrad's LOWERED kernels.

tinygrad builds + lowers a kernel; from_tinygrad_lowered rebuilds it as
thvm ranged IR, renders, and dispatches; we diff vs tinygrad's own result.
Complements test_from_tinygrad.py (tensor-level / full-backend path).

Single-kernel pointwise coverage for now (the lowered importer's scope).
Runs on the default backend (DEV=metal locally, DEV=cuda on the pod).
"""
import os
import unittest

import numpy as np

from thvm.from_tinygrad_lowered import cross_validate

_BACKEND = "cuda" if os.environ.get("DEV", "").lower() == "cuda" else "metal"


def _tg():
    from tinygrad import Tensor
    return Tensor


class TestFromTinygradLowered(unittest.TestCase):
    def _chk(self, build_fn):
        got, ref, err = cross_validate(build_fn, backend=_BACKEND)
        self.assertLess(err, 1e-3, f"got {got.ravel()[:6]} ref {ref.ravel()[:6]}")

    def setUp(self):
        np.random.seed(0)
        self.A = np.random.randn(8).astype(np.float32)
        self.B = np.random.randn(8).astype(np.float32)
        self.A2 = np.random.randn(3, 4).astype(np.float32)
        self.B2 = np.random.randn(3, 4).astype(np.float32)

    def test_add(self):
        T = _tg(); self._chk(lambda: T(self.A) + T(self.B))

    def test_mul(self):
        T = _tg(); self._chk(lambda: T(self.A) * T(self.B))

    def test_mul_add_noncommutative(self):
        T = _tg(); self._chk(lambda: (lambda a, b: a * b + a)(T(self.A), T(self.B)))

    def test_sub(self):
        T = _tg(); self._chk(lambda: T(self.A) - T(self.B))

    def test_div(self):
        T = _tg(); self._chk(lambda: T(self.A) * T(self.B).reciprocal())

    def test_relu(self):
        T = _tg(); self._chk(lambda: T(self.A).relu())

    def test_exp(self):
        T = _tg(); self._chk(lambda: T(self.A).exp())

    def test_add_2d(self):
        T = _tg(); self._chk(lambda: T(self.A2) + T(self.B2))


if __name__ == "__main__":
    unittest.main()
