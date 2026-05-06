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
- [x] (2026-05-06) tinygrad: `bench/autotune-ladder/matmul128.py`
  BEAM=4.

  | mode      | warmup_us | steady_us | speedup |
  |-----------|----------:|----------:|--------:|
  | NOOPT=1   | 33953     | 915       | 1.00x   |
  | BEAM=4    |   788     | 773       | **1.18x** |

  Tinygrad NOOPT cold compile is 34 ms -- a real GEMM kernel
  needs much more codegen than ADD.  After BEAM landed an opt
  sequence the warmup drops to 788 us (close to steady).  1.18x
  speedup is similar to thvm's 1.01x: both autotune surfaces
  find this matmul size already close to optimum on Metal.
- [x] (2026-05-06) Compare.

  | metric              | thvm                | tinygrad           |
  |---------------------|---------------------|--------------------|
  | dispatch path       | metal-gemm (MPS)    | tinygrad GEMM      |
  | baseline (us)       | 214                 | 915                |
  | best (us)           | 211 (TC[0,16])      | 773 (BEAM=4)       |
  | speedup             | **1.01x**           | **1.18x**          |
  | variants tried      | 4 (1 + 3 TC sizes)  | (BEAM=4 hill-climb)|
  | warmup cold (ms)    | n/a (no JIT)        | 33.95              |

  Both autotune surfaces are essentially flat on 128x128 fp32
  matmul.  Absolute timing favours thvm (211 vs 773 us) because
  thvm's metal-gemm path goes straight to
  MPSMatrixMultiplication while tinygrad codegens its own GEMM.
  But the *autotune-relative* speedup ratio favours tinygrad
  slightly (1.18x vs 1.01x) -- BEAM at least found a small
  structural improvement; thvm's TC tile sweep didn't.

  **Open question for level 3+:** at what kernel shape does
  thvm's autotune surface (LOCAL/GLOBAL/UPCAST/UNROLL/GROUP +
  GROUPTOP + TC) start to compose meaningfully?  Softmax has a
  reduce-broadcast pattern -- closer to the elementwise case,
  with a real opt surface.

### Level 3 -- softmax (N=512)

`out = exp(x - max(x)) / sum(exp(x - max(x)))`, rank-1 N=512.

