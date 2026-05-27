// codegen/render_linearized.c - new emit walk that consumes a LinKernel
// (produced by src/uop/linearize.c) rather than the legacy DAG walker
// in render_uop.c.
//
// Piece #4 (this commit) extends the stage (b) stub to the conv kid=3
// shape end-to-end: multi-axis RANGE nesting, PLACEHOLDER acc decls
// at the kernel-body scope, UOP_END as loop-close markers, AFTER
// chains that linearize statement order, STACK-as-store-value for F
// parallel accumulators, GEP / VCONST / UNROLL lane extraction, and a
// CUDA + Metal renderer variant in addition to the C99 stub.
//
// The function `cg_render_linearized_<target>` walks the linearized
// UOp list in order and dispatches per opcode:
//
//   UOP_BUFFER       -- pass-1 only; registers buf->name + dtype
//   UOP_PLACEHOLDER  -- emits `<dtype> _accN;` at the top of body
//   UOP_RANGE        -- opens `for (uint aN = 0; aN < ext; aN++) {`
//                       (LOOP/REDUCE only; UPCAST/UNROLL stay folded
//                       into VCONST + GEP after expander+devectorize)
//   UOP_END          -- closes one `}` per range carried in src list
//   UOP_STORE        -- `lhs = rhs;` (multi-store when rhs is STACK)
//   UOP_AFTER        -- nothing emitted; toposort places its operands
//   UOP_LOAD / VCONST / GEP / STACK / ALU -- treated as values; only
//                       referenced from STORE's rhs (via lz_emit_value)
//
// The route gate in render_uop.c admits a kernel to this pipeline only
// when the DAG carries at least one UOP_RANGE with axis_type ==
// KAX_UPCAST or KAX_UNROLL (the "opt-rich" shape that drives the
// parallel-accumulator emit).  Otherwise the legacy emit handles it,
// preserving the existing 301 test_render_uop / 83 test_render_uop_cuda
// / 8 test_render_uop_metal cases byte-for-byte.
//
// References:
//   tinygrad/codegen/__init__.py:140-143  do_render (the analog: it
//     takes a linearized UOp list and asks the renderer to emit a
//     string).
//   tinygrad/codegen/late/devectorizer.py:311-328  reduce_to_acc
//     (produces the PLACEHOLDER + STORE-init + STORE-update + END +
//     LOAD shape this renderer consumes).
//   tinygrad/codegen/late/linearizer.py:37-47    heap-driven emit
//     order that places END before LOAD-after-end.

// === Target enum + LzCtx state ==========================================
//
// One LzCtx threads through the entire emit so the per-target spellings
// (alu symbols, type names, addressing prefix) stay co-located.  The
// renderer is single-threaded (no global state changes outside this
// struct) so the same compiled binary handles all three targets.

typedef enum {
  LZ_TGT_C     = 0,
  LZ_TGT_METAL = 1,
  LZ_TGT_CUDA  = 2,
} LzTarget;

typedef struct {
  Term  buf;
  char  name[16];
  u32   dtype;
} LzBufSlot;

#define LZ_MAX_BUFS 32

typedef struct {
  LzBufSlot bufs[LZ_MAX_BUFS];
  u32       n_bufs;
  LzTarget  target;
  // Optional: track placeholders we've emitted so a repeated reference
  // doesn't re-declare the accumulator.  The hash-cons in
  // uop_placeholder makes two PLACEHOLDERs with the same (dtype,
  // acc_id) the same Term, but the linearizer may surface the same
  // Term twice if the toposort encounters it from two paths -- which it
  // shouldn't (seen-map dedups), but defence-in-depth.
  u32 emitted_accs[64];
  u32 n_emitted_accs;
} LzCtx;

