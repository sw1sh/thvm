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

## 2. Quick start

Each example is intentionally short; expand the `Method` per section 4
for the cases where the default leaves a goal open.

### 2.1 A group theorem (default Automatic)

```wolfram
TFindProof["InverseOfInverse", "AbelianGroupAxioms"]
```

The `Automatic` schedule detects the Group / AbelianGroup structure
(commutative + associative + has-inverse + has-unit) and front-loads
the GT critical-pair weight with `AutoPrecedence` so the inverse
operator ranks above the binary operator -- the inverse-of-inverse
rewrite then orients cleanly. Proves in a fraction of a second.

### 2.2 A Boolean symmetric theorem (GoalDirected)

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

### 2.3 A Sheffer / Wolfram single-operator goal (Waldmeister preset)

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

### 2.4 A cross-system many-axiom theorem (SInE premise selection)

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

## 3. Methods

`Method` selects the saturator's strategy. Every concrete method head
accepts the same suboption vocabulary (section 4); the head fixes the
broad search shape, the suboptions tune it.

### 3.1 `Automatic` (the default)

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

### 3.2 `"Portfolio"`

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

### 3.3 `"Completion"`

Plain unfailing Knuth-Bendix completion. Default `CriticalPairWeight` is
the engine's `Gt`; default `Ordering` is `KBO`; no MNF front. The
workhorse config for an axiom set that admits a finite complete system
under some reduction ordering (groups, abelian groups, monoids, rings,
many Boolean axiomatizations once the right ordering is picked).

When to use: when you want a single explicit completion config (rather
than a portfolio) and the goal is reachable through saturation.

### 3.4 `"GoalDirected"` / `"MNF"`

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

### 3.5 `"Waldmeister"`

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

### 3.6 `"VampireUEQ"`

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

List form takes the same `Method -> {"VampireUEQ", subopt -> value, ...}`
override pattern as `"Waldmeister"`.

When to use: hard Sheffer / nand goals where the `"Waldmeister"` default
walls; experimentally a complement to the Waldmeister-tuned schedule.

## 4. Suboptions catalog

Every entry in the table below is parsed by `atpParseCompletionOpts` /
the dedicated `atpXxxOpt` helpers and propagated through `cEngineProof`
to the C engine (`thvm_atp_set_*`), except where noted. Defaults are the
engine defaults (which `Automatic` may override per detected structure).

### 4.1 `"CriticalPairWeight" -> ...`

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

### 4.2 `"Ordering" -> "KBO" | "LPO"`

Reduction ordering used to orient equations. KBO (Knuth-Bendix Ordering)
is the default and the natural choice for most algebraic structures
(groups, rings, lattices). LPO (Lexicographic Path Ordering) is needed
for variable-duplicating rules (S/W combinators, some boolean
reductions) that KBO cannot orient.

Default: `"KBO"`. When to set: LPO for combinator logic; combine with
`"AutoPrecedence" -> True` for any LPO run.

### 4.3 `"AutoPrecedence" -> True | False`

Use Waldmeister's structure-driven precedence (the PhilMarlow port). For
groups this puts the inverse on top; for rings the distributor `*` above
`+`; etc. Without it the precedence is the identity (label order).

Default: `False`. When to set: any time you select LPO; for KBO on
groups / rings / lattices to orient unary inverses cleanly.

### 4.4 `"AxiomRelevance" -> ...`

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

### 4.5 `"MaxWeight" -> n`

Drop critical pairs whose combined term weight exceeds `n` symbols.
`0` / `Automatic` = unbounded.

Default: 0 (unbounded). When to set: a very large CP queue that wastes
time on giant pairs; pair with `"GoalInterleave"` so the goal-directed
selections still fire.

### 4.6 `"GoalInterleave" -> n`

Every n-th CP selection is a goal-directed (`CPinGoal`) pick; the rest
use the chosen weight. `0` / `Automatic` = off.

Default: 0 (off). `Automatic` for groups / lattices / monoids uses
`GoalInterleave 50` per Waldmeister's `itl(mi)`.

### 4.7 `"GroundJoin" -> True`

Delete ground-joinable critical pairs (every ground instance of the CP
joins, so it is a redundancy). Sound (Martin-Nipkow / Twee criterion).

Default: `False`. When to set: lattice / Boolean axiomatizations where
the CP queue is dominated by ground-joinable redundancies.

### 4.8 `"Connectedness" -> True`

Bachmair-Dershowitz connectedness CP deletion (Twee section 6.2). Drop
a critical pair whose two sides join through intermediate terms strictly
below the peak in the reduction order. A sound generation-cut redundancy
criterion stronger than trivial joinability.

Default: `False`. When to set: alongside `"GroundJoin"` for axiom sets
with many redundant CPs at the same generation depth.

### 4.9 `"SelectionRatio" -> n`

Waldmeister CPdimension fairness: 1 FIFO (oldest-CP) pick per `n` CP
selections, the rest by weight. The fairness lever against
smallest-weight starvation.

