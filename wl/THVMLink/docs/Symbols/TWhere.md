---
Template: Symbol
Name: TWhere
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWhere
Keywords: [where, select, mask, ternary, CMPLT, RL]
SeeAlso: [TMaximum, TMinimum, TClip, TReLU, TUOpCmplt, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TWhere]()[*cond*, *a*, *b*]</code> is a ternary select: it returns *a* where the 0/1 mask *cond* is 1 and *b* where it is 0. thvm has no `WHERE` opcode, so it lowers to `cond*a + (1 - cond)*b`, where *cond* is a comparison mask such as `a < b`.

## Details & Options

- *cond* is a 0/1 `TTerm`, normally a comparison mask. `a < b` lowers to a `UOP_CMPLT` mask via <code>[TUOpCmplt]()</code>, and `a == b` to a `UOP_CMPEQ` mask via <code>[TUOpCmpeq]()</code>; both read back as `1.` / `0.`.
- The mask carries no gradient (a comparison is not differentiable), so under <code>[TGrad]()</code> the cotangent splits cleanly: *a* receives *cond* and *b* receives `1 - cond`, exactly a true ternary select's rule.
- *a* and *b* broadcast against *cond*; a scalar branch is fine.
- Keep both branches finite. The unselected branch is multiplied by 0, and `0*Infinity` is `NaN`, so an `Infinity` or `NaN` in the branch you did not pick still poisons the result. This is why thvm's attention mask uses a large finite bias rather than `-Infinity`.

## Basic Examples

Select from *a* where the mask is 1 and from *b* where it is 0:

```wl
cond = TTensorCreate[{1., 0., 1., 0.}];
a    = TTensorCreate[{10., 20., 30., 40.}];
b    = TTensorCreate[{-1., -2., -3., -4.}];
Normal @ TRealize @ TWhere[cond, a, b]
```
<!-- => {10., -2., 30., -4.} -->

## Scope

The mask is usually a comparison. Picking the elementwise larger of two tensors is <code>[TWhere]()</code> over an `a < b` mask:

```wl
a = TTensorCreate[{-1., 2., 3.}];
b = TTensorCreate[{-4., -2., 9.}];
Normal @ TRealize @ TWhere[a < b, b, a]
```
<!-- => {-1., 2., 9.} -->

This is exactly how <code>[TMaximum]()</code> is defined.

## Properties and Relations

The cotangent flows only to the selected branch, so <code>[TGrad]()</code> routes the gradient by the mask. Here the mask `{1., 0., 1.}` sends the gradient of a sum entirely to *a* at positions 0 and 2:

```wl
cond = TTensorCreate[{1., 0., 1.}];
a    = TTensorCreate[{5., 6., 7.}];
b    = TTensorCreate[{0., 0., 0.}];
TRealize @ TGrad @ Total[TWhere[cond, a, b]];
Normal @ TRealize @ TGradOf[a]
```
<!-- => {1., 0., 1.} -->

## Possible Issues

Because <code>[TWhere]()</code> evaluates `cond*a + (1 - cond)*b`, **both** branches are computed at every position; the mask only weights them. The value the mask discards still passes through the multiply, so a non-finite entry in the unselected branch becomes `0*Infinity = NaN` and poisons that position. Replacing a discarded value with a different finite value shows the unselected branch never reaching the output, as long as it stays finite:

```wl
cond = TTensorCreate[{1., 0.}];
a    = TTensorCreate[{1., 99.}];
b    = TTensorCreate[{0., 2.}];
Normal @ TRealize @ TWhere[cond, a, b]
```
<!-- => {1., 2.} -- position 1 selects `b`'s 2.; the discarded 99. in `a` is multiplied by 0 and vanishes. An `Infinity` in its place would have given `0*Infinity = NaN`, so keep both branches finite (thvm's attention mask uses a large finite bias, not -Infinity). -->

