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
//                 uint i                           [[thread_position_in_grid]]) {
//     if (i >= OUT_N) return;
//     float r0 = in0[i] * in1[i];
//     ...
//     out[i] = rN;
//   }
//
// Reduce-tail mode (last op is UOP_REDUCE):
//
//     ... // same kernel signature; `i` here = output index
//     if (i >= OUT_N) return;
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
//     supplied by the dispatcher.  OUT_N is baked into generated source
//     so direct dispatchThreads and ICB dispatchThreadgroups share one
//     bounds check.
//   - sqrt / exp2 / log2 (no `f` suffix; MSL is C++-templated).
//   - Address-space qualifiers: `device` for r/w, `device const` for inputs.
//   - INFINITY: MSL provides `INFINITY` via <metal_stdlib>.

#define RM_MAX_MSL_INPUTS 30u

static void rm_emit_src_ref(CgBuf *b, u32 raw, u32 const *in_numels) {
  u32 idx = KSRC_INDEX(raw);
  if (KSRC_IS_INPUT(raw)) {
    if (in_numels[idx] == 1) cg_append(b, "in%u[0]", idx);
    else                     cg_append(b, "in%u[i]", idx);
  } else {
    cg_append(b, "r%u", idx);
  }
}

static u32 rm_output_numel(CgBuf const *b) {
  if (b != NULL && b->ke != NULL && b->ke->n_ops > 0) {
    u32 n = b->ke->program[b->ke->n_ops - 1].numel;
    return n ? n : 1;
  }
  return 1;
}

static void rm_prologue(CgBuf *b, u32 n_inputs) {
  cg_append(b, "#include <metal_stdlib>\nusing namespace metal;\n\n");
  cg_append(b, "kernel void k(device float *out [[buffer(0)]]");
  for (u32 i = 0; i < n_inputs; i++) {
    cg_append(b, ",\n              device const float *in%u [[buffer(%u)]]",
              i, i + 1);
  }
  cg_append(b, ",\n              uint i [[thread_position_in_grid]]) {\n");
  cg_append(b, "  if (i >= %uu) return;\n", rm_output_numel(b));
}

static void rm_epilogue(CgBuf *b, u32 n_inputs) {
  (void)n_inputs;
  cg_append(b, "}\n");
}

static void rm_loop_open_elementwise(CgBuf *b, u32 unroll_factor) {
  // Nothing to do: the kernel signature already binds `i` and bounds-
  // checked it; the body just emits per-op temporaries.  UPCAST
  // factor is unused on Metal -- threadgroup-level dispatch already
  // controls iteration; loop-unroll pragmas would apply only inside
  // a manual inner-loop variant (not yet emitted).
  (void)b; (void)unroll_factor;
}

static void rm_loop_close_elementwise(CgBuf *b, u32 last_step) {
  cg_append(b, "  out[i] = r%u;\n", last_step);
}

