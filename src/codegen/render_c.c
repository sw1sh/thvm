// codegen/render_c.c - C99 renderer for cg_emit.
//
// Emits a fused inner-loop kernel that the CPU JIT (backend/cpu/jit.c)
// shells out to clang to compile into a .dylib.
//
// The kernel's signature is uniform across dtypes:
//   void k(void *out_v, const void *const *ins_v,
//          unsigned n, const unsigned *in_numels);
// The renderer casts the void* parameters to the program's dtype-
// typed pointers in the prologue (via `T * out = ...; const T *in0 =
// ...;`) so the rest of the body uses the typed names directly.
//
// Elementwise mode (no REDUCE in the program):
//
//   void k(...) {
//     T *out = (T *)out_v;
//     const T *in0 = (const T *)ins_v[0];
//     ...
//     for (unsigned i = 0; i < n; i++) {
//       T r0 = ...;
//       ...
//       out[i] = rN;
//     }
//   }
//
// Reduce-tail mode (last op is UOP_REDUCE): the inner k-loop
// accumulates into an `acc` of the kernel dtype.

// Map a wired-and-supported dtype to its C type name (matches the
// stdint.h typedefs that thvm.h pulls in).  cg_dtype_supported has
// already gated everything else; reaching the default here is a
// runtime invariant break.
static const char *rc_c_type(u32 dtype) {
  switch (dtype) {
    case DT_BOOL:   return "uint8_t";
    case DT_INT8:   return "int8_t";
    case DT_UINT8:  return "uint8_t";
    case DT_INT16:  return "int16_t";
    case DT_UINT16: return "uint16_t";
    case DT_INT32:  return "int32_t";
    case DT_UINT32: return "uint32_t";
    case DT_INT64:  return "int64_t";
    case DT_UINT64: return "uint64_t";
    case DT_FP32:   return "float";
    case DT_FP64:   return "double";
    default:        return "float";
  }
}

static int rc_is_float(u32 dtype) {
  return dtype == DT_FP32 || dtype == DT_FP64;
}

static int rc_is_int(u32 dtype) {
  switch (dtype) {
    case DT_BOOL:
    case DT_INT8:  case DT_UINT8:
    case DT_INT16: case DT_UINT16:
    case DT_INT32: case DT_UINT32:
    case DT_INT64: case DT_UINT64:
      return 1;
    default:
      return 0;
  }
}

static int rc_is_signed_int(u32 dtype) {
  return dtype == DT_INT8 || dtype == DT_INT16
      || dtype == DT_INT32 || dtype == DT_INT64;
}

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
  const char *T = rc_c_type(b->program_dtype);
  cg_append(b, "#include <math.h>\n");
  cg_append(b, "#include <stdint.h>\n");
  cg_append(b, "void k(void *out_v, const void *const *ins_v, "
               "unsigned n, const unsigned *in_numels) {\n");
  cg_append(b, "  %s *out = (%s *)out_v;\n", T, T);
  for (u32 i = 0; i < n_inputs; i++)
    cg_append(b, "  const %s *in%u = (const %s *)ins_v[%u];\n",
              T, i, T, i);
}

static void rc_epilogue(CgBuf *b, u32 n_inputs) {
  (void)n_inputs;
  cg_append(b, "  (void)in_numels;\n");
  cg_append(b, "}\n");
}

static void rc_loop_open_elementwise(CgBuf *b, u32 unroll_factor) {
  if (unroll_factor > 1) {
    cg_append(b, "  #pragma clang loop unroll_count(%u)\n", unroll_factor);
  }
  cg_append(b, "  for (unsigned i = 0; i < n; i++) {\n");
}

static void rc_loop_close_elementwise(CgBuf *b, u32 last_step) {
  cg_append(b, "    out[i] = r%u;\n", last_step);
  cg_append(b, "  }\n");
}

