---
Template: TechNote
Name: FindReplacePath
Title: Finding Replacement Paths with Directed Rules
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/FindReplacePath
Keywords: [rewriting, directed rules, oriented rules, semi-Thue, string rewriting, replacement path, equational path, FindEquationalProof, FindReplacePath, multiway]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, Disproof]
---

## Equations, directed rules, and replacement paths

[FindEquationalProof]() answers a yes/no question: is some `lhs == rhs` *derivable* from a set of equations? Its axioms are two-way - the prover may rewrite an equation in whichever direction the term ordering picks, and the witnessing proof is a *join*: the two sides are each rewritten down to a shared normal form that they meet in the middle.

A great many questions are not symmetric, though. "Bubble-sort this word", "fully right-associate this product", "expand this expression with the distributive law" all name a *direction*. The natural object is not a two-way proof but a **replacement path**: a chain

```
start  ->  e1  ->  e2  ->  ...  ->  target
```

in which every arrow is one application of a rule, *fired left-to-right only*. That is exactly what a hypothetical `FindReplacePath` would compute - the [ReplaceAll]() analogue of [FindEquationalProof](), tracing the literal sequence of [Replace]()ments that carries *start* to *target*.

THVMLink builds that object out of two pieces already in the [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP) surface:

- **One-sided rules.** An axiom written with [Rule]() (`a -> b`) instead of [Equal]() (`a == b`) is installed *pre-oriented*: the engine only ever rewrites with it left-to-right. The rule set stays a directed term-rewriting system instead of being symmetrized.
- **The witnessing path.** [TFindEquationalPath]() (and the `"Path"` return spec of [TFindProof]()) reads the rewrite chain back off a proved goal. With one-sided axioms and the goal stated *source* `->` *target*, that chain is a pure forward replacement sequence - the [Replace]()-path you were after.

The Wolfram Function Repository function <code>[ResourceFunction]()["FindEquationalPath"]</code> extracts the analogous path from a two-way proof; [TFindEquationalPath]() is its native port, and the one-sided-rule extension is what turns it into a directed `FindReplacePath`. This note walks the whole surface end to end - first on ordinary terms, then on the dedicated string-rewriting wrapper.

## Setting up

