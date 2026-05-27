// codegen/render_ptx.c - PTX assembly emitter consuming a LinKernel.
//
// Port of tinygrad/renderer/ptx.py.  Unlike render_linearized.c (which
// emits C/CUDA-C source destined for nvrtc's C++ frontend), this walks
// the SAME linearized UOp list and emits PTX text directly, so the CUDA
// jit can cuModuleLoadData() it WITHOUT nvrtc.  That bypass is the whole
// point: nvrtc's C++ frontend is what blew up on the opt-rich conv
// kernels (hundreds of MAD statements -> tens of seconds to compile +
// register-file overflow on V100); a direct PTX emit gives us the same
// register-blocked kernels tinygrad's heuristic assumes.
//
// The model is tinygrad's exactly: walk the topo-ordered uops; assign
// each value-producing UOp its OWN SSA register (`%<prefix>_<type>_<n>`);
// emit one PTX instruction per node referencing the registers of its
// sources.  This is the OPPOSITE of render_linearized.c, which inlines
// value subtrees into `(a + b) * c` C expressions.
//
// Mapping from tinygrad ptx.py to here:
//   asm_for_op        -> ptx_emit_alu
//   string_rewrite    -> the per-opcode switch in ptx_emit_node
//   ssa()             -> ptx_ssa
//   render_kernel     -> cg_render_linearized_ptx prologue/epilogue
//   ptx_matcher INDEX->int64 ptr math -> done inline in UOP_INDEX_E
//     (mad.wide.s32 base + idx*itemsize) rather than as a pre-pass graph
//     rewrite; self-contained and avoids a separate rewrite stage.
//
// COVERAGE (milestone 1): PARAM(BUFFER) / CONST / INDEX_E / LOAD / STORE
// / integer+float ALU / CAST / RANGE(loop) / END.  Anything else
// (PLACEHOLDER acc, STACK/GEP/VCONST/UNROLL lanes, REDUCE, WMMA, gated
// loads, shared mem, bool-pred subtleties) returns 0 so the caller falls
// back to the C-source CUDA renderer.  Milestone 3 extends to the
// opt-rich (UPCAST parallel-accumulator) shape.
//
// References:
//   tinygrad/renderer/ptx.py            -- the spec (asm_for_op,
//                                          string_rewrite, render, render_kernel)
//   tinygrad/renderer/ptx.py:54-56      -- INDEX -> int64 pointer math
//   thvm: src/uop/linearize.c           -- the LinKernel producer
//   thvm: src/codegen/render_linearized.c -- the C-source sibling consumer

// === Register / type tables ===========================================

// PTX register type for a thvm dtype.  Matches tinygrad PTXRenderer.types.
static const char *ptx_reg_type(u32 dt) {
  switch (dt) {
    case DT_INT8:   case DT_INT16:  return "s16";
    case DT_INT32:                  return "s32";
    case DT_INT64:                  return "s64";
    case DT_UINT8:  case DT_UINT16: return "u16";
    case DT_UINT32:                 return "u32";
    case DT_UINT64:                 return "u64";
    case DT_FP16:                   return "f16";
    case DT_FP32:                   return "f32";
    case DT_FP64:                   return "f64";
    case DT_BOOL:                   return "pred";
    default:                        return "s32";
  }
}

// PTX memory type for ld/st.  Matches tinygrad PTXRenderer.mem_types
// (int8 -> s8, uint8/bool -> u8, f16 -> b16).
static const char *ptx_mem_type(u32 dt) {
  switch (dt) {
    case DT_INT8:                   return "s8";
    case DT_UINT8:  case DT_BOOL:   return "u8";
    case DT_INT16:                  return "s16";
    case DT_UINT16:                 return "u16";
    case DT_INT32:                  return "s32";
    case DT_UINT32:                 return "u32";
    case DT_INT64:                  return "s64";
    case DT_UINT64:                 return "u64";
    case DT_FP16:                   return "b16";
    case DT_FP32:                   return "f32";
    case DT_FP64:                   return "f64";
    default:                        return "s32";
  }
}

static int ptx_dtype_is_unsigned(u32 dt) {
  return dt == DT_UINT8 || dt == DT_UINT16 || dt == DT_UINT32
      || dt == DT_UINT64 || dt == DT_UINT4;
}

// === Renderer state ===================================================

#define PTX_MAX_BUFS    32
#define PTX_MAX_REGS    LIN_KERNEL_CAP
#define PTX_MAX_PREFIX  256

