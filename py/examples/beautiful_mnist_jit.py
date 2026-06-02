#!/usr/bin/env python3
"""beautiful_mnist on thvm with a real JIT -- fast, flat memory.

Follows tinygrad's TinyJit model (engine/jit.py): cnt 0 = warmup, cnt 1 =
capture the kernel sequence, cnt >= 2 = replay it.  thvm's C jit_capture /
jit_replay records the GPU kernels (+ in-place ASSIGNs); replay re-dispatches
the SAME kernels against the SAME buffers -- no graph rebuild, no nvrtc
recompile, no fresh allocations.  So per-step time is constant and device
memory is flat, without any per-step reclaim.

The captured region must be pure-GPU, so (like WL's train.wls) the changing
inputs land in STABLE slots written host-side before each replay: xSlot
(images), ohSlot (one-hot labels).  The optimizer updates params/m/v
IN-PLACE (UOP_ASSIGN) so their buffers stay stable across replays.  Loss is
read back through lossSlot.  BatchNorm runs in batch-stat mode (no running
stats / no extra assigns); eval uses batch stats too.

  THVM_MAX_BUF_BYTES=0 DEV=cuda STEPS=70 BS=128 \
      python3 py/examples/beautiful_mnist_jit.py
"""
from __future__ import annotations

import os
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from thvm import Tensor, nn                 # noqa: E402
from thvm.helpers import getenv            # noqa: E402
from thvm.nn.datasets import mnist         # noqa: E402
from thvm.thvm import Thvm                 # noqa: E402

_TH = Thvm()


class Model:
    def __init__(self):
        # track_running_stats=False -> batch stats only, no running-stat
        # assigns (keeps the captured graph pure + lets eval use batch stats).
        self.layers = [
            nn.Conv2d(1, 32, 5), Tensor.relu,
            nn.Conv2d(32, 32, 5), Tensor.relu,
            nn.BatchNorm(32, track_running_stats=False), Tensor.max_pool2d,
            nn.Conv2d(32, 64, 3), Tensor.relu,
            nn.Conv2d(64, 64, 3), Tensor.relu,
            nn.BatchNorm(64, track_running_stats=False), Tensor.max_pool2d,
            lambda x: x.flatten(1), nn.Linear(576, 10)]

    def __call__(self, x):
        return x.sequential(self.layers)


class JitStep:
    """tinygrad TinyJit control flow: warmup, capture, replay."""
    def __init__(self, fn):
        self.fn, self.cnt, self.slot = fn, 0, 0

    def __call__(self):
        if self.cnt == 0:            # warmup -- build + run, establish buffers
            self.fn(); self.cnt = 1
        elif self.cnt == 1:          # capture the kernel sequence
            self.slot = _TH.jit_begin(); self.fn(); _TH.jit_end()
            self.cnt = 2
        else:                        # replay -- no rebuild, no recompile
            _TH.jit_replay(self.slot)


def _gpu_mb():
    if os.environ.get("DEV", "").lower() not in ("cuda", "gpu"):
        return 0
    try:
        import subprocess
        o = subprocess.check_output(["nvidia-smi", "--query-gpu=memory.used",
                                     "--format=csv,noheader,nounits"], timeout=10)
        return int(o.decode().split("\n")[0])
    except Exception:
        return 0


