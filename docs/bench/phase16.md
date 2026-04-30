# Phase 16 -- kernel-variant codegen scaffold

End-to-end pipeline for shape-heuristic kernel optimization:
proposer suggests TOpts, autotune benches each, JIT cache keys
fold opts so distinct dylibs get built, per-program-shape sharing
means a winning opt auto-applies across all kernels with the same
KProgOp[].  Optional fire-time auto-trigger via `THVM_AUTOTUNE=1`.

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

TKernelVariants[kid]                    inspect-only: bench all, leave baseline
  -> kernel_bench_variants(ke, ...)
  -> {TKernelVariant[<|"Kid","Opt","WallUs"|>], ...}

THVM_AUTOTUNE=1                         fire-time trigger -- first dispatch of
                                        each program shape pays the bench
                                        cost; iter 2+ runs the winning variant
```

All opts pass through C-side `KernelAxes` which lives on the
shared `KpCacheSlot` (per-program-shape).  Apply once -> propagates
to every other kid sharing that program.  Mutated by
`axes_apply_opt`; read by `cg_emit` (renderer pragma) and by
`cpu_jit_hash` (distinct dylib per (program, opts) pair).

## Opt classes (so far)

| Opt        | Codegen                                    | Proposer rule                                   |
|------------|--------------------------------------------|-------------------------------------------------|
| `UNROLL`   | `#pragma clang loop unroll_count(N)` over inner k-loop (reduce-tail kernels) | factors {2, 4, 8, 16} where reduce_axis_size divisible |
| `UPCAST`   | `#pragma clang loop unroll_count(N)` over per-output for-loop (elementwise) | factors {2, 4, 8, 16} where output_numel divisible    |
| `SWAP`     | axis-order rewrite in `KernelAxes` (no codegen consumer yet -- our flat emit ignores axis order) | not proposed |
| `LOCAL` / `GLOBAL` / `GROUP_REDUCE` / `PADTO` / `NOLOCALS` / `TC` | recorded only -- codegen support pending (Metal threadgroup binding, etc.) | not proposed |

## Verified end-to-end (smoke)

- 4 candidates proposed for axis_size=32 SUM.
- `TKernelApplyOpt` writes through to C; source picks up the
  pragma; JIT cache key produces a distinct dylib hash.
- Per-program-shape sharing: 3 SUM realizes -> 3 kids, 1 cached
  KProgOp[]; opt on kid_1 surfaces on kid_2, kid_3 automatically.
- Autotune on SUM(x*y + 0.5*x) at 16384 elems picks UPCAST=4;
  source contains `#pragma clang loop unroll_count(4)`;
  computed value matches baseline to f32 tolerance.
- `THVM_AUTOTUNE=1` causes first fire of each program shape to
  dispatch 31 times (1 init + 5 candidates x (1 JIT-warm + 5
  timed)) instead of 1.  Winner persists in shared axes.
- `TKernelVariants[kid]` returns 5 entries (1 baseline + 4
  candidates) with measured WallUs each, leaves axes at baseline.
- Summary boxes render for `TOpt`, `TKernelOpts`, `TKernelVariant`.

## What's NOT (yet) measured

End-to-end LeNet/Adam train.wls A/B (baseline vs autotune-all)
was the goal but ran into two unrelated infrastructure issues:

1. **LeNet per-realize allocation > 32M cells** (Cheney semi-
   space cap = HEAP_CAP/2).  Even N_STEPS=1 exhausts.  This is
   a pre-existing limit -- Phase 14's "lenet train converges"
   target was deferred for the same reason.  Fixing it needs
   either (a) larger HEAP_CAP, (b) GC during a single realize
   (not just at the boundary), or (c) the per-grad memo +
   chain-rule sharing pass that Phase 14 listed under "what's
   still slow".
2. **WL bench harnesses for the smaller training examples
   (linear-train, mlp-mnist) need the per-iter timing to land
   inside the recursive `TPriForce` callback** (the `TWnf` of
   the outer loop completes near-instantly because the callback
   doesn't force a fresh realize).

Both are outside Phase 16's scope (one is heap/GC, the other is
bench infrastructure).  Real perf delta on a training loop
remains the next milestone, and unblocks once the LeNet heap
issue is addressed.

For tiny isolated kernels the autotune correctly reports either
"no opt beat baseline" (when clang's `-O2` already vectorises)
or picks a small-factor UNROLL/UPCAST that nudges wallclock by
~1-3%.  Real wins need bigger compute kernels (e.g., matmul,
conv) where unrolling matters more, plus more opt classes
(UPCAST output axis with structured nest emit, LOCAL/GLOBAL on
Metal).

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