typedef struct {
  Term term;            // the value-producing UOp
  char reg[40];         // "%alu_f32_3"  (scalar register name)
} PtxRegSlot;

typedef struct {
  char prefix[40];      // "alu_f32"  (the per-(prefix,type) decl key)
  char type[8];         // "f32"
  u32  count;           // how many %<prefix>_<n> allocated
} PtxPrefixCount;

typedef struct {
  Term buf;
  u32  inst;            // 0 = output, k = input k-1
  u32  dtype;
  char param[16];       // "data0"
  char reg[40];         // "%dat_u64_0" base pointer register
} PtxBufSlot;

#define PTX_MAX_OPEN_RANGES 16

typedef struct {
  PtxBufSlot bufs[PTX_MAX_BUFS];
  u32        n_bufs;

  PtxRegSlot regs[PTX_MAX_REGS];
  u32        n_regs;

  PtxPrefixCount prefixes[PTX_MAX_PREFIX];
  u32            n_prefixes;

  // Stack of RANGE Terms whose loop is open (scaffold emitted, close
  // pending).  A matching UOP_END pops + emits the close; any still-open
  // range at body end is drained in reverse (mirrors the C renderer's
  // auto-close-at-end fallback for kernels with no explicit END node).
  Term open_ranges[PTX_MAX_OPEN_RANGES];
  u32  n_open;

  int sm;               // compute capability (70 = V100)
} PtxCtx;

// Allocate a fresh SSA register named "%<prefix>_<type>_<n>", bump the
// per-(prefix,type) counter, write the name into `out`.  Mirrors
// tinygrad ptx.py ssa().
static void ptx_ssa(PtxCtx *ctx, const char *prefix, u32 dt, char *out,
                    u32 out_sz) {
  const char *ty = ptx_reg_type(dt);
  char key[40];
  snprintf(key, sizeof(key), "%s_%s", prefix, ty);
  PtxPrefixCount *pc = NULL;
  for (u32 i = 0; i < ctx->n_prefixes; i++) {
    if (strcmp(ctx->prefixes[i].prefix, key) == 0) { pc = &ctx->prefixes[i]; break; }
  }
  if (pc == NULL && ctx->n_prefixes < PTX_MAX_PREFIX) {
    pc = &ctx->prefixes[ctx->n_prefixes++];
    snprintf(pc->prefix, sizeof(pc->prefix), "%s", key);
    snprintf(pc->type, sizeof(pc->type), "%s", ty);
    pc->count = 0;
  }
  u32 n = pc ? pc->count++ : 0;
  snprintf(out, out_sz, "%%%s_%u", key, n);
}

// Map a Term to its allocated register name (set when the node was
// walked).  Returns "" if unset (a renderer bug or unsupported node).
static void ptx_reg_put(PtxCtx *ctx, Term t, const char *reg) {
  if (ctx->n_regs >= PTX_MAX_REGS) return;
  PtxRegSlot *s = &ctx->regs[ctx->n_regs++];
  s->term = t;
  snprintf(s->reg, sizeof(s->reg), "%s", reg);
}

static const char *ptx_reg_get(PtxCtx const *ctx, Term t) {
  for (u32 i = ctx->n_regs; i > 0; i--) {
    if (ctx->regs[i - 1].term == t) return ctx->regs[i - 1].reg;
  }
  return "";
}

// Output dtype of a Term (env 0).  Falls back to fp32 on query failure.
static u32 ptx_dtype_of(Term t) {
  u32 dt = DT_FP32;
  if (term_dtype_in(t, 0, &dt)) return dt;
  return DT_FP32;
}

// Render a constant immediate the way PTX wants it: floats as a typed
// bit pattern ("0f3F800000"), ints as decimal.  Matches tinygrad
// render_val.
static void ptx_render_val(u32 dt, u32 bits, char *out, u32 out_sz) {
  if (dt == DT_FP32) {
    snprintf(out, out_sz, "0f%08X", bits);
  } else if (dt == DT_FP16) {
    snprintf(out, out_sz, "0x%04X", bits & 0xFFFFu);
  } else if (ptx_dtype_is_unsigned(dt)) {
    snprintf(out, out_sz, "%uU", bits);
  } else {
    snprintf(out, out_sz, "%d", (int)bits);
  }
}

// === Buffer (PARAM) registration ======================================

