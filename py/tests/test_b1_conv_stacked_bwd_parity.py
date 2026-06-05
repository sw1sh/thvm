"""B=1 stacked-conv outer-weight-grad parity (a correctness guardrail).

x.conv2d(w1).relu().conv2d(w2), loss=(y*y).sum().  The OUTER weight grad
(d/dw2) is WRONG at batch size B==1 (thvm ssq ~62.05 vs fp64/tinygrad 30.83
for the minimal config).  B>=2 is correct.

Root: topo_sort_boundaries (src/schedule/materialize.c:3276) promotes an
inter-layer activation / upstream-grad BUFFERIZE to a realized read only when
`effectively_full = (n_ranges == out_ndim)` OR `would_strand`.  At B==1 the
size-1 batch axis is dropped from the bufferize closed_ranges (n_ranges=3,
out_ndim=4) so `effectively_full` fails AND the dropped size-1 axis leaves no
free range so `would_strand` is 0 -> the MUL/ADD nodes feeding the d/dw2
REDUCE get INLINED, recomputed, and mis-index.  tinygrad keeps these via the
shape-agnostic `buffer_in_reduce` rule (schedule/rangeify.py:269-293): if a
REDUCE in the bufferize value reaches a PARAM/BUFFERIZE, keep the bufferize
(PCONTIG<=2 -> return None), regardless of batch extent.

`h.contiguous()` between the two convs masks the bug (forces the realize).
"""
import os
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, os.environ.get("THVM_PY", str(ROOT / "py")))

from thvm import Tensor  # noqa: E402

TOL = 2e-2


def conv_np(x, w, pad):
    B, Cin, H, W = x.shape
    Co, Ci, kh, kw = w.shape
    if pad:
        xp = np.zeros((B, Cin, H + 2 * pad, W + 2 * pad), dtype=np.float64)
        xp[:, :, pad:pad + H, pad:pad + W] = x
        x = xp
        H, W = H + 2 * pad, W + 2 * pad
    Ho, Wo = H - kh + 1, W - kw + 1
    out = np.zeros((B, Co, Ho, Wo), dtype=np.float64)
    for b in range(B):
        for co in range(Co):
            for i in range(Ho):
                for j in range(Wo):
                    out[b, co, i, j] = (x[b, :, i:i + kh, j:j + kw] * w[co]).sum()
    return out


def fd_ssq_dw2(x64, w164, w264, pad, eps=1e-4):
    def loss(w2):
        h = np.maximum(conv_np(x64, w164, pad), 0.0)
        y = conv_np(h, w2, pad)
        return float((y * y).sum())
    dw2 = np.zeros_like(w264)
    it = np.nditer(w264, flags=["multi_index"])
    while not it.finished:
        idx = it.multi_index
        wp = w264.copy(); wp[idx] += eps
        wm = w264.copy(); wm[idx] -= eps
        dw2[idx] = (loss(wp) - loss(wm)) / (2 * eps)
        it.iternext()
    return float((dw2 ** 2).sum())


def thvm_ssq_dw2(x_np, w1_np, w2_np, pad):
    x = Tensor(x_np)
    w1 = Tensor(w1_np)
    w2 = Tensor(w2_np)
    h = x.conv2d(w1, padding=pad).relu()
    y = h.conv2d(w2, padding=pad)
    loss = (y * y).sum()
    loss.backward()
    return float((w2.grad.numpy().astype(np.float64) ** 2).sum())


class TestB1StackedConvBwdParity(unittest.TestCase):
    def _case(self, B, Cin, H, W, C1, C2, kH, kW, pad):
        np.random.seed(0)
        x_np = np.random.randn(B, Cin, H, W).astype(np.float32)
        w1_np = (np.random.randn(C1, Cin, kH, kW) * 0.1).astype(np.float32)
        w2_np = (np.random.randn(C2, C1, kH, kW) * 0.1).astype(np.float32)
        ref = fd_ssq_dw2(x_np.astype(np.float64), w1_np.astype(np.float64),
                         w2_np.astype(np.float64), pad)
        got = thvm_ssq_dw2(x_np, w1_np, w2_np, pad)
        self.assertLess(abs(got - ref) / ref, TOL,
                        f"B={B} dw2 ssq thvm={got:.6g} fd={ref:.6g}")

    def test_b1_minimal(self):
        self._case(1, 2, 6, 6, 3, 2, 3, 3, 1)   # FAILS on HEAD (62.05 vs 30.83)

    def test_b1_pad0(self):
        self._case(1, 3, 8, 8, 4, 5, 3, 3, 0)   # FAILS on HEAD (0.69 vs 1.68)

    def test_b2_control(self):
        self._case(2, 2, 6, 6, 3, 2, 3, 3, 1)   # passes on HEAD

    def test_b1_sweep_c2(self):
        for C2 in (1, 3, 5):
            self._case(1, 2, 6, 6, 3, C2, 3, 3, 1)  # FAILS on HEAD


if __name__ == "__main__":
    unittest.main()
