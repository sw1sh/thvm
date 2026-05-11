// backend/cpu/uop_walk.c -- F6-finish (b): UOp DAG walker interpreter.
//
// Replaces cpu_interpret + cpu/op/*.c on the per-op fallback path.
// Lifts a scheduled kernel to a UOp DAG via kernel_lift_to_uop, then
// walks the DAG evaluating instead of emitting C. Mirrors the
// traversal pattern of cg_render_uop_kernel_c (in render_uop.c) but
// allocates result accumulators and writes the output buffer directly.
//
// Op coverage: UOP_BUFFER, UOP_INDEX_E, UOP_STORE, UOP_AFTER, UOP_RANGE,
// UOP_OPT, UOP_CONST, UOP_IADD/ISUB/IMUL/IDIV/IMOD/ILT/IAND, UOP_IWHERE,
// UOP_INVALID, UOP_ADD/MUL/CMPLT/CMPEQ/NEG/RECIP/EXP2/LOG2/SQRT,
// UOP_CAST, UOP_BITCAST, UOP_REDUCE (SUM/MAX) -- the same set the
// renderer emits.
//
// Wired into cpu_dispatch_kernel between cpu_jit_dispatch and the
// scalar_uops + per-op interpreter fallbacks. Gated initially by
// THVM_CPU_UOP_WALK=1 (defaults off in the first landing); once
// validated bit-equal across the surgical suite the default flips.
//
// LIMITATIONS: f32 only for the float ALU evaluator; integer values
// are evaluated as i64. Mixed-dtype kernels (CAST chains) are
// supported via per-leaf dtype reads/writes; intermediate ALU
// computations promote to double / i64.

#define UWALK_MAX_RANGES   16
#define UWALK_MAX_REDUCES  8
#define UWALK_MAX_INPUTS KERNEL_LIFT_MAX_INPUT
#define UWALK_MAX_OUTPUTS KERNEL_LIFT_MAX_OUTPUT

typedef struct {
  // Range state: indexed by range slot in the kernel's collected
  // range list. Each entry holds the axis_id (used for lookup) and
  // current iteration value.
  u32   axis_id[UWALK_MAX_RANGES];
  u32   iter   [UWALK_MAX_RANGES];
  u32   n_ranges;
  // Reduce accumulators: indexed by reduce slot. Float-only for now.
  Term  red_term[UWALK_MAX_REDUCES];   // identity term that selects this acc
  double red_acc[UWALK_MAX_REDUCES];
  u32   n_reduces;
  // Input/output buffer pointers + dtypes, indexed by slot.
  Term  in_terms [UWALK_MAX_INPUTS];   // UOP_BUFFER terms (for slot match)
  void *in_ptrs  [UWALK_MAX_INPUTS];
  u32   in_dtypes[UWALK_MAX_INPUTS];
  u32   n_inputs;
  // Output buffer table.  Slot 0 is the primary; slots 1..n_outputs-1
  // are extras (multi-output kernel-merge support).  Single-output
  // kernels populate slot 0 only and leave n_outputs == 1.
  Term  out_terms [UWALK_MAX_OUTPUTS];
  void *out_ptrs  [UWALK_MAX_OUTPUTS];
  u32   out_dtypes[UWALK_MAX_OUTPUTS];
  u32   n_outputs;
} UWalkCtx;

// Resolve a UOP_BUFFER term to (ptr, dtype). Returns 1 on hit.
static int uwalk_resolve_buf(UWalkCtx const *c, Term buf, void **out_ptr,
                             u32 *out_dt) {
  // Primary + extra outputs match by Term identity.  Multi-output
  // kernels need this scan to dispatch each STORE to the right
  // backing buffer.
  for (u32 i = 0; i < c->n_outputs; i++) {
    if (c->out_terms[i] == buf) {
      *out_ptr = c->out_ptrs  [i];
      *out_dt  = c->out_dtypes[i];
      return 1;
    }
  }
  for (u32 i = 0; i < c->n_inputs; i++) {
    if (c->in_terms[i] == buf) {
      *out_ptr = c->in_ptrs[i];
      *out_dt  = c->in_dtypes[i];
      return 1;
    }
  }
  // Fallback: instance-based lookup (needed when hash-cons collisions
  // make the term identity unreliable across kernel rebuilds).
  u32 inst = uop_buffer_inst_get(buf);
  if (inst == 0) {
    *out_ptr = c->out_ptrs  [0];
    *out_dt  = c->out_dtypes[0];
    return 1;
  }
  // Extras live in the inst range [KERNEL_LIFT_EXTRA_INST_BASE,
  // KERNEL_LIFT_EXTRA_INST_BASE + KERNEL_MAX_EXTRA_OUTPUTS).
  if (inst >= (1u + KERNEL_LIFT_MAX_INPUT)) {
    u32 ei = inst - (1u + KERNEL_LIFT_MAX_INPUT);
    if (1u + ei < c->n_outputs) {
      *out_ptr = c->out_ptrs  [1 + ei];
      *out_dt  = c->out_dtypes[1 + ei];
      return 1;
    }
    return 0;
  }
  u32 slot = inst - 1;
  if (slot < c->n_inputs) {
    *out_ptr = c->in_ptrs[slot];
    *out_dt  = c->in_dtypes[slot];
    return 1;
  }
  return 0;
}

