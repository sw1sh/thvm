---
Template: Symbol
Name: TAtpSystem
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSystem
Keywords: [rewrite system, completed system, normal form, Knuth-Bendix completion]
SeeAlso: [TAtpComplete, TAtpNormalForm, TAtpEqualQ, TKnuthBendixCompletion]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpSystem]()[...]</code> is a completed equational system kept in the engine, given by [TAtpComplete]().

<code>*sys*["*prop*"]</code> gives the property *prop* of the system *sys*.

## Details & Options

- The possible properties are:

| | |
| --- | --- |
| `"Rules"` | the rules `l -> r` and the unorientable equations `l <-> r` of the system |
| `"Ordering"` | the reduction ordering, as an [Association]() of `"Ordering"`, `"Precedence"` and `"SymbolWeights"` |
| `"Status"` | `"Saturated"` when every critical pair was joined, `"BudgetExhausted"` when completion stopped at its budget |
| `"Complete"` | whether the system is ground convergent, so that equal terms have one normal form |

- [TAtpNormalForm]() and [TAtpEqualQ]() answer from the system without completing again.

## Basic Examples

The rules of a completed system:

```wl
TAtpComplete[{ForAll[x, k[k[x]] == x]}]["Rules"]
```

<!-- => {k[k[𝕒_]] -> 𝕒} -->

---

Its reduction ordering:

```wl
TAtpComplete[{ForAll[x, k[k[x]] == x]}]["Ordering"]
```

<!-- => <|"Ordering" -> "KBO", "Precedence" -> {"k"}, "SymbolWeights" -> None|> -->
