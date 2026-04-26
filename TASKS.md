# THVM tasks - memory + kernelization, fresh start (2026-04-26)

> Previous task list archived to `docs/archive/TASKS-2026-04-26.md`.
> That file documents the full f1 fusion arc + bm/wpt/gc memory
> arcs end-to-end; cite it for "why was X tried" but don't extend
> it.  This file holds the current focused work.

## State

LeNet Adam training on Metal works end-to-end.
`wl/Examples/lenet-mnist/verify.wls` converges loss
2.61 → 0.025 in 4 Adam steps, prediction 0 → 4 (correct),
prob[true] 0.074 → 0.997.  166 C tests + 292 WL tests green
on default settings (`MATERIALIZE_USE_REALIZE_INFO=0`).

Bench numbers (see `docs/bench-results.md` "post-f1" column):

| bench                           | backend | kernels | peak_kib | total_kib |
| ------------------------------- | ------- | ------: | -------: | --------: |
| lenet-mnist (Adam step)         | CPU     |     427 |   1882.3 |    3987.7 |
| lenet-mnist (Adam step)         | Metal   |     427 |   1882.3 |    3988.3 |
| beautiful-mnist (forward only)  | CPU     |     217 |  82750.3 |   92832.1 |
| beautiful-mnist (forward only)  | Metal   |     217 |  82750.3 |   92832.1 |

`slot_reuse_headroom_pct` is **52.8%** on lenet and **10.9%** on
beautiful — buffers that COULD share allocator slots if their
lifetimes were partitioned.  The slot allocator infrastructure
(CPU + Metal freelists, rollback push-on-non-preserved, alloc-
side recycle) is in place; it receives 0 entries because the
WL-pinned-Terms walk pins every TTerm wrapper WL has touched in
the current step.

## The two real problems

### Memory (m): peak_concurrent_kib is stuck at the conservative-pin baseline

`pool_rollback_with_preserve` calls `mark_gc_preserve`, which
unions the WL-pinned-Terms (wpt) set into the preserve set.
Every `TTensorCreate`, `TUOpAdd`, `TGrad`, etc. populates one
TTerm wrapper that goes into the wpt table; nothing removes
them until the next `TInit[]; TReset[]`.  Result: the rollback
between realizes preserves every intermediate buf, the freelist
stays empty, the slot allocator has nothing to recycle, peak
memory equals the cumulative live set.

The infrastructure unblocker exists: `TTermUnpin[t]` (shipped in
wpt2) drops a single TTerm from the pin table.  No callsite
uses it yet.

### Kernelization (k): per-UOp emit means 1 kernel per output buf

The legacy materialize path emits exactly one kernel per UOp
(post-memo dedup).  LeNet Adam step = 427 UOps → 427 kernels →
427 output buffers.  The f1 arc's per-kernel fusion mechanism
was the wrong tool (see `docs/archive/TASKS-2026-04-26.md`
f1d-d4b2d): each fused kernel STILL allocates its own output
buf, so even where fusion succeeds it doesn't drop peak memory.

Real kernel-count + memory wins need either:
- A **multi-stage helper** that absorbs intermediate compute
  AND skips the intermediate output buf.
- A **lifetime-aware schedule** that emits explicit "free buf X
  here" ops, decoupling memory recycling from the conservative
  preserve walk.

## Tasks

### Memory

- [x] **m1: TMemoryPlanGantt readability fixes**.  Three landed
      changes: (a) `collateBufs` now propagates dtype from the
      first aliased tid into each Bufs record, with a
      `dtypeName[]` lookup that maps DT_F32/DT_I32 -> human
      strings (was `Missing[KeyAbsent, dtype]` in tooltips);
      (b) `TMemoryPlanGantt` title surfaces the TopN cut as a
      prominent italic line "showing top 40 of 391 bufs (=
      60.2% of bytes)" so the rendering doesn't silently hide
      90%+ of the data; (c) all four bench + two memory-probe
      Export targets switched from `.png` to `.svg` so the LeNet
      Gantt is zoom-friendly + scalable.  Pre-existing PNG files
      removed; docs/bench-results.md + linear-train/README.md
      updated to point at the new SVG paths.  ~50 LOC across
      MemoryPlan.wl + 3 Export sites.

- [x] **m2: per-realize unpin probe in lenet-mnist verify.wls**.
      Modified `stepGrads` (verify.wls) and `lenetStep` (bench
      baseline.wls) to `TTermUnpin[gradTerm]` after each grad's
      host extract.  Bench peak_concurrent_kib UNCHANGED on both
      backends (1882.3 KiB).  Diagnosis recorded in
      docs/bench-results.md "m2" section: the dominant pinned
      set is the forward intermediates (W1..b4 + h1..probs)
      which must stay pinned through the entire 8-grad loop
      because each `TGrad[loss, w_i]` walks them.  Per-grad
      transient bufs (which DO get unpinned) are a small
      fraction of total memory.  Real unblockers are k0
      (multi-output TGrad lets the backward pass free
      intermediates as it consumes them) or a lifetime-aware
      schedule.  TTermUnpin pattern stays in -- correctness-
      preserving + reduces inter-step gap pressure.  m3 is
      gated on m2 success and is therefore moot; closing it too.