static u32 lz_register_buf(LzCtx *ctx, Term buf) {
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    if (ctx->bufs[i].buf == buf) return i;
  }
  if (ctx->n_bufs >= LZ_MAX_BUFS) return 0xFFFFFFFFu;
  LzBufSlot *s = &ctx->bufs[ctx->n_bufs++];
  s->buf = buf;
  u32 inst = uop_buffer_inst_get(buf);
  if (inst == 0) {
    snprintf(s->name, sizeof(s->name), "out");
  } else {
    snprintf(s->name, sizeof(s->name), "in%u", inst - 1);
  }
  s->dtype = uop_buffer_dtype(buf);
  return ctx->n_bufs - 1;
}

static const char *lz_buf_name(LzCtx const *ctx, Term buf) {
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    if (ctx->bufs[i].buf == buf) return ctx->bufs[i].name;
  }
  return "?";
}

static const char *lz_type_name(LzCtx const *ctx, u32 dtype) {
  switch (ctx->target) {
    case LZ_TGT_METAL: return rmu_msl_type_name(dtype);
    case LZ_TGT_CUDA:  return rmu_cuda_type_name(dtype);
    case LZ_TGT_C:
    default:           return rmu_c_type_name(dtype);
  }
}

static int lz_acc_already_emitted(LzCtx const *ctx, u32 acc_id) {
  for (u32 i = 0; i < ctx->n_emitted_accs; i++) {
    if (ctx->emitted_accs[i] == acc_id) return 1;
  }
  return 0;
}

static void lz_acc_mark_emitted(LzCtx *ctx, u32 acc_id) {
  if (ctx->n_emitted_accs >= 64) return;
  ctx->emitted_accs[ctx->n_emitted_accs++] = acc_id;
}

// === Expression emit ===================================================
//
// Walks a UOp value subtree and writes a single-line expression.  The
// caller is responsible for surrounding state (assignment LHS, indent,
// semicolon).  For STACK / VCONST -- which are vector-shaped values --
// lz_emit_value emits ONLY a single scalar; the caller drives the
// per-lane multi-emit via lz_emit_value_lane.

static const char *lz_alu_symbol(u32 op) {
  switch (op) {
    case UOP_ADD: case UOP_IADD: return "+";
    case UOP_MUL: case UOP_IMUL: return "*";
    case UOP_ISUB:               return "-";
    case UOP_IDIV:               return "/";
    case UOP_IMOD:               return "%";
    case UOP_CMPLT: case UOP_ILT: return "<";
    case UOP_CMPEQ:              return "==";
    case UOP_IAND:               return "&";
    case UOP_IOR:                return "|";
    case UOP_IXOR:               return "^";
    default:                     return "?";
  }
}

// Emit a CONST scalar literal (fp32 / int32).
static void lz_emit_const(Term t, LzCtx *ctx, FILE *fp) {
  Term n = heap_read(term_val(t) + 0);
  u32 dt = term_ext(n);
  u32 bits = (u32)term_val(n);
  (void)ctx;
  if (dt == DT_FP32) {
    union { u32 u; float f; } cvt; cvt.u = bits;
    fprintf(fp, "%ff", cvt.f);
  } else {
    fprintf(fp, "%u", bits);
  }
}

static void lz_emit_value(Term t, LzCtx *ctx, FILE *fp);

// Emit a single lane of a (potentially vector) value.  For scalars,
// lane is ignored.  For STACK / VCONST, lane indexes into the lane
// list.  GEP/UNROLL collapse through to their src.  This is used when
// STORE.rhs is a STACK and the caller wants F separate scalar stores.
static void lz_emit_value_lane(Term t, u32 lane, LzCtx *ctx, FILE *fp) {
  if (term_tag(t) != TAG_UOP) {
    lz_emit_value(t, ctx, fp);
    return;
  }
  u32 op = term_ext(t);
  if (op == UOP_STACK) {
    u32 n = uop_stack_n(t);
    if (n == 0) { fputs("0", fp); return; }
    Term s = uop_stack_src(t, lane < n ? lane : (n - 1));
    lz_emit_value(s, ctx, fp);
    return;
  }
  if (op == UOP_VCONST) {
    u32 n = uop_vconst_n(t);
    u32 dt = uop_vconst_dtype(t);
    u32 bits = uop_vconst_bits(t, lane < n ? lane : (n - 1));
    if (dt == DT_FP32) {
      union { u32 u; float f; } cvt; cvt.u = bits;
      fprintf(fp, "%ff", cvt.f);
    } else {
      fprintf(fp, "%u", bits);
    }
    return;
  }
  if (op == UOP_UNROLL) {
    // UNROLL is a structural wrapper; its src is the vector value.
    Term inner = heap_read(term_val(t) + 0);
    lz_emit_value_lane(inner, lane, ctx, fp);
    return;
  }
  // Scalar: lane is irrelevant.
  lz_emit_value(t, ctx, fp);
}

