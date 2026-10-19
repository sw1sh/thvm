---
Template: Symbol
Name: TContext
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TContext
Keywords: [context, runtime context, slot, snapshot, persist, heap]
SeeAlso: [TContextSnapshot, TInitialize, TContextStrip, TTerm, Term, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TContext]()[*n*]</code> represents the runtime context in slot *n*.

<code>[TContext]()[<|"Root" -> *root*, "Cells" -> *cells*, …|>]</code> represents a portable snapshot of a runtime context.

<!-- #| annotation: 26.10.19: Two roles share the head: TContext[slot] is the handle defined in Kernel/Context.wl (summary box in Format.wl), and TContext[<|...|>] is the snapshot container of Kernel/Heap.wl, which has no summary box. TContextNew, TContextCurrent, TContextList, TContextDestroy, TInContext and $TContext have no reference pages yet, so they are written as code rather than linked. Building terms inside a second context is not shown: Term, THeapRead and the other heap walkers read the current context, not the term's own, so a term from another slot reads the wrong heap. TInContext with an unallocated slot silently stays in the current context. -->

## Details & Options

- A runtime context holds a heap, a book heap, named definitions, tensors, compiled kernels and compute backends.
- Slot 0 is the default context, which every computation uses unless another one is current. `TContextNew[]` allocates slots 1 through 15, `TInContext[ctx, expr]` evaluates *expr* with *ctx* current, `TContextCurrent[]` gives the current context and `TContextDestroy[ctx]` frees a context other than slot 0.
- A context handle displays as a summary box with its slot, whether it is current, the size of its heap, and its numbers of tensors and kernels.
- Every [TTerm]() records the slot of the context it was built in as its `"ctx"` property.
- A snapshot is made by [TContextSnapshot](), restored by [TInitialize](), and stripped of its tensor data by [TContextStrip]().
- The parts of a snapshot:

| | |
| --- | --- |
| `"Root"` | the entry term as a heap cell, or <code>[Missing]()["NoRoot"]</code> |
| `"Cells"` | the cells of the heap, an [Association]() from location to <code>[Term]()["*tag*", *ext*, *val*]</code> |
| `"BookCells"` | the cells of the book heap |
| `"Tensors"` | the tensor data, an [Association]() from slot to [NumericArray](), or to shape and dtype after [TContextStrip]() |
| `"Defs"` | the named definitions |
| `"AloStates"` | the states of lazy allocations |
| `"Labels"` | the value of the fresh-label counter |
| `"State"` | `"Initialized"` when every tensor has its data, `"Uninitialized"` when none has, and `"Mixed"` otherwise |

- A snapshot is an ordinary expression: it can be stored and serialized. With its book cells, definitions and allocation states it restores in a fresh session.

## Basic Examples

The handle of the default context, slot 0:

```wl
TContext[0]
```

<!-- => a TContext summary box showing slot 0, current yes, and the heap size, tensor count and kernel count -->

---

Start from an empty heap so the snapshot holds only this example:

```wl
TReset[];
```

A snapshot of the heap behind a sum:

```wl
TContextSnapshot[TNum[3] + TNum[4]]
```

<!-- => TContext[<|"Root" -> Term["OP2", "+", 0], "Cells" -> <|0 -> Term["NUM", "i32", 3], 1 -> Term["NUM", "i32", 4]|>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

## Scope

A term built in the default context records slot 0:

```wl
TNum[1]["ctx"]
```

<!-- => 0 -->

---

Allocate a new context on the central processing unit (CPU):

```wl
ctx = TContextNew["cpu"]
```

<!-- => a TContext summary box for the new slot, TContext[1] when no other context is allocated -->

Its heap is separate from the default one and starts empty:

```wl
TInContext[ctx, THeapPos[]]
```

<!-- => 0 -->

Free it again, which gives its slot:

```wl
TContextDestroy[ctx]
```

<!-- => 1 -->

---

The parts of a snapshot:

```wl
Keys[First[TContextSnapshot[]]]
```

<!-- => {"Root", "Cells", "BookCells", "Tensors", "Defs", "AloStates", "Labels", "State"} -->

---

A snapshot taken without a root records none:

```wl
First[TContextSnapshot[]]["Root"]
```

<!-- => Missing["NoRoot"] -->

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

The snapshot survives serialization unchanged:

```wl
BinaryDeserialize[BinarySerialize[snap]] === snap
```

<!-- => True -->

[TInitialize]() restores it as a live lambda, which adds 1 to its argument:

```wl
TTermVal[TWnf[TApp[TInitialize[snap], TNum[41]]]]
```

<!-- => 42 -->

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

A snapshot holds every cell of the heap, so the snapshot of a sum also holds the lambda's cell 0:

```wl
Keys[First[TContextSnapshot[TNum[3] + TNum[4]]]["Cells"]]
```

<!-- => {0, 1, 2} -->
