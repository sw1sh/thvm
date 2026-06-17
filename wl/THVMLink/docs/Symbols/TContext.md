---
Template: Symbol
Name: TContext
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TContext
Keywords: [context, snapshot, persist, container]
SeeAlso: [TContextSnapshot, TInitialize, TContextStrip, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TContext]()[<|"Root", "Cells", "BookCells", "Tensors", "Defs", "AloStates", "Labels", "State"|>]</code> is a portable snapshot of the live `thvm` runtime context.

## Details & Options

- Construct via [TContextSnapshot](); restore via [TInitialize]().
- When `"BookCells"`, `"Defs"`, and `"AloStates"` are non-empty, the snapshot is self-contained and survives a fresh kernel (`TFree[]` followed by [TInit]()).
- The container's `MakeBoxes` summary box surfaces the key counts (cell count, tensor count, has-root flag); index into the wrapped `Association` for the raw fields.

## Properties and Relations

[TContextSnapshot]() builds it; [TInitialize]() restores it; [TContextStrip]() returns a tensor-buffer-free shape-only variant; `TContextToTermTree` is a read-only projection that mirrors [TTermExpr]() but operates on the snapshot's cells rather than the live runtime.
