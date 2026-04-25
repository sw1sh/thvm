// backend/metal/shaders/placeholder.metal -- no-op kernel.
//
// Exists so the metallib build rule has something to compile until
// the real per-op shaders land (CONST is next).  The kernel is a
// trivial pass-through; the test only checks that the library
// loads and the function name is discoverable.

#include <metal_stdlib>
using namespace metal;

kernel void thvm_placeholder(device float *out [[buffer(0)]],
                             uint            tid [[thread_position_in_grid]])
{
    out[tid] = 0.0f;
}