static u32 uwalk_lookup_iter(UWalkCtx const *c, u32 axis_id) {
  for (u32 i = 0; i < c->n_ranges; i++) {
    if (c->axis_id[i] == axis_id) return c->iter[i];
  }
  return 0;
}

// Read a value at (buf, offset) with dtype dt; returns f32 bits in
// the low half of u64 for float, raw bits otherwise.
//
// `off` is signed (i64) because FLIP-residue input views address
// elements at negative element offsets relative to the view.offset
// pre-bias on the input pointer (F6-finish (b) flip-widen). For
// writes the output is contig so off is always non-negative.
static double uwalk_load_f64(void *p, i64 off, u32 dt) {
  switch (dt) {
    case DT_FP32: { f32 v = ((f32 *)p)[off]; return (double)v; }
    case DT_FP64: { f64 v = ((f64 *)p)[off]; return v; }
    case DT_INT8:  return (double)((i8  *)p)[off];
    case DT_UINT8: return (double)((u8  *)p)[off];
    case DT_INT16: return (double)((i16 *)p)[off];
    case DT_UINT16:return (double)((u16 *)p)[off];
    case DT_INT32: return (double)((i32 *)p)[off];
    case DT_UINT32:return (double)((u32 *)p)[off];
    case DT_INT64: return (double)((i64 *)p)[off];
    case DT_UINT64:return (double)((u64 *)p)[off];
    case DT_BOOL:  return (double)((u8  *)p)[off];
    default:       return 0.0;
  }
}

static i64 uwalk_load_i64(void *p, i64 off, u32 dt) {
  switch (dt) {
    case DT_FP32: { f32 v = ((f32 *)p)[off]; return (i64)v; }
    case DT_FP64: { f64 v = ((f64 *)p)[off]; return (i64)v; }
    case DT_INT8:  return (i64)((i8  *)p)[off];
    case DT_UINT8: return (i64)((u8  *)p)[off];
    case DT_INT16: return (i64)((i16 *)p)[off];
    case DT_UINT16:return (i64)((u16 *)p)[off];
    case DT_INT32: return (i64)((i32 *)p)[off];
    case DT_UINT32:return (i64)((u32 *)p)[off];
    case DT_INT64: return ((i64 *)p)[off];
    case DT_UINT64:return (i64)((u64 *)p)[off];
    case DT_BOOL:  return (i64)((u8  *)p)[off];
    default:       return 0;
  }
}

static int uwalk_dtype_is_float(u32 dt) {
  return dt == DT_FP32 || dt == DT_FP64;
}

static void uwalk_store_f64(void *p, i64 off, u32 dt, double v) {
  switch (dt) {
    case DT_FP32:  ((f32 *)p)[off] = (f32)v; break;
    case DT_FP64:  ((f64 *)p)[off] = v;      break;
    case DT_INT8:  ((i8  *)p)[off] = (i8 )v; break;
    case DT_UINT8: ((u8  *)p)[off] = (u8 )v; break;
    case DT_INT16: ((i16 *)p)[off] = (i16)v; break;
    case DT_UINT16:((u16 *)p)[off] = (u16)v; break;
    case DT_INT32: ((i32 *)p)[off] = (i32)v; break;
    case DT_UINT32:((u32 *)p)[off] = (u32)v; break;
    case DT_INT64: ((i64 *)p)[off] = (i64)v; break;
    case DT_UINT64:((u64 *)p)[off] = (u64)v; break;
    case DT_BOOL:  ((u8  *)p)[off] = (u8)(v != 0.0); break;
    default: break;
  }
}

static void uwalk_store_i64(void *p, i64 off, u32 dt, i64 v) {
  switch (dt) {
    case DT_FP32:  ((f32 *)p)[off] = (f32)v; break;
    case DT_FP64:  ((f64 *)p)[off] = (f64)v; break;
    case DT_INT8:  ((i8  *)p)[off] = (i8 )v; break;
    case DT_UINT8: ((u8  *)p)[off] = (u8 )v; break;
    case DT_INT16: ((i16 *)p)[off] = (i16)v; break;
    case DT_UINT16:((u16 *)p)[off] = (u16)v; break;
    case DT_INT32: ((i32 *)p)[off] = (i32)v; break;
    case DT_UINT32:((u32 *)p)[off] = (u32)v; break;
    case DT_INT64: ((i64 *)p)[off] = v;      break;
    case DT_UINT64:((u64 *)p)[off] = (u64)v; break;
    case DT_BOOL:  ((u8  *)p)[off] = (u8)(v != 0); break;
    default: break;
  }
}

// Forward decls.
static double uwalk_eval_float(UWalkCtx *c, Term t);
static i64    uwalk_eval_int  (UWalkCtx *c, Term t);

