# FindEquationalProof for v2: Wolfram 15's contract and the build on TFindProof

v2, the clean-room Wolfram kernel, needs `FindEquationalProof` for TypeTheory's
`"FindEquationalProof"` tactic
(`TypeTheory/Kernel/Proofs/Equational.wl`), which calls
`FindEquationalProof[s == t, axioms]` and replays the rows of
`Normal[proof["ProofDataset"]]` as a proof of its own. v2 builds the function on
THVMLink's `TFindProof`. This page is the specification it builds from. It is
written from Wolfram 15's observed behaviour, not from Wolfram 15's sources, and
it holds no implementation code: the units are written from it.

Every value below was observed on Wolfram 15.0 on Linux x86-64 (the
`wolframinstitute/wolfram:15.0` container, without network), and the calls are
recorded one Association per case in
[data/fep_w15_records.wl](data/fep_w15_records.wl) (read it with `ReadList`; its
header lists the fields). Each record also holds what `TFindProof` gave on the
same call. The proof check of section 4 is THVMLink code,
`wl/THVMLink/Kernel/ATP/ProofCheck.wl`, tested by
`wl/THVMLink/Tests/atp_proofcheck.wlt`.

Contents:

1. Wolfram 15's `FindEquationalProof`: calls, options, messages, Failures
2. `ProofObject`: its arguments and the rows of a proof
3. `ProofObject`'s properties
4. The proof check: a replay of the rows
5. v2's `FindEquationalProof` on `TFindProof`
6. A minimal `Dataset`
7. Tests: v2's suite and its Wolfram 15 transcript
8. What v2 needs, and what is open

## 1. Wolfram 15's FindEquationalProof

### 1.1 The symbol

| Item | Wolfram 15 |
|---|---|
| Attributes | `{Protected, ReadProtected}` |
| Options | `{TimeConstraint -> Infinity}` |
| Messages | `argr`, `argrx`, `invs`, `nonopt`, `timc` (texts below) |
| First call in a session | loads its package: 480 `General::newsym` messages, all off |

| Message | Text |
|---|---|
| `FindEquationalProof::argr` | `` `1` called with 1 argument; `2` arguments are expected. `` |
| `FindEquationalProof::argrx` | `` `1` called with `2` arguments; `3` arguments are expected. `` |
| `FindEquationalProof::invs` | `` Invalid specification of propositions `1` and axioms `2`. `` |
| `FindEquationalProof::nonopt` | `` Options expected (instead of `1`) beyond position `2` in `3`. An option must be a rule or a list of rules. `` |
| `FindEquationalProof::timc` | `` Number of seconds `1` is not a positive machine-sized number or Infinity. `` |

### 1.2 Calls and results

`groupAxioms` is `{ForAll[{x, y, z}, op[op[x, y], z] == op[x, op[y, z]]], ForAll[x, op[e, x] == x], ForAll[x, op[inv[x], x] == e]}`,
a group from a left identity and left inverses, TypeTheory's test theory. Steps is
the number of rows of the proof (its `"ProofLength"`). Messages are those issued
and on; off messages are in the records.

