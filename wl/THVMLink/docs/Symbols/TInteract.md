---
Template: Symbol
Name: TInteract
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TInteract
Keywords: [interaction, redex, fire, step, reduction order, multiway]
SeeAlso: [TRedexes, TStep, TWnf, TItrs, TMultiTrace]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TInteract]()[*r*]</code> fires the one interaction at the redex *r* and gives its result together with the redexes it creates.

<code>[TInteract]()[*r*, *root*]</code> also looks for created redexes among the terms reachable from *root*.

<!-- #| annotation: 26.10.19: TInteract drives the C step session (thvm_wl_step_begin/fire/fresh/end): an attach builds the inverse index and parent map with one heap walk from the redex and the roots, the fire costs O(uses of the redex), the fresh set is drained, then the session detaches. TStepBegin/TStepEnd (no pages yet) keep one session open across many fires. A redex that has already fired is not reliably detected: firing First[TRedexes[seed]] a second time, after the first fire consumed it, gave a garbage TTerm instead of Failure["NotARedex", ...]. TRedexes[TSup[1, 2] + 3] gives {}, so OP2-SUP redexes are not offered for targeted firing. Prior art: the event-by-event exploration of a multiway system, here at the level of single interaction-net rewrites. -->

## Details & Options

- A redex is a term whose next step is a single interaction, such as an application of a lambda or arithmetic on two numbers. <code>[TRedexes]()</code> lists the redexes of a term.
- [TInteract]() fires the redex it is given, wherever it sits in the term, whereas <code>[TStep]()</code> and <code>[TWnf]()</code> reduce from the head of the term.
- The result is an [Association]() with the key `"result"`, the term that replaces the redex, and the key `"fresh"`, the list of terms that became redexes because of this interaction.
- A term that is not a redex gives <code>[Failure]()["NotARedex", …]</code>.
- Every interaction fired adds one to the count of <code>[TItrs]()</code>.
- After an interaction, the redexes of a term are listed again with <code>[TRedexes]()</code> before the next one is fired.

## Basic Examples

The successor function applied to 5:

```wl
seed = TApp[TLam[x, x + 1], TNum[5]]
```
<!-- => a TTerm summary box with tag APP -->

The application is a redex:

```wl
r = First[TRedexes[seed]]
```
<!-- => a TTerm summary box with tag APP -->

Fire it:

```wl
res = TInteract[r]
```
<!-- => <|"result" -> a TTerm summary box with tag OP2, "fresh" -> {a TTerm summary box with tag OP2}|> -->

The beta reduction leaves the addition of 5 and 1:

```wl
Term[res["result"]]
```
<!-- => Term["OP2", "+", Term["NUM", 5], Term["NUM", 1]] -->

The addition is a new redex; firing it gives 6:

```wl
Term[TInteract[First[res["fresh"]]]["result"]]
```
<!-- => Term["NUM", 6] -->

## Scope

A constructor with two applications as its children has two redexes:

```wl
c = TCtr[0, TApp[TLam[x, x + 1], TNum[1]], TApp[TLam[y, y * 2], TNum[5]]]
```
<!-- => a TTerm summary box with tag CTR and ext 0 -->

List them:

```wl
Term /@ TRedexes[c]
```
<!-- => {Term["APP", Term["LAM", ..., Term["OP2", "+", ...]], Term["NUM", 1]], Term["APP", Term["LAM", ..., Term["OP2", "*", ...]], Term["NUM", 5]]} -->

Fire the second one first, passing the constructor as the root:

```wl
Term[TInteract[Last[TRedexes[c]], c]["result"]]
```
<!-- => Term["OP2", "*", Term["NUM", 2], Term["NUM", 5]] -->

Only the second child has changed:

```wl
Term[c]
```
<!-- => Term["CTR", 0, Term["APP", Term["LAM", ..., Term["OP2", "+", Term["VAR", ...], Term["NUM", 1]]], Term["NUM", 1]], Term["OP2", "*", Term["NUM", 2], Term["NUM", 5]]] -->

---

A number is not a redex:

```wl
TInteract[TNum[5]]
```
<!-- => Failure["NotARedex", <|"Message" -> "TInteract: cell is not a redex right now", "redex" -> TNum summary box 5|>] -->

---

The identity function applied to a number leaves no new redex:

```wl
TInteract[First[TRedexes[TApp[TLam[x, x], TNum[5]]]]]["fresh"]
```
<!-- => {} -->

## Properties and Relations

Start from a reset runtime, so the interaction counter is at 0:

```wl
TReset[];
```

Fire one interaction:

```wl
Term[TInteract[First[TRedexes[TApp[TLam[x, x + 1], TNum[5]]]]]["result"]]
```
<!-- => Term["OP2", "+", Term["NUM", 5], Term["NUM", 1]] -->

It counts as one interaction:

```wl
TItrs[]
```
<!-- => 1 -->

---

At the head of a term, firing its redex agrees with one step of <code>[TStep]()</code>:

```wl
Term[TStep[TApp[TLam[x, x + 1], TNum[5]]]]
```
<!-- => Term["OP2", "+", Term["NUM", 5], Term["NUM", 1]] -->
