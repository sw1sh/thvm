---
Template: Symbol
Name: TTermSub
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermSub
Keywords: [sub, term, inspector, substitution, fired]
SeeAlso: [TTerm, TTermTag, TTermExt, TTermVal]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermSub]()[*term*]</code> returns the `SUB` flag (`0` or `1`) of *term*.

## Details & Options

- The single `SUB` bit marks a heap cell whose interaction has already fired -- it carries the *substituted* result. A subsequent `VAR` or `DP` read at that loc picks the value up and clears the bit.
- Used to thread the chain of `heap_subst_var` writes the C reducer performs on `APP-LAM`, `DUP-XXX`, and other interactions.
- A freshly constructed term has `SUB = 0`; after a reduction touches its binder loc the cell's read returns `SUB = 1`.

## Basic Examples

A freshly built lambda has `SUB = 0` at its binder cell:

```wl
TReset[];
TTermSub[TLam[x, x]]
```
<!-- => 0 -->

## Properties and Relations

[TTermTag](), [TTermExt](), [TTermVal]() round out the packed-field inspectors.
