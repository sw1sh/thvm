"""Tensor.scatter_reduce ("sum", "mean") and Tensor.scatter against
tinygrad's semantics (master 365f87a8a).

tinygrad builds both from a one-hot mask (tinygrad/mixin/op.py):
  _pre_scatter         the checks, then src.shrink_to(index.shape)   :1077-1093
  scatter_reduce sum   S + self, or S + (self where nothing lands)   :1127
  scatter_reduce mean  that sum / (hits + 1), or / (hits + 1 where
                       nothing lands) without include_self           :1131-1133
  scatter              reduce=None keeps the last write along dim    :1162-1168
where S sums src over the valid positions that land on an output position.
On the CPU backend thvm runs them as native UOP_SCATTER kernels and
differentiates them with the C rules: src gets the cotangent gathered back,
self gets it whole, or with the written positions zeroed.

Ports, with numpy as the reference where tinygrad's tests use torch (and
tinygrad master itself when it is importable):
  - test/runtime/test_ops.py:3008 test_scatter (forward and error types);
  - :3041 test_scatter_add (inf and nan sources);
  - :3066 test_scatter_reduce for sum and mean x dims x include_self, plus the
    backward those tests skip (forward_only): against tinygrad's autograd
    through its own one-hot formulation, an analytic numpy gradient and
    central finite differences of a quadratic loss.
Then the ATP trainers' patterns (tools/atp_enigma): history_net.py's
aggregation with -1 padded indices and listwise_net.py's segment
log-sum-exp along dim 1; TinyJit replays with a fresh index buffer per call;
the host fallback; and the core cases on both CPU routes, the C JIT and the
UOp walker, in child processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_scatter_reduce_parity.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Context, Device, Tensor, TinyJit, nn
from thvm import tensor as thvm_tensor
from thvm.dtypes import dtypes

NATIVE = Device.DEFAULT == "CPU"


# ---- numpy references -------------------------------------------------------

def _target(p, d, i):
    q = list(p)
    q[d] = i
    return tuple(q)


def np_hits(shape, dim, idx):
    """How many valid index positions land on each output position."""
    d = dim % len(shape)
    hits = np.zeros(shape, np.int64)
    for p in np.ndindex(*idx.shape):
        i = int(idx[p])
        if 0 <= i < shape[d]:
            hits[_target(p, d, i)] += 1
    return hits


def np_scatter_reduce(base, dim, idx, src, reduce, include_self=True):
    """tinygrad's sum / prod / mean algebra (mixin/op.py:1127-1128, :1131-1133),
    folded in float64 (int64 for an int base).  A float base keeps its dtype; an
    int mean is float32, as tinygrad's int division is."""
    d = dim % base.ndim
    is_float = base.dtype.kind == "f"
    acc = np.float64 if is_float else np.int64
    fold, ident = (np.multiply, 1) if reduce == "prod" else (np.add, 0)
    S = np.full(base.shape, ident, acc)
    for p in np.ndindex(*idx.shape):
        i = int(idx[p])
        if 0 <= i < base.shape[d]:
            t = _target(p, d, i)
            S[t] = fold(S[t], src[p])
    hits = np_hits(base.shape, dim, idx)
    b = base.astype(acc)
    total = fold(S, b if include_self else np.where(hits == 0, b, ident))
    if reduce in ("sum", "prod"):
        return total.astype(base.dtype)
    count = hits + (1 if include_self else (hits == 0))
    return (total / count).astype(base.dtype if is_float else np.float32)


def np_scatter_replace(base, dim, idx, src):
    """tinygrad's scatter with reduce=None (_masked_merge, mixin/op.py:1170):
    the last valid write along dim wins, base where nothing lands.  Positions
    that meet differ only along dim, so row-major order is that order."""
    d = dim % base.ndim
    out = base.copy()
    for p in np.ndindex(*idx.shape):
        i = int(idx[p])
        if 0 <= i < base.shape[d]:
            out[_target(p, d, i)] = src[p]
    return out


def np_scatter_reduce_grad(shape, dim, idx, reduce, include_self, gout):
    """Gradients of <out, gout> with respect to (self, src) for sum and mean:
    src[p] gets gout at its target (over the count for mean) and 0 when its
    index is invalid; self gets gout (over the count), zeroed where written
    unless include_self."""
    d = dim % len(shape)
    hits = np_hits(shape, dim, idx)
    count = (np.ones(shape) if reduce == "sum"
             else (hits + (1 if include_self else (hits == 0))).astype(np.float64))
    g = gout.astype(np.float64) / count
    g_self = g if include_self else np.where(hits == 0, g, 0.0)
    g_src = np.zeros(idx.shape)
    for p in np.ndindex(*idx.shape):
        i = int(idx[p])
        if 0 <= i < shape[d]:
            g_src[p] = g[_target(p, d, i)]
    return g_self, g_src


def fd_grad(f, x, eps=1e-2):
    """Central finite differences of f at x, in float64.  The losses below are
    quadratic in x, so the differences are exact up to rounding."""
    x = x.astype(np.float64)
    g = np.zeros_like(x)
    for p in np.ndindex(*x.shape):
        xp, xm = x.copy(), x.copy()
        xp[p] += eps
        xm[p] -= eps
        g[p] = (f(xp) - f(xm)) / (2 * eps)
    return g


