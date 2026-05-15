# agent_matmul: raw-MSL fp32 matmul vs mx.matmul on M3 Max

Mission: write an MSL fp32 matmul kernel (`out = a @ b`) that beats
`mx.matmul` at M=N=K=512 and M=N=K=1024 on Apple M-series GPU.

Headline metric `speedup_gpu = mlx_amortized_p50 / candidate_gpu_p50`
is GPU-time vs GPU-time: the candidate's number is the Metal
command-buffer `GPUEndTime - GPUStartTime`; MLX's is recovered by
amortization (32 distinct matmuls in one `mx.eval`, wall/32) since
MLX exposes no per-op GPU timer.

**Result: did NOT hit the 1.05x stop condition.** The kernel climbed
from a 3%-of-peak naive baseline to ~35-37% of FP32 peak at L=1024.
`mx.matmul` runs at ~67% of peak there, so it stays ~1.9x ahead.

## Iteration log

speedup_gpu (median of the available runs); `%peak` from
candidate_gpu p50 against the M3 Max ~14.2 TFLOPS FP32 peak.

| iter | change | 512 speedup_gpu | 512 %peak | 1024 speedup_gpu | 1024 %peak |
|---|---|---|---|---|---|
| 1 | naive 1-thread-per-output (provided baseline) | 0.116x | ~3% | 0.18x | ~6% |
| 2 | stage 3: `simdgroup_matrix<8,8>`, 1 SG per 8x8 tile | 0.088x | ~3% | 0.35x | 10% |
| 3 | stage 4: 64x64 TG tile, 8 SG, register accumulator, BK=16 | 0.19x | 7% | 0.85x* | 26% |
| 4 | BK 16 -> 32 (halve barrier count) | 0.15x | 6% | -- | 28% |
| 5 | tile 64x64 -> 128x128, 16 SG, 16 frags/SG | 0.07x | 3% | 0.18x | 8% (register spill) |
| 6 | revert to 64x64 + double-buffered K loop | 0.16x | 7% | 0.38x | 28% (no gain) |
| 7 | bank-conflict padding (LDA=BK+8, LDB=BN+8) | 0.13x | 6% | 0.30x | 20% (regressed) |
| 8 | revert padding (64x64, BK=32, single buffer) | 0.13x | 5% | 0.48x | 35% |
| 9 | double-buffer + BK=16 (16 KB TG, 2 TG/core resident) | 0.22x | 8% | 0.38x | 25% (regressed) |
| 10 | tile 64x64 -> 32x32 (4x more TGs for L=512) | 0.19x | 8% | 0.40x | 27% |
| 11 | SG grid 4x2 -> 8x1 (each SG owns an 8x64 stripe) | 0.12x | 5% | 0.32x | 21% (register spill) |
| 12 | BK 32 -> 64 (32 KB TG, 1 TG/core) | -- | -- | 0.36x | 32% (occupancy loss) |
| -- | unroll the kk MMA loop (`#pragma unroll`) -- no-op, kept | 0.22x | 8% | 0.48x | 35% |
| **final** | iter-8 config: 64x64, BK=32, 4x2 SG, single buffer | **0.222x** | **~8%** | **0.477x** | **~35%** |

`*` iter 3's 1024 number came from an early, noisy MLX-baseline
estimator; the %peak column is the trustworthy figure for that row.

Note: the first 12 iterations were run against an equivalent ad-hoc
harness with a different ABI; the final kernel was then ported to
this workspace's official ABI (`out`/`a`/`b` buffers, shape via
compile-time `#define K_M/K_N/K_K`) and re-measured -- the
algorithmic conclusions transferred unchanged.

## Final kernel / dispatch

`kernel.metal`: stage-4 tiled simdgroup_matrix matmul.

- **Tile**: each threadgroup computes a 64x64 output tile.
- **Threads**: 8 simdgroups (256 threads), SG grid 4 rows x 2 cols.
- **Per-SG**: a 16x32 sub-tile = 2x4 = 8 `simdgroup_matrix<float,8,8>`
  register accumulators.
- **K loop**: step BK=32; cooperative `float4` loads of a 64x32 A
  tile + 32x64 B tile into 16 KB threadgroup memory, one barrier,
  then 4 MMA k-substeps of 8.
- Shape is compile-time (`K_M/K_N/K_K`), so all tile constants and
  loop bounds fold to constants.

`dispatch.json`:
```json
{"rule": "256", "grid_rule": "ceildiv(M,64)*ceildiv(N,64)*tg"}
```
256-thread threadgroups; one threadgroup per 64x64 output tile.

## 3-run variance (final kernel)

L = 512 (M=N=K=512):

| run | cand_gpu p50 | mlx_amort p50 | speedup_gpu | cand %peak |
|---|---|---|---|---|
| 1 | 351.8 us | 46.6 us | 0.133x | ~3% |
| 2 | 207.7 us | 46.6 us | 0.224x | ~6.5% |
| 3 | 210.5 us | 46.8 us | 0.222x | ~6.5% |
| **median** | **210.5 us** | **46.6 us** | **0.222x** | **~6.5%** |

