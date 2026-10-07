"""Tensor.cat and Tensor.stack against tinygrad master 365f87a8a.

tinygrad's cat (mixin/op.py:731-746) resolves dim with an IndexError
(mixin/movement.py:81-84), asserts the shapes, zero-pads every piece to the
output extent along dim and sums the pads; stack (mixin/movement.py:256-272)
takes its tensors through argfix (helpers.py:24-28), resolves dim one past
ndim and asserts equal shapes.  The result is lazy and differentiable: each
input's gradient is its slice of the output's.

Ports, numpy standing in for torch (tinygrad's helper_test_op compares the
forward and the gradient of fxn(x).sum(); here the cotangent is random, so a
gradient landing in the wrong slice shows):
  - test/runtime/test_ops.py:2784 test_cat, :2799 test_multicat,
    :2804 test_stack, :2816 test_stack_max;
  - test/runtime/test_tensor.py:541 test_cat (zero-size pieces);
  - test/null/test_tensor_uop_mixin.py:361-388 (the cat and stack forms).
Then the ATP enigma trainers' patterns (tools/atp_enigma/history_net.py:
621-661 and :821-832): x1.cat(agg, onehot, dim=1), a table grown by
table.cat(act(Z), dim=0) inside a loop, parts[0].cat(*parts[1:], dim=0),
the [0, logit] log_softmax, and the train step under TinyJit with fresh
inputs.  The aggregate is the dense segment sum the trainers' scatter_reduce
computes.  The tinygrad comparisons run when tinygrad is importable.
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.environ.get("THVM_PY", os.path.dirname(os.path.dirname(__file__))))
from thvm import Context, Device, Tensor, TinyJit, nn  # noqa: E402
from thvm.dtypes import dtypes  # noqa: E402
from thvm.tensor import _TH  # noqa: E402
from thvm.thvm import K  # noqa: E402

NATIVE = Device.DEFAULT == "CPU"


def _have_tg():
    try:
        import tinygrad  # noqa: F401
        return True
    except Exception:
        return False


def kernels():
    return _TH.kernel_count()


def np_split(g, sizes, dim):
    return np.split(g, np.cumsum(sizes)[:-1], axis=dim)


def cat_case(arrays, dim, rng):
    """thvm's cat of `arrays` along `dim`: the output and each input's
    gradient under a random cotangent, against numpy."""
    ts = [Tensor(a) for a in arrays]
    out = ts[0].cat(*ts[1:], dim=dim)
    want = np.concatenate(arrays, axis=dim)
    g = rng.standard_normal(want.shape).astype(np.float32)
    # Differentiate before realizing: a realized tensor is a leaf.
    (out * Tensor(g)).sum().backward()
    return out, ts, want, np_split(g, [a.shape[dim] for a in arrays], dim), g


class TestCat(unittest.TestCase):
    def check(self, shapes, dim, seed=0):
        rng = np.random.default_rng(seed)
        arrays = [rng.standard_normal(s).astype(np.float32) for s in shapes]
        out, ts, want, gs, _ = cat_case(arrays, dim, rng)
        self.assertEqual(out.shape, want.shape)
        np.testing.assert_array_equal(out.numpy(), want)
        for t, g in zip(ts, gs):
            np.testing.assert_array_equal(t.grad.numpy(), g)

    # test_ops.py:2784
    def test_cat(self):
        for dim in range(-2, 3):
            self.check([(45, 65, 9)] * 3, dim)
        # zero in the non-cat axis, then in the cat axis
        self.check([(45, 0, 9)] * 3, 0)
        self.check([(45, 0, 9), (45, 1, 9), (45, 2, 9)], 1)
        self.check([(45, 0, 9)] * 3, 1)
        with self.assertRaises(IndexError):
            a = Tensor(3.14)
            a.cat(a)

    # test_ops.py:2799
    def test_multicat(self):
        for dim in range(-1, 2):
            self.check([(45, 65)] * 3, dim)

    def test_cat_two_to_five_unequal_pieces(self):
        base = (4, 5, 6)
        for n in range(2, 6):
            for dim in (0, 1, -1):
                d = dim % 3
                shapes = [tuple(k + 1 if i == d else s for i, s in enumerate(base))
                          for k in range(n)]
                self.check(shapes, dim, seed=n)

    def test_cat_errors(self):
        x = Tensor(np.zeros((2, 3, 4), np.float32))
        with self.assertRaises(AssertionError):
            x.cat(Tensor(np.zeros((2, 3), np.float32)), dim=0)
        with self.assertRaises(AssertionError):
            x.cat(Tensor(np.zeros((2, 4, 4), np.float32)), dim=0)
        with self.assertRaises(IndexError):
            x.cat(x, dim=3)
        with self.assertRaises(IndexError):
            x.cat(x, dim=-4)

    # test_tensor.py:541
    def test_cat_zero_size_pieces(self):
        rng = np.random.default_rng(1)
        a = rng.random((3, 2, 2)).astype(np.float32)
        b = np.zeros((3, 2, 0), np.float32)
        for x, y, dim, shape, want in [(a, b, 2, (3, 2, 2), a), (b, a, 2, (3, 2, 2), a),
                                       (b, b, 0, (6, 2, 0), None), (b, b, 1, (3, 4, 0), None),
                                       (b, b, 2, (3, 2, 0), None)]:
            t = Tensor(x).cat(Tensor(y), dim=dim)
            self.assertEqual(t.shape, shape)
            np.testing.assert_equal(t.numpy(), np.zeros(shape) if want is None else want)

    def test_cat_class_and_instance_forms(self):
        rng = np.random.default_rng(2)
        a, b, c = (rng.standard_normal((2, k)).astype(np.float32) for k in (1, 2, 3))
        want = np.concatenate([a, b, c], 1)
        np.testing.assert_array_equal(Tensor(a).cat(Tensor(b), Tensor(c), dim=1).numpy(), want)
        np.testing.assert_array_equal(Tensor.cat(Tensor(a), Tensor(b), Tensor(c), dim=1).numpy(), want)
        np.testing.assert_array_equal(Tensor.cat(*[Tensor(x) for x in (a, b, c)], dim=1).numpy(), want)

    def test_cat_single(self):
        a = np.arange(6, dtype=np.float32).reshape(2, 3)
        for dim in (0, 1, -1):
            t = Tensor(a)
            out = t.cat(dim=dim)
            self.assertEqual(out.shape, (2, 3))
            np.testing.assert_array_equal(out.numpy(), a)

    def test_cat_int32_exact(self):
        a = np.array([[2**30 + 1, -7], [2**31 - 1, -2**31]], np.int32)
        b = np.array([[5], [-3]], np.int32)
        out = Tensor(a).cat(Tensor(b), dim=1)
        self.assertEqual(out.dtype, dtypes.int32)
        np.testing.assert_array_equal(out.numpy(), np.concatenate([a, b], 1))

    def test_cat_promotes_mixed_dtypes(self):
        # usum's add (pad path) and STACK's promo_dtype both take the least
        # upper dtype: int32 with float32 is float32.
        a = np.array([1, 2], np.int32)
        b = np.array([0.5], np.float32)
        out = Tensor(a).cat(Tensor(b))
        self.assertEqual(out.dtype, dtypes.float32)
        np.testing.assert_array_equal(out.numpy(), np.array([1.0, 2.0, 0.5], np.float32))

    def test_cat_keeps_nonfinite_values_in_their_slice(self):
        a = np.array([[np.inf, -np.inf], [np.nan, 1.0]], np.float32)
        b = np.array([[2.0], [3.0]], np.float32)
        for xs in ([a, b], [b, a, b]):
            np.testing.assert_array_equal(Tensor(xs[0]).cat(*[Tensor(x) for x in xs[1:]], dim=1).numpy(),
                                          np.concatenate(xs, 1))

    def test_cat_is_lazy_and_fuses(self):
        rng = np.random.default_rng(3)
        arrays = [rng.standard_normal((4, k + 1)).astype(np.float32) for k in range(4)]
        ts = [Tensor(a).realize() for a in arrays]
        k0 = kernels()
        out = ts[0].cat(*ts[1:], dim=1)
        self.assertEqual(kernels(), k0)
        self.assertNotEqual(int(_TH.term_tag(out.term)), K.TAG_TEN)
        y = (out * 2 + 1).realize()
        self.assertEqual(kernels() - k0, 1)
        np.testing.assert_array_equal(y.numpy(), np.concatenate(arrays, 1) * 2 + 1)

    def test_cat_of_computed_pieces(self):
        rng = np.random.default_rng(4)
        a = rng.standard_normal((3, 4)).astype(np.float32)
        b = rng.standard_normal((3, 2)).astype(np.float32)
        w = rng.standard_normal((5, 3)).astype(np.float32)
        ta, tb, tw = Tensor(a), Tensor(b), Tensor(w)
        out = (ta * 2).cat(tb.relu(), ta @ tw[:4], dim=1)
        g = rng.standard_normal((3, 9)).astype(np.float32)
        (out * Tensor(g)).sum().backward()
        np.testing.assert_allclose(out.numpy(), np.concatenate([a * 2, np.maximum(b, 0), a @ w[:4]], 1),
                                   rtol=1e-6, atol=1e-6)
        np.testing.assert_allclose(ta.grad.numpy(), g[:, :4] * 2 + g[:, 6:] @ w[:4].T, rtol=1e-5, atol=1e-6)
        np.testing.assert_array_equal(tb.grad.numpy(), g[:, 4:6] * (b > 0))
        want_w = np.zeros_like(w)
        want_w[:4] = a.T @ g[:, 6:]
        np.testing.assert_allclose(tw.grad.numpy(), want_w, rtol=1e-5, atol=1e-6)

    @unittest.skipUnless(_have_tg(), "tinygrad not on path")
    def test_cat_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        rng = np.random.default_rng(5)
        for shapes, dim in [([(3, 4), (3, 4)], 0), ([(3, 4), (3, 1), (3, 2)], 1),
                            ([(2, 3, 4), (2, 3, 1)], -1), ([(1, 5), (2, 5), (3, 5), (4, 5), (5, 5)], 0),
                            ([(4, 2, 3)] * 3, 1)]:
            arrays = [rng.standard_normal(s).astype(np.float32) for s in shapes]
            out, ts, _, _, g = cat_case(arrays, dim, rng)
            tts = [TgTensor(a) for a in arrays]
            tout = tts[0].cat(*tts[1:], dim=dim)
            (tout * TgTensor(g)).sum().backward()
            np.testing.assert_array_equal(out.numpy(), tout.numpy())
            for t, tt in zip(ts, tts):
                np.testing.assert_array_equal(t.grad.numpy(), tt.grad.numpy())


class TestStack(unittest.TestCase):
    # test_ops.py:2804
    def test_stack(self):
        rng = np.random.default_rng(6)
        arrays = [rng.standard_normal((5, 6, 3)).astype(np.float32) for _ in range(3)]
        for dim in range(-1, 3):
            for form in ("args", "tuple"):
                ts = [Tensor(a) for a in arrays]
                out = Tensor.stack(*ts, dim=dim) if form == "args" else Tensor.stack(tuple(ts), dim=dim)
                want = np.stack(arrays, axis=dim)
                g = rng.standard_normal(want.shape).astype(np.float32)
                (out * Tensor(g)).sum().backward()
                self.assertEqual(out.shape, want.shape)
                np.testing.assert_array_equal(out.numpy(), want)
                for i, t in enumerate(ts):
                    np.testing.assert_array_equal(t.grad.numpy(), np.take(g, i, axis=dim))

    def test_stack_errors(self):
        with self.assertRaises(IndexError):
            Tensor.stack(Tensor(np.zeros((45, 65, 3), np.float32)), dim=77)
        with self.assertRaises(ValueError):
            Tensor.stack((Tensor([1, 2]), Tensor([3, 4])), Tensor([5, 6]))
        with self.assertRaises(AssertionError):
            Tensor.stack(Tensor([1.0, 2.0]), Tensor([1.0, 2.0, 3.0]))

    def test_stack_0d(self):
        np.testing.assert_allclose(Tensor.stack(Tensor(3.14), Tensor(3.14)).numpy(), np.array([3.14, 3.14]))

    def test_stack_instance_form(self):
        a = np.arange(6, dtype=np.float32).reshape(2, 3)
        b = -a
        np.testing.assert_array_equal(Tensor(a).stack(Tensor(b), dim=1).numpy(), np.stack([a, b], 1))
        np.testing.assert_array_equal(Tensor(a).stack(dim=0).numpy(), a[None])

    # test_ops.py:2816
    def test_stack_max(self):
        x, y = Tensor([1.0]), Tensor([2.0])
        out = Tensor.stack(x, y).max(axis=0)
        out.sum().backward()
        np.testing.assert_array_equal(out.numpy(), [2.0])
        np.testing.assert_array_equal(x.grad.numpy(), [0.0])
        np.testing.assert_array_equal(y.grad.numpy(), [1.0])

    @unittest.skipUnless(_have_tg(), "tinygrad not on path")
    def test_stack_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        rng = np.random.default_rng(7)
        arrays = [rng.standard_normal((2, 3)).astype(np.float32) for _ in range(4)]
        for dim in (0, 1, 2, -1):
            ts, tts = [Tensor(a) for a in arrays], [TgTensor(a) for a in arrays]
            out, tout = ts[0].stack(*ts[1:], dim=dim), tts[0].stack(*tts[1:], dim=dim)
            g = rng.standard_normal(out.shape).astype(np.float32)
            (out * Tensor(g)).sum().backward()
            (tout * TgTensor(g)).sum().backward()
            np.testing.assert_array_equal(out.numpy(), tout.numpy())
            for t, tt in zip(ts, tts):
                np.testing.assert_array_equal(t.grad.numpy(), tt.grad.numpy())


# --------------------------------------------------------------------------
# the ATP enigma trainers' patterns (tools/atp_enigma/history_net.py)

D, NPHI, NRULE = 4, 3, 3


def pow2(n):
    return 1 << max(0, int(n) - 1).bit_length()


def np_act(x):
    return np.clip(x, 0.0, 6.0)


def np_rows(t, idx):
    """t's rows at idx (a (P, D) repeat of row ids), 0 for an id of -1."""
    ids = idx[:, 0]
    return np.where((ids >= 0)[:, None], t[np.maximum(ids, 0)], 0.0)


