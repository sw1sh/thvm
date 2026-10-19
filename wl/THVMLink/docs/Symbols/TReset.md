---
Template: Symbol
Name: TReset
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TReset
Keywords: [reset, heap, counters, labels, fresh start]
SeeAlso: [TInit, TItrs, TFreshLabel, THeapPos, TStack]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TReset]()[]</code> rewinds the heap of the current runtime context to empty and restarts its counters.

<code>[TReset]()[*ctx*]</code> resets the runtime context *ctx* in the same way.

## Details & Options

- [TReset]() gives 1.
- The heap is rewound, so the next term starts at location 1 and [THeapPos]() gives 1; location 0 never holds a cell.
- The interaction counter of [TItrs]() goes back to 0, the pending frames of [TStack]() are dropped, and the fresh-label counter of [TFreshLabel]() restarts at 1.
- The tensors and compiled kernels that belong to terms on the heap are released, and tensor ids restart at 1.
- Constructors stored in the book heap with [TBookCtr]() survive the reset.
- Cells are reused, not cleared: a [TTerm]() made before the reset refers to cells that the next terms overwrite.
- *ctx* is a [TContext]() handle, such as `TContext[0]` for the default context. <code>[TReset]()[*ctx*]</code> resets that context and leaves the current context as it is.
- [TInit]() initializes the runtime; [TReset]() starts a fresh computation in a runtime that is already initialized.

## Basic Examples

Reset the heap:

```wl
TReset[]
```

<!-- => 1 -->

The heap is now empty:

```wl
THeapPos[]
```

<!-- => 1 -->

## Scope

Build a term on the heap of the default context:

```wl
TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM -->

Reset the default context by its handle:

```wl
TReset[TContext[0]]
```

<!-- => 1 -->

The heap is empty again:

```wl
THeapPos[]
```

<!-- => 1 -->

---

Start from an empty heap:

```wl
TReset[];
```

Reduce an application of a lambda:

```wl
TTermVal[TWnf[TApp[TLam[x, x + 1], TNum[41]]]]
```

<!-- => 42 -->

The reduction fired two interactions:

```wl
TItrs[]
```

<!-- => 2 -->

Reset again:

```wl
TReset[];
```

The interaction counter is back to 0:

```wl
TItrs[]
```

<!-- => 0 -->

---

Start from an empty heap:

```wl
TReset[];
```

A lambda takes the first heap cell:

```wl
TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM@0 -->

The heap now holds one cell:

```wl
THeapPos[]
```

<!-- => 2 -->

---

Start from an empty heap:

```wl
TReset[];
```

The first fresh label is 1:

```wl
TFreshLabel[]
```

<!-- => 1 -->

The next is 2:

```wl
TFreshLabel[]
```

<!-- => 2 -->

Reset again:

```wl
TReset[];
```

The labels start over:

```wl
TFreshLabel[]
```

<!-- => 1 -->

---

Start from an empty heap:

```wl
TReset[];
```

Tensor ids start at 1 after a reset:

```wl
TTermVal[TTensorCreate[{1., 2.}]]
```

<!-- => 1 -->

## Properties and Relations

Store a constructor with the field 42 in the book heap:

```wl
b = TBookCtr[3, TNum[42]]
```

<!-- => a TTerm summary box showing tag CTR and ext 3 -->

Reset the heap:

```wl
TReset[];
```

The field survives the reset, in the cell after the constructor's child count:

```wl
TTermVal[TBookRead[TTermVal[b] + 1]]
```

<!-- => 42 -->

---

Reset the heap:

```wl
TReset[];
```

No eliminator frames are pending after a reset:

```wl
TStack[]
```

<!-- => {} -->

## Possible Issues

Start from an empty heap:

```wl
TReset[];
```

Bind a lambda, which takes the first heap cell:

```wl
f = TLam[x, x + 1]
```

<!-- => a TTerm summary box showing tag LAM@0 -->

Reset the heap:

```wl
TReset[];
```

A new term reuses the first cell:

```wl
g = TLam[y, TNum[7]]
```

<!-- => a TTerm summary box showing tag LAM@0 -->

The handle made before the reset now reads the new term's body:

```wl
Term[f]
```

<!-- => Term["LAM", 1, Term["NUM", 7]] -->
