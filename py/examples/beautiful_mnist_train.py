#!/usr/bin/env python3
"""beautiful_mnist full train on thvm with FLAT memory (cross-step reclaim).

Companion to beautiful_mnist_thvm.py (the literal import-swap).  That one
mirrors tinygrad's script verbatim; this one is the same model + Adam but
with a BATCHED test-accuracy eval (thvm's conv still materializes its
im2col, so a single full-test forward at batch 10000 is infeasible -- the
literal script's unbatched eval OOMs on that, a separate under-fusion
issue from the memory leak).

Memory stays flat because every live Tensor pins its term and the loop
calls thvm's cross-step buffer reclaim each step (GlobalCounters.reset()
triggers it).  Run with THVM_GC=0 so raw term ids never move:

  THVM_GC=0 THVM_MAX_BUF_BYTES=0 DEV=cuda STEPS=70 BS=128 \
      python3 py/examples/beautiful_mnist_train.py
"""
from __future__ import annotations

import os
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from thvm import Tensor, nn, GlobalCounters     # noqa: E402
from thvm.helpers import getenv                 # noqa: E402
from thvm.nn.datasets import mnist              # noqa: E402
from thvm.thvm import Thvm                      # noqa: E402

_TH = Thvm()


class Model:
    def __init__(self):
        self.layers = [
            nn.Conv2d(1, 32, 5), Tensor.relu,
            nn.Conv2d(32, 32, 5), Tensor.relu,
            nn.BatchNorm(32), Tensor.max_pool2d,
            nn.Conv2d(32, 64, 3), Tensor.relu,
            nn.Conv2d(64, 64, 3), Tensor.relu,
            nn.BatchNorm(64), Tensor.max_pool2d,
            lambda x: x.flatten(1), nn.Linear(576, 10)]

    def __call__(self, x):
        return x.sequential(self.layers)


def _gpu_mb():
    if os.environ.get("DEV", "").lower() not in ("cuda", "gpu"):
        return 0
    try:
        import subprocess
        o = subprocess.check_output(
            ["nvidia-smi", "--query-gpu=memory.used",
             "--format=csv,noheader,nounits"], timeout=10)
        return int(o.decode().split("\n")[0])
    except Exception:
        return 0


def main():
    bs = getenv("BS", 128)
    steps = getenv("STEPS", 70)
    test_every = getenv("TEST_EVERY", 10)
    eval_batches = getenv("EVAL_BATCHES", 16)   # batched eval (cap)
    np.random.seed(42)

    Xtr, Ytr, Xte, Yte = (t.numpy() for t in mnist())
    Xtr = Xtr.astype(np.float32); Ytr = Ytr.astype(np.int64)
    Xte = Xte.astype(np.float32); Yte = Yte.astype(np.int64)
    print(f"DEV={os.environ.get('DEV','cpu')} BS={bs} STEPS={steps} "
          f"train={len(Xtr)} test={len(Xte)}")

    model = Model()
    _lr = float(os.environ.get("LR", "1e-3"))
    _params = nn.state.get_parameters(model.layers)
    opt = (nn.optim.SGD(_params, lr=_lr) if os.environ.get("OPT") == "sgd"
           else nn.optim.Adam(_params, lr=_lr))
    print(f"OPT={os.environ.get('OPT','adam')} LR={_lr}")

    def test_acc():
        Tensor.training = False
        correct = total = 0
        for b in range(min(eval_batches, (len(Xte) + bs - 1) // bs)):
            xb = Xte[b * bs:(b + 1) * bs]
            yb = Yte[b * bs:(b + 1) * bs]
            if len(xb) == 0:
                break
            pred = np.argmax(model(Tensor(xb)).numpy(), axis=1)
            correct += int((pred == yb).sum()); total += len(yb)
        Tensor.training = True
        return 100.0 * correct / max(total, 1)

    Tensor.training = True
    wall, peak = [], 0
    acc = float("nan")
    for i in range(steps):
        GlobalCounters.reset()             # triggers cross-step reclaim
        _TH.cpu_peak_reset()               # within-step peak from here
        t0 = time.time()
        idx = np.random.randint(0, len(Xtr), size=bs)
        loss = (model(Tensor(Xtr[idx]))
                .sparse_categorical_crossentropy(Tensor(Ytr[idx]))
                .backward())
        # Realize loss + ALL grads in ONE pass so the memory planner sees
        # the full forward+backward lifetime and frees each activation
        # after backward consumes it (peak ~ live set, not sum-of-all).
        # THEN the in-place optimizer ASSIGNs (writes) -- separate pass so
        # they don't race the param reads above.
        grads = [p.grad for p in opt.params if p.grad is not None]
        Tensor.realize(loss, *grads)
        Tensor.realize(*opt.schedule_step())
        lv = loss.item()
        dt = (time.time() - t0) * 1e3
        wall.append(dt)
        peak = max(peak, _gpu_mb())
        if test_every and (i + 1) % test_every == 0:
            acc = test_acc()
        print(f"step {i+1:3d}: loss={lv:6.3f} wall={dt:8.1f}ms "
              f"mem={_gpu_mb()}MB peak={_TH.cpu_peak_bytes()/1048576:.1f}MB "
              f"live={_TH.cpu_live_bytes()/1048576:.1f}MB"
              + (f" test_acc={acc:5.2f}%" if test_every and (i+1) % test_every == 0 else ""),
              flush=True)

    if not test_every:
        acc = test_acc()
    warm = wall[1:] or wall
    print(f"\nSUMMARY final_acc={acc:.2f}% cold={wall[0]:.0f}ms "
          f"warm_mean={np.mean(warm):.0f}ms peak_mem={peak}MB")


if __name__ == "__main__":
    main()