def padded_ids(rng, n, hi, frac=0.3):
    """n ids in [0, hi) with about frac of them -1 (the ATP padding id)."""
    ids = rng.integers(0, hi, n)
    ids[rng.random(n) < frac] = -1
    return ids


# ---- cases (module level: the route tests run them in child processes) -----

def case_reduce(reduce, dim, include_self, seed=0):
    """test_ops.py:3066 for one (reduce, dim, include_self): (3,4,5) self and
    src, a (3,4,5) index in [0, 3).  Forward against numpy, and the gradient
    of sum(w * out^2) against the analytic gradient and finite differences."""
    rng = np.random.default_rng(seed)
    b = rng.integers(0, 3, (3, 4, 5)).astype(np.int32)
    x0 = rng.uniform(-2, 2, (3, 4, 5)).astype(np.float32)
    s0 = rng.uniform(-2, 2, (3, 4, 5)).astype(np.float32)
    w0 = rng.uniform(-1, 1, (3, 4, 5)).astype(np.float32)
    x, src, w = Tensor(x0), Tensor(s0), Tensor(w0)
    out = x.scatter_reduce(dim, Tensor(b), src, reduce, include_self=include_self)
    assert out.shape == x0.shape and out.dtype == dtypes.float32
    # Differentiate before realizing: a realized tensor is a leaf.
    (out * out * w).sum().backward()
    got = out.numpy()
    want = np_scatter_reduce(x0, dim, b, s0, reduce, include_self)
    np.testing.assert_allclose(got, want, rtol=1e-6, atol=1e-6)
    gout = 2.0 * w0.astype(np.float64) * np_scatter_reduce(
        x0.astype(np.float64), dim, b, s0.astype(np.float64), reduce, include_self)
    g_self, g_src = np_scatter_reduce_grad(x0.shape, dim, b, reduce, include_self, gout)
    np.testing.assert_allclose(x.grad.numpy(), g_self, rtol=1e-5, atol=1e-5)
    np.testing.assert_allclose(src.grad.numpy(), g_src, rtol=1e-5, atol=1e-5)

    def loss(xv, sv):
        y = np_scatter_reduce(xv, dim, b, sv, reduce, include_self)
        return float((w0 * y * y).sum())
    s64 = s0.astype(np.float64)
    x64 = x0.astype(np.float64)
    np.testing.assert_allclose(x.grad.numpy(), fd_grad(lambda v: loss(v, s64), x64),
                               rtol=1e-4, atol=1e-4)
    np.testing.assert_allclose(src.grad.numpy(), fd_grad(lambda v: loss(x64, v), s64),
                               rtol=1e-4, atol=1e-4)


def case_padded_indices(reduce, include_self, seed=1):
    """An index outside [0, self.shape[dim]) is inert: never written, not
    counted by mean, and its src position gets no gradient."""
    rng = np.random.default_rng(seed)
    N, M, D = 6, 10, 3
    ids = np.array([2, -1, 0, 6, 2, -19238, 5, 10982377, 2, -7])
    idx = np.repeat(ids[:, None], D, 1).astype(np.int32)
    x0 = rng.standard_normal((N, D)).astype(np.float32)
    s0 = rng.standard_normal((M, D)).astype(np.float32)
    g0 = rng.standard_normal((N, D)).astype(np.float32)
    x, src = Tensor(x0), Tensor(s0)
    out = x.scatter_reduce(0, Tensor(idx), src, reduce, include_self=include_self)
    (out * Tensor(g0)).sum().backward()
    np.testing.assert_allclose(out.numpy(),
                               np_scatter_reduce(x0, 0, idx, s0, reduce, include_self),
                               rtol=1e-6, atol=1e-6)
    g_self, g_src = np_scatter_reduce_grad(x0.shape, 0, idx, reduce, include_self, g0)
    np.testing.assert_allclose(x.grad.numpy(), g_self, rtol=1e-6, atol=1e-6)
    np.testing.assert_allclose(src.grad.numpy(), g_src, rtol=1e-6, atol=1e-6)
    assert not src.grad.numpy()[(ids < 0) | (ids >= N)].any()


