// backend/metal/shaders/movement.metal -- EXPAND / RESHAPE / FLIP /
// PAD / SHRINK / PERMUTE.  Movement ops only re-arrange bytes (no
// arithmetic), so each kernel is templated on the element type.
// Per-dtype variants suffixed _f32 / _i32.
//
// Buffer-binding convention (set by metal_dispatch_kernel):
//     buffer(0)  : output
//     buffer(1)  : per-op arg                (used only by FLIP)
//     buffer(2)  : input
//     buffer(3)  : src_numels[0] = in_numel
//     buffer(4)  : src0[0] = ndim, src0[1..MAX_DIM] = dims
//     buffer(5)  : outd[0] = ndim, outd[1..MAX_DIM] = dims
//     buffer(6)  : op-specific widths/perm (PAD/SHRINK/PERMUTE)

#include <metal_stdlib>
using namespace metal;

#define METAL_MAX_DIM 8

// EXPAND -------------------------------------------------------------

#define DEFINE_EXPAND(name, T)                                              \
kernel void name(device         T    *out      [[buffer(0)]],               \
                 constant       uint &arg      [[buffer(1)]],               \
                 device   const T    *in       [[buffer(2)]],               \
                 constant       uint &in_numel [[buffer(3)]],               \
                 constant       uint *src0     [[buffer(4)]],               \
                 constant       uint *outd     [[buffer(5)]],               \
                 uint                 tid      [[thread_position_in_grid]], \
                 uint                 tg_size  [[threads_per_grid]])        \
{                                                                           \
    (void)arg;                                                              \
    uint out_numel = tg_size;                                               \
    if (in_numel == 1u) { out[tid] = in[0]; return; }                       \
    if (in_numel == out_numel) { out[tid] = in[tid]; return; }              \
    uint src0_ndim = src0[0];                                               \
    uint out_ndim  = outd[0];                                               \
    if (out_ndim > 0u && src0_ndim == out_ndim) {                           \
        uint oi = tid;                                                      \
        uint src_idx = 0u;                                                  \
        uint src_stride = 1u;                                               \
        for (int axis = (int)out_ndim - 1; axis >= 0; axis--) {             \
            uint od = outd[1 + axis];                                       \
            uint sd = src0[1 + axis];                                       \
            uint coord = oi % od;                                           \
            oi /= od;                                                       \
            if (sd != 1u) src_idx += coord * src_stride;                    \
            src_stride *= sd;                                               \
        }                                                                   \
        out[tid] = in[src_idx];                                             \
        return;                                                             \
    }                                                                       \
    uint denom = (in_numel == 0u) ? 1u : in_numel;                          \
    out[tid] = in[tid % denom];                                             \
}

DEFINE_EXPAND(thvm_expand_f32, float)
DEFINE_EXPAND(thvm_expand_i32, int)

// RESHAPE ------------------------------------------------------------

#define DEFINE_RESHAPE(name, T)                                             \
kernel void name(device         T    *out      [[buffer(0)]],               \
                 constant       uint &arg      [[buffer(1)]],               \
                 device   const T    *in       [[buffer(2)]],               \
                 constant       uint &in_numel [[buffer(3)]],               \
                 uint                 tid      [[thread_position_in_grid]]) \
{                                                                           \
    out[tid] = in[tid];                                                     \
    (void)arg; (void)in_numel;                                              \
}

DEFINE_RESHAPE(thvm_reshape_f32, float)
DEFINE_RESHAPE(thvm_reshape_i32, int)

// FLIP ---------------------------------------------------------------

#define DEFINE_FLIP(name, T)                                                \
kernel void name(device         T    *out      [[buffer(0)]],               \
                 constant       uint &arg      [[buffer(1)]],               \
                 device   const T    *in       [[buffer(2)]],               \
                 constant       uint &in_numel [[buffer(3)]],               \
                 constant       uint *src0     [[buffer(4)]],               \
                 constant       uint *outd     [[buffer(5)]],               \
                 uint                 tid      [[thread_position_in_grid]]) \
{                                                                           \
    (void)outd; (void)in_numel;                                             \
    uint axes_mask = arg;                                                   \
    uint ndim      = src0[0];                                               \
    if (axes_mask == 0u || ndim == 0u) { out[tid] = in[tid]; return; }      \
    uint tmp = tid;                                                         \
    uint src_idx = 0u;                                                      \
    uint stride  = 1u;                                                      \
    for (int axis = (int)ndim - 1; axis >= 0; axis--) {                     \
        uint d = src0[1 + axis];                                            \
        uint c = tmp % d;                                                   \
        tmp   /= d;                                                         \
        if (axes_mask & (1u << (uint)axis)) c = d - 1u - c;                 \
        src_idx += c * stride;                                              \
        stride  *= d;                                                       \
    }                                                                       \
    out[tid] = in[src_idx];                                                 \
}

DEFINE_FLIP(thvm_flip_f32, float)
DEFINE_FLIP(thvm_flip_i32, int)

// PAD ----------------------------------------------------------------

