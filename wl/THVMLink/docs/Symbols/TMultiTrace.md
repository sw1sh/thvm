---
Template: Symbol
Name: TMultiTrace
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMultiTrace
Keywords: [multicomputation, trace, steps, events, slice, superposition]
SeeAlso: [TMultiTraceQ, TMultiwayGraph, TCausalGraph, TCollapse, TStep]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMultiTrace]()[*expr*]</code> evaluates *expr* with event recording on, reduces the resulting term to its values one interaction at a time, and gives a list of records, one per step.

<code>[TMultiTrace]()[*expr*, "*key*"]</code> gives the list of the values of *key*, one per step.

<code>[TMultiTrace]()[*expr*, *keys*]</code> gives records with only the keys in the list *keys*.

<code>[TMultiTrace]()[*expr*, *max*]</code> stops after *max* steps.

<code>[TMultiTrace]()[*expr*, *keys*, *max*]</code> gives records with the keys *keys* for at most *max* steps.

<!-- #| annotation: 26.10.19: Each record carries private _evBefore/_frontier/_anchors/_parent keys during the walk, stripped before return. -->

## Details & Options

- [TMultiTrace]() holds *expr* unevaluated and evaluates it with recording on. Step 0 holds the events that fired while *expr* evaluated, and each later step holds the one interaction a [TStep]() fired.
- The walk follows the order of [TCollapse](): it steps the head until it is a value, then enters the first branch of a superposition and comes back to the second branch afterwards. The walk ends when every branch is a value.
- By default each record has the keys `"CanonicalSlice"` and `"Events"`. *keys* can also be [All](). A key that is not one of those below issues the message `TMultiTrace::key`, and the input returns unevaluated.
- The keys of a record:

| | |
| --- | --- |
| `"Step"` | the step number, starting at 0 |
| `"Term"` | the active term after the step, a [TTerm]() |
| `"ITRS"` | the interaction count of [TItrs]() after the step |
| `"Events"` | the list of events fired in the step |
| `"Slice"` | the live branches of every superposition entered so far, as [TTerm]() handles |
| `"CanonicalSlice"` | the [Term]() forms of the slice |
| `"SliceBoxes"` | the slice typeset in [TraditionalForm](), as boxes |
| `"Diagram"` | a string diagram of the heap around the active term |

- Each event is an [Association]() with the keys `"id"`, `"rule"`, `"ruleCode"`, `"family"`, `"familyCode"`, `"termA"`, `"termB"`, `"deltaLabel"`, `"consumed"` and `"produced"`. `"rule"` names the interaction, such as `"OP2_SUP"`; `"termA"` and `"termB"` are the active pair; `"consumed"` lists the ids of the earlier events whose results the event read, and `"produced"` the heap locations it wrote last.
- `"family"` classifies the interaction: `"TERM"` is a rewrite within one branch, `"SLIDE"` moves an operation into the branches of a superposition, `"FORK"` turns one term into two, `"SPLIT"` and `"MERGE"` are a duplication meeting a superposition with a different or the same label, `"PRUNE"` is an eraser absorbing a term, and `"DIST"` is a duplication copying a term that is not a superposition.
- `"SliceBoxes"` and `"Diagram"` take the longest to compute.
- [TMultiwayGraph]() and [TCausalGraph]() draw a trace. [TMultiTraceQ]() gives whether the runtime records events.
- [TMultiTrace]() has the following option:

| | | |
| --- | --- | --- |
| `"DiagramSeeds"` | `{}` | extra terms whose agents every `"Diagram"` also shows |

## Basic Examples

Start from an empty heap so the event ids and heap locations are predictable:

```wl
TReset[];
```

The records of a sum: the sum, then its value with the event that produced it:

```wl
TMultiTrace[TNum[3] + TNum[4]]
```

<!-- => {<|"CanonicalSlice" -> {Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]]}, "Events" -> {}|>, <|"CanonicalSlice" -> {Term["NUM", 7]}, "Events" -> {<|"id" -> 0, "rule" -> "OP2_NUM_NUM", "ruleCode" -> 34, "family" -> "TERM", "familyCode" -> 0, "termA" -> 0, "termB" -> 1, "deltaLabel" -> 0, "consumed" -> {}, "produced" -> {}|>}|>} -->

---

Start from an empty heap so the labels are predictable:

```wl
TReset[];
```

The slices of a sum over a superposition, one list of branches per step:

```wl
TMultiTrace[TSup[1, 2] + 3, "CanonicalSlice"]
```

<!-- => {{Term["OP2", "+", Term["SUP", 1, Term["NUM", 1], Term["NUM", 2]], Term["NUM", 3]]}, {Term["OP2", "+", Term["NUM", 1], Term["DP0", 1, Term["NUM", 3]]], Term["OP2", "+", Term["NUM", 2], Term["DP1", 1, Term["NUM", 3]]]}, {Term["OP2", "+", Term["NUM", 1], Term["NUM", 3]], Term["OP2", "+", Term["NUM", 2], Term["NUM", 3]]}, {Term["NUM", 4], Term["OP2", "+", Term["NUM", 2], Term["NUM", 3]]}, {Term["NUM", 4], Term["NUM", 5]}} -->

