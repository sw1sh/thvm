# `TFindProof` Methods and Options

A practitioner's tour of thvm's automated theorem-proving (ATP) surface.
Covers every `Method` head, every `Method` suboption, every top-level
`Option`, and every return spec as of main `22ed7308`.

Abbreviations used throughout (spelled out on first appearance):

- ATP: Automated Theorem Prover.
- CP: critical pair (a Knuth-Bendix overlap of two rules whose joinability
  must be checked).
- KBO / LPO: Knuth-Bendix Ordering / Lexicographic Path Ordering. The two
  reduction orderings the engine ships.
- MNF: Mutual Normal Form (a bidirectional goal-directed search that
  expands both the conjecture's left- and right-hand-sides through the
  rule set).
- AC: Associative-Commutative (a commutative + associative operator).
- SInE: Sumo-Inspired premise selection (Hoder & Voronkov, IJCAR 2011).

## 1. Overview

`TFindProof[conjecture, axioms]` runs thvm's C ATP engine and returns a
real WL `ProofObject` -- the same head the built-in `FindEquationalProof`
returns, with the full property interface (`p["ProofDataset"]`,
`p["ProofFunction"]`, `p["ProofLength"]`, ...).

Under the hood the engine is an IC-native port of Waldmeister-style
unfailing Knuth-Bendix completion plus a reconstruction pass that decodes
the C-side rewrite chain into a verifying WL `ProofObject`. The same
surface also drives the engine's MNF goal-directed front search, the
Bachmair-Dershowitz connectedness CP-deletion criterion, ground-join CP
deletion (Martin-Nipkow / Twee), Waldmeister's structure-driven precedence
generation, Waldmeister's CPdimension fairness selection ratio, and
Vampire's SInE premise selection.

When to reach for `TFindProof` vs the built-in `FindEquationalProof`:

- The built-in `FindEquationalProof` is bulletproof and handles many of
  the standard cases out of the box. Use it when its default works.
- `TFindProof` exposes every Waldmeister knob the C engine implements,
  ships a problem-aware `Automatic` portfolio that front-loads a tailored
  config per detected algebraic structure, supports a wider range of
  ordered-rewriting / completion configurations, and is meaningfully
  faster on the harder Sheffer / cross-axiom Boolean / combinator goals.

Legacy name: the symbol was originally `TFindEquationalProof`. It is
kept as a deprecated alias and forwards every call to `TFindProof`, so
existing notebooks and downstream code keep working unchanged.

## 2. Loading

All ATP-related public symbols (`TFindProof`, `TATP`,
`TRelevantAxioms`, `TSatEUF`, `TSmtDecide`, `TFindProofSMT`,
`TPTPImport`, ...) live in `THVMLink\`ATP\``. The single load entry
brings everything into scope by bare name:

```wolfram
<< THVMLink`ATP`
```

(Equivalent to `Get["THVMLink\`ATP\`"]` / `Needs["THVMLink\`ATP\`"]`.)
All examples below assume this entry has run. The SMT and TPTP-import
companion entries are documented in `docs/tutorial/smt.md` and
`docs/tutorial/tptp_import.md` respectively.

## 3. Quick start

Each example is intentionally short; expand the `Method` per section 5
for the cases where the default leaves a goal open.

### 3.1 A group theorem (default Automatic)

```wolfram
TFindProof["InverseOfInverse", "AbelianGroupAxioms"]
```

The `Automatic` schedule detects the Group / AbelianGroup structure
(commutative + associative + has-inverse + has-unit) and front-loads
the GT critical-pair weight with `AutoPrecedence` so the inverse
operator ranks above the binary operator -- the inverse-of-inverse
rewrite then orients cleanly. Proves in a fraction of a second.

### 3.2 A Boolean symmetric theorem (GoalDirected)

```wolfram
TFindProof["DoubleNegation", "BooleanAxioms",
    Method -> "GoalDirected", TimeConstraint -> 10]
```

`DoubleNegation` is a symmetric goal whose two sides never share a single
normal form, so plain completion never closes it. `"GoalDirected"` adds
the MNF bidirectional front search alongside completion; the front
collision then resolves into a verifying critical-pair-lemma proof.
Default `Automatic` also closes this (the schedule falls through to
`"GoalDirected"` after the completion configs), just slower; pinning
`Method -> "GoalDirected"` skips the front-load.

### 3.3 A Sheffer / Wolfram single-operator goal (Waldmeister preset)

```wolfram
TFindProof["AndAssociativity", "WolframAxioms",
    Method -> {"Waldmeister",
        "CriticalPairWeight" -> "Gt",
        "CPSetInterreduce" -> True},
    TimeConstraint -> 30]
```

`Method -> "Waldmeister"` is a preset for Waldmeister's faithful DEFAULT
strategy on an unrecognized single-operator nand / Sheffer / Wolfram
problem: KBO + AutoPrecedence + `SelectionRatio -> 51` (Waldmeister's
`itl(mi)` interleaved CPdimension fairness) + `RHSInterreduce` +
`UnfailingCP` + `CPSetInterreduce`. The override `Gt` weight + CP-set
interreduction combination is the one that reaches `AndAssociativity`
through the Waldmeister structure-precedence rule set.

### 3.4 A cross-system many-axiom theorem (SInE premise selection)

```wolfram
TFindProof["ImpliesWolframAxioms", "MeredithAxioms",
    Method -> {"GoalDirected",
        "AxiomRelevance" -> "SInE"},
    TimeConstraint -> 30]
```

