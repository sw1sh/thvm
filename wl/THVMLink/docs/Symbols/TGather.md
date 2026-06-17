---
Template: Symbol
Name: TGather
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGather
Keywords: [gather, index, select, one-hot, RL, policy gradient]
SeeAlso: [TTakeAlongAxis, TUOpCmpeq, TOneHot, TEmbedding, TUOpReduce, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGather]()[*x*, *axis*, *index*]</code> selects one element per slice along *axis* (0-indexed), keeping that axis at size 1: `out[..., 0, ...] = x[..., index, ...]`. It lowers tinygrad-style to `Total[(arange == index) * x]` reduced over *axis* -- the same `UOP_CMPEQ` one-hot the cross-entropy uses, multiplied by *x* and sum-reduced.

## Details & Options

- *index* is an integer host list or a `TTerm`, giving one index per slice. Its shape is *x*'s shape with *axis* set to 1, or the flat per-slice list (it is reshaped to the keep-shape).
- The selected axis stays at size 1 in the result (a keepdims gather), so a `{batch, n}` input gathered along *axis* `1` yields `{batch, 1}`.
- The lowering builds a one-hot mask with <code>[TUOpCmpeq]()</code> against an `arange`, multiplies by *x*, and sum-reduces with <code>[TUOpReduce]()</code>; no dedicated gather opcode is needed.
- Differentiable: the cotangent scatters back to exactly the selected positions, so it trains. This is what makes it the per-sample action log-prob selector for a policy gradient.
- For the numpy `take_along_axis` argument order `(index, axis)`, use <code>[TTakeAlongAxis]()</code>.

## Basic Examples

Gather one column per row of a 2x3 matrix, picking column 2 from row 0 and column 0 from row 1. The gathered axis stays at size 1:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}];
Normal @ TRealize @ TGather[x, 1, {2, 0}]
```
<!-- => {{3.}, {4.}} -->

## Scope

The index may itself be a `TTerm`. Casting an index tensor to f32 happens inside the lowering, so an integer-valued `TTerm` works:

```wl
x   = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}];
idx = TTensorCreate[{1., 2.}];
Normal @ TRealize @ TGather[x, 1, idx]
```
<!-- => {{2.}, {6.}} -->

## Applications

Pick each sample's action log-prob from a `{batch, n_actions}` table with a `{batch}` action list -- the selection a policy gradient performs every step. Row 0 took action 2, row 1 took action 0:

```wl
logp    = TTensorCreate[{{-2.3, -1.2, -0.5}, {-0.1, -3.0, -2.0}}];
actions = {2, 0};
Normal @ TRealize @ TGather[logp, 1, actions]
```
<!-- => {{-0.5}, {-0.1}} -->

## Properties and Relations

The gradient scatters the cotangent back to the gathered positions only. The gradient of the sum of the gathered elements is a one-hot at each selected index:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}];
TRealize @ TGrad @ Total[TGather[x, 1, {2, 0}], All];
Normal @ TRealize @ TGradOf[x]
```
<!-- => {{0., 0., 1.}, {1., 0., 0.}} -->
