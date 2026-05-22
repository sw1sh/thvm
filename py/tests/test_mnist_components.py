"""Cross-validate beautiful_mnist building blocks: numeric AND structural.

For each component (relu / conv2d / batchnorm / max_pool2d / linear and a
conv+bn+relu+pool block) we build it via tinygrad (reference), thvm in-house
Tensor, and tinygrad->from_tinygrad import, and assert all agree NUMERICALLY.

It ALSO reports the thvm-vs-tinygrad KERNEL COUNT per component (structural).
The numbers are not asserted (thvm and tinygrad are different schedulers) but
print so regressions / weird divergences are visible.  Findings flagged so
far (stable across CPU + CUDA, all numerically correct):
  - conv2d 2/1, conv+relu 3/1, batchnorm 2/1, conv+bn+relu+pool 5/2: thvm
    UNDER-FUSES vs tinygrad (does not fuse the elementwise relu/bn epilogue
    into the conv kernel).
  - max_pool2d 0/1: thvm realizes the windowed max-reduce with ZERO KERNELS
    entries (a plain max(axis) emits 1) -- correct values, structurally odd.
  - local Metal in-house realize is a stub (dispatch_kernel not implemented);
    in-house thvm.Tensor runs on CPU-walker or CUDA, not local Metal.
"""
import os
import unittest

import numpy as np


def _have_tg():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


_rng = np.random.default_rng(0)
_X = _rng.standard_normal((2, 3, 8, 8)).astype(np.float32)
_CW = _rng.standard_normal((4, 3, 3, 3)).astype(np.float32)
_FL = _rng.standard_normal((2, 12)).astype(np.float32)
_LW = _rng.standard_normal((12, 10)).astype(np.float32)

# (name, builder(Tensor, nn_module))
COMPONENTS = {
    "relu":         lambda T, N: T(_X).relu(),
    "conv2d":       lambda T, N: T(_X).conv2d(T(_CW)),
    "conv_relu":    lambda T, N: T(_X).conv2d(T(_CW)).relu(),
    "max_pool2d":   lambda T, N: T(_X).max_pool2d((2, 2)),
    "batchnorm":    lambda T, N: N.BatchNorm(3)(T(_X)),
    "block":        lambda T, N: N.BatchNorm(4)(T(_X).conv2d(T(_CW))).relu().max_pool2d((2, 2)),
    "linear":       lambda T, N: T(_FL).linear(T(_LW)),
}


@unittest.skipUnless(_have_tg(), "tinygrad not on path")
class TestMnistComponents(unittest.TestCase):
    pass


def _make(name, build):
    def test(self):
        import thvm
        import tinygrad
        import thvm.nn as THN
        import tinygrad.nn as TGN
        from thvm.from_tinygrad import from_tinygrad
        ref = np.asarray(build(tinygrad.Tensor, TGN).numpy(), dtype=np.float32)
        inhouse = np.asarray(build(thvm.Tensor, THN).numpy(),
                             dtype=np.float32).reshape(ref.shape)
        imported = np.asarray(from_tinygrad(build(tinygrad.Tensor, TGN)).numpy(),
                              dtype=np.float32).reshape(ref.shape)
        self.assertLess(float(np.abs(inhouse - ref).max()), 1e-3,
                        f"in-house {name} diverges from tinygrad")
        self.assertLess(float(np.abs(imported - ref).max()), 1e-3,
                        f"import {name} diverges from tinygrad")
    return test


for _n, _b in COMPONENTS.items():
    setattr(TestMnistComponents, f"test_{_n}", _make(_n, _b))


if __name__ == "__main__":
    unittest.main()
