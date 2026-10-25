# Endgame queue-economics: implicit critical-pair re-unification

Scoping plan (2026-07-24) for the last open piece of the Waldmeister (WM)
faithfulness arc: the endgame critical-pair (CP) storage economics that
block thvm from proving the near-confluent Sheffer / andassoc class where
`FindEquationalProof` (FEP, the WM the Wolfram Language wraps) succeeds.

## PHASE 0 EXECUTED 2026-07-24 -- verdict GO, with a REDESIGN

The A/B below (section 3, phase 0) was run at main `145c5ed6` on
`andassoc_kbo.pr` (WM preset, FIFO off, 150k steps / 60s, artifacts banked
at `~/.cache/thvm/endgame_implicit_cp/phase0_2026-07-24/`):

- EAGER: 150k steps in 8.4s, rules=606, cps=1679997, peak RSS 2155MB.
- Existing `THVM_ATP_IMPLICIT_CP=1` (the trace-backed lane of section 1):
  **REFUTED** -- trajectory diverges from step 250 (cps 157647 vs 17106 =
  9x queue inflation), 1000 steps take 27.8s (25x slower), RSS blows past
  6.3GB by ~step 1000. Root cause: the lane routes ALL Term-backed
  rule-x-rule CPs, bypassing the WM preset's KPBehandelt <50 push
  treatment (normalize + join-drop), and pins raw pairs via the GC-rooted
  trace.
- Structural finding: the lane does not even COVER the class that matters.
  The WM preset's raw>=50 untreated class (~84% of packed CPs) never
  builds heap Terms -- the emit walk writes the packed byte string
  directly (src/atp/_.c:2608, :2641) -- so the trace-backed lane (which
  predates the KPBehandelt raw port 70b1bd91) passes it to the eager
  packed path.

REDESIGN (what phase 1 actually builds): a NEW opt-in descriptor lane FOR
the raw>=50 untreated class only (`THVM_ATP_RAW_DESC=1`). The emit walk
still runs every push gate and computes weight on the raw pair (WM
C_Classify); only the STORAGE changes -- a flat descriptor (parent ids +
overlap position + weight/priority/seq) replaces the malloc'd byte
string. Pop-time materialization re-derives the pair from the parents
(FT-native unification) byte-exactly; parent RHS-compose mutation is
handled by versioned pre-mutation sides (or reify-before-mutate). The <50
treated class stays eager. Trajectory identity vs eager then holds BY
CONSTRUCTION (same gates, weights, seq -- only storage differs) and is
enforced by a mandatory certifier mode (`THVM_ATP_RAW_DESC_CERT=1` stores
both, memcmps at every pop, fatal on mismatch).

Sections 1-3 below are the original scoping (the trace-backed unpin
framing); superseded by the redesign above but kept for the
infrastructure map and gate definitions.

## 0. Why this arc (verified current state, not the stale trie premise)

The trie / selection-order parity arc is DONE. On `andassoc_kbo.pr` at main
`145c5ed6`, thvm is byte-exact to WM for `firstdiv = 38291` selections
(prefix 38290), 13x past the old pick-2923 fork that
`docs/ATP/atp_andassoc_parity.md` (stale, HEAD 25240924) describes.
Selection order is no longer the blocker.

What blocks proving the class is the ENDGAME. Measured this cycle,
C-direct, on the same HEAD:

- `WolframAxioms__ImpliesShefferAxioms__c3.pr` (the failing conjunct of
  `TFindProof["ImpliesShefferAxioms", "WolframAxioms"]`) diverges under
  every config: KBO 2M steps -> 1993 rules climbing / 16.17M live CPs, no
  close; LPO -> ResourceOut; goal-directed `--mnf --goal-narrow` -> 7515
  rules, no close.
- `andassoc_kbo.pr` itself, despite the 38290-pick parity, does not crack
  (prior banked runs: no crack at 20GB resident / 180s).
- FEP proves the whole `ImpliesShefferAxioms` conjunction (c3 included) in
  112.8s / proof length 689.

Root cause: thvm generates and holds roughly 16M live CPs as fully unified
packed byte strings and is about 8x slower per CP than WM (WM ~646k CPs/s
vs thvm ~23k/s). WM holds its passive set as ~3.75M implicit term-pair
descriptors and materializes a CP only when it is treated, so it never
pays the storage or the up-front normalize for CPs it will discard. thvm
exhausts memory and wall before reaching WM's proof depth (~3.75M CPs /
1601 rules / 6.1s for andassoc_kbo).

