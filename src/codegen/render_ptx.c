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

  // Stack of open loops (scaffold emitted, close pending).  Labels are
  // keyed on a monotonic occurrence id, NOT the axis id -- a malformed
  // linearized list can carry two RANGE nodes with the same axis id, and
  // axis-id labels would collide into invalid PTX.  A matching UOP_END
  // pops + emits the close; still-open loops at body end drain in reverse
  // (mirrors render_linearized.c's auto-close-at-end fallback).
  struct { Term range; u32 label; u32 last_use; } open_ranges[PTX_MAX_OPEN_RANGES];
  u32  n_open;
  u32  next_loop_label;

  // Thread geometry: promoted store-indexing KAX_LOOP axes become
  // parallel grid threads (decoded from the flat thread id) rather than
  // serial loops -- matching render_uop.c's rmu_emit_output_loops + the
  // CUDA dispatch.  `gd` holds the per-axis (stride, modulus) decode.
  RmuGlobalDecode gd;
  int  has_geom;        // 1 -> at least one promoted/LOCAL axis (geom emitted)
  int  emitted_guard;   // 1 -> a `tid >= total` guard branched to DONE
  // Register holding the source for GLOBAL-axis decode: flat thread id
  // (ctaid*ntid+tidx) when there is no LOCAL split; %ctaid.x when there
  // is (mirrors legacy `tid` vs `tg` switch).
  char gtid_reg[40];
  // Register holding %tid.x when LOCAL axes are present; LOCAL RANGEs
  // decode from here.  Empty otherwise.
  char local_reg[40];

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
  // Position-based naming: data0..data_{N-1} in REGISTRATION ORDER,
  // matching cuda_dispatch_kernel's args[0..N-1] (output then inputs in
  // resolved_tids order).  An earlier inst-based naming (data<inst>)
  // emitted the wrong param at any position whose inst differed from
  // the slot index -- e.g. a kernel using buffer insts {0,1,3,4,5,6,7}
  // (no inst=2) put data7 at position 2 and the kernel read args[2]
  // (some other buffer) into the inst=7 register.
  u32 pos = ctx->n_bufs;
  PtxBufSlot *s = &ctx->bufs[ctx->n_bufs++];
  s->buf   = buf;
  s->inst  = uop_buffer_inst_get(buf);
  s->dtype = uop_buffer_dtype(buf);
  snprintf(s->param, sizeof(s->param), "data%u", pos);
  snprintf(s->reg, sizeof(s->reg), "%%dat_u64_%u", pos);
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

