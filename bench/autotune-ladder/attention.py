"""Level 18 -- single multi-head self-attention block on tinygrad.

Smallest unit of the transformer family.  Configuration:
seq_len = 32, d_model = 64, n_heads = 4, d_head = 16.

Compute: Q/K/V projections (3 matmuls) + scaled dot-product
attention (Q @ K^T, softmax, attn @ V) + output projection.

Run:
    cd /Users/swish/src/tinygrad
    PYTHONPATH=. DEV=METAL python3 /Users/swish/src/thvm/bench/autotune-ladder/attention.py
"""
import math
import os, sys, time

os.environ.setdefault("DEV", "METAL")
sys.path.insert(0, "/Users/swish/src/tinygrad")

from tinygrad import Tensor, GlobalCounters, dtypes, nn


SEQ_LEN = 32
D_MODEL = 64
N_HEADS = 4
D_HEAD = D_MODEL // N_HEADS  # 16


class SelfAttention:
  def __init__(self):
    self.q = nn.Linear(D_MODEL, D_MODEL, bias=False)
    self.k = nn.Linear(D_MODEL, D_MODEL, bias=False)
    self.v = nn.Linear(D_MODEL, D_MODEL, bias=False)
    self.o = nn.Linear(D_MODEL, D_MODEL, bias=False)

  def __call__(self, x):
    # x: (S, D)
    S = x.shape[0]
    q = self.q(x).reshape(S, N_HEADS, D_HEAD).transpose(0, 1)  # (H, S, D_H)
    k = self.k(x).reshape(S, N_HEADS, D_HEAD).transpose(0, 1)
    v = self.v(x).reshape(S, N_HEADS, D_HEAD).transpose(0, 1)
    # scores: (H, S, S)
    scores = q.matmul(k.transpose(-2, -1)) * (1.0 / math.sqrt(D_HEAD))
    attn = scores.softmax(axis=-1)
    out = attn.matmul(v)  # (H, S, D_H)
    out = out.transpose(0, 1).reshape(S, D_MODEL)  # (S, D)
    return self.o(out)


def bench_with(env_overrides, label):
  for k, v in env_overrides.items():
    os.environ[k] = str(v)
  net = SelfAttention()
  x = Tensor.rand(SEQ_LEN, D_MODEL, dtype=dtypes.float32).realize()

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
