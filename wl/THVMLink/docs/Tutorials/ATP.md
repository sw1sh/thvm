---
Template: TechNote
Name: ATP
Title: Theorem Proving with THVMLink
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/ATP
Keywords: [theorem proving, ATP, equational, completion, Knuth-Bendix, Waldmeister, Vampire, congruence closure, SMT, TPTP]
RelatedGuides: [THVMLink]
RelatedTutorials: [Overview]
---

## What the ATP surface covers

<code>THVMLink\`ATP\`</code> wraps `thvm`'s C-side proof engines and presents them through a single Wolfram surface. Two engines live behind the same `Needs["THVMLink``ATP``"]`:

- **Unfailing Knuth-Bendix completion** - first-order equational logic. Saturates a set of axioms into a confluent rewrite system; tries to refute the negated conjecture. The flagship entry point is [TFindProof](paclet:WolframInstitute/THVMLink/ref/TFindProof) (`TATP` is the lower-level cousin that returns the raw saturation Association).
- **Congruence closure (QF_UF)** - quantifier-free first-order theory of equality with uninterpreted functions, accessed through [TSatEUF](), [TFindProofSMT](), and [TSmtDecide](). Decides ground entailment in time linear in the term count; `TSmtDecide` lifts it to arbitrary Boolean combinations via DPLL(T).

Either engine takes TPTP problem files directly: pipe `Wolfram``Parser``TPTPImport[File["..."]]` straight into `TFindProof`/`TFindProofSMT`, or pass the path/string and the dispatcher does the parse for you.

This note walks both engines end to end through a single problem family - abelian groups and the Sheffer / nand axioms - so the example shapes carry from one section to the next.

## Setting up

The ATP context is its own load. The thvm context only carries the IC primitives; ATP / SMT symbols arrive on the path through a sibling `Get`:

```wl
Needs["THVMLink`ATP`"];
```

Equational axioms can be supplied directly, or as a name resolved through the built-in [AxiomaticTheory](). Variables are written as patterns (`x_`, `y_`); the engine treats them as universally-quantified meta-variables.

---

## Proving an equational theorem

[TFindProof](paclet:WolframInstitute/THVMLink/ref/TFindProof) takes a conjecture and an axiom set and returns a real Wolfram `ProofObject`. The simplest call resolves both names through `AxiomaticTheory`:

```wl
p = TFindProof["InverseOfInverse", "AbelianGroupAxioms"];
{Head[p], p["Status"], p["ProofLength"]}
```
<!-- => {ProofObject, "Proved", _Integer} -->

The returned object speaks the standard `ProofObject` interface: `p["ProofDataset"]`, `p["ProofGraph"]`, `p["ProofFunction"]`, `p["ProofLength"]`, `p["Properties"]` all work the same way they do for built-in [FindEquationalProof]().

Mix an explicit conjecture against a named theory:

```wl
TFindProof[Inactive[Equal][x*y*z, z*y*x], "AbelianGroupAxioms"]
```

Pass both arguments explicitly when there's no canonical name for the axiom set - typical when working with a custom theory:

```wl
TFindProof[
    Inactive[Equal][x*y, y*x],
    {Inactive[Equal][x*(y*z), (x*y)*z],
     Inactive[Equal][x*1, x],
     Inactive[Equal][x*inv[x], 1]}
]
```

A list-valued conjecture (a multi-equation theorem, e.g. one of the entries in `AxiomaticTheory["BooleanAxioms", "NotableTheorems"]`) returns a list of `ProofObject`s, one per conjunct.

## Single-argument completion

Drop the conjecture entirely and `TFindProof` runs saturation as its own deliverable - it returns the completed rule set as a list of `Inactive[Equal]` lemmas. Bound it with `TimeConstraint`, since non-terminating axiom sets never saturate:

```wl
TFindProof["AbelianGroupAxioms", TimeConstraint -> 5]
```
<!-- => {Inactive[Equal][...], ...} -->

The same call works on a list of axioms. Pair it with `"RawTrace"` to get the decoded saturation trace alongside the lemmas:

```wl
TFindProof[
    {Inactive[Equal][x*(y*z), (x*y)*z], Inactive[Equal][x*1, x]},
    "RawTrace",
    TimeConstraint -> 3
]
```

## Picking what comes back

The last positional argument selects what `TFindProof` returns. A single string returns that bare value; a list returns an Association keyed by the requested names; `All` returns every output:

```wl
TFindProof[Inactive[Equal][x*y, y*x], "AbelianGroupAxioms", All]
```
<!-- => <|"ProofObject" -> _, "Status" -> "Proved", "Lemmas" -> {...},
        "PreprocessedAxioms" -> {...}, "RelevantAxioms" -> <|...|>,
        "RawTrace" -> {...}, "Statistics" -> <|...|>|> -->

Available keys: `"ProofObject"` (default; the bare object), `"Lemmas"` (the completed rule set), `"PreprocessedAxioms"` (the normalised axioms fed to the engine), `"RelevantAxioms"` ([TRelevantAxioms]()'s keep / drop partition), `"RawTrace"` (the decoded completion trace), `"Statistics"` (a small run-stats Association), `"Status"` (`"Proved"` / `"Saturated"` / `"TimedOut"` / `"Failed"`).

---

## Methods, presets, portfolios

`Method` is where the saturator's heuristic surface lives. Defaults to `Automatic`. The full sub-option list is reproduced in [TFindProof](paclet:WolframInstitute/THVMLink/ref/TFindProof)'s reference page; the load-bearing options are:

- `"Completion"` vs `"GoalDirected"` - the algorithm shape.
- `"Ordering" -> "KBO" | "LPO"` - the term ordering used for orientation.
- `"CriticalPairWeight" -> "Add" | "Max" | "Ord" | "Gt" | "Mix" | "Mix2" | "Unif" | "Goal"` - the CP selection heuristic.
- `"AutoPrecedence" -> True | False` - Waldmeister's symbol-precedence inference.
- `"AxiomRelevance" -> None | "Safe" | "Connected" | "SInE"` - the axiom filter (also reachable through [TRelevantAxioms]()).
- `"UnfailingCP" -> True` - superpose both faces of an unorientable equation (completeness requirement of unfailing completion).
- `"GroundJoin" -> True`, `"Connectedness" -> True` - the Twee redundancy criteria.

### Single explicit config

Pass a single Association as `Method`:

```wl
TFindProof[
    Inactive[Equal][x*y, y*x],
    "AbelianGroupAxioms",
    Method -> {"Completion",
        "Ordering"          -> "LPO",
        "AutoPrecedence"    -> True,
        "CriticalPairWeight"-> "Mix2",
        "SelectionRatio"    -> 2}
]
```

### Named presets

Each named preset bundles the defaults of a real-world prover so a one-name call reproduces (close to) that prover's behaviour on the input:

- `"Waldmeister"` - Mix weight + KBO + AutoPrecedence + SelectionRatio 51 + RHSInterreduce + UnfailingCP + CPSetInterreduce.
- `"VampireUEQ"` - LPO + AutoPrecedence + SelectionRatio 10 + UnfailingCP + AutoMaxWeight + BackwardSubsume + BackwardDemod + RHSInterreduce + MNF front.
- `"Twee"` - CPW Twee + GroundJoin + Connectedness + UnfailingCP + BackwardSubsume + BackwardDemod + RHSInterreduce + AutoMaxWeight 20.
- `"EProver"` - CPW ConjSym + KBO + SelectionRatio 10 + AutoMaxWeight 20 + BackwardSubsume + RHSInterreduce + UnfailingCP.

```wl
TFindProof[
    Inactive[Equal][nand[nand[a, b], nand[a, b]],
                    nand[nand[a, a], nand[b, b]]],
    "ShefferAxioms",
    Method -> "Waldmeister"
]
```

### Portfolios

A portfolio is a schedule of single-config Methods tried in turn; the first that proves + verifies wins. `TimeConstraint` divides fairly across the schedule (each remaining entry gets `remaining-budget / remaining-configs` seconds).

- `"Portfolio"` - fixed 4-entry schedule (Mix2 weight, then LPO + AutoPrecedence, then GT weight, then GoalDirected). Adequate baseline for problems without a recognised structure.
- `"VampirePortfolio"` - the 13-entry Vampire UEQ rotation, the canonical "throw everything at it" schedule.
- `"VampirePortfolioCompact"` - 3-entry rotation (VampireUEQ + Twee + Mix2-AutoPrec) sized for small `TimeConstraint`.
- `Automatic` (default) - problem-aware. Analyses the conjecture's algebraic structure (Group / AbelianGroup / Ring / Combinatory / AC / Sheffer-Nand), front-loads a tailored config, then appends the fixed `"Portfolio"` as fallback. Never proves *less* than `"Portfolio"` because the fallback is always there.

```wl
TFindProof[
    Inactive[Equal][x*y*z, z*y*x],
    "AbelianGroupAxioms",
    Method         -> "VampirePortfolio",
    TimeConstraint -> 30,
    All
]
```

`PortfolioFrontLoad -> n` widens the slice given to the first `n` entries (each gets 2x the unweighted share) - use it when an `Automatic` front genuinely deserves more time than the fair share.

## Schedule + method introspection

Before launching a portfolio, you may want to know what schedule it expands to and what each entry's full sub-options will be. Two small inspection helpers do exactly that:

```wl
TAtpSchedule["VampirePortfolio"]
```
<!-- => {{"VampireUEQ"}, {"Twee", ...}, {"EProver", ...}, ...} -->

```wl
TAtpSchedule[Automatic,
    Inactive[Equal][x*y, y*x],
    "AbelianGroupAxioms"]
