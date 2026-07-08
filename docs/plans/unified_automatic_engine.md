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
