---
Template: Symbol
Name: TOp2
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TOp2
Keywords: [op2, arithmetic, integer, comparison, operator]
SeeAlso: [TNum, TEql, TIfZero, TMatNum, TWnf]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TOp2]()["*op*", *x*, *y*]</code> gives a term applying the integer operator *op* to the terms *x* and *y*.

## Details & Options

- The operators are `"+"`, `"-"` and `"*"`; `"/"` and `"%"`, the integer quotient and remainder; `"&"`, `"|"` and `"^"`, the bitwise and, or and exclusive or; `"<<"` and `">>"`, the shifts; and the comparisons `"=="`, `"!="`, `"<"`, `">"`, `"<="` and `">="`. Each also has a name, such as `"ADD"` for `"+"`. Any other operator issues the message `TOp2::badop`, and the input returns unevaluated.
- A bare [Integer]() operand is read as <code>[TNum]()[*i*]</code>.
- The operation is strict in both operands: it fires once both have reduced to numbers, and gives a number.
- Arithmetic is on unsigned 32-bit patterns, and a division by zero gives 0. A comparison gives 1 for true and 0 for false.
- An operand that reduces to a superposition distributes the operation over its branches.
- The `+` and `*` operators between number-valued terms, and `+`, `-`, `*`, `==` and `<` between such a term and an integer, build [TOp2]() terms: <code>*a* + *b*</code> is <code>[TOp2]()["+", *a*, *b*]</code>.
- `"=="` compares numbers. [TEql]() compares arbitrary terms structurally.
- [TIfZero]() and [TMatNum]() branch on the number a comparison gives.

## Basic Examples

A sum of two integers:

```wl
TOp2["+", 3, 4]
```

<!-- => a TTerm summary box showing tag OP2 and ext 0 -->

---

Reducing it gives its value:

```wl
TTermVal[TWnf[TOp2["+", 3, 4]]]
```

<!-- => 7 -->

## Scope

A product:

```wl
TTermVal[TWnf[TOp2["*", 6, 7]]]
```

<!-- => 42 -->

---

A difference:

```wl
TTermVal[TWnf[TOp2["-", 10, 3]]]
```

<!-- => 7 -->

---

An integer quotient:

```wl
TTermVal[TWnf[TOp2["/", 7, 2]]]
```

<!-- => 3 -->

---

A remainder:

```wl
TTermVal[TWnf[TOp2["%", 7, 2]]]
```

<!-- => 1 -->

---

A left shift:

```wl
TTermVal[TWnf[TOp2["<<", 7, 2]]]
```

<!-- => 28 -->

---

An equality test gives 1 when the numbers are equal:

```wl
TTermVal[TWnf[TOp2["==", 3, 3]]]
```

<!-- => 1 -->

---

A comparison gives 0 when it fails:

```wl
TTermVal[TWnf[TOp2["<", 4, 3]]]
```

<!-- => 0 -->

---

Operations nest:

```wl
TTermVal[TWnf[TOp2["+", TOp2["*", 3, 4], 5]]]
```

<!-- => 17 -->

---

An operand is reduced before the operation fires:

```wl
TTermVal[TWnf[TOp2["+", TApp[TLam[x, x], 3], 4]]]
```

<!-- => 7 -->

---

An operation on a superposition distributes over its branches:

```wl
TTermVal /@ TCollapse[TOp2["+", TSup[10, 20], 1]]
```

<!-- => {11, 21} -->

## Properties and Relations

`+` between number terms builds a [TOp2]() term:

```wl
Term[TNum[3] + TNum[4]]
```

<!-- => Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]] -->

---

`<` between a number term and an integer builds one too:

```wl
Term[TNum[3] < 4]
```

<!-- => Term["OP2", "<", Term["NUM", 3], Term["NUM", 4]] -->

---

[TIfZero]() branches on a comparison:

```wl
TTermVal[TWnf[TIfZero[TOp2["==", 3, 4], TNum[100], TNum[200]]]]
```

<!-- => 100 -->

## Possible Issues

A negative result reads as its unsigned 32-bit pattern:

```wl
TTermVal[TWnf[TOp2["-", 3, 4]]]
```

<!-- => 4294967295 -->

---

An operator that is not one of the operators of [TOp2]() is refused:

```wl
TOp2["foo", 1, 2]
```

<!-- => the message TOp2::badop is issued and the expression returns unevaluated -->