static PtxBufSlot *ptx_register_buf(PtxCtx *ctx, Term buf) {
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    if (ctx->bufs[i].buf == buf) return &ctx->bufs[i];
  }
  if (ctx->n_bufs >= PTX_MAX_BUFS) return NULL;
  PtxBufSlot *s = &ctx->bufs[ctx->n_bufs++];
  s->buf   = buf;
  s->inst  = uop_buffer_inst_get(buf);
  s->dtype = uop_buffer_dtype(buf);
  snprintf(s->param, sizeof(s->param), "data%u", s->inst);
  snprintf(s->reg, sizeof(s->reg), "%%dat_u64_%u", s->inst);
  return s;
}

static PtxBufSlot *ptx_find_buf(PtxCtx *ctx, Term buf) {
  for (u32 i = 0; i < ctx->n_bufs; i++) {
    if (ctx->bufs[i].buf == buf) return &ctx->bufs[i];
  }
  return NULL;
}

// === ALU emit =========================================================
//
// One PTX line per ALU op, dst/srcs already resolved to registers.
// Mirrors tinygrad asm_for_op.  `dt` is the op's result dtype; `tn` its
// PTX type name.  Returns 1 if the op was emitted, 0 if unsupported.

static int ptx_emit_alu(u32 op, const char *d, const char *a,
                        const char *b, const char *c, u32 dt, FILE *fp) {
  const char *tn = ptx_reg_type(dt);
  int is_f = dtype_is_float(dt);
  switch (op) {
    case UOP_ADD:  case UOP_IADD:
      fprintf(fp, "\tadd.%s %s, %s, %s;\n", tn, d, a, b); return 1;
    case UOP_ISUB:
      fprintf(fp, "\tsub.%s %s, %s, %s;\n", tn, d, a, b); return 1;
    case UOP_MUL:
      fprintf(fp, "\tmul.%s %s, %s, %s;\n", tn, d, a, b); return 1;
    case UOP_IMUL:
      fprintf(fp, "\tmul.lo.%s %s, %s, %s;\n", tn, d, a, b); return 1;
    case UOP_IDIV:
      fprintf(fp, "\tdiv.%s %s, %s, %s;\n", tn, d, a, b); return 1;
    case UOP_IMOD:
      fprintf(fp, "\trem.%s %s, %s, %s;\n", tn, d, a, b); return 1;
    case UOP_IAND:
      fprintf(fp, "\tand.b%s %s, %s, %s;\n", tn + 1, d, a, b); return 1;
    case UOP_IOR:
      fprintf(fp, "\tor.b%s %s, %s, %s;\n", tn + 1, d, a, b); return 1;
    case UOP_IXOR:
      fprintf(fp, "\txor.b%s %s, %s, %s;\n", tn + 1, d, a, b); return 1;
    case UOP_NEG:
      fprintf(fp, "\tneg.%s %s, %s;\n", tn, d, a); return 1;
    case UOP_RECIP:
      fprintf(fp, "\trcp.approx.%s %s, %s;\n", tn, d, a); return 1;
    case UOP_SQRT:
      fprintf(fp, "\tsqrt.approx.%s %s, %s;\n", tn, d, a); return 1;
    case UOP_EXP2:
      fprintf(fp, "\tex2.approx.%s %s, %s;\n", tn, d, a); return 1;
    case UOP_LOG2:
      fprintf(fp, "\tlg2.approx.%s %s, %s;\n", tn, d, a); return 1;
    case UOP_CMPLT: case UOP_ILT:
      fprintf(fp, "\tsetp.lt.%s %s, %s, %s;\n", tn, d, a, b); return 1;
    case UOP_CMPEQ:
      fprintf(fp, "\tsetp.eq.%s %s, %s, %s;\n", tn, d, a, b); return 1;
    case UOP_IWHERE:
      // selp.<type> d, then(b), else(c), pred(a)
      fprintf(fp, "\tselp.%s %s, %s, %s, %s;\n", tn, d, b, c, a); return 1;
    default:
      (void)is_f;
      return 0;
  }
}

// Comparison ops produce a .pred result regardless of operand dtype; the
// register must be typed pred but the setp suffix uses the OPERAND type.
static int ptx_is_cmp(u32 op) {
  return op == UOP_CMPLT || op == UOP_CMPEQ || op == UOP_ILT;
}

static int ptx_is_int_alu(u32 op) {
  return op == UOP_IADD || op == UOP_ISUB || op == UOP_IMUL
      || op == UOP_IDIV || op == UOP_IMOD
      || op == UOP_IAND || op == UOP_IOR || op == UOP_IXOR;
}

