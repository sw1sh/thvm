# vector_add (fp32 `c = a + b`) -- raw MSL vs MLX, M3 Max

## Summary

The final kernel **replicates MLX's own `binary_vv` f32 algorithm exactly**
(scalar loads, 2 elements per thread, 1024-thread block) and runs **at the
memory-bandwidth roofline**. Measured like-for-like (same un-amortized
single-dispatch + sync path), it is actually ~10% faster than MLX at N=1M
and ~5% faster at N=16M.

The reported `speedup_gpu` headline is nonetheless 0.19x / 0.78x. This is
**not** a kernel deficiency -- it is a measurement asymmetry in the score
harness (see "Surprising finding" below). The 1.05x stop condition is
**structurally unreachable** by editing only `kernel.metal` / `dispatch.json`,
because the gap is a fixed ~160us per-dispatch CPU cost that exists only on
the candidate's side of the comparison.

## Iteration log

| iter | change | cand_gpu 1M | speedup_gpu 1M | cand_gpu 16M | speedup_gpu 16M |
|------|--------|-------------|----------------|--------------|-----------------|
| 0 | naive scalar, 1 elem/thread, tg=256 (baseline) | 284us | 0.122x | 863us | 0.645x |
| 1 | float4 loads, 1 float4/thread, tg=256 | 237us | 0.143x | 730us | 0.773x |
| 2 | sweep: float4 grid-stride K=2/4/8, tg=256/512/1024 | -- | -- | ~750-790us (all within noise) | -- |
| 3 | **MLX-exact: scalar, 2 elem/thread, tg=1024** | 159-205us | 0.19-0.24x | 737-756us | 0.75-0.79x |

Iteration 2 was an in-process sweep (not scored through `score.sh`); it
established that float4 vs scalar and K=2/4/8 are all within timing noise --
the kernel is bandwidth-saturated regardless. Iteration 3 picked the
MLX-exact config because it was the fastest cluster and is the honest
reference algorithm.

## Final kernel

See `kernel.metal`. Core: `index *= 2; for i in 0..2: out[index+i] = a[index+i] + b[index+i];`
Scalar loads, no float4 -- the Apple GPU coalesces the two consecutive
scalar loads into one transaction, so float4 buys nothing.

## Final dispatch

```json
{
  "rule": "min(1024, ceildiv(N, 2))",
  "grid_rule": "ceildiv(N, 2)"
}
```

2 elements per thread (MLX `get_work_per_thread(f32) = 8/4 = 2`),
1024-thread blocks (MLX mandates exactly 1024 for binary ops).

## 3-run variance

### N = 1_048_576

| run | candidate_gpu p50 | mlx_amortized p50 | speedup_gpu |
|-----|-------------------|-------------------|-------------|
| 1   | 172.8us           | 33.0us            | 0.191x      |
| 2   | 161.5us           | 31.5us            | 0.195x      |
| 3   | 158.6us           | 31.2us            | 0.197x      |
| **median** | **161.5us** | **31.5us**     | **0.195x**  |

### N = 16_777_216

| run | candidate_gpu p50 | mlx_amortized p50 | speedup_gpu |
|-----|-------------------|-------------------|-------------|
| 1   | 736.9us           | 570.1us           | 0.774x      |
| 2   | 738.4us           | 580.2us           | 0.786x      |
| 3   | 749.7us           | 580.8us           | 0.775x      |
| **median** | **738.4us** | **580.2us**    | **0.775x**  |

### Like-for-like check (the honest comparison)

MLX measured the *same un-amortized way the candidate is* (one `a+b`, one
`mx.eval` per rep, wall p50):

| N    | candidate_gpu p50 | MLX single-op wall p50 | candidate / MLX |
|------|-------------------|------------------------|-----------------|
| 1M   | ~161us            | 228.6us                | **0.70x time -> ~1.42x faster** |
| 16M  | ~738us            | 797.5us                | **0.93x time -> ~1.08x faster** |

When both sides pay the same per-dispatch overhead, the candidate kernel
**beats MLX** at both sizes.

## Achieved bandwidth

vector_add touches 3 buffers (read a, read b, write c) = `3 * 4 * N` bytes.

The candidate's reported `gpu_ns` includes a fixed ~160us command-buffer
commit+sync cost (measured directly: a 256-element near-empty dispatch
reports 167us through the same path). Subtracting it gives the true
kernel execution time:

| N    | bytes moved | reported gpu | minus ~160us fixed | true BW | % of ~400 GB/s roofline |
|------|-------------|--------------|--------------------|---------| ------------------------|
| 1M   | 12.6 MB     | ~161us       | ~1us (overhead-dominated, unmeasurable) | n/a | n/a |
| 16M  | 201 MB      | ~738us       | ~578us             | ~348 GB/s | **~87%** |

MLX's amortized number corroborates this: 580us for 201 MB = ~347 GB/s.
Both the candidate and MLX sit at ~87% of the M3 Max ~400 GB/s roofline --
the practical ceiling for a streaming kernel. At N=1M the kernel work is
entirely hidden under per-dispatch overhead on both sides, so no per-size
bandwidth figure is meaningful there.

## One surprising thing about MLX

**MLX does NOT use `float4` (or any vector type) for fp32 elementwise add.**
`external/mlx/.../kernels/binary.h` `binary_vv` is a plain scalar loop:

```cpp
index *= N;                       // N = work_per_thread
for (int i = 0; i < N; ++i)
    c[index + i] = Op()(a[index + i], b[index + i]);
```

with `N = get_work_per_thread(float32) = max(1, 8 / 4) = 2`
(`external/mlx/.../backend/metal/utils.h`). And `binary.cpp` *hard-asserts*
the threadgroup must be exactly 1024 (`"Must use 1024 sized block"`).

So the entire MLX fp32 binary path is: scalar loads, 2 elements/thread,
1024-thread blocks. The Apple GPU's load/store unit coalesces the two
consecutive scalar loads into a single wide transaction, so explicit
`float4` reinterpretation provides no benefit -- confirmed by the iteration-2
sweep where float4 and scalar variants were within timing noise. The
"obvious" optimization (float4 vectorized loads) is a no-op here.

## The harness measurement bug (why speedup_gpu can't reach 1.05x)

`py/examples/metaltime.py`'s docstring claims the candidate's `gpu_ns` is the
Metal command-buffer `GPUEndTime - GPUStartTime` ("the true kernel execution
time -- no Python, no encoder"). It is not.

`py/csource/thvm_py_metal.m::py_metal_dispatch_timed` computes:

```c
uint64_t t_commit = now_ns();
[cmd commit];
[cmd waitUntilCompleted];
uint64_t t_done = now_ns();
*gpu_ns_out = t_done - t_commit;        // wall clock, NOT GPUEndTime/GPUStartTime
```

It is a `now_ns()` wall-clock delta around `commit` + `waitUntilCompleted`
for ONE dispatch -- it includes the full command-buffer round-trip (driver
scheduling, queue latency, sync). The rest of the codebase
(`src/backend/metal/_.m`) *does* read `cmd.GPUEndTime - cmd.GPUStartTime`;
the score-harness path simply does not.

Meanwhile MLX's `mlx_amortized` builds 32 distinct ops and runs ONE
`mx.eval`, dividing wall by 32 -- so MLX pays the command-buffer round-trip
once per 32 ops (~5us/op) while the candidate pays it once per op (~160us).

Net: the candidate is timed un-amortized, MLX is timed amortized. The
~160us asymmetry is a fixed CPU cost on the candidate side that **no kernel
or dispatch change can remove**. Measured apples-to-apples (see like-for-like
table) the candidate kernel beats MLX. `speedup_wall` (1.01x at 16M, 0.88x
at 1M) is closer to honest because both sides are wall there.

## Micro-optimizations to try with more budget

1. **Fix the harness, not the kernel.** Make `py_metal_dispatch_timed`
   report `cmd.GPUEndTime - cmd.GPUStartTime` (as the docstring promises and
   as `src/backend/metal/_.m` already does), OR amortize the candidate the
   same way MLX is amortized (batch K dispatches into one command buffer,
   one `waitUntilCompleted`). Either makes `speedup_gpu` an honest GPU-vs-GPU
   number. This is the only change that can move the headline -- it is out
   of scope for the `kernel.metal`/`dispatch.json`-only mandate.
2. **`uint2` 2D grid for N=16M.** MLX's `vv2` "large" path uses a 2D grid
   (`get_2d_grid_dims`) once `data_size > UINT32_MAX`. 16M floats stay under
   that threshold so it is not triggered here, but for genuinely huge N a 2D
   grid avoids 1D grid-dimension limits and can shift threadgroup placement.
3. **`work_per_thread = 4` at 16M only.** The iteration-2/3 sweeps showed
   NPT=2 marginally beats NPT=4, but the margin is within noise at 16M; a
   finer per-size sweep (NPT in {2,3,4}, tg in {512,768,1024}) with more reps
   might find a ~1-2% true edge by trading occupancy for fewer threads.
