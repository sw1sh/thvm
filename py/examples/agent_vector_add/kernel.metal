// Naive vector_add baseline -- one thread per element, scalar load/store.
// Starting point; replace with a float4-vectorized grid-stride kernel.
// N is injected as #define K_N by score.py.  vector_add is purely
// memory-bandwidth bound -- see docs/auto_metal_kernels/agent_brief.md.
#include <metal_stdlib>
using namespace metal;

[[kernel]] void k(
    device       float *out [[buffer(0)]],   // [N]
    device const float *a   [[buffer(1)]],   // [N]
    device const float *b   [[buffer(2)]],   // [N]
    uint gid [[thread_position_in_grid]]) {

    if (gid >= (uint)K_N) return;
    out[gid] = a[gid] + b[gid];
}
