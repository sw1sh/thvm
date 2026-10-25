# Unified Automatic ATP engine: WM completion + Vampire/Twee UEQ superposition

Date: 2026-07-08. Design campaign (workflow wf_265cac0a: 3 scouts ->
architect -> adversarial critique) + a decisive external-prover
diagnostic. Supersedes the "VampireUEQ is a preset not a loop" framing.

Directive: implement a faithful Vampire-style saturation loop as a real
engine Method, unified with WM completion under ONE `Automatic`
scheduler carrying BOTH Knuth-Bendix-completion lessons AND
Vampire/Twee strategy best-practices; goal is to BEAT the `Waldmeister`
single preset on the deep NotableTheorems + CASC-30 UEQ set.

## Central decision (architect, verdict SOUND per critique)

**Family 2 is NOT a separate saturation loop. It is the SAME unfailing
Knuth-Bendix completion loop (`thvm_atp_step`, src/atp/_.c:12930),
extended behind a mode flag with additive policies.** For the
unit-equational (UEQ) fragment, superposition-refutation and unfailing
completion are provably the same calculus:

- superposition between positive units == critical-pair formation;
- demodulation == order-gated rewriting;
- equality resolution degenerates to the ground `x != x` reflexivity
  test; equality factoring is vacuous on single-literal clauses
  (verified: `equality_resolution` / `equality_factoring` appear
  0/148 times across the banked Vampire proofs).

Completeness: Bachmair-Dershowitz-Plaisted 1989 (unfailing completion)
and Bachmair-Ganzinger 1994 (superposition) coincide on units -- both
generate the ordered overlaps of positive units, simplify under the
same redundancy notion, and build the same rewrite model. Refuting
`~(s=t)` is the same computation two ways: superposition keeps the
ground Skolemized `sk_s != sk_t` active and rewrites it toward `x!=x`;
completion joins `s-nf =? t-nf`. `sk_s != sk_t` reduces to `x!=x` iff
`s` and `t` reach a common normal form (theorem of constants). A
separate C loop would be ~90% duplication with zero proving delta and
would double the byte-parity surface guarding the crown-jewel WM path.

**Empirical proof this is the right shape: Twee** (a completion-based
UEQ prover) is CASC-competitive with Vampire. See the diagnostic below.

## Decisive diagnostic (external provers, 2026-07-08)

Ran Twee (`~/.cabal/bin/twee`, pure completion + ground-joinability +
encompassment) on the four hardest gates -- theorems the thvm
`Waldmeister` preset FAILS:

| Theorem | thvm-WM | banked Vampire | **Twee** |
|---|---|---|---|
| McCuneAxioms/EqualityOfInverses | fails >600s | proves (plain superp) | **0.05s** |
| HillmanAxioms/ImpliesShefferAxioms c3 | fails | proves via inequality_splitting + naming | **0.20s** |
| MeredithAxioms/ImpliesShefferAxioms c3 | fails | proves via function_definition | **0.24s** |
| WolframAxioms/ImpliesShefferAxioms c3 | fails | **TIMES OUT** | **9.78s** |

Two campaign-reshaping facts:
1. **Pure completion reaches every gate** -- the non-UEQ machinery
   Vampire used (inequality splitting, definition naming) is NOT
   necessary. This REFUTES the critique's central reachability doubt.
2. **The gap is not calculus, it is selection + redundancy +
   goal-participation.** Twee's McCune proof derives ~16 small lemmas
   then joins the goal by rewriting both sides through them
   (bidirectional `R->L`). thvm-WM stalls at 600s+ not for lack of an
   inference rule but because its WM-faithful selection explores a
   vastly larger space without deriving/prioritizing those lemmas.
   McCune's 0.05s-vs-600s (>12000x) is an ALGORITHMIC guidance gap.

Twee even BEATS Vampire on WolframAxioms/ImpliesSheffer c3 (9.78s vs
Vampire timeout) -- so the ceiling for thvm-with-Twee-mechanisms is
high and the targets are real, one-at-a-time measurable wins.

## Architecture

ONE engine, ONE completion loop, TWO calculus families as portfolio
slices sharing ALL term infrastructure, selected by Method dispatch.

