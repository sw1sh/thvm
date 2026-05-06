"""Level 5 -- single conv2d (1x32x28x28 input, 5x5, 32 c_out) on tinygrad.

Run:
    cd /Users/swish/src/tinygrad
    PYTHONPATH=. DEV=METAL python3 /Users/swish/src/thvm/bench/autotune-ladder/conv2d.py
"""
import os, sys, time

os.environ.setdefault("DEV", "METAL")
sys.path.insert(0, "/Users/swish/src/tinygrad")

from tinygrad import Tensor, GlobalCounters, dtypes, nn


def bench_with(env_overrides, label):
  for k, v in env_overrides.items():
    os.environ[k] = str(v)
  conv = nn.Conv2d(32, 32, 5)
  x = Tensor.rand(1, 32, 28, 28, dtype=dtypes.float32).realize()

  GlobalCounters.reset()
  t0 = time.perf_counter_ns()
  for _ in range(3):
    conv(x).realize()
  t1 = time.perf_counter_ns()
  warmup_us = (t1 - t0) / 3 / 1000

  GlobalCounters.reset()
  t0 = time.perf_counter_ns()
  for _ in range(50):
    conv(x).realize()
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
