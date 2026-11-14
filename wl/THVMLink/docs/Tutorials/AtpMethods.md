---
Template: TechNote
Name: AtpMethods
Title: Methods and Presets for TFindProof
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/AtpMethods
Keywords: [theorem proving, ATP, Method, preset, portfolio, Waldmeister, Vampire, Twee, EProver, SInE, AutoPrecedence, GoalDirected, ENIGMA]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, Waldmeister, GnnGuidance, SMT, TPTPImport, Disproof]
---

The completion engine of [TFindProof]() has many settings: how the critical pairs (CPs) waiting in its queue are ranked, which ordering orients equations into rewrite rules, whether the search also works from both sides of the goal, which redundant critical pairs are deleted, and whether the axioms are filtered before the search. The [Method]() option gathers them. A setting takes one of these forms:

| | |
| --- | --- |
| <code>[Method]() -> {"Completion", *opt* -> *val*, …}</code> | a single configuration with explicit suboptions |
| <code>[Method]() -> "*name*"</code> | a named configuration modeled on another prover, such as `"Waldmeister"` |
| <code>[Method]() -> {"*name*", *opt* -> *val*, …}</code> | a named configuration with some suboptions changed |
| <code>[Method]() -> "*schedule*"</code> | a schedule of configurations tried in turn, such as `"Portfolio"` |
| <code>[Method]() -> [Automatic]()</code> | a schedule chosen from the structure of the axioms (default) |

These functions show what a setting does and support the learned configuration:

| | |
| --- | --- |
| <code>[TAtpSchedule]()[*method*]</code> | the configurations a setting tries in turn |
| <code>[TAtpDescribeMethod]()[*method*]</code> | the suboptions a setting resolves to |
| <code>[TRelevantAxioms]()[*conjecture*, *axioms*]</code> | the axioms kept and dropped by the relevance filter |
| <code>[TAtpCpDataset]()[*conjectures*, *axioms*]</code> | features of the critical pairs of proofs, labeled by their use in the proofs |
| <code>[TAtpTrainScorer]()[*dataset*]</code> | train a model that ranks critical pairs |
| <code>[TAtpSetLearnedScorer]()[*model*]</code> | make *model* the ranking of the `"ENIGMA"` configuration |

## Examples by problem

### A group theorem

The default setting, [Automatic](), detects that the axioms are those of an abelian group and tries configurations suited to them first:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms"]
```

The `"AppliedMethod"` spec shows the configuration that found the proof, here the first of the schedule, which ranks critical pairs by the `"Mix"` weight and chooses the precedence of the symbols automatically:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "AppliedMethod"]
```
<!-- => {"Completion", "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", "AutoPrecedence" -> True, "SelectionRatio" -> 51, "RHSInterreduce" -> True, "UnfailingCP" -> True, "CPSetInterreduce" -> True} -->

### A search from both sides of the goal

`"GoalDirected"` adds to completion a search that rewrites both sides of the goal towards each other, which can close a goal whose two sides have no common normal form under the rules derived so far. Double negation in a Boolean algebra:

```wl
TFindProof["DoubleNegation", "BooleanAxioms", Method -> "GoalDirected", TimeConstraint -> 5]
```

### A single-operator axiomatization

The axioms of `"WolframAxioms"` use a single operator, and the commutativity of that operator is an equation that cannot be oriented. A configuration that proves it searches from both sides of the goal, orients equations with the lexicographic path ordering (LPO), ranks the constants of the goal above every operator, ranks critical pairs by their size alone and forms critical pairs with both sides of equations that cannot be oriented:

```wl
TFindProof["Commutativity", "WolframAxioms", Method -> {"GoalDirected", "Ordering" -> "LPO", "SkolemHighest" -> True, "CriticalPairWeight" -> "Add", "UnfailingCP" -> True}, TimeConstraint -> 10]
```

For a single-operator axiomatization, [Automatic]() runs the `"Waldmeister"` configuration first, with six shares of the time, since it is the configuration that proves the deeper theorems, such as the associativity of the Or operation built from the operator of `"WolframAxioms"`:

```wl
First[TAtpSchedule[Automatic, "OrAssociativity", "WolframAxioms"]]
```
<!-- => {"Waldmeister", "ScheduleShare" -> 6} -->

### Filtering the axioms

