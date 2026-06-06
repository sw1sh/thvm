#!/usr/bin/env python3
"""Real-size MLP train-step benchmark for PyTorch (CPU by default).

Architecturally IDENTICAL to bench_thvm.py / bench_tinygrad.py:

    in -> Linear(IN, H) -> relu -> Linear(H, H) -> relu -> Linear(H, OUT)
    MSE loss against a fixed target, gradient w.r.t. every param, Adam step.

Runs on CPU by default. COMPILE=1 enables torch.compile.
    STEPS=200 python3 tools/mlp_bench/bench_torch.py
"""
import os, time, torch
from torch import nn, optim

IN     = int(os.environ.get("IN", "256"))
H      = int(os.environ.get("H", "1024"))
OUT    = int(os.environ.get("OUT", "256"))
BS     = int(os.environ.get("BS", "256"))
WARMUP = int(os.environ.get("WARMUP", "5"))
STEPS  = int(os.environ.get("STEPS", "200"))
COMPILE = os.environ.get("COMPILE", "0") == "1"
THREADS = int(os.environ.get("THREADS", "0"))
if THREADS > 0:
    torch.set_num_threads(THREADS)

torch.manual_seed(42)
device = torch.device("cpu")
model = nn.Sequential(nn.Linear(IN, H), nn.ReLU(), nn.Linear(H, H), nn.ReLU(),
                      nn.Linear(H, OUT)).to(device)
optimizer = optim.Adam(model.parameters(), lr=0.001)
loss_fn = nn.MSELoss()

X = torch.randn(BS, IN, device=device)
TGT = torch.randn(BS, OUT, device=device)
n_params = sum(p.numel() for p in model.parameters())


def step():
    out = model(X)
    loss = loss_fn(out, TGT)
    optimizer.zero_grad()
    loss.backward()
    optimizer.step()
    return loss


if COMPILE:
    step = torch.compile(step)

print(f"torch device={device} threads={torch.get_num_threads()} "
      f"IN={IN} H={H} OUT={OUT} BS={BS} params={n_params} "
      f"WARMUP={WARMUP} STEPS={STEPS} compile={COMPILE}")

for _ in range(WARMUP):
    step().item()  # sync

t0 = time.perf_counter()
for _ in range(STEPS):
    loss = step()
lv = loss.item()  # sync
t1 = time.perf_counter()

ms = (t1 - t0) * 1000.0 / STEPS
print(f"torch steady_ms_per_step={ms:.3f} (BS={BS} N={STEPS}) loss={lv:.4f}")
