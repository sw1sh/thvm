---
Template: TechNote
Name: Train
Title: Training Neural Networks with THVMLink
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Train
Keywords: [training, gradient descent, optimizer, Adam, MLP, multilayer perceptron, convolution, ReLU, cross-entropy, higher-order gradient, backpropagation, LeNet, NetChain, TNetTrain, TFromNet, TJit, MNIST]
RelatedGuides: [THVMLink]
RelatedTutorials: [Tensors, Overview, Gpt2]
---

Training a model repeats one step: build a scalar loss from the parameters, differentiate it with [TGrad](), which adds the gradient of each tensor to that tensor's gradient slot, read the gradients with [TGradOf](), and write the updated parameters into their buffers with [TSet](). The parameters are ordinary [TTerm]() tensors. [TGrad]() differentiates with respect to every floating-point tensor the loss depends on, so no flag marks which tensors are trained, and the backward pass is a branch added to the graph rather than a recorded tape.

Compute the gradients with [TRealize]() before writing the update. [TSet]() overwrites a parameter's buffer, and the gradient of that parameter reads the same buffer, so an update built from a gradient that is still a lazy graph reads values that the update is overwriting.

## A training loop from scratch

Fit a weight vector so that its dot product with $\{1, 2, 3\}$ is 14, by gradient descent on the squared error. The weights start at zero:

```wl
w = TTensorCreate[{0., 0., 0.}]
```

The input:

```wl
xs = TTensorCreate[{1., 2., 3.}]
```

The target:

```wl
tgt = TTensorCreate[{14.}]
```

The squared error at the start is $14^2$:

```wl
Normal @ TRealize[(Total[w*xs] - tgt)^2]
```
<!-- => {196.} -->

A step clears the gradient slot, differentiates the loss, computes the gradient and updates the weights. [TClearGrad]() clears the slot, since [TGrad]() adds to it:

```wl
TClearGrad[w]
```

Differentiate the squared error:

```wl
TGrad[(Total[w*xs] - tgt)^2]
```

Compute the gradient, $2 (0 - 14) \{1, 2, 3\}$:

```wl
grad = TRealize @ TGradOf[w]
```

Step against the gradient with learning rate 0.01:

```wl
TSet[w, w - 0.01*grad]
```

The updated weights:

```wl
Normal[w]
```
<!-- => {0.28, 0.56, 0.84} -->

Repeating the step drives the error down. [Table]() records the squared error after each of eight more steps:

```wl
Table[
    TClearGrad[w];
    TGrad[(Total[w*xs] - tgt)^2];
    g = TRealize @ TGradOf[w];
    TSet[w, w - 0.01*g];
    First @ Normal @ TRealize[(Total[w*xs] - tgt)^2],
    {8}]
```
<!-- => {52.6728, 27.3056, 14.1552, 7.33806, 3.80405, 1.97202, 1.0223, 0.529959} -->

## A two-layer perceptron

A multilayer perceptron alternates linear layers and a nonlinearity. `TLinear` computes *x* . *W* + *b*, and `TReLU` the elementwise maximum of its argument and 0. [TGlorot]() draws weights from a normal distribution scaled by the number of inputs of the layer, and `TZeros` gives zero biases. The weights of a layer from 3 inputs to 8 hidden units:

```wl
w1 = TGlorot[{3, 8}]
```

Its biases:

```wl
b1 = TZeros[{8}]
```

The weights of a layer from the 8 hidden units to 1 output:

```wl
w2 = TGlorot[{8, 1}]
```

Its bias:

```wl
b2 = TZeros[{1}]
```

The network as a function of its input:

```wl
mlp = xIn |-> TLinear[TReLU[TLinear[xIn, w1, b1]], w2, b2]
```

A batch of one input gives one output:

```wl
TTensorShape @ TRealize @ mlp[TTensorCreate[{{1., 2., 3.}}]]
```
<!-- => {1, 1} -->

With a list of tensors, [TGrad]() differentiates with respect to each of them in one backward pass and gives the gradients in the same order. An input:

```wl
xb = TTensorCreate[{{1., 2., 3.}}]
```

Its target:

```wl
yb = TTensorCreate[{{14.}}]
```

The squared error:

```wl
loss = Total[(mlp[xb] - yb)^2]
```

The parameters:

```wl
params = {w1, b1, w2, b2}
```

Clear their gradient slots:

```wl
TClearGrad /@ params
```

The gradients with respect to all four parameters:

```wl
grads = TGrad[loss, params]
```

Computing the gradients together runs the forward pass they share once. Each gradient has the shape of its parameter:

```wl
TTensorShape /@ TRealize[grads]
```
<!-- => {{3, 8}, {8}, {8, 1}, {1}} -->

## Optimizers

`TAdam` applies one step of the Adam (adaptive moment estimation) optimizer. It takes the loss, the list of parameters, lists of first-moment and second-moment buffers of the same shapes, and the step number, computes the gradients itself and updates the parameters in place. `TZerosLike` makes zero buffers of the shapes of given tensors. A one-element parameter:

```wl
wA = TTensorCreate[{1.}]
```

Its first-moment buffer:

```wl
mA = TTensorCreate[{0.}]
```

Its second-moment buffer:

```wl
vA = TTensorCreate[{0.}]
```

One step on the loss $(w - 0.05)^2$, written with the sum of squares `TL2Loss`. The step gives the list of parameters:

```wl
TAdam[TL2Loss[wA - TTensorCreate[{0.05}]], {wA}, {mA}, {vA}, 1]
```

The first step of Adam moves the parameter by the learning rate, 0.001 by default:

```wl
Normal[wA]
```
<!-- => {0.999} -->

## Recording the training step with TJit

The loops above build the loss graph again in every step. Each new graph is scheduled and compiled again, and the kernels of every step stay in the runtime's tables, so such a loop does more work than it needs and slows down as it runs. [TJit]() avoids both: build the forward pass and the update once, over tensors that stay fixed, record the whole step once, and then, in each step, overwrite the input tensors in place with [TSet]() and replay the recording.

Fix the random seed, so that the data below is reproducible:

```wl
SeedRandom[1];
```

The data comes from a fixed random matrix applied to the input after [Ramp]():

```wl
W = RandomReal[{-1, 1}, {4, 2}]
```

A function that draws a batch of 32 random inputs and their targets:

```wl
minibatch[] := With[{xx = RandomReal[{-1, 1}, {32, 4}]}, {N @ xx, N @ Ramp[xx] . W}]
```

The weights of a layer from 4 inputs to 64 hidden units:

```wl
w1 = TGlorot[{4, 64}]
```

Its biases:

```wl
b1 = TZeros[{64}]
```

The weights of a layer from the 64 hidden units to 2 outputs:

```wl
w2 = TGlorot[{64, 2}]
```

Its biases:

```wl
b2 = TZeros[{2}]
```

The parameters:

```wl
params = {w1, b1, w2, b2}
```

A first batch:

```wl
batch = minibatch[]
```

The input tensor, which every step overwrites:

```wl
input = TTensorCreate[batch[[1]]]
```

The target tensor, which every step overwrites:

```wl
target = TTensorCreate[batch[[2]]]
```

The forward pass, built once:

```wl
forward = TLinear[TReLU[TLinear[input, w1, b1]], w2, b2]
```

It gives 2 outputs for each of the 32 inputs:

```wl
TTensorShape @ TRealize @ forward
```
<!-- => {32, 2} -->

The step computes the gradients of `TMSELoss`, the squared error summed over the batch, with respect to all parameters, then writes each updated parameter into its own buffer with `TAssign`. The gradients are computed before the update, as above. The closure records nothing until its first application:

```wl
trainStep = TJit @ Function[{},
    TRealize @ MapThread[TAssign[#1, #1 - 0.002*#2] &, {params, TRealize @ TGrad[TMSELoss[forward, target], params]}]]
```

The first application records the step and runs it:

```wl
trainStep[]
```