// 1 if the Term's opcode produces a .pred result (a comparison).  Used
// so MUL/ADD with a comparison operand (the thvm "MUL(val, cmp) ->
// masked value" idiom) emits selp instead of mul.f32 (which would be
// invalid: PTX rejects mixed .f32 and .pred operands).
static int ptx_term_is_pred(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  return ptx_is_cmp(term_ext(t));
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

// Emit the loop-close for an open loop: increment the induction var,
// test against the extent, branch back to LOOP if still in range.
// Faithful to tinygrad ptx.py END (bottom-tested do-while).  `label` is
// the occurrence id of the matching open scaffold.
static void ptx_emit_loop_close(PtxCtx *ctx, Term range, u32 label, FILE *fp) {
  u32 extent  = uop_range_extent(range);
  const char *rr = ptx_reg_get(ctx, range);
  char p[40]; ptx_ssa(ctx, "pred", DT_BOOL, p, sizeof(p));
  fprintf(fp, "END_%u:\n", label);
  fprintf(fp, "\tadd.s32 %s, %s, 1;\n", rr, rr);
  fprintf(fp, "\tsetp.lt.s32 %s, %s, %u;\n", p, rr, extent);
  fprintf(fp, "\t@%s bra LOOP_%u;\n", p, label);
}

// Thread-geometry pre-pass.  Mirrors render_uop.c's rmu_emit_output_loops
// promotion: a plain KAX_LOOP output axis that indexes the STORE address
// becomes a parallel grid thread (decoded from the flat thread id), not a
// serial loop.  Builds the RmuGlobalDecode, emits the thread-id builtins
// + `tid >= total` bounds guard.  Returns 1 on success, 0 if the kernel
// uses a shape M3a doesn't cover yet (LOCAL axes) -- caller bails to the
// C-source emit.
static int ptx_emit_thread_geom(LinKernel const *lk, PtxCtx *ctx, FILE *fp) {
  ctx->has_geom = 0;
  ctx->emitted_guard = 0;
  memset(&ctx->gd, 0, sizeof(ctx->gd));

  // Collect output (non-REDUCE) RANGE terms in list order.  LOCAL axes
  // are kept -- rmu_compute_global_decode_ctx detects them by
  // axis_type==4 and fills local_stride/modulus, which the RANGE case
  // decodes from %tid.x.
  Term out_ranges[MAX_DIM];
  u32  out_kinds[MAX_DIM];
  u32  n_out = 0;
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) continue;
    u32 at = uop_range_axis_type(t);
    if (at == KAX_REDUCE) continue;         // reduce stays a loop
    if (n_out >= MAX_DIM) return 0;
    out_ranges[n_out] = t;
    out_kinds[n_out]  = RMU_NO_OPT;
    n_out++;
  }
  if (n_out == 0) return 1;                 // no output axes (scalar)

  // Find the real memory STORE (buf is a BUFFER, not an accumulator
  // PLACEHOLDER) -- that store's address determines which output axes
  // are promoted to threads.
  Term store = 0;
  for (u32 i = 0; i < lk->n; i++) {
    Term t = lk->uops[i];
    if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STORE) continue;
    Term b = heap_read(term_val(t) + 0);
    if (term_tag(b) == TAG_UOP && term_ext(b) == UOP_BUFFER) store = t;
  }
  if (store == 0) return 1;
  Term addr = heap_read(term_val(store) + 1);
  // F-lane store: addr is a STACK of F per-lane INDEX_E.  The promoted
  // (non-upcast) axes are shared across lanes, so collect them from lane
  // 0 -- rmu_collect_ranges does not descend into STACK itself.
  while (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_AFTER)
    addr = heap_read(term_val(addr) + 0);
  if (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_STACK
      && uop_stack_n(addr) > 0) {
    addr = uop_stack_src(addr, 0);
  }
  Term addr_ranges[MAX_DIM];
  u32  addr_n = 0;
  rmu_collect_ranges(addr, addr_ranges, &addr_n);
  u32 addr_axes[MAX_DIM];
  for (u32 i = 0; i < addr_n && i < MAX_DIM; i++) {
    addr_axes[i] = (term_tag(addr_ranges[i]) == TAG_UOP
                    && term_ext(addr_ranges[i]) == UOP_RANGE)
                 ? uop_range_axis_id(addr_ranges[i]) : 0xFFFFFFFFu;
  }

  // promote[i] = KAX_LOOP output axis that indexes the store position.
  u8 promote[MAX_DIM] = {0};
  for (u32 i = 0; i < n_out; i++) {
    u32 aid = uop_range_axis_id(out_ranges[i]);
    if (uop_range_axis_type(out_ranges[i]) != KAX_LOOP) continue;
    for (u32 j = 0; j < addr_n && j < MAX_DIM; j++) {
      if (addr_axes[j] == aid) { promote[i] = 1; break; }
    }
  }

  rmu_compute_global_decode_ctx(out_ranges, n_out, promote, NULL, &ctx->gd);
  if (ctx->gd.n_globals == 0 && !ctx->gd.has_local) return 1;  // all serial

  ctx->has_geom = 1;
  // When LOCAL axes are present, GLOBAL decodes from %ctaid.x ("tg") +
  // LOCAL from %tid.x ("tt").  Otherwise GLOBAL decodes from the flat
  // thread id (ctaid*ntid+tidx).  Mirrors render_uop.c's `idx = has_local
  // ? "tg" : "tid"` switch.
  char rtidx[40], rctaid[40], rntid[40];
  ptx_ssa(ctx, "tidx",  DT_INT32, rtidx,  sizeof(rtidx));
  ptx_ssa(ctx, "ctaid", DT_INT32, rctaid, sizeof(rctaid));
  fprintf(fp, "\tmov.u32 %s, %%tid.x;\n",   rtidx);
  fprintf(fp, "\tmov.u32 %s, %%ctaid.x;\n", rctaid);
  if (ctx->gd.has_local) {
    snprintf(ctx->gtid_reg,  sizeof(ctx->gtid_reg),  "%s", rctaid);
    snprintf(ctx->local_reg, sizeof(ctx->local_reg), "%s", rtidx);
  } else {
    ptx_ssa(ctx, "ntid", DT_INT32, rntid, sizeof(rntid));
    ptx_ssa(ctx, "gtid", DT_INT32, ctx->gtid_reg, sizeof(ctx->gtid_reg));
    fprintf(fp, "\tmov.u32 %s, %%ntid.x;\n", rntid);
    fprintf(fp, "\tmad.lo.s32 %s, %s, %s, %s;\n",
            ctx->gtid_reg, rctaid, rntid, rtidx);
    ctx->local_reg[0] = '\0';
  }
  if (ctx->gd.n_globals > 0 && ctx->gd.total > 0) {
    char gp[40]; ptx_ssa(ctx, "guard", DT_BOOL, gp, sizeof(gp));
    fprintf(fp, "\tsetp.ge.s32 %s, %s, %lld;\n",
            gp, ctx->gtid_reg, (long long)ctx->gd.total);
    fprintf(fp, "\t@%s bra DONE;\n", gp);
    ctx->emitted_guard = 1;
  }
  return 1;
}

