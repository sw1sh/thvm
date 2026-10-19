---
Template: Symbol
Name: TVampireProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TVampireProof
Keywords: [Vampire, theorem proving, ATP, TPTP, SZS, external prover, refutation]
SeeAlso: [TVampireProofObject, TEproverProof, TTweeProof, TWaldmeisterProof, TSZSDerivationToProofObject, TFindProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)", "[Importing TPTP Benchmarks](TPTPImport)"]
---

## Usage

<code>[TVampireProof]()["*file*"]</code> runs the Vampire prover on the TPTP problem in *file*, and gives an Association describing the result.

<code>[TVampireProof]()["*theory*", "*theorem*"]</code> runs Vampire on the problem file of a named theorem of an [AxiomaticTheory]() theory.

## Details & Options

- [TVampireProof]() runs the Vampire program, which has to be installed, on a problem in the TPTP (Thousands of Problems for Theorem Provers) format. The program is looked for in `/opt/homebrew/bin`, `/usr/local/bin` and the directories of the `PATH` environment variable.
- The *file* is a TPTP problem with the extension `.p`. The form <code>[TVampireProof]()["*theory*", "*theorem*"]</code> reads the file `tools/baselines/vampire_tptp/`*theory*`__`*theorem*`.p` under the current directory.
- The result has the following keys:

| | |
| --- | --- |
| `"Status"` | `"Proved"`, `"TimedOut"` or `"Failed"` |
| `"Strategy"` | the Vampire strategy that found the proof |
| `"Seconds"` | the time Vampire took |
| `"ProofLength"` | the number of steps of the proof |
| `"Inferences"` | the steps of the proof, one Association for each |
| `"RawSZS"` | the output of Vampire, in the SZS (Sutcliffe-Zimmer-Schulz) format |

- Each inference has the keys `"Head"`, `"Name"`, `"Role"`, `"Formula"`, `"Rule"`, `"Status"`, `"RawSource"` and `"Parents"`. Steps that only reorient an equation are left out.
- A problem that is not a theorem gives the status `"Failed"`, with no inferences. Vampire runs with the time limit [TimeConstraint]() and reports `"TimedOut"` when it reaches it; a Vampire that has not ended 2 seconds after the limit is stopped.
- [TVampireProofObject]() converts the result into the proof form of [TFindProof](), and <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "VampireProcess"]</code> runs Vampire from [TFindProof]().
- The following options can be given:

| | | |
| --- | --- | --- |
| [TimeConstraint]() | `30` | time limit in seconds given to Vampire |
| `"Mode"` | `"casc"` | the mode Vampire runs in |
| `"Binary"` | [Automatic]() | path of the Vampire program |

## Basic Examples

Write a TPTP problem that asks whether `f[f[f[a]]] == f[a]` follows from `f[f[x]] == x`:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.p"}], "cnf(involution, axiom, f(f(X)) = X).\ncnf(goal, negated_conjecture, f(f(f(a))) != f(a)).\n", "Text"]
```

<!-- => the path of involution.p in $TemporaryDirectory -->

Run Vampire on it:

```wl
TVampireProof[file, TimeConstraint -> 5]
```

<!-- => <|"Status" -> "Proved", "Strategy" -> a strategy such as "lrs+10_1_random_seed=1198963420:st=3:sd=2:ss=axioms:sgt=8_1" (the seed varies), "Seconds" -> a time, "ProofLength" -> 3, "Inferences" -> {<|"Head" -> "fof", "Name" -> "f1", "Role" -> "axiom", ...|>, ...}, "RawSZS" -> "% Running in auto input_syntax mode. Trying TPTP\n..."|> -->

## Scope

Write a TPTP problem:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.p"}], "cnf(involution, axiom, f(f(X)) = X).\ncnf(goal, negated_conjecture, f(f(f(a))) != f(a)).\n", "Text"]
```

<!-- => the path of involution.p in $TemporaryDirectory -->

The status of the run:

```wl
TVampireProof[file, TimeConstraint -> 5]["Status"]
```

<!-- => "Proved" -->

The number of steps of the proof:

```wl
TVampireProof[file, TimeConstraint -> 5]["ProofLength"]
```

<!-- => 3 -->

The rule of each step:

```wl
TVampireProof[file, TimeConstraint -> 5]["Inferences"][[All, "Rule"]]
```

<!-- => {"file", "file", "forward_subsumption_resolution"} -->

---

Prove a named theorem of a theory from its problem file:

```wl
TVampireProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 5]["Status"]
```

<!-- => "Proved" -->

## Options

### TimeConstraint

A time limit of one second is too short for Vampire to settle the Robbins problem:

```wl
TVampireProof["RobbinsAxioms", "DoubleNegation", TimeConstraint -> 1]["Status"]
```

<!-- => "TimedOut" -->

### Mode

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.p"}], "cnf(involution, axiom, f(f(X)) = X).\ncnf(goal, negated_conjecture, f(f(f(a))) != f(a)).\n", "Text"]
```

<!-- => the path of involution.p in $TemporaryDirectory -->

In the `"vampire"` mode, Vampire runs a single strategy and reports no strategy name:

```wl
TVampireProof[file, TimeConstraint -> 5, "Mode" -> "vampire"]["Strategy"]
```

<!-- => Missing["NotFound"] -->

### Binary

`"Binary"` gives the path of the program; a path with no program gives the status `"Failed"`:

```wl
TVampireProof["AbelianGroupAxioms", "InverseOfInverse", TimeConstraint -> 5, "Binary" -> "/nonexistent/vampire"]
```

<!-- => <|"Status" -> "Failed", "Strategy" -> Missing["NoProof"], "Seconds" -> a time, "ProofLength" -> 0, "Inferences" -> {}, "RawSZS" -> "sh: /nonexistent/vampire: No such file or directory\nsh: line 0: exec: /nonexistent/vampire: cannot execute: No such file or directory\n"|> -->

## Properties and Relations

[TVampireProofObject]() converts the proof into the proof form of [TFindProof]():

```wl
TVampireProofObject["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5, "LiftToProofObject" -> False]
```

<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> "skc1"[] != "and"["skc1"[], "or"["skc2"[], "not"["skc2"[]]]], "Axioms" -> {"(\n  ( ! [X0,X1] : (and(X0,or(X1,not(X1))) = X0) ))"}, "ProofDataset" -> <|{"Axiom", 1} -> ..., {"Hypothesis", 1} -> ..., {"Conclusion", 1} -> ...|>, "ProofLength" -> 2, "RuleHistogram" -> <|"forward_subsumption_resolution" -> 1|>|> -->

---

[TSZSDerivationToProofObject]() converts the inferences:

```wl
TSZSDerivationToProofObject[TVampireProof["BooleanAxioms", "DroppingAlwaysTrue", TimeConstraint -> 5]["Inferences"]]
```

<!-- => the same Association as TVampireProofObject gives -->

## Possible Issues

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "nontheorem.p"}], "cnf(involution, axiom, f(f(X)) = X).\ncnf(goal, negated_conjecture, f(a) != a).\n", "Text"]
```

<!-- => the path of nontheorem.p in $TemporaryDirectory -->

A problem that is not a theorem gives the status `"Failed"`:

```wl
TVampireProof[file, TimeConstraint -> 5]["Status"]
```

<!-- => "Failed" -->

---

A file that does not exist issues a message and gives `$Failed`:

```wl
TVampireProof["/nonexistent/problem.p"]
```

<!-- => the message TVampireProof::badfile is issued and the result is $Failed -->