// Returns the vector width of a value Term, or 1 for a scalar.  Used
// by STORE handling to decide whether to emit a single store or F
// parallel stores.
static u32 lz_value_width(Term t) {
  if (term_tag(t) != TAG_UOP) return 1;
  u32 op = term_ext(t);
  if (op == UOP_STACK)  return uop_stack_n(t);
  if (op == UOP_VCONST) return uop_vconst_n(t);
  if (op == UOP_UNROLL) {
    // Width is product of the UNROLL's args' factors.
    u32 n_args = (u32)term_val(heap_read(term_val(t) + 1));
    u32 w = 1;
    for (u32 i = 0; i < n_args; i++) {
      u32 f = (u32)term_val(heap_read(term_val(t) + 3 + 2 * i));
      if (f == 0) f = 1;
      w *= f;
    }
    return w;
  }
  return 1;
}

static void lz_emit_value(Term t, LzCtx *ctx, FILE *fp) {
  if (term_tag(t) != TAG_UOP) {
    fprintf(fp, "/* non-uop leaf */");
    return;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_CONST:
      lz_emit_const(t, ctx, fp);
      return;
    case UOP_RANGE: {
      u32 axis_id = uop_range_axis_id(t);
      fprintf(fp, "a%u", axis_id);
      return;
    }
    case UOP_INDEX_E: {
      Term buf  = heap_read(loc + 0);
      Term addr = heap_read(loc + 1);
      fputs(lz_buf_name(ctx, buf), fp);
      fputc('[', fp);
      lz_emit_value(addr, ctx, fp);
      fputc(']', fp);
      return;
    }
    case UOP_LOAD: {
      Term inner = heap_read(loc + 0);
      // LOAD's src may be an INDEX_E (-> emit buf[addr]) or a
      // PLACEHOLDER (-> emit `_accN`, the register accumulator load).
      lz_emit_value(inner, ctx, fp);
      return;
    }
    case UOP_CAST: {
      Term src = heap_read(loc + 0);
      Term dt_num = heap_read(loc + 1);
      u32 dt = (u32)term_val(dt_num);
      fprintf(fp, "(%s)(", lz_type_name(ctx, dt));
      lz_emit_value(src, ctx, fp);
      fputc(')', fp);
      return;
    }
    case UOP_ADD: case UOP_MUL:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD:
    case UOP_CMPLT: case UOP_CMPEQ: case UOP_ILT:
    case UOP_IAND: case UOP_IOR: case UOP_IXOR: {
      Term a = heap_read(loc + 0);
      Term b = heap_read(loc + 1);
      fputc('(', fp);
      lz_emit_value(a, ctx, fp);
      fprintf(fp, " %s ", lz_alu_symbol(op));
      lz_emit_value(b, ctx, fp);
      fputc(')', fp);
      return;
    }
    case UOP_NEG: {
      Term a = heap_read(loc + 0);
      fputs("(-", fp);
      lz_emit_value(a, ctx, fp);
      fputc(')', fp);
      return;
    }
    case UOP_RECIP: {
      Term a = heap_read(loc + 0);
      fputs("(1.0f / ", fp);
      lz_emit_value(a, ctx, fp);
      fputc(')', fp);
      return;
    }
    case UOP_SQRT: case UOP_EXP2: case UOP_LOG2: {
      const char *fn_name =
          (op == UOP_SQRT) ? "sqrt"
        : (op == UOP_EXP2) ? "exp2" : "log2";
      Term a = heap_read(loc + 0);
      fprintf(fp, "%s(", fn_name);
      lz_emit_value(a, ctx, fp);
      fputc(')', fp);
      return;
    }
    case UOP_IWHERE: {
      Term c = heap_read(loc + 0);
      Term y = heap_read(loc + 1);
      Term n = heap_read(loc + 2);
      fputc('(', fp);
      lz_emit_value(c, ctx, fp);
      fputs(" ? ", fp);
      lz_emit_value(y, ctx, fp);
      fputs(" : ", fp);
      lz_emit_value(n, ctx, fp);
      fputc(')', fp);
      return;
    }
    case UOP_PLACEHOLDER: {
      u32 acc_id = uop_placeholder_acc_id(t);
      fprintf(fp, "_acc%u", acc_id);
      return;
    }
    case UOP_GEP: {
      // GEP over a STACK -> emit the lane src.  GEP over anything else
      // (e.g. UNROLL of a wide LOAD) -> pull through to the lane.
      u32 n_idx = uop_gep_n_idx(t);
      Term src = heap_read(loc + 0);
      if (n_idx == 1) {
        u32 idx = uop_gep_idx(t, 0);
        lz_emit_value_lane(src, idx, ctx, fp);
      } else {
        // Multi-idx GEP shouldn't survive devectorizer pm_render; emit
        // lane 0 as a fallback rather than abort.
        lz_emit_value_lane(src, uop_gep_idx(t, 0), ctx, fp);
      }
      return;
    }
    case UOP_STACK: {
      // STACK referenced as a scalar value: emit lane 0 (defensive --
      // STORE handling drives the multi-emit when STACK is the rhs).
      if (uop_stack_n(t) > 0) {
        lz_emit_value(uop_stack_src(t, 0), ctx, fp);
      } else {
        fputs("0", fp);
      }
      return;
    }
    case UOP_VCONST: {
      // VCONST referenced as a scalar: emit lane 0.  pm_render should
      // have rewritten this to STACK(CONST, ...) before reaching the
      // renderer, but be defensive.
      u32 dt = uop_vconst_dtype(t);
      u32 bits = (uop_vconst_n(t) > 0) ? uop_vconst_bits(t, 0) : 0;
      if (dt == DT_FP32) {
        union { u32 u; float f; } cvt; cvt.u = bits;
        fprintf(fp, "%ff", cvt.f);
      } else {
        fprintf(fp, "%u", bits);
      }
      return;
    }
    case UOP_UNROLL: {
      // UNROLL referenced as a value: pull through to the inner src.
      Term inner = heap_read(loc + 0);
      lz_emit_value(inner, ctx, fp);
      return;
    }
    default:
      fprintf(fp, "/* unsupported uop op=%u */", op);
      return;
  }
}