| Case | Call | Result | Seconds | Messages |
|---|---|---|---|---|
| group-right-identity | `FindEquationalProof[op[a, e] == a, groupAxioms]` | ProofObject, 10 steps | 0.03 | none |
| group-right-inverse | `FindEquationalProof[op[a, inv[a]] == e, groupAxioms]` | ProofObject, 13 steps | 0.01 | none |
| group-inverse-of-product | `FindEquationalProof[inv[op[a, b]] == op[inv[b], inv[a]], groupAxioms]` | ProofObject, 17 steps | 0.01 | none |
| group-forall-goal | `FindEquationalProof[ForAll[{x}, op[x, e] == x], groupAxioms]` | ProofObject, 10 steps | 0.01 | none |
| involution | `FindEquationalProof[f[f[f[f[n]]]] == n, {ForAll[a, f[f[a]] == a]}]` | ProofObject, 4 steps | 0.00 | none |
| ground-chain | `FindEquationalProof[a == c, {a == b, b == c}]` | ProofObject, 5 steps | 0.00 | none |
| single-axiom-not-list | `FindEquationalProof[a == b, a == b]` | ProofObject, 3 steps | 0.00 | none |
| list-of-goals | `FindEquationalProof[{op[a, e] == a, op[a, inv[a]] == e}, groupAxioms]` | ProofObject, 15 steps, two hypotheses and two conclusions | 0.01 | none |
| axiomatic-theory | `FindEquationalProof["InverseOfInverse", "AbelianGroupAxioms"]` | ProofObject, 11 steps | 2.43 | none (AxiomaticTheory's data load issues only off messages) |
| predicate | `FindEquationalProof[mortal[socrates], {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]}]` | ProofObject of logic `"Predicate/EquationalLogic"`, 18 steps | 0.01 | none |
| unknown-option | `FindEquationalProof[a == b, {a == b}, Foo -> 1]` | ProofObject, 3 steps | 0.03 | `OptionValue::nodef` |
| bad-time-constraint | `FindEquationalProof[a == b, {a == b}, TimeConstraint -> -1]` | ProofObject, 3 steps | 0.04 | `FindEquationalProof::timc` |
| unprovable-ground | `FindEquationalProof[a == c, {a == b}]` | `Failure["PropositionFalse", ...]` | 0.00 | none |
| time-constraint | `FindEquationalProof[op[a, b] == op[b, a], groupAxioms, TimeConstraint -> 2]` | `Failure["TimedOut", ...]`, `"Time" -> 2` | 2.01 | none |
| time-constraint-nonterminating | `FindEquationalProof[f[a] == g[a], {ForAll[x, f[g[f[x]]] == g[f[x]]]}, TimeConstraint -> 2]` | `Failure["TimedOut", ...]`, `"Time" -> 2` | 2.02 | none |
| no-axioms-free-functions | `FindEquationalProof[f[x] == g[x], {}]` | unevaluated | 0.04 | `FindEquationalProof::invs` |
| trivial-goal | `FindEquationalProof[a == a, {b == c}]` | unevaluated, as `FindEquationalProof[True, {b == c}]` (the goal evaluates first) | 0.00 | `FindEquationalProof::invs` |
| trivial-goal-no-axioms | `FindEquationalProof[a == a, {}]` | unevaluated, as `FindEquationalProof[True, {}]` | 0.00 | `FindEquationalProof::invs` |
| unequal-goal | `FindEquationalProof[a != c, {a == b}, TimeConstraint -> 5]` | unevaluated, after its time constraint | 5.02 | `FindEquationalProof::invs` |
| one-argument | `FindEquationalProof[a == b]` | unevaluated | 0.03 | `FindEquationalProof::argr` |
| no-arguments | `FindEquationalProof[]` | unevaluated | 0.01 | `FindEquationalProof::argrx` |
| non-option-third-argument | `FindEquationalProof[a == b, {a == b}, 3]` | unevaluated | 0.04 | `FindEquationalProof::nonopt` |
| not-an-equation | `FindEquationalProof[n <= n, {}]` | still running at 120 s | over 120 | none |
| theory-name | `FindEquationalProof[op[a, inv[a]] == e, "GroupAxioms"]` | still running at 120 s | over 120 | none |
| rule-axioms | `FindEquationalProof[a == c, {a -> b, b -> c}]` | still running at 120 s | over 120 | none |

The three slow cases ran under a 120 s `TimeConstrained` of the recorder, with
`FindEquationalProof`'s own `TimeConstraint` at its default, `Infinity`.

### 1.3 The Failures

Wolfram 15 gives exactly these expressions; `Print` shows a Failure as its
message text, `The proposition could not be reduced to True.` and
`No proof could be found within 2 seconds.`

| Outcome | Expression |
|---|---|
| no proof, the search done | `Failure["PropositionFalse", <|"MessageTemplate" -> TemplateObject[{"The proposition could not be reduced to True."}, CombinerFunction -> StringJoin, InsertionFunction -> TextString, MetaInformation -> <||>], "MessageParameters" -> <||>|>]` |
| time limit | `Failure["TimedOut", <|"MessageTemplate" -> TemplateObject[{"No proof could be found within ", TemplateSlot["Time"], " seconds."}, CombinerFunction -> StringJoin, InsertionFunction -> TextString, MetaInformation -> <||>], "MessageParameters" -> <|"Time" -> t|>|>]`, with `t` the `TimeConstraint` as given (the integer `2`, not `2.`) |

### 1.4 Argument checks, as observed

- Zero arguments: `argrx`; one: `argr`; a third argument that is not an option:
  `nonopt`. The call stays unevaluated.
- An unknown option: `OptionValue::nodef`, and the proof goes on.
- A `TimeConstraint` that is not a positive machine number or `Infinity`:
  `timc`, and the proof goes on (which limit it then uses is not observed; v2
  uses `Infinity`).
- `invs`, the call staying unevaluated, for a goal that evaluated to `True`
  (`a == a`), for an empty axiom list with an equation goal, and for an
  `Unequal` goal. For the `Unequal` goal Wolfram 15 says `invs` only after its
  `TimeConstraint` has run out (5.02 s for 5); v2 says it at once, a deliberate
  change for `docs/spec/v2-changes.md`.
- The theorem and the axioms may be names: `"InverseOfInverse"` and
  `"AbelianGroupAxioms"` are read through `AxiomaticTheory`. A single axiom need
  not be in a list (`FindEquationalProof[a == b, a == b]`), and `"Axioms"` then
  gives it unwrapped.
- `n <= n`, an axiom list given by a theory name with an equation goal, and rule
  axioms (`a -> b`) make Wolfram 15 run for more than 120 s (section 1.2).

## 2. ProofObject: arguments and rows

`ProofObject` has attributes `{Protected, ReadProtected}`. A proof is
`ProofObject[logic, theorems, axioms, data]`:

| Argument | Value |
|---|---|
| `logic` | `"EquationalLogic"`, or `"Predicate/EquationalLogic"` when the problem was not equational and was equationalized |
| `theorems` | the goal as given: an equation, a `ForAll` of one, a list of them, or a predicate formula; for a theorem name, the list `{ForAll[\[FormalA], OverBar[OverBar[\[FormalA]]] == \[FormalA]]}` |
| `axioms` | the axioms as given (a single equation stays unwrapped), or the theory's list for a theory name |
| `data` | `<|"Variables" -> {...}, "Constants" -> {...}, "Proof" -> {key -> <|"Statement" -> ..., "Proof" -> <|...|>|>, ...}|>` |

`"Variables"` holds every symbol the proof treats as a variable: the formal
letters its statements use (`\[FormalA]`, `\[FormalB]`, ...), the axioms' bound
variables, and the goal's free symbols that occur in no axiom (`a` in
`op[a, e] == a`; `n` in the involution; not `e`, which the axioms use).
`"Constants"` is empty for equational problems; for the predicate case it is
`{\[FormalC]1, \[FormalC]2}`, which Wolfram 15's own replay treats as variables
too.