// Resolve an operand to a VALUE register, emitting an implicit ld.global
// if the operand is a bare INDEX_E.  thvm's convention is that INDEX_E
// in value position means "load from this address" (the C-source
// renderer prints `buf[addr]`); PTX is SSA and needs an explicit load.
// Cached on a synthetic key (~t) so the same INDEX_E is loaded once.
static const char *ptx_value_resolve(PtxCtx *ctx, Term t,
                                     char *out, u32 out_sz, FILE *fp) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_INDEX_E) {
    const char *r = ptx_reg_get(ctx, t);
    snprintf(out, out_sz, "%s", r);
    return out;
  }
  Term key = (Term)(~(u64)t);
  const char *cached = ptx_reg_get(ctx, key);
  if (cached[0] != '\0') { snprintf(out, out_sz, "%s", cached); return out; }
  const char *ir = ptx_reg_get(ctx, t);
  if (ir[0] == '\0') { out[0] = '\0'; return out; }
  Term buf = heap_read(term_val(t) + 0);
  u32 dt = (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFER)
         ? uop_buffer_dtype(buf) : DT_FP32;
  char vr[40]; ptx_ssa(ctx, "val", dt, vr, sizeof(vr));
  fprintf(fp, "\tld.global.%s %s, [%s+0];\n", ptx_mem_type(dt), vr, ir);
  ptx_reg_put(ctx, key, vr);
  snprintf(out, out_sz, "%s", vr);
  return out;
}

// Does t's subtree reference a UOP_RANGE with this axis_id?  Used to
// compute a loop's liveness span (the last linearized node that uses
// its induction var).  Skips END markers -- they carry a synthetic
// range whose axis_id matches but does not constitute a real use.
static int ptx_term_refs_axis(Term t, u32 axis_id, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_END) return 0;
  if (op == UOP_RANGE) return uop_range_axis_id(t) == axis_id;
  u8 ar = uop_arity((u8)op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar; i++) {
    if (ptx_term_refs_axis(heap_read(loc + i), axis_id, depth + 1)) return 1;
  }
  if (op == UOP_STACK) {
    u32 sn = uop_stack_n(t);
    for (u32 i = 0; i < sn; i++) {
      if (ptx_term_refs_axis(uop_stack_src(t, i), axis_id, depth + 1)) return 1;
    }
  } else if (op == UOP_AFTER) {
    // Only the value arm is a real use; the ordering arm is sequencing.
    if (ptx_term_refs_axis(heap_read(loc + 0), axis_id, depth + 1)) return 1;
  }
  return 0;
}

