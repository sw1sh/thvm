---
Template: Symbol
Name: TFindFiniteModels
Context: THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindFiniteModels
Keywords: [finite model, model finding, congruence closure, Cayley table, axiom, operator, Wolfram axiom, NKS, counterexample]
SeeAlso: [TSatEUF, TSmtDecide, TFindProof, FindFiniteModels]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindFiniteModels]()[*rels*]</code> finds models, as multiplication tables, consistent with the relations *rels* for each operator in *rels*, assuming each variable can take one of two values.

<code>[TFindFiniteModels]()[*rels*, *k*]</code> allows *k* >= 2 values for each variable.

<code>[TFindFiniteModels]()[*rels*, *k*, *prop*]</code> returns a specified property *prop* of the found models.

It is the [FindFiniteModels]() resource function ported into the paclet, behaving identically, with added [Method]() values - `"ExpressionPruneC"`, `"SAT"`, `"CongruenceClosure"`, `"CongruenceClosureC"`, and `"Z3"` - that decide the search with thvm's own engines (or the Z3 SMT solver) instead of the built-in compiled / propositional search.

## Details & Options

- The relations *rels* are given in terms of operators in logical form with equalities (<code>[Equal]()</code>) and inequalities (<code>[Unequal]()</code>), or as string relations.  An [AxiomaticTheory]() axiom set may be passed directly.
- There are *k*^(*k*^*a*) models for an operator of arity *a*.
- The property *prop* may be one of:

| | |
|---|---|
| `"Association"` | an association from model index to multiplication tables (default) |
| `"Indices"` | the list of model indices |
| `"Models"` | the list of models |

- The model index records the position of each operator's table in the enumeration (shifted by 1), so it lines up with [BooleanFunction]() indices under `"ReverseOrdering" -> True`.
- Options:

| | | |
|---|---|---|
| [Method]() | `"ExpressionPrune"` | the search method |
| `"Parallelize"` | [False]() | run parts of the search in parallel |
| [MaxItems]() | [Infinity]() | cap the number of returned models |
| `"ReverseOrdering"` | [False]() | reverse the index ordering |

- [Method]() values:

| | |
|---|---|
| `"ExpressionPrune"` | pruned search over the relations as a conjunction of DNFs (default) |
| `"ExpressionPruneC"` | the same pruned search with the hot join/extend/encode loop in C (WL clausifies, C enumerates) |
| `"BruteForce"` | a complete search over all possible models |
| `"SAT"` | enumeration through the built-in [SatisfiabilityInstances]() solver |
| `"CongruenceClosure"` | backtracking pruned by thvm's WL congruence closure ([TSatEUF]()) |
| `"CongruenceClosureC"` | the same backtracking, decided by thvm's C congruence-closure engine (`src/cc`) through a LibraryLink binding |
| `"Z3"` | encodes the problem for the Z3 SMT solver (via the `WolframInstitute/Z3Link` paclet) and enumerates models by block-and-resolve |

## Basic Examples

Binary models for a nullary operator:

```wl
TFindFiniteModels[a[] -> a[]]
```
<!-- => <|{0} -> <|a -> 0|>, {1} -> <|a -> 1|>|> -->

Ternary models for a nullary operator:

```wl
TFindFiniteModels[a[] == b[], 3]
```
<!-- => <|{0, 0} -> <|a -> 0, b -> 0|>, {1, 1} -> <|a -> 1, b -> 1|>, {2, 2} -> <|a -> 2, b -> 2|>|> -->

Binary models for a unary operator - the identity law:

```wl
TFindFiniteModels[f[a] == a]
```
<!-- => <|{1} -> <|f -> {0, 1}|>|> -->

No model identifies a free variable with a unary image:

```wl
TFindFiniteModels[f[a] == b]
```
<!-- => <||> -->

Binary models for two unary operators forced to agree:

```wl
TFindFiniteModels[f[a] == g[a], 2]
```
<!-- => <|{0, 0} -> <|f -> {0, 0}, g -> {0, 0}|>, {1, 1} -> <|f -> {0, 1}, g -> {0, 1}|>, {2, 2} -> <|f -> {1, 0}, g -> {1, 0}|>, {3, 3} -> <|f -> {1, 1}, g -> {1, 1}|>|> -->

Binary models for a binary operator (left projection):

```wl
TFindFiniteModels[f[a, b] == a]
```
<!-- => <|{3} -> <|f -> {{0, 0}, {1, 1}}|>|> -->

Ternary models for commutativity:

```wl
TFindFiniteModels[f[a, b] == f[b, a], 3] // Short
```

Binary models for a ternary operator:

```wl
TFindFiniteModels[f[a, b, c] == f[a, b, b]]
```

Include constants:

```wl
TFindFiniteModels[f[a, b] == g[0] == 1]
```
<!-- => <|{15, 2} -> <|f -> {{1, 1}, {1, 1}}, g -> {1, 0}|>, {15, 3} -> <|f -> {{1, 1}, {1, 1}}, g -> {1, 1}|>|> -->

## Scope

In the absence of operators every atom is taken as a nullary operator:

```wl
TFindFiniteModels[a -> b]
```
<!-- => <|{0, 0} -> <|a -> 0, b -> 0|>, {1, 1} -> <|a -> 1, b -> 1|>|> -->

Multiple operators with different arities:

```wl
TFindFiniteModels[f[a, b] == g[b, f[b, a], a]] // Short
```

Inequalities:

```wl
TFindFiniteModels[a != b]
```
<!-- => <|{0, 1} -> <|a -> 0, b -> 1|>, {1, 0} -> <|a -> 1, b -> 0|>|> -->

```wl
TFindFiniteModels[f[a, b] != 0]
```
<!-- => <|{15} -> <|f -> {{1, 1}, {1, 1}}|>|> -->

```wl
TFindFiniteModels[f[b, f[a, a]] != b]
```
<!-- => <|{12} -> <|f -> {{1, 1}, {0, 0}}|>|> -->

Alternatives:

```wl
TFindFiniteModels[f[a, b] != a | f[a, b] == a]
```
<!-- => <|{3} -> <|f -> {{0, 0}, {1, 1}}|>, {12} -> <|f -> {{1, 1}, {0, 0}}|>|> -->

Disjunctions:

```wl
TFindFiniteModels[f[a, b] != a || f[a, b] == a]
```

Existential quantifier:

```wl
TFindFiniteModels[Exists[{x}, x == f[x, y]]]
```

Universal quantifier:

```wl
TFindFiniteModels[ForAll[{f, x}, x == f[x, y]]]
```
<!-- => <|{3} -> <|f -> {{0, 0}, {1, 1}}|>|> -->

String relations carry the monoid axioms automatically:

```wl
TFindFiniteModels["ABBA" -> "BAAB"]
```

A string relation is exactly the monoid presentation with a binary associative operator and a nullary identity:

```wl
TFindFiniteModels["A" -> "AA", 3] ===
    TFindFiniteModels[{"A" == "A" \[SmallCircle] "A", ("A" \[SmallCircle] "B") \[SmallCircle] "C" == "A" \[SmallCircle] ("B" \[SmallCircle] "C"), "1"[] \[SmallCircle] "A" == "A", "A" \[SmallCircle] "1"[] == "A"}, 3]
```
<!-- => True -->

## Options

### Method

`"BruteForce"` enumerates every model; it can win when a relation is so general that a small subset is found faster than the pruned search:

```wl
TFindFiniteModels[f[0, a] == 0, 4, MaxItems -> 2] // AbsoluteTiming
```

```wl
TFindFiniteModels[f[0, a] == 0, 4, MaxItems -> 2, Method -> "BruteForce"] // AbsoluteTiming
```

All six methods return the same models - here on the Wolfram axiom:

```wl
SameQ @@ (TFindFiniteModels[AxiomaticTheory["WolframAxioms"], Method -> #]& /@
    {"ExpressionPrune", "ExpressionPruneC", "BruteForce", "SAT", "CongruenceClosure", "CongruenceClosureC", "Z3"})
```
<!-- => True -->

