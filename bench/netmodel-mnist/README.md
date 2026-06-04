# netmodel-mnist

Training real Wolfram / tinygrad architectures from scratch on MNIST through
the THVMLink tensor surface, and comparing accuracy.

```
wolframscript -file lenet_train.wls            # NetModel["LeNet"], as-is
wolframscript -file beautiful_mnist_train.wls  # tinygrad beautiful_mnist arch
```

| Architecture | thvm-trained (from scratch, CPU) | reference |
|---|---|---|
| **LeNet** (`NetModel["LeNet"]`, lifted as-is) | ~89.6% (256 imgs, full-batch, 150 iters) | pretrained `"LeNet Trained on MNIST Data"`: 99.0% (60k) |
| **beautiful_mnist** (exact arch, BatchNorm) | ~95.1% (1024 imgs, mini-batch BS=64, 8 epochs) | tinygrad full-data run: ~99.5% |

Both architectures train end to end on the tensor surface. The from-scratch
runs trail the full-60k references because they see small subsets; the deeper
BatchNorm net (beautiful_mnist) beats the shallower LeNet, as expected.

## Two TFromNet capabilities this exercised

- **LeNet lifts as-is** — the only catch is that `NetModel["LeNet"]` is the
  *uninitialised* architecture (and on an older WL than the model's version,
  its weights are `Automatic`/empty). `NetInitialize` gives concrete random
  weights on the same NetModel (= "from scratch", not a rebuild); TFromNet
  then lifts it directly. The Image encoder / Class decoder are input/output
  *ports* TFromNet never folds over -- they're handled host-side
  (`ImageData` in, argmax / `NetDecoder` out).
- **BatchNorm** -- `fromLayer[BatchNormalizationLayer]` -> `TBatchNormTrain`
  (batch-statistics normalisation + learned Scaling/Biases), so the
  beautiful_mnist arch lifts with all 14 params trainable.

## Metal

Train these on **CPU**. The beautiful_mnist backward over-fuses into 1300+
kernels and can orphan the Metal GPU on dispatch (reboot to clear) -- on both
thvm and tinygrad. Prescreen `TKernelCount[]` on `DEV=cpu` and keep it well
under a few hundred before any `DEV=metal` training step. (The small conv8
head in `../mnist-step` is ~40 kernels and is Metal-safe.)
