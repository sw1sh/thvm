---
Template: Symbol
Name: TFindFiniteModels
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindFiniteModels
Keywords: [finite model, model finding, multiplication table, Cayley table, axiom, operator, congruence closure, Wolfram axiom, A New Kind of Science]
SeeAlso: [TSatEUF, TSmtDecide, TFindProof, CounterexampleObject, AxiomaticTheory, BooleanFunction, SatisfiabilityInstances]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Disproving Conjectures with Countermodels](Disproof)"]
---

## Usage

<code>[TFindFiniteModels]()[*rels*]</code> finds the models of the relations *rels* over a domain of two elements, as tables of values for the operators in *rels*.

<code>[TFindFiniteModels]()[*rels*, *k*]</code> finds the models over a domain of *k* elements.

<code>[TFindFiniteModels]()[*rels*, *k*, "*prop*"]</code> gives the property *prop* of the models.

## Details & Options

- [TFindFiniteModels]() is the Wolfram Function Repository function `FindFiniteModels`, with additional search methods.
- The relations *rels* are equations ([Equal]()), inequations ([Unequal]()), rules, logical combinations of them, and quantified relations with [ForAll]() and [Exists](). The axioms of an [AxiomaticTheory]() theory can be given directly.
- A string relation such as `"AB" -> "BA"` is a relation between words of a monoid, whose product and identity element are added as operators.
- Each operator of arity *a* has a table of $k^a$ values, so there are $k^{k^a}$ possible tables for it. The variables of the relations range over the *k* elements `0`, ..., *k* - 1.
- The possible values of *prop* are:

| | |
| --- | --- |
| `"Association"` | an Association from model index to the tables of the model (default) |
| `"Indices"` | the list of model indices |
| `"Models"` | the list of models |

- The index of a model gives the position of the table of each operator in the enumeration of all tables. With `"ReverseOrdering" -> True`, the indices of binary operators on two elements agree with [BooleanFunction]().
- The following options can be given:

| | | |
| --- | --- | --- |
| [Method]() | [Automatic]() | search method |
| [MaxItems]() | [Infinity]() | maximum number of models to find |
| `"Parallelize"` | [False]() | whether to search in parallel |
| `"ReverseOrdering"` | [False]() | whether to reverse the order of the table entries in the index |

- The possible settings for [Method]() are:

| | |
| --- | --- |
| [Automatic]() | `"ExpressionPrune"` |
| `"ExpressionPrune"` | a search that prunes partial tables against the relations |
| `"ExpressionPruneC"` | the same search with its inner loop in C |
| `"BruteForce"` | a search over all tables |
| `"SAT"` | a search with [SatisfiabilityInstances]() |
| `"CongruenceClosure"` | a backtracking search that rejects a partial table by congruence closure, as [TSatEUF]() does |
| `"CongruenceClosureC"` | the same backtracking search with congruence closure in C |
| `"Z3"` | a search with the Z3 SMT (satisfiability modulo theories) solver |

- Any other setting of [Method]() issues the message `TFindFiniteModels::method` and gives `$Failed`.

## Basic Examples

The models of a nullary operator over two elements:

```wl
TFindFiniteModels[a[] -> a[]]
```

<!-- => <|{0} -> <|a -> 0|>, {1} -> <|a -> 1|>|> -->

---

The models of two equal constants over three elements:

```wl
TFindFiniteModels[a[] == b[], 3]
```

<!-- => <|{0, 0} -> <|a -> 0, b -> 0|>, {1, 1} -> <|a -> 1, b -> 1|>, {2, 2} -> <|a -> 2, b -> 2|>|> -->

---

The only unary operator on two elements that fixes every element is the identity:

```wl
TFindFiniteModels[f[a] == a]
```

<!-- => <|{1} -> <|f -> {0, 1}|>|> -->

---

The commutative binary operators on two elements:

```wl
TFindFiniteModels[f[a, b] == f[b, a]]
```

<!-- => <|{0} -> <|f -> {{0, 0}, {0, 0}}|>, {1} -> <|f -> {{0, 0}, {0, 1}}|>, {6} -> <|f -> {{0, 1}, {1, 0}}|>, {7} -> <|f -> {{0, 1}, {1, 1}}|>, {8} -> <|f -> {{1, 0}, {0, 0}}|>, {9} -> <|f -> {{1, 0}, {0, 1}}|>, {14} -> <|f -> {{1, 1}, {1, 0}}|>, {15} -> <|f -> {{1, 1}, {1, 1}}|>|> -->