The directed-rewriting surface lives in the ATP context, which is its own load:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
```

## A one-sided rule on terms

Take associativity, but as a *direction* rather than an identity: rewrite a left-nested product into a right-nested one, never the other way. Written with [Rule](), the axiom is pre-oriented; the goal asks to fully right-associate a four-factor product. [CircleTimes]() (`\[CircleTimes]`, entered as `[Esc]c*[Esc]`) is a non-[Orderless]() operator, so the two sides stay genuinely distinct instead of being sorted equal at parse time:

```wl
TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20]
```
<!-- => ProofObject[<|"Theorem" -> ..., "Status" -> "Proved", ...|>] -->

The return is an ordinary `ProofObject` - the same head [FindEquationalProof]() produces - so `p["ProofFunction"][]` re-checks it independently. Pre-oriented rules carry a proper trace lineage through the engine, so the lift survives and the proof verifies:

```wl
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20];
p["ProofFunction"][]
```
<!-- => Success["EquationalProof", <|...|>] -->

## Reading off the replacement path

The proof is one thing; the *path* it traces is another. The `"Path"` return spec - or the dedicated [TFindEquationalPath]() surface - returns the chain of terms from the goal's left side to its right side, each consecutive pair differing by exactly one rewrite:

```wl
TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20]
```
<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

Two rewrites: the outermost bracket re-associates, then the inner one. Because the axiom is one-sided and the target is the fully-reduced (right-nested) form, this is a *pure forward chain* - there is no backward leg. We can check that against Wolfram's own [ReplaceList](): every step of the path is a legal left-to-right application of the very same rule, so the path is literally a [Replace]()-path:

```wl
path = TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20];
And @@ (MemberQ[ReplaceList[#[[1]], (x_ \[CircleTimes] y_) \[CircleTimes] z_ :> x \[CircleTimes] (y \[CircleTimes] z)], #[[2]]] & /@ Partition[path, 2, 1])
```
<!-- => True -->

That equivalence is the whole point: directed axioms plus [TFindEquationalPath]() reproduce, inside the prover, the replacement sequence you would get by hand-applying `/.` until the target appears.

## The path lives in the proof object

`"Path"` works prove-time off the live goal chain, but the same path can be recovered from a `ProofObject` you already have in hand. [TFindEquationalPath]() takes the object directly and re-walks its proof dataset's per-goal chain:

```wl
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20];
TFindEquationalPath[p]
```
<!-- => {((a ⊗ b) ⊗ c) ⊗ d, (a ⊗ b) ⊗ (c ⊗ d), a ⊗ (b ⊗ (c ⊗ d))} -->

The proof dataset that the walk reads is the same one `p["ProofGraph"]` renders - the rewrite DAG with one node per derived equation:

```wl
p = TFindProof[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    TimeConstraint -> 20];
p["ProofGraph"]
```
<!-- => Graph[...] - the equational rewrite DAG -->

## Every step, with its provenance

The bare list of terms is the default, but the path carries more: pass a *property* (or `All`) to get the rule, concrete rewrite, variable binding, and justification behind each step. This is the full surface of the Wolfram Function Repository <code>[ResourceFunction]()["FindEquationalPath"]</code>, which the paclet re-homes and runs on [TFindProof]()'s `ProofObject`. `All` returns the whole Association:

```wl
Keys @ TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    All, TimeConstraint -> 20]
```
<!-- => {ProofObject, RewriteTest, Justification, Rewrites, Rules, Substitutions, Bindings, Path} -->

`"Rewrites"` is the most directly *FindReplacePath*-flavoured: the concrete [Replace]() / [ReplaceAt]() operation at each step, ready to apply:

```wl
TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "Rewrites", TimeConstraint -> 20]
```
<!-- => {Replace[((a ⊗ b) ⊗ c) ⊗ d :> (a ⊗ b) ⊗ (c ⊗ d)], Replace[(a ⊗ b) ⊗ (c ⊗ d) :> a ⊗ (b ⊗ (c ⊗ d))]} -->

`"Substitutions"` gives the variable binding the rule matched with at each step, and `"Justification"` names the lemma, orientation, and position. A list of property names returns just those:

```wl
TFindEquationalPath[
    ForAll[{a, b, c, d}, ((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d))],
    {ForAll[{x, y, z}, (x \[CircleTimes] y) \[CircleTimes] z -> x \[CircleTimes] (y \[CircleTimes] z)]},
    "Justification", TimeConstraint -> 20]