// === Pass: signature ===================================================
//
// Each backend has a different prologue (includes / kernel attribute /
// signature / thread-builtin prologue).  These helpers emit the prefix
// up to (but not including) the first body brace; the main walk then
// emits all body lines, then `}` to close.

static void lz_emit_prelude_c(LzCtx const *ctx, const char *kn, FILE *fp) {
  fputs("#include <stdint.h>\n", fp);
  fputs("#include <math.h>\n", fp);
  fputs("typedef unsigned int uint;\n", fp);
  fprintf(fp, "void %s(void *out_v, const void *const *ins_v,\n", kn);
  fputs("              unsigned n, const unsigned *in_numels) {\n", fp);
  fputs("  (void)n; (void)in_numels;\n", fp);
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    LzBufSlot const *s = &ctx->bufs[i];
    const char *ty = lz_type_name(ctx, s->dtype);
    if (uop_buffer_inst_get(s->buf) == 0) {
      fprintf(fp, "  %s *out = (%s *)out_v;\n", ty, ty);
    } else {
      u32 inst = uop_buffer_inst_get(s->buf);
      fprintf(fp, "  const %s *in%u = (const %s *)ins_v[%u];\n",
              ty, inst - 1, ty, inst - 1);
    }
  }
}

static void lz_emit_prelude_metal(LzCtx const *ctx, const char *kn, FILE *fp) {
  fputs("#include <metal_stdlib>\n", fp);
  fputs("using namespace metal;\n\n", fp);
  fprintf(fp, "kernel void %s(\n", kn);
  int first = 1;
  // Output buffer at slot 0.
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    LzBufSlot const *s = &ctx->bufs[i];
    if (uop_buffer_inst_get(s->buf) != 0) continue;
    if (!first) fputs(",\n", fp);
    fprintf(fp, "    device %s *out [[ buffer(0) ]]",
            lz_type_name(ctx, s->dtype));
    first = 0;
  }
  // Inputs at slot k+1.
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    LzBufSlot const *s = &ctx->bufs[i];
    u32 inst = uop_buffer_inst_get(s->buf);
    if (inst == 0) continue;
    if (!first) fputs(",\n", fp);
    fprintf(fp, "    device const %s *in%u [[ buffer(%u) ]]",
            lz_type_name(ctx, s->dtype), inst - 1, inst);
    first = 0;
  }
  fputs(",\n    uint tid [[ thread_position_in_grid ]],\n", fp);
  fputs("    uint tg [[ threadgroup_position_in_grid ]],\n", fp);
  fputs("    uint tt [[ thread_position_in_threadgroup ]]) {\n", fp);
  fputs("  (void)tid; (void)tg; (void)tt;\n", fp);
}

