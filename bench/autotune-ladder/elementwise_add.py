"""Level 1 -- elementwise add (N=1024) on tinygrad.

Compares NOOPT baseline vs BEAM=4 best.  Reports per-iter wall_us,
plus the kernel count and BEAM search overhead.

Run:
    cd /Users/swish/src/tinygrad
    PYTHONPATH=. DEV=METAL python3 /Users/swish/src/thvm/bench/autotune-ladder/elementwise_add.py
"""
import os, sys, time

# Force the search to populate the disk cache cleanly.
os.environ.setdefault("DEV", "METAL")
sys.path.insert(0, "/Users/swish/src/tinygrad")

from tinygrad import Tensor, GlobalCounters, dtypes


def bench_with(env_overrides, label):
  for k, v in env_overrides.items():
    os.environ[k] = str(v)
  # Re-import ops so env changes take effect for the kernel build.
  # tinygrad reads BEAM via getenv at scheduler call time so a single
  # process can switch modes; reset GlobalCounters between runs.
  a = Tensor.rand(1024, dtype=dtypes.float32).realize()
  b = Tensor.rand(1024, dtype=dtypes.float32).realize()

  # Warmup: pay the BEAM search cost (or NOOPT compile cost).
  GlobalCounters.reset()
  t0 = time.perf_counter_ns()
  for _ in range(3):
    (a + b).realize()
  t1 = time.perf_counter_ns()
  warmup_us = (t1 - t0) / 3 / 1000

  # Bench: kernel cached after warmup.
  GlobalCounters.reset()
  t0 = time.perf_counter_ns()
  for _ in range(50):
    (a + b).realize()
  t1 = time.perf_counter_ns()
  steady_us = (t1 - t0) / 50 / 1000

  print(f"{label}_warmup_us:  {warmup_us:.1f}")
  print(f"{label}_steady_us:  {steady_us:.1f}")
  print(f"{label}_kernel_count_post: {GlobalCounters.kernel_count}")
  print(f"{label}_global_ops_post:   {GlobalCounters.global_ops}")
  return steady_us


# 1) baseline: NOOPT -- no kernel optimisation, just default layout.
baseline_us = bench_with({"NOOPT": "1", "BEAM": "0"}, "baseline")

# 2) BEAM=4: tinygrad's hill-climb beam search picks an opt sequence.
beam_us = bench_with({"NOOPT": "0", "BEAM": "4"}, "beam4")

print(f"speedup_baseline_to_beam4: {baseline_us / beam_us:.3f}x")
