---
Template: Symbol
Name: TTermExt
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermExt
Keywords: [ext, term, inspector, packed, label, opcode, dtype]
SeeAlso: [TTerm, TTermTag, TTermVal, TTermSub, TSup, TDup]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermExt]()[*term*]</code> gives the EXT field of *term*, an [Integer]().

## Details & Options

- *term* is a [TTerm]() or its packed [Integer]() value.
- The EXT field is 18 bits wide, and its meaning depends on the tag of *term*.
- For a SUP, a DUP projection (DP0 or DP1) or a CTR it is the label.
- For an OP2 it is the operator code: 0 for `"+"`, 1 for `"-"`, 2 for `"*"`, 3 for `"=="` and 4 for `"<"`.
- For a MAT it is the value the matcher tests for.
- For a NUM it is the dtype code: 5 for `"i32"` and 13 for `"f32"`.
- For a LAM whose binder does not occur in its body, bit 17 is set, giving the value 131072.
- A SUP built without an explicit label takes the next fresh label, as from [TFreshLabel]().

## Basic Examples

The label of a superposition built with the explicit label 9:

```wl
TTermExt[TSup[9, 5, 7]]
```

<!-- => 9 -->

---

The operator code of a multiplication:

```wl
TTermExt[TOp2["*", 3, 4]]
```

<!-- => 2 -->

## Scope

An `"i32"` number has dtype code 5:

```wl
TTermExt[TNum[42]]
```

<!-- => 5 -->

---

An `"f32"` number has dtype code 13:

```wl
TTermExt[TNum[1065353216, "f32"]]
```

<!-- => 13 -->

---

A matcher carries the value it tests for:

```wl
TTermExt[TMatNum[3, TNum[1], TLam[k, TNum[0]]]]
```

<!-- => 3 -->

---

A constructor carries its label:

```wl
TTermExt[TCtr[7, 1, 2]]
```

<!-- => 7 -->

---

Both projections of a duplication carry its label:

```wl
TTermExt /@ TDup[5, TNum[1]]
```

<!-- => {5, 5} -->

---

A lambda that uses its binder has EXT 0:

```wl
TTermExt[TLam[x, x]]
```

<!-- => 0 -->

---

A lambda that ignores its binder has bit 17 set:

```wl
TTermExt[TLam[x, TNum[1]]]
```

<!-- => 131072 -->

## Properties and Relations

Start from an empty heap, which resets the fresh-label counter:

```wl
TReset[];
```

A superposition without an explicit label takes label 1:

```wl
TTermExt[TSup[5, 7]]
```

<!-- => 1 -->

The next one takes label 2:

```wl
TTermExt[TSup[5, 7]]
```

<!-- => 2 -->

---

The label decides how superpositions combine: two with the same label pair their branches:

```wl
TTermVal /@ TCollapse[TSup[1, 1, 2] + TSup[1, 10, 20]]
```

<!-- => {11, 22} -->

---

Two superpositions with different labels give every combination:

```wl
TTermVal /@ TCollapse[TSup[1, 1, 2] + TSup[2, 10, 20]]
```

<!-- => {11, 21, 12, 22} -->
