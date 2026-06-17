---
Template: Symbol
Name: TWnf
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWnf
Keywords: [wnf, reduce, head, weak normal form, hvm4]
SeeAlso: [TStep, TStack, TReduce, TCollapse, TCnf, TNf, TInteract]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TWnf]()[*term*]</code> reduces *term* to *weak normal form* -- enough to expose the head -- and returns the reduced root.

<code>[TWnf]()[*term*, *n*]</code> bails after at most *n* interactions and returns the partially reduced root. Pending eliminator frames are exposed via [TStack](). `*n*=0` means unbounded (same as the one-arg form).

## Details & Options

- WNF is the *weak* discipline: it visits the head position, firing eliminators (`APP`, `OP2`, `DP0`/`DP1`, ...) until a constructor / WHNF surfaces. It does not reach into the body of a LAM, the right of an OP2 once the left is stuck, or into `SUP` branches.
- For full reduction (every reachable redex) use `TNf`; for `cnf` (SUP-lift readback used by [TCollapse]()) use `TCnf`. See `docs/normal_form.md` for the layering.
- Bounded form is how [TStep]() is built: <code>[TStep]()[t] = [TWnf]()[t, 1]</code>. On bail, the C reducer writes the in-flight result back through each frame's primary slot so the heap mutations stick; calling `TWnf` again on the returned root resumes from there.

## Basic Examples

A beta reduction to a `NUM` head:

```wl
TReset[];
TTermVal @ TWnf @ TApp[TLam[x, TOp2["+", x, TNum[1]]], TNum[41]]
```
<!-- => 42 -->

## Scope

Bounded `TWnf` returns a partially reduced root and exposes the pending stack via [TStack]():

```wl
TReset[];
TItrs[]
```
<!-- => 0 -->

```wl
TWnf[TApp[TLam[x, TOp2["+", x, TNum[1]]], TNum[41]], 1];
TItrs[]
```
<!-- => 1 -->

## Properties and Relations

[TReduce]() is `TWnf` plus a return-of-the-original-root for chaining; [TRealize]() pairs `TMaterialize` with `TWnf` to fire the whole pipeline (UOP -> kernel -> dispatch). For an event-by-event trace of the reduction use [TMultiTrace]().
