---
Template: Symbol
Name: TVampireProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TVampireProof
Keywords: [Vampire, ATP, TPTP, SZS, external prover, CLI, theorem proving, refutation]
SeeAlso: [TVampireProofObject, TTweeProof, TEproverProof, TWaldmeisterProof, TSZSDerivationToProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TVampireProof]()[*file*]</code> runs the local Vampire command-line binary on the TPTP (Thousands of Problems for Theorem Provers) problem *file* (a path ending in `.p`) and returns a normalized result Association.

<code>[TVampireProof]()[*theory*, *thm*]</code> resolves the canonical problem file under `tools/baselines/vampire_tptp/{theory}__{thm}.p` from the (*theory*, *thm*) pair (the bench-harness convention) and proves it.

The result Association is keyed `"Status"` (`"Proved"`, `"TimedOut"`, or `"Failed"`), `"Strategy"` (the winning strategy line), `"Seconds"` (wall time), `"ProofLength"` (the SZS derivation step count), `"Inferences"` (the parsed SZS inference DAG), and `"RawSZS"` (the raw SZS output text).

## Details & Options

- [TVampireProof]() shells out to the Vampire 5.0.1 CLI with `--mode casc --proof tptp`, then parses the SZS output through `Wolfram`Parser`TPTPImport[..., "SZS"]` (from the `Wolfram/WolframParser` paclet, which `Needs` auto-loads). It is the validation surface for checking a `thvm` preset's behavior against the actual Vampire proof on the same problem.
- Parse-time `reorient_equations` entries are preprocessing rewrites, not saturation steps, so they are folded into their parents before `"ProofLength"` and `"Inferences"` are reported.
- In `casc` portfolio mode the winning strategy writes its proof to a separate solution file; [TVampireProof]() reads that file when present and falls back to the inline proof body of single-strategy modes.
- For an unsolved problem `"Status"` is `"TimedOut"` or `"Failed"` and the inference fields hold defaults (`"ProofLength" -> 0`, `"Inferences" -> {}`). When Vampire reports a proof that `TPTPImport` cannot parse, the `TVampireProof::badtptp` message fires rather than masquerading as a zero-length proof.
- Options:
  - `TimeConstraint` - wall-clock seconds passed to Vampire's `-t` (default 30).
  - `"Mode"` - the Vampire mode (default `"casc"`).
  - `"Binary"` - an absolute path to the binary; `Automatic` (the default) walks the Homebrew / `/usr/local` prefixes then `$PATH` and uses the first hit. A truly-absent binary triggers the `TVampireProof::novamp` message.
- To lift the result into a `thvm`-shaped proof Association (or a literal `ProofObject`) for structural comparison, use [TVampireProofObject](), which threads this result through [TSZSDerivationToProofObject](). See the [Method-and-preset](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods) tech note for the `Method -> "VampireProcess"` dispatch path.

## Basic Examples

Run Vampire on a canonical bench problem resolved from a (theory, theorem) pair:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TVampireProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 10]
```
<!-- => <|"Status" -> "Proved", "Strategy" -> "lrs+10_1:1_...", "Seconds" -> _, "ProofLength" -> _, "Inferences" -> {...}, "RawSZS" -> "..."|> -->

## Scope

Read the proof length straight off the result:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TVampireProof["AbelianGroupAxioms", "InverseOfInverse"]["ProofLength"]
```
<!-- => an integer SZS derivation step count -->

## Properties & Relations

- [TVampireProofObject]() converts this raw result into the same `thvm` proof shape the internal engine produces, so a comparator sees one structure across the internal preset and the external Vampire CLI.
- [TTweeProof](), [TEproverProof](), and [TWaldmeisterProof]() are the sibling external-prover wrappers. Vampire and E emit a full per-step inference DAG; Twee emits only a human-readable equation chain.

## Possible Issues

- When the Vampire binary is not on `PATH`, [TVampireProof]() returns `$Failed` and issues `TVampireProof::novamp` (`brew install vampire`). A missing problem file issues `TVampireProof::badfile`.
