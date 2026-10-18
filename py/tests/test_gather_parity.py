"""Tensor.gather, Tensor.embedding, advanced getitem and nn.Embedding
against tinygrad's semantics (master 365f87a8a).

Ports, with numpy standing in for torch as the reference (tinygrad's
helper_test_op compares the forward and the gradient of fxn(x).sum()):
  - test/runtime/test_ops.py:2977 test_gather (dims -3..2, the -inf value,
    the error types) and :3001 test_gather_bool_index;
  - test/runtime/test_nn.py:360 test_embedding (empty input, TinyJit calls
    with fresh ids) and :416/:420 the one-kernel embedding;
  - test/runtime/test_arange.py:108 the out-of-bounds index and :182 the
    padded-embedding gradient.
On the CPU backend these run thvm's native UOP_GATHER / UOP_SCATTER, so the
gradients are real (scatter-adds).  The tinygrad-side checks also run
against tinygrad itself when it is importable.
"""
import unittest

import numpy as np

import helpers
from thvm import Context, Device, Tensor, TinyJit, nn
from thvm.dtypes import dtypes
from thvm.tensor import _TH
from thvm.thvm import K

NATIVE = Device.DEFAULT == "CPU"


def np_gather(x, dim, idx):
    """tinygrad Tensor.gather: x read at idx along dim, 0 out of range."""
    d = dim % x.ndim
    sl = tuple(slice(None) if i == d else slice(0, s) for i, s in enumerate(idx.shape))
    valid = (idx >= 0) & (idx < x.shape[d])
    out = np.take_along_axis(x[sl], np.where(valid, idx, 0), axis=d)
    return np.where(valid, out, 0).astype(x.dtype)


def np_scatter_add(shape, dim, idx, v):
    """The gather gradient: v summed into zeros(shape) where idx reads."""
    d = dim % len(shape)
    g = np.zeros(shape, np.float32)
    for p in np.ndindex(*idx.shape):
        i = int(idx[p])
        if 0 <= i < shape[d]:
            q = list(p)
            q[d] = i
            g[tuple(q)] += v[p]
    return g


def np_rows(w, ids):
    """Rows of w at ids, a zero row for an id out of range."""
    valid = (ids >= 0) & (ids < w.shape[0])
    return np.where(valid[..., None], w[np.where(valid, ids, 0)], 0).astype(w.dtype)


def kernels():
    return _TH.kernel_count()


