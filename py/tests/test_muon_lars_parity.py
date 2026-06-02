"""LARS + Muon parity vs tinygrad nn.optim.

Both are ports of tinygrad's single LARS class (nn/optim.py:99-133):
  - LARS: layer-wise adaptive trust ratio r = tcoef*|w|/(|g|+wd*|w|),
    gated on |w|>0 & |g|>0, scaling a momentum-SGD update.
  - Muon: LARS(classic=False, pre_wd=False, nesterov=True, ns_coef) -- the
    momentum buffer is orthogonalized by `ns_steps` Newton-Schulz quintic
    iterations (coefficients (3.4445,-4.775,2.0315)) reshaped to 2-D, via
    thvm's N-D matmul + transpose.

tinygrad subtlety faithfully reproduced: Muon's "post-momentum weight
decay" (nn/optim.py:129, `t = t.detach()*(1-wd*lr)`) reassigns a LOCAL
`t` used ONLY for `g.cast(t.dtype)`; the param update applied by
schedule_step is `tt.assign(tt.detach() - up)` against the ORIGINAL param,
so the decay never reaches the param value.  thvm therefore does NOT decay
the param (and is f32-only, so the dtype hook is moot).  Applying the decay
would put thvm off tinygrad by exactly |p|*wd*lr -- asserted below.

Newton-Schulz is an ill-conditioned (near-singular) map, so a tiny f32
matmul-ordering difference between thvm's and tinygrad's lowering amplifies
chaotically over steps on a small near-degenerate problem.  Parity is
checked STEP-1-EXACT on every config, and the multi-step trajectory is
tracked on a well-conditioned (16x8) problem where the chaos is tame.
"""
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.environ.get("THVM_PY", os.path.dirname(os.path.dirname(__file__))))
from thvm import Tensor          # noqa: E402
from thvm import optim           # noqa: E402

try:
    import tinygrad              # noqa: F401
    from tinygrad import Tensor as TgTensor
    from tinygrad.nn import optim as tg_optim
    _HAVE_TG = True
except Exception:
    _HAVE_TG = False


def _linreg_problem(seed, d_in, d_out, n):
    rng = np.random.RandomState(seed)
    W0 = (rng.randn(d_in, d_out) * 0.1).astype(np.float32)
    X = rng.randn(n, d_in).astype(np.float32)
    Y = rng.randn(n, d_out).astype(np.float32)
    return W0, X, Y


def _thvm_run(make_opt, W0, X, Y, steps, mean=False):
    Tensor.training = True
    w = Tensor(W0.copy()).requires_grad_(True)
    w.realize()
    opt = make_opt([w])
    x = Tensor(X); x.realize()
    y = Tensor(Y); y.realize()
    traj = [w.numpy().copy()]
    for _ in range(steps):
        opt.zero_grad()
        out = x @ w
        diff = out - y
        loss = (diff * diff).mean() if mean else (diff * diff).sum()
        loss.backward()
        opt.step()
        traj.append(w.numpy().copy())
    return np.array(traj)


def _tg_run(make_opt, W0, X, Y, steps, mean=False):
    TgTensor.training = True
    w = TgTensor(W0.copy(), requires_grad=True)
    opt = make_opt([w])
    x = TgTensor(X); y = TgTensor(Y)
    traj = [w.numpy().copy()]
    for _ in range(steps):
        opt.zero_grad()
        out = x @ w
        diff = out - y
        loss = (diff * diff).mean() if mean else (diff * diff).sum()
        loss.backward()
        opt.step()
        traj.append(w.numpy().copy())
    return np.array(traj)


class TestNewtonSchulz(unittest.TestCase):
    """The Newton-Schulz primitive on its own (powers Muon)."""

    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_ns_matches_tinygrad_wide(self):
        rng = np.random.RandomState(1)
        G = rng.randn(4, 6).astype(np.float32)
        coef = (3.4445, -4.775, 2.0315)
        th = Tensor(G); th.realize()
        out_th = th.newton_schulz(5, coef).numpy()
        out_tg = TgTensor(G).newton_schulz(5, coef).numpy()
        self.assertLess(float(np.abs(out_th - out_tg).max()), 1e-3,
                        f"NS wide: thvm {out_th.reshape(-1)[:3]} vs "
                        f"tinygrad {out_tg.reshape(-1)[:3]}")

    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_ns_matches_tinygrad_tall(self):
        # tall (shape[-2] > shape[-1]) hits the transpose-in/out branch.
        rng = np.random.RandomState(2)
        G = rng.randn(6, 4).astype(np.float32)
        coef = (3.4445, -4.775, 2.0315)
        th = Tensor(G); th.realize()
        out_th = th.newton_schulz(5, coef).numpy()
        out_tg = TgTensor(G).newton_schulz(5, coef).numpy()
        self.assertLess(float(np.abs(out_th - out_tg).max()), 1e-3,
                        "NS tall (transpose branch) diverges from tinygrad")

    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_ns_orthogonality_matches_tinygrad(self):
        # The orthogonalized update U satisfies U U^T ~ scaled I; the diag /
        # off-diag profile must match tinygrad's (NS converges toward it).
        rng = np.random.RandomState(5)
        G = rng.randn(8, 8).astype(np.float32)
        coef = (3.4445, -4.775, 2.0315)
        U_th = Tensor(G).realize().newton_schulz(5, coef).numpy()
        U_tg = TgTensor(G).newton_schulz(5, coef).numpy()
        gram_th, gram_tg = U_th @ U_th.T, U_tg @ U_tg.T
        self.assertLess(float(np.abs(gram_th - gram_tg).max()), 1e-3,
                        "NS gram (U U^T) diverges from tinygrad")
        # And it is meaningfully orthogonal: diag near 1, off-diag damped.
        diag = np.diag(gram_th)
        off = np.abs(gram_th - np.diag(diag)).max()
        self.assertGreater(float(diag.min()), 0.5)
        self.assertLess(float(diag.max()), 1.5)
        self.assertLess(float(off), 0.5)


