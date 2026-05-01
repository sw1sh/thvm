# lenet-mnist

Forward-only LeNet inference on a few MNIST samples through the
thvm runtime.

## Usage

CPU (default backend):

    wolframscript -f wl/Examples/lenet-mnist/forward.wls

Metal (Apple Silicon):

    THVM_BACKEND=metal wolframscript -f wl/Examples/lenet-mnist/forward.wls

Autotune a bounded sample of LeNet kernels:

    THVM_TILE=1 wolframscript -f wl/Examples/lenet-mnist/autotune.wls

Full LeNet autotune sweep:

    THVM_TILE=1 MAX_TUNE_KERNELS=All wolframscript -f wl/Examples/lenet-mnist/autotune.wls

Benchmark single-sample LeNet training with baseline vs bounded
autotune:

    THVM_TILE=1 wolframscript -f wl/Examples/lenet-mnist/bench-train.wls

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

`autotune.wls` uses the same forward pass as a concrete autotuning
example.  It materializes LeNet once, lists kernels with
`TKernelProposed` candidates, optionally prints measured
`TKernelVariants`, applies `TKernelAutotune` to the selected kernels,
then reruns the forward pass and checks the softmax probabilities stay
within tolerance.  The default tunes the first 16 candidate kernels so
the script remains interactive; set `MAX_TUNE_KERNELS=All` for the
full sweep.

`bench-train.wls` runs the lazy LeNet/Adam training step from
`train.wls` with a separate warmup phase, optional bounded
`TKernelAutotune`, and timed training steps.  Use
`TRAIN_BENCH_MODE=baseline|autotune|both`, `N_STEPS`,
`WARMUP_STEPS`, and `MAX_TUNE_KERNELS` to control the run.

## Training status

`train.wls`, `verify.wls`, and `bench-train.wls` exercise the current
lazy LeNet/Adam training path end to end on one MNIST sample.  The
benchmark is intentionally single-sample for now so kernel generation,
autotune, and dispatch timing stay visible while the wider tiling
pipeline is still evolving.

## Files

  - `forward.wls`         -- the forward demo script.
  - `autotune.wls`        -- bounded LeNet autotune walkthrough:
                             materialize, inspect proposer
                             candidates, tune, rerun, and compare
                             probabilities.
  - `bench-train.wls`     -- LeNet training benchmark with warmup,
                             optional bounded autotune, per-step
                             timing, losses, and dispatch summary.
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