`MeredithAxioms` is multi-system; the goal cites only a subset of its
predicates. SInE (Sumo-Inspired premise selection) pre-filters the axiom
list to those reachable from the conjecture's symbols by a bounded
breadth-first walk along the D-relation. Defaults `st=3, sd=2, sgt=8`
mirror Vampire's `--sine_tolerance / --sine_depth /
--sine_generality_threshold`, the option block that won the parallel
Vampire benchmark of thvm's then-uncrackable theorems.

## 4. Methods

`Method` selects the saturator's strategy. Every concrete method head
accepts the same suboption vocabulary (section 5); the head fixes the
broad search shape, the suboptions tune it.

### 4.1 `Automatic` (the default)

Problem-aware portfolio. `Automatic` analyzes the axioms + conjecture,
detects the algebraic structure (a port of Waldmeister's PhilMarlow /
XFiles structure recognition), and front-loads a tailored config for
that structure. It then appends the fixed `"Portfolio"` schedule as a
fallback tail, so `Automatic` only reorders -- it never proves strictly
less than `"Portfolio"`.

Detected classes -> front-loaded config:

- AbelianGroup / Group: GT weight + GoalInterleave 50 + AutoPrecedence.
- Ring: KBO + AutoPrecedence (structure-precedence puts `*` above `+`).
- AC (commutative+associative without inverse): GT weight family.
- Lattice: GT weight + GroundJoin, then an LPO fallback.
- Monoid: GT then StdS.
- Combinatory (combinator logic SKI / BCKW): Add weight + LPO +
  AutoPrecedence (variable-duplicating combinator rules need LPO).
- Sheffer (single-operator nand / Wolfram): MNF front search + Mix2
  weight + AutoMaxWeight 20.
- General: no front-load, falls straight into the portfolio tail.

For `>=8`-axiom inputs the schedule also appends a SInE-pruned variant
of Mix2 + a SInE-pruned `"GoalDirected"`, ordered last so a proof
reachable without pruning is never missed.

When to use: as the default. Override only when you already know the
right config (Sheffer / Wolfram single-operator and combinator goals are
the most common reasons).

### 4.2 `"Portfolio"`

The FIXED Waldmeister-style schedule (the prior behaviour, kept reachable
verbatim):

1. Completion with `"CriticalPairWeight" -> "Mix2"` -- the single best
   general weight.
2. Completion with `"Ordering" -> "LPO"` + `"AutoPrecedence" -> True` --
   structural / combinator reductions KBO cannot orient.
3. Completion with `"CriticalPairWeight" -> "Gt"` -- the engine's bare
   default; occasionally reaches a proof the others' CP order misses.
4. `"GoalDirected"` -- the MNF bidirectional front search, the only
   configuration that closes a symmetric goal whose two sides never meet
   at a single normal form.

When to use: when you want a strategy schedule but explicitly do NOT want
the problem-aware front-load.

### 4.3 `"Completion"`

Plain unfailing Knuth-Bendix completion. Default `CriticalPairWeight` is
the engine's `Gt`; default `Ordering` is `KBO`; no MNF front. The
workhorse config for an axiom set that admits a finite complete system
under some reduction ordering (groups, abelian groups, monoids, rings,
many Boolean axiomatizations once the right ordering is picked).

When to use: when you want a single explicit completion config (rather
than a portfolio) and the goal is reachable through saturation.

### 4.4 `"GoalDirected"` / `"MNF"`

Completion + the MNF bidirectional front search. Both spellings are
synonyms.

The front search expands the conjecture's two sides through the live
rule set on every selection step and watches for a collision. This is
the only configuration that closes a symmetric goal whose left- and
right-hand-side never share a single normal form -- the canonical cases
are Boolean `Noncontradiction` / `ExcludedMiddle` / `DoubleNegation` and
Sheffer `Commutativity`.

When to use: when a completion-only run saturates without proving but
you have reason to believe the goal IS a consequence of the axioms
(e.g. it is symmetric). Pay attention to wall budgets; the front search
re-expands its whole node table every time a rule is added.

### 4.5 `"Waldmeister"`

Faithful Waldmeister DEFAULT strategy preset for an unrecognized
single-operator (Sheffer / Wolfram nand) problem. Decoded from
Waldmeister's `Sinai.h` `StdS = kbo(std), itl(mi), zb(mnf)` into thvm
knobs:

- `CriticalPairWeight -> "Mix"` (default Heu_MixWeight).
- `Ordering -> "KBO"`, `AutoPrecedence -> True`.
- `SelectionRatio -> 51` (`itl(mi)` = interleave fifo:heuristic 1:50,
  Waldmeister CPdimension fairness).
- `RHSInterreduce -> True`.
- `UnfailingCP -> True`.
- `CPSetInterreduce -> True`.

List form takes the same suboptions, overriding any default. The
`"GoalDirected" -> True` switch (inside the list form) adds the MNF
front on top of the completion path for a symmetric goal that never
meets at one normal form.

When to use: Sheffer / Wolfram axiom systems and other single-operator
problems where Waldmeister's empirical default is well-tuned.

### 4.6 `"VampirePortfolio"`

A 10-entry rotation modeled on the portfolio-cycling shape Vampire
5.0.1 ships for UEQ: many short strategy slices rather than one tuned
config. With `TimeConstraint -> T`, each entry runs at `T / 10` wall
time and the first that proves wins (the engine already shares
`TimeConstraint` fairly across schedule entries).

The 10 entries exercise the full knob surface (CP weight modes,
orderings, redundancy criteria) shipped through iters 10-25:

1. `"VampireUEQ"` (the flag-complete preset).
2. Twee weight + GroundJoin + Connectedness + BackwardSubsume +
   BackwardDemod + RHSInterreduce + AutoMaxWeight.
3. RelLevel + SInE relevance filter.
4. ConjSym + GoalDirected MNF.
5. Diversity + UnfailingCP.
6. Mix2 + LRS + AutoMaxWeight.
7. Waldmeister preset.
8. LPO + AutoPrecedence + GoalInterleave 50.
9. GoalDirected + SInE.
10. Add weight + AutoMaxWeight.

When to use: experimental "throw everything at it" mode when the
default `Automatic` walls on a hard goal. The cost is that each slice
gets only ~10% of the wall budget; goals that need a long single
configuration won't benefit.

### 4.7 `"VampireUEQ"`

Preset modeled on the Vampire 5.0.1 UEQ-portfolio entry that cracks
`ShefferAxioms/AndAssociativity` in our parallel baseline:

```
dis+10_6_to=lpo:tgt=full:fde=none:sp=arity:nwc=1.2:bs=unit_only:
bd=all:av=off:gtg=exists_sym
```

Decoded to the thvm knobs we have (Vampire's `bd=all` backward
demodulation and `gtg=exists_sym` goal-type-graph premise selection
are not yet ported):

- `GoalDirected -> True` (Vampire `tgt=full`: MNF front alongside
  completion).
- `Ordering -> "LPO"` (`to=lpo`).
- `AutoPrecedence -> True` (`sp=arity`: our layered AutoPrecedence
  reduces to the arity ladder for single-operator Sheffer-shape
  problems; for multi-op problems it adds inverse / distributor
  structure on top -- a strict superset).
- `SelectionRatio -> 10` (`dis+10` = age:weight 1:10; thvm's
  `SelectionRatio` is the inverse FIFO ratio).
- `UnfailingCP -> True` (completeness over unorientable equations
  under LPO).
- `AutoMaxWeight -> True` (closest analog to Vampire's
  `nwc=1.2` non-goal weight skew).
- `BackwardSubsume -> True` (direct port of `bs=unit_only`: after
  adding a new rule, soft-delete any existing rule subsumed by it).
- `BackwardDemod -> True` (direct port of `bd=all` LHS half: after
  a new-rule batch, normalize each older rule's LHS with the new
  rule(s); on reduction, drop and re-queue the simplified equation).
- `RHSInterreduce -> True` (the `bd=all` RHS half: Waldmeister
  `IR_InterreduktionRechts`).

List form takes the same `Method -> {"VampireUEQ", subopt -> value, ...}`
override pattern as `"Waldmeister"`.

When to use: hard Sheffer / nand goals where the `"Waldmeister"` default
walls; experimentally a complement to the Waldmeister-tuned schedule.

### 4.8 `"Twee"`

Preset modeled on Twee 2.x's saturation defaults (Smallbone, 2021+):

- `CriticalPairWeight -> "Twee"` (the iter 11 port of Twee.CP.score:
  asymmetric large/small bias + shared-subterm dedup discount).
- `GroundJoin -> True` (Twee's `cfg_ground_join`: delete ground-
  joinable CPs).
- `Connectedness -> True` (Twee's `cfg_use_connectedness_standalone`:
  Bachmair-Dershowitz below-peak redundancy, Twee section 6.2).
- `UnfailingCP -> True` (Twee always superposes both faces of an
  unorientable equation).
- `BackwardSubsume -> True` + `BackwardDemod -> True` +
  `RHSInterreduce -> True` (Twee's `interreduce` keeps R reduced).
- `AutoMaxWeight -> 20` (Twee doesn't bound CP size explicitly; the
  growing-bound stash keeps the queue budget-tractable).

When to use: shared-subterm-heavy problems (e.g. nested Sheffer
expressions) where the dedup discount in Twee weight pays off, or any
goal where Twee's compute-graph stays tighter than the Vampire-style
flat saturation.

### 4.9 `"EProver"`

Preset modeled on E's typical CASC-mode UEQ classification (Schulz,
2002+).  E rotates dozens of strategies internally; this preset picks
the combination most often selected at E's auto-mode for UEQ:

- `CriticalPairWeight -> "ConjSym"` (iter 22 port of E's
  ConjectureSymbolWeight: conjecture-symbol nodes weight 1, others
  weight 4).
- `Ordering -> "KBO"` (E's default term ordering for UEQ).
  `AutoPrecedence` is intentionally OFF: thvm's Waldmeister-flavored
  precedence demotes AC operators in a way that stalls ConjSym on
  Boolean goals.
- `SelectionRatio -> 10` (E's `dis+10` age:weight 1:10).
- `AutoMaxWeight -> 20` (E manages CP queue size via PCL bounds;
  AutoMaxWeight is our analog).
- `BackwardSubsume -> True` + `RHSInterreduce -> True` (E's standard
  simplification sweep).
- `UnfailingCP -> True` (E's unfailing completion mode is on by
  default for UEQ).

When to use: cross-system goals (Implies-X family) where ConjectureSymbol
weighting biases the search toward conjecture-relevant CPs.

### 4.10 `"VampirePortfolioCompact"`

A 3-entry rotation sized for small `TimeConstraint`s where the
10-entry `"VampirePortfolio"` would give each slice <1s:

1. `"VampireUEQ"` (Vampire's flagship UEQ entry).
2. `"Twee"` (Twee's redundancy + dedup-aware weight).
3. `Completion + Mix2 + AutoPrecedence + AutoMaxWeight 20`.

At `TC=5` each entry gets ~1.67s; at `TC=15` ~5s.

When to use: when budget is too small for the full 10-entry
`"VampirePortfolio"` to give any slice useful time.

### 4.11 `"AllPresets"`

A 4-entry rotation through every named single-config preset:
`{"Waldmeister", "VampireUEQ", "Twee", "EProver"}`.

When to use: as a portfolio when the autotuner's structure guess
might be wrong and you want every approach tried with a fair share
of the budget.

## 5. Suboptions catalog

Every entry in the table below is parsed by `atpParseCompletionOpts` /
the dedicated `atpXxxOpt` helpers and propagated through `cEngineProof`
to the C engine (`thvm_atp_set_*`), except where noted. Defaults are the
engine defaults (which `Automatic` may override per detected structure).

### 5.1 `"CriticalPairWeight" -> ...`

Waldmeister `ClasHeuristics` critical-pair selection weight (which
pending CP to process next).

| Value | Engine code | Notes |
|-------|-------------|-------|
| `"Add"` | `CH_AddWeight` | Bare `wl + wr` symbol-count sum. |
| `"Max"` | `CH_MaxWeight` | Larger of the two faces. |
| `"Ord"` | `CH_OrdWeight` | Ordering-based weight. |
| `"Gt"` | `CH_GtWeight` | Ordering-directed; engine default. |
| `"Mix"` | `CH_MixWeight` | Default in Waldmeister Heu_MixWeight. |
| `"Mix2"` | `CH_MixWeight2` | `g*10 + (wl+wr)`; best general weight on the harder Boolean / cross-axiom theorems. |
| `"Unif"` | `CH_Unifikationsmass` | Unifier-measure weight. |
| `"Goal"` / `"CPinGoal"` | goal-directed | Every selection picks the CP closest to the goal. |
| `"Twee"` | `ATP_CP_WEIGHT_TWEE` | Twee.CP.score (Smallbone) -- asymmetric: `4*size(larger) + 1*size(smaller) + 2*depth`. Biases toward CPs whose smaller (reduct) side is small, regardless of the peak's size. Strong on Sheffer / nand single-operator saturations where `Mix2` walls. Ported from `src/Twee/CP.hs` line 240. |
| `"ConjSym"` | `ATP_CP_WEIGHT_CONJSYM` | E `ConjectureSymbolWeight` port (`HEURISTICS/che_funweights.c`): walks both sides, conjecture-symbol CTR nodes weight 1, off-symbol CTR nodes weight 4, variable nodes weight 1. A cheap symbol-set biasing toward goal-relevant CPs -- a poor man's `"Goal"` mode that does not need structural matching. |
| `"Diversity"` | `ATP_CP_WEIGHT_DIVERSITY` | E `DiversityWeight` port (`HEURISTICS/che_diversityweight.c`): `base + #distinct CTR labels + #distinct FVR ids`. Penalizes CPs whose sides drag in many unrelated symbols / variables -- favors structurally compact CPs. Linear shape (E's `fdiff1=1, fdiff2=0, vdiff1=1, vdiff2=0`). |
| `"RelLevel"` | `ATP_CP_WEIGHT_RELLEVEL` | E `RelevanceLevelWeight` port (`HEURISTICS/che_funweights.c`): N-level scoring. Each symbol gets its BFS distance from the conjecture through the "co-occurs-in-an-axiom" relation (capped at `ATP_REL_LEVEL_MAX = 8`); a CTR node's weight is `1 + sym_level[label]`. Remote symbols (unreachable) collapse to the max penalty. Variable nodes weight 1. Deeper goal-relevance bias than `"ConjSym"` (which is the 1-level analog). |
| `"Staggered"` | `ATP_CP_WEIGHT_STAGGERED` | E `StaggeredWeight` port (`HEURISTICS/che_varweights.c`): `base_weight / max(1, max_axiom_weight / 2)`. Coarse bucketing -- within a bucket, the heap-min tie breaks by insertion order (pair with `"FifoTiebreak" -> True` for the intended behavior). Useful when many CPs are bunched near the same weight and you want age-fair processing inside each bucket. |
| `"Learned"` | `ATP_CP_WEIGHT_LEARNED` | ENIGMA-style learned scorer over CP features. Requires a trained model file. |
| `Automatic` | -1 | Falls back to the engine default (`Gt`). |