Shared verbatim (no forks): FT cells + discrimination-tree indexing;
KBO/LPO + precedence; unification; the CP former (== superposition);
demodulation (== fwd/bwd rewriting); subsumption + FV-index; tautology
deletion; ground joinability; age:weight:random:LRS selection; Skolem
grounding (`atp_grounded_instance` + `min_const`).

New surface, all option/mode-gated so the byte-perfect WM path (mode 0)
is physically unchanged:
- `AtpState.atp_mode` (0 = WM completion; 1 = refutational UEQ) --
  a NEW flag (model it on the LIVE `use_reversed_completion` wiring,
  ATP.wl:2505 -> thvmlink_atp.c:993; NOT a dead stub -- critique fix 8).
- a goal-disequation slot (Skolemized ground sides) that R rewrites;
- a clause-provenance bit (descends-from-goal) biasing selection;
- an encompassment rewrite-permission predicate (the matcher already
  exists: `atp_gj_strictly_more_general` :19497).

## Phased plan (critique-corrected)

Every phase gate: `bin/test_atp` green; the WM byte-parity corpus
(`~/.cache/thvm/wm_parity`, 128/128) IDENTICAL after the change; new
work additive behind Method dispatch. Measurement is per-problem, ONE
theorem at a time (no sweeps).

**Phase 0 -- scaffolding (no proving-power change).** `atp_mode` flag +
setter + positional arg + preset entry; mode-0 branch at the top of
`thvm_atp_step` is a physical passthrough. Confirm fresh
`thvm_atp_init/free` per slot survives a MIXED mode-0/mode-1 schedule.
CRITIQUE FIX 6 (the real feasibility landmine): the `g_ftnfm` NF memo
is reduction-ordering-independent and process-global (gen-bumped per
init). Encompassment (Phase 3) CHANGES what a term normalizes to, so
the mode/encompassment bit MUST enter the `g_ftnfm` structural key (or
gen-bump on every mode transition within a portfolio run) -- else the
`atp_heap_leak.md` wrong-NF poison reappears intra-run. Add a
mixed-mode-schedule test (encompassment-ON slot immediately before a
WM-parity slot). Smoke gate: mode-1 reaches PROVED on
AbelianGroupAxioms/InverseOfInverse.

**Phase 1 -- Automatic = honest two-family portfolio (WIRING, not new
power).** Promote `Method -> Automatic` to a curated complementary
schedule carrying both WM-completion and Vampire/Twee-flavored slices,
front-loaded by structure detection, fixed CASC tail. CRITIQUE FIX 4:
bill this as dispatch, NOT "beat WM" -- portfolio > single-preset is
tautological (VampireUEQ already proves WolframAxioms/ImpliesWolframAlt
with zero C change). Gate: Automatic PROVED-count >= max over
individual presets, >= 1 case no SINGLE preset gets in the same budget.

**Phase 2 -- refutational goal driver.** Admit the negated+Skolemized
ground disequation as a first-class active object; halt on refutation.
CRITIQUE FIX 2 (load-bearing): the halt is NOT only `x!=x` reflexivity
-- 63/141 banked proofs (45%), incl. Meredith c3 and McCune, terminate
via `forward_subsumption_resolution` (the goal disequation resolved
against a STORED, possibly unorientable, equation). Implement the halt
as "goal literal `s'!=t'` is subsumed by a stored equation (its sides
are a ground instance of some `u=v`)" AND the `kbo_eq`-after-normalize
case. CRITIQUE FIX 7: adding the provenance field to the `cp_*` arrays
can perturb selection tie order (libc qsort landmine) even with the
read mode-gated -- run a DEDICATED wm_parity re-cert after the struct
change, before any behavioral use. Gate: >= 1 gate theorem newly
PROVED whose Twee proof confirms pure-UEQ reachability (NOT Hillman c3,
whose Vampire proof needs splitting -- but Twee proves it in 0.20s, so
it IS reachable; re-verify the thvm path finds it).

