---
Template: Symbol
Name: TFindProof
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Wolfram`Parser`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindProof
Keywords: [theorem proving, ATP, equational logic, completion, Knuth-Bendix, proof object, AxiomaticTheory, Waldmeister, Vampire]
SeeAlso: [FindEquationalProof, ProofObject, AxiomaticTheory, TFindEquationalPath, TFindStringProof, TRelevantAxioms, TAtpSchedule, TAtpDescribeMethod, PortfolioFrontLoad, TProofObject, CounterexampleObject]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Theorem Proving with THVMLink](ATP)", "[Methods and Presets for TFindProof](AtpMethods)", "[Disproving Conjectures with Countermodels](Disproof)"]
---

## Usage

<code>[TFindProof]()[*conjecture*, *axioms*]</code> tries to prove the equation *conjecture* from the list of equational *axioms*, and gives a [ProofObject]().

<code>[TFindProof]()[*conjecture*, "*theory*"]</code> proves *conjecture* from the axioms of the named [AxiomaticTheory]() theory.

<code>[TFindProof]()["*theorem*", "*theory*"]</code> proves a named notable theorem of an [AxiomaticTheory]() theory.

<code>[TFindProof]()[*axioms*]</code> completes *axioms* with no goal, giving a [ProofObject]() whose theorem is [None]().

<code>[TFindProof]()["*theory*"]</code> completes the axioms of a named [AxiomaticTheory]() theory.

<code>[TFindProof]()[*conjecture*, *axioms*, "*spec*"]</code> gives the property *spec* of the proof search instead of the proof object.

<!-- #| annotation: 26.10.19: MaxSteps -> Automatic resolves to 200000 critical pairs, and to 500000 under the "Waldmeister" presets. The external-process methods dispatch only on the TFindProof["theorem", "theory"] form ("WaldmeisterProcess" also on explicit equations); elsewhere they issue TFindProof::badmethod and Automatic runs instead. "LiftToProofObject" -> True has no effect on "TweeProcess", whose result stays an Association. -->

## Details & Options

- [TFindProof]() searches for an equational proof by unfailing Knuth-Bendix completion: it orients the axioms into rewrite rules, adds the critical pairs between them, and stops when both sides of the conjecture rewrite to the same term.
- The [ProofObject]() it gives is the one [FindEquationalProof]() gives, with the same properties, such as `"ProofDataset"`, `"ProofGraph"`, `"ProofLength"` and `"ProofFunction"`.
- An equation is written `lhs == rhs` or <code>[ForAll]()[*vars*, *lhs* == *rhs*]</code>. Pattern variables such as `x_` in an axiom are universally quantified, and the free symbols of a conjecture are arbitrary constants.
- When a constant of the conjecture has the name of a variable of the axioms, the proof writes the constant as a formal symbol, such as `\[FormalC]` for `c`, as [FindEquationalProof]() does, so that no lemma reads a constant as a variable.
- An axiom written as a rule `lhs -> rhs` rewrites from left to right only. An axiom written as `lhs <-> rhs` or `{lhs, rhs}` is an equation, oriented by the prover.
- Equations are best written over operators that do not evaluate, such as [CircleTimes](), [CenterDot]() or an undefined head `f`. [Times]() and [Plus]() are [Orderless](), so an equation over them can evaluate before the prover sees it.
- A list of conjectures is proved as one conjunction, and gives one [ProofObject]() with a hypothesis and a conclusion for each conjecture.
- Every lemma of a proof that [TFindProof]() gives derives from the axioms alone: no lemma rests on a conjecture, so no conjecture of a conjunction is proved from itself or from another.
- Predicate-logic input, with predicates, the connectives [And](), [Or](), [Not](), [Implies]() and [Equivalent](), and the quantifiers [ForAll]() and [Exists](), is converted to an equational problem and gives a `"Predicate/EquationalLogic"` proof. A pattern variable such as `x_` in a predicate axiom is universally quantified, as in an equation.
- A problem in the TPTP (Thousands of Problems for Theorem Provers) format, given as text, as <code>[File]()["*file*"]</code> or as the Association that `TPTPImport` gives, is proved from its own axioms and negated conjecture.
- [TFindProof]() gives `$Failed` when it finds no proof: when the axioms complete without joining the two sides of the conjecture, or when the step or time budget runs out. A conjecture that is not an equation gives a <code>[Failure]()["TATPParseError", …]</code>.
- The possible values of *spec* are:

| | |
| --- | --- |
| `"ProofObject"` | the [ProofObject]() (default) |
| `"Status"` | `"Proved"`, `"Saturated"`, `"TimedOut"` or `"Failed"` |
| `"Statistics"` | the status, the number of rewrite steps in the chain that closes the goal (0 for a completion without a goal), and the numbers of rules, trace entries and queued critical pairs |
| `"Lemmas"` | the rewrite rules derived by the completion |
| `"Path"` | the rewrite path from the left side of the conjecture to its right side |
| `"Counterexample"` | a [CounterexampleObject]() refuting the conjecture, or `$Failed` |
| `"AppliedMethod"` | the method that found the proof |
| `"PortfolioTrace"` | the method, time and outcome of each scheduled attempt |
| `"WallTime"` | the time in seconds of the attempt that found the proof |
| `"RelevantAxioms"` | the axioms kept and dropped by the relevance filter |
| `"PreprocessedAxioms"` | the axioms as given to the prover |
| `"TPTP"` | the proof as TPTP text in the SZS (Sutcliffe-Zimmer-Schulz) refutation format |
| `"RawTrace"` | the inference trace of the prover |

- A list of specs gives an Association with one entry for each spec, and [All]() gives every spec.
- The possible settings for [Method]() are:

| | |
| --- | --- |
| [Automatic]() | detect the structure of the axioms, try a matching configuration first, then the `"Portfolio"` schedule |
| `"Portfolio"` | a fixed schedule of four configurations |
| `"Completion"` | plain unfailing completion |
| `"GoalDirected"` | completion with a search from both sides of the goal |
| `"Waldmeister"`, `"Twee"`, `"VampireUEQ"`, `"EProver"`, `"VampireRandom"` | configurations modeled on those provers |
| `"VampirePortfolio"`, `"VampirePortfolioCompact"` | schedules of the prover configurations |
| `"ENIGMA"` | critical pairs ranked by a learned model |
| `"SMT"` | decide a ground problem by congruence closure |
| `"VampireProcess"`, `"EproverProcess"`, `"TweeProcess"`, `"WaldmeisterProcess"` | run the external prover program |
| <code>{*name*, *opt* -> *val*, …}</code> | a configuration with explicit suboptions |

- Any other setting of [Method]() issues the message `TFindProof::badmethod`, and [Automatic]() is used instead. `$AtpMethodPresets` lists the named configurations and schedules.
- [TAtpSchedule]() gives the schedule a method expands to and [TAtpDescribeMethod]() the suboptions of a configuration. The suboptions are described in [Methods and Presets for TFindProof](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods).
- [TimeConstraint]() is the total time for a schedule, shared among its entries: each entry gets the time left in proportion to its weight among the entries left. The weight of an entry is 1, doubled for each of the first [PortfolioFrontLoad]() entries, and multiplied by *w* for an entry <code>{*name*, …, "ScheduleShare" -> *w*}</code>. The [Automatic]() schedule gives the `"Waldmeister"` configuration six shares for a single-operator axiomatization such as `"WolframAxioms"`. With the default setting <code>[TimeConstraint]() -> [Infinity]()</code>, each entry of a schedule is given 60 seconds per unit of weight.
- A completion without a goal keeps its [TimeConstraint]() for every *spec*. When the derivations of the rules it found do not fit in the time left, its [ProofObject]() lists the axioms only, and the message `TFindProof::cplpo` is issued.
- With a process method, the options of the matching builder, such as `"LiftToProofObject"`, `"ParseFormulas"` and `"Binary"` of [TVampireProofObject](), are passed on to it. With the default `"LiftToProofObject" -> Automatic`, `"WaldmeisterProcess"` gives a [ProofObject](), and `"VampireProcess"`, `"TweeProcess"` and `"EproverProcess"` give the Association of the proof as the prover writes it; `"LiftToProofObject" -> True` gives a [ProofObject]() for Vampire and E.
- The following options can be given:

| | | |
| --- | --- | --- |
| [MaxSteps]() | [Automatic]() | maximum number of critical pairs to process |
| [Method]() | [Automatic]() | proof search method |
| [TimeConstraint]() | [Infinity]() | time limit in seconds |
| [PortfolioFrontLoad]() | `1` | number of entries at the start of a schedule that get twice the equal share of the time |
| `"ProofForm"` | `"ProofObject"` | form of the proof, `"ProofObject"` or `"TProofObject"` |
| `"LiftToProofObject"` | [Automatic]() | whether a process method gives a [ProofObject]() |
| `"ParseFormulas"` | [False]() | whether a process method parses the formulas of the proof into expressions |
| `"Binary"` | [Automatic]() | path of the program of a process method |

## Basic Examples

Prove that Wolfram's single axiom for Boolean algebra, in terms of one operator, implies that the operator is commutative:

```wl
TFindProof["Commutativity", "WolframAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

Prove a deeper notable theorem of the same axiom, the associativity of the Or operation built from its operator:

```wl
TFindProof["OrAssociativity", "WolframAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

Prove that the inverse of the inverse of an element of an abelian group is the element itself:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

Prove the same theorem from explicit group axioms over an undefined head `f`:

```wl
TFindProof[inv[inv[a]] == a, {ForAll[{x, y, z}, f[f[x, y], z] == f[x, f[y, z]]], ForAll[x, f[e, x] == x], ForAll[x, f[inv[x], x] == e]}]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

Prove a notable theorem of a theory by its name:

```wl
TFindProof["InverseOfComposite", "AbelianGroupAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

Ask for the status of the proof search instead of the proof:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Status"]
```

<!-- => "Proved" -->

## Scope

### Axioms and conjectures

Axioms can use pattern variables instead of [ForAll]():

```wl
TFindProof[inv[inv[a]] == a, {f[f[x_, y_], z_] == f[x_, f[y_, z_]], f[e, x_] == x_, f[inv[x_], x_] == e}]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

A rule axiom rewrites in one direction only, so the proof follows the rule:

```wl
TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "Path"]
```

<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

---

A list of conjectures is proved as one conjunction:

```wl
TFindProof[{OverBar[OverBar[x]] == x, OverBar[x \[CircleTimes] y] == OverBar[x] \[CircleTimes] OverBar[y]}, "AbelianGroupAxioms", TimeConstraint -> 10]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

Predicate-logic input is converted to an equational problem:

```wl
TFindProof[mortal[socrates], {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]}]
```

<!-- => a ProofObject summary box, logic Predicate/EquationalLogic -->

---

A pattern variable in a predicate axiom is universally quantified:

```wl
TFindProof[mortal[socrates], {Implies[man[x_], mortal[x_]], man[socrates]}]
```

<!-- => a ProofObject summary box, logic Predicate/EquationalLogic -->

---

A TPTP problem is proved from its own axioms and negated conjecture:

```wl
TFindProof["cnf(a1, axiom, f(f(X)) = X).\ncnf(g, negated_conjecture, f(f(f(a))) != f(a)).", "Status"]
```

<!-- => "Proved" -->

---

A problem that `TPTPImport` has parsed is proved the same way:

```wl
TFindProof[TPTPImport["cnf(a1, axiom, f(f(X)) = X).\ncnf(g, negated_conjecture, f(f(f(a))) != f(a))."], "Status"]
```

<!-- => "Proved" -->

### Completion without a goal

With no conjecture, the axioms are completed into a set of rewrite rules:

```wl
TFindProof[{f[f[x_]] == x_}]
```

<!-- => a ProofObject summary box, logic EquationalLogic, with no theorem -->

---

The `"Lemmas"` spec gives the rules a completion derives, here for the monoid axioms:

```wl
TFindProof["MonoidAxioms", "Lemmas"]
```

<!-- => {Inactive[Equal][OverTilde[1] ⊗ a, a], Inactive[Equal][a ⊗ OverTilde[1], a], Inactive[Equal][(a ⊗ b) ⊗ c, a ⊗ (b ⊗ c)]} -->

---

A named theory alone is completed the same way:

```wl
TFindProof["MonoidAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic, with no theorem -->

### Properties of the proof search

The `"Statistics"` spec gives the number of rewrite steps in the chain that closes the goal, and the numbers of rules, trace entries and queued critical pairs:

```wl
TFindProof["InverseOfComposite", "AbelianGroupAxioms", "Statistics"]
```

<!-- => <|"Status" -> "Proved", "Steps" -> 1, "Rules" -> 14, "Trace" -> 67, "QueueSize" -> 29|> -->

---

The `"Lemmas"` spec gives the rules derived on the way to the proof:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Lemmas"]
```

<!-- => {Inactive[Equal][a ⊗ OverTilde[1], a], Inactive[Equal][a ⊗ OverBar[a], OverTilde[1]], ..., Inactive[Equal][OverBar[OverBar[a]], a]} (8 rules) -->

---

The `"AppliedMethod"` spec names the configuration that found the proof:

```wl
TFindProof[x \[CircleTimes] y == y \[CircleTimes] x, "AbelianGroupAxioms", "AppliedMethod", TimeConstraint -> 5]
```

<!-- => {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50} -->

---

The `"PortfolioTrace"` spec shows each scheduled attempt, its time and whether it proved the goal:

```wl
TFindProof[x \[CircleTimes] y == y \[CircleTimes] x, "AbelianGroupAxioms", "PortfolioTrace", TimeConstraint -> 5]
```

<!-- => {<|"Method" -> {"Completion", "CriticalPairWeight" -> "Mix", ...}, "WallTime" -> a time, "Proved" -> False|>, <|..., "Proved" -> False|>, <|"Method" -> {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50}, "WallTime" -> a time, "Proved" -> True|>} -->

---

The `"WallTime"` spec gives the time in seconds of the attempt that found the proof:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "WallTime"]
```

<!-- => a few milliseconds, such as 0.0045 -->

---

The `"RelevantAxioms"` spec shows which axioms the relevance filter kept:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "RelevantAxioms"]
```

<!-- => <|"Kept" -> {Inactive[Equal][(a_) ⊗ ((b_) ⊗ (c_)), ((a_) ⊗ (b_)) ⊗ (c_)], ..., Inactive[Equal][(a_) ⊗ OverBar[a_], OverTilde[1]]}, "Dropped" -> {}, "Mode" -> "Safe"|> -->

---

The `"PreprocessedAxioms"` spec gives the axioms in the form the prover receives them:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "PreprocessedAxioms"]
```

<!-- => {Inactive[Equal][(a_) ⊗ ((b_) ⊗ (c_)), ((a_) ⊗ (b_)) ⊗ (c_)], ...} -->

---

The `"TPTP"` spec writes the proof as a TPTP refutation:

```wl
TFindProof[f[f[f[a]]] == f[a], {ForAll[x, f[f[x]] == x]}, "TPTP"]
```

<!-- => "% SZS status Unsatisfiable for problem\n% SZS output start CNFRefutation for problem\ncnf(ax1, axiom, f(f(Pattern(x,_))) = Pattern(x,_)).\n...\n% SZS output end CNFRefutation for problem" -->

---

The `"RawTrace"` spec gives the inference trace of the prover, one Association for each inference:

```wl
TFindProof[f[f[f[a]]] == f[a], {ForAll[x, f[f[x]] == x]}, "RawTrace"]
```

<!-- => {<|"Reason" -> 1, "ParentA" -> 4294967295, ...|>, <|"Reason" -> 2, "ParentA" -> 0, ...|>} -->

---

A list of specs gives an Association:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", {"Status", "Path"}]
```

<!-- => <|"Status" -> "Proved", "Path" -> {OverBar[OverBar[x]], x}|> -->

---

The `"Counterexample"` spec gives a [CounterexampleObject]() when the conjecture does not follow from the axioms:

```wl
TFindProof[a == c, {a == b}, "Counterexample"]
```

<!-- => a CounterexampleObject summary box, method CongruenceClosure -->

## Options

### MaxSteps

A small step budget stops the search before it finds the proof:

```wl
TFindProof["InverseOfComposite", "AbelianGroupAxioms", "Status", MaxSteps -> 5]
```

<!-- => "TimedOut" -->

### Method

The `"Waldmeister"` configuration proves the commutativity of the Sheffer stroke from the Sheffer axioms:

```wl
TFindProof["Commutativity", "ShefferAxioms", Method -> "Waldmeister"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

An explicit configuration gives its suboptions with the method name:

```wl
TFindProof["InverseOfComposite", "AbelianGroupAxioms", "AppliedMethod", Method -> {"Completion", "Ordering" -> "LPO"}]
```

<!-- => {"Completion", "Ordering" -> "LPO"} -->

---

An unknown method issues a message, and the [Automatic]() schedule proves the theorem:

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Status", Method -> "Foo"]
```

<!-- => the message TFindProof::badmethod is issued and the result is "Proved" -->

---

The `"SMT"` method decides a ground problem by congruence closure and gives the decision:

```wl
TFindProof[f[a] == f[c], {a == b, b == c}, Method -> "SMT"]
```

<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", "Goal" -> f[a] == f[c], "Hypotheses" -> {a == b, b == c}, "Witness" -> f[a] != f[c]|> -->

---

On a ground non-theorem the `"SMT"` method gives a counterexample:

```wl
TFindProof[f[a] == c, {a == b, b == c}, Method -> "SMT"]
```

<!-- => a CounterexampleObject summary box, method CongruenceClosure -->

---

The `"VampireProcess"` method runs the Vampire prover program on a named theorem; with `"LiftToProofObject" -> False`, as by default for this method, the result is an Association of its proof steps:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> "VampireProcess", "LiftToProofObject" -> False, TimeConstraint -> 10]
```

<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> "not"["not"["skc1"[]]] != "skc1"[], "Axioms" -> {...}, "ProofDataset" -> <|...|>, ...|> -->

### TimeConstraint

A time limit bounds a search that does not end:

```wl
TFindProof[x ** y == y ** x, "AbelianGroupAxioms", "Status", TimeConstraint -> 2]
```

<!-- => "TimedOut" -->

### PortfolioFrontLoad

With `PortfolioFrontLoad -> 2`, each of the first two entries of the `"Portfolio"` schedule gets twice the share of the time of each of the others, as the `"PortfolioTrace"` spec shows:

```wl
TFindProof[x ** y == y ** x, "AbelianGroupAxioms", "PortfolioTrace", Method -> "Portfolio", TimeConstraint -> 6, PortfolioFrontLoad -> 2]
```

<!-- => {<|"Method" -> {"Completion", "CriticalPairWeight" -> "Mix2"}, "WallTime" -> a time of about 2, "Proved" -> False|>, <|..., "WallTime" -> a time of about 2, ...|>, <|..., "WallTime" -> a time of about 1, ...|>, <|"Method" -> "GoalDirected", "WallTime" -> a time of about 1, "Proved" -> False|>} -->

### ProofForm

With `"ProofForm" -> "TProofObject"`, the proof is a [TProofObject]():

```wl
TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "ProofForm" -> "TProofObject"]
```

<!-- => a TProofObject summary box -->

## Properties and Relations

Find a proof:

```wl
p = TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

The proof verifies through its own `"ProofFunction"`:

```wl
p["ProofFunction"][p["Theorems"]]
```

<!-- => Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>] -->

---

[FindEquationalProof]() gives a [ProofObject]() for the same theorem:

```wl
FindEquationalProof[OverBar[OverBar[x]] == x, AxiomaticTheory["AbelianGroupAxioms"]]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

[TAtpSchedule]() shows the schedule [Automatic]() tries for a problem:

```wl
TAtpSchedule[Automatic, x \[CircleTimes] y == y \[CircleTimes] x, "AbelianGroupAxioms"]
```

<!-- => {{"Completion", "CriticalPairWeight" -> "Mix", ...}, {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50, ...}, ...} -->

---

[TFindEquationalPath]() gives the rewrite path of the proof:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}]
```

<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

## Possible Issues

A conjecture that does not follow from the axioms gives `$Failed`:

```wl
TFindProof[g[a] == a, {ForAll[x, g[g[x]] == x]}]
```

<!-- => $Failed -->

---

The `"Counterexample"` spec gives the model that refutes it:

```wl
TFindProof[g[a] == a, {ForAll[x, g[g[x]] == x]}, "Counterexample"]
```

<!-- => a CounterexampleObject summary box, method SaturationNormalForm -->

---

[Times]() is [Orderless](), so `x y == y x` evaluates to [True]() and the input returns unevaluated:

```wl
TFindProof[x y == y x, "AbelianGroupAxioms"]
```

<!-- => TFindProof[True, "AbelianGroupAxioms"] -->

---

An equation between two different strings evaluates to [False]() before [TFindProof]() sees it, so the conjecture is not an equation; [TFindStringProof]() proves theorems about strings:

```wl
TFindProof["BBAA" == "AABB", {"BA" -> "AB"}]
```

<!-- => Failure["TATPParseError", <|"Axiom" -> "conjecture", "Reason" -> "expected `lhs == rhs`"|>] -->

---

A completion that its time limit stops gives a [ProofObject]() of the axioms only, with a message:

```wl
TFindProof["AbelianGroupAxioms", TimeConstraint -> 2]
```

<!-- => the message TFindProof::cplpo is issued, and the result is a ProofObject summary box, logic EquationalLogic, with no theorem and the four axioms as its only steps -->

---

The deepest notable theorems take minutes: the Wolfram axiom implies the three Sheffer axioms, and the first two take about a second each, but the third needs about 1.5 million steps of Waldmeister's automatic mode, `"WaldmeisterAuto"`, which the default schedule runs second, and with its proof the whole call takes about four and a half minutes. With a short time limit the search runs out of time:

```wl
TFindProof["ImpliesShefferAxioms", "WolframAxioms", "Status", TimeConstraint -> 10]
```

<!-- => "TimedOut" -->

---

A single configuration can fail where the [Automatic]() schedule succeeds, since the schedule falls back to other configurations:

```wl
TFindProof[x \[CircleTimes] y == y \[CircleTimes] x, "AbelianGroupAxioms", Method -> {"Completion", "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", "AutoPrecedence" -> True}, TimeConstraint -> 5]
```

<!-- => $Failed -->

## Neat Examples

The proof graph of the double-inverse theorem, from the axioms through the derived lemmas to the conclusion, laid out from left to right:

```wl
Graph[TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]["ProofGraph"], GraphLayout -> {"LayeredDigraphEmbedding", "Orientation" -> Left}, ImageSize -> 600]
```

<!-- => a Graph of the proof steps -->