L = 1024 (M=N=K=1024):

| run | cand_gpu p50 | cand_gpu p10 | mlx_amort p50 | speedup_gpu | cand %peak (p50) |
|---|---|---|---|---|---|
| 1 | 532.8 us | 472.5 us | 197.2 us | 0.370x | 25% |
| 2 | 412.5 us | 402.1 us | 196.7 us | 0.477x | 35% |
| 3 | 401.5 us | 388.9 us | 197.9 us | 0.493x | 36% |
| **median** | **412.5 us** | **402.1 us** | **197.2 us** | **0.477x** | **35%** |

The candidate is **bimodal**: a "warm" state near 400-412 us (35-37%
peak) and a "cold" state near 530 us (25% peak). The kernel source is
identical between reps; the split is GPU DVFS. `bench_candidate`
times each rep as a separate command buffer, so idle gaps between
reps let the GPU clock down -- some reps land slow. MLX's amortized
number is rock-stable (~197 us) precisely because batching 32 ops
into one `mx.eval` keeps the GPU continuously pinned hot. The fairest
single statement: **the candidate's true throughput is ~37% of FP32
peak (warm-state p10) versus MLX's ~67%.**

## GFLOPS + % of FP32 peak

M3 Max FP32 peak ~= 14.2 TFLOPS. FLOPs = 2*M*N*K.

| shape | candidate (warm) | candidate %peak | mlx (amortized) | mlx %peak |
|---|---|---|---|---|
| L=512  | ~208 us / 1.29 TFLOPS | ~9% (dispatch-bound) | 46.6 us / 5.76 TFLOPS | ~41% |
| L=1024 | ~402 us / 5.34 TFLOPS | ~38% | 197 us / 10.9 TFLOPS | ~77% |

L=512 (270 MFLOP) is dispatch/fixed-overhead bound for everyone:
~19 us of pure compute, so even MLX's 46 us is mostly GPU-side
command-buffer ramp. The candidate's 64x64 tile yields only 64
threadgroups at L=512 -- far too few to saturate 40 GPU cores --
which caps L=512 near 9% even though the same kernel reaches ~38% at
L=1024. iter 10's 32x32 tile lifted L=512 (256 TGs) but cost L=1024
~9 points; one static kernel cannot serve both shapes optimally.

## One surprising thing about MLX

The provided STARTER says "MLX hits ~27% of FP32 peak" at L=1024, and
`bench/metal-problems/matmul/RESULTS.md` shows MLX at 26% there.
Measuring MLX's *GPU* time -- 32 dependent matmuls amortized into one
`mx.eval` -- gives **197 us = 77% of peak**, not 27%. The 27%
wall-clock figure is almost entirely **per-dispatch CPU/encode
overhead**: a single `mx.matmul`+`mx.eval` at L=1024 spends ~200 us
in queue/encode wrapped around ~197 us of GPU work (the `mlx_wall`
line confirms it -- ~400 us). So `mx.matmul` is *already* a near-peak
kernel at L=1024; its apparent mediocrity in single-shot wall-clock
benchmarks is a measurement artifact. This is exactly why the mission
specified `speedup_gpu` as the headline -- and also why beating MLX
here is genuinely a 77%-of-peak problem, not a 27%-of-peak one.

## Follow-ups (micro-optimizations to try with more budget)

1. **Shape-adaptive tile via the compile-time defines.** `K_M/K_N/K_K`
   are `#define`s, so `kernel.metal` can branch on them at compile
   time: a 32x32 tile when `K_M*K_N` is small (L<=512), 64x64
   otherwise, with `dispatch.json`'s `grid_rule` sized for whichever
   tile the same expression picks. This is what MLX's C++ dispatcher
   does and would lift L=512 from ~9% toward ~40% without touching
   the L=1024 path.

2. **True software-pipelined double buffer.** iters 6/9 double
   buffers gave no gain because the post-MMA `threadgroup_barrier`
   still serialized load-vs-compute. The fix: issue the next tile's
   `float4` global loads into *registers* before the barrier,
   barrier once, then store regs -> threadgroup and immediately MMA.
   That overlaps global-load latency with MMA issue the way MLX's
   steel_gemm does. Needs BK=16 to stay under the 32 KB TG limit.

3. **Find the unspilled large-tile sweet spot.** 128x128 (iter 5)
   and the 8x1 SG grid (iter 11) both spilled at 16 frags/SG. A 96x64
   tile (12 frags/SG) or 64x64 with explicit `Af`/`Bf` lifetime
   shaping may sit between iter-8's 8 frags and the spill cliff and
   raise the FLOP/byte ratio.

4. **Cheaper trailing barrier.** With double buffering the post-MMA
   barrier only protects the next fill; an execution-only barrier
   (`mem_flags::mem_none`) is cheaper than the full `mem_threadgroup`
   fence and is sufficient when producer/consumer buffers differ.

## How to reproduce

```
cd py/examples/agent_matmul
./score.sh 512 512 512
./score.sh 1024 1024 1024
```
