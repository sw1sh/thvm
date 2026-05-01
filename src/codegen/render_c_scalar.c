// codegen/render_c_scalar.c -- C99 renderer for the rangeify
// scalar-UOp graph (ScalarUop[]).  The KProgOp[] pipeline (cg_emit
// + render_c.c) covers the legacy tensor-op kernels; this file
// covers the post-rangeify scalar form so kernels lowered through
// rangeify can hit the same JIT path the legacy KProgOp[] path uses.
//
// Scope (MVP):
//   - Elementwise: S_BUFFERIZE / S_STORE / S_INDEX / S_LOAD /
//     S_LOAD_RAW (pass-through) / S_CONST / S_ADD / S_MUL / S_NEG /
//     S_RECIP / S_SQRT / S_EXP2 / S_LOG2 / S_CMPLT / S_CMPEQ.
//   - Reductions: S_REDUCE_SUM / S_REDUCE_MAX over one REDUCE range.
//   - One LOOP nest at the BUFFERIZE root (rank 1..MAX_DIM).
//   - f32 / f64 only.
//   - Expression indexes: S_INDEX_E plus S_ICONST / S_IADD /
//     S_ISUB / S_IMUL / S_IDIV / S_IMOD / S_ILT / S_IAND /
//     S_IWHERE.
//   - f32 <-> f64 value-preserving S_CAST.
//   - Movement wrappers: S_SHRINK / S_PAD / S_FLIP / S_RESHAPE /
//     S_RESHAPE_V.
//   - No BITCAST yet.
//
// Out of scope (bail; falls through to cpu_dispatch_scalar):
//   - Narrow FPs / packed nibbles
//
// Generated function signature matches render_c.c:
//   void k(void *out_v, const void *const *ins_v, unsigned n,
//          const unsigned *in_numels);

static const char *cs_dtype_to_c(u32 dtype) {
  switch (dtype) {
    case DT_FP32: return "float";
    case DT_FP64: return "double";
    default:      return "float";
  }
}

static int cs_dtype_supported(u32 dtype) {
  return dtype == DT_FP32 || dtype == DT_FP64;
}

// MVP supported-op predicate.  Must mirror the emit switch's
// coverage so cs_supports never green-lights an op the emit doesn't
// know how to render.
static int cs_op_supported(u8 op) {
  switch (op) {
    case S_RANGE:
    case S_DEFINE_PARAM:
    case S_DEFINE_OUTPUT:
    case S_INDEX:
    case S_INDEX_E:
    case S_LOAD:
    case S_LOAD_RAW:
    case S_STORE:
    case S_BUFFERIZE:
    case S_CONST:
    case S_ADD: case S_MUL: case S_NEG:
    case S_RECIP: case S_SQRT: case S_EXP2: case S_LOG2:
    case S_CMPLT: case S_CMPEQ:
    case S_REDUCE_SUM: case S_REDUCE_MAX:
    case S_CAST:
    case S_SHRINK: case S_PAD: case S_FLIP:
    case S_RESHAPE: case S_RESHAPE_V:
    case S_ICONST:
    case S_IADD: case S_ISUB: case S_IMUL:
    case S_IDIV: case S_IMOD: case S_ILT:
    case S_IAND: case S_IWHERE:
      return 1;
    default:
      return 0;
  }
}

static int cs_op_carries_kernel_dtype(ScalarUop const *u) {
  switch (u->op) {
    case S_LOAD:
    case S_LOAD_RAW:
    case S_STORE:
    case S_CONST:
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
    case S_SHRINK:
    case S_PAD:
    case S_FLIP:
    case S_RESHAPE:
    case S_RESHAPE_V:
      return 1;
    case S_IWHERE:
      return u->dtype != DT_INT64;
    default:
      return 0;
  }
}

// Find the BUFFERIZE root + STORE child, gather LOOP ranges, and
// validate the dtype invariants.  Returns 1 on success, 0 to bail.
typedef struct {
  ScalarUop *bufferize;
  u32        store_id;
  u32        n_loops;
  u32        loop_ids    [MAX_DIM];
  u32        loop_extents[MAX_DIM];
  u32        output_dtype;
  int        has_reduce;
} CsKernelInfo;

