// backend/cpu/interpret.c - scalar-UOp + tile interpreters and dispatch ladder.
//
// The legacy KProgOp tree-walker (cpu_interpret) and its per-op kernel
// files (backend/cpu/op/*.c) were deleted alongside this file's F6
// cleanup wedge: the UOp DAG walker (backend/cpu/uop_walk.c) covers
// every kernel shape the surgical suite produces, with zero
// THVM_CPU_INTERPRET_TRACE hits remaining at deletion time.
//
// What stays here:
//   - cpu_dispatch_scalar: rangeify scalar-UOp recursive evaluator
//     (REDUCE / FLIP / PAD / SHRINK / packed nibbles / narrow FPs).
//   - cpu_dispatch_tile:   TileUop interpreter (env-gated;
//     correctness fallback until Phase G also collapses TileUop[]).
//   - cpu_dispatch_kernel: the dispatch ladder itself.

#include <math.h>     // sqrtf/sqrt, exp2f/exp2, log2f/log2, INFINITY


// === Phase B/C: scalar-UOp interpreter ===============================
//
// Recursive evaluator over ke->scalar_uops.  Each op produces a u64
// value (range iter, address, or f32-bit-cast scalar) that is
// either:
//   - immediate-recomputed via eval_scalar (for ops whose value
//     depends on the current iter context -- including REDUCE
//     bodies)
//   - or read from `range_iter[op_id]` for S_RANGE ops, which the
//     dispatcher mutates at loop boundaries.
//
// The dispatcher iterates LOOP-typed ranges in their canonical
// row-major order, sets the per-range iter values, then evaluates
// the kernel's S_STORE op at each iteration.  REDUCE-typed ranges are
// nested INSIDE the eval (S_REDUCE_SUM/_MAX opens inner loops over
// src[1..], mutates each range's iter, accumulates).
//
// f32 only for now; bit-cast through u32 stored in the low 32 bits
// of the returned u64.

typedef struct {
  KernelEntry *ke;
  void       **in_ptrs;
  void        *out_p;
  u32         *range_iter;     // per-op-id; only S_RANGE slots are used
  u32          odtype;
} ScalarCtx;

static u64 eval_scalar(ScalarCtx *c, u32 op_id);

static u32 eval_iter_ref_extent(ScalarCtx *c, u32 ref_id) {
  if (ref_id == 0 || ref_id >= c->ke->n_scalar_uops) return 0;
  ScalarUop const *u = &c->ke->scalar_uops[ref_id];
  switch (u->op) {
    case S_RANGE:
      return (u32)(u->extra & 0xFFFFFFFFu);
    case S_IADD:
    case S_ISUB: {
      u32 a = eval_iter_ref_extent(c, u->src[0]);
      if (a != 0) return a;
      return eval_iter_ref_extent(c, u->src[1]);
    }
    case S_IMOD: {
      u32 rhs = u->src[1];
      if (rhs != 0 && rhs < c->ke->n_scalar_uops
          && c->ke->scalar_uops[rhs].op == S_ICONST
          && c->ke->scalar_uops[rhs].extra > 0
          && c->ke->scalar_uops[rhs].extra <= UINT32_MAX) {
        return (u32)c->ke->scalar_uops[rhs].extra;
      }
      return 0;
    }
    default:
      return 0;
  }
}

// Decode S_INDEX: address = offset + sum(range_iter[src[1+d]] * stride[d]).
// Returns the byte-offset address (input element index, output
// element index, etc.) plus the slot id in the high 32 bits when
// the buffer is a DEFINE_PARAM.  `extra` packs:
//   bits  0..15 -- stride for src[1]
//   bits 16..31 -- stride for src[2]
//   bits 32..47 -- stride for src[3]
//   bits 48..63 -- per-INDEX offset (added to address)
//
// When src[0] is itself an S_INDEX (the chained-INDEX path used
// for ndim > 3), recurse to gather the slot + accumulated address
// from the inner node, then add this node's contributions.
static u64 eval_index(ScalarCtx *c, ScalarUop const *u) {
  u32 buf_id = u->src[0];
  ScalarUop const *bu = &c->ke->scalar_uops[buf_id];
  u64 packed = u->extra;
  u32 addr   = (u32)((packed >> 48) & 0xFFFFu);
  u32 nrng   = (u32)u->src_count - 1;
  for (u32 d = 0; d < nrng; d++) {
    u32 rng_id = u->src[1 + d];
    u32 iter   = c->range_iter[rng_id];
    u32 stride = (u32)((packed >> (16 * d)) & 0xFFFFu);
    addr += iter * stride;
  }
  if (bu->op == S_INDEX) {
    // Chained: inner produces (slot << 32) | partial_addr; add our
    // contributions and forward.
    u64 inner = eval_index(c, bu);
    u32 inner_addr = (u32)(inner & 0xFFFFFFFFu);
    return (inner & 0xFFFFFFFF00000000ULL) | (u64)(inner_addr + addr);
  }
  if (bu->op == S_DEFINE_OUTPUT) return (u64)addr;
  // Input: stash the slot id in the high half so S_LOAD can find the
  // buffer pointer.
  u32 slot = (u32)bu->extra;
  return ((u64)slot << 32) | (u64)addr;
}

// === per-dtype LOAD / STORE helpers =================================
// Read / write one element of `dtype` at `(p + off * itemsize)`.  The
// returned u64 holds:
//   - FP-family (fp16/bf16/fp8/fp32) -- f32 bits widened from native.
//   - fp64 -- raw f64 bits.
//   - int/uint/bool -- raw bits zero-extended.
// Used by S_LOAD, S_STORE, and the REDUCE inner loop accumulator.
// The "promote narrow FPs to f32" policy mirrors
// cpu_op_run_via_f32 in the legacy elementwise dispatch and lets
// ALU stay f32-only for the FP family except fp64.

static u64 scalar_load_typed(const void *p, u32 off, u32 dtype) {
  // Packed nibbles (int4/uint4): 2 elements per byte.  Element i
  // lives at byte i/2; low nibble = even index, high nibble = odd.
  // INT4 sign-extends; UINT4 zero-extends.  Result widens to a
  // u64 that the integer ALU dispatch reads as i32/u32.
  if (dtype == DT_INT4 || dtype == DT_UINT4) {
    const u8 *base = (const u8 *)p;
    u8 byte = base[off >> 1];
    u8 nib  = (off & 1) ? (byte >> 4) : (byte & 0x0Fu);
    if (dtype == DT_INT4 && (nib & 0x08u)) {
      // Sign-extend bit 3 to all higher bits (i32 / i64 register).
      return (u64)(i64)(i32)(int8_t)((i8)(nib | 0xF0u));
    }
    return (u64)nib;
  }
  u32 esz = dtype_itemsize(dtype);
  const u8 *base = (const u8 *)p + (size_t)off * esz;
  // Narrow-FP dtypes need value conversion, not raw memcpy.
  if (dtype == DT_FP16 || dtype == DT_BF16
   || dtype == DT_FP8E4M3 || dtype == DT_FP8E5M2) {
    f32 v;
    to_fp32_lane(&v, base, dtype, 1);
    u32 bits; memcpy(&bits, &v, 4);
    return (u64)bits;
  }
  switch (esz) {
    case 1: { u8  v; memcpy(&v, base, 1); return (u64)v; }
    case 2: { u16 v; memcpy(&v, base, 2); return (u64)v; }
    case 4: { u32 v; memcpy(&v, base, 4); return (u64)v; }
    case 8: { u64 v; memcpy(&v, base, 8); return v; }
    default: return 0;
  }
}

