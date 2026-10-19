---
Template: Symbol
Name: TAtpDescribeMethod
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpDescribeMethod
Keywords: [theorem proving, ATP, Method, preset, suboptions, strategy]
SeeAlso: [TAtpSchedule, TFindProof, TRelevantAxioms]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Methods and Presets for TFindProof](AtpMethods)"]
---

## Usage

<code>[TAtpDescribeMethod]()[*method*]</code> gives an Association of the suboptions that the setting <code>[Method]() -> *method*</code> of [TFindProof]() resolves to.

## Details & Options

- For a named configuration such as `"Waldmeister"`, `"Twee"`, `"VampireUEQ"`, `"EProver"`, `"VampireRandom"` or `"ENIGMA"`, [TAtpDescribeMethod]() gives the suboptions of that configuration.
- For a list <code>{*name*, *opt* -> *val*, …}</code> it gives the suboptions of *name* with the explicit settings added or replacing them.
- For `"Completion"` or `"GoalDirected"` with explicit suboptions, it gives only those suboptions.
- For a schedule such as [Automatic](), `"Portfolio"` or `"VampirePortfolio"`, it gives `<|"Schedule" -> {…}|>` with the entries of the schedule, as [TAtpSchedule]() does.
- The suboptions are described in [Methods and Presets for TFindProof](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods).

## Basic Examples

The suboptions of the `"Twee"` configuration:

```wl
TAtpDescribeMethod["Twee"]
```

<!-- => <|"CriticalPairWeight" -> "Twee", "GroundJoin" -> True, "Connectedness" -> True, "UnfailingCP" -> True, "BackwardSubsume" -> True, "BackwardDemod" -> True, "RHSInterreduce" -> True, "AutoMaxWeight" -> 20|> -->

## Scope

An explicit setting is added to the suboptions of a configuration:

```wl
TAtpDescribeMethod[{"Twee", "Ordering" -> "LPO"}]
```

<!-- => <|"CriticalPairWeight" -> "Twee", ..., "AutoMaxWeight" -> 20, "Ordering" -> "LPO"|> -->

---

An explicit setting replaces the value a configuration gives:

```wl
TAtpDescribeMethod[{"Twee", "AutoMaxWeight" -> 40}]
```

<!-- => <|"CriticalPairWeight" -> "Twee", ..., "AutoMaxWeight" -> 40|> -->

---

Plain completion with explicit suboptions has only those suboptions:

```wl
TAtpDescribeMethod[{"Completion", "Ordering" -> "LPO"}]
```

<!-- => <|"Ordering" -> "LPO"|> -->

---

The `"ENIGMA"` configuration ranks critical pairs with a learned model:

```wl
TAtpDescribeMethod["ENIGMA"]
```

<!-- => <|"CriticalPairWeight" -> "Learned", "Ordering" -> "KBO", "AutoPrecedence" -> True, "UnfailingCP" -> True, "RHSInterreduce" -> True, "AutoMaxWeight" -> 20, "CoopWeight" -> "Gt", "CoopRatio" -> 2|> -->

---

The `"Waldmeister"` configuration sets many suboptions:

```wl
TAtpDescribeMethod["Waldmeister"]
```

<!-- => <|"CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", "AutoPrecedence" -> True, "SkolemHighest" -> True, "SelectionRatio" -> 51, ...|> -->

---

A schedule gives its entries:

```wl
TAtpDescribeMethod["Portfolio"]
```

<!-- => <|"Schedule" -> {{"Completion", "CriticalPairWeight" -> "Mix2"}, {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt"}, "GoalDirected"}|> -->

## Properties and Relations

The schedule given by [TAtpDescribeMethod]() is the one [TAtpSchedule]() gives:

```wl
TAtpDescribeMethod["VampirePortfolioCompact"]["Schedule"] === TAtpSchedule["VampirePortfolioCompact"]
```

<!-- => True -->

## Possible Issues

An unknown method issues a message and gives `$Failed`:

```wl
TAtpDescribeMethod["Foo"]
```

<!-- => the message TFindProof::badmethod is issued and the result is $Failed -->
