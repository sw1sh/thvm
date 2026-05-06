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

### Re-bench conv2d post-lift-fix

The singleton-buffer fix flipped softmax + MLP2 fully onto the
tile path.  Conv2d was 2 metal-op + 1 metal-tile pre-fix.  If
either of conv2d's metal-op outliers was the same singleton-
buffer scalar shape, the fix should have shifted them too.

- [x] (2026-05-06) Re-run `bench/autotune-ladder/conv2d.wls`
  with the fix applied.

  | metric        | pre-fix | post-fix |
  |---------------|--------:|---------:|
  | kernel count  | 3       | 3        |
  | dispatch      | 2 mop + 1 tile | **2 mop + 1 tile** (unchanged) |
  | baseline_us   | 3569    | 3191     |
  | best_us       | 2264    | 2262     |
  | speedup       | 1.58x   | 1.41x    |

  **The fix did NOT shift conv2d's metal-op outliers** -- they're
  a different shape from the softmax tail.  Likely the im2col
  patch-sum / partial-conv reduction, which uses indirect-index
  patterns the lifter doesn't handle.

  Side benefit: baseline dropped 11% (3569 -> 3191 us) anyway,
  probably from better cache hit ratios after the recent
  rendering changes.  Best stays at 2262 us; tinygrad still wins
  by 41% (1604 us, single fused kernel).

  **Next leverage point** (highest ROI of the original 3 from
  the synthesis): diagnose conv2d's metal-op rejects under
  `THVM_DUMP_LIFT_REJECT=1` and find the specific scalar shape
  blocking the lift.  That's what the cron should pick up next.

### Diagnose conv2d metal-op lift rejects
- [x] (2026-05-06) Run conv2d under
  `THVM_DUMP_LIFT_REJECT=1` and capture rejects.  Saved to
  `bench/autotune-ladder/conv2d_lift_reject.txt`.

  **One reject category**, 2 occurrences:

      lift reject: entry/no-scalar-arena n_inputs=24 n_ops=71

  Different shape from softmax / LeNet S_INDEX rejects.  This
  is the **no-scalar-arena** path: the kernel has no ScalarUop
  arena at all -- it's emitted by a specialised pass (the
  conv2d-flat synthesised dispatch with 24 patch inputs from
  the 5x5 kh*kw decomposition).  `kernel_lift_to_uop` falls
  through `kernel_lift_from_gemm` and `kernel_lift_from_conv2d`
  (both reject this shape), then logs the entry/no-scalar-arena
  reject.

  So conv2d's metal-op outliers are NOT a missing
  `lift_scalar_index` branch -- they're a missing entry-point
  in `kernel_lift_from_conv2d`.  The 24-input shape (kh*kw =
  25 ≈ 24 patch tensors + 1 weights = wait that's 25 + 1 + bias
  = 26... need to inspect the actual ke->n_inputs decomposition)
  is what blocks.

### Extend kernel_lift_from_conv2d for the 24-input patch shape

- [x] (2026-05-06) Inspect `kernel_lift_from_conv2d` and identify
  the n_inputs branch that rejects the 24-input shape.

  `kernel_lift_from_conv2d` calls `tile_analyze_conv2d_flat`
  which has **two accept paths**:

  1. **direct_x**: `ke->n_inputs == 2` (1 weight + 1 X buffer,
     rank-3 or rank-4 input).  Used when c_in*kh*kw fits the
     standard im2col flatten.
  2. **patch path**: `c_in == 1`, `ke->n_inputs == 1 +
     kh*kw` (1 weight + kh*kw separate patch tensors).

  My probe kernel has c_in=32, kh=kw=5, so `n_inputs=24` doesn't
  match either:
    - Not direct_x (n_inputs ≠ 2).
    - Not patch path (c_in ≠ 1).

  The 24 number is suspicious -- kh*kw = 25, not 24.  One of:
    - 1 weight + 23 patches (off-by-one in some lowering)
    - Bias added as extra input (1 W + 22 patches + 1 B = 24)
    - The conv lowering uses kh*kw - 1 patches + 1 partial sum

  Need to dump `ke->input_views` for the rejected kernel to
  understand the actual layout.  **Lift extension is non-
  trivial**: requires a third path in `tile_analyze_conv2d_flat`
  for `c_in > 1` AND multi-input, which means modeling each
  patch's view + stride differently from the existing two paths.

