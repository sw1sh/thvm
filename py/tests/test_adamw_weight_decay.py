"""AdamW decoupled weight decay parity vs tinygrad.

Before the fix, AdamW was an alias for Adam and silently ignored
weight_decay (the **_ kwarg swallow). Now AdamW applies the decoupled
decay up += wd*p (tinygrad nn/optim.py:175,182), and plain Adam (wd=0)
stays byte-identical (the wd term is gated off).

Parity is checked at STEP 1 (exact vs tinygrad). A multi-step eager loop
is deliberately NOT used for the tinygrad cross-check: an eager (non-JIT)
multi-step Adam loop diverges from tinygrad from step 2 onward via a
pre-existing C-global GRAD_SLOT/requires_grad corruption (see fa1c448a) --
orthogonal to weight decay and JIT-immune (beautiful_mnist trains fine).
test_adamw_actually_decays uses multi-step but compares thvm-vs-thvm so
that corruption cancels and only the decay effect remains.
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


def _problem():
    np.random.seed(0)
    return np.random.randn(8).astype(np.float32), np.random.randn(8).astype(np.float32)


def _thvm_steps(make_opt, steps):
    w0, target = _problem()
    w = Tensor(w0.copy()).requires_grad_(True)
    opt = make_opt([w])
    Tensor.training = True
    for _ in range(steps):
        opt.zero_grad()
        loss = ((w - Tensor(target)) * (w - Tensor(target))).sum()
        loss.backward()
        opt.step()
    return w.numpy()


def _tg_steps(make_opt, steps):
    w0, target = _problem()
    w = TgTensor(w0.copy(), requires_grad=True)
    opt = make_opt([w])
    TgTensor.training = True
    for _ in range(steps):
        opt.zero_grad()
        loss = ((w - TgTensor(target)) * (w - TgTensor(target))).sum()
        loss.backward()
        opt.step()
    return w.numpy()


class TestAdamWWeightDecay(unittest.TestCase):
    def test_adamw_actually_decays(self):
        # AdamW with a large wd must pull weights closer to 0 than wd=0 Adam
        # (thvm-vs-thvm, so the eager multi-step corruption cancels out).
        w_adam = _thvm_steps(lambda p: optim.Adam(p, lr=0.05), 15)
        w_adamw = _thvm_steps(lambda p: optim.AdamW(p, lr=0.05, weight_decay=0.5), 15)
        self.assertLess(
            float(np.abs(w_adamw).sum()), float(np.abs(w_adam).sum()),
            f"AdamW(wd=0.5) |w|={np.abs(w_adamw).sum():.4f} not < "
            f"Adam |w|={np.abs(w_adam).sum():.4f} -- decay had no effect")

    def test_adamw_decay_term_exact(self):
        # Step 1: AdamW = Adam minus the decoupled decay lr*wd*p0 (the Adam
        # part is identical at step 1). Analytic, no tinygrad dependency.
        w0, _ = _problem()
        lr, wd = 0.05, 0.1
        w_adam = _thvm_steps(lambda p: optim.Adam(p, lr=lr), 1)
        w_adamw = _thvm_steps(lambda p: optim.AdamW(p, lr=lr, weight_decay=wd), 1)
        self.assertLess(float(np.abs((w_adam - w_adamw) - lr * wd * w0).max()), 1e-5,
                        "AdamW step-1 decay term != lr*wd*p0")

    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_adamw_step1_matches_tinygrad(self):
        w_th = _thvm_steps(lambda p: optim.AdamW(p, lr=0.05, weight_decay=0.1), 1)
        w_tg = _tg_steps(lambda p: tg_optim.AdamW(p, lr=0.05, weight_decay=0.1), 1)
        self.assertLess(float(np.abs(w_th - w_tg).max()), 1e-5,
                        f"thvm AdamW {w_th[:3]} vs tinygrad {w_tg[:3]}")

    @unittest.skipUnless(_HAVE_TG, "tinygrad not importable")
    def test_adam_wd0_step1_matches_tinygrad(self):
        # Plain Adam (wd gated off) still matches tinygrad Adam at step 1.
        w_th = _thvm_steps(lambda p: optim.Adam(p, lr=0.05), 1)
        w_tg = _tg_steps(lambda p: tg_optim.Adam(p, lr=0.05), 1)
        self.assertLess(float(np.abs(w_th - w_tg).max()), 1e-5,
                        f"thvm Adam {w_th[:3]} vs tinygrad {w_tg[:3]}")


if __name__ == "__main__":
    unittest.main()
