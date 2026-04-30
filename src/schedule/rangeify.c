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
fn int rangeify_try_lower_elementwise(KernelEntry *ke) {
  if (ke == NULL || ke->n_ops == 0) return 0;
  // f32 only -- the scalar interpreter's bit-cast path would silently
  // misinterpret other dtypes.
  if (ke->output_dtype != DT_FP32) return 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (ke->program[i].dtype != DT_FP32) return 0;
  }
  // Phase B/C supported opcodes: elementwise + (one) REDUCE.
  // Movement / cast / bitcast / load go to later phases.
  int reduce_pos = -1;
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    switch (p->opcode) {
      case UOP_ADD: case UOP_MUL: case UOP_NEG: case UOP_RECIP:
      case UOP_SQRT: case UOP_EXP2: case UOP_LOG2:
      case UOP_CMPLT: case UOP_CMPEQ: case UOP_CONST:
        break;
      case UOP_REDUCE:
        if (reduce_pos != -1) return 0;       // > 1 reduce not yet supported
        reduce_pos = (int)i;
        break;
      default:
        return 0;
    }
  }
  // All input views must be contig over their underlying buffer.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    View const *v = &ke->input_views[i];
    if (!v->contiguous || v->offset != 0) return 0;
  }
  Shape const *os = &ke->output_shape;
  if (os->ndim == 0 || os->ndim > 3) return 0;

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
  //   - REDUCE op produces onum.
  //   - Post-REDUCE non-REDUCE ops produce onum (or 1 for CONST).
  // Without REDUCE, all ops produce onum (or 1 for CONST).
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p   = &ke->program[i];
    if (p->opcode == UOP_REDUCE) {
      if (p->numel != onum) return 0;
      continue;
    }
    u32 expected = (has_reduce && (int)i < reduce_pos) ? reduce_in_numel : onum;
    if (p->numel != expected
        && !(p->opcode == UOP_CONST && p->numel == 1)) return 0;
  }

  // Phase C-2 boundary: forbid an input read both pre- AND post-
  // REDUCE.  That's the softmax pattern -- we'd need two distinct
  // LOAD ops for the same input slot, which our 1:1 KProgOp -> input
  // load mapping can't express today.  Lands in Phase C-3.
  if (has_reduce) {
    for (u32 i = 0; i < ke->n_inputs; i++) {
      int seen_pre = 0, seen_post = 0;
      for (u32 j = 0; j < ke->n_ops; j++) {
        KProgOp *p = &ke->program[j];
        for (u8 s = 0; s < p->n_src; s++) {
          u32 raw = p->src[s];
          if (KSRC_IS_INPUT(raw) && KSRC_INDEX(raw) == i) {
            if ((int)j <  reduce_pos) seen_pre  = 1;
            if ((int)j >= reduce_pos) seen_post = 1;
          }
        }
      }
      if (seen_pre && seen_post) return 0;
    }
  }
  // Inputs: same numel as output (Phase B), or REDUCE-input size, or
  // numel-1 scalar broadcast.
  u32 reduce_numel = onum * (has_reduce ? reduce_size : 1);
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 in_numel = ke->input_numels[i];
    if (in_numel != onum && in_numel != 1 && in_numel != reduce_numel) return 0;
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

  // 3. Per-input load expressions.  Branch on numel:
  //   - in_numel == 1                   -> scalar broadcast (INDEX over 0 ranges)
  //   - in_numel == output_numel        -> per-output-element read (LOOP ranges)
  //   - in_numel == output_numel*axis_size (only with REDUCE)
  //                                      -> per-(LOOP,REDUCE) read
  u32 input_load[KERNEL_INIT_INPUT * 4];
  if (ke->n_inputs > sizeof(input_load) / sizeof(input_load[0])) {
    rangeify_free(ke); return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 dtype    = ke->input_dtypes[i];
    u32 in_numel = ke->input_numels[i];
    u32 param    = rangeify_emit_leaf(ke, S_DEFINE_PARAM, dtype, (u64)i);
    u32 idx;
    if (in_numel == 1) {
      // Scalar broadcast: 0 range sources, address is always 0.
      u32 src[1] = {param};
      idx = rangeify_emit(ke, S_INDEX, dtype, 1, src, 0);
    } else if (has_reduce && in_numel == reduce_numel && in_numel != onum) {
      // Per-(LOOP, REDUCE) read.  Combined ranges + strides.  Only
      // taken in the actual REDUCE case where the input is bigger
      // than the output (in_numel == output_numel * axis_size and
      // axis_size > 1; the axis_size == 1 case collapses back to
      // per-output-element which is the else branch).
      u32 r_ids    [4]; u32 r_strides[4];
      for (u32 d = 0; d < os->ndim; d++) {
        r_ids[d]     = loop_ranges[d];
        r_strides[d] = in_strides[d];
      }
      r_ids    [os->ndim] = reduce_range;
      r_strides[os->ndim] = in_strides[os->ndim];
      idx = emit_index(ke, dtype, param, r_ids, r_strides, in_ndim);
    } else {
      idx = emit_index(ke, dtype, param, loop_ranges, loop_strides, os->ndim);
    }
    if (idx == 0) { rangeify_free(ke); return 0; }
    u32 load = rangeify_emit_unary(ke, S_LOAD, dtype, idx);
    input_load[i] = load;
  }

  // 4. ALU lowering: one S_X per KProgOp.
  u32 prog_value[KPROG_INIT_OPS * 4];
  if (ke->n_ops > sizeof(prog_value) / sizeof(prog_value[0])) {
    rangeify_free(ke); return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    u32 dtype  = p->dtype;
    if (p->opcode == UOP_CONST) {
      prog_value[i] = rangeify_emit_leaf(ke, S_CONST, dtype, (u64)p->arg);
      continue;
    }
    if (p->opcode == UOP_REDUCE) {
      // src[0] body: either an input load or a previous prog op.
      u32 raw = p->src[0];
      u32 body = KSRC_IS_INPUT(raw) ? input_load[KSRC_INDEX(raw)]
                                    : prog_value[KSRC_INDEX(raw)];
      u32 src[2] = {body, reduce_range};
      u8  sop    = (reduce_kind == REDUCE_MAX) ? S_REDUCE_MAX : S_REDUCE_SUM;
      prog_value[i] = rangeify_emit(ke, sop, dtype, 2, src, 0);
      continue;
    }
    // Resolve up to 2 sources.
    u32 src_v[2] = {0, 0};
    for (u8 s = 0; s < p->n_src && s < 2; s++) {
      u32 raw = p->src[s];
      if (KSRC_IS_INPUT(raw)) src_v[s] = input_load[KSRC_INDEX(raw)];
      else                    src_v[s] = prog_value[KSRC_INDEX(raw)];
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
