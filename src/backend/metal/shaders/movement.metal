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

// FLIP mirrors the per-axis stride walk used by axis-aware EXPAND
// but applies a coord mirror (d - 1 - c) on axes whose bit is set
// in arg (axes_bitmask).  Buffer slots 4 (src0[0]=ndim, src0[1..]
// =dims) and 5 (outd, unused for FLIP since out shape == src shape)
// follow the EXPAND convention so metal_dispatch_kernel can reuse
// the same packing path.
kernel void thvm_flip(device         float *out      [[buffer(0)]],
                      constant       uint  &arg      [[buffer(1)]],
                      device   const float *in       [[buffer(2)]],
                      constant       uint  &in_numel [[buffer(3)]],
                      constant       uint  *src0     [[buffer(4)]],
                      constant       uint  *outd     [[buffer(5)]],
                      uint                  tid      [[thread_position_in_grid]])
{
    (void)outd; (void)in_numel;
    uint axes_mask = arg;
    uint ndim      = src0[0];

    // No flip / shape unknown: passthrough.
    if (axes_mask == 0u || ndim == 0u) {
        out[tid] = in[tid];
        return;
    }

    uint tmp = tid;
    uint src_idx = 0u;
    uint stride  = 1u;
    for (int axis = (int)ndim - 1; axis >= 0; axis--) {
        uint d = src0[1 + axis];
        uint c = tmp % d;
        tmp   /= d;
        if (axes_mask & (1u << (uint)axis)) c = d - 1u - c;
        src_idx += c * stride;
        stride  *= d;
    }
    out[tid] = in[src_idx];
}

// PAD: zero-pad selected axes by per-axis (begin, end) widths.
// buffer(4): src0 = [ndim, dims...]
// buffer(5): outd = [ndim, dims...]
// buffer(6): pad_widths -- u32 array of length 2 * MAX_DIM,
//            interleaved {b0, e0, b1, e1, ...}.
// In-bounds: out[oi] = in[src_idx]; out-of-bounds (in pad
// region on any axis): out[oi] = 0.
kernel void thvm_pad(device         float *out      [[buffer(0)]],
                     constant       uint  &arg      [[buffer(1)]],
                     device   const float *in       [[buffer(2)]],
                     constant       uint  &in_numel [[buffer(3)]],
                     constant       uint  *src0     [[buffer(4)]],
                     constant       uint  *outd     [[buffer(5)]],
                     constant       uint  *padw     [[buffer(6)]],
                     uint                  tid      [[thread_position_in_grid]])
{
    (void)arg; (void)in_numel;
    uint ndim = src0[0];
    if (ndim == 0u) {
        out[tid] = in[tid];
        return;
    }

    uint tmp = tid;
    uint src_idx = 0u;
    uint stride  = 1u;
    bool in_pad  = false;
    for (int axis = (int)ndim - 1; axis >= 0; axis--) {
        uint od = outd[1 + axis];
        uint sd = src0[1 + axis];
        uint b  = padw[2 * (uint)axis];
        uint c  = tmp % od;
        tmp    /= od;
        if (c < b || c >= b + sd) { in_pad = true; break; }
        src_idx += (c - b) * stride;
        stride  *= sd;
    }
    out[tid] = in_pad ? 0.0f : in[src_idx];
}

// SHRINK: extract sub-region.  Inverse of PAD: keep slice
// [b_i, e_i) on each axis.  Output coord c on axis i maps to
// source coord (c + b_i).
// buffer(4): src0 = [ndim, dims...]
// buffer(5): outd = [ndim, dims...]
// buffer(6): widths = u32 array of 2 * MAX_DIM, interleaved
//            {b0, e0, b1, e1, ...} (only the b_i values are
//            consulted by SHRINK; e_i is implicit in out_dim).
kernel void thvm_shrink(device         float *out      [[buffer(0)]],
                        constant       uint  &arg      [[buffer(1)]],
                        device   const float *in       [[buffer(2)]],
                        constant       uint  &in_numel [[buffer(3)]],
                        constant       uint  *src0     [[buffer(4)]],
                        constant       uint  *outd     [[buffer(5)]],
                        constant       uint  *widths   [[buffer(6)]],
                        uint                  tid      [[thread_position_in_grid]])
{
    (void)arg; (void)in_numel;
    uint ndim = src0[0];
    if (ndim == 0u) {
        out[tid] = in[tid];
        return;
    }

    // Source row-major strides.
    uint src_stride[METAL_MAX_DIM];
    src_stride[ndim - 1u] = 1u;
    for (int axis = (int)ndim - 2; axis >= 0; axis--) {
        src_stride[axis] = src_stride[axis + 1] * src0[1 + axis + 1];
    }

    uint tmp = tid;
    uint src_idx = 0u;
    for (int axis = (int)ndim - 1; axis >= 0; axis--) {
        uint od = outd[1 + axis];
        uint b  = widths[2 * (uint)axis];
        uint c  = tmp % od;
        tmp    /= od;
        src_idx += (c + b) * src_stride[axis];
    }
    out[tid] = in[src_idx];
}

// PERMUTE: reorder axes.  out[oi] = in[src_idx] where src_idx
// is built by mapping each output axis i to source axis perm[i].
// buffer(4): src0 = [ndim, dims...]
// buffer(5): outd = [ndim, dims...]
// buffer(6): perm[] -- u32 array of length MAX_DIM, perm[i] is
//            the source axis that becomes output axis i.
kernel void thvm_permute(device         float *out      [[buffer(0)]],
                         constant       uint  &arg      [[buffer(1)]],
                         device   const float *in       [[buffer(2)]],
                         constant       uint  &in_numel [[buffer(3)]],
                         constant       uint  *src0     [[buffer(4)]],
                         constant       uint  *outd     [[buffer(5)]],
                         constant       uint  *perm     [[buffer(6)]],
                         uint                  tid      [[thread_position_in_grid]])
{
    (void)arg; (void)in_numel;
    uint ndim = src0[0];
    if (ndim == 0u) {
        out[tid] = in[tid];
        return;
    }

    // Source strides (row-major over src0_dims).
    uint src_stride[METAL_MAX_DIM];
    src_stride[ndim - 1u] = 1u;
    for (int axis = (int)ndim - 2; axis >= 0; axis--) {
        src_stride[axis] = src_stride[axis + 1] * src0[1 + axis + 1];
    }

    uint tmp = tid;
    uint src_idx = 0u;
    for (int axis = (int)ndim - 1; axis >= 0; axis--) {
        uint od = outd[1 + axis];
        uint c  = tmp % od;
        tmp    /= od;
        src_idx += c * src_stride[perm[axis]];
    }
    out[tid] = in[src_idx];
}
