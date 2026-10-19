---
Template: TechNote
Name: ATP
Title: Theorem Proving with THVMLink
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`, Wolfram`Parser`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/ATP
Keywords: [theorem proving, ATP, automated theorem proving, equational, completion, Knuth-Bendix, Waldmeister, Vampire, congruence closure, SMT, TPTP]
RelatedGuides: [THVMLink]
RelatedTutorials: [AtpMethods, Disproof, SMT, TPTPImport, FindReplacePath, Waldmeister]
---

THVMLink proves theorems of equational logic: given axioms that are equations, each holding for all values of its variables, it searches for a proof that a further equation follows from them. Its automated theorem prover is unfailing Knuth-Bendix completion, the method of the Waldmeister, Twee and Vampire provers on equational problems. Completion orients the axioms into rewrite rules, adds the equations that arise where two rules overlap (the critical pairs), and proves the conjecture when its two sides rewrite to the same term. Equations that cannot be oriented are kept and applied in whichever direction makes a term smaller, which is what makes the completion unfailing. Problems without variables are decided by congruence closure instead.

| | |
| --- | --- |
| <code>[TFindProof]()[*conjecture*, *axioms*]</code> | prove *conjecture* from *axioms*, giving a [ProofObject]() |
| <code>[TFindProof]()["*theorem*", "*theory*"]</code> | prove a named theorem of an [AxiomaticTheory]() theory |
| <code>[TFindProof]()[*axioms*, "Lemmas"]</code> | complete *axioms* with no conjecture and give the rewrite rules |
| <code>[TFindProof]()[*conjecture*, *axioms*, "*spec*"]</code> | give a property of the search, such as `"Status"` |
| <code>[TAtpSchedule]()[*method*]</code> | the configurations a [Method]() setting tries in turn |
| <code>[TAtpDescribeMethod]()[*method*]</code> | the suboptions a [Method]() setting resolves to |
| <code>[TRelevantAxioms]()[*conjecture*, *axioms*]</code> | the axioms kept and dropped by the relevance filter |
| <code>[TATP]()[*axioms*, *conjecture*]</code> | run completion with one fixed strategy and report the outcome |

## Proving an equational theorem

[TFindProof]() proves a named theorem of a theory known to [AxiomaticTheory]() and gives a [ProofObject](), the same kind of object [FindEquationalProof]() gives:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", TimeConstraint -> 10]
```

The theory is the list of axioms of an abelian group, with [CircleTimes]() for the product, [OverBar]() for the inverse and `OverTilde[1]` for the unit:

```wl
AxiomaticTheory["AbelianGroupAxioms"]
```
<!-- => {ForAll[{\[FormalA], \[FormalB], \[FormalC]}, \[FormalA] ⊗ (\[FormalB] ⊗ \[FormalC]) == (\[FormalA] ⊗ \[FormalB]) ⊗ \[FormalC]], ForAll[{\[FormalA], \[FormalB]}, \[FormalA] ⊗ \[FormalB] == \[FormalB] ⊗ \[FormalA]], ForAll[\[FormalA], \[FormalA] ⊗ OverTilde[1] == \[FormalA]], ForAll[\[FormalA], \[FormalA] ⊗ OverBar[\[FormalA]] == OverTilde[1]]} -->

A conjecture can also be stated explicitly. The inverse of a product is the product of the inverses:

```wl
TFindProof[ForAll[{x, y}, OverBar[x \[CircleTimes] y] == OverBar[x] \[CircleTimes] OverBar[y]], "AbelianGroupAxioms", TimeConstraint -> 10]
```

Axioms can be given explicitly too, with [ForAll]() or, as here, with pattern variables, which make each axiom hold for all values of its variables. These are the axioms of a group, with a right identity `e` and a right inverse `inv`:

```wl
groupAxioms = {a_ \[CircleTimes] e == a_, a_ \[CircleTimes] inv[a_] == e, a_ \[CircleTimes] (b_ \[CircleTimes] c_) == (a_ \[CircleTimes] b_) \[CircleTimes] c_}
```
<!-- => {a_ ⊗ e == a_, a_ ⊗ inv[a_] == e, a_ ⊗ (b_ ⊗ c_) == (a_ ⊗ b_) ⊗ c_} -->

The inverse of the inverse of an element is the element itself:

```wl
p = TFindProof[ForAll[a, inv[inv[a]] == a], groupAxioms, TimeConstraint -> 10]
```