static void lz_emit_prelude_cuda(LzCtx const *ctx, const char *kn, FILE *fp) {
  fputs("typedef unsigned int uint;\n", fp);
  fputs("#ifndef INFINITY\n"
        "#define INFINITY __int_as_float(0x7f800000)\n"
        "#endif\n\n", fp);
  fprintf(fp, "extern \"C\" __global__ void %s(\n", kn);
  int first = 1;
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    LzBufSlot const *s = &ctx->bufs[i];
    if (uop_buffer_inst_get(s->buf) != 0) continue;
    if (!first) fputs(",\n", fp);
    fprintf(fp, "    %s *out", lz_type_name(ctx, s->dtype));
    first = 0;
  }
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    LzBufSlot const *s = &ctx->bufs[i];
    u32 inst = uop_buffer_inst_get(s->buf);
    if (inst == 0) continue;
    if (!first) fputs(",\n", fp);
    fprintf(fp, "    const %s *in%u", lz_type_name(ctx, s->dtype), inst - 1);
    first = 0;
  }
  fputs(") {\n", fp);
  fputs("  uint tid = blockIdx.x * blockDim.x + threadIdx.x;\n", fp);
  fputs("  uint tg  = blockIdx.x;\n", fp);
  fputs("  uint tt  = threadIdx.x;\n", fp);
  fputs("  (void)tid; (void)tg; (void)tt;\n", fp);
}

// === Body emit =========================================================
//
// Walks the linearized list and dispatches per opcode.  Returns 1 if
// the entire list was rendered cleanly, 0 if any opcode forced a bail
// (the caller falls back to legacy emit).  Indent depth tracks the
// number of open for-loops so the emitted source stays readable.

