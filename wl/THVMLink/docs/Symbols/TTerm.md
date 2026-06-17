---
Template: Symbol
Name: TTerm
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTerm
Keywords: [TTerm, packed term, handle, wrapper, indexing]
SeeAlso: [TTermTag, TTermExt, TTermVal, TTermSub, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTerm]()[*id*]</code> wraps a packed 64-bit `Term` value into a managed handle.

You typically construct a `TTerm` via [TLam](), [TApp](), [TSup](), [TDup](), [TEra](), [TVarFor]() etc. Direct construction <code>[TTerm]()[*raw*]</code> is the low-level escape hatch.

Indexing on a handle exposes its packed fields without unboxing: <code>*t*["tag" | "ext" | "val" | "sub" | "tagName" | "raw"]</code>.

## Details & Options

- The wrapper carries an `ExternPin` managed handle so the runtime knows the value is still referenced from the WL side; once the `TTerm` is GC'd by Wolfram, the pin drops automatically (see [TExternPinCount]() to observe).
- Each `Term` packs `{sub:1, tag:5, ext:18, val:38}` bits into 64 bits. Inspectors read those fields without copying.
- For a structural walk of the heap from a `TTerm` use [Term](); for a tag-only tree use [TTermExpr]().

## Basic Examples

A `TLam` result is a `TTerm`:

```wl
TReset[];
MatchQ[TLam[w, TUOpMul[w, w]], _TTerm]
```
<!-- => True -->

Field-style access mirrors the inspectors:

```wl
TReset[];
TLam[x, x]["tagName"]
```
<!-- => "LAM" -->

## Properties and Relations

The packed-field inspectors are [TTermTag](), [TTermExt](), [TTermVal](), and [TTermSub](). The numeric tag-to-name lookup is `TTagName`.