`TJitOpCount` gives the number of kernel dispatches recorded, for the forward pass, the backward pass and the four updates:

```wl
TJitOpCount[trainStep]
```
<!-- => 22 -->

Each later step overwrites the input and the target with a new batch and replays the recorded dispatches, which update the weights in place. A thousand steps:

```wl
Do[
    MapThread[TSet[#1, TTensorCreate[#2]] &, {{input, target}, minibatch[]}];
    trainStep[],
    {1000}]
```

The squared error on the last batch, summed over the two outputs, is down from about 28 at the start:

```wl
Round[Total @ Normal @ TRealize @ TMSELoss[forward, target], 0.01]
```
<!-- => 0.12 -->

`TAdam` with a step number computes its bias correction $1/(1 - \beta^t)$ when the graph is built, so a recorded step would keep the correction of the step it was recorded at. For a recorded step, pass two buffers of shape $\{1\}$ that hold 1 in place of the step number. Each step multiplies them by the two decay rates $\beta_1$ and $\beta_2$ inside the graph, so the correction follows the replays. A one-element parameter:

```wl
wB = TTensorCreate[{1.}]
```

Its first-moment buffer:

```wl
mB = TTensorCreate[{0.}]
```

Its second-moment buffer:

```wl
vB = TTensorCreate[{0.}]
```

The buffer for the powers of $\beta_1$, made with `TOnes`:

```wl
b1B = TOnes[{1}]
```

The buffer for the powers of $\beta_2$:

```wl
b2B = TOnes[{1}]
```

One step with the buffers:

```wl
TAdam[TL2Loss[wB - TTensorCreate[{0.05}]], {wB}, {mB}, {vB}, b1B, b2B]
```

The first step agrees with the step-number form:

```wl
Normal[wB]
```
<!-- => {0.999} -->

## Convolutional layers

`TConv2D` is a two-dimensional convolution with stride 1 and no padding. Its input has dimensions channels × height × width, possibly with a batch dimension in front; its filters have dimensions output channels × input channels × filter height × filter width; and it takes one bias for each output channel. An image of one channel of 4 × 4 pixels:

```wl
img = TTensorCreate[N @ ArrayReshape[Range[16], {1, 4, 4}]]
```

One filter of 2 × 2:

```wl
cw = TTensorCreate[N @ ArrayReshape[Range[4], {1, 1, 2, 2}]]
```

A zero bias:

```wl
cb = TTensorCreate[{0.}]
```

The convolution gives one map of 3 × 3:

```wl
Normal @ TRealize @ TConv2D[img, cw, cb]
```
<!-- => {{{44., 54., 64.}, {84., 94., 104.}, {124., 134., 144.}}} -->

The convolution is differentiable. Clear the gradient slot of the filter:

```wl
TClearGrad[cw]
```

Differentiate the sum of squares of the output:

```wl
TGrad[Total[TConv2D[img, cw, cb]^2]]
```

The gradient with respect to the filter:

```wl
Normal @ TRealize @ TGradOf[cw]
```
<!-- => {{{{12192., 13884.}, {18960., 20652.}}}} -->

A convolution followed by `TReLU` and the 2 × 2 maximum pooling `TMaxPool2d` is the front end of an image classifier.

## An image classifier

A linear layer after the convolution, the activation and the pooling gives a small classifier of 28 × 28 images into 10 classes. Eight filters of 3 × 3 over one channel:

```wl
w1 = TGlorot[{8, 1, 3, 3}]
```

Their biases:

```wl
b1 = TZeros[{8}]
```

A linear layer from the 8 pooled maps of 13 × 13 to 10 classes:

```wl
w2 = TGlorot[{8*13*13, 10}]
```

Its biases:

```wl
b2 = TZeros[{10}]
```

The classifier, for batches of 64 images:

```wl
classifier = x |-> TLinear[ArrayReshape[TMaxPool2d[TReLU[TConv2D[x, w1, b1]]], {64, 8*13*13}], w2, b2]
```

A batch of 64 random images gives 10 scores, the logits, for each image:

```wl
TTensorShape @ TRealize @ classifier[TTensorCreate[N @ RandomReal[1, {64, 1, 28, 28}]]]
```
<!-- => {64, 10} -->

`TCategoricalCrossEntropy` is the classification loss. It takes the logits, without a softmax, and a one-hot target, and it computes $\max(z) + \log \sum \exp(z - \max(z)) - \sum t z$, which stays finite when one logit is much larger than the others. Here the logit of the true class is 100 below the largest, and the loss is that difference:

```wl
First @ Normal @ TRealize @ TCategoricalCrossEntropy[TTensorCreate[{{0., 1., 100.}}], TTensorCreate[{{1., 0., 0.}}]]
```
<!-- => 100. -->

Training on handwritten digits from the MNIST (Modified National Institute of Standards and Technology) data set is the loop from the start of this tutorial, one batch of images at a time. The cells below are shown without being evaluated. A sample of 2048 training digits:

```wl
#| eval: false
data = RandomSample[ResourceData["MNIST", "TrainingData"], 2048]
```

The images as one array:

```wl
#| eval: false
images = ArrayReshape[N @ Flatten[ImageData /@ Keys[data]], {2048, 1, 28, 28}]
```

The labels as one-hot vectors:

```wl
#| eval: false
onehots = N[UnitVector[10, # + 1] & /@ Values[data]]
```

The parameters of the classifier:

```wl
#| eval: false
params = {w1, b1, w2, b2}
```

Six passes over the 32 batches of 64 images, each step computing the gradients before the update:

```wl
#| eval: false
Do[
    Do[
        batch  = TTensorCreate[images[[64*(s - 1) + 1 ;; 64*s]]];
        target = TTensorCreate[onehots[[64*(s - 1) + 1 ;; 64*s]]];
        TClearGrad /@ params;
        grads = TRealize @ TGrad[TCategoricalCrossEntropy[classifier[batch], target], params];
        MapThread[TSet[#1, #1 - 0.1*#2] &, {params, grads}],
        {s, 32}],
    {epoch, 6}]
```

The network, the cross-entropy and the backward pass through both are ordinary [TTerm]() arithmetic, differentiated by the same [TGrad]() as the dot product at the start of this tutorial.

## Training a Wolfram Language net

`TNetTrain` trains a [NetChain]() directly. Given an initialized net, a list of inputs and a list of one-hot targets, it lifts the net into a lazy graph, trains it by gradient descent on the cross-entropy, and gives an [Association]() with the lifted `"Forward"` pass, its `"Input"` tensor and the trained `"Params"`. `TNetPredict` applies the trained forward pass to new inputs of the same shape. Fix the random seed:

```wl
#| eval: false
SeedRandom[1234];
```

Eight points in the plane:

```wl
#| eval: false
points = {{1., 1.}, {1.5, 1.2}, {0.5, 0.8}, {1.2, 0.9}, {-1., -1.}, {-1.2, -0.8}, {-0.7, -1.1}, {-0.9, -1.}}
```

Their classes as one-hot vectors, the first four in one class and the last four in the other:

```wl
#| eval: false
classes = {{1., 0.}, {1., 0.}, {1., 0.}, {1., 0.}, {0., 1.}, {0., 1.}, {0., 1.}, {0., 1.}}
```

A small net that gives two logits, without a final softmax:

```wl
#| eval: false
clf = NetInitialize[NetChain[{LinearLayer[6], Ramp, LinearLayer[2]}, "Input" -> 2], RandomSeeding -> 7]
```

Train it for 80 rounds:

```wl
#| eval: false
trained = TNetTrain[clf, points, classes, MaxTrainingRounds -> 80]
```

The parts of the result:

```wl
#| eval: false
Keys[trained]
```
<!-- => {"Forward", "Input", "Params"} -->

The logits of the training points:

```wl
#| eval: false
TNetPredict[trained, points]
```
<!-- => {{1.20958, -1.1065}, {1.55489, -1.41893}, {0.864268, -0.794071}, {1.31103, -1.19885}, {-1.87919, 1.41503}, {-2.23472, 1.66808}, {-1.33729, 1.01407}, {-1.69712, 1.27782}} -->

The class with the larger logit, counted from 0, separates the two groups:

```wl
#| eval: false
(Ordering[#, -1][[1]] - 1) & /@ TNetPredict[trained, points]
```
<!-- => {0, 0, 0, 0, 1, 1, 1, 1} -->

`TNetTrain` takes the following options:

| | | |
| --- | --- | --- |
| [MaxTrainingRounds]() | 30 | the number of rounds of training |
| `"LearningRate"` | 0.05 | the size of each gradient-descent step |
| `"Method"` | `"SGD"` | stochastic gradient descent, or `"Adam"` |
| `"Momentum"` | 0. | a positive value adds momentum to the gradient-descent steps |
| `"WeightDecay"` | 0. | a positive value adds a penalty on the squares of the weights |
| `"Loss"` | [Automatic]() | the loss, by default `TCategoricalCrossEntropy`, for a net that ends in logits rather than a [SoftmaxLayer]() |
| [TargetDevice]() | `"CPU"` | the device the training runs on |

## Lifting a Wolfram Language net

`TFromNet` lifts an initialized net into a lazy graph of the same operations as the layers above, so [TRealize](), [TGrad]() and [TJit]() all apply to it. A small net:

```wl
net = NetInitialize[NetChain[{LinearLayer[8], Ramp, LinearLayer[1]}, "Input" -> 3], RandomSeeding -> 1234]
```

An input tensor:

```wl
xn = TTensorCreate[{1., 2., 3.}]
```

The lifted net applied to the input:

```wl
Normal @ TRealize @ TFromNet[net, xn]
```
<!-- => {-0.504425} -->

The net itself gives the same value:

```wl
net[{1., 2., 3.}]
```
<!-- => {-0.504425} -->

Without an input, `TFromNet` gives the net as a lambda whose variable is the input:

```wl
fn = TFromNet[net]
```

`TNetParams` gives the weights and biases of the lifted net, in the order the runtime walks them:

```wl
TTensorShape /@ TNetParams[fn]
```
<!-- => {{1}, {8}, {8, 3}, {1, 8}} -->

Applying the lambda to the input and reducing it with [TWnf]() gives the same forward pass:

```wl
Normal @ TRealize @ TWnf[fn[xn]]
```
<!-- => {-0.504425} -->

## Higher-order gradients

[TGradOf]() gives a lazy graph, so a gradient can be differentiated again. The derivative of $s^3$ is $3 s^2$, and its derivative is $6 s$. A one-element tensor:

```wl
s = TTensorCreate[{3.}]
```

Clear its gradient slot:

```wl
TClearGrad[s]
```

Differentiate $s^3$:

```wl
TGrad[Total[s^3]]
```

The first derivative at 3:

```wl
Normal @ TRealize @ TGradOf[s]
```
<!-- => {27.} -->

The first derivative as a lazy graph:

```wl
g1 = TGradOf[s]
```

Clear the gradient slot again:

```wl
TClearGrad[s]
```

Differentiate the first derivative:

```wl
TGrad[Total[g1]]
```

The second derivative at 3:

```wl
Normal @ TRealize @ TGradOf[s]
```
<!-- => {18.} -->

The backward branch of every layer is itself a lazy graph, so second derivatives pass through convolutions and `TReLU` in the same way.

## Resetting between epochs

Every realized step leaves its compiled kernels and its tensors in the runtime's tables, and scheduling a step scans those tables. In a long loop that builds its graph again in every step, the tables keep growing, and each step takes longer although it does the same work. [TReset]() empties the heap and releases the tensors and kernels of the terms on it, so the next epoch schedules against short tables. A [TTerm]() made before the reset refers to cells that later terms overwrite, so create the parameters again after a reset, from their values read with [Normal]() before it. A step recorded with [TJit]() is scheduled once, and its replays do not add to the tables.