Default: engine default (`Gt`). `Automatic` for the top-level Method
front-loads `Mix2` for many goals.

When to set: when the engine default's CP order gets stuck. `Mix2` is
the most common override; `Add` for combinator-style goals; `Goal` for
short directly-cited proofs; `Twee` for hard single-operator
saturations; `ConjSym` for many-axiom problems where the bulk of CPs
drift away from the conjecture's symbol set.

```wolfram
TFindProof[goal, axioms,
    Method -> {"Completion", "CriticalPairWeight" -> "Mix2"}]
```

### 5.2 `"Ordering" -> "KBO" | "LPO"`

Reduction ordering used to orient equations. KBO (Knuth-Bendix Ordering)
is the default and the natural choice for most algebraic structures
(groups, rings, lattices). LPO (Lexicographic Path Ordering) is needed
for variable-duplicating rules (S/W combinators, some boolean
reductions) that KBO cannot orient.

Default: `"KBO"`. When to set: LPO for combinator logic; combine with
`"AutoPrecedence" -> True` for any LPO run.

### 5.3 `"AutoPrecedence" -> True | False`

Use Waldmeister's structure-driven precedence (the PhilMarlow port). For
groups this puts the inverse on top; for rings the distributor `*` above
`+`; etc. Without it the precedence is the identity (label order).