```
<!-- => structure-aware front + the fixed Portfolio tail -->

```wl
TAtpDescribeMethod["Waldmeister"]
```
<!-- => <|"Completion", "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO",
        "AutoPrecedence" -> True, "SelectionRatio" -> 51, ...|> -->

`TAtpDescribeMethod` resolves an arbitrary `Method` spec to the full options Association the C engine will receive. For a list spec like `{"Twee", "Ordering" -> "LPO"}` it merges the preset defaults with the user's overrides, so you see what actually wins. For a portfolio spec it returns `<|"Schedule" -> {...}|>` describing the multi-entry rotation.

## Axiom relevance

Large theories carry axioms that no proof of the current goal needs. [TRelevantAxioms]() reports the filter's keep / drop partition without running the prover - useful to verify a filter setting before committing to a long portfolio:

```wl
TRelevantAxioms[
    Inactive[Equal][x*y, y*x],
    "AbelianGroupAxioms",
    Method -> {"AxiomRelevance" -> "Safe"}]
```
<!-- => <|"Mode" -> "Safe", "Kept" -> {...},
        "Dropped" -> {<|"Axiom" -> _, "Symbols" -> _, "Reason" -> _|>, ...}|> -->

Modes:

- `None` - keep all axioms.
- `"Safe"` (default) - drop only provably dead-weight axioms (a confined symbol occurring on both sides; sound and completeness-preserving). The Y combinator dropped when the goal is Y-free is a typical case.
- `"Connected"` - symbol-reachability pruning. Coarse heuristic; can drop a needed axiom.
- `"SInE"` - the Hoder-Voronkov SInE algorithm as it ships in Vampire (D-relation + bounded BFS from the conjecture's symbols). Defaults `{"SineTolerance" -> 3, "SineDepth" -> 2, "SineGenerality" -> 8}` mirror Vampire's CLI knobs.

---

## TATP - the raw saturator

[TATP]() is the lower-level entry point. Same engine, simpler return shape: an Association with `"Status"`, `"Steps"`, `"Rules"`, `"QueueSize"`. Use it when you want the saturation statistics without the `ProofObject` reconstruction:

```wl
TATP[
    {Inactive[Equal][x*1, x],
     Inactive[Equal][x*inv[x], 1],
     Inactive[Equal][x*(y*z), (x*y)*z]},
    Inactive[Equal][inv[inv[x]], x]
]
```

`TATP[File["path.pr"]]` parses a Waldmeister `.pr` spec via the C-side `wald_parse_file` and runs the saturator directly.

`TATP` also takes a `Witness -> {x_, ...}` opt that captures the existential bindings on success, and an `AllWitnesses -> True` mode that enumerates bounded existential proofs (with `MaxDepth` / `MaxWitnesses` caps). This is what the verification-driven workflows in the [Overview](paclet:WolframInstitute/THVMLink/tutorial/Overview) tutorial use.

---

## Congruence closure (SMT)

For ground equational entailment - all variables substituted, no quantifiers - the unfailing-completion path is overkill. [TFindProofSMT]() reduces ground entailment to a QF_UF satisfiability check via congruence closure:

```wl
TFindProofSMT[
    Inactive[Equal][a, c],
    {Inactive[Equal][a, b], Inactive[Equal][b, c]}]