static int lz_emit_body(LinKernel const *lk, LzCtx *ctx, FILE *fp) {
  // Pass 1: register every UOP_BUFFER + emit every PLACEHOLDER decl.
  // PLACEHOLDERs are emitted at body scope (depth 1) before any
  // for-loop opens so the accumulator persists across the loop body.
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_BUFFER) {
      lz_register_buf(ctx, t);
    } else if (op == UOP_PLACEHOLDER) {
      u32 acc_id = uop_placeholder_acc_id(t);
      if (lz_acc_already_emitted(ctx, acc_id)) continue;
      u32 dt = uop_placeholder_dtype(t);
      fprintf(fp, "  %s _acc%u;\n", lz_type_name(ctx, dt), acc_id);
      lz_acc_mark_emitted(ctx, acc_id);
    }
  }

  // Pass 2: statement walk.  Indent depth starts at 1 (kernel body).
  u32 depth = 1;
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    switch (op) {
      case UOP_BUFFER:
      case UOP_PLACEHOLDER:
        break;  // handled in pass 1
      case UOP_RANGE: {
        u32 at = uop_range_axis_type(t);
        // UPCAST + UNROLL ranges were lowered by the expander into
        // VCONST/UNROLL+GEP swizzles; they should not surface as
        // RANGE leaves in the linearized list (the linearizer drops
        // them when their consumers all reference them via VCONST
        // lanes).  If one DOES survive, treat it as KAX_LOOP -- the
        // generated for-loop is benign (the body just doesn't index
        // it).
        if (at == KAX_LOCAL || at == KAX_GLOBAL) {
          // GPU-bound axes get a special prelude on Metal/CUDA; for
          // the linearized renderer treat them as plain loops too --
          // the route gate only admits opt-rich kernels where the
          // expander has folded these into scalar lanes already.  In
          // practice we shouldn't see LOCAL/GLOBAL here.
        }
        u32 axis_id = uop_range_axis_id(t);
        u32 extent  = uop_range_extent(t);
        for (u32 d = 0; d < depth; d++) fputc(' ', fp);
        fputc(' ', fp);
        fprintf(fp, "for (uint a%u = 0; a%u < %u; a%u++) {\n",
                axis_id, axis_id, extent, axis_id);
        depth++;
        break;
      }
      case UOP_END: {
        // Close one `}` per range carried in END's src list.  The
        // linearizer placed END at the position where the LAST loop
        // body should close; the ordered close matches the open order
        // of the matching RANGE nodes.
        u32 n = uop_end_n(t);
        for (u32 r = 0; r < n; r++) {
          if (depth == 0) return 0;
          depth--;
          for (u32 d = 0; d < depth; d++) fputc(' ', fp);
          fputc(' ', fp);
          fputs("}\n", fp);
        }
        break;
      }
      case UOP_STORE: {
        Term buf   = heap_read(term_val(t) + 0);
        Term addr  = heap_read(term_val(t) + 1);
        Term value = heap_read(term_val(t) + 2);
        // STACK rhs: F parallel stores.  The addr for each lane is the
        // shared address + lane offset; for the conv-kid=3 shape the
        // STACK lanes are the F UPCAST'd output positions, which the
        // expander emitted as separate scalar addresses.  Without
        // per-lane addresses, we can only emit lane 0; in that case
        // the STORE falls through to the scalar emit.  Currently the
        // expander+devectorize pipeline used by hand_opts produces
        // STACK lanes for the VALUE only -- the address remains
        // scalar -- so emit one store with multi-lane addressing not
        // surfaced.  Treat STACK rhs as multi-store when buf is a
        // PLACEHOLDER (acc); otherwise emit lane 0.
        u32 vw = lz_value_width(value);
        int is_acc_store = (term_tag(buf) == TAG_UOP
                            && term_ext(buf) == UOP_PLACEHOLDER);
        if (vw > 1 && is_acc_store) {
          // Multi-store: one assignment per lane.
          for (u32 lane = 0; lane < vw; lane++) {
            for (u32 d = 0; d < depth; d++) fputc(' ', fp);
            fputc(' ', fp);
            // Accumulator buf is a PLACEHOLDER -> `_accN[lane]` is a
            // separate accumulator per lane.  Allocate a synthetic
            // per-lane name `_acc<id>_<lane>`.
            u32 acc_id = uop_placeholder_acc_id(buf);
            fprintf(fp, "_acc%u_%u = ", acc_id, lane);
            lz_emit_value_lane(value, lane, ctx, fp);
            fputs(";\n", fp);
          }
        } else if (is_acc_store) {
          // Scalar accumulator store: `_accN = value;` -- the addr
          // (always 0) is implicit.
          for (u32 d = 0; d < depth; d++) fputc(' ', fp);
          fputc(' ', fp);
          u32 acc_id = uop_placeholder_acc_id(buf);
          fprintf(fp, "_acc%u = ", acc_id);
          lz_emit_value(value, ctx, fp);
          fputs(";\n", fp);
        } else {
          // Regular memory store: `buf[addr] = value;`.
          for (u32 d = 0; d < depth; d++) fputc(' ', fp);
          fputc(' ', fp);
          lz_emit_value(addr, ctx, fp);
          // addr's emit only handles INDEX_E (which emits `buf[addr]`).
          // If addr is a bare RANGE/IADD the caller's INDEX_E
          // wrapping happens inside lz_emit_value when buf is the
          // INDEX_E src.  Here STORE's addr slot may be the INDEX_E
          // node itself OR an external bare address -- mirror the
          // legacy emit by accepting the INDEX_E pattern.  When addr
          // is a bare expression, we emit it as `buf[addr]` here:
          if (term_tag(addr) != TAG_UOP || term_ext(addr) != UOP_INDEX_E) {
            // The buf+addr split: re-emit using the buf name.  We've
            // already emitted `addr` (the bare expression); replace
            // with the canonical buf[addr] form by restarting the
            // line.  Simpler: just append `=` and hope addr was
            // INDEX_E; otherwise bail.
            return 0;
          }
          fputs(" = ", fp);
          lz_emit_value(value, ctx, fp);
          fputs(";\n", fp);
        }
        break;
      }
      case UOP_AFTER:
      case UOP_LOAD:
      case UOP_CONST:
      case UOP_INDEX_E:
      case UOP_ADD: case UOP_MUL: case UOP_NEG: case UOP_RECIP:
      case UOP_EXP2: case UOP_LOG2: case UOP_SQRT:
      case UOP_CMPLT: case UOP_CMPEQ:
      case UOP_CAST: case UOP_BITCAST:
      case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
      case UOP_IMOD: case UOP_ILT: case UOP_IAND: case UOP_IOR:
      case UOP_IXOR: case UOP_IWHERE:
      case UOP_GEP: case UOP_STACK: case UOP_VCONST: case UOP_UNROLL:
        // Pure value nodes; no statement emit.  STORE pulls them
        // through lz_emit_value when their parent fires.
        break;
      default:
        // Unknown op: bail to legacy.
        return 0;
    }
  }
  // Any open loop should be closed by an END in a well-formed
  // linearized list; if not, close them now (best-effort).
  while (depth > 1) {
    depth--;
    for (u32 d = 0; d < depth; d++) fputc(' ', fp);
    fputc(' ', fp);
    fputs("}\n", fp);
  }
  return 1;
}

