---
Template: Symbol
Name: TWaldmeisterProof
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWaldmeisterProof
Keywords: [Waldmeister, wmcli, theorem proving, ATP, unfailing completion, proof protocol, external prover, equational logic]
SeeAlso: [TWaldmeisterProofObject, TVampireProof, TTweeProof, TEproverProof, TATP, TFindProof, FindEquationalProof]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[The Waldmeister Method in THVMLink](Waldmeister)", "[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TWaldmeisterProof]()["*file*"]</code> runs the Waldmeister prover on the problem in *file*, and gives an Association describing the result.

## Details & Options

- [TWaldmeisterProof]() runs `wmcli`, the command-line Waldmeister program, which has to be installed. The program is looked for at the path in the `WMCLI` environment variable, in the directories of the `PATH` environment variable, and in `~/.local/bin`, `~/bin` and `~/src/wolfram/waldmeister`.
- The *file* is a problem in the Waldmeister `.pr` format, with a signature, an ordering, variables, equations and a conclusion.
- Waldmeister runs in its automatic mode, as it does for [FindEquationalProof]().
- The result has the following keys:

| | |
| --- | --- |
| `"Status"` | `"Proved"`, `"TimedOut"` or `"Failed"` |
| `"Strategy"` | `"waldmeister-auto"` |
| `"Seconds"` | the time Waldmeister took |
| `"ProofLength"` | the number of steps of the proof |
| `"Inferences"` | the steps of the proof, one Association for each |
| `"RawProtocol"` | the output of Waldmeister |

- Each inference has the keys `"Name"`, `"Role"`, `"Formula"`, `"Rule"` and `"Parents"`. The rule is `"orient"` for an equation turned into a rule, `"superposition"` for a critical pair, `"forward_demodulation"` for a rewrite, and `"file"` for an axiom or the goal.
- Waldmeister runs without a time limit, so a problem that is not a theorem can run without end.
- [TWaldmeisterProofObject]() converts the result into the proof form of [TFindProof]().
- The following options can be given:

| | | |
| --- | --- | --- |
| `"Binary"` | [Automatic]() | path of the `wmcli` program |
| `"WMCLI"` | [Automatic]() | path of the `wmcli` program, used instead of `"Binary"` when given |
| `"MathlinkPath"` | [Automatic]() | directory of the MathLink framework the program loads |

## Basic Examples

Write a Waldmeister problem that asks whether `f[f[f[a]]] == f[a]` follows from `f[f[x]] == x`:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

Run Waldmeister on it:

```wl
TWaldmeisterProof[file]
```

<!-- => <|"Status" -> "Proved", "Strategy" -> "waldmeister-auto", "Seconds" -> 0.02, "ProofLength" -> 5, "Inferences" -> {<|"Head" -> "wm", "Name" -> "1", "Role" -> "axiom", "Formula" -> "x1 = f(f(x1))", "Rule" -> "file", "Parents" -> {}|>, ...}, "RawProtocol" -> "****...\n*  W A L D M E I S T E R ...\n..."|> -->

## Scope

Write a problem:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

The formula and rule of each step:

```wl
TWaldmeisterProof[file]["Inferences"][[All, {"Formula", "Rule"}]]
```

<!-- => {<|"Formula" -> "x1 = f(f(x1))", "Rule" -> "file"|>, <|"Formula" -> "f(f(f(a()))) = f(a())", "Rule" -> "file"|>, <|"Formula" -> "f(f(x1)) -> x1", "Rule" -> "orient"|>, <|"Formula" -> "f(a()) = f(a())", "Rule" -> "forward_demodulation"|>, <|"Formula" -> "f(a()) = f(a())", "Rule" -> "trivial_inequality_removal"|>} -->

---

The double-inverse theorem of abelian groups, from its problem file:

```wl
TWaldmeisterProof[FileNameJoin[{Directory[], "tools", "baselines", "wm_pr", "AbelianGroupAxioms__InverseOfInverse.pr"}]]["ProofLength"]
```

<!-- => 19 -->

## Options

### Binary

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

`"Binary"` gives the path of the program; a path with no program gives the status `"Failed"`:

```wl
TWaldmeisterProof[file, "Binary" -> "/nonexistent/wmcli"]
```

<!-- => <|"Status" -> "Failed", "Strategy" -> "waldmeister-auto", "Seconds" -> 0.01, "ProofLength" -> 0, "Inferences" -> {}, "RawProtocol" -> "/bin/sh: /nonexistent/wmcli: No such file or directory\n"|> -->

### WMCLI

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

`"WMCLI"` is used instead of `"Binary"`:

```wl
TWaldmeisterProof[file, "WMCLI" -> "/nonexistent/wmcli", "Binary" -> Automatic]["Status"]
```

<!-- => "Failed" -->

### MathlinkPath

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

The program does not start without the MathLink framework:

```wl
TWaldmeisterProof[file, "MathlinkPath" -> "/nonexistent"]["Status"]
```

<!-- => "Failed" -->

## Properties and Relations

[TWaldmeisterProofObject]() gives the proof in the proof form of [TFindProof]():

```wl
TWaldmeisterProofObject["AbelianGroupAxioms", "InverseOfInverse"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

[TATP]() reads the same `.pr` format and runs the completion of THVMLink on it:

```wl
TATP[File["tools/baselines/wm_pr/AbelianGroupAxioms__InverseOfInverse.pr"]]
```

<!-- => <|"Status" -> "PROVED", "Steps" -> 27, "Rules" -> 8, "QueueSize" -> 9|> -->

## Possible Issues

A file that does not exist issues a message and gives `$Failed`:

```wl
TWaldmeisterProof["/nonexistent/problem.pr"]
```

<!-- => the message TWaldmeisterProof::badfile is issued and the result is $Failed -->
