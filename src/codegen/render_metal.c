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

static int rmt_dtype_supported(u32 dtype) {
  return dtype == DT_FP32;
}

static int rmt_axis_shape_supported(CtKernelInfo const *info) {
  u32 n_global = 0;
  u32 n_local  = 0;
  for (u32 i = 0; i < info->n_axes; i++) {
    if (info->axis_types[i] == KAX_GLOBAL) {
      n_global++;
    } else if (info->axis_types[i] == KAX_LOCAL) {
      n_local++;
    } else {
      return 0;
    }
  }
  return n_global == 1 && n_local == 1;
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
    case S_CMPLT:
    case S_CMPEQ:
    case S_CONST:
    case S_ICONST:
    case S_IADD:
    case S_ISUB:
    case S_IMUL:
    case S_IDIV:
    case S_IMOD:
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
  if (out->scalar.has_reduce || !rmt_axis_shape_supported(out)) {
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

int cg_tile_metal_dispatch_shape(KernelEntry *ke, u32 *groups_x,
                                 u32 *threads_x) {
  if (ke == NULL) {
    return 0;
  }
  if (!tile_sync_from_scalar(ke)) {
    return 0;
  }
  CtKernelInfo info;
  if (!rmt_collect_kernel_info(ke, &info)) {
    return 0;
  }
  u32 groups  = 0;
  u32 threads = 0;
  for (u32 i = 0; i < info.n_axes; i++) {
    if (info.axis_types[i] == KAX_GLOBAL) {
      groups = info.axis_extents[i];
    } else if (info.axis_types[i] == KAX_LOCAL) {
      threads = info.axis_extents[i];
    }
  }
  if (groups == 0 || threads == 0) {
    return 0;
  }
  if (groups_x != NULL) {
    *groups_x = groups;
  }
  if (threads_x != NULL) {
    *threads_x = threads;
  }
  return 1;
}

static int rmt_emit_uint_expr(CgBuf *b, KernelEntry const *ke, u32 op_id);

static int rmt_emit_index_offset(CgBuf *b, KernelEntry const *ke, u32 idx_id) {
  ScalarUop const *u = &ke->scalar_uops[idx_id];
  if (u->op == S_INDEX_E) {
    return rmt_emit_uint_expr(b, ke, u->src[1]);
  }
  if (u->op != S_INDEX) {
    return 0;
  }
  ScalarUop const *child = &ke->scalar_uops[u->src[0]];
  int wrote = 0;
  if (child->op == S_INDEX) {
    if (!rmt_emit_index_offset(b, ke, u->src[0])) {
      return 0;
    }
    wrote = 1;
  } else if (child->op != S_DEFINE_PARAM && child->op != S_DEFINE_OUTPUT) {
    return 0;
  }

  u32 stride0 = (u32)((u->extra >>  0) & 0xFFFFu);
  u32 stride1 = (u32)((u->extra >> 16) & 0xFFFFu);
  u32 stride2 = (u32)((u->extra >> 32) & 0xFFFFu);
  u32 offset  = (u32)((u->extra >> 48) & 0xFFFFu);
  u32 nrng = (u32)u->src_count - 1;
  u32 strides[3] = {stride0, stride1, stride2};
  if (offset != 0) {
    if (wrote) {
      cg_append(b, " + ");
    }
    cg_append(b, "%uu", offset);
    wrote = 1;
  }
  for (u32 d = 0; d < nrng && d < 3; d++) {
    if (strides[d] == 0) {
      continue;
    }
    u32 rng_id = u->src[1 + d];
    if (wrote) {
      cg_append(b, " + ");
    }
    if (strides[d] == 1) {
      cg_append(b, "_v%u", rng_id);
    } else {
      cg_append(b, "%uu*_v%u", strides[d], rng_id);
    }
    wrote = 1;
  }
  if (!wrote) {
    cg_append(b, "0u");
  }
  return 1;
}

static int rmt_index_param_slot(KernelEntry const *ke, u32 idx_id,
                                u32 *slot_out) {
  ScalarUop const *u = &ke->scalar_uops[idx_id];
  if (u->op != S_INDEX && u->op != S_INDEX_E) {
    return 0;
  }
  ScalarUop const *base = &ke->scalar_uops[u->src[0]];
  while (base->op == S_INDEX || base->op == S_INDEX_E) {
    u = base;
    base = &ke->scalar_uops[u->src[0]];
  }
  if (base->op != S_DEFINE_PARAM) {
    return 0;
  }
  *slot_out = (u32)base->extra;
  return *slot_out < ke->n_inputs;
}

static int rmt_emit_uint_expr(CgBuf *b, KernelEntry const *ke, u32 op_id) {
  ScalarUop const *u = &ke->scalar_uops[op_id];
  switch (u->op) {
    case S_RANGE:
      cg_append(b, "_v%u", op_id);
      return 1;
    case S_ICONST:
      cg_append(b, "%uu", (u32)u->extra);
      return 1;
    case S_IADD:
    case S_ISUB:
    case S_IMUL:
    case S_IDIV:
    case S_IMOD: {
      const char *op = (u->op == S_IADD) ? "+"
                     : (u->op == S_ISUB) ? "-"
                     : (u->op == S_IMUL) ? "*"
                     : (u->op == S_IDIV) ? "/"
                     : "%";
      cg_append(b, "(");
      if (!rmt_emit_uint_expr(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, " %s ", op);
      if (!rmt_emit_uint_expr(b, ke, u->src[1])) {
        return 0;
      }
      cg_append(b, ")");
      return 1;
    }
    default:
      return 0;
  }
}

static int rmt_emit_value(CgBuf *b, KernelEntry const *ke, u32 op_id) {
  ScalarUop const *u = &ke->scalar_uops[op_id];
  switch (u->op) {
    case S_CONST: {
      u32 bits = (u32)u->extra;
      rm_emit_f32_bits(b, bits);
      return 1;
    }
    case S_LOAD: {
      u32 slot = 0;
      if (!rmt_index_param_slot(ke, u->src[0], &slot)) {
        return 0;
      }
      cg_append(b, "in%u[", slot);
      if (!rmt_emit_index_offset(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "]");
      return 1;
    }
    case S_ADD:
    case S_MUL:
    case S_CMPLT:
    case S_CMPEQ: {
      const char *op = (u->op == S_ADD) ? "+"
                     : (u->op == S_MUL) ? "*"
                     : (u->op == S_CMPLT) ? "<"
                     : "==";
      cg_append(b, "(");
      if (!rmt_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, " %s ", op);
      if (!rmt_emit_value(b, ke, u->src[1])) {
        return 0;
      }
      if (u->op == S_CMPLT || u->op == S_CMPEQ) {
        cg_append(b, ") ? 1.0f : 0.0f");
      } else {
        cg_append(b, ")");
      }
      return 1;
    }
    case S_NEG:
      cg_append(b, "-(");
      if (!rmt_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, ")");
      return 1;
    default:
      return 0;
  }
}

char *cg_emit_tile_metal(KernelEntry const *ke) {
  CtKernelInfo info;
  if (!rmt_collect_kernel_info(ke, &info)) {
    return NULL;
  }

  CgBuf b = { .buf = (char *)malloc(4096), .cap = 4096, .len = 0,
              .program_dtype = DT_FP32, .ke = ke };
  if (!b.buf) {
    return NULL;
  }

  cg_append(&b, "#include <metal_stdlib>\nusing namespace metal;\n\n");
  cg_append(&b, "kernel void k(device float *out [[buffer(0)]]");
  for (u32 i = 0; i < ke->n_inputs; i++) {
    cg_append(&b, ",\n              device const float *in%u [[buffer(%u)]]",
              i, i + 1);
  }
  cg_append(&b, ",\n              uint3 _tgid3 [[threadgroup_position_in_grid]],");
  cg_append(&b, "\n              uint3 _ltid3 [[thread_position_in_threadgroup]]) {\n");
  cg_append(&b, "  uint _tgid = _tgid3.x;\n");
  cg_append(&b, "  uint _ltid = _ltid3.x;\n");

  for (u32 d = 0; d < info.n_axes; d++) {
    if (info.axis_types[d] == KAX_GLOBAL) {
      cg_append(&b, "  uint _ta%u = _tgid;\n", d);
    } else if (info.axis_types[d] == KAX_LOCAL) {
      cg_append(&b, "  uint _ta%u = _ltid;\n", d);
    } else {
      free(b.buf);
      return NULL;
    }
  }

  cg_append(&b, "  bool _ok = true");
  for (u32 d = 0; d < info.n_axes; d++) {
    cg_append(&b, " && _ta%u < %uu", d, info.axis_extents[d]);
  }
  cg_append(&b, ";\n");
  cg_append(&b, "  threadgroup_barrier(mem_flags::mem_threadgroup);\n");
  cg_append(&b, "  if (!_ok) return;\n");
  cg_append(&b, "  uint _tk = 0u;\n");
  for (u32 d = 0; d < info.n_axes; d++) {
    cg_append(&b, "  _tk = _tk * %uu + _ta%u;\n", info.axis_extents[d], d);
  }

  u32 loop_strides[MAX_DIM] = {0};
  if (info.scalar.n_loops > 0) {
    loop_strides[info.scalar.n_loops - 1] = 1;
    for (i32 d = (i32)info.scalar.n_loops - 2; d >= 0; d--) {
      loop_strides[d] = loop_strides[d + 1] * info.scalar.loop_extents[d + 1];
    }
  }
  for (u32 d = 0; d < info.scalar.n_loops; d++) {
    cg_append(&b, "  uint _v%u = (_tk / %uu) %% %uu;\n",
              info.scalar.loop_ids[d], loop_strides[d],
              info.scalar.loop_extents[d]);
  }

  ScalarUop const *st = &ke->scalar_uops[info.scalar.store_id];
  cg_append(&b, "  out[");
  if (!rmt_emit_index_offset(&b, ke, st->src[0])) {
    free(b.buf);
    return NULL;
  }
  cg_append(&b, "] = ");
  if (!rmt_emit_value(&b, ke, st->src[1])) {
    free(b.buf);
    return NULL;
  }
  cg_append(&b, ";\n}\n");
  return b.buf;
}
