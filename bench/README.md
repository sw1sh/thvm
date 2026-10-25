# Benchmarks

thvm against tinygrad, PyTorch and MLX. The Python scripts take tinygrad from
the sibling checkout `../tinygrad` (put it on `PYTHONPATH`, or set `TINYGRAD`
where a script reads it); the thvm ones need `make py` and `PYTHONPATH=py`.

| Benchmark | What it times |
| --- | --- |
| [bench_train.py](bench_train.py) | thvm's warm train step of the simple 2-conv net and beautiful_mnist (`MODEL`, `BS`, `NITER`, `DEV`): warm min and mean, loss, peak device bytes. The milestone check. |
| [bench_tinygrad_beautiful_mnist.py](bench_tinygrad_beautiful_mnist.py) | tinygrad's beautiful_mnist step on a fixed batch, with its warmup, capture and replay counters, for the same table. |
| `bench_tinygrad.py`, `bench_torch.py`, `bench_mlx.py` | The BS=512 beautiful_mnist step (tinygrad's `examples/beautiful_mnist.py` net, Adam, sparse cross-entropy, real MNIST) per framework: WARMUP=5, then STEPS=20 timed, each on its JIT or compile path. |
| [mlp_bench/](mlp_bench/README.md) | The warm (JIT-replayed) train step of a real-size MLP on CPU: thvm, tinygrad and PyTorch. |
| [gpt2/](gpt2/README.md) | tinygrad's `examples/gpt2.py` on thvm's Python package, only the imports swapped. |
| [mnist-step/](mnist-step/README.md), [mnist-train/](mnist-train/README.md), [netmodel-mnist/](netmodel-mnist/README.md) | MNIST steps and training from the Wolfram side (`NetTrain`, LeNet, beautiful_mnist) beside tinygrad. |
| [xbackend_bench.c](xbackend_bench.c) | Raw kernel dispatch on CPU, Metal and CUDA (`make bin/xbackend_bench`). |
| [dsv2_decode_bench.c](dsv2_decode_bench.c) | The DeepSeek-V2-Lite BS=1 decode step (`make bin/dsv2_decode_bench`); its weight pack comes from `tools/dsv2/dsv2_pack_weights.py`. |

## Run

```bash
make py
PYTHONPATH=py DEV=cpu MODEL=beautiful BS=16 NITER=5 python3 bench/bench_train.py
PYTHONPATH=../tinygrad DEV=METAL BS=512 python3 bench/bench_tinygrad.py
PYTHONPATH=../tinygrad BS=512 python3 bench/bench_torch.py          # COMPILE=1 for torch.compile
PYTHONPATH=../tinygrad BS=512 python3 bench/bench_mlx.py
```

The Wolfram-side canary is `wl/Examples/beautiful-mnist/bench-train.wls`:

```bash
BS=512 WARMUP_STEPS=5 N_STEPS=20 DEV=metal THVM_TILE=1 \
  wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls
```
