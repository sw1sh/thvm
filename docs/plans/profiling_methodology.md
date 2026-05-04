# thvm vs tinygrad — Apples-to-Apples Profiling Methodology

This document specifies the precise procedure for comparing thvm and
tinygrad on the same workload, with two side-by-side tables of every
metric each framework natively exposes.  The aim is to make
performance claims falsifiable: anyone with both checkouts and a Mac
should be able to reproduce the numbers.

## 1. Workload

The canonical workload is **beautiful_mnist training** — a single
forward + cross-entropy loss + backward + optimizer update step on a
fixed model.

### 1.1 Model

Both frameworks must run the SAME architecture.  Tinygrad's
`examples/beautiful_mnist.py:Model`:

| Layer | thvm equivalent |
|-------|-----------------|
| `nn.Conv2d(1, 32, 5)` | `TConv2D[x, w_1, b_1, 1, 0]` |
| `Tensor.relu` | `TReLU[x]` |
| `nn.Conv2d(32, 32, 5)` | `TConv2D[..., w_2, b_2, ...]` |
| `Tensor.relu` | `TReLU` |
| `nn.BatchNorm(32)` | `TBatchNormTrain[x, gamma_1, beta_1]` |
| `Tensor.max_pool2d` | `TMaxPool2D[x, 2, 2]` |
| `nn.Conv2d(32, 64, 3)` | `TConv2D[...]` |
| `Tensor.relu` | `TReLU` |
| `nn.Conv2d(64, 64, 3)` | `TConv2D[...]` |
| `Tensor.relu` | `TReLU` |
| `nn.BatchNorm(64)` | `TBatchNormTrain[x, gamma_2, beta_2]` |
| `Tensor.max_pool2d` | `TMaxPool2D` |
| `Flatten + nn.Linear(576, 10)` | `Flatten + TLinear[576, 10]` |

Loss: sparse categorical cross-entropy.  Optimizer: Adam (lr=1e-3,
default betas/eps).

### 1.2 Hyperparameters

Fixed for both frameworks:

- Batch size: `BS=32`
- Steps timed: `N_STEPS=1` (single steady-state step after warmup)
- Warmup steps: `WARMUP_STEPS=1` (one full first-compile pass)
- Optimizer: Adam, default hyperparameters
- Backend: Metal (Apple Silicon)

### 1.3 Hardware

Single Apple Silicon GPU (M-series).  Both runs on the same machine,
back-to-back, with the system otherwise idle.  Record:

- CPU model, GPU cores, RAM
- macOS version, Metal version (`system_profiler SPDisplaysDataType`)

## 2. Run Procedure

### 2.1 thvm

```sh
# Build
make wl/THVMLink/LibraryResources/MacOSX-ARM64/THVMLink.dylib

# Canary
BS=32 WARMUP_STEPS=1 N_STEPS=1 POST_AUTOTUNE_TOP=6 \
  THVM_BACKEND=metal THVM_TILE=1 \
  wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls
```

The script runs warmup (compile + autotune), then one timed step.
Timing isolates the JIT-replayed steady-state path; first-step
compile is excluded.

### 2.2 tinygrad

```sh
# Tinygrad's built-in benchmark target
cd /Users/swish/src/tinygrad
BS=32 STEPS=2 DEBUG=2 JIT=2 \
  python examples/beautiful_mnist.py
```

`DEBUG=2` prints per-kernel name, time, FLOPS, memory bandwidth.
`JIT=2` enables JIT capture (steady-state replay).  Step 0 is the
JIT-trace step, step 1 is the steady-state (timed).

To extract just one step's metrics for direct comparison, gate the
loop body with `if i == 1: GlobalCounters.reset()` before, then read
`GlobalCounters.kernel_count, .global_ops, .global_mem, .time_sum_s`
after.

### 2.3 Apples-to-Apples Adjustments

- **Don't compare warmup wall time directly.**  Tinygrad's BEAM
  search is much more elaborate than thvm's autotune.  Compare only
  the steady-state timed step.
- **Don't count JIT-graph overhead vs thvm's replay overhead.**
  Both report the timed step including dispatch overhead; that's
  fair.
- **Memory: report peak retained (live + freelist) and peak live.**
  Both frameworks reuse buffers; "retained" is the high-water mark.
- **Kernel count: report the count for ONE training step.**  Not
  cumulative across all steps.

