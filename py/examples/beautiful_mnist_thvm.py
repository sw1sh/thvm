#!/usr/bin/env python3
# tinygrad's examples/beautiful_mnist.py, run on thvm by swapping ONLY the
# imports: `tinygrad` -> `thvm`.  Everything the script needs now lives in
# the thvm package (nn.optim, nn.datasets.mnist, Tensor.randint / fancy
# indexing / sparse_categorical_crossentropy / argmax / train(), helpers
# getenv/colored/trange, GlobalCounters, function).
#
#   DEV=cuda STEPS=70 BS=512 python3 py/examples/beautiful_mnist_thvm.py
#
# NOTE: thvm's py frontend is eager (no JIT) and does not yet reclaim GPU
# buffers across steps, so a long run grows memory and a 16GB GPU
# saturates after a handful of steps -- see the commit message / docs.
# model based off https://medium.com/data-science/going-beyond-99-mnist-handwritten-digits-recognition-cfff96337392
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo py/ on path

from typing import Callable
from thvm import Tensor, TinyJit, nn, GlobalCounters, function
from thvm.helpers import getenv, colored, trange
from thvm.nn.datasets import mnist

class Model:
  def __init__(self):
    self.layers: list[Callable[[Tensor], Tensor]] = [
      nn.Conv2d(1, 32, 5), Tensor.relu,
      nn.Conv2d(32, 32, 5), Tensor.relu,
      nn.BatchNorm(32), Tensor.max_pool2d,
      nn.Conv2d(32, 64, 3), Tensor.relu,
      nn.Conv2d(64, 64, 3), Tensor.relu,
      nn.BatchNorm(64), Tensor.max_pool2d,
      lambda x: x.flatten(1), nn.Linear(576, 10)]

  @function
  def __call__(self, x:Tensor) -> Tensor: return x.sequential(self.layers)

  @TinyJit
  @Tensor.train()
  def train_step(self, X_train:Tensor, Y_train:Tensor) -> Tensor:
    opt.zero_grad()
    samples = Tensor.randint(getenv("BS", 512), high=X_train.shape[0])
    loss = self(X_train[samples]).sparse_categorical_crossentropy(Y_train[samples]).backward()
    return loss.realize(*opt.schedule_step())

  @TinyJit
  def get_test_acc(self, X_test:Tensor, Y_test:Tensor) -> Tensor: return (self(X_test).argmax(axis=1) == Y_test).mean()*100

if __name__ == "__main__":
  X_train, Y_train, X_test, Y_test = mnist(fashion=getenv("FASHION"))

  model = Model()
  opt = (nn.optim.Muon if getenv("MUON") else nn.optim.SGD if getenv("SGD") else nn.optim.Adam)(nn.state.get_parameters(model))

  test_acc = float('nan')
  for i in (t:=trange(getenv("STEPS", 70))):
    GlobalCounters.reset()   # NOTE: this makes it nice for DEBUG=2 timing
    loss = model.train_step(X_train, Y_train)
    if i%10 == 9: test_acc = model.get_test_acc(X_test, Y_test).item()
    t.set_description(f"loss: {loss.item():6.2f} test_accuracy: {test_acc:5.2f}%")

  # verify eval acc
  if target := getenv("TARGET_EVAL_ACC_PCT", 0.0):
    if test_acc >= target and test_acc != 100.0: print(colored(f"{test_acc=} >= {target}", "green"))
    else: raise ValueError(colored(f"{test_acc=} < {target}", "red"))