`"CongruenceClosureC"` decides each search node with thvm's C congruence-closure engine; `"Z3"` hands the encoded problem to the Z3 SMT solver.  Both are alternative backends rather than the default - `ExpressionPrune` is fastest in general.

### Parallelize

Setting `"Parallelize" -> True` may speed up the search:

```wl
#| eval: false
Length @ TFindFiniteModels[b \[SmallCircle] (a \[SmallCircle] a) == a, 8, "Parallelize" -> False] // AbsoluteTiming
```
<!-- => {4.93, 764} -->

```wl
#| eval: false
Length @ TFindFiniteModels[b \[SmallCircle] (a \[SmallCircle] a) == a, 8, "Parallelize" -> True] // AbsoluteTiming
```
<!-- => {1.89, 764} -->

### MaxItems

Limiting the number of returned models reduces the work performed:

```wl
TFindFiniteModels[f[b, f[a, b]] == a, 10, MaxItems -> 2]
```

### ReverseOrdering

By default models are indexed by their inputs shifted by 1; a model of [Not]() for `BooleanAxioms` is `{1, 0}` because `{1, 0}[[0 + 1]] == 1` and `{1, 0}[[1 + 1]] == 0`:

```wl
TFindFiniteModels[AxiomaticTheory["BooleanAxioms"]]
```

With `"ReverseOrdering" -> True` the indices correspond to [BooleanFunction]() indices and the models to truth tables:

```wl
TFindFiniteModels[AxiomaticTheory["BooleanAxioms"], "ReverseOrdering" -> True]
```

Then `CirclePlus` (index 8) is a model of [And](), matching its [BooleanFunction]() truth table:

```wl
Boole @ BooleanTable[BooleanFunction[8, 2], {1}, {2}]
```
<!-- => {{1, 0}, {0, 0}} -->

## Neat Examples

The model index decodes to an operator table through [IntegerDigits]() (the `NestedTupleFromIndex` the function uses internally):

```wl
NestedTupleFromIndex[index_, k_, n_] := ArrayReshape[IntegerDigits[index, k, k^n], Table[k, n]]
```

A helper that plots the operator tables of the found models across domain sizes - the *NKS* operator-table picture:

```wl
plotRelationConstraints[expr_, k_ : 2, limit_ : Infinity, n_ : UpTo[6]] :=
    Module[{indices = TFindFiniteModels[expr, k, "Indices", MaxItems -> limit], sampleIndices, tables},
        sampleIndices = Take[indices, n];
        tables = Map[NestedTupleFromIndex[#[[1]], k, 2]&, sampleIndices];
        Framed @ Row[
            Append[
                ArrayPlot[#, Mesh -> True, ImageSize -> 32]& /@ tables,
                If[ Length[sampleIndices] == 0, Row @ {"(", 0, ")"},
                    If[ Length[indices] - Length[sampleIndices] > 0, Row @ {"...", "(", Length[indices], ")"}, Nothing]]
            ]
        ]
    ]
```

Operator tables of the abelian-semigroup models across two, three, and four elements:

```wl
Row[Riffle[plotRelationConstraints[{a \[CenterDot] b == b \[CenterDot] a, (a \[CenterDot] b) \[CenterDot] c == a \[CenterDot] (b \[CenterDot] c)}, #]& /@ Range[2, 4], Spacer[8]]]
```

