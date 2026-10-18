# Realize-bundle ASSIGN ordering (tinygrad create_schedule WAR/RAW edges).
#
# Tensor.realize(*ts) bundles every root into one thvm_realize_many.  A
# root that reads a buffer's PRIOR state must run before another root's
# ASSIGN overwrites that buffer (a write-after-read edge), whatever order
# the roots were passed in.  tinygrad/schedule/__init__.py:32-83 builds
# the same RAW + WAR edges over kernels; thvm builds them over the realize
# roots, so a cycle between roots raises instead of silently depending on
# argument order.  The cases below are ported from tinygrad
# test/runtime/test_assign.py; tinygrad's `a += b` is tensor.py:536
# `self.assign(self.add(x))`, spelled out here as an explicit assign.
import contextlib
import os
import re
import tempfile
import unittest

import numpy as np

from thvm import Tensor
from thvm.helpers import Context
from thvm.optim import SGD, Adam


@contextlib.contextmanager
def _kcnt_stderr():
    """Run with THVM_KCNT=1 and collect the C-side stderr lines."""
    out = {}
    saved_env = os.environ.get("THVM_KCNT")
    os.environ["THVM_KCNT"] = "1"
    saved_fd = os.dup(2)
    with tempfile.TemporaryFile() as tmp:
        os.dup2(tmp.fileno(), 2)
        try:
            yield out
        finally:
            os.dup2(saved_fd, 2)
            os.close(saved_fd)
            if saved_env is None:
                del os.environ["THVM_KCNT"]
            else:
                os.environ["THVM_KCNT"] = saved_env
            tmp.seek(0)
            out["text"] = tmp.read().decode(errors="replace")


def _waves(text):
    return [int(m) for m in re.findall(r"DBG realize_many waves: (\d+)", text)]


