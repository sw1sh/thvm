---
Template: Symbol
Name: TTweeProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTweeProof
Keywords: [Twee, ATP, TPTP, SZS, TSTP, external prover, CLI, equational, theorem proving]
SeeAlso: [TTweeProofObject, TVampireProof, TEproverProof, TWaldmeisterProof, TSZSDerivationToProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTweeProof]()[*file*]</code> runs the local Twee 2.x binary (with `--tstp --quiet`) on the TPTP (Thousands of Problems for Theorem Provers) problem *file* (a path ending in `.p`) and returns a normalized result Association.

<code>[TTweeProof]()[*theory*, *thm*]</code> resolves the canonical problem file under `tools/baselines/vampire_tptp/{theory}__{thm}.p` from the (*theory*, *thm*) pair and proves it.

The result Association is keyed `"Status"` (`"Proved"`, `"TimedOut"`, or `"Failed"`), `"Strategy"` (always `"twee"`), `"Seconds"` (wall time), `"ProofLength"` (count of Axiom, Lemma, and Goal entries), `"Lemmas"` and `"Axioms"` (each a list of `<|"Name", "Statement"|>` entries), and `"RawProof"` (the proof text after the SZS marker).

## Details & Options

- Twee's `--tstp` output uses SZS framing (status plus Proof start/end markers), but the proof body is Twee's own human-readable `Axiom N / Lemma N / Proof:` equation chain; it carries no per-step inference DAG metadata. [TTweeProof]() parses what is available - the axiom, lemma, and goal lines - into the structured `"Axioms"` / `"Lemmas"` lists and a `"ProofLength"` count.
- For a full inference-DAG match against the internal engine, use [TVampireProof]() or [TEproverProof]() instead; those emit TPTP `fof` clauses with `inference(rule, [], parents)` records.
- Options:
  - `TimeConstraint` - wall-clock seconds (default 30); Twee runs under a `timeout` wrapper of `TimeConstraint + 2`.
  - `"Binary"` - an absolute path override; `Automatic` (the default) walks `~/.cabal/bin`, the Homebrew / `/usr/local` prefixes, then `$PATH`, and uses the first hit.
- To lift the result into a `thvm`-shaped proof Association, use [TTweeProofObject](); because Twee emits no `fof` inference records, that builder constructs the lemma-list shape directly rather than via [TSZSDerivationToProofObject](). See the [Method-and-preset](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods) tech note for the `Method -> "TweeProcess"` dispatch path.

## Basic Examples

Run Twee on a canonical bench problem and read the status:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TTweeProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 10]["Status"]
```
<!-- => "Proved" -->

## Scope

Inspect the lemma list Twee produced:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TTweeProof["AbelianGroupAxioms", "InverseOfInverse"]["Lemmas"]
```
<!-- => {<|"Name" -> "lemma1", "Statement" -> "..."|>, ...} -->

## Properties & Relations

- [TTweeProofObject]() converts this raw result into a `thvm`-shaped proof Association keyed by `{"Axiom", n}` and `{"Lemma", n}` only (no per-step construct-class metadata).
- [TVampireProof](), [TEproverProof](), and [TWaldmeisterProof]() are the sibling external-prover wrappers.

## Possible Issues

- When the Twee binary is absent, [TTweeProof]() returns `$Failed` and issues `TTweeProof::notwee` (`cabal install twee`). A missing problem file issues `TTweeProof::badfile`.
