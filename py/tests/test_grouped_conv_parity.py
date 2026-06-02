"""Grouped/depthwise conv2d parity vs tinygrad (the spec), fwd + bwd.

thvm's conv2d hard-raises NotImplementedError for groups != 1 (tensor.py
conv2d, "Phase 3C"). tinygrad supports grouped/depthwise conv by reshaping
the input to (bs, groups, cin, ...) and the weight to (groups, rcout, cin,
...), broadcasting the per-group rcout axis, multiplying, and summing over
(cin, *HW) -- exactly the groups==1 unfold generalized to G groups
(mixin/__init__.py:1235 conv2d).

These tests build the SAME grouped conv via thvm's in-house Tensor and via
tinygrad and assert they agree to fp tolerance, both forward (output) and
backward (input-grad + weight-grad).  They are EXPECTED TO FAIL at HEAD
(thvm raises NotImplementedError); left red for whoever lands the grouped
path.  groups==1 must stay byte-identical (covered elsewhere); the new path
only fires for groups>1, including depthwise (groups == cin).

    make -C <repo> py && python -m pytest py/tests/test_grouped_conv_parity.py
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
PY = str(ROOT / "py")
sys.path.insert(0, os.environ.get("THVM_PY", PY))


def _have_tg():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


# (name, N, Cin, Cout, H, K, groups)  -- Cin = groups * cin_per_group,
# Cout divisible by groups.  depthwise = groups == Cin (and Cout == Cin).
CASES = {
    "groups2":        (2, 4, 6, 8, 3, 2),
    "groups3":        (2, 6, 9, 8, 3, 3),
    "depthwise":      (2, 4, 4, 8, 3, 4),   # groups == Cin == Cout
    "depthwise_mult": (2, 3, 6, 7, 3, 3),   # groups == Cin, Cout = 2*Cin
}


@unittest.skipUnless(_have_tg(), "tinygrad not on path")
class TestGroupedConvParity(unittest.TestCase):
    pass


def _make(name, cfg):
    N, Cin, Cout, H, K, G = cfg

    def test(self):
        import thvm
        import tinygrad

        rng = np.random.default_rng(0)
        X = rng.standard_normal((N, Cin, H, H)).astype(np.float32)
        # weight: (Cout, Cin // groups, K, K)
        W = (rng.standard_normal((Cout, Cin // G, K, K)) * 0.3).astype(np.float32)

        # ---- tinygrad reference (fwd + bwd) ----
        # NB: backward() BEFORE reading the forward output -- in thvm,
        # calling .numpy() on a tensor realizes+detaches it, severing the
        # autograd tape (pre-existing, affects groups==1 too).  Mirror
        # the ordering on both backends for an apples-to-apples compare.
        xt = tinygrad.Tensor(X, requires_grad=True)
        wt = tinygrad.Tensor(W, requires_grad=True)
        yt = xt.conv2d(wt, groups=G)
        yt.sum().backward()
        ref = yt.numpy()
        gx_tg = xt.grad.numpy()
        gw_tg = wt.grad.numpy()

        # ---- thvm in-house (fwd + bwd) ----
        xh = thvm.Tensor(X.copy()).requires_grad_(True)
        wh = thvm.Tensor(W.copy()).requires_grad_(True)
        yh = xh.conv2d(wh, groups=G)
        yh.sum().backward()
        out = np.asarray(yh.numpy(), np.float32).reshape(ref.shape)
        gx_th = np.asarray(xh.grad.numpy(), np.float32).reshape(gx_tg.shape)
        gw_th = np.asarray(wh.grad.numpy(), np.float32).reshape(gw_tg.shape)

        self.assertLess(float(np.abs(out - ref).max()), 1e-3,
                        f"grouped conv {name} fwd diverges from tinygrad")
        self.assertLess(float(np.abs(gx_th - gx_tg).max()), 1e-3,
                        f"grouped conv {name} input-grad diverges from tinygrad")
        self.assertLess(float(np.abs(gw_th - gw_tg).max()), 1e-3,
                        f"grouped conv {name} weight-grad diverges from tinygrad")

    return test


for _n, _c in CASES.items():
    setattr(TestGroupedConvParity, f"test_{_n}", _make(_n, _c))


if __name__ == "__main__":
    unittest.main()
