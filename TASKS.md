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

- [x] **k0d: TGradMany WL bridge + accessor**.
      `TGradMany[y, {x_1, ..., x_n}]` builds a single
      UOP_GRAD via `thvm_wl_uop_grad_multi`, realizes once,
      and unpacks the TAG_CTR result via
      `thvm_wl_term_ctr_at` into a List of n TTerm wrappers.
      Bridges + WL surface fn in
      wl/THVMLink/CSource/thvmlink.c +
      wl/THVMLink/Kernel/{THVMLink,Tensor}.wl.  3 new tests
      in grad.wlt (295 WL total).  166 C + 295 WL green.

- [x] **k0e: rewire bench + verify; measure delta**.
      lenetStep (baseline.wls) and stepGrads (verify.wls) now
      use `TGradMany[loss, weights]`; materialize descends into
      TAG_CTR children so all n backward kernels emit in ONE
      realize.  Result NEGATIVE: 427 -> 426 kernels (-0.2%),
      peak unchanged on both backends.  Cause: each per-target
      chain rule allocates FRESH UOp cells for its cotangent
      compute (new heap locs); only forward leaves are shared.
      Memo dedups the leaf-references, not the chain ops.
      To hit the original target needs content-addressed
      cotangent dedup OR shared scratch -- both new arcs.
      verify.wls still converges 2.61 -> 0.025 on both
      backends.  Detail in docs/bench-results.md "k0e".

- [x] **k1: per-realize labeled stat dump**.  WL surface
      `TMatStatsLabel["fwd_conv1"]` tags the next thvm_realize's
      `THVM_MAT_STATS` log line with the supplied string; the
      buffer clears after one realize.  C bridge in
      thvmlink.c (`thvm_wl_mat_stats_label`) writes into a 64-byte
      file-scope `MAT_STATS_LABEL` buffer; the dump line in
      materialize.c prefixes `label=...`.  Sample LeNet
      breakdown captured: forward+loss=231 kernels, grad_b1..b3=20
      each, grad_w4=40, grad_b4=36 (totals to the 427 bench
      number).  Forward dominates -- Conv2D-lowered chain is the
      next fusion target.

- [blocked: per-user "don't invent UOps that aren't in tinygrad"
  -- ADD chain fusion must happen in the scheduler/kernelizer
  via the existing binary UOP_ADD primitive.  Replaced by k2'
  below.] **k2a1/k2a2/k2b/k2c: UOP_ADDN approach**.

- [blocked: probe negative -- single TUOpConv2D forward shows
  toggle OFF=100, toggle ON=101 (regression).  helper_bail=52
  on this graph: inline_emit fails on most children even after
  d4b2b1's "recurse via materialize_expr" fix.  The conv2d-
  lowered chain has REDUCE_SUM partials wrapped in EXPAND/
  RESHAPE/SHRINK; the helper recurses into those, materialize_expr
  returns something inline_emit can't slot into a single kernel,
  and the kernel_dealloc_last/legacy fall-through allocates 1
  extra kernel per bail.  Real fix needs the helper to gracefully
  accept partial-fusion (build kernel for what it CAN fuse + leave
  the rest as separate kernels), or a different fusion mechanism
  entirely.  Both are bigger arcs than the forward-only k2' was
  scoped for.] **k2': revisit f1d helper for conv2d-lowered ADD
  chain**.

### Cleanup

- [x] **c1: drop the stat-counter env hook from the hot path**.
      Closing without code change.  k1 promoted `THVM_MAT_STATS`
      + `TMatStatsLabel` from disposable d4b2d instrumentation
      into a public probe feature (used by k2'/future fusion
      arcs).  The env check is `getenv("THVM_MAT_STATS")` once
      per realize and the counter `++`s are unread when the env
      var isn't set -- zero-cost on the default hot path.  The
      original "+42% CPU lenet" regression cited by c1 was
      mostly run-to-run jitter, not the env hook.

---

## Round 2 (2026-04-26): the goal isn't actually met

User feedback: "single forward kernel fusion for linear layer and
minimal memory allocation with good gantt plot?" -- the answer is
NO on all three.  Current linear-train state:

  Forward + loss : 17 kernels   (target: 1)
  Total Adam step: 93 kernels   (target: <=10)
  Peak concurrent: 0.6 KiB      (slot-reuse headroom: 57.6%)
  Gantt          : tiny bufs all stack at y=0; 1 preserved
                   buffer (buf 22) dominates visually

Root causes:
  - **Forward not fused**: the MatVec -> Add -> Softmax -> CE Loss
    chain has REDUCE_SUM at the end (CE = sum of -log(probs)).
    The f1d helper was designed for elementwise chains but bails
    on REDUCE-as-tail-op.  Tinygrad fuses elementwise + final
    REDUCE into one kernel ("local reduction" pattern); we don't.
  - **Memory not reused**: 57.6% headroom = bufs whose lifetimes
    don't overlap could share the same slot.  The slot allocator
    infrastructure is in place; the wpt-pin walk pins them all.
  - **Gantt is tiny-buf-hostile**: linear-scan packing on a
    nbytes-y-axis means the 0.5 KiB W3 weight and the 0.001 KiB
    intermediates land at the same y stripe, indistinguishable.

### New tasks

