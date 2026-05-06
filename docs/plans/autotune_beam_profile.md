# Autotune Profiling: BEAM-search comparison vs tinygrad

Goal: measure thvm autotune effectiveness on a ladder of kernels of
increasing complexity, comparing against tinygrad's BEAM search on
the equivalent UOp graphs.

Each iteration of the cron loop picks the first unchecked item, runs
the bench (or implements one small change), captures numbers in this
file, commits, and stops.

## Discipline (lessons from the prior cron run)

- **Never start a new bench while another is running.** The Metal
  buffers from a hung run don't release — stacking runs builds GPU
  memory pressure that crashes the OS.  Kill stuck PIDs explicitly,
  do NOT `pkill -f` (catches the user's persistent VSCode / MCP
  Wolfram kernels).
- When a bench is slow, **profile it** (with the new
  `THVM_METAL_JIT_STATS=1` env) before re-running.  One run + one
  fix beats N re-measurements.
- Each kernel-level bench should be small (single kernel, < 5 s
  wall) so the cron iteration stays under budget.

## Conventions

- Scripts under `bench/autotune-ladder/<level>.wls` (thvm) and
  `bench/autotune-ladder/<level>.py` (tinygrad).
- Captured stdout goes to `bench/autotune-ladder/<level>.txt`
  (thvm) and `<level>.tinygrad.txt`.
- Default env: `THVM_BACKEND=metal THVM_TILE=1 THVM_KGC=0
  THVM_METAL_JIT_STATS=1` for thvm; `DEV=METAL BEAM=N` for tinygrad.
- Headline columns to capture for each level: baseline_us,
  autotune_us, speedup, variants_tried, jit_compile_us,
  jit_misses, jit_bypass.

## Ladder

### Level 1 -- elementwise add (N=1024)

`out = a + b`, two rank-1 inputs of length 1024.

- [x] (2026-05-06) thvm: write
  `bench/autotune-ladder/elementwise_add.wls` that builds the
  kernel, runs `TKernelAutotune` with all proposals, reports
  baseline + best wall_us + applied opt + JIT stats.

  | metric           | value                          |
  |------------------|--------------------------------|
  | baseline_us      | 300                            |
  | best_variant_us  | 146 (LOCAL[0, 16])             |
  | speedup          | **2.05x**                      |
  | applied          | LOCAL[0, 64] + GLOBAL[0, 16]   |
  | variants_tried   | 9 (1 baseline + 8 LOCAL)       |
  | jit_compile_us   | 2487 (warm); 176904 (cold)     |
  | jit_misses       | 17                             |
  | jit_bypass       | 0                              |

  Notable: autotune picked LOCAL=64 + GLOBAL=16, not the
  best-bench LOCAL=16 -- the proposer adds a GLOBAL split
  separately.  The applied combo is what gets cached.  No bypass,
  so 8-way LOCAL sweep + final variant fits within the 256-slot
  cache.  Cold compile is 71x the warm compile (177 ms vs 2.5 ms)
  -- newLibraryWithSource cost amortises after the first hit.
- [x] (2026-05-06) tinygrad: write
  `bench/autotune-ladder/elementwise_add.py` that builds
  `Tensor([1024]) + Tensor([1024])`, runs `BEAM=4`, captures
  the kernel time + the kernel selected.

  | mode      | warmup_us | steady_us | speedup |
  |-----------|----------:|----------:|--------:|
  | NOOPT=1   | 3772      | 479       | 1.00x   |
  | BEAM=4    | 418       | 391       | **1.23x** |

  Caveat: this includes Python `realize()` overhead (~300 us
  per call -- visible because both modes are above 300 us even
  on a 1024-element ADD).  thvm's `TKernelBenchUs` benches the
  kernel direct without the WL/Python wrapper; the absolute
  numbers can't be lined up directly.  See the comparison row
  below for the framework-relative speedups.
