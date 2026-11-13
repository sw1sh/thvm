---
Template: Symbol
Name: PortfolioFrontLoad
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/PortfolioFrontLoad
Keywords: [theorem proving, ATP, portfolio, schedule, time limit, TimeConstraint]
SeeAlso: [TFindProof, TAtpSchedule, TimeConstraint]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[PortfolioFrontLoad]()</code> is an option for [TFindProof]() that specifies how many entries at the start of a schedule get twice the equal share of the time limit.

## Details & Options

- [TFindProof]() tries the entries of a schedule, such as that of <code>[Method]() -> [Automatic]()</code> or <code>[Method]() -> "Portfolio"</code>, in turn, and shares its [TimeConstraint]() among them. Each entry gets the time left divided by the number of entries left.
- With <code>[PortfolioFrontLoad]() -> *k*</code>, each of the first *k* entries gets twice that share, and the entries after them share what is left equally.
- An entry <code>{*name*, …, "ScheduleShare" -> *w*}</code> weighs *w* times as much as a plain entry, so that each entry gets the time left in proportion to its weight among the entries left. The [Automatic]() schedule gives its `"Waldmeister"` entry six shares for a single-operator axiomatization such as `"WolframAxioms"`.
- The default setting is <code>[PortfolioFrontLoad]() -> 1</code>. <code>[PortfolioFrontLoad]() -> 0</code> shares the time equally among all entries.
- An entry that ends before its share is used up leaves the rest of its time to the entries after it.
- With the default <code>[TimeConstraint]() -> [Infinity]()</code>, each entry gets 60 seconds per unit of weight.
- The `"PortfolioTrace"` spec of [TFindProof]() gives the time each entry took. [TAtpSchedule]() gives the entries of a schedule.

## Basic Examples

By default, the first of the four entries of the `"Portfolio"` schedule gets twice the share of the time of each of the others, here two seconds of five:

```wl
TFindProof[x ** y == y ** x, "AbelianGroupAxioms", "PortfolioTrace", Method -> "Portfolio", TimeConstraint -> 5]
```

<!-- => {<|"Method" -> {"Completion", "CriticalPairWeight" -> "Mix2"}, "WallTime" -> a time of about 2, "Proved" -> False|>, <|"Method" -> {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True}, "WallTime" -> a time of about 1, "Proved" -> False|>, <|"Method" -> {"Completion", "CriticalPairWeight" -> "Gt"}, "WallTime" -> a time of about 1, "Proved" -> False|>, <|"Method" -> "GoalDirected", "WallTime" -> a time of about 1, "Proved" -> False|>} -->

## Scope

Share the time equally among the entries:

```wl
TFindProof[x ** y == y ** x, "AbelianGroupAxioms", "PortfolioTrace", Method -> "Portfolio", TimeConstraint -> 4, PortfolioFrontLoad -> 0]
```

<!-- => {<|"Method" -> {"Completion", "CriticalPairWeight" -> "Mix2"}, "WallTime" -> a time of about 1, "Proved" -> False|>, ..., <|"Method" -> "GoalDirected", "WallTime" -> a time of about 1, "Proved" -> False|>}, with a time of about 1 for each entry -->

---

Give the first two entries twice the share of each of the others:

```wl
TFindProof[x ** y == y ** x, "AbelianGroupAxioms", "PortfolioTrace", Method -> "Portfolio", TimeConstraint -> 6, PortfolioFrontLoad -> 2]
```

<!-- => {<|"Method" -> {"Completion", "CriticalPairWeight" -> "Mix2"}, "WallTime" -> a time of about 2, "Proved" -> False|>, <|..., "WallTime" -> a time of about 2, ...|>, <|..., "WallTime" -> a time of about 1, ...|>, <|"Method" -> "GoalDirected", "WallTime" -> a time of about 1, "Proved" -> False|>} -->

## Properties and Relations

The time is shared among the entries that [TAtpSchedule]() gives:

```wl
TAtpSchedule["Portfolio"]
```

<!-- => {{"Completion", "CriticalPairWeight" -> "Mix2"}, {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt"}, "GoalDirected"} -->

---

An entry that proves the theorem ends the schedule, so the entries after it are not tried:

```wl
TFindProof[x \[CircleTimes] y == y \[CircleTimes] x, "AbelianGroupAxioms", "PortfolioTrace", TimeConstraint -> 5]
```

<!-- => a list of the attempts up to the first one with "Proved" -> True -->
