"""thvm schedules the same graphs as tinygrad 365f87a8a: equal values, and the
kernel counts in KNOWN where they still differ (equal everywhere else).

Each case builds one graph with either Tensor class from the same numpy
inputs, realized first so uploads never count.  tinygrad's count is
GlobalCounters.kernel_count after a reset (engine/realize.py:64-68); thvm's is
the delta of kernel_count().  A case whose counts change in either direction
fails, so the batch that closes a gap updates KNOWN.  The levers L2-L4
(~/.cache/thvm/plans/lever_L2_L4_rangeify_2026-10-19.md) remove the gaps.

Run as a script to print the table:
  PYTHONPATH=py:/root/tinygrad DEV=cpu python3 py/tests/test_schedule_parity.py
"""
import sys
import unittest

import numpy as np

import helpers

# (thvm kernels, tinygrad kernels, the deviation) for every case that differs,
# measured on the pod 2026-10-19 (conv2d_bwd and matmul_bwd 2026-10-25: a reshape to its
# source's shape adds no node).  "Fused row reduce": a reduce whose keepdim
# result is broadcast back over its row stays in the consumer's kernel, where
# tinygrad keeps the stage (buffer_in_reduce, schedule/rangeify.py:87-96).
KNOWN = {
    "sum_keepdim":       (2, 1, "one extra realize; L2e/L4 seeds"),
    "var":               (1, 2, "one kernel fewer: a fused row reduce"),
    "reduce_chain_diff": (2, 1, "one extra realize; L2e/L4 seeds"),
    "softmax":           (2, 3, "one kernel fewer: a fused row reduce"),
    "log_softmax":       (2, 3, "one kernel fewer: a fused row reduce"),
    "layernorm":         (2, 3, "one kernel fewer: fused row reduces, x - mean realized"),
    "matmul_bias_relu":  (2, 1, "one extra realize; the CPU BLAS operand rule is the suspect"),
    "attention":         (6, 4, "two extra realizes; L2e/L4 seeds"),
    "maxpool_bwd":       (5, 3, "two extra realizes; L2e/L4 seeds"),
    "matmul_bwd":        (4, 3, "one extra realize; L2e/L4 seeds"),
}


def _ew_chain(x, y):              return ((x + y) * x).relu() - 0.5
def _broadcast_add(x, b):         return x + b
def _sum_all(x):                  return x.sum()
def _sum_axis(x):                 return x.sum(axis=1)
def _sum_keepdim(x):              return x.sum(axis=1, keepdim=True)
def _max_axis(x):                 return x.max(axis=0)
def _var(x):                      return (x - x.mean(axis=1, keepdim=True)).square().mean(axis=1)
def _reduce_chain_same(x):        return x.sum(axis=2).sum(axis=1)
def _reduce_chain_diff(x):        return x.max(axis=2).sum(axis=1)
def _softmax(x):                  return x.softmax(axis=-1)
def _log_softmax(x):              return x.log_softmax(axis=-1)
def _layernorm(x):                return x.layernorm()
def _matmul(a, b):                return a @ b
def _matmul_bias_relu(a, b, c):   return (a @ b + c).relu()
def _conv2d(x, w):                return x.conv2d(w)
def _maxpool(x):                  return x.max_pool2d()
def _attention(q, k, v):          return (q @ k.transpose(-1, -2) * 0.25).softmax(-1) @ v
def _getitem(x):                  return x[1:3, :, 2:] + 1.0
def _permute(x):                  return x.permute(1, 0, 2) + 1.0
def _pad_shrink(x):               return x.pad(((1, 1), (0, 2))).shrink(((0, 3), (1, 5))) * 2.0
def _cat(x, y):                   return x.cat(y, dim=0) * 2.0
def _gather(x, idx):              return x.gather(1, idx)


def _conv2d_bwd(x, w):
    x.conv2d(w).sum().backward()
    return [x.grad, w.grad]


def _maxpool_bwd(x):
    x.max_pool2d().sum().backward()
    return [x.grad]


def _matmul_bwd(a, b):
    (a @ b).relu().sum().backward()
    return [a.grad, b.grad]


R = np.random.default_rng(0)


def _f(*s):
    return R.standard_normal(s).astype(np.float32)


