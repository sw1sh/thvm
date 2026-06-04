---
Template: TechNote
Name: Train
Title: Training Neural Networks with THVMLink
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Train
Keywords: [training, gradient descent, optimizer, Adam, MLP, convolution, conv2d, ReLU, higher-order gradient, backpropagation]
RelatedGuides: [THVMLink]
RelatedTutorials: [Tensors, Overview]
---

## What training looks like on the tensor surface

The [Tensors](paclet:WolframInstitute/THVMLink/tutorial/Tensors) tutorial built up tensors, the lazy UOp graph, kernels, and a single gradient. Training is just that gradient, in a loop: build a scalar *loss*, fire [TGrad]() to fill every parameter's gradient slot, read the gradients with [TGradOf](), and write the parameter updates back in place with [TSet](). No tape, no session - the parameters are ordinary `TTerm` leaves marked [TRequiresGrad](), and each step is a fresh backward walk over the heap.

One rule carries the whole note: **realize the gradients before you apply the update.** [TSet]() writes a parameter's buffer *in place*, and a parameter's gradient is computed by reading that same buffer - so an update that hasn't first pinned the gradient to its own buffer races its own read and the loop diverges. Pinning the gradient with [TRealize]() first is exactly what an optimizer's step does.

## A training loop from scratch

Fit `w . x = 14` for `x = {1, 2, 3}` by gradient descent on the squared error. The parameter is a [TRequiresGrad]() leaf; the target and input are plain tensors:

```wl
w = TRequiresGrad @ TTensorCreate[{0., 0., 0.}];
xs = TTensorCreate[{1., 2., 3.}];
tgt = TTensorCreate[{14.}];
Normal @ TRealize[(Total[w*xs] - tgt)^2]
```
<!-- => {196.} -->

One step is *forward, backward, pin the gradient, update*. [TClearGrad]() zeroes the slot first (thvm *accumulates* into it), [TGrad]() fills it, <code>[TRealize]() @ [TGradOf]()[*w*]</code> pins it to a buffer, and [TSet]() steps the parameter against it:

```wl
TClearGrad[w];
TGrad[(Total[w*xs] - tgt)^2];
grad = TRealize @ TGradOf[w];
TSet[w, w + (-0.01)*grad];
Normal @ w
```
<!-- => {0.28, 0.56, 0.84}  (w - 0.01 * d/dw of (w.x - 14)^2) -->

Wrapping that in a loop drives the loss down. [Table]() collects the loss after each of eight steps so you can watch it converge:

```wl
Table[
    TClearGrad[w];
    TGrad[(Total[w*xs] - tgt)^2];
    g = TRealize @ TGradOf[w];
    TSet[w, w + (-0.01)*g];
    First @ Normal @ TRealize[(Total[w*xs] - tgt)^2],
    {8}]
```
<!-- => {52.6728, 27.3056, 14.1552, 7.33805, 3.80405, 1.97202, 1.02229, 0.529957} (loss decreasing) -->

## A two-layer MLP

Real models stack linear layers with a nonlinearity. [TLinear]() is `x . W + b`; [TReLU]() is the elementwise `max(x, 0)` (built as `x * (0 < x)`, a CMPLT mask). Initialize the weights with [TGlorot]() (He/Glorot scaling) and the biases with [TZeros](), marking every one a parameter:

```wl
w1 = TRequiresGrad @ TGlorot[{3, 8}];
b1 = TRequiresGrad @ TZeros[{8}];
w2 = TRequiresGrad @ TGlorot[{8, 1}];
b2 = TRequiresGrad @ TZeros[{1}];
mlp = xIn |-> TLinear[TReLU[TLinear[xIn, w1, b1]], w2, b2];
TTensorShape @ TRealize @ mlp[TTensorCreate[{{1., 2., 3.}}]]
```
<!-- => {1, 1} -->

[TGrad]() with a list of targets differentiates every parameter in one shared backward walk, returning the gradients in target order; realize them together so the shared forward emits once:

```wl
xb = TTensorCreate[{{1., 2., 3.}}];
yb = TTensorCreate[{{14.}}];
loss = Total[(mlp[xb] - yb)^2];
params = {w1, b1, w2, b2};
TClearGrad /@ params;
grads = TGrad[loss, params];
Length @ TRealize[grads]
```
<!-- => 4 -->

## Optimizers

Hand-written SGD is the foundation; [TAdam]() packages the adaptive-moment update. It takes the loss, the parameter list, per-parameter first/second moment buffers (seed with [TZerosLike]()), and the step index, computes the gradients internally, and applies the step in place. On the one-element regression `(w - 0.05)^2` from `w = 1`, the textbook first Adam step (lr `0.001`) lands at `0.999`:

```wl
wA = TTensorCreate[{1.}];
mA = TTensorCreate[{0.}];
vA = TTensorCreate[{0.}];
TAdam[TL2Loss[wA - TTensorCreate[{0.05}]], {wA}, {mA}, {vA}, 1];
Normal @ TTensorData[wA]
```
<!-- => {0.999}  (one Adam step: 1 - 0.001 * mhat / (sqrt(vhat) + eps)) -->

