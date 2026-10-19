---
Template: Symbol
Name: TTermSub
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermSub
Keywords: [sub, term, inspector, substitution, flag]
SeeAlso: [TTerm, TTermTag, TTermExt, TTermVal, THeapRead, TWnf]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermSub]()[*term*]</code> gives the SUB flag of *term*, 0 or 1.

## Details & Options

- *term* is a [TTerm]() or its packed [Integer]() value.
- The SUB flag is 1 on a heap cell that holds the value substituted for a variable. When an interaction such as the application of a lambda fires, the argument is written into the binder's cell with the flag set.
- Reading a VAR, DP0 or DP1 through a flagged cell gives the substituted value, which is how [Term]() and [TWnf]() follow substitutions.
- A term built by a constructor has the flag clear.

## Basic Examples

A freshly built lambda has the flag clear:

```wl
TTermSub[TLam[x, x]]
```

<!-- => 0 -->

---

So does a freshly built number:

```wl
TTermSub[TNum[5]]
```

<!-- => 0 -->

## Scope

Bind a lambda:

```wl
f = TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM -->

Before any reduction, the lambda's cell holds its variable with the flag clear:

```wl
TTermSub[THeapRead[TTermVal[f]]]
```

<!-- => 0 -->

Apply the lambda to 5 and reduce:

```wl
TWnf[TApp[f, TNum[5]]]
```

<!-- => a TNum summary box showing number 5 -->

The lambda's cell now holds the substituted argument:

```wl
cell = THeapRead[TTermVal[f]]
```

<!-- => a TNum summary box showing number 5 -->

Its flag is set:

```wl
TTermSub[cell]
```

<!-- => 1 -->

The cell's value is the argument:

```wl
TTermVal[cell]
```

<!-- => 5 -->

## Properties and Relations

The flag matches the `"sub"` property of a [TTerm]():

```wl
TNum[5]["sub"]
```

<!-- => 0 -->
