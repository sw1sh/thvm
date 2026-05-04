"""MLX BS=512 beautiful_mnist train step bench."""
import os, time, sys
sys.path.insert(0, '/Users/swish/src/tinygrad')
import mlx.core as mx
import mlx.nn as nn
import mlx.optimizers as optim
from tinygrad.nn.datasets import mnist
from tinygrad import dtypes
import numpy as np

class Model(nn.Module):
  def __init__(self):
    super().__init__()
    self.c1  = nn.Conv2d(1, 32, 5)
    self.c2  = nn.Conv2d(32, 32, 5)
    self.bn1 = nn.BatchNorm(32)
    self.m1  = nn.MaxPool2d(2)
    self.c3  = nn.Conv2d(32, 64, 3)
    self.c4  = nn.Conv2d(64, 64, 3)
    self.bn2 = nn.BatchNorm(64)
    self.m2  = nn.MaxPool2d(2)
    self.lin = nn.Linear(576, 10)
  def __call__(self, x):
    x = nn.relu(self.c1(x))
    x = nn.relu(self.c2(x))
    x = self.m1(self.bn1(x))
    x = nn.relu(self.c3(x))
    x = nn.relu(self.c4(x))
    x = self.m2(self.bn2(x))
    x = mx.flatten(x, start_axis=1)
    return self.lin(x)

X_train, Y_train, _, _ = mnist()
X_np = X_train.float().numpy().transpose(0, 2, 3, 1).astype(np.float32)
Y_np = Y_train.cast(dtypes.int64).numpy().astype(np.int32)
X_train_m = mx.array(X_np); mx.eval(X_train_m)
Y_train_m = mx.array(Y_np); mx.eval(Y_train_m)

model = Model()
mx.eval(model.parameters())
optimizer = optim.Adam(learning_rate=1e-3)

BS     = int(os.environ.get("BS",     "512"))
WARMUP = int(os.environ.get("WARMUP", "5"))
STEPS  = int(os.environ.get("STEPS",  "20"))

def loss_fn(model, X, Y):
  return nn.losses.cross_entropy(model(X), Y, reduction="mean")

loss_and_grad_fn = nn.value_and_grad(model, loss_fn)

@mx.compile
def step(X, Y, *params):
  # Re-bind model parameters from explicit args so mx.compile sees pure inputs.
  pass  # placeholder; using non-compile path below

def train_step(X, Y):
  loss, grads = loss_and_grad_fn(model, X, Y)
  optimizer.update(model, grads)
  return loss

print(f"mlx device=metal BS={BS} WARMUP={WARMUP} STEPS={STEPS}")

n_train = X_train_m.shape[0]
np.random.seed(42)
for _ in range(WARMUP):
  idx = mx.array(np.random.randint(0, n_train, BS).astype(np.int32))
  loss = train_step(X_train_m[idx], Y_train_m[idx])
  mx.eval(loss, model.parameters(), optimizer.state)

mx.synchronize()
t0 = time.perf_counter()
for _ in range(STEPS):
  idx = mx.array(np.random.randint(0, n_train, BS).astype(np.int32))
  loss = train_step(X_train_m[idx], Y_train_m[idx])
mx.eval(loss, model.parameters(), optimizer.state)
mx.synchronize()
t1 = time.perf_counter()

ms = (t1 - t0) * 1000.0 / STEPS
print(f"mlx steady_ms_per_step={ms:.2f} (BS={BS} N={STEPS})")
