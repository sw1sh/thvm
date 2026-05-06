"""Level 16 -- 2-layer MLP forward (784 -> 128 -> 10) on tinygrad.

Run:
    cd /Users/swish/src/tinygrad
    PYTHONPATH=. DEV=METAL python3 /Users/swish/src/thvm/bench/autotune-ladder/mlp2.py
"""
import os, sys, time

os.environ.setdefault("DEV", "METAL")
sys.path.insert(0, "/Users/swish/src/tinygrad")

from tinygrad import Tensor, GlobalCounters, dtypes, nn


class MLP2NoSoftmax:
  def __init__(self):
    self.l1 = nn.Linear(784, 128)
    self.l2 = nn.Linear(128, 10)

  def __call__(self, x):
    return self.l2(self.l1(x).relu())


def bench_with(env_overrides, label):
  for k, v in env_overrides.items():
    os.environ[k] = str(v)
  net = MLP2NoSoftmax()
  x = Tensor.rand(784, dtype=dtypes.float32).realize()

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
