"""Level 20 -- 2 stacked transformer blocks on tinygrad.

Depth-scaling probe at the transformer level.  Same config
as Level 19 (seq=32, d_model=64, n_heads=4); 2 blocks
instead of 1.

Run:
    cd /Users/swish/src/tinygrad
    PYTHONPATH=. DEV=METAL python3 /Users/swish/src/thvm/bench/autotune-ladder/transformer2.py
"""
import math
import os, sys, time

os.environ.setdefault("DEV", "METAL")
sys.path.insert(0, "/Users/swish/src/tinygrad")

from tinygrad import Tensor, GlobalCounters, dtypes, nn


SEQ_LEN = 32
D_MODEL = 64
N_HEADS = 4
D_HEAD = D_MODEL // N_HEADS
D_FF   = 4 * D_MODEL


class SelfAttention:
  def __init__(self):
    self.q = nn.Linear(D_MODEL, D_MODEL, bias=False)
    self.k = nn.Linear(D_MODEL, D_MODEL, bias=False)
    self.v = nn.Linear(D_MODEL, D_MODEL, bias=False)
    self.o = nn.Linear(D_MODEL, D_MODEL, bias=False)

  def __call__(self, x):
    S = x.shape[0]
    q = self.q(x).reshape(S, N_HEADS, D_HEAD).transpose(0, 1)
    k = self.k(x).reshape(S, N_HEADS, D_HEAD).transpose(0, 1)
    v = self.v(x).reshape(S, N_HEADS, D_HEAD).transpose(0, 1)
    scores = q.matmul(k.transpose(-2, -1)) * (1.0 / math.sqrt(D_HEAD))
    attn = scores.softmax(axis=-1)
    out = attn.matmul(v)
    out = out.transpose(0, 1).reshape(S, D_MODEL)
    return self.o(out)


class TransformerBlock:
  def __init__(self):
    self.attn = SelfAttention()
    self.ln1 = nn.LayerNorm(D_MODEL)
    self.fc1 = nn.Linear(D_MODEL, D_FF)
    self.fc2 = nn.Linear(D_FF, D_MODEL)
    self.ln2 = nn.LayerNorm(D_MODEL)

  def __call__(self, x):
    x = x + self.attn(self.ln1(x))
    x = x + self.fc2(self.fc1(self.ln2(x)).gelu())
    return x


class Transformer2:
  def __init__(self):
    self.b1 = TransformerBlock()
    self.b2 = TransformerBlock()

  def __call__(self, x):
    return self.b2(self.b1(x))


def bench_with(env_overrides, label):
  for k, v in env_overrides.items():
    os.environ[k] = str(v)
  net = Transformer2()
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