static void rm_loop_open_reduce(CgBuf *b, u8 kind, u32 inner, u32 axis_size,
                                u32 unroll_factor) {
  cg_append(b, "  uint _inner = %uu;\n", inner);
  cg_append(b, "  uint _axis  = %uu;\n", axis_size);
  cg_append(b, "  uint _oi    = i;\n");
  cg_append(b, "  uint _outer   = _oi / _inner;\n");
  cg_append(b, "  uint _inner_i = _oi %% _inner;\n");
  if (kind == REDUCE_MAX) cg_append(b, "  float acc = -INFINITY;\n");
  else                    cg_append(b, "  float acc = 0.0f;\n");
  // MSL recognises clang's loop unroll pragma; same syntax as the C
  // renderer.  Skipped at factor=1 so the no-opt source emits as a
  // plain loop.
  if (unroll_factor > 1) {
    cg_append(b, "  #pragma clang loop unroll_count(%u)\n", unroll_factor);
  }
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

static void rm_emit_f32_bits(CgBuf *b, u32 bits) {
  cg_append(b, "as_type<float>(0x%08xu)", bits);
}

static void rm_emit_const(CgBuf *b, u32 step, u32 dtype, u32 bits) {
  (void)dtype;
  cg_append(b, "  float r%u = ", step);
  rm_emit_f32_bits(b, bits);
  cg_append(b, ";\n");
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

static void rm_emit_alias(CgBuf *b, u32 step, u32 src,
                          u32 const *in_numels) {
  cg_append(b, "  float r%u = ", step);
  rm_emit_src_ref(b, src, in_numels);
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
  .emit_alias            = rm_emit_alias,
};

static u32 rm_rx_numel(KernelEntry const *ke, u32 raw) {
  if (KSRC_IS_INPUT(raw)) {
    u32 idx = KSRC_INDEX(raw);
    return idx < ke->n_inputs ? ke->input_numels[idx] : 1;
  }
  u32 idx = KSRC_INDEX(raw);
  return idx < ke->n_ops ? ke->program[idx].numel : 1;
}

static int rm_rx_op_supported(u8 op) {
  switch (op) {
    case UOP_CONST:
    case UOP_ADD:
    case UOP_MUL:
    case UOP_NEG:
    case UOP_RECIP:
    case UOP_SQRT:
    case UOP_EXP2:
    case UOP_LOG2:
    case UOP_CMPLT:
    case UOP_CMPEQ:
    case UOP_RESHAPE:
    case UOP_EXPAND:
    case UOP_PERMUTE:
    case UOP_PAD:
    case UOP_SHRINK:
    case UOP_FLIP:
      return 1;
    default:
      return 0;
  }
}

static int rm_reduce_expr_supports(KernelEntry const *ke) {
  if (ke->n_inputs > RM_MAX_MSL_INPUTS
      || ke->n_ops < 2
      || cg_program_dtype(ke) != DT_FP32) {
    return 0;
  }
  KProgOp const *last = &ke->program[ke->n_ops - 1];
  if (last->opcode != UOP_REDUCE || last->n_src != 1) {
    return 0;
  }
  u8 kind = (u8)((last->arg >> 24) & 0xFFu);
  if (kind != REDUCE_SUM && kind != REDUCE_MAX) {
    return 0;
  }
  for (u32 i = 0; i + 1 < ke->n_ops; i++) {
    if (!rm_rx_op_supported(ke->program[i].opcode)) {
      return 0;
    }
  }
  return 1;
}

static int rm_expr_supports(KernelEntry const *ke) {
  if (ke->n_inputs > RM_MAX_MSL_INPUTS
      || ke->n_ops == 0
      || cg_program_dtype(ke) != DT_FP32) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (ke->program[i].opcode == UOP_REDUCE
        || !rm_rx_op_supported(ke->program[i].opcode)) {
      return 0;
    }
  }
  return 1;
}

static int rm_expr_auto_worthwhile(KernelEntry const *ke) {
  if (!rm_expr_supports(ke)) {
    return 0;
  }
  return ke->n_ops >= 16;
}

static void rm_rx_params(CgBuf *b, u32 n_inputs) {
  for (u32 i = 0; i < n_inputs; i++) {
    if (i > 0) {
      cg_append(b, ", ");
    }
    cg_append(b, "device const float *in%u", i);
  }
  if (n_inputs > 0) {
    cg_append(b, ", ");
  }
  cg_append(b, "uint i");
}

static void rm_rx_args(CgBuf *b, u32 n_inputs, char const *idx) {
  for (u32 i = 0; i < n_inputs; i++) {
    if (i > 0) {
      cg_append(b, ", ");
    }
    cg_append(b, "in%u", i);
  }
  if (n_inputs > 0) {
    cg_append(b, ", ");
  }
  cg_append(b, "%s", idx);
}

static void rm_rx_src_at(CgBuf *b, KernelEntry const *ke, u32 raw,
                         char const *idx) {
  u32 id = KSRC_INDEX(raw);
  if (KSRC_IS_INPUT(raw)) {
    if (id >= ke->n_inputs || ke->input_numels[id] == 1) {
      cg_append(b, "in%u[0]", id);
    } else if (b->ke != NULL && b->ke->input_views != NULL
               && id < b->ke->n_inputs) {
      u32 tid = b->ke->input_tids[id];
      TenDesc const *td = (tid != 0 && tid < TENS_NEXT) ? &TENS[tid] : NULL;
      if (td != NULL && (!td->view.contiguous || td->view.offset != 0
                         || td->nviews != 0)) {
        cg_append(b, "in%u[idx_in%u(%s)]", id, id, idx);
      } else {
        cg_append(b, "in%u[%s]", id, idx);
      }
    } else {
      cg_append(b, "in%u[%s]", id, idx);
    }
    return;
  }
  cg_append(b, "v%u(", id);
  rm_rx_args(b, ke->n_inputs, idx);
  cg_append(b, ")");
}

static void rm_rx_emit_view_index(CgBuf *b, View const *v, char const *var) {
  int wrote = 0;
  if (v->offset != 0) {
    cg_append(b, "%d", (int)v->offset);
    wrote = 1;
  }
  u32 inner = 1;
  for (i32 axis = (i32)v->shape.ndim - 1; axis >= 0; axis--) {
    i32 stride = v->strides[axis];
    u32 dim = v->shape.dims[axis];
    if (stride != 0 && dim > 1) {
      if (wrote) {
        cg_append(b, " + ");
      }
      if (inner == 1) {
        cg_append(b, "%d*(%s %% %u)", (int)stride, var, dim);
      } else {
        cg_append(b, "%d*((%s / %u) %% %u)", (int)stride, var, inner, dim);
      }
      wrote = 1;
    }
    inner *= dim;
  }
  if (!wrote) {
    cg_append(b, "0");
  }
}

static void rm_rx_emit_input_helpers(CgBuf *b, KernelEntry const *ke) {
  if (ke == NULL || ke->input_views == NULL) {
    return;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid == 0 || tid >= TENS_NEXT) {
      continue;
    }
    TenDesc const *td = &TENS[tid];
    if (td->view.contiguous && td->view.offset == 0 && td->nviews == 0) {
      continue;
    }
    cg_append(b, "static inline uint idx_in%u(uint i) {\n", i);
    cg_append(b, "  uint t = (uint)(");
    rm_rx_emit_view_index(b, &ke->input_views[i], "i");
    cg_append(b, ");\n");
    for (i32 v = (i32)td->nviews - 1; v >= 0; v--) {
      cg_append(b, "  t = (uint)(");
      rm_rx_emit_view_index(b, &td->prior_views[v], "t");
      cg_append(b, ");\n");
    }
    cg_append(b, "  return t;\n");
    cg_append(b, "}\n\n");
  }
}