Default: `False`. When to set: any time you select LPO; for KBO on
groups / rings / lattices to orient unary inverses cleanly.

### 5.4 `"AxiomRelevance" -> ...`

The axiom-relevance filter, configurable via Method suboption (back-compat
alias: `"DropDivergentAxioms"`):

| Value | Behaviour | Soundness |
|-------|-----------|-----------|
| `None` / `All` / `False` | Keep every axiom. | Trivially. |
| `Automatic` / `"Safe"` | Drop only axioms with a private symbol on BOTH sides (e.g. the Y combinator when the goal is Y-free). DEFAULT. | Sound AND completeness-preserving. |
| `"Connected"` / `{"Connected", "FrequencyCutoff" -> f, "MaxGenerations" -> n}` | Symbol-reachability pruning (a coarse heuristic). | Heuristic: may drop a needed axiom. |
| `"SInE"` / `{"SInE", "SineTolerance" -> st, "SineDepth" -> sd, "SineGenerality" -> sgt}` | Vampire-faithful SInE (Sumo-Inspired premise selection, Hoder & Voronkov IJCAR 2011). Defaults `3 / 2 / 8` mirror Vampire's `--sine_tolerance / --sine_depth / --sine_generality_threshold`. | Heuristic: may drop a needed axiom. |

Inspect the partition without proving via `TRelevantAxioms`:

```wolfram
TRelevantAxioms["ImpliesWolframAxioms", "MeredithAxioms",
    Method -> {"GoalDirected", "AxiomRelevance" -> "SInE"}]
(* -> <|"Mode" -> "SInE", "Kept" -> {...}, "Dropped" -> {...}|> *)
```

Default: `"Safe"`. When to set `"SInE"`: many-axiom cross-system goals
(the canonical case is `Implies*Axioms` against a multi-system axiom
table).

### 5.5 `"MaxWeight" -> n`

Drop critical pairs whose combined term weight exceeds `n` symbols.
`0` / `Automatic` = unbounded.

Default: 0 (unbounded). When to set: a very large CP queue that wastes
time on giant pairs; pair with `"GoalInterleave"` so the goal-directed
selections still fire.

### 5.6 `"GoalInterleave" -> n`

Every n-th CP selection is a goal-directed (`CPinGoal`) pick; the rest
use the chosen weight. `0` / `Automatic` = off.

Default: 0 (off). `Automatic` for groups / lattices / monoids uses
`GoalInterleave 50` per Waldmeister's `itl(mi)`.

### 5.7 `"GroundJoin" -> True`

Delete ground-joinable critical pairs (every ground instance of the CP
joins, so it is a redundancy). Sound (Martin-Nipkow / Twee criterion).

Default: `False`. When to set: lattice / Boolean axiomatizations where
the CP queue is dominated by ground-joinable redundancies.

### 5.8 `"Connectedness" -> True`

Bachmair-Dershowitz connectedness CP deletion (Twee section 6.2). Drop
a critical pair whose two sides join through intermediate terms strictly
below the peak in the reduction order. A sound generation-cut redundancy
criterion stronger than trivial joinability.

Default: `False`. When to set: alongside `"GroundJoin"` for axiom sets
with many redundant CPs at the same generation depth.

### 5.9 `"SelectionRatio" -> n`

Waldmeister CPdimension fairness: 1 FIFO (oldest-CP) pick per `n` CP
selections, the rest by weight. The fairness lever against
smallest-weight starvation.

Default: 0 (engine default 11). Waldmeister also uses 50 / 100 / 200;
the `"Waldmeister"` preset sets 51.

### 5.10 `"AutoMaxWeight" -> b`

A growing CP-weight bound (`b + 2 * deepest-rule-weight`) that defers
over-weight critical pairs to a stash and force-drains them when the
active queue empties. Keeps the CP queue small (measured ~3.5x smaller
on the hard Sheffer goals) WITHOUT losing completeness (nothing is
permanently dropped).

Default: 0 (off). When to set: `Automatic` for Sheffer uses `20`; try
`10`-`30` on a goal whose CP queue blows up.

### 5.11 `"RHSInterreduce" -> True`

Waldmeister `IR_InterreduktionRechts`: after a rule is oriented,
normalize the RHS of every other rule against it, re-queuing any rule
whose RHS shrinks. Keeps `R` fully reduced so the CP set stays small.

Default: `False`. `Automatic` for `"Waldmeister"` turns it on. When to
set: deep theorem proofs where the rule set otherwise diverges.

### 5.12 `"UnfailingCP" -> True`

Superpose BOTH faces of every unorientable equation (the unfailing
completion completeness requirement). With the default the engine
overlaps the stored lhs only.

Default: `False`. The `"Waldmeister"` preset turns it on. When to set:
any goal whose proof requires reasoning through an unorientable equation
(commutativity-driven Boolean goals).

### 5.13 `"CPSetInterreduce" -> True`

Waldmeister `KPV_KPMengeInterreduzieren`: periodically re-normalize the
whole CP queue against the full rule set, deleting CPs that became
joinable and reweighting the rest, so the heap-min selection tracks
live, irreducible CPs.

