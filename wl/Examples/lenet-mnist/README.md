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

`TLeNet[]` builds the canonical LeNet-5 architecture (Conv 6@5x5
-> ReLU -> MaxPool 2x2 -> Conv 16@5x5 -> ReLU -> MaxPool 2x2 ->
Flatten -> Linear 120 -> ReLU -> Linear 10 -> Softmax) with
NetInitialize random weights, then forwards 5 random MNIST
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

  - `forward.wls`  -- the demo script.
  - `README.md`    -- this file.
