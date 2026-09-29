// backend/cpu/uop_walk.c -- F6-finish (b): UOp DAG walker interpreter.
//
// Replaces cpu_interpret + cpu/op/*.c on the per-op fallback path.
// Lifts a scheduled kernel to a UOp DAG via kernel_lift_to_uop, then
// walks the DAG evaluating instead of emitting C. Mirrors the
// traversal pattern of cg_render_uop_kernel_c (in render_uop.c) but
// allocates result accumulators and writes the output buffer directly.
//
// Op coverage: UOP_BUFFER, UOP_INDEX_E, UOP_STORE, UOP_AFTER, UOP_RANGE,
// UOP_OPT, UOP_CONST, UOP_IADD/ISUB/IMUL/IDIV/IMOD/ILT/IAND/IOR/IXOR, UOP_IWHERE,
// UOP_INVALID, UOP_ADD/MUL/CMPLT/CMPEQ/NEG/RECIP/EXP2/LOG2/SQRT,
// UOP_CAST, UOP_BITCAST, UOP_REDUCE (SUM/MAX) -- the same set the
// renderer emits.
//
// Wired into cpu_dispatch_kernel after cpu_blas_dispatch.  The
// scalar-UOp interpreter fallback (cpu_dispatch_scalar) and the
// tile interpreter (cpu_dispatch_tile) were deleted once the
// walker's coverage reached every kernel shape the suite produces.
// Default-ON; THVM_CPU_UOP_WALK=0 retained for bisection but the
// only remaining downstream fallback is cpu_jit_dispatch.
//
// Each subterm walks in the evaluator its own dtype picks
// (uwalk_term_dtype), as the rendered C types it: a float in doubles, an
// int or bool in i64, a comparison in its operands' dtype (a u64
// unsigned).  A CAST converts as the C emit's `((T)x)` does, and an int
// kernel with a float body converts that body's value as C does.  An int
// or bool REDUCE accumulates exactly in its dtype (uwalk_run_reduce).
//
// LIMITATIONS: float ALU results stay doubles, not rounded to f32 per op;
// int ALU results stay i64, wrapped to a narrower dtype only by a CAST, a
// REDUCE or the store, so a u32 or i32 sum that overflows its width feeds
// a comparison unwrapped.

#define UWALK_MAX_RANGES   16
#define UWALK_MAX_REDUCES  64

// A REDUCE's value.  An int or bool reduce keeps its exact value `i` in
// its dtype (two's complement for u64), and `f` converts it; a float
// reduce has only `f`.
typedef struct {
  double f;
  i64    i;
  int    is_int;
} UWalkRed;

// THVM_TRACE_UOP_WALK_DECLINE=1 emits one stderr line per decline
// site, classified by short tag + detail. Off by default; one cached
// getenv on first call. Stable prefix `THVM_UOP_WALK_DECLINE` so
// `grep | sort | uniq -c` over a suite tabulates the decline taxonomy.
static int uwalk_decline_trace_enabled(void) {
  static int known = 0, on = 0;
  if (!known) {
    char const *e = getenv("THVM_TRACE_UOP_WALK_DECLINE");
    on = (e != NULL && e[0] != '0');
    known = 1;
  }
  return on;
}

// The reason of the latest decline, which cpu_dispatch_kernel names when
// no dispatch rung ran.  cpu_uop_walk clears it on entry.
static char const *UWALK_LAST_DECLINE;

fn char const *cpu_uop_walk_last_decline(void) {
  return UWALK_LAST_DECLINE != NULL ? UWALK_LAST_DECLINE : "none";
}

