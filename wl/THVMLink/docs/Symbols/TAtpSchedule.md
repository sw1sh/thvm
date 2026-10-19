---
Template: Symbol
Name: TAtpSchedule
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpSchedule
Keywords: [theorem proving, ATP, Method, portfolio, schedule, strategy]
SeeAlso: [TAtpDescribeMethod, TFindProof, TRelevantAxioms]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TAtpSchedule]()[*method*]</code> gives the list of configurations that [TFindProof]() tries in turn for the setting <code>[Method]() -> *method*</code>.

<code>[TAtpSchedule]()[*method*, *conjecture*, *axioms*]</code> gives the schedule for proving *conjecture* from *axioms*.

<code>[TAtpSchedule]()[*method*, "*theorem*", "*theory*"]</code> gives the schedule for a named theorem of an [AxiomaticTheory]() theory.

## Details & Options

- [TAtpSchedule]() shows a schedule without running the prover.
- Each entry of a schedule is one configuration: the name of a configuration such as `"Waldmeister"`, or a list <code>{*name*, *opt* -> *val*, …}</code> with explicit suboptions.
- A single configuration gives a list with one entry.
- A schedule such as `"Portfolio"`, `"VampirePortfolio"`, `"VampirePortfolioCompact"` or `"AllPresets"` gives its entries in order.
- [Automatic]() with no problem gives the `"Portfolio"` schedule. For a problem, [Automatic]() detects the structure of the axioms and puts configurations suited to it before the `"Portfolio"` entries.
- [TFindProof]() tries the entries in order, shares its [TimeConstraint]() among them, and stops at the first proof.
- The suboptions of each configuration are described in [Methods and Presets for TFindProof](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods), and [TAtpDescribeMethod]() gives their values.

## Basic Examples

The `"Portfolio"` schedule has four configurations:

```wl
TAtpSchedule["Portfolio"]
```

<!-- => {{"Completion", "CriticalPairWeight" -> "Mix2"}, {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt"}, "GoalDirected"} -->

---

The schedule [Automatic]() tries for a theorem of abelian groups:

```wl
TAtpSchedule[Automatic, OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]
```

<!-- => {{"Completion", "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", ...}, {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50, "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50}, {"Completion", "CriticalPairWeight" -> "Mix2"}, {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt"}, "GoalDirected"} -->

## Scope

A named configuration gives a list with one entry:

```wl
TAtpSchedule["Waldmeister"]
```

<!-- => {"Waldmeister"} -->

---

A configuration with explicit suboptions is kept as it is:

```wl
TAtpSchedule[{"Completion", "Ordering" -> "LPO"}]
```

<!-- => {{"Completion", "Ordering" -> "LPO"}} -->

---

The compact Vampire schedule has three entries:

```wl
TAtpSchedule["VampirePortfolioCompact"]
```

<!-- => {"VampireUEQ", "Twee", {"Completion", "CriticalPairWeight" -> "Mix2", "AutoPrecedence" -> True, "AutoMaxWeight" -> 20}} -->

---

`"AllPresets"` tries the prover configurations in turn:

```wl
TAtpSchedule["AllPresets"]
```

<!-- => {"Waldmeister", "VampireUEQ", "Twee", "EProver"} -->

---

[Automatic]() with no problem gives the `"Portfolio"` schedule:

```wl
TAtpSchedule[Automatic]
```

<!-- => {{"Completion", "CriticalPairWeight" -> "Mix2"}, {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt"}, "GoalDirected"} -->

---

The schedule for a named theorem:

```wl
TAtpSchedule[Automatic, "InverseOfInverse", "AbelianGroupAxioms"]
```

<!-- => the same seven entries as for OverBar[OverBar[x]] == x -->

## Properties and Relations

[TFindProof]() tries the entries in order; its `"PortfolioTrace"` spec lists the attempts it made:

```wl
TFindProof[x \[CircleTimes] y == y \[CircleTimes] x, "AbelianGroupAxioms", "PortfolioTrace", TimeConstraint -> 5]
```

<!-- => the first three entries of the schedule, the third proving the goal -->

---

[TAtpDescribeMethod]() gives the suboptions of one configuration:

```wl
TAtpDescribeMethod["Twee"]
```

<!-- => <|"CriticalPairWeight" -> "Twee", "GroundJoin" -> True, "Connectedness" -> True, "UnfailingCP" -> True, "BackwardSubsume" -> True, "BackwardDemod" -> True, "RHSInterreduce" -> True, "AutoMaxWeight" -> 20|> -->

## Possible Issues

An unknown method issues a message and gives `$Failed`:

```wl
TAtpSchedule["Foo"]
```

<!-- => the message TFindProof::badmethod is issued and the result is $Failed -->