**Phase 3 -- encompassment demodulation (the one genuine calculus
delta).** Permit demodulation by a rule whose matched instance is a
proper encompassment (subsumption-smaller) even when not strictly
reduction-order-smaller; completeness preserved (valid Bachmair-
Ganzinger clause redundancy). CRITIQUE FIX 3: do NOT gate this on
McCune alone -- McCune's Vampire proof uses no encompassment, and the
Twee McCune proof (0.05s) shows the win is mostly good lemma SELECTION
+ goal-directed rewriting + a lean rule set, of which encompassment is
one contributor. Before landing, diagnose McCune's actual thvm stall
(read Twee's ~16-lemma proof; is the missing lever selection, the
subsumption terminator from Fix 2, or encompassment?) and pick the
gate whose Twee proof shows encompassment/interreduction load-bearing.
Encompassment OFF by default / mode-gated; termination re-verified on
the whole suite.

**Phase 4 -- LPO ground joinability + goal-scheduled follow-ons.**
Generalize `gj_less_in` beyond KBO for the LPO slices; fold the goal
disequation into the age:weight queue. Portfolio diversity.

**Phase 5 (stretch) -- DISCOUNT lazy given-clause superposition.** Form
superpositions lazily at activation rather than eagerly at rule-add.
Same UEQ closure; a distinct diversity slice. Skippable if 1-4 clear
the target set. Honest note: RobbinsAxioms/DoubleNegation and
WolframAxioms/ImpliesSheffer c3 may stay hard (banked Vampire times
out on both; Twee gets the latter in 9.78s -- so it IS a target, via
selection quality, not out of reach).

## MEASURED FINDINGS (2026-07-08, direct thvm runs -- correct the agents)

The design + critique agents' gate claims were not independently
verified and are partly WRONG. Measured truth:

1. **McCune/EqualityOfInverses is NOT a gate.** thvm-WM PROVES it in
   **0.09s** (177 steps, 16 rules; goal joins at n_rules=16, matching
   Twee's ~15 lemmas). The "600s stall" was false. Remove it as a gate.
2. **The 3 real gates (Hillman/Meredith/WolframAxioms ImpliesSheffer
   c3) fail thvm-WM via CP-QUEUE EXPLOSION** -- 5.26M CPs -> 5.3GB RSS
   abort in ~24s. Failure mode = REDUNDANCY, not stall.
3. thvm's EXISTING aggressive-redundancy knobs (THVM_ATP_GROUND_JOIN=1
   BWD_SUB=1 BWD_DEMOD=1) cut Hillman c3 CPs **5.26M -> 18.6K (283x)**,
   kill the memory abort -- BUT the pruned search grinds at ~30 steps/s
   (vs 2131 baseline; backward ops are O(rules), unindexed) and **does
   NOT converge in 240s** (133 -> 181 rules, CPs still growing). NOT a
   pure-speed problem. LPO instead of KBO: same. Goal-weighted CP
   selection (mode 7): even slower (4.5 steps/s), no crack. CP-set-IR:
   no crack. **NO existing-knob combination proves any gate.**
4. **=> Phase 1 (portfolio wiring of existing presets) will NOT crack
   these three** -- confirmed by measurement, not just the critique's
   suspicion. Phase 1 is honestly "arm redundancy so we stop CRASHING
   and stay lean," a prerequisite, not a crack.
5. **Twee proves Hillman c3 with 17 TARGETED lemmas in 0.2s** (goal
   joins via a 111-step rewrite chain); thvm wanders to 181
   mostly-irrelevant rules and never assembles the join. The goal is
   ground (skC1/2/3), MNF is armed (mixmost=1), yet the goal NF sides
   stay stuck at different forms -- the goal-RELEVANT lemmas are never
   derived/selected in budget.
6. **Twee's edge, concretely:** (a) goal-directed selection deriving
   FEW goal-relevant lemmas; (b) MULTI-ORDERING ground joinability
   (Twee tries several orderings per gj test; thvm's gj_less_in is
   single-ordering) -- a NEW lever not in the plan; (c) cheap
   (indexed) backward simplification (thvm's is the ~30-steps/s
   bottleneck -> the FT-representation P1 speed work is directly on the
   critical path here, not a separate track).

