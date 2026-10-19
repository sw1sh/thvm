---
Template: Symbol
Name: TFindEquationalPath
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindEquationalPath
Keywords: [equational path, rewriting, rewrite rules, replacement path, proof, FindEquationalProof]
SeeAlso: [TFindProof, TFindStringProof, TStringPath, FindEquationalProof, ProofObject]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Finding Replacement Paths with Directed Rules](FindReplacePath)"]
---

## Usage

<code>[TFindEquationalPath]()[*thm*, *axioms*]</code> proves the equation *thm* from *axioms* and gives the path of terms from its left side to its right side, each obtained from the one before by a single rewrite.

<code>[TFindEquationalPath]()[*proof*]</code> gives the rewrite path of the [ProofObject]() *proof*.

<code>[TFindEquationalPath]()[*thm*, *axioms*, "*prop*"]</code> gives the property *prop* of the rewrite path.

<code>[TFindEquationalPath]()[*proof*, "*prop*"]</code> gives the property *prop* of the rewrite path of *proof*.

## Details & Options

- [TFindEquationalPath]() proves *thm* with [TFindProof]() and reads the path off the proof: the left side is rewritten forward, then the right side is joined in reverse through the term the two sides meet at.
- The *axioms* take the forms [TFindProof]() accepts, including the name of an [AxiomaticTheory]() theory. Each step rewrites with an axiom or with a lemma of the proof.
- An axiom written as a rule `lhs -> rhs` rewrites from left to right only, so with rule axioms and a theorem stated from a start term to an end term, the path is a chain of forward replacements. See [Finding Replacement Paths](paclet:WolframInstitute/THVMLink/tutorial/FindReplacePath).
- The possible values of *prop* are:

| | |
| --- | --- |
| `"Path"` | the list of terms (default) |
| `"Rules"` | the rule used at each step |
| `"Rewrites"` | the [Replace]() or [ReplaceAt]() operator that performs each step |
| `"Substitutions"` | the values of the variables of the rule at each step |
| `"Bindings"` | the match of the left side of the rule at each step |
| `"Justification"` | the lemma, its direction and the position at each step |
| `"RewriteTest"` | a [Success]() or [Failure]() checking that the rewrites reproduce the path |
| `"ProofObject"` | the [ProofObject]() the path is read from |

- A list of properties gives an Association, and [All]() gives every property.
- [TFindEquationalPath]() gives `$Failed` when *thm* is not proved.
- The following options can be given:

| | | |
| --- | --- | --- |
| `"Reverse"` | [False]() | whether to give the path from the right side to the left side |
| `"Simplify"` | [True]() | whether to remove detours from the path |
| `"Canonicalize"` | [False]() | whether to rename the pattern variables of the steps in order |
| `"TerminalLemmas"` | `{}` | lemmas used as single steps instead of being expanded |
| `"Cache"` | [True]() | whether to reuse the expansions of lemmas |
| [MaxSteps]() | [Automatic]() | maximum number of critical pairs [TFindProof]() processes |
| [Method]() | [Automatic]() | proof search method of [TFindProof]() |
| [TimeConstraint]() | [Infinity]() | time limit in seconds for the proof |

## Basic Examples

The path that moves the parentheses of a product to the right with the associativity rule:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}]
```

<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

---

The path of the double-inverse theorem of abelian groups:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]
```

<!-- => {OverBar[OverBar[x]], OverBar[OverBar[x]] ⊗ OverTilde[1], OverTilde[1] ⊗ OverBar[OverBar[x]], (x ⊗ OverBar[x]) ⊗ OverBar[OverBar[x]], x ⊗ (OverBar[x] ⊗ OverBar[OverBar[x]]), x ⊗ OverTilde[1], x} -->

## Scope

### Proof objects

Prove a theorem with [TFindProof]():