#define DEFINE_PAD(name, T)                                                 \
kernel void name(device         T    *out      [[buffer(0)]],               \
                 constant       uint &arg      [[buffer(1)]],               \
                 device   const T    *in       [[buffer(2)]],               \
                 constant       uint &in_numel [[buffer(3)]],               \
                 constant       uint *src0     [[buffer(4)]],               \
                 constant       uint *outd     [[buffer(5)]],               \
                 constant       uint *padw     [[buffer(6)]],               \
                 uint                 tid      [[thread_position_in_grid]]) \
{                                                                           \
    (void)arg; (void)in_numel;                                              \
    uint ndim = src0[0];                                                    \
    if (ndim == 0u) { out[tid] = in[tid]; return; }                         \
    uint tmp = tid;                                                         \
    uint src_idx = 0u;                                                      \
    uint stride  = 1u;                                                      \
    bool in_pad  = false;                                                   \
    for (int axis = (int)ndim - 1; axis >= 0; axis--) {                     \
        uint od = outd[1 + axis];                                           \
        uint sd = src0[1 + axis];                                           \
        uint b  = padw[2 * (uint)axis];                                     \
        uint c  = tmp % od;                                                 \
        tmp    /= od;                                                       \
        if (c < b || c >= b + sd) { in_pad = true; break; }                 \
        src_idx += (c - b) * stride;                                        \
        stride  *= sd;                                                      \
    }                                                                       \
    out[tid] = in_pad ? (T)0 : in[src_idx];                                 \
}

DEFINE_PAD(thvm_pad_f32, float)
DEFINE_PAD(thvm_pad_i32, int)

// SHRINK -------------------------------------------------------------

#define DEFINE_SHRINK(name, T)                                              \
kernel void name(device         T    *out      [[buffer(0)]],               \
                 constant       uint &arg      [[buffer(1)]],               \
                 device   const T    *in       [[buffer(2)]],               \
                 constant       uint &in_numel [[buffer(3)]],               \
                 constant       uint *src0     [[buffer(4)]],               \
                 constant       uint *outd     [[buffer(5)]],               \
                 constant       uint *widths   [[buffer(6)]],               \
                 uint                 tid      [[thread_position_in_grid]]) \
{                                                                           \
    (void)arg; (void)in_numel;                                              \
    uint ndim = src0[0];                                                    \
    if (ndim == 0u) { out[tid] = in[tid]; return; }                         \
    uint src_stride[METAL_MAX_DIM];                                         \
    src_stride[ndim - 1u] = 1u;                                             \
    for (int axis = (int)ndim - 2; axis >= 0; axis--) {                     \
        src_stride[axis] = src_stride[axis + 1] * src0[1 + axis + 1];       \
    }                                                                       \
    uint tmp = tid;                                                         \
    uint src_idx = 0u;                                                      \
    for (int axis = (int)ndim - 1; axis >= 0; axis--) {                     \
        uint od = outd[1 + axis];                                           \
        uint b  = widths[2 * (uint)axis];                                   \
        uint c  = tmp % od;                                                 \
        tmp    /= od;                                                       \
        src_idx += (c + b) * src_stride[axis];                              \
    }                                                                       \
    out[tid] = in[src_idx];                                                 \
}

DEFINE_SHRINK(thvm_shrink_f32, float)
DEFINE_SHRINK(thvm_shrink_i32, int)

// PERMUTE ------------------------------------------------------------

#define DEFINE_PERMUTE(name, T)                                             \
kernel void name(device         T    *out      [[buffer(0)]],               \
                 constant       uint &arg      [[buffer(1)]],               \
                 device   const T    *in       [[buffer(2)]],               \
                 constant       uint &in_numel [[buffer(3)]],               \
                 constant       uint *src0     [[buffer(4)]],               \
                 constant       uint *outd     [[buffer(5)]],               \
                 constant       uint *perm     [[buffer(6)]],               \
                 uint                 tid      [[thread_position_in_grid]]) \
{                                                                           \
    (void)arg; (void)in_numel;                                              \
    uint ndim = src0[0];                                                    \
    if (ndim == 0u) { out[tid] = in[tid]; return; }                         \
    uint src_stride[METAL_MAX_DIM];                                         \
    src_stride[ndim - 1u] = 1u;                                             \
    for (int axis = (int)ndim - 2; axis >= 0; axis--) {                     \
        src_stride[axis] = src_stride[axis + 1] * src0[1 + axis + 1];       \
    }                                                                       \
    uint tmp = tid;                                                         \
    uint src_idx = 0u;                                                      \
    for (int axis = (int)ndim - 1; axis >= 0; axis--) {                     \
        uint od = outd[1 + axis];                                           \
        uint c  = tmp % od;                                                 \
        tmp    /= od;                                                       \
        src_idx += c * src_stride[perm[axis]];                              \
    }                                                                       \
    out[tid] = in[src_idx];                                                 \
}

DEFINE_PERMUTE(thvm_permute_f32, float)
DEFINE_PERMUTE(thvm_permute_i32, int)