```
<!-- => ProofObject-shaped Association on UNSAT (entailment holds);
        $Failed on SAT (a counter-model exists) -->

Inputs may be a TPTP source string or `File[path]` - the dispatcher pipes them through `Wolfram``Parser``TPTPImport` and runs the same procedure:

```wl
TFindProofSMT["cnf(g, negated_conjecture, foo(sk) != sk).
               cnf(a, axiom, foo(sk) = sk)."]
```

For SMT-style satisfiability rather than entailment, [TSatEUF]() takes a pair `{equalities, disequalities}` and returns `<|"Status" -> "SAT"|"UNSAT", ...|>`. On `"SAT"` you also get `"Classes"` - the inferred equivalence classes of subterms.

```wl
TSatEUF[{Inactive[Equal][f[a], a]}, {Inactive[Unequal][f[f[a]], a]}]
```
<!-- => <|"Status" -> "UNSAT", "Witness" -> Inactive[Unequal][f[f[a]], a]|> -->

[TSmtDecide]() lifts the theory solver to arbitrary Boolean combinations via lazy DPLL(T) - Tseitin-free atom abstraction + a Wolfram `SatisfiabilityInstances` propositional kernel + congruence closure on each conflict:

```wl
TSmtDecide[
    And[
        Inactive[Equal][a, b],
        Inactive[Equal][b, c],
        Inactive[Unequal][a, c]]]
```
<!-- => <|"Status" -> "UNSAT"|> -->

```wl
TSmtDecide[
    Or[
        Inactive[Equal][f[a], a],
        Inactive[Unequal][g[b], g[b]]]]
```
<!-- => <|"Status" -> "SAT", "Model" -> {...}|> -->

`TSmtDecide` handles `And`, `Or`, `Not`, `Implies`, `Equivalent`, and `Xor` over equality / disequality atoms.

---

## TPTP problem files end to end

The [Wolfram/WolframParser](paclet:Wolfram/WolframParser/guide/WolframParser) paclet's [TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport) parses a `.p` file (or inline `cnf` / `fof` / `tff` / `tcf` / `thf` source) into the shape `TFindProof` expects:

```wl
Needs["Wolfram`Parser`"];
TPTPImport[File["tools/baselines/vampire_tptp/AbelianGroupAxioms__InverseOfInverse.p"]]
```
<!-- => <|"Axioms" -> {Inactive[Equal][...], ...}, "Conjecture" -> Inactive[Equal][...]|> -->

`TFindProof` accepts the parsed Association directly (`TFindProof[parsed]`), the raw inline string (`TFindProof["cnf(...). cnf(...). cnf(goal, negated_conjecture, ...)."]`), or a `File[path]` (`TFindProof[File["..."], TimeConstraint -> 10]` - the dispatcher does the import for you). `negated_conjecture` clauses are flipped through `Not` so the conjecture is the positive goal you'd state in Wolfram-language.

The corpus walker side - browse 26,264 problems across 57 mathematical domains by status / rating / domain, parse on demand - lives as the [TPTPProblemLibrary](paclet:Wolfram/WolframParser/tutorial/TPTPProblemLibrary) tutorial in WolframParser. The same `TPTPImport` is the front door.

---

## Where to go next

- Per-symbol pages: [TFindProof](paclet:WolframInstitute/THVMLink/ref/TFindProof), [TATP](), [TRelevantAxioms](), [TAtpSchedule](), [TAtpDescribeMethod](), [TSatEUF](), [TSmtDecide](), [TFindProofSMT]().
- Parser side: [Parsing TPTP](paclet:Wolfram/WolframParser/tutorial/ParsingTPTP) (how the parser is built from the BNF) and [TPTP Problem Library](paclet:Wolfram/WolframParser/tutorial/TPTPProblemLibrary) (using `TPTPImport` on the full corpus).
- The portfolio + auto-tune source lives in [`wl/THVMLink/Kernel/ATP/ATP.wl`](../../Kernel/ATP/ATP.wl); the SMT module is [`SMT.wl`](../../Kernel/ATP/SMT.wl). The C-side completion engine is under `src/atp/`.
- The algorithmic intent for the completion engine is written up in `docs/plans/waldmeister_ic_atp.md` at the source-tree root.