#define UWALK_DECLINE(kid, reason, fmt, ...)                              \
  do {                                                                    \
    UWALK_LAST_DECLINE = (reason);                                        \
    if (uwalk_decline_trace_enabled()) {                                  \
      fprintf(stderr,                                                     \
              "THVM_UOP_WALK_DECLINE kid=%u reason=\"%s\" detail=\""      \
              fmt "\"\n",                                                 \
              (unsigned)(kid), (reason), ##__VA_ARGS__);                  \
    }                                                                     \
  } while (0)

typedef struct {
  // Range state: indexed by range slot in the kernel's collected
  // range list. Each entry holds the axis_id (used for lookup) and
  // current iteration value.
  u32   axis_id[UWALK_MAX_RANGES];
  u32   iter   [UWALK_MAX_RANGES];
  u32   n_ranges;
  // Reduce results, indexed by reduce slot.
  Term     red_term[UWALK_MAX_REDUCES];   // identity term that selects this acc
  UWalkRed red_acc [UWALK_MAX_REDUCES];
  // Iter-aware cache validation: each cached REDUCE result carries the
  // FREE-axis iter snapshot at the moment of compute.  A subsequent
  // eval_float of the same REDUCE term reuses the cached value iff the
  // current iter for every free axis matches the snapshot.  Without
  // this, an inner REDUCE inside another REDUCE's body is recomputed
  // on every outer iteration even when its body doesn't depend on the
  // outer axis -- explodes the per-output cost by ~Π(outer extents).
  u8    red_n_free   [UWALK_MAX_REDUCES];                       // valid axes in snapshot
  u32   red_free_ids [UWALK_MAX_REDUCES][UWALK_MAX_RANGES];     // axis_id per snapshot slot
  u32   red_free_iter[UWALK_MAX_REDUCES][UWALK_MAX_RANGES];     // iter value at cache time
  u32   n_reduces;
  // Split-inner axes of the current store: the UNROLL/UPCAST half of a
  // hand_opts-split reduce axis, which REDUCE.axes does not name.  Each is
  // iterated by the reduce owning it (uop_reduce_owns_split_axis), never as
  // a store loop.  An owner of 0 (no split composition found) makes every
  // reduce whose body holds the axis iterate it, as the renderer does.
  u32   split_aid  [UWALK_MAX_RANGES];
  Term  split_owner[UWALK_MAX_RANGES];
  u32   n_split;
  // Input buffer terms (for slot match), pointers and dtypes, indexed by
  // slot: heap arrays of n_inputs entries, so a kernel may take any
  // number of inputs without growing the stack.
  Term  *in_terms;
  void **in_ptrs;
  u32   *in_dtypes;
  u32    n_inputs;
  // Single output buffer (every emitted kernel writes one buffer).
  Term  out_term;
  void *out_ptr;
  u32   out_dtype;
  // 1 while the current store writes an int or bool buffer from a float
  // value, which converts as the rendered C store `out[i] = v` does.
  int   store_from_float;
  // Kid (index into KERNELS[]) of the currently-walking entry, used
  // only for THVM_TRACE_UOP_WALK_DECLINE telemetry. Set by
  // cpu_uop_walk before any uwalk_emit_* helper runs.
  u32   kid;
} UWalkCtx;

// Resolve a UOP_BUFFER term to (ptr, dtype). Returns 1 on hit.
static int uwalk_resolve_buf(UWalkCtx const *c, Term buf, void **out_ptr,
                             u32 *out_dt) {
  if (c->out_term == buf) {
    *out_ptr = c->out_ptr;
    *out_dt  = c->out_dtype;
    return 1;
  }
  for (u32 i = 0; i < c->n_inputs; i++) {
    if (c->in_terms[i] == buf) {
      *out_ptr = c->in_ptrs[i];
      *out_dt  = c->in_dtypes[i];
      return 1;
    }
  }
  // Fallback: instance-based lookup (needed when hash-cons collisions
  // make the term identity unreliable across kernel rebuilds).  This
  // path is ONLY valid for UOP_BUFFER terms -- uop_buffer_inst_get
  // returns 0 for any non-UOP_BUFFER tag (UOP_BUFFERIZE, TAG_TEN), and
  // routing those through the inst==0 -> out_ptr branch silently aliases
  // the (typically large) BUFFERIZE-shaped read to the (typically scalar)
  // output buffer, producing out-of-bounds reads of adjacent heap memory
  // -- the BN-fused-reduce kernel's mean BUFFERIZE hits this exact case
  // and the chain-fusion walker amplifies the garbage into per-step
  // divergence (beautiful_mnist step 2 diverges).  Restrict the fallback
  // to genuine UOP_BUFFER terms so a BUFFERIZE left inlined in the
  // kernel body causes the dispatcher to bail (INDEX_E sees a 0 ptr and
  // returns 0.0) rather than reading wherever out_ptr happens to live.
  if (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFER) {
    u32 inst = uop_buffer_inst_get(buf);
    if (inst == 0) {
      *out_ptr = c->out_ptr;
      *out_dt  = c->out_dtype;
      return 1;
    }
    u32 slot = inst - 1;
    if (slot < c->n_inputs) {
      *out_ptr = c->in_ptrs[slot];
      *out_dt  = c->in_dtypes[slot];
      return 1;
    }
  }
  return 0;
}

static u32 uwalk_lookup_iter(UWalkCtx const *c, u32 axis_id) {
  // Walk last-to-first so the most-recently pushed slot wins.  This
  // gives correct innermost-binding semantics for nested reduces: when
  // an outer REDUCE iterates its axis a1 and its body's inner REDUCE
  // iterates a2, the inner's a2 slot (pushed last) shadows any earlier
  // a2 slot pushed by the output loop.
  for (u32 i = c->n_ranges; i > 0; i--) {
    if (c->axis_id[i - 1] == axis_id) return c->iter[i - 1];
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
    case DT_FP16:  return (double)fp16_to_f32(((u16 *)p)[off]);
    case DT_BF16:  return (double)bf16_to_f32(((u16 *)p)[off]);
    case DT_FP8E4M3: return (double)fp8e4m3_to_f32(((u8 *)p)[off]);
    case DT_FP8E5M2: return (double)fp8e5m2_to_f32(((u8 *)p)[off]);
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
    case DT_FP16:  return (i64)fp16_to_f32(((u16 *)p)[off]);
    case DT_BF16:  return (i64)bf16_to_f32(((u16 *)p)[off]);
    case DT_FP8E4M3: return (i64)fp8e4m3_to_f32(((u8 *)p)[off]);
    case DT_FP8E5M2: return (i64)fp8e5m2_to_f32(((u8 *)p)[off]);
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
  return dt == DT_FP32 || dt == DT_FP64
      || dt == DT_FP16 || dt == DT_BF16
      || dt == DT_FP8E4M3 || dt == DT_FP8E5M2;
}

// fp8 narrows through the _sat encoders, as a tinygrad kernel store does
// (codegen/decomp/dtype.py:115-134): inf saturates to maxnorm, unlike
// the host-side float_to_fp8.
static void uwalk_store_f64(void *p, i64 off, u32 dt, double v) {
  switch (dt) {
    case DT_FP32:  ((f32 *)p)[off] = (f32)v; break;
    case DT_FP64:  ((f64 *)p)[off] = v;      break;
    case DT_FP16:  ((u16 *)p)[off] = f32_to_fp16((f32)v); break;
    case DT_BF16:  ((u16 *)p)[off] = f32_to_bf16((f32)v); break;
    case DT_FP8E4M3: ((u8 *)p)[off] = f32_to_fp8e4m3_sat((f32)v); break;
    case DT_FP8E5M2: ((u8 *)p)[off] = f32_to_fp8e5m2_sat((f32)v); break;
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
    case DT_FP16:  ((u16 *)p)[off] = f32_to_fp16((f32)v); break;
    case DT_BF16:  ((u16 *)p)[off] = f32_to_bf16((f32)v); break;
    case DT_FP8E4M3: ((u8 *)p)[off] = f32_to_fp8e4m3_sat((f32)v); break;
    case DT_FP8E5M2: ((u8 *)p)[off] = f32_to_fp8e5m2_sat((f32)v); break;
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
static double   uwalk_eval_float(UWalkCtx *c, Term t);
static i64      uwalk_eval_int  (UWalkCtx *c, Term t);
static UWalkRed uwalk_run_reduce(UWalkCtx *c, Term red);

// v as a value of int or bool dtype `dt`: its low bits, sign-extended
// when dt is signed.  A bool is v != 0.
static i64 uwalk_int_wrap(u32 dt, i64 v) {
  if (dtype_is_bool(dt)) {
    return v != 0;
  }
  u32 bits = dtype_info(dt)->bits;
  if (bits >= 64) {
    return v;
  }
  u64 mask = ((u64)1 << bits) - 1;
  u64 u    = (u64)v & mask;
  if (dtype_is_signed(dt) && ((u >> (bits - 1)) & 1)) {
    u |= ~mask;
  }
  return (i64)u;
}

// Int or bool value v of dtype `dt` as a double; a u64 is unsigned.
static double uwalk_int_to_f64(u32 dt, i64 v) {
  return (dt == DT_UINT64) ? (double)(u64)v : (double)v;
}

// f converted to int or bool dtype `dt` as a C cast converts it: toward
// zero, then wrapped to the dtype; a bool is f != 0.  Where C leaves the
// conversion undefined, NaN is 0 and a value past the i64 range (the u64
// range for a u64) saturates first.
static i64 uwalk_f64_to_int(u32 dt, double f) {
  if (dtype_is_bool(dt)) {
    return f != 0.0;
  }
  if (f != f) {
    return 0;
  }
  if (dt == DT_UINT64 && f >= 9223372036854775808.0) {
    return (f >= 18446744073709551616.0) ? (i64)UINT64_MAX : (i64)(u64)f;
  }
  if (f >= 9223372036854775808.0) {
    return uwalk_int_wrap(dt, INT64_MAX);
  }
  if (f < -9223372036854775808.0) {
    return uwalk_int_wrap(dt, INT64_MIN);
  }
  return uwalk_int_wrap(dt, (i64)f);
}

// 1 iff `red` reduces an int or bool dtype, written to *dt.
static int uwalk_reduce_int_dtype(Term red, u32 *dt) {
  u32 d = DT_FP32;
  (void)term_dtype_in(red, 0, &d);
  if (!dtype_is_bool(d) && !(dtype_is_int(d) && !dtype_is_packed(d))) {
    return 0;
  }
  *dt = d;
  return 1;
}

// The dtype C's usual arithmetic conversions give an op over dtypes a and
// b, as far as the evaluators tell them apart: a float wins, then a u64;
// any other int pair computes exactly in the i64 evaluator.
static u32 uwalk_dtype_promote(u32 a, u32 b) {
  if (uwalk_dtype_is_float(a)) {
    return a;
  }
  if (uwalk_dtype_is_float(b) || b == DT_UINT64) {
    return b;
  }
  return a;
}

// The dtype of the value `t` evaluates to, which picks the evaluator its
// consumer walks it with.  It follows the C the renderer emits for `t`:
// index arithmetic (RANGE, the I-ops, INVALID) is an int and a comparison
// an int 0 or 1; a load, CONST, CAST or REDUCE has its own dtype; RECIP,
// EXP2, LOG2 and SQRT give a float; ADD, MUL, NEG and an IWHERE's branches
// take their operands' dtype, the float or u64 one when they mix.
static u32 uwalk_term_dtype(UWalkCtx *c, Term t) {
  if (term_tag(t) != TAG_UOP) {
    return DT_INT64;
  }
  u64 loc = term_val(t);
  switch (term_ext(t)) {
    case UOP_CONST:
      return term_ext(heap_read(loc));
    case UOP_CAST: case UOP_BITCAST:
      return (u32)term_val(heap_read(loc + 1));
    case UOP_INDEX_E: {
      void *bp;
      u32   bdt;
      return uwalk_resolve_buf(c, heap_read(loc), &bp, &bdt) ? bdt : DT_FP32;
    }
    case UOP_REDUCE: {
      u32 dt = DT_FP32;
      return uwalk_reduce_int_dtype(t, &dt) ? dt : DT_FP32;
    }
    case UOP_CMPLT: case UOP_CMPEQ: case UOP_ILT:
      return DT_BOOL;
    case UOP_RECIP: case UOP_EXP2: case UOP_LOG2: case UOP_SQRT: {
      u32 d = uwalk_term_dtype(c, heap_read(loc));
      return uwalk_dtype_is_float(d) ? d : DT_FP32;
    }
    case UOP_NEG: case UOP_OPT:
      return uwalk_term_dtype(c, heap_read(loc));
    case UOP_ADD: case UOP_MUL:
      return uwalk_dtype_promote(uwalk_term_dtype(c, heap_read(loc)),
                                 uwalk_term_dtype(c, heap_read(loc + 1)));
    case UOP_IWHERE:
      return uwalk_dtype_promote(uwalk_term_dtype(c, heap_read(loc + 1)),
                                 uwalk_term_dtype(c, heap_read(loc + 2)));
    default:
      return DT_INT64;
  }
}

static int uwalk_term_is_float(UWalkCtx *c, Term t) {
  return uwalk_dtype_is_float(uwalk_term_dtype(c, t));
}

// CAST(src, dst) as the C emit converts it, `((T)x)`, and a bool as
// `(x != 0)`, which is tinygrad's bool(x).  src walks in its own dtype
// (uwalk_term_dtype): a float reaches an int dtype truncated toward zero
// and wrapped, an int wraps into a narrower one, and an int, or an f64
// value, reaches f32 rounded once rather than through a double.  A u64
// converts from its unsigned value.
static i64 uwalk_cast_int(UWalkCtx *c, Term src, u32 dst) {
  if (uwalk_term_is_float(c, src)) {
    return uwalk_f64_to_int(dst, uwalk_eval_float(c, src));
  }
  return uwalk_int_wrap(dst, uwalk_eval_int(c, src));
}

static double uwalk_cast_float(UWalkCtx *c, Term src, u32 dst) {
  u32 sdt = uwalk_term_dtype(c, src);
  if (uwalk_dtype_is_float(sdt)) {
    double f = uwalk_eval_float(c, src);
    return (dst == DT_FP32) ? (double)(f32)f : f;
  }
  i64 v = uwalk_eval_int(c, src);
  if (dst == DT_FP64) {
    return uwalk_int_to_f64(sdt, v);
  }
  return (sdt == DT_UINT64) ? (double)(f32)(u64)v : (double)(f32)v;
}

// Look up a hoisted reduce accumulator by REDUCE term identity.
// NOTE: callers that get a non-negative idx must verify the iter
// snapshot via uwalk_reduce_snapshot_matches before using its value --
// the cache may be stale if a relevant free axis has advanced.
static int uwalk_reduce_lookup(UWalkCtx const *c, Term red) {
  for (u32 i = 0; i < c->n_reduces; i++) {
    if (c->red_term[i] == red) {
      return (int)i;
    }
  }
  return -1;
}

// Forward decl -- uwalk_collect_ranges is defined later (in the block
// that handles the inner-reduce range collection); free-axis discovery
// for the iter-aware reduce cache uses it from here.
static void uwalk_collect_ranges(Term t, Term *ranges, u32 *n_out);

// 1 iff `red` iterates split-inner axis `aid` of the current store: it owns
// the split, or no reduce does and `red`'s body (ranges `body`) holds it.
static int uwalk_reduce_folds_split(UWalkCtx const *c, Term red, u32 aid,
                                    Term const *body, u32 n_body) {
  for (u32 i = 0; i < c->n_split; i++) {
    if (c->split_aid[i] != aid) {
      continue;
    }
    if (c->split_owner[i] != 0) {
      return c->split_owner[i] == red;
    }
    for (u32 j = 0; j < n_body; j++) {
      if ((u32)term_val(heap_read(term_val(body[j]) + 0)) == aid) {
        return 1;
      }
    }
    return 0;
  }
  return 0;
}

// Walk a REDUCE's body and collect axis_ids the body references,
// excluding the axes the REDUCE binds (its n_axes plus the split-inner
// axes it folds).  Returns the count, writes ids into out[] up to cap.
static u8 uwalk_reduce_free_axes(UWalkCtx const *c, Term red, u32 *out, u8 cap) {
  Term body      = uop_reduce_src(red);
  u32  n_axes    = uop_reduce_n_axes(red);
  u32  own_axes[MAX_DIM];
  for (u32 i = 0; i < n_axes; i++) own_axes[i] = uop_reduce_axis(red, i);
  Term ranges_buf[UWALK_MAX_RANGES];
  u32 n_r = 0;
  uwalk_collect_ranges(body, ranges_buf, &n_r);
  u8 n_free = 0;
  for (u32 i = 0; i < n_r; i++) {
    u32 ax = (u32)term_val(heap_read(term_val(ranges_buf[i]) + 0));
    int is_own = 0;
    for (u32 j = 0; j < n_axes; j++) if (own_axes[j] == ax) { is_own = 1; break; }
    if (is_own) continue;
    if (uwalk_reduce_folds_split(c, red, ax, ranges_buf, n_r)) {
      continue;
    }
    if (n_free >= cap) break;
    out[n_free++] = ax;
  }
  return n_free;
}

// Check whether the cached entry's free-axis snapshot matches the
// CURRENT iter values.  Returns 1 if all free axes are still at
// their cached iter, 0 if any has advanced (cache stale).
static int uwalk_reduce_snapshot_matches(UWalkCtx const *c, u32 slot) {
  for (u32 i = 0; i < c->red_n_free[slot]; i++) {
    u32 ax  = c->red_free_ids[slot][i];
    u32 saved = c->red_free_iter[slot][i];
    if (uwalk_lookup_iter(c, ax) != saved) return 0;
  }
  return 1;
}

// Refresh the cached entry's snapshot to current iter values.  Called
// right after a (re)compute so the next lookup is keyed correctly.
static void uwalk_reduce_snapshot_save(UWalkCtx *c, u32 slot) {
  for (u32 i = 0; i < c->red_n_free[slot]; i++) {
    c->red_free_iter[slot][i] = uwalk_lookup_iter(c, c->red_free_ids[slot][i]);
  }
}

// A REDUCE's value through the iter-aware cache: if the REDUCE has a slot
// whose snapshot still matches the current iter for its free axes, return
// the cached value; otherwise recompute and refresh or register the slot.
// Without this, an inner REDUCE inside another REDUCE's body is recomputed
// every outer iteration even when its body is invariant to the outer's
// axis (e.g. conv-backward x-grad has reduces over (Cout,kH,kW) nested with
// the unfold sub-reduces; ~25x speedup for the inner Conv2d weight/input
// grad kernels).
static UWalkRed uwalk_reduce_value(UWalkCtx *c, Term t) {
  int idx = uwalk_reduce_lookup(c, t);
  if (idx >= 0 && uwalk_reduce_snapshot_matches(c, (u32)idx)) {
    return c->red_acc[idx];
  }
  UWalkRed v = uwalk_run_reduce(c, t);
  if (idx >= 0) {
    c->red_acc[idx] = v;
    uwalk_reduce_snapshot_save(c, (u32)idx);
    return v;
  }
  if (c->n_reduces >= UWALK_MAX_REDUCES) {
    return v;
  }
  u32 slot = c->n_reduces++;
  c->red_term[slot]   = t;
  c->red_acc[slot]    = v;
  c->red_n_free[slot] = uwalk_reduce_free_axes(c, t, c->red_free_ids[slot],
                                               UWALK_MAX_RANGES);
  uwalk_reduce_snapshot_save(c, slot);
  return v;
}

// a < b (CMPLT) or a == b (CMPEQ) in the operands' dtype, as the rendered
// C compares them: in doubles when either is a float, else in the int
// evaluator, unsigned when either is a u64.  A double holds an int exactly
// only below 2^53.
static int uwalk_compare(UWalkCtx *c, u32 op, Term a, Term b) {
  u32 da = uwalk_term_dtype(c, a);
  u32 db = uwalk_term_dtype(c, b);
  if (uwalk_dtype_is_float(da) || uwalk_dtype_is_float(db)) {
    double x = uwalk_eval_float(c, a);
    double y = uwalk_eval_float(c, b);
    return (op == UOP_CMPLT) ? (x < y) : (x == y);
  }
  i64 x = uwalk_eval_int(c, a);
  i64 y = uwalk_eval_int(c, b);
  if (op == UOP_CMPEQ) {
    return x == y;
  }
  return (da == DT_UINT64 || db == DT_UINT64) ? ((u64)x < (u64)y) : (x < y);
}

// Truth of an IWHERE condition in its own dtype: an index predicate or a
// comparison is an int 0 or 1, and a float mask is true when nonzero.
static int uwalk_cond_truth(UWalkCtx *c, Term cterm) {
  if (uwalk_term_is_float(c, cterm)) {
    return uwalk_eval_float(c, cterm) != 0.0;
  }
  return uwalk_eval_int(c, cterm) != 0;
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
      // thvm_wl_uop_const stores every float dtype's bits as the f32
      // representation of the input mreal (see thvmlink.c:1836-1848), so
      // CONST nodes flagged DT_FP64/FP16/BF16 still carry an f32 payload
      // here.  Decode uniformly as f32 for any float dtype; upcast to
      // f64 happens via the (double)pun.f promotion.
      if (uwalk_dtype_is_float(dt)) {
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
    case UOP_CMPLT: case UOP_CMPEQ:
      return uwalk_compare(c, op, heap_read(loc + 0), heap_read(loc + 1));
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
      if (uwalk_dtype_is_float(dst)) {
        return uwalk_cast_float(c, src, dst);
      }
      return uwalk_int_to_f64(dst, uwalk_cast_int(c, src, dst));
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
    case UOP_IWHERE:
      if (uwalk_cond_truth(c, heap_read(loc + 0))) {
        return uwalk_eval_float(c, heap_read(loc + 1));
      }
      return uwalk_eval_float(c, heap_read(loc + 2));
    case UOP_REDUCE:
      return uwalk_reduce_value(c, t).f;
    case UOP_OPT:
      return uwalk_eval_float(c, heap_read(loc + 0));
    case UOP_RANGE:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR:
    case UOP_IXOR: case UOP_ISHR:
      return (double)uwalk_eval_int(c, t);
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
      // See uwalk_eval_float's UOP_CONST: producer stores f32 bits for
      // every float dtype, so decode uniformly here too.
      if (uwalk_dtype_is_float(dt)) {
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
    case UOP_IOR:  return uwalk_eval_int(c, heap_read(loc+0))
                        | uwalk_eval_int(c, heap_read(loc+1));
    case UOP_IXOR: return uwalk_eval_int(c, heap_read(loc+0))
                        ^ uwalk_eval_int(c, heap_read(loc+1));
    case UOP_ISHR: return (i64)((i32)uwalk_eval_int(c, heap_read(loc+0))
                        >> (i32)uwalk_eval_int(c, heap_read(loc+1)));
    case UOP_IWHERE:
      return uwalk_cond_truth(c, heap_read(loc + 0))
           ? uwalk_eval_int(c, heap_read(loc + 1))
           : uwalk_eval_int(c, heap_read(loc + 2));
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
      if (uwalk_dtype_is_float(dst)) {
        return uwalk_f64_to_int(DT_INT64, uwalk_cast_float(c, src, dst));
      }
      return uwalk_cast_int(c, src, dst);
    }
    case UOP_BITCAST: {
      Term src = heap_read(loc + 0);
      u32  dst = term_val(heap_read(loc + 1));
      if (dst == DT_INT32 || dst == DT_UINT32) {
        // Int-to-int BITCAST at the same itemsize is a value passthrough
        // at the bit level (e.g. u32 -> i32 keeps the bit pattern).
        // Going through uwalk_eval_float would round-trip the u32 value
        // through a double and lose the original bits when the value
        // can't be represented exactly in f32, or even when it CAN --
        // 1.0_u32 becomes (f32)1.0 whose bit pattern is 0x3F800000.
        if (!uwalk_term_is_float(c, src)) {
          return uwalk_eval_int(c, src);
        }
        double f = uwalk_eval_float(c, src);
        union { u32 b; f32 f; } pun;
        pun.f = (f32)f;
        return (i64)(i32)pun.b;
      }
      // For smaller-than-32-bit bit reinterpret (u16, u8, etc.) and
      // f16/bf16/fp8 sources, eval_int(INDEX_E) would route through
      // fp_convert.c's value-preserving load and lose the bits.  Read
      // raw bytes from the source buffer when src is a direct INDEX_E.
      if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_INDEX_E) {
        u64 iloc = term_val(src);
        Term buf = heap_read(iloc + 0);
        Term addr = heap_read(iloc + 1);
        void *bp; u32 bdt;
        if (uwalk_resolve_buf(c, buf, &bp, &bdt)) {
          i64 off = uwalk_eval_int(c, addr);
          u32 sz = dtype_itemsize(bdt);
          if (sz == 1) return (i64)((u8  *)bp)[off];
          if (sz == 2) return (i64)((u16 *)bp)[off];
          if (sz == 4) return (i64)((u32 *)bp)[off];
          if (sz == 8) return (i64)((u64 *)bp)[off];
        }
      }
      return uwalk_eval_int(c, src);
    }
    case UOP_OPT:
      return uwalk_eval_int(c, heap_read(loc + 0));
    case UOP_REDUCE: {
      UWalkRed r = uwalk_reduce_value(c, t);
      return r.is_int ? r.i : (i64)r.f;
    }
    // UOP_ADD / MUL / NEG over integer-typed operands.  The high-level WL
    // Plus / Times / etc. emit these generic ALU ops regardless of element
    // type; an int value (uwalk_term_dtype) walks here and stays in integer
    // arithmetic to preserve wrap / unsigned bits.
    case UOP_ADD:
      return uwalk_eval_int(c, heap_read(loc + 0))
           + uwalk_eval_int(c, heap_read(loc + 1));
    case UOP_MUL:
      return uwalk_eval_int(c, heap_read(loc + 0))
           * uwalk_eval_int(c, heap_read(loc + 1));
    case UOP_NEG:
      return -uwalk_eval_int(c, heap_read(loc + 0));
    case UOP_CMPLT: case UOP_CMPEQ:
      return uwalk_compare(c, op, heap_read(loc + 0), heap_read(loc + 1));
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
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
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
    case UOP_REDUCE:
      // Recurse into the reduce body so outer reduces over chained
      // inner reduces find their own reduce-axis range (extent) and
      // any output axes used by the inner body.  Without this, an
      // outer REDUCE whose body is itself a UOP_REDUCE sees zero
      // ranges in uwalk_run_reduce -> r_extent==0 -> returns
      // 0.0 / -INFINITY immediately.
      uwalk_collect_ranges(heap_read(loc + 0), ranges, n_out);
      return;
    default:
      return;
  }
}

// Collect TOP-LEVEL REDUCE nodes (those reachable from `t` without
// crossing another UOP_REDUCE boundary).  Reduces nested inside another
// reduce's body are NOT collected here: the outer's uwalk_run_reduce
// will iterate its axis, and evaluating the inner UOP_REDUCE re-runs it
// on every iteration via uwalk_run_reduce (see uwalk_reduce_value's
// cache-miss branch).  Pre-registering inner reduces would
// cache a stale value (computed once with the outer's axis at 0) and
// the outer's accumulator would combine the same value N times.
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
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
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

// Collect every REDUCE under `t`, nested reduce bodies included.
static void uwalk_collect_all_reduces(Term t, Term *reduces, u32 *n_out,
                                      u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP || *n_out >= UWALK_MAX_REDUCES) {
    return;
  }
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_REDUCE) {
    for (u32 i = 0; i < *n_out; i++) {
      if (reduces[i] == t) {
        return;
      }
    }
    reduces[(*n_out)++] = t;
    uwalk_collect_all_reduces(heap_read(loc + 0), reduces, n_out, depth + 1);
    return;
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      uwalk_collect_all_reduces(heap_read(loc + 0), reduces, n_out, depth + 1);
      uwalk_collect_all_reduces(heap_read(loc + 1), reduces, n_out, depth + 1);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:
      uwalk_collect_all_reduces(heap_read(loc + 0), reduces, n_out, depth + 1);
      return;
    case UOP_IWHERE:
      uwalk_collect_all_reduces(heap_read(loc + 0), reduces, n_out, depth + 1);
      uwalk_collect_all_reduces(heap_read(loc + 1), reduces, n_out, depth + 1);
      uwalk_collect_all_reduces(heap_read(loc + 2), reduces, n_out, depth + 1);
      return;
    default:
      return;
  }
}

// Collect the RANGE leaves under `t` that uop_dag_apply_split minted as the
// inner half of a split (wrapped in OPT(UNROLL|UPCAST)), nested reduce
// bodies included.
static void uwalk_collect_split_ranges(Term t, Term *ranges, u32 *n_out,
                                       u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP || *n_out >= UWALK_MAX_RANGES) {
    return;
  }
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_OPT: {
      Term tgt  = heap_read(loc + 0);
      u32  kind = uop_opt_kind(t);
      if ((kind == UOP_OPT_UNROLL || kind == UOP_OPT_UPCAST)
          && term_tag(tgt) == TAG_UOP && term_ext(tgt) == UOP_RANGE) {
        u32 aid = uop_range_axis_id(tgt);
        for (u32 i = 0; i < *n_out; i++) {
          if (uop_range_axis_id(ranges[i]) == aid) {
            return;
          }
        }
        ranges[(*n_out)++] = tgt;
        return;
      }
      uwalk_collect_split_ranges(tgt, ranges, n_out, depth + 1);
      return;
    }
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      uwalk_collect_split_ranges(heap_read(loc + 0), ranges, n_out, depth + 1);
      uwalk_collect_split_ranges(heap_read(loc + 1), ranges, n_out, depth + 1);
      return;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_REDUCE:
      uwalk_collect_split_ranges(heap_read(loc + 0), ranges, n_out, depth + 1);
      return;
    case UOP_IWHERE:
      uwalk_collect_split_ranges(heap_read(loc + 0), ranges, n_out, depth + 1);
      uwalk_collect_split_ranges(heap_read(loc + 1), ranges, n_out, depth + 1);
      uwalk_collect_split_ranges(heap_read(loc + 2), ranges, n_out, depth + 1);
      return;
    default:
      return;
  }
}