def act(x):
    return x.relu() - (x - 6.0).relu()


def make_weights(rng):
    def lin(i, o):
        return (rng.uniform(-0.5, 0.5, (o, i)).astype(np.float32),
                rng.uniform(-0.1, 0.1, o).astype(np.float32))
    return {"cp": lin(2 * D, D), "orl": lin(2 * D + NRULE, D), "sq": lin(2 * D, D),
            "h1": lin(D + NPHI, 5), "out": lin(5, 1),
            "miss": rng.uniform(-0.1, 0.1, (1, D)).astype(np.float32)}


def make_layout(rng, levels, P=4, M=8):
    """Level-major blocks of HistoryNet's DagLayout (history_net.py:355-445)
    at toy size: parent and member ids index the rows built so far (-1
    reads 0), the aggregate is own @ (rows at mem * w)."""
    blocks, rows = [], 1
    for _ in range(levels):
        own = np.zeros((P, M), np.float32)
        own[rng.integers(0, P, M), np.arange(M)] = 1.0
        oh = np.zeros((P, NRULE), np.float32)
        oh[np.arange(P), rng.integers(0, NRULE, P)] = 1.0
        kind = rng.integers(0, 4, P)
        blocks.append({
            "Ptab": pow2(rows),
            "i1": np.repeat(rng.integers(-1, rows, P).astype(np.int32)[:, None], D, 1),
            "i2": np.repeat(rng.integers(-1, rows, P).astype(np.int32)[:, None], D, 1),
            "mem": np.repeat(rng.integers(-1, rows, M).astype(np.int32)[:, None], D, 1),
            "own": own, "w": rng.uniform(0.2, 1.0, (M, 1)).astype(np.float32), "onehot": oh,
            "m_cp": (kind == 0).astype(np.float32)[:, None],
            "m_or": (kind == 1).astype(np.float32)[:, None],
            "m_sq": (kind == 2).astype(np.float32)[:, None]})
        rows += P
    return blocks, rows