// Decide whether to evaluate a term as float vs int. Floats: ADD/MUL/
// CMPLT/CMPEQ/NEG/RECIP/EXP2/LOG2/SQRT/REDUCE/INDEX_E (depending on
// buffer dtype)/CAST(to_float)/BITCAST(to_float)/CONST(float_dtype).
// Ints: RANGE/IADD/.../IWHERE/INVALID/CONST(int_dtype)/CAST(to_int).
static int uwalk_term_is_int(UWalkCtx *c, Term t) {
  if (term_tag(t) == TAG_NUM) return 1;
  if (term_tag(t) != TAG_UOP) return 1;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_RANGE:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD: case UOP_ILT: case UOP_IAND:
    case UOP_IWHERE: case UOP_INVALID:
      return 1;
    case UOP_CONST: {
      u32 dt = term_ext(heap_read(loc));
      return !uwalk_dtype_is_float(dt);
    }
    case UOP_CAST: case UOP_BITCAST: {
      u32 dst = term_val(heap_read(loc + 1));
      return !uwalk_dtype_is_float(dst);
    }
    case UOP_INDEX_E: {
      Term buf = heap_read(loc + 0);
      void *bp; u32 bdt;
      if (uwalk_resolve_buf(c, buf, &bp, &bdt)) {
        return !uwalk_dtype_is_float(bdt);
      }
      return 0;
    }
    case UOP_REDUCE:
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
      return 0;
    case UOP_OPT:
      return uwalk_term_is_int(c, heap_read(loc + 0));
    default:
      return 0;
  }
}

// Look up a hoisted reduce accumulator by REDUCE term identity.
static int uwalk_reduce_lookup(UWalkCtx const *c, Term red, double *out) {
  for (u32 i = 0; i < c->n_reduces; i++) {
    if (c->red_term[i] == red) {
      if (out) *out = c->red_acc[i];
      return (int)i;
    }
  }
  return -1;
}

static double uwalk_eval_float(UWalkCtx *c, Term t) {
  if (term_tag(t) == TAG_NUM) return (double)(u32)term_val(t);
  if (term_tag(t) != TAG_UOP) return 0.0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_CONST: {
      u32 dt   = term_ext(heap_read(loc));
      u32 bits = (u32)term_val(heap_read(loc));
      if (dt == DT_FP32) {
        union { u32 b; f32 f; } pun = { .b = bits };
        return (double)pun.f;
      }
      // Integer const widened to double.
      return (double)(i32)bits;
    }
    case UOP_INDEX_E: {
      Term buf  = heap_read(loc + 0);
      Term addr = heap_read(loc + 1);
      void *bp; u32 bdt;
      if (!uwalk_resolve_buf(c, buf, &bp, &bdt)) return 0.0;
      // FLIP-residue input views address elements at negative element
      // offsets relative to the view.offset pre-bias on bp; pass off
      // through as signed (F6-finish (b) flip-widen).
      i64 off = uwalk_eval_int(c, addr);
      return uwalk_load_f64(bp, off, bdt);
    }
    case UOP_ADD: {
      double a = uwalk_eval_float(c, heap_read(loc + 0));
      double b = uwalk_eval_float(c, heap_read(loc + 1));
      return a + b;
    }
    case UOP_MUL: {
      double a = uwalk_eval_float(c, heap_read(loc + 0));
      double b = uwalk_eval_float(c, heap_read(loc + 1));
      return a * b;
    }
    case UOP_CMPLT: {
      double a = uwalk_eval_float(c, heap_read(loc + 0));
      double b = uwalk_eval_float(c, heap_read(loc + 1));
      return (a < b) ? 1.0 : 0.0;
    }
    case UOP_CMPEQ: {
      double a = uwalk_eval_float(c, heap_read(loc + 0));
      double b = uwalk_eval_float(c, heap_read(loc + 1));
      return (a == b) ? 1.0 : 0.0;
    }
    case UOP_NEG:  return -uwalk_eval_float(c, heap_read(loc + 0));
    case UOP_RECIP:{
      double a = uwalk_eval_float(c, heap_read(loc + 0));
      return 1.0 / a;
    }
    case UOP_EXP2: return exp2(uwalk_eval_float(c, heap_read(loc + 0)));
    case UOP_LOG2: return log2(uwalk_eval_float(c, heap_read(loc + 0)));
    case UOP_SQRT: return sqrt(uwalk_eval_float(c, heap_read(loc + 0)));
    case UOP_CAST: {
      Term src = heap_read(loc + 0);
      u32  dst = term_val(heap_read(loc + 1));
      if (uwalk_dtype_is_float(dst)) return uwalk_eval_float(c, src);
      return (double)uwalk_eval_int(c, src);
    }
    case UOP_BITCAST: {
      Term src = heap_read(loc + 0);
      u32  dst = term_val(heap_read(loc + 1));
      if (dst == DT_FP32) {
        // Reinterpret 32 bits.
        i64 raw = uwalk_eval_int(c, src);
        u32 bits = (u32)raw;
        union { u32 b; f32 f; } pun = { .b = bits };
        return (double)pun.f;
      }
      if (dst == DT_FP64) {
        i64 raw = uwalk_eval_int(c, src);
        u64 bits = (u64)raw;
        union { u64 b; f64 f; } pun = { .b = bits };
        return pun.f;
      }
      return (double)uwalk_eval_int(c, src);
    }
    case UOP_IWHERE: {
      i64 cond = uwalk_eval_int(c, heap_read(loc + 0));
      if (cond) return uwalk_eval_float(c, heap_read(loc + 1));
      return uwalk_eval_float(c, heap_read(loc + 2));
    }
    case UOP_REDUCE: {
      double v = 0.0;
      if (uwalk_reduce_lookup(c, t, &v) >= 0) return v;
      return 0.0;
    }
    case UOP_OPT:
      return uwalk_eval_float(c, heap_read(loc + 0));
    case UOP_INVALID:
      return 0.0;
    default:
      return 0.0;
  }
}

