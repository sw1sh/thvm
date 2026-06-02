# Vampire teardown - AbelianGroupAxioms/ImpliesAbelianMcCuneAxioms

Case picked from the first row of the comparator
(`compare_vampireueq_vs_vampire.tsv`): both Vampire-CLI and thvm
PROVE it, but thvm-VampireUEQ takes **0.69 s** versus Vampire-CLI's
**0.06 s** -- an 11x wall gap on a trivial 5-axiom problem.  This
file documents what Vampire actually does, step by step, so the
port can match the source's speed + shape and not just its
status.

Source paths in this doc are relative to `/Users/swish/.vampire-src/vampire`
(Vampire 5.0.1) and `/Users/swish/src/thvm` (this repo).

## TPTP problem

```tptp
cnf(ax1, axiom, and(X1,and(X2,X3)) = and(and(X1,X2),X3)).
cnf(ax2, axiom, and(X1,X2)         = and(X2,X1)).
cnf(ax3, axiom, and(X1,op_overtilde(k1)) = X1).
cnf(ax4, axiom, and(X1,not(X1))    = op_overtilde(k1)).
cnf(goal, negated_conjecture,
  and(and(and(sk_c1,sk_c2),sk_c3),not(and(sk_c1,sk_c3))) != sk_c2).
```

Abelian group axioms (with `op_overtilde(k1)` = identity `e`, `not`
= inverse) + a McCune-style "absorption" goal.

## Winning Vampire strategy

Vampire 5.0.1's UEQ portfolio (`CASC/Schedules.cpp:5219`) opens with
the line

```
lrs+10_1:1_sil=4000:st=3.0:i=102:sd=2:ss=axioms:sgt=8_0
```

which cracks this problem in the first 5 ms slot.  Decoded:

