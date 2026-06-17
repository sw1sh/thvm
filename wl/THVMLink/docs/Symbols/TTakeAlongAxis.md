---
Template: Symbol
Name: TTakeAlongAxis
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTakeAlongAxis
Keywords: [take, along, axis, gather, index, RL, policy gradient]
SeeAlso: [TGather, TUOpCmpeq, TOneHot, TEmbedding, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTakeAlongAxis]()[*x*, *index*, *axis*]</code> selects *x* at integer *index* along *axis*, keeping the axis at size 1. It is <code>[TGather]()[*x*, *axis*, *index*]</code> with the numpy `take_along_axis` argument order `(index, axis)`.

## Details & Options

- Identical behavior to <code>[TGather]()</code>; only the argument order differs (`(index, axis)` here, `(axis, index)` there), matching numpy's `take_along_axis`.
- *index* is an integer host list or a `TTerm`, one index per slice; the selected axis collapses to size 1 in the result (a keepdims select).
- Lowers to the same one-hot select + sum-reduce, so the gradient scatters the cotangent back to the selected positions and the op trains.
- This is the per-sample selection a policy gradient uses to pick each action's log-prob from `{batch, n_actions}` logits with a `{batch}` action list (*axis* `1`).

## Basic Examples

Pick one column per row of a 2x3 matrix, column 2 from row 0 and column 0 from row 1. The selected axis stays at size 1:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}];
Normal @ TRealize @ TTakeAlongAxis[x, {2, 0}, 1]
```
<!-- => {{3.}, {4.}} -->

## Applications

Pick each sample's action log-prob from a `{batch, n_actions}` table with a `{batch}` action list. Row 0 took action 2, row 1 took action 0, so this reads off `-0.5` and `-0.1` -- the score term a policy gradient maximizes:

```wl
logp    = TTensorCreate[{{-2.3, -1.2, -0.5}, {-0.1, -3.0, -2.0}}];
actions = {2, 0};
Normal @ TRealize @ TTakeAlongAxis[logp, actions, 1]
```
<!-- => {{-0.5}, {-0.1}} -->

## Properties and Relations

It is exactly <code>[TGather]()</code> with the arguments swapped, so the two agree:

```wl
x   = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}];
viaTake = Normal @ TRealize @ TTakeAlongAxis[x, {2, 0}, 1];
viaGather = Normal @ TRealize @ TGather[x, 1, {2, 0}];
viaTake === viaGather
```
<!-- => True -->

---

The gradient scatters the cotangent back to the selected positions, so summing the picked log-probs and differentiating gives a one-hot at each action -- the policy-gradient update direction:

```wl
logp    = TTensorCreate[{{-2.3, -1.2, -0.5}, {-0.1, -3.0, -2.0}}];
actions = {2, 0};
TRealize @ TGrad @ Total[TTakeAlongAxis[logp, actions, 1], All];
Normal @ TRealize @ TGradOf[logp]
```
<!-- => {{0., 0., 1.}, {1., 0., 0.}} -->