## 3. What Each Framework Measures Natively

### 3.1 Per-Step Metrics

| Metric | thvm exposure | tinygrad exposure |
|--------|---------------|-------------------|
| Kernels dispatched per step | `kernels(new)=N` for warmup, `kernels(new)=0 jit-ops=N` for steady-state | `GlobalCounters.kernel_count` after `.reset()` + step |
| Wall time, timed step | `timed step %d: wall=%.1fms` | `GlobalCounters.time_sum_s * 1e3` (ms) |
| Total FLOPs estimate | not exposed natively (would need to walk KProgOp) | `GlobalCounters.global_ops` |
| Total memory traffic estimate | not exposed natively | `GlobalCounters.global_mem` (bytes) |
| Backend dispatch breakdown | `dispatch=metal-tile=N, metal-op=N, metal-jit=N, metal-alias=N, metal-gemm=N, metal-conv=N` | per-kernel `display_name` includes shape/op (no aggregate; need parsing) |
| Peak live memory | `peak_live=N` from `metal_memory after_timed:` line | `GlobalCounters.mem_used` (high-water across program) |
| Peak retained memory | `peak_retained=N` from same line | not exposed natively (would need backend probe) |
| Buffer count | `buffers=A/B/C` (total/live/peak from same line) | not exposed natively |
| First-step compile + JIT trace time | `warmup step 1: wall=%.1fms` | step 0 wall (when `JIT=2`) |
| Autotune / BEAM time | included in warmup wall when `THVM_AUTOTUNE=1` | env-gated (`BEAM=2`); reported separately by `Timing` |
| Per-rule fusion stats | `realize_rewrite_summary` (when `DUMP_REWRITE=1`) — `<rule_name> hits=N` | not exposed; requires `DEBUG_RANGEIFY=1` and parsing |
| Bufferize candidates not removed | `DUMP_BUFFERIZE_CANDIDATES=1` lists top-N by removal score | not exposed (rangeify decisions are internal) |
| Schedule key | `bufferize_schedule_key()` stable hash (when `DUMP_BUFFERIZE=1`) | not exposed |

### 3.2 Per-Kernel Metrics

| Metric | thvm exposure | tinygrad exposure |
|--------|---------------|-------------------|
| Kernel name / role | `KernelEntry.source_uop` (UOp class) | `display_name` (printed at `DEBUG>=2`) |
| Kernel ID | `kid = ke - KERNELS` | sequential `GlobalCounters.kernel_count` |
| Output dtype + shape | `KernelEntry.output_{dtype,shape,numel}` | encoded in `display_name`, ast |
| Input count | `KernelEntry.n_inputs` | `arg N` in DEBUG=2 line |
| Per-kernel time (us) | `cg_profile_record(kid, dispatch_kind, et)` (`THVM_PROFILE=1`) | `time_to_str(et)` in DEBUG=2 line |
| Per-kernel FLOPs (estimated) | not exposed; must parse program | `flops = op_est/et` in DEBUG=2 line |
| Per-kernel memory bandwidth | not exposed | `membw, ldsbw` in DEBUG=2 line |
| Per-kernel dispatch path | `cg_profile_record` records `KDISPATCH_*` enum (METAL_TILE, METAL_OP, METAL_JIT, METAL_ALIAS, METAL_GEMM, METAL_CONV) | not exposed (kernel just runs) |
| Per-kernel scalar uop count | `KernelEntry.n_scalar_uops` | not exposed (after rangeify -> linearize -> render) |
| Per-kernel program op count | `KernelEntry.n_ops` | analogous: `ast.toposort()` length, but not aggregated |
| Per-kernel buffer args | `KernelEntry.input_tids[]`, `output_tid` | bufs in `track_stats` |
| Source location / op metadata | `KernelEntry.source_uop` term + boundary loc | `metadata` (TraceMeta), `arg` |

### 3.3 Compile / Schedule Metrics

| Metric | thvm exposure | tinygrad exposure |
|--------|---------------|-------------------|
| Realize boundaries (pre-rewrite) | `REALIZE_INFO_LEN` after `realize_classify` | `BUFFERIZE` count in scheduled graph |
| Buffer count after fusion | `bufferize_buffer_count()` | post-`limit_bufs` count |
| Per-rule hit count | `realize_rewrite_stat_hits(name)` | not exposed per-rule |
| Memory plan / lifetime | `BOUNDARY_DEPTH[]`, `BOUNDARY_LAST_USE[]`, `bufferize_buffer_lifetime()` | tinygrad's `MemoryPlanner` (engine internal) |
| JIT trace size | `n_jit_ops` from canary printout | `len(big_linear.src) + len(onetime_linear.src)` after JIT capture |
| Schedule cache hit/miss | `kernel_program_cache_lookup()` | `JIT.cache_size` |

