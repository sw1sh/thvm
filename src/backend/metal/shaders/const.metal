// backend/metal/shaders/const.metal -- UOP_CONST forward.
//
// CPU equivalent: backend/cpu/op/const.c.  Output buffer is filled
// with the f32 value reinterpreted from p->arg bits (passed in via
// buffer(1) as a constant uint).  One thread per output element.

#include <metal_stdlib>
using namespace metal;

kernel void thvm_const(device   float *out  [[buffer(0)]],
                       constant uint  &bits [[buffer(1)]],
                       uint            tid  [[thread_position_in_grid]])
{
    out[tid] = as_type<float>(bits);
}
