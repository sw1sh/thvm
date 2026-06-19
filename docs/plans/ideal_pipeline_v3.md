# ideal_pipeline_v3 - faithful AND clean

Successor to `ideal_pipeline_v2.md` (which got faithful realize-seeding correct on
CUDA + Metal). v3's target is twofold and equal-weight:

1. **Ideal** - close the remaining algorithmic-coverage gaps vs tinygrad (the spec at
   `/Users/swish/src/tinygrad/tinygrad`).
2. **Clean** - refactor and beautify the code along the way. Every milestone leaves
   the files it touches cleaner than it found them (project style: no transient
   comments, idiomatic, no dead code, no env-knob sprawl). "Clean" is a deliverable,
   not a nice-to-have.

The detailed framework comparison lives in `docs/thvm_vs_tinygrad.md`. **That report
has two material errors (corrected below); treat its gap analysis as a draft to
verify, not ground truth.**

---

## 0. Corrections to the v2 comparison report (verified 2026-06-01)

The v2 report was assembled by excerpt-reading agents and overstated two gaps. Both
capabilities **exist, are wired into the production fire path, and are WL-exposed.**

- **BEAM / autotune is NOT missing.** `src/codegen/autotune.c` (855 lines) is a real
  search: `kernel_autotune(kid)` runs propose -> bench-each-variant (`BEAM_RUNS`
  timing runs) -> sequence-build -> pick-winner -> disk-cache; `kernel_should_autotune`
  (`autotune.c:843`) gates on `autotune_env_enabled()` which enables on `BEAM>0` or
  `AUTOTUNE=1` (`autotune.c:832-841`). It is wired in the fire path
  (`src/interact/uop_kernel.c:255-256`, right after `kernel_hand_coded_opts`) and
  exposed to WL (`thvm_wl_kernel_autotune`, `wl/THVMLink/.../thvmlink.c:1914`).
  **The real gap:** the candidate **proposer** `src/codegen/propose.c` is
  "deliberately narrow" (its own comment): it only emits UNROLL for reduce-tails and
  LOCAL/TC gated on `DEV=metal && THVM_TILE=1` / GEMM. For a plain beautiful_mnist
  Metal kernel it returns ~0 candidates, so `BEAM=2` no-ops (measured: warm 3.72ms
  with BEAM=2 vs 3.63ms without, zero autotune activity). tinygrad's `search.py`
  enumerates ~200 actions/kernel (UPCAST 6x8, UNROLL 3x5, LOCAL 7x6, GROUPTOP 8x3,
  GROUP 4x3, PADTO, TC specs, SWAP, THREAD). **So the gap is "expand the proposer +
  confirm py-path firing", NOT "build BEAM".** Effort: medium, was mis-scored large.

- **Symbolic rewriting is NOT missing.** `src/uop/symbolic_rewrite.c` (port of a
  *subset* of tinygrad `symbolic.py:76-155`), `src/uop/graph_rewrite.c`,
  `src/uop/graph_simplify.c`, plus constructor-time `uop/rewrite.c` +
  `uop/index_simplify.c`. Wired in materialize (`uop_graph_simplify_materialize`,
  `materialize.c:4691`) and render (`uop_graph_rewrite` / `uop_symbolic_rewrite` at
  many sites in `render_uop.c`). **The real gap:** the *specific* unported rules
  (full ShapeTracker view-cancellation, boolean-OR validity union,
  arange/reduce-collapse). Effort: medium completion, NOT large from-scratch.

**Method consequence:** every v3 milestone begins with an **audit step** that reads the
FULL relevant files (not excerpts) and verifies the gap against actual code + a runtime
test before any change. Each milestone ends with a **refactor/clean step** on the files
it touched. Milestones: audit -> close -> clean -> verify.

---

## 1. This-session baseline (what v3 builds on)

Committed on `py-jit-speedup`: `e14f7da9` (faithful conv-bwd compiles: required_pos
cap), `bb24342d` (beautiful_mnist_jit zero_grad), `d595fcb7` (py dylib links the real
Metal backend). Verified parity (warm train, BS=8):

| | CUDA wall | Metal wall | CPU wall | kernels (eager) | peak mem |
|---|---|---|---|---|---|
| thvm default | 6.15ms | 3.63ms | **7.36ms** | 164 | 31MB |
| thvm faithful | 8.64ms | 4.72ms | **238ms** | 113 | 14.8MB |
| tinygrad | ~8.7ms(BEAM) / 12ms | 6.03ms | 15.09ms | 104 | ~1-3MB |

The `kernels` column is the HONEST eager-schedule count (`kernel_count()` delta on one
clean realize); the earlier 328/226/120 figures were the JIT-harness counts, ~2x
inflated by capture-vs-eager duplication (see the audit-correction entry in section 4).
On kernel count thvm faithful (113) is within 9% of tinygrad (104) and the forward fuses
MORE -- fusion parity is essentially met. The standing gaps are CODEGEN (faithful's CPU
wall is 16x tinygrad because fused convs miss the BLAS path and render unvectorized -- the
real "faithful incl. performance" lever) and peak memory (buffer reuse). thvm default
WINS CPU/CUDA/Metal wall via the Accelerate-BLAS matmul path.

---

## 2. Verified gap inventory (accurate; effort = small/medium/large)

**Opt / search**
- Proposer coverage (`propose.c`) narrow vs `search.py` ~200 actions - **medium**. The
  BEAM engine exists; feed it the full action space (UPCAST/LOCAL/GROUP/GROUPTOP/PADTO/
  SWAP/THREAD) + confirm it fires on the py/JIT path.
- Full-extent `UNROLL(last,0)` disabled by an `rmu_emit_store_reduce` renderer
  assumption - **medium** (verify, then fix).
- Mask-UPCAST (WHERE/IWHERE axis exclusion) - **medium**.
- THREAD axis + CUDA warp-shuffle reduce (vs current 16-thread GROUP_REDUCE) - **large**.
- Multi-reduce TC (`TC_OPT>=1`) - **small** (relax `hand_opt_classify_matmul`).

**Scheduling / fusion**
- Reduce-into-reduce fusion (one-reduce-per-kernel codegen rule) - **large/medium**; the
  named residual. Verify on conv-backward first.
- Symbolic rule completion: boolean-OR validity union, full view-cancellation,
  arange/reduce-collapse - **medium**. UPDATE (2026-06-05): ported the nested-IDIV
  collapse `(x//c1)//c2 -> x//(c1*c2)` (tinygrad symbolic.py:258) -- a genuinely-absent
  self-contained int rule (subsumes the chained `(idx//c1)//c2` the RESHAPE flat-decompose
  composer leaves). The three originally listed are NOT self-contained symbolic-int rules:
  validity-union lives cross-layer in `rangeify_unified.c:838`, view-cancellation is the
  deep lever, arange/reduce-collapse needs REDUCE-collapse infra the int-binary rewriter
  lacks -- so they remain larger follow-ups.
- PCONTIG per-axis ending-ranges check - **small**.

**Autodiff / optim** (`gradient.py:49-82` has, `interact/uop_grad.c` lacks)
- SIN/COS, POW, element-wise MAX, WHERE, CONTIGUOUS(/_BACKWARD), COPY backward - **small each**.
- `zero_grad` safety guard (accumulate-into-C-slot footgun) - **small**.
- Higher-order grads (grad-of-grad) - **large** (needs TENS[tid].grad rearchitected).
- LARS / real Muon - **medium**.

**Infra / memory**
- Reuse-distance memory planner + alias-friendly reduce-output layout - **large**
  (the standing ~7x peak-mem item).
- Graph-batching maturity (Metal/CUDA) - **large**.

**Cleanliness (the "clean" target, cross-cutting)**
- Env-knob sprawl (`thvm_vs_tinygrad.md` section 5.1 lists ~80 thvm knobs; many are
  one-off A/B toggles that should be retired or promoted to real options).
- Transient comments / dated attributions / "REVISED:" notes in source (violates
  project style; sweep as touched).
- `render_uop.c` is ~4.7k lines; the reduce-emission path accreted special cases
  (the cap fixes, FUSE_CONV_BWD strand hacks) - candidate for extraction/refactor
  once M2/M3 retire the hacks.

---

## 3. Milestones (audit -> close -> clean -> verify; refactor woven in)

### M0 - quick wins + first cleanup pass (small; do first)
- Add the missing backward rules to `interact/uop_grad.c` (SIN/COS, POW, MAX, WHERE,
  CONTIGUOUS, COPY) mirroring `gradient.py:49-75`. Verify: per-op finite-difference
  grad-check vs tinygrad.
- Add a `zero_grad` guard in `py/thvm/tensor.py:backward()` (warn if a requires_grad
  leaf enters with non-zero C-side grad). Verify: a test that omits zero_grad warns.
- Clean: sweep transient comments out of the files touched this session
  (`rangeify_unified.c` ru_seed_boundary_holds, `render_uop.c` cap sites). Retire any
  dead one-off debug knob encountered.

### M1 - expand the BEAM proposer (medium; unlocks the existing engine)
- Audit: read `propose.c` + `autotune.c` in full; confirm whether `kernel_autotune`
  fires on the py JIT first-fire (it should via `uop_kernel.c:255`; if not, fix the
  wiring). Confirm WL path already drives it.
- Close: extend `propose.c` to enumerate the full action space (UPCAST/LOCAL/GROUP/
  GROUPTOP/PADTO/SWAP; THREAD after M4) behind the existing `BEAM`/`AUTOTUNE` gate,
  mirroring `search.py:13-25` actions; reuse the existing bench/cache/winner loop.
- Clean: unify the proposer + hand_opts axis-classification helpers (today they
  duplicate axis-type logic).
- Verify: `BEAM=2` on beautiful_mnist (CUDA + Metal) measurably improves hot-kernel
  GFLOPS over the heuristic baseline; second run hits the disk cache; output parity.

### M2 - reduce-into-reduce fusion (large/medium; the named residual)
- Audit: confirm on conv-backward that two reduce kernels are emitted where tinygrad
  fuses (re-verify the v2 claim with a boundary dump).
