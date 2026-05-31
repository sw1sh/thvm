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

| | CUDA | Metal | kernels | peak mem |
|---|---|---|---|---|
| thvm default | 6.15ms | 3.63ms | 328 | 31MB |
| thvm faithful | 8.64ms | 4.72ms | 226 | 14.8MB |
| tinygrad | ~8.7ms(BEAM) / 12ms | 6.03ms | 120 | ~1-3MB |

thvm wins wall-time both backends; tinygrad fuses more (fewer kernels, less memory).
Faithful is the middle. The 226->120 residual is reduce-into-reduce fusion + ShapeTracker
merge (the `rangeify_unified.c:160` comment localizes it).

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
  arange/reduce-collapse - **medium**.
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
