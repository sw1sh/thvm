---
Template: Symbol
Name: TMatNum
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMatNum
Keywords: [match, mat, pattern, num, case, dispatch]
SeeAlso: [TMatCtr, TMatChain, TIfZero, TNum, TCtr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMatNum]()[*matchVal*, *handler*, *fallback*]</code> returns a `TAG_MAT` atom that dispatches by the tag of its applied argument:

- `TAG_NUM` whose value equals *matchVal* -> *handler* (returned as-is).
- `TAG_CTR` whose ext equals *matchVal* -> destructure: *handler* is applied positionally to the constructor's children via an `APP` chain (HVM4's `APP-MAT-CTR-MAT`).
- Anything else -> <code>[TApp]()[*fallback*, *arg*]</code>.

## Details & Options

- The arg is forced to WHNF via `wnf` (and through `cnf` if it is a stuck `DP0`/`DP1`) before the dispatch decides; the matcher itself is an atom.
- `SUP` on the arg side commutes (`APP-MAT-SUP`): the matcher is duplicated over the SUP's branches.
- For NUM-dispatch the handler is *returned*, not applied -- pass a value (e.g. `[TNum]()[100]`) or a graph, not a lambda. For CTR-destructuring pass a *handler* whose lambda arity equals the constructor's child count.
- [TIfZero]() is the WL sugar over the common `NUM` test against `0`; [TMatChain]() chains several `TMatNum` for multi-label dispatch.

## Basic Examples

NUM dispatch returns the handler:

```wl
TReset[];
m = TMatNum[0, TNum[100], TLam[k, TNum[200]]];
TTermVal @ TWnf @ TApp[m, TNum[0]]
```
<!-- => 100 -->

Fallback receives the arg via `APP[fallback, arg]`:

```wl
TReset[];
m = TMatNum[0, TNum[100], TLam[k, TNum[200]]];
TTermVal @ TWnf @ TApp[m, TNum[5]]
```
<!-- => 200 -->

## Scope

CTR destructure: the handler's arity matches the constructor's child count:

```wl
TReset[];
ctr  = TCtr[1, TNum[10], TNum[20]];
add  = TMatNum[1, TLam[a, TLam[b, TOp2["+", a, b]]], TLam[k, TNum[0]]];
TTermVal @ TWnf @ TApp[add, ctr]
```
<!-- => 30 -->

## Properties and Relations

[TMatCtr]() is a sugar alias on the same primitive when the intended use is destructuring. For multi-label dispatch use [TMatChain]() (it folds nested `TMatNum`s).