static void rm_rx_src_current(CgBuf *b, KernelEntry const *ke, u32 raw) {
  if (rm_rx_numel(ke, raw) == 1) {
    rm_rx_src_at(b, ke, raw, "0");
  } else {
    rm_rx_src_at(b, ke, raw, "i");
  }
}

static u32 rm_rx_stride_after(KProgOp const *p, u32 axis) {
  u32 stride = 1;
  for (u32 i = axis + 1; i < p->src0_ndim; i++) {
    stride *= p->src0_dims[i];
  }
  return stride;
}

static void rm_rx_expand(CgBuf *b, KernelEntry const *ke, KProgOp const *p) {
  u32 in_numel = rm_rx_numel(ke, p->src[0]);
  if (in_numel == 1 || in_numel == p->numel) {
    cg_append(b, "  return ");
    rm_rx_src_at(b, ke, p->src[0], in_numel == 1 ? "0" : "i");
    cg_append(b, ";\n");
    return;
  }
  if (p->out_ndim > 0 && p->src0_ndim == p->out_ndim) {
    cg_append(b, "  uint oi = i;\n");
    cg_append(b, "  uint src_idx = 0u;\n");
    cg_append(b, "  uint src_stride = 1u;\n");
    for (i32 axis = (i32)p->out_ndim - 1; axis >= 0; axis--) {
      u32 ax = (u32)axis;
      cg_append(b, "  { uint c = oi %% %uu; oi /= %uu;",
                p->out_dims[ax], p->out_dims[ax]);
      if (p->src0_dims[ax] != 1) {
        cg_append(b, " src_idx += c * src_stride;");
      }
      cg_append(b, " src_stride *= %uu; }\n", p->src0_dims[ax]);
    }
    cg_append(b, "  return ");
    rm_rx_src_at(b, ke, p->src[0], "src_idx");
    cg_append(b, ";\n");
    return;
  }
  cg_append(b, "  uint src_idx = i %% %uu;\n", in_numel ? in_numel : 1);
  cg_append(b, "  return ");
  rm_rx_src_at(b, ke, p->src[0], "src_idx");
  cg_append(b, ";\n");
}

static void rm_rx_permute(CgBuf *b, KernelEntry const *ke, KProgOp const *p) {
  if (p->src0_ndim == 0) {
    cg_append(b, "  return ");
    rm_rx_src_at(b, ke, p->src[0], "i");
    cg_append(b, ";\n");
    return;
  }
  cg_append(b, "  uint tmp = i;\n");
  cg_append(b, "  uint src_idx = 0u;\n");
  for (i32 axis = (i32)p->src0_ndim - 1; axis >= 0; axis--) {
    u32 ax = (u32)axis;
    u32 stride = rm_rx_stride_after(p, p->axis_perm[ax]);
    cg_append(b, "  { uint c = tmp %% %uu; tmp /= %uu;",
              p->out_dims[ax], p->out_dims[ax]);
    if (stride == 1) {
      cg_append(b, " src_idx += c; }\n");
    } else {
      cg_append(b, " src_idx += c * %uu; }\n", stride);
    }
  }
  cg_append(b, "  return ");
  rm_rx_src_at(b, ke, p->src[0], "src_idx");
  cg_append(b, ";\n");
}

static void rm_rx_pad(CgBuf *b, KernelEntry const *ke, KProgOp const *p) {
  if (p->src0_ndim == 0) {
    cg_append(b, "  return ");
    rm_rx_src_at(b, ke, p->src[0], "i");
    cg_append(b, ";\n");
    return;
  }
  cg_append(b, "  uint tmp = i;\n");
  cg_append(b, "  uint src_idx = 0u;\n");
  cg_append(b, "  uint src_stride = 1u;\n");
  cg_append(b, "  bool in_pad = false;\n");
  for (i32 axis = (i32)p->src0_ndim - 1; axis >= 0; axis--) {
    u32 ax = (u32)axis;
    u32 begin = p->pad_widths[2 * ax];
    cg_append(b, "  { uint c = tmp %% %uu; tmp /= %uu;",
              p->out_dims[ax], p->out_dims[ax]);
    cg_append(b, " if (c < %uu || c >= %uu) { in_pad = true; }",
              begin, begin + p->src0_dims[ax]);
    cg_append(b, " else { src_idx += (c - %uu) * src_stride;", begin);
    cg_append(b, " src_stride *= %uu; } }\n", p->src0_dims[ax]);
  }
  cg_append(b, "  return in_pad ? 0.0f : ");
  rm_rx_src_at(b, ke, p->src[0], "src_idx");
  cg_append(b, ";\n");
}

