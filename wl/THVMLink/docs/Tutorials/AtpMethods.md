---
Template: TechNote
Name: AtpMethods
Title: Methods and Presets for TFindProof
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/AtpMethods
Keywords: [theorem proving, ATP, Method, preset, portfolio, Waldmeister, Vampire, Twee, EProver, SInE, AutoPrecedence, GoalDirected, MNF]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, SMT, TPTPImport, Overview]
---

## What the Method system does

[TFindProof]() runs a Knuth-Bendix completion engine with a fairly large search surface: which critical-pair (CP) weight to sort the queue on, which reduction ordering to orient rules with, whether to also run the goal-directed MNF front search, whether to drop ground-joinable CPs, whether to prune the axiom set with [SInE](https://link.springer.com/chapter/10.1007/978-3-642-31365-3_24), and a dozen more knobs. The `Method` option is where that surface lives.

Three shapes:

- **Single explicit config** - a list head like `"Completion"` (or `"GoalDirected"`) plus rule-shaped suboptions: `Method -> {"Completion", "Ordering" -> "LPO", "CriticalPairWeight" -> "Mix2"}`.
- **Named preset** - a one-word name that bundles a known-good set of suboptions: `Method -> "Waldmeister"` (or `Method -> "Twee"`, `"VampireUEQ"`, `"EProver"`).  List form lets you override a preset's defaults: `Method -> {"Waldmeister", "CriticalPairWeight" -> "Gt"}`.
- **Portfolio** - a schedule of configs tried in turn, the first that proves wins.  `Method -> Automatic` (the default), `"Portfolio"`, `"VampirePortfolio"`, `"VampirePortfolioCompact"`.

[TAtpSchedule]() returns the schedule a Method spec expands to; [TAtpDescribeMethod]() returns the full options Association the C engine will receive once a list-form spec is merged with a preset's defaults.

## Setting up

```wl
Needs["THVMLink`ATP`"];
```

Equational axioms can be supplied directly as a list, or as a theory name resolved through [AxiomaticTheory]().  All examples below assume the load entry has run.

## A few problem-shaped examples

The defaults handle the common cases; the four examples below cover the situations where reaching past `Automatic` actually pays off.

### Group theorem (the default suffices)

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms"]
```

The `Automatic` schedule detects the AbelianGroup structure (commutative + associative + has-inverse + has-unit) and front-loads `GT` weight with `AutoPrecedence`, so the inverse operator ranks above the binary operator.  The double-inverse rewrite orients cleanly and the proof falls out in a fraction of a second.

### A symmetric Boolean goal (needs the MNF front)

```wl
TFindProof["DoubleNegation", "BooleanAxioms",
    Method -> "GoalDirected", TimeConstraint -> 10]
```

`DoubleNegation` is a symmetric goal whose two sides never share a normal form, so plain completion never closes it.  `"GoalDirected"` adds the MNF bidirectional front search alongside completion; the front collision then resolves into a critical-pair-lemma proof.  `Automatic` also closes this (its tail eventually tries `"GoalDirected"`), but pinning the Method skips the upstream attempts.

### A Sheffer / Wolfram single-operator goal (Waldmeister preset)

```wl
TFindProof["AndAssociativity", "WolframAxioms",
    Method -> {"Waldmeister",
        "CriticalPairWeight" -> "Gt",
        "CPSetInterreduce" -> True},
    TimeConstraint -> 30]
```

`Method -> "Waldmeister"` bundles Waldmeister's faithful default strategy for an unrecognized single-operator problem: KBO + AutoPrecedence + `SelectionRatio -> 51` + RHSInterreduce + UnfailingCP + CPSetInterreduce.  The `Gt` weight + CPSetInterreduce override is the empirically-cracking combination for `AndAssociativity`.

### A cross-system many-axiom theorem (SInE premise selection)

```wl
TFindProof["ImpliesWolframAxioms", "MeredithAxioms",
    Method -> {"GoalDirected", "AxiomRelevance" -> "SInE"},
    TimeConstraint -> 30]
```

The conjecture only cites a subset of `MeredithAxioms`'s predicates.  SInE pre-filters the axiom list to those reachable from the conjecture's symbols by a bounded breadth-first walk along the D-relation.  Defaults `st = 3, sd = 2, sgt = 8` mirror Vampire's `--sine_tolerance / --sine_depth / --sine_generality_threshold`.

## Methods

`Method` selects the saturator's strategy.  Every concrete method head accepts the same suboption vocabulary; the head fixes the broad search shape, the suboptions tune it.

- `Automatic` (default) - problem-aware portfolio.  Analyses the axioms + conjecture, detects the algebraic structure (Group / AbelianGroup / Ring / AC / Lattice / Combinatory / Sheffer-Nand / general), and front-loads a tailored config.  Appends the fixed `"Portfolio"` schedule as the fallback tail, so `Automatic` never proves less than `"Portfolio"`.
- `"Portfolio"` - the fixed 4-entry schedule (Mix2 weight, LPO + AutoPrecedence, Gt weight, GoalDirected).  Adequate baseline for problems without a recognised structure.
- `"Completion"` - plain unfailing Knuth-Bendix completion.  Default `Gt` weight + `KBO` + no MNF.  The workhorse for an axiom set that admits a finite complete system under some reduction ordering (groups, abelian groups, monoids, many Ring axiomatisations).
- `"GoalDirected"` / `"MNF"` (synonyms) - completion + the MNF bidirectional front search.  The only configuration that closes a symmetric goal whose two sides never meet at a single normal form.
- `"VampirePortfolio"` - the 13-entry Vampire UEQ rotation.  Many short slices; each entry gets `TimeConstraint / 13`.  Throw-everything-at-it mode.
- `"VampirePortfolioCompact"` - a 3-entry rotation (`"VampireUEQ"` + `"Twee"` + `Mix2 + AutoPrecedence`) sized for small `TimeConstraint`.

## Named presets

Each preset bundles the defaults of a real-world prover so a one-name call reproduces (close to) that prover's behaviour on the input.  List form `{"Preset", subopt -> ...}` lets you override individual defaults.  Inspect the full merged options with [TAtpDescribeMethod]().

- `"Waldmeister"` - `Mix` weight + KBO + `AutoPrecedence` + `SelectionRatio -> 51` + `RHSInterreduce` + `UnfailingCP` + `CPSetInterreduce`.  The faithful Waldmeister default for an unrecognised single-operator Sheffer / nand problem.
- `"VampireUEQ"` - LPO + `AutoPrecedence` + `SelectionRatio -> 10` + `UnfailingCP` + `AutoMaxWeight` + `BackwardSubsume` + `BackwardDemod` + `RHSInterreduce` + MNF front.  Modeled on the Vampire 5.0.1 portfolio entry that cracks `ShefferAxioms/AndAssociativity` in the cross-system baseline.
- `"Twee"` - `CriticalPairWeight -> "Twee"` + `GroundJoin` + `Connectedness` + `UnfailingCP` + `BackwardSubsume` + `BackwardDemod` + `RHSInterreduce` + `AutoMaxWeight -> 20`.  Twee 2.x defaults (Smallbone, 2021+).
- `"EProver"` - `CriticalPairWeight -> "ConjSym"` + KBO + `AutoPrecedence -> "Occurrence"` + `SelectionRatio -> 10` + `AutoMaxWeight -> 20` + `BackwardSubsume` + `RHSInterreduce` + `UnfailingCP`.  E's typical CASC config (the `Occurrence` precedence mirrors E's `-G InvFreqRank`).
- `"VampireRandom"` - LPO + `AutoPrecedence` + `SelectionRatio -> 10` + `UnfailingCP` + `GroundJoin` + `BackwardDemod` + `RHSInterreduce` + `RandomRatio -> 32` + `RandomSeed -> 3681690318` + `LRS`.  Vampire's `lrs+10_32:to=lpo:sp=arity:fgj=on:bd=all:random_seed=...` cracking entry for `McCuneAxioms/EqualityOfInverses`.

## Suboptions

The list-form spec accepts any of the following; defaults match the C engine's defaults so unspecified suboptions are byte-identical to leaving them out.

| Suboption                       | Values                                                 | What it changes |
|---------------------------------|--------------------------------------------------------|-----------------|
| `"CriticalPairWeight"`          | `"Add" \| "Max" \| "Ord" \| "Gt" \| "Mix" \| "Mix2" \| "Unif" \| "Goal" \| "Twee" \| "ConjSym" \| "Diversity" \| "RelLevel" \| "Staggered"` | Sorts the CP queue.  `Gt` is the engine default; `Mix2` is the strongest general baseline; `Twee` is Twee's asymmetric-bias scorer; `ConjSym` is E's conjecture-symbol bias. |
| `"Ordering"`                    | `"KBO" \| "LPO"`                                       | The reduction ordering used for orientation.  KBO orients more rules; LPO supports variable-duplicating rules KBO refuses. |
| `"AutoPrecedence"`              | `True \| False \| "Occurrence"`                        | `True` runs Waldmeister's structure-driven Praezedenzgenerator; `"Occurrence"` runs Vampire `sp=occurrence` / E `InvFreqRank` (rare symbols rank highest). |
| `"Precedence"` / `"SkolemHighest"` | `{sym1, sym2, ...}` / `True`                        | An explicit per-symbol precedence chain; `SkolemHighest` ranks goal Skolem constants above every operator. |
| `"AxiomRelevance"`              | `None \| "Safe" \| "Connected" \| "SInE" \| {"SInE", "SineTolerance" -> st, "SineDepth" -> sd, "SineGenerality" -> sgt}` | Pre-filters axioms.  Safe drops dead-weight; Connected is symbol-reachability; SInE is the Hoder-Voronkov premise selector. |
| `"MaxWeight"`                   | non-negative integer                                   | Drops CPs whose combined symbol count exceeds the bound.  0 = unbounded. |
| `"AutoMaxWeight"`               | non-negative integer                                   | Growing CP-weight bound base; defers over-weight CPs to a stash and drains them when the active queue empties.  Keeps the queue small without losing completeness. |
| `"GoalInterleave"`              | non-negative integer                                   | Every n-th selection is a goal-directed pick (closest CP to the goal). |
| `"SelectionRatio"`              | non-negative integer                                   | Waldmeister CPdimension fairness: 1 FIFO pick per n weight picks.  Default 11. |
| `"GroundJoin"`                  | `True \| False`                                        | Delete ground-joinable CPs (Twee redundancy criterion). |
| `"Connectedness"`               | `True \| False`                                        | Delete a CP whose two sides join through terms strictly below the peak (Bachmair-Dershowitz; Twee section 6.2). |
| `"UnfailingCP"`                 | `True \| False`                                        | Superpose BOTH faces of an unorientable equation - the completeness requirement of unfailing completion. |
| `"RHSInterreduce"`              | `True \| False`                                        | Waldmeister `IR_InterreduktionRechts`: normalise the RHS of every rule against each new rule. |
| `"CPSetInterreduce"`            | `True \| False`                                        | Periodic full-queue re-normalise against R; drops CPs that became joinable, reweights the rest. |
| `"FifoTiebreak"`                | `True \| False`                                        | Waldmeister `-:w1=fifo`: preserve insertion age across the post-orient sweep so equal-weight ties resolve oldest-first. |
| `"BackwardSubsume"` / `"BackwardDemod"` | `True \| False`                                | After adding a new rule, soft-delete older rules subsumed by it / normalise older rules against the new ones. |
| `"ForwardSubsume"`              | `True \| False`                                        | Skip a new CP that is already subsumed by a queued CP. |
| `"LRS"`                         | `True \| False`                                        | Vampire's Limited Resource Strategy: predict whether each CP will be reached within the wall budget, prune the unreachable ones. |
| `"SOS"`                         | `True \| False`                                        | Set of Support: restrict CP generation to overlaps that touch the conjecture's symbols.  |
| `"RandomRatio"` / `"RandomSeed"` | integer ratio / `u64` seed                            | Vampire-style random CP selection.  Every n-th selection picks uniformly random from the queue via a seeded xorshift64. |
| `"RecordNorm"`                  | `True \| False`                                        | Per-step normalisation trace for the `ProofObject` builder.  `False` routes search through the fast indexed normalize for long completions. |
| `"SymbolWeights"` / `"VarWeight"` | per-label array / integer                            | Per-symbol KBO weight override; var weight (Waldmeister `-w VAR=N`). |

## Schedule and method introspection

Before launching a portfolio, you may want to see what schedule it expands to and what each entry's full suboptions will be.

```wl
TAtpSchedule["VampirePortfolio"]
```
<!-- => {{"VampireUEQ"}, {"Completion", "CriticalPairWeight" -> "Twee", ...}, ...} -->

```wl
TAtpSchedule[Automatic,
    Inactive[Equal][x*y, y*x], "AbelianGroup"]
```
<!-- => the structure-aware front + the fixed Portfolio tail -->

```wl
TAtpDescribeMethod["Waldmeister"]
```
<!-- => <|"CriticalPairWeight" -> "Mix", "Ordering" -> "KBO",
        "AutoPrecedence" -> True, "SelectionRatio" -> 51, ...|> -->

For a list spec like `{"Twee", "Ordering" -> "LPO"}`, [TAtpDescribeMethod]() merges the preset defaults with the user's overrides so you see what actually wins.  For a portfolio spec it returns `<|"Schedule" -> {...}|>`.

## Axiom relevance

Large theories carry axioms that no proof of the current goal needs.  [TRelevantAxioms]() reports the filter's keep / drop partition without running the prover:

```wl
TRelevantAxioms[
    Inactive[Equal][x*y, y*x],
    "AbelianGroup",
    Method -> {"AxiomRelevance" -> "Safe"}]
```
<!-- => <|"Mode" -> "Safe", "Kept" -> {...},
        "Dropped" -> {<|"Axiom" -> _, "Symbols" -> _, "Reason" -> _|>, ...}|> -->

Modes:

- `None` - keep all axioms.
- `"Safe"` (default) - drop only provably dead-weight axioms (a confined symbol on both sides; sound and completeness-preserving).
- `"Connected"` - symbol-reachability pruning.  Coarser heuristic; can drop a needed axiom.
- `"SInE"` - Hoder-Voronkov premise selection as it ships in Vampire.  Defaults mirror Vampire's CLI knobs.

## Top-level options

Beyond `Method`:

- `MaxSteps -> n` (default 200000) - hard cap on the CP-processing step counter.
- `TimeConstraint -> seconds` (default `Infinity`; portfolios divide fairly across schedule entries; `TimeConstrained[...]` and `Abort[]` interrupt the running C engine).
- `PortfolioFrontLoad -> n` (default 0) - widen the slice given to the first n schedule entries (each gets 2x what an unweighted recurrence would assign).  Use when an `Automatic` front genuinely deserves more time than the fair share.

## Return-spec selection

The last positional argument selects what [TFindProof]() returns:

- A single String returns that bare value.
- A list of Strings returns an Association keyed by the requested names.
- `All` returns an Association of every spec.

Available keys: `"ProofObject"` (default; the bare object), `"Lemmas"` (the completed rule set), `"PreprocessedAxioms"` (the normalised axioms fed to the engine), `"RelevantAxioms"` ([TRelevantAxioms]()'s keep / drop partition), `"RawTrace"` (the decoded completion trace), `"Statistics"` (a small run-stats Association), `"Status"` (`"Proved"` / `"Saturated"` / `"TimedOut"` / `"Failed"`).

```wl
TFindProof[Inactive[Equal][x*y, y*x], "AbelianGroup", All]
```
<!-- => <|"ProofObject" -> _, "Status" -> "Proved", "Lemmas" -> {...}, ...|> -->

## Recipes

A few combinations that come up often beyond the four [problem-shaped examples](#a-few-problem-shaped-examples) above.

### Maximum CP-queue pruning

Stack the redundancy criteria so the queue stays small over long completions.  Trades CPU time per step against queue-size growth:

```wl
TFindProof[conjecture, axioms,
    Method -> {"Completion",
        "GroundJoin" -> True,
        "Connectedness" -> True,
        "BackwardSubsume" -> True,
        "BackwardDemod" -> True,
        "AutoMaxWeight" -> 20}]
```

### Variable-duplicating combinator goal (S, W, M)

Combinator-logic axioms (`S x y z = x z (y z)` etc.) require an ordering that allows variable-duplicating RHS.  LPO does; KBO does not.

```wl
TFindProof[conjecture, axioms,
    Method -> {"Completion",
        "Ordering" -> "LPO",
        "AutoPrecedence" -> True,
        "CriticalPairWeight" -> "Add"}]
```

### Cross-system Sheffer "implies" goals (tight age bias)

The cross-system `ShefferAxioms/ImpliesWolframAxioms` family responds to `Mix2` weight with `SelectionRatio -> 2` - one FIFO pick per two heuristic picks, an aggressive age bias.  The tight 1-FIFO-per-2 bias is a sharp sweet spot; `SelectionRatio -> 1` and `-> 5` both miss.

```wl
TFindProof["ImpliesWolframAxioms", "ShefferAxioms",
    Method -> {"Completion",
        "CriticalPairWeight" -> "Mix2",
        "SelectionRatio" -> 2,
        "AutoMaxWeight" -> 20},
    TimeConstraint -> 30]
```

### Vampire's McCune cracking config

The single Vampire 5.0.1 portfolio entry that proves `McCuneAxioms/EqualityOfInverses` in the cross-system baseline (`lrs+10_32:to=lpo:sp=arity:fgj=on:bd=all:random_seed=3681690318`) is bundled as `"VampireRandom"`:

```wl
TFindProof["EqualityOfInverses", "McCuneAxioms",
    Method -> "VampireRandom", TimeConstraint -> 60]
```

The seeded xorshift64 inside the random pick makes the trajectory reproducible; pass `"RandomSeed" -> n` to switch seed.

## Where the code lives

- `wl/THVMLink/Kernel/ATP/ATP.wl` - the WL surface: method parser, preset dispatcher, portfolio scheduler, `Automatic` problem-aware front-load.
- `src/atp/_.c` and `src/atp/precedence.c` - the C engine: completion loop, CP selection, redundancy checks, precedence generation.
- `docs/tutorial/atp_methods.md` - the long-form reference covering every single suboption and every method head.
