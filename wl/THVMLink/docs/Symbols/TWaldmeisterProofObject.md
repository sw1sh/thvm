---
Template: Symbol
Name: TWaldmeisterProofObject
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWaldmeisterProofObject
Keywords: [Waldmeister, wmcli, theorem proving, ATP, ProofObject, proof protocol, external prover]
SeeAlso: [TWaldmeisterProof, TSZSDerivationToProofObject, TVampireProofObject, TTweeProofObject, TEproverProofObject, TFindProof, FindEquationalProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[The Waldmeister Method in THVMLink](Waldmeister)", "[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TWaldmeisterProofObject]()["*file*"]</code> runs the Waldmeister prover on the problem in the `.pr` *file*, and gives its proof as a [ProofObject]().

<code>[TWaldmeisterProofObject]()["*theory*", "*theorem*"]</code> runs Waldmeister on a named theorem of an [AxiomaticTheory]() theory.

## Details & Options

- [TWaldmeisterProofObject]() runs [TWaldmeisterProof]() and converts its proof with [TSZSDerivationToProofObject]().
- The form <code>[TWaldmeisterProofObject]()["*theory*", "*theorem*"]</code> reads the file `tools/baselines/wm_pr/`*theory*`__`*theorem*`.pr` under the current directory when it exists, and otherwise writes the problem from the axioms and the theorem of the theory.
- The steps of the proof are keyed as in the proofs of [TFindProof](): `{"Axiom", n}`, `{"Hypothesis", n}`, `{"CriticalPairLemma", n}`, `{"SubstitutionLemma", n}` and `{"Conclusion", 1}`.
- With the default `"LiftToProofObject" -> True`, the formulas are parsed into expressions and the result is a [ProofObject](). With `"LiftToProofObject" -> False`, the result is an Association with the keys `"Backend"`, `"Status"`, `"Goal"`, `"Axioms"`, `"ProofDataset"`, `"ProofLength"` and `"RuleHistogram"`, with the formulas as Waldmeister writes them.
- When Waldmeister finds no proof, the result is <code>[Failure]()["ExternalNoProof", …]</code>, whose `"Status"` is `"Failed"`, or `"TimedOut"` when Waldmeister was stopped at the time limit.
- A *file* that does not exist issues the message `TWaldmeisterProof::badfile` and gives `$Failed`; a theorem that the theory does not have gives <code>[Failure]()["WmGenerate", …]</code>.
- <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "WaldmeisterProcess"]</code> gives the same result.
- The following options can be given:

| | | |
| --- | --- | --- |
| [TimeConstraint]() | `30` | time limit in seconds |
| `"LiftToProofObject"` | [True]() | whether to give a [ProofObject]() |
| `"ParseFormulas"` | [False]() | whether to parse the formulas into expressions |
| `"Binary"` | [Automatic]() | path of the `wmcli` program |
| `"WMCLI"` | [Automatic]() | path of the `wmcli` program, used instead of `"Binary"` when given |
| `"MathlinkPath"` | [Automatic]() | directory of the MathLink framework the program loads |

## Basic Examples

Prove the double-inverse theorem of abelian groups with Waldmeister:

```wl
TWaldmeisterProofObject["AbelianGroupAxioms", "InverseOfInverse"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

With `"LiftToProofObject" -> False`, the result is an Association of the steps, with the formulas as Waldmeister writes them:

```wl
TWaldmeisterProofObject["AbelianGroupAxioms", "InverseOfInverse", "LiftToProofObject" -> False]
```

<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> "not(not(skC1())) = skC1()", "Axioms" -> {"x1 = and(x1,opOvertilde(k1()))", ...}, "ProofDataset" -> <|...|>, "ProofLength" -> 15, "RuleHistogram" -> <|"equation_copy" -> 1, "forward_demodulation" -> 3, "orient" -> 6, "superposition" -> 3, "trivial_inequality_removal" -> 1|>|> -->

With `"ParseFormulas" -> True`, the formulas are parsed into expressions:

```wl
TWaldmeisterProofObject["AbelianGroupAxioms", "InverseOfInverse", "LiftToProofObject" -> False, "ParseFormulas" -> True]["Goal"]
```

<!-- => OverBar[OverBar["skc1"[]]] == "skc1"[] -->

## Scope

Write a Waldmeister problem:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

The steps of its proof:

```wl
TWaldmeisterProofObject[file, "LiftToProofObject" -> False]
```

<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> "f(f(f(a()))) = f(a())", "Axioms" -> {"x1 = f(f(x1))"}, "ProofDataset" -> <|...|>, "ProofLength" -> 4, "RuleHistogram" -> <|"forward_demodulation" -> 1, "orient" -> 1, "trivial_inequality_removal" -> 1|>|> -->

---

The rules of a group proof:

```wl
TWaldmeisterProofObject["GroupAxioms", "LeftIdentity", "LiftToProofObject" -> False]["RuleHistogram"]
```

<!-- => <|"forward_demodulation" -> 3, "orient" -> 8, "superposition" -> 4, "trivial_inequality_removal" -> 1|> -->

## Options

### TimeConstraint

A time limit too short for Waldmeister gives a [Failure]():

```wl
TWaldmeisterProofObject["RobbinsAxioms", "DoubleNegation", TimeConstraint -> 1]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Waldmeister", "Status" -> "TimedOut", "Seconds" -> a time of about 1|>] -->

### LiftToProofObject

With `"LiftToProofObject" -> False`, the result is an Association rather than a [ProofObject]():

```wl
TWaldmeisterProofObject["GroupAxioms", "LeftIdentity", "LiftToProofObject" -> False]["Backend"]
```

<!-- => "SZS" -->

### ParseFormulas

`"ParseFormulas" -> True` parses the formulas of the steps into expressions:

```wl
TWaldmeisterProofObject["AbelianGroupAxioms", "InverseOfInverse", "LiftToProofObject" -> False, "ParseFormulas" -> True]["Goal"]
```

<!-- => OverBar[OverBar["skc1"[]]] == "skc1"[] -->

### Binary

A path with no program gives a [Failure]():

```wl
TWaldmeisterProofObject["GroupAxioms", "LeftIdentity", "Binary" -> "/nonexistent/wmcli"]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Waldmeister", "Status" -> "Failed", "Seconds" -> a time|>] -->

### WMCLI

`"WMCLI"` is used instead of `"Binary"`:

```wl
TWaldmeisterProofObject["GroupAxioms", "LeftIdentity", "WMCLI" -> "/nonexistent/wmcli"]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Waldmeister", "Status" -> "Failed", "Seconds" -> a time|>] -->

### MathlinkPath

The program does not start without the MathLink framework:

```wl
TWaldmeisterProofObject["GroupAxioms", "LeftIdentity", "MathlinkPath" -> "/nonexistent"]
```

<!-- => Failure["ExternalNoProof", <|"Tool" -> "Waldmeister", "Status" -> "Failed", "Seconds" -> a time|>] -->

## Properties and Relations

[TFindProof]() with the `"WaldmeisterProcess"` method gives the same result:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> "WaldmeisterProcess", "LiftToProofObject" -> False] === TWaldmeisterProofObject["AbelianGroupAxioms", "InverseOfInverse", "LiftToProofObject" -> False]
```

<!-- => True -->

## Possible Issues

A file that does not exist issues a message and gives `$Failed`:

```wl
TWaldmeisterProofObject["/nonexistent/problem.pr"]
```

<!-- => the message TWaldmeisterProof::badfile is issued and the result is $Failed -->

---

A theorem that the theory does not have gives a [Failure]():

```wl
TWaldmeisterProofObject["GroupAxioms", "NoSuchTheorem"]
```

<!-- => Failure["WmGenerate", <|"Reason" -> "theorem \"NoSuchTheorem\" not in AxiomaticTheory[\"GroupAxioms\", \"NotableTheorems\"]"|>] -->