## Scope

### Relations

No unary operator sends every element to a fixed other variable:

```wl
TFindFiniteModels[f[a] == b]
```

<!-- => <||> -->

---

Two unary operators forced to agree:

```wl
TFindFiniteModels[f[a] == g[a]]
```

<!-- => <|{0, 0} -> <|f -> {0, 0}, g -> {0, 0}|>, {1, 1} -> <|f -> {0, 1}, g -> {0, 1}|>, {2, 2} -> <|f -> {1, 0}, g -> {1, 0}|>, {3, 3} -> <|f -> {1, 1}, g -> {1, 1}|>|> -->

---

The left projection is the only binary operator with `f[a, b] == a`:

```wl
TFindFiniteModels[f[a, b] == a]
```

<!-- => <|{3} -> <|f -> {{0, 0}, {1, 1}}|>|> -->

---

Operators of different arities:

```wl
TFindFiniteModels[f[a, b] == g[b]]
```

<!-- => <|{0, 0} -> <|f -> {{0, 0}, {0, 0}}, g -> {0, 0}|>, {5, 1} -> <|f -> {{0, 1}, {0, 1}}, g -> {0, 1}|>, {10, 2} -> <|f -> {{1, 0}, {1, 0}}, g -> {1, 0}|>, {15, 3} -> <|f -> {{1, 1}, {1, 1}}, g -> {1, 1}|>|> -->

---

A ternary operator:

```wl
TFindFiniteModels[f[a, b, c] == f[a, b, b]]
```

<!-- => <|{0} -> <|f -> {{{0, 0}, {0, 0}}, {{0, 0}, {0, 0}}}|>, {3} -> <|f -> {{{0, 0}, {0, 0}}, {{0, 0}, {1, 1}}}|>, ...|> -->

---

Constants in a relation are elements of the domain:

```wl
TFindFiniteModels[f[a, b] == g[0] == 1]
```

<!-- => <|{15, 2} -> <|f -> {{1, 1}, {1, 1}}, g -> {1, 0}|>, {15, 3} -> <|f -> {{1, 1}, {1, 1}}, g -> {1, 1}|>|> -->

---

With no operator, every symbol is a nullary operator:

```wl
TFindFiniteModels[a -> b]
```

<!-- => <|{0, 0} -> <|a -> 0, b -> 0|>, {1, 1} -> <|a -> 1, b -> 1|>|> -->

### Inequations and logical combinations

An inequation:

```wl
TFindFiniteModels[a != b]
```

<!-- => <|{0, 1} -> <|a -> 0, b -> 1|>, {1, 0} -> <|a -> 1, b -> 0|>|> -->

---

An inequation between an operator and a constant:

```wl
TFindFiniteModels[f[a, b] != 0]
```

<!-- => <|{15} -> <|f -> {{1, 1}, {1, 1}}|>|> -->

---

Alternatives:

```wl
TFindFiniteModels[f[a, b] != a | f[a, b] == a]
```

<!-- => <|{3} -> <|f -> {{0, 0}, {1, 1}}|>, {12} -> <|f -> {{1, 1}, {0, 0}}|>|> -->

---

A disjunction:

```wl
TFindFiniteModels[f[a, b] == a || f[a, b] == b]
```

<!-- => <|{1} -> <|f -> {{0, 0}, {0, 1}}|>, {3} -> <|f -> {{0, 0}, {1, 1}}|>, {5} -> <|f -> {{0, 1}, {0, 1}}|>, {7} -> <|f -> {{0, 1}, {1, 1}}|>|> -->

### Quantifiers

An existential quantifier:

```wl
TFindFiniteModels[Exists[{x}, x == f[x, y]]]
```

<!-- => <|{0, 0} -> <|f -> {{0, 0}, {0, 0}}, x -> 0|>, {1, 0} -> <|f -> {{0, 0}, {0, 1}}, x -> 0|>, ...|> -->

---

A universal quantifier:

```wl
TFindFiniteModels[ForAll[{f, x}, x == f[x, y]]]
```

<!-- => <|{3} -> <|f -> {{0, 0}, {1, 1}}|>|> -->

### Strings and theories

A string relation finds the monoids in which it holds:

```wl
TFindFiniteModels["ABBA" -> "BAAB"]
```

