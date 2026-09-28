"""Cross-validate thvm's codegen against tinygrad's LOWERED kernels.

tinygrad builds + lowers a kernel; from_tinygrad_lowered rebuilds it as
thvm ranged IR, renders, and dispatches; we diff vs tinygrad's own result.
Complements test_from_tinygrad.py (tensor-level / full-backend path).

Single-kernel pointwise coverage for now (the lowered importer's scope).
Runs on the DEV-selected backend: cpu by default (C99, in-process), or
DEV=metal / DEV=cuda to dispatch on a GPU.  tinygrad lowers for the same
device.
"""
import os
import unittest

import numpy as np

from thvm.from_tinygrad_lowered import cross_validate

_BACKEND = (os.environ.get("DEV", "") or "cpu").split(":", 1)[0].lower()


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
        self.A2 = np.random.randn(4, 8).astype(np.float32)
        self.B2 = np.random.randn(4, 8).astype(np.float32)
        self.C = np.random.randn(16).astype(np.float32)
        self.C2 = np.random.randn(16).astype(np.float32)

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

    def test_sum(self):
        T = _tg(); self._chk(lambda: T(self.C).sum())

    def test_sum_axis1(self):
        T = _tg(); self._chk(lambda: T(self.A2).sum(axis=1))

    def test_sum_axis0(self):
        T = _tg(); self._chk(lambda: T(self.A2).sum(axis=0))

    def test_max(self):
        T = _tg(); self._chk(lambda: T(self.C).max())

    def test_bitcast_then_cast(self):
        # BITCAST reinterprets the bits; a value cast would give ~x.
        from tinygrad import dtypes
        T = _tg()
        P = np.abs(self.A) + 0.5
        got, ref, _ = cross_validate(lambda: T(P).bitcast(dtypes.int32).cast(dtypes.float32),
                                     backend=_BACKEND)
        np.testing.assert_allclose(got, ref, rtol=1e-6)

    def test_alu_param_refused(self):
        # A bound Variable is an ALU-addrspace PARAM (slot -1, tinygrad
        # #18444): a scalar argument, not a buffer this bridge can bind.
        from tinygrad.uop.ops import UOp, Ops
        try:
            from tinygrad.uop.ops import ParamArg
            from tinygrad.dtype import AddrSpace, dtypes
        except ImportError:
            self.skipTest("tinygrad checkout predates ParamArg.addrspace")
        if not {"addrspace", "dtype"} <= set(getattr(ParamArg, "__dataclass_fields__", {})):
            self.skipTest("tinygrad checkout predates ParamArg(dtype, addrspace)")
        from thvm.thvm import Thvm
        from thvm.from_tinygrad_lowered import _Lowerer
        u = UOp(Ops.PARAM, arg=ParamArg(slot=-1, dtype=dtypes.int32, addrspace=AddrSpace.ALU))
        low = _Lowerer(Thvm())
        with self.assertRaises(NotImplementedError):
            low.emit(u)
        self.assertNotIn(-1, low.params)

    # --- KOpt level: apply a thvm KOpt to the imported kernel; the
    #     OPTIMIZED kernel must still match tinygrad's result. ---
    def _chk_opt(self, build_fn, opts):
        got, ref, err = cross_validate(build_fn, backend=_BACKEND, opts=opts)
        self.assertLess(err, 1e-3, f"got {got.ravel()[:6]} ref {ref.ravel()[:6]}")

    def test_upcast_add(self):
        from thvm import K
        T = _tg(); self._chk_opt(lambda: T(self.C) + T(self.C2),
                                 [(K.KOP_UPCAST, 0, 4)])

    def test_upcast_mul(self):
        from thvm import K
        T = _tg(); self._chk_opt(lambda: T(self.C) * T(self.C2),
                                 [(K.KOP_UPCAST, 0, 8)])

    def test_upcast_noncommutative(self):
        from thvm import K
        T = _tg(); self._chk_opt(
            lambda: (lambda a, b: a * b + a)(T(self.C), T(self.C2)),
            [(K.KOP_UPCAST, 0, 4)])


if __name__ == "__main__":
    unittest.main()