REVISED PHASING CONSEQUENCE: the FIRST genuine crack is a real Phase-2
build (goal-directed lemma selection) + likely multi-ordering GJ, on
Hillman or Meredith c3 (NOT McCune). Redundancy knobs (Phase 1) are
necessary scaffolding. The FT-representation speed roadmap (P1
preorder-streaming, indexed backward simplification) is ON the critical
path -- it turns the lean-but-30-steps/s search into a fast one, which
is half of why Twee wins. Speed and coverage are the same lever here.

## TERMINAL DIAGNOSTIC VERDICT (config space exhausted)

Two campaign premises were CORRECTED by direct measurement (details in
Measured Findings): McCune is not a gate (proves 0.09s), and thvm
ALREADY HAS Twee's multi-ordering ground joinability (gj_cover, "exactly
as Twee's groundJoin"). Every existing lever was then tested on the real
gates (Hillman/Meredith c3) and NONE crack them:
- WM preset: CP explosion (5.26M -> 5.3GB abort).
- +redundancy (GJ+bwd-sub+bwd-demod): lean 18K CPs, no converge @240s.
- +AutoMaxWeight: bounded, Timeout (no crash, no proof).
- LPO / goal-weight (mode 7) / CP-set-IR / FindProof --schedule: all fail.
- GJ_BUDGET=16 (17x throughput, 520 steps/s, 386 rules @60s): STILL no
  crack. THROUGHPUT IS NOT THE GATE-LEVER.
=> The gate gap is DEFINITIVELY goal-directed SELECTION QUALITY: thvm
derives hundreds of rules but not the ~17 goal-relevant lemmas Twee
targets. No configuration of existing mechanisms closes it. Speed
(FT-representation P1) remains the lever for the ~1.6x WM-EMULATION gap
and helps throughput, but the GATE crack requires genuinely better
goal-directed CP selection than any thvm mode provides -- a real Phase-2
build (the core of what makes Twee good; uncertain, research-grade).
thvm's existing CP_WEIGHT_GOAL (mode 7) and MNF front are the seeds but
are demonstrably insufficient. This is the honest terminal state: the
config space is mapped and exhausted; the remaining work is a selection-
heuristic build, not a knob.

## The revised proving-power thesis (post-diagnostic)

The first GENUINE-new-power win is **matching Twee on
McCune/EqualityOfInverses** (0.05s, the most extreme WM stall) -- a
theorem NO thvm preset proves. Its mechanism per Twee's proof is
selection + goal-directed bidirectional rewriting + a lean interreduced
rule set (Phases 2+3 combined), NOT a portfolio re-skin (Phase 1) and
NOT exotic calculus. Reproducing Twee's guidance is the campaign's
technical core.

## Open correctness item (critique)
Confirm `atp_rewrite_normalize` applies the ordered-instance (unfailing)
rewrites to the goal literal, not only oriented rules -- refutational
completeness for a goal joined via an UNORIENTABLE equation depends on
it. The design asserts but does not show this.