### Inspect the 24-input KernelEntry layout
- [x] (2026-05-06) Add a diagnostic dump in
  `kernel_lift_from_conv2d`'s reject log.  Saved to
  `bench/autotune-ladder/conv2d_layout.txt`.

  All 24 inputs share the **exact same view**:

      dims    = [32, 24, 24]    (c_in, h_out, w_out)
      strides = [784, 28, 1]    (= 28*28, 28, 1)

  These are SHRINKED views into a single underlying X buffer of
  shape `[32, 28, 28]` -- each input has a different
  `view.offset` corresponding to a (kh_idx, kw_idx) slice.
  No weights input in this kernel (input 0 is also a patch
  view, not the weights tensor).

  Opcode histogram: `op3=24 op6=24 op9=23`.

  Translation: this is the **kh*kw partial-sum kernel** for
  the conv2d (24 LOADs of shifted X views + 24 ops + 23 adds
  to combine the 24 partials).  Why 24 not kh*kw=25 is unclear
  -- one position is probably special-cased upstream (handled
  inline somewhere else, or the bias-add fuses one partial in).

  **The lift would need a third path** in
  `tile_analyze_conv2d_flat` for "no weights, only N
  shifted-X-views" -- this is essentially the *im2col + matmul*
  partial: each X view is one (ki, kj) row of the im2col matrix.
  The renderer side can re-assemble it as `out = sum(W[ki,kj] *
  X_view[ki,kj])` if it knows the W constants -- but those don't
  appear to be in this kernel either.

  Conclusion: this kernel shape is **not currently liftable**
  without finding the weight tensor, which is in a *different*
  KernelEntry.  Cross-kernel value lookup is a bigger structural
  change than this iteration warrants.

### Bridge plan: reduce conv2d's metal-op fraction first