// Record the store's split-inner axes and their owning reduces: every
// OPT(UNROLL|UPCAST) range inside some reduce body that indexes no store
// position.  Mirrors the renderer's inner-reduce-decomposition set
// (rmu_range_is_inner_reduce_decomp + rmu_reduce_folds_split_axis).
static void uwalk_split_axes_init(UWalkCtx *c, Term store_addr, Term value) {
  c->n_split = 0;
  Term reds[UWALK_MAX_REDUCES];
  u32  n_reds = 0;
  uwalk_collect_all_reduces(value, reds, &n_reds, 0);
  if (n_reds == 0) {
    return;
  }
  Term split[UWALK_MAX_RANGES];
  u32  n_split = 0;
  for (u32 i = 0; i < n_reds; i++) {
    uwalk_collect_split_ranges(uop_reduce_src(reds[i]), split, &n_split, 0);
  }
  Term addr_ranges[UWALK_MAX_RANGES];
  u32  n_addr = 0;
  uwalk_collect_ranges(store_addr, addr_ranges, &n_addr);
  for (u32 i = 0; i < n_split && c->n_split < UWALK_MAX_RANGES; i++) {
    u32 aid = uop_range_axis_id(split[i]);
    int in_addr = 0;
    for (u32 j = 0; j < n_addr; j++) {
      if ((u32)term_val(heap_read(term_val(addr_ranges[j]) + 0)) == aid) {
        in_addr = 1;
        break;
      }
    }
    if (in_addr) {
      continue;
    }
    Term owner = 0;
    for (u32 k = 0; k < n_reds && owner == 0; k++) {
      if (uop_reduce_owns_split_axis(reds[k], aid)) {
        owner = reds[k];
      }
    }
    c->split_aid  [c->n_split] = aid;
    c->split_owner[c->n_split] = owner;
    c->n_split++;
  }
}