// REDUCE-tail accumulator type follows the program dtype.  MIN_VAL
// for MAX-init: signed ints get the appropriate INT*_MIN, unsigned
// ints get 0, floats get -INFINITY.
static void rc_loop_open_reduce(CgBuf *b, u8 kind, u32 inner, u32 axis_size,
                                u32 unroll_factor) {
  const char *T = rc_c_type(b->program_dtype);
  cg_append(b, "  unsigned _inner = %uu;\n", inner);
  cg_append(b, "  unsigned _axis  = %uu;\n", axis_size);
  cg_append(b, "  for (unsigned oi = 0; oi < n; oi++) {\n");
  cg_append(b, "    unsigned _outer   = oi / _inner;\n");
  cg_append(b, "    unsigned _inner_i = oi %% _inner;\n");
  if (kind == REDUCE_MAX) {
    if (rc_is_float(b->program_dtype)) {
      cg_append(b, "    %s acc = (%s)-INFINITY;\n", T, T);
    } else if (rc_is_signed_int(b->program_dtype)) {
      const char *MIN = "0";
      switch (b->program_dtype) {
        case DT_INT8:  MIN = "INT8_MIN";  break;
        case DT_INT16: MIN = "INT16_MIN"; break;
        case DT_INT32: MIN = "INT32_MIN"; break;
        case DT_INT64: MIN = "INT64_MIN"; break;
      }
      cg_append(b, "    %s acc = %s;\n", T, MIN);
    } else {
      cg_append(b, "    %s acc = 0;\n", T);
    }
  } else {
    cg_append(b, "    %s acc = 0;\n", T);
  }
  if (unroll_factor > 1) {
    cg_append(b, "    #pragma clang loop unroll_count(%u)\n", unroll_factor);
  }
  cg_append(b, "    for (unsigned _k = 0; _k < _axis; _k++) {\n");
  cg_append(b, "      unsigned i = _outer * (_axis * _inner) + _k * _inner + _inner_i;\n");
}

static void rc_loop_close_reduce(CgBuf *b, u32 reduce_src_raw, u8 kind,
                                 u32 const *in_numels) {
  const char *T = rc_c_type(b->program_dtype);
  if (kind == REDUCE_MAX) {
    cg_append(b, "      { %s _v = ", T);
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

// CONST literal: route through a per-dtype format.  KProgOp.arg
// stores the f32 bit pattern for floats or the integer value
// (sign-extended low 32 bits) for ints.  Same convention the
// interpreter uses (see backend/cpu/op/const.c).
static void rc_emit_const(CgBuf *b, u32 step, u32 dtype, u32 bits) {
  const char *T = rc_c_type(dtype);
  if (dtype == DT_FP32) {
    f32 v;
    memcpy(&v, &bits, sizeof v);
    cg_append(b, "    %s r%u = %.17gf;\n", T, step, v);
  } else if (dtype == DT_FP64) {
    f32 v;
    memcpy(&v, &bits, sizeof v);
    cg_append(b, "    %s r%u = %.17g;\n", T, step, (double)v);
  } else if (rc_is_signed_int(dtype)) {
    cg_append(b, "    %s r%u = (%s)(int32_t)%dl;\n",
              T, step, T, (int32_t)bits);
  } else {
    // unsigned int / bool: zero-extend the u32 bits.
    cg_append(b, "    %s r%u = (%s)%uu;\n", T, step, T, bits);
  }
}

static void rc_emit_binary(CgBuf *b, u32 step, u8 opcode,
                           u32 src_a, u32 src_b,
                           u32 const *in_numels) {
  const char *T = rc_c_type(b->program_dtype);
  cg_append(b, "    %s r%u = ", T, step);
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
      // Match the interpreter convention: result dtype = input dtype,
      // 1 / 0 typed.  The cast keeps unsigned dtypes correct.
      cg_append(b, ") ? (%s)1 : (%s)0", T, T);
      break;
    case UOP_CMPEQ:
      cg_append(b, "(");
      rc_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " == ");
      rc_emit_src_ref(b, src_b, in_numels);
      cg_append(b, ") ? (%s)1 : (%s)0", T, T);
      break;
  }
  cg_append(b, ";\n");
}

static void rc_emit_unary(CgBuf *b, u32 step, u8 opcode,
                          u32 src, u32 const *in_numels) {
  const char *T = rc_c_type(b->program_dtype);
  int is_f64 = b->program_dtype == DT_FP64;
  cg_append(b, "    %s r%u = ", T, step);
  switch (opcode) {
    case UOP_NEG:
      cg_append(b, "-(");
      rc_emit_src_ref(b, src, in_numels);
      cg_append(b, ")");
      break;
    case UOP_RECIP:
      cg_append(b, "%s / (", is_f64 ? "1.0" : "1.0f");
      rc_emit_src_ref(b, src, in_numels);
      cg_append(b, ")");
      break;
    case UOP_SQRT:
      cg_append(b, "%s(", is_f64 ? "sqrt" : "sqrtf");
      rc_emit_src_ref(b, src, in_numels);
      cg_append(b, ")");
      break;
    case UOP_EXP2:
      cg_append(b, "%s(", is_f64 ? "exp2" : "exp2f");
      rc_emit_src_ref(b, src, in_numels);
      cg_append(b, ")");
      break;
    case UOP_LOG2:
      cg_append(b, "%s(", is_f64 ? "log2" : "log2f");
      rc_emit_src_ref(b, src, in_numels);
      cg_append(b, ")");
      break;
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
