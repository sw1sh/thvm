# Cross-backend performance: CPU vs Metal vs CUDA

Raw kernel-dispatch microbench of thvm's own codegen across its three
backends. The same hand-written UOp DAG (matmul, elementwise, row-reduce)
is wrapped in a synthetic `KernelEntry` and dispatched in a steady-state
loop drained by one `buf_read`. Backend is selected at runtime with
`DEV={cpu,metal,cuda}`. This compares thvm's generated kernels, not vendor
BLAS, except that the CPU backend's dispatch ladder tries Accelerate
(`cblas`) first on macOS for matmul-shaped DAGs.

Harness: [tools/xbackend_bench.c](../tools/xbackend_bench.c). Build with
`make bin/xbackend_bench` (links Metal on macOS, CUDA on Linux+CUDA).
Run: `DEV=<dev> ./bin/xbackend_bench <matmul|ew|reduce> <n> <reps>`.

Hardware: Apple M3 Max (CPU + Metal); NVIDIA Tesla V100-SXM2-16GB (CUDA,
Prime Intellect pod, sm_70).

## matmul C[n,n] = A[n,n] @ B[n,n]  (2 n^3 FLOPs)

| n    | backend | ms/iter | GFLOPS | out[0] |
|------|---------|---------|--------|--------|
| 512  | CPU (Accelerate) | 0.125  | 2147 | 202.475 |
| 512  | Metal (thvm naive) | 30.03  | 8.9  | 202.475 |
| 512  | CUDA (thvm naive)  | 0.225  | 1195 | 202.475 |
| 1024 | CPU (Accelerate) | 0.745  | 2883 | 407.601 |
| 1024 | Metal (thvm naive) | 137.6  | 15.6 | 407.601 |
| 1024 | CUDA (thvm naive)  | 1.81   | 1187 | 407.601 |

All three backends produce identical results (correctness confirmed).

Key finding: for the SAME thvm-generated naive matmul kernel, the V100
CUDA path reaches ~1190 GFLOPS while the M3 Max Metal path manages only
9-16 GFLOPS, a ~75-130x gap. The Metal time scales linearly with n^3
(512->1024 is 8x compute for ~4.6x time), so it is compute-bound in the
kernel, not dispatch-overhead-bound: the Metal matmul codegen emits a
naive per-thread scalar K-loop with no threadgroup-memory tiling, while
the CUDA naive kernel coalesces acceptably on the V100's wide memory bus.
The CPU number is Accelerate BLAS, not thvm codegen, so it is not directly
comparable to the two GPU naive kernels.

Optimization target: thvm's Metal matmul codegen (threadgroup tiling /
SIMD-group cooperation) is the largest single cross-backend performance
gap.

## row-reduce out[r] = sum_c in[r,c]  (n x n)

| n   | backend | ms/iter | out[0] |
|-----|---------|---------|--------|
| 512 | CPU (scalar walker) | 4.20  | 254.500 |
| 512 | Metal | 0.266 | 254.500 |
| 512 | CUDA  | 0.114 | 254.500 |

The CPU reduce uses the scalar UOp walker (no BLAS pattern), so both GPUs
win comfortably. On Linux (no Accelerate) the CPU matmul also falls to the
walker and is ~2.4 s at n=256, which is why the pod CPU matmul is omitted
above (it is the same scalar interpreter, not a meaningful comparison).

## Notes

- The CUDA fp32 matmul renders the tiled-scalar fallback (the V100's WMMA
  is fp16-only), so even the CUDA number is not using tensor cores.
- Pod image now ships the full CUDA 12.4 toolkit (`/usr/local/cuda-12.4`,
  `nvcc`, `libnvrtc`), not the driver-only image the older setup notes
  describe. `make bin/xbackend_bench` builds straight away.
- `test_cuda_backend` passes 46/46 on the V100.