static UWalkRed uwalk_int_red(u32 dt, i64 v) {
  UWalkRed r;
  r.i      = v;
  r.f      = uwalk_int_to_f64(dt, v);
  r.is_int = 1;
  return r;
}

// tinygrad's identity_element (uop/ops.py:61): 0 for SUM, dt.min for MAX,
// which is -2^(bits-1) for a signed int, 0 for an unsigned int or bool
// (dtype.py:70-72), and -inf for a float.
static UWalkRed uwalk_reduce_identity(u32 kind, int is_int, u32 dt) {
  if (!is_int) {
    UWalkRed r = { (kind == REDUCE_MAX) ? -INFINITY : 0.0, 0, 0 };
    return r;
  }
  i64 id = 0;
  if (kind == REDUCE_MAX && dtype_is_signed(dt)) {
    id = uwalk_int_wrap(dt, (i64)((u64)1 << (dtype_info(dt)->bits - 1)));
  }
  return uwalk_int_red(dt, id);
}

// acc <kind> v in int or bool dtype `dt`.  SUM wraps as the dtype's
// accumulator does, MAX compares in the dtype's order, and a bool SUM or
// MAX is an OR (tinygrad uop/symbolic.py:163-166).
static i64 uwalk_int_combine(u32 kind, u32 dt, i64 acc, i64 v) {
  v = uwalk_int_wrap(dt, v);
  if (dtype_is_bool(dt)) {
    return acc | v;
  }
  if (kind == REDUCE_SUM) {
    return uwalk_int_wrap(dt, (i64)((u64)acc + (u64)v));
  }
  if (dt == DT_UINT64) {
    return ((u64)acc < (u64)v) ? v : acc;
  }
  return (acc < v) ? v : acc;
}