| token | meaning | thvm equivalent |
|---|---|---|
| `lrs+10` | saturation algorithm = LRS (`Saturation/LRS.{hpp,cpp}`, extends `Otter`), strategy template id +10 | `"LRS" -> True` (partial: thvm's LRS is queue-prune only, no estimated-reachable-count) |
| `_1:1` | age:weight ratio in the passive queue alternation | `"SelectionRatio" -> 2` (thvm SR=N means 1 FIFO per N picks; SR=2 ~ 1:1) -- **MISMATCH**: thvm's `VampireUEQ` preset uses SR=10 (= 1:9 weight-bias) |
| `sil=4000` | strategy instruction limit (kInstr) | -- (thvm uses wall-seconds not instruction count) |
| `st=3.0` | `sine_tolerance` = 3.0 | `"AxiomRelevance" -> {"SInE", "SineTolerance" -> 3.0, ...}` -- thvm default 3.0 already |
| `i=102` | per-strategy instruction budget | -- (thvm divides wall fairly across portfolio slots) |
| `sd=2` | `sine_depth` = 2 | `"SineDepth" -> 2` (thvm default 2) |
| `ss=axioms` | `sine_selection` = axioms only (don't filter the conjecture's symbols) | -- thvm's SInE always filters axioms; no `ss=` toggle |
| `sgt=8` | `sine_generality_threshold` = 8 | `"SineGenerality" -> 8` (thvm default 8) |

**Key port gap (config-level)**: thvm's `"VampireUEQ"` preset
(`wl/THVMLink/Kernel/ATP/ATP.wl:2442`) currently uses:

```mathematica
"Ordering" -> "LPO", "AutoPrecedence" -> True,
"SelectionRatio" -> 10, "UnfailingCP" -> True,
"AutoMaxWeight" -> True,
"BackwardSubsume" -> True, "BackwardDemod" -> True,
"RHSInterreduce" -> True
```

The winning Vampire UEQ schedule entry for this case uses default
ordering (KBO), age:weight=1:1, SInE, and *no* explicit Backward*
flags.  thvm's preset is modeled on a DIFFERENT Vampire UEQ entry
(later slot, `to=lpo`-flavored).  Either we drop SR to 2, switch to
KBO, and add SInE -- or document which exact Vampire portfolio entry
the preset clones.  Right now the preset's slot label doesn't match
the slot that actually wins for this class of input.

## Proof DAG (15 SZS steps, captured in `/tmp/vamp_full.out`)

```
f1 axiom        ax1 (assoc)                        file
f2 axiom        ax2 (comm)                         file
f3 axiom        ax3 (identity)                     file
f4 axiom        ax4 (inverse)                      file
f5 plain        e = and(X0, not(X0))               reorient_equations(f4)
f6 negated_conj sk_c2 != ...                       file
f7 plain        ... != sk_c2                       reorient_equations(f6)
f8 plain        rhs rewritten by assoc once        forward_demodulation(f7, f1)
f9 plain        rhs rewritten by assoc again       forward_demodulation(f8, f1)
f13 plain       and(e,X0) = X0                     superposition(f3, f2)
f23 plain       and(e,X1) = and(X0,and(not(X0),X1))superposition(f1, f5)
f33 plain       e = and(X0,and(X1,not(and(X0,X1))))superposition(f5, f1)
f35 plain       and(X0,and(not(X0),X1)) = X1       forward_demodulation(f23, f13)
f40 plain       and(X1,and(X0,not(X1))) = X0       superposition(f35, f2)
f41 plain       and(X1,X0) = and(not(not(X1)),X0)  superposition(f35, f35)
f293 plain      and(X0,e) = and(X1,not(and(not(X0),X1)))   superposition(f35, f33)
f296 plain      and(X1,not(and(not(X0),X1))) = X0  forward_demodulation(f293, f3)
f335 plain      not(X0) = and(X1,not(and(X0,X1)))  superposition(f296, f41)
f564 plain      sk_c2 != and(sk_c1,and(sk_c2,not(sk_c1)))  superposition(f9, f335)
f565 plain      $false                             forward_subsumption_resolution(f564, f40)
```

Breakdown of inference rules:

* **2** `reorient_equations` -- PARSE-TIME equation flipping
  (`Parse/TPTP.cpp:3701` constructs the `FormulaClauseTransformation`
  with `InferenceRule::REORIENT_EQUATIONS`).  NOT a saturation
  inference; the ProofObject reconstructor should collapse these
  into their parent axioms.
* **8** `superposition` -- (`Inferences/Superposition.cpp:104`
  `generateClauses`).  ONLY into maximal-side subterms via
  `EqHelper::getSubtermIterator(lit, ordering)`; both forward (this
  clause's literal has rewritable subterm) and backward (this
  clause is an LHS rewriting another's subterm) directions every
  invocation.  Index-driven (`_lhsIndex->getUwa(...)` /
  `_subtermIndex->getUwa(...)`).
* **4** `forward_demodulation` -- (`Inferences/ForwardDemodulation.cpp:90`
  `perform`).  `NonVariableNonTypeIterator` over every non-variable
  subterm of every literal, with an `attempted` `DHSet` to avoid
  retry, generalization-index lookup, encompassing-redundancy gate
  (`_helper.redundancyCheckNeededForPremise`), `it.right()` skip on
  failure to drop the whole subterm subtree.
* **1** `forward_subsumption_resolution` -- closes to $false.

## Per-step throughput gap

Vampire runs ~11 saturation inferences in 5 ms = **0.45 ms / step**.
thvm runs ~25 saturation steps in 690 ms = **27.6 ms / step**.
**60x per-step gap on this problem class.**

(Counting only saturation work: subtract the 4 input axioms and 1
conclusion from each side.  thvm's "ProofLength" 27 includes
SubstitutionLemma intermediates that Vampire bundles into the
forward_demodulation step's parent chain.)

Candidate root causes to instrument (in priority order):

1. **Eager push-time normalize on every CP** -- thvm's
   `atp_cp_trivially_joinable` runs full-R normalize on both sides
   of every push (`src/atp/_.c:11280` and the surrounding eager
   path).  Vampire normalizes lazily via the demodulation index at
   the inference level (one walk per `attempted` set per clause,
   not per CP).  Recent `LazyNormalize` knob (commit `b2acc699`)
   exists but defaults off and breaks on hard cases; the per-step
   savings here would be the bulk of the 60x gap.
2. **Encompassing-redundancy gate missing** --
   `DemodulationHelper::redundancyCheckNeededForPremise` decides
   whether to apply the encompassing check before firing a
   rewrite.  thvm's rewrite path doesn't differentiate; it just
   matches.  Cost: extra unprofitable rewrites that immediately
   get redone.
3. **`attempted` DHSet skip-subtree optimization** --
   `ForwardDemodulation::perform` calls `it.right()` to skip every
   subterm of a failed-rewrite subterm in the same outer pass,
   because if the rewrite didn't fire on `f(t)`, it can't fire on
   `t`'s subterms with the same rule LHS.  thvm's normalize walks
   every subterm independently.
4. **Per-CP weight calc walks full term** --
   `atp_cp_priority_sized` calls `atp_symbol_count` /
   `atp_kbo_weight` on every push, walking the full term.  Vampire
   caches the weight on the literal at construction.

## Inference-rule mapping (Vampire -> thvm)

| Vampire rule | thvm construct | notes |
|---|---|---|
| `superposition(parent_eq, parent_into)` | `{CriticalPairLemma, n}` with `Construct`/`Rule`/`MatchingConstruct`/`MatchingRule`/`Subpattern`/`Position` | thvm carries more metadata (the unifier position, the matching rule's orientation); the ProofObject builder should drop fields that Vampire's SZS doesn't expose |
| `forward_demodulation(parent_target, parent_eq)` | `{SubstitutionLemma, n}` with `Input`/`Construct`/`Rule`/`Position`/`Side`/`Source -> norm` | direct correspondence; chain of forward_demodulation steps maps to a chain of SubstitutionLemma rewrites |
| `reorient_equations(parent)` | (collapse: replace with `{Axiom, n}` or `{Hypothesis, n}` of the reoriented form) | parse-time only |
| `forward_subsumption_resolution(parent_target, parent_subsumer)` | `{Conclusion, n}` with `Source -> cpl` | the empty-clause closer; thvm's final SubstitutionLemma that hits a contradicting hypothesis is the equivalent |

## What to commit next

1. **Tighten `Method -> "VampireUEQ"` preset to match the slot
   that actually fires for UEQ defaults**:
   * SR 10 -> 2 (matches `1:1` age:weight)
   * Add `"AxiomRelevance" -> {"SInE", "SineTolerance" -> 3.0,
     "SineDepth" -> 2, "SineGenerality" -> 8}`
   * Drop `"BackwardSubsume"` / `"BackwardDemod"` from the preset
     defaults (they're not in the winning slot for THIS class) -- or
     ship a SECOND preset `"VampireUEQDefault"` that matches the
     first portfolio slot, keeping the current `"VampireUEQ"` as
     the `to=lpo` variant.
2. **Build SZS -> ProofObject translator** (next iter; the data
   is already parsed by `Wolfram\`Parser\`TPTPImport`).
3. **Profile the per-step gap on this exact case** (instrument
   thvm with `THVM_ATP_PROF=1` if the C engine has it; else add
   timers around the per-push pipeline).  60x is too big to be a
   single missing optimization -- bisect.