## Scope

Start from an empty heap so the interaction counter starts at 0:

```wl
TReset[];
```

The interaction count after each step:

```wl
TMultiTrace[TSup[1, 2] + 3, "ITRS"]
```

<!-- => {0, 1, 2, 3, 4} -->

---

The rules that fired in each step:

```wl
Map[#["rule"] &, TMultiTrace[TSup[1, 2] + 3, "Events"], {2}]
```

<!-- => {{}, {"OP2_SUP"}, {"DUP_NUM"}, {"OP2_NUM_NUM"}, {"OP2_NUM_NUM"}} -->

---

The families of those rules:

```wl
Map[#["family"] &, TMultiTrace[TSup[1, 2] + 3, "Events"], {2}]
```

<!-- => {{}, {"SLIDE"}, {"DIST"}, {"TERM"}, {"TERM"}} -->

---

Start from an empty heap so the heap locations are predictable:

```wl
TReset[];
```

The first event, in full:

```wl
Dataset[TMultiTrace[TSup[1, 2] + 3, "Events"][[2, 1]]]
```

<!-- => a Dataset of the event: id 0, rule OP2_SUP, ruleCode 21, family SLIDE, familyCode 1, termA 2, termB 3, deltaLabel 1, consumed {}, produced {5, 6, 7, 8} -->

---

Start from an empty heap:

```wl
TReset[];
```

A list of keys gives records with only those keys:

```wl
Dataset[TMultiTrace[TSup[1, 2] + 3, {"Step", "ITRS"}]]
```

<!-- => a Dataset with the columns Step and ITRS and the rows {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4} -->

---

The active terms, one per step:

```wl
Term /@ TMultiTrace[TNum[3] + TNum[4], "Term"]
```

<!-- => {Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]], Term["NUM", 7]} -->

---

Start from an empty heap:

```wl
TReset[];
```

A cap of one step gives the records of step 0 and step 1:

```wl
TMultiTrace[TNum[3] * TNum[4] + 5, 1]
```

<!-- => {<|"CanonicalSlice" -> {Term["OP2", "+", Term["OP2", "*", Term["NUM", 3], Term["NUM", 4]], Term["NUM", 5]]}, "Events" -> {}|>, <|"CanonicalSlice" -> {Term["OP2", "+", Term["NUM", 12], Term["NUM", 5]]}, "Events" -> {<|"id" -> 0, "rule" -> "OP2_NUM_NUM", "ruleCode" -> 34, "family" -> "TERM", "familyCode" -> 0, "termA" -> 0, "termB" -> 1, "deltaLabel" -> 2, "consumed" -> {}, "produced" -> {2}|>}|>} -->

---

A cap applies with a key too:

```wl
TMultiTrace[TSup[1, 2] + 3, "Step", 2]
```

<!-- => {0, 1, 2} -->

---

Start from an empty heap:

```wl
TReset[];
```

When *expr* itself does all the reduction, everything is recorded at step 0:

```wl
Map[#["rule"] &, TMultiTrace[TCollapse[TSup[1, 2] + 3], "Events"], {2}]
```

<!-- => {{"OP2_SUP", "DUP_NUM", "OP2_NUM_NUM", "OP2_NUM_NUM"}} -->

---

A string diagram of the heap at each step, drawn by [THeapDiagram]():

```wl
TMultiTrace[TNum[3] + TNum[4], "Diagram"]
```

<!-- => a list of two string diagrams: the numbers 3 and 4 wired into an OP2 + box, then the number 7 alone -->

## Options

### DiagramSeeds

A seed term is drawn in the diagram of every step, here a superposition beside the sum:

```wl
TMultiTrace[TNum[3] + TNum[4], "Diagram", "DiagramSeeds" -> {TSup[5, 6]}]
```

<!-- => a list of two string diagrams, each with the numbers 5 and 6 wired into a SUP box: the first with the numbers 3 and 4 wired into an OP2 + box beside it, the second with the number 7 -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

The trace reduces the term, so [TItrs]() counts its interactions:

```wl
Length[TMultiTrace[TSup[1, 2] + 3]]
```

<!-- => 5 -->

Four interactions fired over the five records:

```wl
TItrs[]
```

<!-- => 4 -->

---

The last slice holds the values [TCollapse]() lists:

```wl
Last[TMultiTrace[TSup[1, 2] + 3, "CanonicalSlice"]]
```

<!-- => {Term["NUM", 4], Term["NUM", 5]} -->

---

[TMultiwayGraph]() draws a trace that records the slices and their typeset forms:

```wl
TMultiwayGraph[TMultiTrace[TSup[1, 2] + 3, {"Events", "CanonicalSlice", "SliceBoxes"}]]
```

<!-- => a Graph from the vertex <1, 2> + 3 down two branches, OP2_SUP then DUP_NUM then OP2_NUM_NUM, ending at the vertices 4 and 5 -->

## Possible Issues

A key that is not a key of a record is refused:

```wl
TMultiTrace[TNum[3] + TNum[4], "Foo"]
```

<!-- => the message TMultiTrace::key is issued and the expression returns unevaluated -->