// acc <kind> v for a float body value v of an int or bool reduce, as the
// rendered C combines it: `acc = acc + v` adds in float and converts the
// sum back, while a MAX converts v into the dtype before it compares.
static i64 uwalk_int_combine_f64(u32 kind, u32 dt, i64 acc, double v) {
  if (kind == REDUCE_SUM && !dtype_is_bool(dt)) {
    return uwalk_f64_to_int(dt, uwalk_int_to_f64(dt, acc) + v);
  }
  return uwalk_int_combine(kind, dt, acc, uwalk_f64_to_int(dt, v));
}

// Advance the reduce odometer over the n axes pushed at slot0 (innermost
// last).  Returns 0 once every combination has run.
static int uwalk_odometer_next(UWalkCtx *c, u32 slot0, u32 const *extents,
                               u32 n) {
  for (u32 d = n; d > 0; d--) {
    if (++c->iter[slot0 + d - 1] < extents[d - 1]) {
      return 1;
    }
    c->iter[slot0 + d - 1] = 0;
  }
  return 0;
}

// Compute one reduce accumulator over its axis range.
// Run a (possibly multi-axis) REDUCE by chaining through nested
// same-kind UOP_REDUCE shells.  Mirrors tinygrad's lowered form
// `UOp(Ops.REDUCE, (body,) + tuple(reduce_range), alu_op)` where one
// REDUCE node iterates over MULTIPLE range axes simultaneously
// (tinygrad/codegen/lowerer.py:100, tinygrad/schedule/indexing.py:94).
//
// thvm's UOP_REDUCE encodes a single axis per node, so the Python
// `sum(axis=(4,5,6))` lowers to three nested REDUCE shells.  Without
// chain-fusion the walker re-evaluates the inner REDUCE on every
// outer iteration: even with the iter-aware cache that's still
// recursive-recompute O(prod(axes)) per output position, vs the
// flat O(prod(axes)) we get from one nested loop.  For a Conv2d
// (C_in*kH*kW = 32*5*5 = 800) at 2*32*20*20 = 25600 outputs that's
// the difference between seconds and "doesn't terminate".
//
// Both SUM and MAX are commutative+associative, so fusing axes into
// one nested loop preserves the value.  Chain only follows direct
// `r_src == UOP_REDUCE(same_kind)` shells; if the body branches into
// elementwise ops over multiple sibling REDUCEs, those siblings stay
// as separate uwalk_eval_float entries (handled via the iter-cache).
//
// An int or bool reduce accumulates in its dtype, as tinygrad allocates
// the accumulator like the reduce (reduce_ranges_to_acc,
// codegen/__init__.py:197-204); a double accumulator rounds past 2^53.
static UWalkRed uwalk_run_reduce(UWalkCtx *c, Term red) {
  u32  axes  [UWALK_MAX_RANGES];
  u32  extents[UWALK_MAX_RANGES];
  u32  chain_len = 0;
  Term cur   = red;
  u32  kind  = term_val(heap_read(term_val(red) + 1));
  Term body  = cur;
  u32  dt    = DT_FP32;
  int  is_int = uwalk_reduce_int_dtype(red, &dt);
  UWalkRed identity = uwalk_reduce_identity(kind, is_int, dt);
  // Walk down the chain of same-kind UOP_REDUCE shells.  Each shell may
  // itself be multi-axis (mirrors tinygrad REDUCE with src=(body,
  // range_0, range_1, ...) -- uop/ops.py + schedule/indexing.py:94).
  while (term_tag(cur) == TAG_UOP && term_ext(cur) == UOP_REDUCE
         && chain_len < UWALK_MAX_RANGES) {
    u32 ckind = uop_reduce_kind(cur);
    if (ckind != kind) break;
    u32 cn = uop_reduce_n_axes(cur);
    Term csrc = uop_reduce_src(cur);
    Term src_ranges[UWALK_MAX_RANGES];
    u32  n_src_r = 0;
    uwalk_collect_ranges(csrc, src_ranges, &n_src_r);
    // Add each axis of THIS shell to the chain.  The order within a
    // shell follows builder order (same as tinygrad's range order in
    // src[1:]); across shells we visit outer-shell first so the
    // innermost shell's axes appear last in the chain (matching the
    // old single-axis nested REDUCE traversal).
    for (u32 ai = 0; ai < cn && chain_len < UWALK_MAX_RANGES; ai++) {
      u32 caxis = uop_reduce_axis(cur, ai);
      u32 cext = 0;
      for (u32 i = 0; i < n_src_r; i++) {
        u32 ax = term_val(heap_read(term_val(src_ranges[i]) + 0));
        if (ax == caxis) {
          // kvar_extent_runtime: a symbolic reduce axis loops the BOUND
          // value (kvar_hi if unbound), a literal axis its own extent.
          cext = kvar_extent_runtime(term_val(heap_read(term_val(src_ranges[i]) + 2)));
          break;
        }
      }
      if (cext == 0) {
        // No contributions on this axis -> the whole chain yields
        // identity.  (Identity-or-bail matches the old single-axis
        // r_extent==0 short-circuit.)
        return identity;
      }
      axes[chain_len]    = caxis;
      extents[chain_len] = cext;
      chain_len++;
    }
    // The split-inner halves this shell folds iterate innermost, like the
    // renderer's inner-reduce-decomposition loops.
    for (u32 i = 0; i < n_src_r && chain_len < UWALK_MAX_RANGES; i++) {
      u32 ax = term_val(heap_read(term_val(src_ranges[i]) + 0));
      if (!uwalk_reduce_folds_split(c, cur, ax, src_ranges, n_src_r)) {
        continue;
      }
      int dup = 0;
      for (u32 j = 0; j < chain_len; j++) {
        if (axes[j] == ax) {
          dup = 1;
          break;
        }
      }
      if (dup) {
        continue;
      }
      axes[chain_len]    = ax;
      extents[chain_len] = kvar_extent_runtime(term_val(heap_read(term_val(src_ranges[i]) + 2)));
      chain_len++;
    }
    body = csrc;
    cur  = csrc;
  }
  if (chain_len == 0) {
    // Caller passed a non-REDUCE (defensive).
    return identity;
  }
  // Push one range slot per chained axis.
  if (c->n_ranges + chain_len > UWALK_MAX_RANGES) {
    UWalkRed none = { 0.0, 0, is_int };
    return none;
  }
  u32 slot0 = c->n_ranges;
  for (u32 i = 0; i < chain_len; i++) {
    c->axis_id[slot0 + i] = axes[i];
    c->iter   [slot0 + i] = 0;
  }
  c->n_ranges += chain_len;
  // Iterate the full Cartesian product of axes via odometer.
  if (is_int) {
    int body_float = uwalk_term_is_float(c, body);
    i64 acc        = identity.i;
    do {
      if (body_float) {
        acc = uwalk_int_combine_f64(kind, dt, acc, uwalk_eval_float(c, body));
      } else {
        acc = uwalk_int_combine(kind, dt, acc, uwalk_eval_int(c, body));
      }
    } while (uwalk_odometer_next(c, slot0, extents, chain_len));
    c->n_ranges -= chain_len;
    return uwalk_int_red(dt, acc);
  }
  double acc = identity.f;
  do {
    double v = uwalk_eval_float(c, body);
    acc = (kind == REDUCE_MAX) ? ((v > acc) ? v : acc) : (acc + v);
  } while (uwalk_odometer_next(c, slot0, extents, chain_len));
  c->n_ranges -= chain_len;
  UWalkRed r = { acc, 0, 0 };
  return r;
}

