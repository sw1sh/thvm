#!/usr/bin/env python3
"""tinygrad reference for the LeNet Metal tutorial: the SAME architecture as
Wolfram NetModel["LeNet"] -- Conv2d(1->20, 5x5) -> ReLU -> 2x2 max-pool ->
Conv2d(20->50, 5x5) -> ReLU -> 2x2 max-pool -> Flatten -> Linear(800->500) ->
ReLU -> Linear(500->10), trained from scratch on a small MNIST subset by
full-batch SGD, then evaluated on a held-out test set.

Pairs with lenet_metal_tutorial.wls (thvm side) for a same-box, same-net,
same-backend (Metal) accuracy + warm-step timing comparison.  Run:
    DEV=METAL python3 lenet_tinygrad.py
Override the tinygrad checkout with TINYGRAD=/path/to/tinygrad.
"""
import os, sys, time

os.environ.setdefault("DEV", "METAL")
_here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.environ.get("TINYGRAD", os.path.join(_here, "..", "..", "..", "tinygrad")))

from tinygrad import Tensor, nn, Device
from tinygrad.nn.datasets import mnist

print("device =>", Device.DEFAULT)

N_TRAIN = int(os.environ.get("N_TRAIN", "256"))
N_TEST = int(os.environ.get("N_TEST", "256"))
ROUNDS = int(os.environ.get("ROUNDS", "60"))
LR = float(os.environ.get("LR", "0.1"))


class LeNet:
    def __init__(self):
        self.c1 = nn.Conv2d(1, 20, kernel_size=5, bias=True)
        self.c2 = nn.Conv2d(20, 50, kernel_size=5, bias=True)
        self.l1 = nn.Linear(800, 500)
        self.l2 = nn.Linear(500, 10)

    def __call__(self, x):
        x = self.c1(x).relu().max_pool2d(kernel_size=2)
        x = self.c2(x).relu().max_pool2d(kernel_size=2)
        return self.l2(self.l1(x.flatten(1)).relu())


Tensor.manual_seed(7)
X_train, Y_train, X_test, Y_test = mnist()
xb = (X_train[:N_TRAIN].float() / 255.0).realize()
yb = Y_train[:N_TRAIN].realize()
xt = (X_test[:N_TEST].float() / 255.0).realize()
yt = Y_test[:N_TEST]

net = LeNet()
params = nn.state.get_parameters(net)
opt = nn.optim.SGD(params, lr=LR)
Tensor.training = True


def step():
    opt.zero_grad()
    loss = net(xb).sparse_categorical_crossentropy(yb)
    loss.backward()
    opt.step()
    Tensor.realize(loss, *params)


for _ in range(3):  # warmup (compiles the fixed kernel set)
    step()
t0 = time.perf_counter()
for _ in range(ROUNDS):
    step()
dt = time.perf_counter() - t0
print("tinygrad LeNet warm ms/round (full-batch SGD, bs%d) =>" % N_TRAIN, round(1000 * dt / ROUNDS, 1))

Tensor.training = False
preds = net(xt).argmax(axis=1).numpy()
acc = float((preds == yt.numpy()).mean())
print("tinygrad LeNet held-out test acc (%d imgs, %d rounds) =>" % (N_TEST, ROUNDS), round(acc, 3))
