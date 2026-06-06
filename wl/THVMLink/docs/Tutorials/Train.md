---
Template: TechNote
Name: Train
Title: Training Neural Networks with THVMLink
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Train
Keywords: [training, gradient descent, optimizer, Adam, MLP, convolution, conv2d, ReLU, higher-order gradient, backpropagation, LeNet, NetModel, NetTrain, NetPredict, Metal, GPU, MNIST]
RelatedGuides: [THVMLink]
RelatedTutorials: [Tensors, Overview]
---

## What training looks like on the tensor surface

The [Tensors](paclet:WolframInstitute/THVMLink/tutorial/Tensors) tutorial built up tensors, the lazy UOp graph, kernels, and a single gradient. Training is just that gradient, in a loop: build a scalar *loss*, fire [TGrad]() to fill every float leaf's gradient slot, read the gradients with [TGradOf](), and write the parameter updates back in place with [TSet](). No tape, no session, no `requires_grad` flag (matching tinygrad) - the parameters are ordinary `TTerm` leaves, and each step is a fresh backward walk over the heap that auto-grads every reachable float leaf.

One rule carries the whole note: **realize the gradients before you apply the update.** [TSet]() writes a parameter's buffer *in place*, and a parameter's gradient is computed by reading that same buffer - so an update that hasn't first pinned the gradient to its own buffer races its own read and the loop diverges. Pinning the gradient with [TRealize]() first is exactly what an optimizer's step does.

## A training loop from scratch

Fit `w . x = 14` for `x = {1, 2, 3}` by gradient descent on the squared error. The parameter is an ordinary leaf - [TGrad]() auto-grads every float leaf, so the target and input get gradients too (you simply ignore the ones you do not update):

