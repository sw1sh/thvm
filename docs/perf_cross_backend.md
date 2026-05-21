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
| 512  | Metal (pre-fix)  | 30.03  | 8.9  | 202.475 |
| 512  | Metal (parallel TC) | 0.339 | 793 | 202.475 |
| 512  | CUDA (thvm naive)  | 0.225  | 1195 | 202.475 |
| 1024 | CPU (Accelerate) | 0.745  | 2883 | 407.601 |
| 1024 | Metal (pre-fix)  | 137.6  | 15.6 | 407.601 |
| 1024 | Metal (parallel TC) | 1.05 | 2053 | 407.601 |
| 1024 | CUDA (thvm naive)  | 1.81   | 1187 | 407.601 |

All backends produce identical results (correctness confirmed).

Original finding (pre-fix): the Metal matmul reached only 9-16 GFLOPS vs
the V100's ~1190. The kernel DID emit Apple `simdgroup_matrix` (8x8
tensor-core) ops, but the entire computation ran in a SINGLE simdgroup of
a single threadgroup (`if (sgi == 0u && tg == 0u)`), looping over every
output tile serially while the rest of the GPU idled.

Fix (parallel TC): hand_opts now promotes the matmul's 8-multiple output
(M/N) axes from LOOP to GLOBAL after applying the TC marker, and
cg_tile_metal_dispatch_shape sizes the grid as product(extent/8)
threadgroups x 32 threads -- one simdgroup per 8x8 output tile, across the
whole GPU (render_uop.c's parallel_tc branch). Metal matmul-1024 goes
15.6 -> 2053 GFLOPS (~130x), now ahead of the V100 CUDA path and
approaching CPU Accelerate. Non-8-multiple shapes keep the guarded serial
path (correct, slow); only Metal is affected (hand_opts is Metal-gated,
CPU matmul uses Accelerate).

Remaining: the CUDA fp32 matmul still renders the tiled-scalar fallback
(V100 WMMA is fp16-only); a CUDA-side parallelisation of its output tiles
is the next gap.

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
