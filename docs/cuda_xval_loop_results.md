# CUDA cross-validation: thvm-CUDA vs tinygrad-CUDA vs numpy

Device: Tesla V100-SXM2-16GB (sm_70). thvm reps=200 (warmup 20); tinygrad reps=60. All GPU times are CUDA-event time (thvm: `dispatch_timed` gpu ns; tinygrad: parsed DEBUG=2 per-kernel `tm`).

| op + shape | thvm correct | thvm GPU p50/p10 (us) | thvm tuned p50 (us) | tinygrad p50/p10 (us) | tinygrad BEAM2 p50/p10 (us) | gap (thvm / tg-default) | gap (thvm / tg-BEAM2) |
|---|---|---|---|---|---|---|---|
| matmul 256x256x256 | abs=1.4e-05 rel=4.2e-02 OK | 28.67 / 27.65 | 27.65 | 41.98 / 40.96 | 22.53 / 21.50 | 0.7x (base 0.7x) | 1.2x |
| matmul 512x512x512 | abs=2.9e-05 rel=5.1e-02 OK | 182.27 / 179.20 | 152.57 | 113.66 / 112.64 | 55.30 / 55.30 | 1.3x (base 1.6x) | 2.8x |
| matmul 1024x1024x1024 | abs=8.3e-05 rel=4.3e-01 OK | 1506.30 / 1498.11 | 1121.28 | 691.20 / 690.18 | 207.87 / 205.82 | 1.6x (base 2.2x) | 5.4x |
| softmax 4096x4096 | abs=7.2e-09 rel=4.0e-06 OK | 3025.92 / 3019.78 | 376.83 | 820.23 / 818.17 | 352.26 / 350.21 | 0.5x (base 3.7x) | 1.1x |
| row_reduce 4096x4096 | abs=3.0e-04 rel=1.6e-03 OK | 826.37 / 749.57 | 80.89 | 250.88 / 248.83 | 89.09 / 89.09 | 0.3x (base 3.3x) | 0.9x |

PASS: thvm-CUDA matches numpy everywhere.