static void rm_rx_shrink(CgBuf *b, KernelEntry const *ke, KProgOp const *p) {
  if (p->src0_ndim == 0) {
    cg_append(b, "  return ");
    rm_rx_src_at(b, ke, p->src[0], "i");
    cg_append(b, ";\n");
    return;
  }
  cg_append(b, "  uint tmp = i;\n");
  cg_append(b, "  uint src_idx = 0u;\n");
  for (i32 axis = (i32)p->src0_ndim - 1; axis >= 0; axis--) {
    u32 ax = (u32)axis;
    u32 stride = rm_rx_stride_after(p, ax);
    u32 begin = p->pad_widths[2 * ax];
    cg_append(b, "  { uint c = tmp %% %uu; tmp /= %uu;",
              p->out_dims[ax], p->out_dims[ax]);
    if (stride == 1) {
      cg_append(b, " src_idx += c + %uu; }\n", begin);
    } else {
      cg_append(b, " src_idx += (c + %uu) * %uu; }\n", begin, stride);
    }
  }
  cg_append(b, "  return ");
  rm_rx_src_at(b, ke, p->src[0], "src_idx");
  cg_append(b, ";\n");
}

static void rm_rx_flip(CgBuf *b, KernelEntry const *ke, KProgOp const *p) {
  if (p->src0_ndim == 0 || p->arg == 0) {
    cg_append(b, "  return ");
    rm_rx_src_at(b, ke, p->src[0], "i");
    cg_append(b, ";\n");
    return;
  }
  cg_append(b, "  uint tmp = i;\n");
  cg_append(b, "  uint src_idx = 0u;\n");
  cg_append(b, "  uint src_stride = 1u;\n");
  for (i32 axis = (i32)p->src0_ndim - 1; axis >= 0; axis--) {
    u32 ax = (u32)axis;
    cg_append(b, "  { uint c = tmp %% %uu; tmp /= %uu;",
              p->src0_dims[ax], p->src0_dims[ax]);
    if ((p->arg & (1u << ax)) != 0) {
      cg_append(b, " c = %uu - 1u - c;", p->src0_dims[ax]);
    }
    cg_append(b, " src_idx += c * src_stride;");
    cg_append(b, " src_stride *= %uu; }\n", p->src0_dims[ax]);
  }
  cg_append(b, "  return ");
  rm_rx_src_at(b, ke, p->src[0], "src_idx");
  cg_append(b, ";\n");
}

static void rm_rx_emit_op(CgBuf *b, KernelEntry const *ke, u32 step) {
  KProgOp const *p = &ke->program[step];
  cg_append(b, "static inline float v%u(", step);
  rm_rx_params(b, ke->n_inputs);
  cg_append(b, ") {\n");
  switch (p->opcode) {
    case UOP_CONST:
      cg_append(b, "  return ");
      rm_emit_f32_bits(b, p->arg);
      cg_append(b, ";\n");
      break;
    case UOP_ADD:
      cg_append(b, "  return ");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, " + ");
      rm_rx_src_current(b, ke, p->src[1]);
      cg_append(b, ";\n");
      break;
    case UOP_MUL:
      cg_append(b, "  return ");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, " * ");
      rm_rx_src_current(b, ke, p->src[1]);
      cg_append(b, ";\n");
      break;
    case UOP_NEG:
      cg_append(b, "  return -(");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, ");\n");
      break;
    case UOP_RECIP:
      cg_append(b, "  return 1.0f / (");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, ");\n");
      break;
    case UOP_SQRT:
      cg_append(b, "  return sqrt(");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, ");\n");
      break;
    case UOP_EXP2:
      cg_append(b, "  return exp2(");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, ");\n");
      break;
    case UOP_LOG2:
      cg_append(b, "  return log2(");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, ");\n");
      break;
    case UOP_CMPLT:
      cg_append(b, "  return (");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, " < ");
      rm_rx_src_current(b, ke, p->src[1]);
      cg_append(b, ") ? 1.0f : 0.0f;\n");
      break;
    case UOP_CMPEQ:
      cg_append(b, "  return (");
      rm_rx_src_current(b, ke, p->src[0]);
      cg_append(b, " == ");
      rm_rx_src_current(b, ke, p->src[1]);
      cg_append(b, ") ? 1.0f : 0.0f;\n");
      break;
    case UOP_RESHAPE:
      cg_append(b, "  return ");
      rm_rx_src_at(b, ke, p->src[0], "i");
      cg_append(b, ";\n");
      break;
    case UOP_EXPAND:
      rm_rx_expand(b, ke, p);
      break;
    case UOP_PERMUTE:
      rm_rx_permute(b, ke, p);
      break;
    case UOP_PAD:
      rm_rx_pad(b, ke, p);
      break;
    case UOP_SHRINK:
      rm_rx_shrink(b, ke, p);
      break;
    case UOP_FLIP:
      rm_rx_flip(b, ke, p);
      break;
    default:
      cg_append(b, "  return 0.0f;\n");
      break;
  }
  cg_append(b, "}\n\n");
}