Win threshold: thvm proving c3 / andassoc AT ALL (it currently cannot),
ideally under FEP's 112.8s.

## 1. Infrastructure that already exists (opt-in, currently KEEP-OFF)

The deferred-CP arc landed the scaffolding as opt-in and was benched to a
KEEP-OFF verdict; it is on main behind `use_implicit_cp`:

- `AtpCpImplicit` (src/thvm.h:4930), a 20-byte descriptor, static-asserted:
  `parent_a_trace_id`, `parent_b_trace_id`, `overlap_position` (reserved,
  currently 0), `weight`, `priority`.
- Parallel arrays `cp_implicit[]` / `cp_is_implicit[]` (bitset), lazily
  allocated under `use_implicit_cp`; both stay NULL when off so the default
  path is byte-identical.
- `atp_cp_implicit_push` (src/atp/_.c:14311), `atp_cp_implicit_materialize`
  (src/atp/_.c:14652), the shared slot reader `atp_cp_slot_read`
  (src/atp/_.c:14678).
- Landed by commits 4e8c1cb7, 239c04f0, c08d3504, e3df0326, 5bba9577.

Documented KEEP-OFF verdict (see the sheffer-lpo memory, 2026-06-10):
failed 3 of 4 flip criteria as built. On the trace-backed design it
regressed peak resident set (andassoc 2.13x, to 3591MB), inflated steps
(mccune 2.8x), and added wall (+55%). Two named causes: (a) the descriptor
pins its raw overlap terms through the trace root set; (b) it loses the
queue-vs-queue subsumption the eager path does.

## 2. Re-diagnosis: why the verdict is beatable for the WM-faithful class

The 2026-06-10 verdict tested the wrong regime for this goal.

