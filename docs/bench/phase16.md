# Phase 16 -- kernel-variant codegen scaffold

End-to-end pipeline for shape-heuristic kernel optimization:
proposer suggests TOpts, autotune benches each, JIT cache keys
fold opts so distinct dylibs get built, and per-shape schedule
sharing means a winning opt auto-applies across kernels with the same
structural key.  Optional fire-time auto-trigger via
`THVM_AUTOTUNE=1`.

## Architecture

C-side owns the optimization passes; WL is a thin LibraryLink
wrapper with summary boxes for every typed object.  The scaffold:

```
TKernelProposed[kid]
  -> kernel_opts_propose(ke)            heuristic candidate generator
  -> {TOpt[op, axis, arg], ...}

TKernelAutotune[kid]                    bench-and-pick the winner
  -> kernel_autotune(ke)                applies winning opt to shared axes

TKernelAutotuneAll[]                    sweep every live kid (one-shot pre-warm)

TKernelProgramKey[kid]                  structural key for the shared
                                        KernelAxes schedule slot:
                                        KProgOp[] cache key for legacy
                                        programs, scalar graph key for
                                        rangeified/tile kernels, or 0
                                        when no shared axes entry exists

TKernelAutotuneUnique[]                 group proposer candidates by
                                        nonzero TKernelProgramKey and tune
                                        one representative per shared
                                        schedule shape

TKernelVariants[kid]                    inspect-only: bench all, leave baseline
  -> kernel_bench_variants(ke, ...)
  -> {TKernelVariant[<|"Kid","Opt","WallUs"|>], ...}

THVM_AUTOTUNE=1                         fire-time trigger -- first dispatch of
                                        each program shape pays the bench
                                        cost; iter 2+ runs the winning variant
                                        without recording benchmark fires into
                                        TJit captures

THVM_AUTOTUNE_RUNS=N                    timed dispatches per candidate
                                        (default 5; min wallclock wins)

THVM_AUTOTUNE_CACHE_DIR=path            optional on-disk winner cache
                                        root; default is
                                        $XDG_CACHE_HOME/thvm/autotune
                                        or $HOME/.cache/thvm/autotune

THVM_AUTOTUNE_CACHE=0 or
THVM_AUTOTUNE_DISABLE_CACHE=1           force fresh benchmarking
```

All opts pass through C-side `KernelAxes`.  For KProg kernels, axes
live on the `KpCacheSlot`.  For rangeified/tile or axes-only kernels,
axes live on a parallel structural schedule cache.  Apply once ->
propagates to every kid sharing that structural key.  Mutated by
`axes_apply_opt`; read by renderers and JIT hashes so distinct opt
plans build distinct variants.

## Opt classes (so far)

| Opt        | Codegen                                    | Proposer rule                                   |
|------------|--------------------------------------------|-------------------------------------------------|
| `UNROLL`   | `#pragma clang loop unroll_count(N)` over inner k-loop (reduce-tail kernels) | factors {2, 4, 8, 16} where reduce_axis_size divisible |
| `UPCAST`   | `#pragma clang loop unroll_count(N)` over per-output for-loop (elementwise) | factors {2, 4, 8, 16} where selected output axis divisible |
| `SWAP`     | axis-order rewrite in `KernelAxes` (no codegen consumer yet -- our flat emit ignores axis order) | not proposed |
| `LOCAL`    | split output axis for tile/Metal local-thread binding | not proposed |
| `GLOBAL`   | mark a full LOOP axis for tile/Metal grid binding; pairs with `LOCAL` as `GLOBAL x LOCAL` | not proposed |
| `GROUP` / `GROUPTOP` | split reduce axis to `GROUP_REDUCE` schedule metadata | not proposed |
| `TC`       | Metal GEMM/MMA tile-size metadata for recognized f32 `TILE_MMA` plans | factors {32, 16, 8} on Metal f32 GEMM |
| `PADTO` / `NOLOCALS` | reserved and rejected until a renderer consumes them | not proposed |

## Verified end-to-end (smoke)

- 4 candidates proposed for axis_size=32 SUM.
- `TKernelApplyOpt` writes through to C; source picks up the
  pragma; JIT cache key produces a distinct dylib hash.
- Per-program-shape sharing: 3 SUM realizes -> 3 kids, 1 cached
  KProgOp[]; opt on kid_1 surfaces on kid_2, kid_3 automatically.
- `TKernelProgramKey` exposes the shared structural schedule key to
  WL for both KProg kernels and rangeified/tile kernels.
  `TKernelAutotuneUnique[]` uses nonzero keys to avoid
  re-benchmarking duplicate live kids.  Zero-key kernels are left
  separate because no shared `KernelAxes` slot exists for them.
- Autotune on SUM(x*y + 0.5*x) at 16384 elems picks UPCAST=4;
  source contains `#pragma clang loop unroll_count(4)`;
  computed value matches baseline to f32 tolerance.
