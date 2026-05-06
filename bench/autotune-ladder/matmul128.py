"""Level 2 -- matmul (M=N=K=128) on tinygrad.

Compares NOOPT baseline vs BEAM=4 best.

Run:
    cd /Users/swish/src/tinygrad
    PYTHONPATH=. DEV=METAL python3 /Users/swish/src/thvm/bench/autotune-ladder/matmul128.py
"""
import os, sys, time

os.environ.setdefault("DEV", "METAL")
sys.path.insert(0, "/Users/swish/src/tinygrad")

from tinygrad import Tensor, GlobalCounters, dtypes


def bench_with(env_overrides, label):
  for k, v in env_overrides.items():
    os.environ[k] = str(v)
  a = Tensor.rand(128, 128, dtype=dtypes.float32).realize()
  b = Tensor.rand(128, 128, dtype=dtypes.float32).realize()

  # Warmup -- pays the BEAM search / NOOPT compile cost.
  GlobalCounters.reset()
  t0 = time.perf_counter_ns()
  for _ in range(3):
    (a @ b).realize()
  t1 = time.perf_counter_ns()
  warmup_us = (t1 - t0) / 3 / 1000

  # Steady -- kernel cached after warmup.
  GlobalCounters.reset()
  t0 = time.perf_counter_ns()
  for _ in range(50):
    (a @ b).realize()
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