## Measurement harness
Per-problem, one theorem at a time. For a named theorem record
{status, wall, steps, proof-length} for (1) thvm `Method -> Automatic`,
(2) thvm `Method -> "Waldmeister"`, (3) banked Vampire (read
tools/baselines/vampire_raw/, not re-run), (4) Twee (the new pure-UEQ
oracle, ~/.cabal/bin/twee --tstp on tools/baselines/vampire_tptp/*.p).
First target: McCuneAxioms/EqualityOfInverses. Rotating CASC-30 UEQ
pick from ~/.cache/thvm/casc/Problems30/UEQ (300 problems).

## Simpler-join port (2026-07-11): Twee's join discipline, measured

`--simpler-join` / `THVM_ATP_SIMPLER_JOIN` ports the redundancy
discipline behind Twee `joinCriticalPair` (Join.hs:55, allSteps =
step1 >=> step2 >=> step3): when the standard join normalize leaves a
CP's sides distinct, a second gated R+E pass may still join them --
every step requires the rewriting equation strictly simpler than the
CP equation (Equation.hs:59 `simplerThan` over `lessEqSkolem`, the
skolem-ground lex compare in src/atp/ft_order.c), and unorientable
instances additionally fire when strictly decreasing under the
skolem-ground KBO (Rule.hs:425 `reducesSkolem`).  Verdict-only (kept
CPs queue their engine NFs; trace/replay untouched); value 2 =
selection-time only (Twee's own placement).  Known divergences: no
step3 below-peak constraint, no checkOrder, no variable
canonicalisation in the compare; hence the mode gates
cli_strategy_complete (GaveUp, never Satisfiable).

Measured on the 14 remaining twee-residual UEQ timeouts (30s wall,
single-strategy): bare SJ 0/14; SJ+encompass 0/14; SJ+flatten 0/14;
SJ+mnf-gj 0/14; SJ+twee-score 1/14 -- LAT159-1 proves in 1.8s vs
20.3s for the identical config WITHOUT the gate (11x; the 20.3s
variant never fit the backup slot's leftover wall, so schedule-wide
the problem timed out).  The join pass culls heavily where it engages
(LAT138-1: 154K of 497K push-joinable drops from the gate at 12s) but
pruning volume alone flips nothing -- the win appears only combined
with Twee-native SELECTION (cp-weight=8 best-first), consistent with
the terminal diagnostic above.  Composed by adding --simpler-join to
the schedule's twee slice.

## Phase 2 goal driver: BUILT (--goal-drive), sound, uncracked (2026-07-19)

Built the refutational goal driver as an opt-in knob (--goal-drive /
THVM_ATP_GOAL_DRIVE / thvm_atp_set_use_goal_drive), interleaved with
completion inside thvm_atp_goal_check (src/atp/_.c).  It makes the
ground negated goal a first-class ACTIVE participant: a backward
condensed-detachment regression over the RAW ifeq-guarded equations
(no decode -- unlike --horn-back), reading the LIVE rule set R so
completion's oriented definitions bridge the goal's connective forms
(BASE-1 goal-side NF) and completion's derived theorem(P)=true facts
discharge the ifeq guards (BASE-2 unify / the bidirectional meet).
Design: functional SLD over immutable hash-consed Terms (the unifier
sigma is applied to the continuation, no binding trail); iterative
deepening + a ground-goal memo (proved lemmas, regularity/onpath loop
prune, per-depth fail cutoff); per-run bump-heap checkpoint+reset
(reclaims the transient search Terms wholesale), a wall deadline, a
node/cell budget, and a subgoal size cap (horn-back's g_bk_size_cap
twin -- without it the backward MP regression diverges into
problem-INDEPENDENT nested implications theorem(implies(Y,implies(Y',
...))), the tell being byte-identical variant counts across distinct
problems).

Soundness: every closure is a genuine ground instance of R union the
dispatcher equations (the reducer ifeq(A,A,B,C)=B, whose presence
atp_ifeq_sym_is confirms by detection).  VERIFIED: proves a hand-built
2-step CD chain in 8 variants (mechanism correct); NEVER closes a
non-theorem (5.9M-variant search on an unprovable goal, 0 closes); all
CASC-J13 Satisfiable canaries + RNG030/031/032 stay GaveUp WITH the
driver armed; test_atp 136264/136264 and soundness.sh unchanged
(default-off, byte-identical default path).

Result on the '-10' gate family: does NOT crack LCL306-10 / LCL348-10 /
LCL339-10 / LCL890-10 within a 60-240s budget under any tested
composition (bare, tgt, tgt-fold; strides 1-150; depths 12-60; nodes
15k-100M).  Diagnosis, corroborated by the banked Vampire proof
(winning strategy dis+10_50:sp=weighted_frequency:tgt=full,
definition_folding of the goal into sF constants; 92 superpositions +
22 forward_demodulations): the proof is a forward chain that SUPERPOSES
rule_2 with derived theorem units under GOOD SELECTION.  Backward
regression must guess the modus-ponens major premise at every step --
the exact DUAL of the forward selection gap this doc's terminal
diagnostic already isolated.  The driver's crude rigid-first selection
explores ~400k goal variants without following the ~92-step chain.
The mechanism (goal as active backward participant) is now in place and
sound; closing the family still needs the goal-directed SELECTION
heuristic (the research-grade lever), now applicable to EITHER the
forward CP queue or this backward frontier.
