"""Differential test: thvm's in-house Tensor vs the tinygrad->thvm import.

For each op we build the SAME computation three ways and diff them:
  - tinygrad (the reference),
  - thvm in-house (thvm.Tensor, the drop-in API -> thvm backend),
  - thvm via import (tinygrad graph -> from_tinygrad -> thvm backend).

Both thvm paths must match tinygrad AND each other.  This is how the
keepdim-reduce broadcast bug (in-house) and the importer REDUCE-keepdim
overflow (softmax) were caught: the two thvm paths diverged from the
tinygrad oracle.  Runs on the default backend (DEV).
"""
import unittest

import numpy as np


def _have_tinygrad():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


_rng = np.random.default_rng(0)
_V = _rng.standard_normal(8).astype(np.float32)
_W = _rng.standard_normal(8).astype(np.float32)
_M1 = _rng.standard_normal((4, 6)).astype(np.float32)
_M2 = _rng.standard_normal((6, 5)).astype(np.float32)
_T2 = _rng.standard_normal((3, 8)).astype(np.float32)
_X = _rng.standard_normal((2, 3, 8, 8)).astype(np.float32)
_CW = _rng.standard_normal((4, 3, 3, 3)).astype(np.float32)
_ROW = _rng.standard_normal((1, 8)).astype(np.float32)

# op name -> builder(Tensor_class) -> tensor.  The drop-in API means the
# same lambda runs on thvm.Tensor and tinygrad.Tensor.
OPS = {
    "add":        lambda T: T(_V) + T(_W),
    "mul":        lambda T: T(_V) * T(_W),
    "sub":        lambda T: T(_V) - T(_W),
    "neg":        lambda T: -T(_V),
    "relu":       lambda T: T(_V).relu(),
    "exp":        lambda T: T(_V).exp(),
    "log":        lambda T: (T(_V) + 5.0).log(),
    "sqrt":       lambda T: (T(_V) * T(_V)).sqrt(),
    "recip":      lambda T: (T(_V) + 3.0).reciprocal(),
    "rsqrt":      lambda T: (T(_V) * T(_V) + 1.0).rsqrt(),
    "abs":        lambda T: T(_V).abs(),
    "sigmoid":    lambda T: T(_V).sigmoid(),
    "tanh":       lambda T: T(_V).tanh(),
    "mul_add":    lambda T: T(_V) * T(_W) + T(_V),
    "sum":        lambda T: T(_T2).sum(),
    "sum_ax1":    lambda T: T(_T2).sum(axis=1),
    "sum_ax0":    lambda T: T(_T2).sum(axis=0),
    "max":        lambda T: T(_T2).max(),
    "max_ax1":    lambda T: T(_T2).max(axis=1),
    "mean":       lambda T: T(_T2).mean(),
    "mean_ax1":   lambda T: T(_T2).mean(axis=1),
    "softmax":    lambda T: T(_T2).softmax(axis=1),
    "broadcast":  lambda T: T(_T2) + T(_ROW),
    "matmul":     lambda T: T(_M1) @ T(_M2),
    "reshape":    lambda T: T(_T2).reshape(4, 6),
    "permute":    lambda T: T(_T2).permute(1, 0),
    "flatten":    lambda T: T(_T2).flatten(),
    "conv2d":     lambda T: T(_X).conv2d(T(_CW)),
    "conv_relu":  lambda T: T(_X).conv2d(T(_CW)).relu(),
    "max_pool2d": lambda T: T(_X).max_pool2d((2, 2)),
    "relu_sum":   lambda T: T(_T2).relu().sum(axis=1),
}


@unittest.skipUnless(_have_tinygrad(), "tinygrad not on path")
class TestDiffInhouseVsImport(unittest.TestCase):
    pass


def _make(name, build):
    def test(self):
        import thvm
        import tinygrad
        from thvm.from_tinygrad import from_tinygrad
        ref = np.asarray(build(tinygrad.Tensor).numpy(), dtype=np.float32)
        inhouse = np.asarray(build(thvm.Tensor).numpy(), dtype=np.float32)
        imported = np.asarray(from_tinygrad(build(tinygrad.Tensor)).numpy(),
                              dtype=np.float32)
        inhouse = inhouse.reshape(ref.shape)
        imported = imported.reshape(ref.shape)
        self.assertLess(float(np.abs(inhouse - ref).max()), 1e-3,
                        f"in-house diverges from tinygrad on {name}")
        self.assertLess(float(np.abs(imported - ref).max()), 1e-3,
                        f"import diverges from tinygrad on {name}")
        self.assertLess(float(np.abs(inhouse - imported).max()), 1e-3,
                        f"in-house vs import diverge on {name}")
    return test


for _n, _b in OPS.items():
    setattr(TestDiffInhouseVsImport, f"test_{_n}", _make(_n, _b))


if __name__ == "__main__":
    unittest.main()
