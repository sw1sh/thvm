---
Template: Symbol
Name: TRedexes
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TRedexes
Keywords: [redex, list, heap, multiway, debug]
SeeAlso: [TInteract, TStep, THeapDiagram]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TRedexes]()[]</code> lists every redex in the live heap.

<code>[TRedexes]()[*t*]</code> additionally DFS-walks *t* so a root the caller holds directly (which may not be stored in any heap cell yet) is included in the enumeration.

## Details & Options

- Each entry is a `TTerm` uniquely identifying the redex by its packed `Term` value, suitable as the argument to [TInteract]().
- A redex is any active-pair the runtime can fire next: an APP whose function is a LAM, a DP-projection over a SUP / LAM / CTR body, an OP2 with NUM operands, ...
- Multiway tracers ([TMultiTrace]()) and interactive explorers consume the list to pick which fire to drive next.

## Basic Examples

A single beta redex sitting on the heap:

```wl
TReset[];
Length @ TRedexes[TApp[TLam[x, x], TNum[5]]]
```
<!-- => 1 -->

## Properties and Relations

`TRedexes` is enumeration; [TInteract]() is fire. Pair them when you want to explore a reduction off the head-driven path that [TStep]() / [TWnf]() take.
