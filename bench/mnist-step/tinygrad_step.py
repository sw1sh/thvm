#!/usr/bin/env python3
"""Warm per-step wall time + peak RSS for the Train.md LeNet-head benchmark:
Conv2d(1->8, 3x3) -> ReLU -> 2x2 max-pool -> Linear(1352->10), batch 64,
softmax cross-entropy, eager SGD. Matches thvm_step.wls.

CPU is forced (DEV=CPU): tinygrad on Metal can over-fuse the backward into a
watchdog-busting kernel and orphan the GPU on this box. Run:
    python3 tinygrad_step.py
Override the tinygrad checkout with TINYGRAD=/path/to/tinygrad.
"""
import os, sys, time, resource

os.environ.setdefault("DEV", "CPU")
_here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.environ.get("TINYGRAD", os.path.join(_here, "..", "..", "..", "tinygrad")))

from tinygrad import Tensor, nn, Device

print("device =>", Device.DEFAULT)


class Net:
    def __init__(self):
        self.c1 = nn.Conv2d(1, 8, kernel_size=3, bias=True)
        self.l1 = nn.Linear(8 * 13 * 13, 10)

    def __call__(self, x):
        x = self.c1(x).relu().max_pool2d(kernel_size=2)
        return self.l1(x.flatten(1))


Tensor.manual_seed(7)
net = Net()
params = nn.state.get_parameters(net)
opt = nn.optim.SGD(params, lr=0.1)
x = Tensor.rand(64, 1, 28, 28).realize()
y = Tensor.randint(64, low=0, high=10).realize()
Tensor.training = True


def step():
    opt.zero_grad()
    loss = net(x).sparse_categorical_crossentropy(y)
    loss.backward()
    opt.step()
    Tensor.realize(loss, *params)


for _ in range(3):  # warmup
    step()
t0 = time.perf_counter()
for _ in range(30):
    step()
dt = time.perf_counter() - t0
print("tinygrad warm ms/step (eager, B=64, conv8+relu+maxpool+linear) =>", round(1000 * dt / 30, 1))
# macOS ru_maxrss is in bytes (Linux: KiB); this run targets macOS.
print("tinygrad peak RSS MB =>", round(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1e6, 1))
