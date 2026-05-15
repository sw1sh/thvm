// Naive matmul baseline -- one thread per output element, scalar K loop.
// This is the starting point; replace with a tiled / simdgroup_matrix
// implementation.  Shape is injected as #define K_M / K_N / K_K by
// score.py.  See docs/auto_metal_kernels/ -- mlx_reference.md for the
// MLX steel_gemm techniques to port.
#include <metal_stdlib>
#include <metal_simdgroup_matrix>
using namespace metal;

[[kernel]] void k(
    device       float *out [[buffer(0)]],   // [M*N] row-major
    device const float *a   [[buffer(1)]],   // [M*K] row-major
    device const float *b   [[buffer(2)]],   // [K*N] row-major
    uint gid [[thread_position_in_grid]]) {

    if (gid >= (uint)(K_M * K_N)) return;
    uint row = gid / (uint)K_N;
    uint col = gid % (uint)K_N;

    float acc = 0.0f;
    for (uint kk = 0; kk < (uint)K_K; kk++) {
        acc += a[row * (uint)K_K + kk] * b[kk * (uint)K_N + col];
    }
    out[gid] = acc;
}
