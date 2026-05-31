#!/usr/bin/env python3
"""Warm train-step benchmark for the simple 2-conv and beautiful_mnist models.

Consolidates the ad-hoc /tmp bench scripts used during ideal_pipeline_v2/v3 perf
work into one stable, parameterized harness so milestone verification stops
re-deriving it each session.

Reports warm-min / warm-mean wall time, loss, peak device bytes (Metal/CUDA),
and -- with THVM_KERNEL_PROFILE=1 -- the per-step kernel count.

Env knobs (read at import-time by the C runtime, so set them in the shell):
  DEV=cpu|cuda|metal          backend (default cpu)
  MODEL=simple|beautiful       model (default beautiful)
  BS=<int>                     batch size (default 8)
  THVM_RU_FAITHFUL_SEED=1      faithful realize-seed
  BEAM=<int> / AUTOTUNE=1      enable the autotune/BEAM search
  NITER=<int>                  iterations (default 12; warm = iters[2:])

Always uses opt.zero_grad() (thvm accumulates into the C-side grad slot; a bare
`p.grad = None` would pile gradients across steps -- see bb24342d).
"""
import os, sys, time, ctypes
import numpy as np

sys.path.insert(0, os.environ.get("THVM_PY", "py"))
from thvm import Tensor, nn
from thvm.thvm import Thvm

_TH = Thvm()
DEV   = os.environ.get("DEV", "cpu")
MODEL = os.environ.get("MODEL", "beautiful")
BS    = int(os.environ.get("BS", "8"))
NITER = int(os.environ.get("NITER", "12"))
np.random.seed(42)


def _peak_bytes():
    """Device peak-live bytes for the active backend (0 if unavailable).
    Metal exposes thvm_metal_peak_live_bytes via the dylib; CPU/CUDA go
    through py_cpu_peak_bytes (which sums the CPU + CUDA arenas)."""
    if DEV == "metal":
        try:
            so = ctypes.CDLL(os.path.join(os.environ.get("THVM_PY", "py"),
                                          "thvm", "libthvm_py.dylib"))
            so.thvm_metal_peak_live_bytes.restype = ctypes.c_uint64
            return int(so.thvm_metal_peak_live_bytes())
        except (OSError, AttributeError):
            return 0
    try:
        return int(_TH.cpu_peak_bytes())
    except AttributeError:
        return 0


def _model():
    if MODEL == "simple":
        return [nn.Conv2d(1, 32, 5), Tensor.relu,
                nn.Conv2d(32, 32, 5), Tensor.relu,
                lambda x: x.flatten(1), nn.Linear(32 * 20 * 20, 10)]
    return [nn.Conv2d(1, 32, 5), Tensor.relu, nn.Conv2d(32, 32, 5), Tensor.relu,
            nn.BatchNorm(32, track_running_stats=False), Tensor.max_pool2d,
            nn.Conv2d(32, 64, 3), Tensor.relu, nn.Conv2d(64, 64, 3), Tensor.relu,
            nn.BatchNorm(64, track_running_stats=False), Tensor.max_pool2d,
            lambda x: x.flatten(1), nn.Linear(576, 10)]


layers = _model()
opt = nn.optim.Adam(nn.state.get_parameters(layers), lr=0.001)
xSlot = Tensor(np.zeros((BS, 1, 28, 28), np.float32)); xSlot.realize()
ohSlot = Tensor(np.zeros((BS, 10), np.float32)); ohSlot.realize()
lossSlot = Tensor(np.zeros((1,), np.float32)); lossSlot.realize()
Tensor.training = True


def train_step():
    logits = xSlot.sequential(layers)
    ls = logits.log_softmax(axis=1)
    loss = (ls * ohSlot).sum(axis=1).mean() * -1.0
    loss.backward(); loss.realize()
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


jit = JitStep(train_step)


def feed():
    _TH.ten_write(xSlot.term,
                  np.ascontiguousarray(np.random.randn(BS, 1, 28, 28).astype(np.float32)).tobytes())
    oh = np.zeros((BS, 10), np.float32)
    oh[np.arange(BS), np.random.randint(0, 10, BS)] = 1.0
    _TH.ten_write(ohSlot.term, oh.tobytes())


wall = []
for i in range(NITER):
    feed()
    t0 = time.time(); jit(); lv = float(lossSlot.numpy()[0])
    wall.append((time.time() - t0) * 1e3)
warm = wall[2:] or wall
mode = "faithful" if os.environ.get("THVM_RU_FAITHFUL_SEED") else "default"
beam = os.environ.get("BEAM", "0")
print(f"thvm {DEV} {MODEL} {mode} BEAM={beam}: "
      f"warm_min={min(warm):.2f}ms warm_mean={np.mean(warm):.2f}ms "
      f"loss={lv:.4f} peak_mb={_peak_bytes() / 1e6:.1f}")