- [x] (2026-05-06) thvm: `bench/autotune-ladder/softmax.wls`.

  TSoftmax(N=512) decomposes into 3 kernels (max-reduce +
  exp/centred chain + sum-reduce-divide).  Per-kernel autotune:

  | kid | kind        | baseline | best | opt              | applied                   |
  |-----|-------------|---------:|-----:|------------------|---------------------------|
  | 2   | metal-op    | 212 us   | 154  | LOCAL[0,32]      | LOCAL[0,32] + GLOBAL[0,16]|
  | 3   | metal-tile  | 236 us   | 231  | UNROLL[1,16]     | GROUP[1,4]                |

  Totals: **baseline 624 us, best 558 us, speedup 1.12x.**
  Both kernels with proposals got applied winners.  Note kid 3
  is on the metal-tile path -- the autotune surface here is the
  one we built; kid 2 is on metal-op (unfused reduce / broadcast
  outliers don't yet lift through `kernel_lift_to_uop`).

  jit_misses=48, jit_bypass=0, jit_compile_us=121548 (cold).
  Cache fits cleanly inside the 256-slot cap.
- [x] (2026-05-06) tinygrad: `bench/autotune-ladder/softmax.py`
  BEAM=4.

  | mode      | warmup_us | steady_us | speedup |
  |-----------|----------:|----------:|--------:|
  | NOOPT=1   | 21832     | 1245      | 1.00x   |
  | BEAM=4    |  1354     | 1141      | **1.09x** |

  Tinygrad also decomposes softmax into 3 kernels per iter
  (kernel_count=150 = 3 * 50 reps), matching thvm.  BEAM=4 finds
  a 1.09x win -- close to thvm's 1.12x.  Both autotune surfaces
  agree this kernel chain is mostly memory-bound and there's
  little to extract.
- [x] (2026-05-06) Compare.

  | metric              | thvm                | tinygrad           |
  |---------------------|---------------------|--------------------|
  | kernels per softmax | 3                   | 3                  |
  | baseline (us)       | 624 (sum of 3)      | 1245 (per call)    |
  | best (us)           | 558                 | 1141               |
  | speedup             | **1.12x**           | **1.09x**          |

  **Convergent finding.**  Both frameworks decompose softmax
  into the same 3-kernel structural shape (max-reduce + centred-
  exp + sum-reduce-divide), and both autotune surfaces find
  ~10% wins.  The chain is memory-bound (mostly a few load /
  reduce / store passes); per-kernel rewriting can only do so
  much.

  Absolute numbers don't line up: thvm's 624/558 us is a sum of
  per-kernel benches (TKernelBenchUs at 100 reps each), tinygrad's
  1245/1141 us is the wall-time of one softmax() call including
  Python overhead and 3 sequential kernel dispatches.  Both
  bracket the same physical work.

  **Implication for thvm**: the unfused metal-op outlier (kid 2)
  on softmax is the biggest target -- if it lifted onto metal-
  tile, the whole chain might fuse via reduce-broadcast collapse
  in rangeify and shrink to 1-2 kernels.

### Level 4 -- 2-layer MLP forward

LinearLayer(784->128) -> ReLU -> LinearLayer(128->10) -> softmax.

- [x] (2026-05-06) thvm: `bench/autotune-ladder/mlp2.wls`.

  MLP2(784->128->10) emits 7 kernels (1 metal-gemm + 5 metal-tile
  + 1 metal-op).  Per-kernel autotune:

  | kid | kind        | baseline | best | opt              | applied                   |
  |-----|-------------|---------:|-----:|------------------|---------------------------|
  | 1   | metal-gemm  | 480 us   | 440  | TC[0,16]         | TC[0,16]                  |
  | 2   | metal-tile  | 203 us   | 152  | LOCAL[0,16]      | LOCAL[0,2] + GLOBAL[0,64] |
  | 3   | metal-tile  | 279 us   | 203  | UNROLL[1,16]     | GROUP[1,64]               |
  | 4   | metal-tile  | 155 us   | 149  | LOCAL[0,2]       | LOCAL[0,2] + GLOBAL[0,5]  |
  | 5   | metal-tile  | 154 us   | 148  | GROUP[1,2]       | (none)                    |
  | 6   | metal-op    | 194 us   | 185  | LOCAL[0,2]       | (none)                    |
  | 7   | metal-tile  | 144 us   | 143  | GROUP[1,10]      | GROUP[1,10]               |

  Totals: **baseline 1609 us, best 1420 us, speedup 1.13x.**
  5 of 7 kernels got applied winners.  The biggest per-kernel
  wins are kid 2 and kid 3 (the LinearLayer reduce-broadcast
  composite kernels) at 1.34x and 1.37x respectively.

  jit_misses=54, jit_bypass=0, jit_compile_us=152632 (cold).
  Cache fits cleanly.  71% of kernels (5/7) are on the new
  metal-tile path -- the metal-op outlier (kid 6) is the same
  unfused softmax-tail pattern from level 3.
- [x] (2026-05-06) tinygrad: `bench/autotune-ladder/mlp2.py`
  BEAM=4.

  | mode      | warmup_us | steady_us | speedup | kernels/iter |
  |-----------|----------:|----------:|--------:|-------------:|
  | NOOPT=1   | 167088    | 2729      | 1.00x   | 5            |
  | BEAM=4    |   5276    | 2499      | **1.09x** | 5            |

  Tinygrad emits **5 kernels per forward** vs thvm's 7 -- 28%
  better structural fusion at the scheduler level.  NOOPT cold
  compile is 167 ms (BEAM warmup is 5.3 ms because BEAM caches
  kernels across the 3 warmup iters).  BEAM=4 finds 1.09x --
  again close to thvm's 1.13x relative gain, but tinygrad starts
  from a smaller kernel-count budget so the absolute work is
  less.
- [x] (2026-05-06) Compare.

  | metric                | thvm                | tinygrad           |
  |-----------------------|---------------------|--------------------|
  | kernels per forward   | **7**               | **5**              |
  | baseline (us)         | 1609 (per-kern sum) | 2729 (call wall)   |
  | best (us)             | 1420                | 2499               |
  | speedup               | **1.13x**           | **1.09x**          |

  **Structural finding (the headline of this level).** Tinygrad's
  scheduler fuses 2 more layer-pairs' worth of work per forward
  -- 5 kernels vs 7.  Both autotune surfaces extract ~10%
  per-kernel; the absolute speedup ratio between the two is
  similar (1.13 ≈ 1.09).

  Where does thvm leak 2 extra kernels?
  - Softmax tail (kid 6 metal-op) -- the unfused outlier
    documented in level 3.
  - One additional kernel on the LinearLayer chain --
    bufferize boundary between matmul + activation that
    tinygrad fuses through.

  Next-tier work for thvm is **structural fusion** (collapse
  the metal-op outliers, fuse activation into linear, fuse
  the centred-exp into the max-reduce kernel), not tighter
  autotune.  Per-kernel tuning is already at parity (~10% wins)
  but the unit on which we tune is too small.

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
