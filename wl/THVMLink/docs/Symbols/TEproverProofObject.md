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

## Details & Options

- [TEproverProofObject]() runs [TEproverProof]() on the problem file of the theorem, and converts the proof with [TSZSDerivationToProofObject]().
- The steps of the proof are keyed as in the proofs of [TFindProof](): `{"Axiom", n}`, `{"Hypothesis", n}`, `{"CriticalPairLemma", n}`, `{"SubstitutionLemma", n}` and `{"Conclusion", 1}`.
- With `"LiftToProofObject" -> False`, the result is an Association with the keys `"Backend"`, `"Status"`, `"Goal"`, `"Axioms"`, `"ProofDataset"`, `"ProofLength"` and `"RuleHistogram"`, with the formulas in the symbols of the TPTP problem, such as `"and"[X1_, X2_]`.
- With the default `"LiftToProofObject" -> True`, the formulas are parsed into expressions and the result is a [ProofObject]().
- When E finds no proof, the result is <code>[Failure]()["ExternalNoProof", …]</code>.
- A theorem with no problem file issues the message `TEproverProof::badfile` and gives `$Failed`.
- <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "EproverProcess"]</code> gives the Association of `"LiftToProofObject" -> False`, and the [ProofObject]() when it is given `"LiftToProofObject" -> True`.
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
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

## Scope

With `"LiftToProofObject" -> False`, the result is an Association of the steps:

```wl
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]
```

<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> "skc1"[] != "and"["skc1"[], "or"["skc2"[], "not"["skc2"[]]]], "Axioms" -> {"and"[X1_, "or"[X2_, "not"[X2_]]] == X1_}, "ProofDataset" -> <|{"Hypothesis", 1} -> ..., {"Axiom", 1} -> ..., {"Conclusion", 1} -> ...|>, "ProofLength" -> 5, "RuleHistogram" -> <|"cn" -> 1, "equation_copy" -> 3|>|> -->

---

The proof of a group theorem has a critical-pair lemma and two substitution lemmas:

```wl
Keys[TEproverProofObject["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 5, "LiftToProofObject" -> False]["ProofDataset"]]
```

<!-- => {{"Axiom", 1}, {"Axiom", 2}, {"Axiom", 3}, {"Axiom", 4}, {"Hypothesis", 1}, {"CriticalPairLemma", 1}, {"SubstitutionLemma", 1}, {"SubstitutionLemma", 2}, {"Conclusion", 1}} -->

## Options

### LiftToProofObject

With `"LiftToProofObject" -> False`, the result is an Association of the steps:

```wl
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]["Backend"]
```

<!-- => "SZS" -->

### ParseFormulas

With `"ParseFormulas" -> True`, the formulas of the steps are parsed into expressions:

```wl
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False, "ParseFormulas" -> True]["Axioms"]
```

<!-- => {X1_ ⊗ (X2_ ⊕ OverBar[X2_]) == X1_} -->

### TimeConstraint

The time limit is given to E, in whole seconds:

```wl
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 1, "LiftToProofObject" -> False]["Status"]
```

<!-- => "Proved" -->

### Binary

A path with no program gives a [Failure]():

```wl
TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "Binary" -> "/nonexistent/eprover"]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Eprover", "Status" -> "Failed", "Seconds" -> a time, "Strategy" -> "eprover-auto-schedule"|>] -->

## Properties and Relations

[TFindProof]() with the `"EproverProcess"` method gives the Association of `"LiftToProofObject" -> False`:

```wl
TFindProof["DroppingAlwaysTrue", "BooleanAxioms", Method -> "EproverProcess", TimeConstraint -> 5] === TEproverProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]
```

<!-- => True -->

---

[TVampireProofObject]() gives the proof of the same theorem by Vampire, in the same form:

```wl
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]["RuleHistogram"]
```

<!-- => <|"forward_subsumption_resolution" -> 1|> -->

## Possible Issues

A theorem with no problem file issues a message and gives `$Failed`:

```wl
TEproverProofObject["GroupAxioms", "NoSuchTheorem", TimeConstraint -> 5]
```

<!-- => the message TEproverProof::badfile is issued and the result is $Failed -->