@unittest.skipUnless(NATIVE, "native gather runs on the CPU backend")
class TestGather(unittest.TestCase):
    # test_ops.py:2977
    def test_gather(self):
        rng = np.random.default_rng(0)
        b = rng.integers(0, 3, size=(3, 4, 5)).astype(np.int32)
        for shape, dim in [((4, 5, 6), 0), ((4, 5, 6), 1), ((4, 5, 6), 2), ((3, 4, 5), 0),
                           ((4, 5, 6), -1), ((4, 5, 6), -2), ((4, 5, 6), -3)]:
            x = rng.uniform(-2, 2, size=shape).astype(np.float32)
            t = Tensor(x)
            out = t.gather(dim, Tensor(b))
            self.assertEqual(out.shape, b.shape)
            self.assertEqual(out.dtype, dtypes.float32)
            # Differentiate before realizing: a realized tensor is a leaf.
            out.sum().backward()
            np.testing.assert_array_equal(out.numpy(), np_gather(x, dim, b))
            np.testing.assert_array_equal(t.grad.numpy(),
                                          np_scatter_add(shape, dim, b, np.ones(b.shape)))

    def test_gather_errors(self):
        x = Tensor(np.zeros((4, 5, 6), np.float32))
        with self.assertRaises(RuntimeError):
            x.gather(0, Tensor(np.array([1], np.int32)))
        b = Tensor(np.zeros((3, 4, 5), np.int32))
        with self.assertRaises(AssertionError):
            Tensor(np.zeros((2, 1, 1), np.float32)).gather(0, b)
        with self.assertRaises(IndexError):
            x.gather(3, b)
        with self.assertRaises(RuntimeError):
            x.gather(0, Tensor(np.zeros((3, 4, 5), np.float32)))

    def test_gather_repeats_and_inf(self):
        idx = [2, 1, 0, 1, 2]
        for vals in ([1.0, 2.0, 3.0], [-float("inf"), 2.0, 3.0]):
            x = Tensor(np.array(vals, np.float32))
            out = x.gather(0, Tensor(idx))
            out.sum().backward()
            np.testing.assert_array_equal(out.numpy(), np.array(vals, np.float32)[idx])
            np.testing.assert_array_equal(x.grad.numpy(), [1.0, 2.0, 2.0])

    # test_ops.py:3001; thvm has no bool dtype, so (y != 0) is the bool cast.
    def test_gather_bool_index(self):
        x = Tensor(np.array([1.0, 2.0, 3.0], np.float32))
        y = Tensor(np.array([0.5, 0.0, 2.0], np.float32))
        index = (y != 0).cast(dtypes.int)
        np.testing.assert_array_equal(x.gather(0, index).numpy(), [2.0, 1.0, 2.0])
        np.testing.assert_array_equal(x[index].numpy(), [2.0, 1.0, 2.0])

    def test_gather_empty_index(self):
        x = Tensor(np.ones((4, 3), np.float32))
        out = x.gather(0, Tensor(np.zeros((0, 3), np.int32)))
        self.assertEqual(out.shape, (0, 3))
        self.assertEqual(out.numpy().shape, (0, 3))

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_gather_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        rng = np.random.default_rng(1)
        x = rng.uniform(-2, 2, size=(4, 5, 6)).astype(np.float32)
        b = rng.integers(-1, 5, size=(3, 4, 5)).astype(np.int32)
        for dim in (0, 1, 2):
            t = Tensor(x)
            out = t.gather(dim, Tensor(b))
            # master takes no requires_grad: backward fills .grad for every
            # float tensor in the graph (tensor.py:486-488).
            tg = TgTensor(x)
            tout = tg.gather(dim, TgTensor(b))
            out.sum().backward()
            tout.sum().backward()
            np.testing.assert_allclose(out.numpy(), tout.numpy(), atol=1e-6)
            np.testing.assert_allclose(t.grad.numpy(), tg.grad.numpy(), atol=1e-6)


@unittest.skipUnless(NATIVE, "native gather runs on the CPU backend")
class TestIndex(unittest.TestCase):
    # test_arange.py:108
    def test_index_out_of_bounds(self):
        data = np.random.default_rng(2).random((256, 256)).astype(np.float32)
        dataset = Tensor(data)
        bad = [-19238, -257, 256, 495, 10982377]
        np.testing.assert_equal(dataset[Tensor(bad)].numpy(), 0)
        np.testing.assert_equal(dataset[bad].numpy(), 0)
        np.testing.assert_equal(dataset[np.array(bad)].numpy(), 0)

    def test_index_negative_wraps(self):
        data = np.arange(12, dtype=np.float32).reshape(4, 3)
        t = Tensor(data)
        for idx in ([-1, 0, -4, 2], np.array([[3, -2], [0, -1]])):
            want = data[np.asarray(idx)]
            np.testing.assert_array_equal(t[idx].numpy(), want)
            np.testing.assert_array_equal(t[Tensor(np.asarray(idx, np.int32))].numpy(), want)

    def test_index_one_kernel(self):
        data = np.random.default_rng(3).random((64, 16)).astype(np.float32)
        dataset = Tensor(data).realize()
        idxs = Tensor([0, 3, 5, 6, -1]).realize()
        k0 = kernels()
        x = dataset[idxs].realize()
        self.assertEqual(kernels() - k0, 1)
        np.testing.assert_array_equal(x.numpy(), data[[0, 3, 5, 6, -1]])

    def test_index_int_labels(self):
        labels = np.random.default_rng(4).integers(0, 10, 100).astype(np.int32)
        samples = np.array([5, 0, 99, 42, 42, 7], np.int32)
        y = Tensor(labels)[Tensor(samples)]
        self.assertEqual(y.dtype, dtypes.int32)
        np.testing.assert_array_equal(y.numpy(), labels[samples])

    def test_index_rejects_non_int(self):
        t = Tensor(np.zeros((5, 6), np.float32))
        with self.assertRaises(IndexError):
            t[[True, False]]
        with self.assertRaises(IndexError):
            t[Tensor(np.array([0.0, 1.0], np.float32))]
        np.testing.assert_array_equal(t[[True, False, 2]].numpy(), np.zeros((3, 6)))

    def test_index_grad(self):
        rng = np.random.default_rng(5)
        data = rng.standard_normal((6, 4)).astype(np.float32)
        ids = np.array([2, -1, 0, 6, 2, -7, 5, 1], np.int32)
        g = rng.standard_normal((8, 4)).astype(np.float32)
        t = Tensor(data)
        (t[Tensor(ids)] * Tensor(g)).sum().backward()
        wrapped = np.where(ids < 0, ids + 6, ids)
        np.testing.assert_allclose(t.grad.numpy(), np_scatter_add((6, 4), 0, np.repeat(
            wrapped[:, None], 4, axis=1), g), rtol=1e-6)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_index_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        data = np.random.default_rng(6).random((16, 5)).astype(np.float32)
        ids = np.array([3, -1, 16, -17, 0, 15], np.int32)
        np.testing.assert_allclose(Tensor(data)[Tensor(ids)].numpy(),
                                   TgTensor(data)[TgTensor(ids)].numpy())


