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

- [x] (2026-05-06) thvm: `bench/autotune-ladder/conv2d.wls`.

  Conv2D(1x32x28x28, 5x5 kernel, 32 c_out) emits 3 kernels:

  | kid | kind        | baseline | best | opt              | applied                      |
  |-----|-------------|---------:|-----:|------------------|------------------------------|
  | 2   | metal-op    | 2847 us  | 1969 | UNROLL[2,4]      | UNROLL[2,2]                  |
  | 3   | metal-tile  | 722 us   | 295  | LOCAL[0,16]      | LOCAL[0,16] + GLOBAL[0,2]    |

  Totals: **baseline 3569 us, best 2264 us, speedup 1.58x.**
  Both proposing kernels got applied winners.

  **This is the biggest thvm autotune speedup in the ladder so
  far** (1.58x vs the 1.01-1.13x at simpler kernels).  Conv2D
  has real autotune surface: the kh*kw inner-loop UNROLL fits
  the metal-op patch-sum kernel; the output-axis LOCAL+GLOBAL
  split saturates GPU threads on the metal-tile reduce.  At this
  shape thvm's per-kernel tuning is doing meaningful work, not
  just polishing.

  jit_misses=11, jit_bypass=0, jit_compile_us=117752 cold.
- [x] (2026-05-06) tinygrad: `bench/autotune-ladder/conv2d.py`
  BEAM=4.

  | mode      | warmup_us | steady_us | speedup | kernels/iter |
  |-----------|----------:|----------:|--------:|-------------:|
  | NOOPT=1   | 86887     | 1592      | 1.00x   | 1            |
  | BEAM=4    |  2990     | 1604      | **0.99x** | 1            |

  Tinygrad fuses the entire conv2d into **1 kernel per call**
  (vs thvm's 3).  BEAM=4 finds NO improvement -- the default
  heuristic kernel is already at its local optimum.  NOOPT cold
  compile is 87 ms (vs 3 ms BEAM warmup, since BEAM caches).

  Absolute steady at 1592 us beats thvm's tuned 3-kernel chain
  at 2264 us.  This is the level where the structural fusion gap
  is most visible: thvm spends 2x the work because it dispatches
  3 kernels for what tinygrad does in 1.
- [x] (2026-05-06) Compare.

  | metric              | thvm                | tinygrad           |
  |---------------------|---------------------|--------------------|
  | kernels per conv    | **3**               | **1**              |
  | baseline (us)       | 3569 (per-kern sum) | 1592 (call wall)   |
  | best (us)           | 2264                | 1604               |
  | autotune speedup    | **1.58x**           | **0.99x**          |
  | absolute best       | 2264 us             | **1604 us** (-29%) |

  **The biggest absolute gap in the ladder.**  Three findings:

  1. **Structural**: tinygrad fuses the entire conv2d into 1
     kernel; thvm dispatches 3 (im2col patch-sum + matmul-style
     reduce + bias-broadcast).
  2. **Autotune**: thvm extracts a real 1.58x from per-kernel
     tuning -- the biggest in the ladder.  Tinygrad's BEAM=4
     finds 0.99x because the default-heuristic kernel is
     already at its local optimum.
  3. **Net**: thvm's tuned 3-kernel chain (2264 us) is still
     41% slower than tinygrad's untuned single kernel (1604 us).

  Restated: **thvm's autotune is doing more work but on the
  wrong unit.**  Per-kernel tuning saturates at ~1.58x but
  the fundamental win comes from fusing the chain into 1 kernel
  in the first place.  A monoidic-fused conv2d kernel on the
  thvm tile path could close most of the 41% gap before any
  autotune is applied -- and at that point per-kernel tuning
  from there is gravy.

### Cross-level synthesis

- [x] (2026-05-06) Build a single-table comparison across all 5
  levels.  Saved to
  [bench/autotune-ladder/comparison.md](../../bench/autotune-ladder/comparison.md).

  | level | shape          | thvm best | thvm   | tinygrad best | tinygrad | absolute winner   |
  |-------|----------------|----------:|-------:|--------------:|---------:|-------------------|
  | 1     | elem add 1024  |    146 us |  2.05x |        391 us |    1.23x | thvm   (2.7x)     |
  | 2     | matmul 128     |    211 us |  1.01x |        773 us |    1.18x | thvm   (3.7x)     |
  | 3     | softmax 512    |    558 us |  1.12x |       1141 us |    1.09x | thvm   (2.0x)     |
  | 4     | MLP2           |   1420 us |  1.13x |       2499 us |    1.09x | thvm   (1.8x)     |
  | 5     | conv2d 28x28   |   2264 us |  1.58x |       1604 us |    0.99x | tinygrad (-29%)   |

  **Key findings**: per-kernel autotune is at parity (both
  extract 1.0-1.6x); thvm leaks +2 kernels per forward at MLP2
  and conv2d; **conv2d is where thvm falls furthest behind**
  in absolute wall (-29%, despite tuning extracting the biggest
  relative win 1.58x), and the mechanism is **structural
  fusion**, not autotune.

  Top 3 leverage points (ordered by ROI):
  1. Fuse the conv2d patch-sum + reduce + bias-broadcast chain
     into 1 kernel on the metal-tile path.
  2. Lift the softmax tail (metal-op outlier) through
     `kernel_lift_to_uop` to enable reduce-broadcast collapse.
  3. Fuse LinearLayer + activation boundary.

  Per-kernel autotune extension is low-ROI until 1, 2, 3 land;
  the kernels we tune today are too small.

### Lift the softmax tail (highest-ROI single fix)

The softmax chain (level 3) emits 3 kernels: max-reduce,
centred-exp, sum-reduce-divide.  In MLP2 (level 4) the same
pattern surfaces -- kid 6 lands on metal-op (the unfused
outlier).  The metal-op kernel is the divide-after-sum that
the rangeify reduce-broadcast collapse can't fold because
`kernel_lift_to_uop` rejects its scalar shape.

- [x] (2026-05-06) Run a softmax(N=512) bench under
  `THVM_DUMP_LIFT_REJECT=1` and capture the exact reject reason
  for the metal-op outlier.  Saved to
  `bench/autotune-ladder/softmax_lift_reject.txt`.

  **One reject category**, 4 occurrences:

      lift reject: index/ndim-mismatch buf_ndim=1 src_count=1

  Translation: outer S_INDEX has `src_count == 1` (just the
  buffer, no range srcs); the buffer has rank 1.  Semantically
  this is a **scalar read from a singleton {1}-shape buffer** --
  the result of a reduce broadcast back into the per-element
  divide.  `lift_scalar_index` walks ranges and bails because
  `ndim != outer_rank`.

  Different shape from the LeNet S_INDEX-of-S_INDEX rejects.
  Simpler fix: special-case `outer_rank == 0 && ndim == 1` ->
  return `uop_const(DT_INT32, 0)` (address always 0; the {1}
  buffer has a single element at offset 0).

### Softmax-tail lift fix
- [x] (2026-05-06) Add the singleton-buffer special case to
  `lift_scalar_index`.

  Added a one-line branch: `outer_rank == 0 && ndim == 1 &&
  uop_buffer_dim(buf, 0) == 1 -> return uop_const(DT_INT32, 0)`.
  The {1}-buffer has its only element at offset 0, and a
  zero-range S_INDEX is the scalar read.

  **Effect**: the metal-op outlier disappears on both softmax
  and MLP2.  Re-bench:

  | level | metric         | pre-fix             | post-fix         |
  |-------|----------------|---------------------|------------------|
  | 3 softmax | dispatch    | 2 tile + 1 metal-op | **3 tile**       |
  | 3 softmax | baseline    | 624 us              | 617 us           |
  | 3 softmax | best        | 558 us              | **457 us**       |
  | 3 softmax | speedup     | 1.12x               | **1.35x**        |
  | 4 MLP2    | dispatch    | 6 tile + 1 metal-op + 1 gemm | **6 tile + 1 gemm** |
  | 4 MLP2    | baseline    | 1609 us             | 1448 us          |
  | 4 MLP2    | best        | 1420 us             | **1383 us**      |

  18% absolute drop on softmax (558 -> 457 us best) and 10%
  drop on MLP2 baseline -- the metal-tile dispatch is faster
  per call than the metal-op fallback even before autotune.

  make test 274/274 unchanged.
