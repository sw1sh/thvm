---
Template: Symbol
Name: THeapSet
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapSet
Keywords: [heap, write, cell, loc, low-level]
SeeAlso: [THeapRead, THeapAlloc, THeapBase, THeapPos]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapSet]()[*loc*, *term*]</code> writes *term* into `heap[*loc*]`.

## Details & Options

- Low-level: writes the raw `Term` word verbatim, no coercion. Caller is responsible for shape and tag correctness.
- Use it for hand-authored heap layouts in tests / fixtures, for low-level term graph surgery, and inside constructors like [TLam](), [TSup](), and [TCtr]() that compose larger graphs out of cell-sized writes.
- Pair with [THeapAlloc]() to reserve cells before writing.

## Basic Examples

Allocate, write, read:

```wl
TReset[];
loc = THeapAlloc[1];
THeapSet[loc, TNum[7]];
TTermVal @ THeapRead[loc]
```
<!-- => 7 -->

## Properties and Relations

The matching read is [THeapRead](). Constructor sugar ([TLam](), [TApp](), ...) wraps these primitives so the typical user never reaches for `THeapSet`.