- The step-inflation penalty is a NON-ISSUE on the WM path. The WM presets
  already set `use_queue_subsume = 0` (src/atp/_.c:4665-4670, "the queue
  must match WM"). The 2.8x inflation was measured on mccune, a
  queue-subsume-ON preset. On andassoc / c3 under the WM preset, BOTH the
  eager and implicit paths skip queue subsumption, so there is no
  inflation from that axis. Phase 0 verifies this directly.
- The resident-set pin is the real killer, and it has a DESIGNED fix that
  was reserved but never built. `atp_cp_implicit_materialize`
  (src/atp/_.c:14665-14668) reads the raw unified pair from the `TRACE_CP`
  entry's children 2 and 3; those terms are reachable from `s->trace[]`,
  which is a garbage-collection root set (src/atp/_.c:8021), so they are
  never reclaimed. WM does not keep the raw pair; it re-derives it from the
  two parent rules on treatment (`U1_KPRekonstruieren` /
  `TPR_TP2ParIntermed`). The descriptor's `overlap_position` field is
  reserved for exactly this WM-style re-unification variant. Building it
  lets an implicit slot re-unify `parent_a x parent_b` at materialize (both
  parents are rooted rules regardless), so the `TRACE_CP` raw pair need not
  be rooted for implicit slots. That is the 20-byte-descriptor memory win
  the arc set out to get, currently swamped by the pin.

## 3. Phased plan

Phase 0 -- measure, no code (cheap decision gate):
- Reconfirm the firstdiv 38290 baseline (done this cycle).
- A / B eager vs `use_implicit_cp` ON under the WM preset on
  `andassoc_kbo.pr`: are the step / rule / CP counts byte-identical on the
  raw-large CP path (the KPBehandelt raw>=50 class that queues raw and
  normalizes at pop)? If yes, implicit is a pure memory shrink with no
  trajectory change on the WM class -- the ideal case. If no, characterize
  the delta and its source (normalize timing, ordering).
- Profile where andassoc_kbo currently dies (term heap vs resident set vs
  throughput) and the projected proof depth versus WM's 3.75M CPs.

Phase 1 -- unpin via re-unification (the core lever):
- Implement the `overlap_position` variant: `atp_cp_implicit_push` records
  the overlap position; `atp_cp_implicit_materialize` re-unifies
  `parent_a x parent_b` at that position instead of reading the rooted
  trace children.
- Stop rooting the `TRACE_CP` raw pair (children 2 / 3) for implicit slots
  (a collectable tier, or drop them and let proof reconstruction re-derive
  from parents too).
- Gate: peak resident set drops below the eager path on andassoc (target
  sub-1x, realizing the descriptor shrink). Trajectory byte-identical to
  eager under the WM preset (established in phase 0).

Phase 2 -- throughput, only if phase 1 is insufficient to crack:
- Re-unify-at-materialize adds pop-time unifier cost. Keep it cheap (the
  flatterm-native unifier, memoized). Profile whether the residual ~8x
  per-CP gap now binds the wall. This is the uncertain phase; the
  per-component throughput levers are largely tapped (sheffer-lpo memory).

Phase 3 -- flip evaluation:
- Does andassoc_kbo / c3 crack within budget now? Measure wall against
  FEP's 112.8s.
- Flip the WM preset default to `use_implicit_cp` ON only if ALL hold:
  proof validity, resident-set reduction, trajectory-identical on the
  66-theorem matrix, and a crack (or clear, quantified progress) on
  andassoc.

## 4. Gates (hard; a regression on any is a bug in the change)

Default path (`use_implicit_cp` off) stays byte-identical throughout:
- `bin/test_atp` exact (capture the current count first).
- OA lazy `WolframAxioms__OrAssociativity.pr 500000 280` ->
  257971 / 735 / 2461555 PROVED, byte-exact.
- DN `WolframAxioms__DoubleNegation.pr` -> 2848 / 254 / 768876 PROVED.
- FEP-style non-lazy OA (`THVM_ATP_LAZY_NORM=0`) ->
  271083 / 775 / 1730452 PROVED.
- andassoc_kbo firstdiv 38290 (align.py).
- The 66-theorem faithful-corpus parity sweep (tools/parity_diff_wm.sh)
  byte-identical vs baseline. MANDATORY: regressions here are invisible to
  the standard gates (orassoc-enclosing-jump memory).

Implicit-ON success criteria (phase 3):
- resident set below eager on andassoc, trajectory-identical on the WM
  class, and a crack or quantified progress toward WM's proof depth.

## 5. Risk assessment (honest)

- Memory-holding is NECESSARY but may not be SUFFICIENT. Even with the pin
  fixed, thvm is ~8x slower per CP and generates ~4x more CPs than WM (16M
  vs 3.75M), so roughly 32x WM's CP-work. andassoc could take minutes even
  memory-bounded, possibly beyond FEP's 112.8s. Phases 1-2 might deliver
  the resident-set win without the crack. That is still progress (the arc's
  stated purpose was resident-set reduction) but would not, by itself, beat
  FEP on this class.
- Re-unification correctness: the `overlap_position` re-derive must
  byte-match the trace-stored pair. Gate on the soa / CPSEL byte-identity
  stream, not just totals.
- Proof reconstruction: dropping the rooted trace children must not break
  the proof extractor; it must re-derive from parents.
- Regression risk is contained. The implicit path is opt-in; the default
  stays byte-identical, so risk lands only at the phase-3 flip decision,
  gated on the mandatory matrix sweep.

## 6. Effort

- Phase 0: about half a day (measurement, one theorem, C-direct).
- Phase 1: one to two days (the re-unification variant, unpin, gates).
- Phase 2: uncertain (throughput; quick win or rabbit hole).
- Phase 3: about half a day (flip evaluation).
- Total roughly three to five focused days, one dedicated worktree worker,
  serialized runs, resident-set-guarded, matrix-validated.

## 7. Go / no-go

Phase 0 is the cheap decision gate. GO to phase 1 if implicit-ON is
byte-identical to eager on the WM andassoc path (no trajectory change) AND
the profile projects that a resident-set win lets andassoc reach roughly
WM's proof depth within a plausible wall. If phase 0 shows implicit-ON
changes the WM-class trajectory, or the throughput projection puts a crack
far beyond FEP even memory-bounded, STOP after banking the resident-set
result and report that the class is throughput-bound, not storage-bound
(which would redirect the remaining effort to the per-CP throughput axis,
the harder and less certain lever).

The disciplined recommendation: run phase 0 (cheap, one theorem, no code),
then decide phase 1 on its result rather than committing the full arc up
front.
