// backend/metal/shaders/binary.metal -- broadcast-aware binary
// elementwise kernels (ADD, MUL, CMPLT, CMPEQ).  Per-dtype variants
// suffixed _f32 / _i32 so metal_pipeline_for(opcode, dtype) can pick
// the right pipeline.
//
// Buffer-binding convention (set by metal_dispatch_kernel):
//     buffer(0)  : output
//     buffer(1)  : per-op arg                (unused for binary)
//     buffer(2)  : input a
//     buffer(3)  : input b
//     buffer(4)  : src_numels[0]            (uint; 1 -> broadcast)
//     buffer(5)  : src_numels[1]
//
// One thread per output element; the broadcast index is selected
// by the per-input numel: numel == 1 means "repeat element 0".

#include <metal_stdlib>
using namespace metal;

#define BIN_ELEMENTWISE(name, T, op)                                       \
kernel void name(device         T    *out  [[buffer(0)]],                  \
                 constant       uint &arg  [[buffer(1)]],                  \
                 device   const T    *a    [[buffer(2)]],                  \
                 device   const T    *b    [[buffer(3)]],                  \
                 constant       uint &na   [[buffer(4)]],                  \
                 constant       uint &nb   [[buffer(5)]],                  \
                 uint                 tid  [[thread_position_in_grid]])    \
{                                                                          \
    uint ia = (na == 1u) ? 0u : tid;                                       \
    uint ib = (nb == 1u) ? 0u : tid;                                       \
    out[tid] = op(a[ia], b[ib]);                                           \
    (void)arg;                                                             \
}

// Operator helpers per (kind, T).  CMP returns the input's typed
// 1 / 0 to match the interpreter convention.
inline float thvm_op_add_f32  (float x, float y) { return x + y; }
inline float thvm_op_mul_f32  (float x, float y) { return x * y; }
inline float thvm_op_cmplt_f32(float x, float y) { return (x <  y) ? 1.0f : 0.0f; }
inline float thvm_op_cmpeq_f32(float x, float y) { return (x == y) ? 1.0f : 0.0f; }

inline int   thvm_op_add_i32  (int x, int y)     { return x + y; }
inline int   thvm_op_mul_i32  (int x, int y)     { return x * y; }
inline int   thvm_op_cmplt_i32(int x, int y)     { return (x <  y) ? 1 : 0; }
inline int   thvm_op_cmpeq_i32(int x, int y)     { return (x == y) ? 1 : 0; }

BIN_ELEMENTWISE(thvm_add_f32,   float, thvm_op_add_f32)
BIN_ELEMENTWISE(thvm_mul_f32,   float, thvm_op_mul_f32)
BIN_ELEMENTWISE(thvm_cmplt_f32, float, thvm_op_cmplt_f32)
BIN_ELEMENTWISE(thvm_cmpeq_f32, float, thvm_op_cmpeq_f32)

BIN_ELEMENTWISE(thvm_add_i32,   int,   thvm_op_add_i32)
BIN_ELEMENTWISE(thvm_mul_i32,   int,   thvm_op_mul_i32)
BIN_ELEMENTWISE(thvm_cmplt_i32, int,   thvm_op_cmplt_i32)
BIN_ELEMENTWISE(thvm_cmpeq_i32, int,   thvm_op_cmpeq_i32)