```wl
p = TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

The rewrite path of the proof:

```wl
TFindEquationalPath[p]
```

<!-- => {OverBar[OverBar[x]], OverBar[OverBar[x]] ⊗ OverTilde[1], ..., x ⊗ OverTilde[1], x} -->

A property of the rewrite path of the proof:

```wl
TFindEquationalPath[p, "Justification"]
```

<!-- => {{{"Axiom", 3}, Left, {}}, {{"Axiom", 2}, Right, {}}, {{"Axiom", 4}, Left, {1}}, {{"Axiom", 1}, Left, {}}, {{"Axiom", 4}, Right, {2}}, {{"Axiom", 3}, Right, {}}} -->

### Properties

The rule used at each step:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Rules"]
```

<!-- => {a_ -> (a_) ⊗ OverTilde[1], (a_) ⊗ (b_) -> (b_) ⊗ (a_), OverTilde[1] -> (a_) ⊗ OverBar[a_], ((a_) ⊗ (b_)) ⊗ (c_) -> (a_) ⊗ ((b_) ⊗ (c_)), (a_) ⊗ OverBar[a_] -> OverTilde[1], (a_) ⊗ OverTilde[1] -> a_} -->

---

The operator that performs each step:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "Rewrites"]
```

<!-- => {Replace[((a ⊗ b) ⊗ c) ⊗ d :> (a ⊗ b) ⊗ (c ⊗ d)], Replace[(a ⊗ b) ⊗ (c ⊗ d) :> a ⊗ (b ⊗ (c ⊗ d))]} -->

---

The values of the variables of the rule at each step:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "Substitutions"]
```

<!-- => {<|x_ -> a ⊗ b, y_ -> c, z_ -> d|>, <|x_ -> a, y_ -> b, z_ -> c ⊗ d|>} -->

---

The match of the left side of the rule at each step:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "Bindings"]
```

<!-- => {<|x_ -> a ⊗ b, y_ -> c, z_ -> d|>, <|x_ -> a, y_ -> b, z_ -> c ⊗ d|>} -->

---

The lemma, its direction and the position of each step:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "Justification"]
```

<!-- => {{{"Axiom", 1}, Right, {}}, {{"Axiom", 1}, Right, {}}} -->

---

A check that the rewrites reproduce the path:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "RewriteTest"]
```

<!-- => Success["Rewrite", <|"MessageTemplate" -> "Running rewrite sequence reproduced the path."|>] -->

---

The proof object the path is read from:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "ProofObject"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

---

A list of properties gives an Association:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, {"Path", "Justification"}]
```

<!-- => <|"Path" -> {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))}, "Justification" -> {{{"Axiom", 1}, Right, {}}, {{"Axiom", 1}, Right, {}}}|> -->

---

[All]() gives every property:

```wl
TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, All]
```

<!-- => <|"ProofObject" -> ProofObject[...], "RewriteTest" -> Success[...], "Justification" -> {...}, "Rewrites" -> {...}, "Rules" -> {...}, "Substitutions" -> {...}, "Bindings" -> {...}, "Path" -> {...}|> -->

## Options

### Reverse

Give the path from the right side to the left side:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Reverse" -> True]
```

<!-- => {x, x ⊗ OverTilde[1], x ⊗ (OverBar[x] ⊗ OverBar[OverBar[x]]), (x ⊗ OverBar[x]) ⊗ OverBar[OverBar[x]], OverTilde[1] ⊗ OverBar[OverBar[x]], OverBar[OverBar[x]] ⊗ OverTilde[1], OverBar[OverBar[x]]} -->

### Simplify

By default, detours are removed from the path:

```wl
TFindEquationalPath[inv[inv[a]] == a, {f[f[x_, y_], z_] == f[x_, f[y_, z_]], f[e, x_] == x_, f[inv[x_], x_] == e}]
```

<!-- => a path of 15 terms from inv[inv[a]] to a -->

---

With `"Simplify" -> False`, the path keeps the detours of the proof, here through `f[a, e]` and back:

```wl
TFindEquationalPath[inv[inv[a]] == a, {f[f[x_, y_], z_] == f[x_, f[y_, z_]], f[e, x_] == x_, f[inv[x_], x_] == e}, "Simplify" -> False]
```

<!-- => a path of 27 terms from inv[inv[a]] to a -->

### Canonicalize

By default, the variables a lemma introduces carry generated names:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Substitutions"]
```