Default: `False`. The `"Waldmeister"` preset turns it on; the Sheffer
`"AndAssociativity"` proof needs it.

### 5.14 `"Precedence" -> {sym1, sym2, ...}`

Explicit reduction-ordering precedence (symbol names highest-to-lowest),
mirroring Waldmeister's `p > q > nand` ORDERING block. Resolved against
the engine's symbol labels and applied to both LPO and KBO.

Default: `Automatic` (the chosen identity or `AutoPrecedence`). When to
set: when you know the right precedence from the algebra (e.g. Sheffer
goal-skolem-constants above the binary operator).

### 5.15 `"SkolemHighest" -> True`

Rank the goal's ground / skolemized constants above every operator (the
structural rule Waldmeister's `p > q > nand` precedence encodes). Takes
effect only when supplied; leaves the default precedence byte-identical
otherwise.

Default: `Automatic` (off). When to set: any single-operator Sheffer /
Wolfram goal where the conjecture's skolem constants should orient.

### 5.16 `"FifoTiebreak" -> True`

Waldmeister `-:w1=fifo` secondary CP key: preserve each surviving
critical pair's insertion age across the post-orient CP-normalize sweep,
so equal-weight ties resolve oldest-first run-wide. Off by default,
engine byte-identical when unset.

Default: `False`. When to set: a run where the weight distribution has
many ties and the order of FIFO-tied CPs measurably matters.

### 5.17 `"RecordNorm" -> True | False`

Per-step normalize-trace recording for the ProofObject builder. Default
`True` is the historical path -- WL walks `CP -> NORM_STEP* -> ORIENT`
linearly to reconstruct the chain. `False` routes search through the
fast indexed / flatterm normalize so a long completion saturates at the
C-bench rate; WL then reconstructs the chain through the emitNorm BFS
over the `CP / ORIENT / SIMPLIFY` trace DAG.

Default: `True`. When to set `False`: long-running completions where the
per-step recording overhead dominates and you can pay the BFS
reconstruction cost.

### 5.18 `"ForwardSubsume" -> True`

When adding a new rule `l' = r'` to R, scan existing rules; if some
existing rule `l = r` subsumes the new one (`\E sigma`, `l*sigma = l'`
and `r*sigma = r'`, or the cross-orientation since equations are
unoriented), skip the add. Sound + completeness-preserving: the new
equation is a substitution instance of the existing rule, so any
rewrite step it could fire is already reachable from the more general
rule. Vampire `--forward_subsumption` analog, unit-only (every UEQ
equation is a unit clause).

Default: `False`. Pair with `"BackwardSubsume" -> True` for the full
subsumption pruning that classical saturation provers run by default.

### 5.19 `"BackwardSubsume" -> True`

After adding a new rule `l = r` to R, scan existing rules; for each
existing rule that the new one subsumes, soft-delete it. The
soft-delete writes an out-of-range FVR sentinel (id=255, beyond
`REWRITE_MAX_VAR=64`) into the slot's lhs / rhs so `thvm_match` and
`thvm_unify` return 0 naturally on every rule-firing site without
threading an explicit `r_dead[]` check through 14 ATP rule-iteration
loops. The original (lhs, rhs) is preserved in a parallel save array
(GC-rooted) so proof reconstruction can read the killed rule's content
when the trace cites it. Sound + completeness-preserving by the same
argument as `"ForwardSubsume"`. Vampire's `bs=unit_only` direct port.

Default: `False`. The `"VampireUEQ"` preset turns it on.

### 5.20 `"BackwardDemod" -> True`

After each newly-added rule batch, normalize each older rule's LHS
against the new rule(s); if it reduces, drop the rule and re-queue the
simplified equation `(reduced_lhs, old_rhs)`. The companion
`"RHSInterreduce"` option performs the RHS half. Pairing both gives
the full `bd=all` analog from Vampire's UEQ flag block.

Sound + completeness-preserving: the rewritten equation is a logical
consequence of the original rule plus the new rule, so any rewrite step
reachable from the old rule remains reachable from the rewritten
equation. The new rule itself stays in R.

Default: `False`. The `"VampireUEQ"` preset turns it on alongside
`"RHSInterreduce" -> True`.

### 5.22 `"VarWeight" -> n`

Per-variable KBO weight override. Default is `1` (every TAG_FVR node
contributes weight `1` to the KBO sum). Mirrors Waldmeister's
`-w VAR=N` flag. Useful when a per-symbol weight scheme via
`"SymbolWeights"` shifts the balance so variables need their own
counter-weight, or when reducing variable weight to favor more-
heavily-quantified clauses.

Pass any positive integer to override; a non-positive value (or
absent) keeps the default `1` (engine byte-identical).

### 5.21 `"SymbolWeights" -> {sym -> w, ...}`

Per-symbol KBO weight overrides. Default per-symbol weight is `1` (with
variable weight `1` too). An association or list of rules
`{sym -> w, ...}` sets `weight(sym) = w` for each named symbol; unlisted
symbols keep the default `1`. Sentinel `0` in the input means "leave
at default" so a partial map is fine.

Waldmeister `SymbolGewichte` port (`CLAS/SymbolGewichte.c::SG_Symb-
GewichteEintragen`, the `-w DEF=2:VAR=5:f=5:g=0` flag). Useful for
hand-tuned problem-specific KBO configurations: making a "heavy" symbol
(e.g. one whose terms tend to blow up) more expensive lets the engine
preferentially orient away from it.

Default: identity (uniform 1, engine byte-identical).

## 6. Per-class recommendations

Empirical patterns from the recent benchmark sweep + parallel Vampire
study. These are the defaults `Automatic` already front-loads; the same
table is useful when you pin `Method` explicitly.

| Class | First-choice config | Rationale |
|-------|---------------------|-----------|
| Boolean symmetric (`DoubleNegation`, `ExcludedMiddle`, `Noncontradiction`) | `"GoalDirected"` | Two sides never share an NF; MNF closes via front collision. (e.g. `BooleanAxioms::DoubleNegation` proves in seconds via GoalDirected.) |
| Combinator SKI / BCKW | `{"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True, "CriticalPairWeight" -> "Add"}` | Variable-duplicating S/W rules need LPO; Add weight matches Waldmeister's KombS. |
| Sheffer / Wolfram single-operator deep (e.g. `AndAssociativity`) | `{"Waldmeister", "CriticalPairWeight" -> "Gt", "CPSetInterreduce" -> True}` | Waldmeister default + CP-set interreduction reaches the deep proofs. |
| Cross-system many-axiom (e.g. `Implies*Axioms` against `MeredithAxioms`) | `{"GoalDirected", "AxiomRelevance" -> "SInE"}` (or pair with `"CriticalPairWeight" -> "RelLevel"` for a deeper goal-relevance bias on CP selection in addition to axiom pruning) | SInE prunes the irrelevant cross-system axioms; MNF closes the remaining goal. (BooleanAxioms::DeMorgan proves in 3s via Automatic which front-loads GoalDirected on this class.) |
| Group / AbelianGroup | `{"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50, "AutoPrecedence" -> True}` | Waldmeister `GtS`; AutoPrecedence puts inverse on top. |
| Ring | `{"Completion", "Ordering" -> "KBO", "AutoPrecedence" -> True}` | Waldmeister `kbo(Std)`; structure-precedence puts `*` above `+`. |
| AC (commutative+associative, no inverse) | `{"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50}` | Waldmeister `GtS` family. |
| Lattice | `{"Completion", "CriticalPairWeight" -> "Gt", "GroundJoin" -> True, "GoalInterleave" -> 50}` then LPO fallback | Waldmeister `Verband` row uses `gj()` + an LPO pass. |

