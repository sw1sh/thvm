// backend/metal/shaders/unary.metal -- broadcast-aware unary
// elementwise kernels (NEG, RECIP, SQRT, EXP2, LOG2).  f32 + i32
// variants for NEG; RECIP/SQRT/EXP2/LOG2 are float-only (i32
// kernels would need a CAST in user code first).
//
// Buffer-binding convention (set by metal_dispatch_kernel):
//     buffer(0)  : output
//     buffer(1)  : KProgOp.arg              (unused for unary)
//     buffer(2)  : input a
//     buffer(3)  : src_numels[0]            (uint; 1 -> broadcast)
//
// One thread per output element.  Mirrors backend/cpu/op/{neg,
// recip, sqrt, exp2, log2}.c semantics including the broadcast
// (numel == 1) shortcut.

#include <metal_stdlib>
using namespace metal;

#define UNARY_ELEMENTWISE(name, T, op)                                     \
kernel void name(device         T    *out  [[buffer(0)]],                  \
                 constant       uint &arg  [[buffer(1)]],                  \
                 device   const T    *a    [[buffer(2)]],                  \
                 constant       uint &na   [[buffer(3)]],                  \
                 uint                 tid  [[thread_position_in_grid]])    \
{                                                                          \
    uint ia = (na == 1u) ? 0u : tid;                                       \
    out[tid] = op(a[ia]);                                                  \
    (void)arg;                                                             \
}

inline float thvm_op_neg_f32  (float x) { return -x; }
inline float thvm_op_recip_f32(float x) { return 1.0f / x; }
inline float thvm_op_sqrt_f32 (float x) { return sqrt(x); }
inline float thvm_op_exp2_f32 (float x) { return exp2(x); }
inline float thvm_op_log2_f32 (float x) { return log2(x); }

inline int   thvm_op_neg_i32  (int x)   { return -x; }

UNARY_ELEMENTWISE(thvm_neg_f32,   float, thvm_op_neg_f32)
UNARY_ELEMENTWISE(thvm_recip_f32, float, thvm_op_recip_f32)
UNARY_ELEMENTWISE(thvm_sqrt_f32,  float, thvm_op_sqrt_f32)
UNARY_ELEMENTWISE(thvm_exp2_f32,  float, thvm_op_exp2_f32)
UNARY_ELEMENTWISE(thvm_log2_f32,  float, thvm_op_log2_f32)

UNARY_ELEMENTWISE(thvm_neg_i32,   int,   thvm_op_neg_i32)
