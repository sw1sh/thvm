---
Template: Symbol
Name: TRelevantAxioms
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TRelevantAxioms
Keywords: [theorem proving, ATP, axiom relevance, premise selection, SInE, filter]
SeeAlso: [TFindProof, TAtpSchedule, TAtpDescribeMethod, AxiomaticTheory]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TRelevantAxioms]()[*conjecture*, *axioms*]</code> gives the axioms that the relevance filter of [TFindProof]() keeps and drops for proving *conjecture*, without proving it.

<code>[TRelevantAxioms]()[*conjecture*, "*theory*"]</code> filters the axioms of the named [AxiomaticTheory]() theory.

<code>[TRelevantAxioms]()["*theorem*", "*theory*"]</code> filters the axioms of a theory for one of its named theorems.

## Details & Options

- [TRelevantAxioms]() gives an Association with the keys `"Kept"` (the kept axioms), `"Dropped"` (an Association for each dropped axiom, with its `"Axiom"`, `"Symbols"` and `"Reason"`) and `"Mode"` (the filter used).
- [TFindProof]() applies the same filter to the axioms of a named theory before it starts the proof search, and issues a `TFindProof::dropax` message when `"Connected"` or `"SInE"` drops an axiom. Its `"RelevantAxioms"` spec gives the partition.
- The filter is set by the `"AxiomRelevance"` suboption of [Method](), as in `Method -> {"AxiomRelevance" -> "SInE"}`. The possible settings are:

| | |
| --- | --- |
| [None]() | keep every axiom |
| `"Safe"` | drop only axioms that cannot take part in a proof (default) |
| `"Connected"` | keep the axioms reachable from the symbols of the conjecture |
| `"SInE"` | SInE (Sine Qua Non) premise selection, as in the Vampire prover |

- `"Safe"` drops an axiom when a symbol occurs on both of its sides and in no other axiom or the conjecture: such an axiom cannot take part in a proof, so every axiom a proof can need is kept. `"Connected"` and `"SInE"` are heuristics that can drop an axiom a proof needs.
- `"Connected"` keeps the axioms that share a symbol with the conjecture, then the axioms that share a symbol with those, ignoring symbols that occur in every axiom. The setting `{"Connected", "FrequencyCutoff" -> f, "MaxGenerations" -> n}` ignores symbols that occur in a fraction f of the axioms and stops after n rounds.
- `"SInE"` follows the rarest symbols of each axiom from the symbols of the conjecture. The setting `{"SInE", "SineTolerance" -> t, "SineDepth" -> d, "SineGenerality" -> g}` sets the tolerance (default `3`), the search depth (default `2`) and the number of axioms above which a symbol is too general to follow (default `8`).
- The `"Reason"` of a dropped axiom is `"ConfinedBothSides"` for `"Safe"`, `"Disconnected"` for `"Connected"` and `"SInEUnreachable"` for `"SInE"`.
- The following option can be given:

| | | |
| --- | --- | --- |
| [Method]() | [Automatic]() | proof method whose `"AxiomRelevance"` suboption selects the filter |

## Basic Examples

The default filter drops an axiom about `h`, a symbol that occurs nowhere else:

```wl
TRelevantAxioms[f[a] == a, {f[x_] == x_, g[y_] == y_, h[g[z_]] == h[z_]}]
```

<!-- => <|"Kept" -> {Inactive[Equal][f[x_], x_], Inactive[Equal][g[y_], y_]}, "Dropped" -> {<|"Axiom" -> Inactive[Equal][h[g[z_]], h[z_]], "Symbols" -> {h}, "Reason" -> "ConfinedBothSides"|>}, "Mode" -> "Safe"|> -->

## Scope

Filter the axioms of a named theory:

```wl
TRelevantAxioms[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]
```

<!-- => <|"Kept" -> {...the four axioms...}, "Dropped" -> {}, "Mode" -> "Safe"|> -->

---

Filter the axioms of a theory for one of its named theorems:

```wl
TRelevantAxioms["ImpliesWolframAxioms", "MeredithAxioms"]
```

<!-- => <|"Kept" -> {...the two axioms...}, "Dropped" -> {}, "Mode" -> "Safe"|> -->

## Options

### Method

With `"AxiomRelevance" -> None`, every axiom is kept:

```wl
TRelevantAxioms[f[a] == a, {f[x_] == x_, g[y_] == y_, h[g[z_]] == h[z_]}, Method -> {"AxiomRelevance" -> None}]
```

<!-- => <|"Kept" -> {Inactive[Equal][f[x_], x_], Inactive[Equal][g[y_], y_], Inactive[Equal][h[g[z_]], h[z_]]}, "Dropped" -> {}, "Mode" -> None|> -->

---

`"Connected"` keeps only the axioms reachable from the symbols of the conjecture:

```wl
TRelevantAxioms[f[a] == a, {f[x_] == x_, g[y_] == y_, h[g[z_]] == h[z_]}, Method -> {"AxiomRelevance" -> "Connected"}]
```

<!-- => <|"Kept" -> {Inactive[Equal][f[x_], x_]}, "Dropped" -> {<|..., "Reason" -> "Disconnected"|>, <|..., "Reason" -> "Disconnected"|>}, "Mode" -> "Connected"|> -->

---

`"SInE"` follows the symbols of the conjecture through the axioms:

```wl
TRelevantAxioms[f[a] == a, {f[x_] == g[x_], g[y_] == y_, h[g[z_]] == h[z_]}, Method -> {"AxiomRelevance" -> "SInE"}]
```

<!-- => <|"Kept" -> {Inactive[Equal][f[x_], g[x_]], Inactive[Equal][g[y_], y_], Inactive[Equal][h[g[z_]], h[z_]]}, "Dropped" -> {}, "Mode" -> "SInE"|> -->

---

A search depth of `1` stops after the axioms that contain a symbol of the conjecture:

```wl
TRelevantAxioms[f[a] == a, {f[x_] == g[x_], g[y_] == y_, h[g[z_]] == h[z_]}, Method -> {"AxiomRelevance" -> {"SInE", "SineDepth" -> 1}}]
```

<!-- => <|"Kept" -> {Inactive[Equal][f[x_], g[x_]]}, "Dropped" -> {<|..., "Reason" -> "SInEUnreachable"|>, <|..., "Reason" -> "SInEUnreachable"|>}, "Mode" -> "SInE"|> -->

## Properties and Relations

The `"RelevantAxioms"` spec of [TFindProof]() gives the partition of the axioms of a theory:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "RelevantAxioms"]
```

<!-- => <|"Kept" -> {...the four axioms...}, "Dropped" -> {}, "Mode" -> "Safe"|> -->

## Possible Issues

`"Connected"` ignores a symbol that occurs in every axiom, so for the double-inverse theorem it drops the associativity and commutativity axioms:

```wl
TRelevantAxioms["InverseOfInverse", "AbelianGroupAxioms", Method -> {"AxiomRelevance" -> "Connected"}]
```

<!-- => <|"Kept" -> {...the identity and inverse axioms...}, "Dropped" -> {<|..., "Reason" -> "Disconnected"|>, <|..., "Reason" -> "Disconnected"|>}, "Mode" -> "Connected"|> -->

---

With that filter, the proof search fails, and the message TFindProof::dropax is issued:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "Status", Method -> {"Completion", "AxiomRelevance" -> "Connected"}]
```

<!-- => the message TFindProof::dropax is issued and the result is "Saturated" -->

---

The default filter keeps every axiom, and the proof succeeds:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "Status", Method -> "Completion"]
```

<!-- => "Proved" -->
