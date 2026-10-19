---
Template: Symbol
Name: TMultiTraceQ
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMultiTraceQ
Keywords: [multicomputation, trace, event recording, library]
SeeAlso: [TMultiTrace, TMultiwayGraph, TCausalGraph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMultiTraceQ]()[]</code> gives [True]() if the loaded THVMLink runtime can record reduction events, and [False]() otherwise.

<!-- #| annotation: 26.10.19: TMultiTraceQ reports whether the dylib was compiled with THVM_TRACE; make wl builds with it by default, and WL_TRACE=0 opts out for benchmarking. -->

## Details & Options

- The runtime THVMLink ships with records events, so [TMultiTraceQ]() gives [True]().
- A runtime built without event recording gives [False](). [TMultiTrace]() then issues `TMultiTrace::notrace` and gives `$Failed`. [TMultiwayGraph]() and [TCausalGraph]() of a term run [TMultiTrace](), so they need recording too.
- [TMultiTraceQ]() initializes the runtime if needed and does not change its state.

## Basic Examples

The loaded runtime records events:

```wl
TMultiTraceQ[]
```

<!-- => True -->

## Properties and Relations

With recording available, [TMultiTrace]() gives one record per step, here the step numbers:

```wl
TMultiTrace[TNum[3] + TNum[4], "Step"]
```

<!-- => {0, 1} -->

---

[TMultiwayGraph]() draws the recorded steps:

```wl
TMultiwayGraph[TSup[1, 2] + 3]
```

<!-- => a Graph from the vertex <1, 2> + 3 down two branches, OP2_SUP then DUP_NUM then OP2_NUM_NUM, ending at the vertices 4 and 5 -->
