---
Template: Symbol
Name: TNf
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TNf
Keywords: [nf, normal form, redex, full reduction]
SeeAlso: [TWnf, TCnf, TCollapse, TRedexes]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TNf]()[*term*]</code> reduces *term* to **full normal form**: a `nf` sweep of the live heap firing every redex via `redex_fire`, followed by a `cnf` run at the surviving root so the user-visible term is DP-free.

## Details & Options

- Where [TWnf]() surfaces only the head, `TNf` reaches grads, kernels, OP2s, ... nested anywhere in the graph.
- `TAG_REF` and `TAG_ALO` are excluded from eager firing so a recursive named definition does not non-terminatingly unfold.
- See `docs/normal_form.md` for the three-layer reducer story (`wnf` / `cnf` / `nf`).

## Basic Examples

A nested OP2 collapses to the integer result:

```wl
TReset[];
TTermVal @ TNf @ TOp2["+", TOp2["*", TNum[3], TNum[4]], TNum[5]]
```
<!-- => 17 -->

## Properties and Relations

For weak (head-only) reduction use [TWnf](); for the SUP-lift readback layer use [TCnf](). [TCollapse]() observes the SUP-tree leaves on top of `TCnf`.