static int cs_collect_kernel_info(KernelEntry const *ke, CsKernelInfo *out) {
  if (ke->scalar_uops == NULL || ke->n_scalar_uops < 2) return 0;
  if (!cs_dtype_supported(ke->output_dtype)) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (!cs_dtype_supported(ke->input_dtypes[i])) {
      return 0;
    }
  }

  out->has_reduce = 0;

  // Walk every scalar op; bail on unsupported dtypes.
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (!cs_op_supported(u->op)) return 0;
    if (cs_op_carries_kernel_dtype(u)) {
      if (!cs_dtype_supported(u->dtype)) {
        return 0;
      }
    }
    if (u->op == S_REDUCE_SUM || u->op == S_REDUCE_MAX) {
      out->has_reduce = 1;
    }
  }
  out->output_dtype = ke->output_dtype;

  // Find BUFFERIZE.
  u32 buf_id = 0;
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_BUFFERIZE) { buf_id = i; break; }
  }
  if (buf_id == 0) return 0;
  out->bufferize = &ke->scalar_uops[buf_id];
  out->store_id  = out->bufferize->src[0];
  if (ke->scalar_uops[out->store_id].op != S_STORE) return 0;

  out->n_loops = (u32)out->bufferize->src_count - 1;
  if (out->n_loops > MAX_DIM) return 0;
  for (u32 d = 0; d < out->n_loops; d++) {
    u32 r = out->bufferize->src[1 + d];
    if (ke->scalar_uops[r].op != S_RANGE) return 0;
    // LOOP-typed only (axis_type == S_AXIS_LOOP, in extra high 32).
    u32 axis_type = (u32)((ke->scalar_uops[r].extra >> 32) & 0xFFFFFFFFu);
    if (axis_type != S_AXIS_LOOP) return 0;
    out->loop_ids    [d] = r;
    out->loop_extents[d] = (u32)(ke->scalar_uops[r].extra & 0xFFFFFFFFu);
  }
  return 1;
}

// Predicate: can this kernel be JIT-rendered via cg_emit_scalar?
fn int cg_supports_scalar(KernelEntry const *ke) {
  CsKernelInfo info;
  return cs_collect_kernel_info(ke, &info);
}

// Recursively emit a C expression for the value produced by `op_id`.
// Each scalar op except S_BUFFERIZE / S_STORE produces a value when
// the outer LOOP iter context (bound to range_iter[loop_ids[d]] = the
// per-axis loop variable) is fixed.  The emitter inlines the
// computation -- no temporaries -- which lets clang fuse the chain.
static int cs_emit_value(CgBuf *b, KernelEntry const *ke, u32 op_id);

// Emit an INDEX expression evaluated to its flat-buffer offset.
// S_INDEX.src[0] is the buffer (DEFINE_PARAM/DEFINE_OUTPUT); src[1..]
// are the LOOP RANGE refs, and extra packs (stride0, stride1, stride2,
// offset) as 4 x u16.  Only up to 3 strided axes per node; longer
// chains nest INDEX nodes (each later INDEX takes the previous as its
// buffer src and adds 0 offset).
static int cs_emit_index_offset(CgBuf *b, KernelEntry const *ke, u32 idx_id) {
  ScalarUop const *u = &ke->scalar_uops[idx_id];
  if (u->op == S_INDEX_E) {
    return cs_emit_value(b, ke, u->src[1]);
  }
  if (u->op != S_INDEX) {
    return 0;
  }
  // Recurse into src[0] if it's another S_INDEX (chain); else it's a
  // DEFINE_PARAM/DEFINE_OUTPUT terminal (no offset contribution).
  ScalarUop const *child = &ke->scalar_uops[u->src[0]];
  int wrote = 0;
  if (child->op == S_INDEX) {
    if (!cs_emit_index_offset(b, ke, u->src[0])) {
      return 0;
    }
    wrote = 1;
  } else if (child->op != S_DEFINE_PARAM && child->op != S_DEFINE_OUTPUT) {
    return 0;
  }
  // Extra: bits[0..15]=stride0, [16..31]=stride1, [32..47]=stride2,
  //        [48..63]=offset.
  u32 stride0 = (u32)((u->extra >>  0) & 0xFFFFu);
  u32 stride1 = (u32)((u->extra >> 16) & 0xFFFFu);
  u32 stride2 = (u32)((u->extra >> 32) & 0xFFFFu);
  u32 offset  = (u32)((u->extra >> 48) & 0xFFFFu);
  u32 nrng = (u32)u->src_count - 1;  // RANGE refs after the buffer
  u32 strides[3] = {stride0, stride1, stride2};
  if (offset != 0) {
    if (wrote) cg_append(b, " + ");
    cg_append(b, "%uu", offset);
    wrote = 1;
  }
  for (u32 d = 0; d < nrng && d < 3; d++) {
    if (strides[d] == 0) continue;
    u32 rng_id = u->src[1 + d];
    if (wrote) cg_append(b, " + ");
    if (strides[d] == 1) cg_append(b, "_v%u", rng_id);
    else                 cg_append(b, "%uu*_v%u", strides[d], rng_id);
    wrote = 1;
  }
  if (!wrote) cg_append(b, "0u");
  return 1;
}

