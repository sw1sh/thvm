// schedule/rangeify.c -- Phase A scaffolding for the scalar-UOp
// lowering pass.  Owns the per-kernel ScalarUop[] arena and the
// minimal construction surface; the actual high-level-UOp ->
// scalar-UOp rewrite logic lands in Phase B.  See
// docs/plans/scalar_uops_lowering.md for the full plan.
//
// Slot 0 of each ScalarUop[] is the S_NONE sentinel and is never
// allocated to a real op; callers test `src[i] == 0` to mean
// "no source".  Geometric growth via realloc; cap at
// SUOP_MAX_CAP (2^20) -- past that the lowering pass should
// have introduced more BUFFERIZE boundaries to break the kernel.

fn void rangeify_reserve(KernelEntry *ke, u32 needed) {
  if (needed <= ke->scalar_uops_cap) return;
  if (needed > SUOP_MAX_CAP) {
    fprintf(stderr, "rangeify_reserve: needed=%u exceeds cap %u\n",
            needed, SUOP_MAX_CAP);
    exit(1);
  }
  u32 new_cap = ke->scalar_uops_cap == 0 ? SUOP_INIT_CAP : ke->scalar_uops_cap * 2;
  while (new_cap < needed) new_cap *= 2;
  ke->scalar_uops = (ScalarUop *)realloc(ke->scalar_uops,
                                         (size_t)new_cap * sizeof(ScalarUop));
  // Zero the new tail.  Slot 0 is reserved as S_NONE; on first
  // alloc we also zero it explicitly so introspection sees a
  // clean sentinel.
  if (ke->scalar_uops_cap == 0) {
    memset(&ke->scalar_uops[0], 0, sizeof(ScalarUop));
    ke->n_scalar_uops = 1;
  }
  for (u32 i = ke->scalar_uops_cap == 0 ? 1 : ke->scalar_uops_cap;
       i < new_cap; i++) {
    memset(&ke->scalar_uops[i], 0, sizeof(ScalarUop));
  }
  ke->scalar_uops_cap = new_cap;
}

fn u32 rangeify_emit(KernelEntry *ke, u8 op, u32 dtype,
                     u8 src_count, const u32 *src, u64 extra) {
  if (op == S_NONE || op >= S__COUNT) {
    fprintf(stderr, "rangeify_emit: bad op=%u\n", op);
    exit(1);
  }
  if (src_count > 4) {
    fprintf(stderr, "rangeify_emit: src_count=%u exceeds max 4\n", src_count);
    exit(1);
  }
  rangeify_reserve(ke, ke->n_scalar_uops + 1);
  u32 id = ke->n_scalar_uops++;
  ScalarUop *u = &ke->scalar_uops[id];
  u->op        = op;
  u->src_count = src_count;
  u->dtype     = dtype;
  u->extra     = extra;
  for (u8 i = 0; i < 4; i++) u->src[i] = (i < src_count && src != NULL) ? src[i] : 0;
  return id;
}

fn u32 rangeify_emit_leaf(KernelEntry *ke, u8 op, u32 dtype, u64 extra) {
  return rangeify_emit(ke, op, dtype, 0, NULL, extra);
}

fn u32 rangeify_emit_unary(KernelEntry *ke, u8 op, u32 dtype, u32 a) {
  u32 src[1] = {a};
  return rangeify_emit(ke, op, dtype, 1, src, 0);
}

fn u32 rangeify_emit_binary(KernelEntry *ke, u8 op, u32 dtype, u32 a, u32 b) {
  u32 src[2] = {a, b};
  return rangeify_emit(ke, op, dtype, 2, src, 0);
}

fn void rangeify_free(KernelEntry *ke) {
  if (ke->scalar_uops != NULL) free(ke->scalar_uops);
  ke->scalar_uops      = NULL;
  ke->n_scalar_uops    = 0;
  ke->scalar_uops_cap  = 0;
}