@unittest.skipUnless(NATIVE, "native gather runs on the CPU backend")
class TestEmbedding(unittest.TestCase):
    # test_nn.py:360
    def test_embedding(self):
        B, T, embed_size, vocab_size = 4, 10, 20, 28
        layer = nn.Embedding(vocab_size, embed_size)
        w = layer.weight.numpy()
        rng = np.random.default_rng(7)
        ids = rng.integers(0, vocab_size, (B, T)).astype(np.int32)
        np.testing.assert_array_equal(layer(Tensor(ids)).numpy(), w[ids])
        empty = layer(Tensor(np.zeros((B, 0), np.int32)))
        self.assertEqual(empty.shape, (B, 0, embed_size))
        self.assertEqual(empty.numpy().shape, (B, 0, embed_size))

        @TinyJit
        def layer_jit(x):
            return layer(x).realize()

        for _ in range(5):
            ids = rng.integers(0, vocab_size, (B, T)).astype(np.int32)
            np.testing.assert_array_equal(layer_jit(Tensor(ids)).numpy(), w[ids])

    # test_nn.py:416 / :420
    def test_embedding_one_kernel(self):
        layer = nn.Embedding(20, 30)
        layer.weight = Tensor.zeros_like(layer.weight).contiguous()
        for ids in ([[1, 5, 9, 11], [12, 19, 8, 1]], [[1, 2, 3], [4, 5, 6], [7, 8, 9]]):
            k0 = kernels()
            out = layer(Tensor(ids)).realize()
            self.assertEqual(kernels() - k0, 1)
            np.testing.assert_array_equal(out.numpy(), np.zeros((*np.shape(ids), 30)))

    def test_embedding_out_of_range_reads_zero(self):
        layer = nn.Embedding(6, 3)
        w = layer.weight.numpy()
        ids = np.array([[-1, 2, 6], [10982377, 0, -19238]], np.int32)
        np.testing.assert_array_equal(layer(Tensor(ids)).numpy(), np_rows(w, ids))

    # test_arange.py:182 (tinygrad runs it under USE_ATOMICS=1; the native
    # scatter-add gives the same gradient with or without the flag).
    def test_embedding_backward_padded_embed(self):
        vocab_size, embed_size = 1000, 300
        bs, seqlen = 4, 256
        ids = np.random.default_rng(8).integers(0, vocab_size, (bs, seqlen)).astype(np.int32)
        ids[:, -16:] = -1
        for atomics in (0, 1):
            with Context(USE_ATOMICS=atomics):
                emb = nn.Embedding(vocab_size, embed_size)
                emb.weight = Tensor.ones(vocab_size, embed_size)
                gt = Tensor.zeros(bs, seqlen, embed_size)
                loss = (emb(Tensor(ids)) - gt).square().sum()
                loss.backward()
                expected = np.zeros((vocab_size, embed_size), np.float32)
                for i in ids.flatten():
                    if i >= 0:
                        expected[i] += 2
                np.testing.assert_allclose(emb.weight.grad.numpy(), expected, rtol=1e-5, atol=1e-5)

    def test_embedding_type_error(self):
        with self.assertRaises(TypeError):
            nn.Embedding(10, 3)(Tensor(np.array([1.0, 2.0], np.float32)))

    def test_embedding_jit_train_step(self):
        # SGD on an embedding under TinyJit, fresh ids each call: the gather
        # and its scatter-add gradient replay with the new index buffer.
        V, D, N, lr = 10, 4, 6, 0.1
        rng = np.random.default_rng(9)
        w0 = rng.standard_normal((V, D)).astype(np.float32)
        g = rng.standard_normal((N, D)).astype(np.float32)
        emb = nn.Embedding(V, D)
        emb.weight = Tensor(w0.copy()).realize()
        gt = Tensor(g).realize().detach()
        with Context(TRAINING=1):
            opt = nn.optim.SGD([emb.weight], lr=lr)

            @TinyJit
            def step(ids):
                opt.zero_grad()
                loss = (emb(ids) * gt).sum()
                loss.backward()
                return loss.realize(*opt.schedule_step())

            want = w0.astype(np.float64)
            for _ in range(6):
                ids = rng.integers(-1, V + 1, N).astype(np.int32)
                step(Tensor(ids))
                grad = np.zeros((V, D))
                for r, i in enumerate(ids):
                    if 0 <= i < V:
                        grad[i] += g[r]
                want = want - lr * grad
                np.testing.assert_allclose(emb.weight.numpy(), want, rtol=1e-5, atol=1e-5)

    def test_embedding_jit_index_rewritten_in_place(self):
        # One index tensor whose buffer is rewritten between replays.
        V, D, N = 12, 5, 7
        rng = np.random.default_rng(10)
        emb = nn.Embedding(V, D)
        w = emb.weight.numpy()
        idx = Tensor(np.zeros(N, np.int32)).realize()

        @TinyJit
        def fwd(i):
            return emb(i).realize()

        for _ in range(5):
            ids = rng.integers(-2, V + 2, N).astype(np.int32)
            idx.assign(Tensor(ids)).realize()
            np.testing.assert_array_equal(fwd(idx).numpy(), np_rows(w, ids))

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_embedding_matches_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        from tinygrad import nn as tgnn
        layer = nn.Embedding(28, 20)
        tg_layer = tgnn.Embedding(28, 20)
        tg_layer.weight = TgTensor(layer.weight.numpy())
        ids = np.random.default_rng(11).integers(-2, 30, (4, 10)).astype(np.int32)
        np.testing.assert_allclose(layer(Tensor(ids)).numpy(), tg_layer(TgTensor(ids)).numpy())