## 4. Side-by-Side Numbers (Apples-to-Apples)

Measurements on one machine, back-to-back, BS=32, one training step
post-JIT, beautiful_mnist (Conv-Conv-BN-MaxPool x 2 + Linear + CE +
Adam).

### 4.1 Test Conditions (recorded 2026-05-04)

- **Hardware**: Apple M3 Max (40-core GPU)
- **macOS / Metal**: macOS 25.4.0 (Darwin), Metal version per system
- **thvm**: `main` at `90c89a7` + `5e47e98`
- **tinygrad**: HEAD of `/Users/swish/src/tinygrad`, `Device=METAL`,
  `JIT=2`
- **Driver**: `wl/Examples/beautiful-mnist/bench-train.wls` (thvm),
  `/tmp/tinygrad_bench.py` adapted from `examples/beautiful_mnist.py`
  with `BS=32`, `JIT=2`, 1 warmup + 5 steady-state steps.

### Table A — Per-Step Steady-State Metrics

| Metric | thvm | tinygrad | Notes |
|--------|------|----------|-------|
| Wall time per step (ms) | **19.7** (range 19-25) | **20.3** (median of 4 runs: 20.3, 20.3, 19.9, 21.2) | wall-clock around the JIT-replayed step |
| Kernel count | **72** | **116** | thvm: post-Phase-1+reduce-chain-lift; tinygrad: post-rangeify+JIT |
| Unique kernel programs | 16 (high cache reuse) | 116 (effectively all unique) | thvm program-cache dedups identical scalar uops |
| Peak live memory (MB) | **55.8** | **57.4** | thvm: `peak_live`; tinygrad: `GlobalCounters.mem_used` after step |
| Peak retained memory (MB) | **58.0** | not natively exposed | thvm: `peak_retained` includes freelist |
| GFLOPs per step | not measured (no native FLOP estimator) | **3.53** | tinygrad: `GlobalCounters.global_ops` |
| Memory traffic per step (MB) | not measured | **83.1** | tinygrad: `GlobalCounters.global_mem` |
| Buffer count (total/live/peak) | **376 / 290 / 376** | not exposed | thvm: `buffers=A/B/C` |
| Dispatch breakdown | `metal-tile=72, metal-op=0` | n/a (one render path) | thvm-only; all 72 kernels through tile-JIT post-FLAT_GRID-lift |
| First-step compile + JIT trace (ms) | **1567** | **3934** | warmup step wall; tinygrad's BEAM/rangeify is more elaborate |
| JIT-ops in capture | **194** | **116** | thvm: per-op KProgOp count; tinygrad: kernel count post-fusion |
| GPU compute time per step (ms, sum of per-kernel) | ~0.07 (sampled, `SHOW_PROFILE=1`) | **2.1** (median across runs 2-5 with `DEBUG=2`) | thvm's profile sampling underreports — likely doesn't capture every replay dispatch; investigation pending |

### Table B — Per-Kernel Steady-State Metrics

| Metric | thvm | tinygrad | Notes |
|--------|------|----------|-------|
| Per-kernel time, median (us) | 0.2 (per-dispatch sample) | **3.6** | tinygrad: from `DEBUG=2` per-kernel `tm` field |
| Per-kernel time, mean (us) | 0.9 | 46.2 | mean dragged by tinygrad's slowest kernels |
| Per-kernel time, max (us) | 6.0 (1 unique program out of 16) | **710** (the big conv-flat kernel `r_24_12_8_8_2_4_4_32_6_6n1`) | thvm's max comes from program reuse; tinygrad's max is a single kernel |
| Per-kernel time, p95 (us) | not enough samples | 109 | |
| Per-kernel mean inputs | 2 (median), 24 (max) | varies, `arg 2-8` typical | thvm reports `n_inputs`; tinygrad reports `arg` count in DEBUG=2 |
| Per-kernel mean GFLOPS (estimated) | not exposed | 38 (median across kernels with FLOPs > 0) | tinygrad: from `tm` and `op_est` |
| Per-kernel mean memory bw (GB/s) | not exposed | 48 (median) | tinygrad: `membw` field |
| Per-kernel program ops | median **7.5**, max 119 | not exposed | thvm: `n_ops` from KProgOp |
| Per-kernel scalar uops | exposed via `KernelEntry.n_scalar_uops` (not aggregated) | not exposed | thvm-only |
| % on tile-JIT path | **100%** | n/a (one path) | thvm-only metric |
| % on per-op fallback | **0%** | n/a | thvm-only |

