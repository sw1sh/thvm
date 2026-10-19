---
Template: Symbol
Name: TVampireProofObject
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TVampireProofObject
Keywords: [Vampire, theorem proving, ATP, ProofObject, SZS, external prover]
SeeAlso: [TVampireProof, TSZSDerivationToProofObject, TTweeProofObject, TEproverProofObject, TWaldmeisterProofObject, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TVampireProofObject]()["*theory*", "*theorem*"]</code> runs the Vampire prover on a named theorem of an [AxiomaticTheory]() theory, and gives its proof as a [ProofObject]().

## Details & Options

- [TVampireProofObject]() runs [TVampireProof]() on the problem file of the theorem, and converts the proof with [TSZSDerivationToProofObject]().
- The steps of the proof are keyed as in the proofs of [TFindProof](): `{"Axiom", n}`, `{"Hypothesis", n}`, `{"CriticalPairLemma", n}`, `{"SubstitutionLemma", n}` and `{"Conclusion", 1}`.
- With `"LiftToProofObject" -> False`, the result is an Association with the keys `"Backend"`, `"Status"`, `"Goal"`, `"Axioms"`, `"ProofDataset"`, `"ProofLength"` and `"RuleHistogram"`, with the formulas as Vampire writes them.
- With the default `"LiftToProofObject" -> True`, the formulas are parsed into expressions and the result is a [ProofObject](). Parsing takes a few seconds for each formula.
- When Vampire finds no proof, the result is <code>[Failure]()["ExternalNoProof", …]</code>.
- The steps of a Vampire proof do not record the positions and orientations of the rewrites, so the `"ProofFunction"` of the [ProofObject]() cannot check them.
- <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "VampireProcess"]</code> gives the same result.
- The following options can be given:

| | | |
| --- | --- | --- |
| [TimeConstraint]() | `30` | time limit in seconds given to Vampire |
| `"Mode"` | `"casc"` | the mode Vampire runs in |
| `"Binary"` | [Automatic]() | path of the Vampire program |
| `"LiftToProofObject"` | [True]() | whether to give a [ProofObject]() |
| `"ParseFormulas"` | [False]() | whether to parse the formulas into expressions |

## Basic Examples

Prove a Boolean identity with Vampire and give the proof as a [ProofObject]():

```wl
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

With `"LiftToProofObject" -> False`, the result is an Association of the steps, with the formulas as Vampire writes them:

```wl
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]
```

<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> "skc1"[] != "and"["skc1"[], "or"["skc2"[], "not"["skc2"[]]]], "Axioms" -> {"(\n  ( ! [X0,X1] : (and(X0,or(X1,not(X1))) = X0) ))"}, "ProofDataset" -> <|...|>, "ProofLength" -> 2, "RuleHistogram" -> <|"forward_subsumption_resolution" -> 1|>|> -->

With `"ParseFormulas" -> True`, the formulas are parsed into expressions:

```wl
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False, "ParseFormulas" -> True]["Axioms"]
```

<!-- => {X0_ ⊗ (X1_ ⊕ OverBar[X1_]) == X0_} -->

## Scope

The proof of a group theorem has critical-pair and substitution lemmas:

```wl
TVampireProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5, "LiftToProofObject" -> False]["RuleHistogram"]
```

<!-- => <|"forward_demodulation" -> 1, "superposition" -> 6, "trivial_inequality_removal" -> 1|> -->

---

The number of steps of the proof:

```wl
TVampireProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5, "LiftToProofObject" -> False]["ProofLength"]
```

<!-- => 9 -->

## Options

### LiftToProofObject

With `"LiftToProofObject" -> False`, the result is an Association rather than a [ProofObject]():

```wl
TVampireProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5, "LiftToProofObject" -> False]["Backend"]
```

<!-- => "SZS" -->

### ParseFormulas

`"ParseFormulas" -> True` parses the formulas of the steps into expressions, as in the last of the basic examples:

```wl
#| eval: false
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False, "ParseFormulas" -> True]["Axioms"]
```

<!-- => {X0_ ⊗ (X1_ ⊕ OverBar[X1_]) == X0_} -->

### TimeConstraint

A time limit too short for Vampire gives a [Failure]():

```wl
TVampireProofObject["RobbinsAxioms", "DoubleNegation", TimeConstraint -> 1]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Vampire", "Status" -> "TimedOut", "Seconds" -> 0.94, "Strategy" -> Missing["NoProof"]|>] -->

### Mode

In the `"vampire"` mode, Vampire runs a single strategy and finds a proof by superposition:

```wl
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False, "Mode" -> "vampire"]["RuleHistogram"]
```

<!-- => <|"superposition" -> 1, "trivial_inequality_removal" -> 1|> -->

### Binary

A path with no program gives a [Failure]():

```wl
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "Binary" -> "/nonexistent/vampire"]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Vampire", "Status" -> "Failed", "Seconds" -> 0.01, "Strategy" -> Missing["NoProof"]|>] -->

## Properties and Relations

[TFindProof]() with the `"VampireProcess"` method gives the same result:

```wl
TFindProof["DroppingAlwaysTrue", "BooleanAxioms", Method -> "VampireProcess", TimeConstraint -> 5, "LiftToProofObject" -> False] === TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]
```

<!-- => True -->
