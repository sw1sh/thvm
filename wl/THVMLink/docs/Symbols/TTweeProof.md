---
Template: Symbol
Name: TTweeProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTweeProof
Keywords: [Twee, theorem proving, ATP, TPTP, equational logic, external prover]
SeeAlso: [TTweeProofObject, TVampireProof, TEproverProof, TWaldmeisterProof, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TTweeProof]()["*file*"]</code> runs the Twee prover on the TPTP problem in *file*, and gives an Association describing the result.

<code>[TTweeProof]()["*theory*", "*theorem*"]</code> runs Twee on the problem file of a named theorem of an [AxiomaticTheory]() theory.

## Details & Options

- [TTweeProof]() runs the Twee program, an equational prover, which has to be installed. The program is looked for in `~/.cabal/bin`, `/opt/homebrew/bin`, `/usr/local/bin` and the directories of the `PATH` environment variable.
- The *file* is a problem in the TPTP (Thousands of Problems for Theorem Provers) format with the extension `.p`. The form <code>[TTweeProof]()["*theory*", "*theorem*"]</code> reads the file `tools/baselines/vampire_tptp/`*theory*`__`*theorem*`.p` under the current directory.
- The result has the following keys:

| | |
| --- | --- |
| `"Status"` | `"Proved"` or `"Failed"` |
| `"Strategy"` | `"twee"` |
| `"Seconds"` | the time Twee took |
| `"ProofLength"` | the number of axioms, lemmas and goals of the proof |
| `"Axioms"` | the axioms the proof uses, each with a `"Name"` and a `"Statement"` |
| `"Lemmas"` | the lemmas of the proof, each with a `"Name"` and a `"Statement"` |
| `"RawProof"` | the proof as Twee writes it |

- Twee writes its proof as a chain of equations, with no record of the inference rules, so [TTweeProof]() gives the axioms and lemmas of the proof but no inference steps.
- A problem that is not a theorem, or that Twee does not prove within the time limit, gives the status `"Failed"`.
- [TTweeProofObject]() converts the result into the proof form of [TFindProof](), and <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "TweeProcess"]</code> runs Twee from [TFindProof]().
- The following options can be given:

| | | |
| --- | --- | --- |
| [TimeConstraint]() | `30` | time limit in seconds |
| `"Binary"` | [Automatic]() | path of the Twee program |

## Basic Examples

Write a TPTP problem that asks whether `f[f[f[a]]] == f[a]` follows from `f[f[x]] == x`:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.p"}], "cnf(involution, axiom, f(f(X)) = X).\ncnf(goal, negated_conjecture, f(f(f(a))) != f(a)).\n", "Text"]
```

<!-- => the path of involution.p in $TemporaryDirectory -->

Run Twee on it:

```wl
TTweeProof[file, TimeConstraint -> 5]
```

<!-- => <|"Status" -> "Proved", "Strategy" -> "twee", "Seconds" -> 0.07, "ProofLength" -> 2, "Axioms" -> {<|"Name" -> "axiom1", "Statement" -> "f(f(X)) = X."|>}, "Lemmas" -> {}, "RawProof" -> "Axiom 1 (involution): f(f(X)) = X.\n\nGoal 1 (goal): f(f(f(a))) = f(a).\nProof:\n  f(f(f(a)))\n= { by axiom 1 (involution) }\n  f(a)\n%"|> -->

## Scope

Prove a named theorem of a theory from its problem file:

```wl
TTweeProof["GroupAxioms", "LeftIdentity", TimeConstraint -> 5]["Status"]
```

<!-- => "Proved" -->

---

The lemmas of the proof:

```wl
TTweeProof["GroupAxioms", "LeftIdentity", TimeConstraint -> 5]["Lemmas"]
```

<!-- => {<|"Name" -> "lemma4", "Statement" -> "and(X, and(not(X), Y)) = and(op_overtilde(k1), Y)."|>, <|"Name" -> "lemma5", "Statement" -> "and(op_overtilde(k1), not(not(X))) = X."|>} -->

---

The axioms the proof uses:

```wl
TTweeProof["GroupAxioms", "LeftIdentity", TimeConstraint -> 5]["Axioms"][[All, "Statement"]]
```

<!-- => {"and(X, op_overtilde(k1)) = X.", "and(X, not(X)) = op_overtilde(k1).", "and(X, and(Y, Z)) = and(and(X, Y), Z)."} -->

## Options

### TimeConstraint

A time limit of one second is too short for Twee to settle the Robbins problem:

```wl
TTweeProof["RobbinsAxioms", "DoubleNegation", TimeConstraint -> 1]
```

<!-- => <|"Status" -> "Failed", "Strategy" -> "twee", "Seconds" -> 3.06, "ProofLength" -> 0, "Axioms" -> {}, "Lemmas" -> {}, "RawProof" -> ""|> -->

### Binary

`"Binary"` gives the path of the program; a path with no program gives the status `"Failed"`:

```wl
TTweeProof["GroupAxioms", "LeftIdentity", TimeConstraint -> 5, "Binary" -> "/nonexistent/twee"]
```

<!-- => <|"Status" -> "Failed", "Strategy" -> "twee", "Seconds" -> 0.01, "ProofLength" -> 0, "Axioms" -> {}, "Lemmas" -> {}, "RawProof" -> ""|> -->

## Properties and Relations

[TTweeProofObject]() converts the result into the proof form of [TFindProof]():

```wl
TTweeProofObject["GroupAxioms", "LeftIdentity", TimeConstraint -> 5]
```

<!-- => <|"Backend" -> "Twee-TSTP", "Status" -> "Proved", "Goal" -> Missing["NotEmittedByTwee"], ..., "ProofLength" -> 6, "RuleHistogram" -> <|"twee-rewrite" -> 2|>|> -->

---

[TVampireProof]() proves the same problem and records each inference:

```wl
TVampireProof["GroupAxioms", "LeftIdentity", TimeConstraint -> 5]["ProofLength"]
```

<!-- => 12 -->

## Possible Issues

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "nontheorem.p"}], "cnf(involution, axiom, f(f(X)) = X).\ncnf(goal, negated_conjecture, f(a) != a).\n", "Text"]
```

<!-- => the path of nontheorem.p in $TemporaryDirectory -->

A problem that is not a theorem gives the status `"Failed"`:

```wl
TTweeProof[file, TimeConstraint -> 5]["Status"]
```

<!-- => "Failed" -->

---

A file that does not exist issues a message and gives `$Failed`:

```wl
TTweeProof["/nonexistent/problem.p"]
```

<!-- => the message TTweeProof::badfile is issued and the result is $Failed -->
