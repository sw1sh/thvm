#!/usr/bin/env python3
"""Real-size MLP train-step benchmark for tinygrad (CPU by default).

Architecturally IDENTICAL to bench_thvm.py so the per-step wall numbers
compare kernel quality (not dispatch overhead) at a compute-bound size:

    in -> Linear(IN, H) -> relu -> Linear(H, H) -> relu -> Linear(H, OUT)
    MSE loss against a fixed target, gradient w.r.t. every param, Adam step.

Force the CPU backend with CPU=1 (tinygrad defaults to METAL on macOS):
    CPU=1 STEPS=200 python3 tools/mlp_bench/bench_tinygrad.py
"""
import os, sys, time
sys.path.insert(0, os.environ.get("TINYGRAD", "/Users/swish/src/tinygrad"))

from tinygrad import Tensor, TinyJit, nn, Device, GlobalCounters
from tinygrad.helpers import getenv

IN     = int(os.environ.get("IN", "256"))
H      = int(os.environ.get("H", "1024"))
OUT    = int(os.environ.get("OUT", "256"))
BS     = int(os.environ.get("BS", "256"))
WARMUP = int(os.environ.get("WARMUP", "5"))
STEPS  = int(os.environ.get("STEPS", "200"))

Tensor.manual_seed(42)
layers = [nn.Linear(IN, H), Tensor.relu, nn.Linear(H, H), Tensor.relu,
          nn.Linear(H, OUT)]
opt = nn.optim.Adam(nn.state.get_parameters(layers), lr=0.001)

X = Tensor.randn(BS, IN).realize()
TGT = Tensor.randn(BS, OUT).realize()
n_params = sum(int(p.numel()) for p in nn.state.get_parameters(layers))


@TinyJit
@Tensor.train()
def train_step():
    opt.zero_grad()
    out = X.sequential(layers)
    loss = ((out - TGT) ** 2).mean().backward()
    opt.step()
    return loss


print(f"tinygrad device={Device.DEFAULT} IN={IN} H={H} OUT={OUT} BS={BS} "
      f"params={n_params} WARMUP={WARMUP} STEPS={STEPS}")

kernels_before = GlobalCounters.kernel_count
for _ in range(WARMUP):
    train_step().item()  # sync
kernels_per_step = GlobalCounters.kernel_count - kernels_before  # over WARMUP steps

Device[Device.DEFAULT].synchronize()
t0 = time.perf_counter()
for _ in range(STEPS):
    loss = train_step()
Device[Device.DEFAULT].synchronize()
lv = loss.item()
t1 = time.perf_counter()

ms = (t1 - t0) * 1000.0 / STEPS
# kernels_per_step counts the WARMUP builds; the replayed count is what matters.
print(f"tinygrad steady_ms_per_step={ms:.3f} (BS={BS} N={STEPS}) loss={lv:.4f}")