- [blocked: gated on m2 success; m2 showed 0% peak drop so
  TPinScope wouldn't move the metric either.  Real unblockers
  are k0 or a lifetime-aware schedule.] **m3: TPinScope[]
  auto-unpin block**.

### Kernelization

- [x] **k0a: TAG_CTR labelled constructor (HVM4 CTR)**.
      TAG_CTR=20 in src/thvm.h with heap layout
      [NUM(arity), c_0, ..., c_{n-1}]; ext = label.
      Constructor `term_new_ctr(label, children, n)` +
      accessors `term_ctr_n(t)` / `term_ctr_at(t, i)` in
      src/term/new_ctr.c.  tests/test_ctr.c (22 checks).
      No DUP-CTR / ERA-CTR yet; land when an IC consumer
      needs them.  166 C + 292 WL tests green.

- [x] **k0b: multi-target uop_grad constructor + heap layout**.
      `uop_grad_multi(y, gy, targets, n)` builds the new layout
      `[y, gy, NUM(n), x_1, ..., x_n]`; `uop_grad(y, gy, x)`
      is a thin wrapper with n=1.  Accessors `uop_grad_n` /
      `uop_grad_target` in src/uop/grad.c.  interact_grad now
      reads target from loc+3 and bails on n>1 (k0c will fire).
      Variable-arity readers updated (wnf/redex.c term_arity
      reads NUM(n) from heap; book/from_dynamic.c dyn_arity
      and alo/realize.c alo_node_arity take `val` to read NUM
      via heap_read/book_read so book templates with embedded
      UOP_GRAD -- TOptim's recursive lambdas -- clone correctly).
      tests/test_grad.c adds 3 k0b checks (75 total).  166 C +
      292 WL tests green.

- [x] **k0c: interact_grad multi-target chain rule**.
      n>1 case in interact_grad lowers to a TAG_CTR of n
      unary uop_grad(y, gy, x_i) terms.  Each unary grad
      walks the chain rule independently; the forward DAG
      lives at shared heap locs so materialize's memo dedups
      kernels emitted from those forward UOps across all n
      targets.  Single-target case (n=1) is unchanged.
      tests/test_grad.c adds 4 multi-target checks (79 total).
      Backward compute sharing across targets is future-work
      (would need scratch cotangent slots).

- [ ] **k0d: TGradMany WL bridge + accessor**.  Add
      `TGradMany[y, {x_1, ..., x_n}]` in
      wl/THVMLink/Kernel/Tensor.wl that builds a single
      UOP_GRAD_MULTI Term, then unpacks the resulting
      TAG_CTR (post k0c materialize) into n TTerm
      wrappers via term_ctr_at.  Bridge fns added to
      wl/THVMLink/CSource/thvmlink.c.  Tests in
      wl/THVMLink/Tests/grad.wlt assert that
      `TGradMany[loss, {a, b}]` and
      `{TGrad[loss, a], TGrad[loss, b]}` yield equal
      gradient values.  ~50 LOC + tests.

- [ ] **k0e: rewire bench + verify; measure delta**.  Switch
      lenetStep (baseline.wls) and stepGrads (verify.wls)
      to `TGradMany[loss, {w1, b1, ..., w4, b4}]`.
      Acceptance: lenet kernel_count drops from 427 to
      <=200 AND peak_concurrent_kib drops by at least 20%
      (currently 1882.3, target <= 1500).  verify.wls
      still converges loss 2.61 -> 0.025.  Update
      docs/bench-results.md with a "post-k0" column.
      ~20 LOC + measurement.

- [ ] **k1: per-realize labeled stat dump**.  Currently we know
      "lenet Adam step = 427 kernels" but not which forward layer
      / backward chain contributes how many.  Extend the
      `THVM_MAT_STATS=<path>` env hook (added in
      `src/schedule/materialize_memo.c` during d4b2d) to take an
      optional WL-supplied label per `thvm_realize` call (e.g.,
      "fwd_conv1", "grad_w3").  Acceptance: a probe script that
      prints "label: N kernels" lines for every realize in a
      LeNet Adam step, grouped by layer/grad.  ~30 LOC across
      thvmlink.c (new `TMatStatsLabel` bridge fn) + materialize.c
      (write the label into the dump line).

- [ ] **k2: pick the next fusion target from k1's data**.
      Decompose once k1 is in.  Likely candidates per the
      d4b2d analysis: `Fold[TUOpAdd, partials]` in
      TUOpConv2DLowered (one kernel per partial vs. one kernel
      total), softmax `exp / sum / div` chain, cross-entropy
      `log / mul / sum` chain.  This is a NEW decomposition,
      not "f2" from the archived list -- start fresh with k1's
      breakdown.

### Cleanup

- [ ] **c1: drop the stat-counter env hook from the hot path**
      (defer until m / k arcs settle).  Once the investigation
      arcs above stop needing per-realize counters, remove the
      `THVM_MAT_STATS` instrumentation from `materialize_memo.c`
      / `materialize.c` / `materialize_in_env.c` (added in
      d4b2d for the f1 investigation).  Bench wall-time should
      tick down ~5% on CPU lenet (the +42% post-wpt regression
      partly attributable to it).  ~30 LOC of removals + bench
      re-run + a `docs/bench-results.md` "post-c1" column.
