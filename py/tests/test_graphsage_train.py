"""GraphSAGE with mean aggregation trains under TinyJit on the CPU backend.

A two-layer GraphSAGE (Hamilton et al. 2017, the "mean" aggregator) regresses
a node target on a graph of N = 1000 nodes and E = 1e4 edge slots, width
D = 32.  Each layer transforms, then averages over the incoming edges:

    P = H @ Wn^T                                       (nn.Linear, no bias)
    A = zeros(N, D).scatter_reduce(0, dst, P.gather(0, src), "mean",
                                   include_self=False)
    H' = relu(H @ Ws^T + bs + A)                        (no relu on layer 2)

so a node with no incoming edge aggregates 0, and the gradient runs back
through the mean's division, the scatter (gathered back to the messages) and
the gather (scatter-added onto P).  The loss is the mean squared error.

Edges come as fixed-size slot buffers padded with -1 at both ends, the
padding the ATP trainers use: a padded slot reads a zero row and its message
is never written.  Every step passes fresh buffers: the real edges of a base
graph with a random tenth of them dropped, shuffled into the slots.  The
checks:
  - the step-0 gradients match a float64 numpy backward;
  - 10 TinyJit Adam steps (warmup, capture, 8 replays) track a numpy Adam
    run on the same per-step graphs, so a replay that kept a stale index
    buffer would show, and the loss falls every step;
  - padded slots change nothing: the same edges compact and padded give the
    same loss and gradients, bit for bit.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_graphsage_train.py -q
"""
import time
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Context, Device, GlobalCounters, Tensor, TinyJit, nn

N, E, D = 1000, 10_000, 32
E_REAL, STEPS, LR = 8_000, 10, 1e-2
B1, B2, EPS = 0.9, 0.999, 1e-8
PARAMS = ("Ws1", "bs1", "Wn1", "Ws2", "bs2", "Wn2")


def make_data(seed=0):
    rng = np.random.default_rng(seed)
    X = rng.standard_normal((N, D)).astype(np.float32)
    Y = np.tanh(X @ rng.standard_normal((D, D)) / np.sqrt(D)).astype(np.float32)
    src = rng.integers(0, N, E_REAL)
    dst = rng.integers(0, N, E_REAL)
    bound = 1.0 / np.sqrt(D)
    p = {k: rng.uniform(-bound, bound, (D,) if k.startswith("b") else (D, D)).astype(np.float32)
         for k in PARAMS}
    return rng, X, Y, src, dst, p


def padded_slots(rng, src, dst, keep):
    """The kept edges, in their order, at random positions of E slots; the
    other slots are -1 at both ends."""
    pos = np.sort(rng.choice(E, int(keep.sum()), replace=False))
    s = np.full(E, -1, np.int64)
    t = np.full(E, -1, np.int64)
    s[pos], t[pos] = src[keep], dst[keep]
    return s, t


def expand_ids(ids):
    """(E,) ids -> the (E, D) index a row gather / scatter takes."""
    return Tensor(np.repeat(ids[:, None], D, 1).astype(np.int32))


# ---- numpy reference (float64) ------------------------------------------------

def np_agg(P, src, dst):
    ok = dst >= 0
    S = np.zeros((N, D))
    deg = np.zeros(N)
    np.add.at(S, dst[ok], P[src[ok]])
    np.add.at(deg, dst[ok], 1.0)
    return S / np.maximum(deg, 1.0)[:, None], deg


def np_agg_backward(dA, src, dst, deg):
    ok = dst >= 0
    dP = np.zeros((N, D))
    np.add.at(dP, src[ok], dA[dst[ok]] / deg[dst[ok]][:, None])
    return dP


def np_loss_grads(p, X, Y, src, dst):
    X = X.astype(np.float64)
    P1 = X @ p["Wn1"].T
    A1, deg = np_agg(P1, src, dst)
    Z1 = X @ p["Ws1"].T + p["bs1"] + A1
    H1 = np.maximum(Z1, 0.0)
    P2 = H1 @ p["Wn2"].T
    A2, _ = np_agg(P2, src, dst)
    Z2 = H1 @ p["Ws2"].T + p["bs2"] + A2
    diff = Z2 - Y
    loss = float((diff * diff).mean())
    dZ2 = 2.0 * diff / diff.size
    dP2 = np_agg_backward(dZ2, src, dst, deg)
    dZ1 = (dZ2 @ p["Ws2"] + dP2 @ p["Wn2"]) * (Z1 > 0)
    dP1 = np_agg_backward(dZ1, src, dst, deg)
    g = {"Ws2": dZ2.T @ H1, "bs2": dZ2.sum(0), "Wn2": dP2.T @ H1,
         "Ws1": dZ1.T @ X, "bs1": dZ1.sum(0), "Wn1": dP1.T @ X}
    return loss, g


# ---- the thvm model -------------------------------------------------------------