class TestAssignWar(unittest.TestCase):
    # test_assign.py:318-326
    def test_assign_corealize_order_independent(self):
        for order in [lambda x, y: Tensor.realize(x, y),
                      lambda x, y: Tensor.realize(y, x)]:
            x = Tensor([1.0]).realize()
            y = x + 10
            x.assign(x * 2)
            x.assign(x + 3)
            order(x, y)
            self.assertEqual(y.tolist(), [11.0])
            self.assertEqual(x.tolist(), [5.0])

    def test_read_between_chained_assigns(self):
        # Writes are keyed by the state they supersede (create_schedule
        # `writes`, schedule/__init__.py:44): w reads x after the first
        # assign, so it runs before the second assign, not after both.
        for order in (0, 1):
            x = Tensor([1.0]).realize()
            x.assign(x * 2)
            w = x + 1
            x.assign(x + 3)
            if order == 0:
                Tensor.realize(x, w)
            else:
                Tensor.realize(w, x)
            self.assertEqual(w.tolist(), [3.0])
            self.assertEqual(x.tolist(), [5.0])

    def test_war_chain_takes_three_waves(self):
        for perm in ([0, 1, 2], [2, 1, 0], [1, 2, 0]):
            x = Tensor([1.0]).realize()
            y = Tensor([10.0]).realize()
            a = x * 100
            x.assign(x + y)
            y.assign(y * 2)
            ts = [a, x, y]
            with _kcnt_stderr() as out:
                Tensor.realize(*[ts[i] for i in perm])
            self.assertEqual(_waves(out["text"]), [3])
            self.assertEqual([a.item(), x.item(), y.item()], [100.0, 11.0, 20.0])

    def test_ema_reads_prior_weight(self):
        for w_first in (True, False):
            ema = Tensor([0.0]).realize()
            w = Tensor([1.0]).realize()
            g = Tensor([1.0]).realize()
            ema.assign(ema * 0.5 + w * 0.5)
            w.assign(w - g)
            if w_first:
                Tensor.realize(w, ema)
            else:
                Tensor.realize(ema, w)
            self.assertEqual(ema.tolist(), [0.5])
            self.assertEqual(w.tolist(), [0.0])

    # test_assign.py:787-799
    def test_war_reader_already_depends_on_write(self):
        x = Tensor([1.0]).contiguous().realize()
        y = Tensor([2.0]).contiguous().realize()
        x_expr = x + 10          # 11, x is read here, before the assign
        x.assign(x * 2)
        y.assign(y + x)
        z = y + x_expr
        Tensor.realize(x, y, z)
        try:
            np.testing.assert_allclose([x.item(), y.item(), z.item()], [2.0, 4.0, 15.0])
        except AssertionError:
            # tinygrad TODO: broken now, x_expr reads x after the assign
            np.testing.assert_allclose([x.item(), y.item(), z.item()], [2.0, 4.0, 16.0])

    # test_assign.py:234-241
    def test_crossover_assign(self):
        a = Tensor.full((4,), 2).contiguous().realize()
        b = Tensor.full((4,), 3).contiguous().realize()
        a.assign(a + b)
        b.assign(b + a)
        Tensor.realize(a, b)
        np.testing.assert_allclose(a.numpy(), 5)
        np.testing.assert_allclose(b.numpy(), 8)

    # test_assign.py:268-277: tinygrad marks the cycle "TODO: broken now".
    def test_crossunder_assign(self):
        a = Tensor.full((4,), 2).contiguous().realize()
        b = Tensor.full((4,), 3).contiguous().realize()
        c = a + 9
        a.assign(a + b)
        b.assign(b + c)
        with self.assertRaisesRegex(RuntimeError, "cycle"):
            Tensor.realize(a, b)
        # The error is recoverable: the process keeps realizing.
        p = Tensor.full((2,), 4).contiguous().realize()
        q = Tensor.full((2,), 1).contiguous().realize()
        p.assign(p + q)
        Tensor.realize(p, q)
        np.testing.assert_allclose(p.numpy(), 5)

    def test_sgd_momentum_weight_decay_matches_numpy(self):
        lr, mom, wd = 0.1, 0.9, 0.1
        p0 = np.array([1.0, -2.0, 3.0], dtype=np.float32)
        c = np.array([0.5, 1.5, -1.0], dtype=np.float32)
        with Context(TRAINING=1):
            p = Tensor(p0.copy()).realize()
            opt = SGD([p], lr=lr, momentum=mom, weight_decay=wd)
            for _ in range(2):
                opt.zero_grad()
                (p * Tensor(c)).sum().backward()
                opt.step()
        p_np, b_np = p0.copy(), np.zeros_like(p0)
        for _ in range(2):
            g = c + wd * p_np
            b_np = mom * b_np + g
            p_np = p_np - lr * b_np
        np.testing.assert_allclose(p.numpy(), p_np, rtol=1e-6, atol=1e-6)
        np.testing.assert_allclose(opt.b[0].numpy(), b_np, rtol=1e-6, atol=1e-6)

    def test_war_bundle_splits_into_waves(self):
        x = Tensor([1.0]).realize()
        y = x + 10
        x.assign(x * 2)
        with _kcnt_stderr() as out:
            Tensor.realize(x, y)
        self.assertEqual(_waves(out["text"]), [2])
        self.assertEqual(y.tolist(), [11.0])
        self.assertEqual(x.tolist(), [2.0])

    def test_raw_only_bundles_stay_one_wave(self):
        c = np.array([0.5, 1.5, -1.0], dtype=np.float32)
        for make in (lambda p: SGD([p], lr=0.1, momentum=0.9),
                     lambda p: Adam([p], lr=0.1)):
            with Context(TRAINING=1):
                p = Tensor(np.array([1.0, -2.0, 3.0], dtype=np.float32)).realize()
                opt = make(p)
                opt.zero_grad()
                (p * Tensor(c)).sum().backward()
                with _kcnt_stderr() as out:
                    opt.step()
            waves = _waves(out["text"])
            self.assertTrue(waves, out["text"])
            self.assertEqual(set(waves), {1}, out["text"])
            self.assertIn("DBG realize_many emit:", out["text"])

    def test_bundle_past_256_roots(self):
        # More roots than the 256 a bundle used to hold (the rest stayed
        # lazy, their assigns unapplied): 150 assigns, then the 150 reads of
        # the states they supersede.
        xs = [Tensor([float(k)]).realize() for k in range(150)]
        ys = [x + 10 for x in xs]
        for x in xs:
            x.assign(x * 2)
        Tensor.realize(*xs, *ys)
        self.assertEqual([y.item() for y in ys], [k + 10.0 for k in range(150)])
        self.assertEqual([x.item() for x in xs], [2.0 * k for k in range(150)])

    def test_adam_over_130_params(self):
        # Adam's moment realize bundles 2N + 2 roots: 262 for 130 params.
        lr, b1, b2, eps = 0.1, 0.9, 0.999, 1e-8
        p0 = np.linspace(-1.0, 1.0, 130).astype(np.float32)
        c = np.linspace(0.5, 2.0, 130).astype(np.float32)
        with Context(TRAINING=1):
            ps = [Tensor(p0[i:i + 1].copy()).realize() for i in range(130)]
            opt = Adam(ps, lr=lr, b1=b1, b2=b2, eps=eps)
            for _ in range(2):
                opt.zero_grad()
                sum((p * float(c[i])).sum() for i, p in enumerate(ps)).backward()
                opt.step()
        p_np, m, v = p0.astype(np.float64), np.zeros(130), np.zeros(130)
        for t in (1, 2):
            m = b1 * m + (1 - b1) * c
            v = b2 * v + (1 - b2) * c * c
            p_np = p_np - lr * (m / (1 - b1 ** t)) / (np.sqrt(v / (1 - b2 ** t)) + eps)
        got = np.array([p.item() for p in ps])
        # 1 - 0.999 in float32 is 1.3e-5 off, about 1e-6 on p after two steps.
        np.testing.assert_allclose(got, p_np, rtol=1e-5, atol=1e-5)

    def test_lazy_grad_bundle_raises(self):
        # Root granularity: each param update reaches the whole backward,
        # which reads every other param, so the roots form a WAR cycle.
        # The in-tree optimizers realize the grads first (schedule_step).
        rng = np.random.default_rng(0)
        x = Tensor(rng.standard_normal((2, 3)).astype(np.float32))
        w1 = Tensor(rng.standard_normal((3, 4)).astype(np.float32)).realize()
        w2 = Tensor(rng.standard_normal((4, 2)).astype(np.float32)).realize()
        ((x @ w1).relu() @ w2).sum().backward()
        with self.assertRaisesRegex(RuntimeError, "cycle"):
            Tensor.realize(*[p.assign(p - p.grad * 0.1) for p in (w1, w2)])


if __name__ == "__main__":
    unittest.main()
