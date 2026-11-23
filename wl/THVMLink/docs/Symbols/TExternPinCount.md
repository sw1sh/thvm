---
Template: Symbol
Name: TExternPinCount
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TExternPinCount
Keywords: [pin, garbage collection, root, term handle, heap]
SeeAlso: [TTerm, TGCCollect, TReset, THeapPos]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TExternPinCount]()[]</code> gives the number of entries in the runtime's table of terms pinned by the Wolfram Language.

<!-- #| annotation: 26.11.23: The pin table is the C-side root set (src/schedule/extern_pin.c). Every TTerm wrapper carries a ManagedLibraryExpression["ExternPin"] handle created in makePinHandle, and the library manager (extern_pin_manager in wl/THVMLink/CSource/thvmlink.c) unpins when the Wolfram Language releases the handle, which it does when the last reference to the wrapper goes: Clear, the end of a Module scope, a constructor returning and dropping the wrappers of its child terms (wl/THVMLink/Tests/extern_pin.wlt). So the count is the number of wrappers still held, not of wrappers ever built, and it is per wrapper, not per cell. In wl -f probes the wrappers a top-level evaluation produced without binding them to a variable were still counted in the evaluation right after it (50 after Table[TSup[i, i + 1], {i, 50}];, 1 after TSup[1, 2], 1 after Do[TEra[], {100}]) and gone in the one after that, so the examples hold their terms in variables and release them with Clear or a Module scope. TTermUnpin on a wrapper also clears the wrapper's handle entry, so dropping the wrapper later does not unpin a second time. -->

## Details & Options

- Each [TTerm]() held by the Wolfram Language pins its heap cell, so that a garbage collection with <code>[TGCCollect]()</code> keeps the cell and everything reachable from it.
- Every [TTerm]() wrapper holds one pin for as long as the Wolfram Language holds the wrapper. The pin is released when the wrapper is dropped, for example when its variable is cleared with [Clear]() or its [Module]() scope ends.
- The wrappers a constructor builds for the children of a term are dropped when it returns, so after <code>[TSup]()[1, 2]</code> only the wrapper of the superposition holds a pin.
- Two wrappers of the same term hold two pins.
- <code>[TReset]()</code> empties the table, and a pin can be released by hand, which lowers the count by one. Dropping a wrapper whose pin was released by hand releases nothing more.

## Basic Examples

Start from an empty pin table:

```wl
TReset[];
```

The table is empty:

```wl
TExternPinCount[]
```
<!-- => 0 -->

Build an eraser term:

```wl
e = TEra[]
```
<!-- => a TTerm summary box with tag ERA -->

The new term holds one pin:

```wl
TExternPinCount[]
```
<!-- => 1 -->

## Scope

Start again from an empty pin table:

```wl
TReset[];
```

A superposition of two numbers builds three terms, the two numbers and the superposition:

```wl
s = TSup[1, 2]
```
<!-- => a TTerm summary box with tag SUP -->

The wrappers of the two numbers were dropped when the constructor returned, so only the superposition holds a pin:

```wl
TExternPinCount[]
```
<!-- => 1 -->

---

Start again from an empty pin table:

```wl
TReset[];
```

Build a wrapper of the eraser:

```wl
e1 = TEra[]
```
<!-- => a TTerm summary box with tag ERA -->

Build a second wrapper of the eraser:

```wl
e2 = TEra[]
```
<!-- => a TTerm summary box with tag ERA -->

They refer to the same term:

```wl
e1 == e2
```
<!-- => True -->

Each wrapper holds its own pin:

```wl
TExternPinCount[]
```
<!-- => 2 -->

---

Start again from an empty pin table:

```wl
TReset[];
```

Hold a term in a variable:

```wl
e = TEra[];
```

Its wrapper holds one pin:

```wl
TExternPinCount[]
```
<!-- => 1 -->

Clear the variable, which drops the wrapper:

```wl
Clear[e];
```

The pin is released with it:

```wl
TExternPinCount[]
```
<!-- => 0 -->

---

Start again from an empty pin table:

```wl
TReset[];
```

A wrapper local to a [Module]() holds its pin inside the scope:

```wl
Module[{t = TEra[]}, TExternPinCount[]]
```
<!-- => 1 -->

The pin is released when the scope ends:

```wl
TExternPinCount[]
```
<!-- => 0 -->

---

Start again from an empty pin table:

```wl
TReset[];
```

Build a term:

```wl
t = TEra[]
```
<!-- => a TTerm summary box with tag ERA -->

Release its pin by hand:

```wl
TTermUnpin[t]
```
<!-- => 1 -->

The count goes back to zero:

```wl
TExternPinCount[]
```
<!-- => 0 -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

A term held by the Wolfram Language is pinned:

```wl
x = TSup[1, 2] + 3
```
<!-- => a TTerm summary box with tag OP2 -->

A garbage collection packs the live cells at the start of the heap and gives the new heap position:

```wl
TGCCollect[]
```
<!-- => 5 -->

The pinned term survives the collection:

```wl
TTermVal /@ TCollapse[x]
```
<!-- => {4, 5} -->