- Close: relax the `bufferize_classify.c` reduce seed for single-consumer reduce
  chains; handle multi-REDUCE outputs in `RU_REDUCE_RANGES`; extend multi-output
  `kernel_lift` in `materialize.c`.
- Clean: this should let the `THVM_FUSE_CONV_BWD` strand hacks be deleted (verify) -
  a real beautification, not just a feature.
- Verify: faithful kernel count drops toward 120; conv-bwd output parity on both
  weight grads; grad.wlt/nn.wlt regression-clean.

### M3 - symbolic rule completion (medium)
- Audit: diff thvm's ported rules (`symbolic_rewrite.c`, `index_simplify.c`,
  `graph_simplify.c`) against `tinygrad/uop/symbolic.py` + `schedule/indexing.py`
  view-merge; list the genuinely-missing rules.
- Close: boolean-OR validity union (`rangeify_unified.c:838-862`; needs term-algebra
  bool ops), full RESHAPE/SHRINK/PERMUTE view-cancellation in `ru_compose_view_chain`,
  arange/reduce-collapse late rewrite.
- Clean: fold the FUSE_CONV_BWD RESHAPE-round-trip special case into the general
  symbolic cancellation (retire the env flag).
- Verify: split+resplit cancels to identity; conv-bwd fuses without `THVM_FUSE_CONV_BWD`.

### M4 - opt-section completion (medium)
- Full-extent UNROLL renderer fix (`rmu_emit_store_reduce`); mask-UPCAST WHERE walker;
  THREAD axis + CUDA warp-shuffle; multi-reduce TC (`TC_OPT>=1`). Each: audit -> impl ->
  clean the touched render path -> verify GFLOPS/parity. Feed THREAD into M1's proposer.

### M5 - memory planner + graph batching (large)
- Reuse-distance/linear-scan buffer planner + alias-friendly reduce-output layout
  (extend the TLSF arena); mature Metal/CUDA graph batching. Ties to the standing ~7x
  peak-mem follow-up. Verify: peak working-set ratio vs tinygrad; dispatch-count delta.

### M6 - optimizers + higher-order grads (independent track)
- LARS + real Muon (Newton-Schulz) in `py/thvm/optim.py`; then rearchitect
  `TENS[tid].grad` as a differentiable Tensor for grad-of-grad + FUNCTION/grad_fxn.

### M7 - backend breadth (large; lowest priority)
- CL/ROCm vtable; MultiBuffer/MSELECT/MSTACK multi-device.

**Cross-cutting clean track (run continuously):** knob audit (retire/promote one-off
toggles; document the survivors in one place), transient-comment sweep, and a
`render_uop.c` reduce-emission refactor once M2/M3 retire the strand/cap hacks.

**Ordering rationale:** M0 (cheap correctness + first clean) and M1 (unlock the
already-built BEAM) give the most parity-per-effort. M2 is the named fusion residual and
its cleanup deletes hacks. M3 unlocks symbolic-dependent wins and retires FUSE_CONV_BWD.
M4-M5 are incremental opt/memory parity. M6-M7 are independent tracks.

---

## 4. Status log
(appended as milestones land; each entry rides with its code commit per the
no-standalone-doc-commit rule.)

### M0 + M1 audit (2026-06-01) - findings reshape the plan

**M0 (backward rules / zero_grad) = mostly NON-ISSUES (4th+ v2-report correction).**
- `uop_grad.c` already has EXP2/LOG2 backward (lines 1024/1040) + ADD/MUL/NEG/RECIP/
  SQRT/REDUCE/movement. thvm has NO primitive UOP_SIN/COS/MAX/MIN/WHERE/POW; Tensor
  `maximum`/`minimum`/`relu` DECOMPOSE to CMPLT+MUL+ADD, so their grads flow through
  existing rules. There are NO missing backward rules. `sin`/`cos`/`pow`/`where` are
  absent Tensor *features* (decomposed adds if wanted), not autodiff gaps.
- `zero_grad` warning guard: SKIPPED - it would false-positive on legitimate
  gradient-accumulation-over-microbatches; tinygrad doesn't warn either. The benches
  already use opt.zero_grad (bb24342d).

**M1 (BEAM) - root cause is a WIRING BUG, not coverage, and a heuristic finding.**
- Added `THVM_AUTOTUNE_TRACE` (autotune.c). Confirmed: BEAM writes 0 cache entries on
  the py beautiful_mnist Metal run; `kernel_should_autotune` returns 0 BEFORE proposing.
- **THE BUG:** `kernel_hand_coded_opts` sets `ke->schedule->autotuned = 1` at its start
  (`hand_opts.c:405`), and `uop_kernel.c` runs hand_opts THEN `kernel_should_autotune`
  (which early-returns on `autotuned` already set). So **hand_opts blocks BEAM on every
  non-WL path** - exactly the "BEAM only in WL" symptom (the WL entry calls
  `kernel_autotune` directly, bypassing the gate). Confirmed: `NOOPT=1 BEAM=2` makes
  the autotune trace fire (328 kernels). Fix: give hand_opts its own `hand_coded_done`
  flag, leave `autotuned` to autotune.
- **Proposer is narrow:** with autotune firing (NOOPT+BEAM), 240/328 kernels get
  n_cand=0 (only ~88 reduce-tail kernels propose UNROLL). Needs expansion to the full
  action space (UPCAST/LOCAL/GROUP/GROUPTOP/PADTO/SWAP).
- **HEADLINE: hand_opts is NET-NEGATIVE on Metal/M3 Max (post-bench-fix).** Stable warm:
  default(hand_opts) ~3.8ms, NOOPT ~2.1ms, NOOPT+BEAM=2 ~1.8ms. The heuristic makes
  Metal beautiful_mnist ~1.8x SLOWER. Bisect: no single opt is the culprit
  (GROUPTOP-off 3.60, floor-off 3.52, both-off 3.60, upcast-cap-4 3.59) - it's the
  UPCAST+LOCAL+GROUPTOP *combination*; full NOOPT is needed to recover 2.1ms. The
  heuristic was tuned for CUDA on the gradient-accumulating (buggy) bench; the GROUPTOP
  "2.2x" claim in hand_opts.c needs re-measurement. (CUDA re-eval pending GPU.)

