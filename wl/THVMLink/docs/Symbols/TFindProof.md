---
Template: Symbol
Name: TFindProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindProof
Keywords: [theorem proving, ATP, equational, completion, Knuth-Bendix, Waldmeister, Vampire]
SeeAlso: [TRelevantAxioms, TAtpSchedule, TAtpDescribeMethod, TATP, TProofObject, TFindEquationalPath, TFindStringProof, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindProof]()[$conjecture$, $axioms$]</code> runs `thvm`'s C ATP completion engine and returns a Wolfram `ProofObject` - the same head [FindEquationalProof]() returns, supporting `p["ProofDataset"]`, `p["ProofGraph"]`, `p["ProofFunction"]`, `p["ProofLength"]`.

<code>[TFindProof]()["$Theorem$", "$Theory$"]</code> resolves names through [AxiomaticTheory]().

<code>[TFindProof]()[$conjecture$, "$Theory$"]</code> proves $conjecture$ against the axioms of the named theory.

<code>[TFindProof]()[$axioms$]</code> or <code>[TFindProof]()["$Theory$"]</code> runs a completion with no goal and returns a `ProofObject` whose `Theorems` is `None`; bound it with [TimeConstraint]() since a non-terminating axiom set never saturates.

## Details & Options

- The C engine saturates the axioms via unfailing Knuth-Bendix completion; the resulting equational rewrite chain is decoded into a verifier-shaped `ProofObject`. Returns `$Failed` when the conjecture is not proved.
- An optional positional argument (after the conjecture / axioms, before any options) selects what is returned, drawn from `{"ProofObject", "Lemmas", "PreprocessedAxioms", "RelevantAxioms", "RawTrace", "Statistics", "Status", "Path", "AppliedMethod", "WallTime", "PortfolioTrace", "Counterexample", "TPTP"}` (a single String returns that bare, a list returns an Association, `All` returns every spec). `"TPTP"` renders the proof as an SZS-wrapped TPTP `CNFRefutation` string - the CASC output format.
- Axioms may be written as equations (`a == b`, a two-element list, or [TwoWayRule]() `a <-> b`) or as one-sided [Rule]()s (`a -> b`). A `Rule` axiom is installed *pre-oriented* - the engine rewrites with it left-to-right only, so the rule set stays a directed term-rewriting system. The `"Path"` spec returns the witnessing rewrite chain; with one-sided axioms it is a forward replacement path. See [TFindEquationalPath]() and the [Finding Replacement Paths](paclet:WolframInstitute/THVMLink/tutorial/FindReplacePath) tech note; [TFindStringProof]() specializes the whole surface to string rewriting.
- `"Counterexample"` is the equational dual of `"ProofObject"` - a disproof rather than a proof, returned as a [CounterexampleObject]() (the summary-boxed object that mirrors the Wolfram Function Repository's `FindEquationalCounterexample`), or `$Failed` when there is no extractable countermodel. The engine is chosen by the problem shape:
  - A fully **ground** problem is decided by congruence closure (a complete decision procedure); the quotient is returned as a finite model in [FindFiniteModels]() structure - `co["Model"]` is an Association `op -> Cayley table` (0-indexed nested list) for each operator and `const -> element` for each constant, over the domain `{0, ..., k-1}`.
  - A **quantified** problem is refuted by the saturated completion: when the completion saturates into a convergent term-rewriting system (Status `"Saturated"`, no unorientable equations) whose normal forms separate the goal's two sides, `co["NormalForms"]` are those normal forms and `co["Model"]` is a finite model in FindFiniteModels structure when the initial term algebra closes (else the convergent rules). It declines (`$Failed`) on a commutative/AC-saturated theory whose unorientable equations would need ordered rewriting.
- For a ground problem, `Method -> "SMT"` decides the entailment by congruence closure directly (returning a `"Proved"` decision Association, or a `CounterexampleObject` on refute) and also accepts a TPTP `File` / `cnf`-`fof` string. Mirrors how [FindEquationalProof]() itself returns a countermodel-bearing `Failure` on a non-theorem.
- **First-order / predicate-logic input** (predicate atoms, logical connectives `And` / `Or` / `Not` / `Implies` / `Equivalent` / `Xor` / `Nand` / `Nor`, and `ForAll` / `Exists` quantifiers) is proved by automatic *equationalization*: each proposition is Skolemized and encoded as `<boolean-form> == or[a, not[a]]` over a boolean-algebra base, then the resulting equational problem is discharged by the same completion engine - the exact construction [FindEquationalProof]() uses. The equational proof is then reconstructed into a `"Predicate/EquationalLogic"` `ProofObject` whose `"Theorems"` and `"Axioms"` are the *original* predicate statements and whose steps read in predicate form (`Inactive[And]` / `Inactive[Or]` / `Inactive[Not]`), matching what [FindEquationalProof]() returns; it verifies via `p["ProofFunction"][p["Theorems"]]`. The documented predicate examples (the Socrates syllogism, existential quantification, Lewis Carroll's crocodile puzzle, Knights and Knaves) all prove and verify this way. A purely ground Boolean problem still routes to the `"SMT"` decider above; only genuinely quantified / predicate input is equationalized.
- `Method -> "WaldmeisterProcess"` discharges the equationalized problem with the actual Waldmeister binary (`wmcli`, the same engine [FindEquationalProof]() drives) instead of thvm's completion engine, then reconstructs the same predicate `ProofObject`. It verifies on the simpler examples (the Socrates syllogism, existential quantification); reconstructing a *verifiable* proof from the binary's SZS derivation is bounded by the critical-pair-lemma metadata reconstruction, so on more complex proofs the returned `ProofObject` may not verify (use the default engine there). The binary resolves through `$WMCLI`, `$PATH`, then the common dev-build locations under the home directory; `"VampireProcess"` / `"TweeProcess"` / `"EproverProcess"` dispatch the sibling external-prover builders the same way (see [TVampireProofObject]()).
- Options:
  - `MaxSteps` - CP-processing cap (default `Automatic`: 200000, raised to 500000 under the `"Waldmeister"`/`"WaldmeisterLazy"` presets so the deep `"WolframAxioms"` proofs fit; an explicit number always wins).
  - `TimeConstraint` - wall-clock seconds (default `Infinity`; bounds non-terminating recursive-axiom saturations; `TimeConstrained[]` and `Abort[]` also interrupt the running C engine).
  - `Method` - `Automatic`, a single explicit config, a named preset (`"Waldmeister"`, `"VampireUEQ"`, `"Twee"`, `"EProver"`, `"VampireRandom"`, `"ENIGMA"`), or a schedule name (`"Portfolio"`, `"VampirePortfolio"`, `"VampirePortfolioCompact"`, `"AllPresets"`).
  - `PortfolioFrontLoad` - how many leading schedule entries get a widened share of `TimeConstraint` (default 1).
  - `"ProofForm"` - `"ProofObject"` (default) returns the built-in `ProofObject`; `"TProofObject"` returns the native [TProofObject]().
  - `"Binary"`, `"ParseFormulas"`, `"LiftToProofObject"` - forwarded to the `*Process` external-prover builders; ignored by the internal engine.
- `Method` exposes the CP-selection heuristic, reduction ordering, Waldmeister structure-driven precedence, axiom relevance, CP redundancy, interreduction, and queue fairness, plus the Twee-ported joins (`"SimplerJoin"`, `"EncompassDemod"`, `"PopRulesOnly"`, `"GjDeleteRule"`, `"FullInterreduce"`, `"CPSetIRBudget"`) and the goal transforms (`"GoalNarrow"`, `"FlattenGoal"`). See <code>[TAtpDescribeMethod]()</code> for the full sub-option surface.
- `Automatic` is problem-aware: it analyzes the conjunction's algebraic structure (Group / AbelianGroup / Ring / Combinatory / AC / Sheffer-Nand) and front-loads a tailored config, then appends the fixed `"Portfolio"` as a fallback tail. It never proves less than `"Portfolio"`.

## Basic Examples

Prove the double-inverse identity from the abelian-group axioms:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]
```
<!-- => ProofObject[<|"Theorem" -> ..., "Status" -> "Proved", ...|>] - the standard summary box -->

## Scope

Single-argument completion saturates the axioms with no goal; the `"Lemmas"` spec returns the derived rule set:

```wl
TFindProof[ "AbelianGroupAxioms", "Lemmas", TimeConstraint -> 5 ]
```
<!-- => {Inactive[Equal][...], ...} - the lemmas derived within 5s -->

---

Predicate-logic input is equationalized automatically, so the Socrates syllogism proves the same way an equation does:

```wl
TFindProof[
    mortal[socrates],
    {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]},
    TimeConstraint -> 60
]
```
<!-- => ProofObject[<|"Status" -> "Proved", ...|>] - the same head FindEquationalProof returns -->

---

Name a theorem and pin a Method preset: the Waldmeister preset proves Sheffer-stroke commutativity directly, without the portfolio rotation:

```wl
TFindProof[
    "Commutativity",
    "ShefferAxioms",
    Method         -> "Waldmeister",
    TimeConstraint -> 60
]
```
<!-- => ProofObject[<|"Status" -> "Proved", ...|>] -->

## Applications

Inspect the relevance filter's keep / drop partition before running the prover:

```wl
TRelevantAxioms[
    Inactive[Equal][x + y, y + x],  (* Plus is Orderless: a bare == would evaluate to True *)
    "AbelianGroupAxioms",
    Method -> {"AxiomRelevance" -> "Safe"}
]
```
<!-- => <|"Kept" -> {...}, "Dropped" -> {<|"Axiom" -> _, "Symbols" -> _, "Reason" -> _|>, ...}, "Mode" -> "Safe"|> -->

---

Preview the schedule a `Method` will expand to under `Automatic`:

```wl
TAtpSchedule[Automatic, a \[CircleTimes] b == b \[CircleTimes] a, "AbelianGroupAxioms"]
```
<!-- => {<config1>, <config2>, ...} - the structure-aware front + the fixed Portfolio tail -->

## Properties and Relations

`TFindProof` returns a real `ProofObject`, so the standard property API works:

```wl
p = TFindProof[
    x \[CircleTimes] OverTilde[1] == x,
    "AbelianGroupAxioms",
    TimeConstraint -> 10
];
p["ProofGraph"]
```
<!-- => Graph[...] - the equational rewrite DAG -->


## Possible Issues

A non-terminating axiom set never saturates.  Bound completion runs with `TimeConstraint`; the rewrite expands forever (`f` rewrites to `f[f]`, then `f[f[f]]`, ...) so the prover times out instead of finding the goal:

```wl
TFindProof[
    a == b,
    {f[x_] == f[f[x_]]},
    TimeConstraint -> 2
]
```
<!-- => $Failed within 2s (engine saturated against the timeout) -->

---

`Automatic` is the safer default; an explicit config can prove LESS than a portfolio because it discards the fallback rotation.  The full `Automatic` schedule proves this commutativity goal within a few seconds, but its own first entry, run alone, exhausts the entire budget:

```wl
TFindProof[
    x \[CircleTimes] y == y \[CircleTimes] x,
    "AbelianGroupAxioms",
    Method         -> First @ TAtpSchedule[Automatic,
        x \[CircleTimes] y == y \[CircleTimes] x, "AbelianGroupAxioms"],
    TimeConstraint -> 10
]
```
<!-- => $Failed - the same goal under Method -> Automatic proves once the rotation moves past this entry -->

## Neat Examples

Run the full multi-config portfolio with a fair time slice and report every return spec side by side (the spec argument precedes the options; `All` returns a per-spec Association, projected here onto its keys):

```wl
Keys @ TFindProof[
    x \[CircleTimes] y == y \[CircleTimes] x,
    "AbelianGroupAxioms",
    All,
    Method         -> "VampirePortfolio",
    TimeConstraint -> 10
]
```
<!-- => {ProofObject, Lemmas, PreprocessedAxioms, RelevantAxioms, RawTrace, Statistics, Status, Path, AppliedMethod, WallTime, PortfolioTrace, Counterexample, TPTP} -->