```wl
w = TTensorCreate[{0., 0., 0.}];
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
w1 = TGlorot[{3, 8}];
b1 = TZeros[{8}];
w2 = TGlorot[{8, 1}];
b2 = TZeros[{1}];
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
cw = TTensorCreate[N @ ArrayReshape[Range[4], {1, 1, 2, 2}]];
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

Once lifted, the forward is an ordinary `TTerm`: differentiate it with [TGrad](), step its weights with [TSet](), or capture the step with [TJit]() - exactly the conv classifier the next section trains on MNIST. The high-level [TNetTrain]() / [TNetPredict]() surface trains the published LeNet end to end on the GPU below, lifting its batched forward straight from the [NetChain](); see *End to end: the real LeNet*.

## A convolutional MNIST classifier

The pooled conv stack from the previous section is the front end of an image classifier. Cap it with a [TLinear]() head and you have a LeNet-style network. Mark every weight a parameter and confirm the forward maps a batch of `{1, 28, 28}` images to a `{batch, 10}` logit matrix - convolution, ReLU, max-pool, flatten, linear:

```wl
w1 = TGlorot[{8, 1, 3, 3}]; b1 = TZeros[{8}];
w2 = TGlorot[{8*13*13, 10}]; b2 = TZeros[{10}];
lenet = x |-> TLinear[ArrayReshape[TMaxPool2d[TReLU[TConv2D[x, w1, b1]]], {64, 8*13*13}], w2, b2];
TTensorShape @ TRealize @ lenet[TTensorCreate[N @ RandomReal[1, {64, 1, 28, 28}]]]
```
<!-- => {64, 10} -->

[TCategoricalCrossEntropy]() is the classification loss: it takes the *raw logits* (no explicit softmax) and a one-hot target, lowering to the stable logsumexp `max(z) + log(sum(exp(z - max(z)))) - sum(target*z)`. The max-subtract keeps it finite even when one logit dwarfs the rest - here the true class sits 100 below the peak, so the loss is exactly that gap, where a naive `log(sum(exp(z)))` would overflow `exp` to infinity:

```wl
First @ Normal @ TRealize @ TCategoricalCrossEntropy[TTensorCreate[{{0., 1., 100.}}], TTensorCreate[{{1., 0., 0.}}]]
```
<!-- => 100.  (stable: the true-class logit 0 is 100 below the max) -->

Training is the loop from the top of this note, one batch of images at a time: clear the gradient slots, fire [TGrad]() through the cross-entropy, *realize the gradients before the update*, and step each weight with [TSet](). Six epochs over 2048 shuffled MNIST digits (batch 64, SGD at `lr = 0.1`) reaches **92% test accuracy** on a held-out set:

```wl
#| eval: false
data    = RandomSample[ResourceData["MNIST", "TrainingData"], 2048];
images  = ArrayReshape[N @ Flatten[ImageData /@ Keys[data]], {2048, 1, 28, 28}];
onehots = N[UnitVector[10, # + 1] & /@ Values[data]];
params  = {w1, b1, w2, b2};
Do[
    Do[
        batch  = TTensorCreate[images[[64*(s - 1) + 1 ;; 64*s]]];
        target = TTensorCreate[onehots[[64*(s - 1) + 1 ;; 64*s]]];
        TClearGrad /@ params;
        grads = TRealize @ TGrad[TCategoricalCrossEntropy[lenet[batch], target], params];
        MapThread[TSet[#1, #1 - 0.1*#2] &, {params, grads}],
        {s, 32}],
    {epoch, 6}]
```

Nothing here is a special training mode: the network, the softmax cross-entropy, and the backward pass through all of it are plain `TTerm` arithmetic differentiated by the same [TGrad]() that handled `w . x` at the top of the note.

### The same net, lifted from a NetChain

You need not hand-assemble the layers. Build the network as a [NetChain]() and lift it with [TFromNet](): the conv head, pooling, flatten, and linear all convert, each lifted weight a plain `TTerm` leaf baked into the forward. [TNetParams]() reads exactly those weight `TTerm`s straight off the GRAPH - the float-leaf tensors reachable from the forward, minus the input slot - so a [TSet]() update flows straight back into it (the graph leaves, not a flag or a stored net, are how the trainable weights are identified). Lift the same LeNet head over a batch-shaped input slot and collect its four trainable tensors:

```wl
lenet = NetInitialize[NetChain[{ConvolutionLayer[8, {3, 3}], Ramp, PoolingLayer[{2, 2}, {2, 2}], FlattenLayer[], LinearLayer[10]}, "Input" -> {1, 28, 28}], RandomSeeding -> 1234];
slot = TTensorCreate[N @ RandomReal[1, {64, 1, 28, 28}]];
fwd = TFromNet[lenet, slot];
params = Select[TNetParams[fwd], TTermVal[#] =!= TTermVal[slot] &];
{Length[params], TTensorShape @ TRealize @ fwd}
```
<!-- => {4, {64, 10}}  (4 params: conv W/b + linear W/b; forward is {batch, 10} logits) -->

Now `fwd` is the logit `TTerm` and `params` are its weights - the same two ingredients the hand-built loop used. Training is identical, except each batch is fed by overwriting the input slot in place with [TAssign]() rather than rebuilding the forward. It reaches the same **~92.6% test accuracy**:

```wl
#| eval: false
Do[
    Do[
        TRealize @ TAssign[slot, TTensorCreate[images[[64*(s - 1) + 1 ;; 64*s]]]];
        target = TTensorCreate[onehots[[64*(s - 1) + 1 ;; 64*s]]];
        TClearGrad /@ params;
        grads = TRealize @ TGrad[TCategoricalCrossEntropy[fwd, target], params];
        MapThread[TSet[#1, #1 - 0.1*#2] &, {params, grads}],
        {s, 32}],
    {epoch, 5}]
```

A Wolfram [NetChain](), lifted, trained by the same four-line step. And that whole loop is itself packaged as one call - the next section.

### One call: TNetTrain and TNetPredict

[TNetTrain]() wraps the lift-Reap-train loop: pass an initialised net plus host arrays of inputs and one-hot targets, and it returns a handle carrying the lifted `"Forward"` `TTerm`, its `"Input"` slot, and the trained `"Params"`. [TNetPredict]() then evaluates that handle on a fresh same-shape batch. A linearly-separable two-class set trains and classifies in two lines:

```wl
SeedRandom[1234];
xs = N @ {{1., 1.}, {1.5, 1.2}, {0.5, 0.8}, {1.2, 0.9}, {-1., -1.}, {-1.2, -0.8}, {-0.7, -1.1}, {-0.9, -1.}};
ys = N @ {{1., 0.}, {1., 0.}, {1., 0.}, {1., 0.}, {0., 1.}, {0., 1.}, {0., 1.}, {0., 1.}};
clf = NetInitialize[NetChain[{LinearLayer[6], Ramp, LinearLayer[2]}, "Input" -> 2], RandomSeeding -> 7];
trained = TNetTrain[clf, xs, ys, "MaxTrainingRounds" -> 80];
(Ordering[#, -1][[1]] - 1) & /@ TNetPredict[trained, xs]
```
<!-- => {0, 0, 0, 0, 1, 1, 1, 1}  (both classes separated) -->

`"LearningRate"` (default 0.3), `"MaxTrainingRounds"` (100), and `"Loss"` (the stable [TCategoricalCrossEntropy](), expecting a logits net - no final `SoftmaxLayer`) are options. The same call scales to the conv MNIST head above; the loop inside is exactly the realize-grads-first SGD this note opened with.

## End to end: the real LeNet, trained on the Metal GPU

Everything so far built the net by hand or as a small [NetChain](). The same pipeline lifts a *published* model unchanged. [NetModel]() carries LeNet - the original two-convolution digit classifier - as a [NetChain]() with an [Image]() input encoder and a `"Class"` output decoder. On this machine's Wolfram Language version it comes back **uninitialised** (its weights are `Automatic`), so [NetInitialize]() gives it concrete random weights: training it is training *from scratch*. [TFromNet]() lifts the initialised net into a `TTerm` for inference; [TNetTrain]() (below) lifts the batched forward from the same [NetChain]() for training, dropping the trailing `SoftmaxLayer` internally so the loss sees logits:

```wl
init = NetInitialize[NetModel["LeNet"], RandomSeeding -> 7];
lenet = TFromNet[init];
Length @ TNetParams[lenet]
```
<!-- => 8  (2 conv + 2 fully-connected layers, each a weight + a bias) -->

The lifted `lenet` is an ordinary `TTerm` that runs inference at its lifted input shape. Training over a batch lifts the batched forward directly from the [NetChain](), so [TNetTrain]() takes the initialised net `init`. Real MNIST digits are [Image]() objects; [TNetTrain]() takes them as the *keys* of `image -> class` rules and reshapes each through the encoder's `{1, 28, 28}` array internally, with no host-side [ImageData]() ceremony at the call site:

```wl
data = RandomSample[ResourceData["MNIST", "TrainingData"], 256];
First @ Keys @ data
```
<!-- => (a 28x28 MNIST digit Image) -->

### Prescreen the step, then dispatch on the GPU

The inert loop materialises the *entire* training step once - forward, cross-entropy, the shared backward, and one in-place update per weight - and a single [TWnf]() re-fires that **fixed** kernel set every round. So the kernels added while building the `"TrainingNet"` term (nothing has fired yet) are exactly the set the GPU will re-run. Counting them with [TKernelCount]() is the Metal safety check: a small, fixed set is safe to dispatch, because re-firing it cannot over-fuse the backward into the thousands-of-kernels shape that can hang the GPU. Read the count before and after the build for the loop's own contribution, isolated from kernels the rest of the page has accumulated. A small subset (64 digits, fifteen full-batch rounds) keeps the cell quick while still training from scratch:

```wl
SeedRandom[7];
init = NetInitialize[NetModel["LeNet"], RandomSeeding -> 7];
lenet = TFromNet[init];
data = RandomSample[ResourceData["MNIST", "TrainingData"], 64];
before = TKernelCount[];
loop = TNetTrain[init, data, "TrainingNet", MaxTrainingRounds -> 15, "LearningRate" -> 0.2];
TKernelCount[] - before
```
<!-- => 294  (the fixed step set; well under the few-hundred Metal-safety bar) -->

294 kernels, built before a single round runs, re-fired in place by [TWnf]() - so training this LeNet on the GPU is safe. Drive the loop with one [TWnf]() and no new kernels appear: the fixed set just fires fifteen times in place. The cell below really runs the training on the CPU (the doc build never dispatches Metal), reports its per-round wall time, and asserts the kernel set never grew - exactly the safety invariant the GPU relies on:

```wl
t = First @ AbsoluteTiming @ TWnf[loop];
{TKernelCount[] - before, Round[1000.*t/15, 1]}
```
<!-- => {294, _}  (294 UNCHANGED: TWnf re-fired the fixed set, no over-fusion; second number = live CPU ms/round) -->

The kernel count is unchanged: [TWnf]() re-fired the fixed 294-kernel set fifteen times in place, no over-fusion. The training really happened - the trained `lenet` now classifies its training digits well above the 10% chance baseline:

```wl
N @ Mean @ MapThread[Boole[#1 == #2] &, {TNetPredict[lenet, Keys[data]], Values[data]}]
```
<!-- => 0.969  (train-subset accuracy: fifteen SGD rounds carried it from ~0.1 chance) -->

The convenience form `TNetTrain[init, data, MaxTrainingRounds -> 15, "LearningRate" -> 0.2]` does the [TWnf]() for you and returns the trained forward. The same fixed 294-kernel set dispatches unchanged on the Metal GPU: set `DEV=metal` in the environment (the runtime reads it on init) and the loop runs on the GPU instead, far faster per round.

The per-round wall time the cell above produces is the live CPU number; the Metal and tinygrad columns are representative measured references on an Apple M3 Max (the GPU figures come from running [`lenet_metal_tutorial.wls`](../../Examples/lenet_metal_tutorial.wls) with `DEV=metal`, the same fixed step set warm):

| backend (this step) | ms/round | held-out accuracy |
|---|---|---|
| thvm CPU (clang, live above) | (the number the cell prints) | 0.97 (train subset) |
| thvm Metal (M3 Max, measured ref) | ~53 | - |
| thvm Metal, 256 imgs / 60 rounds (measured ref) | ~86 | 0.88 |
| tinygrad Metal, 256 imgs / 60 rounds (measured ref) | ~32 | 0.77 |

The CPU number is order-of-seconds per round; the same 294-kernel step on the Metal GPU is roughly 20x faster once warm. On the larger 256-image / 60-round run the GPU reaches **~88%** held-out accuracy at about **86 ms/round** - the figures [`lenet_metal_tutorial.wls`](../../Examples/lenet_metal_tutorial.wls) reports. tinygrad's fused schedule for the identical LeNet runs faster still (~32 ms/round) but converges to a lower subset accuracy here; both train the real LeNet end to end on the GPU.

### Predict on held-out images

[TNetPredict]() runs the trained forward on a list of [Image]() objects and returns the integer class for each (the logits' argmax - the `"Class"` decoder). The live cells above trained on a 64-digit subset for fifteen rounds to keep the page fast; train the full 256-image / 60-round run (the configuration [`lenet_metal_tutorial.wls`](../../Examples/lenet_metal_tutorial.wls) uses, ideally with `DEV=metal`) and the same `lenet`, trained in place, reaches about **88%** accuracy from scratch on a held-out test set:

```wl
#| eval: false
test = RandomSample[ResourceData["MNIST", "TestData"], 256];
preds = TNetPredict[lenet, Keys[test]];
N @ Mean @ MapThread[Boole[#1 == #2] &, {preds, Values[test]}]
```
<!-- => 0.879  (held-out test accuracy, from scratch, 256-image train + test subset) -->

The predictions are sensible digit-by-digit - a sample of held-out images with the model's call against the truth:

```wl
#| eval: false
test = RandomSample[ResourceData["MNIST", "TestData"], 8];
preds = TNetPredict[lenet, Keys[test]];
Grid[{Keys[test], MapThread["pred " <> ToString[#1] <> " / true " <> ToString[#2] &, {preds, Values[test]}]}]
```
<!-- => (8 digit images over their predicted / true labels; most match) -->

[`wl/THVMLink/Examples/lenet_metal_tutorial.wls`](../../Examples/lenet_metal_tutorial.wls) runs this whole pipeline at the full 256-image / 60-round size - lift, prescreen, train, predict - and prints the accuracy, the per-round timing, and a sample-prediction grid. The tinygrad reference for the same architecture and backend is [`bench/netmodel-mnist/lenet_tinygrad.py`](../../../../bench/netmodel-mnist/lenet_tinygrad.py): the identical LeNet (`Conv2d[1 -> 20, 5x5]` and `Conv2d[20 -> 50, 5x5]`, two fully-connected layers) trained the same way on the same Metal GPU runs at about **32 ms/round** and reaches **~0.77** test accuracy on this subset. thvm is slower per round (the inert loop re-fires more, smaller kernels than tinygrad's fused schedule) but converges to a higher subset accuracy here; both train the real LeNet end to end on the GPU.

### Time and memory against tinygrad

That same network (`Conv2d[1 -> 8, 3x3]` -> ReLU -> 2x2 max-pool -> `Linear[1352 -> 10]`, batch 64, softmax cross-entropy) runs as an eager forward + backward + SGD step in both thvm and [tinygrad](https://tinygrad.org) on the CPU. Warm (post-warmup) per-step wall time and the memory the *training itself* allocates:

| | per-step (warm) | training working set |
|---|---|---|
| thvm (`TTerm` eager) | ~32 ms | ~60 MB |
| tinygrad (eager) | ~15 ms | ~119 MB (whole process) |

thvm is about 2x slower than tinygrad for this small eager step on the CPU. The gap closes - and on the larger fused `beautiful_mnist` pipeline with the JIT and the faithful realize point, reverses - where thvm's CPU codegen edges out tinygrad. Absolute process footprint is not comparable: thvm runs inside a WolframKernel whose runtime baseline (~2.7 GB) dwarfs the tens of megabytes the training allocates, while tinygrad's lean Python process peaks at 119 MB. The per-step working set - what the conv buffers, activations, and gradients actually cost - is comparable, in the tens of megabytes either way.

## Higher-order gradients

Because [TGradOf]() returns a live UOP graph, you can differentiate it again. The first gradient of `x^3` is `3 x^2`; differentiating *that* gives the second derivative `6 x`:

```wl
x = TTensorCreate[{3.}];
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

This is not limited to scalars. Because every layer's backward is itself a `TTerm` graph, the second derivative composes through a real network: the filter Hessian of `Total[TConv2D[x, w]^2]`, the input Hessian across the [TReLU]() mask, and a `linear -> ReLU -> linear` MLP's input Hessian all come out correct (the last verified against a central finite difference). The `nn` test suite pins these.

## Where to go next

- The [Tensors](paclet:WolframInstitute/THVMLink/tutorial/Tensors) tutorial for the tensor / UOp / kernel / autodiff machinery underneath this loop.
- Per-symbol pages: [TGrad](paclet:WolframInstitute/THVMLink/ref/TGrad), [TSet](paclet:WolframInstitute/THVMLink/ref/TSet), [TLinear](paclet:WolframInstitute/THVMLink/ref/TLinear), [TConv2D](paclet:WolframInstitute/THVMLink/ref/TConv2D), [TAdam](paclet:WolframInstitute/THVMLink/ref/TAdam), [TFromNet](paclet:WolframInstitute/THVMLink/ref/TFromNet).
- Capture a step with [TJit]() so the loop compiles once and replays every epoch (see the [Tensors](paclet:WolframInstitute/THVMLink/tutorial/Tensors) "Capturing a step" section).
- The [TNetTrain]() / [TNetPredict]() one-liner above for the packaged surface - <code>[TNetTrain]()[*net*, *data*, [MaxTrainingRounds]() -> *n*]</code> on a [NetChain]() (its batched forward lifted directly), with `data` a list of `input -> class` rules or the string `"MNIST"`.
- [`wl/THVMLink/Examples/lenet_metal_tutorial.wls`](../../Examples/lenet_metal_tutorial.wls) for the full LeNet-on-Metal run end to end.
- **Still coming:** reconstructing a trained [NetChain]() back out of a lifted-and-trained `TTerm` (the weights train in place; round-tripping them into a fresh Wolfram net is the open piece).