static char *rm_reduce_expr_emit(KernelEntry const *ke) {
  if (!rm_reduce_expr_supports(ke)) {
    return NULL;
  }
  CgBuf b = { .buf = (char *)malloc(4096), .cap = 4096, .len = 0,
              .program_dtype = DT_FP32, .ke = ke };
  if (!b.buf) {
    return NULL;
  }
  KProgOp const *rd = &ke->program[ke->n_ops - 1];
  u8 kind = (u8)((rd->arg >> 24) & 0xFFu);
  u32 inner = rd->arg & 0xFFFFFFu;
  if (inner == 0) {
    inner = 1;
  }
  u32 src_numel = rm_rx_numel(ke, rd->src[0]);
  u32 out_numel = rd->numel ? rd->numel : 1;
  u32 axis = out_numel ? src_numel / out_numel : 1;
  cg_append(&b, "#include <metal_stdlib>\nusing namespace metal;\n\n");
  rm_rx_emit_input_helpers(&b, ke);
  for (u32 i = 0; i + 1 < ke->n_ops; i++) {
    rm_rx_emit_op(&b, ke, i);
  }
  cg_append(&b, "kernel void k(device float *out [[buffer(0)]]");
  for (u32 i = 0; i < ke->n_inputs; i++) {
    cg_append(&b, ",\n              device const float *in%u [[buffer(%u)]]",
              i, i + 1);
  }
  cg_append(&b, ",\n              uint oi [[thread_position_in_grid]]) {\n");
  cg_append(&b, "  if (oi >= %uu) return;\n", out_numel);
  cg_append(&b, "  uint inner = %uu;\n", inner);
  cg_append(&b, "  uint axis = %uu;\n", axis ? axis : 1);
  cg_append(&b, "  uint outer = oi / inner;\n");
  cg_append(&b, "  uint inner_i = oi %% inner;\n");
  cg_append(&b, kind == REDUCE_MAX ? "  float acc = -INFINITY;\n"
                                   : "  float acc = 0.0f;\n");
  cg_append(&b, "  for (uint k = 0; k < axis; k++) {\n");
  cg_append(&b, "    uint i = outer * (axis * inner) + k * inner + inner_i;\n");
  if (kind == REDUCE_MAX) {
    cg_append(&b, "    float v = ");
    rm_rx_src_at(&b, ke, rd->src[0], "i");
    cg_append(&b, ";\n");
    cg_append(&b, "    if (v > acc) acc = v;\n");
  } else {
    cg_append(&b, "    acc += ");
    rm_rx_src_at(&b, ke, rd->src[0], "i");
    cg_append(&b, ";\n");
  }
  cg_append(&b, "  }\n");
  cg_append(&b, "  out[oi] = acc;\n");
  cg_append(&b, "}\n");
  return b.buf;
}

static char *rm_expr_emit(KernelEntry const *ke) {
  if (!rm_expr_supports(ke)) {
    return NULL;
  }
  CgBuf b = { .buf = (char *)malloc(4096), .cap = 4096, .len = 0,
              .program_dtype = DT_FP32, .ke = ke };
  if (!b.buf) {
    return NULL;
  }
  cg_append(&b, "#include <metal_stdlib>\nusing namespace metal;\n\n");
  rm_rx_emit_input_helpers(&b, ke);
  for (u32 i = 0; i < ke->n_ops; i++) {
    rm_rx_emit_op(&b, ke, i);
  }
  cg_append(&b, "kernel void k(device float *out [[buffer(0)]]");
  for (u32 i = 0; i < ke->n_inputs; i++) {
    cg_append(&b, ",\n              device const float *in%u [[buffer(%u)]]",
              i, i + 1);
  }
  u32 out_numel = ke->program[ke->n_ops - 1].numel;
  if (out_numel == 0) {
    out_numel = 1;
  }
  cg_append(&b, ",\n              uint i [[thread_position_in_grid]]) {\n");
  cg_append(&b, "  if (i >= %uu) return;\n", out_numel);
  cg_append(&b, "  out[i] = v%u(", ke->n_ops - 1);
  rm_rx_args(&b, ke->n_inputs, "i");
  cg_append(&b, ");\n");
  cg_append(&b, "}\n");
  return b.buf;
}

int cg_supports_metal_reduce_expr(KernelEntry const *ke) {
  // Multi-output kernels need an N-output Metal kernel signature;
  // the reduce-expr renderer emits a single `device float *out`.
  // Bail until step 4 wires the multi-output dispatch.
  if (cg_kernel_has_extra_outputs(ke)) return 0;
  return rm_reduce_expr_supports(ke);
}

// Bridge for tests: render an arbitrary KernelEntry to MSL and return
// the source.  Lets the WL-side test grid sanity-check that the same
// KProgOp[] emits valid Metal source -- proves the Renderer
// abstraction holds without requiring a Metal-side compile/dispatch
// path.
char *cg_emit_metal(KernelEntry const *ke) {
  if (ke->n_inputs > RM_MAX_MSL_INPUTS) {
    return NULL;
  }
  // Multi-output kernels are not yet renderable through this path
  // (single `device float *out` argument).  Bail until step 4 wires
  // the multi-output dispatch.
  if (cg_kernel_has_extra_outputs(ke)) {
    return NULL;
  }
  char *src = cg_emit(ke, &METAL_RENDERER);
  if (src != NULL) {
    return src;
  }
  char const *expr = getenv("THVM_METAL_EXPR");
  int force_expr = expr != NULL && expr[0] == '1';
  int deny_expr  = expr != NULL && expr[0] == '0';
  if (force_expr || (!deny_expr && rm_expr_auto_worthwhile(ke))) {
    src = rm_expr_emit(ke);
    if (src != NULL) {
      return src;
    }
  }
  return rm_reduce_expr_emit(ke);
}

static int rmt_dtype_supported(u32 dtype) {
  return dtype == DT_FP32;
}

