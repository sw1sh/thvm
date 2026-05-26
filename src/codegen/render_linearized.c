// codegen/render_linearized.c - new emit walk that consumes a LinKernel
// (produced by src/uop/linearize.c) rather than the legacy DAG walker
// in render_uop.c.
//
// Stage (b) of the architectural piece #3 wiring: the legacy walker
// in render_uop.c (rmu_emit_store + ~4500 lines) handles every shape
// the production scheduler can produce -- conv recognition, TC
// templates, parallel accumulators, broadcast hoisting, multi-output
// AFTER chains, kvar wedging, etc.  This new file does NONE of that.
// It exists as a proof-of-concept demonstrating the post-linearize
// emit shape so the next port agent has a known-good starting point.
//
// SCOPE: a single-store elementwise kernel over a single LOOP range.
// Specifically, it can emit code for the shape that test_uop_linearize
// case 1 builds: STORE(out, INDEX_E(out, r), ADD(LOAD(INDEX_E(in, r)),
// CONST)).  Anything beyond that (REDUCE, multi-axis, OPT-wrapped
// ranges) falls back by writing a `/* unsupported */` comment.
//
// The renderer entry points in render_uop.c are NOT modified -- they
// continue to call the legacy emit.  When the legacy emit eventually
// matches every test_render_uop case after a full port, the entry
// points can flip to this file.  Until then, render_linearized
// remains test-only infrastructure exercised by test_render_linearized.
//
// Reference:
//   tinygrad/codegen/__init__.py:140-143  do_render (the analog: it
//     takes a linearized UOp list and asks the renderer to emit a
//     string).  Tinygrad has one renderer per backend that walks the
//     LINEAR's src in order; this is the same pattern, single-backend
//     for the proof of concept.

// === Buffer-name + dtype lookup =========================================
//
// The linearized list has UOP_BUFFERs in emission order before any
// LOAD/STORE that uses them.  We assign names "out" / "in0" / "in1"
// based on their UOP_BUFFER.instance the same way rmu_buf_name does.
// Because we don't deduplicate against the legacy buffer-name table,
// this renderer is independent of any RMU_* state.

typedef struct {
  Term  buf;
  char  name[16];
  u32   dtype;
} LzBufSlot;

#define LZ_MAX_BUFS 16

typedef struct {
  LzBufSlot bufs[LZ_MAX_BUFS];
  u32       n_bufs;
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

// === Expression emit (recursive, terminal) ============================
//
// Walks a UOp value subtree and writes a single-line C/MSL expression.
// Mirrors rmu_emit_value but with no inheritance from the legacy
// emit's accumulator/hoist/opt machinery.  Supports: CONST (scalar),
// LOAD(INDEX_E(buf, addr)), binary/unary ALU, CAST, RANGE-as-int.

static const char *lz_alu_symbol(u32 op) {
  switch (op) {
    case UOP_ADD: case UOP_IADD: return "+";
    case UOP_MUL: case UOP_IMUL: return "*";
    case UOP_ISUB:               return "-";
    case UOP_IDIV:               return "/";
    case UOP_IMOD:               return "%";
    default:                     return "?";
  }
}

static void lz_emit_value(Term t, LzCtx *ctx, FILE *fp) {
  if (term_tag(t) != TAG_UOP) {
    fprintf(fp, "/* non-uop leaf */");
    return;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_CONST: {
      Term n = heap_read(loc + 0);
      u32 dt = term_ext(n);
      u32 bits = (u32)term_val(n);
      if (dt == DT_FP32) {
        union { u32 u; float f; } cvt; cvt.u = bits;
        fprintf(fp, "%ff", cvt.f);
      } else {
        fprintf(fp, "%u", bits);
      }
      return;
    }
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
      lz_emit_value(inner, ctx, fp);
      return;
    }
    case UOP_CAST: {
      Term src = heap_read(loc + 0);
      Term dt_num = heap_read(loc + 1);
      u32 dt = (u32)term_val(dt_num);
      fprintf(fp, "(%s)(", rmu_c_type_name(dt));
      lz_emit_value(src, ctx, fp);
      fputc(')', fp);
      return;
    }
    case UOP_ADD: case UOP_MUL:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD: {
      Term a = heap_read(loc + 0);
      Term b = heap_read(loc + 1);
      fputc('(', fp);
      lz_emit_value(a, ctx, fp);
      fprintf(fp, " %s ", lz_alu_symbol(op));
      lz_emit_value(b, ctx, fp);
      fputc(')', fp);
      return;
    }
    case UOP_PLACEHOLDER: {
      u32 acc_id = uop_placeholder_acc_id(t);
      fprintf(fp, "_acc%u", acc_id);
      return;
    }
    default:
      fprintf(fp, "/* unsupported uop op=%u */", op);
      return;
  }
}

