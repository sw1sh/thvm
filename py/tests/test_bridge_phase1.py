"""Phase 1 bridge smoke test -- exercises the py.thvm C bridge directly.

Verifies the high-level TAG_TEN surface (docs/plans/py_tensor_frontend.md
Phase 1): tensor create, host I/O, the tensor algebra + movement ops,
autodiff, and realize -- without the Phase-2 Tensor class.

    make py && python -m pytest py/tests/test_bridge_phase1.py
"""
import pathlib
import struct
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from thvm.thvm import Thvm, K  # noqa: E402


def _f32(values):
    return struct.pack(f"<{len(values)}f", *values)


def _unf32(raw):
    return list(struct.unpack(f"<{len(raw) // 4}f", raw))


class TestBridgePhase1(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.th = Thvm()

    def _const(self, dims, values):
        """Allocate an FP32 leaf tensor and fill it from host."""
        t = self.th.ten_create(K.FP32, dims)
        self.assertTrue(self.th.ten_write(t, _f32(values)))
        return t

    def _eval(self, term, numel):
        r = self.th.realize(term)
        self.assertEqual(self.th.term_tag(r), K.TAG_TEN)
        return _unf32(self.th.ten_read(r, numel * 4))

    # --- tensor create / host I/O ---

    def test_create_roundtrip(self):
        t = self._const([4], [1.0, 2.0, 3.0, 4.0])
        self.assertEqual(self.th.ten_shape(t), (4,))
        self.assertEqual(self.th.ten_numel(t), 4)
        self.assertEqual(self.th.ten_dtype(t), K.FP32)
        self.assertEqual(_unf32(self.th.ten_read(t, 16)), [1.0, 2.0, 3.0, 4.0])

    # --- tensor algebra ---

    def test_add(self):
        a = self._const([4], [1, 2, 3, 4])
        b = self._const([4], [10, 20, 30, 40])
        self.assertEqual(self._eval(self.th.add(a, b), 4), [11, 22, 33, 44])

    def test_mul(self):
        a = self._const([3], [2, 3, 4])
        b = self._const([3], [5, 6, 7])
        self.assertEqual(self._eval(self.th.mul(a, b), 3), [10, 18, 28])

    def test_neg(self):
        x = self._const([3], [1, -2, 3])
        self.assertEqual(self._eval(self.th.neg(x), 3), [-1, 2, -3])

    def test_reduce_sum(self):
        x = self._const([4], [1, 2, 3, 4])
        self.assertEqual(self._eval(self.th.reduce(K.REDUCE_SUM, 0, x), 1), [10.0])

    # --- movement ---

    def test_reshape(self):
        x = self._const([6], [1, 2, 3, 4, 5, 6])
        r = self.th.realize(self.th.reshape(x, [2, 3]))
        self.assertEqual(self.th.ten_shape(r), (2, 3))
        self.assertEqual(_unf32(self.th.ten_read(r, 24)), [1, 2, 3, 4, 5, 6])

    # --- autodiff: thvm's real uop_grad ---

    def test_grad_with_target(self):
        a = self._const([3], [2, 3, 4])
        b = self._const([3], [5, 6, 7])
        y = self.th.mul(a, b)
        gy = self._const([3], [1, 1, 1])
        # d(a*b)/da = b
        self.assertEqual(self._eval(self.th.grad_with_target(y, gy, a), 3),
                         [5, 6, 7])

    def test_grad_with_target_unmarked_under_requires_grad(self):
        # Explicit-target gradient must ignore requires_grad on the
        # target: tinygrad Tensor.gradient(*targets) (tensor.py:836)
        # differentiates w.r.t. exactly the named targets, no filter.
        # Regression for the post-Adam-loop grad=0 corruption: marking
        # ANY tensor requires_grad bumps GRAD_REQ_NCOUNT, which used to
        # make grad_with_target on an UNMARKED target return zero
        # (uop_grad.c target-aware leaf filter).
        param = self.th.ten_create(K.FP32, [2])
        self.th.ten_write(param, _f32([1.0, 1.0]))
        self.assertTrue(self.th.ten_set_requires_grad(param, True))
        try:
            a = self._const([3], [2, 3, 4])
            b = self._const([3], [5, 6, 7])
            y = self.th.mul(a, b)
            gy = self._const([3], [1, 1, 1])
            # a is NOT requires_grad, but it IS the explicit target:
            # d(a*b)/da = b, unaffected by param's requires_grad.
            self.assertEqual(
                self._eval(self.th.grad_with_target(y, gy, a), 3),
                [5, 6, 7])
        finally:
            self.th.ten_set_requires_grad(param, False)

    # --- introspection (Phase-4 cross-check surface) ---

    def test_introspection(self):
        self.assertGreater(self.th.tens_count(), 0)


if __name__ == "__main__":
    unittest.main()