static i64 uwalk_eval_int(UWalkCtx *c, Term t) {
  if (term_tag(t) == TAG_NUM) return (i64)(u32)term_val(t);
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_CONST: {
      u32 dt   = term_ext(heap_read(loc));
      u32 bits = (u32)term_val(heap_read(loc));
      if (dt == DT_FP32) {
        union { u32 b; f32 f; } pun = { .b = bits };
        return (i64)pun.f;
      }
      return (i64)(i32)bits;
    }
    case UOP_RANGE: {
      u32 axis_id = term_val(heap_read(loc + 0));
      return (i64)uwalk_lookup_iter(c, axis_id);
    }
    case UOP_IADD: return uwalk_eval_int(c, heap_read(loc+0))
                       + uwalk_eval_int(c, heap_read(loc+1));
    case UOP_ISUB: return uwalk_eval_int(c, heap_read(loc+0))
                       - uwalk_eval_int(c, heap_read(loc+1));
    case UOP_IMUL: return uwalk_eval_int(c, heap_read(loc+0))
                       * uwalk_eval_int(c, heap_read(loc+1));
    case UOP_IDIV: {
      i64 b = uwalk_eval_int(c, heap_read(loc+1));
      if (b == 0) return 0;
      return uwalk_eval_int(c, heap_read(loc+0)) / b;
    }
    case UOP_IMOD: {
      i64 b = uwalk_eval_int(c, heap_read(loc+1));
      if (b == 0) return 0;
      return uwalk_eval_int(c, heap_read(loc+0)) % b;
    }
    case UOP_ILT:  return (uwalk_eval_int(c, heap_read(loc+0))
                        <  uwalk_eval_int(c, heap_read(loc+1))) ? 1 : 0;
    case UOP_IAND: return uwalk_eval_int(c, heap_read(loc+0))
                        & uwalk_eval_int(c, heap_read(loc+1));
    case UOP_IWHERE: {
      i64 cond = uwalk_eval_int(c, heap_read(loc+0));
      return cond ? uwalk_eval_int(c, heap_read(loc+1))
                  : uwalk_eval_int(c, heap_read(loc+2));
    }
    case UOP_INVALID: return 0;
    case UOP_INDEX_E: {
      Term buf = heap_read(loc + 0);
      Term addr = heap_read(loc + 1);
      void *bp; u32 bdt;
      if (!uwalk_resolve_buf(c, buf, &bp, &bdt)) return 0;
      // Signed offset: see uwalk_eval_float's UOP_INDEX_E for rationale.
      i64 off = uwalk_eval_int(c, addr);
      return uwalk_load_i64(bp, off, bdt);
    }
    case UOP_CAST: {
      Term src = heap_read(loc + 0);
      u32  dst = term_val(heap_read(loc + 1));
      if (uwalk_dtype_is_float(dst)) return (i64)uwalk_eval_float(c, src);
      return uwalk_eval_int(c, src);
    }
    case UOP_BITCAST: {
      Term src = heap_read(loc + 0);
      u32  dst = term_val(heap_read(loc + 1));
      if (dst == DT_INT32 || dst == DT_UINT32) {
        double f = uwalk_eval_float(c, src);
        union { u32 b; f32 f; } pun;
        pun.f = (f32)f;
        return (i64)(i32)pun.b;
      }
      return uwalk_eval_int(c, src);
    }
    case UOP_OPT:
      return uwalk_eval_int(c, heap_read(loc + 0));
    case UOP_REDUCE: {
      double v = 0.0;
      if (uwalk_reduce_lookup(c, t, &v) >= 0) return (i64)v;
      return 0;
    }
    default:
      return 0;
  }
}

// Collect unique RANGE leaves from a term tree (mirrors
// rmu_collect_ranges in render_uop.c). axis_id-deduped; up to
// UWALK_MAX_RANGES entries.
static void uwalk_collect_ranges(Term t, Term *ranges, u32 *n_out) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= UWALK_MAX_RANGES) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 axis_id = term_val(heap_read(loc + 0));
    for (u32 i = 0; i < *n_out; i++) {
      u32 existing = term_val(heap_read(term_val(ranges[i]) + 0));
      if (existing == axis_id) return;
    }
    ranges[*n_out] = t;
    (*n_out)++;
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      uwalk_collect_ranges(heap_read(loc + 0), ranges, n_out);
      uwalk_collect_ranges(heap_read(loc + 1), ranges, n_out);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      uwalk_collect_ranges(heap_read(loc + 0), ranges, n_out);
      return;
    case UOP_IWHERE:
      uwalk_collect_ranges(heap_read(loc + 0), ranges, n_out);
      uwalk_collect_ranges(heap_read(loc + 1), ranges, n_out);
      uwalk_collect_ranges(heap_read(loc + 2), ranges, n_out);
      return;
    default:
      return;
  }
}