Default: 0 (engine default 11). Waldmeister also uses 50 / 100 / 200;
the `"Waldmeister"` preset sets 51.

### 4.10 `"AutoMaxWeight" -> b`

A growing CP-weight bound (`b + 2 * deepest-rule-weight`) that defers
over-weight critical pairs to a stash and force-drains them when the
active queue empties. Keeps the CP queue small (measured ~3.5x smaller
on the hard Sheffer goals) WITHOUT losing completeness (nothing is
permanently dropped).

Default: 0 (off). When to set: `Automatic` for Sheffer uses `20`; try
`10`-`30` on a goal whose CP queue blows up.

### 4.11 `"RHSInterreduce" -> True`

Waldmeister `IR_InterreduktionRechts`: after a rule is oriented,
normalize the RHS of every other rule against it, re-queuing any rule
whose RHS shrinks. Keeps `R` fully reduced so the CP set stays small.

Default: `False`. `Automatic` for `"Waldmeister"` turns it on. When to
set: deep theorem proofs where the rule set otherwise diverges.

### 4.12 `"UnfailingCP" -> True`

Superpose BOTH faces of every unorientable equation (the unfailing
completion completeness requirement). With the default the engine
overlaps the stored lhs only.

Default: `False`. The `"Waldmeister"` preset turns it on. When to set:
any goal whose proof requires reasoning through an unorientable equation
(commutativity-driven Boolean goals).

### 4.13 `"CPSetInterreduce" -> True`

Waldmeister `KPV_KPMengeInterreduzieren`: periodically re-normalize the
whole CP queue against the full rule set, deleting CPs that became
joinable and reweighting the rest, so the heap-min selection tracks
live, irreducible CPs.

Default: `False`. The `"Waldmeister"` preset turns it on; the Sheffer
`"AndAssociativity"` proof needs it.

### 4.14 `"Precedence" -> {sym1, sym2, ...}`

Explicit reduction-ordering precedence (symbol names highest-to-lowest),
mirroring Waldmeister's `p > q > nand` ORDERING block. Resolved against
the engine's symbol labels and applied to both LPO and KBO.

Default: `Automatic` (the chosen identity or `AutoPrecedence`). When to
set: when you know the right precedence from the algebra (e.g. Sheffer
goal-skolem-constants above the binary operator).

### 4.15 `"SkolemHighest" -> True`

Rank the goal's ground / skolemized constants above every operator (the
structural rule Waldmeister's `p > q > nand` precedence encodes). Takes
effect only when supplied; leaves the default precedence byte-identical
otherwise.

Default: `Automatic` (off). When to set: any single-operator Sheffer /
Wolfram goal where the conjecture's skolem constants should orient.

### 4.16 `"FifoTiebreak" -> True`

Waldmeister `-:w1=fifo` secondary CP key: preserve each surviving
critical pair's insertion age across the post-orient CP-normalize sweep,
so equal-weight ties resolve oldest-first run-wide. Off by default,
engine byte-identical when unset.

Default: `False`. When to set: a run where the weight distribution has
many ties and the order of FIFO-tied CPs measurably matters.

### 4.17 `"RecordNorm" -> True | False`

Per-step normalize-trace recording for the ProofObject builder. Default
`True` is the historical path -- WL walks `CP -> NORM_STEP* -> ORIENT`
linearly to reconstruct the chain. `False` routes search through the
fast indexed / flatterm normalize so a long completion saturates at the
C-bench rate; WL then reconstructs the chain through the emitNorm BFS
over the `CP / ORIENT / SIMPLIFY` trace DAG.

Default: `True`. When to set `False`: long-running completions where the
per-step recording overhead dominates and you can pay the BFS
reconstruction cost.

### 4.18 `"ForwardSubsume" -> True`

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

### 4.19 `"BackwardSubsume" -> True`

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

## 5. Per-class recommendations

Empirical patterns from the recent benchmark sweep + parallel Vampire
study. These are the defaults `Automatic` already front-loads; the same
table is useful when you pin `Method` explicitly.