The [ProofObject]() has the properties of the ones [FindEquationalProof]() gives. Its proof function checks the proof step by step:

```wl
p["ProofFunction"][]
```
<!-- => Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>] -->

Its proof graph shows how the lemmas derived from the axioms lead to the conclusion:

```wl
p["ProofGraph"]
```

A list of conjectures is proved as one conjunction, with one [ProofObject](). The right identity and right inverse are also left ones:

```wl
TFindProof[{ForAll[a, e \[CircleTimes] a == a], ForAll[a, inv[a] \[CircleTimes] a == e]}, groupAxioms, TimeConstraint -> 10]
```

Equations are best written with operators that do not evaluate, such as [CircleTimes](), [CenterDot]() or an undefined function. [Times]() is [Orderless](), so its arguments are sorted before the prover sees them, and the commutativity of [Times]() evaluates to [True]():

```wl
x y == y x
```
<!-- => True -->

## Completion without a goal

Without a conjecture, [TFindProof]() completes the axioms into a set of rewrite rules. The `"Lemmas"` spec gives the rules. The axioms of a monoid complete to three rules:

```wl
TFindProof["MonoidAxioms", "Lemmas"]
```
<!-- => {Inactive[Equal][OverTilde[1] ⊗ a, a], Inactive[Equal][a ⊗ OverTilde[1], a], Inactive[Equal][(a ⊗ b) ⊗ c, a ⊗ (b ⊗ c)]} -->

Completion need not end. The group axioms above generate new rules for as long as it runs, so [TimeConstraint]() bounds the search, and the status reports that the bound was reached:

```wl
TFindProof[groupAxioms, "Status", TimeConstraint -> 2]
```
<!-- => "TimedOut" -->

## Choosing what comes back

A last argument selects what [TFindProof]() gives instead of the [ProofObject](). The `"Path"` spec gives the rewrite path from the left side of the conjecture to its right side, here a single rewrite with a derived lemma:

```wl
TFindProof[ForAll[a, inv[inv[a]] == a], groupAxioms, "Path", TimeConstraint -> 10]
```
<!-- => {inv[inv[a]], a} -->

The `"AppliedMethod"` spec gives the configuration that found the proof:

```wl
TFindProof[ForAll[a, inv[inv[a]] == a], groupAxioms, "AppliedMethod", TimeConstraint -> 10]
```
<!-- => {"Completion", "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", "AutoPrecedence" -> True, "SelectionRatio" -> 51, "RHSInterreduce" -> True, "UnfailingCP" -> True, "CPSetInterreduce" -> True} -->

A list of specs gives an [Association](). `"Statistics"` gives the number of rewrite steps in the chain that closes the goal, and the numbers of rules, entries of the inference trace and critical pairs left in the queue:

```wl
TFindProof[ForAll[a, inv[inv[a]] == a], groupAxioms, {"Status", "Statistics"}, TimeConstraint -> 10]
```
<!-- => <|"Status" -> "Proved", "Statistics" -> <|"Status" -> "Proved", "Steps" -> 1, "Rules" -> 7, "Trace" -> 45, "QueueSize" -> 9|>|> -->

The possible specs are:

| | |
| --- | --- |
| `"ProofObject"` | the [ProofObject]() (default) |
| `"Status"` | `"Proved"`, `"Saturated"`, `"TimedOut"` or `"Failed"` |
| `"Statistics"` | the status, the rewrite steps that close the goal, and the numbers of rules, trace entries and queued critical pairs |
| `"Lemmas"` | the rewrite rules derived by the completion |
| `"Path"` | the rewrite path between the two sides of the conjecture |
| `"Counterexample"` | a [CounterexampleObject]() refuting the conjecture, or `$Failed` |
| `"AppliedMethod"` | the configuration that found the proof |
| `"PortfolioTrace"` | the outcome of each scheduled configuration |
| `"WallTime"` | the time of the configuration that found the proof |
| `"RelevantAxioms"` | the axioms kept and dropped by the relevance filter |
| `"PreprocessedAxioms"` | the axioms as the prover receives them |
| `"TPTP"` | the proof as a refutation in TPTP format |
| `"RawTrace"` | the inference trace of the prover |
| [All]() | an [Association]() of every spec |

The `"Counterexample"` spec is the subject of [Disproving Conjectures with Countermodels](paclet:WolframInstitute/THVMLink/tutorial/Disproof).

## Methods