typedef enum {
  RMT_AXIS_UNSUPPORTED = 0,
  RMT_AXIS_FLAT_GRID,
  RMT_AXIS_LOCAL_GLOBAL,
  RMT_AXIS_GROUP_REDUCE,
} RmtAxisMode;

static u64 rmt_axis_numel(CtKernelInfo const *info) {
  u64 n = 1;
  for (u32 i = 0; i < info->n_axes; i++) {
    n *= info->axis_extents[i];
  }
  return n;
}

static RmtAxisMode rmt_axis_mode(CtKernelInfo const *info) {
  u32 n_global = 0;
  u32 n_local  = 0;
  u32 n_group_reduce = 0;
  int flat_ok = 1;
  int nested_reduce = info->scalar.has_reduce
                   && info->tile.scalar_reduce_id != 0
                   && info->tile.reduce_tile_id == 0;
  for (u32 i = 0; i < info->tile.n_axes; i++) {
    if (info->tile.axis_types[i] == KAX_GROUP_REDUCE) {
      n_group_reduce++;
    }
  }
  for (u32 i = 0; i < info->n_axes; i++) {
    if (info->axis_types[i] == KAX_GLOBAL) {
      n_global++;
    } else if (info->axis_types[i] == KAX_LOCAL) {
      n_local++;
    }
    if (info->axis_types[i] != KAX_LOOP
        && info->axis_types[i] != KAX_UPCAST) {
      flat_ok = 0;
    }
  }
  int local_global_ok = !info->scalar.has_reduce
                     && n_global == 1
                     && n_local == 1;
  if (local_global_ok) {
    for (u32 i = 0; i < info->n_axes; i++) {
      u8 ty = info->axis_types[i];
      if (ty != KAX_GLOBAL && ty != KAX_LOCAL
          && ty != KAX_LOOP && ty != KAX_UPCAST) {
        local_global_ok = 0;
        break;
      }
    }
  }
  if (local_global_ok) {
    return RMT_AXIS_LOCAL_GLOBAL;
  }
  if (info->scalar.has_reduce && n_group_reduce == 1 && flat_ok) {
    return RMT_AXIS_GROUP_REDUCE;
  }
  // Phase 7-structural: allow FLAT_GRID with a nested scalar reduce.
  // The renderer's `rmt_emit_value_with_reduce` already handles the
  // post-reduce substitution by wrapping the reduce loop, caching the
  // accumulator in a register, and substituting the register for the
  // reduce node when emitting the surrounding scalar expression.
  // Nested reduces show up post-Phase-1 when BN-mean / BN-var inlines
  // into a consumer kernel as `MUL(REDUCE_SUM(x), CONST)` etc.
  // Default-on; THVM_TILE_NESTED_REDUCE_FLAT_GRID=0 reverts.
  int allow_nested_flat = 1;
  char const *e_nest = getenv("THVM_TILE_NESTED_REDUCE_FLAT_GRID");
  if (e_nest != NULL && e_nest[0] == '0') allow_nested_flat = 0;
  if (flat_ok && (allow_nested_flat || !nested_reduce)) {
    return RMT_AXIS_FLAT_GRID;
  }
  return RMT_AXIS_UNSUPPORTED;
}

static u32 rmt_group_reduce_extent(CtKernelInfo const *info) {
  for (u32 i = 0; i < info->tile.n_axes; i++) {
    if (info->tile.axis_types[i] == KAX_GROUP_REDUCE) {
      return info->tile.axis_extents[i];
    }
  }
  return 0;
}

static int rmt_scalar_op_supported(ScalarUop const *u) {
  switch (u->op) {
    case S_NONE:
    case S_RANGE:
    case S_DEFINE_PARAM:
    case S_DEFINE_OUTPUT:
    case S_INDEX:
    case S_INDEX_E:
    case S_LOAD:
    case S_STORE:
    case S_BUFFERIZE:
    case S_ADD:
    case S_MUL:
    case S_NEG:
    case S_RECIP:
    case S_SQRT:
    case S_EXP2:
    case S_LOG2:
    case S_CMPLT:
    case S_CMPEQ:
    case S_REDUCE_SUM:
    case S_REDUCE_MAX:
    case S_CAST:
    case S_RESHAPE_V:
    case S_CONST:
    case S_ICONST:
    case S_IADD:
    case S_ISUB:
    case S_IMUL:
    case S_IDIV:
    case S_IMOD:
    case S_ILT:
    case S_IAND:
    case S_IWHERE:
      return 1;
    default:
      return 0;
  }
}

static int rmt_collect_kernel_info(KernelEntry const *ke, CtKernelInfo *out) {
  if (ke->n_inputs > 30) {
    return 0;
  }
  if (!ct_collect_kernel_info(ke, out)) {
    return 0;
  }
  if (rmt_axis_mode(out) == RMT_AXIS_UNSUPPORTED) {
    return 0;
  }
  if (!rmt_dtype_supported(ke->output_dtype)) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (!rmt_dtype_supported(ke->input_dtypes[i])) {
      return 0;
    }
  }
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (!rmt_scalar_op_supported(u)) {
      return 0;
    }
    if (cs_op_carries_kernel_dtype(u) && !rmt_dtype_supported(u->dtype)) {
      return 0;
    }
  }
  return 1;
}

