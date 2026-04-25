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
// EXPAND additionally takes movement-op shape info (only set by
// metal_dispatch_kernel when opcode == UOP_EXPAND):
//     buffer(4)  : src0[0] = src0_ndim, src0[1..MAX_DIM] = src0_dims
//     buffer(5)  : outd[0] = out_ndim,  outd[1..MAX_DIM] = out_dims
//
// EXPAND mirrors backend/cpu/op/expand.c's axis-aware path:
//     in_numel == 1            -> scalar broadcast (out[tid] = in[0])
//     in_numel == out_numel    -> straight copy
//     same-rank w/ shape info  -> per-axis stride walk; ignore coords
//                                 on broadcast axes (src_dim == 1)
//     otherwise                -> legacy cycle (in[tid % in_numel])
//
// RESHAPE mirrors backend/cpu/op/reshape.c: row-major contiguous
// reshape preserves total element count, so it's just memcpy.

#include <metal_stdlib>
using namespace metal;

// MAX_DIM matches src/thvm.h.  Keep in sync.
#define METAL_MAX_DIM 8

kernel void thvm_expand(device         float *out      [[buffer(0)]],
                        constant       uint  &arg      [[buffer(1)]],
                        device   const float *in       [[buffer(2)]],
                        constant       uint  &in_numel [[buffer(3)]],
                        constant       uint  *src0     [[buffer(4)]],
                        constant       uint  *outd     [[buffer(5)]],
                        uint                  tid      [[thread_position_in_grid]],
                        uint                  tg_size  [[threads_per_grid]])
{
    (void)arg;
    uint out_numel = tg_size;

    // Scalar broadcast fast path.
    if (in_numel == 1u) {
        out[tid] = in[0];
        return;
    }
    // Identity memcpy fast path.
    if (in_numel == out_numel) {
        out[tid] = in[tid];
        return;
    }

    uint src0_ndim = src0[0];
    uint out_ndim  = outd[0];

    // Axis-aware path: ranks match and we have shape info.
    if (out_ndim > 0u && src0_ndim == out_ndim) {
        uint oi = tid;
        uint src_idx = 0u;
        uint src_stride = 1u;
        // Walk axes from innermost to outermost.
        for (int axis = (int)out_ndim - 1; axis >= 0; axis--) {
            uint od = outd[1 + axis];
            uint sd = src0[1 + axis];
            uint coord = oi % od;
            oi /= od;
            if (sd != 1u) {
                src_idx += coord * src_stride;
            }
            src_stride *= sd;
        }
        out[tid] = in[src_idx];
        return;
    }

    // Legacy fallback (correct for trailing-axis broadcast only).
    uint denom = (in_numel == 0u) ? 1u : in_numel;
    out[tid] = in[tid % denom];
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
