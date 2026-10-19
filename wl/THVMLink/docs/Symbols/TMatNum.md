---
Template: Symbol
Name: TMatNum
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMatNum
Keywords: [match, mat, pattern, num, case, dispatch, constructor]
SeeAlso: [TMatCtr, TMatChain, TIfZero, TNum, TCtr, TApp]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMatNum]()[*n*, *handler*, *fallback*]</code> gives a matcher that, applied to an argument equal to the number *n*, gives *handler*, and otherwise applies *fallback* to the argument.

## Details & Options

- A matcher acts when it is applied, as <code>[TApp]()[*m*, *arg*]</code> or <code>*m*[*arg*]</code>. The argument is reduced first.
- When the argument is a number equal to *n*, the application gives *handler* as it is, without applying it.
- When the argument is a constructor ([TCtr]()) with label *n*, the application applies *handler* to the constructor's fields in order, so *handler* is a curried lambda with one parameter per field.
- Otherwise the application gives <code>[TApp]()[*fallback*, *arg*]</code>, so *fallback* is a lambda of one parameter.
- A superposition argument distributes: the matcher is applied to each branch.
- *n* is an [Integer](). *handler* and *fallback* are terms; write a number handler as <code>[TNum]()[*i*]</code>.
- [TIfZero]() applies a matcher for 0 to a counter, [TMatChain]() chains matchers for several values, and [TMatCtr]() is the same matcher named for destructuring.

## Basic Examples

A matcher that gives 100 for the argument 0 and 200 otherwise:

```wl
TMatNum[0, TNum[100], TLam[k, TNum[200]]]
```

<!-- => a TTerm summary box showing tag MAT and ext 0 -->

---

Applied to 0, it gives the handler:

```wl
TTermVal[TWnf[TApp[TMatNum[0, TNum[100], TLam[k, TNum[200]]], TNum[0]]]]
```

<!-- => 100 -->

---

Applied to any other number, it applies the fallback:

```wl
TTermVal[TWnf[TApp[TMatNum[0, TNum[100], TLam[k, TNum[200]]], TNum[5]]]]
```

<!-- => 200 -->

## Scope

The fallback receives the argument:

```wl
TTermVal[TWnf[TApp[TMatNum[0, TNum[100], TLam[k, k + 1]], TNum[5]]]]
```

<!-- => 6 -->

---

The argument is reduced before the match:

```wl
TTermVal[TWnf[TApp[TMatNum[0, TNum[100], TLam[k, TNum[200]]], TOp2["-", 3, 3]]]]
```

<!-- => 100 -->

---

A constructor with the matching label is taken apart, and the handler receives its fields:

```wl
TTermVal[TWnf[TApp[TMatNum[1, TLam[a, TLam[b, a + b]], TLam[k, TNum[0]]], TCtr[1, TNum[10], TNum[20]]]]]
```

<!-- => 30 -->

---

A constructor with another label goes to the fallback:

```wl
TTermVal[TWnf[TApp[TMatNum[1, TLam[a, TLam[b, a + b]], TLam[k, TNum[0]]], TCtr[2, TNum[10], TNum[20]]]]]
```

<!-- => 0 -->

---

A superposition argument is matched branch by branch:

```wl
TTermVal /@ TCollapse[TApp[TMatNum[0, TNum[100], TLam[k, k + 1]], TSup[0, 7]]]
```

<!-- => {100, 8} -->

## Properties and Relations

The walk of a matcher shows the value it tests for, its handler and its fallback:

```wl
TTermExpr[TMatNum[0, TNum[100], TLam[k, TNum[200]]]]
```

<!-- => "MAT"[0, "NUM"[100], "LAM"["NUM"[200]]] -->

---

Applying the matcher as a function builds the same application:

```wl
TTermVal[TWnf[TMatNum[0, TNum[100], TLam[k, TNum[200]]][TNum[0]]]]
```

<!-- => 100 -->

---

[TIfZero]() is the matcher for 0 applied to a counter:

```wl
TTermVal[TWnf[TIfZero[TNum[0], TNum[1], TNum[2]]]]
```

<!-- => 1 -->

## Possible Issues

A number match gives the handler without applying it, so a lambda handler comes back as a lambda:

```wl
TWnf[TApp[TMatNum[1, TLam[a, a + 1], TLam[k, TNum[0]]], TNum[1]]]["tagName"]
```

<!-- => "LAM" -->