<!-- => {<|a_ -> OverBar[OverBar[x]]|>, <|a_ -> OverBar[OverBar[x]], b_ -> OverTilde[1]|>, <|a_ -> x|>, <|a_ -> x, b_ -> OverBar[x], c_ -> OverBar[OverBar[x]]|>, <|a_ -> a$7874_|>, <|a_ -> x|>}, with a generated name such as a$7874_ in the fifth step -->

---

With `"Canonicalize" -> True`, they are renamed in order:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Substitutions", "Canonicalize" -> True]
```

<!-- => {<|a_ -> OverBar[OverBar[x]]|>, <|a_ -> OverBar[OverBar[x]], b_ -> OverTilde[1]|>, <|a_ -> x|>, <|a_ -> x, b_ -> OverBar[x], c_ -> OverBar[OverBar[x]]|>, <|a_ -> a_|>, <|a_ -> x|>} -->

### TerminalLemmas

By default, every lemma of the proof is expanded into rewrites with the axioms:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Justification"]
```

<!-- => {{{"Axiom", 3}, Left, {}}, {{"Axiom", 2}, Right, {}}, {{"Axiom", 4}, Left, {1}}, {{"Axiom", 1}, Left, {}}, {{"Axiom", 4}, Right, {2}}, {{"Axiom", 3}, Right, {}}} -->

---

A terminal lemma is used as a single step:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Justification", "TerminalLemmas" -> {{"SubstitutionLemma", 1}}]
```

<!-- => {{{"SubstitutionLemma", 1}, Left, {}}, {{"Axiom", 4}, Right, {2}}, {{"Axiom", 3}, Right, {}}} -->

---

The path is then shorter:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "TerminalLemmas" -> {{"SubstitutionLemma", 1}}]
```

<!-- => {OverBar[OverBar[x]], x ⊗ (OverBar[x] ⊗ OverBar[OverBar[x]]), x ⊗ OverTilde[1], x} -->

### Cache

Expanding the lemmas without reusing earlier expansions gives the same path:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Cache" -> False]
```

<!-- => the same seven terms as with the default -->

### MaxSteps

A small step budget leaves the theorem unproved:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", MaxSteps -> 2]
```

<!-- => $Failed -->

### Method

Prove the theorem with plain completion:

```wl
TFindEquationalPath[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", Method -> "Completion"]
```

<!-- => {OverBar[OverBar[x]], ..., x} -->

### TimeConstraint

A time limit bounds the proof search:

```wl
TFindEquationalPath[x ** y == y ** x, "AbelianGroupAxioms", TimeConstraint -> 2]
```

<!-- => $Failed -->

## Properties and Relations

The rewrite path:

```wl
path = TFindEquationalPath[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}]
```

<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

With rule axioms, each step of the path is a replacement with the rule:

```wl
AllTrue[Partition[path, 2, 1], MemberQ[ReplaceList[#[[1]], (x_ \[CircleTimes] y_) \[CircleTimes] z_ :> x \[CircleTimes] (y \[CircleTimes] z)], #[[2]]] &]
```

<!-- => True -->

---

The `"Path"` spec of [TFindProof]() gives the same path:

```wl
TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "Path"]
```

<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

## Possible Issues

A theorem stated against the direction of a rule still proves, since equality is symmetric, and its path runs the rule backward:

```wl
TFindEquationalPath[a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)) == ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d, {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}]
```

<!-- => {a ⊗ (b ⊗ (c ⊗ d)), (a ⊗ b) ⊗ (c ⊗ d), ((a ⊗ b) ⊗ c) ⊗ d} -->

---

A theorem that does not follow from the axioms gives `$Failed`:

```wl
TFindEquationalPath[x \[CircleTimes] y == y \[CircleTimes] x, {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}]
```

<!-- => $Failed -->