static int cs_index_input_slot(KernelEntry const *ke, u32 idx_id, u32 *slot) {
  ScalarUop const *idx = &ke->scalar_uops[idx_id];
  if (idx->op == S_INDEX_E) {
    ScalarUop const *param = &ke->scalar_uops[idx->src[0]];
    if (param->op != S_DEFINE_PARAM) {
      return 0;
    }
    *slot = (u32)param->extra;
    return 1;
  }
  while (idx->op == S_INDEX
      && ke->scalar_uops[idx->src[0]].op == S_INDEX) {
    idx = &ke->scalar_uops[idx->src[0]];
  }
  if (idx->op != S_INDEX) {
    return 0;
  }
  ScalarUop const *param = &ke->scalar_uops[idx->src[0]];
  if (param->op != S_DEFINE_PARAM) {
    return 0;
  }
  *slot = (u32)param->extra;
  return 1;
}

static u32 cs_range_extent(KernelEntry const *ke, u32 range_id) {
  if (range_id == 0 || range_id >= ke->n_scalar_uops) {
    return 0;
  }
  ScalarUop const *r = &ke->scalar_uops[range_id];
  if (r->op != S_RANGE) {
    return 0;
  }
  return (u32)(r->extra & 0xFFFFFFFFu);
}

static u32 cs_range_axis_type(KernelEntry const *ke, u32 range_id) {
  if (range_id == 0 || range_id >= ke->n_scalar_uops) {
    return (u32)-1;
  }
  ScalarUop const *r = &ke->scalar_uops[range_id];
  if (r->op != S_RANGE) {
    return (u32)-1;
  }
  return (u32)((r->extra >> 32) & 0xFFFFFFFFu);
}

static u32 cs_iter_ref_extent(KernelEntry const *ke, u32 ref_id) {
  if (ref_id == 0 || ref_id >= ke->n_scalar_uops) {
    return 0;
  }
  ScalarUop const *u = &ke->scalar_uops[ref_id];
  if (u->op == S_RANGE) {
    return cs_range_extent(ke, ref_id);
  }
  if (u->op == S_IADD || u->op == S_ISUB) {
    ScalarUop const *a = &ke->scalar_uops[u->src[0]];
    ScalarUop const *b = &ke->scalar_uops[u->src[1]];
    if (a->op == S_RANGE) {
      return cs_range_extent(ke, u->src[0]);
    }
    if (b->op == S_RANGE) {
      return cs_range_extent(ke, u->src[1]);
    }
  }
  return 0;
}