- `THVM_AUTOTUNE=1` causes first fire of each program shape to
  dispatch `(1 + candidates * (1 + THVM_AUTOTUNE_RUNS))` times
  instead of 1; with the default five timed runs and five candidates
  this is 31 dispatches.  Winner persists in shared axes.  If this
  happens inside a `TJit` capture, those internal benchmark dispatches
  are suppressed from the capture; replay contains only the user's
  real kernel sequence.
- `TKernelVariants[kid]` returns 5 entries (1 baseline + 4
  candidates) with measured WallUs each, leaves axes at baseline.
- `TKernelAutotune` persists winners on disk keyed by backend,
  structural program shape, tensor shapes/dtypes, candidate list, and
  `THVM_AUTOTUNE_RUNS`.  The regression in `kernel_opts.wlt`
  confirms a second runtime session replays the cached winner without
  running benchmark dispatches.
- Summary boxes render for `TOpt`, `TKernelOpts`, `TKernelVariant`.
- LeNet/Adam CPU training works end to end.  `N_STEPS=4
  train.wls` produced `{2.6071, 1.8054, 1.1324, 0.6546, 0.3559}`
  on May 2, 2026.
- A bounded CPU LeNet A/B benchmark now runs:
  `TRAIN_BENCH_MODE=both N_STEPS=1 WARMUP_STEPS=1
  MAX_TUNE_KERNELS=3 THVM_TILE=1 bench-train.wls` saw 1543 live
  candidates collapse to 68 representative schedule keys, tuned 3
  reps in 0.4 ms, and measured 33.6s baseline vs 29.2s autotune
  for the timed step.  No candidate beat baseline in that tiny
  bounded sample; the speedup is within benchmark noise until the
  heavier opt classes land.
- The old `Conv2D + ReLU + MaxPool2d` weight-gradient shape bug is
  fixed in the current tree; the repro returns `{2, 1, 3, 3}`.
- Metal primitive coverage is green on Apple M3 Max:
  `test_metal_real` 218/218 and `metal_dtypes.wlt` 7/7.
- Metal LeNet forward, full input-gradient smoke, and Adam training
  pass.  `THVM_BACKEND=metal THVM_TILE=1 N_STEPS=4 train.wls`
  produced `{2.6071, 1.8054, 1.1324, 0.6546, 0.3559}` with no
  buffer-table-full warnings on May 2, 2026.
- Metal dispatch now has a scoped command-buffer batch path around
  `TRealize` and `TJit` replay (`THVM_METAL_BATCH=0` disables it),
  plus backend-native `buf_copy` so `ASSIGN` can use a Metal blit
  instead of forcing a host read/write pair.
- Metal f32 matmul kernels now have a direct `metal-gemm` route fed by
  a validated `TILE_MMA` plan.  The dispatch reads the original A/B
  buffers instead of pre-materializing their expanded `{M,K,N}` views,
  and now runs a fixed 16x16 threadgroup-memory tiled shader.  The
  `MUL + REDUCE_SUM` recognition lives in the tile scheduler rather
  than the Metal backend.  Focused coverage confirms `TMatMul`
  dispatches as `metal-gemm` and exposes `TILE_MMA` metadata.
- Metal GEMM autotune now has a first real `TC` knob: recognized f32
  GEMM kernels propose 32/16/8 tile sizes, and `metal-gemm` compiles
  one threadgroup-memory shader per selected tile size.
  `wl/Examples/metal-gemm-autotune.wls` is the focused harness for
  inspecting these variants without the LeNet graph in the way.
- Metal fire-time autotune is wired for supported tile kernels:
  `LOCAL` proposals are expanded internally to `LOCAL + GLOBAL`, and
  impossible tile variants with more than 30 input buffers are rejected
  before MSL compilation.

## Current Measurement Plan

1. **CPU LeNet baseline vs bounded autotune.**  Run
   `bench-train.wls` with `TRAIN_BENCH_MODE=both`, `N_STEPS=2`,
   `WARMUP_STEPS=1`, and bounded `MAX_TUNE_KERNELS` first.  This is
   now a valid correctness/perf comparison because baseline training
   and gradients pass.
2. **Representative tuning coverage.**  Track live proposer
   candidates, `TKernelProgramKey` groups, and selected reps.  The
   rangeified/axes-only schedule cache should prevent repeated
   training iterations from benchmarking identical schedule shapes.
3. **Metal correctness and autotune smoke.**  Run
   `THVM_BACKEND=metal THVM_TILE=1 N_STEPS=4 train.wls` for the clean
   training baseline, then `THVM_BACKEND=metal THVM_TILE=1
   THVM_AUTOTUNE=1 N_STEPS=1 train.wls` to exercise fire-time tuning
   without post-realize kernel stripping.
4. **Post-hoc inspection.**  Metal-output kernels now stay
   inspectable after `TRealize`; kernel GC skips non-CPU backends
   because its liveness signal is CPU-buffer refcounts.  Use
   `THVM_KGC=0` for CPU-only post-hoc inspection when you need old
   kernels' program arrays after their output buffers were released.