// === Linearized-list emit =============================================
//
// Walks `lk` in emission order and writes a self-contained C99 kernel.
// Strategy:
//   1. First pass: discover every UOP_BUFFER + dtype, register names.
//      Find the unique LOOP RANGE (the single output dimension).
//      Find the single STORE (the sink).
//   2. Emit the signature.
//   3. Emit `for (uint a<id> = 0; a<id> < extent; a<id>++) {`.
//   4. Emit the STORE body using lz_emit_value on the value subtree.
//   5. Close the loop.
//
// Returns 1 if the shape matched (elementwise STORE over one LOOP);
// 0 otherwise (caller falls back to legacy emit).

fn int cg_render_linearized_c(LinKernel const *lk, const char *kernel_name,
                              FILE *fp) {
  if (lk == NULL || lk->n == 0 || fp == NULL) return 0;
  if (kernel_name == NULL) kernel_name = "uop_kernel";

  LzCtx ctx = (LzCtx){ .n_bufs = 0 };
  Term store = 0;
  Term range = 0;
  u32  range_axis = 0;
  u32  range_extent = 0;
  Term out_buf_local = 0;

  // First pass: scan for sink STORE + unique RANGE + register BUFFERs.
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    switch (op) {
      case UOP_BUFFER:
        lz_register_buf(&ctx, t);
        break;
      case UOP_RANGE: {
        u32 at = uop_range_axis_type(t);
        if (at != KAX_LOOP) return 0;   // out of scope for the stub
        if (range != 0) return 0;       // multi-axis: out of scope
        range        = t;
        range_axis   = uop_range_axis_id(t);
        range_extent = uop_range_extent(t);
        break;
      }
      case UOP_STORE:
        if (store != 0) return 0;       // multi-store: out of scope
        store = t;
        out_buf_local = heap_read(term_val(t) + 0);
        break;
      case UOP_REDUCE:
      case UOP_AFTER:
        return 0;                       // stub doesn't handle these
      default:
        break;
    }
  }
  if (store == 0 || range == 0 || out_buf_local == 0) return 0;
  // Ensure the output buffer got registered (it must have, but be safe).
  lz_register_buf(&ctx, out_buf_local);

  // Signature.
  fputs("#include <stdint.h>\n", fp);
  fputs("typedef unsigned int uint;\n", fp);
  fprintf(fp, "void %s(void *out_v, const void *const *ins_v,\n",
          kernel_name);
  fputs("              unsigned n, const unsigned *in_numels) {\n", fp);
  fputs("  (void)n; (void)in_numels;\n", fp);
  // Output pointer (instance == 0).
  for (u32 i = 0; i < ctx.n_bufs; i++) {
    LzBufSlot *s = &ctx.bufs[i];
    if (uop_buffer_inst_get(s->buf) == 0) {
      const char *ty = rmu_c_type_name(s->dtype);
      fprintf(fp, "  %s *out = (%s *)out_v;\n", ty, ty);
    }
  }
  // Input pointers (instance > 0).
  u32 in_count = 0;
  for (u32 i = 0; i < ctx.n_bufs; i++) {
    LzBufSlot *s = &ctx.bufs[i];
    u32 inst = uop_buffer_inst_get(s->buf);
    if (inst == 0) continue;
    const char *ty = rmu_c_type_name(s->dtype);
    fprintf(fp, "  const %s *in%u = (const %s *)ins_v[%u];\n",
            ty, inst - 1, ty, inst - 1);
    in_count++;
  }
  (void)in_count;

  // Body.
  fprintf(fp, "  for (uint a%u = 0; a%u < %u; a%u++) {\n",
          range_axis, range_axis, range_extent, range_axis);
  Term addr  = heap_read(term_val(store) + 1);
  Term value = heap_read(term_val(store) + 2);
  fputs("    ", fp);
  lz_emit_value(addr, &ctx, fp);
  fputs(" = ", fp);
  lz_emit_value(value, &ctx, fp);
  fputs(";\n", fp);
  fputs("  }\n", fp);
  fputs("}\n", fp);
  return 1;
}
