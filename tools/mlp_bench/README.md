# Real-size MLP CPU benchmark

A small, reusable harness that measures the warm (JIT-replayed) per-step wall
time of a real-size multilayer perceptron (MLP) train step on CPU, across
three frameworks sharing the **same architecture**:

```
in -> Linear(IN, H) -> relu -> Linear(H, H) -> relu -> Linear(H, OUT)
mean-squared-error (MSE) loss against a fixed target,
gradient w.r.t. every parameter, Adam optimizer step
```

Defaults: `IN=256`, `H=1024`, `OUT=256`, `BS=256` (batch size), `STEPS=200`
timed steps after `WARMUP=5` untimed warm steps.

## Why this exists

A 4-64-2 toy net is dispatch-overhead-bound: every framework spends its time in
launch overhead, so wall time reflects the dispatcher, not the kernels. At
`256->1024->1024->256` with batch 256 the step is dominated by three
`(BS x IN) @ (IN x H)`-class matmuls per forward plus their transposes in
backward, so the per-step number actually reflects **kernel codegen quality**.

CPU only. Do NOT run a real-size net on Metal (it can orphan the GPU and force a
reboot, per a hard project constraint).

## Scripts

| Script | Framework | Backend selection |
|---|---|---|
| `bench_thvm.py` | thvm (this repo) | `DEV=cpu` (default) |
| `bench_tinygrad.py` | tinygrad | `DEV=CPU` (tinygrad defaults to METAL on macOS) |
| `bench_torch.py` | PyTorch | always CPU; `COMPILE=1` for `torch.compile` |

All three read `IN H OUT BS WARMUP STEPS` from the environment.

```bash
# from the repo root (build the thvm dylib first: make py)
DEV=cpu  STEPS=200 python3 tools/mlp_bench/bench_thvm.py
DEV=CPU  STEPS=200 python3 tools/mlp_bench/bench_tinygrad.py
         STEPS=200 python3 tools/mlp_bench/bench_torch.py
```

Each script captures the step once (thvm: `jit_begin`/`jit_end`; tinygrad:
`TinyJit`; torch: eager, optionally `torch.compile`) then replays it for the
timed loop, so first-build cost is excluded. Each prints
`steady_ms_per_step=<ms>` and the converged `loss` (the loss falling toward 0
confirms the replayed step really trains, not a no-op).

`bench_thvm.py` also prints `sched_kernels=<n>`: the honest per-step kernel
count measured as the KERNELS-registry delta of one plain eager
forward+backward+optimizer realize (no JIT capture/replay union double-count).

## Measured numbers

Host: Apple M3 Max, macOS 26.4, 16 cores. thvm built with `make py`. Same arch,
JIT-warm, median of 3 trials, per-step wall in milliseconds (lower is better).

| config | thvm CPU | tinygrad CPU | torch CPU |
|---|---|---|---|
| 256-1024-1024-256, BS=256 | 6.7 | 27.2 | 4.3 |
| 256-1024-1024-256, BS=512 | 8.6 | 47.9 | 4.7 |
| H=2048, BS=256 | 21.1 | 91.1 | 8.8 |

Per-step kernel counts at the BS=256 default: thvm `sched_kernels=53`; tinygrad
batches ~41 kernels per replayed step (32 + 9 under `JIT GRAPHing`, via
`DEBUG=2`). The counts are comparable, so the wall-time gap is **kernel
codegen**, not dispatch.

## Read

thvm CPU is consistently ~4x faster than tinygrad CPU and ~1.5-2.4x slower than
PyTorch CPU at every size tested. Since thvm and tinygrad dispatch a comparable
number of kernels, thvm's large win over tinygrad is per-kernel CPU codegen
(Accelerate-backed matmuls plus the faithful realize-seed path that kills
forward recompute and gather), not fewer launches. The remaining gap to PyTorch
is BLAS-level matmul micro-kernel tuning (PyTorch dispatches a handful of fused
oneDNN/MKL ops). The 4-64-2 toy net does NOT reveal any of this: at the real
size, thvm's kernels are competitive.