// The dtype to use for an ALU op's instruction suffix + result register.
// The integer I-family ops are not always tracked by term_dtype_in
// (index arithmetic over ranges defaults to fp32 there), so derive their
// type from the first operand -- a range / int const is int32.
static u32 ptx_alu_dtype(u32 op, Term t, Term a) {
  if (ptx_is_int_alu(op)) {
    u32 dt = ptx_dtype_of(a);
    return dtype_is_int(dt) ? dt : DT_INT32;
  }
  return ptx_dtype_of(t);
}

// Emit the loop-close for an open RANGE: increment the induction var,
// test against the extent, branch back to LOOP if still in range.
// Faithful to tinygrad ptx.py END (bottom-tested do-while).
static void ptx_emit_loop_close(PtxCtx *ctx, Term range, FILE *fp) {
  u32 axis_id = uop_range_axis_id(range);
  u32 extent  = uop_range_extent(range);
  const char *rr = ptx_reg_get(ctx, range);
  char p[40]; ptx_ssa(ctx, "pred", DT_BOOL, p, sizeof(p));
  fprintf(fp, "END_%u:\n", axis_id);
  fprintf(fp, "\tadd.s32 %s, %s, 1;\n", rr, rr);
  fprintf(fp, "\tsetp.lt.s32 %s, %s, %u;\n", p, rr, extent);
  fprintf(fp, "\t@%s bra LOOP_%u;\n", p, axis_id);
}

// === Body walk ========================================================
//
// Returns 1 if every node rendered, 0 if any node is outside coverage
// (caller falls back to the C-source CUDA renderer).