- [x] **r1a: probe linear-train forward UOp graph; decide what
      "1 kernel" really means**.  The forward graph
      (`loss = CrossEntropyLoss(Softmax(MatVec(w,x)+b), target)`)
      expands to 16 UOps with **THREE REDUCE_SUMs**:
        - MatVec       = REDUCE_SUM(MUL(w, EXPAND(x)), axis=1)
        - Softmax      = MUL(EXP, EXPAND(RECIP(REDUCE_SUM(EXP,0))))
        - CE Loss      = NEG(REDUCE_SUM(MUL(target, LOG(p)), 0))
      Currently 16 kernels (1 per UOp).  "1 forward kernel"
      target is unreachable -- tinygrad emits 1 kernel per REDUCE
      boundary too.  Realistic target updated to **<= 4 forward
      kernels** (one per REDUCE + maybe one for tail elementwise).
      Probe captured via TMatStatsLabel: r1b's job is now
      "elementwise + outermost-tail-REDUCE collapse so each REDUCE
      absorbs its upstream chain".

- [x] **r1b: extend f1d helper to absorb an outermost-tail REDUCE**.
      `materialize_kernel_inlined` now accepts `root_op == UOP_REDUCE`
      when the source is a fully-inlinable elementwise chain.  Built
      kernel: N LOAD prefix + N-1 elementwise ops into a register +
      one REDUCE op into the output buffer.  REDUCE arg packed to
      (kind << 24 | inner) per cpu_op_reduce's runtime encoding;
      output shape derived by dropping the reduce axis from the
      first input slot's shape.  Metal still bails (helper is
      CPU-only via the existing backend gate).  Tests:
      tests/test_materialize_inlined.c gains "reduce-as-tail-
      collapses" + a non-elementwise-non-reduce-bails check.
      Linear-train probe with toggle ON: forward+loss drops
      from 16 -> 8 kernels (REDUCE_SUM(MUL(...)) chains in
      Softmax-norm and CE-loss collapse).  ~80 LOC.

- [x] **r1c: re-bench linear-train + verify acceptance**.
      Toggle ON / OFF per-phase numbers:
        forward+loss : 16 -> 8   (-50%)
        + grad w     : 40 -> 75  (+88%)
        + grad b     : 36 -> 67  (+86%)
        Adam step    : 92 -> 150 (+63%)
      Forward halves (good!), backward regresses hard (same
      pattern as d4b2d).  Acceptance ("<=4 forward kernels")
      not met -- got 8.  Default toggle stays OFF because the
      backward regression dominates per-Adam-step totals.
      Detail recorded in docs/bench-results.md "r1" section;
      follow-up note: a TRealizeFused[] WL surface (toggle
      ON only for forward realizes) would let LeNet harvest
      the forward win without the backward cost.

- [x] **r2a: probe + decide what bufs are recyclable**.
      Probed both graphs by computing the "naive lifetime-aware
      peak" = max-over-depth of (sum of nbytes of bufs whose
      [alloc_depth, last_use_depth] interval covers that depth):
        linear-train: peak 0.562 KiB, naive lifetime-aware
                      peak 0.562 KiB -> **0% potential drop**.
        lenet      : peak 1882.3 KiB, naive lifetime-aware
                      peak 1882.3 KiB -> **0% potential drop**.
      The peak IS already the lifetime-aware optimum.  The
      57.6%/52.8% slot-reuse-headroom metric is misleading:
      it's `(total_alloc - peak) / total_alloc`, which is the
      gap between cumulative allocations and concurrent peak.
      The slot allocator can only reduce peak if some buf is
      freed AND a new alloc could reuse the slot during the
      same depth window.  Here every buf alive at peak depth
      is concurrently needed by the DAG -- nothing to release.
      r2's premise was wrong; r2b/r2c/r2d would deliver 0%
      peak drop and are now blocked.

- [blocked: r2a probe shows naive lifetime-aware peak == current
  peak (0% potential drop).  The bufs alive at peak depth are
  concurrently needed by the DAG; no scheduling-side fix can
  reduce peak.  Real memory wins need a graph-structural
  change (smaller forward, fewer realized intermediates) or
  in-place compute -- both are bigger arcs than r2 was
  scoped for.] **r2b: per-kernel "free-after" list at
  materialize time**.

- [blocked: same as r2b -- gated on r2's premise being
  correct, which r2a falsified.] **r2c: kernel_fire releases
  free_tids[] to the freelist**.

- [blocked: same as r2b/r2c -- no peak drop is reachable
  from this mechanism per r2a.] **r2d: bench delta + verify
  acceptance**.

- [ ] **r3: Gantt rendering that survives tiny-buf graphs**.
      Linear-train's bufs are sub-1-KiB so linear-scan packing
      with a nbytes-y-axis stacks them all at y=0 indistinguishably.
      Fix: switch the y-axis to LOG2(1 + nbytes) (already an
      option in TMemoryPlanGantt's "BarHeight" -> "Log" mode --
      currently DEFAULT but per the m1 PNG seems not active OR
      not wired into the linear-scan packer).  Verify the option
      actually works; if "Log" is degenerate for tiny bufs,
      fall back to UNIFORM bar height with bytes shown in the
      tooltip.  Acceptance: linear-train memory-plan-cpu.svg
      shows distinct y-stripes for each buf; the dominant
      preserved buf is visually smaller (proportional to its
      tiny footprint), not the whole-page band it is now.
      ~30 LOC in MemoryPlan.wl + visual smoke test.