Pass a list of parameters and a matching list of moment buffers to step a whole network's weights at once - that is how an optimizer loops over the [TGrad]()-filled `params` from the previous section.

## Convolutional layers

[TConv2D]() is a stride-1, no-padding 2-D convolution: *input* `{C_in, H, W}` (or batched `{B, C_in, H, W}`), *weights* `{C_out, C_in, kh, kw}`, *bias* `{C_out}`. It lowers through im2col + a fused matmul. Here a single 2x2 filter over a 4x4 image:

```wl
img = TTensorCreate[N @ ArrayReshape[Range[16], {1, 4, 4}]];
cw = TRequiresGrad @ TTensorCreate[N @ ArrayReshape[Range[4], {1, 1, 2, 2}]];
cb = TTensorCreate[{0.}];
Normal @ TRealize @ TConv2D[img, cw, cb]
```
<!-- => {{{44., 54., 64.}, {84., 94., 104.}, {124., 134., 144.}}}  (a {1, 3, 3} feature map) -->

The whole convolution is differentiable. Mark the filter a parameter, build a scalar from the output, and [TGrad]() fills the filter's gradient - this is the backward pass a conv-net training step runs:

```wl
TClearGrad[cw];
TGrad[Total[TConv2D[img, cw, cb]^2]];
Normal @ TRealize @ TGradOf[cw]
```
<!-- => {{{{12192., 13884.}, {18960., 20652.}}}}  (d/d(filter) of sum(conv^2)) -->

A pooled two-layer conv stack - <code>[TConv2D]()</code> -> [TReLU]() -> <code>[TMaxPool2d]()</code> -> [TConv2D]() - is the standard image-classifier front end; its stacked backward pass is the stress test for the scheduler's reduce-boundary handling.

## From a Wolfram net

You need not assemble layers by hand. [TFromNet]() lifts an initialised Wolfram NeuralNetworks net into the same `TTerm` UOp graph, so the whole pipeline - [TRealize](), [TGrad](), [TJit]() - applies to a net you built with [NetChain]() and [NetInitialize](). The lifted graph computes exactly what the net does:

```wl
net = NetInitialize[NetChain[{LinearLayer[8], Ramp, LinearLayer[1]}, "Input" -> 3], RandomSeeding -> 1234];
xn = TTensorCreate[{1., 2., 3.}];
Normal @ TRealize @ TFromNet[net, xn]
```
<!-- => {-0.504425}  (identical to net[{1., 2., 3.}]) -->

Once lifted, the forward is an ordinary `TTerm`: differentiate it with [TGrad](), step its weights with [TSet](), or capture the step with [TJit](). Training a [TFromNet]()-built net end to end - `NetTrain` and `TNetPredict` installed directly on the `TTerm`, with `data` as input-class rules or the built-in `"MNIST"` dataset, plus a head-to-head time/memory comparison against tinygrad - is the subject of the next stage of this note (see *Where to go next*).

## Higher-order gradients

Because [TGradOf]() returns a live UOP graph, you can differentiate it again. The first gradient of `x^3` is `3 x^2`; differentiating *that* gives the second derivative `6 x`:

```wl
x = TRequiresGrad @ TTensorCreate[{3.}];
TClearGrad[x];
TGrad[Total[x^3]];
Normal @ TRealize @ TGradOf[x]
```
<!-- => {27.}  (3 x^2 at x = 3) -->

```wl
g1 = TGradOf[x];
TClearGrad[x];
TGrad[Total[g1]];
Normal @ TRealize @ TGradOf[x]
```
<!-- => {18.}  (d/dx of 3 x^2 = 6 x at x = 3) -->

## Where to go next

- The [Tensors](paclet:WolframInstitute/THVMLink/tutorial/Tensors) tutorial for the tensor / UOp / kernel / autodiff machinery underneath this loop.
- Per-symbol pages: [TGrad](paclet:WolframInstitute/THVMLink/ref/TGrad), [TSet](paclet:WolframInstitute/THVMLink/ref/TSet), [TLinear](paclet:WolframInstitute/THVMLink/ref/TLinear), [TConv2D](paclet:WolframInstitute/THVMLink/ref/TConv2D), [TAdam](paclet:WolframInstitute/THVMLink/ref/TAdam), [TFromNet](paclet:WolframInstitute/THVMLink/ref/TFromNet).
- Capture a step with [TJit]() so the loop compiles once and replays every epoch (see the [Tensors](paclet:WolframInstitute/THVMLink/tutorial/Tensors) "Capturing a step" section).
- **Coming next (nettrain integration):** the high-level `NetTrain[net, data]` / `TNetPredict` surface installed directly on a [TFromNet]()-built `TTerm` - the inert recursive optimizer term that [TWnf]() drives in place - a full sugarified end-to-end **MNIST** example (`data -> "MNIST"`), and a head-to-head **time + memory** comparison against tinygrad. This lands with `Kernel/Train.wl`.