The `"AxiomRelevance"` suboption filters the axioms before the search. `"SInE"` selects axioms by following symbols from the goal, as the Vampire prover does:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> {"Completion", "AxiomRelevance" -> "SInE"}]
```

The `"RelevantAxioms"` spec shows the selection. Each of the four axioms of an abelian group shares a symbol with the goal, so all four are kept; the filter matters for theories with many axioms:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "RelevantAxioms", Method -> {"Completion", "AxiomRelevance" -> "SInE"}]
```
<!-- => <|"Kept" -> {Inactive[Equal][a_ ⊗ (b_ ⊗ c_), (a_ ⊗ b_) ⊗ c_], Inactive[Equal][a_ ⊗ b_, b_ ⊗ a_], Inactive[Equal][a_ ⊗ OverTilde[1], a_], Inactive[Equal][a_ ⊗ OverBar[a_], OverTilde[1]]}, "Dropped" -> {}, "Mode" -> "SInE"|> -->

## Methods

The possible settings of [Method]() are:

| | |
| --- | --- |
| [Automatic]() | detect the structure of the axioms, such as a group, a ring or a single-operator axiomatization, try configurations suited to it, then the `"Portfolio"` schedule (default) |
| `"Portfolio"` | four configurations: the `"Mix2"` ranking, the LPO with an automatic precedence, the `"Gt"` ranking, and `"GoalDirected"` |
| `"Completion"` | plain unfailing completion, with the `"Gt"` ranking and the Knuth-Bendix ordering (KBO) |
| `"GoalDirected"`, `"MNF"` | completion together with a search from both sides of the goal |
| `"VampirePortfolio"` | a schedule of 12 configurations modeled on the Vampire prover |
| `"VampirePortfolioCompact"` | a schedule of three configurations for short time limits |
| `"Waldmeister"`, `"WaldmeisterAuto"`, `"VampireUEQ"`, `"Twee"`, `"EProver"`, `"VampireRandom"`, `"ENIGMA"` | named configurations |
| `"SMT"` | congruence closure, for a problem without variables |
| `"VampireProcess"`, `"EproverProcess"`, `"TweeProcess"`, `"WaldmeisterProcess"` | the external prover programs |

Since [Automatic]() ends with the `"Portfolio"` schedule, it proves every theorem that `"Portfolio"` proves, given time for both. Any other setting issues the message `TFindProof::badmethod`, and [Automatic]() runs instead.

The variable `$AtpMethodPresets` lists the named configurations and schedules:

```wl
$AtpMethodPresets
```
<!-- => {"Waldmeister", "WaldmeisterAuto", "VampireUEQ", "Twee", "EProver", "VampireRandom", "ENIGMA", "Portfolio", "VampirePortfolio", "VampirePortfolioCompact", "AllPresets"} -->

## Named configurations

Each named configuration sets the suboptions after the defaults of a prover. [TAtpDescribeMethod]() gives them:

```wl
TAtpDescribeMethod["VampireUEQ"]
```
<!-- => <|"Ordering" -> "LPO", "AutoPrecedence" -> True, "SelectionRatio" -> 10, "UnfailingCP" -> True, "AutoMaxWeight" -> True, "BackwardSubsume" -> True, "BackwardDemod" -> True, "RHSInterreduce" -> True, "GoalDirected" -> True|> -->

The named configurations are:

| | |
| --- | --- |
| `"Waldmeister"` | the `"Mix"` ranking, KBO, an automatic precedence and Waldmeister's order of operations; see [The Waldmeister Method in THVMLink](paclet:WolframInstitute/THVMLink/tutorial/Waldmeister) |
| `"WaldmeisterAuto"` | Waldmeister's automatic mode: as `"Waldmeister"`, but critical pairs of 50 or more symbols are queued unnormalized, equations and axioms keep their sides as given, and the goal is also searched from both sides |
| `"VampireUEQ"` | LPO, an automatic precedence, a selection ratio of 10, a growing weight bound, backward simplification and a search from both sides of the goal, after the Vampire prover |
| `"Twee"` | the `"Twee"` ranking, deletion of ground-joinable and connected critical pairs, backward simplification and a growing weight bound starting at 20, after the Twee prover |
| `"EProver"` | the `"ConjSym"` ranking, KBO with the rarest symbols ranked highest, a selection ratio of 10 and a growing weight bound starting at 20, after the E prover |
| `"VampireRandom"` | LPO, an automatic precedence, a selection ratio of 10, deletion of ground-joinable critical pairs, a seeded random selection of every 32nd critical pair, the limited resource strategy and a search from both sides of the goal, after a configuration of the Vampire prover |
| `"ENIGMA"` | critical pairs ranked by a learned model, on a base of KBO with an automatic precedence |

