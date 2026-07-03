---
Template: Symbol
Name: TFindProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindProof
Keywords: [theorem proving, ATP, equational, completion, Knuth-Bendix, Waldmeister, Vampire]
SeeAlso: [TRelevantAxioms, TAtpSchedule, TAtpDescribeMethod, TATP, TFindEquationalPath, TFindStringProof, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindProof]()[$conjecture$, $axioms$]</code> runs `thvm`'s C ATP completion engine and returns a Wolfram `ProofObject` - the same head [FindEquationalProof]() returns, supporting `p["ProofDataset"]`, `p["ProofGraph"]`, `p["ProofFunction"]`, `p["ProofLength"]`.

<code>[TFindProof]()["$Theorem$", "$Theory$"]</code> resolves names through [AxiomaticTheory]().

<code>[TFindProof]()[$conjecture$, "$Theory$"]</code> proves $conjecture$ against the axioms of the named theory.

<code>[TFindProof]()[$axioms$]</code> or <code>[TFindProof]()["$Theory$"]</code> runs a time-constrained completion with no goal and returns the derived lemmas.

## Details & Options

- The C engine saturates the axioms via unfailing Knuth-Bendix completion; the resulting equational rewrite chain is decoded into a verifier-shaped `ProofObject`. Returns `$Failed` when the conjecture is not proved.
- An optional trailing positional argument selects what is returned, drawn from `{"ProofObject", "Lemmas", "PreprocessedAxioms", "RelevantAxioms", "RawTrace", "Statistics", "Status", "Path", "Counterexample"}` (a single String returns that bare, a list returns an Association, `All` returns every spec).
- Axioms may be written as equations (`a == b`, a two-element list, or [TwoWayRule]() `a <-> b`) or as one-sided [Rule]()s (`a -> b`). A `Rule` axiom is installed *pre-oriented* - the engine rewrites with it left-to-right only, so the rule set stays a directed term-rewriting system. The `"Path"` spec returns the witnessing rewrite chain; with one-sided axioms it is a forward replacement path. See [TFindEquationalPath]() and the [Finding Replacement Paths](paclet:WolframInstitute/THVMLink/tutorial/FindReplacePath) tech note; [TFindStringProof]() specializes the whole surface to string rewriting.
- `"Counterexample"` is the equational dual of `"ProofObject"` - a disproof rather than a proof, returned as a [CounterexampleObject]() (the summary-boxed object that mirrors the Wolfram Function Repository's `FindEquationalCounterexample`), or `$Failed` when there is no extractable countermodel. The engine is chosen by the problem shape:
  - A fully **ground** problem is decided by congruence closure (a complete decision procedure); the quotient is returned as a finite model in [FindFiniteModels]() structure - `co["Model"]` is an Association `op -> Cayley table` (0-indexed nested list) for each operator and `const -> element` for each constant, over the domain `{0, ..., k-1}`.
  - A **quantified** problem is refuted by the saturated completion: when the completion saturates into a convergent term-rewriting system (Status `"Saturated"`, no unorientable equations) whose normal forms separate the goal's two sides, `co["NormalForms"]` are those normal forms and `co["Model"]` is a finite model in FindFiniteModels structure when the initial term algebra closes (else the convergent rules). It declines (`$Failed`) on a commutative/AC-saturated theory whose unorientable equations would need ordered rewriting.
- For a ground problem, `Method -> "SMT"` decides the entailment by congruence closure directly (returning a `"Proved"` decision Association, or a `CounterexampleObject` on refute) and also accepts a TPTP `File` / `cnf`-`fof` string. Mirrors how [FindEquationalProof]() itself returns a countermodel-bearing `Failure` on a non-theorem.
- Options:
  - `MaxSteps` - CP-processing cap (default `Automatic`: 200000, raised to 500000 under the `"Waldmeister"`/`"WaldmeisterLazy"` presets so the deep `"WolframAxioms"` proofs fit; an explicit number always wins).
  - `TimeConstraint` - wall-clock seconds (default `Infinity`; bounds non-terminating recursive-axiom saturations; `TimeConstrained[]` and `Abort[]` also interrupt the running C engine).
  - `Method` - `Automatic`, `"Portfolio"`, a single explicit config Association, or a preset (`"Waldmeister"`, `"VampireUEQ"`, `"Twee"`, `"EProver"`, `"VampirePortfolio"`, `"VampirePortfolioCompact"`).
- `Method` exposes the CP-selection heuristic, reduction ordering, Waldmeister structure-driven precedence, axiom relevance, CP redundancy, interreduction, and queue fairness. See <code>[TAtpDescribeMethod]()</code> for the full sub-option surface.
- `Automatic` is problem-aware: it analyzes the conjunction's algebraic structure (Group / AbelianGroup / Ring / Combinatory / AC / Sheffer-Nand) and front-loads a tailored config, then appends the fixed `"Portfolio"` as a fallback tail. It never proves less than `"Portfolio"`.

## Basic Examples

Prove commutativity from the abelian-group axioms:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindProof[
    Inactive[Equal][x \[CircleTimes] y \[CircleTimes] z, z \[CircleTimes] y \[CircleTimes] x],
    "AbelianGroupAxioms",
    TimeConstraint -> 10
]
```
<!-- => ProofObject[<|"Theorem" -> ..., "Status" -> "Proved", "Method" -> "Equational", ...|>] -->

## Scope

Single-argument completion saturates the axioms and returns the derived lemmas:

```wl
TFindProof[ "AbelianGroupAxioms", TimeConstraint -> 5 ]
```
<!-- => {Inactive[Equal][...], ...} - the saturated rule set within 5s -->

---

Mix an explicit conjecture against a named theory plus a Method preset.  Sheffer is hard enough that the portfolio's Waldmeister preset typically wins; run interactively to inspect the result:

```wl
#| eval: False
TFindProof[
    Inactive[Equal][nand[nand[a, b], nand[a, b]], nand[nand[a, a], nand[b, b]]],
    "ShefferAxioms",
    Method         -> "Waldmeister",
    TimeConstraint -> 60
]
```
<!-- => ProofObject[...] -->

## Applications

Inspect the relevance filter's keep / drop partition before running the prover:

```wl
TRelevantAxioms[
    Inactive[Equal][x + y, y + x],
    "AbelianGroupAxioms",
    Method -> {"AxiomRelevance" -> "Safe"}
]
```
<!-- => <|"Mode" -> "Safe", "Kept" -> {...}, "Dropped" -> {<|"Axiom" -> _, "Symbols" -> _, "Reason" -> _|>, ...}|> -->

---

Preview the schedule a `Method` will expand to under `Automatic`:

```wl
TAtpSchedule[Automatic, Inactive[Equal][a \[CircleTimes] b, b \[CircleTimes] a], "AbelianGroupAxioms"]
```
<!-- => {<config1>, <config2>, ...} - the structure-aware front + the fixed Portfolio tail -->

## Properties and Relations

`TFindProof` returns a real `ProofObject`, so the standard property API works:

```wl
p = TFindProof[
    Inactive[Equal][x \[CircleTimes] OverTilde[1], x],
    "AbelianGroupAxioms",
    TimeConstraint -> 10
];
p["ProofGraph"]
```
<!-- => Graph[...] - the equational rewrite DAG -->

---

`TFindEquationalProof` is a deprecated alias that forwards to `TFindProof`; new code should use `TFindProof`.

## Possible Issues

A non-terminating axiom set never saturates.  Bound completion runs with `TimeConstraint`; the rewrite expands forever (`f` rewrites to `f[f]`, then `f[f[f]]`, ...) so the prover times out instead of finding the goal:

```wl
TFindProof[
    Inactive[Equal][a, b],
    {Inactive[Equal][f[x_], f[f[x_]]]},
    TimeConstraint -> 2
]
```
<!-- => $Failed within 2s (engine saturated against the timeout) -->

---

`Automatic` is the safer default; an explicit config can prove LESS than `"Portfolio"` because it discards the fallback rotation:

```wl
TFindProof[
    Inactive[Equal][x, x],
    "AbelianGroupAxioms",
    Method         -> {"Completion", "CriticalPairWeight" -> "Add", "Ordering" -> "LPO"},
    TimeConstraint -> 10
]
```
<!-- => may $Failed on problems the portfolio would crack -->

## Neat Examples

Run the full multi-config portfolio with a fair time slice and report status, statistics, and the proof side by side:

```wl
#| eval: False
TFindProof[
    Inactive[Equal][x \[CircleTimes] y \[CircleTimes] z, z \[CircleTimes] y \[CircleTimes] x],
    "AbelianGroupAxioms",
    Method         -> "VampirePortfolio",
    TimeConstraint -> 30,
    All
]
```
<!-- => <|"ProofObject" -> ProofObject[...], "Status" -> "Proved", "Lemmas" -> {...}, "Statistics" -> <|...|>, ...|> -->