def case_aggregation(seed=2):
    """The ATP history-net aggregation (tools/atp_enigma/history_net.py):
    agg = zeros(P, D).scatter_reduce(0, own, T.gather(0, mem) * w, "sum") with
    -1 padded own and mem.  A padded mem reads a zero row; a padded own drops
    its message.  Forward and the gradients of T and w against numpy."""
    rng = np.random.default_rng(seed)
    P, M, R, D = 5, 12, 7, 4
    own = padded_ids(rng, M, P)
    mem = padded_ids(rng, M, R)
    T0 = rng.standard_normal((R, D)).astype(np.float32)
    w0 = rng.uniform(0.2, 1.0, (M, 1)).astype(np.float32)
    g0 = rng.standard_normal((P, D)).astype(np.float32)
    T, w = Tensor(T0), Tensor(w0)
    own_e = Tensor(np.repeat(own[:, None], D, 1).astype(np.int32))
    mem_e = Tensor(np.repeat(mem[:, None], D, 1).astype(np.int32))
    agg = Tensor.zeros(P, D).scatter_reduce(0, own_e, T.gather(0, mem_e) * w, reduce="sum")
    (agg * Tensor(g0)).sum().backward()
    rows = np.where((mem >= 0)[:, None], T0[np.maximum(mem, 0)], 0.0)
    msg = rows * w0
    want = np.zeros((P, D))
    gT = np.zeros((R, D))
    gw = np.zeros((M, 1))
    for m in range(M):
        if own[m] >= 0:
            want[own[m]] += msg[m]
            gw[m, 0] = (g0[own[m]] * rows[m]).sum()
            if mem[m] >= 0:
                gT[mem[m]] += w0[m, 0] * g0[own[m]]
    np.testing.assert_allclose(agg.numpy(), want, rtol=1e-6, atol=1e-6)
    np.testing.assert_allclose(T.grad.numpy(), gT, rtol=1e-6, atol=1e-6)
    np.testing.assert_allclose(w.grad.numpy(), gw, rtol=1e-6, atol=1e-6)


def segment_inputs(seed=13):
    """S steps of R scores, each step's positions split into B nonempty
    segments along dim 1 (index.shape[1] = R > B = self.shape[1])."""
    rng = np.random.default_rng(seed)
    S, B, R = 3, 4, 16
    seg = np.sort(np.concatenate([np.tile(np.arange(B), (S, 1)),
                                  rng.integers(0, B, (S, R - B))], axis=1), axis=1)
    x0 = (3 * rng.standard_normal((S, R))).astype(np.float32)
    sw0 = rng.uniform(0.5, 1.0, (S, B)).astype(np.float32)
    return seg.astype(np.int32), x0, sw0


def case_segment_lse(seed=13):
    """The ATP listwise loss (tools/atp_enigma/listwise_net.py list_loss):
    sum over (step k, segment b) of sw[k, b] * log(sum exp(x[k, r]) over the
    r with seg[k, r] = b), each segment shifted by a detached statistic: a
    segment mean here, the trainer's segment max in
    test_segment_log_softmax.py; both give the same log-sum-exp.  Forward and the gradient (sw
    times the in-segment softmax) against numpy."""
    seg, x0, sw0 = segment_inputs(seed)
    S, R = x0.shape
    B = sw0.shape[1]
    X, segt = Tensor(x0), Tensor(seg)
    m = Tensor.zeros(S, B).scatter_reduce(1, segt, X.detach(), "mean", include_self=False)
    den = Tensor.zeros(S, B).scatter_reduce(1, segt, (X - m.gather(1, segt)).exp(), "sum")
    loss = (Tensor(sw0) * ((den + 1e-30).log() + m)).sum()
    loss.backward()
    want, g = 0.0, np.zeros((S, R))
    for k in range(S):
        for b in range(B):
            xs = x0[k, seg[k] == b].astype(np.float64)
            lse = xs.max() + np.log(np.exp(xs - xs.max()).sum())
            want += sw0[k, b] * lse
            g[k, seg[k] == b] = sw0[k, b] * np.exp(xs - lse)
    np.testing.assert_allclose(float(loss.numpy()), want, rtol=1e-5)
    np.testing.assert_allclose(X.grad.numpy(), g, rtol=1e-5, atol=1e-6)


def case_scatter_replace(seed=3):
    """scatter with reduce=None: the last write along dim wins, invalid ids
    write nothing, a scalar src fills index's shape (test_ops.py:3008)."""
    rng = np.random.default_rng(seed)
    x0 = rng.standard_normal((4, 5, 6)).astype(np.float32)
    s0 = rng.standard_normal((4, 5, 6)).astype(np.float32)
    b = rng.integers(-1, 4, (3, 4, 5)).astype(np.int32)
    for dim in (0, 1, 2):
        got = Tensor(x0).scatter(dim, Tensor(b), Tensor(s0)).numpy()
        np.testing.assert_array_equal(got, np_scatter_replace(x0, dim, b, s0))
    got = Tensor(x0).scatter(1, Tensor(b), 3).numpy()
    np.testing.assert_array_equal(got, np_scatter_replace(x0, 1, b, np.full(b.shape, 3.0)))


def case_scatter_add_inf_nan(seed=4):
    """test_ops.py:3041: a scalar inf or nan src added where an index lands;
    positions no valid index names keep self, finite."""
    rng = np.random.default_rng(seed)
    x0 = rng.standard_normal((4, 5, 6)).astype(np.float32)
    b = rng.integers(-1, 4, (3, 4, 5)).astype(np.int32)
    for v in (float("inf"), float("-inf"), float("nan")):
        got = Tensor(x0).scatter(1, Tensor(b), v, reduce="add").numpy()
        want = np_scatter_reduce(x0, 1, b, np.full(b.shape, v, np.float32), "sum")
        np.testing.assert_array_equal(got, want)
        untouched = np_hits(x0.shape, 1, b) == 0
        np.testing.assert_array_equal(got[untouched], x0[untouched])


