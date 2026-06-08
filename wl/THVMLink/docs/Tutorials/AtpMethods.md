---
Template: TechNote
Name: AtpMethods
Title: Methods and Presets for TFindProof
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/AtpMethods
Keywords: [theorem proving, ATP, Method, preset, portfolio, Waldmeister, Vampire, Twee, EProver, SInE, AutoPrecedence, GoalDirected, MNF]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, SMT, TPTPImport, Overview, Disproof]
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
    Method -> "GoalDirected", TimeConstraint -> 5]
```

`DoubleNegation` is a symmetric goal whose two sides never share a normal form, so plain completion never closes it.  `"GoalDirected"` adds the MNF bidirectional front search alongside completion; the front collision then resolves into a critical-pair-lemma proof.  `Automatic` also closes this (its tail eventually tries `"GoalDirected"`), but pinning the Method skips the upstream attempts.

### A Sheffer / Wolfram single-operator goal (the orientation matters)

```wl
TFindProof["Commutativity", "WolframAxioms",
    Method -> {"GoalDirected",
        "Ordering"           -> "LPO",
        "SkolemHighest"      -> True,
        "CriticalPairWeight" -> "Add",
        "FifoTiebreak"       -> True,
        "UnfailingCP"        -> True},
    TimeConstraint -> 15]
```

The Sheffer / nand axiomatisations (`WolframAxioms`, `ShefferAxioms`) crack under a non-default config: the goal `nand(p,q) == nand(q,p)` is unorientable, so `"GoalDirected"` adds the bidirectional MNF front (pure completion never collides the two sides); LPO with `"SkolemHighest" -> True` ranks the goal Skolems above `nand`, which is Waldmeister's own cracking ordering on this signature; `"Add"` weight + `"FifoTiebreak"` give a uniform age-biased CP queue that doesn't preferentially explore deep terms.  With this combination `Commutativity` proves in a few seconds.  The harder `AndAssociativity` over the same axioms uses the same recipe but takes ~25s on the paclet (~15s in the C bench, ~14s under real Waldmeister); the bundled `"Waldmeister"` preset is slower on AndAssoc because its Mix-weight CP selection picks a different saturation trajectory.

### A many-axiom theory (SInE premise selection)

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    Method -> {"Completion", "AxiomRelevance" -> "SInE"}]
```

SInE pre-filters the axiom list to those reachable from the conjecture's symbols by a bounded breadth-first walk along the D-relation: a goal that only mentions the inverse function does not need every group axiom in scope, only the ones whose head symbols transitively reach `Inverse`.  Defaults `st = 3, sd = 2, sgt = 8` mirror Vampire's `--sine_tolerance / --sine_depth / --sine_generality_threshold`.  On a many-hundred-axiom cross-system theory (e.g. `"MeredithAxioms"` against a Wolfram-axiom goal) the filter is what keeps the CP queue tractable.

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
- `"ENIGMA"` - ML-guided critical-pair selection: a trained proof-relevance scorer (`"CriticalPairWeight" -> "Learned"`) on a sound bounded-queue base (KBO + `AutoPrecedence` + `UnfailingCP` + `RHSInterreduce` + `AutoMaxWeight -> 20`).  Uses the model pushed by `TAtpSetLearnedScorer`, or a baked-in logistic regression otherwise; completeness holds regardless because the engine still takes a periodic FIFO pick.  See the learn loop below.

### Learned critical-pair selection (ENIGMA)

`Method -> "ENIGMA"` ranks the critical-pair queue with a learned proof-relevance model instead of a hand-tuned weight.  Close the loop in four steps - prove a corpus with per-CP feature recording on, label the processed CPs by proof-trace reachability, train a scorer on thvm's own deep-learning stack, then push it back into the engine:

```wl
#| eval: false
ds      = TAtpCpDataset["AbelianGroupAxioms"];
trained = TAtpTrainScorer[ds];
TAtpSetLearnedScorer[trained["Model"]];
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> "ENIGMA"]
```

`TAtpTrainScorer["AbelianGroupAxioms"]` collapses the dataset + train steps into one call.  A structural graph variant - `TAtpCpGraph` / `TAtpGraphDataset` - emits symbol/variable-anonymised hypergraphs for a graph neural network instead of the 14-feature vectors.  `TAtpSetLearnedScorer[Clear]` drops the model and reverts to the baked-in scorer.

## External CLI process methods