static void scalar_store_typed(void *p, u32 off, u32 dtype, u64 bits) {
  // Packed nibbles: read-modify-write the byte holding this nibble.
  // Note: NOT thread-safe across nibbles in the same byte; the
  // cpu_dispatch_scalar outer loop is single-threaded today.
  if (dtype == DT_INT4 || dtype == DT_UINT4) {
    u8 *base = (u8 *)p;
    u8 *bp   = &base[off >> 1];
    u8 nib   = (u8)(bits & 0x0Fu);
    if (off & 1) *bp = (u8)((*bp & 0x0Fu) | (nib << 4));
    else         *bp = (u8)((*bp & 0xF0u) | nib);
    return;
  }
  u32 esz = dtype_itemsize(dtype);
  u8 *base = (u8 *)p + (size_t)off * esz;
  if (dtype == DT_FP16 || dtype == DT_BF16
   || dtype == DT_FP8E4M3 || dtype == DT_FP8E5M2) {
    u32 b32 = (u32)bits;
    f32 v; memcpy(&v, &b32, 4);
    from_fp32_lane(base, dtype, &v, 1);
    return;
  }
  switch (esz) {
    case 1: { u8  v = (u8 )bits; memcpy(base, &v, 1); break; }
    case 2: { u16 v = (u16)bits; memcpy(base, &v, 2); break; }
    case 4: { u32 v = (u32)bits; memcpy(base, &v, 4); break; }
    case 8: memcpy(base, &bits, 8); break;
  }
}

// Bit-cast u64 register slot to a typed C value and back.  These
// macros let the per-op switch stay readable while still covering
// the f32 / f64 / int8..int64 / uint8..uint64 dtype space.
#define DECODE2(T, dst_a, dst_b)                                              \
  T dst_a, dst_b;                                                              \
  do { u64 _ab = eval_scalar(c, u->src[0]);                                    \
       u64 _bb = eval_scalar(c, u->src[1]);                                    \
       memcpy(&dst_a, &_ab, sizeof(T));                                        \
       memcpy(&dst_b, &_bb, sizeof(T));                                        \
  } while (0)
#define DECODE1(T, dst_a)                                                      \
  T dst_a;                                                                     \
  do { u64 _ab = eval_scalar(c, u->src[0]);                                    \
       memcpy(&dst_a, &_ab, sizeof(T));                                        \
  } while (0)
#define ENCODE(T, val)                                                         \
  do { u64 _r = 0; T _v = (val); memcpy(&_r, &_v, sizeof(T)); return _r; } while (0)
#define ENCODE_BOOL(val)                                                       \
  return (val) ? 1ULL : 0ULL

// Per-dtype binary op dispatch.  Narrow FPs (fp16/bf16/fp8) use the
// f32 ALU path -- LOAD/STORE widen/narrow at the buffer boundary,
// reg[] internally holds f32 bits for the entire FP family except
// fp64.
#define DISPATCH_FP_BINARY(op_macro)                                          \
  switch (u->dtype) {                                                          \
    case DT_FP32:                                                              \
    case DT_FP16: case DT_BF16:                                                \
    case DT_FP8E4M3: case DT_FP8E5M2: {                                        \
      DECODE2(f32, a, b); ENCODE(f32, op_macro(a, b));                        \
    }                                                                          \
    case DT_FP64: { DECODE2(f64, a, b); ENCODE(f64, op_macro(a, b)); }        \
    case DT_INT8:   { DECODE2(i8 , a, b); ENCODE(i8 , (i8 )op_macro(a, b)); } \
    case DT_UINT8:  { DECODE2(u8 , a, b); ENCODE(u8 , (u8 )op_macro(a, b)); } \
    case DT_INT16:  { DECODE2(i16, a, b); ENCODE(i16, (i16)op_macro(a, b)); } \
    case DT_UINT16: { DECODE2(u16, a, b); ENCODE(u16, (u16)op_macro(a, b)); } \
    case DT_INT32:  { DECODE2(i32, a, b); ENCODE(i32, (i32)op_macro(a, b)); } \
    case DT_UINT32: { DECODE2(u32, a, b); ENCODE(u32, (u32)op_macro(a, b)); } \
    case DT_INT64:  { DECODE2(i64, a, b); ENCODE(i64, (i64)op_macro(a, b)); } \
    case DT_UINT64: { DECODE2(u64, a, b); ENCODE(u64, (u64)op_macro(a, b)); } \
    case DT_BOOL:   { DECODE2(u8 , a, b); ENCODE(u8 , (u8 )op_macro(a, b)); } \
    case DT_INT4:   { DECODE2(i32, a, b); ENCODE(i32, I4_SEXT(op_macro(a, b))); } \
    case DT_UINT4:  { DECODE2(u32, a, b); ENCODE(u32, U4_ZEXT(op_macro(a, b))); } \
    default: return 0;                                                         \
  }

