// vector_add -- replicates MLX's `binary_vv` f32 path.
// N is injected as #define K_N by score.py.  vector_add is purely
// memory-bandwidth bound -- see docs/auto_metal_kernels/agent_brief.md.
//
// MLX (external/mlx .../kernels/binary.h binary_vv) does NOT use
// float4 here: for f32 it picks work_per_thread = 8/sizeof(f32) = 2
// and emits a *scalar* 2-element loop with a 1024-thread block.  The
// Apple GPU coalesces the two consecutive scalar loads into one
// 64-bit transaction, so float4 buys nothing; 2 elem/thread keeps
// occupancy maximal.  We replicate that exactly.
#include <metal_stdlib>
using namespace metal;

#define K_NPT 2   // work per thread, matches MLX get_work_per_thread(f32)

[[kernel]] void k(
    device       float *out [[buffer(0)]],   // [N]
    device const float *a   [[buffer(1)]],   // [N]
    device const float *b   [[buffer(2)]],   // [N]
    uint index [[thread_position_in_grid]]) {

    index *= K_NPT;
    // K_N is a power of 2 (1M / 16M), divisible by K_NPT -> no tail.
    #pragma unroll
    for (uint i = 0; i < K_NPT; ++i) {
        out[index + i] = a[index + i] + b[index + i];
    }
}
