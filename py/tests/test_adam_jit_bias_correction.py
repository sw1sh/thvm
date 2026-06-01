# Faithfulness test: Adam bias-correction must ADVANCE under TinyJit replay.
#
# tinygrad (nn/optim.py:161-183) keeps the bias-correction state b1_t,b2_t
# as Tensors advanced by an ON-GRAPH kernel (b1_t *= b1) that is in the
# realize set, so TinyJit captures the advance and re-fires it each replay.
#
# thvm previously advanced t HOST-SIDE in Python (self.t += 1; ten_write of
# 1/bc1, 1/bc2).  TinyJit captures the Python body ONCE; replay only re-fires
# the CAPTURED kernels, so the host-side advance is DEAD after step 1 -- the
# effective bias correction is frozen at the t=1 value forever.
#
# This test runs N>1 Adam steps under TinyJit and asserts the bias-correction
# factor used at step k matches the tinygrad value 1-b1**k (i.e. it advances).
import unittest
import numpy as np

from thvm import Tensor, TinyJit, nn
from thvm.helpers import getenv


def _bc_inv_values(opt):
    """Read the per-step inverse bias-correction (1/(1-b1**t)) currently
    sitting in the optimizer state, however it is stored."""
    if hasattr(opt, "b1_t"):
        b1_t = float(np.asarray(opt.b1_t.numpy()).reshape(-1)[0])
        b2_t = float(np.asarray(opt.b2_t.numpy()).reshape(-1)[0])
        return 1.0 / (1.0 - b1_t), 1.0 / (1.0 - b2_t)
    # legacy host-side storage
    bc1_inv = float(np.asarray(opt._bc1_inv.numpy()).reshape(-1)[0])
    bc2_inv = float(np.asarray(opt._bc2_inv.numpy()).reshape(-1)[0])
    return bc1_inv, bc2_inv


class TestAdamJitBiasCorrection(unittest.TestCase):
    def setUp(self):
        # Restore Tensor.training after the test so eval-mode tests
        # (BatchNorm in test_mnist_components) aren't polluted by a leaked
        # global -- both test cases need training=True for the optimizer.
        self._prev_training = Tensor.training
        self.addCleanup(lambda: setattr(Tensor, "training", self._prev_training))

    def test_bias_correction_advances_under_jit(self):
        Tensor.training = True
        b1, b2 = 0.9, 0.999

        # Tiny problem: minimize (w - target)^2; stable pre-allocated input.
        w = Tensor(np.array([0.0, 0.0, 0.0, 0.0], dtype=np.float32))
        w.realize()
        target = Tensor(np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32))
        target.realize()
        opt = nn.optim.Adam([w], lr=0.01, b1=b1, b2=b2)

        @TinyJit
        def step():
            opt.zero_grad()
            loss = ((w - target) * (w - target)).sum()
            loss.backward()
            opt.schedule_step()
            return loss

        N = 6
        observed = []
        for k in range(1, N + 1):
            step()
            bc1_inv, _ = _bc_inv_values(opt)
            observed.append(bc1_inv)

        # tinygrad reference: after k advances, effective inverse bias
        # correction is 1/(1-b1**k).  It must grow toward 1.0 as k grows.
        expected = [1.0 / (1.0 - b1 ** k) for k in range(1, N + 1)]

        # The frozen-t bug pins observed[k] == observed[0] (== 1/(1-b1) == 10)
        # for all k.  Assert it ADVANCES: observed must track expected.
        self.assertFalse(
            all(abs(o - observed[0]) < 1e-6 for o in observed),
            f"bias correction FROZEN under JIT replay: {observed} "
            f"(all stuck at step-1 value {observed[0]}); expected {expected}",
        )
        for k, (o, e) in enumerate(zip(observed, expected), start=1):
            self.assertAlmostEqual(
                o, e, places=4,
                msg=f"step {k}: bias-corr-inv {o} != tinygrad {e}",
            )

    def test_trajectory_matches_tinygrad_adam_eager(self):
        # Eager (no JIT) faithfulness: param trajectory matches the exact
        # tinygrad Adam update over many steps on a fixed seed/problem.
        Tensor.training = True
        b1, b2, lr, eps = 0.9, 0.999, 0.01, 1e-8
        rng = np.random.RandomState(0)
        w0 = rng.randn(8).astype(np.float32)
        tgt = rng.randn(8).astype(np.float32)

        w = Tensor(w0.copy())
        w.realize()
        target = Tensor(tgt.copy())
        target.realize()
        opt = nn.optim.Adam([w], lr=lr, b1=b1, b2=b2, eps=eps)

        # Reference Adam in numpy (tinygrad algebra: m_hat=m/(1-b1**t)).
        rw = w0.copy().astype(np.float64)
        rm = np.zeros_like(rw)
        rv = np.zeros_like(rw)

        for t in range(1, 16):
            opt.zero_grad()
            loss = ((w - target) * (w - target)).sum()
            loss.backward()
            opt.schedule_step()
            w.realize()

            g = 2.0 * (rw - tgt.astype(np.float64))
            rm = b1 * rm + (1 - b1) * g
            rv = b2 * rv + (1 - b2) * (g * g)
            mh = rm / (1 - b1 ** t)
            vh = rv / (1 - b2 ** t)
            rw = rw - lr * mh / (np.sqrt(vh) + eps)

            got = w.numpy().astype(np.float64)
            np.testing.assert_allclose(got, rw, rtol=1e-3, atol=1e-4,
                                       err_msg=f"step {t} eager mismatch")


if __name__ == "__main__":
    unittest.main()
