---
Template: Symbol
Name: TContextStrip
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TContextStrip
Keywords: [context, snapshot, strip, tensors, shape only]
SeeAlso: [TContext, TContextSnapshot, TInitialize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TContextStrip]()[*snap*]</code> gives the snapshot *snap* with the data of every tensor replaced by its shape and dtype.

## Details & Options

- Each [NumericArray]() under `"Tensors"` becomes an association with the keys `"shape"` and `"dtype"`, the dtype being `"f32"` or `"i32"`.
- The root, the cells and the other parts of *snap* are kept, and `"State"` becomes `"Uninitialized"`.
- [TContextStrip]() works on the expression alone and does not touch the runtime.
- A stripped snapshot keeps the structure of a computation without its data, so it is much smaller when the tensors are large.
- <code>[TInitialize]()[*snap*, "ZeroFill" -> True]</code> restores a stripped snapshot with zero-filled tensors of the recorded shapes.
- A snapshot without tensors comes back unchanged, and input that is not a snapshot returns unevaluated.

## Basic Examples

Start from an empty heap:

```wl
TReset[];
```

Snapshot a sum of two tensors:

```wl
snap = TContextSnapshot[TTensorCreate[{1., 2., 3.}] + TTensorCreate[{4., 5., 6.}]]
```

<!-- => TContext[<|"Root" -> Term["UOP", "ADD", 1], "Cells" -> <|1 -> Term["TEN", "f32", 0], 2 -> Term["TEN", "f32", 1]|>, "BookCells" -> <||>, "Tensors" -> <|0 -> NumericArray[{1., 2., 3.}, "Real32"], 1 -> NumericArray[{4., 5., 6.}, "Real32"]|>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

Strip the tensor data:

```wl
TContextStrip[snap]
```

<!-- => TContext[<|"Root" -> Term["UOP", "ADD", 1], "Cells" -> <|1 -> Term["TEN", "f32", 0], 2 -> Term["TEN", "f32", 1]|>, "BookCells" -> <||>, "Tensors" -> <|0 -> <|"shape" -> {3}, "dtype" -> "f32"|>, 1 -> <|"shape" -> {3}, "dtype" -> "f32"|>|>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Uninitialized"|>] -->

The tensors are now described by shape and dtype:

```wl
Dataset[First[TContextStrip[snap]]["Tensors"]]
```

<!-- => a Dataset with the rows 0 and 1, each with shape {3} and dtype "f32" -->

The stripped snapshot is marked as having no tensor data:

```wl
First[TContextStrip[snap]]["State"]
```

<!-- => "Uninitialized" -->

## Scope

Start from an empty heap:

```wl
TReset[];
```

Snapshot a computation on a tensor of 1000 elements:

```wl
big = TContextSnapshot[TTensorCreate[ConstantArray[1., {1000}]] * 2.]
```

<!-- => a TContext snapshot whose "Tensors" part holds a NumericArray of 1000 ones -->

The full snapshot takes this many bytes:

```wl
ByteCount[big]
```

<!-- => 7992 -->

The stripped snapshot takes half as many:

```wl
ByteCount[TContextStrip[big]]
```

<!-- => 3992 -->

---

Start from an empty heap:

```wl
TReset[];
```

Snapshot a lambda, which refers to no tensors:

```wl
s = TContextSnapshot[TLam[x, x + 1]]
```

<!-- => TContext[<|"Root" -> Term["LAM", 0, 1], "Cells" -> <|1 -> Term["OP2", "+", 2], 2 -> Term["VAR", 0, 1], 3 -> Term["NUM", "i32", 1]|>, "BookCells" -> <||>, "Tensors" -> <||>, "Defs" -> <||>, "AloStates" -> {}, "Labels" -> 1, "State" -> "Initialized"|>] -->

Stripping it changes nothing:

```wl
TContextStrip[s] === s
```

<!-- => True -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

Snapshot a sum of two tensors:

```wl
snap = TContextSnapshot[TTensorCreate[{1., 2., 3.}] + TTensorCreate[{4., 5., 6.}]]
```

<!-- => TContext[<|"Root" -> Term["UOP", "ADD", 0], ..., "State" -> "Initialized"|>] with the data of both tensors -->

The cells are the same in the stripped snapshot:

```wl
First[TContextStrip[snap]]["Cells"] === First[snap]["Cells"]
```

<!-- => True -->

The full snapshot restores the sum with its data:

```wl
Normal[TRealize[TInitialize[snap]]]
```

<!-- => {5., 7., 9.} -->

The stripped snapshot restores the same sum over zero-filled tensors:

```wl
Normal[TRealize[TInitialize[TContextStrip[snap], "ZeroFill" -> True]]]
```

<!-- => {0., 0., 0.} -->

## Possible Issues

A context handle is not a snapshot and returns unevaluated:

```wl
TContextStrip[TContext[0]]
```

<!-- => TContextStrip[TContext[0]], unevaluated, with TContext[0] shown as a summary box -->
