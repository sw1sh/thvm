"""Level 10 -- mini-LeNet (1 conv-block + linear + softmax) on tinygrad.

Mirrors bench/autotune-ladder/mini_lenet.wls so kernel_count
comparison is apples-to-apples.

Run:
    cd /Users/swish/src/tinygrad
    PYTHONPATH=. DEV=METAL python3 /Users/swish/src/thvm/bench/autotune-ladder/mini_lenet.py
"""
import os, sys, time

os.environ.setdefault("DEV", "METAL")
sys.path.insert(0, "/Users/swish/src/tinygrad")

from tinygrad import Tensor, GlobalCounters, dtypes, nn


class MiniLeNet:
  def __init__(self):
    self.c1 = nn.Conv2d(1, 6, 5)
    # After Conv(6, 5x5) with 28x28 input -> 24x24, Pool 2x2 -> 12x12;
    # 6 * 12 * 12 = 864 features.
    self.l1 = nn.Linear(6 * 12 * 12, 10)

  def __call__(self, x):
    x = self.c1(x).relu().max_pool2d(kernel_size=(2, 2))
    x = x.flatten(0)
    return self.l1(x).softmax()


def bench_with(env_overrides, label):
  for k, v in env_overrides.items():
    os.environ[k] = str(v)
  net = MiniLeNet()
  x = Tensor.rand(1, 1, 28, 28, dtype=dtypes.float32).realize()

  GlobalCounters.reset()
  t0 = time.perf_counter_ns()
  for _ in range(3):
    net(x).realize()
  t1 = time.perf_counter_ns()
  warmup_us = (t1 - t0) / 3 / 1000

  GlobalCounters.reset()
  t0 = time.perf_counter_ns()
  for _ in range(50):
    net(x).realize()
  t1 = time.perf_counter_ns()
  steady_us = (t1 - t0) / 50 / 1000

  print(f"{label}_warmup_us:  {warmup_us:.1f}")
  print(f"{label}_steady_us:  {steady_us:.1f}")
  print(f"{label}_kernel_count_post: {GlobalCounters.kernel_count}")
  print(f"{label}_global_ops_post:   {GlobalCounters.global_ops}")
  return steady_us


baseline_us = bench_with({"NOOPT": "1", "BEAM": "0"}, "baseline")
beam_us     = bench_with({"NOOPT": "0", "BEAM": "4"}, "beam4")

print(f"speedup_baseline_to_beam4: {baseline_us / beam_us:.3f}x")