The [Method]() option sets the strategy of the search: how critical pairs are ranked, which ordering orients the equations, and which simplifications are applied. Its settings are single configurations, named configurations modeled on other provers, and schedules that try several configurations in turn:

| | |
| --- | --- |
| [Automatic]() | detect the structure of the axioms, try configurations suited to it, then the `"Portfolio"` schedule (default) |
| `"Completion"`, `"GoalDirected"` | plain completion, and completion with a search from both sides of the goal |
| `"Waldmeister"`, `"Twee"`, `"VampireUEQ"`, `"EProver"`, `"VampireRandom"` | configurations modeled on those provers |
| `"Portfolio"`, `"VampirePortfolio"`, `"VampirePortfolioCompact"` | schedules of configurations |
| `"ENIGMA"` | critical pairs ranked by a learned model |
| `"SMT"` | congruence closure, for a ground problem |
| <code>{*name*, *opt* -> *val*, …}</code> | a configuration with explicit suboptions |

[TAtpSchedule]() shows the configurations a setting tries. For the group problem, [Automatic]() puts three configurations suited to a group first and the four of the `"Portfolio"` schedule after them:

```wl
TAtpSchedule[Automatic, ForAll[a, inv[inv[a]] == a], groupAxioms]
```
<!-- => {{"Completion", "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", "AutoPrecedence" -> True, "SelectionRatio" -> 51, "RHSInterreduce" -> True, "UnfailingCP" -> True, "CPSetInterreduce" -> True}, {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50, "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50}, {"Completion", "CriticalPairWeight" -> "Mix2"}, {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt"}, "GoalDirected"} -->

[TAtpDescribeMethod]() gives the suboptions a named configuration sets:

```wl
TAtpDescribeMethod["Twee"]
```
<!-- => <|"CriticalPairWeight" -> "Twee", "GroundJoin" -> True, "Connectedness" -> True, "UnfailingCP" -> True, "BackwardSubsume" -> True, "BackwardDemod" -> True, "RHSInterreduce" -> True, "AutoMaxWeight" -> 20, "GoalDirected" -> False|> -->

A list with a name and suboptions changes or adds to the settings of the named configuration:

```wl
TAtpDescribeMethod[{"Twee", "Ordering" -> "LPO"}]
```
<!-- => <|"CriticalPairWeight" -> "Twee", "GroundJoin" -> True, "Connectedness" -> True, "UnfailingCP" -> True, "BackwardSubsume" -> True, "BackwardDemod" -> True, "RHSInterreduce" -> True, "AutoMaxWeight" -> 20, "GoalDirected" -> False, "Ordering" -> "LPO"|> -->

A single configuration with explicit suboptions, here the Knuth-Bendix ordering (KBO) with the `"Mix"` ranking of critical pairs, proves commutativity in an abelian group:

```wl
TFindProof[ForAll[{x, y}, x \[CircleTimes] y == y \[CircleTimes] x], "AbelianGroupAxioms", Method -> {"Completion", "Ordering" -> "KBO", "CriticalPairWeight" -> "Mix"}, TimeConstraint -> 10]
```

The `"VampireUEQ"` configuration searches from both sides of the goal, which closes symmetric goals such as double negation in a Boolean algebra:

```wl
TFindProof["DoubleNegation", "BooleanAxioms", Method -> "VampireUEQ", TimeConstraint -> 10]
```

A schedule shares the [TimeConstraint]() among its entries and stops at the first proof. With the `"VampirePortfolio"` schedule, its first entry does not prove commutativity in its share of the time, and its second, a configuration with the `"Twee"` ranking of critical pairs, does:

```wl
TFindProof[ForAll[{x, y}, x \[CircleTimes] y == y \[CircleTimes] x], "AbelianGroupAxioms", {"Status", "AppliedMethod"}, Method -> "VampirePortfolio", TimeConstraint -> 10]
```
<!-- => <|"Status" -> "Proved", "AppliedMethod" -> {"Completion", "CriticalPairWeight" -> "Twee", "GroundJoin" -> True, "Connectedness" -> True, "BackwardSubsume" -> True, "BackwardDemod" -> True, "RHSInterreduce" -> True, "AutoMaxWeight" -> 20}|> -->

The settings `"VampireProcess"`, `"EproverProcess"`, `"TweeProcess"` and `"WaldmeisterProcess"` run the external prover programs of those names, when they are installed, and convert their proofs to the same [ProofObject]() form. The suboptions, the named configurations and the external programs are described in [Methods and Presets for TFindProof](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods), and the Waldmeister configuration in [The Waldmeister Method in THVMLink](paclet:WolframInstitute/THVMLink/tutorial/Waldmeister).

