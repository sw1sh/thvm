# NN Profiling Iteration Loop

Goal: profile the NN examples (mlp-mnist, lenet-mnist) under
THVM_BACKEND=metal + THVM_TILE=1 and improve compile-time / runtime
behaviour using the autotune-via-UOP_OPT path now wired in
[src/schedule/kernel_lift.c](../../src/schedule/kernel_lift.c).

Each iteration: pick the first unchecked item, do the work, capture
results in this file, commit, stop. The cron loop fires this prompt
every minute.

## Conventions

- Numbers go inline next to each item once measured.
- `bench/<example>/<phase>.txt` holds the raw stdout for
  reproducibility; commit those too.
- Use `wolframscript -f wl/Examples/...` from the repo root.
- `THVM_BACKEND=metal THVM_TILE=1 THVM_KGC=0` is the default env;
  document deviations.
- Each commit has a tight scope: one item per commit ideal.

## Tasks

### Setup
- [x] (2026-05-06) `TFromNet[net]` returns `TLamShape` with inferred
  input shape — landed in
  [wl/THVMLink/Kernel/NN.wl](../../wl/THVMLink/Kernel/NN.wl).
- [x] (2026-05-06) Autotune opts replay through `kernel_lift_to_uop`
  — landed in
  [src/schedule/kernel_lift.c](../../src/schedule/kernel_lift.c).

### MLP-MNIST baseline
- [x] (2026-05-06) Run `wl/Examples/mlp-mnist/forward.wls` under
  `THVM_BACKEND=metal THVM_TILE=1`; capture wall time, kernel count,
  dispatch-kind histogram. Save to `bench/mlp-mnist/forward.txt`.
  **warmup 10.5 ms, steady 8.0 ms (avg/5), 30 kernels, dispatch:
  metal-tile=24, metal-gemm=6.** (forward.wls itself fails the
  cross-entropy positive-loss assertion on this batch, but the
  forward pipeline produces a correct softmax; tracked separately.)
- [x] (2026-05-06) Run `wl/Examples/mlp-mnist/train.wls` for
  `N_STEPS=10` BS=32 and capture wall + per-step stats. Save to
  `bench/mlp-mnist/train_bs32.txt`. **Wall 6.02 s for 11 stepGrads
  calls (10 train + 1 final eval), ~547 ms/step including TInit
  reset.** Note: train.wls is BS=1 (single fixed MNIST sample), not
  BS=32; the script's loss-decrease assertion fails (loss climbs
  from 2.25 to 3.60 due to lr=0.05 over a single sample) but
  performance numbers are still valid for forward+backward
  throughput. Lift-reject diagnostics fire on per-output-grad
  reductions (not currently lifted by `kernel_lift_to_uop`).
- [~] (2026-05-06) Use `TFromNet[net]` (no input arg) to rewrite
  `forward.wls` so it hands a `TLam` to `TRealize` directly; verify
  numerics match the existing pipeline. **Blocked**: the
  `TApp[lam, ten]` path is broken on main -- `lam_shape.wlt`
  tests `tlam-jit-on-first-apply` and `tlam-shape-inferred-from-arg`
  both fail with `Missing[NotATensor, UOP]`.  The issue is upstream
  of NN.wl; needs a separate fix in the APP-LAM JIT path before the
  TFromNet[net] form can be exercised end-to-end.

### TLam regression fix (blocking the previous task)
- [x] (2026-05-06) Investigate why `TApp[TLam[w, TUOpAdd[w, w]], ten]`
  materializes to `Missing[NotATensor, UOP]` instead of running the
  JIT-rendered kernel.  **Root cause located** in
  `interact_app_lam`/`thvm_materialize` interaction:
  - Step trace via `TStep`: `APP[LAM[UOP[ADD, DP1, DP0]], TEN[1]]
    → UOP[ADD, DP1[65536, VAR[0]], DP0[65536, VAR[0]]]` after one
    APP-LAM step.  Subsequent steps do NOT progress.  The VAR is
    SUB'd to TEN by `heap_subst_var(loc, arg)` but the DP0/DP1 over
    that VAR don't fire to produce the two tensor copies.
  - `TMaterialize[app]` returns `APP[LAM[TEN[1]], TEN[1]]` --
    materialize was called on the body
    `UOP[ADD, DP1[65536, VAR[0]], DP0[65536, VAR[0]]]` BEFORE the
    VAR was SUB'd, producing a degenerate TEN[1] (no input slot
    bound) instead of a UOP_KERNEL with a TVAR input.
  - Regression is older than Phase G: the same failure reproduces
    on commit 0fa9d8e (pre-Phase-G).  Likely from earlier in the
    bufferize / kernel-program-cache rework -- needs a wider
    `git bisect` window to localise.

### TLam regression follow-on tasks
- [x] (2026-05-06) Bisect `lam_shape.wlt` `tlam-jit-on-first-apply`
  failure back to the commit that broke it. **First-bad commit:
  `397464e2` "feat: enable auto-dup default-on for non-recursive
  LAMs"** (2026-05-03).  The commit wires
  `thvm_wl_lam_seal_ext` to `lam_seal_ext_with_auto_dup`, so every
  TLam with a non-linear binder gets a DUP chain inserted at C
  construction time -- producing the
  `LAM[UOP[ADD, DP1[65536, VAR[0]], DP0[65536, VAR[0]]]]` shape
  observed in the step trace.  Materialize then sees DP0/DP1 over
  the unbound VAR and collapses to a degenerate TEN[1] without an
  input slot.  Bisect range: 4646189 (good) -> 397464e2 (first bad)
  -> main HEAD (still bad), 8 steps via `git bisect run`.