// Collect REDUCE nodes (mirrors rmu_collect_reduces). Doesn't recurse
// into the reduce body.
static void uwalk_collect_reduces(Term t, Term *reduces, u32 *n_out) {
  if (term_tag(t) != TAG_UOP) return;
  if (*n_out >= UWALK_MAX_REDUCES) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) {
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) return;
    }
    reduces[*n_out] = t;
    (*n_out)++;
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      uwalk_collect_reduces(heap_read(loc + 0), reduces, n_out);
      uwalk_collect_reduces(heap_read(loc + 1), reduces, n_out);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      uwalk_collect_reduces(heap_read(loc + 0), reduces, n_out);
      return;
    case UOP_IWHERE:
      uwalk_collect_reduces(heap_read(loc + 0), reduces, n_out);
      uwalk_collect_reduces(heap_read(loc + 1), reduces, n_out);
      uwalk_collect_reduces(heap_read(loc + 2), reduces, n_out);
      return;
    default:
      return;
  }
}

// Compute one reduce accumulator over its axis range.
static double uwalk_run_reduce(UWalkCtx *c, Term red) {
  u64 rloc      = term_val(red);
  Term r_src    = heap_read(rloc + 0);
  u32  r_kind   = term_val(heap_read(rloc + 1));
  u32  r_axis   = term_val(heap_read(rloc + 2));
  // Find the RANGE term for this axis in the body (extent).
  Term src_ranges[UWALK_MAX_RANGES];
  u32  n_src_r = 0;
  uwalk_collect_ranges(r_src, src_ranges, &n_src_r);
  u32 r_extent = 0;
  for (u32 i = 0; i < n_src_r; i++) {
    u32 axis = term_val(heap_read(term_val(src_ranges[i]) + 0));
    if (axis == r_axis) {
      r_extent = term_val(heap_read(term_val(src_ranges[i]) + 2));
      break;
    }
  }
  if (r_extent == 0) {
    // Match render_uop init: SUM=0, MAX=-INFINITY (no contributions).
    return (r_kind == REDUCE_MAX) ? -INFINITY : 0.0;
  }
  // Push a fresh range slot for the reduce axis.
  if (c->n_ranges >= UWALK_MAX_RANGES) return 0.0;
  u32 slot = c->n_ranges++;
  c->axis_id[slot] = r_axis;
  double acc = (r_kind == REDUCE_MAX) ? -INFINITY : 0.0;
  for (u32 k = 0; k < r_extent; k++) {
    c->iter[slot] = k;
    double v = uwalk_eval_float(c, r_src);
    if (r_kind == REDUCE_MAX) acc = (v > acc) ? v : acc;
    else                      acc = acc + v;
  }
  c->n_ranges--;
  return acc;
}

// Find the output-axis ranges (those that appear in addr) vs reduce
// ranges. Mirrors rmu_emit_store / rmu_emit_store_reduce's logic.
//
// `reduce_axis` is the axis_id of a REDUCE-as-store-value pattern
// (UINT32_MAX if not). Output ranges fill out_ranges; the reduce
// range, if found, is excluded.
static void uwalk_split_ranges(Term *all_ranges, u32 n_all,
                               u32 reduce_axis,
                               Term *out_ranges, u32 *n_out) {
  *n_out = 0;
  for (u32 i = 0; i < n_all; i++) {
    u32 aid = term_val(heap_read(term_val(all_ranges[i]) + 0));
    if (aid == reduce_axis) continue;
    if (*n_out < UWALK_MAX_RANGES) out_ranges[(*n_out)++] = all_ranges[i];
  }
}

// Push a range slot, returning the slot index. Returns 0 on overflow
// (caller should detect via n_ranges before calling).
static u32 uwalk_push_range_slot(UWalkCtx *c, u32 axis_id) {
  if (c->n_ranges >= UWALK_MAX_RANGES) return UWALK_MAX_RANGES;
  u32 slot = c->n_ranges++;
  c->axis_id[slot] = axis_id;
  c->iter   [slot] = 0;
  return slot;
}

static void uwalk_pop_range_slot(UWalkCtx *c) {
  if (c->n_ranges > 0) c->n_ranges--;
}

