---
Template: Symbol
Name: TStep
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TStep
Keywords: [step, single interaction, trace, debug]
SeeAlso: [TStack, TWnf, TItrs, TInteract, TMultiTrace]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TStep]()[*term*]</code> = <code>[TWnf]()[*term*, 1]</code>: fires exactly one interaction at *term*'s head and returns the partially reduced root. Inspect [TStack]() for the eliminator frames pending at the bail point.

## Details & Options

- One *interaction* is one `ITRS` bump -- one beta, one DUP-XXX, one OP2-XXX, etc. SUP-descents and other plumbing do not count.
- On bail, the C reducer writes the in-flight WHNF back through each pending eliminator frame's primary slot so the heap mutations stick; a subsequent <code>[TWnf]()[next]</code> or [TStep]() picks up from there.
- Calling [TStep]() in a loop is how a step-by-step view drives the reduction; [TMultiTrace]() builds the trace this way.

## Basic Examples

After one step the interaction counter bumps to 1:

```wl
TReset[];
TStep[TApp[TLam[x, TOp2["+", x, TNum[1]]], TNum[41]]];
TItrs[]
```
<!-- => 1 -->

## Properties and Relations

For unbounded reduction to a head use [TWnf](); for the full event trace use [TMultiTrace](). [TInteract]() fires a *specific* redex (rather than the head) by its `TTerm` handle.