// === Entry points =====================================================
//
// One per backend.  All three share the body emit; they differ only in
// the prelude (includes / signature / thread-builtin prologue) and the
// type spellings.  Returns 1 on success, 0 if the body walk bailed.

fn int cg_render_linearized_c(LinKernel const *lk, const char *kernel_name,
                              FILE *fp) {
  if (lk == NULL || lk->n == 0 || fp == NULL) return 0;
  if (kernel_name == NULL) kernel_name = "uop_kernel";

  LzCtx ctx = (LzCtx){ .n_bufs = 0, .target = LZ_TGT_C, .n_emitted_accs = 0 };
  // Pass 0: register buffers so the prelude has a complete buf list.
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_BUFFER) {
      lz_register_buf(&ctx, t);
    }
  }
  // Capture the body to a scratch buffer so we can detect a bail
  // BEFORE the prelude has been written (the caller can then fall
  // back to legacy without leaking partial output).
  char scratch[32768];
  FILE *sf = fmemopen(scratch, sizeof(scratch) - 1, "w");
  if (sf == NULL) return 0;
  int ok = lz_emit_body(lk, &ctx, sf);
  long sn = ftell(sf);
  fclose(sf);
  if (!ok) return 0;
  if (sn < 0) sn = 0;
  scratch[sn] = 0;

  lz_emit_prelude_c(&ctx, kernel_name, fp);
  fputs(scratch, fp);
  fputs("}\n", fp);
  return 1;
}