// Emit the C value-producing expression for one scalar op.  S_RANGE
// expands to its bound loop variable; S_LOAD reads from in%u; S_CONST
// is a literal; ALU ops parenthesize their children; etc.
static int cs_emit_value(CgBuf *b, KernelEntry const *ke, u32 op_id) {
  ScalarUop const *u = &ke->scalar_uops[op_id];
  switch (u->op) {
    case S_RANGE:
      cg_append(b, "_v%u", op_id);
      return 1;
    case S_DEFINE_PARAM:
    case S_DEFINE_OUTPUT:
      // Buffer leaves never appear in value position (they appear
      // only as INDEX.src[0]); reaching them here is a bug.
      return 0;
    case S_LOAD:
    case S_LOAD_RAW: {
      // Recover the input slot from the INDEX chain's terminal
      // DEFINE_PARAM, then emit `in%u[(offset)]`.
      u32 slot = 0;
      if (!cs_index_input_slot(ke, u->src[0], &slot)) {
        return 0;
      }
      cg_append(b, "in%u[", slot);
      if (!cs_emit_index_offset(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "]");
      return 1;
    }
    case S_CONST: {
      u32 bits = (u32)(u->extra & 0xFFFFFFFFu);
      if (u->dtype == DT_FP32) {
        f32 v; memcpy(&v, &bits, 4);
        cg_append(b, "%.9gf", (double)v);
      } else {
        f32 v; memcpy(&v, &bits, 4);
        cg_append(b, "%.17g", (double)(f64)v);
      }
      return 1;
    }
    case S_ADD: {
      cg_append(b, "(");
      if (!cs_emit_value(b, ke, u->src[0])) return 0;
      cg_append(b, " + ");
      if (!cs_emit_value(b, ke, u->src[1])) return 0;
      cg_append(b, ")");
      return 1;
    }
    case S_MUL: {
      cg_append(b, "(");
      if (!cs_emit_value(b, ke, u->src[0])) return 0;
      cg_append(b, " * ");
      if (!cs_emit_value(b, ke, u->src[1])) return 0;
      cg_append(b, ")");
      return 1;
    }
    case S_NEG: {
      cg_append(b, "(-(");
      if (!cs_emit_value(b, ke, u->src[0])) return 0;
      cg_append(b, "))");
      return 1;
    }
    case S_RECIP: {
      const char *one = (u->dtype == DT_FP32) ? "1.0f" : "1.0";
      cg_append(b, "(%s/(", one);
      if (!cs_emit_value(b, ke, u->src[0])) return 0;
      cg_append(b, "))");
      return 1;
    }
    case S_SQRT:
    case S_EXP2:
    case S_LOG2: {
      const char *suffix = (u->dtype == DT_FP32) ? "f" : "";
      const char *fname  = (u->op == S_SQRT) ? "sqrt"
                         : (u->op == S_EXP2) ? "exp2"
                                             : "log2";
      cg_append(b, "%s%s(", fname, suffix);
      if (!cs_emit_value(b, ke, u->src[0])) return 0;
      cg_append(b, ")");
      return 1;
    }
    case S_CMPLT:
    case S_CMPEQ: {
      const char *cmp = (u->op == S_CMPLT) ? "<" : "==";
      const char *one = (u->dtype == DT_FP32) ? "1.0f" : "1.0";
      const char *zero = (u->dtype == DT_FP32) ? "0.0f" : "0.0";
      cg_append(b, "((");
      if (!cs_emit_value(b, ke, u->src[0])) return 0;
      cg_append(b, " %s ", cmp);
      if (!cs_emit_value(b, ke, u->src[1])) return 0;
      cg_append(b, ") ? %s : %s)", one, zero);
      return 1;
    }
    case S_REDUCE_SUM:
    case S_REDUCE_MAX: {
      u32 rng_id = u->src[1];
      if (rng_id == 0 || rng_id >= ke->n_scalar_uops) {
        return 0;
      }
      ScalarUop const *r = &ke->scalar_uops[rng_id];
      if (r->op != S_RANGE) {
        return 0;
      }
      u32 axis_type = (u32)((r->extra >> 32) & 0xFFFFFFFFu);
      if (axis_type != S_AXIS_REDUCE && axis_type != S_AXIS_UNROLL) {
        return 0;
      }
      u32 extent = (u32)(r->extra & 0xFFFFFFFFu);
      const char *T = cs_dtype_to_c(u->dtype);
      const char *init = (u->op == S_REDUCE_MAX) ? "-INFINITY" : "0";
      cg_append(b, "({ %s _acc%u = (%s)%s; ", T, op_id, T, init);
      cg_append(b, "for (unsigned _v%u = 0; _v%u < %uu; _v%u++) { ",
                rng_id, rng_id, extent, rng_id);
      cg_append(b, "%s _rv%u = ", T, op_id);
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "; ");
      if (u->op == S_REDUCE_MAX) {
        cg_append(b, "if (_rv%u > _acc%u) { _acc%u = _rv%u; } ",
                  op_id, op_id, op_id, op_id);
      } else {
        cg_append(b, "_acc%u += _rv%u; ", op_id, op_id);
      }
      cg_append(b, "} _acc%u; })", op_id);
      return 1;
    }
    case S_CAST: {
      const char *T = cs_dtype_to_c(u->dtype);
      cg_append(b, "((%s)(", T);
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "))");
      return 1;
    }
    case S_SHRINK: {
      u32 nrng = (u32)u->src_count - 1;
      const char *T = cs_dtype_to_c(u->dtype);
      cg_append(b, "({ %s _mv%u; ", T, op_id);
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        if (cs_range_extent(ke, rng_id) == 0) {
          return 0;
        }
        u32 begin = (u32)((u->extra >> (16 * d)) & 0xFFFFu);
        cg_append(b, "unsigned _sv%u_%u = _v%u; ", op_id, d, rng_id);
        cg_append(b, "_v%u = _sv%u_%u + %uu; ",
                  rng_id, op_id, d, begin);
      }
      cg_append(b, "_mv%u = ", op_id);
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "; ");
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        cg_append(b, "_v%u = _sv%u_%u; ", rng_id, op_id, d);
      }
      cg_append(b, "_mv%u; })", op_id);
      return 1;
    }
    case S_PAD: {
      u32 nrng = (u32)u->src_count - 1;
      const char *T = cs_dtype_to_c(u->dtype);
      cg_append(b, "({ int _ok%u = 1; %s _mv%u = (%s)0; ",
                op_id, T, op_id, T);
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        if (cs_range_extent(ke, rng_id) == 0) {
          return 0;
        }
        u32 packed  = (u32)((u->extra >> (16 * d)) & 0xFFFFu);
        u32 begin   = packed & 0xFFu;
        u32 src_dim = (packed >> 8) & 0xFFu;
        if (src_dim == 0) {
          return 0;
        }
        cg_append(b, "unsigned _sv%u_%u = _v%u; ", op_id, d, rng_id);
        cg_append(b, "if (_sv%u_%u < %uu || _sv%u_%u >= %uu) { _ok%u = 0; } ",
                  op_id, d, begin, op_id, d, begin + src_dim, op_id);
        cg_append(b, "_v%u = _sv%u_%u - %uu; ",
                  rng_id, op_id, d, begin);
      }
      cg_append(b, "if (_ok%u) { _mv%u = ", op_id, op_id);
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "; } ");
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        cg_append(b, "_v%u = _sv%u_%u; ", rng_id, op_id, d);
      }
      cg_append(b, "_mv%u; })", op_id);
      return 1;
    }
    case S_FLIP: {
      u32 nrng = (u32)u->src_count - 1;
      u32 mask = (u32)(u->extra & 0xFFu);
      const char *T = cs_dtype_to_c(u->dtype);
      cg_append(b, "({ %s _mv%u; ", T, op_id);
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        u32 extent = cs_range_extent(ke, rng_id);
        if (extent == 0) {
          return 0;
        }
        cg_append(b, "unsigned _sv%u_%u = _v%u; ", op_id, d, rng_id);
        if (mask & (1u << d)) {
          cg_append(b, "_v%u = %uu - 1u - _sv%u_%u; ",
                    rng_id, extent, op_id, d);
        }
      }
      cg_append(b, "_mv%u = ", op_id);
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "; ");
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        cg_append(b, "_v%u = _sv%u_%u; ", rng_id, op_id, d);
      }
      cg_append(b, "_mv%u; })", op_id);
      return 1;
    }
    case S_RESHAPE: {
      u32 nrng = (u32)u->src_count - 1;
      if (nrng > MAX_DIM) {
        return 0;
      }
      u8 out_dims[MAX_DIM] = {0};
      u8 in_dims [MAX_DIM] = {0};
      u32 lo = (u32)(u->extra & 0xFFFFFFFFu);
      u32 hi = (u32)((u->extra >> 32) & 0xFFFFFFFFu);
      for (u32 d = 0; d < 4 && d < MAX_DIM; d++) {
        out_dims[d] = (u8)((lo >> (8 * d)) & 0xFFu);
        in_dims [d] = (u8)((hi >> (8 * d)) & 0xFFu);
      }
      u64 in_stride[MAX_DIM] = {0};
      u64 s = 1;
      for (i32 d = (i32)nrng - 1; d >= 0; d--) {
        u32 dim = (in_dims[d] != 0 ? in_dims[d] : 1);
        in_stride[d] = s;
        s *= dim;
      }
      const char *T = cs_dtype_to_c(u->dtype);
      cg_append(b, "({ uint64_t _flat%u = 0; uint64_t _st%u = 1; ",
                op_id, op_id);
      for (i32 d = (i32)nrng - 1; d >= 0; d--) {
        u32 rng_id = u->src[1 + (u32)d];
        if (cs_range_extent(ke, rng_id) == 0) {
          return 0;
        }
        u32 dim = (out_dims[d] != 0 ? out_dims[d] : 1);
        cg_append(b, "unsigned _sv%u_%u = _v%u; ",
                  op_id, (u32)d, rng_id);
        cg_append(b, "_flat%u += (uint64_t)_sv%u_%u * _st%u; ",
                  op_id, op_id, (u32)d, op_id);
        cg_append(b, "_st%u *= %uu; ", op_id, dim);
      }
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        u32 dim = (in_dims[d] != 0 ? in_dims[d] : 1);
        cg_append(b, "_v%u = (unsigned)((_flat%u / %lluULL) %% %uu); ",
                  rng_id, op_id, (unsigned long long)in_stride[d], dim);
      }
      cg_append(b, "%s _mv%u = ", T, op_id);
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "; ");
      for (u32 d = 0; d < nrng; d++) {
        u32 rng_id = u->src[1 + d];
        cg_append(b, "_v%u = _sv%u_%u; ", rng_id, op_id, d);
      }
      cg_append(b, "_mv%u; })", op_id);
      return 1;
    }
    case S_RESHAPE_V: {
      u32 n_out = (u32)(u->extra & 0xFFu);
      u32 nrest = (u32)u->src_count - 1;
      if (n_out == 0 || n_out > nrest) {
        return 0;
      }
      u32 n_in = nrest - n_out;
      if (n_in > SCALAR_MAX_SRC || n_out > SCALAR_MAX_SRC) {
        return 0;
      }
      u32 in_ranges[SCALAR_MAX_SRC] = {0};
      u32 in_extent[SCALAR_MAX_SRC] = {0};
      u64 in_stride[SCALAR_MAX_SRC] = {0};
      u64 s = 1;
      for (i32 d = (i32)n_in - 1; d >= 0; d--) {
        u32 rng_id = u->src[1 + n_out + (u32)d];
        u32 extent = cs_range_extent(ke, rng_id);
        if (extent == 0) {
          return 0;
        }
        in_ranges[d] = rng_id;
        in_extent[d] = extent;
        in_stride[d] = s;
        s *= extent;
      }
      cg_append(b, "({ uint64_t _flat%u = 0; uint64_t _st%u = 1; ",
                op_id, op_id);
      for (i32 d = (i32)n_out - 1; d >= 0; d--) {
        u32 ref = u->src[1 + (u32)d];
        u32 extent = cs_iter_ref_extent(ke, ref);
        if (extent == 0) {
          return 0;
        }
        cg_append(b, "_flat%u += (uint64_t)(", op_id);
        if (!cs_emit_value(b, ke, ref)) {
          return 0;
        }
        cg_append(b, ") * _st%u; ", op_id);
        cg_append(b, "_st%u *= %uu; ", op_id, extent);
      }
      for (u32 d = 0; d < n_in; d++) {
        u32 rng_id = in_ranges[d];
        u32 axis_type = cs_range_axis_type(ke, rng_id);
        if (axis_type == S_AXIS_VIRT) {
          cg_append(b, "unsigned _v%u = (unsigned)((_flat%u / %lluULL) %% %uu); ",
                    rng_id, op_id, (unsigned long long)in_stride[d],
                    in_extent[d]);
        } else {
          cg_append(b, "unsigned _sv%u_%u = _v%u; ", op_id, d, rng_id);
          cg_append(b, "_v%u = (unsigned)((_flat%u / %lluULL) %% %uu); ",
                    rng_id, op_id, (unsigned long long)in_stride[d],
                    in_extent[d]);
        }
      }
      const char *T = cs_dtype_to_c(u->dtype);
      cg_append(b, "%s _mv%u = ", T, op_id);
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, "; ");
      for (u32 d = 0; d < n_in; d++) {
        u32 rng_id = in_ranges[d];
        u32 axis_type = cs_range_axis_type(ke, rng_id);
        if (axis_type != S_AXIS_VIRT) {
          cg_append(b, "_v%u = _sv%u_%u; ", rng_id, op_id, d);
        }
      }
      cg_append(b, "_mv%u; })", op_id);
      return 1;
    }
    case S_ICONST: {
      cg_append(b, "((int64_t)%lldLL)", (long long)(i64)u->extra);
      return 1;
    }
    case S_IADD:
    case S_ISUB:
    case S_IMUL:
    case S_IAND: {
      const char *op = (u->op == S_IADD) ? "+"
                     : (u->op == S_ISUB) ? "-"
                     : (u->op == S_IMUL) ? "*"
                                          : "&";
      cg_append(b, "((int64_t)(");
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, ") %s (int64_t)(", op);
      if (!cs_emit_value(b, ke, u->src[1])) {
        return 0;
      }
      cg_append(b, "))");
      return 1;
    }
    case S_IDIV:
    case S_IMOD: {
      const char *op = (u->op == S_IDIV) ? "/" : "%";
      cg_append(b, "(((int64_t)(");
      if (!cs_emit_value(b, ke, u->src[1])) {
        return 0;
      }
      cg_append(b, ") == 0) ? (int64_t)0 : ((int64_t)(");
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, ") %s (int64_t)(", op);
      if (!cs_emit_value(b, ke, u->src[1])) {
        return 0;
      }
      cg_append(b, ")))");
      return 1;
    }
    case S_ILT: {
      cg_append(b, "(((int64_t)(");
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, ") < (int64_t)(");
      if (!cs_emit_value(b, ke, u->src[1])) {
        return 0;
      }
      cg_append(b, ")) ? (int64_t)1 : (int64_t)0)");
      return 1;
    }
    case S_IWHERE: {
      cg_append(b, "((");
      if (!cs_emit_value(b, ke, u->src[0])) {
        return 0;
      }
      cg_append(b, ") ? (");
      if (!cs_emit_value(b, ke, u->src[1])) {
        return 0;
      }
      cg_append(b, ") : (");
      if (!cs_emit_value(b, ke, u->src[2])) {
        return 0;
      }
      cg_append(b, "))");
      return 1;
    }
    default:
      return 0;
  }
}

