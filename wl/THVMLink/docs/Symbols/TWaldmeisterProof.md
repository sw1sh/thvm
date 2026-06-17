---
Template: Symbol
Name: TWaldmeisterProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWaldmeisterProof
Keywords: [Waldmeister, wmcli, ATP, unfailing completion, proof protocol, external prover, CLI, equational, theorem proving]
SeeAlso: [TWaldmeisterProofObject, TVampireProof, TTweeProof, TEproverProof, TSZSDerivationToProofObject, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TWaldmeisterProof]()[*file*]</code> runs the local Waldmeister `wmcli` binary on the Waldmeister `.pr` problem *file*, parses its proof-protocol output, and returns a normalized result Association.

The result Association is keyed `"Status"` (`"Proved"`, `"TimedOut"`, or `"Failed"`), `"Strategy"` (always `"waldmeister-default"`), `"Seconds"` (wall time), `"ProofLength"` (the protocol step count), `"Inferences"` (the parsed proof-protocol derivation), and `"RawProtocol"` (the raw output text).

## Details & Options

- Waldmeister emits its own proof-protocol format (not SZS / TPTP `fof`): numbered `tes-eqn` / `tes-goal` / `tes-rule` / `tes-final` lines whose source field records the inference (`orient(K, x)`, `cp(I, ..., J, ...)`, `tes-red(I, ..., J, ...)`, `initial`, `hypothesis`). [TWaldmeisterProof]() parses these into a structured derivation list shaped for [TSZSDerivationToProofObject]()'s input, mapping each protocol kind to a `thvm` inference role (`cp` to `superposition`, `tes-red` to `forward_demodulation`, `orient` to `orient`, and so on).
- The input must be Waldmeister's `.pr` format, not TPTP. A two-argument `(theory, thm)` form would need a TPTP-to-`.pr` converter and is not supported here; use [TWaldmeisterProofObject]()`[theory, thm]`, which resolves a pre-generated `.pr` file under `tools/baselines/wm_pr/`.
- Options:
  - `TimeConstraint` - wall-clock seconds (default 30).
  - `"Binary"` - an absolute path override; `Automatic` (the default) reads the `$WMCLI` environment variable, then scans `$PATH` for `wmcli`. There is no canonical install location for the `wmcli` build, so point `$WMCLI` at it.
  - `"MathlinkPath"` - the `DYLD_FRAMEWORK_PATH` Waldmeister's MathLink shim needs; `Automatic` resolves it under the current kernel's `$InstallationDirectory`.
- To lift the result into a `thvm`-shaped proof Association (or a literal `ProofObject`), use [TWaldmeisterProofObject](). See the [Method-and-preset](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods) tech note for the `Method -> "WaldmeisterProcess"` dispatch path.

## Basic Examples

Run Waldmeister on a `.pr` problem file and read the status:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TWaldmeisterProof[
    FileNameJoin[{Directory[], "tools", "baselines", "wm_pr",
        "AbelianGroupAxioms__InverseOfInverse.pr"}],
    TimeConstraint -> 10]["Status"]
```
<!-- => "Proved" -->

## Scope

Read the protocol step count straight off the result:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
TWaldmeisterProof[
    FileNameJoin[{Directory[], "tools", "baselines", "wm_pr",
        "AbelianGroupAxioms__InverseOfInverse.pr"}]]["ProofLength"]
```
<!-- => an integer protocol step count -->

## Properties & Relations

- [TWaldmeisterProofObject]() converts the parsed protocol into the same `thvm` proof shape the internal engine produces, and accepts a `(theory, thm)` pair that resolves a pre-generated `.pr` file.
- [TVampireProof](), [TTweeProof](), and [TEproverProof]() are the sibling external-prover wrappers; those consume TPTP `.p` files, whereas Waldmeister consumes its own `.pr` format.

## Possible Issues

- When the `wmcli` binary is absent, [TWaldmeisterProof]() returns `$Failed` and issues `TWaldmeisterProof::nowm`. A missing `.pr` file issues `TWaldmeisterProof::badfile`.