CASES = [
    ("ew_chain", _ew_chain, [_f(4, 8), _f(4, 8)]),
    ("broadcast_add", _broadcast_add, [_f(4, 8), _f(8)]),
    ("sum_all", _sum_all, [_f(16, 8)]),
    ("sum_axis", _sum_axis, [_f(16, 8)]),
    ("sum_keepdim", _sum_keepdim, [_f(16, 8)]),
    ("max_axis", _max_axis, [_f(16, 8)]),
    ("var", _var, [_f(6, 10)]),
    ("reduce_chain_same", _reduce_chain_same, [_f(3, 4, 5)]),
    ("reduce_chain_diff", _reduce_chain_diff, [_f(3, 4, 5)]),
    ("softmax", _softmax, [_f(4, 10)]),
    ("log_softmax", _log_softmax, [_f(4, 10)]),
    ("layernorm", _layernorm, [_f(4, 16)]),
    ("matmul", _matmul, [_f(8, 16), _f(16, 4)]),
    ("matmul_bias_relu", _matmul_bias_relu, [_f(8, 16), _f(16, 4), _f(4)]),
    ("conv2d", _conv2d, [_f(2, 3, 8, 8), _f(4, 3, 3, 3)]),
    ("maxpool", _maxpool, [_f(2, 3, 8, 8)]),
    ("attention", _attention, [_f(2, 6, 8), _f(2, 6, 8), _f(2, 6, 8)]),
    ("getitem", _getitem, [_f(4, 3, 5)]),
    ("permute", _permute, [_f(4, 3, 5)]),
    ("pad_shrink", _pad_shrink, [_f(3, 4)]),
    ("cat", _cat, [_f(2, 5), _f(3, 5)]),
    ("gather", _gather, [_f(4, 6), R.integers(0, 6, (4, 3)).astype(np.int32)]),
    ("conv2d_bwd", _conv2d_bwd, [_f(2, 3, 8, 8), _f(4, 3, 3, 3)]),
    ("maxpool_bwd", _maxpool_bwd, [_f(2, 3, 8, 8)]),
    ("matmul_bwd", _matmul_bwd, [_f(8, 16), _f(16, 4)]),
    # split_reduceop (schedule/prepare.py:110-132): one split, then two.
    ("sum_split", _sum_all, [_f(256, 256)]),
    ("max_split", _max_axis, [_f(65536, 4)]),
    ("sum_split_twice", _sum_all, [_f(4096, 2048)]),
]


def _run_thvm(fn, arrays):
    from thvm import Context, Tensor
    from thvm.tensor import _TH
    xs = [Tensor(a) for a in arrays]
    with Context(TRAINING=1):
        out = fn(*xs)
        outs = out if isinstance(out, list) else [out]
        k0 = _TH.kernel_count()
        Tensor.realize(*outs)
        return _TH.kernel_count() - k0, [o.numpy() for o in outs]


def _run_tinygrad(fn, arrays):
    from tinygrad import Context, Tensor
    from tinygrad.helpers import GlobalCounters
    xs = [Tensor(a).realize() for a in arrays]
    with Context(TRAINING=1):
        out = fn(*xs)
        outs = out if isinstance(out, list) else [out]
        GlobalCounters.reset()
        Tensor.realize(*outs)
        return GlobalCounters.kernel_count, [o.numpy() for o in outs]


def table():
    rows = []
    for name, fn, arrays in CASES:
        try:
            kt, vt = _run_thvm(fn, arrays)
            kg, vg = _run_tinygrad(fn, arrays)
            ok = all(np.allclose(a, b, rtol=1e-4, atol=1e-5) for a, b in zip(vt, vg))
            rows.append((name, kt, kg, ok, ""))
        except Exception as e:  # noqa: BLE001 - the table reports it
            rows.append((name, -1, -1, False, f"{type(e).__name__}: {e}"))
    return rows


@unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not importable")
class TestScheduleParity(unittest.TestCase):
    def test_cases(self):
        for name, fn, arrays in CASES:
            with self.subTest(name=name):
                kt, vt = _run_thvm(fn, arrays)
                kg, vg = _run_tinygrad(fn, arrays)
                for a, b in zip(vt, vg):
                    np.testing.assert_allclose(a, b, rtol=1e-4, atol=1e-5)
                want = KNOWN.get(name, (kg, kg, ""))[:2]
                self.assertEqual((kt, kg), want, f"{name}: thvm {kt} kernels, tinygrad {kg}")


if __name__ == "__main__":
    if "--table" in sys.argv or len(sys.argv) == 1:
        for name, kt, kg, ok, err in table():
            print(f"PARITY {name:18s} thvm={kt:3d} tinygrad={kg:3d} values={'ok' if ok else 'BAD'} {err}")
    else:
        unittest.main()