def case_jit_fresh_index(reduce, include_self, seed=5):
    """TinyJit replays a scatter_reduce with a fresh index (and src) buffer
    per call, ids out of range included."""
    rng = np.random.default_rng(seed)
    N, M, D = 7, 11, 3
    x0 = rng.standard_normal((N, D)).astype(np.float32)
    x = Tensor(x0).realize()

    @TinyJit
    def f(idx, src):
        return x.scatter_reduce(0, idx, src, reduce, include_self=include_self).realize()

    for _ in range(5):
        ids = rng.integers(-1, N + 1, M)
        idx = np.repeat(ids[:, None], D, 1).astype(np.int32)
        s0 = rng.standard_normal((M, D)).astype(np.float32)
        got = f(Tensor(idx), Tensor(s0)).numpy()
        np.testing.assert_allclose(got, np_scatter_reduce(x0, 0, idx, s0, reduce, include_self),
                                   rtol=1e-6, atol=1e-6)


def case_jit_train_aggregation(seed=6):
    """SGD under TinyJit through the ATP aggregation, fresh -1 padded own /
    mem buffers each call: the gather and the scatter-add, forward and
    backward, replay against the new indices."""
    rng = np.random.default_rng(seed)
    P, M, R, D, lr = 5, 12, 7, 4, 0.1
    T0 = rng.standard_normal((R, D)).astype(np.float32)
    g0 = rng.standard_normal((P, D)).astype(np.float32)
    T = Tensor(T0.copy()).realize()
    gt = Tensor(g0).realize().detach()
    with Context(TRAINING=1):
        opt = nn.optim.SGD([T], lr=lr)

        @TinyJit
        def step(own_e, mem_e, w):
            opt.zero_grad()
            agg = Tensor.zeros(P, D).scatter_reduce(0, own_e, T.gather(0, mem_e) * w,
                                                    reduce="sum")
            loss = (agg * gt).sum()
            loss.backward()
            return loss.realize(*opt.schedule_step())

        want = T0.astype(np.float64)
        for _ in range(6):
            own, mem = padded_ids(rng, M, P), padded_ids(rng, M, R)
            w0 = rng.uniform(0.2, 1.0, (M, 1)).astype(np.float32)
            step(Tensor(np.repeat(own[:, None], D, 1).astype(np.int32)),
                 Tensor(np.repeat(mem[:, None], D, 1).astype(np.int32)), Tensor(w0))
            grad = np.zeros((R, D))
            for m in range(M):
                if own[m] >= 0 and mem[m] >= 0:
                    grad[mem[m]] += w0[m, 0] * g0[own[m]]
            want = want - lr * grad
            np.testing.assert_allclose(T.numpy(), want, rtol=1e-5, atol=1e-5)


ROUTE_CASES = [
    ("sum-dim1-self", lambda: case_reduce("sum", 1, True)),
    ("sum-dim-1-noself", lambda: case_reduce("sum", -1, False)),
    ("mean-dim-3-self", lambda: case_reduce("mean", -3, True)),
    ("mean-dim1-noself", lambda: case_reduce("mean", 1, False)),
    ("padded-sum-noself", lambda: case_padded_indices("sum", False)),
    ("padded-mean-self", lambda: case_padded_indices("mean", True)),
    ("aggregation", case_aggregation),
    ("segment-lse", case_segment_lse),
    ("replace", case_scatter_replace),
    ("add-inf-nan", case_scatter_add_inf_nan),
    ("jit-mean-noself", lambda: case_jit_fresh_index("mean", False)),
    ("jit-train-aggregation", case_jit_train_aggregation),
]


# ---- tests ------------------------------------------------------------------

