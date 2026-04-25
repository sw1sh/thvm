// backend/metal/shaders/reduce.metal -- REDUCE_SUM + REDUCE_MAX
// over an arbitrary single axis.
//
// Mirrors backend/cpu/op/reduce.c.  KProgOp.arg packing (set in
// materialize_in_env.c after the recent stride fix):
//     bits 24..31 : kind  (REDUCE_SUM = 0, REDUCE_MAX = 1)
//     bits  0..23 : inner = product of input dims after the
//                   reduced axis (1 means innermost).
//
// Buffer-binding convention (set by metal_dispatch_kernel):
//     buffer(0)  : output
//     buffer(1)  : KProgOp.arg
//     buffer(2)  : input
//     buffer(3)  : src_numels[0] = in_numel
//
// out_numel is `threads_per_grid` -- the dispatch sets one thread
// per output element so axis_size = in_numel / out_numel.
//
// Per output index oi:
//     outer_idx = oi / inner;  inner_idx = oi % inner
//     in_flat   = outer_idx * (axis_size * inner) + k*inner + inner_idx

#include <metal_stdlib>
using namespace metal;

kernel void thvm_reduce(device         float *out      [[buffer(0)]],
                        constant       uint  &arg      [[buffer(1)]],
                        device   const float *a        [[buffer(2)]],
                        constant       uint  &in_numel [[buffer(3)]],
                        uint                  tid      [[thread_position_in_grid]],
                        uint                  tg_size  [[threads_per_grid]])
{
    uint kind  = (arg >> 24) & 0xFFu;
    uint inner =  arg        & 0x00FFFFFFu;
    if (inner == 0u) inner = 1u;
    uint out_numel = tg_size;
    uint axis_size = in_numel / out_numel;
    uint outer_idx = tid / inner;
    uint inner_idx = tid - outer_idx * inner;     // tid % inner
    float acc = (kind == 1u) ? -INFINITY : 0.0f;
    for (uint k = 0u; k < axis_size; k++) {
        float v = a[outer_idx * (axis_size * inner) + k * inner + inner_idx];
        acc = (kind == 1u) ? max(acc, v) : (acc + v);
    }
    out[tid] = acc;
}