```
<!-- => {{{Axiom, 1}, Right, {}}, {{Axiom, 1}, Right, {}}} -->

## String rewriting: the ergonomic wrapper

The classic home of one-sided rules is *string rewriting* - semi-Thue systems, where a rule replaces one substring by another wherever it occurs. [TFindStringProof](), [TStringPath](), and [TFindEquationalPath]() wrap the term machinery so you never spell out the encoding: a word is a string, an axiom is a pair of words, and the associativity bookkeeping that lets a rule fire at *any* position is appended for you. (The encoding follows the Wolfram Function Repository's <code>[ResourceFunction]()["FindStringProof"]</code>.)

A rule written with [Rule]() is one-directional. The single rule `"BA" -> "AB"` is a bubble-sort: it moves every `A` left past a `B`, and [TStringPath]() returns the whole sorting trajectory from the scrambled word to the sorted one:

```wl
TStringPath["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, TimeConstraint -> 30]
```
<!-- => {BBBAAA, BBABAA, BBAAAB, BABAAB, BAABAB, BAAABB, ABAABB, AABABB, AAABBB} -->

A sequence of replacements, each swapping a `"BA"` to `"AB"` (some steps fire a *derived* completion rule, a composition of the axiom - still strictly left-to-right; the `BBAAAB` step jumps two positions at once). The endpoints are the theorem's two words; the intermediate route is the one the completion chose. [TFindStringProof]() returns the underlying `ProofObject` and takes the same return specs as [TFindProof](), so `"Status"` reports the bare decision:

```wl
TFindStringProof["BBBAAA" -> "AAABBB", {"BA" -> "AB"}, "Status", TimeConstraint -> 30]
```
<!-- => Proved -->

## One conjunction, many goals

A list of theorems is a single multi-goal conjunction - proved off *one* saturation and returned as one `ProofObject` carrying a hypothesis/conclusion pair per conjunct (the [TFindProof]() multi-goal contract). `"Status"` reports the single decision for the whole conjunction:

```wl
TFindStringProof[{"BA" -> "AB", "BBA" -> "ABB"}, {"BA" -> "AB"}, "Status", TimeConstraint -> 30]
```
<!-- => Proved -->

The conjunction's proof dataset carries the per-conjunct rows, so the multi-goal object holds every goal's chain at once:

```wl
po = TFindStringProof[{"BA" -> "AB", "BBA" -> "ABB"}, {"BA" -> "AB"}, TimeConstraint -> 30];
ContainsAll[Keys @ Normal @ po["ProofDataset"], {{"Hypothesis", 1}, {"Hypothesis", 2}}]
```
<!-- => True - one hypothesis row per conjunct -->

[TStringPath]() hands back one decoded path per goal for a list of theorems (each subject to the same one-route-per-goal caveat as the single-goal case).

## Direction is a property of the axiom, not the goal

It is worth being precise about *what* the orientation controls. Equational provability is symmetric: a goal `s == t` is provable exactly when `s` and `t` are *joinable* (both reach a common form), and that does not depend on which way you write the goal. So even with the one-sided `"BA" -> "AB"`, the *reversed* goal still proves - but its path runs the chain backwards, un-sorting the already-sorted word:

```wl
TStringPath["AAABBB" -> "BBBAAA", {"BA" -> "AB"}, TimeConstraint -> 20]
```
<!-- => {AAABBB, AABABB, ABAABB, BAAABB, BAABAB, BABAAB, BBAAAB, BBABAA, BBBAAA} - still proves; the route un-sorts -->

The orientation is real all the same: it constrains *the rewrite relation*, not the joinability question. The one-sided rule fires only `"BA"` -> `"AB"` during completion, so the system terminates (every word has a unique sorted normal form) and a *forward*-stated goal yields a forward-only path. Make the axiom an equation instead - a two-string list, `==`, or [TwoWayRule]() (`a <-> b`) - and the engine is free to orient it either way and to superpose both faces:

```wl
TStringPath["AAABBB" -> "BBBAAA", {"BA" <-> "AB"}, TimeConstraint -> 30]
```
<!-- => {AAABBB, AABABB, ABAABB, BAAABB, BAABAB, BABAAB, BBAAAB, BBAABA, BBABAA, BBBAAA} - proves either way -->

The practical rule of thumb: write the axiom as `->` when you want a directed `FindReplacePath`, and state the goal *source* `->` *target* with *source* the word you start from.

## Relationship to FindEquationalProof

- [TFindProof]() is the general engine. Equational (`==`) axioms reproduce [FindEquationalProof](); switching one or more axioms to [Rule]() pre-orients them, and the `"Path"` return spec exposes the witnessing rewrite chain. See its [reference page](paclet:WolframInstitute/THVMLink/ref/TFindProof) for the full method and return-spec surface.
- [TFindEquationalPath]() is the dedicated path surface - prove-time (`thm$, axioms$`) or off a precomputed `proof$`. It is the native analogue of the Wolfram Function Repository <code>[ResourceFunction]()["FindEquationalPath"]</code>; with one-sided axioms it behaves as a directed `FindReplacePath`.
- [TFindStringProof]() / [TStringPath]() specialize the whole thing to semi-Thue string rewriting, encoding words and the position-bridge axiom automatically.

## Where to go next

- Per-symbol pages: [TFindEquationalPath](), [TFindStringProof](), [TStringPath](), [TFindProof]().
- The general theorem-proving surface - methods, presets, portfolios, the SMT path - is the [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP) tech note; refutation and counterexamples are in [Disproof](paclet:WolframInstitute/THVMLink/tutorial/Disproof).
- The encoder, dispatcher, and `"Path"` assembly live in [`ATP.wl`](../../Kernel/ATP/ATP.wl); the string wrapper and the dataset path re-walk in [`ATP_Strings.wl`](../../Kernel/ATP/ATP_Strings.wl).