// Find the output-axis ranges (those that appear in addr) vs reduce
// ranges. Mirrors rmu_emit_store / rmu_emit_store_reduce's logic.
//
// `reduce_axes` is the list of axis_ids (possibly multiple) of a
// REDUCE-as-store-value pattern; `n_reduce_axes==0` means "no reduce".
// Output ranges fill out_ranges; ranges matching any reduce axis are
// excluded.
static void uwalk_split_ranges(Term *all_ranges, u32 n_all,
                               u32 const *reduce_axes, u32 n_reduce_axes,
                               Term *out_ranges, u32 *n_out) {
  *n_out = 0;
  for (u32 i = 0; i < n_all; i++) {
    u32 aid = term_val(heap_read(term_val(all_ranges[i]) + 0));
    int is_reduce = 0;
    for (u32 j = 0; j < n_reduce_axes; j++) {
      if (reduce_axes[j] == aid) { is_reduce = 1; break; }
    }
    if (is_reduce) continue;
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
      int idx = uwalk_reduce_lookup(c, value_reduces[i]);
      if (idx < 0) {
        // First time: register slot + free-axis snapshot fields so
        // the iter-aware cache check in eval_float can validate it.
        if (c->n_reduces >= UWALK_MAX_REDUCES) return;
        idx = (int)c->n_reduces++;
        c->red_term[idx]   = value_reduces[i];
        c->red_n_free[idx] = uwalk_reduce_free_axes(c, value_reduces[i],
                                c->red_free_ids[idx], UWALK_MAX_RANGES);
      }
      c->red_acc[idx] = uwalk_run_reduce(c, value_reduces[i]);
      uwalk_reduce_snapshot_save(c, (u32)idx);
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
    } else if (c->store_from_float) {
      i64 v = uwalk_f64_to_int(out_dt, uwalk_eval_float(c, store_value));
      uwalk_store_i64(out_p, addr_v, out_dt, v);
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
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) {
    UWALK_DECLINE(c->kid, "store_root_not_store",
                  "tag=%u ext=%u",
                  (unsigned)term_tag(store), (unsigned)term_ext(store));
    return 0;
  }
  u64 sloc = term_val(store);
  Term store_buf  = heap_read(sloc + 0);
  Term store_addr = heap_read(sloc + 1);
  Term store_val  = heap_read(sloc + 2);
  // Resolve output buffer.
  void *out_p; u32 out_dt;
  if (!uwalk_resolve_buf(c, store_buf, &out_p, &out_dt)) {
    UWALK_DECLINE(c->kid, "store_buf_unresolved", "buf_term=%llu",
                  (unsigned long long)store_buf);
    return 0;
  }
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
    Term r_src = uop_reduce_src(reduce_as_value);
    uwalk_collect_ranges(r_src, all_ranges, &n_all);
  }
  u32 reduce_axes[MAX_DIM + UWALK_MAX_RANGES];
  u32 n_reduce_axes = 0;
  if (reduce_as_value != 0) {
    n_reduce_axes = uop_reduce_n_axes(reduce_as_value);
    for (u32 i = 0; i < n_reduce_axes; i++) {
      reduce_axes[i] = uop_reduce_axis(reduce_as_value, i);
    }
  }
  // Split-inner axes are iterated inside their owning reduce.
  uwalk_split_axes_init(c, store_addr, value_for_emit);
  for (u32 i = 0; i < c->n_split; i++) {
    reduce_axes[n_reduce_axes++] = c->split_aid[i];
  }
  // Output ranges = all_ranges minus all reduce axes (if any).
  Term out_ranges[UWALK_MAX_RANGES];
  u32 n_out = 0;
  uwalk_split_ranges(all_ranges, n_all, reduce_axes, n_reduce_axes, out_ranges, &n_out);
  // For each output range push a slot and record extent.
  u32 range_extents[UWALK_MAX_RANGES] = {0};
  u32 range_slots  [UWALK_MAX_RANGES] = {0};
  for (u32 i = 0; i < n_out; i++) {
    Term r = out_ranges[i];
    u32 axis_id = term_val(heap_read(term_val(r) + 0));
    // kvar_extent_runtime: a symbolic output axis iterates the BOUND value.
    u32 ext     = kvar_extent_runtime(term_val(heap_read(term_val(r) + 2)));
    u32 slot = uwalk_push_range_slot(c, axis_id);
    if (slot >= UWALK_MAX_RANGES) {
      // Overflow: pop already-pushed slots and bail.
      for (u32 j = 0; j < i; j++) uwalk_pop_range_slot(c);
      UWALK_DECLINE(c->kid, "range_slot_overflow",
                    "axis_id=%u n_ranges=%u cap=%u",
                    (unsigned)axis_id, (unsigned)c->n_ranges,
                    (unsigned)UWALK_MAX_RANGES);
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
      UWALK_DECLINE(c->kid, "reduce_slot_overflow",
                    "n_reduces=%u cap=%u",
                    (unsigned)c->n_reduces,
                    (unsigned)UWALK_MAX_REDUCES);
      return 0;
    }
    u32 idx = c->n_reduces++;
    c->red_term[idx]   = value_reduces[i];
    c->red_n_free[idx] = uwalk_reduce_free_axes(c, value_reduces[i],
                            c->red_free_ids[idx], UWALK_MAX_RANGES);
    c->red_acc[idx]    = uwalk_run_reduce(c, value_reduces[i]);
    uwalk_reduce_snapshot_save(c, idx);
  }
  // Run nested for-loops over out_ranges.
  c->store_from_float = !uwalk_dtype_is_float(out_dt)
                     && uwalk_term_is_float(c, value_term);
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
  if (term_tag(node) != TAG_UOP) {
    UWALK_DECLINE(c->kid, "node_non_uop", "tag=%u",
                  (unsigned)term_tag(node));
    return 0;
  }
  u32 op = term_ext(node);
  if (op == UOP_STORE) return uwalk_emit_store(c, node);
  if (op == UOP_AFTER) return uwalk_emit_after(c, node);
  UWALK_DECLINE(c->kid, "node_unsupported_op", "op=%u", (unsigned)op);
  return 0;
}

