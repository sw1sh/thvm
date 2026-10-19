---
Template: Symbol
Name: TInitialize
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TInitialize
Keywords: [snapshot, restore, context, canonical form, persist, round trip]
SeeAlso: [TContextSnapshot, TContextStrip, TContext, Term, TInit, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TInitialize]()[*snapshot*]</code> restores the [TContext]() snapshot *snapshot* into the runtime and gives its root as a term.

<code>[TInitialize]()[*expr*]</code> builds the term whose canonical form is the [Term]() expression *expr*.

<!-- #| annotation: 26.10.19: The snapshot form (Heap.wl) resets the book heap, DEFS and ALO_STATES when the snapshot bundles them, then TResets the dynamic heap, reallocates the tensors (renumbered densely, so a snapshot survives TFree and a fresh kernel), writes the cells back from location 0, restores the label counter and repacks the root. The Term form walks the expression bottom-up through the public constructors with Unique[] binders, so binder ids are alpha-renamed; it has rules for NUM, ERA, ANY, REF, TEN, VAR, LAM, APP, SUP, OP2, DUP, DP0 and DP1 only, so CTR and MAT come back unevaluated. An orphan VAR inside a lambda, as in TInitialize[Term["LAM", 5, Term["VAR", 99]]], issues TInitialize::orphanvar and then THeapSet::loc for location 0, and still gives a LAM term (reported 26.10.19). -->

## Details & Options

- A snapshot made by <code>[TContextSnapshot]()</code> holds the heap cells, the tensors with their data, the label counter and, when present, the book heap and the named definitions.
- Restoring a snapshot first empties the runtime, as <code>[TReset]()</code> does, then writes its cells and tensors back and gives its root as a [TTerm](). Terms built before the restore are no longer valid.
- The runtime does not need to be the one that took the snapshot: a snapshot restores after the runtime has been shut down and initialized again.
- A snapshot with no root gives <code>[Missing]()["NoRoot"]</code>.
- A snapshot whose tensor data was removed by <code>[TContextStrip]()</code> is restored only with the option `"ZeroFill" -> True`, which fills its tensors with zeros. Without it, the message `TInitialize::uninit` is issued and the input returns unevaluated.
- <code>[TInitialize]()[*expr*]</code> inverts <code>[Term]()</code> for numbers, the eraser, lambdas, applications, superpositions, arithmetic, duplications and tensors. Lambda and duplication binders are renamed, so the new term has the same structure as the original but its own heap locations.
- A [TTerm]() is left as it is, and an expression with a constructor or a matcher returns unevaluated. A variable outside the lambda that binds it issues the message `TInitialize::orphanvar` and gives `$Failed`.
- [TInitialize]() takes the following option:

| | | |
| --- | --- | --- |
| `"ZeroFill"` | [False]() | whether to restore a snapshot without tensor data by filling its tensors with zeros |

## Basic Examples

Start from an empty heap, so the snapshot holds only this term:

```wl
TReset[];
```

Take a snapshot of a superposition with 3 added to it:

```wl
snap = TContextSnapshot[TSup[1, 2] + 3]
```
<!-- => TContext[<|"Root" -> Term["OP2", "+", 2], "Cells" -> <|0 -> Term["NUM", "i32", 1], 1 -> Term["NUM", "i32", 2], 2 -> Term["SUP", 1, 0], 3 -> Term["NUM", "i32", 3]|>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 2, "State" -> "Initialized"|>], up to the label numbers -->

Restore it:

```wl
t = TInitialize[snap]
```
<!-- => a TTerm summary box with tag OP2 -->

The restored term reduces as the original did:

```wl
TTermVal /@ TCollapse[t]
```
<!-- => {4, 5} -->

## Scope

Build a term from its canonical form:

```wl
TInitialize[Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]]]
```
<!-- => a TTerm summary box with tag OP2 -->

---

A lambda rebuilt from its canonical form can be applied:

```wl
TTermVal[TWnf[TApp[TInitialize[Term[TLam[x, x + 1]]], TNum[41]]]]
```
<!-- => 42 -->

---

A superposition survives the round trip through its canonical form:

```wl
TTermVal /@ TCollapse[TInitialize[Term[TSup[1, 2] + 3]]]
```
<!-- => {4, 5} -->

---

A projection of a duplication keeps its label:

```wl
Term[TInitialize[Term[First[TDup[3, TNum[4]]]]]]
```
<!-- => Term["DP0", 3, Term["NUM", 4]] -->

---

Start from an empty heap, so the snapshot holds only this term:

```wl
TReset[];
```

A snapshot of a tensor carries its data:

```wl
snap = TContextSnapshot[TTensorCreate[{1., 2., 3.}]]
```
<!-- => TContext[<|"Root" -> Term["TEN", "f32", 0], "Cells" -> <||>, "BookCells" -> <||>, "Tensors" -> <|0 -> NumericArray[{1., 2., 3.}, "Real32"]|>, ..., "State" -> "Initialized"|>] -->

Restore the tensor and read it back:

```wl
Normal[TRealize[TInitialize[snap]]]
```
<!-- => {1., 2., 3.} -->

---

A snapshot taken with no root restores to no term:

```wl
TInitialize[TContextSnapshot[]]
```
<!-- => Missing["NoRoot"] -->

---

Start from an empty heap, so the snapshot holds only this term:

```wl
TReset[];
```

Take a snapshot:

```wl
snap = TContextSnapshot[TSup[1, 2] + 3]
```
<!-- => TContext[<|"Root" -> Term["OP2", "+", 2], "Cells" -> <|0 -> Term["NUM", "i32", 1], ...|>, ...|>] -->

Shut the runtime down:

```wl
TFree[]
```
<!-- => 1 -->

The snapshot restores into the runtime started again:

```wl
TTermVal /@ TCollapse[TInitialize[snap]]
```
<!-- => {4, 5} -->

## Options

### ZeroFill

Start from an empty heap, so the snapshot holds only this term:

```wl
TReset[];
```

Strip the data from a snapshot of a tensor:

```wl
stripped = TContextStrip[TContextSnapshot[TTensorCreate[{1., 2., 3.}]]]
```
<!-- => TContext[<|"Root" -> Term["TEN", "f32", 0], ..., "Tensors" -> <|0 -> <|"shape" -> {3}, "dtype" -> "f32"|>|>, ..., "State" -> "Uninitialized"|>] -->

Without the option, the snapshot is refused:

```wl
TInitialize[stripped]
```
<!-- => the message TInitialize::uninit is issued and the expression returns unevaluated -->

Restore it with its tensor filled with zeros:

```wl
Normal[TRealize[TInitialize[stripped, "ZeroFill" -> True]]]
```
<!-- => {0., 0., 0.} -->

## Properties and Relations

Rebuilding a term from its canonical form gives the same canonical form back:

```wl
Term[TInitialize[Term[TSup[5, 1, 2] + 3]]] === Term[TSup[5, 1, 2] + 3]
```
<!-- => True -->

---

A term is left as it is:

```wl
TInitialize[TNum[3]]
```
<!-- => a TNum summary box showing the number 3 -->

## Possible Issues

The canonical form of a constructor is not rebuilt, and the expression returns unevaluated:

```wl
TInitialize[Term["CTR", 1, Term["NUM", 2]]]
```
<!-- => TInitialize[Term["CTR", 1, Term["NUM", 2]]] -->

---

A variable outside its lambda is refused:

```wl
TInitialize[Term["VAR", 7]]
```
<!-- => the message TInitialize::orphanvar is issued and the result is $Failed -->