@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestScatterReduce(unittest.TestCase):
    # test_ops.py:3066, sum and mean, with the backward.
    def test_scatter_reduce(self):
        for reduce in ("sum", "mean"):
            for dim in (-1, 1, -3):
                for include_self in (True, False):
                    with self.subTest(reduce=reduce, dim=dim, include_self=include_self):
                        case_reduce(reduce, dim, include_self)

    def test_padded_indices(self):
        for reduce in ("sum", "mean"):
            for include_self in (True, False):
                with self.subTest(reduce=reduce, include_self=include_self):
                    case_padded_indices(reduce, include_self)

    def test_aggregation(self):
        case_aggregation()

    def test_segment_lse(self):
        case_segment_lse()

    def test_src_larger_than_index(self):
        # _pre_scatter shrinks src to index's shape (mixin/op.py:1088).
        x0 = np.zeros((3, 3), np.float32)
        idx = np.array([[0, 1, 0], [2, 0, 1]], np.int32)
        s0 = np.arange(12, dtype=np.float32).reshape(3, 4)
        src = Tensor(s0)
        out = Tensor(x0).scatter_reduce(0, Tensor(idx), src, "sum")
        out.sum().backward()
        np.testing.assert_array_equal(out.numpy(), [[0, 5, 2], [0, 1, 6], [4, 0, 0]])
        np.testing.assert_array_equal(src.grad.numpy(),
                                      [[1, 1, 1, 0], [1, 1, 1, 0], [0, 0, 0, 0]])

    def test_dtypes(self):
        # tinygrad sums in sum_acc_dtype (dtype.py:220-224, mixin/reduce.py:44),
        # which is the dtype itself for float32 and int32, and divides an int
        # mean in float32 (mixin/elementwise.py:253).
        base = np.array([[1, 2, 3], [4, 5, 6], [7, 8, 9]], np.int32)
        idx = np.array([[0, 1, 0], [2, 0, 1], [0, 0, 0]], np.int32)
        src = np.array([[10, 20, 30], [40, 50, 60], [5, 7, 11]], np.int32)
        for include_self in (True, False):
            out = Tensor(base).scatter_reduce(0, Tensor(idx), Tensor(src), "sum",
                                              include_self=include_self)
            self.assertEqual(out.dtype, dtypes.int32)
            self.assertEqual(out.numpy().dtype, np.int32)
            np.testing.assert_array_equal(
                out.numpy(), np_scatter_reduce(base, 0, idx, src, "sum", include_self))
        # tinygrad master's int mean on these values, as float32.  Its CPU
        # renderer divides (codegen/decomp/op.py:122-125); thvm multiplies by
        # the reciprocal, as every thvm division does, which can land one ulp
        # away (59 / 3 here).  The int mean is the float mean bit for bit.
        want = {True: [[5.333333492279053, 19.66666603088379, 14.666666984558105],
                       [4.0, 12.5, 33.0], [23.5, 8.0, 9.0]],
                False: [[7.5, 28.5, 20.5], [4.0, 20.0, 60.0], [40.0, 8.0, 9.0]]}
        for include_self in (True, False):
            out = Tensor(base).scatter_reduce(0, Tensor(idx), Tensor(src), "mean",
                                              include_self=include_self)
            self.assertEqual(out.dtype, dtypes.float32)
            np.testing.assert_array_max_ulp(out.numpy(),
                                            np.array(want[include_self], np.float32), maxulp=1)
            outf = Tensor(base.astype(np.float32)).scatter_reduce(
                0, Tensor(idx), Tensor(src.astype(np.float32)), "mean",
                include_self=include_self)
            np.testing.assert_array_equal(outf.numpy(), out.numpy())

    def test_int32_sum_is_exact(self):
        # Past 2^24 a float32 accumulator rounds; an int32 one does not.
        base = np.array([16777217, 3, -16777219, 5], np.int32)
        idx = np.array([0, 0, 2, 2, 3, 0], np.int32)
        src = np.array([1, 2, 16777216, 1, 7, 16777216], np.int32)
        got = Tensor(base).scatter_reduce(0, Tensor(idx), Tensor(src), "sum").numpy()
        np.testing.assert_array_equal(
            got, np_scatter_reduce(base, 0, idx, src, "sum").astype(np.int32))
        self.assertEqual(got.dtype, np.int32)

    def test_empty(self):
        # Nothing lands: sum and mean keep self exactly (a -0.0 stays -0.0;
        # mean in float32), and scatter(reduce=None) raises tinygrad's
        # IndexError: its one-hot merge cannot broadcast an empty axis.
        x0 = np.array([[-0.0, 1, -0.0], [2, -0.0, 3]], np.float32)
        idx = Tensor(np.zeros((0, 3), np.int32))
        src = Tensor(np.zeros((0, 3), np.float32))
        for reduce in ("sum", "mean"):
            for inc in (True, False):
                got = Tensor(x0).scatter_reduce(0, idx, src, reduce, include_self=inc).numpy()
                np.testing.assert_array_equal(got, x0)
                np.testing.assert_array_equal(np.signbit(got), np.signbit(x0))
        x = Tensor(x0)
        out = x.scatter_reduce(0, idx, src, "sum")
        (out * 2.0).sum().backward()
        np.testing.assert_array_equal(x.grad.numpy(), np.full((2, 3), 2.0))
        xi = np.arange(6, dtype=np.int32).reshape(2, 3)
        mean = Tensor(xi).scatter_reduce(0, idx, Tensor(np.zeros((0, 3), np.int32)), "mean").numpy()
        self.assertEqual(mean.dtype, np.float32)
        np.testing.assert_array_equal(mean, xi.astype(np.float32))
        for s in (src, 5.0):
            with self.assertRaisesRegex(IndexError, r"\(\(2, 3, 0\), \(2, 3, 0\), \(2, 3\)\)"):
                Tensor(x0).scatter(0, idx, s)
        empty = Tensor(np.zeros((0, 3), np.float32)).scatter_reduce(0, idx, src, "sum")
        self.assertEqual(empty.numpy().shape, (0, 3))

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_matches_tinygrad(self):
        # Forward against tinygrad master, and the gradient against
        # tinygrad's autograd through its one-hot formulation (its test_ops
        # cases are forward_only), ids out of range included.
        from tinygrad import Tensor as TgTensor
        from tinygrad import dtypes as tg_dtypes
        rng = np.random.default_rng(7)
        b = rng.integers(-1, 4, (3, 4, 5)).astype(np.int32)
        x0 = rng.uniform(-2, 2, (3, 4, 5)).astype(np.float32)
        s0 = rng.uniform(-2, 2, (3, 4, 5)).astype(np.float32)
        w0 = rng.uniform(-1, 1, (3, 4, 5)).astype(np.float32)
        for reduce in ("sum", "mean"):
            for dim in (-1, 1, -3):
                for include_self in (True, False):
                    with self.subTest(reduce=reduce, dim=dim, include_self=include_self):
                        x, src = Tensor(x0), Tensor(s0)
                        out = x.scatter_reduce(dim, Tensor(b), src, reduce,
                                               include_self=include_self)
                        (out * out * Tensor(w0)).sum().backward()
                        tx, tsrc = TgTensor(x0), TgTensor(s0)
                        tout = tx.scatter_reduce(dim, TgTensor(b, dtype=tg_dtypes.int32), tsrc,
                                                 reduce, include_self=include_self)
                        (tout * tout * TgTensor(w0)).sum().backward()
                        np.testing.assert_allclose(out.numpy(), tout.numpy(),
                                                   rtol=1e-6, atol=1e-6)
                        np.testing.assert_allclose(x.grad.numpy(), tx.grad.numpy(),
                                                   rtol=1e-5, atol=1e-6)
                        np.testing.assert_allclose(src.grad.numpy(), tsrc.grad.numpy(),
                                                   rtol=1e-5, atol=1e-6)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_segment_lse_matches_tinygrad(self):
        # thvm's mean-shifted segment log-sum-exp against the trainer's own
        # amax-shifted one on tinygrad master: same loss, same gradient.
        from tinygrad import Tensor as TgTensor
        from tinygrad import dtypes as tg_dtypes
        seg, x0, sw0 = segment_inputs(14)
        S, B = sw0.shape
        X, segt = Tensor(x0), Tensor(seg)
        m = Tensor.zeros(S, B).scatter_reduce(1, segt, X.detach(), "mean", include_self=False)
        den = Tensor.zeros(S, B).scatter_reduce(1, segt, (X - m.gather(1, segt)).exp(), "sum")
        loss = (Tensor(sw0) * ((den + 1e-30).log() + m)).sum()
        loss.backward()
        tX, tseg = TgTensor(x0), TgTensor(seg, dtype=tg_dtypes.int32)
        tm = TgTensor.full((S, B), -1e30).scatter_reduce(1, tseg, tX.detach(), "amax")
        tden = TgTensor.zeros(S, B).scatter_reduce(1, tseg, (tX - tm.gather(1, tseg)).exp(), "sum")
        tloss = (TgTensor(sw0) * ((tden + 1e-30).log() + tm)).sum()
        tloss.backward()
        np.testing.assert_allclose(float(loss.numpy()), float(tloss.numpy()), rtol=1e-5)
        np.testing.assert_allclose(X.grad.numpy(), tX.grad.numpy(), rtol=1e-5, atol=1e-6)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_aggregation_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        rng = np.random.default_rng(8)
        P, M, R, D = 6, 16, 9, 4
        own = np.repeat(padded_ids(rng, M, P)[:, None], D, 1).astype(np.int32)
        mem = np.repeat(padded_ids(rng, M, R)[:, None], D, 1).astype(np.int32)
        T0 = rng.standard_normal((R, D)).astype(np.float32)
        w0 = rng.uniform(0.2, 1.0, (M, 1)).astype(np.float32)
        g0 = rng.standard_normal((P, D)).astype(np.float32)
        for reduce in ("sum", "mean"):
            with self.subTest(reduce=reduce):
                T, w = Tensor(T0), Tensor(w0)
                agg = Tensor.zeros(P, D).scatter_reduce(
                    0, Tensor(own), T.gather(0, Tensor(mem)) * w, reduce=reduce)
                (agg * Tensor(g0)).sum().backward()
                tT, tw = TgTensor(T0), TgTensor(w0)
                tagg = TgTensor.zeros(P, D).scatter_reduce(
                    0, TgTensor(own), tT.gather(0, TgTensor(mem)) * tw, reduce=reduce)
                (tagg * TgTensor(g0)).sum().backward()
                np.testing.assert_allclose(agg.numpy(), tagg.numpy(), rtol=1e-6, atol=1e-6)
                np.testing.assert_allclose(T.grad.numpy(), tT.grad.numpy(),
                                           rtol=1e-5, atol=1e-6)
                np.testing.assert_allclose(w.grad.numpy(), tw.grad.numpy(),
                                           rtol=1e-5, atol=1e-6)