static int uwalk_emit_after(UWalkCtx *c, Term after) {
  if (term_tag(after) != TAG_UOP || term_ext(after) != UOP_AFTER) {
    UWALK_DECLINE(c->kid, "after_not_after_op",
                  "tag=%u ext=%u",
                  (unsigned)term_tag(after), (unsigned)term_ext(after));
    return 0;
  }
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
  // Diagnostic env knob THVM_CPU_UOP_WALK_TRACE: print one line per
  // walker entry summarising lift outcome + root op; useful for
  // measuring how many kernels the walker covers.
  static int trace_known = 0, trace_on = 0;
  if (!trace_known) {
    char const *e = getenv("THVM_CPU_UOP_WALK_TRACE");
    trace_on = (e != NULL && e[0] == '1');
    trace_known = 1;
  }
  u32 kid = (u32)(ke - KERNELS);
  UWALK_LAST_DECLINE = NULL;
  // store_root==0 means the lift declined; the walker can't help in
  // that case (legacy fallback runs).
  if (ke->cached_lift.store_root == 0) {
    if (trace_on) fprintf(stderr, "uop_walk: lift declined n_inputs=%u\n",
                          ke->n_inputs);
    UWALK_DECLINE(kid, "lift_declined",
                  "n_inputs=%u",
                  (unsigned)ke->n_inputs);
    return 0;
  }
  KernelUopLift const *lift = &ke->cached_lift;
  UWalkCtx ctx = {0};
  ctx.kid       = kid;
  ctx.n_inputs  = lift->n_inputs;
  // Output buffer (single).
  ctx.out_term  = lift->out_buf;
  ctx.out_ptr   = CPU_BUFS[out_buf_id].data;
  ctx.out_dtype = uop_buffer_dtype(lift->out_buf);
  if (ctx.out_dtype == 0) ctx.out_dtype = ke->output_dtype;
  u32 n_tab = lift->n_inputs ? lift->n_inputs : 1;
  ctx.in_terms  = (Term  *)calloc(n_tab, sizeof(Term));
  ctx.in_ptrs   = (void **)calloc(n_tab, sizeof(void *));
  ctx.in_dtypes = (u32   *)calloc(n_tab, sizeof(u32));
  if (ctx.in_terms == NULL || ctx.in_ptrs == NULL || ctx.in_dtypes == NULL) {
    thvm_fatal("cpu_uop_walk: out of memory");
  }
  int  ok   = 0;
  Term root = lift->store_root;
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
      UWALK_DECLINE(kid, "input_ptr_unresolved",
                    "slot=%u buf_id=%u",
                    (unsigned)i, (unsigned)in_buf_ids[i]);
      goto done;
    }
    ctx.in_ptrs[i] = p;
  }
  // Walk the root.
  if (root != 0 && term_tag(root) == TAG_UOP) {
    u32 op = term_ext(root);
    if      (op == UOP_STORE) ok = uwalk_emit_store(&ctx, root);
    else if (op == UOP_AFTER) ok = uwalk_emit_after(&ctx, root);
    else {
      UWALK_DECLINE(kid, "root_unsupported_op", "op=%u", (unsigned)op);
    }
    if (trace_on) fprintf(stderr,
                          "uop_walk: n_inputs=%u root_op=%u rc=%d\n",
                          ke->n_inputs, op, ok);
  } else {
    UWALK_DECLINE(kid, "root_empty_or_non_uop",
                  "root=%llu tag=%u",
                  (unsigned long long)root, (unsigned)term_tag(root));
    if (trace_on) {
      fprintf(stderr, "uop_walk: empty/non-UOP root tag=%u\n",
              (unsigned)term_tag(root));
    }
  }
done:
  free(ctx.in_terms);
  free(ctx.in_ptrs);
  free(ctx.in_dtypes);
  return ok;
}