A list with the name and suboptions changes or adds suboptions:

```wl
TAtpDescribeMethod[{"EProver", "Ordering" -> "LPO"}]
```
<!-- => <|"CriticalPairWeight" -> "ConjSym", "Ordering" -> "LPO", "AutoPrecedence" -> "Occurrence", "SelectionRatio" -> 10, "AutoMaxWeight" -> 20, "BackwardSubsume" -> True, "RHSInterreduce" -> True, "UnfailingCP" -> True, "GoalDirected" -> False|> -->

The random selection of `"VampireRandom"` is seeded, so its search is reproducible, and the suboption `"RandomSeed"` changes the seed:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> {"VampireRandom", "RandomSeed" -> 17}, TimeConstraint -> 10]
```

## Learned ranking of critical pairs

The `"ENIGMA"` configuration ranks critical pairs with a model of how likely each is to be used in a proof, the approach of the ENIGMA (Efficient learNing-based Inference Guiding MAchine) provers. Without a model of your own, it uses a built-in logistic regression. The model is trained on proofs in four steps: prove some theorems while recording the features of each processed critical pair, label each critical pair by whether the proof uses it, train a model on the labeled features, and make the model the ranking of the configuration.

Two theorems about abelian groups:

```wl
thms = Catenate @ Lookup[AxiomaticTheory["AbelianGroupAxioms", "NotableTheorems"], {"InverseOfInverse", "InverseOfComposite"}]
```
<!-- => {ForAll[\[FormalA], OverBar[OverBar[\[FormalA]]] == \[FormalA]], ForAll[{\[FormalA], \[FormalB]}, OverBar[\[FormalA] ⊗ \[FormalB]] == OverBar[\[FormalA]] ⊗ OverBar[\[FormalB]]]} -->

[TAtpCpDataset]() proves them and records 14 features of each processed critical pair, with the label `1` for a pair used in a proof and `0` otherwise:

```wl
ds = TAtpCpDataset[thms, AxiomaticTheory["AbelianGroupAxioms"], TimeConstraint -> 5]
```
<!-- => <|"Features" -> {…36 rows of 14 features…}, "Labels" -> {…}, "FeatureNames" -> {"size_sum", "max_depth", "n_distinct_vars", "n_var_occ", "weight_add", "weight_gt", "weight_mix2", "goal_weight", "age", "top_symbol_l", "top_symbol_r", "shares_goal_sub", "orientable", "unif_measure"}, "NRows" -> 36, "NPositive" -> 14, "NProofs" -> 2|> -->

[TAtpTrainScorer]() trains a network with one hidden layer on the features, with the tensor runtime of THVMLink, and reports the area under the receiver operating characteristic curve on the training rows:

```wl
trained = TAtpTrainScorer[ds]
```
<!-- => <|"Model" -> <|"Kind" -> "MLP", "Mean" -> {…}, "InvStd" -> {…}, "W1" -> {…}, "B1" -> {…}, "W2" -> {…}, "B2" -> {…}|>, "TrainAUC" -> 0.76, "NRows" -> 36, "NPositive" -> 14, "Hidden" -> 16|> -->

[TAtpSetLearnedScorer]() makes the model the ranking of `"ENIGMA"`:

```wl
TAtpSetLearnedScorer[trained["Model"]]
```
<!-- => True -->

The configuration proves with the trained model:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> "ENIGMA", TimeConstraint -> 10]
```

The model only reorders the queue: the configuration still selects the oldest critical pair at regular intervals, so every pair is eventually processed. <code>[TAtpSetLearnedScorer]()[[Clear]()]</code> removes the model and restores the built-in one:

```wl
TAtpSetLearnedScorer[Clear]
```
<!-- => True -->

[TAtpCpDataset]() and [TAtpTrainScorer]() also take the name of an [AxiomaticTheory]() theory and use its notable theorems. A model that reads the structure of each critical pair as a graph, rather than fixed features, is the subject of [Learned Guidance: a GNN for Critical-Pair Selection](paclet:WolframInstitute/THVMLink/tutorial/GnnGuidance).

## External prover programs

Four settings run an external prover program on the problem instead of the completion engine, when the program is installed, and convert its proof into the form the completion engine gives. Each takes a named theorem of an [AxiomaticTheory]() theory, and `"WaldmeisterProcess"` also takes explicit equations:

| | |
| --- | --- |
| `"VampireProcess"` | the Vampire prover, in its competition mode |
| `"EproverProcess"` | the E prover, with its automatic schedule |
| `"TweeProcess"` | the Twee prover |
| `"WaldmeisterProcess"` | the Waldmeister prover |

With `"WaldmeisterProcess"`, the result is a [ProofObject](), with the steps of the external proof as its lemmas:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> "WaldmeisterProcess", TimeConstraint -> 10]
```
<!-- => a ProofObject summary box, logic EquationalLogic -->

With `"LiftToProofObject" -> False`, the result is an [Association]() of the steps as the program states them:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> "WaldmeisterProcess", "LiftToProofObject" -> False, TimeConstraint -> 10]
```
<!-- => <|"Backend" -> "SZS", "Status" -> "Proved", "Goal" -> "not(not(skC1())) = skC1()", "Axioms" -> {"x1 = and(x1,opOvertilde(k1()))", "and(x1,x2) = and(x2,x1)", "and(x1,and(x2,x3)) = and(and(x1,x2),x3)", "and(x1,not(x1)) = opOvertilde(k1())"}, "ProofDataset" -> <|{"Axiom", 1} -> <|"Statement" -> "x1 = and(x1,opOvertilde(k1()))", "Proof" -> <||>|>, …|>, "ProofLength" -> …, "RuleHistogram" -> <|…|>|> -->

`"VampireProcess"` and `"EproverProcess"` give such an [Association]() unless they are given `"LiftToProofObject" -> True`, and `"TweeProcess"` always gives one. When a program is not installed, or gives no proof, the result is a [Failure]() with the tag `"ExternalNoProof"`. The setting `"Binary"` gives the location of the program, and with `"ParseFormulas" -> True` the statements of an [Association]() are parsed into Wolfram Language expressions. The builders behind these settings are [TVampireProofObject](), [TEproverProofObject](), [TTweeProofObject]() and [TWaldmeisterProofObject](), and the conversion from the SZS (Sutcliffe-Zimmer-Schulz) output format of the provers is [TSZSDerivationToProofObject]().

## Suboptions

The suboptions of a configuration are:

| Suboption | Values | Effect |
| --- | --- | --- |
| `"CriticalPairWeight"` | `"Add"`, `"Max"`, `"Ord"`, `"Gt"`, `"Mix"`, `"Mix2"`, `"Unif"`, `"Goal"`, `"Twee"`, `"ConjSym"`, `"Diversity"`, `"RelLevel"`, `"Staggered"`, `"Learned"` | how critical pairs are ranked; `"Gt"` by default, `"Learned"` by the model of `"ENIGMA"` |
| `"Ordering"` | `"KBO"`, `"LPO"` | the ordering that orients equations; LPO can orient rules whose right side repeats a variable, which KBO cannot |
| `"AutoPrecedence"` | [True](), [False](), `"Occurrence"` | choose the precedence of the symbols from the structure of the axioms, as Waldmeister does, or rank the rarest symbols highest, as Vampire and E can |
| `"Precedence"` | a list of symbols | an explicit precedence |
| `"SkolemHighest"` | [True](), [False]() | rank the constants of the goal above every operator |
| `"AxiomRelevance"` | [None](), `"Safe"`, `"Connected"`, `"SInE"` | the filter applied to the axioms before the search |
| `"MaxWeight"` | an integer | delete critical pairs with more symbols than this; `0` for no bound |
| `"AutoMaxWeight"` | an integer | set aside critical pairs above a growing weight bound, and process them when the queue is empty |
| `"GoalInterleave"` | an integer | select the critical pair closest to the goal once in every *n* selections |
| `"SelectionRatio"` | an integer | select the oldest critical pair once in every *n* selections |
| `"GroundJoin"` | [True](), [False]() | delete critical pairs whose ground instances all rewrite to a common term |
| `"Connectedness"` | [True](), [False]() | delete critical pairs whose sides are joined through terms smaller than their common ancestor |
| `"UnfailingCP"` | [True](), [False]() | form critical pairs with both sides of equations that cannot be oriented |
| `"RHSInterreduce"` | [True](), [False]() | simplify the right sides of rules with each new rule |
| `"CPSetInterreduce"` | [True](), [False]() | simplify the whole queue with the rules from time to time |
| `"BackwardSubsume"`, `"BackwardDemod"` | [True](), [False]() | delete older rules that a new rule subsumes, and simplify older rules with new ones |
| `"ForwardSubsume"` | [True](), [False]() | skip a new critical pair that a queued one subsumes |
| `"LRS"` | [True](), [False]() | the limited resource strategy of Vampire: delete critical pairs that cannot be reached in the time left |
| `"SetOfSupport"` | [True](), [False]() | prefer critical pairs that involve the symbols of the goal |
| `"RandomRatio"`, `"RandomSeed"` | integers | select a random critical pair once in every *n* selections, from a seeded generator |
| `"SymbolWeights"`, `"VarWeight"` | weights | the weights of the symbols and of variables in KBO |