// Recursively iterate a list of output ranges. At the innermost level,
// runs hoistable reduces (those independent of output axes are run
// once before this), then evaluates value and writes to (out, addr).
//
// `range_extents` parallels `out_ranges`. `range_slots` maps each
// out_range to its ctx slot index.
static void uwalk_loop_and_store(UWalkCtx *c,
                                 Term *out_ranges, u32 *range_extents,
                                 u32 *range_slots,
                                 u32 n_out, u32 depth,
                                 Term store_buf, Term store_addr,
                                 Term store_value,
                                 void *out_p, u32 out_dt,
                                 Term *value_reduces, int *hoistable,
                                 u32 n_reduces) {
  if (depth == n_out) {
    // Run all reduces. Hoistable ones were precomputed; non-hoistable
    // ones rerun here per output position (they reference output
    // axes).
    for (u32 i = 0; i < n_reduces; i++) {
      if (hoistable[i]) continue;
      int idx = uwalk_reduce_lookup(c, value_reduces[i], NULL);
      if (idx < 0) {
        // First time: register slot.
        if (c->n_reduces >= UWALK_MAX_REDUCES) return;
        idx = (int)c->n_reduces++;
        c->red_term[idx] = value_reduces[i];
      }
      c->red_acc[idx] = uwalk_run_reduce(c, value_reduces[i]);
    }
    // Evaluate address (always integer).
    i64 addr_v = uwalk_eval_int(c, store_addr);
    if (addr_v < 0) {
      (void)store_buf;
      return;
    }
    if (uwalk_dtype_is_float(out_dt)) {
      double v = uwalk_eval_float(c, store_value);
      uwalk_store_f64(out_p, addr_v, out_dt, v);
    } else {
      i64 v = uwalk_eval_int(c, store_value);
      uwalk_store_i64(out_p, addr_v, out_dt, v);
    }
    return;
  }
  u32 ext = range_extents[depth];
  u32 slot = range_slots[depth];
  for (u32 k = 0; k < ext; k++) {
    c->iter[slot] = k;
    uwalk_loop_and_store(c, out_ranges, range_extents, range_slots,
                         n_out, depth + 1, store_buf, store_addr,
                         store_value, out_p, out_dt,
                         value_reduces, hoistable, n_reduces);
  }
}

// Drive a single STORE through the walker.
static int uwalk_emit_store(UWalkCtx *c, Term store) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  u64 sloc = term_val(store);
  Term store_buf  = heap_read(sloc + 0);
  Term store_addr = heap_read(sloc + 1);
  Term store_val  = heap_read(sloc + 2);
  // Resolve output buffer.
  void *out_p; u32 out_dt;
  if (!uwalk_resolve_buf(c, store_buf, &out_p, &out_dt)) return 0;
  // Detect REDUCE-as-store-value (possibly wrapped in OPT(_, TC, _)).
  Term value_for_emit = store_val;
  Term reduce_as_value = 0;
  if (term_tag(store_val) == TAG_UOP && term_ext(store_val) == UOP_OPT) {
    Term inner = heap_read(term_val(store_val) + 0);
    if (term_tag(inner) == TAG_UOP && term_ext(inner) == UOP_REDUCE) {
      reduce_as_value = inner;
      value_for_emit  = inner;
    }
  } else if (term_tag(store_val) == TAG_UOP && term_ext(store_val) == UOP_REDUCE) {
    reduce_as_value = store_val;
  }
  // Collect ranges from addr + value.
  Term all_ranges[UWALK_MAX_RANGES];
  u32 n_all = 0;
  uwalk_collect_ranges(store_addr,    all_ranges, &n_all);
  uwalk_collect_ranges(value_for_emit, all_ranges, &n_all);
  // For REDUCE-as-value, run reduce body to also pick up ranges in src.
  if (reduce_as_value != 0) {
    Term r_src = heap_read(term_val(reduce_as_value) + 0);
    uwalk_collect_ranges(r_src, all_ranges, &n_all);
  }
  u32 reduce_axis = 0xFFFFFFFFu;
  if (reduce_as_value != 0) {
    reduce_axis = term_val(heap_read(term_val(reduce_as_value) + 2));
  }
  // Output ranges = all_ranges minus the reduce axis (if any).
  Term out_ranges[UWALK_MAX_RANGES];
  u32 n_out = 0;
  uwalk_split_ranges(all_ranges, n_all, reduce_axis, out_ranges, &n_out);
  // For each output range push a slot and record extent.
  u32 range_extents[UWALK_MAX_RANGES] = {0};
  u32 range_slots  [UWALK_MAX_RANGES] = {0};
  for (u32 i = 0; i < n_out; i++) {
    Term r = out_ranges[i];
    u32 axis_id = term_val(heap_read(term_val(r) + 0));
    u32 ext     = term_val(heap_read(term_val(r) + 2));
    u32 slot = uwalk_push_range_slot(c, axis_id);
    if (slot >= UWALK_MAX_RANGES) {
      // Overflow: pop already-pushed slots and bail.
      for (u32 j = 0; j < i; j++) uwalk_pop_range_slot(c);
      return 0;
    }
    range_extents[i] = ext;
    range_slots  [i] = slot;
  }
  // Determine the actual value expression to evaluate at the
  // innermost loop. For REDUCE-as-store-value we synthesise a
  // placeholder REDUCE reference (the reduce will be emitted as a
  // non-hoistable reduce).
  Term value_term   = (reduce_as_value != 0) ? reduce_as_value : store_val;
  // Collect reduces in the value expression.
  Term value_reduces[UWALK_MAX_REDUCES];
  u32  n_reduces = 0;
  uwalk_collect_reduces(value_term, value_reduces, &n_reduces);
  // For each reduce, determine if hoistable: body has no range that
  // matches an output axis.
  int hoistable[UWALK_MAX_REDUCES] = {0};
  for (u32 i = 0; i < n_reduces; i++) {
    Term r_src = heap_read(term_val(value_reduces[i]) + 0);
    Term body_ranges[UWALK_MAX_RANGES];
    u32  n_br = 0;
    uwalk_collect_ranges(r_src, body_ranges, &n_br);
    int uses_out = 0;
    for (u32 j = 0; j < n_br; j++) {
      u32 aid = term_val(heap_read(term_val(body_ranges[j]) + 0));
      for (u32 k = 0; k < n_out; k++) {
        u32 oaid = term_val(heap_read(term_val(out_ranges[k]) + 0));
        if (oaid == aid) { uses_out = 1; break; }
      }
      if (uses_out) break;
    }
    hoistable[i] = !uses_out;
  }
  // Run hoistable reduces once before the output loops.
  for (u32 i = 0; i < n_reduces; i++) {
    if (!hoistable[i]) continue;
    if (c->n_reduces >= UWALK_MAX_REDUCES) {
      for (u32 j = 0; j < n_out; j++) uwalk_pop_range_slot(c);
      return 0;
    }
    u32 idx = c->n_reduces++;
    c->red_term[idx] = value_reduces[i];
    c->red_acc [idx] = uwalk_run_reduce(c, value_reduces[i]);
  }
  // Run nested for-loops over out_ranges.
  uwalk_loop_and_store(c, out_ranges, range_extents, range_slots,
                       n_out, 0,
                       store_buf, store_addr, value_term,
                       out_p, out_dt,
                       value_reduces, hoistable, n_reduces);
  // Pop range slots + reduces created here.
  for (u32 i = 0; i < n_out; i++) uwalk_pop_range_slot(c);
  // Drop reduce slots we pushed (hoistable + non-hoistable per-iter).
  // Non-hoistable reduces are rebuilt each iter so they may leak slots
  // if not popped here too. Keep it simple: reset both counts to a
  // baseline taken at entry.
  // (Simpler: count-based reset since this STORE is the topmost call.)
  c->n_reduces = 0;
  return 1;
}

