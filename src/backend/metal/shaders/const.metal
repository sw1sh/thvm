// backend/metal/shaders/const.metal -- UOP_CONST forward.
//
// CPU equivalent: backend/cpu/op/const.c.  Output buffer is filled
// with the f32 / i32 value reinterpreted from p->arg bits (passed
// in via buffer(1) as a constant uint).  One thread per output
// element.  Per-dtype variants suffixed _f32 / _i32.

#include <metal_stdlib>
using namespace metal;

kernel void thvm_const_f32(device   float *out  [[buffer(0)]],
                           constant uint  &bits [[buffer(1)]],
                           uint            tid  [[thread_position_in_grid]])
{
    out[tid] = as_type<float>(bits);
}

kernel void thvm_const_i32(device   int   *out  [[buffer(0)]],
                           constant uint  &bits [[buffer(1)]],
                           uint            tid  [[thread_position_in_grid]])
{
    out[tid] = (int)bits;
}
