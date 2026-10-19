---
Template: Symbol
Name: TTermVal
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermVal
Keywords: [val, term, inspector, packed, heap location, number value]
SeeAlso: [TTerm, TTermTag, TTermExt, TTermSub, THeapRead, TWnf]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermVal]()[*term*]</code> gives the VAL field of *term*, an [Integer]().

## Details & Options

- *term* is a [TTerm]() or its packed [Integer]() value.
- The VAL field is 38 bits wide, and its meaning depends on the tag of *term*.
- For a NUM it is the number: an `"i32"` number holds the unsigned 32-bit pattern of its value, an `"f32"` number the bit pattern of the float.
- For a compound term such as an APP, LAM, SUP, OP2, MAT or CTR it is the heap location of the first child, which [THeapRead]() reads.
- For a VAR it is the heap location of its binder, and for a DP0 or DP1 the location of its duplication.
- For a tensor handle it is the tensor id.
- <code>[TTermVal]()[[TWnf]()[*term*]]</code> reads the value of an integer computation.

## Basic Examples

A number holds its value in the VAL field:

```wl
TTermVal[TNum[42]]
```

<!-- => 42 -->

---

The value of a reduced sum:

```wl
TTermVal[TWnf[TNum[3] + TNum[4]]]
```

<!-- => 7 -->

## Scope

An `"f32"` number holds the bit pattern of the float, here the pattern of 1.0:

```wl
TTermVal[TNum[1065353216, "f32"]]
```

<!-- => 1065353216 -->

---

Start from an empty heap so the locations are predictable:

```wl
TReset[];
```

Build an application, whose lambda takes heap cell 0 and whose two children go to cells 1 and 2:

```wl
t = TApp[TLam[x, x], TNum[5]]
```

<!-- => a TTerm summary box showing tag APP@1 -->

The VAL field of the application is the location of its first child:

```wl
TTermVal[t]
```

<!-- => 2 -->

The cell at that location holds the function:

```wl
Term[THeapRead[TTermVal[t]]]
```

<!-- => Term["LAM", 1, Term["VAR", 1]] -->

The next cell holds the argument:

```wl
Term[THeapRead[TTermVal[t] + 1]]
```

<!-- => Term["NUM", 5] -->

---

Bind a lambda:

```wl
f = TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM -->

Its body is the variable, read from the cell the lambda points at:

```wl
v = THeapRead[TTermVal[f]]
```

<!-- => a TTerm summary box showing tag VAR -->

The VAL field of the variable is the location of its binder, the lambda's own cell:

```wl
TTermVal[v] == TTermVal[f]
```

<!-- => True -->

## Properties and Relations

The VAL field matches the `"val"` property of a [TTerm]():

```wl
TNum[42]["val"]
```

<!-- => 42 -->

## Possible Issues

Integer arithmetic is unsigned 32-bit, so a negative result reads as its 32-bit two's complement:

```wl
TTermVal[TWnf[TOp2["-", 3, 4]]]
```

<!-- => 4294967295 -->

---

A result past the 32-bit range wraps around:

```wl
TTermVal[TWnf[TNum[4294967295] + 1]]
```

<!-- => 0 -->