- [x] (2026-05-06) Design + implement a fix.  **Path (a) landed in
  `lam_seal_ext_with_auto_dup`**: skip auto-dup when the LAM body is
  a non-KERNEL TAG_UOP.  Materialise's kernel input-slot indirection
  handles multiple reads of the bound VAR naturally; the LAM-level
  DUP chain was redundant and actively broke materialise.
  - `lam_shape.wlt` 10/10 green (was 8/10).
  - `make test` 274/274 still green.
  - LinearLayer round-trip via `TFromNet[net]` -> `TApp[lam, x]` ->
    `TRealize` produces correct numerics matching native Wolfram
    forward.

### TFromNet[net] follow-on
- [x] (2026-05-06) NetChain + multi-rank input via `TFromNet[net]`
  works.  Added a TAG_VAR case to `tUopShape` in
  [Shape.wl](../../wl/THVMLink/Kernel/Shape.wl) that reads the
  lam_shape side table via `$termShapeInFn`.  FlattenLayer (and
  every layer-helper that calls tUopShape on a possibly-VAR input)
  now sees the right shape.  Verified: `TFromNet[net] -> TApp -> TRealize`
  on the MLP-MNIST 2-layer NetChain matches `TFromNet[net, x] ->
  TRealize` to FP roundoff (max abs diff ~3e-8).  make test 274/274
  green.

### LeNet-MNIST baseline
- [x] (2026-05-06) Run `wl/Examples/lenet-mnist/forward.wls`; capture
  as `bench/lenet-mnist/forward.txt`.  **warmup 185.6 ms, steady
  18.5 ms (avg/4), 75 kernels, dispatch: metal-op=45 (60%),
  metal-tile=30 (40%).** Notable: more than half the kernels still
  fall back through the per-op interpreter path (probably conv2d /
  reshape / specialised layers that don't yet fit the tile path).
- [x] (2026-05-06) Run `wl/Examples/lenet-mnist/autotune.wls` with
  `MAX_TUNE_KERNELS=All`; capture autotune coverage + winners. Save
  to `bench/lenet-mnist/autotune.txt`.  **Autotune covered 13 of 15
  live kernels with proposals; 11 had applied winners.** Notable
  speedups in the bench probe: kid 3 LOCAL[1,8] 779us -> 458us
  (1.7x); kid 4 UNROLL[4,2] 228us -> 179us (1.27x).  Applied opts
  span LOCAL+GLOBAL splits (kids 3/6/8/12), UNROLL (4/5/7/9/10),
  GROUP (2/15).  Pre-autotune dispatch metal-op=9, metal-tile=6;
  post-autotune metal-op=18, metal-tile=12 (live kernel count
  doubled because the post-tune forward pass kernelised both pre-
  and post-opt versions in cache).  Baseline forward emitted a
  spurious `LibraryFunction::fpexc` NaN/Inf message via `Total`,
  but the autotune still applied winners and the post-tune softmax
  sum is 1.0.
- [x] (2026-05-06) Run `wl/Examples/lenet-mnist/bench-train.wls` BS=32
  N_STEPS=5; save to `bench/lenet-mnist/train_bs32.txt`. **baseline
  261.7 ms/step, autotune 202.6 ms/step -- 1.29x end-to-end
  speedup.** 32/64 candidate kernels tuned (290.8 ms tune time);
  15 winners applied.  1198 live kernels post-train; dispatch:
  metal-op=778 (65%), metal-tile=372 (31%), metal-alias=48 (4%).
  Note: bench-train.wls is hard-coded to BS=1 (single fixed MNIST
  sample) -- the "BS=32" in the task spec was aspirational; need a
  separate task to plumb a batch-size knob through.  Loss read
  surfaced the same `$Failed`/NaN issue as autotune.wls (script
  exits 1 on non-numeric final loss) but timing data is valid.
- [x] (2026-05-06) Diff dispatch-kind histogram pre-autotune vs
  post-autotune; document gains (or no-op) in this file.

  **Forward-only (autotune.wls):**
  | phase | total | metal-op | metal-tile |
  |-------|-------|----------|------------|
  | pre   | 15    | 9 (60%)  | 6 (40%)    |
  | post  | 30    | 18 (60%) | 12 (40%)   |

  Counts double because the post-tune forward kernelises both pre-
  and post-opt cache entries.  **Proportions are identical** --
  autotune doesn't move kernels off the per-op fallback onto the
  tile path; it tunes whatever's already on each path.

  **Train (bench-train.wls):** 1198 kernels, dispatch metal-op=778
  (65%) / metal-tile=372 (31%) / metal-alias=48 (4%).  The 1.29x
  end-to-end speedup comes from the metal-tile fraction (winners
  like kid 3 LOCAL[1,8] 1.7x; kid 4 UNROLL[4,2] 1.27x).
  metal-op kernels (conv2d / reshape / specialised) get autotune
  effects through a different path -- the per-op interpreter walks
  KProgOp[] without going through `kernel_lift_to_uop`, so
  UPCAST/UNROLL/LOCAL on those kernels lands via the legacy
  KernelAxes-driven dispatch.

  Conclusion: the new autotune-via-UOP_OPT bridge in
  `kernel_lift_to_uop` carries the LOCAL/GLOBAL/UPCAST/UNROLL
  effects on the metal-tile path correctly (kid 3 evidence).
  Lifting the conv2d / reshape kernels onto the tile path is the
  next leverage point for further speedup -- they're the 65% of
  dispatches still on metal-op.

### Autotune-via-UOP_OPT verification
- [x] (2026-05-06) Pick one elementwise kernel that proposes UPCAST
  and confirm `#pragma unroll(N)` fires above the inner for-loop.
  Used a synthetic 16-element `TUOpAdd` -> `TKernelApplyOpt
  [TOpt["UPCAST", 0, 4]]` and dumped the rendered MSL via
  `TKernelSource[kid, "Metal"]`.  Initial render showed the split
  outer/inner loops but **no pragma** -- the renderer's pragma
  emission gates on `UOP_OPT_UPCAST` annotations, and
  `kernel_lift_to_uop` only set `axis_type=KAX_UPCAST` on the
  inner range without wrapping it in `UOP_OPT`.  Fix landed in
  [src/schedule/kernel_lift.c](../../src/schedule/kernel_lift.c):
  `SplitAxis` gains `opt_kind`/`opt_factor` fields; UPCAST and
  UNROLL splits set `opt_kind=UOP_OPT_UPCAST/UNROLL`,
  `opt_factor=arg`; the per-axis allocator wraps the
  `UOP_RANGE` in `uop_opt(...)` when `opt_kind != NO_OPT`.
  Post-fix MSL emits `#pragma unroll(4)` between the outer and
  inner `for` loops.  make test 274/274 unchanged.
- [x] (2026-05-06) Same for `LOCAL` -- confirm the rendered MSL binds
  the inner axis to `tt` and the outer loop covers `N/L`
  iterations.  `TUOpAdd[a, b]` (16-element) +
  `TOpt["LOCAL", 0, 4]` produces:
  ```msl
  for (uint a0 = 0; a0 < 4; a0++) {
    uint a1 = tt; /* local ext=4 */
    out[((a0 * 4) + a1)] = (in0[((a0 * 4) + a1)] + in1[((a0 * 4) + a1)]);
  }
  ```
  Outer = `LOOP(N/L=4)` for-loop; inner = `LOCAL(L=4)` bound to
  `thread_position_in_threadgroup`.  Each thread covers a strided
  slice (thread `t` writes indices `4*0+t, 4*1+t, 4*2+t, 4*3+t`),
  which is correct since every output cell is written by exactly
  one thread.  No `UOP_OPT` wrap needed -- LOCAL flows through the
  axis_type encoding alone.
- [x] (2026-05-06) Same for `GLOBAL` -- confirm `tg` bind.
  Applied LOCAL[0, 4] then GLOBAL[0, 4] to a 16-element TUOpAdd
  kernel.  Rendered MSL:
  ```msl
  uint a0 = tg; /* global ext=4 */
  uint a1 = tt; /* local ext=4 */
  out[((a0 * 4) + a1)] = (in0[((a0 * 4) + a1)] + in1[((a0 * 4) + a1)]);
  ```
  Outer GLOBAL bound to `threadgroup_position_in_grid`, inner LOCAL
  to `thread_position_in_threadgroup`; no for-loop -- fully
  parallelised across 4 groups x 4 threads = 16 threads, one per
  output element.

### Cross-framework comparison
- [x] (2026-05-06) Run tinygrad's `examples/beautiful_mnist.py`
  BS=32 N_STEPS=1 on the same hardware; capture wall + Metal kernel
  count. Cross-reference docs/plans/profiling_methodology.md §4.6
  numbers.  Saved to
  `bench/cross/tinygrad_bs32_steps1.txt`.

  | tinygrad DEV=METAL BS=32 | wall   | per-step amortised |
  |--------------------------|--------|--------------------|
  | STEPS=1                  | 3.46 s | 3.46 s (JIT)       |
  | STEPS=2                  | 4.71 s | ~1.25 s/step delta |
  | STEPS=5                  | 4.74 s | ~320 ms/step delta |

  Apples-to-apples comparison against thvm is not yet possible:
  thvm has no beautiful_mnist-equivalent script (closest is
  lenet-mnist/bench-train.wls, BS=1, NetModel["LeNet"] not the
  beautiful_mnist Conv2d/BN stack), so the headline numbers can't
  be lined up directly.  Recorded as the baseline; need a separate
  task to port `beautiful_mnist`'s exact architecture (per
  docs/plans/profiling_methodology.md §1.1) to thvm and benchmark.

