// codegen/render_c.c - C99 renderer for cg_emit.
//
// Emits a single fused inner loop that the CPU JIT (backend/cpu/jit.c)
// shells out to clang to compile into a .dylib.  Function shape:
//
//   #include <math.h>
//   void k(float *out, const float *const *ins,
//          unsigned n, const unsigned *in_numels) {
//     const float *in0 = ins[0];
//     ...
//     for (unsigned i = 0; i < n; i++) {
//       float r0 = ...;
//       float r1 = ...;
//       ...
//       out[i] = rN;
//     }
//     (void)in_numels;
//   }
//
// `ins` is an array of float-pointers; numels are passed alongside for
// future use (today the broadcast decision -- "input numel == 1?" --
// is baked into the source at emit time).

static void rc_emit_src_ref(CgBuf *b, u32 raw, u32 const *in_numels) {
  u32 idx = KSRC_INDEX(raw);
  if (KSRC_IS_INPUT(raw)) {
    if (in_numels[idx] == 1) cg_append(b, "in%u[0]", idx);
    else                     cg_append(b, "in%u[i]", idx);
  } else {
    cg_append(b, "r%u", idx);
  }
}

static void rc_prologue(CgBuf *b, u32 n_inputs) {
  cg_append(b, "#include <math.h>\n");
  cg_append(b, "void k(float *out, const float *const *ins, "
               "unsigned n, const unsigned *in_numels) {\n");
  for (u32 i = 0; i < n_inputs; i++)
    cg_append(b, "  const float *in%u = ins[%u];\n", i, i);
  cg_append(b, "  for (unsigned i = 0; i < n; i++) {\n");
}

static void rc_epilogue(CgBuf *b, u32 n_inputs) {
  (void)n_inputs;
  cg_append(b, "  }\n");
  cg_append(b, "  (void)in_numels;\n");
  cg_append(b, "}\n");
}

static void rc_emit_const(CgBuf *b, u32 step, u32 dtype, u32 bits) {
  (void)dtype;
  f32 v;
  memcpy(&v, &bits, sizeof v);
  cg_append(b, "    float r%u = %.17gf;\n", step, v);
}

static void rc_emit_binary(CgBuf *b, u32 step, u8 opcode,
                           u32 src_a, u32 src_b,
                           u32 const *in_numels) {
  cg_append(b, "    float r%u = ", step);
  switch (opcode) {
    case UOP_ADD:
      rc_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " + ");
      rc_emit_src_ref(b, src_b, in_numels);
      break;
    case UOP_MUL:
      rc_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " * ");
      rc_emit_src_ref(b, src_b, in_numels);
      break;
    case UOP_CMPLT:
      cg_append(b, "(");
      rc_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " < ");
      rc_emit_src_ref(b, src_b, in_numels);
      cg_append(b, ") ? 1.0f : 0.0f");
      break;
    case UOP_CMPEQ:
      cg_append(b, "(");
      rc_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " == ");
      rc_emit_src_ref(b, src_b, in_numels);
      cg_append(b, ") ? 1.0f : 0.0f");
      break;
  }
  cg_append(b, ";\n");
}

static void rc_emit_unary(CgBuf *b, u32 step, u8 opcode,
                          u32 src, u32 const *in_numels) {
  cg_append(b, "    float r%u = ", step);
  switch (opcode) {
    case UOP_NEG:   cg_append(b, "-(");      rc_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
    case UOP_RECIP: cg_append(b, "1.0f / ("); rc_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
    case UOP_SQRT:  cg_append(b, "sqrtf(");  rc_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
    case UOP_EXP2:  cg_append(b, "exp2f(");  rc_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
    case UOP_LOG2:  cg_append(b, "log2f(");  rc_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
  }
  cg_append(b, ";\n");
}

static void rc_emit_store(CgBuf *b, u32 step) {
  cg_append(b, "    out[i] = r%u;\n", step);
}

static const Renderer C_RENDERER = {
  .prologue    = rc_prologue,
  .epilogue    = rc_epilogue,
  .emit_const  = rc_emit_const,
  .emit_binary = rc_emit_binary,
  .emit_unary  = rc_emit_unary,
  .emit_store  = rc_emit_store,
};
