---
Template: Symbol
Name: TCnf
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TCnf
Keywords: [cnf, collapsed normal form, SUP lift, readback, dp]
SeeAlso: [TWnf, TNf, TCollapse, TSup]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TCnf]()[*term*]</code> runs the *collapsed normal form* readback (`src/cnf/_.c`): reduces *term* to WHNF, then lifts the first `SUP` to the top and recursively drives plain DP projections through their `DUP-XXX` interactions.

## Details & Options

- CNF is the layer between weak normal form ([TWnf]()) and full normal form ([TNf]()): it surfaces `SUP` heads so a DP-rooted reduction has a usable WHNF, but does not chase every reachable redex.
- Use this when you need a DP-free reading of a term without paying for `TNf`'s whole-heap sweep.
- [TCollapse]() is built on `TCnf`: each branch of the SUP tree is `cnf`-driven before the walker recurses.
- The eager DUP-cloning CNF performs is what makes a shared variable collapse to the diagonal (`x*x` over `SUP{5,7}` collapses to `{25, 49}`) rather than the cross-product.

## Basic Examples

CNF lifts a `SUP` to the top of the term, leaving DP-projections inside each arm for the caller's next reduction step:

```wl
TReset[];
Head @ TCnf[TLam[x, TOp2["*", x, x]][TSup[5, 7]]]
```
<!-- => TTerm -->

## Properties and Relations

The three-layer reducer story (`wnf` / `cnf` / `nf`) is in `docs/normal_form.md`. For full reduction over every reachable redex use [TNf](); for the user-facing leaf enumeration use [TCollapse]().