// Top-level entry: render a complete C source file the JIT can
// compile + dlopen.  Returns malloc'd buffer (caller free), or NULL
// on emit failure.  The C function signature matches render_c.c so
// cpu_jit_dispatch can call into it via the same CpuJitFn typedef.
fn char *cg_emit_scalar(KernelEntry const *ke) {
  CsKernelInfo info;
  if (!cs_collect_kernel_info(ke, &info)) return NULL;

  CgBuf b = { .buf = (char *)malloc(4096), .cap = 4096, .len = 0,
              .program_dtype = info.output_dtype, .ke = ke };
  if (!b.buf) return NULL;

  const char *out_T = cs_dtype_to_c(info.output_dtype);
  cg_append(&b, "#include <math.h>\n");
  cg_append(&b, "#include <stdint.h>\n\n");
  cg_append(&b, "void k(void *out_v, const void *const *ins_v, "
                "unsigned n, const unsigned *in_numels) {\n");
  cg_append(&b, "  %s *out = (%s *)out_v;\n", out_T, out_T);
  for (u32 i = 0; i < ke->n_inputs; i++) {
    const char *in_T = cs_dtype_to_c(ke->input_dtypes[i]);
    cg_append(&b, "  const %s *in%u = (const %s *)ins_v[%u];\n",
              in_T, i, in_T, i);
  }
  cg_append(&b, "  (void)n; (void)in_numels;\n");

  // Open the LOOP nest.  Outermost = loop_ids[0].  Use the loop
  // op_id as the variable suffix so cs_emit_value's S_RANGE branch
  // can refer to it directly: `_v%u`.
  for (u32 d = 0; d < info.n_loops; d++) {
    for (u32 indent = 0; indent < d + 1; indent++) cg_append(&b, "  ");
    cg_append(&b, "for (unsigned _v%u = 0; _v%u < %uu; _v%u++) {\n",
              info.loop_ids[d], info.loop_ids[d],
              info.loop_extents[d], info.loop_ids[d]);
  }

  // Emit the STORE: out[idx] = value.
  ScalarUop const *st = &ke->scalar_uops[info.store_id];
  for (u32 indent = 0; indent < info.n_loops + 1; indent++) cg_append(&b, "  ");
  cg_append(&b, "out[");
  if (!cs_emit_index_offset(&b, ke, st->src[0])) {
    free(b.buf);
    return NULL;
  }
  cg_append(&b, "] = ");
  if (!cs_emit_value(&b, ke, st->src[1])) {
    free(b.buf);
    return NULL;
  }
  cg_append(&b, ";\n");

  // Close the LOOP nest.
  for (i32 d = (i32)info.n_loops - 1; d >= 0; d--) {
    for (u32 indent = 0; indent < (u32)d + 1; indent++) cg_append(&b, "  ");
    cg_append(&b, "}\n");
  }
  cg_append(&b, "}\n");

  return b.buf;
}

