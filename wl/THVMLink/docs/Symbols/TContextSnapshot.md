---
Template: Symbol
Name: TContextSnapshot
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TContextSnapshot
Keywords: [snapshot, serialization, context, restart, heap]
SeeAlso: [TInitialize, TContextStrip, TContextToTermTree, TContext, TInContext]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TContextSnapshot]()[]</code> returns a `TContext[<|...|>]` capturing every cell in `[0, THeapPos[])`, every book cell in `[1, BookPos[])`, all referenced tensors with data, the `DEFS` table (names interned in `TDef`), and the `ALO` substitution chain.

<code>[TContextSnapshot]()[$root\_TTerm$]</code> additionally records $root$ as the snapshot's entry point.

## Details & Options

- The snapshot is a self-contained Wolfram expression - `BinarySerialize`-able, sendable across kernels, and replayable after a `TFree` + `TInit` cycle.
- When `BookCells`, `Defs`, and `AloStates` are non-empty, the snapshot is fully self-contained and survives a fresh kernel.
- <code>[TInitialize]()[snap]</code> restores `snap` into the live runtime and returns the recorded root as a live `TTerm` (or `Missing["NoRoot"]` if no root was recorded).
- <code>[TContextStrip]()</code> drops the tensor `NumericArray` buffers (replacing each with `<|"shape", "dtype"|>`) to ship a small portable copy of the term shape.
- <code>[TContextToTermTree]()</code> renders the snapshot's term tree without re-initializing the runtime.

## Basic Examples

Snapshot a small lambda and restore it after a fresh init:

```wl
#| eval: False
Needs["THVMLink`"];
TInit[];
snap = TContextSnapshot @ TLam[x, TUOpAdd[x, x]];
TFree[]; TInit[];
TInitialize[snap]
```
<!-- => TTerm[<LAM with UOP_ADD body>] live in the freshly initialized runtime -->

## Scope

Strip the tensor data for a portable shape-only snapshot:

```wl
snap = TContextSnapshot @ (TTensorCreate[{1., 2., 3.}] + TTensorCreate[{4., 5., 6.}]);
TContextStrip[snap]
```
<!-- => a TContext where every tensor entry is <|"shape" -> _, "dtype" -> _|> -->

## Applications

Inspect a snapshot's term tree without re-entering the runtime - the rendering walks the snapshot's `Cells` list, not the live heap:

```wl
TContextToTermTree @ TContextSnapshot @ TLam[x, TUOpMul[x, x]]
```
<!-- => "LAM"["VAR"[id], "UOP_MUL"["VAR"[id], "VAR"[id]]] -->

## Properties and Relations

A snapshot's `"Cells"` length agrees with `THeapPos[] - THeapBase[]` at snapshot time:

```wl
t       = TTensorCreate[{1.}] + TTensorCreate[{2.}];
snap    = TContextSnapshot[t];
liveLen = THeapPos[] - THeapBase[];
Length[Lookup[First @ snap, "Cells"]] === liveLen
```
<!-- => True -->

## Possible Issues

Cross-kernel restore requires `"ZeroFill" -> True` when the snapshot's tensors were stripped:

```wl
#| eval: False
snap = TContextStrip @ TContextSnapshot @ (TTensorCreate[{1.}] + TTensorCreate[{2.}]);
TFree[]; TInit[];
TInitialize[snap, "ZeroFill" -> True]
```
<!-- => the restored TTerm with zero-filled tensor buffers -->
