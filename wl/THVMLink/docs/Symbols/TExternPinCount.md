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

<!-- #| annotation: 26.10.19: The pin table is the C-side root set (src/schedule/extern_pin.c). Every TTerm wrapper carries a ManagedLibraryExpression["ExternPin"] handle created in makePinHandle, and the library manager is meant to unpin when the handle is collected. In wl -f probes the count never went down after Clear, ClearSystemCache[] and $HistoryLength = 0, and Tests/extern_pin.wlt only asserts that it does not grow, so the page shows the explicit release (TTermUnpin) and TReset rather than the automatic one. Counts are per wrapper, not per cell: constructors that build child terms add a pin for each wrapper they create. -->

## Details & Options

- Each [TTerm]() held by the Wolfram Language pins its heap cell, so that a garbage collection with <code>[TGCCollect]()</code> keeps the cell and everything reachable from it.
- Every new [TTerm]() wrapper adds one pin, including the wrappers built for the children of a term, so building <code>[TSup]()[1, 2]</code> adds three pins.
- Two wrappers of the same term hold two pins.
- <code>[TReset]()</code> empties the table, and a pin can be released by hand, which lowers the count by one.

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

Each of them holds a pin:

```wl
TExternPinCount[]
```
<!-- => 3 -->

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

A garbage collection moves the live cells and gives the new heap position:

```wl
TGCCollect[]
```
<!-- => 134217732 -->

The pinned term survives the collection:

```wl
TTermVal /@ TCollapse[x]
```
<!-- => {4, 5} -->
