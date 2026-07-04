# ATP in-place architectural rewrite (Phase A: decision-grade plan)

Status: PHASE A DELIVERED 2026-07-04. This is the consolidated payoff model
plus a dependency-ordered phased design for closing the residual ~1.1-1.25x
engine-throughput gap vs native Waldmeister (FEQ / the Wolfram-bundled
ELProver) on the BYTE-IDENTICAL OrAssociativity (OA) trajectory. It is the
last-resort axis after four profile-guided bounded levers were each measured
sub-bar (retrieval index, normalize recursion, heap-Term interchange,
CP-selection heap; see memory project_atp_wm_speed_profiled.md later-21..27).

READ-ORDER for a future implementing agent: this doc (payoff + phases) ->
docs/plans/atp_heap_leak.md ("Connection to the heap-overhead refactor") ->
project_atp_wm_speed_profiled.md later-23/24/25/27 (the sub-bar refutations
this plan must not re-litigate).

THE HEADLINE VERDICT (see Section 6 for the full argument):
- The DOMINANT excess is the normalizer's per-node CONSTANT FACTOR (thvm
  ftdt_descend3 ~1.43x native's goto-DFS, ~50% of thvm engine self-time,
  ~5 us/sel excess ~= the whole net gap). NONE of the four in-place phases
  touch it; it was refuted as control-flow (later-24) and as retrieval
  (later-23), and it is representation-micro-architectural (24-byte cell
  traversal + non-linear repeat-var equality walks), not allocation.
- The in-place rewrite's four phases attack ALLOCATION (~0.25s bound),
  CP-QUEUE PACK (~0.2s bound), and DUAL-STORAGE (~0.05s) -- the NON-normalize
  excess. Their honest aggregate bound is ~0.4-0.7s IF every coherent
  sub-path conversion banks its bound without the net-negative surprise that
  killed EVERY piecemeal attempt so far (O(1) free +0.35s, iterative descent
  +1.15s, FT-pop -0.07s, index-heap ~0.00s).
- GO/NO-GO: Phase 1 (arena consolidation) has an INDEPENDENT correctness
  justification (it subsumes the heap-leak bug class, docs/plans/atp_heap_leak.md)
  and is ~90% already realized for FT cells -- recommended. Phase 2 (in-place
  RHS splice) is the largest speed lever and the one untested combination --
  worth ONE gated round. Phases 3-4 (dual-storage collapse, flatterm CP queue)
  are multi-week, sub-0.3s, and carry the campaign's byte-parity foundation as
  downside -- NOT recommended as speed plays; pursue only if Phase 2 banks and
  a broader (non-norm-core) memory-traffic goal is separately sanctioned.

---

## 1. The two models (what "in-place" means here)

### Native Waldmeister (READ-ONLY at src/wolfram/waldmeister; built binary at
~/.cache/thvm/wm_ref/l20_clean_build/)

- Flat termpairs. Rules/equations are `GNTermpaarT` records
  (include/Termpaare.h:61), allocated from a per-manager bump pool
  (`TP_FreispeicherManager`, sources/TPR/Termpaare.c:53). Terms are
  `TermzellenT` cells threaded by `Nachf` (next) / `Ende` (end).