<!-- => <|{0, 6} -> <|"1" -> 0, SmallCircle -> {{0, 1}, {1, 0}}|>, {0, 7} -> <|"1" -> 0, SmallCircle -> {{0, 1}, {1, 1}}|>, {1, 1} -> <|"1" -> 1, SmallCircle -> {{0, 0}, {0, 1}}|>, {1, 9} -> <|"1" -> 1, SmallCircle -> {{1, 0}, {0, 1}}|>|> -->

---

The axioms of a theory:

```wl
TFindFiniteModels[AxiomaticTheory["BooleanAxioms"]]
```

<!-- => <|{1, 7, 2} -> <|CirclePlus -> {{0, 0}, {0, 1}}, CircleTimes -> {{0, 1}, {1, 1}}, OverBar -> {1, 0}|>, {7, 1, 2} -> <|CirclePlus -> {{0, 1}, {1, 1}}, CircleTimes -> {{0, 0}, {0, 1}}, OverBar -> {1, 0}|>|> -->

### Properties

The model indices:

```wl
TFindFiniteModels[f[a, b] == a || f[a, b] == b, 2, "Indices"]
```

<!-- => {{1}, {3}, {5}, {7}} -->

---

The models:

```wl
TFindFiniteModels[f[a, b] == a || f[a, b] == b, 2, "Models"]
```

<!-- => {<|f -> {{0, 0}, {0, 1}}|>, <|f -> {{0, 0}, {1, 1}}|>, <|f -> {{0, 1}, {0, 1}}|>, <|f -> {{0, 1}, {1, 1}}|>} -->

## Options

### Method

Search with [SatisfiabilityInstances]():

```wl
TFindFiniteModels[f[b, f[a, b]] == a, 3, Method -> "SAT"]
```

<!-- => <|{5681} -> <|f -> {{0, 2, 1}, {2, 1, 0}, {1, 0, 2}}|>, {8229} -> <|f -> {{1, 0, 2}, {0, 2, 1}, {2, 1, 0}}|>, {15613} -> <|f -> {{2, 1, 0}, {1, 0, 2}, {0, 2, 1}}|>|> -->

---

Search with congruence closure in C:

```wl
TFindFiniteModels[f[b, f[a, b]] == a, 3, Method -> "CongruenceClosureC"]
```

<!-- => the same three models -->

---

Search over all tables:

```wl
TFindFiniteModels[f[0, a] == 0, 4, MaxItems -> 2, Method -> "BruteForce"]
```

<!-- => <|{0} -> <|f -> {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}|>, {1} -> <|f -> {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 1}}|>|> -->

---

Every method finds the same models of the Wolfram axiom:

```wl
SameQ @@ (TFindFiniteModels[AxiomaticTheory["WolframAxioms"], Method -> #] & /@ {"ExpressionPrune", "ExpressionPruneC", "BruteForce", "SAT", "CongruenceClosure", "CongruenceClosureC", "Z3"})
```

<!-- => True -->

### MaxItems

Find only the first two models:

```wl
TFindFiniteModels[f[b, f[a, b]] == a, 4, MaxItems -> 2]
```

<!-- => <|{458142180} -> <|f -> {{0, 1, 2, 3}, {1, 0, 3, 2}, {2, 3, 0, 1}, {3, 2, 1, 0}}|>, {1260302820} -> <|f -> {{1, 0, 2, 3}, {0, 1, 3, 2}, {2, 3, 0, 1}, {3, 2, 1, 0}}|>|> -->

### Parallelize

Search in parallel; the number of models is the same as with a sequential search:

```wl
Length @ TFindFiniteModels[b \[SmallCircle] (a \[SmallCircle] a) == a, 8, "Parallelize" -> True]
```

<!-- => 764; evaluated inside a parallel subkernel, as the example runner does, the message ParallelMap::subpar is issued and the search runs sequentially -->

### ReverseOrdering

By default, the negation of the Boolean axioms has index 2, and its table `{1, 0}` sends `0` to `1`:

```wl
TFindFiniteModels[AxiomaticTheory["BooleanAxioms"]]
```

<!-- => <|{1, 7, 2} -> <|CirclePlus -> {{0, 0}, {0, 1}}, CircleTimes -> {{0, 1}, {1, 1}}, OverBar -> {1, 0}|>, {7, 1, 2} -> <|...|>|> -->