static int ptx_emit_body(LinKernel const *lk, PtxCtx *ctx, FILE *fp) {
  // Pre-pass: bail on any opcode milestone 1 doesn't cover, so we never
  // emit half a kernel.  These are the opt-rich / reduce / vector
  // shapes handled by milestone 3.
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_PLACEHOLDER || op == UOP_STACK || op == UOP_GEP
        || op == UOP_VCONST || op == UOP_UNROLL || op == UOP_CONTRACT
        || op == UOP_OPT || op == UOP_BUFFERIZE || op == UOP_REDUCE
        || op == UOP_BITCAST || op == UOP_INVALID) {
      return 0;
    }
  }

  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op  = term_ext(t);
    u64 loc = term_val(t);

    switch (op) {
      case UOP_BUFFER: {
        PtxBufSlot *s = ptx_register_buf(ctx, t);
        if (s == NULL) return 0;
        // Load the param pointer into its base register at the top.
        fprintf(fp, "\tld.param.u64 %s, [%s+0];\n", s->reg, s->param);
        break;
      }

      case UOP_CONST: {
        Term num = heap_read(loc + 0);
        u32 dt   = term_ext(num);
        u32 bits = (u32)term_val(num);
        char r[40]; ptx_ssa(ctx, "const", dt, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
        char val[24]; ptx_render_val(dt, bits, val, sizeof(val));
        fprintf(fp, "\tmov.b%s %s, %s;\n", ptx_reg_type(dt) + 1, r, val);
        break;
      }

      case UOP_RANGE: {
        // Loop induction variable.  Faithful to tinygrad's do-while:
        //   mov ridx, -1 ; bra END ; LOOP:
        // END (matching UOP_END, or the body-end drain) increments +
        // tests + branches back to LOOP.
        u32 axis_id = uop_range_axis_id(t);
        char r[40]; ptx_ssa(ctx, "ridx", DT_INT32, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
        fprintf(fp, "\tmov.u32 %s, 0xFFFFFFFF;\n", r);
        fprintf(fp, "\tbra END_%u;\n", axis_id);
        fprintf(fp, "LOOP_%u:\n", axis_id);
        if (ctx->n_open >= PTX_MAX_OPEN_RANGES) return 0;
        ctx->open_ranges[ctx->n_open++] = t;
        break;
      }

      case UOP_END: {
        // Close the range(s) this END marks.  Pop each from the open
        // stack (it must be open) and emit the close.
        u32 n = uop_end_n(t);
        for (u32 e = 0; e < n; e++) {
          Term r = uop_end_range(t, e);
          if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
          ptx_emit_loop_close(ctx, r, fp);
          // Remove r from the open stack.
          for (u32 k = 0; k < ctx->n_open; k++) {
            if (ctx->open_ranges[k] == r) {
              for (u32 m = k + 1; m < ctx->n_open; m++) {
                ctx->open_ranges[m - 1] = ctx->open_ranges[m];
              }
              ctx->n_open--;
              break;
            }
          }
        }
        break;
      }

      case UOP_INDEX_E: {
        // Pointer arithmetic: byte_addr = base + idx * itemsize, in u64.
        // mad.wide.s32 does (s32 * s32) -> s64, + s64, in one op.
        Term buf  = heap_read(loc + 0);
        Term addr = heap_read(loc + 1);
        PtxBufSlot *bs = ptx_find_buf(ctx, buf);
        if (bs == NULL) bs = ptx_register_buf(ctx, buf);
        if (bs == NULL) return 0;
        const char *ar = ptx_reg_get(ctx, addr);
        u32 isz = dtype_itemsize(bs->dtype);
        char r[40]; ptx_ssa(ctx, "index", DT_INT64, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
        if (ar[0] == '\0') {
          // addr is a non-register (shouldn't happen post-linearize);
          // treat as offset 0.
          fprintf(fp, "\tmov.u64 %s, %s;\n", r, bs->reg);
        } else {
          fprintf(fp, "\tmad.wide.s32 %s, %s, %u, %s;\n",
                  r, ar, isz, bs->reg);
        }
        break;
      }

      case UOP_LOAD: {
        Term idx = heap_read(loc + 0);
        const char *ir = ptx_reg_get(ctx, idx);
        if (ir[0] == '\0') return 0;
        u32 dt = ptx_dtype_of(t);
        char r[40]; ptx_ssa(ctx, "val", dt, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
        fprintf(fp, "\tld.global.%s %s, [%s+0];\n", ptx_mem_type(dt), r, ir);
        break;
      }

      case UOP_STORE: {
        Term addr  = heap_read(loc + 1);
        Term value = heap_read(loc + 2);
        const char *ir = ptx_reg_get(ctx, addr);
        const char *vr = ptx_reg_get(ctx, value);
        if (ir[0] == '\0' || vr[0] == '\0') return 0;
        u32 dt = ptx_dtype_of(value);
        fprintf(fp, "\tst.global.%s [%s+0], %s;\n", ptx_mem_type(dt), ir, vr);
        break;
      }

      case UOP_CAST: {
        Term src    = heap_read(loc + 0);
        Term dt_num = heap_read(loc + 1);
        u32 dst_dt  = (u32)term_val(dt_num);
        u32 src_dt  = ptx_dtype_of(src);
        const char *sr = ptx_reg_get(ctx, src);
        if (sr[0] == '\0') return 0;
        if (dst_dt == src_dt) { ptx_reg_put(ctx, t, sr); break; }
        char r[40]; ptx_ssa(ctx, "cast", dst_dt, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
        // Rounding modifier: float->int truncates (.rzi); narrowing or
        // int->float rounds-to-nearest (.rn).  Mirrors tinygrad modifier().
        const char *mod = "";
        if (dtype_is_int(dst_dt) && dtype_is_float(src_dt)) mod = ".rzi";
        else if (dtype_is_float(dst_dt)
                 && (dtype_is_int(src_dt)
                     || dtype_itemsize(dst_dt) < dtype_itemsize(src_dt)))
          mod = ".rn";
        fprintf(fp, "\tcvt%s.%s.%s %s, %s;\n", mod,
                ptx_reg_type(dst_dt), ptx_reg_type(src_dt), r, sr);
        break;
      }

      case UOP_ADD: case UOP_MUL:
      case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
      case UOP_IDIV: case UOP_IMOD:
      case UOP_IAND: case UOP_IOR: case UOP_IXOR:
      case UOP_NEG: case UOP_RECIP:
      case UOP_SQRT: case UOP_EXP2: case UOP_LOG2:
      case UOP_CMPLT: case UOP_CMPEQ: case UOP_ILT:
      case UOP_IWHERE: {
        u8 ar = uop_arity((u8)op);
        Term a = (ar >= 1) ? heap_read(loc + 0) : 0;
        Term b = (ar >= 2) ? heap_read(loc + 1) : 0;
        Term c = (ar >= 3) ? heap_read(loc + 2) : 0;
        const char *ra = a ? ptx_reg_get(ctx, a) : "";
        const char *rb = b ? ptx_reg_get(ctx, b) : "";
        const char *rc = c ? ptx_reg_get(ctx, c) : "";
        if ((ar >= 1 && ra[0] == '\0')
            || (ar >= 2 && rb[0] == '\0')
            || (ar >= 3 && rc[0] == '\0')) return 0;
        // Comparisons produce a pred register but use the operand type
        // for the setp suffix.  Integer-family ALU derives its type from
        // the operand (term_dtype_in mistypes index arithmetic as fp32).
        u32 res_dt, alu_dt;
        if (op == UOP_ILT) {                   // integer comparison
          res_dt = DT_BOOL;
          alu_dt = ptx_alu_dtype(UOP_IADD, a, a);
        } else if (ptx_is_cmp(op)) {           // float CMPLT / CMPEQ
          res_dt = DT_BOOL;
          alu_dt = ptx_dtype_of(a);
        } else {
          res_dt = ptx_alu_dtype(op, t, a);
          alu_dt = res_dt;
        }
        char r[40]; ptx_ssa(ctx, "alu", res_dt, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
        if (!ptx_emit_alu(op, r, ra, rb, rc, alu_dt, fp)) return 0;
        break;
      }

      case UOP_AFTER:
        // Statement-ordering marker; its register aliases src0.
        ptx_reg_put(ctx, t, ptx_reg_get(ctx, heap_read(loc + 0)));
        break;

      default:
        return 0;
    }
  }
  // Drain: close any range with no explicit UOP_END node, in reverse
  // (innermost first).  Mirrors render_linearized.c's auto-close-at-end.
  while (ctx->n_open > 0) {
    ptx_emit_loop_close(ctx, ctx->open_ranges[--ctx->n_open], fp);
  }
  return 1;
}

// === Reg-declaration + kernel prologue ================================

static void ptx_emit_reg_decls(PtxCtx const *ctx, FILE *fp) {
  for (u32 i = 0; i < ctx->n_prefixes; i++) {
    PtxPrefixCount const *pc = &ctx->prefixes[i];
    if (pc->count == 0) continue;
    fprintf(fp, "\t.reg .%s %%%s_<%u>;\n", pc->type, pc->prefix, pc->count);
  }
  // Base-pointer registers (one per buffer) are a distinct family.
  if (ctx->n_bufs > 0) {
    fprintf(fp, "\t.reg .u64 %%dat_u64_<%u>;\n", ctx->n_bufs);
  }
}

// === Entry point ======================================================
//
// Renders the LinKernel to a complete PTX module.  `sm` is the compute
// capability (70 for V100); <=0 defaults to 70.  Returns 1 on success,
// 0 if the body walk bailed (caller falls back to C-source CUDA emit).

fn int cg_render_linearized_ptx(LinKernel const *lk, const char *kernel_name,
                                int sm, FILE *fp) {
  if (lk == NULL || lk->n == 0 || fp == NULL) return 0;
  if (kernel_name == NULL) kernel_name = "uop_kernel";
  if (sm <= 0) sm = 70;

  PtxCtx ctx = (PtxCtx){ .n_bufs = 0, .n_regs = 0, .n_prefixes = 0, .sm = sm };

  // Pass 0: register buffers so the .param signature is complete + in
  // slot order (output first as data0, inputs data1..).
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_BUFFER) {
      ptx_register_buf(&ctx, t);
    }
  }

  // Emit the body to scratch first so a bail leaves no partial output
  // and so the reg-decl counts are final before we write the prologue.
  static char scratch[1 << 18];
  FILE *sf = fmemopen(scratch, sizeof(scratch) - 1, "w");
  if (sf == NULL) return 0;
  int ok = ptx_emit_body(lk, &ctx, sf);
  long sn = ftell(sf);
  fclose(sf);
  if (!ok) return 0;
  if (sn < 0) sn = 0;
  scratch[sn] = 0;

  // Prologue.
  fputs(".version 7.8\n", fp);
  fprintf(fp, ".target sm_%d\n", sm);
  fputs(".address_size 64\n", fp);
  fprintf(fp, ".visible .entry %s (\n", kernel_name);
  for (u32 i = 0; i < ctx.n_bufs; i++) {
    fprintf(fp, "\t.param .u64 %s%s\n",
            ctx.bufs[i].param, (i + 1 < ctx.n_bufs) ? "," : "");
  }
  fputs(")\n{\n", fp);
  ptx_emit_reg_decls(&ctx, fp);
  fputs(scratch, fp);
  fputs("\tret;\n", fp);
  fputs("}\n", fp);
  return 1;
}