static int rmt_collect_conv2d_info(KernelEntry const *ke,
                                   TileConv2DInfo *out) {
  {
    u32 n_app = tile_anno_applied_opts_count(ke);
    KOpt const *opts = tile_anno_applied_opts(ke);
    for (u32 i = 0; i < n_app; i++) {
      u8 op = opts[i].op;
      if (op == KOP_GROUP || op == KOP_GROUPTOP) {
        return 0;
      }
    }
  }
  if (!tile_analyze_conv2d_flat(ke, out)) {
    return 0;
  }
  return out->threads > 0 && out->threads <= 256;
}

static int rmt_kprog_has_opcode(KernelEntry const *ke, u8 opcode) {
  if (ke == NULL || ke->program == NULL) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (ke->program[i].opcode == opcode) {
      return 1;
    }
  }
  return 0;
}

int cg_tile_metal_dispatch_shape(KernelEntry *ke, u32 *groups_x,
                                 u32 *threads_x) {
  if (ke == NULL) {
    return 0;
  }
  TileConv2DInfo conv;
  if (rmt_collect_conv2d_info(ke, &conv)) {
    u64 total = (u64)conv.c_out * (u64)conv.patches;
    if (total == 0 || total > 0xFFFFFFFFu) {
      return 0;
    }
    u32 outputs = conv.outputs_per_thread ? conv.outputs_per_thread : 1;
    u64 threads_total = (total + (u64)outputs - 1) / (u64)outputs;
    u32 threads = conv.threads;
    u32 groups  = (u32)((threads_total + (u64)threads - 1) / (u64)threads);
    if (groups_x != NULL) {
      *groups_x = groups;
    }
    if (threads_x != NULL) {
      *threads_x = threads;
    }
    return groups != 0;
  }
  if (tile_rejects_conv2d_flat_cin1(ke)) {
    return 0;
  }
  if (!tile_sync_from_scalar(ke)) {
    return 0;
  }
  CtKernelInfo info;
  if (!rmt_collect_kernel_info(ke, &info)) {
    return 0;
  }
  RmtAxisMode mode = rmt_axis_mode(&info);
  u32 groups  = 1;
  u32 threads = 1;
  if (mode == RMT_AXIS_LOCAL_GLOBAL) {
    u64 group_total = 1;
    threads = 0;
    for (u32 i = 0; i < info.n_axes; i++) {
      if (info.axis_types[i] == KAX_GLOBAL) {
        group_total *= info.axis_extents[i];
      } else if (info.axis_types[i] == KAX_LOCAL) {
        threads = info.axis_extents[i];
      } else {
        group_total *= info.axis_extents[i];
      }
    }
    if (group_total == 0 || group_total > 0xFFFFFFFFu) {
      return 0;
    }
    groups = (u32)group_total;
  } else if (mode == RMT_AXIS_FLAT_GRID) {
    u64 total = rmt_axis_numel(&info);
    if (total == 0 || total > 0xFFFFFFFFu) {
      return 0;
    }
    threads = total < 256 ? (u32)total : 256u;
    groups  = (u32)((total + (u64)threads - 1) / (u64)threads);
  } else if (mode == RMT_AXIS_GROUP_REDUCE) {
    u64 total = rmt_axis_numel(&info);
    u32 group = rmt_group_reduce_extent(&info);
    if (total == 0 || total > 0xFFFFFFFFu || group == 0 || group > 256) {
      return 0;
    }
    groups  = (u32)total;
    threads = group;
  }
  if (groups == 0 || threads == 0) {
    return 0;
  }
  // Phase F prep: optional parity check against the tile-IR-native
  // tile_compute_dispatch_shape walker.  Gated by env var so the
  // shadow comparison only runs when explicitly requested
  // (THVM_DISPATCH_SHAPE_PARITY=1); reports disagreements to stderr
  // without aborting.  Used to validate the renderer rewrite seam
  // before flipping the dispatch path.
  static int dispatch_parity_inited = 0;
  static int dispatch_parity_on     = 0;
  if (!dispatch_parity_inited) {
    char const *e = getenv("THVM_DISPATCH_SHAPE_PARITY");
    dispatch_parity_on    = (e != NULL && e[0] == '1');
    dispatch_parity_inited = 1;
  }
  if (dispatch_parity_on) {
    u32 tg = 0, tt = 0;
    if (tile_compute_dispatch_shape(ke, &tg, &tt)) {
      if (tg != groups || tt != threads) {
        fprintf(stderr,
                "thvm: dispatch_shape_parity mismatch -- "
                "ct=(g=%u,t=%u) tile=(g=%u,t=%u)\n",
                groups, threads, tg, tt);
      }
    }
  }
  if (groups_x != NULL) {
    *groups_x = groups;
  }
  if (threads_x != NULL) {
    *threads_x = threads;
  }
  return 1;
}


