"""Tensor.scatter_reduce "amax" and "amin" against tinygrad master
(365f87a8a, tinygrad/mixin/op.py:1095-1135).

tinygrad builds both from the one-hot mask of _pre_scatter (:1077-1093):
  amax   mask.where(src, I).max(-1).maximum(self or _inv_mask(self, I))  :1128
  amin   mask.where(src, I).min(-1).minimum(self or _inv_mask(self, I))  :1129
with I the dtype's minimum (amax) or maximum (amin).  Its gradient is the
REDUCE MAX rule (ties split by their count, mixin/gradient.py:8-12) and the
MAX rule (a tie of the reduced value and self halves, :78-79).  On the CPU
backend thvm runs one native UOP_SCATTER(MAX / MIN) and differentiates it
with the C rule (grad_scatter_maxmin, docs/grad.md).

The reference is that dense formulation, written out in float64 numpy, and
tinygrad master itself when it is importable: forward values bit for bit
and gradients, over dims 0 / 1 / -1, include_self on and off, tie-rich src,
-inf bases and -1 / out-of-range padded ids; int32; the host fallback; and
the core cases on both CPU routes, the C JIT and the UOp walker, in child
processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_scatter_amax_parity.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Device, Tensor
from thvm import tensor as thvm_tensor
from thvm.dtypes import dtypes

NATIVE = Device.DEFAULT == "CPU"


# ---- the dense reference ------------------------------------------------------

def _dense(base, dim, idx, src, include_self):
    """tinygrad's amax operands for every output position j and one-hot
    entry k < K = idx.shape[dim]: the hit mask, the masked values (I where
    the entry misses), the reduced M, the any-hit mask and T."""
    d = dim % base.ndim
    K = idx.shape[d]
    I = -np.inf if base.dtype.kind == "f" else np.iinfo(base.dtype).min
    hit = np.zeros(base.shape + (K,), bool)
    val = np.full(base.shape + (K,), I, np.float64)
    for j in np.ndindex(*base.shape):
        for k in range(K):
            p = list(j)
            p[d] = k
            p = tuple(p)
            if all(a < s for a, s in zip(p, idx.shape)) and int(idx[p]) == j[d]:
                hit[j + (k,)] = True
                val[j + (k,)] = src[p]
    M = val.max(-1)
    anyhit = hit.any(-1)
    T = base.astype(np.float64) if include_self else np.where(anyhit, I, base.astype(np.float64))
    return d, K, hit, val, M, anyhit, T


def np_amax(base, dim, idx, src, include_self, gout=None):
    """Forward (base's dtype) and, given the cotangent gout, the gradients
    of <out, gout> for self and src, as tinygrad's autograd computes them."""
    d, K, hit, val, M, anyhit, T = _dense(base, dim, idx, src, include_self)
    out = np.where(M < T, T, M).astype(base.dtype)
    if gout is None:
        return out
    g = gout.astype(np.float64)
    gM = np.where(M > T, g, np.where(M == T, 0.5 * g, 0.0))
    gT = np.where(T > M, g, np.where(M == T, 0.5 * g, 0.0))
    ties = (val == M[..., None]).astype(np.float64)
    gval = ties / ties.sum(-1, keepdims=True) * gM[..., None]
    g_self = gT if include_self else np.where(anyhit, 0.0, gT)
    g_src = np.zeros(src.shape)
    for j in np.ndindex(*base.shape):
        for k in range(K):
            if hit[j + (k,)]:
                p = list(j)
                p[d] = k
                g_src[tuple(p)] += gval[j + (k,)]
    return out, g_self, g_src


def np_scatter_extreme(reduce, base, dim, idx, src, include_self, gout=None):
    """amin is amax on the negated (float) or inverted (int) operands, as
    tinygrad's min is (mixin/op.py:494, elementwise.py:404-408); the
    cotangent passes through unchanged."""
    if reduce == "amax":
        return np_amax(base, dim, idx, src, include_self, gout)
    neg = (lambda a: -a) if base.dtype.kind == "f" else (lambda a: ~a)
    r = np_amax(neg(base), dim, idx, neg(src), include_self, gout)
    if gout is None:
        return neg(r)
    return (neg(r[0]),) + r[1:]


# ---- cases (module level: the route tests run them in child processes) -----

def extreme_inputs(dim, seed):
    """(3, 4, 5) self and src drawn from a few half-integers (ties are
    common), some -inf bases, and an index in [-1, size] along dim: -1 and
    size are padding, dropped."""
    rng = np.random.default_rng(seed)
    shape = (3, 4, 5)
    size = shape[dim % 3]
    x0 = (rng.integers(-3, 4, shape) / 2).astype(np.float32)
    x0[rng.random(shape) < 0.2] = -np.inf
    s0 = (rng.integers(-3, 4, shape) / 2).astype(np.float32)
    b = rng.integers(-1, size + 1, shape).astype(np.int32)
    w0 = rng.uniform(-1, 1, shape).astype(np.float32)
    return x0, s0, b, w0


def case_extreme(reduce, dim, include_self, seed=0):
    """Forward bit for bit and both gradients of sum(w * out) against the
    dense reference."""
    x0, s0, b, w0 = extreme_inputs(dim, seed)
    x, src = Tensor(x0), Tensor(s0)
    out = x.scatter_reduce(dim, Tensor(b), src, reduce, include_self=include_self)
    assert out.shape == x0.shape and out.dtype == dtypes.float32
    (out * Tensor(w0)).sum().backward()
    want, g_self, g_src = np_scatter_extreme(reduce, x0, dim, b, s0, include_self, w0)
    np.testing.assert_array_equal(out.numpy(), want)
    np.testing.assert_allclose(x.grad.numpy(), g_self, rtol=1e-6, atol=1e-7)
    np.testing.assert_allclose(src.grad.numpy(), g_src, rtol=1e-6, atol=1e-7)


def case_extreme_tinygrad(reduce, dim, include_self, seed=0):
    """The same against tinygrad master's forward and autograd."""
    from tinygrad import Tensor as TgTensor
    from tinygrad import dtypes as tg_dtypes
    x0, s0, b, w0 = extreme_inputs(dim, seed)
    x, src = Tensor(x0), Tensor(s0)
    out = x.scatter_reduce(dim, Tensor(b), src, reduce, include_self=include_self)
    (out * Tensor(w0)).sum().backward()
    tx, tsrc = TgTensor(x0), TgTensor(s0)
    tout = tx.scatter_reduce(dim, TgTensor(b, dtype=tg_dtypes.int32), tsrc, reduce,
                             include_self=include_self)
    (tout * TgTensor(w0)).sum().backward()
    np.testing.assert_array_equal(out.numpy(), tout.numpy())
    np.testing.assert_allclose(x.grad.numpy(), tx.grad.numpy(), rtol=1e-6, atol=1e-7)
    np.testing.assert_allclose(src.grad.numpy(), tsrc.grad.numpy(), rtol=1e-6, atol=1e-7)


def case_int32(reduce, include_self):
    rng = np.random.default_rng(3)
    base = rng.integers(-50, 50, (4, 6)).astype(np.int32)
    base[0, 0] = np.iinfo(np.int32).min
    base[1, 1] = np.iinfo(np.int32).max
    src = rng.integers(-50, 50, (5, 6)).astype(np.int32)
    idx = rng.integers(-1, 5, (5, 6)).astype(np.int32)
    out = Tensor(base).scatter_reduce(0, Tensor(idx), Tensor(src), reduce,
                                      include_self=include_self)
    assert out.dtype == dtypes.int32
    got = out.numpy()
    assert got.dtype == np.int32
    np.testing.assert_array_equal(got, np_scatter_extreme(reduce, base, 0, idx, src, include_self))


def case_segment_max(seed=4):
    """The ATP listwise shift (tools/atp_enigma/listwise_net.py list_loss):
    full((S, B), -BIG).scatter_reduce(1, seg, X, "amax") with BIG = 1e4,
    each step's R scores split into B segments along dim 1, some rows
    padding at -BIG."""
    rng = np.random.default_rng(seed)
    S, B, R, BIG = 4, 3, 24, 1e4
    seg = np.sort(rng.integers(0, B, (S, R)), axis=1).astype(np.int32)
    x0 = rng.standard_normal((S, R)).astype(np.float32)
    x0[rng.random((S, R)) < 0.25] = -BIG
    for reduce, fill in (("amax", -BIG), ("amin", BIG)):
        m = Tensor.full((S, B), fill).scatter_reduce(1, Tensor(seg), Tensor(x0), reduce)
        want = np_scatter_extreme(reduce, np.full((S, B), fill, np.float32), 1, seg, x0, True)
        np.testing.assert_array_equal(m.numpy(), want)


ROUTE_CASES = [
    ("amax-dim0-self", lambda: case_extreme("amax", 0, True)),
    ("amax-dim1-noself", lambda: case_extreme("amax", 1, False)),
    ("amin-dim-1-self", lambda: case_extreme("amin", -1, True)),
    ("amin-dim0-noself", lambda: case_extreme("amin", 0, False)),
    ("int32-amax-noself", lambda: case_int32("amax", False)),
    ("int32-amin-self", lambda: case_int32("amin", True)),
    ("segment-max", case_segment_max),
]
if helpers.have_tinygrad():
    ROUTE_CASES += [
        ("tg-amax-dim-1-noself", lambda: case_extreme_tinygrad("amax", -1, False, seed=5)),
        ("tg-amin-dim1-self", lambda: case_extreme_tinygrad("amin", 1, True, seed=6)),
    ]


# ---- tests ------------------------------------------------------------------

@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestScatterExtreme(unittest.TestCase):
    def test_against_reference(self):
        for reduce in ("amax", "amin"):
            for dim in (0, 1, -1):
                for include_self in (True, False):
                    for seed in (0, 1):
                        with self.subTest(reduce=reduce, dim=dim, include_self=include_self,
                                          seed=seed):
                            case_extreme(reduce, dim, include_self, seed)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_against_tinygrad(self):
        for reduce in ("amax", "amin"):
            for dim in (0, 1, -1):
                for include_self in (True, False):
                    for seed in (0, 1):
                        with self.subTest(reduce=reduce, dim=dim, include_self=include_self,
                                          seed=seed):
                            case_extreme_tinygrad(reduce, dim, include_self, seed)

    def test_reference_is_tinygrad(self):
        # The dense reference reproduces tinygrad's own docstring examples
        # (mixin/op.py:1118-1123).
        src = np.arange(1, 11, dtype=np.float32).reshape(2, 5)
        idx = np.zeros((2, 5), np.int32)
        base = np.array([[-10, 20, 0, 5, 10]], np.float32)
        np.testing.assert_array_equal(np_scatter_extreme("amax", base, 0, idx, src, True),
                                      [[6, 20, 8, 9, 10]])
        np.testing.assert_array_equal(np_scatter_extreme("amin", base, 0, idx, src, True),
                                      [[-10, 2, 0, 4, 5]])
        for reduce, want in (("amax", [[6, 20, 8, 9, 10]]), ("amin", [[-10, 2, 0, 4, 5]])):
            got = Tensor(base).scatter_reduce(0, Tensor(idx), Tensor(src), reduce).numpy()
            np.testing.assert_array_equal(got, want)

    def test_int32(self):
        for reduce in ("amax", "amin"):
            for include_self in (True, False):
                with self.subTest(reduce=reduce, include_self=include_self):
                    case_int32(reduce, include_self)

    def test_segment_max(self):
        case_segment_max()

    def test_ties_split(self):
        # Three equal maxima on one position share its gradient; with
        # include_self a base equal to them takes half and they split the
        # other half (mixin/gradient.py:8-12, :78-79).
        base = np.array([2.0, 0.0], np.float32)
        src = np.array([2.0, 2.0, 2.0, 1.0], np.float32)
        idx = np.array([0, 0, 0, 1], np.int32)
        for include_self, gx, gs in ((True, [0.5, 0.0], [1 / 6, 1 / 6, 1 / 6, 1.0]),
                                     (False, [0.0, 0.0], [1 / 3, 1 / 3, 1 / 3, 1.0])):
            x, s = Tensor(base), Tensor(src)
            out = x.scatter_reduce(0, Tensor(idx), s, "amax", include_self=include_self)
            out.sum().backward()
            np.testing.assert_array_equal(out.numpy(), [2.0, 1.0])
            np.testing.assert_allclose(x.grad.numpy(), gx, rtol=1e-6)
            np.testing.assert_allclose(s.grad.numpy(), gs, rtol=1e-6)

    def test_empty_index_keeps_self(self):
        x0 = np.array([[-0.0, 1, -np.inf], [2, -0.0, 3]], np.float32)
        idx = Tensor(np.zeros((0, 3), np.int32))
        src = Tensor(np.zeros((0, 3), np.float32))
        for reduce in ("amax", "amin"):
            for inc in (True, False):
                got = Tensor(x0).scatter_reduce(0, idx, src, reduce, include_self=inc).numpy()
                np.testing.assert_array_equal(got, x0)
                np.testing.assert_array_equal(np.signbit(got), np.signbit(x0))

    def test_errors(self):
        # tinygrad's checks and exception types (mixin/op.py:1077-1093, :1135).
        x = Tensor(np.zeros((3, 3), np.float32))
        idx = Tensor(np.array([[0, 1, 0], [2, 0, 1]], np.int32))
        for reduce in ("amax", "amin"):
            with self.assertRaises(RuntimeError):
                x.scatter_reduce(0, idx, Tensor(np.ones((2, 3), np.int32)), reduce)
            with self.assertRaises(RuntimeError):
                x.scatter_reduce(0, idx.cast(dtypes.float32), Tensor(np.ones((2, 3), np.float32)),
                                 reduce)
            with self.assertRaises(IndexError):
                x.scatter_reduce(2, idx, Tensor(np.ones((2, 3), np.float32)), reduce)


class TestScatterExtremeHost(unittest.TestCase):
    # Off the CPU backend amax / amin run on the host with the same fold.
    def test_host_values(self):
        saved = thvm_tensor._native_indexing
        thvm_tensor._native_indexing = lambda: False
        try:
            for reduce in ("amax", "amin"):
                for dim in (0, 1, -1):
                    for include_self in (True, False):
                        x0, s0, b, _ = extreme_inputs(dim, 2)
                        got = Tensor(x0).scatter_reduce(dim, Tensor(b), Tensor(s0), reduce,
                                                        include_self=include_self).numpy()
                        np.testing.assert_array_equal(
                            got, np_scatter_extreme(reduce, x0, dim, b, s0, include_self))
            for reduce in ("amax", "amin"):
                case_int32(reduce, False)
        finally:
            thvm_tensor._native_indexing = saved


# ---- both CPU routes ----------------------------------------------------------

@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_scatter_amax_parity"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