@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestScatter(unittest.TestCase):
    # test_ops.py:3008
    def test_scatter(self):
        rng = np.random.default_rng(9)
        b = rng.integers(0, 3, (3, 4, 5)).astype(np.int32)
        for xs, ss, dims in [((4, 5, 6), (4, 5, 6), (0, 1, 2, -1, -2, -3)),
                             ((3, 4, 5), (3, 4, 5), (1,)),
                             ((10, 3, 10), (10, 10, 10), (1,))]:
            x0 = rng.standard_normal(xs).astype(np.float32)
            s0 = rng.standard_normal(ss).astype(np.float32)
            for dim in dims:
                with self.subTest(shape=xs, dim=dim):
                    got = Tensor(x0).scatter(dim, Tensor(b), Tensor(s0)).numpy()
                    np.testing.assert_array_equal(got, np_scatter_replace(x0, dim, b, s0))
        x0 = rng.standard_normal((4, 5, 6)).astype(np.float32)
        for v in (3, float("inf")):
            got = Tensor(x0).scatter(1, Tensor(b), v).numpy()
            np.testing.assert_array_equal(
                got, np_scatter_replace(x0, 1, b, np.full(b.shape, v, np.float32)))
        # overlapping indices: the last write wins
        got = Tensor(np.array([1., 2., 3., 4.], np.float32)).scatter(
            0, Tensor(np.array([0, 0], np.int32)), Tensor(np.array([1., 0.], np.float32)))
        np.testing.assert_array_equal(got.numpy(), [0., 2., 3., 4.])

    def test_scatter_replace_cases(self):
        case_scatter_replace()

    def test_scatter_scalar_casts_to_self_dtype(self):
        # tinygrad fills full(index.shape, src, dtype=self.dtype) (:1163).
        idx = Tensor(np.array([[0, 1, 0], [2, 0, 1]], np.int32))
        z = Tensor(np.zeros((3, 3), np.int32))
        np.testing.assert_array_equal(z.scatter(0, idx, 2.5).numpy(),
                                      [[2, 2, 2], [0, 2, 2], [2, 0, 0]])
        np.testing.assert_array_equal(z.scatter(0, idx, True).numpy(),
                                      [[1, 1, 1], [0, 1, 1], [1, 0, 0]])
        zf = Tensor(np.zeros((3, 3), np.float32))
        np.testing.assert_array_equal(zf.scatter(0, idx, 1.5, reduce="add").numpy(),
                                      [[1.5, 1.5, 1.5], [0, 1.5, 1.5], [1.5, 0, 0]])

    # test_ops.py:3041
    def test_scatter_add(self):
        rng = np.random.default_rng(10)
        b = rng.integers(0, 3, (3, 4, 5)).astype(np.int32)
        x0 = rng.standard_normal((4, 5, 6)).astype(np.float32)
        for v in (float("inf"), float("nan")):
            got = Tensor(x0).scatter(1, Tensor(b), v, reduce="add").numpy()
            np.testing.assert_array_equal(
                got, np_scatter_reduce(x0, 1, b, np.full(b.shape, v, np.float32), "sum"))
        case_scatter_add_inf_nan()

    def test_scatter_backward_raises(self):
        # The REPLACE scatter has no gradient rule yet (plan step C3).
        x = Tensor(np.zeros((4, 2), np.float32))
        src = Tensor(np.ones((3, 2), np.float32))
        idx = Tensor(np.array([[0, 1], [2, 3], [1, 1]], np.int32))
        with self.assertRaisesRegex(RuntimeError, "replace"):
            x.scatter(0, idx, src).sum().backward()

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_scatter_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        from tinygrad import dtypes as tg_dtypes
        rng = np.random.default_rng(11)
        b = rng.integers(-1, 4, (3, 4, 5)).astype(np.int32)
        x0 = rng.standard_normal((4, 5, 6)).astype(np.float32)
        s0 = rng.standard_normal((4, 5, 6)).astype(np.float32)
        tb = TgTensor(b, dtype=tg_dtypes.int32)
        for dim in (0, 1, 2, -1):
            np.testing.assert_array_equal(Tensor(x0).scatter(dim, Tensor(b), Tensor(s0)).numpy(),
                                          TgTensor(x0).scatter(dim, tb, TgTensor(s0)).numpy())
        for v, reduce in ((3.0, None), (float("inf"), "add"), (float("nan"), "add")):
            np.testing.assert_array_equal(
                Tensor(x0).scatter(1, Tensor(b), v, reduce=reduce).numpy(),
                TgTensor(x0).scatter(1, tb, v, reduce=reduce).numpy())


