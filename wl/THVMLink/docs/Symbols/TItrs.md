---
Template: Symbol
Name: TItrs
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TItrs
Keywords: [interactions, counter, ITRS, hvm4]
SeeAlso: [TStep, TWnf, TReset, TMultiTrace]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TItrs]()[]</code> returns the cumulative number of interactions fired since the most recent [TReset]() (or runtime init).

## Details & Options

- One *interaction* is one `ITRS++` site -- one beta, one DUP-XXX, one OP2-XXX, one MAT-dispatch, ... SUP-descent and other plumbing do not count.
- Use it to measure the cost of a reduction or to detect non-termination in tests (`TItrs[]` before and after, assert the delta is bounded).
- The [TMultiTrace]() event ids are derived from this counter: event 0 is the very first interaction in the session.

## Basic Examples

After one beta `+` fold the counter shows two fires (APP-LAM and OP2-NUM-NUM):

```wl
TReset[];
TWnf[TApp[TLam[x, TOp2["+", x, TNum[1]]], TNum[41]]];
TItrs[]
```
<!-- => 2 -->

## Properties and Relations

[TReset]() zeroes the counter; [TStack]() shows the *remaining* work after a bounded run; [TStep]() bumps it by one.
