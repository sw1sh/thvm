---
Template: Symbol
Name: TTweeProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTweeProofObject
Keywords: [Twee, theorem proving, ATP, proof steps, external prover]
SeeAlso: [TTweeProof, TVampireProofObject, TEproverProofObject, TWaldmeisterProofObject, TSZSDerivationToProofObject, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TTweeProofObject]()["*theory*", "*theorem*"]</code> runs the Twee prover on a named theorem of an [AxiomaticTheory]() theory, and gives its proof as an Association of steps.

## Details & Options

- [TTweeProofObject]() runs [TTweeProof]() on the problem file of the theorem and collects its axioms and lemmas.
- The result has the keys `"Backend"` (`"Twee-TSTP"`), `"Status"`, `"Goal"`, `"Axioms"` (the statement of every axiom the proof uses), `"ProofDataset"`, `"ProofLength"` and `"RuleHistogram"`.
- `"ProofDataset"` keys the steps `{"Axiom", n}` and `{"Lemma", n}`, with each statement as Twee writes it. Twee records no inference rules and does not repeat the goal, so the steps carry no derivations and `"Goal"` is [Missing]().
- When Twee finds no proof, the result is <code>[Failure]()["ExternalNoProof", …]</code>, whose `"Status"` is `"Failed"`, or `"TimedOut"` when Twee was stopped at the time limit.
- A theorem with no problem file issues the message `TTweeProof::badfile` and gives `$Failed`.
- <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "TweeProcess"]</code> gives the same result.
- The following options can be given:

| | | |
| --- | --- | --- |
| [TimeConstraint]() | `30` | time limit in seconds |
| `"Binary"` | [Automatic]() | path of the Twee program |

## Basic Examples

The steps of a Twee proof of a group theorem:

```wl
TTweeProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5]
```

<!-- => <|"Backend" -> "Twee-TSTP", "Status" -> "Proved", "Goal" -> Missing["NotEmittedByTwee"], "Axioms" -> {"and(X, op_overtilde(k1)) = X.", "and(X, not(X)) = op_overtilde(k1).", "and(X, and(Y, Z)) = and(and(X, Y), Z)."}, "ProofDataset" -> <|{"Axiom", 1} -> ..., {"Axiom", 2} -> ..., {"Axiom", 3} -> ..., {"Lemma", 1} -> ..., {"Lemma", 2} -> ...|>, "ProofLength" -> 6, "RuleHistogram" -> <|"twee-rewrite" -> 2|>|> -->

## Scope

The steps of the proof:

```wl
TTweeProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5]["ProofDataset"]
```

<!-- => <|{"Axiom", 1} -> <|"Statement" -> "and(X, op_overtilde(k1)) = X.", "Proof" -> <||>|>, ..., {"Lemma", 2} -> <|"Statement" -> "and(op_overtilde(k1), not(not(X))) = X.", "Proof" -> <||>|>|> -->

## Options

### TimeConstraint

A time limit too short for Twee gives a [Failure]():

```wl
TTweeProofObject["RobbinsAxioms", "DoubleNegation", TimeConstraint -> 1]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Twee", "Status" -> "TimedOut", "Seconds" -> a time of about 1|>] -->

### Binary

A path with no program gives a [Failure]():

```wl
TTweeProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5, "Binary" -> "/nonexistent/twee"]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Twee", "Status" -> "Failed", "Seconds" -> a time|>] -->

## Properties and Relations

[TFindProof]() with the `"TweeProcess"` method gives the same result:

```wl
TFindProof["LeftIdentity", "GroupAxioms", Method -> "TweeProcess", TimeConstraint -> 5] === TTweeProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5]
```

<!-- => True -->

---

[TVampireProofObject]() records the inference of each step of a proof of the same theorem:

```wl
TVampireProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5, "LiftToProofObject" -> False]["RuleHistogram"]
```

<!-- => <|"forward_demodulation" -> 1, "superposition" -> 6, "trivial_inequality_removal" -> 1|> -->

## Possible Issues

A theorem with no problem file issues a message and gives `$Failed`:

```wl
TTweeProofObject["GroupAxioms", "NoSuchTheorem", TimeConstraint -> 5]
```

<!-- => the message TTweeProof::badfile is issued and the result is $Failed -->