typedef struct {
  CsKernelInfo scalar;
  TilePlanInfo tile;
  u32          n_axes;
  u32          axis_types  [MAX_AXES];
  u32          axis_extents[MAX_AXES];
} CtKernelInfo;

static int ct_axis_supported(u32 axis_type) {
  switch (axis_type) {
    case KAX_LOOP:
    case KAX_REDUCE:
    case KAX_UPCAST:
    case KAX_UNROLL:
    case KAX_LOCAL:
    case KAX_GLOBAL:
    case KAX_GROUP_REDUCE:
      return 1;
    default:
      return 0;
  }
}

static int ct_axis_is_output(u32 axis_type) {
  switch (axis_type) {
    case KAX_LOOP:
    case KAX_UPCAST:
    case KAX_LOCAL:
    case KAX_GLOBAL:
      return 1;
    default:
      return 0;
  }
}

static int ct_collect_kernel_info(KernelEntry const *ke, CtKernelInfo *out) {
  if (!cs_collect_kernel_info(ke, &out->scalar)) {
    return 0;
  }
  if (!tile_collect_plan_info(ke, &out->tile)) {
    return 0;
  }
  if (out->tile.scalar_store_id != out->scalar.store_id) {
    return 0;
  }

  u64 tile_numel = 1;
  out->n_axes = 0;
  for (u32 i = 0; i < out->tile.n_axes; i++) {
    u32 axis_type = out->tile.axis_types[i];
    u32 extent    = out->tile.axis_extents[i];
    if (!ct_axis_supported(axis_type) || extent == 0) {
      return 0;
    }
    if (!ct_axis_is_output(axis_type)) {
      if (!out->scalar.has_reduce) {
        return 0;
      }
      continue;
    }
    if (out->n_axes >= MAX_AXES) {
      return 0;
    }
    out->axis_types  [out->n_axes] = axis_type;
    out->axis_extents[out->n_axes] = extent;
    out->n_axes++;
    tile_numel *= extent;
  }
  if (tile_numel != (u64)(ke->output_numel ? ke->output_numel : 1)) {
    return 0;
  }
  return 1;
}

