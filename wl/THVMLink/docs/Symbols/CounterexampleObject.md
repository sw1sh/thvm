---
Template: Symbol
Name: CounterexampleObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/CounterexampleObject
Keywords: [counterexample, countermodel, disproof, refutation, finite model, congruence closure, saturation, equational]
SeeAlso: [TFindProof, FindFiniteModels, TSatEUF, TSmtDecide]
RelatedGuides: [THVMLink]
---

## Usage

<code>[CounterexampleObject]()[*method*, *proposition*, *axioms*, *data*]</code> is the equational dual of `ProofObject` - the disproof artifact <code>[TFindProof]()[*conjecture*, *axioms*, "Counterexample"]</code> returns when the goal is refutable, mirroring the Wolfram Function Repository's `FindEquationalCounterexample` result.

It renders as a summary box and supports a property interface `co[*prop*]`.

## Details & Options

- The property interface answers `"Method"`, `"Proposition"` / `"Goal"`, `"Axioms"` / `"Hypotheses"`, `"Setup"` / `"Model"` (the refuting model), `"Counterexample"` / `"Witness"` (the falsifying assignment), `"NormalForms"`, `"Domain"`, `"FalsificationFunction"` (a nullary function returning `False` in the model), `"VerificationFunction"` (returns `True` since the axioms hold), `"Data"`, and `"Properties"`.
- For a finite model (a ground congruence-closure quotient) `"Model"` follows the [FindFiniteModels]() structure: a Cayley table per operator and an element per constant over the domain `{0, ..., k - 1}`. For an infinite initial term algebra it carries the convergent rules, and `"NormalForms"` the two normal forms separating the goal's sides.
- [TFindProof]() picks the refuting engine by problem shape: congruence closure ([TSatEUF]()) for a ground goal, the saturating completion engine for a quantified one. It declines (returns `$Failed`) on an associative-commutative theory whose unorientable equations would need ordered rewriting. See the [Disproof](paclet:WolframInstitute/THVMLink/tutorial/Disproof) tech note.

## Basic Examples

Refute a non-theorem and read its falsification function:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
co = TFindProof[g[a] == a, {ForAll[{x}, g[g[x]] == x]}, "Counterexample"];
co["FalsificationFunction"]
```
<!-- => Function[{}, g[a] == a /. {g -> ({2, 1}[[##]]&), a -> 1}] -->

## Scope

The refuting model in [FindFiniteModels]() form:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TFindProof[a == c, {a == b}, "Counterexample"]["Model"]
```
<!-- => <|a -> 0, b -> 0, c -> 1, ...|> (operators to Cayley tables, constants to elements) -->

## Properties & Relations

- [TFindProof]() with the `"Counterexample"` output kind produces this object; `Method -> "SMT"` returns a decision Association on a proved entailment and the same `CounterexampleObject` on a refuted one.
- [FindFiniteModels]() and the Wolfram Function Repository's `FindEquationalCounterexample` are the standalone finite-model finders this object's model is shaped after; [TSatEUF]() / [TSmtDecide]() are the ground decision procedures behind the congruence-closure path.

## Possible Issues

- The model is self-certifying: `co["FalsificationFunction"][]` evaluates the goal to `False` and `co["VerificationFunction"][]` the axioms to `True`. The operator table is 1-indexed inside those functions (so `Part` lines up), versus the 0-indexed `co["Model"]`.