// Compare ops: per cpu_op_cmplt convention, the output dtype matches
// the input dtype.  For FP32, the result bits are 1.0f / 0.0f
// (NOT the integer 1/0).  For ints, just 1 / 0 of that width.
// Narrow FPs route through f32 internally (see scalar_load_typed).
#define DISPATCH_CMP(op_macro)                                                \
  switch (u->dtype) {                                                          \
    case DT_FP32:                                                              \
    case DT_FP16: case DT_BF16:                                                \
    case DT_FP8E4M3: case DT_FP8E5M2: {                                        \
      DECODE2(f32, a, b); ENCODE(f32, op_macro(a, b) ? 1.0f : 0.0f);          \
    }                                                                          \
    case DT_FP64: { DECODE2(f64, a, b); ENCODE(f64, op_macro(a, b) ? 1.0  : 0.0 ); } \
    case DT_INT8:   { DECODE2(i8 , a, b); ENCODE(i8 , (i8 )(op_macro(a, b) ? 1 : 0)); } \
    case DT_UINT8:  { DECODE2(u8 , a, b); ENCODE(u8 , (u8 )(op_macro(a, b) ? 1 : 0)); } \
    case DT_INT16:  { DECODE2(i16, a, b); ENCODE(i16, (i16)(op_macro(a, b) ? 1 : 0)); } \
    case DT_UINT16: { DECODE2(u16, a, b); ENCODE(u16, (u16)(op_macro(a, b) ? 1 : 0)); } \
    case DT_INT32:  { DECODE2(i32, a, b); ENCODE(i32, (i32)(op_macro(a, b) ? 1 : 0)); } \
    case DT_UINT32: { DECODE2(u32, a, b); ENCODE(u32, (u32)(op_macro(a, b) ? 1 : 0)); } \
    case DT_INT64:  { DECODE2(i64, a, b); ENCODE(i64, (i64)(op_macro(a, b) ? 1 : 0)); } \
    case DT_UINT64: { DECODE2(u64, a, b); ENCODE(u64, (u64)(op_macro(a, b) ? 1 : 0)); } \
    case DT_BOOL:   { DECODE2(u8 , a, b); ENCODE(u8 , (u8 )((op_macro((a)&1, (b)&1)) ? 1 : 0)); } \
    case DT_INT4:   { DECODE2(i32, a, b); ENCODE(i32, (i32)(op_macro(a, b) ? 1 : 0)); } \
    case DT_UINT4:  { DECODE2(u32, a, b); ENCODE(u32, (u32)(op_macro(a, b) ? 1 : 0)); } \
    default: return 0;                                                         \
  }

// FP-only dispatch (RECIP / SQRT / EXP2 / LOG2).  Integer types are
// not meaningful and bail.
#define DISPATCH_FP_UNARY(op_macro)                                           \
  switch (u->dtype) {                                                          \
    case DT_FP32: { DECODE1(f32, a); ENCODE(f32, op_macro(a)); }              \
    case DT_FP64: { DECODE1(f64, a); ENCODE(f64, op_macro(a)); }              \
    default: return 0;                                                         \
  }

// NEG: signed/FP unary minus.  Unsigned NEG wraps via two's complement.
// Narrow FPs route through f32.
#define DISPATCH_NEG()                                                         \
  switch (u->dtype) {                                                          \
    case DT_FP32:                                                              \
    case DT_FP16: case DT_BF16:                                                \
    case DT_FP8E4M3: case DT_FP8E5M2: {                                        \
      DECODE1(f32, a); ENCODE(f32, -a);                                       \
    }                                                                          \
    case DT_FP64: { DECODE1(f64, a); ENCODE(f64, -a); }                       \
    case DT_INT8:   { DECODE1(i8 , a); ENCODE(i8 , (i8 )-a); }                \
    case DT_UINT8:  { DECODE1(u8 , a); ENCODE(u8 , (u8 )(-(i32)a)); }         \
    case DT_INT16:  { DECODE1(i16, a); ENCODE(i16, (i16)-a); }                \
    case DT_UINT16: { DECODE1(u16, a); ENCODE(u16, (u16)(-(i32)a)); }         \
    case DT_INT32:  { DECODE1(i32, a); ENCODE(i32, -a); }                     \
    case DT_UINT32: { DECODE1(u32, a); ENCODE(u32, (u32)(-(i64)a)); }         \
    case DT_INT64:  { DECODE1(i64, a); ENCODE(i64, -a); }                     \
    case DT_UINT64: { DECODE1(u64, a); ENCODE(u64, 0u - a); }                 \
    case DT_BOOL:   { DECODE1(u8 , a); ENCODE(u8 , (u8 )((~a) & 1)); }        \
    case DT_INT4:   { DECODE1(i32, a); ENCODE(i32, I4_SEXT(-a)); }            \
    case DT_UINT4:  { DECODE1(u32, a); ENCODE(u32, U4_ZEXT(-(i32)a)); }       \
    default: return 0;                                                         \
  }

#define BINOP_ADD(a, b)     ((a) + (b))
#define BINOP_MUL(a, b)     ((a) * (b))
#define BINOP_LT(a, b)      ((a) <  (b))
#define BINOP_EQ(a, b)      ((a) == (b))
#define BINOP_LT_FLIP(a, b) ((a) >  (b))   // used by REDUCE_MAX inner loop

// Packed-nibble register canonicalization.  The "register form" of an
// int4 value is its sign-extended i32 bit pattern; uint4 is its
// zero-extended u32.  scalar_load_typed produces this form and
// scalar_store_typed masks back to a nibble at STORE.  Every producer
// of an int4 register value (ALU, NEG, CAST encode) must sign-extend
// so chained CASTs / ALUs see the right scalar.
#define I4_SEXT(x) (((i32)(x) << 28) >> 28)
#define U4_ZEXT(x) ((u32)(x) & 0x0Fu)