def make_batch(rng, rows, Q=4, R=3):
    n = 2 * Q + R
    return {"cp": [tuple(np.repeat(rng.integers(-1, rows, Q).astype(np.int32)[:, None], D, 1)
                         for _ in range(2)) for _ in range(2)],
            "other": np.repeat(rng.integers(-1, rows, R).astype(np.int32)[:, None], D, 1),
            "phi": rng.standard_normal((n, NPHI)).astype(np.float32),
            "y": (rng.random(n) < 0.5).astype(np.float32),
            "w": rng.uniform(0.0, 1.0, n).astype(np.float32)}


class MiniHistory:
    """HistoryNet (history_net.py:621-661) at toy size in thvm or tinygrad."""

    def __init__(self, T, nn_, weights):
        self.T = T

        def lin(name):
            w, b = weights[name]
            layer = nn_.Linear(w.shape[1], w.shape[0])
            layer.weight, layer.bias = T(w.copy()), T(b.copy())
            return layer
        self.cp, self.orl, self.sq = lin("cp"), lin("orl"), lin("sq")
        self.h1, self.out = lin("h1"), lin("out")
        self.miss = T(weights["miss"].copy())

    def params(self):
        return [self.miss] + [t for layer in (self.cp, self.orl, self.sq, self.h1, self.out)
                              for t in (layer.weight, layer.bias)]

    def table(self, blocks):
        table = self.miss
        for b in blocks:
            Tp = table.pad(((0, b["Ptab"] - table.shape[0]), None))
            x1, x2 = Tp.gather(0, b["i1"]), Tp.gather(0, b["i2"])
            agg = b["own"] @ (Tp.gather(0, b["mem"]) * b["w"])
            Z = (self.cp(x1.cat(x2, dim=1)) * b["m_cp"]
                 + self.orl(x1.cat(agg, b["onehot"], dim=1)) * b["m_or"]
                 + self.sq(x1.cat(agg, dim=1)) * b["m_sq"])
            table = table.cat(act(Z), dim=0)
        return table.pad(((0, pow2(table.shape[0]) - table.shape[0]), None))

    def score(self, Tt, cp_blocks, other, phi):
        parts = [act(self.cp(Tt.gather(0, i1).cat(Tt.gather(0, i2), dim=1))) for i1, i2 in cp_blocks]
        parts.append(Tt.gather(0, other))
        E = parts[0].cat(*parts[1:], dim=0)
        return self.out(self.h1(E.cat(phi, dim=1)).relu()).reshape(-1)

    def loss(self, blocks, cp_blocks, other, phi, y, w):
        logit = self.score(self.table(blocks), cp_blocks, other, phi)
        n = logit.shape[0]
        ls = self.T.zeros(n, 1).cat(logit.reshape(n, 1), dim=1).log_softmax(1)
        return (-(ls[:, 1] * y + ls[:, 0] * (1.0 - y)) * w).sum(), logit


