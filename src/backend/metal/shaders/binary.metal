// backend/metal/shaders/binary.metal -- broadcast-aware binary
// elementwise kernels (ADD, MUL, CMPLT).  f32 only in v1; i32
// support follows when grad rules need it.
//
// Buffer-binding convention (set by metal_dispatch_kernel):
//     buffer(0)  : output
//     buffer(1)  : KProgOp.arg              (unused for binary)
//     buffer(2)  : input a
//     buffer(3)  : input b
//     buffer(4)  : src_numels[0]            (uint; 1 -> broadcast)
//     buffer(5)  : src_numels[1]
//
// One thread per output element; the broadcast index is selected
// by the per-input numel: numel == 1 means "repeat element 0".

#include <metal_stdlib>
using namespace metal;

#define BIN_ELEMENTWISE(name, op)                                          \
kernel void name(device         float *out  [[buffer(0)]],                 \
                 constant       uint  &arg  [[buffer(1)]],                 \
                 device   const float *a    [[buffer(2)]],                 \
                 device   const float *b    [[buffer(3)]],                 \
                 constant       uint  &na   [[buffer(4)]],                 \
                 constant       uint  &nb   [[buffer(5)]],                 \
                 uint                  tid  [[thread_position_in_grid]])   \
{                                                                          \
    uint ia = (na == 1u) ? 0u : tid;                                       \
    uint ib = (nb == 1u) ? 0u : tid;                                       \
    out[tid] = op(a[ia], b[ib]);                                           \
    (void)arg;                                                             \
}

inline float thvm_op_add  (float x, float y) { return x + y; }
inline float thvm_op_mul  (float x, float y) { return x * y; }
inline float thvm_op_cmplt(float x, float y) { return (x < y) ? 1.0f : 0.0f; }

BIN_ELEMENTWISE(thvm_add,   thvm_op_add)
BIN_ELEMENTWISE(thvm_mul,   thvm_op_mul)
BIN_ELEMENTWISE(thvm_cmplt, thvm_op_cmplt)
