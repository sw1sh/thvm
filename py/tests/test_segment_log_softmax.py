"""The ATP listwise loss (tools/atp_enigma/listwise_net.py list_loss) on the
CPU backend, against tinygrad master and a float64 numpy reference.

The loss is a per-(step, problem) segment log-softmax: with X = s + lw (the
queue) and Xp = s + pm (its pending positives), each row r of step k in
segment b = seg[k, r],

    m   = full((S, B), -BIG).scatter_reduce(1, seg, X.detach(), "amax")
    den = zeros(S, B).scatter_reduce(1, seg, (X - m.gather(1, seg)).exp(), "sum")
    loss = sum(sw * (log(den + 1e-30) + m - log(num + 1e-30) - mp))

with mp and num the same over Xp.  The scores s come from the trainer's
history-free ListNet head (its H32 variant): relu(h1(phi) + he(eng)) through
out, minus kappa * raw.  The checks:
  - the loss and its gradient in s match tinygrad master and float64 numpy;
  - every parameter's gradient matches tinygrad master;
  - 10 TinyJit Adam steps on fresh inputs move the parameters as tinygrad
    master's do, step by step;
  - the Adam steps on both CPU routes, the C JIT and the UOp walker, in child
    processes.
The step realizes the loss before opt.step(): a loss realized after the
step reads the pre-step parameters on thvm's TinyJit replays (an open bug,
docs/NN/thvm_vs_tinygrad.md).

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_segment_log_softmax.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Device

NATIVE = Device.DEFAULT == "CPU"

BIG = 1e4
S, B, R, F, N_ENG, HID = 4, 3, 64, 6, 3, 8
STEPS = 10
LR = 3e-3


# ---- inputs -------------------------------------------------------------------

def draw(rng):
    """One batch with ListBatch.draw's layout: problem b owns rows
    [off_b, off_b + n_b), the rest is padding in segment 0 at -BIG; step
    slot k < steps_b holds a queue (log-weights lw, members only) with at
    least one pending positive (pm = 0), and sw[k, b] = 1 / (B steps_b)."""
    n = rng.integers(8, 20, B)
    off = np.concatenate([[0], np.cumsum(n)])
    phi = rng.standard_normal((R, F)).astype(np.float32)
    eng = rng.standard_normal((S, R, N_ENG)).astype(np.float32)
    raw = rng.uniform(0, 3, (S, R)).astype(np.float32)
    lw = np.full((S, R), -BIG, np.float32)
    pm = np.full((S, R), -BIG, np.float32)
    seg = np.zeros((S, R), np.int32)
    sw = np.zeros((S, B), np.float32)
    for b in range(B):
        sl = slice(int(off[b]), int(off[b + 1]))
        seg[:, sl] = b
        steps = int(rng.integers(1, S + 1))
        for k in range(steps):
            member = rng.random(n[b]) < 0.7
            member[rng.integers(n[b])] = True
            pos = member & (rng.random(n[b]) < 0.3)
            pos[np.flatnonzero(member)[0]] = True
            lw[k, sl] = np.where(member, np.log(rng.uniform(1, 4, n[b])), -BIG)
            pm[k, sl] = np.where(pos, 0.0, -BIG)
        sw[:steps, b] = 1.0 / (B * steps)
    return phi, eng, raw, lw, pm, seg, sw


def init_params(seed=0):
    rng = np.random.default_rng(seed)
    return {
        "h1.weight": (rng.standard_normal((HID, F)) / np.sqrt(F)).astype(np.float32),
        "h1.bias": (0.1 * rng.standard_normal(HID)).astype(np.float32),
        "he.weight": (rng.standard_normal((HID, N_ENG)) / np.sqrt(N_ENG)).astype(np.float32),
        "out.weight": rng.uniform(-0.5, 0.5, (1, HID)).astype(np.float32),
        "out.bias": np.zeros(1, np.float32),
        "kappa": np.ones(1, np.float32),
    }


# ---- the trainer's model and loss, for either framework -------------------------

def framework(name):
    if name == "thvm":
        from thvm import Context, Tensor, TinyJit, nn
    else:
        from tinygrad import Tensor, TinyJit, nn
        from tinygrad.helpers import Context
    return Context, Tensor, TinyJit, nn


class ListHead:
    """listwise_net.py ListNet with history off: rows, pairs and scores."""

    def __init__(self, fw, p0):
        _, Tensor, _, nn = framework(fw)
        self.h1 = nn.Linear(F, HID)
        self.he = nn.Linear(N_ENG, HID, bias=False)
        self.out = nn.Linear(HID, 1)
        self.h1.weight, self.h1.bias = Tensor(p0["h1.weight"]), Tensor(p0["h1.bias"])
        self.he.weight = Tensor(p0["he.weight"])
        self.out.weight, self.out.bias = Tensor(p0["out.weight"]), Tensor(p0["out.bias"])
        self.kappa = Tensor(p0["kappa"])

    def params(self):
        return {"h1.weight": self.h1.weight, "h1.bias": self.h1.bias,
                "he.weight": self.he.weight, "out.weight": self.out.weight,
                "out.bias": self.out.bias, "kappa": self.kappa}

    def scores(self, phi, eng, raw):
        Z = self.h1(phi)
        H = (Z.reshape(1, R, HID) + self.he(eng)).relu()
        return self.out(H).reshape(S, R) - self.kappa * raw

    def loss(self, Tensor, phi, eng, raw, lw, pm, seg, sw):
        return list_loss(Tensor, self.scores(phi, eng, raw), lw, pm, seg, sw)


def list_loss(Tensor, s, lw, pm, seg, sw):
    """listwise_net.py list_loss, verbatim."""
    S_, B_ = sw.shape
    X = s + lw
    Xp = s + pm
    m = Tensor.full((S_, B_), -BIG).scatter_reduce(1, seg, X.detach(), "amax")
    mp = Tensor.full((S_, B_), -BIG).scatter_reduce(1, seg, Xp.detach(), "amax")
    den = Tensor.zeros(S_, B_).scatter_reduce(1, seg, (X - m.gather(1, seg)).exp(), "sum")
    num = Tensor.zeros(S_, B_).scatter_reduce(1, seg, (Xp - mp.gather(1, seg)).exp(), "sum")
    return (sw * ((den + 1e-30).log() + m - (num + 1e-30).log() - mp)).sum()


def np_list_loss(s, lw, pm, seg, sw):
    """The same formula in float64, with its gradient in s (m and mp carry
    none: they come from the detached scores)."""
    s = s.astype(np.float64)
    loss, g = 0.0, np.zeros_like(s)
    for k in range(sw.shape[0]):
        for b in range(sw.shape[1]):
            rows = seg[k] == b
            X = s[k, rows] + lw[k, rows]
            Xp = s[k, rows] + pm[k, rows]
            m = max(-BIG, X.max()) if rows.any() else -BIG
            mp = max(-BIG, Xp.max()) if rows.any() else -BIG
            e, ep = np.exp(X - m), np.exp(Xp - mp)
            den, num = e.sum() + 1e-30, ep.sum() + 1e-30
            loss += sw[k, b] * (np.log(den) + m - np.log(num) - mp)
            g[k, rows] = sw[k, b] * (e / den - ep / num)
    return loss, g


def tensors(fw, arrays):
    _, Tensor, _, _ = framework(fw)
    return [Tensor(a).realize() for a in arrays]


# ---- cases (module level: the route tests run them in child processes) -----

def case_loss_and_grad(fw="thvm"):
    """The loss and its gradient in s against float64 numpy, s a leaf."""
    _, Tensor, _, _ = framework(fw)
    rng = np.random.default_rng(1)
    _, _, _, lw, pm, seg, sw = draw(rng)
    s0 = (3 * rng.standard_normal((S, R))).astype(np.float32)
    s = Tensor(s0)
    lw_t, pm_t, seg_t, sw_t = tensors(fw, (lw, pm, seg, sw))
    loss = list_loss(Tensor, s, lw_t, pm_t, seg_t, sw_t)
    loss.backward()
    want, g = np_list_loss(s0, lw, pm, seg, sw)
    np.testing.assert_allclose(float(loss.numpy()), want, rtol=1e-5)
    np.testing.assert_allclose(s.grad.numpy(), g, rtol=1e-4, atol=1e-6)
    return float(loss.numpy()), s.grad.numpy()


def model_grads(fw, p0, arrays):
    _, Tensor, _, _ = framework(fw)
    model = ListHead(fw, p0)
    loss = model.loss(Tensor, *tensors(fw, arrays))
    loss.backward()
    return float(loss.numpy()), {k: v.grad.numpy() for k, v in model.params().items()}


def jit_adam(fw, p0, batches):
    """STEPS TinyJit Adam steps of the trainer's form, the loss realized
    before opt.step(); returns the losses and the parameters after each
    step.  out.bias stays out of the optimizer: a softmax ignores a shift,
    so its gradient is 0 up to rounding, and Adam's first step moves it by
    lr times the sign of that rounding."""
    Context, Tensor, TinyJit, nn = framework(fw)
    model = ListHead(fw, p0)
    params = model.params()
    names = [k for k in params if k != "out.bias"]
    out_losses, out_params = [], []
    with Context(TRAINING=1):
        opt = nn.optim.Adam([params[k] for k in names], lr=LR)

        def step(*ins):
            loss = model.loss(Tensor, *ins)
            opt.zero_grad()
            loss.backward()
            loss.realize()
            opt.step()
            return loss
        jit = TinyJit(step)
        for arrays in batches:
            out_losses.append(float(jit(*tensors(fw, arrays)).numpy()))
            out_params.append({k: params[k].numpy().copy() for k in names})
    return out_losses, out_params


def adam_batches(seed=2):
    rng = np.random.default_rng(seed)
    return [draw(rng) for _ in range(STEPS)]


def np_adam(p0, batches):
    """The float64 reference of jit_adam: forward and backward of the head
    in numpy, then Adam (tinygrad's defaults b1 0.9, b2 0.999, eps 1e-8)."""
    p = {k: v.astype(np.float64) for k, v in p0.items()}
    m = {k: np.zeros_like(v) for k, v in p.items()}
    v2 = {k: np.zeros_like(v) for k, v in p.items()}
    losses, hist = [], []
    for i, (phi, eng, raw, lw, pm, seg, sw) in enumerate(batches):
        Z = phi @ p["h1.weight"].T + p["h1.bias"]
        Hpre = Z[None] + eng @ p["he.weight"].T
        H = np.maximum(Hpre, 0)
        s = (H @ p["out.weight"].T)[..., 0] + p["out.bias"][0] - p["kappa"][0] * raw
        loss, gs = np_list_loss(s, lw, pm, seg, sw)
        gH = gs[..., None] * p["out.weight"][0]
        gHpre = gH * (Hpre > 0)
        gZ = gHpre.sum(0)
        g = {"out.weight": np.einsum("kr,krh->h", gs, H)[None],
             "out.bias": np.array([gs.sum()]),
             "kappa": np.array([-(gs * raw).sum()]),
             "he.weight": np.einsum("krh,kre->he", gHpre, eng),
             "h1.weight": gZ.T @ phi,
             "h1.bias": gZ.sum(0)}
        for k in p:
            if k == "out.bias":
                continue
            m[k] = 0.9 * m[k] + 0.1 * g[k]
            v2[k] = 0.999 * v2[k] + 0.001 * g[k] * g[k]
            p[k] = p[k] - LR * (m[k] / (1 - 0.9 ** (i + 1))) / (
                np.sqrt(v2[k] / (1 - 0.999 ** (i + 1))) + 1e-8)
        losses.append(loss)
        hist.append({k: v.copy() for k, v in p.items()})
    return losses, hist


def case_jit_adam():
    """thvm's 10 jitted Adam steps against float64 numpy (and tinygrad
    master when it is importable): parameters after every step, and the
    losses."""
    p0 = init_params()
    batches = adam_batches()
    losses, hist = jit_adam("thvm", p0, batches)
    want_losses, want_hist = np_adam(p0, batches)
    np.testing.assert_allclose(losses, want_losses, rtol=1e-4)
    for i, (got, want) in enumerate(zip(hist, want_hist)):
        for k in got:
            np.testing.assert_allclose(got[k], want[k], rtol=1e-5, atol=1e-6,
                                       err_msg=f"step {i} {k} vs numpy")
    if helpers.have_tinygrad():
        tg_losses, tg_hist = jit_adam("tinygrad", p0, batches)
        np.testing.assert_allclose(losses, tg_losses, rtol=1e-4)
        for i, (got, want) in enumerate(zip(hist, tg_hist)):
            for k in got:
                np.testing.assert_allclose(got[k], want[k], rtol=1e-5, atol=1e-6,
                                           err_msg=f"step {i} {k} vs tinygrad")


ROUTE_CASES = [
    ("loss-and-grad", case_loss_and_grad),
    ("jit-adam", case_jit_adam),
]


# ---- tests ------------------------------------------------------------------

@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestSegmentLogSoftmax(unittest.TestCase):
    def test_loss_and_grad_numpy(self):
        case_loss_and_grad()

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_loss_and_grad_tinygrad(self):
        loss, g = case_loss_and_grad("thvm")
        tg_loss, tg_g = case_loss_and_grad("tinygrad")
        np.testing.assert_allclose(loss, tg_loss, rtol=1e-5)
        np.testing.assert_allclose(g, tg_g, rtol=1e-4, atol=1e-6)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_param_grads_tinygrad(self):
        p0 = init_params()
        arrays = draw(np.random.default_rng(3))
        loss, g = model_grads("thvm", p0, arrays)
        tg_loss, tg_g = model_grads("tinygrad", p0, arrays)
        np.testing.assert_allclose(loss, tg_loss, rtol=1e-5)
        for k in g:
            np.testing.assert_allclose(g[k], tg_g[k], rtol=1e-4, atol=1e-6, err_msg=k)

    def test_jit_adam(self):
        case_jit_adam()


@unittest.skipUnless(NATIVE, "native scatter runs on the CPU backend")
class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_segment_log_softmax"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