def to_tensors(T, obj):
    if isinstance(obj, np.ndarray):
        return T(obj)
    if isinstance(obj, dict):
        return {k: to_tensors(T, v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return type(obj)(to_tensors(T, v) for v in obj)
    return obj


def np_model(weights, blocks, batch):
    """numpy forward of MiniHistory: (loss, logits, table)."""
    def lin(name, x):
        w, b = weights[name]
        return x.astype(np.float64) @ w.T.astype(np.float64) + b
    table = weights["miss"].astype(np.float64)
    for b in blocks:
        Tp = np.concatenate([table, np.zeros((b["Ptab"] - len(table), D))], 0)
        x1, x2 = np_rows(Tp, b["i1"]), np_rows(Tp, b["i2"])
        agg = b["own"] @ (np_rows(Tp, b["mem"]) * b["w"])
        Z = (lin("cp", np.concatenate([x1, x2], 1)) * b["m_cp"]
             + lin("orl", np.concatenate([x1, agg, b["onehot"]], 1)) * b["m_or"]
             + lin("sq", np.concatenate([x1, agg], 1)) * b["m_sq"])
        table = np.concatenate([table, np_act(Z)], 0)
    table = np.concatenate([table, np.zeros((pow2(len(table)) - len(table), D))], 0)
    parts = [np_act(lin("cp", np.concatenate([np_rows(table, i1), np_rows(table, i2)], 1)))
             for i1, i2 in batch["cp"]]
    parts.append(np_rows(table, batch["other"]))
    E = np.concatenate(parts, 0)
    logit = lin("out", np.maximum(lin("h1", np.concatenate([E, batch["phi"]], 1)), 0)).reshape(-1)
    z = np.stack([np.zeros_like(logit), logit], 1)
    ls = z - z.max(1, keepdims=True)
    ls = ls - np.log(np.exp(ls).sum(1, keepdims=True))
    y, w = batch["y"], batch["w"]
    return (-(ls[:, 1] * y + ls[:, 0] * (1 - y)) * w).sum(), logit, table


def run_model(T, nn_, weights, blocks_np, batch_np):
    model = MiniHistory(T, nn_, weights)
    blocks, batch = to_tensors(T, blocks_np), to_tensors(T, batch_np)
    loss, logit = model.loss(blocks, batch["cp"], batch["other"], batch["phi"], batch["y"], batch["w"])
    loss.backward()
    return loss.numpy(), logit.numpy(), [p.grad.numpy() for p in model.params()]


@unittest.skipUnless(NATIVE, "native gather runs on the CPU backend")
class TestCatATP(unittest.TestCase):
    def test_three_way_cat_with_aggregate_and_onehot(self):
        # x1.cat(agg, onehot, dim=1) (history_net.py:646): the aggregate and
        # the parent rows reach the table, the one-hot is data.
        rng = np.random.default_rng(8)
        P, M, R = 4, 8, 6
        tab = rng.standard_normal((R, D)).astype(np.float32)
        i1 = np.repeat(np.array([2, -1, 0, 5], np.int32)[:, None], D, 1)
        mem = np.repeat(rng.integers(-1, R, M).astype(np.int32)[:, None], D, 1)
        own = np.zeros((P, M), np.float32)
        own[rng.integers(0, P, M), np.arange(M)] = 1.0
        w = rng.uniform(0.2, 1.0, (M, 1)).astype(np.float32)
        oh = np.eye(NRULE, dtype=np.float32)[rng.integers(0, NRULE, P)]
        W, b = make_weights(rng)["orl"]
        g = rng.standard_normal((P, D)).astype(np.float32)
        T = Tensor(tab)
        lin = nn.Linear(2 * D + NRULE, D)
        lin.weight, lin.bias = Tensor(W), Tensor(b)
        x1 = T.gather(0, Tensor(i1))
        agg = Tensor(own) @ (T.gather(0, Tensor(mem)) * Tensor(w))
        Z = lin(x1.cat(agg, Tensor(oh), dim=1))
        (Z * Tensor(g)).sum().backward()
        C = np.concatenate([np_rows(tab, i1), own @ (np_rows(tab, mem) * w), oh], 1)
        np.testing.assert_allclose(Z.numpy(), C @ W.T + b, rtol=1e-5, atol=1e-6)
        dC = g @ W
        want_T = np.zeros_like(tab)
        np.add.at(want_T, np.maximum(i1[:, 0], 0), np.where((i1[:, 0] >= 0)[:, None], dC[:, :D], 0))
        dm = (own.T @ dC[:, D:2 * D]) * w
        np.add.at(want_T, np.maximum(mem[:, 0], 0), np.where((mem[:, 0] >= 0)[:, None], dm, 0))
        np.testing.assert_allclose(T.grad.numpy(), want_T, rtol=1e-5, atol=1e-5)
        np.testing.assert_allclose(lin.weight.grad.numpy(), g.T @ C, rtol=1e-5, atol=1e-5)
        np.testing.assert_allclose(lin.bias.grad.numpy(), g.sum(0), rtol=1e-5, atol=1e-5)

    def test_table_grown_by_cat_in_a_loop(self):
        rng = np.random.default_rng(9)
        weights = make_weights(rng)
        for levels in (1, 4, 24):
            blocks, rows = make_layout(rng, levels)
            batch = make_batch(rng, pow2(rows))
            loss, logit, _ = run_model(Tensor, nn, weights, blocks, batch)
            want_loss, want_logit, _ = np_model(weights, blocks, batch)
            np.testing.assert_allclose(logit, want_logit, rtol=1e-4, atol=1e-5)
            np.testing.assert_allclose(loss, want_loss, rtol=1e-4, atol=1e-5)

    def test_levels_of_different_widths_share_one_linear(self):
        # history_net.py's table with levels of P = 1, 4, 8, 2 rows through
        # one nn.Linear: each level's matmul broadcasts the weight over its
        # own rows, so the weight's view is realized for consumers of
        # different extents, and a level read past the weight's buffer.
        rng = np.random.default_rng(13)
        W0 = rng.standard_normal((D, 2 * D)).astype(np.float32)
        b0 = rng.standard_normal(D).astype(np.float32)
        lin = nn.Linear(2 * D, D)
        lin.weight, lin.bias = Tensor(W0), Tensor(b0)
        t0 = rng.standard_normal((1, D)).astype(np.float32)
        table, ref = Tensor(t0), t0.astype(np.float64)
        for P in (1, 4, 8, 2):
            i1 = np.repeat(rng.integers(0, ref.shape[0], (P, 1)), D, 1).astype(np.int32)
            i2 = np.repeat(rng.integers(0, ref.shape[0], (P, 1)), D, 1).astype(np.int32)
            Z = lin(table.gather(0, Tensor(i1)).cat(table.gather(0, Tensor(i2)), dim=1))
            Zr = np.concatenate([np_rows(ref, i1), np_rows(ref, i2)], 1) @ W0.T + b0
            table, ref = table.cat(Z, dim=0), np.concatenate([ref, Zr], 0)
        np.testing.assert_allclose(table.numpy(), ref, rtol=1e-5, atol=1e-5)

    def test_table_cat_forward_is_exact_rows(self):
        # table.cat(act(Z), dim=0) keeps every earlier row and appends act(Z).
        rng = np.random.default_rng(10)
        weights = make_weights(rng)
        blocks, rows = make_layout(rng, 6)
        model = MiniHistory(Tensor, nn, weights)
        got = model.table(to_tensors(Tensor, blocks)).numpy()
        _, _, want = np_model(weights, blocks, make_batch(rng, pow2(rows)))
        self.assertEqual(got.shape, want.shape)
        np.testing.assert_allclose(got, want, rtol=1e-5, atol=1e-6)

    @unittest.skipUnless(_have_tg(), "tinygrad not on path")
    def test_model_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        from tinygrad import nn as tgnn
        rng = np.random.default_rng(11)
        weights = make_weights(rng)
        for levels in (2, 8):
            blocks, rows = make_layout(rng, levels)
            batch = make_batch(rng, pow2(rows))
            loss, logit, grads = run_model(Tensor, nn, weights, blocks, batch)
            tloss, tlogit, tgrads = run_model(TgTensor, tgnn, weights, blocks, batch)
            np.testing.assert_allclose(logit, tlogit, rtol=1e-4, atol=1e-5)
            np.testing.assert_allclose(loss, tloss, rtol=1e-4, atol=1e-5)
            for g, tg in zip(grads, tgrads):
                np.testing.assert_allclose(g, tg, rtol=1e-3, atol=1e-5)

    def test_bce_cat_log_softmax(self):
        # history_net.py:826: zeros(n, 1).cat(logit.reshape(n, 1), dim=1).log_softmax(1)
        rng = np.random.default_rng(12)
        x = rng.standard_normal(7).astype(np.float32) * 3
        y = (rng.random(7) < 0.5).astype(np.float32)
        logit = Tensor(x)
        ls = Tensor.zeros(7, 1).cat(logit.reshape(7, 1), dim=1).log_softmax(1)
        loss = -(ls[:, 1] * Tensor(y) + ls[:, 0] * (1.0 - Tensor(y))).sum()
        loss.backward()
        p = 1.0 / (1.0 + np.exp(-x.astype(np.float64)))
        want = -(y * np.log(p) + (1 - y) * np.log(1 - p)).sum()
        np.testing.assert_allclose(loss.numpy(), want, rtol=1e-5)
        np.testing.assert_allclose(logit.grad.numpy(), p - y, rtol=1e-4, atol=1e-6)


@unittest.skipUnless(NATIVE, "native gather runs on the CPU backend")
class TestCatJit(unittest.TestCase):
    """The trainer's step under TinyJit: a fixed layout, fresh index /
    feature / label / weight tensors every call.  Losses compare under
    loss.realize(*opt.schedule_step()).  The trainer's own step
    (history_net.py:875-881) realizes the loss after opt.step(); there only
    the parameters compare, because of an open thvm bug that predates cat
    (it reproduces at 2185bba8 without it): a loss realized after opt.step()
    reads the post-step parameters in eager mode but the pre-step ones on
    TinyJit replays, while tinygrad master returns the same loss eager and
    jitted.  The parameters themselves match tinygrad in every form."""

    STEPS = 5

    @staticmethod
    def train(T, nn_, ctx, weights, blocks_np, batches, jit, trainer_step=False):
        model = MiniHistory(T, nn_, weights)
        blocks = to_tensors(T, blocks_np)
        opt = nn_.optim.Adam(model.params(), lr=1e-2)

        def step(a1, a2, b1, b2, other, phi, y, w):
            loss, _ = model.loss(blocks, [(a1, a2), (b1, b2)], other, phi, y, w)
            opt.zero_grad()
            loss.backward()
            if trainer_step:
                opt.step()
                return loss.realize()
            return loss.realize(*opt.schedule_step())
        f = TinyJit(step) if jit else step
        losses = []
        with ctx:
            for bt in batches:
                ins = [bt["cp"][0][0], bt["cp"][0][1], bt["cp"][1][0], bt["cp"][1][1],
                       bt["other"], bt["phi"], bt["y"], bt["w"]]
                losses.append(float(f(*[T(x).realize() for x in ins]).item()))
        return np.array(losses), [p.numpy() for p in model.params()]

    def setUp(self):
        rng = np.random.default_rng(13)
        self.weights = make_weights(rng)
        self.blocks, rows = make_layout(rng, 5)
        self.batches = [make_batch(rng, pow2(rows)) for _ in range(self.STEPS)]

    def run_thvm(self, jit, trainer_step=False):
        return self.train(Tensor, nn, Context(TRAINING=1), self.weights, self.blocks,
                          self.batches, jit, trainer_step)

    def test_jit_matches_eager(self):
        lj, pj = self.run_thvm(jit=True)
        le, pe = self.run_thvm(jit=False)
        np.testing.assert_allclose(lj, le, rtol=1e-5, atol=1e-6)
        for a, b in zip(pj, pe):
            np.testing.assert_allclose(a, b, rtol=1e-5, atol=1e-6)
        # the first loss is the forward at the initial weights
        want0, _, _ = np_model(self.weights, self.blocks, self.batches[0])
        np.testing.assert_allclose(le[0], want0, rtol=1e-5)
        # the parameters moved: every step's gradient reached them
        for a, w0 in zip(pe, MiniHistory(Tensor, nn, self.weights).params()):
            self.assertGreater(np.abs(a - w0.numpy()).max(), 1e-4)

    def test_trainer_step_jit_trains_like_eager(self):
        _, pj = self.run_thvm(jit=True, trainer_step=True)
        _, pe = self.run_thvm(jit=False, trainer_step=True)
        _, ps = self.run_thvm(jit=False)
        for a, b, c in zip(pj, pe, ps):
            np.testing.assert_allclose(a, b, rtol=1e-5, atol=1e-6)
            np.testing.assert_allclose(b, c, rtol=1e-5, atol=1e-6)

    @unittest.skipUnless(_have_tg(), "tinygrad not on path")
    def test_jit_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        from tinygrad import nn as tgnn
        from tinygrad import Context as TgContext
        lj, pj = self.run_thvm(jit=True)
        lt, pt = self.train(TgTensor, tgnn, TgContext(TRAINING=1), self.weights, self.blocks,
                            self.batches, jit=False)
        np.testing.assert_allclose(lj, lt, rtol=1e-4, atol=1e-5)
        for a, b in zip(pj, pt):
            np.testing.assert_allclose(a, b, rtol=1e-3, atol=1e-5)


if __name__ == "__main__":
    unittest.main()