def main():
    bs = getenv("BS", 128)
    steps = getenv("STEPS", 70)
    test_every = getenv("TEST_EVERY", 10)
    eval_batches = getenv("EVAL_BATCHES", 20)
    lr = float(getenv("LR", "0.001"))
    np.random.seed(42)

    Xtr, Ytr, Xte, Yte = (t.numpy() for t in mnist())
    Xtr = Xtr.astype(np.float32); Ytr = Ytr.astype(np.int64)
    Xte = Xte.astype(np.float32); Yte = Yte.astype(np.int64)
    print(f"DEV={os.environ.get('DEV','cpu')} BS={bs} STEPS={steps} "
          f"train={len(Xtr)} test={len(Xte)}", flush=True)

    model = Model()
    opt = nn.optim.Adam(nn.state.get_parameters(model.layers), lr=lr)

    # Stable slots (allocated + realized once; written in place each step).
    xSlot = Tensor(np.zeros((bs, 1, 28, 28), np.float32)); xSlot.realize()
    ohSlot = Tensor(np.zeros((bs, 10), np.float32)); ohSlot.realize()
    lossSlot = Tensor(np.zeros((1,), np.float32)); lossSlot.realize()

    Tensor.training = True

    def train_step():
        logits = model(xSlot)
        # sparse CE via the one-hot slot: -mean(sum(onehot * log_softmax))
        ls = logits.log_softmax(axis=1)
        loss = (ls * ohSlot).sum(axis=1).mean() * -1.0
        loss.backward()
        loss.realize()                                   # phase 1: fwd + grads
        Tensor.realize(*[p.grad for p in opt.params if p.grad is not None])
        lossSlot.assign(loss)
        Tensor.realize(lossSlot, *opt.schedule_step())   # phase 2: in-place
        # Clear the C-side gradient accumulator (TENS[tid].grad).  thvm's
        # backward() ACCUMULATES cotangents (tinygrad-faithful grads[k]+=v),
        # so a bare `p.grad = None` (which only drops the Python handle) lets
        # each step's gradient pile onto the previous one: step N's weight
        # grad becomes ADD(step1_grad, ..., stepN_grad), which the faithful
        # realize-seed fuses into one slow multi-accumulator reduce kernel.
        # opt.zero_grad() calls ten_clear_grad so each step reduces once.
        opt.zero_grad()

    jit = JitStep(train_step)

    def feed(idx):
        _TH.ten_write(xSlot.term, np.ascontiguousarray(Xtr[idx]).tobytes())
        oh = np.zeros((bs, 10), np.float32)
        oh[np.arange(bs), Ytr[idx]] = 1.0
        _TH.ten_write(ohSlot.term, oh.tobytes())

    def test_acc():
        correct = total = 0
        for b in range(min(eval_batches, (len(Xte) + bs - 1) // bs)):
            xb = Xte[b * bs:(b + 1) * bs]; yb = Yte[b * bs:(b + 1) * bs]
            if len(xb) == 0:
                break
            if len(xb) < bs:
                xb = np.concatenate([xb, np.zeros((bs - len(xb), 1, 28, 28), np.float32)])
            _TH.ten_write(xSlot.term, np.ascontiguousarray(xb).tobytes())
            pred = np.argmax(model(xSlot).numpy(), axis=1)[:len(yb)]
            correct += int((pred == yb).sum()); total += len(yb)
        return 100.0 * correct / max(total, 1)

    wall, peak, acc = [], 0, float("nan")
    for i in range(steps):
        t0 = time.time()
        idx = np.random.randint(0, len(Xtr), size=bs)
        feed(idx)
        jit()
        lv = float(lossSlot.numpy()[0])
        dt = (time.time() - t0) * 1e3
        wall.append(dt); peak = max(peak, _gpu_mb())
        if test_every and (i + 1) % test_every == 0:
            acc = test_acc()
        print(f"step {i+1:3d}: loss={lv:6.3f} wall={dt:8.1f}ms mem={_gpu_mb()}MB"
              + (f" acc={acc:5.2f}%" if test_every and (i+1) % test_every == 0 else ""),
              flush=True)

    warm = wall[2:] or wall[-1:]
    print(f"\nSUMMARY acc={acc:.2f}% capture={wall[1] if len(wall)>1 else wall[0]:.0f}ms "
          f"warm_mean={np.mean(warm):.1f}ms warm_min={np.min(warm):.1f}ms peak_mem={peak}MB",
          flush=True)


if __name__ == "__main__":
    main()