// Phase F shadow lift: try kernel_lift_to_uop alongside the existing
// render path so we can quantify lifter coverage on real workloads
// without changing dispatch.  Increments KERNEL_LIFT_ATTEMPTS for
// every call; KERNEL_LIFT_SUCCESSES when the lifter returns 1; and
// (when env-gated) KERNEL_LIFT_COMPILES / FAILS by rendering through
// cg_render_uop_kernel and shelling out to xcrun metal.  Returns
// nothing -- the existing render path is unchanged.
static void cg_shadow_lift_metal(KernelEntry const *ke) {
  KernelUopLift lift = {0};
  kernel_lift_count_attempt();
  // Metal hardware caps buffer attributes at index 30; kernels with
  // > 30 inputs can't be rendered through buffer-arg signatures.
  if (ke->n_inputs > 30) return;
  if (!kernel_lift_to_uop(ke, &lift)) return;
  kernel_lift_count_success();
  static int shadow_compile_inited = 0;
  static int shadow_compile_on     = 0;
  if (!shadow_compile_inited) {
    char const *e = getenv("THVM_RENDER_UOP_SHADOW_COMPILE");
    shadow_compile_on    = (e != NULL && e[0] == '1');
    shadow_compile_inited = 1;
  }
  if (!shadow_compile_on) return;
  char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  if (fp == NULL) return;
  cg_render_uop_kernel(lift.store_root, "shadow", lift.out_buf,
                       lift.in_bufs, lift.n_inputs, fp);
  fclose(fp);
  // Write to a temp file and shell out to xcrun metal.  Slow but
  // gives concrete signal on rendered-MSL compilability for real
  // kernels.  Caller gates via THVM_RENDER_UOP_SHADOW_COMPILE=1.
  extern int system(const char *);
  extern int unlink(const char *);
  extern int getpid(void);
  char path[64];
  snprintf(path, sizeof(path), "/tmp/thvm_shadow_%d.metal", getpid());
  FILE *out = fopen(path, "w");
  if (out == NULL) return;
  fputs(buf, out);
  fclose(out);
  char cmd[256];
  // When THVM_DUMP_LIFT_COMPILE_FAIL=1, leave the failing .metal file
  // in /tmp and dump the compiler stderr so we can see what's wrong.
  static int dump_fail_inited = 0;
  static int dump_fail_on     = 0;
  if (!dump_fail_inited) {
    char const *e = getenv("THVM_DUMP_LIFT_COMPILE_FAIL");
    dump_fail_on    = (e != NULL && e[0] == '1');
    dump_fail_inited = 1;
  }
  snprintf(cmd, sizeof(cmd),
           "xcrun metal -x metal -c %s -o /dev/null 2>/dev/null", path);
  int rc = system(cmd);
  if (WEXITSTATUS(rc) != 0 && dump_fail_on) {
    // Copy to a stable path that survives the subsequent unlink so a
    // follow-up shell can inspect the final failing rendering.
    char saved[64];
    snprintf(saved, sizeof(saved), "/tmp/thvm_shadow_last_fail.metal");
    char cp_cmd[256];
    snprintf(cp_cmd, sizeof(cp_cmd), "cp %s %s", path, saved);
    system(cp_cmd);
    fprintf(stderr, "=== compile-fail (saved as %s): ", saved);
    char err_cmd[256];
    snprintf(err_cmd, sizeof(err_cmd),
             "xcrun metal -x metal -c %s -o /dev/null 2>&1 | head -5",
             path);
    system(err_cmd);
  }
  unlink(path);
  kernel_lift_count_compile(WEXITSTATUS(rc) == 0);
}

// Render through kernel_lift_to_uop + cg_render_uop_kernel.  This is
// the primary (and only) Metal MSL emit path: the lifter handles
// every kernel shape (matmul, conv2d_flat including multi-input X
// im2col, elementwise, reduce, movement-fused subtrees).
static char *cg_emit_via_uop(KernelEntry const *ke) {
  // Metal hardware caps buffer attributes at index 30 (31 slots total
  // including output).  Reject kernels with too many inputs.
  if (ke->n_inputs > 30) return NULL;
  KernelUopLift lift = {0};
  if (!kernel_lift_to_uop(ke, &lift)) return NULL;
  // Render to a malloc'd string, matching cg_emit_tile_metal's
  // contract.  Use kernel name "k" so MTLLibrary lookup behaves like
  // the existing path.
  char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  if (fp == NULL) return NULL;
  cg_render_uop_kernel(lift.store_root, "k", lift.out_buf,
                       lift.in_bufs, lift.n_inputs, fp);
  long n = ftell(fp);
  fclose(fp);
  if (n <= 0) return NULL;
  char *out = (char *)malloc((size_t)n + 1);
  if (out == NULL) return NULL;
  memcpy(out, buf, (size_t)n);
  out[n] = '\0';
  return out;
}

char *cg_emit_tile_metal(KernelEntry const *ke) {
  // Multi-output kernels are not yet renderable through the tile
  // metal path (single `device float *out` arg + single S_STORE).
  // Bail until step 4 wires the multi-output dispatch.
  if (cg_kernel_has_extra_outputs(ke)) {
    return NULL;
  }
  cg_shadow_lift_metal(ke);
  // Render through the UOp-DAG renderer.  The lifter handles every
  // kernel shape (matmul / conv2d / elementwise / reduce / movement-
  // fused / im2col multi-input).  Returns NULL when the lifter
  // declines (n_inputs > 30, malformed shape, etc.) so the dispatch
  // ladder can route the kernel through a non-tile path if any.
  return cg_emit_via_uop(ke);
}

