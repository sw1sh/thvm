// codegen/render_metal.c - Metal Shading Language renderer for cg_emit.
//
// Stubs out the Renderer interface for MSL.  Generated function shape:
//
//   #include <metal_stdlib>
//   using namespace metal;
//
//   kernel void k(device float *out                [[buffer(0)]],
//                 device const float *in0          [[buffer(1)]],
//                 device const float *in1          [[buffer(2)]],
//                 ...
//                 constant const uint *in_numels   [[buffer(N+1)]],
//                 uint i                           [[thread_position_in_grid]],
//                 uint n                           [[threads_per_grid]]) {
//     if (i >= n) return;
//     float r0 = in0[i] * in1[i];
//     ...
//     out[i] = rN;
//   }
//
// Differences from C99 renderer (render_c.c):
//   - No `#include <math.h>`; MSL pulls intrinsics from
//     <metal_stdlib>.
//   - No outer for-loop -- the thread index `i` is supplied by the
//     dispatcher, and one thread maps to one output element.
//     The bounds check `if (i >= n) return;` covers the over-dispatch
//     when threads_per_grid is rounded up to a threadgroup multiple.
//   - Math intrinsics drop the `f` suffix (MSL is C++-templated):
//     sqrt / exp2 / log2 instead of sqrtf / exp2f / log2f.
//   - Address-space qualifiers on every pointer parameter (device,
//     constant, threadgroup).  We use `device` for r/w buffers,
//     `device const` for read-only inputs.
//
// This file ONLY emits source.  Compiling MSL into a .metallib +
// dispatching it lives in src/backend/metal/_.m, which would need a
// per-fused-program path (currently it builds one shader per UOP
// primitive).  The stub here is enough to validate the codegen
// Renderer abstraction; wiring the dispatcher comes when we
// actually move thvm's Metal backend to fused kernels.

static void rm_emit_src_ref(CgBuf *b, u32 raw, u32 const *in_numels) {
  u32 idx = KSRC_INDEX(raw);
  if (KSRC_IS_INPUT(raw)) {
    if (in_numels[idx] == 1) cg_append(b, "in%u[0]", idx);
    else                     cg_append(b, "in%u[i]", idx);
  } else {
    cg_append(b, "r%u", idx);
  }
}

static void rm_prologue(CgBuf *b, u32 n_inputs) {
  cg_append(b, "#include <metal_stdlib>\nusing namespace metal;\n\n");
  cg_append(b, "kernel void k(device float *out [[buffer(0)]]");
  for (u32 i = 0; i < n_inputs; i++) {
    cg_append(b, ",\n              device const float *in%u [[buffer(%u)]]",
              i, i + 1);
  }
  cg_append(b, ",\n              uint i [[thread_position_in_grid]],");
  cg_append(b, "\n              uint n [[threads_per_grid]]) {\n");
  cg_append(b, "  if (i >= n) return;\n");
}

static void rm_epilogue(CgBuf *b, u32 n_inputs) {
  (void)n_inputs;
  cg_append(b, "}\n");
}

static void rm_emit_const(CgBuf *b, u32 step, u32 dtype, u32 bits) {
  (void)dtype;
  f32 v;
  memcpy(&v, &bits, sizeof v);
  cg_append(b, "  float r%u = %.17gf;\n", step, v);
}

static void rm_emit_binary(CgBuf *b, u32 step, u8 opcode,
                           u32 src_a, u32 src_b,
                           u32 const *in_numels) {
  cg_append(b, "  float r%u = ", step);
  switch (opcode) {
    case UOP_ADD:
      rm_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " + ");
      rm_emit_src_ref(b, src_b, in_numels);
      break;
    case UOP_MUL:
      rm_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " * ");
      rm_emit_src_ref(b, src_b, in_numels);
      break;
    case UOP_CMPLT:
      cg_append(b, "(");
      rm_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " < ");
      rm_emit_src_ref(b, src_b, in_numels);
      cg_append(b, ") ? 1.0f : 0.0f");
      break;
    case UOP_CMPEQ:
      cg_append(b, "(");
      rm_emit_src_ref(b, src_a, in_numels);
      cg_append(b, " == ");
      rm_emit_src_ref(b, src_b, in_numels);
      cg_append(b, ") ? 1.0f : 0.0f");
      break;
  }
  cg_append(b, ";\n");
}

static void rm_emit_unary(CgBuf *b, u32 step, u8 opcode,
                          u32 src, u32 const *in_numels) {
  cg_append(b, "  float r%u = ", step);
  switch (opcode) {
    case UOP_NEG:   cg_append(b, "-(");      rm_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
    case UOP_RECIP: cg_append(b, "1.0f / ("); rm_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
    case UOP_SQRT:  cg_append(b, "sqrt(");   rm_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
    case UOP_EXP2:  cg_append(b, "exp2(");   rm_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
    case UOP_LOG2:  cg_append(b, "log2(");   rm_emit_src_ref(b, src, in_numels); cg_append(b, ")"); break;
  }
  cg_append(b, ";\n");
}

static void rm_emit_store(CgBuf *b, u32 step) {
  cg_append(b, "  out[i] = r%u;\n", step);
}

static const Renderer METAL_RENDERER = {
  .prologue    = rm_prologue,
  .epilogue    = rm_epilogue,
  .emit_const  = rm_emit_const,
  .emit_binary = rm_emit_binary,
  .emit_unary  = rm_emit_unary,
  .emit_store  = rm_emit_store,
};

// Bridge for tests: render an arbitrary KernelEntry to MSL and return
// the source.  Lets the WL-side test grid sanity-check that the same
// KProgOp[] emits valid Metal source -- proves the Renderer
// abstraction holds without requiring a Metal-side compile/dispatch
// path (which lives in backend/metal/_.m and is single-op for now).
fn char *cg_emit_metal(KernelEntry const *ke) {
  return cg_emit(ke, &METAL_RENDERER);
}
