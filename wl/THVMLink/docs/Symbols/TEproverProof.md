---
Template: Symbol
Name: TEproverProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TEproverProof
Keywords: [E prover, eprover, theorem proving, ATP, TPTP, SZS, external prover, paramodulation]
SeeAlso: [TEproverProofObject, TVampireProof, TTweeProof, TWaldmeisterProof, TSZSDerivationToProofObject, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TEproverProof]()["*file*"]</code> runs the E prover on the TPTP problem in *file*, and gives an Association describing the result.

<code>[TEproverProof]()["*theory*", "*theorem*"]</code> runs E on the problem file of a named theorem of an [AxiomaticTheory]() theory.

## Details & Options

- [TEproverProof]() runs the E program, which has to be installed, on a problem in the TPTP (Thousands of Problems for Theorem Provers) format. The program is looked for in `/opt/homebrew/bin`, `/usr/local/bin` and the directories of the `PATH` environment variable.
- The *file* is a TPTP problem with the extension `.p`. The form <code>[TEproverProof]()["*theory*", "*theorem*"]</code> reads the file `tools/baselines/vampire_tptp/`*theory*`__`*theorem*`.p` under the current directory.
- E runs with its automatic strategy schedule and writes its proof in the SZS (Sutcliffe-Zimmer-Schulz) format.
- The result has the following keys:

| | |
| --- | --- |
| `"Status"` | `"Proved"`, `"TimedOut"` or `"Failed"` |
| `"Strategy"` | `"eprover-auto-schedule"` |
| `"Seconds"` | the time E took |
| `"ProofLength"` | the number of steps of the proof |
| `"Inferences"` | the steps of the proof, one Association for each |
| `"RawSZS"` | the output of E |

- Each inference has the keys `"Head"`, `"Name"`, `"Role"`, `"Formula"`, `"Rule"`, `"Status"`, `"RawSource"` and `"Parents"`. E names its rules `"pm"` and `"spm"` (paramodulation), `"rw"` (rewriting), `"cn"` and others; a copy of a clause has the rule `"equation_copy"`, and an axiom or the goal has the rule `"file"`.
- E takes its time limit in whole seconds, so a [TimeConstraint]() that is not a whole number is rounded up. E stops itself at the limit.
- [TEproverProofObject]() converts the result into the proof form of [TFindProof](), and <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "EproverProcess"]</code> runs E from [TFindProof]().
- The following options can be given:

| | | |
| --- | --- | --- |
| [TimeConstraint]() | `30` | time limit in seconds given to E |
| `"Binary"` | [Automatic]() | path of the E program |

## Basic Examples

Write a TPTP problem that asks whether `f[f[f[a]]] == f[a]` follows from `f[f[x]] == x`:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.p"}], "cnf(involution, axiom, f(f(X)) = X).\ncnf(goal, negated_conjecture, f(f(f(a))) != f(a)).\n", "Text"]
```

<!-- => the path of involution.p in $TemporaryDirectory -->

Run E on it:

```wl
TEproverProof[file, TimeConstraint -> 5]
```

<!-- => <|"Status" -> "Proved", "Strategy" -> "eprover-auto-schedule", "Seconds" -> a time, "ProofLength" -> 6, "Inferences" -> {<|"Head" -> "cnf", "Name" -> "goal", "Role" -> "negated_conjecture", ...|>, ...}, "RawSZS" -> "# Preprocessing class: ...\n..."|> -->

## Scope

Prove a named theorem of a theory from its problem file:

```wl
TEproverProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 5]["ProofLength"]
```

<!-- => 15 -->

---

The rule of each step of the proof:

```wl
TEproverProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 5]["Inferences"][[All, "Rule"]]
```

<!-- => {"file", "file", "file", "file", "file", "equation_copy", "equation_copy", "equation_copy", "equation_copy", "spm", "equation_copy", "rw", "equation_copy", "rw", "cn"} -->

## Options

### TimeConstraint

The time limit is given to E, which proves this theorem within one second:

```wl
TEproverProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 1]["Status"]
```

<!-- => "Proved" -->

---

A problem E does not settle within the limit has the status `"TimedOut"`:

```wl
TEproverProof["RobbinsAxioms", "DoubleNegation", TimeConstraint -> 1]["Status"]
```

<!-- => "TimedOut" -->

### Binary

A path with no program gives the status `"Failed"`:

```wl
TEproverProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 5, "Binary" -> "/nonexistent/eprover"]["Status"]
```

<!-- => "Failed" -->

## Properties and Relations

[TVampireProof]() proves the same problems and gives a result with the same keys:

```wl
TVampireProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 5]["Status"]
```

<!-- => "Proved" -->

## Possible Issues

A file that does not exist issues a message and gives `$Failed`:

```wl
TEproverProof["/nonexistent/problem.p"]
```

<!-- => the message TEproverProof::badfile is issued and the result is $Failed -->
