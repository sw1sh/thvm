# Cross-framework BS=512 beautiful_mnist benchmarks

Three bench scripts mirroring tinygrad's `examples/beautiful_mnist.py`
architecture (conv 1->32 5, ReLU, conv 32->32 5, ReLU, BN32, maxpool,
conv 32->64 3, ReLU, conv 64->64 3, ReLU, BN64, maxpool, flatten,
linear 576->10), Adam optimizer, sparse cross-entropy loss, real
MNIST data via tinygrad's loader.

Each script: WARMUP=5 → STEPS=20 timed → divide.  All frameworks use
their JIT/compile path (TinyJit, torch.compile, mlx eager).  Python
overhead is amortised across STEPS; the GPU `synchronize()` boundary
at each end means the timing window is bounded by kernel work +
encoder dispatch, not Python loop overhead.

## Run

```bash
PY=/Users/swish/src/tinygrad/.venv/bin/python
PYTHONPATH=/Users/swish/src/tinygrad DEV=METAL BS=512 $PY bench/bench_tinygrad.py
PYTHONPATH=/Users/swish/src/tinygrad BS=512 $PY bench/bench_torch.py
PYTHONPATH=/Users/swish/src/tinygrad COMPILE=1 BS=512 $PY bench/bench_torch.py
PYTHONPATH=/Users/swish/src/tinygrad BS=512 $PY bench/bench_mlx.py
```

The thvm equivalent is the canary at
[wl/Examples/beautiful-mnist/bench-train.wls]:

```bash
BS=512 WARMUP_STEPS=5 N_STEPS=20 DEV=metal THVM_TILE=1 \
  wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls
```

Results recorded in [docs/plans/profiling_methodology.md] §4.6.
