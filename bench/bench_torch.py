"""PyTorch MPS BS=512 beautiful_mnist train step bench.
Architecture identical to tinygrad's beautiful_mnist.py."""
import os, time, sys, torch
sys.path.insert(0, '/Users/swish/src/tinygrad')
from torch import nn, optim
from tinygrad.nn.datasets import mnist
from tinygrad import dtypes

class Model(nn.Module):
  def __init__(self):
    super().__init__()
    self.c1 = nn.Conv2d(1, 32, 5)
    self.c2 = nn.Conv2d(32, 32, 5)
    self.bn1 = nn.BatchNorm2d(32)
    self.m1 = nn.MaxPool2d(2)
    self.c3 = nn.Conv2d(32, 64, 3)
    self.c4 = nn.Conv2d(64, 64, 3)
    self.bn2 = nn.BatchNorm2d(64)
    self.m2 = nn.MaxPool2d(2)
    self.lin = nn.Linear(576, 10)
  def forward(self, x):
    x = nn.functional.relu(self.c1(x))
    x = nn.functional.relu(self.c2(x))
    x = self.m1(self.bn1(x))
    x = nn.functional.relu(self.c3(x))
    x = nn.functional.relu(self.c4(x))
    x = self.m2(self.bn2(x))
    return self.lin(torch.flatten(x, 1))

device = torch.device("mps") if torch.backends.mps.is_available() else torch.device("cpu")
X_train, Y_train, X_test, Y_test = mnist()
X_train = torch.tensor(X_train.float().numpy(), device=device)
Y_train = torch.tensor(Y_train.cast(dtypes.int64).numpy(), device=device)

model = Model().to(device)
optimizer = optim.Adam(model.parameters())
loss_fn = nn.CrossEntropyLoss()

BS     = int(os.environ.get("BS",     "512"))
WARMUP = int(os.environ.get("WARMUP", "5"))
STEPS  = int(os.environ.get("STEPS",  "20"))
COMPILE = os.environ.get("COMPILE", "0") == "1"

def step(samples):
  X, Y = X_train[samples], Y_train[samples]
  out = model(X)
  loss = loss_fn(out, Y)
  optimizer.zero_grad()
  loss.backward()
  optimizer.step()
  return loss

if COMPILE:
  step = torch.compile(step)

print(f"torch device={device} BS={BS} WARMUP={WARMUP} STEPS={STEPS} compile={COMPILE}")

# Warmup
for _ in range(WARMUP):
  samples = torch.randint(0, X_train.shape[0], (BS,), device=device)
  loss = step(samples)
  loss.item()    # sync

# Steady
torch.mps.synchronize()
t0 = time.perf_counter()
for _ in range(STEPS):
  samples = torch.randint(0, X_train.shape[0], (BS,), device=device)
  step(samples)
torch.mps.synchronize()
t1 = time.perf_counter()

ms = (t1 - t0) * 1000.0 / STEPS
print(f"torch steady_ms_per_step={ms:.2f} (BS={BS} N={STEPS})")