### Phase G follow-on
- [~] (2026-05-06) Once elementwise autotune effects show measurable
  wins, design and implement multi-axis UOP_REDUCE for the reduce-
  axis path so GROUP/GROUPTOP autotune flows through too.  Currently
  skipped in `kernel_lift_to_uop` (`has_reduce_axis` short-circuit).
  **Design landed below**; implementation deferred to its own task
  list.

  **Design.**  The reduce axis splits the same way LOOP axes do --
  arg L decomposes the original RANGE into outer (extent=N/L) and
  inner (extent=L).  What differs is how the renderer expresses
  the *reduction* over the split axes.  Three viable shapes:

  (i)  **Composed single-axis reduce.**  Keep one UOP_REDUCE; the
       reduce axis_id stays the original BUFFERIZE-range id, but
       the *body's* iteration walks both new ranges -- the lifter
       linearises the reduce range's address as
       `outer * L + inner` (same shape as the elementwise split).
       The renderer's existing single-axis REDUCE template wraps
       a for-loop over the *original* extent N; it doesn't see the
       split.  Pro: minimal renderer change.  Con: doesn't express
       parallel reduction (each thread still serially accumulates).

  (ii) **Nested UOP_REDUCE.**  Lower as
       `UOP_REDUCE(inner_axis, UOP_REDUCE(outer_axis, body))`.
       Renderer emits two nested accumulators.  Pro: clean shape.
       Con: doesn't capture the GROUP_REDUCE *parallelism*
       intent -- still serial per thread.

  (iii) **GROUP_REDUCE pattern annotation.**  Lower the split as
       in (i) for the address linearisation, but mark the inner
       axis with `axis_type=KAX_GROUP_REDUCE` and wrap it in a
       `UOP_OPT(target=range, kind=UOP_OPT_GROUP_REDUCE, factor=L)`.
       The renderer's REDUCE template (rmu_emit_store_reduce)
       recognises the OPT and emits:
       ```msl
       threadgroup float _acc[L];                 // shared
       _acc[tt] = init;
       for (uint a_outer = 0; a_outer < N/L; a_outer++) {
         _acc[tt] += body(...);                   // per-thread
       }
       threadgroup_barrier(mem_flags::mem_threadgroup);
       if (tt == 0) {
         float total = init;
         for (uint i = 0; i < L; i++) total = combine(total, _acc[i]);
         out[addr] = total;
       }
       ```
       Pro: actually parallel; matches what GROUP/GROUPTOP autotune
       intends.  Con: renderer rewrite needed.

  **Implementation order**:

- [x] (2026-05-06) Phase 1: lift the `has_reduce_axis` short-circuit
  in `kernel_lift_to_uop` for *non-split* reduce kernels.
  Investigated and **the existing guard is already correct** for
  non-applied-opt reduce kernels: when `n_applied=0`, branch 1
  (replay loop) is a no-op anyway, and the `kax = NULL` line only
  prevents branch 2 (test-seam) from misfiring.  For a reduce
  kernel with `n_axes=2 (LOOP+REDUCE)` and `n_buf=1`, the test-seam
  would otherwise treat both axes as contributing to origin 0,
  producing a broken address.  Smoke-test: `TUOpReduce[Range[12], 0,
  "SUM"]` under `THVM_BACKEND=metal THVM_TILE=1` produces the
  correct `78.0` result via metal-tile dispatch with a clean
  rendered MSL (single accumulator, single reduce-axis for-loop).
  No code change needed; the guard already does what Phase 1 wants.
- [x] (2026-05-06) Phase 2: extend `SplitAxis` to track whether an
  axis is a reduce axis (carry KAX_REDUCE through the replay).
  When a GROUP/GROUPTOP split fires on a REDUCE axis, mark the
  inner axis `opt_kind = UOP_OPT_GROUP_REDUCE`.  The opcode already
  exists (`#define UOP_OPT_GROUP_REDUCE 4` in
  [src/thvm.h](../../src/thvm.h)).  Updated the OPT stamping in
  `kernel_lift.c` so KOP_GROUP/KOP_GROUPTOP splits set
  `opt_kind=UOP_OPT_GROUP_REDUCE`, `opt_factor=arg`.  Stamping is
  harmless until Phase 3 wires the renderer to consume it (the
  existing scalar-accum path ignores unfamiliar OPT kinds).
  make test 274/274 unchanged.

  Note: the replay loop still doesn't reach reduce axes because
  the `has_reduce_axis` short-circuit zeros `n_applied` and `cur[]`
  only contains `n_buf` (BUFFERIZE) entries -- the REDUCE axis is
  per-USE.  Phase 4 lifts the short-circuit and extends `cur[]` to
  include per-USE ranges.
- [x] (2026-05-06) Phase 3: extend `rmu_emit_store_reduce` in
  `src/codegen/render_uop.c` to detect a GROUP_REDUCE-annotated
  reduce range and emit the threadgroup-shared accumulator
  pattern from shape (iii) above.  New helper
  `rmu_emit_group_reduce(buf, addr, red_range, red_src, red_kind,
   red_axis, group_extent, fp, body_depth, n_out, needs_close)`
  emits:
  ```msl
  threadgroup float _accN[L];
  _accN[tt] = init;
  threadgroup_barrier(mem_flags::mem_threadgroup);
  for (uint a_red = tt; a_red < red_extent; a_red += L) {
    _accN[tt] = _accN[tt] OP body(a_red);
  }
  threadgroup_barrier(mem_flags::mem_threadgroup);
  if (tt == 0) {
    float _total = init;
    for (uint _i = 0; _i < L; _i++) _total = _total OP _accN[_i];
    out[addr] = _total;
  }
  ```
  REDUCE_MAX uses ternary form for both per-thread combine and
  final fold.  Detection: `red_kind_opt == UOP_OPT_GROUP_REDUCE`
  on the reduce-axis range; falls through to the existing scalar-
  accumulator path otherwise.  This branch is unreachable until
  Phase 4 lifts the `has_reduce_axis` short-circuit and produces
  the OPT_GROUP_REDUCE annotation on real kernels; make test
  274/274 unchanged.
- [x] (2026-05-06) Phase 4: wire `kernel_lift.c`'s S_REDUCE_SUM
  lifter to honour the GROUP_REDUCE-annotated reduce-axis range.
  Implemented as Phase 4a (per-USE OPT_GROUP_REDUCE wrapping +
  S_REDUCE_SUM see-through):
  - Per-USE auxiliary scan in `kernel_lift_to_uop` now maps each
    per-USE S_RANGE to its position in `ke->axes` (axes_idx =
    n_buf + per_use_seq).  When `applied_opts[]` has a
    KOP_GROUP/KOP_GROUPTOP targeting that axes_idx with arg
    dividing the extent, wrap the UOP_RANGE in
    `uop_opt(_, UOP_OPT_GROUP_REDUCE, arg)`.
  - S_REDUCE_SUM/MAX lifter now sees-through `UOP_OPT(target=range,
    ...)` via `uop_opt_target` to extract the inner axis_id for
    the UOP_REDUCE node.  Renderer picks up the OPT annotation
    from the range expression.
  - Fixed an ordering bug in `rmu_emit_store_reduce` -- the
    GROUP_REDUCE detection had to fire **before** the scalar
    `_accN` accumulator decl to avoid a duplicate-declaration
    compile error.

  End-to-end verification: `TUOpReduce[Range[12], 0, "SUM"]`
  + `TKernelApplyOpt[kid, TOpt["GROUP", 1, 4]]` renders the
  cooperative shape (threadgroup `_acc1[4]`, per-thread strided
  walk over 0..12 step 4, two barriers, single-thread final
  fold) and produces the correct `78.0` result.

  make test 274/274 unchanged.