@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestBridge(unittest.TestCase):
    def test_scatter_sum(self):
        base = np.arange(15, dtype=np.float32).reshape(5, 3)
        idx = np.array([[1, 4, 0], [1, -1, 0], [5, 2, 2]], np.int32)
        src = np.arange(9, dtype=np.float32).reshape(3, 3) + 0.5
        # The operand Tensors stay alive while their raw terms are in use: a
        # Tensor allocation may reclaim the buffer of a Tensor already gone.
        tb, ti, ts, ts2 = Tensor(base), Tensor(idx), Tensor(src), Tensor(src[:2])
        t = _TH.scatter(tb.term, ti.term, ts.term, 0, K.SCATTER_SUM, True)
        self.assertNotEqual(int(t), 0)
        want = base + np_scatter_add((5, 3), 0, idx, src)
        got = Tensor._from_term(t, dtypes.float32, (5, 3)).numpy()
        np.testing.assert_array_equal(got, want)
        self.assertEqual(int(_TH.scatter(tb.term, ti.term, ts2.term, 0, K.SCATTER_SUM, True)), 0)

    def test_backward_raises_grad_error(self):
        # A scatter kind with no gradient rule yet: backward() raises.
        base = Tensor(np.zeros((4, 2), np.float32))
        src = Tensor(np.ones((3, 2), np.float32))
        idx = Tensor(np.array([[0, 1], [2, 3], [1, 1]], np.int32))
        t = _TH.scatter(base.term, idx.term, src.term, 0, K.SCATTER_PROD, True)
        y = Tensor._from_term(t, dtypes.float32, (4, 2))
        with self.assertRaisesRegex(RuntimeError, "prod"):
            y.sum().backward()
        self.assertIsNone(_TH.grad_err_take())


if __name__ == "__main__":
    unittest.main()
