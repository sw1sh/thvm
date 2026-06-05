"""Benchmark thvm GPT2 generation: ms/token + tokens/sec.

Usage: DEV=cpu|metal PYTHONPATH=py:bench/gpt2 python bench/gpt2/bench.py \
         [--model_size gpt2] [--count 30] [--warmup 3]

Times the per-token generation loop (weight load excluded).  Greedy
(temperature 0) for determinism.  Reports min/median/mean step time."""
import argparse
import sys
import time

import numpy as np

import thvm_gpt2 as G
from thvm import Tensor, Variable, Device

p = argparse.ArgumentParser()
p.add_argument("--model_size", default="gpt2")
p.add_argument("--count", type=int, default=30)
p.add_argument("--warmup", type=int, default=3)
p.add_argument("--prompt", default="What is the answer to life, the universe, and everything?")
args = p.parse_args()

print(f"backend={Device.DEFAULT} model={args.model_size} count={args.count}")
gpt2 = G.GPT2.build(args.model_size)
enc = gpt2.tokenizer
m = gpt2.model

toks = enc.encode(args.prompt)
start_pos = 0
step_times = []
generated = toks[:]
total = args.count + args.warmup
for step in range(total):
    if start_pos == 0:
        tk = Tensor([generated])
        sp = 0
    else:
        tk = Variable("t", 0, G.VOCAB_SIZE - 1).bind(generated[start_pos])
        sp = start_pos
    t0 = time.perf_counter()
    nxt = m(tk, Variable("sp", 1 if start_pos else 0, G.MAX_CONTEXT - 1).bind(sp),
            0.0).tolist()[0]
    dt = (time.perf_counter() - t0) * 1e3
    if step >= args.warmup:
        step_times.append(dt)
    start_pos = len(generated)
    generated.append(nxt)

st = np.array(step_times)
print(f"steps timed: {len(st)} (after {args.warmup} warmup)")
print(f"  min    {st.min():.1f} ms/tok  ({1000/st.min():.1f} tok/s)")
print(f"  median {np.median(st):.1f} ms/tok  ({1000/np.median(st):.1f} tok/s)")
print(f"  mean   {st.mean():.1f} ms/tok  ({1000/st.mean():.1f} tok/s)")
print(f"text: {enc.decode(generated)!r}")