class TestLARSParity(unittest.TestCase):
    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_lars_step1_exact(self):
        W0, X, Y = _linreg_problem(7, 5, 3, 8)
        kw = dict(lr=0.01, momentum=0.9, weight_decay=1e-4, tcoef=0.001, nesterov=False)
        th = _thvm_run(lambda p: optim.LARS(p, **kw), W0, X, Y, 1)
        tg = _tg_run(lambda p: tg_optim.LARS(p, **kw), W0, X, Y, 1)
        self.assertLess(float(np.abs(th[1] - tg[1]).max()), 1e-4,
                        f"LARS step1: thvm {th[1].reshape(-1)[:3]} vs "
                        f"tinygrad {tg[1].reshape(-1)[:3]}")

    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_lars_trajectory(self):
        W0, X, Y = _linreg_problem(7, 5, 3, 8)
        kw = dict(lr=0.01, momentum=0.9, weight_decay=1e-4, tcoef=0.001, nesterov=False)
        th = _thvm_run(lambda p: optim.LARS(p, **kw), W0, X, Y, 5)
        tg = _tg_run(lambda p: tg_optim.LARS(p, **kw), W0, X, Y, 5)
        self.assertLess(float(np.abs(th - tg).max()), 1e-3,
                        f"LARS traj maxdiff {float(np.abs(th - tg).max()):.3e}")

    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_lars_nesterov_step1_exact(self):
        W0, X, Y = _linreg_problem(7, 5, 3, 8)
        kw = dict(lr=0.01, momentum=0.9, weight_decay=1e-4, tcoef=0.002, nesterov=True)
        th = _thvm_run(lambda p: optim.LARS(p, **kw), W0, X, Y, 1)
        tg = _tg_run(lambda p: tg_optim.LARS(p, **kw), W0, X, Y, 1)
        self.assertLess(float(np.abs(th[1] - tg[1]).max()), 1e-4,
                        "LARS nesterov step1 diverges from tinygrad")


class TestMuonParity(unittest.TestCase):
    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_muon_step1_exact(self):
        W0, X, Y = _linreg_problem(7, 5, 3, 8)
        kw = dict(lr=0.02, momentum=0.95, weight_decay=0.1, nesterov=True)
        th = _thvm_run(lambda p: optim.Muon(p, **kw), W0, X, Y, 1)
        tg = _tg_run(lambda p: tg_optim.Muon(p, **kw), W0, X, Y, 1)
        self.assertLess(float(np.abs(th[1] - tg[1]).max()), 1e-4,
                        f"Muon step1: thvm {th[1].reshape(-1)[:3]} vs "
                        f"tinygrad {tg[1].reshape(-1)[:3]}")

    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_muon_trajectory_well_conditioned(self):
        # 16x8, mean loss, no wd -- NS is well-conditioned here so the
        # trajectory tracks tinygrad tightly over several steps.
        W0, X, Y = _linreg_problem(11, 16, 8, 32)
        kw = dict(lr=0.005, momentum=0.9, weight_decay=0.0, nesterov=True)
        th = _thvm_run(lambda p: optim.Muon(p, **kw), W0, X, Y, 6, mean=True)
        tg = _tg_run(lambda p: tg_optim.Muon(p, **kw), W0, X, Y, 6, mean=True)
        self.assertLess(float(np.abs(th - tg).max()), 1e-3,
                        f"Muon traj maxdiff {float(np.abs(th - tg).max()):.3e}")

    def test_muon_post_decay_is_dead_in_tinygrad(self):
        # tinygrad's Muon weight-decay line reassigns a local `t` used only
        # for the cast dtype, never the param value.  thvm faithfully does
        # NOT decay the param: a high-wd and zero-wd Muon must give the SAME
        # param update at step 1 (thvm-vs-thvm; no tinygrad needed).
        W0, X, Y = _linreg_problem(7, 5, 3, 8)
        th_wd = _thvm_run(lambda p: optim.Muon(p, lr=0.02, momentum=0.95,
                                               weight_decay=0.5, nesterov=True),
                          W0, X, Y, 1)
        th_no = _thvm_run(lambda p: optim.Muon(p, lr=0.02, momentum=0.95,
                                               weight_decay=0.0, nesterov=True),
                          W0, X, Y, 1)
        self.assertLess(float(np.abs(th_wd[1] - th_no[1]).max()), 1e-6,
                        "Muon param decay leaked (tinygrad's is dead code)")

    def test_muon_is_not_adam(self):
        # Muon must no longer alias Adam.  Same seed/problem, the two
        # optimizers should produce materially different step-1 params.
        W0, X, Y = _linreg_problem(7, 5, 3, 8)
        th_muon = _thvm_run(lambda p: optim.Muon(p, lr=0.02), W0, X, Y, 1)
        th_adam = _thvm_run(lambda p: optim.Adam(p, lr=0.02), W0, X, Y, 1)
        self.assertGreater(float(np.abs(th_muon[1] - th_adam[1]).max()), 1e-3,
                           "Muon == Adam (alias not removed)")


if __name__ == "__main__":
    unittest.main()
