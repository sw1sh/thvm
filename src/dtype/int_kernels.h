// dtype/int_kernels.h - macro expansion helpers for integer-family
// CPU op kernels.  Each user file defines a per-(dtype, C-type)
// CASE expander and invokes EACH_INT_DTYPE(CASE) inside a switch.
//
// Reference: tinygrad uses ctypes per dtype (ops_python.py); we mirror
// the same shape with stdint types.  bool is treated as a u8 carrier
// with logical ADD = OR / MUL = AND semantics (handled by the caller's
// CASE expander, since the operator differs).
//
// Examples:
//   CPU_OP_INT_BIN(cpu_op_add_int_body, +)
// expands to a switch with one body per integer dtype that does
// element-wise `+` with broadcast.

// Per-dtype lane: maps DT_* id -> the C type used in the kernel body.
// The bool row uses u8 since bool storage is one byte and arithmetic
// happens on the byte (wrap/saturation only matters for nonsense
// inputs).
#define EACH_INT_DTYPE(F)         \
    F(DT_INT8,   i8 )             \
    F(DT_UINT8,  u8 )             \
    F(DT_INT16,  i16)             \
    F(DT_UINT16, u16)             \
    F(DT_INT32,  i32)             \
    F(DT_UINT32, u32)             \
    F(DT_INT64,  i64)             \
    F(DT_UINT64, u64)

// Bool is broken out so callers that route ADD->OR / MUL->AND
// can opt-in without touching the integer cases.
#define EACH_INT_DTYPE_PLUS_BOOL(F)  \
    EACH_INT_DTYPE(F)                \
    F(DT_BOOL,   u8 )

// Body templates --------------------------------------------------

// Binary elementwise with broadcast (numel==1 source repeats).
// `OP` is a binary C operator token (+, -, *, &, |, ...).
#define INT_BIN_CASE(DT, T, OP)                                             \
    case DT: {                                                              \
        T *o = (T *)out;                                                    \
        T *a = (T *)srcs[0];                                                \
        T *b = (T *)srcs[1];                                                \
        u8 ba = (src_numels[0] == 1);                                       \
        u8 bb = (src_numels[1] == 1);                                       \
        for (u32 i = 0; i < out_numel; i++) o[i] = a[ba ? 0 : i] OP b[bb ? 0 : i]; \
        break;                                                              \
    }

// Unary elementwise.
#define INT_UN_CASE(DT, T, EXPR)                                            \
    case DT: {                                                              \
        T *o = (T *)out;                                                    \
        T *a = (T *)srcs[0];                                                \
        u8 ba = (src_numels[0] == 1);                                       \
        for (u32 i = 0; i < out_numel; i++) {                               \
            T x = a[ba ? 0 : i]; o[i] = (T)(EXPR);                          \
        }                                                                   \
        break;                                                              \
    }

// Comparison (==, <).  Output is i32 1/0 (matches the F32 path which
// emits 1.0f/0.0f); the WL surface coerces with explicit casts when
// a different output dtype is needed.
#define INT_CMP_CASE(DT, T, OP)                                             \
    case DT: {                                                              \
        i32 *o = (i32 *)out;                                                \
        T   *a = (T   *)srcs[0];                                            \
        T   *b = (T   *)srcs[1];                                            \
        u8 ba = (src_numels[0] == 1);                                       \
        u8 bb = (src_numels[1] == 1);                                       \
        for (u32 i = 0; i < out_numel; i++)                                 \
            o[i] = (a[ba ? 0 : i] OP b[bb ? 0 : i]) ? 1 : 0;                \
        break;                                                              \
    }