5. **Autotune value.**  Metal now has real `LOCAL/GLOBAL` tile
   candidates for rank-1 f32 scalar/tile kernels and MSL `UNROLL` for
   supported reduce-tail JIT kernels.  The direct `metal-gemm` path
   now consumes the introspectable `TILE_MMA` plan root with
   M/N/K/layout metadata and can autotune fixed 32/16/8 tiled
   threadgroup-memory shader variants.  Full
   beautiful-mnist performance still needs the next structural fix:
   avoiding PAD-and-sum im2col materialization, then structured
   multi-axis tile rendering, reductions over local memory, and
   eventually autotuned `TILE_MMA`/simdgroup rendering.

For tiny isolated kernels the autotune correctly reports either
"no opt beat baseline" (when clang's `-O2` already vectorises)
or picks a small-factor UNROLL/UPCAST that nudges wallclock by
~1-3%.  Real wins need bigger compute kernels (e.g., matmul,
conv) where unrolling matters more, plus more opt classes
(UPCAST output axis with structured nest emit, broader LOCAL/GLOBAL
proposer coverage on Metal).

May 2, 2026 beautiful-mnist forward canary on Apple M3 Max:

- Generic Metal tile path, no autotune:
  `144.4ms / 66.9ms / 66.0ms`.
- Generic Metal tile path, fire-time autotune with a fresh disk cache:
  first process `1802.0ms / 63.5ms / 62.6ms`; second process using
  the same cache `1799.8ms / 64.1ms / 65.5ms`.  The cache files are
  produced and replayed, but the first-sample wall time is still
  dominated by WL/TJit/materialization and selected MSL compilation,
  not just benchmark rediscovery.
- Diagnostic `THVM_METAL_SPECIALIZED=1` oracle:
  `58.6ms / 8.2ms / 10.4ms`.

Conclusion: persistent autotune cache is useful plumbing for repeated
experiments, but it does not close the kernel-throughput gap.  The
steady generic path remains roughly `64ms` versus `~9ms` for the
diagnostic oracle, so the next real win is still generated conv/reduce
schedule quality, not more decision-cache work.

## Files added

- `src/codegen/axis.c`         -- `KernelAxes` default constructor
- `src/codegen/apply_opt.c`    -- `axes_apply_opt` (UPCAST/UNROLL/SWAP/...)
- `src/codegen/propose.c`      -- `kernel_opts_propose` (UNROLL + UPCAST rules)
- `src/codegen/autotune.c`     -- `kernel_autotune`, `kernel_bench_us`,
                                  `kernel_bench_variants`,
                                  `kernel_should_autotune` (env-gated)
- `wl/THVMLink/Tests/kernel_opts.wlt` -- 25+ regression tests

## Files modified

- `src/thvm.h`                          -- `KAxisType`, `KOpt`,
                                          `KernelAxes`, `KOP_*`, fwd decls
- `src/schedule/kernel_program_cache.c` -- `KpCacheSlot.axes` field +
                                          `*_lookup_slot` / `*_insert_slot`
- `src/schedule/materialize.c`          -- point `ke->axes` at slot's axes
- `src/schedule/realize.c`              -- TEN-input short-circuit
- `src/codegen/cg.c`                    -- pass `unroll_factor` to renderers
- `src/codegen/render_c.c` / `render_metal.c` -- emit pragma when factor > 1
- `src/backend/cpu/jit.c`               -- fold `applied_opts[]` into hash
- `src/interact/uop_kernel.c`           -- fire-time autotune trigger
- `wl/THVMLink/CSource/thvmlink.c`      -- LibraryLink wrappers
- `wl/THVMLink/Kernel/THVMLink.wl`      -- loaders
- `wl/THVMLink/Kernel/Kernel.wl`        -- thin WL surface
- `wl/THVMLink/Kernel/Format.wl`        -- summary boxes
- `wl/THVMLink/Kernel/README.md`        -- ownership matrix updated

## Stack trace (commits, oldest first)

- `8206ee5` -- TOpt/TKernelOpts WL surface + summary boxes (scaffold)
- `67544cf` -- `cg_emit` consumes axes; UNROLL pragma; JIT key folds opts
- `a465ac0` -- 16 kernel_opts.wlt tests
- `2e04c95` -- per-program-shape sharing via `KpCacheSlot.axes`
- `fc6a4c7` -- `TRealize` short-circuits on TEN input
- `21e32a1` -- `kernel_opts_propose` + `kernel_autotune` +
              `TKernelProposed` / `TKernelAutotune`
- `9611f3d` -- UPCAST elementwise opt class
- `f3b3860` -- fire-time autotune trigger via `THVM_AUTOTUNE=1`
- `24e4055` -- `TKernelAutotuneAll[]`
- `7b91ffd` -- `kernel_bench_variants` + `TKernelVariant` + summary box

WL grid: 422/422 throughout (no opts applied -> emit unchanged
from pre-Phase-16; autotune is opt-in).
