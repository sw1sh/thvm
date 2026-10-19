---
Template: TechNote
Name: Waldmeister
Title: The Waldmeister Method in THVMLink
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Waldmeister
Keywords: [Waldmeister, ATP, theorem proving, unfailing completion, Knuth-Bendix, critical pair, emission order, wmcli, FindEquationalProof, Sheffer]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, AtpMethods, Disproof, TPTPImport]
---

<!-- 26.10.19: maintainer note: under the preset, the critical-pair selection sequence matched the wmcli trace on 73 of the 82 notable theorems of the alignment sweep (4 deep cross-axiom implications diverge, among them CombinatorAxioms/SKIToBCKW near pick 1868 and the MeredithAxioms family; 5 were skipped because the wmcli reference run itself is memory-unsafe); ShefferAxioms/OrAssociativity matches through its whole proof. -->

[Waldmeister](https://www.mpi-inf.mpg.de/departments/automation-of-logic/software/waldmeister) is a theorem prover for unit equational logic, the logic of axioms and conjectures that are single equations. Like the default prover of [TFindProof](), it works by unfailing Knuth-Bendix completion. The setting <code>[Method]() -> "Waldmeister"</code> configures the completion engine of [TFindProof]() the way Waldmeister configures its own: the same ranking of critical pairs, the same term ordering and the same rules for the order in which critical pairs are formed, ranked and simplified, so that the engine selects critical pairs, and so derives lemmas, in Waldmeister's order. The search runs inside the Wolfram Language session, and its result is an ordinary [ProofObject]().

| | |
| --- | --- |
| <code>[TFindProof]()[*conjecture*, *axioms*, [Method]() -> "Waldmeister"]</code> | prove *conjecture* with the Waldmeister configuration |
| <code>[TFindProof]()[*conjecture*, *axioms*, [Method]() -> {"Waldmeister", *opt* -> *val*, …}]</code> | prove it with some suboptions changed |
| <code>[TAtpDescribeMethod]()["Waldmeister"]</code> | the suboptions of the Waldmeister configuration |
| <code>[TFindProof]()["*theorem*", "*theory*", [Method]() -> "WaldmeisterProcess"]</code> | prove a named theorem with the Waldmeister program itself |

## The configuration

A named theorem of an [AxiomaticTheory]() theory, proved with the Waldmeister configuration:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> "Waldmeister", TimeConstraint -> 10]
```

The `"Lemmas"` spec gives the rewrite rules of the completion, the axioms first and then the derived lemmas in the order the engine selected them:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "Lemmas", Method -> "Waldmeister", TimeConstraint -> 10]
```
<!-- => {Inactive[Equal][a ⊗ OverTilde[1], a], Inactive[Equal][a ⊗ OverBar[a], OverTilde[1]], Inactive[Equal][(a ⊗ b) ⊗ c, a ⊗ (b ⊗ c)], Inactive[Equal][a ⊗ b, b ⊗ a], Inactive[Equal][OverTilde[1] ⊗ a, a], Inactive[Equal][OverBar[OverTilde[1]], OverTilde[1]], Inactive[Equal][a ⊗ (OverBar[a] ⊗ b), b], Inactive[Equal][OverBar[OverBar[a]], a]} -->

[TAtpDescribeMethod]() gives every suboption the configuration sets:

```wl
TAtpDescribeMethod["Waldmeister"]
```
<!-- => <|"CriticalPairWeight" -> "Mix", "Ordering" -> "KBO", "AutoPrecedence" -> True, "SkolemHighest" -> True, "SelectionRatio" -> 51, "RHSInterreduce" -> True, "UnfailingCP" -> True, "CPSetInterreduce" -> False, "DemoteOnLhsSimplify" -> True, "OrphanMurder" -> True, "PopSubsume" -> True, "ESetSubsume" -> True, "QueueSubsume" -> False, "EmissionOrder" -> True, "IntakeOrder" -> True, "MixmostNF" -> True, "Einsstern" -> False, "NoOverlapBelowSkolem" -> False, "Reclassify" -> False, "ReversedCompletion" -> False, "SUEManagement" -> False, "CriticalGoalInterreduce" -> False, "CriticalGoalWeight" -> False, "BackwardGoalArgue" -> False, "TracePack" -> True, "RecordNorm" -> False, "UseImplicitCp" -> False, "CPSide" -> True, "FlatSubsume" -> True, "FormationFifo" -> True, "EsetDistdir" -> True, "ForceCombinatorOrient" -> True, "GoalDirected" -> False|> -->

The first group of suboptions sets the search strategy, as in other configurations:

| | |
| --- | --- |
| `"CriticalPairWeight" -> "Mix"` | rank critical pairs by Waldmeister's default weight function |
| `"Ordering" -> "KBO"` | orient equations with the Knuth-Bendix ordering (KBO) |
| `"AutoPrecedence" -> True` | choose the precedence of the symbols from the structure of the axioms, as Waldmeister does |
| `"SkolemHighest" -> True` | rank the constants of the goal above every operator |
| `"SelectionRatio" -> 51` | select the oldest critical pair once for every 51 selections by weight, so that every pair is eventually processed |
| `"RHSInterreduce" -> True` | simplify the right sides of rules with each new rule |
| `"UnfailingCP" -> True` | form critical pairs with both sides of an equation that cannot be oriented |

The second group reproduces the order of Waldmeister's operations, which decides which of several critical pairs of equal weight is selected first:

| | |
| --- | --- |
| `"EmissionOrder"` | form the critical pairs of each new rule in Waldmeister's order, which fixes the age of each pair |
| `"IntakeOrder"` | sort the axioms the way Waldmeister sorts them before the search |
| `"MixmostNF"` | compute normal forms with Waldmeister's default strategy |
| `"CPSide"` | store each new equation with its sides in the order Waldmeister uses |
| `"FlatSubsume"` | decide which equations are redundant the way Waldmeister's subsumption test does |
| `"FormationFifo"` | apply Waldmeister's order outside the formation of critical pairs, when simplified rules are queued again |
| `"EsetDistdir"` | test each stored equation for redundancy in its stored direction only; [Automatic]() follows `"FormationFifo"` |

These suboptions are off in a plain `"Completion"` configuration and can be turned on in any configuration. A list with the name changes or adds suboptions and keeps the rest of the configuration:

```wl
Lookup[TAtpDescribeMethod[{"Waldmeister", "CriticalPairWeight" -> "Gt"}], {"CriticalPairWeight", "Ordering", "EmissionOrder"}]
```
<!-- => {"Gt", "KBO", True} -->

The `"AppliedMethod"` spec records the configuration a proof used:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", "AppliedMethod", Method -> {"Waldmeister", "CriticalPairWeight" -> "Gt"}, TimeConstraint -> 10]
```
<!-- => {"Waldmeister", "CriticalPairWeight" -> "Gt"} -->

## Harder problems

The Sheffer stroke is a single operator from which all Boolean operations can be built. Its commutativity follows from the Sheffer axioms:

```wl
TFindProof["Commutativity", "ShefferAxioms", Method -> "Waldmeister", TimeConstraint -> 10]
```

The `"Statistics"` spec shows the size of the search: the rewrite steps in the chain that closes the goal, the rules derived, the entries of the inference trace and the critical pairs still waiting in the queue when the proof was found:

```wl
TFindProof["Commutativity", "ShefferAxioms", "Statistics", Method -> "Waldmeister", TimeConstraint -> 10]
```
<!-- => <|"Status" -> "Proved", "Steps" -> 1, "Rules" -> 6, "Trace" -> 187, "QueueSize" -> 168|> -->

Some equations cannot be oriented because each side has a variable the other lacks, such as `x ⊗ OverBar[x] == y ⊗ OverBar[y]`. Replacing the variables of one side by the smallest constant gives an instance that can be oriented, and the configuration rewrites with such instances when they make a term smaller, without adding them as rules. That is enough to prove the equality of inverses from McCune's axioms for groups:

```wl
TFindProof["EqualityOfInverses", "McCuneAxioms", Method -> "Waldmeister", TimeConstraint -> 10]
```

The suboption `"FreeVarInstance" -> True` adds those instances as rules instead, as Waldmeister does; this changes the course of the search, and the default leaves it off.

Searches on single-operator axiomatizations such as those of the Sheffer stroke can keep millions of critical pairs and need a large amount of memory. [TimeConstraint]() bounds the time of a search, and [MaxSteps]() the number of critical pairs it processes.

## Comparing with other provers

The setting `"WaldmeisterProcess"` runs the Waldmeister program itself, when it is installed, and converts its proof to a [ProofObject](). Without the program, the result is a [Failure]() with the tag `"ExternalNoProof"`:

```wl
TFindProof["InverseOfInverse", "AbelianGroupAxioms", Method -> "WaldmeisterProcess", TimeConstraint -> 10]
```

The function [TWaldmeisterProofObject]() runs the program on a problem file in Waldmeister's own format. [FindEquationalProof](), the equational prover built into the Wolfram Language, proves the same theorem:

```wl
FindEquationalProof[OverBar[OverBar[x]] == x, AxiomaticTheory["AbelianGroupAxioms"]]
```

The three proofs are [ProofObject]() expressions with the same properties, such as `"ProofGraph"` and `"ProofLength"`. The other configurations of [TFindProof]() are described in [Methods and Presets for TFindProof](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods), and the prover in general in [Theorem Proving with THVMLink](paclet:WolframInstitute/THVMLink/tutorial/ATP).
