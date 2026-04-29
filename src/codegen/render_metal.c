// codegen/render_metal.c - Metal Shading Language renderer for cg_emit.
//
// Mirrors render_c.c structurally but emits MSL.  Two emission modes:
//
// Elementwise mode (no REDUCE in the program):
//
//   #include <metal_stdlib>
//   using namespace metal;
//
//   kernel void k(device float *out                [[buffer(0)]],
//                 device const float *in0          [[buffer(1)]],
//                 ...
//                 uint i                           [[thread_position_in_grid]],
//                 uint n                           [[threads_per_grid]]) {
//     if (i >= n) return;
//     float r0 = in0[i] * in1[i];
//     ...
//     out[i] = rN;
//   }
//
// Reduce-tail mode (last op is UOP_REDUCE):
//
//     ... // same kernel signature; `i` here = output index
//     if (i >= n) return;
//     uint _outer = i / _inner;
//     uint _inner_i = i % _inner;
//     float acc = 0.0f;     // or -INFINITY for MAX
//     for (uint _k = 0; _k < _axis; _k++) {
//       uint i = _outer * (_axis * _inner) + _k * _inner + _inner_i;  // shadow
//       float r0 = ...;
//       ...
//       acc += r{N-1};
//     }
//     out[i_outer_was_thread_idx] = acc;   // emitted as `out[/* threadId */]`
//
// The MSL kernel parameter `uint i [[thread_position_in_grid]]` doubles
// as the output index in reduce mode; we save it into `_oi` before
// shadowing `i` inside the inner loop, then store back via `out[_oi]`.
//
// Differences from C99 renderer (render_c.c):
//   - <metal_stdlib> for intrinsics; no <math.h>.
//   - No outer for-loop in elementwise mode -- the thread index `i` is
//     supplied by the dispatcher, one thread per output element.
//   - sqrt / exp2 / log2 (no `f` suffix; MSL is C++-templated).
//   - Address-space qualifiers: `device` for r/w, `device const` for inputs.
//   - INFINITY: MSL provides `INFINITY` via <metal_stdlib>.

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

static void rm_loop_open_elementwise(CgBuf *b) {
  // Nothing to do: the kernel signature already binds `i` and bounds-
  // checked it; the body just emits per-op temporaries.
  (void)b;
}

static void rm_loop_close_elementwise(CgBuf *b, u32 last_step) {
  cg_append(b, "  out[i] = r%u;\n", last_step);
}

static void rm_loop_open_reduce(CgBuf *b, u8 kind, u32 inner, u32 axis_size) {
  cg_append(b, "  uint _inner = %uu;\n", inner);
  cg_append(b, "  uint _axis  = %uu;\n", axis_size);
  cg_append(b, "  uint _oi    = i;\n");
  cg_append(b, "  uint _outer   = _oi / _inner;\n");
  cg_append(b, "  uint _inner_i = _oi %% _inner;\n");
  if (kind == REDUCE_MAX) cg_append(b, "  float acc = -INFINITY;\n");
  else                    cg_append(b, "  float acc = 0.0f;\n");
  cg_append(b, "  for (uint _k = 0; _k < _axis; _k++) {\n");
  cg_append(b, "    uint i = _outer * (_axis * _inner) + _k * _inner + _inner_i;\n");
}

static void rm_loop_close_reduce(CgBuf *b, u32 reduce_src_raw, u8 kind,
                                 u32 const *in_numels) {
  if (kind == REDUCE_MAX) {
    cg_append(b, "    { float _v = ");
    rm_emit_src_ref(b, reduce_src_raw, in_numels);
    cg_append(b, "; if (_v > acc) acc = _v; }\n");
  } else {
    cg_append(b, "    acc += ");
    rm_emit_src_ref(b, reduce_src_raw, in_numels);
    cg_append(b, ";\n");
  }
  cg_append(b, "  }\n");
  cg_append(b, "  out[_oi] = acc;\n");
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

static const Renderer METAL_RENDERER = {
  .prologue              = rm_prologue,
  .epilogue              = rm_epilogue,
  .loop_open_elementwise  = rm_loop_open_elementwise,
  .loop_close_elementwise = rm_loop_close_elementwise,
  .loop_open_reduce       = rm_loop_open_reduce,
  .loop_close_reduce      = rm_loop_close_reduce,
  .emit_const            = rm_emit_const,
  .emit_binary           = rm_emit_binary,
  .emit_unary            = rm_emit_unary,
};

// Bridge for tests: render an arbitrary KernelEntry to MSL and return
// the source.  Lets the WL-side test grid sanity-check that the same
// KProgOp[] emits valid Metal source -- proves the Renderer
// abstraction holds without requiring a Metal-side compile/dispatch
// path.
char *cg_emit_metal(KernelEntry const *ke) {
  return cg_emit(ke, &METAL_RENDERER);
}