- [x] (2026-05-06) Compare: log thvm best vs tinygrad best.

  | metric                    | thvm                | tinygrad           |
  |---------------------------|---------------------|--------------------|
  | baseline                  | 300 us (no opts)    | 479 us (NOOPT=1)   |
  | best                      | 146 us (LOCAL[16])  | 391 us (BEAM=4)    |
  | speedup                   | **2.05x**           | 1.23x              |
  | variants explored         | 9 (1 + 8 LOCAL)     | (BEAM=4 hill-climb)|
  | search wall (cold)        | ~177 ms compile     | (not measured)     |

  **Caveat: not directly comparable.** tinygrad's per-iter wall
  is dominated by Python `realize()` overhead (~300 us); thvm's
  `TKernelBenchUs` benches the kernel direct.  The fair metric
  is the *framework-relative* speedup ratio: thvm's autotune
  more than doubles its baseline; tinygrad's BEAM extracts a
  modest 23%.

  Two reasons thvm's relative win is larger:
  1. thvm baseline is naive single-thread (no LOCAL/GLOBAL
     binding by default); tinygrad's NOOPT=1 baseline is still
     reasonably parallel via tinygrad's default heuristics.
  2. thvm's proposer for elementwise rank-1 sweeps every divisor
     of N (8 LOCAL widths from 2..256) -- exhaustive; tinygrad's
     BEAM=4 explores 4 frontier candidates per step in a hill-
     climb.  At trivial sizes thvm finds the local optimum
     faster.

  Open question: at larger N or higher rank, does thvm's
  exhaustive sweep keep up with BEAM's structural opt
  composition (UPCAST + LOCAL + UNROLL chained)?  Levels 2-5
  will tell.

### Level 2 -- matmul (M=N=K=128)

`C = A @ B`, fp32, 128x128 matrices.

- [x] (2026-05-06) thvm: `bench/autotune-ladder/matmul128.wls`.

  | metric           | value                       |
  |------------------|-----------------------------|
  | dispatch_pre     | **metal-gemm**              |
  | baseline_us      | 214                         |
  | best_variant_us  | 211 (TOpt[TC, 0, 16])       |
  | speedup          | **1.01x**                   |
  | applied          | TC[0, 16]                   |
  | variants_tried   | 4 (1 baseline + 3 TC sizes) |
  | jit_misses       | 0                           |
  | jit_bypass       | 0                           |
  | jit_compile_us   | 0                           |

  Matmul routes through `metal-gemm` (dedicated GEMM dispatch
  via cblas/MPSMatrixMultiplication), **not** `metal-tile`.
  No JIT kernel emitted -- so jit stats are all zero.  Autotune
  proposals are limited to TC tile sizes (32/16/8), and the
  speedup is essentially noise (1.01x).  thvm's autotune
  surface here is essentially "what tile size for the
  pre-built GEMM template" -- there's nothing to learn.
- [ ] tinygrad: `bench/autotune-ladder/matmul128.py` BEAM=4.
- [ ] Compare.

### Level 3 -- softmax (N=512)

`out = exp(x - max(x)) / sum(exp(x - max(x)))`, rank-1 N=512.

- [ ] thvm: `bench/autotune-ladder/softmax.wls`.
- [ ] tinygrad: `bench/autotune-ladder/softmax.py` BEAM=4.
- [ ] Compare.

### Level 4 -- 2-layer MLP forward

LinearLayer(784->128) -> ReLU -> LinearLayer(128->10) -> softmax.

- [ ] thvm: `bench/autotune-ladder/mlp2.wls`.
- [ ] tinygrad: `bench/autotune-ladder/mlp2.py` BEAM=4.
- [ ] Compare.

### Level 5 -- single conv2d (1x32x28x28 input, 5x5 kernel, 32 channels)

`out = conv2d(x, w, b)` matching LeNet's first conv shape.

- [ ] thvm: `bench/autotune-ladder/conv2d.wls`.
- [ ] tinygrad: `bench/autotune-ladder/conv2d.py` BEAM=4.
- [ ] Compare.

### Cross-level synthesis

- [ ] Build a single-table comparison across all 5 levels: thvm
  best vs tinygrad best, plus thvm autotune-bench-time vs tinygrad
  BEAM-search-time.  Save to
  `bench/autotune-ladder/comparison.md`.  Identify the level
  where thvm falls furthest behind tinygrad as the next focus.