// Last linearized index (in [open_idx+1, n)) whose node references the
// loop's axis.  The loop closes right after that node, so anything later
// (e.g. the reduce's final F-lane store, which does not use the reduce
// axis) lands OUTSIDE the loop -- the fix for "stores emitted inside the
// reduce loop".  Returns open_idx if nothing uses it (degenerate).
static u32 ptx_loop_last_use(LinKernel const *lk, u32 open_idx, u32 axis_id) {
  u32 last = open_idx;
  for (u32 j = open_idx + 1; j < lk->n; j++) {
    Term t = lk->uops[j];
    if (term_tag(t) != TAG_UOP) continue;
    if (term_ext(t) == UOP_END) continue;        // synthetic marker range
    if (ptx_term_refs_axis(t, axis_id, 0)) last = j;
  }
  return last;
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
    // PLACEHOLDER (reduce acc, M3b) + STACK/GEP (F-lane vectors, M3c)
    // are handled.  VCONST/UNROLL/CONTRACT/OPT/BUFFERIZE + a raw
    // un-lowered REDUCE/BITCAST are not -- bail so the caller falls back
    // to the C-source emit (a fully-lowered opt-rich kernel carries none
    // of these).
    if (op == UOP_VCONST || op == UOP_UNROLL || op == UOP_CONTRACT
        || op == UOP_OPT || op == UOP_BUFFERIZE || op == UOP_REDUCE
        || op == UOP_BITCAST || op == UOP_INVALID) {
      if (getenv("THVM_ROUTE_TRACE"))
        fprintf(stderr, "[ptx-bail] uncovered opcode %u\n", op);
      return 0;
    }
    // GROUP_REDUCE: cooperative shared-mem accumulator needs a `.shared`
    // declaration + `bar.sync` + `if (tt==0)` guarded final fold.  Not
    // yet covered by this renderer -- fall back to the C-source emit
    // (rmu_emit_group_reduce now emits the CUDA __shared__ form).
    if (op == UOP_RANGE && uop_range_axis_type(t) == KAX_GROUP_REDUCE) {
      if (getenv("THVM_ROUTE_TRACE"))
        fprintf(stderr, "[ptx-bail] KAX_GROUP_REDUCE axis -- falls back to C\n");
      return 0;
    }
  }

  // Thread geometry: promote store-indexing KAX_LOOP axes to parallel
  // threads (emits the flat thread id + bounds guard).  Must run before
  // the BUFFER param loads so the guard sits near the top.
  if (!ptx_emit_thread_geom(lk, ctx, fp)) return 0;

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

      case UOP_PLACEHOLDER: {
        // Reduce accumulator: a single persistent register reassigned by
        // the init / update STOREs and read by the final LOAD.  Unlike
        // the other value nodes this register is NOT single-assignment
        // (PTX registers aren't SSA at the hardware level).
        u32 dt = uop_placeholder_dtype(t);
        char r[40]; ptx_ssa(ctx, "acc", dt, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
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
        u32 axis_id = uop_range_axis_id(t);
        // Promoted parallel axis: decode a<id> from the flat thread id
        // rather than opening a serial loop.  a<id> = tid (n_globals==1),
        // or (tid / stride) % mod for a multi-axis grid.
        if (ctx->has_geom
            && rmu_gd_g_mod(&ctx->gd, axis_id) != 0) {
          u32 stride = rmu_gd_g_stride(&ctx->gd, axis_id);
          u32 mod    = rmu_gd_g_mod(&ctx->gd, axis_id);
          char r[40]; ptx_ssa(ctx, "ridx", DT_INT32, r, sizeof(r));
          ptx_reg_put(ctx, t, r);
          if (ctx->gd.n_globals == 1) {
            fprintf(fp, "\tmov.s32 %s, %s;\n", r, ctx->gtid_reg);
          } else if (stride <= 1) {
            fprintf(fp, "\trem.s32 %s, %s, %u;\n", r, ctx->gtid_reg, mod);
          } else {
            char tmp[40]; ptx_ssa(ctx, "gdiv", DT_INT32, tmp, sizeof(tmp));
            fprintf(fp, "\tdiv.s32 %s, %s, %u;\n", tmp, ctx->gtid_reg, stride);
            fprintf(fp, "\trem.s32 %s, %s, %u;\n", r, tmp, mod);
          }
          break;
        }
        // LOCAL (threadgroup-bound) axis: decode from %tid.x ("tt").
        if (ctx->has_geom
            && rmu_gd_l_mod(&ctx->gd, axis_id) != 0
            && ctx->local_reg[0] != '\0') {
          u32 lstride = rmu_gd_l_stride(&ctx->gd, axis_id);
          u32 lmod    = rmu_gd_l_mod(&ctx->gd, axis_id);
          char r[40]; ptx_ssa(ctx, "ridx", DT_INT32, r, sizeof(r));
          ptx_reg_put(ctx, t, r);
          if (ctx->gd.n_locals == 1) {
            fprintf(fp, "\tmov.s32 %s, %s;\n", r, ctx->local_reg);
          } else if (lstride <= 1) {
            fprintf(fp, "\trem.s32 %s, %s, %u;\n", r, ctx->local_reg, lmod);
          } else {
            char tmp[40]; ptx_ssa(ctx, "ldiv", DT_INT32, tmp, sizeof(tmp));
            fprintf(fp, "\tdiv.s32 %s, %s, %u;\n", tmp, ctx->local_reg, lstride);
            fprintf(fp, "\trem.s32 %s, %s, %u;\n", r, tmp, lmod);
          }
          break;
        }
        // A degenerate extent-0 RANGE is the synthetic marker the reduce
        // accumulator's END carries -- not a real loop.  Skip it (no
        // register, no loop); liveness-based closing (below) handles the
        // real reduce loop, and the END node is a no-op.
        u32 ext0 = uop_range_extent(t);
        if (ext0 == 0) break;
        // Serial loop (reduce / unpromoted axis).  Faithful to tinygrad's
        // do-while: mov ridx,-1 ; bra END ; LOOP: ... ; END incr+test+bra.
        // The loop CLOSES by liveness -- after the last node that uses its
        // axis -- NOT at the UOP_END marker (whose synthetic range matched
        // a phantom empty loop, leaving the real loop to auto-close after
        // the final store, i.e. re-storing every iteration).
        char r[40]; ptx_ssa(ctx, "ridx", DT_INT32, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
        if (ctx->n_open >= PTX_MAX_OPEN_RANGES) return 0;
        u32 label = ctx->next_loop_label++;
        fprintf(fp, "\tmov.u32 %s, 0xFFFFFFFF;\n", r);
        fprintf(fp, "\tbra END_%u;\n", label);
        fprintf(fp, "LOOP_%u:\n", label);
        ctx->open_ranges[ctx->n_open].range    = t;
        ctx->open_ranges[ctx->n_open].label    = label;
        ctx->open_ranges[ctx->n_open].last_use =
            ptx_loop_last_use(lk, i, axis_id);
        ctx->n_open++;
        break;
      }

      case UOP_END:
        // No-op: loops close by liveness (after this node, below), not by
        // the END marker.  The marker's synthetic range does not name the
        // real loop reliably.
        break;

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
        // LOAD of a PLACEHOLDER reads the accumulator register directly
        // (the final reduce result); no global load.
        if (term_tag(idx) == TAG_UOP && term_ext(idx) == UOP_PLACEHOLDER) {
          const char *ar = ptx_reg_get(ctx, idx);
          if (ar[0] == '\0') return 0;
          ptx_reg_put(ctx, t, ar);
          break;
        }
        const char *ir = ptx_reg_get(ctx, idx);
        if (ir[0] == '\0') return 0;
        u32 dt = ptx_dtype_of(t);
        char r[40]; ptx_ssa(ctx, "val", dt, r, sizeof(r));
        ptx_reg_put(ctx, t, r);
        fprintf(fp, "\tld.global.%s %s, [%s+0];\n", ptx_mem_type(dt), r, ir);
        break;
      }

      case UOP_STORE: {
        Term buf   = heap_read(loc + 0);
        Term addr  = heap_read(loc + 1);
        Term value = heap_read(loc + 2);
        // STORE into a PLACEHOLDER is an accumulator reassignment: a bit
        // copy into the persistent acc register (init = const, update =
        // acc+x).  Mirrors render_linearized.c's `_accN = value;`.
        if (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_PLACEHOLDER) {
          const char *vr = ptx_reg_get(ctx, value);
          const char *ar = ptx_reg_get(ctx, buf);
          if (vr[0] == '\0' || ar[0] == '\0') return 0;
          u32 dt = uop_placeholder_dtype(buf);
          fprintf(fp, "\tmov.b%s %s, %s;\n", ptx_reg_type(dt) + 1, ar, vr);
          break;
        }
        // Peel AFTER ordering wrappers off addr + value (the reduce
        // accumulator chain wraps the final value bundle in AFTER).
        while (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_AFTER)
          addr = heap_read(term_val(addr) + 0);
        while (term_tag(value) == TAG_UOP && term_ext(value) == UOP_AFTER)
          value = heap_read(term_val(value) + 0);
        // F-LANE PARALLEL STORE: an UPCAST'd output store has a STACK of
        // F addresses (and F values) -- emit F scalar stores, one per
        // lane.  Each addr lane is an INDEX_E (its register is the byte
        // address); each value lane has its own register.
        if (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_STACK) {
          u32 F = uop_stack_n(addr);
          int val_is_stack = (term_tag(value) == TAG_UOP
                              && term_ext(value) == UOP_STACK
                              && uop_stack_n(value) == F);
          // The buffer ptr is on STORE slot 0; some F-lane store shapes
          // carry plain integer indices in the STACK (not INDEX_E), in
          // which case we compute the byte address per lane via
          // mad.wide.s32 + the buffer's base pointer.
          PtxBufSlot *bs = NULL;
          if (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFER) {
            bs = ptx_find_buf(ctx, buf);
            if (bs == NULL) bs = ptx_register_buf(ctx, buf);
          }
          for (u32 i = 0; i < F; i++) {
            Term addr_i = uop_stack_src(addr, i);
            Term val_i  = val_is_stack ? uop_stack_src(value, i) : value;
            const char *ir;
            char ibuf[40];
            if (term_tag(addr_i) == TAG_UOP && term_ext(addr_i) == UOP_INDEX_E) {
              ir = ptx_reg_get(ctx, addr_i);
            } else {
              // Plain integer index lane: compute the byte address.
              const char *idx_reg = ptx_reg_get(ctx, addr_i);
              if (idx_reg[0] == '\0' || bs == NULL) return 0;
              u32 isz = dtype_itemsize(bs->dtype);
              ptx_ssa(ctx, "index", DT_INT64, ibuf, sizeof(ibuf));
              fprintf(fp, "\tmad.wide.s32 %s, %s, %u, %s;\n",
                      ibuf, idx_reg, isz, bs->reg);
              ir = ibuf;
            }
            char vbuf[40];
            const char *vr = ptx_value_resolve(ctx, val_i, vbuf, sizeof(vbuf), fp);
            if (ir[0] == '\0' || vr[0] == '\0') return 0;
            u32 dt = ptx_dtype_of(val_i);
            fprintf(fp, "\tst.global.%s [%s+0], %s;\n",
                    ptx_mem_type(dt), ir, vr);
          }
          break;
        }
        // Scalar memory store.
        const char *ir = ptx_reg_get(ctx, addr);
        char vbuf[40];
        const char *vr = ptx_value_resolve(ctx, value, vbuf, sizeof(vbuf), fp);
        if (ir[0] == '\0' || vr[0] == '\0') return 0;
        u32 dt = ptx_dtype_of(value);
        fprintf(fp, "\tst.global.%s [%s+0], %s;\n", ptx_mem_type(dt), ir, vr);
        break;
      }

      case UOP_STACK:
        // A STACK is a lane bundle; its lanes are separate nodes that
        // already have registers.  No instruction -- consumers (STORE
        // F-lane, GEP) read the lanes directly.
        break;

      case UOP_GEP: {
        // GEP(STACK, i) extracts lane i: alias that lane's register.
        Term src = heap_read(loc + 0);
        u32 ni = uop_gep_n_idx(t);
        if (ni < 1) return 0;
        u32 idx = uop_gep_idx(t, 0);
        if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_STACK
            && idx < uop_stack_n(src)) {
          ptx_reg_put(ctx, t, ptx_reg_get(ctx, uop_stack_src(src, idx)));
          break;
        }
        return 0;   // GEP over a non-STACK (UNROLL vector) -- M3 follow-up
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
        // Implicit-load INDEX_E in value position (thvm convention).
        char ra_buf[40] = {0}, rb_buf[40] = {0}, rc_buf[40] = {0};
        const char *ra = a ? ptx_value_resolve(ctx, a, ra_buf, sizeof(ra_buf), fp) : "";
        const char *rb = b ? ptx_value_resolve(ctx, b, rb_buf, sizeof(rb_buf), fp) : "";
        const char *rc = c ? ptx_value_resolve(ctx, c, rc_buf, sizeof(rc_buf), fp) : "";
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
        // MUL/ADD-with-comparison-operand: the IR pattern MUL(val, cmp)
        // means "val if cmp else 0" (masked value).  PTX rejects mixing
        // .f32 with .pred in mul/add; emit selp instead.
        if ((op == UOP_MUL || op == UOP_ADD)
            && dtype_is_float(res_dt)
            && ar >= 2 && (ptx_term_is_pred(a) || ptx_term_is_pred(b))) {
          const char *val_reg = ptx_term_is_pred(a) ? rb : ra;
          const char *pred_reg = ptx_term_is_pred(a) ? ra : rb;
          char zero[24]; ptx_render_val(res_dt, 0, zero, sizeof(zero));
          // selp.f32 r, val, 0.0, pred  ->  pred ? val : 0
          fprintf(fp, "\tselp.%s %s, %s, %s, %s;\n",
                  ptx_reg_type(res_dt), r, val_reg, zero, pred_reg);
          break;
        }
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

    // Liveness close: after emitting node i, close any open loop whose
    // last referencing node was i (innermost first).  This places the
    // loop-close right after the loop body, so later nodes (the reduce's
    // final F-lane store) fall OUTSIDE the loop.
    while (ctx->n_open > 0
           && ctx->open_ranges[ctx->n_open - 1].last_use <= i) {
      ctx->n_open--;
      ptx_emit_loop_close(ctx, ctx->open_ranges[ctx->n_open].range,
                          ctx->open_ranges[ctx->n_open].label, fp);
    }
  }
  // Drain: close any loop still open (defensive -- liveness should have
  // closed them all), innermost first.
  while (ctx->n_open > 0) {
    ctx->n_open--;
    ptx_emit_loop_close(ctx, ctx->open_ranges[ctx->n_open].range,
                        ctx->open_ranges[ctx->n_open].label, fp);
  }
  // The bounds guard branches here; the caller's `ret;` follows.
  if (ctx->emitted_guard) fputs("DONE:\n", fp);
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
