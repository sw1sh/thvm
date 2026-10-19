---
Template: Symbol
Name: TEproverProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TEproverProofObject
Keywords: [E prover, eprover, theorem proving, ATP, ProofObject, SZS, external prover]
SeeAlso: [TEproverProof, TSZSDerivationToProofObject, TVampireProofObject, TTweeProofObject, TWaldmeisterProofObject, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TEproverProofObject]()["*theory*", "*theorem*"]</code> runs the E prover on a named theorem of an [AxiomaticTheory]() theory, and gives its proof as a [ProofObject]().

<!-- #| annotation: 26.10.19: The E examples are kept unevaluated: TEproverProof passes ToString[N[tc]] ("5.") to eprover --cpu-limit, which E 3.2 rejects, so every run reports "Failed" and this function gives Failure["ExternalNoProof", ...]. With the limit fixed, E's bare-name parent sources (c_0_3 for an inference copied from a clause) still map to Private`constructTypeOf[Missing["NoRule"]] keys in the proof dataset; evaluate the cells again once both are fixed. -->

## Details & Options

- [TEproverProofObject]() runs [TEproverProof]() on the problem file of the theorem, and converts the proof with [TSZSDerivationToProofObject]().
- The steps of the proof are keyed as in the proofs of [TFindProof](): `{"Axiom", n}`, `{"Hypothesis", n}`, `{"CriticalPairLemma", n}`, `{"SubstitutionLemma", n}` and `{"Conclusion", 1}`.
- With `"LiftToProofObject" -> False`, the result is an Association with the keys `"Backend"`, `"Status"`, `"Goal"`, `"Axioms"`, `"ProofDataset"`, `"ProofLength"` and `"RuleHistogram"`, with the formulas as E writes them.
- With the default `"LiftToProofObject" -> True`, the formulas are parsed into expressions and the result is a [ProofObject]().
- When E finds no proof, the result is <code>[Failure]()["ExternalNoProof", …]</code>.
- <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "EproverProcess"]</code> gives the same result.
- The following options can be given:

| | | |
| --- | --- | --- |
| [TimeConstraint]() | `30` | time limit in seconds given to E |
| `"Binary"` | [Automatic]() | path of the E program |
| `"LiftToProofObject"` | [True]() | whether to give a [ProofObject]() |
| `"ParseFormulas"` | [False]() | whether to parse the formulas into expressions |

## Basic Examples

Prove a Boolean identity with E and give the proof as a [ProofObject]():

```wl
#| eval: false
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5]
```

<!-- => a ProofObject of the E proof -->

## Options

### LiftToProofObject

With `"LiftToProofObject" -> False`, the result is an Association of the steps:

```wl
#| eval: false
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]["Backend"]
```

<!-- => "SZS" -->

### ParseFormulas

With `"ParseFormulas" -> True`, the formulas of the steps are parsed into expressions:

```wl
#| eval: false
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False, "ParseFormulas" -> True]["Axioms"]
```

<!-- => the axioms as expressions -->

### TimeConstraint

The time limit is given to E:

```wl
#| eval: false
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 1, "LiftToProofObject" -> False]["Status"]
```

<!-- => "Proved" -->

### Binary

A path with no program gives a [Failure]():

```wl
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "Binary" -> "/nonexistent/eprover"]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Eprover", "Status" -> "Failed", "Seconds" -> 0.01, "Strategy" -> "eprover-auto-schedule"|>] -->

## Properties and Relations

[TVampireProofObject]() gives the proof of the same theorem by Vampire, in the same form:

```wl
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]["RuleHistogram"]
```

<!-- => <|"forward_subsumption_resolution" -> 1|> -->
