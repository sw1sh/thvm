---
Template: Symbol
Name: TATP
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TATP
Keywords: [theorem proving, ATP, saturation, completion, Waldmeister, .pr file, statistics]
SeeAlso: [TFindProof, TRelevantAxioms, TAtpSchedule]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Theorem Proving with THVMLink](ATP)"]
---

## Usage

<code>[TATP]()[*axioms*, *conjecture*]</code> runs the saturation prover on the equational *axioms* and *conjecture*, and gives an Association describing the outcome.

<code>[TATP]()[[File]()["*file*"]]</code> runs the saturation prover on a problem in the Waldmeister `.pr` format.

## Details & Options

- [TATP]() runs unfailing Knuth-Bendix completion with one fixed strategy and reports how the run ended. It gives no proof; [TFindProof]() runs the same engine with a choice of strategies and gives a [ProofObject]().
- The *axioms* are a list of equations, or a single equation. An equation is written `lhs == rhs` or <code>[ForAll]()[*vars*, *lhs* == *rhs*]</code>, and pattern variables such as `x_` in an axiom are universally quantified.
- The result has the keys `"Status"`, `"Steps"` (the number of completion steps), `"Rules"` (the number of rewrite rules) and `"QueueSize"` (the number of critical pairs left in the queue).
- The possible values of `"Status"` are:

| | |
| --- | --- |
| `"PROVED"` | the two sides of the conjecture were joined |
| `"QUEUE_EMPTY"` | the axioms completed without joining the two sides |
| `"TIMEOUT"` | the [MaxSteps]() budget ran out |

- [TATP]() has attribute [HoldAll](), so an equation such as `a == a` reaches it unevaluated. A <code>[File]()[*path*]</code> argument has to be given with a literal string, or evaluated with [Evaluate](). A file that does not exist issues the message `TATP::nofile` and gives `$Failed`.
- Equations are best written over operators that do not evaluate, such as [CircleTimes](), [CenterDot]() or an undefined head `f`.
- The following option can be given:

| | | |
| --- | --- | --- |
| [MaxSteps]() | `64` | maximum number of completion steps |

## Basic Examples

Prove that the inverse is an involution, from the right identity, right inverse and associativity axioms:

```wl
TATP[{x_ \[CircleTimes] 1 == x_, x_ \[CircleTimes] inv[x_] == 1, x_ \[CircleTimes] (y_ \[CircleTimes] z_) == (x_ \[CircleTimes] y_) \[CircleTimes] z_}, inv[inv[a]] == a]
```

<!-- => <|"Status" -> "PROVED", "Steps" -> 41, "Rules" -> 8, "QueueSize" -> 11|> -->

---

A conjecture that does not follow from the axioms completes without a proof:

```wl
TATP[{f[f[x_]] == x_}, f[a] == a]
```

<!-- => <|"Status" -> "QUEUE_EMPTY", "Steps" -> 2, "Rules" -> 1, "QueueSize" -> 0|> -->

## Scope

A single axiom can be given without a list, and quantified with [ForAll]():

```wl
TATP[ForAll[{x, y, z}, f[f[x, y], z] == f[x, f[y, z]]], f[f[a, b], f[c, d]] == f[a, f[b, f[c, d]]]]
```

<!-- => <|"Status" -> "PROVED", "Steps" -> 2, "Rules" -> 1, "QueueSize" -> 0|> -->

---

Write a problem in the Waldmeister `.pr` format to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

Run the prover on the file, evaluating the argument that [TATP]() holds:

```wl
TATP[Evaluate[File[file]]]
```

<!-- => <|"Status" -> "PROVED", "Steps" -> 2, "Rules" -> 1, "QueueSize" -> 0|> -->

## Options

### MaxSteps

A small budget stops the completion before it joins the two sides:

```wl
TATP[{x_ \[CircleTimes] 1 == x_, x_ \[CircleTimes] inv[x_] == 1, x_ \[CircleTimes] (y_ \[CircleTimes] z_) == (x_ \[CircleTimes] y_) \[CircleTimes] z_}, inv[inv[a]] == a, MaxSteps -> 10]
```

<!-- => <|"Status" -> "TIMEOUT", "Steps" -> 32, "Rules" -> 7, "QueueSize" -> 9|> -->

## Properties and Relations

[TFindProof]() proves the same theorem and gives a [ProofObject]():

```wl
TFindProof[inv[inv[a]] == a, {x_ \[CircleTimes] 1 == x_, x_ \[CircleTimes] inv[x_] == 1, x_ \[CircleTimes] (y_ \[CircleTimes] z_) == (x_ \[CircleTimes] y_) \[CircleTimes] z_}]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

[TATP]() holds its arguments, so a goal `a == a` is not evaluated to [True]() first:

```wl
TATP[{f[x_] == x_}, a == a]
```

<!-- => <|"Status" -> "PROVED", "Steps" -> 1, "Rules" -> 0, "QueueSize" -> 1|> -->

## Possible Issues

Write the problem to a file:

```wl
file = Export[FileNameJoin[{$TemporaryDirectory, "involution.pr"}], "NAME involution\nMODE PROOF\nSORTS ANY\nSIGNATURE a: -> ANY\n          f: ANY -> ANY\nORDERING LPO\n         f > a\nVARIABLES x : ANY\nEQUATIONS f(f(x)) = x\nCONCLUSION f(f(f(a))) = f(a)\n", "Text"]
```

<!-- => the path of involution.pr in $TemporaryDirectory -->

[TATP]() holds its arguments, so a file name given through a variable is not evaluated, and the input returns unevaluated:

```wl
TATP[File[file]]
```

<!-- => TATP[File[file]] -->

---

A file that does not exist issues a message and gives `$Failed`:

```wl
TATP[File["/nonexistent/problem.pr"]]
```

<!-- => the message TATP::nofile is issued and the result is $Failed -->
