---
Template: Symbol
Name: TVarFor
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TVarFor
Keywords: [var, binder, lambda, heap]
SeeAlso: [TLam, TApp, THeapAlloc, TVarShape]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TVarFor]()[*lamLoc*]</code> constructs a `TAG_VAR` term pointing at the binder heap loc *lamLoc*.

## Details & Options

- The runtime [TLam]() macro builds this for you: it allocates the binder loc, threads <code>[TVarFor]()[*loc*]</code> into the held body, then seals the lambda. So you rarely call `TVarFor` directly -- only when constructing a heap-side term by hand (e.g. tests that drop into the C-level layout).
- Reading a `VAR` follows the substitution chain: if the binder's cell has been `SUB`-flagged by an `APP-LAM`, the read returns the substituted argument; otherwise the `VAR` is its own WNF.

## Basic Examples

A bare `VAR` pointing at a freshly allocated binder loc:

```wl
TReset[];
loc = THeapAlloc[1];
Term[TVarFor[loc]]
```
<!-- => Term["VAR", 0] -->

## Properties and Relations

The user-facing way to introduce a binder is [TLam](); it handles the [THeapAlloc]() + `TVarFor` + body installation in one call. `TVarFor` is the low-level escape hatch used by the runtime and by test fixtures that hand-author a heap layout.
