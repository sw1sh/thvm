---
Template: Symbol
Name: TFindProof
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindProof
Keywords: [theorem proving, ATP, equational, completion, Knuth-Bendix, Waldmeister, Vampire]
SeeAlso: [TRelevantAxioms, TAtpSchedule, TAtpDescribeMethod, TATP, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindProof]()[$conjecture$, $axioms$]</code> runs `thvm`'s C ATP completion engine and returns a Wolfram `ProofObject` - the same head [FindEquationalProof]() returns, supporting `p["ProofDataset"]`, `p["ProofGraph"]`, `p["ProofFunction"]`, `p["ProofLength"]`.

<code>[TFindProof]()["$Theorem$", "$Theory$"]</code> resolves names through [AxiomaticTheory]().

<code>[TFindProof]()[$conjecture$, "$Theory$"]</code> proves $conjecture$ against the axioms of the named theory.

<code>[TFindProof]()[$axioms$]</code> or <code>[TFindProof]()["$Theory$"]</code> runs a time-constrained completion with no goal and returns the derived lemmas.

## Details & Options

- The C engine saturates the axioms via unfailing Knuth-Bendix completion; the resulting equational rewrite chain is decoded into a verifier-shaped `ProofObject`. Returns `$Failed` when the conjecture is not proved.
- An optional trailing positional argument selects what is returned, drawn from `{"ProofObject", "Lemmas", "PreprocessedAxioms", "RelevantAxioms", "RawTrace", "Statistics", "Status"}` (a single String returns that bare, a list returns an Association, `All` returns every spec).
- Options:
  - `MaxSteps` - CP-processing cap (default 200000).
  - `TimeConstraint` - wall-clock seconds (default `Infinity`; bounds non-terminating recursive-axiom saturations; `TimeConstrained[]` and `Abort[]` also interrupt the running C engine).
  - `Method` - `Automatic`, `"Portfolio"`, a single explicit config Association, or a preset (`"Waldmeister"`, `"VampireUEQ"`, `"Twee"`, `"EProver"`, `"VampirePortfolio"`, `"VampirePortfolioCompact"`).
- `Method` exposes the CP-selection heuristic, reduction ordering, Waldmeister structure-driven precedence, axiom relevance, CP redundancy, interreduction, and queue fairness. See <code>[TAtpDescribeMethod]()</code> for the full sub-option surface.
- `Automatic` is problem-aware: it analyzes the conjunction's algebraic structure (Group / AbelianGroup / Ring / Combinatory / AC / Sheffer-Nand) and front-loads a tailored config, then appends the fixed `"Portfolio"` as a fallback tail. It never proves less than `"Portfolio"`.

## Basic Examples

Prove commutativity from the abelian-group axioms:

```wl
Needs["THVMLink`ATP`"];
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

A non-terminating axiom set never saturates. Bound completion runs with `TimeConstraint`:

```wl
TFindProof[ {Inactive[Equal][S[x_][y_][z_], x[z][y[z]]]}, TimeConstraint -> 2 ]
```
<!-- => {...partial lemmas within 2s...} -->

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