class GraphSAGE:
    def __init__(self, p):
        self.self1, self.neigh1 = nn.Linear(D, D), nn.Linear(D, D, bias=False)
        self.self2, self.neigh2 = nn.Linear(D, D), nn.Linear(D, D, bias=False)
        self.self1.weight, self.self1.bias = Tensor(p["Ws1"]), Tensor(p["bs1"])
        self.self2.weight, self.self2.bias = Tensor(p["Ws2"]), Tensor(p["bs2"])
        self.neigh1.weight, self.neigh2.weight = Tensor(p["Wn1"]), Tensor(p["Wn2"])

    def params(self):
        return {"Ws1": self.self1.weight, "bs1": self.self1.bias, "Wn1": self.neigh1.weight,
                "Ws2": self.self2.weight, "bs2": self.self2.bias, "Wn2": self.neigh2.weight}

    @staticmethod
    def layer(H, lin_self, lin_neigh, src_e, dst_e):
        msg = lin_neigh(H).gather(0, src_e)
        agg = Tensor.zeros(N, D).scatter_reduce(0, dst_e, msg, "mean", include_self=False)
        return lin_self(H) + agg

    def loss(self, X, Y, src_e, dst_e):
        h = self.layer(X, self.self1, self.neigh1, src_e, dst_e).relu()
        diff = self.layer(h, self.self2, self.neigh2, src_e, dst_e) - Y
        return (diff * diff).mean()


def step0_grads(p, X, Y, src, dst):
    """thvm's loss and gradients for one eager forward / backward."""
    model = GraphSAGE(p)
    loss = model.loss(Tensor(X), Tensor(Y), expand_ids(src), expand_ids(dst))
    loss.backward()
    return float(loss.numpy()), {k: t.grad.numpy() for k, t in model.params().items()}


@unittest.skipUnless(Device.DEFAULT == "CPU", "native gather / scatter run on the CPU backend")
class TestGraphSAGETrain(unittest.TestCase):
    def test_padded_slots_change_nothing(self):
        rng, X, Y, src, dst, p = make_data(1)
        loss_c, g_c = step0_grads(p, X, Y, src, dst)
        s, t = padded_slots(rng, src, dst, np.ones(E_REAL, bool))
        self.assertEqual(int((s >= 0).sum()), E_REAL)
        loss_p, g_p = step0_grads(p, X, Y, s, t)
        self.assertEqual(loss_c, loss_p)
        for k in PARAMS:
            np.testing.assert_array_equal(g_c[k], g_p[k], err_msg=k)
        want_loss, want = np_loss_grads(p, X, Y, src, dst)
        self.assertAlmostEqual(loss_c, want_loss, delta=1e-5 * want_loss)

    def test_jit_adam_steps(self):
        rng, X, Y, src, dst, p0 = make_data(0)
        model = GraphSAGE(p0)
        params = model.params()
        # Data, not parameters: detached, so backward() gives them no gradient
        # to accumulate across steps.
        Xt, Yt = Tensor(X).realize().detach(), Tensor(Y).realize().detach()
        with Context(TRAINING=1):
            opt = nn.optim.Adam([params[k] for k in PARAMS], lr=LR, b1=B1, b2=B2, eps=EPS)

            @TinyJit
            def step(src_e, dst_e):
                opt.zero_grad()
                loss = model.loss(Xt, Yt, src_e, dst_e)
                loss.backward()
                return loss.realize(*opt.schedule_step())

            p = {k: v.astype(np.float64) for k, v in p0.items()}
            m = {k: np.zeros_like(v) for k, v in p.items()}
            v2 = {k: np.zeros_like(v) for k, v in p.items()}
            losses, want_losses, times = [], [], []
            for i in range(STEPS):
                GlobalCounters.reset()
                keep = rng.random(E_REAL) >= 0.1
                s, t = padded_slots(rng, src, dst, keep)
                t0 = time.perf_counter()
                loss = step(expand_ids(s), expand_ids(t))
                losses.append(float(loss.numpy()))
                times.append(time.perf_counter() - t0)
                want_loss, g = np_loss_grads(p, X, Y, s, t)
                want_losses.append(want_loss)
                if i == 0:
                    for k in PARAMS:
                        np.testing.assert_allclose(params[k].grad.numpy(), g[k],
                                                   rtol=1e-4, atol=1e-6, err_msg=k)
                for k in PARAMS:
                    m[k] = B1 * m[k] + (1 - B1) * g[k]
                    v2[k] = B2 * v2[k] + (1 - B2) * g[k] * g[k]
                    m_hat = m[k] / (1 - B1 ** (i + 1))
                    v_hat = v2[k] / (1 - B2 ** (i + 1))
                    p[k] = p[k] - LR * m_hat / (np.sqrt(v_hat) + EPS)
        print(f"\nGraphSAGE N={N} E={E} D={D}: loss {losses[0]:.5f} -> {losses[-1]:.5f}; "
              f"step times (s) {', '.join(f'{x:.3f}' for x in times)}")
        np.testing.assert_allclose(losses, want_losses, rtol=1e-4)
        self.assertTrue(all(b < a for a, b in zip(losses, losses[1:])), losses)
        for k in PARAMS:
            np.testing.assert_allclose(params[k].numpy(), p[k], rtol=1e-3, atol=1e-5, err_msg=k)


if __name__ == "__main__":
    unittest.main()
