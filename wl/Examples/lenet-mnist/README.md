# lenet-mnist

Forward-only LeNet inference on a few MNIST samples through the
thvm runtime.

## Usage

CPU (default backend):

    wolframscript -f wl/Examples/lenet-mnist/forward.wls

Metal (Apple Silicon):

    THVM_BACKEND=metal wolframscript -f wl/Examples/lenet-mnist/forward.wls

The Metal path looks for `build/default.metallib` relative to the
current working directory, so run from the repo root.

## What it shows

`NetInitialize @ NetModel["LeNet"]` returns the canonical LeCun
LeNet architecture from Mathematica's network model registry
(Conv 20@5x5 -> ReLU -> MaxPool 2x2 -> Conv 50@5x5 -> ReLU ->
MaxPool 2x2 -> Flatten -> Linear 500 -> ReLU -> Linear 10 ->
Softmax) with random weights, then forwards 5 random MNIST
training samples through `TFromNet[net, x]` and reports the
predicted digit + softmax confidence per sample.

Predictions are essentially random (~10% accuracy) -- no training
has happened.  This is the framework smoke test: end-to-end
materialize + dispatch through every layer type LeNet uses.

## Why no training (yet)

`interact_grad` currently has chain-rule rewrite rules for
ADD / MUL / NEG / REDUCE_SUM / KERNEL only.  Adam-on-LeNet needs
grad rules for at least:

  - `UOP_CONV2D`  (the heaviest; matches LeNet's two conv layers)
  - `UOP_REDUCE` with `kind = MAX`  (max-pool gradient = one-hot)
  - `UOP_RESHAPE`  (Flatten = identity-on-data, identity-on-grad)
  - `UOP_EXP2`, `UOP_RECIP`  (softmax + Adam's denom both call them)
  - propagation through CMPLT (the ReLU mask we built in NN.wl)

See `docs/grad-roadmap.md` (lands with the next task item) for
the order to land them in.

## Files

  - `forward.wls`         -- the forward demo script.
  - `grad-check.wls`      -- end-to-end forward + grad smoke test:
                             materializes the full LeNet chain
                             and takes `TGrad[loss, x]`,
                             asserting a finite, correctly-shaped
                             {1, 28, 28} gradient.
  - `grad-perweight.wls`  -- diagnostic: TGrad against each of
                             LeNet's 8 weight tensors
                             individually.  Useful for
                             localising which weight's chain
                             breaks if backprop ever regresses.
  - `train.wls`           -- 4 Adam steps on a fixed MNIST sample
                             through the full LeNet stack
                             (Conv -> ReLU -> Pool -> Conv ->
                             ReLU -> Pool -> Flatten -> Linear ->
                             ReLU -> Linear -> Softmax + CE).
                             Asserts the loss curve trends down.
                             CPU monotonically reaches loss ~0.36
                             from a starting point of ~2.61
                             (roughly ln(10) = uniform softmax);
                             Metal trains more slowly (saturation
                             through the chain on this random
                             init -- documented under the
                             grad-check Metal-vs-CPU follow-up).
  - `verify.wls`          -- end-to-end correctness check: trains
                             on one sample (4 Adam steps), then
                             asserts the trained model predicts
                             that sample's true label correctly
                             (overfit-on-one validation, since
                             LeNet doesn't generalize from 1
                             sample but a correct prediction
                             after 4 updates does prove every
                             layer's grad rule + the optimizer
                             is plumbed correctly).  Confidence
                             in true class typically goes from
                             ~0.07 (chance) to ~1.0 (lr 0.05;
                             see verify.wls comment).
  - `README.md`           -- this file.