**Reshaped M1 (do in this order):**
1. Fix the `autotuned`/`hand_coded_done` flag conflict so BEAM can run on the py/JIT
   path (small; the highest-value unblock). Make autotune ADDITIVE on the hand_opts
   baseline (don't `axes_reset_to_default`) OR make BEAM replace hand_opts when BEAM>0
   (tinygrad model) - decide by measurement.
2. Re-evaluate hand_opts on Metal (and re-measure on CUDA with the fixed bench): the
   net-negative result means the heuristic's factor/axis choices are mistuned for the
   Metal threadgroup model. Either retune or prefer BEAM on Metal.
3. Expand `propose.c` to the full action space so BEAM covers the 240 no-candidate
   kernels.
Verify each on Metal + CUDA warm with the zero_grad bench; correctness via step-0
parity; never regress default below NOOPT.

### M1 step 1 LANDED (2026-06-01): hand_coded_done/autotuned flag split

Split the shared `autotuned` flag: `kernel_hand_coded_opts` now sets its own
`hand_coded_done` (thvm.h KpSchedule) instead of `autotuned`, and
`kernel_should_hand_code_opts` checks `hand_coded_done`. autotune keeps `autotuned`.
This unblocks BEAM/autotune on the py/JIT path (previously hand_opts' `autotuned=1`
suppressed it on every non-WL path - the "BEAM only in WL" bug).

Verified (Metal M3 Max, beautiful_mnist, BS=8, zero_grad):
- default BEAM=0: 3.57ms (UNCHANGED - the fix is behavior-neutral when BEAM off).
- default BEAM=2: **2.19ms** (1.6x faster; autotune now fires: 328 kernels evaluated
  vs 0 before, 84 with candidates, 27 cache entries written).
- Correctness: BEAM=2 fwd-sum 36021.043 vs BEAM=0 36021.035 (fp reduction-order); CPU
  default step0 loss 2.3536 unchanged.

Remaining M1: (a) expand `propose.c` so the 240/328 no-candidate kernels get tuned
(would close 2.19->~1.8ms, the NOOPT+BEAM floor); (b) re-evaluate hand_opts on Metal
(net-negative) + CUDA (pending free GPU) with the fixed bench; decide additive-BEAM vs
replace-on-Metal.

### M1 step 2 LANDED (2026-06-01): expand the Metal BEAM proposer

Added a general Metal candidate block to `propose.c`: UPCAST{4,2}/LOCAL{32,16,8} on
output LOOP axes + GROUPTOP{16} on the reduce axis, for EVERY Metal kernel (previously
only TC/conv2d-tile/reduce-tail special cases proposed anything, so 240/328 kernels got
0 candidates and BEAM no-op'd them). The opts are correctness-preserving axis transforms;
`kernel_apply_tune_candidate` skips inapplicable ones and the time-only bench keeps a
variant only when faster, so it self-filters to no-op where the hand-coded-style opts
would slow the kernel (the common case on Metal).

Verified (Metal M3 Max, beautiful_mnist BS=8, zero_grad, BEAM=2): warm_min 2.07ms
(was 2.19ms with the partial proposer; default BEAM=0 is 3.57ms -> 1.7x), candidate
coverage 84 -> 206/328 kernels, 73 cache entries. Correctness: BEAM=2 fwd-sum matches
BEAM=0 within fp (11098.66 vs 11098.67); no GPU orphan (recoveryCount=0); CPU/CUDA
propose paths unchanged (the new block is Metal-gated).

M1 status: BEAM is now usable + effective on the py/JIT path for Metal (3.57 -> 2.07ms).
Remaining: the 122 still-uncovered kernels (no divisible LOOP axis >= the factors);
the default-policy decision (hand_opts is net-negative on Metal, so default should
perhaps prefer BEAM/NOOPT on Metal) + CUDA hand_opts re-eval -- both pending a free
CUDA pod. Next: M2 (reduce-into-reduce fusion) is CPU-verifiable and not GPU-gated.

### M2 audit (2026-06-01, post-restart): the kernel-count gap is NOT reduce-into-reduce

Re-scope. Audited the faithful beautiful_mnist (CPU, BS=8) boundary structure:
- 207 boundaries total. Top-op: ADD 111, MUL 44, RESHAPE 26, REDUCE **only 14**, RECIP
  6, CMPLT 4, PERMUTE 2. So the gap is ELEMENTWISE, not reduce-chains -- M2's
  reduce-into-reduce hypothesis (from v2/the rangeify_unified.c:160 comment) is WRONG.
- forward+backward ALONE = 123 boundaries ~= tinygrad's TOTAL of 120. So thvm's fwd+bwd
  fusion is already competitive; the optimizer adds the other 84 (207-123).
- Reasons: 114 ROOT (0x1; inherent STORE/ASSIGN -- tinygrad seeds these too) + **74
  walk-realized (0x0)** + a few REDUCE/MULTI. The 74 walk-realized are the gap (tinygrad
  fuses them into consumers; thvm's rangeify walk realizes them).
- Root mechanism: thvm's consumer-divergence realize (rangeify_unified.c:838-862)
  lacks tinygrad's boolean-OR-of-valids merge (indexing.py:211-213) -- when consumers
  diverge thvm realizes per-axis where tinygrad merges valids and stays a view. The
  ending-ranges PCONTIG path (864-888) is NOT the gap: tinygrad's PCONTIG defaults to 0
  (helpers.py:254), so its default also realizes-all there -- thvm is already faithful.

**M2 status (landed):** `THVM_FUSE_REDUCE_INTO_REDUCE` is now **default-on** (the multi-axis
REDUCE renderer landed). It is faithful + bit-exact (CPU; Metal ~1e-7 float-reorder) and a
small win (attn 8->7, qwen 26->25/layer, FLUX double-block 72->70) -- confirming the audit:
reduce-into-reduce was NOT the main gap.

**M3 status (range-side OR-of-valids is a DEAD END):** prototyped the boolean-OR-of-valids
merge faithfully (port of indexing.py:206 `get_idx` + :217-218 `usum(valids).where`): compare
consumer ranges by `get_idx` (strip the IWHERE guard) and OR the validity conds in
`ru_merge_axis`. Result: no-valid chains stay byte-identical (attn/qwen unchanged) and
conv+maxpool backward stays correct, BUT the merge **fires 0 times** -- because thvm's
PAD/SHRINK validity is **value-side** (a WHERE on the node VALUE, line 921), not carried on
the consumer RANGE Term. So the range-side OR-of-valids has nothing to merge. The real M3 must
handle the value-side valid union, OR the 74 walk-realized diverge on the INDEX (which both
engines realize). Reverted the dormant range-side prototype; INDEX-vs-valid attribution of the
74 walk-realized is the prerequisite before re-attempting.

**M3 re-diagnosed (the gap is CODEGEN grouping, not the rangeify realize decision):**
instrumented tinygrad's `realize_map` (schedule/indexing.py:225,235) on the {1,4,16,8} attention.
tinygrad REALIZES the softmax EXP2, the normalize MUL, the RECIPROCAL, and the reduces -- it does
NOT recompute them (correcting both the doc's "boolean-OR-of-valids" hypothesis and the
recompute hypothesis). Yet it emits only 4 kernels because its linearizer packs each realized
elementwise boundary together with its adjacent REDUCE into ONE kernel (reduce-prologue/epilogue
in-kernel). thvm realizes the same nodes (7 kernels post-INTO_REDUCE: qk, scale, max, exp, sum
into av, normalize, head-merge reshape) but its one-reduce-per-kernel codegen splits them.
The EXACT missing pass is tinygrad's **`remove_bufferize`** (schedule/rangeify.py:242-309), run
AFTER the realize_map marks boundaries.  Its cost model: a bufferize is REMOVED (its source
substituted back into each consumer = recompute) UNLESS its computation contains a REDUCE that
reads a buffer (`buffer_in_reduce`, rangeify.py:280-303) -- then it is KEPT.  On the attention:
`scores` (qk-matmul) contains a reduce reading q/k buffers -> KEPT; the softmax `exp`/`*scale`/
`normalize` are elementwise over already-realized buffers with NO reduce inside -> REMOVED ->
recomputed inline into each consuming kernel.  thvm's rangeify WALK marks the same realizes
(via consumer-divergence) but has NO remove_bufferize, so it keeps them all (7 kernels vs 4).
thvm already computes the predicate (`bufferize_elementwise_src_has_reduce`,
bufferize_classify.c:994 -- false for the exp) but only consults it in the dropped MULTI seed,
never to un-realize a WALK-realized node.  THE PORT: a post-walk remove_bufferize pass that
un-marks RU_REALIZE_MAP for a removable elementwise node whose source has no buffer-reading
REDUCE, letting materialize inline (recompute) it per-consumer.  This is a new substitution
pass touching every multi-consumer node -- the substantial remaining rangeify piece.

**remove_bufferize PROTOTYPE landed (rangeify side done; materialize side is the blocker):**
implemented the pass in `run_rangeify_unified` (after the walk, before `pm_apply_rangeify`),
gated `THVM_REMOVE_BUFFERIZE` (DEFAULT-OFF).  The selection predicate `rb_src_has_live_reduce`
is the faithful `buffer_in_reduce` (rangeify.py:256-285): walk the node's source, STOP at any
realized boundary (a realized reduce = a buffer, like tinygrad's red_gate stopping at
AFTER/STAGE), keep only if a NON-realized REDUCE is reached.  It fires on EXACTLY the right
nodes -- the softmax `*scale`/`exp`/`normalize` (realized scores/max, no live reduce) -> removed;
the qk/av matmul (live reduce) -> kept.  Attention 7 -> 6 kernels.  BUT the output is ~5e-3 off
the oracle: clearing the realize flag makes the materializer INLINE the node with the wrong
index (the recomputed `exp` loses its per-row `max` reference).  tinygrad's pass does an
EXPLICIT per-consumer index substitution (rangeify.py:308-309
`src.substitute({buf.src[1:] -> idx.src[1:]})`); thvm's materializer does not re-index an
inlined un-realized node automatically.  THE REMAINING BLOCKER is materialize-side: either
substitute the bufferize's source into each consumer with that consumer's INDEX ranges at
removal time, or teach materialize to re-derive an inlined node's index from its consumer.
That is the next concrete step -- the rangeify selection is done and correct.

REVISED milestone ordering for the kernel-count/memory gap (the goal is memory parity;
thvm already wins wall-time on both backends):
- The real lever is the **boolean-OR validity union** (was M3) + the **optimizer
  fusion** (84 boundaries; needs multi-output kernels / a FUSE_OPTIM analog -- ties to
  M5 graph-batching). Both are LARGE: the OR-merge needs term-algebra boolean ops;
  optimizer fusion needs multi-output kernel_lift. M2-as-reduce-into-reduce is retired.
- fwd+bwd fusion is NOT a priority (already ~= tinygrad).

Infra: added `tools/bench_train.py` -- the stable warm-train harness (simple/beautiful,
DEV, BS, faithful, BEAM, zero_grad) replacing the per-session /tmp scratch scripts.

### Optimizer-fusion audit + parity guardrail (2026-06-01)

Audited the Adam optimizer's boundary structure (simple model, 6 params): 79 boundaries
total, 50 without the optimizer -> 29 from the step. Each m/v/p update is ALREADY a
single ADD boundary (the in-place assign reads m + g and writes m in one kernel; the
RHS is NOT split). So the optimizer is near-minimal PER-PARAM (3 boundaries/param); the
only remaining fusion lever is CROSS-PARAM (fuse all 16 m-updates into one kernel, etc.)
which requires multi-output kernels -- a `kernel_lift` that emits N independent stores.
So optimizer fusion is LARGE infrastructure, not a contained intermediate-fusion win.

**Conclusion of the contained-win sweep:** every remaining lever is large infrastructure
for MEMORY parity (thvm already wins wall-time on both backends): boolean-OR validity
merge (needs term-algebra bool ops), multi-output kernels (optimizer + reduce fusion),
reuse-distance memory planner, THREAD/warp-shuffle. These are deliberate, verified,
multi-session builds -- not autonomous loop-chunks -- because they touch the rangeify
walk / kernel_lift that the hard-won faithful correctness depends on.

Added `py/tests/test_faithful_parity.py` -- a faithful<->default forward+grad parity
guardrail (subprocess per seed; step-0 loss + conv1 weight-grad ssq within fp). This
locks in the correctness invariant so the future deep memory-fusion work can be verified
against a fast regression check, not just the full WL suite. Passes (2.3s).

### Kernel-count audit CORRECTION + the real gap is CODEGEN, not fusion (2026-06-01)

Measured the actual per-step kernel count three ways, apples-to-apples (CPU, BS=8, full
fwd+bwd+Adam, 14 params), to settle the OR-merge question before building it. The result
overturns the prior re-scope:

- **The "207 vs 120" 2x kernel gap was a measurement artifact.** A clean EAGER realize
  (`kernel_count()` delta, no JIT) gives thvm **faithful 113**, **default 164**,
  **tinygrad 104**. Faithful is within 9% of tinygrad; the forward pass alone fuses
  MORE than tinygrad (thvm 13 vs tg 16). The bench harness's "207" UNIONs the
  eager-built kernels with the JIT-capture-rebuilt set (~2x) plus the slot-assign /
  zero_grad kernels -- that duplication is JIT dispatch redundancy
  ([[project_thvm_jit_dispatch_redundancy]]), NOT a fusion gap. The M2 audit's tinygrad
  baseline of 120 was bogus (real tinygrad fwd+bwd=46, +Adam=58, total=104).
- **OR-merge is NOT a kernel-count lever -- REFUTED.** At the multi-consumer divergence
  site, tinygrad with default `PCONTIG=0` (helpers.py:254) collapses the per-axis
  decision to a single `all_all_same` flag (indexing.py:208): either keep a FULL view
  (all consumer indices match -> merge valids) or FULL-realize every axis. thvm
  (`rangeify_unified.c:845-861`) PARTIAL-realizes only the diverging axes, so it fuses
  AT LEAST as much as tinygrad's default there. The boolean-OR-of-valids merge only
  affects WHICH valid the shared view in the all-same case carries -- a correctness
  nuance thvm already handles correctly by deferring valids to address-build time
  (`movement_index.c` IAND'd ILT masks), not a realize-count win. Built nothing; the
  audit (temporary `THVM_RU_REALIZE_TRACE`, since removed) showed thvm's divergence
  realizes are genuinely-different indices, which tinygrad realizes too.

**The real "faithful means FAITHFUL incl. performance" gap is CPU CODEGEN, surfaced by
the same measurement (warm wall, BS=8, beautiful_mnist):**
- thvm **default 7.36ms** (164 kernels) -- BEATS tinygrad.
- tinygrad **15.09ms** (104 kernels).
- thvm **faithful 238ms** (113 kernels) -- 16x SLOWER than tinygrad despite ~= kernel
  count. One fused conv kernel runs ~81ms/fire at ~2 GFLOPS single-threaded.

Mechanism: thvm's default seed keeps convs as MATMUL, routed to Accelerate `cblas_sgemm`
(`src/backend/cpu/blas.c`, "10-100x speedup") -- that BLAS path is why default beats
tinygrad. The FAITHFUL seed fuses the matmul into elementwise+reduce, which the DAG-side
GEMM classifier no longer recognises, so it renders as a naive triple-nested C loop with
no UPCAST/UNROLL/vectorization. tinygrad runs ITS equally-fused schedule at 15ms because
its CLANG codegen applies hand opts (UPCAST/UNROLL) that vectorize the C loop; thvm's
faithful path does not. So:

**Re-scope (supersedes the "every lever is large memory infra" conclusion):**
- Kernel-count/fusion parity is **essentially MET** (faithful 113 ~= tg 104). Stop
  chasing it. Retire the OR-merge and multi-output-for-kernel-count items.
- The genuine remaining gap is **faithful-kernel CPU codegen quality**: extend
  hand_opts/`propose.c` (M1/M4) to apply UPCAST/UNROLL to the big fused conv kernels so
  the non-BLAS path vectorizes, closing the 238ms -> ~15ms gap. This IS the
  performance half of "faithful." CPU-verifiable, not GPU-gated.
- Standing-separate: peak-memory buffer reuse (~7x, [[project_memory_parity_followup]])
  and JIT dispatch redundancy -- both real, both independent of rangeify fusion.

Harness: `tools/bench_train.py` now reports `sched_kernels` (the honest eager
`kernel_count()` delta) alongside warm wall, so this measurement error can't recur.

### Faithful-CPU codegen root-caused: it's index arithmetic + gather, NOT missing opts (2026-06-01)

Pursued the faithful-CPU gap (238ms vs tinygrad 15ms). Mapped the CPU backend
(`src/backend/cpu/{jit,blas,interpret,uop_walk}.c`): three routes -- cBLAS first
(`blas.c`, the six `uop_dag_classify_*` shapes), then a clang `-O3` C-codegen JIT
(`jit.c:216`), then a scalar UOP-walker interpreter. There is NO thread parallelism off
the BLAS path and NO explicit SIMD. `hand_opts.c:417` gates ALL hand opts off for CPU
(rationale: opts mutate the DAG and defeat the cBLAS classifiers).

Hypothesis tested (agent, worktree, CPU): un-gate the renderer-safe UNROLL/UPCAST for
the NON-BLAS-eligible CPU kernels (the fused convs, which never reach cBLAS anyway). The
gate change is correct -- parity-safe, default-mode unaffected (BLAS kernels stay bare) --
but **UPCAST/UNROLL do NOT help (~3.6%, run-to-run noise)**. Two findings:
- **The real bottleneck is the index/gather, not float throughput.** The dominant fused
  kernel (conv1+bias+relu -> conv2 -> maxpool, one kernel) has inner loops full of
  `IDIV/9`, `IMOD/9`, `<27` bounds, and chained `? : INVALID` validity ternaries feeding
  STRIDED gathered loads (`in4[a9*576+..]`, `in5[a9*36+..]`). clang `-O3` cannot vectorize
  gathered loads under data-dependent masks; UPCAST just adds `#pragma unroll`. A
  microbench hoisting the IDIV/IMOD by hand moved only 20.8 -> 19.0ms/iter. So the
  prerequisite is **index-simplification** (collapse the redundant per-element
  IDIV/IMOD + hoist/loop-restrict the validity masks instead of per-element ternaries)
  AND loop-ordering to expose a unit-stride inner loop -- exactly what tinygrad's
  symbolic.py + opt search do to run the SAME fused schedule at 15ms.
- **Bug surfaced (recorded, not hidden):** the Section-7 split-by-4 reduce UNROLL
  MISCOMPILES on the CPU reduce path (faithful loss 2.3228 vs 2.3486 reference) -- the
  same renderer reduce-lane defect already documented for full-extent UNROLL
  (`hand_opts.c:755-762`), but it also hits the split-by-4 case on CPU, not just
  full-extent on GPU. So the documented "RENDERER LIMIT" is broader than the comment
  claims. The reduce-lane matcher in `rmu_emit_store_reduce` reads the fused reduce body
  through the wrong accumulator lane after an UNROLL.

The gate change was REVERTED (perf-neutral churn; commit-only-verified-wins). Re-apply
recipe when index-simplification lands: add `hand_opt_kernel_blas_eligible(ke)` (OR of the
six `uop_dag_classify_*_shape`), and at `hand_opts.c:417` route non-BLAS CPU kernels
through a UPCAST-only `hand_opt_cpu_vectorize` (NOT reduce-UNROLL until the reduce-lane
bug is fixed).

**Sharpened next lever (supersedes "extend hand_opts to vectorize"):** index/address
simplification for the fused conv kernels -- diff `src/uop/index_simplify.c` +
`symbolic_rewrite.c` against tinygrad `uop/symbolic.py` (IDIV/IMOD fold under known
bounds) and `schedule/indexing.py` valid-mask handling; render validity as loop-range
restriction, not per-element ternary; hoist loop-invariant index subexpressions to named
locals at the right loop depth. Plus fixing the reduce-lane UNROLL bug to unlock the
UNROLL half. All large; CPU-verifiable; gated by the parity guardrail.

### Memory audit: thvm HAS a linear-scan planner; the gap is the multi-pass realize loop (2026-06-01)

Audited the peak-memory item (M5 / [[project_memory_parity_followup]]) before porting
anything. thvm ALREADY has a linear-scan buffer planner: `materialize.c:789-915` computes
per-boundary lifetimes (`first_pos`/`last_pos`), builds open/close events, and runs a TLSF
suballocator (`src/schedule/tlsf.c`) that allocs at open / frees at close, so
non-overlapping buffers share bytes. So the follow-up's "need linear-scan reuse in the
scheduler" is STALE -- it exists. `THVM_ARENA_DUMP=1` (standalone eager, CPU, full step):
- default: a pass with 25/70 boundaries plannable, sum 12.82MB packed to 12.25MB arena
  (only ~4.5% reuse) + 45 legacy allocs.
- faithful: 2/13 plannable, 0.10MB arena + 11 legacy.

The peak is dominated by NON-plannable (legacy, never-recycled) buffers. `arena_boundary_is_plannable`
(`materialize.c:721`) forces legacy when: (A) inside a JIT-capture dedup span -- so the
JIT BENCH gets ZERO arena planning (line 733); (B) the boundary is a realize root/output
(`BOUNDARY_LAST_USE==0`, correct, matches tinygrad memory.py:22); (C) `consumer_count != 1`
(line 751). (A) and (C) share ONE root: `thvm_realize` runs `materialize` in a LOOP of
passes; per-pass `BOUNDARY_LAST_USE` can't see a buffer read by a LATER pass, so anything
that might cross a pass boundary is forced legacy for safety. `arena_last_pos_at_depth`
(line 778) further OVER-estimates within-pass lifetimes (depth-bucketed, not exact
consumer position), which is why even plannable buffers show peak~=sum.

**This is the same root as the JIT dispatch-redundancy item** (the capture-vs-eager kernel
duplication, [[project_thvm_jit_dispatch_redundancy]], also from the multi-pass model).
tinygrad's SINGLE-PASS schedule co-plans every buffer's lifetime across the whole step,
which is why its peak is ~flat (~1MB vs thvm 14.8-31MB). So the real M5 fix is **step-global
buffer lifetimes / collapsing `thvm_realize`'s loop to a single pass** -- large,
architectural, correctness-critical (the realize driver), and it closes BOTH peak-memory
AND JIT-redundancy. NOT "add a planner" (have one) and NOT a rangeify/fusion change.

### Meta-assessment after 3 audit ticks (2026-06-01)

The kernel-count (tick 1), faithful-CPU (tick 2), and memory (tick 3) audits converge on
one conclusion: **thvm is at or near tinygrad parity on every achievable axis** -- it WINS
warm wall on CPU/CUDA/Metal in default mode, matches tinygrad's kernel count within 9% in
faithful mode, and is numerically correct (parity guardrail). The three residual gaps are
all LARGE architectural arcs on correctness-critical paths, and two share a root:
1. **Faithful-CPU codegen** (index-simp + loop-reorder for fused convs; the gather/IDIV
   bottleneck). Narrow benefit (you'd use default on CPU, which already beats tinygrad).
2. **Peak memory** -- step-global buffer planning (collapse the realize loop).
3. **JIT dispatch redundancy** -- same multi-pass-realize root as #2.
Highest leverage: the **single-pass / step-global realize** change (closes #2 and #3). It
is the right next arc, approached incrementally + parity-gated. None of the three is a
quick win; the v3 plan's milestone framing understated their architectural depth.

### Faithful-CPU lever PRECISELY located: ShapeTracker view-cancellation (M3), with evidence (2026-06-01 tick 4)

Two refinements that re-rank the gaps:
- **Memory is a NON-lever for faithful (closed).** `THVM_ARENA_DUMP_BUFS` shows faithful
  has ZERO buffers >=1MB -- it fuses the fat activations away (no materialized 1.77MB
  conv1-out). The 11MB peak is many small buffers + pool high-water across the 3 separate
  realize calls, NOT avoidable activations. So faithful memory is near-optimal; the
  step-global work would mainly help DEFAULT mode (the intentional materialize-for-BLAS
  tradeoff). De-prioritized.
- **The faithful-CPU 16x is the conv index form, and the fix is M3.** Dumped tinygrad's
  fused-conv CPU kernels (DEBUG=4): `r_8_8_2_8_12_3_4_5_5` etc. have CLEAN AFFINE indexing
  (no IDIV/IMOD, no `<27 ? : INVALID` masks) and a VECTORIZED accumulator `float acc0[12]`
  / `acc0[16]` (UPCAST into SIMD lanes). thvm's faithful conv kernel (tick 2) is the
  opposite: `(a2+8*a4)/9`, `%9`, per-element validity ternaries, strided gathers. So
  tinygrad collapses the conv window (reshape/permute/shrink chain) to an AFFINE strided
  view via ShapeTracker merge; thvm's `ru_compose_view_chain` does not, leaving the
  reshape's IDIV/IMOD -- which blocks clang vectorization regardless of UPCAST (why tick
  2's UPCAST/UNROLL experiment was ~noise). **The lever is M3 ShapeTracker
  view-cancellation** (collapse the conv movement chain to affine), THEN un-gate CPU
  UPCAST for the now-affine kernel. M3 is a DEFINED tinygrad port (View merge /
  ShapeTracker.simplify -> `ru_compose_view_chain`, rangeify_unified.c:1255-1337 +
  `symbolic_rewrite.c`), not a vague "index-simp" and not the step-global re-architecture.

**Re-ranked next arc: M3 ShapeTracker view-cancellation** -- it is the highest-value lever
that is BOTH a real measurable win (faithful-CPU 238ms -> toward tinygrad 15ms) AND a clean
tinygrad-spec port (medium, parity-gated), unlike the step-global re-architecture (large,
correctness-critical, low value now that faithful memory is near-optimal).

### M3 narrowed to the rangeify conv-backward LOWERING, not symbolic recombine (2026-06-01 tick 5, LANDED + decisive)

Ported tinygrad's `fold_add_divmod_recombine` (symbolic.py:28-49) into thvm's
`index_simplify.c` -- generalised thvm's narrow `(r//M)*M + (r%M) -> r` rule (mul=1,
two-direct-operand) to the full multi-term ADD split with the `*mul` scaling + nested-IDIV
+ nested-MOD cases. Verified value-exact (parity bit-identical loss; grad-ssq within ~1e-7
reduction-order; index_simplify 89/89 incl. the dedicated recombination tests, grad 68/68,
cpu_jit 342/342, rangeify 45/45, movement_index 51/51). It FIRES 200+ times/2-steps on
real index sums -- a genuine spec-faithfulness + cleanliness win (subsumes the narrow rule)
-- BUT it does NOT fix the faithful conv-backward (IDIV/IMOD 112->104; the 5 dominant
col2im kernels unchanged at 16 each; warm ~2% = noise).

**Why (decisive, cross-checked against tinygrad's own CPU lowering):** thvm's faithful
conv-BACKWARD is lowered as an INVALID-gated col2im GATHER -- the `//div` decode lives in
one LOAD's address sum and the `%div` decode in a DIFFERENT LOAD's (separate input arrays),
so `fold_add_divmod_recombine` (single `split_uop(ADD)`) can never pair them, and the base
is `IWHERE(cond, p, INVALID)`-masked. tinygrad lowers the SAME conv-backward as a clean
strided reduce (`r_2_5_5_16_8_24_24`, <=2 div/mod per kernel) -- it does NOT col2im-gather.
So the faithful-CPU 16x is the **rangeify conv-backward lowering** (masked gather vs strided
reduce), a rangeify/movement-lowering change, NOT the symbolic recombine (now ported) and
NOT view-merge (the prior tick's framing -- superseded). This is the deepest localization:
the lever is how rangeify lowers the conv-backward col2im, correctness-critical, and on a
met goal with narrow benefit (default+GPU faithful are fine). Recombine port stands as a
faithful-spec improvement; the conv-backward-lowering rewrite is the (large, deferred) rest.

### SESSION CONCLUSION (2026-06-01): the faithful IDIV/IMOD is STRUCTURAL (1-D flatten vs N-D)

Final root cause, proven: every faithful conv IDIV/IMOD (fwd fused conv+maxpool AND
conv-backward) is `(spatial_lo + stride*spatial_hi) / W` then `% W` -- thvm FLATTENS a 2-D
spatial coordinate into a 1-D index somewhere in the `_pool`/window movement chain, then
the consumer DECODES it back (IDIV/IMOD). The encode is coprime with the window stride, so
it is NOT recombinable (the landed `fold_add_divmod_recombine` doesn't touch it) and NOT
bounds-foldable -- even tinygrad's full `divandmod.py` (nest-by-factor, divide-by-gcd,
factor-remainder, vmin/vmax folding) would not fold it. tinygrad AVOIDS it structurally: it
keeps the window position as a SEPARATE N-D reduce axis (never forms `spatial_lo +
stride*spatial_hi`), so its conv kernels are affine + `acc0[12]`-vectorized. The fix is to
keep spatial dims N-D through the rangeify `_pool`/window lowering (a movement-chain
composition that preserves separate strided axes instead of flatten+decode) -- a structural
rangeify change, correctness-critical, NARROW benefit (default WINS CPU via BLAS; GPU
faithful is competitive; only faithful-CPU is slow), on an already-MET goal. A separately
useful but infra-blocked item: thvm has NO uop vmin/vmax bounds tracking, so porting
tinygrad's bounds-based div/mod rules needs a bounds-inference pass built first.

### DE-RISKING REFRAME (2026-06-01, later same session): the fix is SYMBOLIC completion, not a rangeify rewrite

Read tinygrad's actual rangeify (`schedule/indexing.py`). `_apply_reshape` (line 112-125)
does the SAME flatten+decode thvm does -- `combined_axes = usum(acc_k*src_k)` then
`combined_axes % s` / `//= s` (IDIV/IMOD) -- and `remove_movement_op_after_rangeify`
(line 98) just deletes the movement node post-rangeify. The affine conv kernels come from
line 124-125, quoted verbatim: *"this simplify is doing a lot of heavy lifting. this is the
replacement for the reshape view merging code"* -- `graph_rewrite(sink, symbolic +
pm_simplify_valid + pm_drop_and_clauses)`. So tinygrad ALSO emits the flatten/decode +
valid masks, then COLLAPSES them symbolically. The faithful-CPU fix is therefore SYMBOLIC
COMPLETION (value-preserving, parity-gated, the same pattern as the landed recombine port),
NOT the high-risk N-D rangeify restructure the prior note feared. Concrete missing pieces
vs thvm (all in `tinygrad/uop/`): (1) a uop `vmin`/`vmax` bounds-inference pass (thvm has
NONE -- the prerequisite); (2) `pm_simplify_valid` (collapse `cond ? X : INVALID` using
bounds); (3) `pm_drop_and_clauses` (drop redundant AND clauses in the compound valid
masks -- the giant `& & &` chains); (4) `divandmod.py` bounds-based folding
(nest-by-factor / divide-by-gcd / factor-remainder / vmin-vmax). Build order: (1) first
(infra, unit-testable in isolation), then (2)-(4) build on it. Still narrow-benefit on a
met goal, but LOWER-risk + DEFINED + the vmin/vmax pass is broadly useful spec
infrastructure (tinygrad uses bounds pervasively), so it advances "everything per tinygrad
spec" beyond just faithful-CPU. Supersedes the "N-D-preserving rangeify change" framing
above.

### DECISIVE NEGATIVE RESULT (2026-06-01, later): the symbolic/valid path does NOT fix faithful-CPU

Tested the de-risk hypothesis empirically. thvm ALREADY has the valid-simplification
machinery -- `uop_given_valid` (fake constrained-RANGE substitution), `drop_and_clauses`,
`gated_given_valid`, `uop_parse_valid` (index_simplify.c:1112-1340) -- gated behind
`THVM_FUSE_CONV_BWD`. Enabling it: faithful parity PASSES (loss identical, correct) but
faithful-CPU warm does NOT improve (237 -> 248ms, slightly SLOWER). So the symbolic/valid
path -- which `uop_int_bounds` (79d16c39) was built to strengthen -- does NOT deliver the
faithful-CPU speedup. The de-risk reframe ("symbolic completion suffices") was WRONG:
tinygrad's affine conv kernels come from keeping the window positions as SEPARATE LOOP
AXES (N-D: `r_8_20_5_8_4_4_32_5_5`), and index-VALUE simplification alone does not
restructure thvm's flattened single-axis loop into separate strided axes. The bottleneck
is the GATHER (memory-access structure / loop nesting), not the index arithmetic that
bounds + valid-fold simplify.

**So faithful-CPU speed genuinely needs the structural N-D loop restructuring** (rangeify
keeps the _pool window axes separate instead of flattening) -- large, narrow benefit
(default WINS CPU via BLAS; GPU faithful is competitive), correctness-critical, on an
already-MET goal whose original "faithful must be fast" steer was the resolved zero_grad
bench bug. The symbolic path is a confirmed dead-end for this. `uop_int_bounds` and
`fold_add_divmod_recombine` stand as sound, tested spec-completeness infrastructure (thvm
lacked bounds entirely; tinygrad uses them pervasively), useful for future bounds-based
work -- but they do not, and cannot alone, close the faithful-CPU gap. Recombine + bounds
were the tractable wins; the residual is the deferred structural change.

### DEFINITIVE faithful-CPU diagnosis via workflow + clang -Rpass (2026-06-01)

Ran a 4-phase workflow (map both codebases / design / implement / adversarial-verify, 10
agents). It settled the faithful-CPU root cause with clang `-Rpass-analysis` on the actual
generated conv kernel, correcting every earlier guess:

**Removing IDIV/IMOD is necessary but NOT sufficient.** The workflow ported tinygrad's
`fast_idiv`/magicgu late-rewrite (lower `x//c` -> `(x*m)>>s`, `x%c` -> `x - c*(x//c)`;
new UOP_ISHR), taking the conv kernel to idiv/imod = 0. clang STILL vectorizes ZERO loops
even with `-march=native -ffast-math`. The actual blockers, confirmed by the compiler's own
diagnostics:
1. **maxpool-fusion validity ternaries** -- thvm FUSES the maxpool into the conv2 reduce,
   so the kernel is ~31 `(cond ? val : INVALID)` masked gathers. tinygrad REALIZES a
   separate maxpool buffer (`r_2560_10_2_2`) so conv2 reads a clean affine buffer with NO
   ternaries.
2. **scalar accumulator** -- thvm emits one scalar `_acc`; clang won't reorder the FP
   reduction. tinygrad register-blocks it (`float acc0[16]`, from UPCAST) so independent
   lanes vectorize without fast-math.

**So the real perf levers (evidence-backed) are TWO structural changes, both previously
dismissed:**
- (A) **Realize the maxpool separately** so conv2 is an affine reduce with no validity
  ternaries. This is FAITHFUL -- tinygrad's own realize-map does it; thvm's faithful seed
  + walk OVER-fuses it. (A rangeify/seed change at the maxpool boundary.)
- (B) **Register-block the conv accumulator** = un-gate CPU UPCAST (hand_opts.c:417 gates
  ALL opts off for CPU; tick-2 found UPCAST "doesn't help" -- but that was BECAUSE the
  ternaries (A) blocked vectorization; with A done, B's `acc0[N]` should vectorize).
Neither alone helps (tick-2 proved B-alone is a no-op; fast_idiv proves arithmetic-alone is
a no-op). A + B + the affine arithmetic (fast_idiv) together are what tinygrad has. NEXT
ARC: implement A then B, measuring vectorization (clang -Rpass) + warm at each step.

**fast_idiv/magicgu LANDED as a prerequisite leg** (not the fix): faithful tinygrad codegen
(it emits `(x*21)>>9` for `x//25`), correct (the adversarial-verify caught a silent
magicgu-degenerate `m=1,s=0` mis-lowering of a gated numerator bounded below the divisor --
`x//c -> x` instead of 0; fixed with a `hi < bv -> exact` guard + regression test), all
suites green (grad 68, cpu_jit 342, rangeify 45, pool_im2col 13, index_simplify 172),
parity-exact, perf-neutral (~1%, expected). It is the affine-arithmetic leg that matters
once A+B unblock vectorization.

### LEVER B LANDED: CPU UPCAST register-blocking -> faithful-CPU 2.9x (2026-06-01)

A second workflow (map / implA / implB / adversarial-verify) settled the A+B plan with
clang-`-Rpass` + a hook on tinygrad's own renderer:
- **Lever A (realize maxpool) was correctly SKIPPED.** Post-`fast_idiv` the forward convs
  are ALREADY ternary-free (the realize-map already matches tinygrad). The only
  ternary-bearing kernels are the backward maxpool/conv-grad col2im scatters -- and
  tinygrad's equivalent has MORE of them (its dominant kernel `r_2_8_2_24_12_4_4_32_6_6`
  has 136 ternaries + `acc0[96]`). So removing them would DIVERGE from tinygrad, not match
  it. tinygrad's speed is 100% register-blocking, not ternary elimination.
- **Lever B LANDED (the real fix):** un-gate CPU UPCAST for NON-BLAS-eligible reduce
  kernels (`hand_opts.c`: `hand_opt_cpu_blas_eligible` mirrors the cpu_blas_dispatch ladder
  so cBLAS kernels stay bare) + register-block the accumulator in the generic store path
  (`render_uop.c`: `RmuLaneBlock` -> N independent `_acc<axis>_<k>` lanes sharing each
  reduce loop, N-way store fan-out). The scalar `_acc` clang refused to reorder becomes N
  independent reduction lanes -> SLP -> NEON `fmla.4s`. **faithful warm ~217ms -> ~74ms
  (2.9x)**, parity bit-exact, default unchanged (6.3ms), peak 25.7MB + 113 kernels
  unchanged (purely codegen-internal, NO realize-boundary movement), all core suites green
  (grad 68, cpu_jit 342, rangeify 45, pool_im2col 13, index_simplify 172), GPU paths
  untouched (render_metal 8, render_cuda 83).
- **Latent correctness bug FIXED en route:** un-gating CPU UNROLL exposed that the generic
  store path emitted a split-K inner-decomp axis as an OUTER loop that reset the
  accumulator -> the Linear matmul+bias summed only 1/4 of K (loss 2.32 vs 2.35). Fixed by
  folding inner-decomp INSIDE the reduce (matching `rmu_emit_store_reduce`/`rmu_emit_conv`);
  verified vs numpy ground truth (K up to 12800) + multi-shape JIT-replay parity (rel 0).
- Cleanup: deleted the now-dead `rmu_emit_one_reduce` scalar wrapper + fixed its comment refs.

Remaining toward tinygrad's ~15ms (follow-ups, NOT blockers): only ~2 of the 7 largest
kernels emit packed `fmla.4s` so far (lane-blocking is necessary-but-not-sufficient); the
maxpool-grad col2im per-lane body duplication inflates its inherent ternaries (mask-hoisting
is the next lever). The clang `-Rpass=loop-vectorize=0` metric is an x86 artifact on this
arm64 box -- the real signal is the NEON `fmla.4s` + the 2.9x warm drop.

### NEON codegen close-out + the wall is now GATHER-bound (2026-06-01)

Third workflow pushed faithful-CPU codegen further (float4 lane-store + lane-invariant
mask-hoist, render_uop.c). Both levers are correct (independently memcmp-verified bit-exact:
0/102400 diffs on the conv-recompute kernel, 0/32768 on the hardest fused col2im+mask
kernel), parity-exact, all 7 suites green, GPU paths untouched. Codegen is strictly better:
the conv-recompute kernels now emit 20-36 packed NEON `fmla.4s` (were 0 -- the float4 store
flips clang's SLP cost model so the scalar lane accumulators pack), and the fused kernel's
col2im validity ternaries dropped 416 -> 62 (lane-invariant masks hoisted to one `_sh` local
each). Faithful warm is NET-NEUTRAL (~78ms; the earlier "73.74ms" was machine-state drift,
re-verified against a freshly-built clean-HEAD worktree -- NOT a regression).

**Decisive: the faithful-CPU wall is now GATHER-bound, not codegen-bound.** Normalized
profiling shows 47% of wall in TWO fused kernels -- kid 254 ([8,64,8,8], 28%): a conv2-
forward-recompute FUSED with a col2im/_pool GATHER (`in4[<IDIV/IMOD/IWHERE index>]`, a
data-dependent per-lane address that CANNOT vectorize and is memory-latency-bound) -- and
kid 262 ([8,32,24,24], 19%): the maxpool-grad scatter. NEON FMA + mask-hoisting optimize
everything they can but the gather LOAD dominates. **The genuine remaining lever is
SCHEDULER de-fusion** of the col2im gather from the conv recompute (kid 254 over-fuses them
into one kernel) -- a rangeify/materialize change, OUT OF SCOPE for render_uop.c codegen,
and the same arc [[project_beautiful_mnist_speed]] flags as the "6-D fusion lever (still
hangs on data-grad)". That is the next target for the wall; the codegen NEON work is the
faithful (tinygrad-devectorizer-shaped) prerequisite that pays off once the gather is
de-fused. faithful-CPU: 217ms (session start) -> 78ms (2.8x, Lever B), gap to tinygrad ~15ms
is now the gather-fusion arc.

### BREAKTHROUGH: seed REDUCE on the faithful path -> faithful-CPU 77.6ms -> 5.18ms (15x) (2026-06-01)

The wall lever turned out to be the realize SEED, not the codegen. The ROOT-only faithful
seed (ideal_pipeline_v2's premise) was UNDER-realizing: it left the FORWARD conv REDUCE
un-realized, so the backward maxpool-grad RECOMPUTED the conv forward inline AND lowered the
conv data-grad as a per-lane col2im GATHER (`in4[<IDIV/IMOD/IWHERE>]`) -- the 62KB kid 254
(28% of wall) + kid 262 (19%). That violated tinygrad's actual rule ("one reduce per kernel;
a REDUCE output always escapes into a buffer"). Fix: `ru_seed_boundary_holds` now seeds
ROOT || REDUCE on the faithful path (22 lines, `THVM_RU_NO_SEED_REDUCE=1` A/B revert). The
conv forward is realized once; the argmax mask reads an affine materialized buffer and the
data-grad lowers as a strided reduce (tinygrad's shape). The col2im-gather kernels are GONE
(largest emitted kernel 62653B -> 6338B; gather count 2 -> 0).

Result (verified, SHIP): faithful warm **77.61ms -> 5.18ms (15x)** -- now BELOW tinygrad
(~15ms) AND thvm's own default (6.45ms). Parity bit-EXACT (step-0 loss + data-grad SSQ
identical to the default schedule -- actually CLOSER to default than the old col2im-gather
faithful). All core suites green; default mode byte-unchanged (6.45ms / 164 kernels / loss
identical). Peak 25.7 -> 41.3MB (UP, but under default's 45.3MB, flat over 25 iters, linear
at BS=16 -- seeding a boundary is monotonically DE-fusing, the OPPOSITE of the over-fusion
hang, so no hang/balloon). Kernel count 113 -> 161 (more realized, each now affine + BLAS-
eligible + register-blocked -> fast; kernel count is NOT the speed metric).

So the codegen levers landed earlier this session (register-blocking 3fb0cbf6, float4 +
mask-hoist d4c83e1c, fast_idiv 60102482) were correct + faithful prerequisites, and this
seed change is what put faithful-CPU AHEAD of tinygrad. The faithful-CPU arc is DONE
(exceeded the goal). Residual: a few small (<1% each) masked-reduce kernels still carry a
divmod mask, off the hot path -- not worth the deeper index reformulation.

**State of v3 after this session:** achievable compiler goals MET (wins warm wall on
CPU/CUDA/Metal in default mode; faithful kernel count within 9% of tinygrad; numerically
correct). Landed this session: honest `sched_kernels` probe (ea2f6ff6), realize.c
transient-comment trim (d3317049), full `fold_add_divmod_recombine` port (4febd18e), plus
five plan corrections + one stale-memory correction. Every REMAINING lever is large infra
for narrow benefit on a met goal: (1) N-D-preserving rangeify window lowering (faithful-CPU
speed); (2) uop vmin/vmax bounds pass + fuller symbolic div/mod; (3) step-global buffer
planning (memory; faithful already near-optimal, default is an intentional tradeoff) +
JIT-redundancy (shared multi-pass-realize root); (4) M4 GPU opt-section (mask-UPCAST /
THREAD / multi-reduce-TC, GPU-gated). The next deliberate arc, per the "faithful must be
fast" steer, is (1); it warrants a focused effort given its blast radius (every conv, every
backend) and the parity-correctness risk.

**M3 scoped to the exact code (2026-06-01 tick 4):** the IDIV/IMOD source is
`ru_compose_one_view` (`rangeify_unified.c:1295-1321`): it composes EACH view in
`TENS[tid].prior_views[]` by flatten-then-decompose (`coord_d = (cur/suffix[d]) %
dims[d]`), so an N-view chain nests N levels of IDIV/IMOD. A SINGLE view composes
affinely (d=0 gets no IMOD, contiguous suffix gets no IDIV). So the fix is a
**view-chain MERGE pre-pass**: collapse `prior_views[]` into the fewest views whose
composition is affine-expressible (classic ShapeTracker `merge_views` math: v_outer o
v_inner merges iff the strides compose without a genuine reshape-remainder). The conv
window (reshape->permute->shrink->reshape) then collapses to ~1 strided view -> affine
address -> clang vectorizes (matching tinygrad's `acc0[12]` affine conv kernel). NOTE:
current tinygrad folded ShapeTracker into UOps (no `View.__add__` file); port the classic
merge_views algorithm onto thvm's `View` struct (shape/strides/offset/mask), NOT a 1:1
file copy. Implement incrementally (start: merge a contiguous inner reshape into its outer
view), parity-gated by `test_faithful_parity.py` (silent index bugs are the risk -- verify
EXACT values). On GPU faithful is already competitive (gather is cheaper there), so this is
primarily a faithful-CPU + index-cleanliness win; medium value, defined port.

### FAITHFUL VALIDATED ON A TRANSFORMER (GPT-2) -- arena-aliasing bug fixed (2026-06-10)

The whole v3 status log above is conv-net (beautiful_mnist). The faithful seed had NEVER
been run on a transformer. On GPT-2 it (a) broke `gpt2/cached-single-query-attention-vs-
full-row` (7/8) and (b) regresses Metal ~3.5x at seq=256 (218->755ms); CPU warm + kernel
count (346->343) are ~unchanged.

(a) ROOT-CAUSED + FIXED (`materialize.c boundary_last_use_pos_descend`).  NOT a rangeify
or codegen bug -- the UOP-walk interpreter reproduced it bit-identically.  It is a BUFFER
ARENA aliasing bug surfaced only by the faithful seed's fusion.  The single-query (seqQ=1)
batched attention has a masked-scores ADD with TWO consumers (the softmax-denom REDUCE and
the @V kernel).  bufferize_classify flags it `realized` (MULTI), but the faithful rangeify
walk FUSES it into both consumers, so it never becomes a buffer.  The boundary-lifetime
walk terminated at any `BUFFERIZE_NODES[idx].realized` flag, so this fused-away node
SHADOWED the real boundary it reads -- the QK^T scores REDUCE buffer -- leaving the scores
buffer's last-use unbumped.  The TLSF arena then recycled the scores offset onto the @V
OUTPUT, so the @V kernel read its own partially-written output back (the tell: for p=1
softmax is exactly [1,0,..] yet out[0,d>0] came out a constant 0.749x the truth while
out[0,0] was exact -- self-referential corruption, not a math error).  Fix: terminate the
lifetime walk only at an ACTUAL materialized boundary (present in BOUNDARY_ORDER via
`boundary_index_for_loc`), not the classify flag -- mirrors tinygrad/schedule/memory.py,
which plans lifetimes over the realized BUFFER set (the linearized schedule), not the
classify set.  gpt2.wlt 7/8->8/8 faithful; default byte-unchanged (8/8, 175 kernels, peak
43MB); nn.wlt 66/66 both seeds; test_cc 86463 both seeds; test_faithful_parity OK; faithful
conv unchanged (loss 2.5761, 167 kernels, peak 42.6MB -- no balloon).  Found via a
read-only audit workflow (4 agents + synthesis; the synthesis correctly overrode 3 of the
4 agents' headline root causes) + a realize-barrier bisect + a buffer-content discriminator.

(b) OPEN -- faithful Metal regression: the fused reduce-epilogue kernels (LayerNorm/softmax)
get a pathological Metal dispatch grid (threads = reduced output_numel, e.g. 16, each
serially looping the full 768-element body) where the heuristic launched wide elementwise
grids.  Fix lives in `render_metal.c cg_tile_metal_dispatch_shape` (grid over the body
iteration product, not the reduced output_numel).  Next.

### Faithful levers sweep (2026-06-10, sequential worktree workflow) -- 2 land, 2 close as non-issues

Ran the four remaining faithful levers one at a time (brick-safe: no concurrent build/GPU).
Re-applied + re-verified each on current main (the workflow worktrees branched from a stale
551f9f8d, 32 commits back, so their in-worktree gates were re-run from scratch on HEAD).

- **L3 view-chain merge LANDED (`a3d407cd`).** ru_compose_view_chain skips a contiguous
  (row-major, offset-0) inner view -- it composes as the IDENTITY on the running flat index,
  so skipping it emits the same affine map with fewer IDIV/IMOD (tinygrad View.__add__
  `if vm2.contiguous: return vm1`).  READ-ONLY (no prior_views mutation -- TENS is
  refcount-shared/DUP).  Only bites the reshape-of-permute chain the constructor simplifier
  can't fold (merged=2 vs full=7 idiv/imod, byte-exact).  Low perf value, real index/spec
  cleanliness.

- **L0 Metal per-kernel profiling LANDED.** thvm_metal_jit_replay_run recorded
  `wall/n_ops` (uniform smear, gpu_us always 0); the true-per-kernel-GPU branch existed but
  was gated only on THVM_METAL_PROFILE_PEROP.  Now metal_perop is also enabled when
  THVM_KERNEL_PROFILE is active (cg_profile_kernel_enabled), so a plain profile run gets real
  per-kernel gpu_us.  Unblocks Metal diagnosis; greedy-19 unchanged when profiling off.

- **L2 LayerNorm reduce-epilogue fusion -- CLOSED (misdiagnosed).** The x-mean relaxation
  is unreachable: TLayerNorm lowers mean as REDUCE->MUL(/N)->RESHAPE->EXPAND, the chain-hop
  predicate ALREADY unmarks both the mean and var reduces (chain terminates at EXPAND), and
  the `centered = x - mean` SUB is the EXPAND's CONSUMER, never visited.  Enabling the hop =
  ZERO kernel-count change (4->4).  tinygrad's own rangeify realizes mean+var as separate
  reduces too (3-kernel LayerNorm vs thvm 4); the 1-kernel gap is the normalize tail, not the
  mean fusion.  Faithful already fuses what the spec fuses; no lever here.

- **L1 faithful Metal perf -- CLOSED (regression does not reproduce).** Careful brick-safe
  single-forward measurement at HEAD: Metal faithful seq256 = 307ms vs default 310ms
  (faithful marginally FASTER); seq64 85.2 vs 85.8; CPU seq256 254 vs 268.  GROUP_REDUCE is
  ALREADY applied to the LN/softmax reduce kernels (hand_opts GROUPTOP gate fires, tx=16 in
  BOTH seeds); THVM_GROUP_SZ 16/32/64 all within ~1% (the forward is GEMM-bound -- the
  vocab=50257 LM-head dominates gpu_us, confirmed via the L0 profiler -- not reduce-bound).
  The earlier "747ms vs 218ms" was a bad pre-0cf2ea3d/stale seqscale number; it supersedes
  the "(b) OPEN -- faithful Metal regression" note in the prior entry.  Faithful is now
  correct AND competitive on BOTH backends; a real future Metal lever targets the GEMMs
  (TC tiling / JIT dispatch redundancy), not the already-grouped reduces.

### CORRECTION (2026-06-10): the 44b12e86 arena-lifetime change was NET-NEGATIVE -- REVERTED

44b12e86 ("plan boundary lifetimes over the realized BUFFER set, not the classify flag")
changed boundary_last_use_pos_descend to terminate the lifetime walk at an ACTUAL boundary
(boundary_index_for_loc) instead of the classify `realized` flag, to fix the faithful
seqQ=1 attention forward (the @V kernel read its own arena-recycled output).  It DID fix
that 1 faithful-only forward case -- but the full py suite (which the WL tests + test_cc do
NOT exercise) shows it BROKE backward parity for 12 tests under BOTH seeds: softmax /
layernorm / attention / rmsnorm / maxpool-sum BACKWARD on rank>=3 shapes (rel ~27; forward
bit-exact).  Root: the walk must terminate at MATERIALIZED buffers (so it does not descend
into their already-consumed inputs) but DESCEND PAST inlined nodes (so the real boundary
behind a fused masked-scores ADD gets its last-use bumped).  The fused ADD and the
materialized backward buffers are INDISTINGUISHABLE by local flags (both classify-realized,
both missed by boundary_index_for_loc, both rangeify-fused, both mostly elementwise) -- a
boundary-OR-realized-REDUCE variant fixes softmax/layernorm but not attention/groupnorm.  So
the global-termination approach fundamentally cannot separate the two cases.

REVERTED 44b12e86 (this commit): restores the classify-realized termination -> full py suite
173 passed / 0 failed, default gpt2 8/8 + nn 66/66 + grad 62/62, test_cc 86463 both seeds.
The faithful seqQ=1 attention FORWARD is re-broken (gpt2.wlt 7/8 under faithful only; default
is heuristic + unaffected) -- it is a faithful-only, default-OFF issue, the correct trade vs
backward correctness for both seeds.  The proper fix needs a reliable "is this node
MATERIALIZED (gets a buffer) vs INLINED" predicate (the materialize gate's actual output,
incl. legacy allocations -- BOUNDARY_ORDER alone is incomplete) so the walk descends past
inlined nodes only.  That is the prerequisite to faithful-default; deferred.

### RE-LANDED correctly (2026-06-10): boundary lifetime = materialized-OR-cross-pass-shared

The reverted 44b12e86 had the right INTENT (descend the arena lifetime walk past an inlined
node to bump the real boundary behind it -- fixes the faithful seqQ=1 attention forward) but
the wrong PREDICATE: it terminated at `boundary_index_for_loc` ALONE, which drops cross-pass-
shared buffers (produced in one `thvm_realize` pass, read in another -- the per-pass arena
can't see their full lifetime), corrupting softmax/layernorm/attention BACKWARD parity (12
py tests, both seeds).  The fix: terminate at a node iff it is classify-realized AND
(a this-pass boundary OR `xpass_is_shared`) -- i.e. actually MATERIALIZED.  The inlined
masked-scores ADD is neither, so the walk descends past it (forward fixed); cross-pass-shared
backward buffers ARE xpass-shared, so the walk terminates at them (backward preserved).
Verified: full py suite 173/0, gpt2.wlt 8/8 FAITHFUL (seqQ=1 fixed) + default, nn 66/66,
grad 62/62, test_cc 86463 both seeds.  This unblocks the faithful-default flip (next).

### Faithful seed honors maxpool-input pre-realize; flip now 172/173 (2026-06-10)

With the lifetime fix (a323d302) in, the faithful-default flip experiment is full py suite
171/173 (the 9 cross-seed backward fails are gone).  The 2 remaining are faithful-specific
maxpool-grad NaNs (the /count tie-split: RECIP(0) when the argmax mask count hits 0 because
the forward window-max and the backward CMPEQ mask read fp-disagreeing recomputes of the
activation instead of one buffer).  thvm's "maxpool-input pre-realize (ROUTE A)" realizes
that activation, but it marked only BUFFERIZE_REASON_MULTI, which the faithful seed ignores.
Fix: a dedicated BUFFERIZE_REASON_MAXPOOL_INPUT (thvm.h) that ru_seed_boundary_holds honors
under faithful too (correctness realize, not a heuristic).  Clears the non-BN case
(relu->maxpool->sum); dormant + byte-unchanged under the heuristic default (MULTI already
realizes it).  Flip now 172/173.

LAST flip blocker: test_relu_bn_maxpool_sum_bwd_n1 -- N=1 relu->BatchNorm(train)->maxpool->
sum conv-weight grad, faithful-only NaN.  Even with the maxpool activation (the BN output)
realized, the forward-max and backward-mask still don't share bit-exact ties -> count 0 ->
recip(0)=inf -> NaN.  A deeper tie-split DEDUP issue specific to BN+maxpool+N=1 under faithful
(the BN normalize/detach between relu and maxpool breaks the cross-realize dedup span).  The
flip is HELD (faithful stays opt-in) until this closes -- will not ship a default that NaNs.

### LANDED: faithful is now the DEFAULT seed (2026-06-10)

The last faithful-only blocker -- N=1 BatchNorm+maxpool conv-weight-grad zero/NaN -- was the
maxpool backward MASK (CMPEQ(a, lift(MAX(a))) + its mask_norm = mask*RECIP(count)) being
marked only BUFFERIZE_REASON_MULTI, which the faithful seed drops.  Under faithful the mask
fused into the N=1 conv-weight backward SUM-reduce, where the walker's nested-reduce-iter
mis-reads the /count tie-split over the size-1 batch axis -> the mask matches nothing -> grad
collapses (~1e-7 vs tinygrad 1.574).  db747d5c (MAXPOOL_INPUT) realized the ACTIVATION but
not the MASK; a BatchNorm/detach between relu and maxpool makes the activation chain long
enough to trigger the fused miscount, so the mask itself must materialize.  Fix:
BUFFERIZE_REASON_MAXPOOL_MASK (1u<<7), marked on the mask/mask_norm node alongside MULTI, and
honored by ru_seed_boundary_holds under faithful (a correctness realize).

Then FLIPPED ru_faithful_seed_on to default ON (opt out: THVM_HEURISTIC_SEED=1 or
THVM_RU_FAITHFUL_SEED=0).  The tinygrad-structural rangeify seed (ROOT/STORE + one-reduce-
per-kernel + the maxpool correctness realizes, deriving the rest via the consumer-divergence
walk) is now production.  Validated on main: test_cc 86463 both seeds; full py suite 173/0
(faithful default AND heuristic opt-out); WL gpt2 8/8 + nn 66/66 + grad 62/62; Metal greedy
token 19 (CPU == Metal); conv training loss 2.5761, no NaN, 174 kernels, 43MB.  Found via a
sequential workflow (fix+flip agent + an adversarial re-verify agent that independently
re-applied the diff to a fresh worktree off main and re-ran every gate).

GPT-2 perf under the new default: seq256 CPU 108 vs the old heuristic 162 ms (faithful is
FASTER -- the goal was perf-positive).  Remaining "clean" half (the M-major->TRealize barrier,
attention dual-path collapse, env-knob retirement) is now unblocked and independent.

### FLUX fusion audit + matmul-input-fuse made backend-safe (2026-06-19)

Audited the three "clean" FLUX/transformer levers against the actual lowering on a
transformer-shaped probe (AdaLN modulation -> Q/K/V projection matmul, M=256 K=N=512,
CPU `kernel_count()` delta).  Two were NON-issues, one was a latent miscompile:

- **The "M-major -> TRealize barrier" is REDUNDANT, not a kernel-count splitter.** FLUX runs
  under the faithful seed by DEFAULT (FluxGenerate sets no `THVM_HEURISTIC_SEED`/seed
  override on the Examples path -- the velocity is plain TJit-captured), so it already gets the
  faithful structural fusion.  The WL `fxLinear` (`FluxForward.wl:35`)
  `TRealize[TMatMul[TRealize[x], Transpose[w]]]` wraps BOTH the matmul input and output in
  `TRealize`, but the C scheduler ALREADY realizes the matmul reduce unconditionally
  (`BUFFERIZE_REASON_MATMUL`, bufferize_classify.c:137/1282 -- a buffer operand for BLAS/TC,
  tinygrad's ALWAYS_CONTIGUOUS) and the input is kept realized as a GEMM operand by
  `rb_feeds_matmul_reduce`.  Measured: realize-input vs scheduler-input give the SAME 3 kernels.
  So the WL `TRealize`s are not blocking a fusion the scheduler would otherwise do; removing
  them is byte-neutral and they carry real reasons (collapse a symbolic-Plus for the shape
  query; the q8 output handle).  Left as-is.

- **Attention dual-path is the symbolic-vs-literal-seq split, not a hand-fused-vs-faithful
  hack** (Attention.wl:265-295: per-head loop for a symbolic KV-cache-decode seq vs ONE batched
  scaled-dot for a literal seq -- token-identical).  `remove_bufferize` is default-ON and fires
  on the softmax exp/scale/normalize (no rank/transformer/backend gate excludes FLUX attention);
  the materialize-side index-substitution blocker the prior prototype hit is resolved
  (`ru_remove_bufferize_on` default-on, 4 gates).  No lever here.

- **`THVM_FUSE_MATMUL_INPUT` MISCOMPILED off Metal (the real bug, now FIXED).** The opt-in
  matmul-input fuse (0bb8dc87) un-realizes a single-consumer elementwise producer so it inlines
  into the matmul.  Only the Metal register-blocked tiled emitter
  (`render_uop.c rmu_emit_matmul_tc_tiled`) can emit that inline -- it reconstructs the
  producer's (m,k) from the threadgroup tile origin.  On CPU/CUDA the producer feeds the matmul
  through the matmul-lowering reshape(+unit)+expand(N), a rank-changing movement the POSITIONAL
  per-consumer re-index (`ru_build_axis_subst`) cannot bind, so the producer's own M/K ranges
  LEAKED as extra output loops -> a `|M||N||M||K||K|` ~8.8e12-iter runaway kernel (a clang `-O3`
  hang on the FLUX AdaLN modulation pattern; root-caused via `THVM_DUMP_KERNEL_SRC` of the
  5-nested-loop kernel).  `rmu_emit_matmul_tc` already bails fused-A on non-Metal
  (`a_val != 0 && RMU_TARGET != CG_TARGET_METAL`), but the rangeify un-realize was target-blind
  and still un-realized the producer, leaving the generic accumulator to leak the ranges.
  FIX (this commit): `ru_fuse_matmul_input_target_ok` gates the un-realize on
  `default_device == THVM_DEV_METAL` (set by realize.c's device routing before rangeify runs),
  matching the codegen capability boundary -- Metal fuses correctly, CPU/CUDA keep the producer
  a realized BLAS operand.  So `THVM_FUSE_MATMUL_INPUT` is now SAFE on every backend; FLUX (Metal
  warm) can opt in for the tinygrad-style modulation-into-matmul fusion without breaking the CPU
  parity gates.  Verified: the runaway-loop repro terminates; `test_metal_fuse_matmul_input`
  correct (maxAbsDiff 0) in both modes on M3 Max; `test_lnmatmul_fuse` strengthened to assert
  BLAS fires in both modes; new `test_fuse_matmul_input_cpu` regression (CPU AdaLN-into-matmul
  terminates + matches the realized-input reference); `test_faithful_parity` OK default AND
  `THVM_FUSE_MATMUL_INPUT=1`; gpt2 8/8, nn 72/72, grad 62/62; default codegen byte-unchanged.