// Walk an UOP_AFTER chain (mirrors rmu_emit_after) -- emits each store
// in chain order. The walker doesn't need barriers (single-threaded).
static int uwalk_emit_after(UWalkCtx *c, Term after);

static int uwalk_emit_node(UWalkCtx *c, Term node) {
  if (term_tag(node) != TAG_UOP) return 0;
  u32 op = term_ext(node);
  if (op == UOP_STORE) return uwalk_emit_store(c, node);
  if (op == UOP_AFTER) return uwalk_emit_after(c, node);
  return 0;
}

static int uwalk_emit_after(UWalkCtx *c, Term after) {
  if (term_tag(after) != TAG_UOP || term_ext(after) != UOP_AFTER) return 0;
  u64 loc = term_val(after);
  Term node       = heap_read(loc + 0);
  Term after_node = heap_read(loc + 1);
  if (!uwalk_emit_node(c, after_node)) return 0;
  if (!uwalk_emit_node(c, node))       return 0;
  return 1;
}

// Resolve a CPU input slot to (raw buffer pointer, base offset).
//
// IMPORTANT: the walker mirrors the lifter's view.strides addressing
// (kernel_lift_to_uop's INDEX_E lift uses ke->input_views[slot].strides[d]
// directly, F3.5 fix 099c78f6). That means UOP_INDEX_E expressions
// reference offsets relative to the View's `offset` (not the start of
// the contig storage). Pre-materializing here would lose the view.offset
// shift and double-apply the strides; we instead pass the RAW underlying
// buffer pointer biased by view.offset so the lifter's view-stride
// addressing lands on the correct elements.
//
// Returns 0; never bails (pre-mat path would have for packed nibbles
// but the walker doesn't pre-mat -- packed dtypes are out of scope for
// the F6-finish landing and surface in cg_supports gating instead).
static int uwalk_resolve_input_ptr(KernelEntry *ke, u32 slot, u32 buf_id,
                                   void **out_ptr) {
  void *raw = CPU_BUFS[buf_id].data;
  u32 tid = ke->input_tids[slot];
  if (tid == 0 || tid >= TENS_NEXT) {
    *out_ptr = raw;
    return 0;
  }
  TenDesc const *td = &TENS[tid];
  // ShapeTracker-chain composed into the INDEX (input_chain_composed):
  // the emitted address already folds the FULL chain -- the public
  // view's strides+offset AND every inner prior_views step -- down to a
  // raw-buffer element index.  Biasing by view.offset here would
  // double-apply it; pass the raw buffer pointer untouched.
  if (ke->input_chain_composed != NULL && slot < ke->n_inputs
      && ke->input_chain_composed[slot]) {
    *out_ptr = raw;
    return 0;
  }
  // Bias the pointer by view.offset * itemsize. The lifter's
  // view-stride addressing produces (sum of iter * stride[d]) without
  // the offset, expecting the caller to bias the base pointer.
  // Itemsize from dtype; only standard (non-packed) types reach here.
  u32 esz = dtype_itemsize(td->dtype);
  if (esz == 0 || dtype_is_packed(td->dtype)) {
    *out_ptr = raw;
    return 0;
  }
  *out_ptr = (void *)((u8 *)raw + (size_t)td->view.offset * esz);
  return 0;
}