static u64 eval_scalar(ScalarCtx *c, u32 op_id) {
  if (op_id == 0) return 0;
  ScalarUop const *u = &c->ke->scalar_uops[op_id];
  switch (u->op) {
    case S_RANGE:           return (u64)c->range_iter[op_id];
    case S_DEFINE_PARAM:    return u->extra;
    case S_DEFINE_OUTPUT:   return 0;
    case S_CONST: {
      // The u32 arg is encoded by materialize as either:
      //   - raw FP32 bits (also reused for FP64 -- legacy
      //     cpu_op_const converts via from_fp32_lane / explicit cast)
      //   - sign-extended low 32 bits for INT/UINT
      // Mirror that so single-precision-derived constants survive the
      // f64 path (e.g. CONST(-1.0) is encoded as 0xBF800000 even for
      // f64 outputs; we must cast f32->f64 to recover -1.0).
      u32 bits = (u32)(u->extra & 0xFFFFFFFFu);
      switch (u->dtype) {
        // Narrow FPs see CONST.arg as f32 bits (matches legacy
        // cpu_op_const).  The STORE narrows at the buffer boundary.
        case DT_FP32:
        case DT_FP16: case DT_BF16:
        case DT_FP8E4M3: case DT_FP8E5M2:
          return (u64)bits;
        case DT_FP64: {
          f32 v32; memcpy(&v32, &bits, 4);
          f64 v64 = (f64)v32;
          u64 r; memcpy(&r, &v64, 8); return r;
        }
        case DT_INT8:  case DT_INT16:
        case DT_INT32: case DT_INT64:
        case DT_INT4:
          // Sign-extend the low 32 bits.
          return (u64)(i64)(i32)bits;
        case DT_UINT8: case DT_UINT16: case DT_UINT32:
        case DT_UINT64: case DT_BOOL: case DT_UINT4:
          return (u64)bits;
        default: return (u64)bits;
      }
    }
    case S_INDEX:           return eval_index(c, u);
    case S_LOAD: {
      u64 idx_r = eval_scalar(c, u->src[0]);
      u32 slot  = (u32)(idx_r >> 32);
      u32 off   = (u32)(idx_r & 0xFFFFFFFFu);
      void *p   = c->in_ptrs[slot];
      u32 dtype = c->ke->input_dtypes[slot];
      return scalar_load_typed(p, off, dtype);
    }
    case S_LOAD_RAW: {
      // Bit-preserving load: emit by BITCAST(narrow-FP -> int) so the
      // original fp16/bf16/fp8 bit pattern survives to the integer
      // STORE.  Skips scalar_load_typed's narrow-FP widening; returns
      // the raw 1/2/4/8-byte read in the low bits of the register.
      u64 idx_r = eval_scalar(c, u->src[0]);
      u32 slot  = (u32)(idx_r >> 32);
      u32 off   = (u32)(idx_r & 0xFFFFFFFFu);
      void *p   = c->in_ptrs[slot];
      u32 dtype = c->ke->input_dtypes[slot];
      u32 esz   = dtype_itemsize(dtype);
      const u8 *base = (const u8 *)p + (size_t)off * esz;
      switch (esz) {
        case 1: { u8  v; memcpy(&v, base, 1); return (u64)v; }
        case 2: { u16 v; memcpy(&v, base, 2); return (u64)v; }
        case 4: { u32 v; memcpy(&v, base, 4); return (u64)v; }
        case 8: { u64 v; memcpy(&v, base, 8); return v; }
        default: return 0;
      }
    }
    case S_REDUCE_SUM:
    case S_REDUCE_MAX: {
      u32 n_ranges = (u->src_count > 1) ? (u32)u->src_count - 1 : 0;
      u32 range_ids[SCALAR_MAX_SRC];
      u32 extents  [SCALAR_MAX_SRC];
      u32 saved    [SCALAR_MAX_SRC];
      u32 iters    [SCALAR_MAX_SRC];
      int empty = 0;
      for (u32 d = 0; d < n_ranges; d++) {
        u32 rng_id = u->src[1 + d];
        ScalarUop const *r = &c->ke->scalar_uops[rng_id];
        range_ids[d] = rng_id;
        extents  [d] = (u32)(r->extra & 0xFFFFFFFFu);
        saved    [d] = c->range_iter[rng_id];
        iters    [d] = 0;
        if (extents[d] == 0) {
          empty = 1;
        }
      }
      // Type-specific accumulator.  Iterate inside dtype branch.
#define REDUCE_BODY(T, init_max, init_sum, cmpgt)                              \
      do {                                                                     \
        T acc = (u->op == S_REDUCE_MAX) ? (T)(init_max) : (T)(init_sum);       \
        if (n_ranges == 0) {                                                    \
          u64 b = eval_scalar(c, u->src[0]);                                   \
          T v; memcpy(&v, &b, sizeof(T));                                      \
          acc = v;                                                             \
        } else if (!empty) {                                                    \
          int done = 0;                                                        \
          while (!done) {                                                      \
            for (u32 d = 0; d < n_ranges; d++) {                               \
              c->range_iter[range_ids[d]] = iters[d];                          \
            }                                                                  \
            u64 b = eval_scalar(c, u->src[0]);                                 \
            T v; memcpy(&v, &b, sizeof(T));                                    \
            if (u->op == S_REDUCE_MAX) acc = cmpgt(v, acc) ? v : acc;          \
            else                       acc = (T)(acc + v);                     \
            for (i32 d = (i32)n_ranges - 1; d >= 0; d--) {                     \
              iters[d]++;                                                      \
              if (iters[d] < extents[d]) {                                     \
                break;                                                         \
              }                                                                \
              iters[d] = 0;                                                    \
              if (d == 0) {                                                    \
                done = 1;                                                      \
              }                                                                \
            }                                                                  \
          }                                                                    \
        }                                                                      \
        for (u32 d = 0; d < n_ranges; d++) {                                   \
          c->range_iter[range_ids[d]] = saved[d];                              \
        }                                                                      \
        u64 _r = 0; memcpy(&_r, &acc, sizeof(T));                              \
        return _r;                                                             \
      } while (0)
      switch (u->dtype) {
        // Narrow FPs (fp16/bf16/fp8) use f32 accumulator since
        // scalar_load_typed already widened the body's loads.
        case DT_FP32:
        case DT_FP16: case DT_BF16:
        case DT_FP8E4M3: case DT_FP8E5M2:
                        REDUCE_BODY(f32, -INFINITY, 0.0f, BINOP_LT_FLIP);
        case DT_FP64:   REDUCE_BODY(f64, -INFINITY, 0.0,  BINOP_LT_FLIP);
        case DT_INT8:   REDUCE_BODY(i8 , INT8_MIN,   0,   BINOP_LT_FLIP);
        case DT_UINT8:  REDUCE_BODY(u8 , 0,          0,   BINOP_LT_FLIP);
        case DT_INT16:  REDUCE_BODY(i16, INT16_MIN,  0,   BINOP_LT_FLIP);
        case DT_UINT16: REDUCE_BODY(u16, 0,          0,   BINOP_LT_FLIP);
        case DT_INT32:  REDUCE_BODY(i32, INT32_MIN,  0,   BINOP_LT_FLIP);
        case DT_UINT32: REDUCE_BODY(u32, 0,          0,   BINOP_LT_FLIP);
        case DT_INT64:  REDUCE_BODY(i64, INT64_MIN,  0,   BINOP_LT_FLIP);
        case DT_UINT64: REDUCE_BODY(u64, 0,          0,   BINOP_LT_FLIP);
        case DT_BOOL:   REDUCE_BODY(u8 , 0,          0,   BINOP_LT_FLIP);
        // int4/uint4 use a wider accumulator (no overflow risk for
        // realistic reduce sizes); store-time mask preserves nibble
        // semantics.
        case DT_INT4:   REDUCE_BODY(i32, -8,         0,   BINOP_LT_FLIP);
        case DT_UINT4:  REDUCE_BODY(u32, 0,          0,   BINOP_LT_FLIP);
        default:
          for (u32 d = 0; d < n_ranges; d++) {
            c->range_iter[range_ids[d]] = saved[d];
          }
          return 0;
      }
#undef REDUCE_BODY
    }
    case S_ADD:    DISPATCH_FP_BINARY(BINOP_ADD);
    case S_MUL:    DISPATCH_FP_BINARY(BINOP_MUL);
    case S_NEG:    DISPATCH_NEG();
#define DISPATCH_FP_UNARY(op_f32, op_f64)                                     \
  switch (u->dtype) {                                                          \
    case DT_FP32:                                                              \
    case DT_FP16: case DT_BF16:                                                \
    case DT_FP8E4M3: case DT_FP8E5M2: {                                        \
      DECODE1(f32, a); ENCODE(f32, op_f32(a));                                \
    }                                                                          \
    case DT_FP64: { DECODE1(f64, a); ENCODE(f64, op_f64(a)); }                \
    default: return 0;                                                         \
  }
#define UNOP_RECIP_F(a) (1.0f / (a))
#define UNOP_RECIP_D(a) (1.0  / (a))
    case S_RECIP: DISPATCH_FP_UNARY(UNOP_RECIP_F, UNOP_RECIP_D);
    case S_SQRT:  DISPATCH_FP_UNARY(sqrtf, sqrt);
    case S_EXP2:  DISPATCH_FP_UNARY(exp2f, exp2);
    case S_LOG2:  DISPATCH_FP_UNARY(log2f, log2);
    case S_CMPLT:  DISPATCH_CMP(BINOP_LT);
    case S_CMPEQ:  DISPATCH_CMP(BINOP_EQ);
    case S_SHRINK: {
      // Shift each LOOP range iter by per-axis begin, evaluate the
      // body, restore.  Up to 3 axes via the u16-packed extra.
      u32 nrng = (u32)u->src_count - 1;
      u32 saved[SCALAR_MAX_SRC];
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        u32 begin  = (u32)((u->extra >> (16 * d)) & 0xFFFFu);
        saved[d] = c->range_iter[rng_id];
        c->range_iter[rng_id] = saved[d] + begin;
      }
      u64 v = eval_scalar(c, u->src[0]);
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        c->range_iter[rng_id] = saved[d];
      }
      return v;
    }
    case S_PAD: {
      // Per-axis shifted read with bounds check.  Output is 0
      // (typed zero) when the shifted iter is outside [0, src_dim);
      // otherwise it's the source value at the shifted iter.
      u32 nrng = (u32)u->src_count - 1;
      u32 saved[SCALAR_MAX_SRC];
      int in_bounds = 1;
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id  = u->src[1 + d];
        u32 packed  = (u32)((u->extra >> (16 * d)) & 0xFFFFu);
        u32 begin   = packed & 0xFFu;
        u32 src_dim = (packed >> 8) & 0xFFu;
        u32 iter    = c->range_iter[rng_id];
        // Shift: source coord = iter - begin.  In-bounds iff
        // [0, src_dim).  begin is positive in our encoding so the
        // shift is `iter < begin || iter >= begin + src_dim`.
        if (iter < begin || iter >= begin + src_dim) {
          in_bounds = 0;
        }
        saved[d] = iter;
        c->range_iter[rng_id] = iter - begin;
      }
      u64 v = in_bounds ? eval_scalar(c, u->src[0]) : 0ULL;
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        c->range_iter[rng_id] = saved[d];
      }
      return v;
    }
    case S_FLIP: {
      // Per-axis index reversal.  For each LOOP range src[1+d] with
      // bit d set in extra, replace iter with (extent-1-iter) before
      // evaluating the body, restore after.  No bounds check needed
      // (extent is the LOOP range's own extent, so the reversed iter
      // always stays in-range).
      u32 nrng = (u32)u->src_count - 1;
      u32 saved[SCALAR_MAX_SRC];
      u32 mask = (u32)(u->extra & 0xFFu);
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        saved[d] = c->range_iter[rng_id];
        if (mask & (1u << d)) {
          ScalarUop const *r = &c->ke->scalar_uops[rng_id];
          u32 extent = (u32)(r->extra & 0xFFFFFFFFu);
          c->range_iter[rng_id] = extent - 1u - saved[d];
        }
      }
      u64 v = eval_scalar(c, u->src[0]);
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        c->range_iter[rng_id] = saved[d];
      }
      return v;
    }
    case S_RESHAPE: {
      // Legacy shared-LOOP-refs form.  src[1..nrng) are LOOP ranges
      // used as both input and output via in-place iter shift.
      // extra packs out_dims (low 32, 4xu8) and in_dims (high 32,
      // 4xu8).  Body S_LOAD's strides must match in_dims.
      u32 nrng = (u32)u->src_count - 1;
      u32 saved[SCALAR_MAX_SRC];
      u8 out_dims[MAX_DIM] = {0}, in_dims[MAX_DIM] = {0};
      u32 lo = (u32)(u->extra & 0xFFFFFFFFu);
      u32 hi = (u32)((u->extra >> 32) & 0xFFFFFFFFu);
      for (u32 d = 0; d < 4 && d < MAX_DIM; d++) {
        out_dims[d] = (u8)((lo >> (8 * d)) & 0xFFu);
        in_dims [d] = (u8)((hi >> (8 * d)) & 0xFFu);
      }
      u64 flat_idx = 0;
      u64 stride = 1;
      for (i32 d = (i32)nrng - 1; d >= 0; d--) {
        u32 rng_id = u->src[1 + d];
        saved[d] = c->range_iter[rng_id];
        flat_idx += (u64)saved[d] * stride;
        stride   *= (out_dims[d] != 0 ? out_dims[d] : 1);
      }
      u64 in_stride[MAX_DIM];
      u64 s = 1;
      for (i32 d = (i32)nrng - 1; d >= 0; d--) {
        in_stride[d] = s;
        s *= (in_dims[d] != 0 ? in_dims[d] : 1);
      }
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        u32 dim    = (in_dims[d] != 0 ? in_dims[d] : 1);
        c->range_iter[rng_id] = (u32)((flat_idx / in_stride[d]) % dim);
      }
      u64 v = eval_scalar(c, u->src[0]);
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        c->range_iter[rng_id] = saved[d];
      }
      return v;
    }
    case S_RESHAPE_V: {
      // Split-src form: src[1..1+N_out) are OUTPUT iter refs whose
      // values drive flat_idx; src[1+N_out..src_count) are INPUT range
      // refs whose iters get written from the decomposition.  N_out
      // lives in extra's low byte.  See thvm.h ScalarOp::S_RESHAPE_V.
      //
      // Output refs accept iter expressions.  The expression's value
      // is read via eval_scalar; its extent is recovered from an
      // underlying S_RANGE or from S_IMOD(..., extent).
      u32 n_out = (u32)(u->extra & 0xFFu);
      u32 nrest = (u32)u->src_count - 1;
      if (n_out == 0 || n_out > nrest) return 0;
      u32 n_in  = nrest - n_out;
      u64 flat_idx = 0;
      u64 out_stride = 1;
      for (i32 d = (i32)n_out - 1; d >= 0; d--) {
        u32 ref = u->src[1 + d];
        u32 iter = (u32)eval_scalar(c, ref);
        u32 ext = eval_iter_ref_extent(c, ref);
        flat_idx  += (u64)iter * out_stride;
        out_stride *= (ext != 0 ? ext : 1);
      }
      u32 saved_in [SCALAR_MAX_SRC];
      u32 in_extent[SCALAR_MAX_SRC];
      u64 in_stride[SCALAR_MAX_SRC];
      u64 s = 1;
      for (i32 d = (i32)n_in - 1; d >= 0; d--) {
        u32 rng_id = u->src[1 + n_out + d];
        u32 ext    = (u32)(c->ke->scalar_uops[rng_id].extra & 0xFFFFFFFFu);
        in_extent[d] = (ext != 0 ? ext : 1);
        in_stride[d] = s;
        s *= in_extent[d];
      }
      for (u32 d = 0; d < n_in; d++) {
        u32 rng_id = u->src[1 + n_out + d];
        saved_in[d] = c->range_iter[rng_id];
        c->range_iter[rng_id] =
            (u32)((flat_idx / in_stride[d]) % in_extent[d]);
      }
      u64 v = eval_scalar(c, u->src[0]);
      for (u32 d = 0; d < n_in; d++) {
        u32 rng_id = u->src[1 + n_out + d];
        c->range_iter[rng_id] = saved_in[d];
      }
      return v;
    }
    // === Integer iter-arithmetic (S_I* family) ===========================
    // Each returns an i64 value in the low 64 bits of the u64 result.
    // Sources are other S_I* ops, S_RANGE iters (returned as integer),
    // or S_ICONST.  See thvm.h S_I* comments for the use cases.
    case S_ICONST: return u->extra;
    case S_IADD:   return (u64)((i64)eval_scalar(c, u->src[0]) +
                                (i64)eval_scalar(c, u->src[1]));
    case S_ISUB:   return (u64)((i64)eval_scalar(c, u->src[0]) -
                                (i64)eval_scalar(c, u->src[1]));
    case S_IMUL:   return (u64)((i64)eval_scalar(c, u->src[0]) *
                                (i64)eval_scalar(c, u->src[1]));
    case S_IDIV: {
      i64 b = (i64)eval_scalar(c, u->src[1]);
      if (b == 0) return 0;
      return (u64)((i64)eval_scalar(c, u->src[0]) / b);
    }
    case S_IMOD: {
      i64 b = (i64)eval_scalar(c, u->src[1]);
      if (b == 0) return 0;
      return (u64)((i64)eval_scalar(c, u->src[0]) % b);
    }
    case S_ILT:    return ((i64)eval_scalar(c, u->src[0]) <
                           (i64)eval_scalar(c, u->src[1])) ? 1ULL : 0ULL;
    case S_IAND:   return eval_scalar(c, u->src[0]) & eval_scalar(c, u->src[1]);
    case S_IWHERE: return eval_scalar(c, u->src[0])
                          ? eval_scalar(c, u->src[1])
                          : eval_scalar(c, u->src[2]);
    // Expression-based INDEX: addr = eval_scalar(src[1]).  src[0] is
    // the buffer (DEFINE_PARAM or DEFINE_OUTPUT).  Mirrors S_INDEX's
    // returns: high 32 bits hold the slot id (for DEFINE_PARAM, so
    // S_LOAD/STORE finds the right buffer pointer); low 32 bits hold
    // the address.
    case S_INDEX_E: {
      u32 buf_id = u->src[0];
      ScalarUop const *bu = &c->ke->scalar_uops[buf_id];
      u64 addr = eval_scalar(c, u->src[1]);
      if (bu->op == S_DEFINE_OUTPUT) return addr & 0xFFFFFFFFu;
      u32 slot = (u32)bu->extra;
      return ((u64)slot << 32) | (addr & 0xFFFFFFFFu);
    }
    case S_CAST: {
      // Value-preserving cross-dtype cast.  The source op carries
      // its own dtype; we decode the bits as that type, convert to
      // the C type matching u->dtype, and re-encode.  Dispatch is
      // src_dtype x dst_dtype = O(N^2) but inlined per-pair; the
      // helper macros keep it readable.
      u32 src_id = u->src[0];
      ScalarUop const *su = &c->ke->scalar_uops[src_id];
      u32 src_dtype = su->dtype;
      u64 raw = eval_scalar(c, src_id);

#define CAST_DECODE(T, dst)                                                    \
        T dst; do { memcpy(&dst, &raw, sizeof(T)); } while (0)

      // Step 1: decode `raw` to a C value of the source dtype, into
      // a single double-precision intermediate (sufficient for f32 +
      // all int widths).  i64/u64 lose precision past 2^53; that's
      // the same lossy semantics legacy cpu_op_cast inherits.
      f64 v;
      switch (src_dtype) {
        // Narrow FPs already widened to f32 bits at LOAD time, so
        // decode as f32.
        case DT_FP32:
        case DT_FP16: case DT_BF16:
        case DT_FP8E4M3: case DT_FP8E5M2:
                      { CAST_DECODE(f32, x); v = (f64)x; break; }
        case DT_FP64: { CAST_DECODE(f64, x); v = x; break; }
        case DT_BOOL:
        case DT_UINT8:  { CAST_DECODE(u8 , x); v = (f64)x; break; }
        case DT_UINT16: { CAST_DECODE(u16, x); v = (f64)x; break; }
        case DT_UINT32: { CAST_DECODE(u32, x); v = (f64)x; break; }
        case DT_UINT64: { CAST_DECODE(u64, x); v = (f64)x; break; }
        case DT_INT8:   { CAST_DECODE(i8 , x); v = (f64)x; break; }
        case DT_INT16:  { CAST_DECODE(i16, x); v = (f64)x; break; }
        case DT_INT32:  { CAST_DECODE(i32, x); v = (f64)x; break; }
        case DT_INT64:  { CAST_DECODE(i64, x); v = (f64)x; break; }
        // int4/uint4 already widened by scalar_load_typed; reg
        // holds i32/u32 bits (sign- or zero-extended).
        case DT_INT4:   { CAST_DECODE(i32, x); v = (f64)x; break; }
        case DT_UINT4:  { CAST_DECODE(u32, x); v = (f64)x; break; }
        default: return 0;
      }
#undef CAST_DECODE

      // Step 2: encode v as u->dtype.  Narrow FPs encode as f32
      // bits; STORE later narrows.
      switch (u->dtype) {
        case DT_FP32:
        case DT_FP16: case DT_BF16:
        case DT_FP8E4M3: case DT_FP8E5M2: ENCODE(f32, (f32)v);
        case DT_FP64:   ENCODE(f64, v);
        case DT_BOOL:   ENCODE(u8 , (u8 )(v != 0.0 ? 1 : 0));
        case DT_UINT8:  ENCODE(u8 , (u8 )v);
        case DT_UINT16: ENCODE(u16, (u16)v);
        case DT_UINT32: ENCODE(u32, (u32)v);
        case DT_UINT64: ENCODE(u64, (u64)v);
        case DT_INT8:   ENCODE(i8 , (i8 )v);
        case DT_INT16:  ENCODE(i16, (i16)v);
        case DT_INT32:  ENCODE(i32, (i32)v);
        case DT_INT64:  ENCODE(i64, (i64)v);
        // Sign-/zero-extend to register form so chained CASTs and
        // ALU consumers see the correct scalar.  STORE masks back to
        // a nibble at the buffer boundary.
        case DT_INT4:   ENCODE(i32, I4_SEXT((i32)v));
        case DT_UINT4:  ENCODE(u32, U4_ZEXT((u32)v));
        default: return 0;
      }
    }
    default:
      return 0;
  }
}