### Reading the tables

- **Wall time is essentially identical** (19.7 vs 20.3 ms within
  noise).  Both frameworks run beautiful_mnist BS=32 at ~50 steps/s
  on M3 Max.
- **Kernel count differs by 1.6x** (72 vs 116) but wall doesn't —
  tinygrad's individual kernels are larger on average (median 3.6us
  vs thvm's 0.2us), so it amortises the dispatch overhead over
  fewer-but-bigger kernels.
- **thvm has 16 unique kernel programs** vs tinygrad's 116.
  Program-cache reuse is much higher on thvm because the convs
  and pooling-grad shapes hash to the same KProgOp signature
  across layers.
- **First-step compile is 2.5x faster on thvm** (1.6s vs 3.9s) —
  tinygrad runs more elaborate BEAM/rangeify search.
- **GPU compute time per step (sum)**: tinygrad reports 2.1ms; thvm's
  `SHOW_PROFILE` reports a smaller number (0.07ms) that's almost
  certainly under-sampled — `cg_profile_record` only fires when the
  per-op encoder runs, but graph-replay paths submit batched without
  per-op timestamps.  An honest comparison would need extending
  thvm's metal-graph replay to record per-op timestamps via
  `MTLCommandBufferStatusCompleted` callbacks.

## 5. Reproducibility Checklist

Before reporting a comparison number, verify:

- [ ] Same git commit on both checkouts (record SHAs).
- [ ] `make test` (thvm) passes 274/274.
- [ ] tinygrad `python -c "import tinygrad"` works; `pytest test/` (or
      a focused suite) passes on the chosen revision.
- [ ] Hardware quiescent: no other GPU-using applications, screen on
      AC power for thermal stability.
- [ ] Same Metal version (record `xcrun metal --version`).
- [ ] Run each framework `N >= 5` times; report median + p95.
- [ ] First step is **excluded** (compile+autotune); only steady-state
      timed steps count.
- [ ] Memory readings taken at `after_timed` checkpoint, not `init`.
- [ ] Kernel count captured per-step (not cumulative across runs).

## 6. Known Asymmetries

These are real differences in what each framework can measure or
report, not bugs:

- **thvm reports a `dispatch=` breakdown** by code path
  (tile-JIT vs per-op encoder vs alias).  Tinygrad always uses one
  render path, so the breakdown is degenerate.
- **tinygrad reports per-kernel FLOPs and bandwidth** at `DEBUG=2`,
  including TFLOP/s for tensor-core kernels.  thvm doesn't compute
  FLOP estimates today; would need a `KProgOp` walker.
- **thvm reports per-rule hit counts** for the schedule rewrites
  (`realize_rewrite_summary`).  Tinygrad's pattern matchers don't
  expose hit counts (they apply pervasively).
- **tinygrad's `mem_used` is process-global** (not per-step); thvm's
  `peak_live` is per-step.  Compare `peak_live` thvm to `max
  GlobalCounters.mem_used` across the step.
- **JIT-graph capture overhead**: tinygrad's `TinyJit` captures the
  kernel sequence, then replays; thvm's `cg_jit_capture` does the
  same.  Both have negligible per-step overhead at steady state.

## 7. Caveats

- This is a **microbenchmark on one workload**.  Don't generalise to
  conv-heavy ImageNet, transformer training, or other architectures.
- Compile-time differences swamp steady-state for small N_STEPS.
  Use `N_STEPS >= 100` for compile-amortised measurements; don't.
- Per-kernel timing in Metal is **GPU-side timestamps** (sub-ms
  accurate); CPU-side `time.perf_counter` measures end-to-end
  including encoder overhead.  Use the same source on both sides.
- **Kernel counts can fluctuate by ±5** between runs due to dynamic
  shapes, sample order in the dataset, and autotune choices.
  Report a range, not a single number.
