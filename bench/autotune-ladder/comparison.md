# Autotune Ladder: thvm vs tinygrad on Equivalent UOp Graphs

5 levels of increasing complexity, each benched on Apple M3 Max under
`DEV=METAL`. Captured 2026-05-06. Per-level details and raw outputs
in `bench/autotune-ladder/<level>.{txt,tinygrad.txt}`. Plan and
discussion in [docs/plans/autotune_beam_profile.md](../../docs/plans/autotune_beam_profile.md).

## Headline table

| level | shape                          | thvm kernels | thvm best (us) | thvm speedup | tinygrad kernels | tinygrad best (us) | tinygrad speedup | absolute winner   |
|-------|--------------------------------|-------------:|---------------:|-------------:|-----------------:|-------------------:|-----------------:|-------------------|
| 1     | elementwise add (N=1024)       |            1 |            146 |        2.05x |                1 |                391 |            1.23x | **thvm** (2.7x)   |
| 2     | matmul (M=N=K=128)             |            1 |            211 |        1.01x |                1 |                773 |            1.18x | **thvm** (3.7x)   |
| 3     | softmax (N=512)                |            3 |            558 |        1.12x |                3 |               1141 |            1.09x | **thvm** (2.0x)   |
| 4     | MLP2 (784 -> 128 -> 10)        |            7 |           1420 |        1.13x |                5 |               2499 |            1.09x | **thvm** (1.8x)   |
| 5     | conv2d (1x32x28x28, 5x5, 32)   |            3 |           2264 |        1.58x |                1 |               1604 |            0.99x | **tinygrad** (-29%)|

## Notes on apples-to-oranges

- thvm "best" is the sum of per-kernel `TKernelBenchUs` (100 reps each, GPU-only, no Python overhead).
- tinygrad "best" is the wall time of one Python `realize()` call (50-rep average), includes Python + dispatch overhead (~300 us per call).
- For levels 1-4, thvm's absolute "wins" are partly a measurement-mode artefact: TKernelBenchUs benches the kernel direct; tinygrad pays Python wrapper cost.
- Level 5 is the cleanest comparison because both frameworks hit a real GPU ceiling and the gap is 41% in tinygrad's favour despite the Python overhead.

## Findings

### Per-kernel autotune is at parity

Both frameworks extract roughly 1.0–1.6x from per-kernel tuning, with thvm slightly ahead at extremes (2.05x on N=1024 elementwise; 1.58x on conv2d). Tinygrad's BEAM=4 finds 0.99–1.23x consistently. **Neither framework's autotune is the bottleneck on the workloads tested.**

### thvm leaks kernels at every level above the trivial

| level | thvm kernels | tinygrad kernels | excess |
|-------|-------------:|-----------------:|-------:|
| 1     |            1 |                1 |    0   |
| 2     |            1 |                1 |    0   |
| 3     |            3 |                3 |    0   |
| 4     |            7 |                5 | **+2** |
| 5     |            3 |                1 | **+2** |

At MLP2 + conv2d, thvm dispatches 2 extra kernels per forward. Suspected leak sites:
- **Softmax tail**: an unfused metal-op outlier (kid 6 in MLP2, kid 2 in softmax) that doesn't lift through `kernel_lift_to_uop` and breaks the reduce-broadcast collapse.
- **LinearLayer + activation boundary**: tinygrad fuses the matmul + activation; thvm bufferizes between them.
- **Conv2D im2col + reduce + bias chain**: tinygrad emits 1 kernel; thvm emits 3 (im2col patch-sum, matmul-style reduce, bias-broadcast).

### Autotune is on the wrong unit

Conv2d is the cleanest illustration. thvm's autotune extracts **1.58x** from its 3-kernel chain — the biggest win in the ladder. Tinygrad's BEAM=4 finds **0.99x** because the default-heuristic kernel is already at its local optimum. Yet **tinygrad's untuned single kernel (1604 us) beats thvm's tuned 3-kernel chain (2264 us) by 41%**.

Per-kernel tuning saturates around 1.5–2.0x; fusing N kernels into 1 unlocks an N-fold dispatch-overhead win independent of tuning. **Structural fusion has higher leverage than autotune**, especially at compute-heavy shapes like conv2d.

## Next focus

The level where thvm falls furthest behind tinygrad in absolute wall is **level 5 (conv2d, -29%)**. The mechanism is structural fusion, not autotune. Concrete leverage points, ordered by ROI:

1. **Fuse the conv2d patch-sum + reduce + bias-broadcast chain** into one kernel on the metal-tile path. Targets the biggest absolute gap (level 5). Expected win: ≥30% on conv-heavy workloads (LeNet, beautiful_mnist).
2. **Lift the softmax tail (metal-op outlier)** through `kernel_lift_to_uop`. Targets level 3 / 4. Expected win: 10–20% on softmax-tail kernels via reduce-broadcast collapse.
3. **Fuse LinearLayer + activation boundary**. Targets level 4. Expected win: ~15% on MLP / transformer feed-forward chains.

Per-kernel autotune work (extending the proposer, deepening the search beyond 8 LOCAL widths) has lower ROI until 1, 2, 3 land — at which point the kernels we tune are bigger and the autotune surface widens naturally.