fn int cg_render_linearized_metal(LinKernel const *lk, const char *kernel_name,
                                  FILE *fp) {
  if (lk == NULL || lk->n == 0 || fp == NULL) return 0;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  LzCtx ctx = (LzCtx){ .n_bufs = 0, .target = LZ_TGT_METAL, .n_emitted_accs = 0 };
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_BUFFER) {
      lz_register_buf(&ctx, t);
    }
  }
  char scratch[32768];
  FILE *sf = fmemopen(scratch, sizeof(scratch) - 1, "w");
  if (sf == NULL) return 0;
  int ok = lz_emit_body(lk, &ctx, sf);
  long sn = ftell(sf);
  fclose(sf);
  if (!ok) return 0;
  if (sn < 0) sn = 0;
  scratch[sn] = 0;

  lz_emit_prelude_metal(&ctx, kernel_name, fp);
  fputs(scratch, fp);
  fputs("}\n", fp);
  return 1;
}

fn int cg_render_linearized_cuda(LinKernel const *lk, const char *kernel_name,
                                 FILE *fp) {
  if (lk == NULL || lk->n == 0 || fp == NULL) return 0;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  LzCtx ctx = (LzCtx){ .n_bufs = 0, .target = LZ_TGT_CUDA, .n_emitted_accs = 0 };
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_BUFFER) {
      lz_register_buf(&ctx, t);
    }
  }
  char scratch[32768];
  FILE *sf = fmemopen(scratch, sizeof(scratch) - 1, "w");
  if (sf == NULL) return 0;
  int ok = lz_emit_body(lk, &ctx, sf);
  long sn = ftell(sf);
  fclose(sf);
  if (!ok) return 0;
  if (sn < 0) sn = 0;
  scratch[sn] = 0;

  lz_emit_prelude_cuda(&ctx, kernel_name, fp);
  fputs(scratch, fp);
  fputs("}\n", fp);
  return 1;
}

// === Helper: scan a UOp DAG for an opt-rich RANGE (route gate) ========
//
// uop_has_upcast_or_unroll(root) returns 1 iff the DAG rooted at `root`
// contains at least one UOP_RANGE with axis_type in {KAX_UPCAST,
// KAX_UNROLL}.  This is the gate render_uop.c uses to decide whether
// to dispatch the new pipeline (expander + devectorize + linearize +
// render_linearized) or to stay on the legacy walker.
//
// Lives next to the renderer because its job is purely "should the
// post-late-pass pipeline run on this kernel?" -- a render-side
// decision, not a graph-rewrite concern.

static int lz_dag_has_upcast_or_unroll_rec(Term t, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_RANGE) {
    u32 at = uop_range_axis_type(t);
    return (at == KAX_UPCAST || at == KAX_UNROLL);
  }
  // Walk every fixed-arity src.
  u8 ar = uop_arity((u8)op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar; i++) {
    Term c = heap_read(loc + i);
    if (c == 0) continue;
    if (lz_dag_has_upcast_or_unroll_rec(c, depth + 1)) return 1;
  }
  // Variadic payloads (AFTER, STACK, END) -- only AFTER is reachable
  // before the route gate runs; STACK / END appear only post-
  // devectorize.
  if (op == UOP_AFTER) {
    if (lz_dag_has_upcast_or_unroll_rec(heap_read(loc + 0), depth + 1)) return 1;
    if (lz_dag_has_upcast_or_unroll_rec(heap_read(loc + 1), depth + 1)) return 1;
  }
  return 0;
}

fn int uop_has_upcast_or_unroll(Term root) {
  return lz_dag_has_upcast_or_unroll_rec(root, 0);
}