### 2.1 The rows

`"Proof"` is a list of rules, in proof order: every row cites only rows before
it. Keys are `{kind, n}`, numbered per kind from 1:

| Kind | Statement | `"Proof"` fields |
|---|---|---|
| `"Axiom"` | `HoldForm[l == r]`: an axiom, possibly turned around, its variables renamed to formal letters (`\[FormalA] == op[e, \[FormalA]]` for `ForAll[x, op[e, x] == x]`); for a predicate axiom, the formula itself, unheld | none (`<||>`) |
| `"Hypothesis"` | `HoldForm[l == r]`: theorem n, its bound variables renamed (`op[\[FormalC], e] == \[FormalC]` for `ForAll[{x}, op[x, e] == x]`); for a predicate goal, the formula | none |
| `"CriticalPairLemma"` | `HoldForm[l == r]`, the critical pair | `Construct`, `Orientation`, `Rule`, `Side`, `Subpattern`, `MatchingConstruct`, `MatchingOrientation`, `MatchingRule`, `MatchingSide`, `Position` |
| `"SubstitutionLemma"` | `HoldForm[l == r]` | `Input`, `Position`, `Construct`, `Orientation`, `Rule`, `OutputExpression`, `ConstructSide`, `InputOrientation`, `Side` |
| `"Conclusion"` | `HoldForm[t == t]` | as a SubstitutionLemma |
| `"EquationalizedAxiom"` | `HoldForm[l == r]` over `Inactive[And]`, `Inactive[Or]`, `Inactive[Not]` | `Input -> {{"Axiom", 1}, {"Axiom", 2}, ...}` |
| `"EquationalizedHypothesis"` | `HoldForm[l == r]` | `Input -> {{"Hypothesis", 1}}` |

The fields:

| Field | Meaning |
|---|---|
| `Construct`, `MatchingConstruct`, `Input` | keys of earlier rows |
| `Orientation`, `MatchingOrientation`, `InputOrientation` | `1` reads the cited statement left to right, `-1` right to left |
| `ConstructSide`, `Side` (of a critical pair), `MatchingSide` | `2` turns the orientation round once more (in the predicate proof's conclusion, `Orientation -> -1` with `ConstructSide -> 2` reads the statement left to right); `1` everywhere else in the records |
| `Side` (of a substitution) | the side of the oriented input that is rewritten, 1 or 2 |
| `Position` | a list of positive integers: within that side for a substitution; within the first rule's left side for a critical pair; `{}` is the root |
| `Rule`, `MatchingRule` | the oriented rule with its variables as patterns, `op[\[FormalA]_, e] -> \[FormalA]`; a commutativity rule shows as a `TwoWayRule` |
| `Subpattern` | the first rule's left side at `Position`, with patterns |
| `OutputExpression` | the statement again |

Quirks of Wolfram 15's own proofs, which a reader of the rows must allow:

- In the predicate proof, `SubstitutionLemma 2`'s statement is a rule with
  patterns, `HoldForm[Inactive[Or][mortal[\[FormalA]_], Inactive[Not][man[\[FormalA]_]]] -> man[socrates]]`,
  not an equation.
- In the same proof, `CriticalPairLemma 4`'s `MatchingRule` is the rule of
  `EquationalizedAxiom 5`, not of its `MatchingConstruct`, `SubstitutionLemma 2`.
  The `Rule` fields are display; the statements and keys carry the proof.
- In `list-of-goals`, `Conclusion 1` comes before `CriticalPairLemma 4`: rows are
  in proof order, not grouped by kind.

## 3. ProofObject's properties

`p["Properties"]` gives the fifteen names below, in this order. `p[name]` for
another name stays unevaluated, without a message. Two names outside the list
also answer: `p["Theorem"]` gives the theorems, and `p["ProofAssociation"]` gives
`Normal[p["ProofDataset"]]`.

| Property | Value |
|---|---|
| `"Axioms"` | the third argument |
| `"Logic"` | the first argument |
| `"ProofDataset"` | `Dataset[assoc, type, <||>]`, where `Normal` gives `Association[data["Proof"]]` (true on all twelve recorded proofs); `type` is a `TypeSystem` description v2 does not need |
| `"ProofFunction"` | `Function[{}, ...]`, a replay; applied to nothing or to the theorems it gives `Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>]` |
| `"ProofGraph"` | the proof as a `Graph` (section 3.1) |
| `"ProofGraphLabeled"` | the same vertices and edges, its vertices drawn as framed formulas (`VertexShapeFunction`) |
| `"ProofGraphWeighted"` | the same vertices and edges, `VertexSize` by a weight per vertex, `GraphLayout -> {"Dimension" -> 2, "VertexLayout" -> "LayeredDigraphEmbedding"}` |
| `"ProofGraphSizeWeighted"` | the same vertices and edges, `VertexSize` by the size of each statement |
| `"TokenEventProofGraph"` | the proof with an event vertex per derived row (section 3.1) |
| `"TokenEventProofGraphLabeled"` | the same vertices and edges, drawn as framed formulas |
| `"ProofLength"` | the number of rows |
| `"ProofNotebook"` | `DocumentNotebook[{cells}]` (section 3.2) |
| `"Theorems"` | the second argument |
| `"Variables"` | `data["Variables"]` |
| `"Constants"` | `data["Constants"]` |

The values on the recorded proofs:

| Case | Logic | Theorems | ProofLength | Variables | Constants |
|---|---|---|---|---|---|
| group-right-identity | EquationalLogic | `op[a, e] == a` | 10 | `{\[FormalA], \[FormalB], \[FormalC], a, x, y, z}` | `{}` |
| group-right-inverse | EquationalLogic | `op[a, inv[a]] == e` | 13 | `{\[FormalA], \[FormalB], \[FormalC], a, x, y, z}` | `{}` |
| group-inverse-of-product | EquationalLogic | `inv[op[a, b]] == op[inv[b], inv[a]]` | 17 | `{\[FormalA], \[FormalB], \[FormalC], a, b, x, y, z}` | `{}` |
| group-forall-goal | EquationalLogic | `ForAll[{x}, op[x, e] == x]` | 10 | `{\[FormalA], \[FormalB], \[FormalC], x, y, z}` | `{}` |
| involution | EquationalLogic | `f[f[f[f[n]]]] == n` | 4 | `{a, n}` | `{}` |
| ground-chain | EquationalLogic | `a == c` | 5 | `{}` | `{}` |
| single-axiom-not-list | EquationalLogic | `a == b` | 3 | `{}` | `{}` |
| list-of-goals | EquationalLogic | `{op[a, e] == a, op[a, inv[a]] == e}` | 15 | `{\[FormalA], \[FormalB], \[FormalC], a, x, y, z}` | `{}` |
| axiomatic-theory | EquationalLogic | `{ForAll[\[FormalA], OverBar[OverBar[\[FormalA]]] == \[FormalA]]}` | 11 | `{\[FormalA], ..., \[FormalH]}` | `{}` |
| predicate | Predicate/EquationalLogic | `mortal[socrates]` | 18 | `{\[FormalA], ..., \[FormalN], \[FormalC]2}` | `{\[FormalC]1, \[FormalC]2}` |

The involution's axiom keeps its own variable name: its statement is
`HoldForm[a == f[f[a]]]`, not a formal letter.

### 3.1 The graphs

All six graphs have the same statement vertices: one per row,
`HoldForm[Tooltip[statement, key]]`, the statement being the row's statement
without its `HoldForm` (`HoldForm[Tooltip[\[FormalA] == op[e, \[FormalA]], {"Axiom", 1}]]`).
`VertexList` gives them in canonical order, the order `Sort` gives (checked on
`group-inverse-of-product`).

Edges, `DirectedEdge[from, to, label]`, in row order, from each cited row to the
row that cites it:

| Row kind | Edges into it |
|---|---|
| SubstitutionLemma, Conclusion | from `Input`, label `"Input"`; from `Construct`, label `"Construct"` |
| CriticalPairLemma | from `Construct`, label `"CPConstruct"`; from `MatchingConstruct`, label `"MatchingConstruct"` |
| EquationalizedAxiom, EquationalizedHypothesis | from each `Input`, no label (`DirectedEdge[from, to]`) |

The token-event graphs add a vertex `{"Event", key}` per derived row, after the
statement vertices; each edge above runs into the event instead, with the same
label, and the event has an unlabelled edge to its row.

Options of `ProofGraph`, `ProofGraphWeighted`, `ProofGraphSizeWeighted` and
`TokenEventProofGraph`: `EdgeLabels`, `EdgeStyle`, `GraphLayout`,
`PerformanceGoal`, `VertexLabels`, `VertexSize`, `VertexStyle`; the two
`...Labeled` graphs add `VertexShapeFunction`. On `involution`: `GraphLayout ->
"LayeredDigraphEmbedding"`, `PerformanceGoal -> "Quality"`, every edge label
`None`, an `"Input"` edge in `Hue[0.1, 0.34, 0.49]` and a `"Construct"` edge
dashed (`AbsoluteDashing[{3, 1}]`, `AbsoluteThickness[1.2]`) in the same hue,
`VertexSize -> {"Scaled", 0.052000000000000005}` per vertex, and a vertex label
`Placed[Tooltip[Row[{"Substitution", "Lemma", 1}, " "], HoldForm[statement]], Above]`,
the kind split into its words. Event vertices are labelled
`Placed[Row[{"SubstitutionLemma", 1}, " "], Tooltip]`. Vertex colours depend on the
kind; v2 draws them with the standard colours of its own graphics.

### 3.2 The notebook

`"ProofNotebook"` is `DocumentNotebook[{cells}]`, three cells per row, plus a
cell group for a derived row (36 cells for the 10 rows of group-right-identity):

| Row | Cells |
|---|---|
| any | `TextCell["Axiom 1", "Subsection"]` (the kind in words and its number), a `"CodeText"` cell, `ExpressionCell[statement, "Code"]` |
| Axiom | `"We are given that:"` |
| Hypothesis | `"We would like to show that:"` |
| SubstitutionLemma | `"It can be shown that:"`, then `CellGroup[{TextCell["Proof", "Subsubsection", FontVariations -> {"CapsType" -> "SmallCaps"}], "We start by taking <Input>, and apply the substitution:", the rule as a code cell, "which follows from <Construct>."}, 1]` |
| Conclusion | `"We obtain the conclusion:"`, then the group with `"Take <Input>, and apply the substitution:"` |
| CriticalPairLemma | `"The following expressions are equivalent:"`, then the group with `"Note that the input for the rule:"`, the rule, `"contains a subpattern of the form:"`, the subpattern, `"which can be unified with the input for the rule:"`, the matching rule, `"where these rules follow from <Construct> and <MatchingConstruct> respectively."` |
| EquationalizedAxiom, EquationalizedHypothesis | `"We generate the ''equationalized'' axiom:"` or `hypothesis:` |

## 4. The proof check

`ProofFunction` is the property TypeTheory does not read but THVMLink does:
`TFindProof` accepts a proof it built only when
`p["ProofFunction"][p["Theorems"]]` gives a `Success`. A kernel without Wolfram
15's `ProofObject` has no such Function, so THVMLink carries its own check,
`atpProofCheck` in `wl/THVMLink/Kernel/ATP/ProofCheck.wl`, and `TFindProof`
uses it whenever `p["ProofFunction"]` does not give a Function
(`atpProofVerify`). On Wolfram 15 it keeps the built-in. v2's `"ProofFunction"`
property can be this same check, wrapped as a Function of no arguments.

The check replays the rows from the four arguments alone. Its value is
`Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>]`, Wolfram 15's,
or a Failure that names the first step that does not hold:
`Failure["TFindProof", <|"MessageTemplate" :> TFindProof::pfstep, "MessageParameters" -> {key, reason}, "Tag" -> "pfstep"|>]`,
with `pfgoal` when a theorem has no conclusion and `pfform` for an argument that
is not a four-argument `ProofObject`.

Terms. The symbols in `"Variables"` and `"Constants"` are variables, as Wolfram
15's replay treats them; every other symbol is a constant. Terms are compared as
inert trees, so no step evaluates them. A statement holds when it is the
computed equation up to a renaming of its variables (a bijection) and the order
of its sides. A rule `l -> r` read off a row is its statement oriented by the
orientation times the sign of the side field (side 2 is -1).

| Row | Holds when |
|---|---|
| Axiom | its statement is one of the axioms, or one turned around, the axiom's `ForAll` variables and pattern names renamed; a predicate axiom row is an axiom itself |
| Hypothesis n | its statement is theorem n, its bound and free variables renamed; a predicate hypothesis row is the theorem itself |
| SubstitutionLemma | take the Input's statement, turned around when `InputOrientation` is -1; the Construct's rule matches (one-way: only the rule's variables bind) the subterm at `Position` of side `Side`; replacing that subterm by the instantiated right side gives the statement |
| Conclusion | as a SubstitutionLemma, its two sides are the same term, and it rests on a hypothesis |
| CriticalPairLemma | the Construct's rule and the MatchingConstruct's rule, their variables renamed apart (even when both are one row), have a most general unifier of the first rule's left side at `Position` with the second rule's left side; the statement is `{right side of the first, its left side with the subterm at Position replaced by the right side of the second}` under the unifier |
| EquationalizedAxiom | an equation citing earlier Axiom rows; taken as given, as Wolfram 15's replay takes it |
| EquationalizedHypothesis | an equation citing earlier Hypothesis rows; taken as given |

And over the whole proof:

- every cited key names an earlier row, which is an equation;
- a row derived from a hypothesis (through Input links) is no lemma for all
  values: it may be an Input but not a Construct or MatchingConstruct, unless
  its rule rewrites a term to itself (THVMLink states an already reflexive goal
  so). Wolfram 15's replay does not check this; THVMLink's acceptance did
  separately (`atpProofWellFoundedQ`);
- every theorem has a Conclusion derived from its hypothesis.

The `Rule`, `MatchingRule`, `Subpattern` and `OutputExpression` fields are not
read (section 2.1's second quirk). Equationalization is not replayed.

Tests (`wl/THVMLink/Tests/atp_proofcheck.wlt`, run on Wolfram 15.0 on the w15
pod): 12 of 12 pass. The check gives Success on Wolfram 15's ten proofs of the
cases in that file, on the twelve recorded proofs read back from
`data/fep_w15_records.wl`, and on THVMLink's ten proofs of the same goals, which
`TFindProof` accepted on the check itself; Wolfram 15's own ProofFunction agrees
on all of them. Changed proofs fail at the step changed: a critical pair's
statement or position; a substitution's orientation, side or position; an
axiom, the hypothesis or the conclusion stated otherwise; the conclusion
dropped (`pfgoal`); a critical pair citing the hypothesis as its rule; rows out
of order; a THVMLink critical pair's statement or its matching row; the
predicate proof's hypothesis. The replay takes 0.2 to 3.7 ms per recorded
proof, Wolfram 15's ProofFunction 0.6 to 11.5 ms.

## 5. v2's FindEquationalProof on TFindProof

### 5.1 Loading THVMLink

As `FullSimplify` loads the prover package (`$proverDirectories` and
`equationalProverQ[]` in `library/SystemLibrary/Algebra/FullSimplify.wl`), the
library loads THVMLink at the first call of `FindEquationalProof`, from the
first of these directories that exists:

1. `FileNameJoin[{$InstallationDirectory, "SystemFiles", "Components", "THVMLink"}]`;
2. `FileNameJoin[{DirectoryName[$InstallationDirectory], "thvm", "wl", "THVMLink"}]`,
   for a kernel run from its repository with `third_party/wolfram` as its
   installation (`third_party/thvm/wl/THVMLink`).

It loads with `PacletDirectoryLoad[dir]` and ``Needs["WolframInstitute`THVMLink`"]``
inside `Block[{$ContextPath = $ContextPath}, ...]` and `Quiet`, and calls
THVMLink by full names (``WolframInstitute`THVMLink`ATP`TFindProof``), so a
session's own contexts are untouched. Whether it loaded is read off a call that
needs THVMLink's library, not off messages:
``Head[WolframInstitute`THVMLink`ATP`TAtpComplete[{}]] === WolframInstitute`THVMLink`ATP`TAtpSystem``
(true on Wolfram 15 with the 0.2.6 paclet). The answer is kept, so the load is
tried once. Without THVMLink the call cannot start; v2 then gives a Failure of
its own, a deliberate change (Wolfram 15 always has its prover).

### 5.2 The call

After the argument checks of section 1.4, v2 calls

`` WolframInstitute`THVMLink`ATP`TFindProof[goal, axioms, {"ProofObject", "Status"}, TimeConstraint -> t] ``

with `t` the `TimeConstraint` option (default `Infinity`). The list return spec
gives `<|"ProofObject" -> proof or a Failure, "Status" -> status|>`, and v2 maps
on the status tag, not on message texts:

| Status | TFindProof's reason | v2 returns |
|---|---|---|
| `"Proved"` with a ProofObject | | that ProofObject |
| `"Saturated"` | the completion saturated without joining the two sides of the conjecture | `Failure["PropositionFalse", ...]` of section 1.3 |
| `"TimedOut"` | the time limit was reached | `Failure["TimedOut", ...]` with `"Time" -> t` |
| `"Aborted"` | memory filled or the evaluation was aborted | `$Aborted` |
| `"Failed"` | the step limit `MaxSteps` was reached, the conjecture refuted, or the proof not built | TFindProof's Failure, passed through: no Wolfram 15 counterpart |

TFindProof resolves theorem and theory names through `AxiomaticTheory` itself,
and equationalizes predicate problems itself; its ProofObject has Wolfram 15's
layout (section 2), its `"Logic"` included. It accepts a proof only on its check
(section 4), which on v2 is THVMLink's own replay, with one exception it already
had: a proof whose completion used Waldmeister's free-variable instantiation
(FVI, a rule instantiated at variables only its right side has) cites steps no
replay teaches, and `TFindProof` accepts it on the engine's own gate.

Checked on Wolfram 15 with `FindEquationalProof`'s rules replaced, in the test
kernel, by this entry: the PropositionFalse and both TimedOut Failures are
`===` to Wolfram 15's recorded ones, the empty-axiom call says `invs`, and
TypeTheory's `Tests/Proofs/Equational.wlt` passes 3 of 3 on THVMLink's proofs.

### 5.3 THVMLink against Wolfram 15, case by case

`TFindProof[goal, axioms, "ProofObject", TimeConstraint -> 30]` on each recorded
call (the records' `"THVMLink"` field):

| Case | Wolfram 15 | TFindProof | Seconds |
|---|---|---|---|
| the ten provable cases of section 1.2, and unknown-option | ProofObject | ProofObject; the check and Wolfram 15's ProofFunction both give Success | 0.00 to 0.38 |
| unprovable-ground | PropositionFalse | no proof: saturated | 0.02 |
| time-constraint, time-constraint-nonterminating | TimedOut | no proof: the time limit (at 2 s, as asked) | 2.03, 2.05 |
| no-axioms-free-functions | `invs` | no proof: saturated (v2's argument check answers first) | 0.01 |
| trivial-goal, trivial-goal-no-axioms, unequal-goal | `invs` | `Failure["TATPParseError", ...]` (v2's argument check answers first) | 0.00 |
| bad-time-constraint | `timc`, then a ProofObject | `$Failed` (v2 checks the option and passes `Infinity`) | 0.00 |
| not-an-equation, theory-name | running past 120 s | no proof: the time limit (30 s) | 30.6, 30.1 |
| rule-axioms | running past 120 s | ProofObject | 0.05 |

THVMLink's proofs differ from Wolfram 15's where nothing reads the difference:
their `"Variables"` hold the axioms' variables and the goal's Skolem symbols, not
the formal letters (`{x, y, z}` for group-right-identity); statements name
variables as the axioms do; the rows carry an extra `"Source"` field; their
number differs. They proved `rule-axioms`, where Wolfram 15 runs on: a
divergence for `v2-changes.md`.

One difference did matter and is fixed in THVMLink: on its rewrite-chain path,
`TFindProof` stated an axiom with the encoder's patterns,
`HoldForm[f1[f1[x1_]] == x1_]`, where Wolfram 15 writes `f1[f1[x1]] == x1`.
Both replays accepted it, but TypeTheory's tactic read `x1_` as a term outside
the goal and stopped (`a step of the proof has a term outside the goal and the
hypotheses (Axiom 1)`), failing its involution test. `toHoldEq`
(`wl/THVMLink/Kernel/ATP/ATP_ProofGraph.wl`) now writes statements without
patterns; the `Rule` fields keep them.

### 5.4 The ProofObject unit

v2's `library/SystemLibrary/Logic/ProofObject.wl` gives the properties of
section 3 from the four arguments. In the order to build them:

1. `"Properties"` (the full list of fifteen, whatever is built), `"Theorems"`,
   `"Axioms"`, `"Logic"`, `"Variables"`, `"Constants"`, `"ProofLength"`,
   `"ProofDataset"` (`Dataset[Association[data["Proof"]]]`, section 6) and
   `"ProofFunction"` (the check of section 4 as `Function[{}, check]`, a function
   that ignores its arguments, as Wolfram 15's `Function[{}, ...]` does); an
   unknown property stays unevaluated without a message. These are what
   TypeTheory and the tests read.
2. The graphs (section 3.1): vertices, edges and labels first, styles after.
3. `"ProofNotebook"` (section 3.2).

## 6. A minimal Dataset

`"ProofDataset"` needs a `Dataset` and TypeTheory reads it only through
`Normal`. v2 has none yet, so its first `Dataset` is a unit of its own, loaded at
first use: an inert head of one argument, `Dataset[data]`, with
`Normal[Dataset[data, ___]]` giving `data`. Wolfram 15's carries a type and
metadata, `Dataset[data, type, <||>]`; v2 accepts and ignores them, so a
Wolfram 15 dataset read back also gives its data. Display, queries and the type
system come later.

## 7. Tests

v2's suite `tests/library/findequationalproof/` can be written from this
program and its output, recorded on Wolfram 15.0 through a copy of
`tools/oracle/run-program.wls` (messages printed by name). It reads only values
that do not depend on the proof the prover finds.

`main.wl`:

```wolfram
(* FindEquationalProof and ProofObject as TypeTheory's tactic reads them: the group laws from a left
   identity and left inverses, a fourfold involution, several goals at once, and the Failures. Values
   that depend on the proof the prover finds, its steps and their number, are left out. *)

groupAxioms = {ForAll[{x, y, z}, op[op[x, y], z] == op[x, op[y, z]]], ForAll[x, op[e, x] == x], ForAll[x, op[inv[x], x] == e]}
summary[p_] := {Head[p], p["Theorems"], p["Axioms"], p["Logic"], Head[p["ProofDataset"]], p["ProofFunction"][], p["ProofFunction"][p["Theorems"]]}
Print[summary[FindEquationalProof[op[a, e] == a, groupAxioms]]]
Print[summary[FindEquationalProof[op[a, inv[a]] == e, groupAxioms]]]
Print[summary[FindEquationalProof[inv[op[a, b]] == op[inv[b], inv[a]], groupAxioms]]]
Print[summary[FindEquationalProof[ForAll[{x}, op[x, e] == x], groupAxioms]]]
Print[summary[FindEquationalProof[f[f[f[f[n]]]] == n, {ForAll[a, f[f[a]] == a]}]]]
Print[summary[FindEquationalProof[{op[a, e] == a, op[a, inv[a]] == e}, groupAxioms]]]
Print[summary[FindEquationalProof[a == c, {a == b, b == c}]]]
Print[summary[FindEquationalProof[a == b, a == b]]]
p = FindEquationalProof[op[a, e] == a, groupAxioms];
steps = Normal[p["ProofDataset"]];
Print[{p["Properties"], p["ProofLength"] === Length[steps], SubsetQ[First /@ Keys[steps], {"Axiom", "Hypothesis", "Conclusion"}], steps[{"Hypothesis", 1}, "Statement"]}]
Print[{MatchQ[steps, <|({_String, _Integer} -> KeyValuePattern[{"Statement" -> _, "Proof" -> _Association}])..|>], p["Variables"] === p[[4, "Variables"]], p["Constants"]}]
Print[MatchQ[p["NoSuchProperty"], _ProofObject["NoSuchProperty"]]]
Print[FindEquationalProof[a == c, {a == b}]]
Print[FindEquationalProof[f[a] == g[a], {ForAll[x, f[g[f[x]]] == g[f[x]]]}, TimeConstraint -> 2]]
Print[FindEquationalProof[f[x] == g[x], {}]]
Print[FindEquationalProof[a == b]]
```

`main.out`, as Wolfram 15.0 printed it:

```text
{ProofObject, op[a, e] == a, {ForAll[{x, y, z}, op[op[x, y], z] == op[x, op[y, z]]], ForAll[x, op[e, x] == x], ForAll[x, op[inv[x], x] == e]}, EquationalLogic, Dataset, Success[ProofFunction, <|MessageTemplate -> Success!|>], Success[ProofFunction, <|MessageTemplate -> Success!|>]}
{ProofObject, op[a, inv[a]] == e, {ForAll[{x, y, z}, op[op[x, y], z] == op[x, op[y, z]]], ForAll[x, op[e, x] == x], ForAll[x, op[inv[x], x] == e]}, EquationalLogic, Dataset, Success[ProofFunction, <|MessageTemplate -> Success!|>], Success[ProofFunction, <|MessageTemplate -> Success!|>]}
{ProofObject, inv[op[a, b]] == op[inv[b], inv[a]], {ForAll[{x, y, z}, op[op[x, y], z] == op[x, op[y, z]]], ForAll[x, op[e, x] == x], ForAll[x, op[inv[x], x] == e]}, EquationalLogic, Dataset, Success[ProofFunction, <|MessageTemplate -> Success!|>], Success[ProofFunction, <|MessageTemplate -> Success!|>]}
{ProofObject, ForAll[{x}, op[x, e] == x], {ForAll[{x, y, z}, op[op[x, y], z] == op[x, op[y, z]]], ForAll[x, op[e, x] == x], ForAll[x, op[inv[x], x] == e]}, EquationalLogic, Dataset, Success[ProofFunction, <|MessageTemplate -> Success!|>], Success[ProofFunction, <|MessageTemplate -> Success!|>]}
{ProofObject, f[f[f[f[n]]]] == n, {ForAll[a, f[f[a]] == a]}, EquationalLogic, Dataset, Success[ProofFunction, <|MessageTemplate -> Success!|>], Success[ProofFunction, <|MessageTemplate -> Success!|>]}
{ProofObject, {op[a, e] == a, op[a, inv[a]] == e}, {ForAll[{x, y, z}, op[op[x, y], z] == op[x, op[y, z]]], ForAll[x, op[e, x] == x], ForAll[x, op[inv[x], x] == e]}, EquationalLogic, Dataset, Success[ProofFunction, <|MessageTemplate -> Success!|>], Success[ProofFunction, <|MessageTemplate -> Success!|>]}
{ProofObject, a == c, {a == b, b == c}, EquationalLogic, Dataset, Success[ProofFunction, <|MessageTemplate -> Success!|>], Success[ProofFunction, <|MessageTemplate -> Success!|>]}
{ProofObject, a == b, a == b, EquationalLogic, Dataset, Success[ProofFunction, <|MessageTemplate -> Success!|>], Success[ProofFunction, <|MessageTemplate -> Success!|>]}
{{Axioms, Logic, ProofDataset, ProofFunction, ProofGraph, ProofGraphLabeled, ProofGraphWeighted, ProofGraphSizeWeighted, TokenEventProofGraph, TokenEventProofGraphLabeled, ProofLength, ProofNotebook, Theorems, Variables, Constants}, True, True, HoldForm[op[a, e] == a]}
{True, True, {}}
True
The proposition could not be reduced to True.
No proof could be found within 2 seconds.
FindEquationalProof::invs
FindEquationalProof[f[x] == g[x], {}]
FindEquationalProof::argr
FindEquationalProof[a == b]
```

Every value in it holds for THVMLink's proofs too: the hypothesis statement of
group-right-identity is `HoldForm[op[a, e] == a]` in both, `"Constants"` is
`{}`, and the TimedOut case uses a goal on which `TFindProof` also runs into the
limit rather than saturating.

## 8. What v2 needs, and what is open

v2 functions the units rely on:

- THVMLink itself, loaded whole: its kernel files (``GeneralUtilities` `` for
  `SetUsage`), its LibraryLink library through `LibraryFunctionLoad`, and its
  paclet dependency Wolfram/Parser (used for TPTP input only, not by
  `FindEquationalProof`). The proof check runs on v2's evaluator: `HoldForm`,
  `HoldPattern`, `Verbatim`, `Unevaluated` and `HoldAllComplete` under pattern
  matching, `ReplaceAll` with `Dispatch`, `ReplacePart`, `Cases` with
  `Heads -> True`, `NestWhile`, `FreeQ`, `Catch`/`Throw` with tags, `Fold`,
  `KeyExistsQ`, `Lookup`, `Union`, `Thread`.
- For the entry: `OptionsPattern`/`OptionValue`, `Message`, `Failure`,
  `Success`, `TemplateObject` and `TemplateSlot` as inert expressions with
  Wolfram 15's option order, `PacletDirectoryLoad`, `Needs`, `Block`, `Quiet`,
  and `Print` of a Failure showing its message text.
- For the properties: `Dataset` (section 6), `Normal`; later `Graph`,
  `DirectedEdge`, `Tooltip`, `Placed`, `Row` and the graph options;
  `DocumentNotebook`, `TextCell`, `ExpressionCell`, `CellGroup`, `DisplayForm`,
  `RowBox`.

Open:

- The graph properties and `"ProofNotebook"` on v2.
- `TFindProof` with `TimeConstraint -> -1` gives `$Failed`; v2's option check
  must answer first.
- The `"Failed"` status (step limit, refuted, proof not built) has no Wolfram 15
  counterpart; its Failure passes through.
- The equationalized rows are taken as given by both replays; a check of the
  Boolean encoding is not written.
