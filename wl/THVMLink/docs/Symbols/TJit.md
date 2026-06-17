---
Template: Symbol
Name: TJit
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TJit
Keywords: [JIT, closure, capture, replay, dispatch]
SeeAlso: [TJitOpCount, TSet, TApp, TLam, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TJit]()[*fn*]</code> returns a closure that captures *fn*'s kernel-dispatch sequence on its first call and replays it on subsequent calls.

## Details & Options

- [HoldFirst](). Build *fn* as <code>[Function]()[{}, [TRealize]() @ *expr over fixed tensors*]</code>; the first `f[]` captures and returns the result, and each later `f[]` replays just the dispatch (no re-materialize), updating the captured result in place.
- To feed new inputs, mutate a captured tensor with <code>[TSet]()</code>, then call `f[]` again; the previously returned result reflects the new values.
- Read the number of captured dispatches with <code>[TJitOpCount]()</code> (0 before the first call); recapture with <code>[TJitDrop]()[*f*]</code>.
- This is distinct from the CPU backend's per-kernel "jit" dispatch kind, which clang-compiles each kernel on dispatch regardless of [TJit]().

## Basic Examples

Build a squaring closure. Before its first call it is *uncaptured* - the summary box shows a gray play-button and zero captured dispatches:

```wl
xBasic = TTensorCreate[{1., 2., 3., 4.}];
fBasic = TJit[Function[{}, TRealize @ (xBasic*xBasic)]]
```
<!-- => TJitClosure summary box: captured: no, ops: 0 -->

The first call captures the dispatch sequence and returns the result:

```wl
Normal @ fBasic[]
```
<!-- => {1., 4., 9., 16.} -->

The closure now reports its captured state - the play-button greens up and it carries one dispatch:

```wl
fBasic
```
<!-- => TJitClosure summary box: captured: yes, ops: 1 -->

## Scope

Mutate the captured input with <code>[TSet]()</code> and replay; the result updates in place:

```wl
xScope = TTensorCreate[{1., 2., 3., 4.}];
fScope = TJit[Function[{}, TRealize @ (xScope*xScope)]];
out = fScope[];
TSet[xScope, TTensorCreate[{10., 20., 30., 40.}]];
fScope[];
Normal @ out
```
<!-- => {100., 400., 900., 1600.} -->

The closure holds the captured step, ready to replay on whatever the buffer next holds:

```wl
fScope
```
<!-- => TJitClosure summary box: captured: yes, ops: 1 -->

## Properties and Relations

The summary box previews the captured dispatch count at a glance - here, one:

```wl
xProp = TTensorCreate[{1., 2., 3., 4.}];
fProp = TJit[Function[{}, TRealize @ (xProp*xProp)]];
fProp[];
fProp
```
<!-- => TJitClosure summary box: captured: yes, ops: 1 -->

<code>[TJitOpCount]()</code> reads that same count as a plain integer:

```wl
TJitOpCount[fProp]
```
<!-- => 1 -->
