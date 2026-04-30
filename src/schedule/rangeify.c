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
  // Phase B handles f32-only.  Other dtypes (i32 / f16 / bf16 / int
  // family) go to later phases -- the scalar interpreter's f32
  // bit-cast path would silently misinterpret them.
  if (ke->output_dtype != DT_FP32) return 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_dtypes[i] != DT_FP32) return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (ke->program[i].dtype != DT_FP32) return 0;
  }
  // Bail if any op is outside the supported set.  Reduce / movement
  // / load / cast / bitcast all go to later phases (or stay legacy).
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    switch (p->opcode) {
      case UOP_ADD: case UOP_MUL: case UOP_NEG: case UOP_RECIP:
      case UOP_SQRT: case UOP_EXP2: case UOP_LOG2:
      case UOP_CMPLT: case UOP_CMPEQ: case UOP_CONST:
        break;
      default:
        return 0;
    }
  }
  // Bail if any input view is non-contig.  Phase B only handles
  // flat reads; absorbing strides into INDEX expressions is a
  // follow-up.  Strided pre-mat already happens in cpu_interpret;
  // we'd be replicating that work otherwise.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    View const *v = &ke->input_views[i];
    if (!v->contiguous || v->offset != 0) return 0;
  }
  // Output rank must fit our per-call ranges[] stack array.
  Shape const *os = &ke->output_shape;
  if (os->ndim == 0 || os->ndim > MAX_DIM) return 0;
  // Numel sanity: every program op must produce output_numel
  // (broadcast handled below for CONST scalars at numel == 1).
  u32 onum = ke->output_numel;
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    if (p->numel != onum && !(p->opcode == UOP_CONST && p->numel == 1)) return 0;
  }
  // Every input also must match output numel (broadcast NIY) OR be
  // a numel-1 scalar that gets ranged at zero offset.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_numels[i] != onum && ke->input_numels[i] != 1) return 0;
  }

  // Start fresh -- emit_kernel_for_boundary may have run rangeify on
  // a previous attempt that bailed mid-way.
  rangeify_free(ke);

  // 1. Loop iterators: one S_RANGE per output dim, axis_type LOOP.
  u32 ranges[MAX_DIM];
  for (u32 d = 0; d < os->ndim; d++) {
    u64 extra = ((u64)S_AXIS_LOOP << 32) | (u64)os->dims[d];
    ranges[d] = rangeify_emit_leaf(ke, S_RANGE, DT_INT32, extra);
  }

  // 2. Output buffer + per-element STORE address.
  u32 out_buf = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, ke->output_dtype, 0);
  u32 out_idx_src[5] = {out_buf, 0,0,0,0};
  for (u32 d = 0; d < os->ndim && d < 4; d++) out_idx_src[1 + d] = ranges[d];
  u32 out_index = rangeify_emit(ke, S_INDEX, ke->output_dtype,
                                (u8)(1 + (os->ndim < 3 ? os->ndim : 3)),
                                out_idx_src, 0);
  // Note: S_INDEX max src_count is 4; if ndim > 3 we'd need to chain
  // INDEX nodes.  Bail conservatively for now.
  if (os->ndim > 3) { rangeify_free(ke); return 0; }

  // 3. Per-input load expressions (cached so multiple consumers share).
  u32 input_load[KERNEL_INIT_INPUT * 4];   // headroom; emit_kernel
                                           // typically runs <16 inputs
  if (ke->n_inputs > sizeof(input_load) / sizeof(input_load[0])) {
    rangeify_free(ke); return 0;
  }
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 dtype = ke->input_dtypes[i];
    u32 param = rangeify_emit_leaf(ke, S_DEFINE_PARAM, dtype, (u64)i);
    u32 idx_src[5] = {param, 0, 0, 0, 0};
    u8  idx_count  = 1;
    if (ke->input_numels[i] == 1) {
      // Scalar broadcast: INDEX with no range sources -- the load
      // resolves to a constant element of the input buffer.
      idx_count = 1;
    } else {
      for (u32 d = 0; d < os->ndim && d < 3; d++) {
        idx_src[1 + d] = ranges[d];
      }
      idx_count = (u8)(1 + (os->ndim < 3 ? os->ndim : 3));
    }
    u32 idx  = rangeify_emit(ke, S_INDEX, dtype, idx_count, idx_src, 0);
    u32 load = rangeify_emit_unary(ke, S_LOAD, dtype, idx);
    input_load[i] = load;
  }

  // 4. ALU lowering: one S_X per KProgOp.  Source resolution mirrors
  // cpu_interpret's KSRC_IS_INPUT/_INDEX path.
  u32 prog_value[KPROG_INIT_OPS * 4];
  if (ke->n_ops > sizeof(prog_value) / sizeof(prog_value[0])) {
    rangeify_free(ke); return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    u32 dtype  = p->dtype;
    if (p->opcode == UOP_CONST) {
      // arg holds the f32/i32 bit pattern.
      prog_value[i] = rangeify_emit_leaf(ke, S_CONST, dtype, (u64)p->arg);
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
  u32 buf_src[5] = {store, 0, 0, 0, 0};
  u8  buf_count  = (u8)(1 + (os->ndim < 3 ? os->ndim : 3));
  for (u32 d = 0; d < os->ndim && d < 3; d++) buf_src[1 + d] = ranges[d];
  rangeify_emit(ke, S_BUFFERIZE, ke->output_dtype, buf_count, buf_src, 0);
  return 1;
}
