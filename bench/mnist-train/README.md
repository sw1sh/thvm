# mnist-train

MNIST trained **from scratch** on the THVMLink tensor surface, end to end in
one `TNetTrain` call, with a NetDecoder-decoded example-picture prediction.

```
wolframscript -file train_predict.wls          # CPU
DEV=metal wolframscript -file train_predict.wls # Metal GPU
```

A conv8 LeNet head (`Conv2d[1->8,3x3]` -> ReLU -> 2x2 max-pool -> `Linear[10]`)
is randomly initialised, then trained by the sugared one-liner
`TNetTrain[net, images, oneHot, "MaxTrainingRounds"->250, "LearningRate"->0.2]`
(realize-grads-first SGD over a 512-image batch). `TNetPredict` +
`NetDecoder[{"Class", Range[0,9]}]` turn the logits into digit labels.

Result (Apple-silicon, CPU): **~87% test accuracy on 512 held-out digits in
~17 s**. The script writes `prediction.png` (10 test digits with their decoded
predictions; green = correct). More data / a second conv layer lifts accuracy;
this is the minimal from-scratch demo, matched to the `../mnist-step` benchmark
net so the speed numbers there apply.

The full-training loop is also documented (as a hand-built variant) in the
[Train tutorial](../../wl/THVMLink/docs/Tutorials/Train.md) "A convolutional
MNIST classifier" section; this directory is the runnable, saved version.