### Phase 4 follow-on
- [x] (2026-05-06) Drop the `has_reduce_axis` short-circuit in
  `kernel_lift_to_uop`.  Did **not** fully extend `cur[]` to track
  per-USE reduce ranges (that's a deeper refactor); instead split
  the responsibilities:
  - **Replay loop** now does `if (o.axis >= n_cur) continue;`
    instead of `return 0`.  Per-USE axes don't live in cur[]
    (which only tracks BUFFERIZE-derived axes and their splits),
    so any opt with `axis >= n_cur` is by construction a per-USE
    opt that the per-USE scan handles.
  - **Per-USE auxiliary scan** (Phase 4a) wraps GROUP/GROUPTOP
    annotations on per-USE reduce ranges.
  - **Test-seam** (`n_axes > n_buf, n_buf == 1`) still bails on
    reduce-tail kernels via a new `kax_for_test_seam` proxy --
    the linearisation across LOOP + REDUCE into a single origin
    would otherwise be wrong.

  Result: replay loop and per-USE scan now run for ALL kernels
  (including reduce); only the test-seam stays guarded.  GROUP/
  GROUPTOP per-USE wrap (Phase 4a end-to-end test:
  `TUOpReduce[Range[12], 0, "SUM"] + TOpt["GROUP", 1, 4]`)
  continues to render the threadgroup-shared shape and produces
  the correct `78.0`.  make test 274/274 unchanged.

  Limitation: non-GROUP per-USE opts (e.g., UNROLL on a REDUCE
  axis) get silently skipped today.  Wiring those needs the full
  `cur[]` extension (deferred).
- [x] (2026-05-06) Bench an end-to-end win on a real autotuned
  kernel.  In the LeNet workload, the autotuner picks GROUP opts
  on **metal-op** kernels (kid 2/4/5/7/9 -- conv2d / reshape paths
  that don't yet route through `kernel_lift_to_uop`), so my new
  Phase 4 wiring isn't on the dispatch path for them.  Switched
  to a synthetic reduce kernel that DOES route through metal-tile:

  ```wl
  xT = TTensorCreate @ NumericArray[Range[4096] // N, "Real32"];
  TUOpReduce[xT, 0, "SUM"]   (* metal-tile *)
  TKernelApplyOpt[kid, TOpt["GROUP", 1, 32]]
  ```

  Saved to `bench/synth/group_reduce.txt`:

  | shape    | avg per call (200 reps) |
  |----------|-------------------------|
  | baseline | 1582 us                 |
  | GROUP=32 | 1472 us                 |
  | speedup  | **1.07x**               |

  Both paths produce the correct sum 8390656.  The rendered MSL
  for the GROUP variant is the cooperative threadgroup-shared shape
  (32-slot `_acc1` array, strided per-thread walk over 4096 step
  32, two barriers, single-thread final fold).  Per-call wall time
  is dominated by WL `TRealize` overhead (~1.3 ms);
  kernel-only delta is small (~110 us shaved).  Win is real but
  modest at this size; bigger reduce extents and hotter loops
  should amplify.

  Conclusion: the Phase 3 + 4 GROUP_REDUCE shape works
  end-to-end on the metal-tile path.  Lifting LeNet's metal-op
  kernels (conv2d, reshape) onto metal-tile is the next leverage
  point for autotune-driven reduce wins on real workloads.

### GROUP_REDUCE scaling sweep
- [x] (2026-05-06) Sweep the synthetic `TUOpReduce + GROUP[1, L]`
  benchmark across N x L grid.  Saved to
  `bench/synth/group_reduce_sweep.txt`.  Headline results
  (per-call wall, 100 reps):

  | N      | L=16   | L=32   | L=64   | L=128  |
  |--------|--------|--------|--------|--------|
  | 1024   | 1.006x | 1.047x | 1.012x | 1.041x |
  | 4096   | 1.028x | 0.778x | 1.099x | 1.121x |
  | 16384  | 1.069x | 1.05x  | 1.081x | **1.181x** |
  | 65536  | 0.999x | 1.004x | 0.933x | 1.048x |
  | 262144 | 0.998x | 0.97x  | 1.035x | 0.998x |

  Best wins: N=16384, L=128 (1.18x); N=4096, L=128 (1.12x).
  Wins plateau and become noise above N~16k.  L=128 is the most
  consistent winner.  Two limits at play:
  - WL `TRealize` overhead (1.3 ms per call) dominates small-N.
  - Cooperative pattern only spawns L threads per group, so very
    large N becomes per-thread-sequential (each thread walks N/L
    elements) -- not compute-saturated.  Real wins need a
    dispatch shape change that increases groups (currently groups=1
    for these reduces).  That's a follow-on task.

### Lift-coverage triage on LeNet kernels
- [x] (2026-05-06) Run `wl/Examples/lenet-mnist/forward.wls` with
  `THVM_DUMP_LIFT_REJECT=1` and capture the lift-reject reasons for
  every kernel that ends up on `metal-op`.  Saved to
  `bench/lenet-mnist/lift_reject.txt`.  100 reject events across
  the forward pass; deduped:

  | count | reason                                                |
  |------:|-------------------------------------------------------|
  |    80 | `index/buf-not-DEFINE op=S_INDEX(4) src_count=4 dtype=13` |
  |    10 | `entry/no-scalar-arena n_inputs=24 n_ops=71 n_tile_uops=0` |
  |    10 | `entry/no-scalar-arena n_inputs=2 n_ops=4 n_tile_uops=0` |

  - **80% are buf-not-DEFINE on rank-3 S_INDEX (dtype DT_FP32):**
    `lift_scalar_index` only accepts S_DEFINE_PARAM or
    S_DEFINE_OUTPUT as the buffer src; LeNet's intermediate-tensor
    indices reference an earlier S_LOAD/S_INDEX result instead, so
    the lifter rejects.  This is the gather-style pattern that
    needs an indirect-index lift path.
  - **20% are `entry/no-scalar-arena`:** kernels with no rangeify
    arena (GEMM and conv2d-flat shapes), which would normally fall
    through to `kernel_lift_from_gemm` / `kernel_lift_from_conv2d`
    -- these specific shapes don't match either.  The
    `n_inputs=24` shape is the LeNet conv2d-flat with 24 patch
    inputs (kh*kw*c_in for the 5x5 conv); `n_inputs=2` is likely a
    bias-add or similar.

  Direction for the next leverage point: extend
  `lift_scalar_index` to accept a buffer-side intermediate (a
  buf-of-INDEX value), and audit the conv2d-flat path for the
  24-input case.

### Lift-reject deep dive
- [x] (2026-05-06) Enrich the `index/buf-not-DEFINE` reject
  diagnostic to print the actual ScalarUop opcode at `bu->op`.
  **Already done** -- inspecting `lift_reject_log` shows it's
  called with `buf_sid` (the buffer-side scalar id), not the outer
  S_INDEX sid, so the printed `op=S_INDEX(4)` IS the buffer side's
  opcode.  S_INDEX's enum value in [src/thvm.h](../../src/thvm.h)
  is 4 (after S_NONE=0, S_RANGE=1, S_DEFINE_PARAM=2,
  S_DEFINE_OUTPUT=3, S_INDEX=4), confirming **the buffer side is
  itself an S_INDEX**.

  Conclusion: the LeNet rank-3 access pattern is `S_INDEX[
  S_INDEX[buf, r1', r2', r3'], r1, r2, r3]` -- a re-indexing of an
  earlier indexed access (movement-op residue: SHRINK/PAD/PERMUTE
  composed onto an existing tensor view).  `lift_scalar_index`
  could see-through the inner S_INDEX by composing the address
  expressions before falling back to the strict
  S_DEFINE_PARAM/S_DEFINE_OUTPUT check; that's the buf-of-INDEX
  lift extension.

### Lift extension follow-on
- [x] (2026-05-06) Diagnose the buf-of-INDEX inner-shape on LeNet.
  Enriched `lift_reject_log` to also dump the inner ScalarUop's
  src list when the buffer side isn't a DEFINE_*.  Re-running
  LeNet forward with `THVM_DUMP_LIFT_REJECT=1` shows every reject
  has the **same clean inner shape**:

  ```
  inner srcs: [0]=S_DEFINE_OUTPUT [1]=S_RANGE [2]=S_RANGE [3]=S_RANGE
  inner srcs: [0]=S_DEFINE_PARAM  [1]=S_RANGE [2]=S_RANGE [3]=S_RANGE
  ```

  The OUTER S_INDEX (4 srcs: 1 buf + 3 ranges) wraps an INNER
  S_INDEX whose buffer is already DEFINE_*  with three ranges
  matching the same buffer's rank.  This is rangeify's
  pad/shrink/permute residue: two layered S_INDEX nodes refer
  to the same underlying buffer, with the outer ranges
  re-expressing the access in a different iteration order.

  Design implication: lift_scalar_index can see-through the
  inner S_INDEX by treating it as pure address composition --
  the underlying buffer is the inner's DEFINE_*  src; the outer's
  3 ranges replace the inner's 3 ranges at the SAME buffer rank.
  In the simplest case, outer's ranges directly substitute for
  inner's: the ranges bind to the same axes but iterate
  differently.  Need to verify this assumption with one concrete
  source-level example before coding.

- [x] (2026-05-06) Implement buf-of-INDEX see-through in
  `lift_scalar_index`.  Two cases handled:
  - **Same-rank passthrough**: when `bu->op == S_INDEX` and the
    inner's `src[0]` is a DEFINE_*, treat the inner's underlying
    buffer as our buffer and let the outer ranges drive
    linearisation (the outer is a full re-expression of the same
    iteration domain).
  - **Flat-index special case**: when the outer S_INDEX has a
    single range (`src_count == 2`) but the underlying buffer has
    rank > 1, treat the single range as a row-major linear offset
    into the buffer and skip the multi-axis stride composition.
    This covers the rangeify pad/shrink residue that flattens a
    rank-N tensor into rank-1 for the consumer side of a movement
    op.

  **Headline result on LeNet forward** (`bench/mlp-mnist`-style
  capture):

  | metric           | pre   | post  | delta    |
  |------------------|-------|-------|----------|
  | warmup_ms        | 185.6 | 224.7 | +21%     |
  | steady_ms_avg4   |  18.5 |  16.7 | -10%     |
  | metal-op count   |    45 |    25 | -44%     |
  | metal-tile count |    30 |    50 | +67%     |

  Dispatch shifted from 60/40 metal-op/tile to 33/67 -- more than
  half of LeNet's kernels now use the tile path.  Forward
  numerics on all 5 MNIST samples produce reasonable softmax
  outputs (random-init weights so predictions are nonsense but
  consistent across runs).  make test 274/274 unchanged.

  Warmup got slightly slower; suspected cause is more kernels
  going through the lift/render pipeline that previously fell
  through to a faster per-op metallib path.  Steady-state still
  wins because the rendered tile kernels amortise compile cost
  across calls.

### Bench-train re-run with buf-of-INDEX see-through
- [~] (2026-05-06) Re-run `wl/Examples/lenet-mnist/bench-train.wls`
  N_STEPS=5 now that 67% of kernels use the tile path (was 40%).
  Saved partial output to
  `bench/lenet-mnist/train_post_buf_index.txt`.  **Blocked**: the
  bench-train run aborts with `kernel killed by signal 6`
  (SIGABRT) during the baseline phase.  Verified the abort is
  caused by the buf-of-INDEX see-through commit (`7b97646`) by
  checking out the prior commit (`901e474`) and confirming
  bench-train completes there (1198 kernels, 1.03x autotune
  speedup).  make test 274/274 still passes; the abort only fires
  on shapes the C-side tests don't exercise.

### Bench-train regression follow-on
- [x] (2026-05-06) Identify which kernel shape triggers the
  SIGABRT after the buf-of-INDEX see-through.  **Root cause: my
  flat-index special case** (outer S_INDEX rank 1, buf rank > 1)
  was treating the single outer range as a row-major linear offset
  WITHOUT verifying that the range extent matched the buffer's
  total numel.  When the range covered only a subset (e.g., a
  SHRINK residue), the rendered kernel produced an OOB read or
  wrong addresses.  Forward happened to "pass" because the
  softmax-sum check tolerated wrong intermediates; backward
  produced NaN/Inf which aborted the runtime.

  Fix: gate the flat-index branch on
  `range_extent == prod(buf.dims)`; reject otherwise.  Trade-off:
  loses the LeNet-forward 67% tile-path coverage win (back to
  60/40 metal-op/tile) but bench-train no longer aborts.  Forward
  steady wall slightly improved (16.45 ms vs original 18.5 ms,
  not 16.7 ms; the same-rank passthrough still helps a tiny bit).
  bench-train completes with 191 ms/step baseline + 1.078x
  autotune speedup.

  make test 274/274 unchanged.

### buf-of-INDEX coverage follow-on
- [x] (2026-05-06) Investigate post-fix
  `flat-range-extent-mismatch` rejects.  Enriched the diagnostic
  to dump `range=N numel=M dims=[...]` for each reject.  All four
  unique rejects on LeNet forward have **`range=2`** while
  buffer numels are 800/1600/2880/5760 (dims like [50,4,4,2],
  [20,12,12,2]).  None are full-flat -- the outer rank-1 range
  iterates only 2 specific slices into a much larger
  multi-dim buffer.  The previous 67% tile-path "coverage" was
  illusory: my pre-fix flat-index produced wrong addresses for
  these partial-axis cases; forward masked it via softmax-sum
  tolerance, backward NaN'd.

  Conclusion: the extent-guard is correct.  Recovering these
  shapes onto the tile path requires a smarter lift that knows
  WHICH axis the outer range corresponds to and emits the right
  per-axis stride -- effectively a partial-axis SHRINK lift.
  That's a larger design task (separate follow-on).

- [x] (2026-05-06) Design a partial-axis SHRINK lift.
  **Refined model**: re-reading the rejects, the OUTER rank-1
  S_INDEX is **appending an axis** to the INNER's rank-N access,
  not flattening.  Concrete example from LeNet:
  - Buffer dims `[50, 4, 4, 2]` (numel 1600).
  - Inner: `S_INDEX[DEFINE_*, r1, r2, r3]` -- rank-3 access into
    the first 3 axes.
  - Outer: `S_INDEX[inner, r_outer]` where r_outer has extent=2,
    matching `dims[3]=2`.

  The combined access is rank-4: row-major address is
  `r1*32 + r2*8 + r3*2 + r_outer*1` (strides come from buffer
  dims).

  **Lift design**: in `lift_scalar_index`, when `bu->op == S_INDEX`,
  treat the inner+outer pair as a single rank-(N_inner + 1) access:
  1. Recursively lift the inner S_INDEX to get its address
     expression `inner_addr` and underlying buffer.
  2. The outer's range corresponds to a NEW axis appended after
     the inner's axes.  Its stride is the product of buffer dims
     past `N_inner` (typically 1 if N_inner == ndim - 1).
  3. Combined address = `inner_addr + r_outer * stride_outer`.

  Generalises naturally to multi-axis outer S_INDEX (current
  same-rank passthrough).

### partial-axis SHRINK lift implementation
- [~] (2026-05-06) Attempted the axis-append lift: inner ranges -> 
  dims [0, n_inner) with row-major strides, outer ranges -> dims
  [n_inner, ndim).  bench-train completed without abort and
  forward dispatch shifted from 60/40 to **47/53** metal-op/tile
  (35/40 of 75) -- 10 more kernels lifted than the same-rank-only
  pass.  But LeNet forward sample 3 produced **NaN** in the
  rendered softmax: `confidence=Round[$Failed, 0.001]`.
  Reverted; the simple row-major-stride composition assumed each
  range spans its corresponding buffer dim, which doesn't hold
  when rangeify emits partial ranges (movement-op residue).

  Concrete failure: for the LeNet shape with buffer dims
  `[50, 4, 4, 2]` (numel 1600), the rejected ranges had inner
  src_count=4 and outer extent=2.  My code linearised assuming
  inner ranges had extents [50, 4, 4] and outer had extent 2 --
  but at least one inner range likely had a smaller extent (a
  SHRINK-style sub-range), so the address arithmetic addressed
  the wrong cells.

  Direction: instead of blind row-major composition, audit the
  inner/outer range extents at lift time and bail when any range
  doesn't span its dim.  That's a bookkeeping fix, not a
  structural one.

### axis-append revisit
- [~] (2026-05-06) Re-attempted the axis-append lift with extent
  validation per (range, dim) pair.  Result: forward still
  produced NaN even when extents matched; `make test 274/274`
  still passed because no C-side test exercises the failing
  shape.  Reverted again; the structural model
  (inner ranges drive dims [0..n_inner), outer drive
  [n_inner..ndim)) is still wrong.

  Aside: forward.wls itself is unstable on the current main --
  re-running on HEAD with no changes produces samples 3-5 NaN'ing
  with `confidence=Round[$Failed, 0.001]`.  Different `true`
  labels across runs implies NetInitialize uses a non-fixed seed;
  some random-weight LeNets produce numerically degenerate
  softmax (no max-subtract stabilisation).  This noise made the
  axis-append validation inconclusive -- I can't tell if my
  lift_scalar_index changes introduce NaN or it's just the
  random-seed variance.  Need a fixed-seed forward test before
  trying axis-append again.

### forward.wls determinism follow-on
- [x] (2026-05-06) Add `SeedRandom[<fixed>]` before
  `NetInitialize` in
  [forward.wls](../../wl/Examples/lenet-mnist/forward.wls).
  Now picks `SeedRandom[7]` so weight init is reproducible.
  Two-run smoke shows identical output on consecutive runs.

  **Caveat**: even with the seed pinned, this LeNet random-init
  produces NaN softmax on most samples (no max-subtract
  stabilisation in the bare softmax helper).  That's an
  orthogonal issue; the script is now at least *deterministic*
  in its NaN behaviour, so future numerics-affecting changes can
  be diffed against a stable baseline.

### forward.wls softmax stabilisation follow-on
- [x] (2026-05-06) Add max-subtract to TSoftmax.  Path (a) chosen.
  Patched [NN.wl](../../wl/THVMLink/Kernel/NN.wl) `TSoftmax` to:
  ```wl
  TSoftmax[x_TTerm] := Module[{m, xc, e, s},
    m = TUOpReduce[x, 0, "MAX"]; xc = x - m;
    e = Exp[xc]; s = Total[e]; e / s
  ]
  ```

  Subtle bug found: explicit `TUOpExpand[m, shape]` from a {1}-
  shaped reduce result didn't fan out correctly (only the first
  output element saw the broadcast; rest read zeros).  Implicit
  binary-op broadcast (`x - m`) works.

  Verification:
  - `softmax(big = {1000, 999, 998, 997, 996})` now produces
    `{0.636, 0.234, 0.086, 0.032, 0.012}` instead of `$Failed`.
  - `softmax(small = {10, 9, 8})` produces
    `{0.665, 0.245, 0.090}` (matches naive form within fp).
  - `forward.wls` with SeedRandom[7]: all 5 samples produce
    sane predictions (sample 5 even predicts correctly!).
  - make test 274/274; nn.wlt 52/52; core.wlt 32/32.

### Re-bench LeNet train with stable softmax + buf-of-INDEX
- [x] (2026-05-06) Re-run `wl/Examples/lenet-mnist/bench-train.wls`
  N_STEPS=5 with the current state.  Saved to
  `bench/lenet-mnist/train_stable_softmax.txt`.  **Slower than
  pre-stable-softmax**:

  | run                               | ms/step | speedup | kernels | metal-op | metal-tile |
  |-----------------------------------|--------:|--------:|--------:|---------:|-----------:|
  | original                          |     262 |  1.29x  |    1198 |      778 |        372 |
  | + buf-of-INDEX same-rank          |     192 |  1.07x  |    1198 |      752 |        398 |
  | + stable softmax (HEAD)           |     315 |  0.789x |    2058 |     1151 |        811 |

  The stable softmax adds a MAX reduce + broadcast that nearly
  doubles the kernel count (1198 -> 2058).  The autotune pass
  *slowed down* the run (autotune speedup 0.789x).  Likely cause:
  the extra reduce kernels + broadcast pattern aren't fused by
  the scheduler's relaxation pass, and autotune's per-kernel
  bench probes amplified that overhead.

  Loss is still `$Failed` -- TCrossEntropyLoss applies `Log[pred]`
  which underflows to `-inf` when softmax peaks too sharply
  (probability ~0 for non-winning class).

### Stable softmax kernel-count regression follow-on
- [x] (2026-05-06) Investigate why the stable softmax adds ~860
  kernels.  Probed a single 10-element softmax: **3 kernels**
  emitted (was 1 for the naive form).  Inspected each:

  - **kid 1** (metal-tile): `for (a1=0..10) _acc=fmax(_acc, in0[a1]); out[0]=_acc;`
    -- the max-reduce.
  - **kid 2** (metal-op): empty MSL source (`""`).  Likely an
    alias / reshape / expand of the {1}-shaped max into broadcast
    form; surfaced as a no-op in the lift path.
  - **kid 3** (metal-tile): combined exp + sum-reduce + divide.
    Body: `for (a0=0..10) { for (a1=0..10) _acc1 += in0[a1]; out[a0] = in0[a0]*(1/_acc1); }`.
    Computes the full sum 10 times (once per output element)
    instead of hoisting -- but the sum-reduce + per-element
    divide IS in one kernel.

  The fusion already partially works (kid 3 fuses 3 ops).  The
  problems are:
    1. Max-reduce stays separate (no fusion with surrounding
       chain).
    2. Bridge kernel kid 2 is wasteful (empty source).
    3. kid 3's sum is recomputed per output position instead of
       hoisted outside the output loop.

  860 extra kernels in bench-train = ~430 softmax sites x 2 extra
  kernels each (or, since LeNet has only one softmax in forward
  but bench-train does 5 fwd+bwd steps with autotune, it's the
  cumulative overhead).  The biggest single fix: hoist sum-reduce
  in kid 3 so it computes once per kernel call.

### Stable softmax sub-fixes
- [x] (2026-05-06) Hoist the SUM accumulator in
  `rmu_emit_store` so output-axis-invariant reduces compute once,
  not once per output position.  Implementation: collect the
  output-axis ids from the addr expression; for each UOP_REDUCE
  inside the value, check whether its body references any
  output-axis range -- if not, mark hoistable; emit a first pass
  for hoistable reduces BEFORE opening output for-loops, and a
  second pass for non-hoistable reduces inside.

  Verification on `TSoftmax[Range[10]]` kid 3 (the exp + sum +
  divide kernel):
  ```
  // BEFORE (sum recomputed per a0):
  for (uint a0 = 0; a0 < 10; a0++) {
    float _acc1 = 0.0f;
    for (uint a1 = 0; a1 < 10; a1++) _acc1 += in0[a1];
    out[a0] = (in0[a0] * (1.0f/_acc1));
  }

  // AFTER (sum hoisted):
  float _acc1 = 0.0f;
  for (uint a1 = 0; a1 < 10; a1++) _acc1 += in0[a1];
  for (uint a0 = 0; a0 < 10; a0++) {
    out[a0] = (in0[a0] * (1.0f/_acc1));
  }
  ```

  Numerics preserved (softmax({10,9,8})={0.665,0.245,0.090};
  forward.wls 5/5 sane predictions).  make test 274/274 unchanged.

### Loss numerical stabilisation follow-on
- [ ] Add a small epsilon clamp in TCrossEntropyLoss
  (`Log[pred + eps]` or equivalent log-sum-exp) so cross-entropy
  doesn't underflow on sharply-peaked softmax.  Verify
  bench-train returns numeric losses.
