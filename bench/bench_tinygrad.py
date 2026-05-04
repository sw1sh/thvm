"""Tinygrad BS=512 beautiful_mnist train step bench, JIT-warm.
Mirrors examples/beautiful_mnist.py exactly; just adds timing wrapper."""
import os, time, sys
sys.path.insert(0, '/Users/swish/src/tinygrad')

from tinygrad import Tensor, TinyJit, nn, function, Device, GlobalCounters
from tinygrad.helpers import getenv
from tinygrad.nn.datasets import mnist

class Model:
  def __init__(self):
    self.layers = [
      nn.Conv2d(1, 32, 5), Tensor.relu,
      nn.Conv2d(32, 32, 5), Tensor.relu,
      nn.BatchNorm(32), Tensor.max_pool2d,
      nn.Conv2d(32, 64, 3), Tensor.relu,
      nn.Conv2d(64, 64, 3), Tensor.relu,
      nn.BatchNorm(64), Tensor.max_pool2d,
      lambda x: x.flatten(1), nn.Linear(576, 10)]

  @function
  def __call__(self, x): return x.sequential(self.layers)

  @TinyJit
  @Tensor.train()
  def train_step(self, X_train, Y_train):
    opt.zero_grad()
    samples = Tensor.randint(getenv("BS", 512), high=X_train.shape[0])
    loss = self(X_train[samples]).sparse_categorical_crossentropy(Y_train[samples]).backward()
    return loss.realize(*opt.schedule_step())

X_train, Y_train, X_test, Y_test = mnist()
model = Model()
opt = nn.optim.Adam(nn.state.get_parameters(model))

WARMUP = int(os.environ.get("WARMUP", "5"))
STEPS  = int(os.environ.get("STEPS",  "20"))
BS     = int(os.environ.get("BS",     "512"))
print(f"tinygrad device={Device.DEFAULT} BS={BS} WARMUP={WARMUP} STEPS={STEPS}")

# Warmup
for _ in range(WARMUP):
  loss = model.train_step(X_train, Y_train)
  loss.item()    # sync

# Steady measurement
Device[Device.DEFAULT].synchronize()
t0 = time.perf_counter()
for _ in range(STEPS):
  loss = model.train_step(X_train, Y_train)
Device[Device.DEFAULT].synchronize()
t1 = time.perf_counter()

ms = (t1 - t0) * 1000.0 / STEPS
print(f"tinygrad steady_ms_per_step={ms:.2f} (BS={BS} N={STEPS})")