fn int cpu_dispatch_scalar(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (cg_kernel_has_extra_outputs(ke)) return -1;
  if (ke->scalar_uops == NULL || ke->n_scalar_uops < 2) return -1;
  // Find the S_BUFFERIZE root.  Its src[0] is the kernel's S_STORE.
  u32 buf_id = 0;
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_BUFFERIZE) { buf_id = i; break; }
  }
  if (buf_id == 0) return -1;
  u32 store_id = ke->scalar_uops[buf_id].src[0];
  if (ke->scalar_uops[store_id].op != S_STORE) return -1;

  // Collect LOOP ranges + their extents from the BUFFERIZE root.
  // BUFFERIZE.src[0..n) = STORE then 1+n LOOP ranges in axis order.
  ScalarUop *bu = &ke->scalar_uops[buf_id];
  u32 n_loops = (u32)bu->src_count - 1;
  if (n_loops > MAX_DIM) return -1;
  u32 loop_ids    [MAX_DIM];
  u32 loop_extents[MAX_DIM];
  for (u32 d = 0; d < n_loops; d++) {
    u32 r = bu->src[1 + d];
    loop_ids    [d] = r;
    loop_extents[d] = (u32)(ke->scalar_uops[r].extra & 0xFFFFFFFFu);
  }

  // Resolve raw input pointers.  We bailed at lower-time on non-
  // contig inputs, so no view pre-mat is needed.
  u32 n_inputs = ke->n_inputs;
  void *in_ptrs_buf[n_inputs ? n_inputs : 1];
  void **in_ptrs = in_ptrs_buf;
  for (u32 i = 0; i < n_inputs; i++) {
    in_ptrs[i] = CPU_BUFS[in_buf_ids[i]].data;
  }

  // Per-op iter slot.  Only S_RANGE op ids are read; reusing one
  // u32 array indexed by op id keeps the dispatcher branch-free.
  u32 *range_iter = (u32 *)calloc(ke->n_scalar_uops, sizeof(u32));
  if (range_iter == NULL) return -1;

  ScalarCtx ctx = {
    .ke          = ke,
    .in_ptrs     = in_ptrs,
    .out_p       = CPU_BUFS[out_buf_id].data,
    .range_iter  = range_iter,
    .odtype      = ke->output_dtype,
  };

  // Outer LOOP nest: iterate every LOOP range.  For Phase B/C we
  // support up to 3 LOOP dims; the BUFFERIZE captured them in
  // canonical row-major order so we can iterate flat by k and
  // decode strides.
  u32 onum = ke->output_numel;
  // Precompute the LOOP strides for k -> per-range-iter decoding.
  u32 loop_strides[MAX_DIM] = {0};
  if (n_loops > 0) {
    loop_strides[n_loops - 1] = 1;
    for (i32 d = (i32)n_loops - 2; d >= 0; d--)
      loop_strides[d] = loop_strides[d + 1] * loop_extents[d + 1];
  }
  for (u32 k = 0; k < onum; k++) {
    for (u32 d = 0; d < n_loops; d++) {
      u32 ext = loop_extents[d];
      u32 str = loop_strides[d];
      range_iter[loop_ids[d]] = (str > 0) ? ((k / str) % ext) : 0;
    }
    // Evaluate the STORE: writes one element to out_p.  Address +
    // value width are both dtype-aware via scalar_store_typed.
    ScalarUop const *st = &ke->scalar_uops[store_id];
    u64 idx_r   = eval_scalar(&ctx, st->src[0]);
    u32 off     = (u32)(idx_r & 0xFFFFFFFFu);
    u64 bits    = eval_scalar(&ctx, st->src[1]);
    scalar_store_typed(ctx.out_p, off, ctx.odtype, bits);
  }

  free(range_iter);
  return 0;
}

