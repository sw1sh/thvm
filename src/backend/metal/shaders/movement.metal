// backend/metal/shaders/movement.metal -- EXPAND + RESHAPE.
// Both kernels are single-input "one thread per output element"
// data-movement shapes; no per-op constant arg used.
//
// Buffer-binding convention (set by metal_dispatch_kernel):
//     buffer(0)  : output
//     buffer(1)  : KProgOp.arg              (unused)
//     buffer(2)  : input
//     buffer(3)  : src_numels[0] = in_numel
//
// EXPAND mirrors backend/cpu/op/expand.c:
//     in_numel == 1            -> scalar broadcast (out[tid] = in[0])
//     in_numel == out_numel    -> straight copy
//     general                  -> cycle (out[tid] = in[tid % in_numel])
// All three collapse to `out[tid] = in[tid % in_numel]`.
//
// RESHAPE mirrors backend/cpu/op/reshape.c: row-major contiguous
// reshape preserves total element count, so it's just memcpy.

#include <metal_stdlib>
using namespace metal;

kernel void thvm_expand(device         float *out      [[buffer(0)]],
                        constant       uint  &arg      [[buffer(1)]],
                        device   const float *in       [[buffer(2)]],
                        constant       uint  &in_numel [[buffer(3)]],
                        uint                  tid      [[thread_position_in_grid]])
{
    uint denom = (in_numel == 0u) ? 1u : in_numel;
    out[tid] = in[tid % denom];
    (void)arg;
}

kernel void thvm_reshape(device         float *out      [[buffer(0)]],
                         constant       uint  &arg      [[buffer(1)]],
                         device   const float *in       [[buffer(2)]],
                         constant       uint  &in_numel [[buffer(3)]],
                         uint                  tid      [[thread_position_in_grid]])
{
    out[tid] = in[tid];
    (void)arg;
    (void)in_numel;
}
