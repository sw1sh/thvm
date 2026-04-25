// backend/metal/shaders/unary.metal -- broadcast-aware unary
// elementwise kernels (NEG, RECIP, SQRT, EXP2, LOG2).  f32 only.
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

#define UNARY_ELEMENTWISE(name, op)                                        \
kernel void name(device         float *out  [[buffer(0)]],                 \
                 constant       uint  &arg  [[buffer(1)]],                 \
                 device   const float *a    [[buffer(2)]],                 \
                 constant       uint  &na   [[buffer(3)]],                 \
                 uint                  tid  [[thread_position_in_grid]])   \
{                                                                          \
    uint ia = (na == 1u) ? 0u : tid;                                       \
    out[tid] = op(a[ia]);                                                  \
    (void)arg;                                                             \
}

inline float thvm_op_neg  (float x) { return -x; }
inline float thvm_op_recip(float x) { return 1.0f / x; }
inline float thvm_op_sqrt (float x) { return sqrt(x); }
inline float thvm_op_exp2 (float x) { return exp2(x); }
inline float thvm_op_log2 (float x) { return log2(x); }

UNARY_ELEMENTWISE(thvm_neg,   thvm_op_neg)
UNARY_ELEMENTWISE(thvm_recip, thvm_op_recip)
UNARY_ELEMENTWISE(thvm_sqrt,  thvm_op_sqrt)
UNARY_ELEMENTWISE(thvm_exp2,  thvm_op_exp2)
UNARY_ELEMENTWISE(thvm_log2,  thvm_op_log2)