class TestScatterErrors(unittest.TestCase):
    # The checks of _pre_scatter, scatter_reduce and scatter, with tinygrad's
    # exception types (test_ops.py:3022-3029 and mixin/op.py:1077-1168).
    def setUp(self):
        self.x = Tensor(np.zeros((3, 3), np.float32))
        self.idx = Tensor(np.array([[0, 1, 0], [2, 0, 1]], np.int32))
        self.src = Tensor(np.ones((2, 3), np.float32))

    def test_shape_checks(self):
        b = Tensor(np.zeros((3, 4, 5), np.int32))
        with self.assertRaises((RuntimeError, AssertionError)):
            Tensor(np.zeros((2, 3, 10), np.float32)).scatter(
                1, b, Tensor(np.zeros((10, 10, 10), np.float32)))
        with self.assertRaises((RuntimeError, AssertionError)):
            Tensor(np.zeros((10, 3, 10), np.float32)).scatter(
                1, b, Tensor(np.zeros((10, 3, 10), np.float32)))
        with self.assertRaises(AssertionError):
            self.x.scatter_reduce(0, self.idx.reshape(6), self.src, "sum")
        with self.assertRaises(AssertionError):
            self.x.scatter_reduce(0, self.idx, Tensor(np.ones((1, 3), np.float32)), "sum")
        with self.assertRaises(AssertionError):
            Tensor(np.zeros((3, 2), np.float32)).scatter_reduce(0, self.idx, self.src, "sum")
        with self.assertRaises(IndexError):
            self.x.scatter_reduce(2, self.idx, self.src, "sum")

    def test_type_checks(self):
        with self.assertRaises(RuntimeError):
            self.x.scatter_reduce(0, self.idx, Tensor(np.ones((2, 3), np.int32)), "sum")
        with self.assertRaises(RuntimeError):
            self.x.scatter(0, self.idx, Tensor(np.ones((2, 3), np.int32)))
        with self.assertRaises(RuntimeError):
            self.x.scatter_reduce(0, self.idx.cast(dtypes.float32), self.src, "sum")
        with self.assertRaises(TypeError):
            self.x.scatter(0, self.idx, self.src, mode="typo")

    def test_reduce_checks(self):
        with self.assertRaisesRegex(RuntimeError, "must be one of"):
            self.x.scatter_reduce(0, self.idx, self.src, "bad")
        with self.assertRaisesRegex(TypeError, "must be one of"):
            self.x.scatter(0, self.idx, 1.0, reduce="typo")
        with self.assertRaisesRegex(TypeError, "non-scalar src"):
            self.x.scatter(0, self.idx, self.src, reduce="add")

    def test_prod_and_multiply(self):
        # test_ops.py test_scatter_reduce "prod", test_scatter_mul and
        # test_scatter_reduce_prod_zeros, forward only as theirs; ids out of range dropped.
        rng = np.random.default_rng(11)
        b = rng.integers(-1, 4, (3, 4, 5)).astype(np.int32)
        x0 = rng.uniform(-2, 2, (3, 4, 5)).astype(np.float32)
        s0 = rng.uniform(-2, 2, (3, 4, 5)).astype(np.float32)
        for dim in (-1, 1, -3):
            for include_self in (True, False):
                with self.subTest(dim=dim, include_self=include_self):
                    got = Tensor(x0).scatter_reduce(dim, Tensor(b), Tensor(s0), "prod",
                                                    include_self=include_self).numpy()
                    want = np_scatter_reduce(x0, dim, b, s0, "prod", include_self)
                    np.testing.assert_allclose(got, want, rtol=2e-6, atol=1e-7)
        zeros = np.zeros((3, 4, 5), np.float32)
        got = Tensor(zeros).scatter_reduce(1, Tensor(b), Tensor(s0), "prod").numpy()
        np.testing.assert_array_equal(got, np_scatter_reduce(zeros, 1, b, s0, "prod"))
        for v in (float("inf"), float("nan"), 2.0):
            with self.subTest(multiply=v):
                got = Tensor(x0).scatter(1, Tensor(b), v, reduce="multiply").numpy()
                want = np_scatter_reduce(x0, 1, b, np.full(b.shape, v, np.float32), "prod")
                np.testing.assert_array_equal(got, want)


