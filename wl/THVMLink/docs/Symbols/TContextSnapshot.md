---
Template: Symbol
Name: TContextSnapshot
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TContextSnapshot
Keywords: [snapshot, serialization, context, restore, heap]
SeeAlso: [TContext, TInitialize, TContextStrip, Term, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TContextSnapshot]()[]</code> gives a [TContext]() snapshot of the current runtime context.

<code>[TContextSnapshot]()[*root*]</code> also records the term *root* as the entry point of the snapshot.

<!-- #| annotation: 26.10.19: TContextToTermTree has no reference page yet, so it is written as code rather than linked. -->

## Details & Options

- The snapshot holds every cell of the heap from its base up to [THeapPos](), every cell of the book heap, the data of every tensor a cell refers to, the named definitions and the states of lazy allocations. [TContext]() lists its parts.
- Each heap cell is recorded as <code>[Term]()["*tag*", *ext*, *val*]</code>, with the EXT field given by name for an integer operator, a tensor operation or a dtype, and a fourth argument 1 when the SUB flag is set.
- Tensor ids are renumbered from 0, so the snapshot does not depend on the runtime's own tensor numbering.
- *root* is recorded in cell form under `"Root"`. Without a root, `"Root"` is <code>[Missing]()["NoRoot"]</code>.
- Taking a snapshot reads the runtime and does not change it.
- <code>[TInitialize]()[*snap*]</code> resets the heap, writes the cells back at the same locations, recreates the tensors, and gives the root as a live [TTerm]().
- [TContextStrip]() replaces the tensor data by shapes and dtypes, and such a snapshot restores with `"ZeroFill" -> True`.
- `TContextToTermTree[snap]` gives the tag-name walk of the root, as [TTermExpr]() does, from the cells of the snapshot without restoring it.

## Basic Examples

Start from an empty heap so the snapshot holds only this example:

```wl
TReset[];
```

A snapshot of the heap behind a lambda, with the lambda as its root:

```wl
TContextSnapshot[TLam[x, x + 1]]
```

<!-- => TContext[<|"Root" -> Term["LAM", 0, 0], "Cells" -> <|0 -> Term["OP2", "+", 1], 1 -> Term["VAR", 0, 0], 2 -> Term["NUM", "i32", 1]|>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

---

Start from an empty heap:

```wl
TReset[];
```

A snapshot of an empty heap without a root:

```wl
TContextSnapshot[]
```

<!-- => TContext[<|"Root" -> Missing["NoRoot"], "Cells" -> <||>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

## Scope

Start from an empty heap:

```wl
TReset[];
```

A snapshot of a tensor sum carries the data of both tensors:

```wl
TContextSnapshot[TTensorCreate[{1., 2., 3.}] + TTensorCreate[{4., 5., 6.}]]
```

<!-- => TContext[<|"Root" -> Term["UOP", "ADD", 0], "Cells" -> <|0 -> Term["TEN", "f32", 0], 1 -> Term["TEN", "f32", 1]|>, "BookCells" -> <||>, "Tensors" -> <|0 -> NumericArray[{1., 2., 3.}, "Real32"], 1 -> NumericArray[{4., 5., 6.}, "Real32"]|>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

---

Start from an empty heap:

```wl
TReset[];
```

A number is an atom, so its snapshot has a root but no cells:

```wl
TContextSnapshot[TNum[5]]
```

<!-- => TContext[<|"Root" -> Term["NUM", "i32", 5], "Cells" -> <||>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

---

Start from an empty heap:

```wl
TReset[];
```

The snapshot records the fresh-label counter, here after one superposition took label 1:

```wl
First[TContextSnapshot[TSup[1, 2]]]["Labels"]
```

<!-- => 2 -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

Snapshot a lambda:

```wl
snap = TContextSnapshot[TLam[x, x + 1]]
```

<!-- => TContext[<|"Root" -> Term["LAM", 0, 0], "Cells" -> <|0 -> Term["OP2", "+", 1], 1 -> Term["VAR", 0, 0], 2 -> Term["NUM", "i32", 1]|>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

Clear the heap:

```wl
TReset[];
```

[TInitialize]() restores the lambda at its old location:

```wl
r = TInitialize[snap]
```

<!-- => a TTerm summary box showing tag LAM@0 -->

The restored lambda has the same canonical form:

```wl
Term[r]
```

<!-- => Term["LAM", 0, Term["OP2", "+", Term["VAR", 0], Term["NUM", 1]]] -->

It adds 1 to its argument:

```wl
TTermVal[TWnf[TApp[r, TNum[41]]]]
```

<!-- => 42 -->

The snapshot survives serialization unchanged:

```wl
BinaryDeserialize[BinarySerialize[snap]] === snap
```

<!-- => True -->

`TContextToTermTree` walks the root through the snapshot's cells:

```wl
TContextToTermTree[snap]
```

<!-- => "LAM"["OP2"["+", "VAR"[0], "NUM"[1]]] -->

## Possible Issues

Start from an empty heap:

```wl
TReset[];
```

Build an unrelated lambda first:

```wl
f = TLam[y, TNum[9]]
```

<!-- => a TTerm summary box showing tag LAM@0 -->

The snapshot of a number has no cells of its own, yet it holds the lambda's cell:

```wl
Keys[First[TContextSnapshot[TNum[5]]]["Cells"]]
```

<!-- => {0} -->
