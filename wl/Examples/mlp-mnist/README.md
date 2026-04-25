# mlp-mnist

Tiny 2-layer fully-connected MNIST classifier exercised through the
thvm runtime.  Built up incrementally:

  - `forward.wls`    -- forward pass + cross-entropy loss on a
    single sample.  Smoke test for the materialize chain.
  - `grad-check.wls` -- per-weight `TGrad[loss, W]` materialization;
    asserts each gradient is finite and correctly-shaped.  Pure
    structural sanity, no parameter updates.
  - `train.wls`      -- (next task) K manual SGD steps on a fixed
    batch, asserts the loss curve trends down.  Validates the full
    backprop chain end-to-end.

## Usage

CPU (default backend):

    wolframscript -f wl/Examples/mlp-mnist/forward.wls

Metal (Apple Silicon):

    THVM_BACKEND=metal wolframscript -f wl/Examples/mlp-mnist/forward.wls

The Metal path looks for `build/default.metallib` relative to the
current working directory, so run from the repo root.

## Why this network

LeNet's the eventual goal but its convolutional + max-pool layers
have no grad rules yet (see `docs/grad-roadmap.md`).  This
fully-connected MLP exercises only operations whose grad rules ARE
implemented: Linear (MUL + REDUCE_SUM + ADD), ReLU (MUL + CMPLT),
Softmax (EXP2 + REDUCE_SUM + RECIP + EXPAND + MUL), and
CrossEntropyLoss (LOG2 + MUL + REDUCE_SUM + NEG).  If the loss
trends down here, every grad rule landed in the easy-unaries batch
is wired up correctly and we're ready to tackle REDUCE_MAX +
CONV2D for LeNet.

## Architecture

```
input {1, 28, 28}
  -> FlattenLayer        (UOP_RESHAPE to {784})
  -> LinearLayer[32]     (MUL + REDUCE_SUM + ADD)
  -> ElementwiseLayer[Ramp]  (= ReLU = MUL + CMPLT)
  -> LinearLayer[10]
  -> SoftmaxLayer        (EXP2 + REDUCE_SUM + RECIP + EXPAND + MUL)
output {10}
loss = -sum(target * log(probs))   (LOG2 + MUL + REDUCE_SUM + NEG)
```