static int cpu_tile_enabled(void) {
  char const *e = getenv("THVM_TILE");
  return e != NULL && e[0] == '1';
}

static u32 cpu_scalar_bufferize_root(KernelEntry const *ke) {
  if (ke == NULL || ke->scalar_uops == NULL) {
    return 0;
  }
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_BUFFERIZE) {
      return i;
    }
  }
  return 0;
}

static int cpu_tile_axis_is_output(u32 axis_type) {
  switch (axis_type) {
    case KAX_LOOP:
    case KAX_UPCAST:
    case KAX_LOCAL:
    case KAX_GLOBAL:
      return 1;
    default:
      return 0;
  }
}

fn int cpu_dispatch_tile(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (cg_kernel_has_extra_outputs(ke)) {
    return 0;
  }
  if (!cpu_tile_enabled()) {
    return 0;
  }
  if (!tile_sync_from_scalar(ke)) {
    return 0;
  }

  TilePlanInfo info;
  if (!tile_collect_plan_info(ke, &info)) {
    return 0;
  }
  if (info.mma_tile_id != 0) {
    return 0;
  }

  u32 buf_id = cpu_scalar_bufferize_root(ke);
  if (buf_id == 0) {
    return 0;
  }
  ScalarUop const *bu = &ke->scalar_uops[buf_id];
  u32 n_loops = (u32)bu->src_count - 1;
  if (n_loops > MAX_DIM) {
    return 0;
  }

  u32 loop_ids    [MAX_DIM] = {0};
  u32 loop_extents[MAX_DIM] = {0};
  for (u32 d = 0; d < n_loops; d++) {
    u32 r = bu->src[1 + d];
    if (r == 0 || r >= ke->n_scalar_uops) {
      return 0;
    }
    loop_ids    [d] = r;
    loop_extents[d] = (u32)(ke->scalar_uops[r].extra & 0xFFFFFFFFu);
  }

  u32 n_tile_loops = 0;
  u64 tile_numel = 1;
  for (u32 i = 0; i < info.n_axes; i++) {
    if (!cpu_tile_axis_is_output(info.axis_types[i])) {
      continue;
    }
    if (n_tile_loops >= MAX_AXES || info.axis_extents[i] == 0) {
      return 0;
    }
    n_tile_loops++;
    tile_numel *= info.axis_extents[i];
  }
  if (tile_numel != (u64)(ke->output_numel ? ke->output_numel : 1)) {
    return 0;
  }

  u32 loop_strides[MAX_DIM] = {0};
  if (n_loops > 0) {
    loop_strides[n_loops - 1] = 1;
    for (i32 d = (i32)n_loops - 2; d >= 0; d--) {
      loop_strides[d] = loop_strides[d + 1] * loop_extents[d + 1];
    }
  }

  void *in_ptrs_buf[ke->n_inputs ? ke->n_inputs : 1];
  void **in_ptrs = in_ptrs_buf;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    in_ptrs[i] = CPU_BUFS[in_buf_ids[i]].data;
  }

  u32 *range_iter = (u32 *)calloc(ke->n_scalar_uops, sizeof(u32));
  if (range_iter == NULL) {
    return 0;
  }

  ScalarCtx ctx = {
    .ke          = ke,
    .in_ptrs     = in_ptrs,
    .out_p       = CPU_BUFS[out_buf_id].data,
    .range_iter  = range_iter,
    .odtype      = ke->output_dtype,
  };

  ScalarUop const *st = &ke->scalar_uops[info.scalar_store_id];
  for (u32 k = 0; k < (u32)tile_numel; k++) {
    for (u32 d = 0; d < n_loops; d++) {
      u32 ext = loop_extents[d];
      u32 str = loop_strides[d];
      range_iter[loop_ids[d]] = (str > 0 && ext > 0) ? ((k / str) % ext) : 0;
    }
    u64 idx_r = eval_scalar(&ctx, st->src[0]);
    u32 off   = (u32)(idx_r & 0xFFFFFFFFu);
    u64 bits  = eval_scalar(&ctx, st->src[1]);
    scalar_store_typed(ctx.out_p, off, ctx.odtype, bits);
  }

  free(range_iter);
  return 1;
}

