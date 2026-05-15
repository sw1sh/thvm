# matmul via raw MSL -- agent task: BEAT MLX

Goal: write a raw Metal Shading Language fp32 matmul kernel that beats
`mx.matmul` (`a @ b`) on Apple M-series GPU at:

- M=N=K = 512   (dispatch-bound regime)
- M=N=K = 1024  (MLX hits ~27% of FP32 peak here)

Read `docs/auto_metal_kernels/agent_brief.md` first, then
`docs/auto_metal_kernels/mlx_reference.md` (the `simdgroup_matrix`
section + steel_gemm pointers).

## This is the hard problem

MLX's matmul is the strongest baseline in the corpus -- it reaches
66% of FP32 peak at L=2048 (see `bench/metal-problems/matmul/RESULTS.md`).
A naive kernel sits at ~2% of peak.  The path to competitive:

1. Threadgroup tiling: cooperative load of A/B tiles into
   `threadgroup` memory, barrier, compute from shared.
2. `simdgroup_matrix<float, 8, 8>` MMA for the inner product.
3. Multi-frag accumulator: each simdgroup holds `BM/8 * BN/8` frags,
   chain `simdgroup_multiply_accumulate` over them.
4. Register-tile the accumulator; minimize threadgroup-memory
   round-trips.

Don't expect to beat MLX on iteration 1.  Expect to climb from 2% ->
20% -> 50%+ over the iteration budget.

## ABI (do not change)

```msl
[[kernel]] void k(
    device       float *out [[buffer(0)]],   // [M*N] row-major
    device const float *a   [[buffer(1)]],   // [M*K] row-major
    device const float *b   [[buffer(2)]],   // [K*N] row-major
    /* you choose the builtins: thread/threadgroup/simd positions */);
```

Shape arrives as compile-time `#define K_M`, `K_N`, `K_K` (score.py
prepends them).  Specialize tile sizes per shape if you want.

## dispatch.json

`rule` -> threadgroup total threads; `grid_rule` -> grid total
threads.  Both are Python exprs eval'd with `M N K ceildiv max min
simd tg` in scope.  Example for 32x32 output tiles, 256-thread TGs:

```json
{"rule": "256", "grid_rule": "ceildiv(M,32)*ceildiv(N,32)*256"}
```

## Iteration loop

1. Edit `kernel.metal` (one change per iteration).
2. `./score.sh 512 512 512` and `./score.sh 1024 1024 1024`.
3. Read the score block, append a row to `RESULTS.md`.

## Score output

8 lines.  `speedup_gpu` is the headline (GPU-time vs GPU-time);
`speedup_wall` is the legacy wall metric.  When they disagree the
kernel is dispatch-bound -- trust `speedup_gpu`.

## Stop conditions

- `speedup_gpu >= 1.05x` median across 3 fresh runs at BOTH shapes, OR
- 12 iterations.

If you stop without 1.05x, that's fine -- matmul is hard.  Log every
iteration; a climb from 2% to 40% of peak is a useful run even if it
doesn't pass MLX.

## Final report

Write `RESULTS.md`: iteration log table, final kernel + dispatch,
3-run variance, GFLOPS + % of FP32 peak at each shape (M3 Max FP32
peak ~14.2 TFLOPS), one surprising MLX-source finding, follow-ups.
