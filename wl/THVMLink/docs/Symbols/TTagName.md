---
Template: Symbol
Name: TTagName
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTagName
Keywords: [tag, name, string, term, inspector]
SeeAlso: [TTermTag, TTerm, Term, TTermExpr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTagName]()[*tag*]</code> gives the name of the integer tag *tag* as a string.

## Details & Options

- The names are the ones [TTermExpr]() and [Term]() use: `"APP"`, `"LAM"`, `"VAR"`, `"ERA"`, `"DP0"`, `"DP1"`, `"SUP"`, `"DUP"`, `"TEN"`, `"UOP"`, `"NUM"`, `"OP2"`, `"MAT"`, `"CTR"` and others.
- <code>*t*["tagName"]</code> gives <code>[TTagName]()[[TTermTag]()[*t*]]</code> for a [TTerm]() *t*.
- An integer that is not a tag gives `"TAG?"` followed by the integer.

## Basic Examples

The name of tag 1:

```wl
TTagName[1]
```

<!-- => "LAM" -->

---

The name of tag 6:

```wl
TTagName[6]
```

<!-- => "SUP" -->

## Scope

The names of the first eleven tags:

```wl
TTagName /@ Range[0, 10]
```

<!-- => {"APP", "LAM", "VAR", "ERA", "DP0", "DP1", "SUP", "DUP", "TEN", "UOP", "NUM"} -->

---

The tag of a term, by name:

```wl
TTagName[TTermTag[TOp2["+", 1, 2]]]
```

<!-- => "OP2" -->

## Properties and Relations

The `"tagName"` property of a [TTerm]() gives the same name:

```wl
TOp2["+", 1, 2]["tagName"]
```

<!-- => "OP2" -->

## Possible Issues

An integer that is not a tag gives a placeholder name:

```wl
TTagName[99]
```

<!-- => "TAG?99" -->