// Forward decls: defined in backend/cpu/{blas,jit,uop_walk}.c (included
// after this file in thvm.c, so declare here for the dispatcher).
fn int cpu_blas_dispatch       (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn int cpu_jit_dispatch        (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn int cpu_uop_walk            (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);

// F6-finish (b): does the per-call gate let cpu_uop_walk fire?
// Default-ON after surgical-suite bit-equal validation. The walker
// fires AHEAD of cpu_jit_dispatch in the dispatch ladder, so every
// kernel the lifter accepts goes through the UOp DAG evaluator -- no
// clang-compile, no warmup gate.
//
// Bisection knob `THVM_CPU_UOP_WALK=0` is RETAINED for diagnosis but
// no longer falls back to a per-op interpreter -- the legacy
// cpu_interpret + cpu/op/*.c fallback was deleted in the F6-cleanup
// wedge.  With the walker disabled, dispatch flows JIT -> scalar_uops
// only; if both decline, cpu_dispatch_kernel returns the
// scalar-uops dispatch failure code (or, when scalar_uops is empty,
// the JIT decline propagates as 0 -- the dispatcher returns its rc
// from the last reachable rung).
static int cpu_uop_walk_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_CPU_UOP_WALK");
    enabled = (e == NULL) ? 1 : (e[0] != '0');
    known = 1;
  }
  return enabled;
}

