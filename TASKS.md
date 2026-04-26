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

- [ ] **m2: per-realize unpin probe in lenet-mnist verify.wls**.
      Add `TTermUnpin` calls in `wl/Examples/lenet-mnist/verify.wls`
      after each grad realize completes.  Re-run the bench (CPU +
      Metal); record the new peak_concurrent_kib.  Acceptance:
      peak drops by at least 20% on lenet OR a clear
      diagnosis of why it doesn't (e.g., the unpinned tensors are
      still rooted via DEFS / WNF_LAST_STACK).  ~30 LOC + measure
      + a one-paragraph result note in `docs/bench-results.md`.

- [ ] **m3: TPinScope[] auto-unpin block** (gated on m2 success).
      If m2 shows a clear win, codify it as a WL primitive that
      auto-clears the wpt set at scope exit -- easier ergonomics
      than scattered `TTermUnpin` calls.  Acceptance: lenet
      verify.wls + memory-probe.wls migrated to `TPinScope[...]`
      blocks; bench delta matches m2's measured drop; new
      `wl/THVMLink/Tests/pin_scope.wlt` covers the basic
      enter/exit semantics.  ~50 LOC + tests.

### Kernelization

- [ ] **k0: multi-output TGrad / UOP_GRAD**.  Today every
      `TGrad[loss, x_i]` is a separate UOP_GRAD that allocates
      independent backward kernels even though the chain rule
      shares almost all intermediate cotangents across the
      `x_i`.  In LeNet's Adam step this means 8 independent
      `TGrad` calls (w1, b1, w2, b2, w3, b3, w4, b4) each
      walking the same forward graph from `loss` -- ~300
      kernels of duplicated backward compute.

      Redesign: `TGrad[loss, {x_1, ..., x_n}]` materializes
      ONE UOP_GRAD whose chain rule fires once and writes
      one output buf per requires_grad ten (zero buffers for
      anything else).  Sketch:
        - new heap layout for UOP_GRAD: `[y, seed, n,
          x_1, ..., x_n]` (was `[y, seed, x]`).
        - `interact_grad` walks the forward DAG once, building
          a cotangent map ten -> term; emits a multi-output
          KERNEL whose program produces n outputs in one fire.
        - `materialize` allocates n output bufs (one per x_i),
          links them via a new `outputs[]` table on KernelEntry
          (was `output_tid`).
        - WL bridge: `TGrad[loss, list]` returns a TList of
          TTerm wrappers; existing `TGrad[loss, x]` keeps
          working as the unary case (auto-wraps in `{x}`).

      Acceptance: lenet bench Adam step kernel count drops
      from 427 to <=200 (the 8 grad realizes collapse to 1
      multi-output backward); peak_concurrent_kib should also
      drop because the shared cotangents only allocate once.
      Big change -- expected ~200-300 LOC across uop/grad.c +
      interact/uop_grad.c + schedule/materialize.c +
      schedule/kernel_alloc.c (KernelEntry layout) + the WL
      bridge.  DECOMPOSE on next fire before implementation.

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
