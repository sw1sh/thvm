---
Template: Symbol
Name: TNum
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TNum
Keywords: [number, num, integer, atom, dtype]
SeeAlso: [TOp2, TUOpConst, TIfZero, TMatNum, TTermVal]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TNum]()[*i*]</code> gives a number term holding the integer *i* with dtype `"i32"`.

<code>[TNum]()[*i*, "*dtype*"]</code> gives a number term holding the 32-bit pattern *i* with dtype *dtype*.

## Details & Options

- A number term is an atom with the NUM tag. Its VAL field holds the 32-bit pattern and its EXT field the dtype code, read by [TTermVal]() and [TTermExt]().
- The usual dtypes are `"i32"`, the default, and `"f32"`. For `"f32"`, *i* is the bit pattern of the float, not its value: 1065353216 is the pattern of 1.0.
- Integer arithmetic with [TOp2]() and the `+`, `-` and `*` operators works on unsigned 32-bit patterns. A negative *i* is stored as its 32-bit two's complement, so <code>[TNum]()[-5]</code> holds 4294967291, the value arithmetic gives for 0 - 5.
- An integer outside the range from $-2^{31}$ to $2^{32} - 1$ issues the message `TNum::range`, and a *dtype* that is not a dtype name issues `TNum::dtype`; the input returns unevaluated.
- A bare [Integer]() in a term position of [TApp](), [TSup](), [TOp2]() and similar constructors is read as <code>[TNum]()[*i*]</code>.
- A number is already reduced: [TWnf]() gives it back unchanged.
- A number displays as a summary box with its value and dtype.
- For a floating-point constant in a tensor computation, [TUOpConst]() gives a tensor constant.

## Basic Examples

A number term:

```wl
TNum[42]
```

<!-- => a TNum summary box showing number 42 and dtype i32 -->

---

Its canonical form:

```wl
Term[TNum[42]]
```

<!-- => Term["NUM", 42] -->

---

Its value is the VAL field:

```wl
TTermVal[TNum[42]]
```

<!-- => 42 -->

## Scope

An `"f32"` number holding the bit pattern of 1.0:

```wl
TNum[1065353216, "f32"]
```

<!-- => a TNum summary box showing number 1065353216 and dtype f32 -->

---

Its EXT field is the dtype code 13 of `"f32"`:

```wl
TTermExt[TNum[1065353216, "f32"]]
```

<!-- => 13 -->

---

Numbers add with `+`:

```wl
TTermVal[TWnf[TNum[3] + TNum[4]]]
```

<!-- => 7 -->

---

A number multiplies by an integer with `*`:

```wl
TTermVal[TWnf[TNum[6] * 7]]
```

<!-- => 42 -->

---

A bare integer in a constructor is read as a number term:

```wl
Term[TOp2["+", 3, 4]]
```

<!-- => Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]] -->

## Properties and Relations

A number is already reduced, so [TWnf]() gives it back:

```wl
TWnf[TNum[5]] == TNum[5]
```

<!-- => True -->

---

A comparison gives the number 1 for true:

```wl
TTermVal[TWnf[TOp2["<", 3, 4]]]
```

<!-- => 1 -->

## Possible Issues

Arithmetic is unsigned 32-bit, so a negative difference reads as its 32-bit two's complement:

```wl
TTermVal[TWnf[TNum[3] - TNum[5]]]
```

<!-- => 4294967294 -->

---

A negative integer is stored as its 32-bit two's complement too:

```wl
TTermVal[TNum[-5]]
```

<!-- => 4294967291 -->

---

An integer that does not fit in 32 bits is refused:

```wl
TNum[2^40]
```

<!-- => the message TNum::range is issued and TNum[1099511627776, "i32"] returns unevaluated -->

---

A dtype name that does not exist is refused:

```wl
TNum[3, "q7"]
```

<!-- => the message TNum::dtype is issued and the expression returns unevaluated -->

---

A real number is not a number term and returns unevaluated:

```wl
TNum[1.5]
```

<!-- => TNum[1.5] -->