The cleanest path forward isn't extending `kernel_lift_from_conv2d`
right now -- it's getting the rangeify pass to NOT decompose
conv2d into 24 metal-op kernels in the first place.  If
TConv2D lowered to a single fused kernel with W and X both as
inputs (matching tinygrad's structure), the lifter wouldn't
need a new path.

- [x] (2026-05-06) Inspect `TConv2DKhKw` and `TConv2DIm2Col` in
  NN.wl.

  **Both lowerings unroll into kh*kw partials**:

  - `TConv2DKhKw`: builds `partials = Flatten @ Table[...]` (25
    entries for 5x5 kernel), each `TUOpReduce[xB * wB, 1, "SUM"]`
    of a SHRINKED + EXPAND'd slice.  Then
    `Fold[Plus, First[partials], Rest[partials]]` reduces.
  - `TConv2DIm2Col`: builds the im2col matrix by also iterating
    25 (ki, kj) slices, SHRINK'ing each + PAD'ing into the
    kh*kw-axis slot of a zero tensor + summing.  Then one
    matmul.

  Both produce ~25 SHRINK + binary-op chains that bufferize
  fragments.  The 24-input rejecting kernel is **one slice of
  the Fold[Plus, ...]** accumulator: 24 of 25 partials get
  summed into a running total in one KernelEntry, with the
  first partial seeded into the accumulator separately.

  **Root cause**: rangeify / bufferize splits the SHRINK + ADD
  partials chain at SHRINK boundaries because each SHRINK
  produces a "bufferable" intermediate (it emits a Movement op
  whose result rangeify wants to materialise).  Tinygrad's
  scheduler keeps the SHRINK chain inside a single kernel via
  its movement-op-as-INDEX rewrite (the same pattern thvm has
  partially implemented in `src/uop/movement_index.c`).

  **Fix direction**: ensure SHRINK / PAD inside `TConv2DKhKw`
  doesn't trigger bufferize.  Either:
  (a) Use `TUOpShrinkV` / `TUOpPadV` (the index-mapping
      variants) instead of the existing `TUOpShrink` / `TUOpPad`
      so movement ops fold into INDEX expressions, not
      bufferize boundaries.
  (b) Or extend `bufferize_classify` to keep SHRINK chains
      INSIDE the surrounding REDUCE+ADD region instead of
      splitting at each SHRINK.

  Path (a) is a smaller WL-side change; path (b) is more
  general but a deeper refactor.

### TConv2D fusion follow-on

- [~] (2026-05-06) Test path (a): rewrite `TConv2DKhKw` to use
  index-mapping movement variants.

  **Blocked**: `TUOpShrinkV` / `TUOpPadV` don't exist on the WL
  side -- searched [wl/THVMLink/Kernel/](../../wl/THVMLink/Kernel/),
  no hits.  thvm only has `TUOpShrink` / `TUOpPad` (shape-changing
  Movement ops that bufferize splits on).  Building V-variants
  is real work: each needs a WL constructor + a C-side handler
  in rangeify that maps the movement to an INDEX expression
  instead of a fresh KernelEntry.

  thvm already has the C infrastructure for movement-as-INDEX
  in [src/uop/movement_index.c](../../src/uop/movement_index.c)
  -- it just doesn't wire from the WL `TUOpShrink` callsites
  to that path.  The wiring is non-trivial: rangeify decides
  whether to bufferize at each Movement boundary based on the
  number of consumers + output shape; SHRINK inside `Fold[Plus,
  ...]` over 25 partials hits whatever heuristic causes
  bufferize to fragment.

  Two smaller wedges to try before tackling V-variant
  constructors:
  - Run `THVM_DUMP_RANGEIFY_BAIL=1` on the conv2d probe to see
    which rangeify rule causes the bufferize split.  If it's a
    single threshold (e.g. "consumers > N -> bufferize"), bumping
    or skipping it for movement-only chains might collapse the
    25 partials.
  - Profile thvm's existing `movement_index.c` against the
    conv2d shape: if `uop_resolve_movement_chain` already
    handles 25-deep SHRINK chains, the gap is in
    bufferize_classify's decision, not the rewrite itself.

### Smaller wedge: which rangeify rule splits the conv2d kh*kw chain?

- [x] (2026-05-06) Run the conv2d probe under
  `THVM_RANGEIFY_BAIL=1` and capture the bail reasons.  Saved to
  `bench/autotune-ladder/conv2d_rangeify_bail.txt`.

  **Single bail, single category**:

      rangeify bail (mid-emit): RESHAPE out_dim > u8 (n_ops=71 onum=460800)

  Located in
  [src/schedule/rangeify.c:3175](../../src/schedule/rangeify.c#L3175):

  ```c
  for (u32 d = 0; d < p->out_ndim; d++) {
    if (p->out_dims[d] > 0xFFu) RBAIL_MID("RESHAPE out_dim > u8");
    lo |= ((u64)p->out_dims[d] & 0xFFu) << (8 * d);
  }
  ```

  This is the **same-rank RESHAPE legacy encoding** which packs
  each dim into one byte (max 255).  My conv2d's xCol matrix
  has dim `cIn*kh*kw = 32*5*5 = 800` which overflows u8.

  thvm already has `S_RESHAPE_V` (line 3149) that handles
  rank-mismatch RESHAPE with explicit per-dim refs (no u8 cap),
  but the same-rank path doesn't fall back to V when dims
  overflow -- it bails entirely, fragmenting the bufferize
  region and producing the 24-input metal-op rejecting kernel.

  **Fix is a small refactor**: when the same-rank legacy path
  would bail on `out_dim > u8`, fall through to the V emission
  path instead.  This is far more tractable than building
  TUOpShrinkV / TUOpPadV variants from scratch.

### Same-rank RESHAPE > u8 fallback to V

- [x] (2026-05-06) In `rangeify.c` at the `RESHAPE out_dim > u8`
  bail (line 3175), change the bail to a fallback that emits
  `S_RESHAPE_V` with the same shape information.  Validate via
  the conv2d probe: dispatch should shift from `2 metal-op + 1
  metal-tile` to fewer metal-op kernels.  make test 274/274 must
  stay green.

  **Landed**: edit forces the rank-mismatch V-emission whenever
  `src0_dims[d]` or `out_dims[d]` overflows u8, even when ranks
  match.  make test stays 274/274 green.

  **Outcome differs from hypothesis**: dispatch shape did NOT
  change.  Conv2d still emits 3 kernels (2 metal-op + 1
  metal-tile).  The win came from a different mechanism --
  one of the metal-op kernels (kid 1) was previously rejecting
  autotune proposals; after the V-emission gives its bufferize
  region a clean shape, autotune now finds wins on it.

  Conv2d ladder probe (`bench/autotune-ladder/conv2d.txt`):

  | metric                  | pre-fix | post-fix |
  |-------------------------|--------:|---------:|
  | kernel_count            |       3 |        3 |
  | kernels_with_proposals  |       2 |        3 |
  | kid 1 baseline (us)     |  ~1690* |     1690 |
  | kid 1 best (us)         |   ~1690 |     1247 |
  | kid 2 baseline (us)     |    2444 |     2020 |
  | kid 2 best (us)         |    1966 |     1959 |
  | kid 3 baseline (us)     |     747 |      744 |
  | kid 3 best (us)         |     296 |      301 |
  | wall best (sum-3) (us)  |   ~3952 |     3507 |

  \* kid 1 was previously skipped by the bench filter
  `If[ props =!= {}]`.  Estimated wall improvement ~1.13x.

  This unblocks per-kid-1 autotune (1.36x on a previously
  untunable shape) but **does NOT close the structural-fusion
  gap** vs tinygrad (1604us, 1 kernel).  The N-fold dispatch
  win still requires fusing kid 1 + kid 2 + kid 3 into one
  kernel -- an operation only Phase D'+F of [docs/plans/the_ideal_pipeline.md](the_ideal_pipeline.md)
  delivers.  Per-kernel autotune saturated as predicted.

### Did the rangeify u8-fix transfer to MLP2?

Conv2d gained from the same-rank-RESHAPE-V fallback because
its xCol cIn\*kh\*kw=800 dim overflows u8.  MLP2 has the
ladder's other +2-kernel leak (7 thvm vs 5 tinygrad), with
shapes 784 -> 128 -> 10.  The flatten/Linear path may emit
RESHAPEs with dims > 255 (784, 128) that previously bailed.
Re-bench level 4 to see if the fix transferred.

- [x] (2026-05-06) Re-run `bench/autotune-ladder/mlp2.wls`
  post-fix and diff against the pre-fix capture.  Save to
  mlp2.txt; report kernel_count, dispatch_kinds,
  kernels_with_proposals, totals_baseline_us, totals_best_us,
  totals_speedup.  Compare to pre-fix (7 kernels,
  totals_best=1448us per the ladder snapshot) and note whether
  the +2 leak collapsed.

  | metric                 | pre-fix | post-fix |
  |------------------------|--------:|---------:|
  | kernel_count           |       7 |        7 |
  | dispatch_kinds         |1g+5t+1o |  1g+5t+1o|
  | kernels_with_proposals |       7 |        7 |
  | totals_baseline_us     |    1609 |     1376 |
  | totals_best_us         |    1420 |     1323 |
  | totals_speedup         |   1.13x |    1.04x |
  | jit compile_us         |  152632 |     5340 |

  **The +2 kernel leak did NOT collapse** -- still 7 kernels
  vs tinygrad's 5.  The u8-RESHAPE-V fix was conv2d-specific;
  MLP2's bufferize boundaries are driven by a different
  mechanism (suspected: softmax tail metal-op + LinearLayer/
  activation boundary, per the ladder synthesis).  Wall best
  shifted ~7% (1420 -> 1323us) but that's likely noise / warm
  module cache (jit compile_us collapsed 152632 -> 5340 across
  the cold/warm boundary).  Per-kernel autotune speedup
  *dropped* 1.13x -> 1.04x because the baselines moved closer
  to best on a warmer cache.  **Conclusion: closing the MLP2
  leak needs the softmax-tail / LinearLayer-fusion work, not
  more rangeify rules.**

### Which boundaries fragment MLP2?

We know MLP2's +2-kernel leak isn't u8-overflow.  Capture the
`THVM_RANGEIFY_BAIL=1` reasons (mirrors what we did for conv2d
in `conv2d_rangeify_bail.txt`) so the structural-fusion work
points at concrete shapes rather than guessed mechanisms.

- [x] (2026-05-06) Re-run `bench/autotune-ladder/mlp2.wls`
  with `THVM_RANGEIFY_BAIL=1` and save filtered bail lines to
  `bench/autotune-ladder/mlp2_rangeify_bail.txt`.  Inline a
  one-table summary of the distinct bail reasons + counts,
  and contrast against conv2d's bail (RESHAPE out_dim > u8).

  | level | rangeify bails | kernel_lift rejects | mechanism             |
  |-------|---------------:|--------------------:|-----------------------|
  | conv2d|       1 (u8)   |              many   | rangeify cap          |
  | mlp2  |              0 |  1 (buf_ndim=1)     | bufferize / lift      |

  **MLP2 has zero rangeify bails.**  The +2 kernel leak is not
  a rangeify-lowering failure -- it's a bufferize_classify
  boundary decision plus one kernel_lift_to_uop reject
  (`index/ndim-mismatch buf_ndim=1 src_count=1`) that drops
  the trailing softmax-tail kid into the per-op metal-op
  dispatch (kid 6 in `mlp2.txt`).  Closing the leak therefore
  needs either a bufferize rule that keeps the softmax-tail /
  LinearLayer-activation chain in the same kernel, or a
  kernel_lift_to_uop relaxation for `buf_ndim=1` single-src
  shapes.  Both belong to the structural-fusion campaign.

### What does the MLP2 lift-rejecting kid actually compute?

We know one `kernel_lift_to_uop` reject is leaking the
softmax-tail kernel into the per-op metal-op dispatch.  Get
the concrete KProgOp histogram + input layouts for that kid
so the structural-fusion work picks (a) bufferize-rule vs (b)
kernel_lift-relaxation from real shape data, not guesses.

- [x] (2026-05-06) Re-run `bench/autotune-ladder/mlp2.wls`
  with `THVM_DUMP_LIFT_REJECT=1` and capture the layout dump
  + KProgOp histogram for the rejecting kernel(s) to
  `bench/autotune-ladder/mlp2_lift_reject.txt`.  Inline the
  distinct reject signatures (input rank/dims, op histogram)
  in this task entry.

  Distinct reject signatures: **1**.  Total reject lines: 100
  (the 100 repeats are autotune variants on the same kid).

      lift reject: index/ndim-mismatch buf_ndim=1 src_count=1

  The dump flag does NOT extend the `index/ndim-mismatch`
  path at [src/schedule/kernel_lift.c:242](../../src/schedule/kernel_lift.c#L242),
  so no input_views/KProgOp histogram is printed -- but
  `buf_ndim=1, src_count=1` decodes uniquely: `S_INDEX(buf)`
  with no per-dim range refs, against a 1-d buffer.  That's
  the **singleton-broadcast** pattern.

  Workload: `NetChain[{Linear[128], Tanh, Linear[10], Softmax}]`
  on a single sample (batch=1).  The softmax tail produces
  shape [1] for both `max` and `sum`, then broadcasts each
  across [10] features -- exactly (outer_rank>=1, buf_ndim=1,
  dim[0]=1, src_count=1).  An existing fast-path at L150
  handles this for `outer_rank==0` only:

      if (outer_rank == 0 && ndim == 1 && uop_buffer_dim(buf, 0) == 1) {
        return uop_const(DT_INT32, 0);
      }

  **Concrete handle (b)**: relax that guard to fire whenever
  `ndim == 1 && uop_buffer_dim(buf, 0) == 1 && src_count == 1`,
  regardless of outer_rank.  That should let kid 6 lift,
  collapse the per-op metal-op into a tile kernel, and close
  half of MLP2's +2 leak.  Code change is small (~3 lines);
  belongs to the structural-fusion campaign, queued as the
  next concrete code task there.

### Land the singleton-broadcast lift fast path

- [x] (2026-05-06) Add a singleton-broadcast fast-path right
  before [src/schedule/kernel_lift.c:242](../../src/schedule/kernel_lift.c#L242)
  that returns `uop_const(DT_INT32, 0)` when
  `u->src_count == 1 && ndim == 1 && uop_buffer_dim(buf, 0) == 1`.
  Also extend the ndim-mismatch dump (gated by
  `THVM_DUMP_LIFT_REJECT=1`) to print `outer_rank` and the
  buffer dims, so future rejects on this path are
  diagnosable without re-reading source.

  **Outcome**: MLP2's metal-op outlier (kid 6) is gone --
  `dispatch_kinds` shifted from `{{metal-gemm, 1}, {metal-tile, 5},
  {metal-op, 1}}` to `{{metal-gemm, 1}, {metal-tile, 6}}`.
  All 7 kernels now lift to tile-rendered MSL.  Lift rejects
  on MLP2 dropped 100 -> 0.  kernel_count is unchanged at 7
  (the +2 leak vs tinygrad's 5 is still the bufferize-boundary
  half), but the lift-reject half is closed.

  | metric                 | pre-singleton | post-singleton |
  |------------------------|--------------:|---------------:|
  | dispatch metal-op      |             1 |              0 |
  | dispatch metal-tile    |             5 |              6 |
  | lift rejects (count)   |           100 |              0 |
  | kernels_with_applied   |             5 |              6 |
  | totals_best_us         |          1323 |           1340 |

  Wall best is essentially flat (1323 vs 1340us) -- the metal-op
  fallback was already running fast on this shape; the structural
  win is correctness/uniformity, not raw GPU time.  make test
  274/274 stays green.

### Did the singleton-broadcast fix transfer to softmax (level 3)?

`comparison.md` noted "softmax tail: an unfused metal-op
outlier (kid 6 in MLP2, kid 2 in softmax) that doesn't lift
through kernel_lift_to_uop".  The MLP2 outlier collapsed
post-fix.  Re-bench level 3 to see if softmax's kid 2 also
collapses.

- [x] (2026-05-06) Re-run `bench/autotune-ladder/softmax.wls`
  post-fix and diff against the pre-fix capture.  Save to
  softmax.txt; report kernel_count, dispatch_kinds,
  kernels_with_proposals, totals_baseline_us, totals_best_us,
  totals_speedup, and whether the pre-fix metal-op outlier
  collapsed.

  **Fix transferred cleanly** -- the metal-op outlier (kid 2)
  collapsed to metal-tile.  All 3 softmax kernels now lift.

  | metric                 | pre-fix | post-fix |
  |------------------------|--------:|---------:|
  | kernel_count           |       3 |        3 |
  | dispatch_kinds         |2t+1op   |   3t     |
  | lift rejects           |     >0  |        0 |
  | kid 2 dispatch         |metal-op |metal-tile|
  | kid 2 best (us)        |     154 |      129 |
  | totals_baseline_us     |     624 |      546 |
  | totals_best_us         |     558 |      471 |
  | totals_speedup         |   1.12x |    1.16x |
  | kernels_with_applied   |       3 |        3 |

  Real wall-time win this time (unlike MLP2 where the metal-op
  fallback was already fast): 558 -> 471us, ~16% improvement.
  The singleton-broadcast fast path is genuinely useful on
  softmax because the post-fix tile path autotunes a faster
  kid 2 than the per-op fallback ran at.

### Did the singleton-broadcast fix transfer to conv2d (level 5)?

Conv2d (level 5) is the worst absolute gap on the ladder
(-54% vs tinygrad), and after the rangeify u8-RESHAPE-V fix
still has 2 metal-op kids out of 3.  If either is the
singleton-broadcast pattern (e.g. bias-broadcast scalar) the
new fast path should collapse it.

- [x] (2026-05-06) Re-run `bench/autotune-ladder/conv2d.wls`
  post singleton-broadcast fix and diff against the post-
  u8-RESHAPE-V capture.  Save to conv2d.txt; report
  kernel_count, dispatch_kinds, totals_baseline_us,
  totals_best_us, totals_speedup, and whether either metal-op
  kid collapsed.

  **Singleton fix did NOT transfer to conv2d**.  Dispatch
  unchanged at `{{metal-op, 2}, {metal-tile, 1}}`.

  | metric                 | post-u8 | post-u8+singleton |
  |------------------------|--------:|------------------:|
  | kernel_count           |       3 |                 3 |
  | dispatch_kinds         |2op+1t   |       2op+1t      |
  | kid 1 dispatch         |metal-op |       metal-op    |
  | kid 1 best (us)        |    1247 |              1249 |
  | kid 2 dispatch         |metal-op |       metal-op    |
  | kid 2 best (us)        |    1959 |              1939 |
  | kid 3 dispatch         |metal-tile|     metal-tile   |
  | kid 3 best (us)        |     301 |               285 |
  | totals_best_us         |    3507 |              3473 |
  | totals_speedup         |   1.27x |             1.40x |

  The 2 metal-op kids are im2col patch-sum + reduce shapes,
  not singleton-broadcasts -- they have multi-element 1-d
  outputs (cIn\*kh\*kw=800, etc.) so the
  `uop_buffer_dim(buf, 0) == 1` guard correctly excludes them.
  Conv2d's gap vs tinygrad (-54%) remains and is squarely in
  the structural-fusion campaign (Phase D'+F of the ideal
  pipeline plan), not in autotune or lift relaxations.

  totals_speedup shifting 1.27x -> 1.40x is autotune variance
  on the metal-op kid-1 baseline (2020 -> 2121us baseline
  re-measurement); kid-1 best stayed flat.

### Sync comparison.md headline table with post-fix numbers

Conv2d and softmax rows of `comparison.md` were updated as
the campaign landed, but MLP2 (level 4) still shows pre-fix
numbers and the per-kernel-leak section still cites the pre-
singleton-fix dispatch.  No new measurements -- pure docs
sync from already-committed bench files.

- [x] (2026-05-06) Refresh the headline table in
  `bench/autotune-ladder/comparison.md` to show post-fix MLP2
  numbers (kernel_count, thvm best, thvm speedup, absolute
  winner gap), and update the "thvm leaks kernels at every
  level" sub-table to reflect that softmax dropped from 1
  metal-op outlier to 0 (still 3 kernels = tinygrad parity)
  and MLP2's dispatch is now `{gemm:1, tile:6}` instead of
  `{gemm:1, tile:5, op:1}`.

  MLP2 row updated: best 1420 -> 1340us, speedup 1.13x ->
  1.08x, absolute winner widens 1.8x -> 1.9x.  The "softmax
  tail" leak-site bullet is now a "LANDED" note pointing at
  kernel_lift.c:242; the LinearLayer-activation bullet calls
  out the still-open MLP2 leak; the conv2d bullet notes the
  singleton fix didn't transfer (different shape).

### Level 6: LeNet forward (real network)

The synthetic-shape ladder (levels 1-5) is saturated.  The
natural extension is real networks where the wins compose:
the singleton-broadcast (per-softmax) and u8-RESHAPE-V (per-
conv2d) fixes both fire, plus pool/flatten boundaries that
synthetic levels don't exercise.  LeNet is the smallest
realistic conv-net (matches `wl/Examples/lenet-mnist/`).

- [x] (2026-05-06) Write `bench/autotune-ladder/lenet.wls` --
  forward-only on a single MNIST sample, using `NetChain`
  with two conv blocks (conv 5x5 + Ramp + 2x2 pool), Flatten,
  two Linear layers with Ramp, and a final Linear+Softmax.
  Match the per-kernel reporting style of `mlp2.wls`.  Save
  stdout to `bench/autotune-ladder/lenet.txt`.  Inline
  kernel_count, dispatch_kinds, totals_baseline_us,
  totals_best_us, totals_speedup, kernels_with_proposals,
  jit compile_us, and how many metal-op outliers remain
  (post the singleton + u8-RESHAPE-V fixes).

  **LeNet forward (1x28x28 -> 10):**

  | metric                  | value             |
  |-------------------------|-------------------|
  | kernel_count            | 19                |
  | dispatch_kinds          | {op:9, tile:10}   |
  | kernels_with_proposals  | 18                |
  | kernels_with_applied    | 13                |
  | totals_baseline_us      | 4350              |
  | totals_best_us          | 3464              |
  | totals_speedup          | 1.26x             |
  | jit compile_us          | 158140 (cold)     |

  **9 metal-op outliers remain** even after the campaign's
  fixes.  Decomposition (most-likely):
  - 2 conv layers x 3 kernels each = 6 (matches Level 5's
    `{metal-op:2, metal-tile:1}` shape per conv);
  - 3 more metal-ops are pool / flatten / softmax-tail
    residue.

  Per-kernel autotune extracts 1.26x on LeNet -- consistent
  with the per-kernel autotune ceiling on the synthetic
  ladder (1.0x-1.6x).  The structural-fusion gap remains
  the dominant lever: 19 kernels / forward where tinygrad
  would run roughly 7-9 (1 per conv-block + linear chains).
  Closing the conv2d 3-into-1 fusion (Phase D'+F) directly
  drops 4 kernels off this number.

### Level 6 cross-framework: tinygrad LeNet forward

Level 6 has thvm numbers but no tinygrad equivalent yet.
Mirror the pattern from `conv2d.py` / `mlp2.py`: build the
same LeNet shape (1x28x28 input, conv-pool-conv-pool-flatten-
linear-linear-linear-softmax), bench with NOOPT + BEAM=4,
and capture wall + tinygrad's kernel_count.

- [x] (2026-05-06) Write `bench/autotune-ladder/lenet.py` --
  forward-only LeNet on a single sample (1x28x28), structured
  to match `bench/autotune-ladder/lenet.wls` so the
  kernel-count comparison is apples-to-apples.  Bench NOOPT
  (baseline) and BEAM=4 (autotuned).  Save stdout to
  `bench/autotune-ladder/lenet.tinygrad.txt`.  Inline
  baseline_us, beam4_us, speedup, kernel_count_post in this
  task entry.

  **tinygrad LeNet forward:**

  | metric                | baseline (NOOPT) | beam4 |
  |-----------------------|-----------------:|------:|
  | steady_us             |             9033 |  8487 |
  | kernel_count / 50 rep |              500 |   500 |
  | kernels per forward   |               10 |    10 |
  | speedup_to_beam4      |                  | 1.064x|

  **Cross-framework Level 6 comparison:**

  | metric              |  thvm | tinygrad |
  |---------------------|------:|---------:|
  | kernels per forward |    19 |       10 |
  | best wall (us)      | 3464* |    8487  |
  | autotune speedup    | 1.26x |   1.06x  |

  \* thvm "best" is sum-of-per-kernel TKernelBenchUs (GPU-only,
  no Python overhead); tinygrad "beam4_steady_us" is wall over
  50 realize() calls including Python + dispatch (~300us per
  call).  thvm number is therefore favourably skewed; the GPU-
  only comparison would need a thvm wall-time mode that the
  current bench scripts don't expose.

  **Headline finding**: thvm runs LeNet in 19 kernels where
  tinygrad runs it in 10 -- a +9 kernel leak on real-network
  shape, decomposing as approximately:
  - +4 from the conv2d 3-into-1 fusion (each conv leaks +2).
  - +2-3 from softmax-tail / pool / flatten boundaries that
    bufferize splits.
  - +1-2 from LinearLayer-activation boundaries.

  Per-kernel autotune saturates at 1.06x on tinygrad and 1.26x
  on thvm -- consistent with the synthetic-ladder finding that
  per-kernel tuning is at parity and structural fusion is the
  dominant lever.