Waldmeister's order of operations has suboptions of its own, such as `"EmissionOrder"` and `"FormationFifo"`, described in [The Waldmeister Method in THVMLink](paclet:WolframInstitute/THVMLink/tutorial/Waldmeister). Unspecified suboptions keep the defaults of the engine.

## Schedules

[TAtpSchedule]() gives the configurations a setting tries. The compact Vampire schedule:

```wl
TAtpSchedule["VampirePortfolioCompact"]
```
<!-- => {"VampireUEQ", "Twee", {"Completion", "CriticalPairWeight" -> "Mix2", "AutoPrecedence" -> True, "AutoMaxWeight" -> 20}} -->

For a problem, [Automatic]() puts the configurations it chose for the structure of the axioms before the `"Portfolio"` schedule:

```wl
TAtpSchedule[Automatic, x \[CircleTimes] y == y \[CircleTimes] x, "AbelianGroupAxioms"]
```
<!-- => {{"Completion", "CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", "AutoPrecedence" -> True, "SelectionRatio" -> 51, "RHSInterreduce" -> True, "UnfailingCP" -> True, "CPSetInterreduce" -> True}, {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50, "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt", "GoalInterleave" -> 50}, {"Completion", "CriticalPairWeight" -> "Mix2"}, {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True}, {"Completion", "CriticalPairWeight" -> "Gt"}, "GoalDirected"} -->

For a schedule, [TAtpDescribeMethod]() gives the entries under `"Schedule"`:

```wl
TAtpDescribeMethod["VampirePortfolioCompact"]
```
<!-- => <|"Schedule" -> {"VampireUEQ", "Twee", {"Completion", "CriticalPairWeight" -> "Mix2", "AutoPrecedence" -> True, "AutoMaxWeight" -> 20}}|> -->

A schedule shares the [TimeConstraint]() of [TFindProof]() among its entries: each entry gets the time left in proportion to its weight among the entries left. The weight of an entry is 1, doubled for each of the first *n* entries with the option <code>[PortfolioFrontLoad]() -> *n*</code>, whose default is 1, and multiplied by *w* for an entry <code>{*name*, …, "ScheduleShare" -> *w*}</code>. <code>[PortfolioFrontLoad]() -> 0</code> shares the time equally among plain entries. With the default <code>[TimeConstraint]() -> [Infinity]()</code>, each entry gets 60 seconds per unit of weight. The option [MaxSteps]() bounds the number of critical pairs a configuration processes; its default, [Automatic](), is 200000, 500000 for the `"Waldmeister"` configuration, and for `"WaldmeisterAuto"` as many as its slice of the time allows.

## More configurations

### Deleting redundant critical pairs

The redundancy criteria of Twee, together with backward simplification and a weight bound, keep the queue small over a long completion, at some cost for each step:

```wl
TFindProof["InverseOfComposite", "AbelianGroupAxioms", Method -> {"Completion", "GroundJoin" -> True, "Connectedness" -> True, "BackwardSubsume" -> True, "BackwardDemod" -> True, "AutoMaxWeight" -> 20}, TimeConstraint -> 10]
```

### Combinators

The axioms of combinatory logic have right sides that repeat a variable, such as the rule of the combinator `s`, which needs LPO. With application written as [CenterDot](), `s k k` behaves as the identity:

```wl
TFindProof[ForAll[x, ((s \[CenterDot] k) \[CenterDot] k) \[CenterDot] x == x], {ForAll[{x, y, z}, ((s \[CenterDot] x) \[CenterDot] y) \[CenterDot] z == (x \[CenterDot] z) \[CenterDot] (y \[CenterDot] z)], ForAll[{x, y}, (k \[CenterDot] x) \[CenterDot] y == x]}, Method -> {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True, "CriticalPairWeight" -> "Add"}, TimeConstraint -> 10]
```