fn const char *scalar_op_name(u8 op) {
  switch (op) {
    case S_NONE:           return "S_NONE";
    case S_RANGE:          return "S_RANGE";
    case S_DEFINE_PARAM:   return "S_DEFINE_PARAM";
    case S_DEFINE_OUTPUT:  return "S_DEFINE_OUTPUT";
    case S_INDEX:          return "S_INDEX";
    case S_LOAD:           return "S_LOAD";
    case S_STORE:          return "S_STORE";
    case S_BUFFERIZE:      return "S_BUFFERIZE";
    case S_CONST:          return "S_CONST";
    case S_ADD:            return "S_ADD";
    case S_MUL:            return "S_MUL";
    case S_NEG:            return "S_NEG";
    case S_RECIP:          return "S_RECIP";
    case S_EXP2:           return "S_EXP2";
    case S_LOG2:           return "S_LOG2";
    case S_SQRT:           return "S_SQRT";
    case S_CMPLT:          return "S_CMPLT";
    case S_CMPEQ:          return "S_CMPEQ";
    case S_REDUCE_SUM:     return "S_REDUCE_SUM";
    case S_REDUCE_MAX:     return "S_REDUCE_MAX";
    default:               return "S_?";
  }
}

fn const char *scalar_axis_name(u32 axis_type) {
  switch (axis_type) {
    case S_AXIS_LOOP:    return "LOOP";
    case S_AXIS_REDUCE:  return "REDUCE";
    case S_AXIS_UNROLL:  return "UNROLL";
    case S_AXIS_GLOBAL:  return "GLOBAL";
    default:             return "?";
  }
}

// Pack up to 3 u16 strides into the S_INDEX `extra` field.  Strides
// > 65535 force a bail (caller falls back to legacy KProgOp[]).
// Stride layout: stride[d] is at bit [16*d, 16*d+16); d in [0, 3).
static u64 pack_strides_u16(u32 const *strides, u32 ndim) {
  u64 packed = 0;
  if (ndim > 3) return UINT64_MAX;
  for (u32 d = 0; d < ndim; d++) {
    if (strides[d] > 0xFFFFu) return UINT64_MAX;
    packed |= ((u64)strides[d] & 0xFFFFu) << (16 * d);
  }
  return packed;
}

// Compute canonical row-major strides for `dims[0..ndim)`.
static void row_major_strides(u32 const *dims, u32 ndim, u32 *out) {
  if (ndim == 0) return;
  out[ndim - 1] = 1;
  for (i32 d = (i32)ndim - 2; d >= 0; d--) {
    out[d] = out[d + 1] * dims[d + 1];
  }
}

// Emit S_INDEX with strides packed into extra.  Returns the new uop
// id, or 0 on overflow (stride > 65535).
static u32 emit_index(KernelEntry *ke, u32 dtype, u32 buf_id,
                      u32 const *range_ids, u32 const *strides, u32 ndim) {
  if (ndim > 3) return 0;
  u64 packed = pack_strides_u16(strides, ndim);
  if (packed == UINT64_MAX) return 0;
  u32 src[4] = {buf_id, 0, 0, 0};
  for (u32 d = 0; d < ndim; d++) src[1 + d] = range_ids[d];
  return rangeify_emit(ke, S_INDEX, dtype, (u8)(1 + ndim), src, packed);
}

// === Phase B: rangeify a pure-elementwise KProgOp[] into ScalarUop[] ===
//
// Inputs:
//   ke -- a fully-emitted KernelEntry whose `program[0..n_ops)` is a
//         linearized SSA elementwise chain (UOP_ADD / MUL / NEG /
//         RECIP / SQRT / EXP2 / LOG2 / CMPLT / CMPEQ / CONST).
//         input_tids[] / input_views[] / output_shape are bound.
//
// Output (on success):
//   ke->scalar_uops populated with the lowered graph:
//     - S_DEFINE_PARAM per input slot
//     - S_DEFINE_OUTPUT for the kernel output
//     - S_RANGE per output dim (LOOP type) -- the loop iterators
//     - S_INDEX(buf, *ranges) + S_LOAD per concrete input read
//     - S_ADD / S_MUL / ... per ALU op (one S_X per KProgOp)
//     - S_CONST per CONST op
//     - S_STORE(S_INDEX(output, *ranges), final_value)
//     - S_BUFFERIZE(store, *ranges) at the very end (root marker)
//
// Returns 1 on success (caller can route dispatch through scalar
// path), 0 on bail (caller keeps using KProgOp[] directly).  Bail
// conditions covered today:
//   - KProgOp uses an opcode outside the elementwise/CONST set
//   - KProgOp numel mismatch (broadcast not yet handled, except for
//     CONST scalars at numel=1)
//   - Output rank > MAX_DIM (the per-call ranges[] stack array)
//   - Input has a non-contig view (Phase B handles contig only;
//     stride absorption lands when we add INDEX expression
//     compositing in a later phase)
//
// The lowering keeps the legacy KProgOp[] in place; cpu_dispatch_kernel
// gates on `ke->scalar_uops != NULL` to choose which path runs.
// Bail-debug: when THVM_RANGEIFY_BAIL=1, prints the reason any kernel
// failed to lower along with the kernel's identifying info.  Used in
// development to track down the next-supported pattern.
#define RBAIL(reason) do {                                              \
  if (getenv("THVM_RANGEIFY_BAIL")) {                                   \
    fprintf(stderr, "rangeify bail: " reason                            \
            " (n_ops=%u onum=%u ndim=%u)\n",                            \
            ke ? ke->n_ops : 0,                                         \
            ke ? ke->output_numel : 0,                                  \
            ke ? ke->output_shape.ndim : 0);                            \
  }                                                                     \
  rangeify_free(ke); return 0;                                          \
} while (0)
#define RBAIL_PRE(reason) do {                                          \
  if (getenv("THVM_RANGEIFY_BAIL")) {                                   \
    fprintf(stderr, "rangeify bail (pre-emit): " reason                 \
            " (n_ops=%u onum=%u ndim=%u)\n",                            \
            ke ? ke->n_ops : 0,                                         \
            ke ? ke->output_numel : 0,                                  \
            ke ? ke->output_shape.ndim : 0);                            \
  }                                                                     \
  return 0;                                                             \
} while (0)