The presets above stay inside `thvm`'s own C engine.  Four more methods take the same conjecture and axioms but dispatch the proof through an external prover's command-line binary, then lift the SZS / TSTP output back into the same `ProofObject` shape the internal presets produce - so a comparator, a [ProofFunction]() verifier, or a dataset reader sees one structure across both paths.

| Method                | Binary it shells out to                          | CLI strategy                                   |
|-----------------------|--------------------------------------------------|------------------------------------------------|
| `"VampireProcess"`    | `/opt/homebrew/bin/vampire` (`brew install vampire`)  | `--mode casc --proof tptp`                |
| `"TweeProcess"`       | `~/.cabal/bin/twee` or `/opt/homebrew/bin/twee` (`cabal install twee`) | `--tstp --quiet`        |
| `"WaldmeisterProcess"`| Path in `$WMCLI` (build from source)             | wmcli on a pre-generated `.pr` file            |
| `"EproverProcess"`    | `/opt/homebrew/bin/eprover` (`brew install eprover`)  | `--auto-schedule --proof-object --tstp-format` |

All four route through a per-CLI builder (`TVampireProofObject` / `TTweeProofObject` / `TWaldmeisterProofObject` / `TEproverProofObject`) which calls the binary, parses its SZS / TSTP derivation into a normalised inference list, and threads the result through the shared `TSZSDerivationToProofObject` builder.  Each inference step lands under one of the same `ProofDataset` construct keys the internal engine uses (`{"Axiom", n}`, `{"Hypothesis", n}`, `{"CriticalPairLemma", n}`, `{"SubstitutionLemma", n}`, `{"Conclusion", 1}`).

The four Process builders share the same option set on top of `TimeConstraint`:

- `"Binary" -> Automatic` - absolute path override.  `Automatic` walks the per-CLI binary-discovery list above and uses the first hit.
- `"ParseFormulas" -> False` - when `True`, the per-step `Statement` field is `TPTPImport`-parsed into a WL expression so structural comparison against the internal engine's output works.  Slow (~5 s per formula on a multi-step proof).
- `"LiftToProofObject" -> False` - when `True`, wraps the Association into a literal `ProofObject["EquationalLogic", goal, axioms, data]` head (implies `"ParseFormulas" -> True`).  The standard property accessors then work: `pf["ProofFunction"]`, `pf["ProofGraph"]`, `pf["ProofLength"]`, `pf["Theorems"]`.

When the underlying CLI binary isn't installed, the builder returns `Failure["ExternalNoProof", <|"Tool" -> _, "Status" -> _, "Seconds" -> _|>]` rather than raising an error - so a fresh-checkout build does not break just because `eprover` hasn't been brew-installed.

A quick parity check, internal `"Waldmeister"` preset vs the external `wmcli` Process route on the same conjecture:

```wl
{Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "Waldmeister"],
 Head @ TFindProof["InverseOfInverse", "AbelianGroupAxioms",
        Method -> "WaldmeisterProcess", "LiftToProofObject" -> True]}
```
<!-- => {ProofObject, ProofObject} - same shape, different prover -->