- Rewrite IN PLACE. `MO_SigmaRInLZ` ("apply Sigma to the RHS in
  letzteZelle", sources/INF/MatchOperationen.c:1273) writes the instantiated
  RHS image directly over the query term's own cells and frees only the
  SURPLUS. The nullary cases overwrite the head/last cell in place
  (:1278, :1294, :1303); the general "Kasus Knackus" case (:1330-1347)
  overlays the RHS onto the query cells, splices bound-substituts by pointer,
  and calls `TO_TermzellenlisteLoeschen` on the leftover prefix only. The
  normalizer drivers (NFBildung.c:246/516/701, Grundzusammenfuehrung.c:279/285,
  pcl.c:796/1142) all consume `MO_SigmaRInLZ()`'s in-place result.
- No garbage collection. Freeing is O(1): `SV_NestedDealloc`
  (include/SpeicherVerwaltung.h:305) is `last->next_free = mbm.next_free;
  mbm.next_free = first` -- a two-assignment chain-splice of a pre-linked span,
  independent of span length. Term-cell spans reuse this via
  `TO_TermzellenlisteLoeschen`.
- Net: ~0.7% self-time in alloc/free on the OA trajectory (later-21).

### thvm

- FT cells. `AtpFtCell` (src/atp/ft.h:45) is a 24-byte cell (next 8, end 8,
  sym 4, arity 2, flags 1, pad 1), the normalizer's working form.
- FT cells ALREADY live in a bump/slab arena, NOT the GC heap. ft_alloc.c is a
  dual-arena allocator: Arena A = 30000-cell slab pool with an O(1) free list
  (ft_free_span, ft_alloc.c:162); Arena B = contiguous scratch bump. This is
  structurally the SAME design as native's SV manager, and its NORMCORE2 O(1)
  free (ft_alloc.c:181-202) is byte-for-byte `SV_NestedDealloc`.
- The Cheney copying GC (src/heap/collect.c, thvm_atp_gc_collect at
  src/atp/_.c:6679) manages the HEAP TERMS -- the dual rule storage `s->lhs[i]`
  (Term) parallel to `s->lhs_ft[i]` (FT), the packed-byte CP queue's unpack
  images, and the proof trace Terms the WL ProofObject lift reads. On the FEQ
  trajectory this GC fires ONCE (measured: "gc: passes=1", Section 2).
- The rewrite is NOT in place. On OA, EVERY rewrite is regime-c
  (ft_splice.c:375): build a FRESH RHS image (ft_splice_build_rhs) into Arena
  A, then free the whole displaced span. Per-rewrite: 3.92 cells alloc + 13.21
  cells freed via the default O(span) counting walk (later-23). Native reuses
  the redex cells (~0 net alloc) + O(1) dealloc.
- The heap-leak recycle stopgap (693cedfa, recycle via full gc_collect) is
  live; a dedicated arena for the remaining heap-Terms would supersede it
  (docs/plans/atp_heap_leak.md).

The mission's one-line framing -- "native uses a bump arena + in-place rewrite,
thvm uses a GC + per-rewrite alloc" -- is thus HALF ALREADY DONE: the FT arena
exists and matches native's allocator; regime-(a) in-place overwrite exists and
matches MO_SigmaRInLZ's nullary path. The residual is (i) regime-c does not
reuse cells, (ii) heap-Terms still sit on the shared GC, (iii) the CP queue is
packed bytes (native keeps flat termpairs).

---

## 2. Fresh measurement (this Phase A), the byte-identical FEQ trajectory

C-bench non-lazy proxy reproduces the WL FEQ trajectory byte-for-byte:

    THVM_ATP_RSS_ABORT_MB=5300 THVM_ATP_WALDMEISTER=1 THVM_ATP_LAZY_NORM=0 \
      ./bin/test_atp_wolfram_bench \
      tools/baselines/wm_pr/WolframAxioms__OrAssociativity.pr 500000 240

Result: PROVED steps=263550 rules=771 cps=1654086 (== gate 263550/771/1654086),
engine 8.1s, wall 8.64s, RSS 2.71GB, **gc passes=1**, unorientable 44/771,
ri-index 499552 queries @ 14.63 tree-nodes/query, push-time full-R
normalizes=2959847. thvm engine us/sel = 8.1s / 263551 = **30.7 us/sel**
(banked runs 33-37 us/sel under WL/load); native FEQ ~7.5s (later-22) =
~28.5 us/sel; clean-native reference 26.4 us/sel (later-20/21).

/usr/bin/sample of the running engine, 5526 leaf self-samples (>=5 collapsed).
Grouped into the mission's subsystems (share of 5526 -> us/sel at 30.7 total):

| subsystem | key leaves (samples) | thvm share | thvm us/sel | native ref | EXCESS us/sel | representation cause of the excess | in-place phase that addresses it |
|---|---|---|---|---|---|---|---|
| **Normalize** (DT descent + NF memo + rule-trial + retrieval + apply-logic) | ftdt_descend3 1989, ftnfm_probe 189, ft_cell_try_rules 175, atp_normalize_mixmost_ft 71, atp_rewrite_normalize_ft_impl 63, ftdt_emit_leaf(_class) 62, atp_ri_descend_rec 48, ftnfm_store 43, ft_mixmost_reduce_here 29, atp_ordered_* 41, ftdt_sort_u32 21, atp_ri_insert_term 26, ft_splice(_build_rhs) 48 | 50.8% (2805) | 15.6 | ~10.5 (37% x 28.5, at 1.43x lower per-node) | **~5.1 (DOMINANT)** | per-node CONSTANT FACTOR: 24B cell traversal + want-mask prune + 264.8M non-linear repeat-var ft_eq (752M cells) + kid_mask/emit; node-touch COUNT is at parity with native (16.71 vs 16.16, later-23). Neither sharing nor control-flow: native also tree-copies + walks repeat-vars. | **NONE** (refuted: retrieval parity l23, recursion compiler-optimal l24) |
| **Alloc / free** (FT arena) | ft_alloc_persistent 174, ft_free_span 22, term_new_ctr 14 | 3.8% (210) | 1.17 | 0.20 (0.7%) | **~0.97** | regime-c fresh-RHS build (3.92 cells/rewrite) + O(span) free-walk (13.21 cells) vs native in-place reuse (~0 net) + O(1) SV_NestedDealloc | **Phase 2** (in-place RHS splice + revived O(1) surplus free) |
| **CP formation** (overlap emit + FT build) | vcp_ft_build_emit_rec 461, vcp_emit_side 56, sw_vater_emit_hit 38, sw_form_push 36, atp_push_cps_traced 29, vcp_treat_joinable 23, cp_walk_positions 16, sw_vater_visit 15 | 12.2% (674) | 3.75 | ~2.7 (uncertain; -auto share inflated) | ~1.0 (uncertain) | per-CP FT-cell build + trace bytes vs native's single flat-termpair pack; already FT-native/fused (later-25) | **Phase 4** (flatterm CP queue) partial |
| **Overlap / unify** | unify_occurs 148, thvm_unify 81, wmo_unify_cells 61, thvm_match 45 | 6.1% (335) | 1.86 | ~7 (-auto, more CPs) / ~parity on FEQ | ~0 or NEGATIVE (thvm cheaper) | thvm's unify is competitive; NOT an excess | -- |
| **KBO / ordering** | kbo_flat_vortest 153, kbo_lin_addto 149, kbo_eq 130, atpft_kbo_flat_encode(_subst) 103, kbo_vortest 50, kbo_compare_core 43, ft_wm_pattern_before 47 | 13.0% (718) | 3.99 | ~3.1 (11.5%) | ~0.9 | FT->flat KBO re-encode per compare (atpft_kbo_flat_encode) that a native flatterm compare avoids | **Phase 3** (dual-storage collapse) minor |
| **Selection / CP-heap + WM emission mirror** | atp_cp_heap_push 188, atp_cp_swap 150, wmo_dfs 142, atp_cp_before 52, atp_cp_heap_insert_packed 19, wmo_var_counts 13 | 10.2% (564) | 3.13 | ~0.43 (1.6%) | ~2.7 (but only ~0.2s ADDRESSABLE) | atp_cp_heap_push = acp_PACK + KBO-weigh + insert; atp_cp_swap = 9-array move (l27: 0.19s hard ceiling); wmo_dfs = WM-faithful emission-order mirror | **Phase 4** drops acp_pack; swap ceiling 0.19s (l27) |
| **Subsumption** | (not a hot leaf on FEQ) | ~0 | ~0 | ~0 | ~0 | cold on FEQ (later-25) | -- |
| **Trace** | (record path off on canonical FEQ) | ~0 | ~0 | ~0 | ~0 | ft_to_term only for psteps; cone-scoped since l15 | -- |
| **GC** | thvm_atp_gc_collect (not a leaf) | ~0 | ~0 | 0 | ~0 | 1 collection on FEQ; NOT a hot cost here (heavy on lazy OA only) | Phase 1 (removes it entirely) |

**The excess adds up to the gap and localizes it:** normalize ~5.1 us/sel
(~1.3s) is the dominant positive excess and is NOT in-place-addressable;
alloc ~0.97 (~0.25s) is the LARGEST in-place-addressable excess; selection
~2.7 has only ~0.2s addressable (later-27); formation/KBO ~1.9 combined are
uncertain (the native FEQ per-subsystem breakdown is NOT banked -- only the
-auto trajectory is, later-21 -- so those two rows carry a trajectory caveat)
and each bound <0.3s. The observed NET gap is only ~0.6-1.2s because thvm's
normalize/formation/selection excess is PARTLY OFFSET by a genuine thvm
advantage in unification. Trajectory-independent facts that anchor the model
(no native re-sample needed): node-touch parity 16.71 vs 16.16 and the 1.43x
per-node normalize cost (later-23, direct instrumented A/B); alloc 3.92+13.21
vs ~0 per rewrite (later-23); swap ceiling 0.19s (later-27 amplifier).

---

## 3. Native in-place mechanisms to port (file:line)

| mechanism | native site | thvm status |
|---|---|---|
| Bump arena for term cells | SpeicherVerwaltung SV manager; SV_ErstesNextFree/SV_AufNextFree (include/SpeicherVerwaltung.h:286/292) | DONE for FT cells (ft_alloc.c slab pool); NOT for heap-Terms (still GC) |
| O(1) span free | SV_NestedDealloc (include/SpeicherVerwaltung.h:305); TO_TermzellenlisteLoeschen | DONE behind NORMCORE2 (ft_alloc.c:181); net-negative ALONE (later-23), needs Phase 2 to pay |
| Nullary in-place overwrite | MO_SigmaRInLZ nullary cases (MatchOperationen.c:1278/1294/1303) | DONE as regime-a (ft_splice.c:233) -- but does not fire on OA (all regime-c) |
| **General in-place RHS image (reuse redex cells, free surplus)** | **MO_SigmaRInLZ "Kasus Knackus" (MatchOperationen.c:1330-1347) + SubstAnwendenLZ (:1230)** | **MISSING -- regime-c builds fresh + frees all (ft_splice.c:375). Phase 2.** |
| Flat termpair rule/CP record | GNTermpaarT (Termpaare.h:61); STT_TermpaarEinpacken | thvm packs CP to bytes (acp_pack) + dual s->lhs/s->lhs_ft rule store. Phases 3-4. |

---

## 4. Phased plan (dependency-ordered)

Discipline for EVERY phase (non-negotiable, this campaign's method): (1) build
the differential CERTIFIER first; (2) prove it FIRES on a deliberate break;
(3) put the change behind a default-OFF, same-binary kill switch; (4) measure
C-bench + non-lazy FEQ x2 each side under /usr/bin/time -l; (5) accept/flip
ONLY if byte-identical (all pins EXACT) AND a real >0.4s win AND RSS-neutral;
otherwise keep default-OFF and record the measured negative.

### Phase 1 -- Arena consolidation of the remaining heap-Terms (FOUNDATION)

Change: carve the ATP heap-Terms (s->lhs dual store, trace Terms, CP-unpack
images) out of the shared Cheney semi-space into a dedicated ATP bump arena,
reset per outermost run. This SUPERSEDES the heap-leak recycle stopgap
(docs/plans/atp_heap_leak.md) and removes thvm_atp_gc_collect from the ATP
steady state. The KBO weight memo is keyed by (epoch, Term loc); an arena
never relocates, so the memo's GC-relocation invalidation dance
(src/atp/_.c:6633) disappears and the memo stays valid across the whole run.

Certifier: THVM_ATP_ARENA_CHECK -- run with the arena AND the GC path,
asserting (a) every s->lhs / trace Term the WL lift consumes is bit-identical
in structure (kbo_eq) between the two, and (b) the selection/NF/verdict stream
is byte-identical. The trajectory does not read Term ADDRESSES (ftnfm is
content-addressed, Section 5), so an arena that changes addresses is safe by
construction; the certifier proves the WL-lift consumer (constraint 2) sees
identical Terms.

Payoff: ~0 speed on FEQ (gc passes=1). REAL on lazy OA (gc passes>1) and on
multi-proof sessions (no cross-run poison). PRIMARY value is CORRECTNESS +
simplification (deletes the recycle/epoch/checkpoint machinery). Risk: MEDIUM
-- touches the GC boundary (constraint 3: heap shared with tensor work) and the
WL trace-lift (constraint 2). Must keep tensor cells on the GC; only ATP Terms
move to the arena. Go/no-go: **GO** on the correctness merit, independent of
speed. Largely realized already for FT cells -- the incremental work is the
heap-Term subset.

### Phase 2 -- In-place RHS splice (the largest speed lever; one gated round)

Change: convert regime-c (ft_splice.c:375) from "build fresh RHS + free whole
span" to native's MO_SigmaRInLZ discipline -- REUSE the redex's own displaced
cells for the RHS image, free ONLY the surplus. The aliasing-safe formulation
(avoids native's intricate cell-overlay): (i) deep-copy every rhs_tmpl var
binding to persistent images FIRST (bindings are subterms of the redex; they
must be independent before the redex cells are recycled); (ii) thread the
displaced span [redex..redex->end] as a cache-HOT reuse pool; (iii) build the
RHS skeleton pulling cells from the reuse pool before ft_alloc_persistent, with
var-leaves pointing at the step-(i) images; (iv) ft_free_span the surplus
(now O(1) and cache-hot -- the pool cells were just touched, so the LIFO
cache-warming that made the standalone O(1) free NET-NEGATIVE at +0.35s in
later-23 no longer applies). This is the ONE untested combination: prior
rounds measured O(1) free ALONE (net-neg) and in-place-reuse never.

Certifier: THVM_ATP_INPLACE_SPLICE_CHECK -- per splice, snapshot the subject
subtree, run the reference (fresh-build) regime-c on the copy, run the in-place
splice on the original, assert kbo_eq(result, reference) AND the free-list
accounting (n_persistent_alive delta = skeleton_reused - surplus_freed matches
the reference's alloc-fresh - free-all). Must FIRE on a deliberate break
(e.g. skip the surplus free, or mis-order the binding copy).

Payoff bound: ~0.25s (alloc excess 0.97 us/sel; later-24 bounded the standalone
in-place splice at 0.15-0.25s). REVIVES the O(1) free from net-negative.
Risk: MEDIUM-HIGH -- the binding-copy-first ordering and the reuse-pool
surplus accounting are the two places a subtle bug breaks byte-identity or
leaks/corrupts cells. Go/no-go: **CONDITIONAL GO for one gated round** -- it is
the largest addressable lever and the untested combination; flip only if it
clears the 0.4s bar (evidence says it likely lands ~0.15-0.25s = sub-bar, in
which case keep default-OFF like NORMCORE2/FT_POP/HEAP2).

### Phase 3 -- Collapse the dual s->lhs / s->lhs_ft rule storage to FT-only

Change: delete the heap-Term rule side (s->lhs/s->rhs), keeping only s->lhs_ft.
Requires FT-NATIVE OVERLAP (positions + unify on FT cells; today sw_form_push
overlaps Term parents and the vcp resolve-walk reconstructs from Term-parents),
FT-native subsumption (today kbo_eq / atp_rule_subsumes_unit read Terms,
src/atp/_.c:11395/11408/11476/11562), FT-native unorientable-eq match
(thvm_match on Terms, :4980/5390/5446/13003), and an FT/bytes trace-lift the WL
ProofObject consumes (constraint 2). This is the "load-bearing dual storage"
(later-25): s->lhs is READ in the hot loop by formation, subsumption, and the
trace lift -- so this is a multi-week rewrite of the overlap/unify/subsume/trace
machinery, not a deletion.

Certifier: FT-native-overlap differential (existing FT_EMIT_CHECK is the seed)
+ a trace-lift equality check.

Payoff: ~0.05s compute (interchange is 0.57%, later-25) + minor RSS (771-rule
Term arrays, a few MB). Risk: HIGH (touches formation, subsumption, trace, the
WL lift). Go/no-go: **NO-GO as a speed play** -- multi-week for ~0.05s. Pursue
ONLY if a separate goal (RSS, engine simplification) is sanctioned.

### Phase 4 -- Flatterm CP queue (drop the packed-byte pack/unpack)

Change: replace the packed-byte CP queue (cp_packed[], acp_pack/acp_unpack)
with FT cells carried from formation, so acp_pack (in atp_cp_heap_push) and
acp_unpack (at pop) disappear. Ties to the shelved THVM_ATPFT_CPQ (later-22).

Certifier: the pop-sequence + NF invariants (HEAP2_CHECK is the seed).

Payoff: drops acp_pack (~half of atp_cp_heap_push's 188 samples ~ 0.2s) + the
pop unpack (already measured -0.07s as FT-pop, later-22). Swap floor is 0.19s
(later-27, structural, NOT queue-format-addressable). Bound ~0.2-0.3s but the
packed queue is a deliberate RSS win (2.6M-CP queue) -- FT cells are 24B each,
so this likely REGRESSES RSS materially. Risk: HIGH (RSS) + touches selection.
Go/no-go: **NO-GO** unless Phase 2 banks and RSS headroom is re-measured; the
RSS regression likely dominates the ~0.2s.

---

## 5. Why this is byte-identity-safe at the representation level

The trajectory (selection order, NFs, KBO verdicts, proof) is a function of
term STRUCTURE, not cell/Term addresses:
- The FT NF memo `ftnfm` is CONTENT-ADDRESSED: ftnfm_key (src/atp/ft_norm.c:2788)
  is an FNV-1a hash over the pre-order symbol sequence + a memcmp of the sym
  array. In-place cell reuse (Phase 2) and the arena (Phase 1) change
  ADDRESSES, not symbol sequences -> memo hits are unchanged.
- The KBO weight memo is (epoch, loc)-keyed for HEAP Terms; an arena removes
  relocation, so it needs the epoch dance LESS, not more. FT-side KBO reads
  cells structurally.
- Certifiers assert kbo_eq (structural), never pointer identity.

The one HARD constraint the design must never violate: the byte-identical
trajectory at EVERY phase (bench 278807/753/2642990; FEQ non-lazy
263550/771/1654086; DN 2848/254/768876; WL 263551/771/1705144/1981537 + 292
Verify Success). Every phase is gated on it.

---

## 6. Honest total-payoff verdict

Optimistic ceiling: if Phase 2 (~0.25s) + Phase 4 pack-drop (~0.2s) + Phase
1/3 minor (~0.1s) ALL bank their bounds with no net-negative surprise, thvm
8.1s -> ~7.5s -- i.e. it WOULD reach native FEQ's ~7.5s. This is the ceiling
the mission authorized chasing.

Evidence-weighted expectation: every bounded lever attempted in this campaign
(6 rounds) measured sub-bar OR net-negative -- O(1) free +0.35s, iterative
descent +1.15s, FT-pop -0.07s, index-heap ~0.00s, interchange 0.57%. The base
rate for "bounded us/sel excess converts to wall-clock" in this engine is POOR,
because the excess is distributed and the removed work is often cache-warming
or compiler-optimal. The most likely outcome is a PLATEAU at ~7.8-8.0s: Phase
2 banks maybe 0.1-0.2s (below the 0.4s bar), Phase 4 regresses RSS, and the
dominant ~5 us/sel normalize constant factor is untouched. The normalizer
per-node cost is the real floor and is orthogonal to in-place rewriting.

Where further work stops being worth the parity risk: after Phase 2. Phases
3-4 are multi-week, sub-0.3s (Phase 3 sub-0.1s), and put the campaign's entire
byte-identity foundation -- on which the PROOF and PARITY goals both rest -- at
risk for a fraction of a second. Phase 1 is worth doing for correctness
regardless. The decision-grade recommendation: **DO Phase 1 (correctness),
attempt Phase 2 ONCE under its certifier (largest lever, untested combo),
and STOP.** Do not open Phases 3-4 as speed plays.

---

## 7. Phase A first increment (LANDED 2026-07-04, commit 157f5d58)

The Phase-2 in-place RHS splice (the largest lever, Section 4) was DEFERRED to
a dedicated round -- it is genuinely entangled for a single safe pass: (a)
non-linear RHS (a var appearing 2+ times in the template) needs a distinct
cell-copy per occurrence, and (b) the bindings are subterms of the redex, so
the redex cells cannot be recycled until every binding is copied out first;
native handles this with the intricate MO_SigmaRInLZ "Kasus Knackus" pointer
surgery, and ft_splice.c already carries known chain-corruption defensive caps.
Forcing it risks the campaign's byte-parity foundation for a sub-bar (~0.25s)
payoff. Its full spec is Section 4 Phase 2. Per the mission's deferral clause,
this is a dedicated round -- and it is the first Phase-2 lever to try.

LANDED instead -- the smallest SAFE, foundational, byte-identity-gated piece of
the allocation phase: **THVM_ATP_FT_RAW_ALLOC** (default OFF). ft_alloc_one
routes to a raw (non-zeroing) allocator; the six-field zeroing in
ft_alloc_persistent is redundant because the three ftnew_* constructors
overwrite every READ field (next/end/sym/arity/flags) and `_pad` is never read
(static audit: no reader in src/atp, no whole-cell compare). This is native's
SV_Alloc discipline (raw hand-out) at the granularity that is safe today.

Certifier (built + validated FIRST): **THVM_ATP_FT_RAW_ALLOC_CHECK** forces the
raw route and poisons the scalar fields (sym=0xFFFFFFFF, arity=0xFFFF,
flags=0xFF) on hand-out; ft_raw_check at each constructor's return asserts every
read scalar was overwritten. CLEAN over the full FEQ trajectory (pins EXACT);
PROVEN TO FIRE -- a deliberate skip of ftnew_var's `c->flags=0` tripped
"constructor left a READ field poisoned (... flags=255)" and aborted; reverted.

Measured (alternating OFF/ON x2, /usr/bin/time -l, FEQ non-lazy): **NET-NEUTRAL**
-- steps/sec ON 27765/27735 vs OFF 27837/28131 (ON marginally slower, within the
~1% run-to-run noise); engine ~9.4-9.5s both; RSS 2.70GB both (NEUTRAL). WHY: the
zeroing is six consecutive stores to one already-in-cache line that the
constructor writes anyway -- the compiler handles it and there is no cache-traffic
saving. Same pattern as every prior bounded lever. **DISPOSITION: DEFAULT-OFF**
(sub-bar, per campaign discipline), committed as a certified scaffold + a reusable
raw-alloc primitive the Phase-2 in-place splice will consume. This EMPIRICALLY
CONFIRMS the payoff model's row for alloc self-time: removing the alloc-side store
traffic banks ~0 without the in-place REUSE that native pairs it with (later-23's
"O(1) free needs in-place reuse to pay").

## 8. Gate baselines + first-increment gates (Phase A, HEAD 3a43975b -> 157f5d58)

Baselines (clean, binaries rebuilt): FEQ non-lazy steps=263550 rules=771
cps=1654086, engine 8.1s, RSS 2.71GB, gc passes=1; lazy OA 278807/753/2642990;
DN 2848/254/768876; test_atp 136241/136241.

First-increment gates (both switch states + CHECK, verbatim):
- FEQ non-lazy 263550/771/1654086 EXACT (OFF, ON, CHECK)
- lazy OA 278807/753/2642990 EXACT (OFF, ON)
- DN 2848/254/768876 EXACT (OFF, ON, CHECK)
- test_atp 136241/136241 (unset, RAW_ALLOC=1, RAW_ALLOC_CHECK=1)
- NORMCORE2_CHECK + FT_EMIT_CHECK on DN clean (2848/254/768876); orthogonal/inert at default
- make rc=0; make wl rc=0
- RAW_ALLOC_CHECK deliberate-break FIRES (flags=255 poison), reverted
</content>