fn int rangeify_try_lower_elementwise(KernelEntry *ke) {
  if (ke == NULL || ke->n_ops == 0) return 0;
  // f32 only -- the scalar interpreter's bit-cast path would silently
  // misinterpret other dtypes.
  if (ke->output_dtype != DT_FP32) RBAIL_PRE("non-f32 output");
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) RBAIL_PRE("non-f32 input");
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (ke->program[i].dtype != DT_FP32) RBAIL_PRE("non-f32 op");
  }
  // Phase B/C/D supported opcodes: elementwise + (one) REDUCE +
  // movement-as-identity (EXPAND, RESHAPE).  Movement ops between
  // contig shapes are no-ops at the scalar level since the flat
  // buffer is the same; the LOOP ranges adopt the output shape.
  // PERMUTE / PAD / SHRINK / FLIP / CAST / BITCAST land in later
  // phases.
  int reduce_pos = -1;
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    switch (p->opcode) {
      case UOP_ADD: case UOP_MUL: case UOP_NEG: case UOP_RECIP:
      case UOP_SQRT: case UOP_EXP2: case UOP_LOG2:
      case UOP_CMPLT: case UOP_CMPEQ: case UOP_CONST:
      case UOP_EXPAND: case UOP_RESHAPE:
        break;
      case UOP_REDUCE:
        if (reduce_pos != -1) RBAIL_PRE("> 1 reduce");
        reduce_pos = (int)i;
        break;
      default:
        RBAIL_PRE("unsupported opcode");
    }
  }
  // Phase D: input views can be non-contig (view.strides encode the
  // access pattern; 0 strides == broadcast, used heavily by chain-
  // rule grad expansion).  We bail only on:
  //   - non-zero offset (offset would need adding to every INDEX)
  //   - negative strides (FLIP; lands later)
  //   - shape mismatch (input rank > output rank, etc.)
  // Strides up to 65535 (u16-packed) are accepted; larger forces a
  // bail at emit_index.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    View const *v = &ke->input_views[i];
    if (v->offset != 0) RBAIL_PRE("non-zero view offset");
    for (u32 d = 0; d < v->shape.ndim; d++) {
      if (v->strides[d] < 0) RBAIL_PRE("negative stride (FLIP)");
    }
  }
  Shape const *os = &ke->output_shape;
  if (os->ndim == 0 || os->ndim > 3) RBAIL_PRE("output ndim out of range");

  // Detect REDUCE.  When present, the kernel emits a nested loop:
  // outer LOOP ranges over the kernel's output dims (== post-reduce
  // shape), inner REDUCE range over axis_size.  Phase C handles
  // inner==1 (reduce axis is the trailing dim of the input).
  int      has_reduce  = (reduce_pos >= 0);
  u32      reduce_kind = 0;
  u32      reduce_size = 0;
  KProgOp *red         = NULL;
  if (has_reduce) {
    red                = &ke->program[reduce_pos];
    reduce_kind        =  (red->arg >> 24) & 0xFFu;
    u32 reduce_inner   =   red->arg        & 0x00FFFFFFu;
    if (reduce_inner != 1) return 0;          // inner != 1 NIY
    if (red->n_src    != 1) return 0;
    u32 src_numel;
    if (KSRC_IS_INPUT(red->src[0])) {
      u32 in_slot = KSRC_INDEX(red->src[0]);
      src_numel   = ke->input_numels[in_slot];
    } else {
      u32 src_idx = KSRC_INDEX(red->src[0]);
      if (src_idx >= ke->n_ops) return 0;
      src_numel = ke->program[src_idx].numel;
    }
    if (src_numel == 0 || src_numel < red->numel) return 0;
    reduce_size = src_numel / red->numel;
    if (reduce_size * red->numel != src_numel) return 0;
    if (reduce_size == 0 || reduce_size > 0xFFFFu) return 0;
  }

  u32 onum            = ke->output_numel;
  u32 reduce_in_numel = has_reduce ? (red->numel * reduce_size) : 0;

  // Numel sanity by region (pre / post REDUCE):
  //   - Pre-REDUCE non-REDUCE ops produce reduce_in_numel
  //     (or 1 for CONST broadcast).
  //   - REDUCE op produces red->numel (the post-reduce-pre-broadcast
  //     numel; can differ from onum when an EXPAND broadcasts the
  //     reduce result back to the kernel's output shape).
  //   - Post-REDUCE non-REDUCE ops produce either red->numel
  //     (still scalar, e.g. RECIP after REDUCE) or onum (after
  //     EXPAND broadcasts back; e.g. MUL of input * scalar).
  // Without REDUCE, all ops produce onum (or 1 for CONST).
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    if (p->opcode == UOP_REDUCE) continue;     // numel == red->numel
                                               // (trivially)
    // Scalar ops (numel == 1) -- includes CONST broadcasts, MUL of
    // two scalars, RESHAPE of a scalar, etc.  Always allowed: in
    // the per-LOOP-element scalar walk they evaluate to a single
    // value reused across iterations.
    if (p->numel == 1) continue;
    if ((int)i < reduce_pos) {
      if (p->numel != reduce_in_numel) RBAIL_PRE("pre-reduce numel mismatch");
    } else {
      if (p->numel != onum
          && (!has_reduce || p->numel != red->numel)) RBAIL_PRE("post-reduce numel mismatch");
    }
  }

  // Per-input scope usage: track whether each input slot is read
  // pre-REDUCE (inside the reduce loop body) or post-REDUCE
  // (per-element after the reduce result is broadcast).  When
  // an input is used in BOTH scopes, we emit TWO LOAD nodes (the
  // per-(LOOP, REDUCE) and per-(LOOP) reads).  This is the Phase
  // C-3 path that unblocks softmax / BatchNorm-style fusion.
  //
  // Linear position semantics:
  //   - position <= reduce_pos: ref is inside the reduce body
  //     (pre-scope).  The REDUCE op itself sits here -- its src
  //     IS the body.
  //   - position >  reduce_pos: ref is post-reduce (scalar or
  //     per-element).
  u8 input_used_pre [KERNEL_INIT_INPUT * 4] = {0};
  u8 input_used_post[KERNEL_INIT_INPUT * 4] = {0};
  if (ke->n_inputs > sizeof(input_used_pre) / sizeof(input_used_pre[0])) {
    return 0;
  }
  if (has_reduce) {
    for (u32 j = 0; j < ke->n_ops; j++) {
      KProgOp *p = &ke->program[j];
      int pre = ((int)j <= reduce_pos);
      for (u8 s = 0; s < p->n_src; s++) {
        u32 raw = p->src[s];
        if (KSRC_IS_INPUT(raw)) {
          u32 slot = KSRC_INDEX(raw);
          if (pre)  input_used_pre [slot] = 1;
          else      input_used_post[slot] = 1;
        }
      }
    }
  } else {
    for (u32 i = 0; i < ke->n_inputs; i++) input_used_post[i] = 1;
  }
  // Inputs: same numel as output (Phase B), or REDUCE-input size, or
  // numel-1 scalar broadcast.
  u32 reduce_numel = onum * (has_reduce ? reduce_size : 1);
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 in_numel = ke->input_numels[i];
    if (in_numel != onum && in_numel != 1 && in_numel != reduce_numel) {
      RBAIL_PRE("input numel mismatch");
    }
  }

  // Start fresh -- emit_kernel_for_boundary may have run rangeify on
  // a previous attempt that bailed mid-way.
  rangeify_free(ke);

  // 1. LOOP ranges: one S_RANGE per output dim, axis_type LOOP.
  u32 loop_ranges[MAX_DIM];
  u32 loop_strides[MAX_DIM];
  row_major_strides(os->dims, os->ndim, loop_strides);
  for (u32 d = 0; d < os->ndim; d++) {
    u64 extra = ((u64)S_AXIS_LOOP << 32) | (u64)os->dims[d];
    loop_ranges[d] = rangeify_emit_leaf(ke, S_RANGE, DT_INT32, extra);
  }
  // REDUCE range (Phase C): an additional inner-most range with
  // axis_type=REDUCE.  Only present when the kernel ends in
  // UOP_REDUCE.
  u32 reduce_range = 0;
  u32 in_ndim      = os->ndim;
  u32 in_strides[MAX_DIM + 1];
  if (has_reduce) {
    u64 extra = ((u64)S_AXIS_REDUCE << 32) | (u64)reduce_size;
    reduce_range = rangeify_emit_leaf(ke, S_RANGE, DT_INT32, extra);
    // Pre-reduce input shape = output_shape ++ {reduce_size}.
    in_ndim = os->ndim + 1;
    if (in_ndim > 3) { rangeify_free(ke); return 0; }
    u32 in_dims[MAX_DIM];
    for (u32 d = 0; d < os->ndim; d++) in_dims[d] = os->dims[d];
    in_dims[os->ndim] = reduce_size;
    row_major_strides(in_dims, in_ndim, in_strides);
  }

  // 2. Output buffer + STORE address (uses LOOP ranges only).
  u32 out_buf = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, ke->output_dtype, 0);
  u32 out_index = emit_index(ke, ke->output_dtype, out_buf,
                             loop_ranges, loop_strides, os->ndim);
  if (out_index == 0) { rangeify_free(ke); return 0; }

  // 3. Per-input load expressions.  Two parallel tables when an
  // input is used in BOTH scopes:
  //   input_load_pre [i] -- LOAD inside the reduce body (per-
  //                         (LOOP, REDUCE) address).  Used by ops
  //                         at position <= reduce_pos.
  //   input_load_post[i] -- LOAD outside the reduce body
  //                         (per-LOOP address).  Used by ops at
  //                         position > reduce_pos.
  // For inputs used in only one scope, the unused slot stays 0.
  // For non-REDUCE kernels, input_load_post is always populated.
  u32 input_load_pre [KERNEL_INIT_INPUT * 4] = {0};
  u32 input_load_post[KERNEL_INIT_INPUT * 4] = {0};
  if (ke->n_inputs > sizeof(input_load_pre) / sizeof(input_load_pre[0])) {
    rangeify_free(ke); return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 dtype    = ke->input_dtypes[i];
    u32 in_numel = ke->input_numels[i];
    u32 param    = rangeify_emit_leaf(ke, S_DEFINE_PARAM, dtype, (u64)i);

    // Pre-scope LOAD: how we index inside the reduce body depends
    // on the relationship between input shape and (output_shape +
    // reduce_size).  Three cases handled today:
    //  1. in_numel == 1 -> scalar broadcast (no ranges).
    //  2. in_numel == onum (full-reduce-with-broadcast-back, the
    //     softmax pattern): input shape == output shape.  The
    //     reduce body indexes by the REDUCE range alone (the
    //     LOOP range is the OUTER position; we sum across the
    //     full input).  Post-reduce reads use the LOOP range.
    //     Only valid when red->numel == 1 (fully reduced) AND
    //     reduce_size == in_numel.
    //  3. in_numel == reduce_in_numel != onum (partial reduce
    //     where output has >=1 LOOP dim and reduce axis is the
    //     trailing dim): index by (LOOP, REDUCE) ranges.
    if (input_used_pre[i]) {
      u32 idx;
      if (in_numel == 1) {
        u32 src[1] = {param};
        idx = rangeify_emit(ke, S_INDEX, dtype, 1, src, 0);
      } else if (in_numel == onum && red->numel == 1
                 && in_numel == reduce_size) {
        // Softmax-style symmetric: reduce body reads input[REDUCE]
        // independently of LOOP_iter.  Only the REDUCE range
        // contributes to the address; stride matches the single
        // input axis.
        u32 r_ids[1]    = {reduce_range};
        u32 r_strides[1] = {1};
        idx = emit_index(ke, dtype, param, r_ids, r_strides, 1);
      } else if (red->numel == 1 && in_numel == reduce_size
                 && ke->input_views[i].shape.ndim == 1) {
        // Full reduce over a rank-1 input.  Output is {1} (LOOP
        // range extent 1 -- positional info collapses to nothing).
        // Reduce body indexes input[REDUCE] using the input view's
        // single stride.
        View const *v = &ke->input_views[i];
        u32 r_ids[1]    = {reduce_range};
        u32 r_strides[1] = {(u32)v->strides[0]};
        idx = emit_index(ke, dtype, param, r_ids, r_strides, 1);
      } else if (in_numel == reduce_in_numel && in_numel != red->numel) {
        // Partial reduce over trailing axis: input shape is
        // (output_shape ++ {axis_size}).  Walk the input view's
        // actual strides per axis -- broadcast (stride==0) inputs
        // and other view-aware patterns must use the View, not
        // the synthesized canonical strides.
        View const *v = &ke->input_views[i];
        if (v->shape.ndim != in_ndim) { rangeify_free(ke); return 0; }
        for (u32 d = 0; d < in_ndim; d++) {
          u32 expected_dim = (d < os->ndim) ? os->dims[d] : reduce_size;
          // Allow stride==0 (broadcast) regardless of dim mismatch;
          // otherwise dims must match.
          if (v->shape.dims[d] != expected_dim && v->strides[d] != 0) {
            rangeify_free(ke); return 0;
          }
        }
        u32 r_ids    [4]; u32 r_strides[4];
        for (u32 d = 0; d < os->ndim; d++) {
          r_ids[d]     = loop_ranges[d];
          r_strides[d] = (u32)v->strides[d];
        }
        r_ids    [os->ndim] = reduce_range;
        r_strides[os->ndim] = (u32)v->strides[os->ndim];
        idx = emit_index(ke, dtype, param, r_ids, r_strides, in_ndim);
      } else {
        idx = emit_index(ke, dtype, param, loop_ranges, loop_strides, os->ndim);
      }
      if (idx == 0) { rangeify_free(ke); return 0; }
      input_load_pre[i] = rangeify_emit_unary(ke, S_LOAD, dtype, idx);
    }

    // Post-scope LOAD: per-LOOP-element read.  Stride encoding
    // mirrors the input's View -- non-contig inputs (e.g. broadcast
    // EXPAND with stride==0 on a dim, or transpose patterns landed
    // by Phase D's chain-rule grad expansion) are absorbed into
    // INDEX without bailing.
    if (input_used_post[i]) {
      u32 idx;
      if (in_numel == 1) {
        u32 src[1] = {param};
        idx = rangeify_emit(ke, S_INDEX, dtype, 1, src, 0);
      } else if (ke->input_views[i].shape.ndim == os->ndim) {
        // Same rank: walk the View's strides directly.  Stride 0
        // marks a broadcast axis (the load reuses the same value
        // for every iter of that range).
        View const *v = &ke->input_views[i];
        u32 strides_u32[MAX_DIM];
        for (u32 d = 0; d < os->ndim; d++) {
          if (v->shape.dims[d] != os->dims[d] && v->strides[d] != 0) {
            // Mismatched dims with non-broadcast stride: not yet
            // supported; bail.
            rangeify_free(ke); return 0;
          }
          strides_u32[d] = (u32)v->strides[d];
        }
        idx = emit_index(ke, dtype, param, loop_ranges, strides_u32, os->ndim);
      } else if (has_reduce && in_numel == reduce_in_numel
                 && reduce_in_numel == onum) {
        // Softmax-symmetric: input rank may differ from output
        // rank but numel matches; treat as same-shape contig.
        idx = emit_index(ke, dtype, param, loop_ranges, loop_strides, os->ndim);
      } else {
        rangeify_free(ke); return 0;
      }
      if (idx == 0) { rangeify_free(ke); return 0; }
      input_load_post[i] = rangeify_emit_unary(ke, S_LOAD, dtype, idx);
    }
  }

  // 4. ALU lowering: one S_X per KProgOp.  An op at position
  // <= reduce_pos uses input_load_pre[] for its input refs; at
  // position > reduce_pos uses input_load_post[].  The REDUCE op
  // itself sits at reduce_pos (pre-scope ref to body input).
  // UOP_EXPAND lowers as identity -- broadcast is implicit at the
  // per-LOOP-element scalar level.
  u32 prog_value[KPROG_INIT_OPS * 4];
  if (ke->n_ops > sizeof(prog_value) / sizeof(prog_value[0])) {
    rangeify_free(ke); return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    u32 dtype  = p->dtype;
    int pre    = (has_reduce && (int)i <= reduce_pos);
    u32 *input_load = pre ? input_load_pre : input_load_post;
    if (p->opcode == UOP_CONST) {
      prog_value[i] = rangeify_emit_leaf(ke, S_CONST, dtype, (u64)p->arg);
      continue;
    }
    if (p->opcode == UOP_EXPAND || p->opcode == UOP_RESHAPE) {
      // Movement-as-identity: src[0] is the value, output uses the
      // same scalar bits.  At the per-LOOP-element scalar level
      // EXPAND broadcast and RESHAPE flat-buffer rewrap are both
      // no-ops -- the LOOP ranges are already the output shape's
      // ranges, and contig-source movement ops don't change the
      // flat read pattern.
      u32 raw = p->src[0];
      u32 v   = KSRC_IS_INPUT(raw) ? input_load[KSRC_INDEX(raw)]
                                   : prog_value[KSRC_INDEX(raw)];
      if (v == 0) { rangeify_free(ke); return 0; }
      prog_value[i] = v;
      continue;
    }
    if (p->opcode == UOP_REDUCE) {
      // src[0] body: either an input load or a previous prog op.
      // Sourced from pre-scope (the body sits inside the reduce loop).
      u32 raw = p->src[0];
      u32 body = KSRC_IS_INPUT(raw) ? input_load_pre[KSRC_INDEX(raw)]
                                    : prog_value[KSRC_INDEX(raw)];
      if (body == 0) { rangeify_free(ke); return 0; }
      u32 src[2] = {body, reduce_range};
      u8  sop    = (reduce_kind == REDUCE_MAX) ? S_REDUCE_MAX : S_REDUCE_SUM;
      prog_value[i] = rangeify_emit(ke, sop, dtype, 2, src, 0);
      continue;
    }
    // Resolve up to 2 sources, picking the scope-appropriate input load.
    u32 src_v[2] = {0, 0};
    for (u8 s = 0; s < p->n_src && s < 2; s++) {
      u32 raw = p->src[s];
      if (KSRC_IS_INPUT(raw)) {
        src_v[s] = input_load[KSRC_INDEX(raw)];
        if (src_v[s] == 0) { rangeify_free(ke); return 0; }
      } else {
        src_v[s] = prog_value[KSRC_INDEX(raw)];
      }
    }
    u8 sop;
    switch (p->opcode) {
      case UOP_ADD:    sop = S_ADD;   break;
      case UOP_MUL:    sop = S_MUL;   break;
      case UOP_NEG:    sop = S_NEG;   break;
      case UOP_RECIP:  sop = S_RECIP; break;
      case UOP_SQRT:   sop = S_SQRT;  break;
      case UOP_EXP2:   sop = S_EXP2;  break;
      case UOP_LOG2:   sop = S_LOG2;  break;
      case UOP_CMPLT:  sop = S_CMPLT; break;
      case UOP_CMPEQ:  sop = S_CMPEQ; break;
      default: rangeify_free(ke); return 0;
    }
    if (p->n_src == 1) {
      prog_value[i] = rangeify_emit_unary(ke, sop, dtype, src_v[0]);
    } else if (p->n_src == 2) {
      prog_value[i] = rangeify_emit_binary(ke, sop, dtype, src_v[0], src_v[1]);
    } else {
      rangeify_free(ke); return 0;
    }
  }

  // 5. STORE the final value, BUFFERIZE the kernel.
  u32 final_v = prog_value[ke->n_ops - 1];
  u32 store   = rangeify_emit_binary(ke, S_STORE, ke->output_dtype,
                                     out_index, final_v);
  u32 buf_src[4] = {store, 0, 0, 0};
  u8  buf_count  = (u8)(1 + os->ndim);
  for (u32 d = 0; d < os->ndim; d++) buf_src[1 + d] = loop_ranges[d];
  rangeify_emit(ke, S_BUFFERIZE, ke->output_dtype, buf_count, buf_src, 0);
  return 1;
}