Without the lift, the Process methods return the raw Association from `TSZSDerivationToProofObject`, with `"Backend" -> "SZS"` and the same per-step keys:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    Method -> "VampireProcess"]
```
<!-- => <|"Status" -> "Proved", "Backend" -> "SZS", "Steps" -> {...}, ...|> -->

## Suboptions

The list-form spec accepts any of the following; defaults match the C engine's defaults so unspecified suboptions are byte-identical to leaving them out.

| Suboption                       | Values                                                 | What it changes |
|---------------------------------|--------------------------------------------------------|-----------------|
| `"CriticalPairWeight"`          | `"Add" \| "Max" \| "Ord" \| "Gt" \| "Mix" \| "Mix2" \| "Unif" \| "Goal" \| "Twee" \| "ConjSym" \| "Diversity" \| "RelLevel" \| "Staggered" \| "Learned"` | Sorts the CP queue.  `Gt` is the engine default; `Mix2` is the strongest general baseline; `Twee` is Twee's asymmetric-bias scorer; `ConjSym` is E's conjecture-symbol bias; `Learned` ranks by the pushed ENIGMA model (`Method -> "ENIGMA"`). |
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
    Inactive[Equal][x \[CircleTimes] y, y \[CircleTimes] x], "AbelianGroupAxioms"]
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
    Inactive[Equal][x \[CircleTimes] y, y \[CircleTimes] x],
    "AbelianGroupAxioms",
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
- `PortfolioFrontLoad -> n` (default 1) - widen the slice given to the first n schedule entries (each gets 2x what an unweighted recurrence would assign).  Use when an `Automatic` front genuinely deserves more time than the fair share.

## Return-spec selection

The last positional argument selects what [TFindProof]() returns:

- A single String returns that bare value.
- A list of Strings returns an Association keyed by the requested names.
- `All` returns an Association of every spec.

Available keys: `"ProofObject"` (default; the bare object), `"Lemmas"` (the completed rule set), `"PreprocessedAxioms"` (the normalised axioms fed to the engine), `"RelevantAxioms"` ([TRelevantAxioms]()'s keep / drop partition), `"RawTrace"` (the decoded completion trace), `"Statistics"` (a small run-stats Association), `"Status"` (`"Proved"` / `"Saturated"` / `"TimedOut"` / `"Failed"`).

```wl
TFindProof[Inactive[Equal][x \[CircleTimes] y, y \[CircleTimes] x], "AbelianGroupAxioms", All]
```
<!-- => <|"ProofObject" -> _, "Status" -> "Proved", "Lemmas" -> {...}, ...|> -->

## Recipes

A few combinations that come up often beyond the four [problem-shaped examples](#a-few-problem-shaped-examples) above.

### Maximum CP-queue pruning

Stack the redundancy criteria so the queue stays small over long completions.  Trades CPU time per step against queue-size growth:

```wl
#| eval: false
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
#| eval: false
TFindProof[conjecture, axioms,
    Method -> {"Completion",
        "Ordering" -> "LPO",
        "AutoPrecedence" -> True,
        "CriticalPairWeight" -> "Add"}]
```

### Cross-system Sheffer "implies" goals (tight age bias)

The cross-system `ShefferAxioms/ImpliesWolframAxioms` family responds to `Mix2` weight with `SelectionRatio -> 2` - one FIFO pick per two heuristic picks, an aggressive age bias.  The tight 1-FIFO-per-2 bias is a sharp sweet spot; `SelectionRatio -> 1` and `-> 5` both miss.

```wl
#| eval: false
TFindProof["ImpliesWolframAxioms", "ShefferAxioms",
    Method -> {"Completion",
        "CriticalPairWeight" -> "Mix2",
        "SelectionRatio" -> 2,
        "AutoMaxWeight" -> 20},
    TimeConstraint -> 60]
```
<!-- => ProofObject[...] - proves in ~4s (covered by atp.wlt) -->

Pinned `eval: false` because the shared build kernel accumulates state across earlier cells and Sheffer Mix2 saturation can push it past the per-file memory budget; the same config runs cleanly in a fresh kernel and is exercised by `wl/THVMLink/Tests/atp.wlt`.

### Vampire's seeded-random preset

`"VampireRandom"` bundles `LPO` + `AutoPrecedence` + `SelectionRatio -> 10` + `UnfailingCP` + `GroundJoin` + `BackwardDemod` + `RHSInterreduce` + `RandomRatio -> 32` + `RandomSeed -> 3681690318` + `LRS` - the Vampire 5.0.1 portfolio entry `lrs+10_32:to=lpo:sp=arity:fgj=on:bd=all:random_seed=3681690318` that cracks `McCuneAxioms/EqualityOfInverses` in the cross-system baseline (which thvm currently misses; the recipe stays here as the cross-system reference).  Applied to a problem within reach it still closes cleanly:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    Method -> "VampireRandom", TimeConstraint -> 10]
```

The seeded xorshift64 inside the random pick makes the trajectory reproducible; pass `"RandomSeed" -> n` to switch seed.

## Where the code lives

- `wl/THVMLink/Kernel/ATP/ATP.wl` - the WL surface: method parser (`atpParseMethod`) and preset dispatcher.
- `wl/THVMLink/Kernel/ATP/ATP_Method.wl` - the portfolio scheduler, `atpAnalyzeStructure`, and the `Automatic` problem-aware front-load (`atpAutoTune`).
- `src/atp/_.c` and `src/atp/precedence.c` - the C engine: completion loop, CP selection, redundancy checks, precedence generation.
- `docs/tutorial/atp_methods.md` - the long-form reference covering every single suboption and every method head.
