// codegen/render_c.c - C99 renderer for cg_emit.
//
// Emits a fused inner-loop kernel that the CPU JIT (backend/cpu/jit.c)
// shells out to clang to compile into a .dylib.
//
// Elementwise mode (no REDUCE in the program):
//
//   #include <math.h>
//   void k(float *out, const float *const *ins,
//          unsigned n, const unsigned *in_numels) {
//     const float *in0 = ins[0];
//     ...
//     for (unsigned i = 0; i < n; i++) {
//       float r0 = ...;
//       ...
//       out[i] = rN;
//     }
//     (void)in_numels;
//   }
//
// Reduce-tail mode (last op is UOP_REDUCE):
//
//   ...
//   for (unsigned oi = 0; oi < n; oi++) {
//     unsigned _outer = oi / _inner;
//     unsigned _inner_i = oi % _inner;
//     float acc = 0.0f;     // or -INFINITY for MAX
//     for (unsigned _k = 0; _k < _axis; _k++) {
//       unsigned i = _outer * (_axis * _inner) + _k * _inner + _inner_i;
//       float r0 = ...;     // chain ops re-emitted inside k loop
//       ...
//       acc += r{N-1};      // (or `if (rX > acc) acc = rX;` for MAX)
//     }
//     out[oi] = acc;
//   }
//
// Variable shadowing: inside the inner k-loop, `unsigned i = ...;` shadows
// the outer `oi` so per-op input refs (`in0[i]`) keep working with no
// renderer-side changes.
//
// `ins` is an array of float-pointers; numels are passed alongside for
// future use (today the broadcast decision -- "input numel == 1?" -- is
// baked into the source at emit time).

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
}

static void rc_epilogue(CgBuf *b, u32 n_inputs) {
  (void)n_inputs;
  cg_append(b, "  (void)in_numels;\n");
  cg_append(b, "}\n");
}

static void rc_loop_open_elementwise(CgBuf *b) {
  cg_append(b, "  for (unsigned i = 0; i < n; i++) {\n");
}

static void rc_loop_close_elementwise(CgBuf *b, u32 last_step) {
  cg_append(b, "    out[i] = r%u;\n", last_step);
  cg_append(b, "  }\n");
}

static void rc_loop_open_reduce(CgBuf *b, u8 kind, u32 inner, u32 axis_size,
                                u32 unroll_factor) {
  cg_append(b, "  unsigned _inner = %uu;\n", inner);
  cg_append(b, "  unsigned _axis  = %uu;\n", axis_size);
  cg_append(b, "  for (unsigned oi = 0; oi < n; oi++) {\n");
  cg_append(b, "    unsigned _outer   = oi / _inner;\n");
  cg_append(b, "    unsigned _inner_i = oi %% _inner;\n");
  if (kind == REDUCE_MAX) cg_append(b, "    float acc = -INFINITY;\n");
  else                    cg_append(b, "    float acc = 0.0f;\n");
  // TOpt["UNROLL", reduce_axis, factor] -> emit a clang loop hint so
  // the JIT pre-unrolls the inner k-loop.  factor=1 (default) skips
  // the pragma; the back-compat path matches the pre-Phase-16 emit
  // byte-for-byte so the kernel-program-cache key stays stable for
  // unopt'd kernels (393/393 must keep passing).
  if (unroll_factor > 1) {
    cg_append(b, "    #pragma clang loop unroll_count(%u)\n", unroll_factor);
  }
  cg_append(b, "    for (unsigned _k = 0; _k < _axis; _k++) {\n");
  cg_append(b, "      unsigned i = _outer * (_axis * _inner) + _k * _inner + _inner_i;\n");
}

static void rc_loop_close_reduce(CgBuf *b, u32 reduce_src_raw, u8 kind,
                                 u32 const *in_numels) {
  if (kind == REDUCE_MAX) {
    cg_append(b, "      { float _v = ");
    rc_emit_src_ref(b, reduce_src_raw, in_numels);
    cg_append(b, "; if (_v > acc) acc = _v; }\n");
  } else {
    cg_append(b, "      acc += ");
    rc_emit_src_ref(b, reduce_src_raw, in_numels);
    cg_append(b, ";\n");
  }
  cg_append(b, "    }\n");
  cg_append(b, "    out[oi] = acc;\n");
  cg_append(b, "  }\n");
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

static const Renderer C_RENDERER = {
  .prologue              = rc_prologue,
  .epilogue              = rc_epilogue,
  .loop_open_elementwise  = rc_loop_open_elementwise,
  .loop_close_elementwise = rc_loop_close_elementwise,
  .loop_open_reduce       = rc_loop_open_reduce,
  .loop_close_reduce      = rc_loop_close_reduce,
  .emit_const            = rc_emit_const,
  .emit_binary           = rc_emit_binary,
  .emit_unary            = rc_emit_unary,
};