fn int cpu_dispatch_kernel(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // Recover kid by pointer arithmetic into KERNELS[].  Used for
  // per-kid profiling (cg_profile_record).
  u32 kid = (u32)(ke - KERNELS);
  u64 t0  = cg_now_us();
  // 1. BLAS first: matmul / matvec / dot patterns get cblas_*
  //    (Accelerate on macOS) -- 10-100x faster than anything we can
  //    JIT-compile or scalar-interpret near-term.  Pattern-matches
  //    on KProgOp[]; scalar_uops fallback runs only on no-match.
  int blas_kind = cpu_blas_dispatch(ke, in_buf_ids, out_buf_id);
  if (blas_kind) {
    cg_profile_record(kid, (KDispatchKind)blas_kind, cg_now_us() - t0);
    return 0;
  }
  // 2. TileUop interpreter (env-gated).  The JIT'd tile C path was
  //    deleted along with render_c_scalar; the interpreter remains
  //    as the correctness fallback for elementwise tile plans until
  //    Phase G also collapses TileUop[] into the UOp DAG.
  if (cpu_tile_enabled()) {
    if (cpu_dispatch_tile(ke, in_buf_ids, out_buf_id)) {
      cg_profile_record(kid, KDISPATCH_CPU_TILE, cg_now_us() - t0);
      return 0;
    }
  }
  // 3. UOp DAG walker (F6-finish (b)).  Lifts the kernel via
  //    kernel_lift_to_uop and evaluates the resulting UOp DAG
  //    directly. Mirrors cpu_jit_dispatch's lifter call but skips the
  //    clang-compile + dlopen step, so it amortises faster on
  //    one-shot kernels (no JIT warmup gate). The primary CPU
  //    fallback path now that cpu_interpret + cpu/op/*.c are gone.
  //
  //    Order: AHEAD of cpu_jit_dispatch when THVM_CPU_UOP_WALK=1, so
  //    the walker is exercised for steady-state kernels too. The JIT
  //    path is still reachable when the walker declines (e.g. the
  //    lifter takes a kernel but the walker hits an unsupported op).
  if (cpu_uop_walk_enabled()) {
    if (cpu_uop_walk(ke, in_buf_ids, out_buf_id)) {
      cg_profile_record(kid, KDISPATCH_INTERPRETER, cg_now_us() - t0);
      return 0;
    }
  }
  // 4. KProgOp JIT: clang-compiled fused inner loop for elementwise
  //    chains (cached by program hash).  Faster than the scalar
  //    interpreter for the patterns it covers (no REDUCE > 1, etc.).
  if (cpu_jit_dispatch(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_JIT, cg_now_us() - t0);
    return 0;
  }
  // 5. Rangeify scalar-uops interpreter: the broad fallback that
  //    handles every pattern the WL grid produces (REDUCE, FLIP,
  //    PAD/SHRINK chains, BITCAST, packed nibbles, narrow FPs).
  //    Final rung now that the legacy cpu_interpret per-op fallback
  //    is gone -- the F6 surgical sweep showed walker + scalar_uops
  //    cover every kernel shape the suite produces.
  if (ke->scalar_uops != NULL && ke->n_scalar_uops > 1) {
    int rc = cpu_dispatch_scalar(ke, in_buf_ids, out_buf_id);
    cg_profile_record(kid, KDISPATCH_INTERPRETER, cg_now_us() - t0);
    return rc;
  }
  cg_profile_record(kid, KDISPATCH_INTERPRETER, cg_now_us() - t0);
  return 0;
}