// === Public entry point ===
//
// Returns 1 if the walker handled the kernel (output buffer written),
// 0 if it declined (caller should fall back).
fn int cpu_uop_walk(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // Diagnostic env knob: print one line per walker entry summarising
  // lift outcome + root op. Mirrors THVM_CPU_INTERPRET_TRACE; useful
  // for measuring how many kernels the walker covers.
  static int trace_known = 0, trace_on = 0;
  if (!trace_known) {
    char const *e = getenv("THVM_CPU_UOP_WALK_TRACE");
    trace_on = (e != NULL && e[0] == '1');
    trace_known = 1;
  }
  // F6 multi-output: walker now handles n_extra_outputs > 0 kernels
  // when the lifter (kernel_lift_from_kprog) emitted a STORE-AFTER
  // chain for them.  store_root==0 means the lift declined; the
  // walker can't help in that case (legacy fallback runs).
  if (ke->cached_lift.store_root == 0) {
    if (trace_on) fprintf(stderr, "uop_walk: lift declined n_inputs=%u n_ops=%u "
                          "n_extra=%u\n",
                          ke->n_inputs, ke->n_ops, (u32)ke->n_extra_outputs);
    return 0;
  }
  KernelUopLift const *lift = &ke->cached_lift;
  if (lift->n_inputs > UWALK_MAX_INPUTS) return 0;
  UWalkCtx ctx = {0};
  ctx.n_inputs  = lift->n_inputs;
  // Populate primary output (slot 0).
  ctx.out_terms [0] = lift->out_buf;
  ctx.out_ptrs  [0] = CPU_BUFS[out_buf_id].data;
  ctx.out_dtypes[0] = uop_buffer_dtype(lift->out_buf);
  if (ctx.out_dtypes[0] == 0) ctx.out_dtypes[0] = ke->output_dtype;
  ctx.n_outputs = 1;
  // Populate extras: each extra_output_tids[ei] resolves to a CpuBuf
  // via the producer-kid wiring set up by splice_child_into_host.
  // The lift records out_bufs[1+ei] (UOP_BUFFER Term) so STOREs in the
  // AFTER chain dispatch correctly via uwalk_resolve_buf's identity
  // match on the Term.
  if (lift->n_outputs > 1) {
    if (lift->n_outputs > UWALK_MAX_OUTPUTS) return 0;
    for (u32 ei = 0; ei < lift->n_outputs - 1; ei++) {
      if (ei >= ke->n_extra_outputs) return 0;
      u32 extra_tid = ke->extra_output_tids[ei];
      if (extra_tid == 0 || extra_tid >= TENS_NEXT) return 0;
      u32 buf_id = TENS[extra_tid].buf_id;
      if (buf_id == 0 || buf_id >= CPU_BUFS_NEXT) return 0;
      ctx.out_terms [1 + ei] = lift->out_bufs[1 + ei];
      ctx.out_ptrs  [1 + ei] = CPU_BUFS[buf_id].data;
      ctx.out_dtypes[1 + ei] = ke->extra_output_dtypes[ei];
      if (ctx.out_dtypes[1 + ei] == 0) {
        ctx.out_dtypes[1 + ei] = uop_buffer_dtype(lift->out_bufs[1 + ei]);
      }
      ctx.n_outputs++;
    }
  }
  // Resolve raw pointers (no pre-mat -- the lifter's view-stride
  // addressing reads the underlying buffer directly).
  for (u32 i = 0; i < lift->n_inputs; i++) {
    ctx.in_terms [i] = lift->in_bufs[i];
    ctx.in_dtypes[i] = uop_buffer_dtype(lift->in_bufs[i]);
    if (ctx.in_dtypes[i] == 0
        && ke->input_dtypes != NULL && i < ke->n_inputs) {
      ctx.in_dtypes[i] = ke->input_dtypes[i];
    }
    void *p = NULL;
    if (uwalk_resolve_input_ptr(ke, i, in_buf_ids[i], &p) < 0) {
      return 0;
    }
    ctx.in_ptrs[i] = p;
  }
  // Walk the root.
  Term root = lift->store_root;
  int ok = 0;
  if (root != 0 && term_tag(root) == TAG_UOP) {
    u32 op = term_ext(root);
    if      (op == UOP_STORE) ok = uwalk_emit_store(&ctx, root);
    else if (op == UOP_AFTER) ok = uwalk_emit_after(&ctx, root);
    if (trace_on) fprintf(stderr,
                          "uop_walk: n_inputs=%u root_op=%u rc=%d\n",
                          ke->n_inputs, op, ok);
  } else if (trace_on) {
    fprintf(stderr, "uop_walk: empty/non-UOP root tag=%u\n",
            (unsigned)term_tag(root));
  }
  return ok;
}
