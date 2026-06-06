#!/usr/bin/env python3
"""Real-size MLP train-step benchmark for thvm (CPU by default).

Companion to bench_tinygrad.py / bench_torch.py in this directory; all three
share the SAME architecture so per-step wall numbers compare kernel quality
at a size where compute dominates dispatch:

    in -> Linear(IN, H) -> relu -> Linear(H, H) -> relu -> Linear(H, OUT)
    MSE loss against a fixed target, gradient w.r.t. every param, Adam step.

Defaults: IN=256, H=1024, OUT=256, BS=256.  At these sizes the dominant cost
is three (BS x IN) @ (IN x H)-class matmuls per layer forward + their
transposes in backward, so the step is genuinely matmul-bound rather than
dispatch-overhead-bound (which was Finding #6 about the 4-64-2 toy net).

The step is captured ONCE with thvm's JIT (jit_begin/jit_end) after a warm
build, then replayed for the timed loop -- mirroring bench_train.py's JitStep.

Env knobs (read at import-time by the C runtime; set in the shell):
  DEV=cpu|cuda|metal   backend (default cpu; KEEP cpu for this finding)
  IN, H, OUT, BS       dimensions (defaults 256/1024/256/256)
  WARMUP               untimed warm steps (default 5)
  STEPS                timed steps (default 200)
"""
import os, sys, time
import numpy as np

sys.path.insert(0, os.environ.get("THVM_PY", "py"))
from thvm import Tensor, nn
from thvm.thvm import Thvm

_TH = Thvm()
DEV    = os.environ.get("DEV", "cpu")
IN     = int(os.environ.get("IN", "256"))
H      = int(os.environ.get("H", "1024"))
OUT    = int(os.environ.get("OUT", "256"))
BS     = int(os.environ.get("BS", "256"))
WARMUP = int(os.environ.get("WARMUP", "5"))
STEPS  = int(os.environ.get("STEPS", "200"))
np.random.seed(42)

layers = [nn.Linear(IN, H), Tensor.relu, nn.Linear(H, H), Tensor.relu,
          nn.Linear(H, OUT)]
opt = nn.optim.Adam(nn.state.get_parameters(layers), lr=0.001)

xSlot = Tensor(np.random.randn(BS, IN).astype(np.float32)); xSlot.realize()
tgtSlot = Tensor(np.random.randn(BS, OUT).astype(np.float32)); tgtSlot.realize()
lossSlot = Tensor(np.zeros((1,), np.float32)); lossSlot.realize()
Tensor.training = True

n_params = sum(int(np.prod(p.shape)) for p in opt.params)


def train_step():
    out = xSlot.sequential(layers)
    loss = ((out - tgtSlot) * (out - tgtSlot)).mean()
    loss.backward()
    loss.realize()
    Tensor.realize(*[p.grad for p in opt.params if p.grad is not None])
    lossSlot.assign(loss)
    Tensor.realize(lossSlot, *opt.schedule_step())
    opt.zero_grad()


class JitStep:
    def __init__(s, fn): s.fn, s.cnt, s.slot = fn, 0, 0
    def __call__(s):
        if s.cnt == 0: s.fn(); s.cnt = 1
        elif s.cnt == 1: s.slot = _TH.jit_begin(); s.fn(); _TH.jit_end(); s.cnt = 2
        else: _TH.jit_replay(s.slot)


def schedule_kernels():
    """Honest per-step kernel count via the KERNELS-registry delta of one
    plain eager fwd+bwd+optimizer realize (no JIT union double-count)."""
    k0 = _TH.kernel_count()
    out = xSlot.sequential(layers)
    loss = ((out - tgtSlot) * (out - tgtSlot)).mean()
    loss.backward(); loss.realize()
    Tensor.realize(*[p.grad for p in opt.params if p.grad is not None],
                   *opt.schedule_step())
    opt.zero_grad()
    return _TH.kernel_count() - k0


sched_k = schedule_kernels()
opt.zero_grad()
jit = JitStep(train_step)

print(f"thvm device={DEV} IN={IN} H={H} OUT={OUT} BS={BS} "
      f"params={n_params} WARMUP={WARMUP} STEPS={STEPS}")

for _ in range(WARMUP):
    jit()
    float(lossSlot.numpy()[0])  # sync

t0 = time.perf_counter()
for _ in range(STEPS):
    jit()
lv = float(lossSlot.numpy()[0])  # sync (forces the queue to drain)
t1 = time.perf_counter()

ms = (t1 - t0) * 1000.0 / STEPS
print(f"thvm steady_ms_per_step={ms:.3f} (BS={BS} N={STEPS}) "
      f"loss={lv:.4f} sched_kernels={sched_k}")