### 6.1 Currently out of reach

These 5 unit-equality goals are known to be cracked by Vampire 5.0.1 (UEQ
portfolio) within 30 s in our parallel baseline, but neither the
`Automatic` schedule nor any tried explicit `Method` -- including the
faithful `"VampireUEQ"` preset (after iters 18-21 it bundles real
`BackwardSubsume` + `BackwardDemod` + `RHSInterreduce`, every orderable
flag from Vampire's winning block for Sheffer/AndAssociativity),
`"CriticalPairWeight" -> "ConjSym"`, `"Diversity"`, `"Twee"`,
`"ForwardSubsume" -> True`, and pairings with `"GroundJoin" -> True` or
`"Connectedness" -> True` -- closes them within 30 s on the single-config
form:

- `McCuneAxioms / EqualityOfInverses`
- `RobbinsAxioms / DoubleNegation`
- `ShefferAxioms / AndAssociativity`
- `ShefferAxioms / ImpliesWolframAxioms`
- `ShefferAxioms / ImpliesWolframAlternateAxioms`

Vampire's winning flag block on `Sheffer/AndAssociativity` is

```
dis+10_6_to=lpo:tgt=full:fde=none:sp=arity:nwc=1.2:bs=unit_only:
bd=all:av=off:gtg=exists_sym
```

The orderable subset is shipped as `Method -> "VampireUEQ"`. After iters
18 (BackwardSubsume) and 20 (BackwardDemod) every orderable flag in
Vampire's winning Sheffer/AndAssociativity block is now ported:

| Vampire flag | thvm equivalent |
|-|-|
| `to=lpo` | `Ordering -> "LPO"` |
| `sp=arity` | `AutoPrecedence -> True` (strict superset for multi-op) |
| `dis+10` | `SelectionRatio -> 10` |
| `nwc=1.2` | `AutoMaxWeight -> True` |
| `tgt=full` | `GoalDirected -> True` (MNF front alongside completion) |
| `bs=unit_only` | `BackwardSubsume -> True` |
| `bd=all` | `BackwardDemod -> True` + `RHSInterreduce -> True` |
| `fde=none` | (not exposed; thvm's forward demodulation is built into normalize and not toggleable) |
| `av=off` | (n/a; thvm has no AVATAR splitting to begin with) |
| `gtg=exists_sym` | (not ported; Vampire's GoalGuessing flags clauses with conjecture-symbol occurrences as goal-like for premise selection.  Less directly applicable to thvm: every problem here has an explicit `negated_conjecture` role.) |

That the faithful preset still does not close these 5 targets within
30 s suggests the remaining gap is in the search-shape variables not
visible at the flag layer -- Vampire's wider portfolio runs many
100 ms strategies until one of them happens to surface the right
critical pair early, which is a different lever than any single-config
tuning can replicate.

Iter 26 added `Method -> "VampirePortfolio"` as a 10-entry rotation
that approximates Vampire's portfolio-cycling shape (see §3.6). Iter
27 ran it at `TimeConstraint -> 60` (~6 s per slice) on
`Sheffer/AndAssociativity` -- still does not close. Confirms the
practical reading: cracking these 5 targets at single-machine clock
times comparable to Vampire's UEQ-portfolio wall budget needs either
many minutes of cycling (Vampire spends 30 s -- 5 minutes in
practice) or a deeper search-space restructuring not in the flag /
schedule layer.

## 7. Return specs

`TFindProof` takes an optional LAST positional argument selecting what
to return. A single String returns that one value bare; a list of
Strings returns an `Association` keyed by the requested names; `All`
returns an `Association` of every spec. The default (`"ProofObject"`)
returns the bare `ProofObject`, so existing call shapes are unchanged.

| Spec | Value |
|------|-------|
| `"ProofObject"` | The verifying WL `ProofObject` (default). |
| `"Lemmas"` | The completed rule set rendered as `Inactive[Equal]` equations. |
| `"PreprocessedAxioms"` | The normalized axioms fed to the C engine. |
| `"RelevantAxioms"` | The `TRelevantAxioms` `<|"Mode", "Kept", "Dropped"|>` partition. |
| `"RawTrace"` | The decoded C-engine completion trace (CP / ORIENT / SIMPLIFY entries). |
| `"Statistics"` | A small run-stats `Association` (`Status`, `Steps`, `Rules`, `Trace`, `QueueSize`). |
| `"Status"` | A `"Proved"` / `"Saturated"` / `"TimedOut"` / `"Failed"` tag. |
| `"AppliedMethod"` | The actual `Method` config that produced the bundle. For a portfolio run, the winning schedule entry; for a single-config run, the only entry tried. Useful for inspecting what `Automatic` chose. |
| `"WallTime"` | Seconds (`AbsoluteTiming`) the C-engine `cEngineProof` call took for the SINGLE config that produced the bundle. For a portfolio run this is the WINNING config's slice only -- earlier non-proving slices are not summed in. |
| `"PortfolioTrace"` | List of `<|"Method", "WallTime", "Proved"|>` records, one per schedule entry the portfolio dispatcher tried in order. Last entry is the winning slice; earlier entries are non-proving slices and useful for portfolio-budget debugging. For a single-config call, falls back to a 1-element list mirroring `"AppliedMethod"` / `"WallTime"`. |
| `All` | An `Association` of every spec above. |

Examples:

```wolfram
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "Lemmas"]
TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    {"Statistics", "ProofObject"}]
TFindProof["InverseOfInverse", "AbelianGroupAxioms", All]
```

The single-argument completion form `TFindProof[axioms]` (no
conjecture) defaults to `"Lemmas"` instead of `"ProofObject"`, since
there is no goal:

```wolfram
TFindProof[AxiomaticTheory["AbelianGroupAxioms"],
    TimeConstraint -> 10]   (* -> completed lemmas *)
```

Bound completion with `TimeConstraint` / `TimeConstraint`, since a
non-terminating axiom set never saturates.

## 8. Debugging and introspection

### 8.1 Statistics

```wolfram
TFindProof[goal, axioms, "Statistics"]
(* -> <|"Status" -> "Proved", "Steps" -> 314,
        "Rules" -> 27, "Trace" -> 4096, "QueueSize" -> 0|> *)
```

Fast first sanity check: a "Proved" with `Steps` in the hundreds and
`QueueSize` near 0 is a clean saturation; a "TimedOut" with `QueueSize`
in the millions is CP explosion.

### 8.2 `"PreprocessedAxioms"`

```wolfram
TFindProof[goal, axioms, "PreprocessedAxioms"]
```

Returns the axioms after `ForAll -> Pattern` quantifier elimination,
`Exists -> Skolem`, and canonical pattern-variable naming. The exact
shape the C engine encodes.

### 8.3 `"RelevantAxioms"` and `TRelevantAxioms`

```wolfram
TRelevantAxioms["InverseOfInverse", "AbelianGroupAxioms",
    Method -> {"Completion", "AxiomRelevance" -> "SInE"}]
```

Inspect what the relevance filter kept vs. dropped, BEFORE the prove.
The Mode tag identifies the filter (`None`, `"Safe"`, `"Connected"`,
`"SInE"`); each entry of `Dropped` carries the witnessing axiom and the
symbols that triggered the drop reason.

### 8.4 Environment variables

| Env var | Effect |
|---------|--------|
| `THVM_HEAP_CELLS` | Override the IC heap allowance. Useful when a deep saturation needs more node memory than the default. |
| `THVM_ATP_TRACE_MAX` | Raise the trace-entry cap. The default is fine for almost all proofs; raise it for completion runs that exceed the default and need full trace reconstruction. |
| `THVM_ATP_RULE_TRACE` | Set to `1` for a per-rule derivation trace to stderr. Verbose; pin to a single problem at a time. |

All env vars are read once per process at first call.

### 8.5 Tracing a specific Method config

The `RawTrace` return spec is the raw decoded C-engine trace -- one
record per CP / ORIENT / SIMPLIFY event:

```wolfram
TFindProof[goal, axioms, "RawTrace", TimeConstraint -> 5]
(* -> {<|"Reason" -> 3, "ParentA" -> 5, ...|>, ...} *)
```

Pair with `"Lemmas"` to read the final rule set, or with `"Statistics"`
to see how much of the trace cap was used.

### 8.6 Scripting footgun: short iterator names

When calling `TFindProof` inside a script-level `Do` / `Module` /
`Table`, **do not use short iterator names** like `c`, `theory`, `thm`,
`m`, `x`, `f` for the iteration variable. These collide with internal
pattern variables or iterator scopes inside the prover and the
dispatch path SIGSEGV-crashes the wolframscript kernel before
producing useful output.

Example that crashes:

```wolfram
Do[
    TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        TimeConstraint -> 5],
    {c, {"InverseOfInverse"}}]   (* iter var `c` *)
```

Example that works:

```wolfram
Do[
    TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        TimeConstraint -> 5],
    {benchIter, {"InverseOfInverse"}}]   (* iter var `benchIter` *)
```

The same applies inside `Module[{thm = ..., theory = ...}, ...]` and
`Table[..., {m, ...}]`. Prefix with `bench`, `loop`, or any longer
name to be safe. `tools/baselines/bench_methods.wls` demonstrates the
workaround.

### 8.7 Catching a wedge

A goal that hangs longer than expected almost always either explodes the
CP queue (visible via `"Statistics"["QueueSize"]` on a `TimeConstraint`
timeout) or runs the MNF front search on a divergent rule set (visible
in the same way; `Method -> "Completion"` skips MNF). When the queue
explodes, the first lever to pull is `"AutoMaxWeight" -> 20` (defers
over-weight CPs); if that does not help, switch the weight (`Mix2 ->
Mix -> Gt`) and bound `MaxWeight` directly.

### 8.8 Which config actually ran -- the introspection trio

When `Method -> Automatic` or `Method -> "VampirePortfolio"` cycles
through multiple schedule entries, three return specs disclose what
happened:

```wolfram
TFindProof[goal, axioms, "AppliedMethod"]
(* -> {"Completion", "CriticalPairWeight" -> "Mix2"} *)
```

`"AppliedMethod"` (§7) returns the winning schedule entry -- the
config the bundle actually ran under. For a single-config call this
is just the `Method` argument; for portfolio runs it's the entry that
proved.

```wolfram
TFindProof[goal, axioms, "WallTime"]
(* -> 0.0014 *)
```

`"WallTime"` returns the `AbsoluteTiming` seconds the C-engine
`cEngineProof` call took for that single (winning, in portfolio
runs) config. The encode / dispatch overhead is NOT included; this
is the C side only.

```wolfram
TFindProof[goal, axioms, "PortfolioTrace"]
(* -> {
    <|"Method" -> {"Completion", "CriticalPairWeight" -> "Mix2"},
      "WallTime" -> 3.22, "Proved" -> False|>,
    <|"Method" -> {"Completion", "Ordering" -> "LPO", ...},
      "WallTime" -> 3.00, "Proved" -> False|>,
    ...
    <|"Method" -> "GoalDirected", "WallTime" -> 1.20,
      "Proved" -> True|>
  } *)
```

`"PortfolioTrace"` returns the full per-slice record: every schedule
entry the dispatcher tried in order, the slice's `cEngineProof`
WallTime, and whether the slice produced a verifying ProofObject.
The last entry has `Proved -> True` (the winner) when the goal was
closed; for a single-config call the list has one entry mirroring
`"AppliedMethod"` / `"WallTime"`.

Useful when a goal "should" be cheap but takes a long time: look at
which slices ran and how long each took, then either pin the cheap
config explicitly with `Method -> ...` or feed the slow ones a tighter
`"AutoMaxWeight"`.

### 8.9 What does my Method actually mean? -- `TAtpSchedule` + `TAtpDescribeMethod`

Two helpers expose what a `Method` spec resolves to without paying for
the C engine:

```wolfram
TAtpSchedule[Method]
(* -> the schedule (list of single-config Methods) the dispatcher
      would expand to.  Schedule presets ("Portfolio", "VampirePortfolio",
      "AllPresets", ...) return their full rotation; single-config
      Methods return a 1-element list. *)

TAtpSchedule[Method, conjecture, axioms]
(* For Automatic, threads the conj + ax through the structure-
   recognized auto-tuner so the returned schedule matches what
   TFindProof[conj, ax, Method -> spec] would dispatch. *)

TAtpSchedule[Method, "Theorem", "Theory"]
(* AxiomaticTheory-resolved variant. *)
```

```wolfram
TAtpDescribeMethod["Twee"]
(* -> <|"CriticalPairWeight" -> "Twee", "GroundJoin" -> True,
        "Connectedness" -> True, "UnfailingCP" -> True,
        "BackwardSubsume" -> True, "BackwardDemod" -> True,
        "RHSInterreduce" -> True, "AutoMaxWeight" -> 20|> *)

TAtpDescribeMethod[{"Twee", "AutoMaxWeight" -> 0}]
(* -> defaults merged with the user's overrides. *)

TAtpDescribeMethod["VampirePortfolioCompact"]
(* -> <|"Schedule" -> {"VampireUEQ", "Twee",
        {"Completion", "CriticalPairWeight" -> "Mix2",
            "AutoPrecedence" -> True, "AutoMaxWeight" -> 20}}|> *)
```

`$AtpMethodPresets` enumerates the named presets the dispatcher
recognizes:

```wolfram
THVMLink`ATP`Private`$AtpMethodPresets
(* -> {"Waldmeister", "VampireUEQ", "Twee", "EProver",
       "Portfolio", "VampirePortfolio", "VampirePortfolioCompact",
       "AllPresets"} *)
```

### 8.10 `PortfolioFrontLoad -> n`

Multi-entry schedules (Automatic / Portfolio / VampirePortfolio /
AllPresets) divide `TimeConstraint` fairly across all remaining entries.
At small total budgets each entry gets a sliver -- e.g. VampirePortfolio
at `TC=5` gives each of 10 configs only 0.5s.

`PortfolioFrontLoad -> n` widens the slice for the first `n` entries:
each gets 2x the share that a fair recurrence would assign them;
entries past `n` revert to fair share.  Default `0` reproduces fair
sharing exactly.

```wolfram
TFindProof[goal, axioms, Method -> "AllPresets",
    TimeConstraint -> 20, PortfolioFrontLoad -> 2]
(* The first two presets (Waldmeister, VampireUEQ) each get ~6.7s
   instead of the fair 5s; EProver + Twee split the remaining ~6.6s. *)
```

When `Automatic`'s structure-detector front-loads a config it's
confident about (e.g. Gt + AutoPrecedence for Group), pair it with
`PortfolioFrontLoad -> 1` or `-> 2` to actually give those tuned
entries the time they need.

## 9. Recipes

Combinations of the suboptions in §5 that pair well, organized by what
you're trying to attack.

### 9.1 Throw everything at it

```wolfram
TFindProof[goal, axioms, Method -> "VampirePortfolio",
    TimeConstraint -> 60]
```

10-entry portfolio rotation (§3.6). Each slice gets `TimeConstraint /
10` wall time. Best when you don't know what shape the goal has and
want to spend a fixed budget exploring.

### 9.2 Maximum CP-queue pruning

```wolfram
TFindProof[goal, axioms, Method -> {"Completion",
    "BackwardSubsume" -> True, "BackwardDemod" -> True,
    "RHSInterreduce" -> True, "GroundJoin" -> True,
    "Connectedness" -> True, "AutoMaxWeight" -> 20}]
```

Every sound redundancy criterion the engine has, on a single completion
run. Tightest CP queue at the cost of more per-step interreduction
work. Good when the CP-explosion pathway is the suspected wedge (see
§7.6) and you have a long wall budget to amortize the bookkeeping.

### 9.3 Goal-relevance bias

```wolfram
TFindProof[goal, axioms, Method -> {"Completion",
    "CriticalPairWeight" -> "RelLevel",
    "AxiomRelevance" -> "SInE"}]
```

CP weight scales with BFS distance from the conjecture symbol set
(§4.1 `"RelLevel"`); SInE prunes axioms unreachable from the goal
(§4.4). Good for cross-system many-axiom theorems (e.g.
`Implies*Axioms` against a foreign axiom system) where the bulk of
the search wanders far from the conjecture.

### 9.4 Cheap relevance bias (no SInE filter)

```wolfram
TFindProof[goal, axioms, Method -> {"Completion",
    "CriticalPairWeight" -> "ConjSym"}]
```

A poor man's `"RelLevel"`: just the symbol-set bias, no axiom pruning.
1-level vs. N-level (§4.1 `"ConjSym"` vs. `"RelLevel"`). Cheap, and
sufficient when the conjecture's symbol set already covers most of the
relevant axioms.

### 9.5 Sheffer / Wolfram nand single-operator

```wolfram
TFindProof[goal, axioms, Method -> {"Waldmeister",
    "CriticalPairWeight" -> "Gt", "CPSetInterreduce" -> True}]
```

The faithful Waldmeister default (§3.5) plus CP-set interreduction.
Best baseline for deep single-operator saturations. The `"Sheffer"`
class in §5 lists more specialized variants that `Automatic`
front-loads on this shape.

### 9.6 Variable-duplicating combinator (S, W, M)

```wolfram
TFindProof[goal, axioms, Method -> {"Completion",
    "Ordering" -> "LPO", "AutoPrecedence" -> True,
    "CriticalPairWeight" -> "Add"}]
```

KBO cannot orient variable-duplicating rules; LPO + the auto-precedence
puts the relevant heads on top. Bare `"Add"` CP weight matches
Waldmeister's `KombS` heuristic for combinator logic.

### 9.7 Stagger-bucketed age fairness

```wolfram
TFindProof[goal, axioms, Method -> {"Completion",
    "CriticalPairWeight" -> "Staggered",
    "FifoTiebreak" -> True}]
```

`"Staggered"` coarse-grains CP weights into buckets of size
`max_axiom_weight / 2`; within each bucket `"FifoTiebreak"` picks the
oldest CP first. Use when many CPs share near-identical raw weights
and the implementation's arbitrary heap order is starving the older
half. The combination mirrors E's `StaggeredWeight` heuristic family.

### 9.8 Cross-system Sheffer "implies" goals (tight age bias)

```wolfram
TFindProof[goal, "ShefferAxioms", Method -> {"Completion",
    "CriticalPairWeight" -> "Mix2",
    "SelectionRatio" -> 2, "AutoMaxWeight" -> 20}]
```

For the cross-system "Sheffer stroke implies Wolfram's nand axioms"
goals (`ImpliesWolframAxioms`, `ImpliesWolframAlternateAxioms`), the
default `SelectionRatio` (11) leaves the proof unreachable in a
reasonable budget. `SelectionRatio -> 2` -- a 1-FIFO-pick-per-2-
selections age bias -- forces the saturator through the long
cross-system derivation chain before the CP queue explodes, cracking
both in ~4s. The sweet spot is sharp: `SelectionRatio -> 1` (pure
FIFO) and `>= 5` both miss it. `Automatic` front-loads this config
on Sheffer goals, so you usually get it for free; pin it explicitly
when you want the single fast config without the portfolio's
budget split.