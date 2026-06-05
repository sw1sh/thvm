# GPT2 on thvm

tinygrad's [`examples/gpt2.py`](https://github.com/tinygrad/tinygrad/blob/master/examples/gpt2.py)
(255 lines) run against thvm's Python bridge with **only the imports swapped**
(`from tinygrad import ...` -> `from thvm import ...`).  See
[`thvm_gpt2.py`](thvm_gpt2.py); every line below the import block is
byte-identical to upstream gpt2.py.  The one non-frontend import,
`extra.bench_log` (a tinygrad-repo extra, not part of the tinygrad frontend
surface), is satisfied by the local [`bench_log.py`](bench_log.py) shim.

The model loads the real HuggingFace GPT2 checkpoint
(`pytorch_model.bin`), runs inference, and generates text.

## Run

```sh
# CPU (correct + validated)
PYTHONPATH=py:bench/gpt2 DEV=cpu JIT=0 python bench/gpt2/thvm_gpt2.py \
    --model_size gpt2-medium --prompt "Hello." --count 10 --temperature 0
# -> Hello. I'm a little late to the party, but
#    output validated
```

`JIT=0` is required: thvm's `Variable` recompiles per concrete value
(it has no symbolic-shape kernel sharing yet), and thvm's `TinyJit`
replay freezes the first decode step's data, so the JIT single-token
path produces garbage.  gpt2.py's own `JIT and ...` gate honours this.

## Generated text (real, greedy / temperature 0, gpt2-medium)

thvm reproduces tinygrad's gpt2.py **token-for-token** on the model's
built-in validation prompts (CPU):

| prompt | output (thvm CPU == tinygrad) |
|---|---|
| `What is the answer to life, the universe, and everything?` | `…?\n\nThe answer is that we are all one` |
| `Hello.` | `Hello. I'm a little late to the party, but` |

gpt2-small, greedy, longer sample (thvm CPU):

> What is the answer to life, the universe, and everything?
> The answer to life, the universe, and everything is that we are all
> connected.

## Benchmark (gpt2-small, M3 Max, JIT=0, greedy)

Two phases measured separately:
* **prompt-forward** = the fixed forward over a 13-token prompt (gpt2.py's
  `--benchmark` shape).
* **decode** = per-token single-step generation reading the kv-cache.

| backend | phase | thvm | tinygrad | ratio |
|---|---|---:|---:|---:|
| CPU   | prompt-forward (13 tok) | 2039 ms | 299 ms | 6.8x |
| CPU   | decode (ms/tok, min)    | 423 ms  | 297 ms | 1.4x |
| Metal | prompt-forward (13 tok) | 379 ms  | 214 ms | 1.8x |
| Metal | decode (ms/tok)         | **incorrect** (see below) | 262 ms | - |

Notes:
* thvm CPU **decode** (423 ms/tok min) is the competitive number -- within
  1.4x of tinygrad.  It grows mildly with context (405 -> 600 ms over 15
  steps) because each `start_pos` value triggers a thvm recompile
  (`Variable` = recompile-per-value); tinygrad reuses one symbolic kernel
  and stays flat.
* thvm CPU **prompt-forward** is slow (6.8x) because thvm re-schedules the
  whole graph on every call (no warm-schedule cache); tinygrad caches the
  schedule.  This is a thvm scheduling-cache gap, not a kernel-speed gap.
* thvm **Metal prompt-forward** (379 ms) is correct and 5.4x faster than
  thvm CPU, within 1.8x of tinygrad Metal.  No GPU orphaning observed
  (forward only, gpt2-small).

### Known issue: Metal decode is incorrect

The Metal **prompt-forward** first token matches CPU exactly, but the
**single-token decode step** diverges on Metal (CPU `little` vs Metal
`up` for "Hello, I am a"), producing garbage continuations.  The
host-side kv-cache scatter and the symbolic token-embed shrink are both
verified correct on Metal in isolation, so this is a Metal-codegen bug in
the multi-layer decode-shape forward (attention over the cached kv at the
1-query decode shape).  CPU is fully correct.  Filed for the Metal-codegen
fix workflow; not worked around here.

## CUDA

This box is Apple Silicon (no CUDA).  Per thvm's CUDA pod (V100, sm_70),
run there:

```sh
# on the laptop: sync the worktree's src + py to the pod
rsync -az --delete src/ py/ <pod>:/root/thvm/
# on the pod:
cd /root/thvm && make && make py
PYTHONPATH=py:bench/gpt2 DEV=cuda JIT=0 python bench/gpt2/thvm_gpt2.py \
    --model_size gpt2-medium --prompt "Hello." --count 10 --temperature 0
# tinygrad reference on the same pod:
PYTHONPATH=/root/tinygrad DEV=CUDA JIT=0 /root/tgvenv/bin/python \
    /root/tinygrad/examples/gpt2.py --model_size gpt2-medium \
    --prompt "Hello." --count 10 --temperature 0
```

## Files

* `thvm_gpt2.py` -- gpt2.py with imports swapped to thvm (logic identical).
* `bench_log.py` -- local BenchEvent/WallTimeEvent shim (tinygrad extra).
* `bench.py`     -- ms/token + tokens/sec harness.