## Axiom relevance

An axiom that no proof of the conjecture can use only enlarges the search. Before it searches, [TFindProof]() filters the axioms, and [TRelevantAxioms]() shows the result of the filter without proving anything. An axiom about a function `h` that occurs in no other axiom and not in the conjecture is dropped:

```wl
TRelevantAxioms[ForAll[a, inv[inv[a]] == a], Append[groupAxioms, h[h[x_]] == h[x_]]]
```
<!-- => <|"Kept" -> {Inactive[Equal][a_ ⊗ e, a_], Inactive[Equal][a_ ⊗ inv[a_], e], Inactive[Equal][a_ ⊗ (b_ ⊗ c_), (a_ ⊗ b_) ⊗ c_]}, "Dropped" -> {<|"Axiom" -> Inactive[Equal][h[h[x_]], h[x_]], "Symbols" -> {h}, "Reason" -> "ConfinedBothSides"|>}, "Mode" -> "Safe"|> -->

The filter is set by the `"AxiomRelevance"` suboption of [Method]():

| | |
| --- | --- |
| [None]() | keep every axiom |
| `"Safe"` | drop only axioms that cannot take part in a proof (default) |
| `"Connected"` | keep the axioms connected to the conjecture through shared symbols |
| `"SInE"` | SInE (Sine Qua Non) premise selection, as in the Vampire prover |

`"Safe"` drops an axiom only when a symbol occurs on both of its sides and nowhere else, so it never drops an axiom a proof needs. `"Connected"` and `"SInE"` are heuristics for theories with many axioms, and can drop an axiom a proof needs.

## The completion engine alone

[TATP]() runs completion with one fixed strategy and reports how the run ended, with no [ProofObject](). It holds its arguments, so a list of axioms stored in a variable is passed with [Evaluate]():

```wl
TATP[Evaluate[groupAxioms], ForAll[a, inv[inv[a]] == a]]
```
<!-- => <|"Status" -> "PROVED", "Steps" -> 41, "Rules" -> 8, "QueueSize" -> 11|> -->

## Ground problems and TPTP files

A problem without variables is ground, and the `"SMT"` method decides it by congruence closure, as described in [Congruence Closure and Lazy DPLL(T)](paclet:WolframInstitute/THVMLink/tutorial/SMT):

```wl
TFindProof[a == c, {a == b, b == c}, Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", "Goal" -> a == c, "Hypotheses" -> {a == b, b == c}, "Witness" -> a != c|> -->

Problems from the TPTP (Thousands of Problems for Theorem Provers) library, the standard benchmark collection for theorem provers, are written in a text format of their own. [TPTPImport](paclet:Wolfram/WolframParser/ref/TPTPImport), from the WolframParser paclet, parses it into axioms and a conjecture, with function symbols as string heads and variables as patterns:

```wl
TPTPImport["cnf(right_identity, axiom, mult(X, e) = X).
cnf(right_inverse, axiom, mult(X, inv(X)) = e).
cnf(associativity, axiom, mult(mult(X, Y), Z) = mult(X, mult(Y, Z))).
cnf(inverse_of_inverse, negated_conjecture, inv(inv(a)) != a)."]
```
<!-- => <|"Axioms" -> {"mult"[X_, "e"[]] == X_, "mult"[X_, "inv"[X_]] == "e"[], "mult"["mult"[X_, Y_], Z_] == "mult"[X_, "mult"[Y_, Z_]]}, "Conjecture" -> "inv"["inv"["a"[]]] == "a"[]|> -->

[TFindProof]() takes the text, or a file given as <code>[File]()["*file*"]</code>, and proves the problem:

```wl
TFindProof["cnf(right_identity, axiom, mult(X, e) = X).
cnf(right_inverse, axiom, mult(X, inv(X)) = e).
cnf(associativity, axiom, mult(mult(X, Y), Z) = mult(X, mult(Y, Z))).
cnf(inverse_of_inverse, negated_conjecture, inv(inv(a)) != a).", TimeConstraint -> 10]
```

[Importing TPTP Benchmarks](paclet:WolframInstitute/THVMLink/tutorial/TPTPImport) covers the format. Rules that rewrite in one direction only, and the rewrite paths of proofs, are the subject of [Finding Replacement Paths with Directed Rules](paclet:WolframInstitute/THVMLink/tutorial/FindReplacePath).