@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestScatterJit(unittest.TestCase):
    def test_jit_fresh_index(self):
        for reduce in ("sum", "mean"):
            for include_self in (True, False):
                with self.subTest(reduce=reduce, include_self=include_self):
                    case_jit_fresh_index(reduce, include_self)

    def test_jit_train_aggregation(self):
        case_jit_train_aggregation()


class TestHostFallback(unittest.TestCase):
    # Off the CPU backend scatter runs on the host, as gather does, and
    # carries no gradient; its values match the native kernels.
    def test_host_values(self):
        rng = np.random.default_rng(12)
        x0 = rng.standard_normal((5, 3)).astype(np.float32)
        s0 = rng.standard_normal((6, 3)).astype(np.float32)
        idx = np.repeat(np.array([4, -1, 0, 4, 9, 2])[:, None], 3, 1).astype(np.int32)
        saved = thvm_tensor._native_indexing
        thvm_tensor._native_indexing = lambda: False
        try:
            for reduce in ("sum", "mean"):
                for include_self in (True, False):
                    got = Tensor(x0).scatter_reduce(0, Tensor(idx), Tensor(s0), reduce,
                                                    include_self=include_self).numpy()
                    np.testing.assert_allclose(
                        got, np_scatter_reduce(x0, 0, idx, s0, reduce, include_self),
                        rtol=1e-6, atol=1e-6)
            got = Tensor(x0).scatter(0, Tensor(idx), Tensor(s0)).numpy()
            np.testing.assert_array_equal(got, np_scatter_replace(x0, 0, idx, s0))
            ib = np.array([[1, 2, 3], [4, 5, 6], [7, 8, 9]], np.int32)
            ii = np.array([[0, 1, 0], [2, 0, 1], [0, 0, 0]], np.int32)
            out = Tensor(ib).scatter_reduce(0, Tensor(ii), Tensor(ib * 3), "mean")
            self.assertEqual(out.dtype, dtypes.float32)
            np.testing.assert_allclose(out.numpy(), np_scatter_reduce(ib, 0, ii, ib * 3, "mean"),
                                       rtol=1e-6)
        finally:
            thvm_tensor._native_indexing = saved


# ---- both CPU routes ----------------------------------------------------------

@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_scatter_reduce_parity"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
