"""Faithful-seed vs default-seed numerical parity (a regression guardrail).

The faithful realize-seed (`THVM_RU_FAITHFUL_SEED=1`) schedules the same UOP
graph into a different, more-fused set of kernels than the default heuristic
seed. It must produce the SAME forward + gradient numbers (within fp
reduction-order). This test locks that in so the in-progress memory-fusion work
(boolean-OR validity merge, multi-output optimizer kernels) can't silently
regress correctness on the faithful path.

The seed is read once per process (env-memoized in C), so each mode runs in its
own subprocess.

    make py && python -m pytest py/tests/test_faithful_parity.py
"""
import os
import pathlib
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
PY = str(ROOT / "py")

# Single forward+backward of the 2-conv model on a fixed input; prints the loss
# and conv1 weight-grad sum-of-squares. Step 0 only (before any optimizer
# update), so default and faithful must agree up to fp reduction-order.
_CHILD = r"""
import os, sys
import numpy as np
sys.path.insert(0, os.environ["THVM_PY"])
from thvm import Tensor, nn
np.random.seed(0)
Tensor.training = True
bs = 8
c1 = nn.Conv2d(1, 32, 5); c2 = nn.Conv2d(32, 32, 5); lin = nn.Linear(32 * 20 * 20, 10)
# Adam registers requires_grad on the params (so backward populates .grad).
nn.optim.Adam(nn.state.get_parameters([c1, c2, lin]), lr=0.001)
x = Tensor(np.random.randn(bs, 1, 28, 28).astype(np.float32)); x.realize()
oh = Tensor(np.eye(10)[np.random.randint(0, 10, bs)].astype(np.float32)); oh.realize()
logits = lin(c2(c1(x).relu()).relu().flatten(1))
loss = (logits.log_softmax(axis=1) * oh).sum(axis=1).mean() * -1.0
loss.backward(); loss.realize()
gssq = float((c1.weight.grad * c1.weight.grad).sum().numpy())
print(f"LOSS={float(loss.numpy()):.7g} GSSQ={gssq:.7g}")
"""


def _run(faithful: bool):
    env = dict(os.environ, THVM_PY=PY, DEV="cpu")
    if faithful:
        env["THVM_RU_FAITHFUL_SEED"] = "1"
    else:
        env.pop("THVM_RU_FAITHFUL_SEED", None)
    out = subprocess.run([sys.executable, "-c", _CHILD], env=env,
                         capture_output=True, text=True, timeout=300)
    line = next((l for l in out.stdout.splitlines() if l.startswith("LOSS=")), None)
    assert line is not None, f"child produced no result:\n{out.stdout}\n{out.stderr}"
    loss = float(line.split("LOSS=")[1].split()[0])
    gssq = float(line.split("GSSQ=")[1])
    return loss, gssq


class TestFaithfulParity(unittest.TestCase):
    def test_forward_and_grad_parity(self):
        d_loss, d_gssq = _run(faithful=False)
        f_loss, f_gssq = _run(faithful=True)
        # fp reduction-order tolerance: faithful fuses reduces differently.
        self.assertAlmostEqual(f_loss, d_loss, delta=1e-4,
                               msg=f"loss: faithful {f_loss} vs default {d_loss}")
        self.assertLessEqual(abs(f_gssq - d_gssq) / max(abs(d_gssq), 1.0), 1e-4,
                             f"conv1 weight-grad ssq: faithful {f_gssq} vs default {d_gssq}")


if __name__ == "__main__":
    unittest.main()