Reproduce the [NKS book examples](https://www.wolframscience.com/nks/p804--implications-for-mathematics-and-its-foundations/) - each relation's model tables across domain sizes, timed:

```wl
Scan[
    relation |-> Print @ AbsoluteTiming @ Row[Riffle[Prepend[plotRelationConstraints[{relation}, #]& /@ Range[2, 4], Framed[relation, Background -> LightGray]], Spacer[8]]],
    {b \[SmallCircle] (a \[SmallCircle] a) == a, b \[SmallCircle] (a \[SmallCircle] b) == a, b \[SmallCircle] (b \[SmallCircle] a) == a}
]
```

A relation fixing an operator value:

```wl
Scan[
    relation |-> Print @ AbsoluteTiming @ Row[Riffle[Prepend[plotRelationConstraints[{relation}, #]& /@ Range[2, 3], Framed[relation, Background -> LightGray]], Spacer[8]]],
    {b \[SmallCircle] (a \[SmallCircle] a) == 0}
]
```

Left- and right-projection relations:

```wl
Scan[
    relation |-> Print @ AbsoluteTiming @ Row[Riffle[Prepend[plotRelationConstraints[{relation}, #]& /@ Range[2, 3], Framed[relation, Background -> LightGray]], Spacer[8]]],
    {a \[SmallCircle] b == a, b \[SmallCircle] a == a}
]
```

Models for the **Wolfram axiom** - the single binary law for Boolean algebra from *A New Kind of Science* - at two elements are exactly the Sheffer operations **NAND** (index 8) and **NOR** (index 14):

```wl
TFindFiniteModels[AxiomaticTheory["WolframAxioms"]]
```
<!-- => <|{8} -> <|CenterDot -> {{1, 0}, {0, 0}}|>, {14} -> <|CenterDot -> {{1, 1}, {1, 0}}|>|> -->

There are none at three elements:

```wl
TFindFiniteModels[AxiomaticTheory["WolframAxioms"], 3]
```
<!-- => <||> -->

At four elements there are many (this enumeration is slow):

```wl
#| eval: false
TFindFiniteModels[AxiomaticTheory["WolframAxioms"], 4, "Parallelize" -> True]
```

Rendering the two two-element tables as colored Cayley grids gives the NKS picture - NAND on the left, NOR on the right:

```wl
Row[ArrayPlot[#, ColorRules -> {0 -> StandardBlue, 1 -> StandardOrange}, Mesh -> True, ImageSize -> 70]& /@ Catenate[Values /@ TFindFiniteModels[AxiomaticTheory["WolframAxioms"], 2, "Models"]], Spacer[15]]
```
<!-- => two 2x2 colored grids: NAND (left) and NOR (right) -->

Group axiom models over two and three elements:

```wl
TFindFiniteModels[AxiomaticTheory["GroupAxioms"] /. OverTilde[x_] :> x[], 2]
```
<!-- => <|{0, 6, 1} -> ..., {1, 9, 1} -> ...|> -->

```wl
TFindFiniteModels[AxiomaticTheory["GroupAxioms"] /. OverTilde[x_] :> x[], 3]
```

Ring, boolean, and Hillman axiom models:

```wl
TFindFiniteModels[AxiomaticTheory["RingAxioms"] /. OverTilde[x_] :> x[]]
```

```wl
TFindFiniteModels[AxiomaticTheory["BooleanAxioms"]]
```

```wl
TFindFiniteModels[AxiomaticTheory["HillmanAxioms"]]
```
<!-- => <|{8} -> <|CenterDot -> {{1, 0}, {0, 0}}|>, {14} -> <|CenterDot -> {{1, 1}, {1, 0}}|>|> -->

## Properties & Relations

- The `"CongruenceClosure"` and `"CongruenceClosureC"` methods prune with the same reasoning [TSatEUF]() and [TFindProof]() use: distinct domain elements are asserted apart, an operation cell is a single congruence class, and an inconsistent partial table is rejected before it is completed.  `"CongruenceClosureC"` runs that decision in the C module `src/cc`; the WL [TSatEUF]() stays as its reference oracle.
- A theorem and its countermodels are dual: when [TFindProof]() saturates an equational conjecture without a proof, the refuting algebra it returns is a model [TFindFiniteModels]() finds for the axioms together with the negated goal.

## Possible Issues

- `"CongruenceClosure"` and `"CongruenceClosureC"` enumerate the *k*^*a* cells of each operator with a backtracking DFS, so they are small-domain methods; for large *k* or arity use the default `"ExpressionPrune"` or `"BruteForce"`.
- The function requires the [FindHeadArities]() resource function (fetched on first use) to infer operator arities, exactly as [FindFiniteModels]() does.