fn int cg_supports_tile(KernelEntry const *ke) {
  CtKernelInfo info;
  return ct_collect_kernel_info(ke, &info);
}

fn char *cg_emit_tile(KernelEntry const *ke) {
  CtKernelInfo info;
  if (!ct_collect_kernel_info(ke, &info)) {
    return NULL;
  }

  CgBuf b = { .buf = (char *)malloc(4096), .cap = 4096, .len = 0,
              .program_dtype = info.scalar.output_dtype, .ke = ke };
  if (!b.buf) {
    return NULL;
  }

  const char *out_T = cs_dtype_to_c(info.scalar.output_dtype);
  cg_append(&b, "#include <math.h>\n");
  cg_append(&b, "#include <stdint.h>\n\n");
  cg_append(&b, "void k(void *out_v, const void *const *ins_v, "
                "unsigned n, const unsigned *in_numels) {\n");
  cg_append(&b, "  %s *out = (%s *)out_v;\n", out_T, out_T);
  for (u32 i = 0; i < ke->n_inputs; i++) {
    const char *in_T = cs_dtype_to_c(ke->input_dtypes[i]);
    cg_append(&b, "  const %s *in%u = (const %s *)ins_v[%u];\n",
              in_T, i, in_T, i);
  }
  cg_append(&b, "  (void)n; (void)in_numels;\n");

  for (u32 d = 0; d < info.n_axes; d++) {
    for (u32 indent = 0; indent < d + 1; indent++) {
      cg_append(&b, "  ");
    }
    if (info.axis_types[d] == KAX_UPCAST && info.axis_extents[d] > 1) {
      cg_append(&b, "#pragma clang loop unroll_count(%u)\n",
                info.axis_extents[d]);
      for (u32 indent = 0; indent < d + 1; indent++) {
        cg_append(&b, "  ");
      }
    }
    cg_append(&b, "for (unsigned _ta%u = 0; _ta%u < %uu; _ta%u++) {\n",
              d, d, info.axis_extents[d], d);
  }

  for (u32 indent = 0; indent < info.n_axes + 1; indent++) {
    cg_append(&b, "  ");
  }
  cg_append(&b, "unsigned _tk = 0u;\n");
  for (u32 d = 0; d < info.n_axes; d++) {
    for (u32 indent = 0; indent < info.n_axes + 1; indent++) {
      cg_append(&b, "  ");
    }
    cg_append(&b, "_tk = _tk * %uu + _ta%u;\n", info.axis_extents[d], d);
  }

  u32 loop_strides[MAX_DIM] = {0};
  if (info.scalar.n_loops > 0) {
    loop_strides[info.scalar.n_loops - 1] = 1;
    for (i32 d = (i32)info.scalar.n_loops - 2; d >= 0; d--) {
      loop_strides[d] = loop_strides[d + 1] * info.scalar.loop_extents[d + 1];
    }
  }
  for (u32 d = 0; d < info.scalar.n_loops; d++) {
    for (u32 indent = 0; indent < info.n_axes + 1; indent++) {
      cg_append(&b, "  ");
    }
    cg_append(&b, "unsigned _v%u = (_tk / %uu) %% %uu;\n",
              info.scalar.loop_ids[d], loop_strides[d],
              info.scalar.loop_extents[d]);
  }

  ScalarUop const *st = &ke->scalar_uops[info.scalar.store_id];
  for (u32 indent = 0; indent < info.n_axes + 1; indent++) {
    cg_append(&b, "  ");
  }
  cg_append(&b, "out[");
  if (!cs_emit_index_offset(&b, ke, st->src[0])) {
    free(b.buf);
    return NULL;
  }
  cg_append(&b, "] = ");
  if (!cs_emit_value(&b, ke, st->src[1])) {
    free(b.buf);
    return NULL;
  }
  cg_append(&b, ";\n");

  for (i32 d = (i32)info.n_axes - 1; d >= 0; d--) {
    for (u32 indent = 0; indent < (u32)d + 1; indent++) {
      cg_append(&b, "  ");
    }
    cg_append(&b, "}\n");
  }
  cg_append(&b, "}\n");

  return b.buf;
}