---

With `"ReverseOrdering" -> True`, the indices of the binary operators are those of [BooleanFunction]():

```wl
TFindFiniteModels[AxiomaticTheory["BooleanAxioms"], "ReverseOrdering" -> True]
```

<!-- => <|{8, 14, 1} -> <|CirclePlus -> {{1, 0}, {0, 0}}, CircleTimes -> {{1, 1}, {1, 0}}, OverBar -> {0, 1}|>, {14, 8, 1} -> <|CirclePlus -> {{1, 1}, {1, 0}}, CircleTimes -> {{1, 0}, {0, 0}}, OverBar -> {0, 1}|>|> -->

---

The table of [BooleanFunction]() 8 is that of [And]() on the truth values in reverse order:

```wl
Boole @ BooleanTable[BooleanFunction[8, 2], {1}, {2}]
```

<!-- => {{1, 0}, {0, 0}} -->

## Properties and Relations

A string relation is the same as the relations of a monoid with that relation between words:

```wl
TFindFiniteModels["A" -> "AA", 3] === TFindFiniteModels[{"A" == "A" \[SmallCircle] "A", ("A" \[SmallCircle] "B") \[SmallCircle] "C" == "A" \[SmallCircle] ("B" \[SmallCircle] "C"), "1"[] \[SmallCircle] "A" == "A", "A" \[SmallCircle] "1"[] == "A"}, 3]
```

<!-- => True -->

---

A model of the axioms in which the conjecture fails refutes the conjecture, as the counterexamples of [TFindProof]() do:

```wl
TFindProof[f[a] == c, {a == b, b == c}, "Counterexample"]["Model"]
```

<!-- => <|f -> {1, 0}, a -> 0, b -> 0, c -> 0|> -->

## Possible Issues

[Plus]() is [Orderless](), so `a + b == b + a` evaluates to [True]() first, and [True]() is taken as a nullary operator:

```wl
TFindFiniteModels[a + b == b + a]
```

<!-- => <|{0} -> <|True -> 0|>, {1} -> <|True -> 1|>|> -->

---

An unknown method issues a message and gives `$Failed`:

```wl
TFindFiniteModels[f[x] == x, Method -> "Foo"]
```

<!-- => the message TFindFiniteModels::method is issued and the result is $Failed -->

## Neat Examples

The Wolfram axiom, the single axiom for Boolean algebra in A New Kind of Science, has two models on two elements, the Sheffer operations NAND and NOR:

```wl
TFindFiniteModels[AxiomaticTheory["WolframAxioms"]]
```

<!-- => <|{8} -> <|CenterDot -> {{1, 0}, {0, 0}}|>, {14} -> <|CenterDot -> {{1, 1}, {1, 0}}|>|> -->

---

It has no models on three elements:

```wl
TFindFiniteModels[AxiomaticTheory["WolframAxioms"], 3]
```

<!-- => <||> -->

---

The tables of NAND and NOR as colored grids:

```wl
Row[ArrayPlot[#, ColorRules -> {0 -> StandardBlue, 1 -> StandardOrange}, Mesh -> True, ImageSize -> 70] & /@ Catenate[Values /@ TFindFiniteModels[AxiomaticTheory["WolframAxioms"], 2, "Models"]], Spacer[15]]
```

<!-- => two 2x2 colored grids: NAND (left) and NOR (right) -->

---

The groups on three elements, with the identity element written as a nullary operator, are the cyclic groups with each choice of identity:

```wl
TFindFiniteModels[AxiomaticTheory["GroupAxioms"] /. OverTilde[x_] :> x[], 3]
```

<!-- => <|{0, 4069, 7} -> <|1 -> 0, CircleTimes -> {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}}, OverBar -> {0, 2, 1}|>, ...|> (3 models) -->

---

The tables of the commutative semigroups on two and three elements:

```wl
Row[Riffle[Table[Row[ArrayPlot[#, Mesh -> True, ImageSize -> 32] & /@ Take[Catenate[Values /@ TFindFiniteModels[{a \[CenterDot] b == b \[CenterDot] a, (a \[CenterDot] b) \[CenterDot] c == a \[CenterDot] (b \[CenterDot] c)}, k, "Models"]], UpTo[6]]], {k, 2, 3}], Spacer[16]]]
```

<!-- => a row of the first six semigroup tables on two elements, then on three -->
