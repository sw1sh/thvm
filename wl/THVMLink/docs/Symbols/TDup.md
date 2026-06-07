---
Template: Symbol
Name: TDup
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TDup
Keywords: [duplicator, dup, projections, sharing, multicomputation]
SeeAlso: [TSup, TCollapse, TMultiTrace, TFreshLabel, TDdu]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TDup]()[*body*]</code> constructs a `DUP` over *body* with a fresh integer label and returns the pair `{dp0, dp1}` of its two projections.

<code>[TDup]()[*label*, *body*]</code> uses an explicit *label*.

<code>[TDup]()[*body*, *k*]</code> / <code>[TDup]()[*label*, *body*, *k*]</code> are CPS variants: instead of returning the pair, they call *k*[*dp0*, *dp1*].

## Details & Options

- A `DUP` shares *body* so its two projections fan out without re-computing -- the runtime fires the matching `DUP-XXX` rule when each projection is forced, atomically copying scalars and commuting through compounds.
- Labels matter: `DUP` of a `SUP` *with the same label* annihilates (pairs the SUP's arms diagonally to the two projections); a *different label* commutes (cross-product). See [TSup]() and `docs/multicomputation.md`.
- The CPS form is handy when the projections feed an immediate body without leaking through `Hold`-free WL bindings.

## Basic Examples

Two projections of the same DUP. Each projection of <code>[TOp2]()["+", [TSup]()[1, 2], 3]</code> evaluates the same arithmetic on the duplicated branches; collapsing one of them yields the per-branch sums:

```wl
TReset[];
{dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
TTermVal /@ TCollapse[dp0]
```
<!-- => {4, 5} -->

## Scope

Same-label `DUP-SUP` annihilates and picks one arm per projection:

```wl
TReset[];
{a, b} = TDup[7, TSup[7, 1, 2]];
TTermVal /@ TCollapse[a]
```
<!-- => {1} -->

```wl
TReset[];
{a, b} = TDup[7, TSup[7, 1, 2]];
TTermVal /@ TCollapse[b]
```
<!-- => {2} -->

## Properties and Relations

A `TLam` whose binder is used more than once auto-inserts a `DUP` chain on the substituted argument (see `src/lam/auto_dup.c`); the auto-DUP uses a fresh label drawn from `0x10000..0x1FFFF` so it cannot collide with user [TFreshLabel]() labels. For a label resolved at reduction time use [TDdu]().