| Class | First-choice config | Rationale |
|-------|---------------------|-----------|
| Boolean symmetric (`DoubleNegation`, `ExcludedMiddle`, `Noncontradiction`) | `"GoalDirected"` | Two sides never share an NF; MNF closes via front collision. (e.g. `BooleanAxioms::DoubleNegation` proves in seconds via GoalDirected.) |
| Combinator SKI / BCKW | `{"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True, "CriticalPairWeight" -> "Add"}` | Variable-duplicating S/W rules need LPO; Add weight matches Waldmeister's KombS. |
| Sheffer / Wolfram single-operator deep (e.g. `AndAssociativity`) | `{"Waldmeister", "CriticalPairWeight" -> "Gt", "CPSetInterreduce" -> True}` | Waldmeister default + CP-set interreduction reaches the deep proofs. |
| Cross-system many-axiom (e.g. `Implies*Axioms` against `MeredithAxioms`) | `{"GoalDirected", "AxiomRelevance" -> "SInE"}` | SInE prunes the irrelevant cross-system axioms; MNF closes the remaining goal. (BooleanAxioms::DeMorgan proves in 3s via Automatic which front-loads GoalDirected on this class.) |
| Group / AbelianGroup | `{"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50, "AutoPrecedence" -> True}` | Waldmeister `GtS`; AutoPrecedence puts inverse on top. |
| Ring | `{"Completion", "Ordering" -> "KBO", "AutoPrecedence" -> True}` | Waldmeister `kbo(Std)`; structure-precedence puts `*` above `+`. |
| AC (commutative+associative, no inverse) | `{"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50}` | Waldmeister `GtS` family. |
| Lattice | `{"Completion", "CriticalPairWeight" -> "Gt", "GroundJoin" -> True, "GoalInterleave" -> 50}` then LPO fallback | Waldmeister `Verband` row uses `gj()` + an LPO pass. |

### 5.1 Currently out of reach

These 5 unit-equality goals are known to be cracked by Vampire 5.0.1 (UEQ
portfolio) within 30 s in our parallel baseline, but neither the
`Automatic` schedule nor any tried explicit `Method` (including
`"VampireUEQ"`, `"CriticalPairWeight" -> "ConjSym"`, `"Diversity"`,
`"Twee"`, `"ForwardSubsume" -> True`, and pairings with `"GroundJoin"
-> True` or `"Connectedness" -> True`) closes them within 25 s on the
single-config form:

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

The orderable subset is shipped as `Method -> "VampireUEQ"`. The three
flags still unported -- backward subsumption (`bs=unit_only` proper:
delete an EXISTING rule when a new one subsumes it, vs. the current
forward-only `"ForwardSubsume"` flag which skips the new rule add),
backward demodulation (`bd=all`), and goal-type-graph premise
selection (`gtg=exists_sym`) -- look like the missing pieces for at
least this target.

## 6. Return specs

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

## 7. Debugging and introspection

### 7.1 Statistics

```wolfram
TFindProof[goal, axioms, "Statistics"]
(* -> <|"Status" -> "Proved", "Steps" -> 314,
        "Rules" -> 27, "Trace" -> 4096, "QueueSize" -> 0|> *)
```

Fast first sanity check: a "Proved" with `Steps` in the hundreds and
`QueueSize` near 0 is a clean saturation; a "TimedOut" with `QueueSize`
in the millions is CP explosion.

### 7.2 `"PreprocessedAxioms"`

```wolfram
TFindProof[goal, axioms, "PreprocessedAxioms"]
```

Returns the axioms after `ForAll -> Pattern` quantifier elimination,
`Exists -> Skolem`, and canonical pattern-variable naming. The exact
shape the C engine encodes.

### 7.3 `"RelevantAxioms"` and `TRelevantAxioms`

```wolfram
TRelevantAxioms["InverseOfInverse", "AbelianGroupAxioms",
    Method -> {"Completion", "AxiomRelevance" -> "SInE"}]
```

Inspect what the relevance filter kept vs. dropped, BEFORE the prove.
The Mode tag identifies the filter (`None`, `"Safe"`, `"Connected"`,
`"SInE"`); each entry of `Dropped` carries the witnessing axiom and the
symbols that triggered the drop reason.

### 7.4 Environment variables

| Env var | Effect |
|---------|--------|
| `THVM_HEAP_CELLS` | Override the IC heap allowance. Useful when a deep saturation needs more node memory than the default. |
| `THVM_ATP_TRACE_MAX` | Raise the trace-entry cap. The default is fine for almost all proofs; raise it for completion runs that exceed the default and need full trace reconstruction. |
| `THVM_ATP_RULE_TRACE` | Set to `1` for a per-rule derivation trace to stderr. Verbose; pin to a single problem at a time. |

All env vars are read once per process at first call.

### 7.5 Tracing a specific Method config

The `RawTrace` return spec is the raw decoded C-engine trace -- one
record per CP / ORIENT / SIMPLIFY event:

```wolfram
TFindProof[goal, axioms, "RawTrace", TimeConstraint -> 5]
(* -> {<|"Reason" -> 3, "ParentA" -> 5, ...|>, ...} *)
```

Pair with `"Lemmas"` to read the final rule set, or with `"Statistics"`
to see how much of the trace cap was used.

### 7.6 Catching a wedge

A goal that hangs longer than expected almost always either explodes the
CP queue (visible via `"Statistics"["QueueSize"]` on a `TimeConstraint`
timeout) or runs the MNF front search on a divergent rule set (visible
in the same way; `Method -> "Completion"` skips MNF). When the queue
explodes, the first lever to pull is `"AutoMaxWeight" -> 20` (defers
over-weight CPs); if that does not help, switch the weight (`Mix2 ->
Mix -> Gt`) and bound `MaxWeight` directly.
