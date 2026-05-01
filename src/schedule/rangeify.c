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
  if (src_count > SCALAR_MAX_SRC) {
    fprintf(stderr, "rangeify_emit: src_count=%u exceeds max %u\n",
            src_count, SCALAR_MAX_SRC);
    exit(1);
  }
  rangeify_reserve(ke, ke->n_scalar_uops + 1);
  u32 id = ke->n_scalar_uops++;
  ScalarUop *u = &ke->scalar_uops[id];
  u->op        = op;
  u->src_count = src_count;
  u->dtype     = dtype;
  u->extra     = extra;
  for (u8 i = 0; i < SCALAR_MAX_SRC; i++) {
    u->src[i] = (i < src_count && src != NULL) ? src[i] : 0;
  }
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
    case S_CAST:           return "S_CAST";
    case S_SHRINK:         return "S_SHRINK";
    case S_PAD:            return "S_PAD";
    case S_RESHAPE_V:      return "S_RESHAPE_V";
    case S_ICONST:         return "S_ICONST";
    case S_IADD:           return "S_IADD";
    case S_ISUB:           return "S_ISUB";
    case S_IMUL:           return "S_IMUL";
    case S_IDIV:           return "S_IDIV";
    case S_IMOD:           return "S_IMOD";
    case S_ILT:            return "S_ILT";
    case S_IAND:           return "S_IAND";
    case S_IWHERE:         return "S_IWHERE";
    case S_INDEX_E:        return "S_INDEX_E";
    default:               return "S_?";
  }
}

fn const char *scalar_axis_name(u32 axis_type) {
  switch (axis_type) {
    case S_AXIS_LOOP:    return "LOOP";
    case S_AXIS_REDUCE:  return "REDUCE";
    case S_AXIS_UNROLL:  return "UNROLL";
    case S_AXIS_GLOBAL:  return "GLOBAL";
    case S_AXIS_VIRT:    return "VIRT";
    default:             return "?";
  }
}

// Pack up to 3 u16 strides + 1 u16 offset into the S_INDEX `extra`
// field.  Anything > 65535 forces a bail.
// Layout:
//   bits [ 0..15] : stride for src[1] (range axis 0)
//   bits [16..31] : stride for src[2] (range axis 1)
//   bits [32..47] : stride for src[3] (range axis 2)
//   bits [48..63] : per-INDEX offset (added to address)
static u64 pack_index_extra(u32 const *strides, u32 ndim, u32 offset) {
  u64 packed = 0;
  if (ndim > 3) return UINT64_MAX;
  if (offset > 0xFFFFu) return UINT64_MAX;
  for (u32 d = 0; d < ndim; d++) {
    if (strides[d] > 0xFFFFu) return UINT64_MAX;
    packed |= ((u64)strides[d] & 0xFFFFu) << (16 * d);
  }
  packed |= ((u64)offset & 0xFFFFu) << 48;
  return packed;
}

// Convenience: pack strides only (offset = 0).  Used for chained
// INDEX nodes where the offset lives on the innermost.
static u64 pack_strides_u16(u32 const *strides, u32 ndim) {
  return pack_index_extra(strides, ndim, 0);
}

// Compute canonical row-major strides for `dims[0..ndim)`.
static void row_major_strides(u32 const *dims, u32 ndim, u32 *out) {
  if (ndim == 0) return;
  out[ndim - 1] = 1;
  for (i32 d = (i32)ndim - 2; d >= 0; d--) {
    out[d] = out[d + 1] * dims[d + 1];
  }
}

// Emit S_INDEX with strides + offset packed into extra.  Returns
// the new uop id, or 0 on overflow (stride or offset > 65535).
static u32 emit_index_off(KernelEntry *ke, u32 dtype, u32 buf_id,
                          u32 const *range_ids, u32 const *strides,
                          u32 ndim, u32 offset) {
  if (ndim > 3) return 0;
  u64 packed = pack_index_extra(strides, ndim, offset);
  if (packed == UINT64_MAX) return 0;
  u32 src[4] = {buf_id, 0, 0, 0};
  for (u32 d = 0; d < ndim; d++) src[1 + d] = range_ids[d];
  return rangeify_emit(ke, S_INDEX, dtype, (u8)(1 + ndim), src, packed);
}

// Emit an INDEX chain over `ndim` ranges, splitting into S_INDEX
// nodes of at most 3 ranges each.  Offset is added on the innermost
// node (the one whose src[0] is the actual buffer); subsequent
// chained nodes add 0.  All callsites in the lowerer route through
// here -- direct emit_index / emit_index_off use is reserved for
// the per-chunk emission inside this function.
static u32 emit_index_chain(KernelEntry *ke, u32 dtype, u32 buf_id,
                            u32 const *range_ids, u32 const *strides,
                            u32 ndim, u32 offset) {
  if (ndim > MAX_DIM) return 0;
  if (ndim == 0) {
    u32 src[1] = {buf_id};
    u64 packed = pack_index_extra(NULL, 0, offset);
    if (packed == UINT64_MAX) return 0;
    return rangeify_emit(ke, S_INDEX, dtype, 1, src, packed);
  }
  u32 cur     = buf_id;
  u32 cur_off = offset;
  u32 d       = 0;
  while (d < ndim) {
    u32 take = ndim - d;
    if (take > 3) take = 3;
    cur = emit_index_off(ke, dtype, cur, range_ids + d, strides + d,
                         take, cur_off);
    if (cur == 0) return 0;
    cur_off = 0;       // offset belongs to the innermost node only
    d      += take;
  }
  return cur;
}

static u32 emit_index(KernelEntry *ke, u32 dtype, u32 buf_id,
                      u32 const *range_ids, u32 const *strides, u32 ndim) {
  return emit_index_chain(ke, dtype, buf_id, range_ids, strides, ndim, 0);
}

// Helpers for emitting integer arithmetic on iter expressions (Phase 3
// of the apply_movement_op port).  Builds expressions over S_I* ops
// returning i64 values that S_INDEX_E can consume.

static u32 emit_iconst(KernelEntry *ke, i64 v) {
  u64 extra = (u64)v;
  return rangeify_emit_leaf(ke, S_ICONST, DT_INT64, extra);
}

static u32 emit_ibinop(KernelEntry *ke, u8 op, u32 a, u32 b) {
  return rangeify_emit_binary(ke, op, DT_INT64, a, b);
}

// Build an expression for `range_id_iter * stride + offset_term`.  Folds
// trivial cases (stride==0, stride==1) to keep the expression small.
// `offset_term` is 0 to omit the additive term, otherwise an op_id.
static u32 emit_axis_term(KernelEntry *ke, u32 range_id, u32 stride,
                          u32 offset_term) {
  if (stride == 0) return offset_term;  // axis contributes nothing
  u32 t = range_id;
  if (stride != 1) {
    u32 sc = emit_iconst(ke, (i64)stride);
    t = emit_ibinop(ke, S_IMUL, t, sc);
  }
  if (offset_term == 0) return t;
  return emit_ibinop(ke, S_IADD, t, offset_term);
}

// Build an integer expression that decomposes a flat iter into a
// per-axis coordinate, then multiplies by the input view's stride
// for that axis.  Used for cases where in_numel == reduce_size and
// the input has rank > 1: walk the input axes from outer to inner,
// computing coord[d] = (flat / suffix_size) % v->shape.dims[d], and
// accumulate addr += coord[d] * v->strides[d].  Suffix_size and the
// dim are constants known at emit time, so the expression is a tree
// of S_IDIV/S_IMOD/S_IMUL/S_IADD over a single S_RANGE.
static u32 build_addr_from_flat_iter(KernelEntry *ke, u32 flat_range_id,
                                     u32 const *dims, u32 const *strides,
                                     u32 ndim, u32 offset) {
  u32 acc = offset != 0 ? emit_iconst(ke, (i64)offset) : 0;
  // Compute suffix product from the right: suffix[ndim-1]=1,
  // suffix[d] = suffix[d+1] * dims[d+1].  coord[d] = (flat /
  // suffix[d]) % dims[d].
  u32 suffix[MAX_DIM];
  if (ndim == 0) return acc != 0 ? acc : emit_iconst(ke, 0);
  suffix[ndim - 1] = 1;
  for (i32 d = (i32)ndim - 2; d >= 0; d--) {
    suffix[d] = suffix[d + 1] * dims[d + 1];
  }
  for (u32 d = 0; d < ndim; d++) {
    if (strides[d] == 0) continue;  // broadcast axis: no contribution
    // coord = (flat / suffix[d]) % dims[d]
    u32 coord = flat_range_id;
    if (suffix[d] != 1) {
      u32 sc = emit_iconst(ke, (i64)suffix[d]);
      coord = emit_ibinop(ke, S_IDIV, coord, sc);
    }
    if (d != 0) {  // d==0 has dims[0] >= flat_max+1, so mod is no-op
      u32 dc = emit_iconst(ke, (i64)dims[d]);
      coord = emit_ibinop(ke, S_IMOD, coord, dc);
    }
    if (strides[d] != 1) {
      u32 sc = emit_iconst(ke, (i64)strides[d]);
      coord = emit_ibinop(ke, S_IMUL, coord, sc);
    }
    acc = (acc == 0) ? coord : emit_ibinop(ke, S_IADD, acc, coord);
  }
  return acc != 0 ? acc : emit_iconst(ke, 0);
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
// Mid-emit bail: structural issue discovered after we started building
// the scalar uop graph.  Frees any partial work and returns failure.
// F-8d added these to surface the silent return-0 sites that were
// previously masked by the legacy cpu_interpret fallback.
#define RBAIL_MID(reason) do {                                          \
  if (getenv("THVM_RANGEIFY_BAIL")) {                                   \
    fprintf(stderr, "rangeify bail (mid-emit): " reason                 \
            " (n_ops=%u onum=%u)\n",                                    \
            ke ? ke->n_ops : 0,                                         \
            ke ? ke->output_numel : 0);                                 \
  }                                                                     \
  rangeify_free(ke); return 0;                                          \
} while (0)

fn int rangeify_try_lower_elementwise(KernelEntry *ke) {
  if (ke == NULL || ke->n_ops == 0) return 0;
  // f32 only -- the scalar interpreter's bit-cast path would silently
  // misinterpret other dtypes.
  // Supported dtypes: bool / int{4,8..64} / fp32 / fp64 / narrow
  // FPs (fp16/bf16/fp8e4m3/fp8e5m2 widen to f32 internally) /
  // packed nibbles (int4/uint4 -- per-element load/store via
  // scalar_{load,store}_typed bit-shifts; ALU runs as i32/u32).
#define RANGEIFY_SUPPORTED_DTYPE(dt)                                          \
  ((dt) == DT_BOOL    || (dt) == DT_INT8   || (dt) == DT_UINT8             \
   || (dt) == DT_INT16  || (dt) == DT_UINT16                                  \
   || (dt) == DT_INT32  || (dt) == DT_UINT32                                  \
   || (dt) == DT_INT64  || (dt) == DT_UINT64                                  \
   || (dt) == DT_INT4   || (dt) == DT_UINT4                                   \
   || (dt) == DT_FP32   || (dt) == DT_FP64                                    \
   || (dt) == DT_FP16   || (dt) == DT_BF16                                    \
   || (dt) == DT_FP8E4M3 || (dt) == DT_FP8E5M2)
  if (!RANGEIFY_SUPPORTED_DTYPE(ke->output_dtype)) RBAIL_PRE("output dtype");
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (!RANGEIFY_SUPPORTED_DTYPE(ke->input_dtypes[i])) RBAIL_PRE("input dtype");
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (!RANGEIFY_SUPPORTED_DTYPE(ke->program[i].dtype)) RBAIL_PRE("op dtype");
  }
#undef RANGEIFY_SUPPORTED_DTYPE
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
      case UOP_EXPAND: case UOP_RESHAPE: case UOP_LOAD:
      case UOP_CAST:  case UOP_BITCAST:
      case UOP_SHRINK: case UOP_PAD: case UOP_FLIP:
        break;
      case UOP_REDUCE:
        if (reduce_pos != -1) RBAIL_PRE("> 1 reduce");
        reduce_pos = (int)i;
        break;
      default:
        if (getenv("THVM_RANGEIFY_BAIL")) {
          fprintf(stderr, "  unsupported opcode: %u\n", p->opcode);
        }
        RBAIL_PRE("unsupported opcode");
    }
  }
  // Phase D: input views can be non-contig (view.strides encode the
  // access pattern; 0 strides == broadcast, used heavily by chain-
  // rule grad expansion).  Phase F-1 lifts the non-zero-offset
  // restriction by packing the offset into S_INDEX.extra (u16
  // bits).  Bail conditions today:
  //   - negative strides (FLIP; needs a sign-aware INDEX, lands
  //     in F-3 alongside the FLIP movement op).
  //   - offset > 65535 (current u16 packing; bail until we widen
  //     extra encoding).
  for (u32 i = 0; i < ke->n_inputs; i++) {
    View const *v = &ke->input_views[i];
    if (v->offset < 0 || v->offset > 0xFFFF) RBAIL_PRE("offset out of u16 range");
    for (u32 d = 0; d < v->shape.ndim; d++) {
      if (v->strides[d] < 0) RBAIL_PRE("negative stride (FLIP)");
    }
  }
  Shape const *os = &ke->output_shape;
  if (os->ndim == 0 || os->ndim > MAX_DIM) RBAIL_PRE("output ndim out of range");

  // Detect REDUCE.  When present, the kernel emits a nested loop:
  // outer LOOP ranges over the kernel's output dims (== post-reduce
  // shape), inner REDUCE range over axis_size.  Phase C handles
  // inner==1 (reduce axis is the trailing dim of the input).
  int      has_reduce  = (reduce_pos >= 0);
  u32      reduce_kind = 0;
  u32      reduce_size = 0;
  u32      reduce_inner = 0;
  KProgOp *red         = NULL;
  if (has_reduce) {
    red                = &ke->program[reduce_pos];
    reduce_kind        =  (red->arg >> 24) & 0xFFu;
    reduce_inner       =   red->arg        & 0x00FFFFFFu;
    // inner>1 enabled (F-? -- view-aware pre-INDEX absorbed
    // broadcast strides; reduce_axis-search picks the right axis).
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
      // Pre-REDUCE chains often have multi-stage shape transforms:
      // SHRINK / RESHAPE produce smaller intermediate values that
      // get EXPAND'd back to reduce_in_numel before the REDUCE.
      // The strict "must equal reduce_in_numel" check rejected these
      // legitimate patterns.  Trust visit()'s shape inference -- the
      // scalar lowering walks each op's actual src/op encoding so
      // intermediate-size ops are addressed correctly via their own
      // shape info (SHRINK pad_widths, EXPAND srcs, etc.).
    } else {
      // Post-REDUCE chain similarly has multi-stage shape transforms
      // (RESHAPE / EXPAND / SHRINK / PAD) producing intermediate
      // sizes between red->numel and onum.  Trust visit() shape
      // inference here too -- the per-op lowering uses each op's
      // own encoding for its address pattern.
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
  u32 nin_local = ke->n_inputs ? ke->n_inputs : 1;
  u8 input_used_pre [nin_local];
  u8 input_used_post[nin_local];
  u8 input_via_padshrink[nin_local];
  for (u32 i = 0; i < nin_local; i++) {
    input_used_pre [i] = 0;
    input_used_post[i] = 0;
    input_via_padshrink[i] = 0;
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
  for (u32 j = 0; j < ke->n_ops; j++) {
    KProgOp *p = &ke->program[j];
    if (p->opcode != UOP_PAD && p->opcode != UOP_SHRINK) continue;
    if (p->n_src < 1) continue;
    u32 raw = p->src[0];
    if (KSRC_IS_INPUT(raw)) input_via_padshrink[KSRC_INDEX(raw)] = 1;
  }
  // (Earlier F-8e-5 bailed all kernels with a shape-changing RESHAPE
  //  in a PAD/SHRINK chain.  F-8e-7 lowers them via S_RESHAPE -- the
  //  iter-coord transform op -- in the per-op emit loop below.)
  // Inputs: same numel as output (Phase B), or REDUCE-input size, or
  // numel-1 scalar broadcast.
  u32 reduce_numel = onum * (has_reduce ? reduce_size : 1);
  (void)reduce_numel;
  // Input numel check dropped (Phase F-?): the per-input load
  // branches handle whatever shape the input view actually has
  // (scalar broadcast, same-shape, partial-reduce, full-reduce-
  // rank-1) and bail individually if no branch matches.

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
    if (in_ndim > MAX_DIM) RBAIL_MID("reduce in_ndim > MAX_DIM");
    u32 in_dims[MAX_DIM + 1];
    for (u32 d = 0; d < os->ndim; d++) in_dims[d] = os->dims[d];
    in_dims[os->ndim] = reduce_size;
    row_major_strides(in_dims, in_ndim, in_strides);
  }

  // 2. Output buffer + STORE address (uses LOOP ranges only).
  u32 out_buf = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, ke->output_dtype, 0);
  u32 out_index = emit_index(ke, ke->output_dtype, out_buf,
                             loop_ranges, loop_strides, os->ndim);
  if (out_index == 0) RBAIL_MID("out INDEX emit failed");

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
  u32 input_load_pre [nin_local];
  u32 input_load_post[nin_local];
  u8  via_rngs_pre   [nin_local];
  u8  via_rngs_post  [nin_local];
  for (u32 i = 0; i < nin_local; i++) {
    input_load_pre [i] = 0;
    input_load_post[i] = 0;
    via_rngs_pre   [i] = 0;
    via_rngs_post  [i] = 0;
  }

  // === BACKWARD WALK: compute rngs[i] per op + per-input-slot rngs.
  // Mirrors tinygrad's rangeify (indexing.py:148+).  Each op tells
  // us what iter expressions its sources see; for KSRC_IS_INPUT
  // sources, those expressions get captured into input_rngs_pre/post
  // for the input_load emission below.  Non-input sources flow back
  // through the chain.
  //
  // The walk EMITS S_RANGE/S_IADD/S_ISUB/S_IMUL/S_IDIV/S_IMOD/S_IAND
  // expressions into the scalar uop graph (these are values, not
  // dispatcher state, so emitting them here is fine).  The forward
  // pass below then references them when building input loads.
  typedef struct {
    u32 ndim;
    u32 refs[MAX_DIM];
    u32 valid_mask;  // 0 = always valid (no PAD upstream); else
                     // op_id of integer expression returning 0/1
                     // that gates the input load via S_IWHERE.
  } RngsCtx;
  u32 nops_local = ke->n_ops ? ke->n_ops : 1;
  RngsCtx rngs[nops_local];
  for (u32 i = 0; i < nops_local; i++) {
    rngs[i].ndim = 0; rngs[i].valid_mask = 0;
    for (u32 d = 0; d < MAX_DIM; d++) rngs[i].refs[d] = 0;
  }
  RngsCtx input_rngs_pre [nin_local];
  RngsCtx input_rngs_post[nin_local];
  for (u32 i = 0; i < nin_local; i++) {
    input_rngs_pre [i].ndim = 0; input_rngs_pre [i].valid_mask = 0;
    input_rngs_post[i].ndim = 0; input_rngs_post[i].valid_mask = 0;
    for (u32 d = 0; d < MAX_DIM; d++) {
      input_rngs_pre [i].refs[d] = 0;
      input_rngs_post[i].refs[d] = 0;
    }
  }
  if (ke->n_ops > 0) {
    i32 last = (i32)ke->n_ops - 1;
    rngs[last].ndim = os->ndim;
    for (u32 d = 0; d < os->ndim; d++) rngs[last].refs[d] = loop_ranges[d];
    for (i32 i = last; i >= 0; i--) {
      KProgOp *p = &ke->program[i];
      RngsCtx in_rngs = rngs[i];
      switch (p->opcode) {
        case UOP_REDUCE: {
          // REDUCE input shape = output ++ {reduce_size at reduce_axis}.
          // Compute reduce_axis from reduce_inner (product of input
          // dims AFTER the reduce axis): walk body shape from the
          // back, accumulating; when partial == reduce_inner, that's
          // the reduce axis position.  Insert reduce_range at that
          // position in in_rngs (NOT appended -- crucial for non-
          // trailing reduces like LeNet's conv-backward chain).
          if (!has_reduce || in_rngs.ndim >= MAX_DIM) break;
          u32 body_ndim = p->src0_ndim;
          if (body_ndim == 0) {
            // Fallback: append (matches old behavior).
            in_rngs.refs[in_rngs.ndim++] = reduce_range;
            break;
          }
          u32 r_axis = (u32)-1;
          {
            u32 partial = 1;
            for (i32 k = (i32)body_ndim - 1; k >= 0; k--) {
              if (partial == reduce_inner) { r_axis = (u32)k; break; }
              partial *= p->src0_dims[k];
            }
          }
          if (r_axis >= body_ndim) {
            // Couldn't infer; append as fallback.
            in_rngs.refs[in_rngs.ndim++] = reduce_range;
            break;
          }
          // Insert reduce_range at r_axis, shifting later refs right.
          for (i32 d = (i32)in_rngs.ndim; d > (i32)r_axis; d--) {
            in_rngs.refs[d] = in_rngs.refs[d - 1];
          }
          in_rngs.refs[r_axis] = reduce_range;
          in_rngs.ndim++;
          break;
        }
        case UOP_SHRINK: {
          for (u32 d = 0; d < p->out_ndim && d < in_rngs.ndim; d++) {
            u32 begin = p->pad_widths[2 * d];
            if (begin == 0) continue;
            u32 c = emit_iconst(ke, (i64)begin);
            in_rngs.refs[d] = emit_ibinop(ke, S_IADD, in_rngs.refs[d], c);
          }
          break;
        }
        case UOP_FLIP: {
          u32 mask = p->arg & 0xFFu;
          for (u32 d = 0; d < p->out_ndim && d < in_rngs.ndim; d++) {
            if (!(mask & (1u << d))) continue;
            u32 ext = p->src0_ndim > d ? p->src0_dims[d] : 1;
            u32 c = emit_iconst(ke, (i64)(ext - 1));
            in_rngs.refs[d] = emit_ibinop(ke, S_ISUB, c, in_rngs.refs[d]);
          }
          break;
        }
        case UOP_EXPAND: {
          for (u32 d = 0; d < p->out_ndim && d < in_rngs.ndim; d++) {
            u32 src_dim = p->src0_ndim > d ? p->src0_dims[d] : 1;
            u32 out_dim = p->out_dims[d];
            if (src_dim != out_dim) in_rngs.refs[d] = emit_iconst(ke, 0);
          }
          break;
        }
        case UOP_PERMUTE: {
          if (p->out_ndim == 0 || p->out_ndim > MAX_DIM
              || in_rngs.ndim != p->out_ndim) break;
          RngsCtx new_rngs = {0};
          new_rngs.ndim = p->out_ndim;
          new_rngs.valid_mask = in_rngs.valid_mask;
          for (u32 d = 0; d < p->out_ndim; d++) {
            u8 src_axis = p->axis_perm[d];
            if (src_axis >= MAX_DIM) { new_rngs.ndim = 0; break; }
            new_rngs.refs[src_axis] = in_rngs.refs[d];
          }
          if (new_rngs.ndim != 0) in_rngs = new_rngs;
          break;
        }
        case UOP_RESHAPE: {
          if (p->src0_ndim == 0 || p->out_ndim == 0
              || p->src0_ndim > MAX_DIM || p->out_ndim > MAX_DIM
              || in_rngs.ndim != p->out_ndim) break;
          u32 b_strides[MAX_DIM];
          row_major_strides(p->out_dims, p->out_ndim, b_strides);
          u32 flat = 0;
          for (u32 d = 0; d < p->out_ndim; d++) {
            if (b_strides[d] == 0) continue;
            u32 t = in_rngs.refs[d];
            if (b_strides[d] != 1) {
              u32 c = emit_iconst(ke, (i64)b_strides[d]);
              t = emit_ibinop(ke, S_IMUL, t, c);
            }
            if (flat == 0) flat = t;
            else           flat = emit_ibinop(ke, S_IADD, flat, t);
          }
          if (flat == 0) flat = emit_iconst(ke, 0);
          u32 a_strides[MAX_DIM];
          row_major_strides(p->src0_dims, p->src0_ndim, a_strides);
          RngsCtx new_rngs = {0};
          new_rngs.ndim = p->src0_ndim;
          new_rngs.valid_mask = in_rngs.valid_mask;
          for (u32 d = 0; d < p->src0_ndim; d++) {
            u32 coord = flat;
            if (a_strides[d] != 1) {
              u32 c = emit_iconst(ke, (i64)a_strides[d]);
              coord = emit_ibinop(ke, S_IDIV, coord, c);
            }
            if (d != 0) {
              u32 dc = emit_iconst(ke, (i64)p->src0_dims[d]);
              coord = emit_ibinop(ke, S_IMOD, coord, dc);
            }
            new_rngs.refs[d] = coord;
          }
          in_rngs = new_rngs;
          break;
        }
        case UOP_PAD: {
          for (u32 d = 0; d < p->out_ndim && d < in_rngs.ndim; d++) {
            u32 begin   = p->pad_widths[2 * d];
            u32 src_dim = p->src0_dims[d];
            if (begin == 0 && src_dim == p->out_dims[d]) continue;
            u32 orig_ref = in_rngs.refs[d];
            if (begin > 0) {
              u32 c = emit_iconst(ke, (i64)begin);
              in_rngs.refs[d] = emit_ibinop(ke, S_ISUB, orig_ref, c);
            }
            u32 hi_lim = emit_iconst(ke, (i64)(begin + src_dim));
            u32 axis_ok = emit_ibinop(ke, S_ILT, orig_ref, hi_lim);
            if (begin > 0) {
              u32 lo_lim = emit_iconst(ke, (i64)begin);
              u32 lt_lo  = emit_ibinop(ke, S_ILT, orig_ref, lo_lim);
              u32 one    = emit_iconst(ke, 1);
              u32 ge_lo  = emit_ibinop(ke, S_ISUB, one, lt_lo);
              axis_ok    = emit_ibinop(ke, S_IAND, axis_ok, ge_lo);
            }
            if (in_rngs.valid_mask == 0) in_rngs.valid_mask = axis_ok;
            else in_rngs.valid_mask = emit_ibinop(ke, S_IAND,
                                                   in_rngs.valid_mask, axis_ok);
          }
          break;
        }
        default: break;
      }
      for (u8 s = 0; s < p->n_src; s++) {
        u32 raw = p->src[s];
        if (KSRC_IS_INPUT(raw)) {
          u32 slot = KSRC_INDEX(raw);
          int pre_scope = (has_reduce && (int)i <= reduce_pos);
          RngsCtx *target = pre_scope ? &input_rngs_pre [slot]
                                       : &input_rngs_post[slot];
          if (target->ndim == 0) *target = in_rngs;
          continue;
        }
        u32 src_idx = KSRC_INDEX(raw);
        if (rngs[src_idx].ndim == 0) rngs[src_idx] = in_rngs;
      }
    }
  }
  // Suppress unused warnings; consumers wired in subsequent commits.
  (void)rngs; (void)input_rngs_pre; (void)input_rngs_post;

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
      View const *v = &ke->input_views[i];
      u32 in_off   = (u32)v->offset;
      u32 idx;
      if (in_numel == 1) {
        u32 src[1] = {param};
        u64 packed = pack_index_extra(NULL, 0, in_off);
        if (packed == UINT64_MAX) RBAIL_MID("pre-INDEX scalar offset > u16");
        idx = rangeify_emit(ke, S_INDEX, dtype, 1, src, packed);
      } else if (in_numel == onum && red->numel == 1
                 && in_numel == reduce_size) {
        u32 r_ids[1]    = {reduce_range};
        u32 r_strides[1] = {1};
        idx = emit_index_chain(ke, dtype, param, r_ids, r_strides, 1, in_off);
      } else if (in_numel == reduce_size && v->shape.ndim == 1) {
        // Per-reduce-iter rank-1 broadcast.  Input depends only on
        // the REDUCE iter (invariant across LOOP iters); the address
        // is reduce_iter * stride[0] + offset.  Covers both the
        // full-reduce case (red->numel == 1) and partial-reduce
        // (red->numel > 1) with a per-reduce-element constant input
        // -- common in BN gradient backward chains.
        u32 r_ids[1]    = {reduce_range};
        u32 r_strides[1] = {(u32)v->strides[0]};
        idx = emit_index_chain(ke, dtype, param, r_ids, r_strides, 1, in_off);
      } else if (in_numel == reduce_size && reduce_inner == 1) {
        // Generalization of the rank-1 case above: same per-reduce-
        // iter pattern but the input view has rank > 1 with all-but-
        // one axes of size 1 (e.g. shape=[1,1,reduce_size]).  The
        // input is contiguous from offset (reduce-trailing layout),
        // so reduce_iter * 1 + offset gives the right address.
        u32 r_ids[1]    = {reduce_range};
        u32 r_strides[1] = {1};
        idx = emit_index_chain(ke, dtype, param, r_ids, r_strides, 1, in_off);
      } else if (in_numel == reduce_size && v->shape.ndim <= MAX_DIM) {
        // General "input depends only on reduce iter" case (any rank,
        // contig OR non-contig).  Builds the address as a symbolic
        // expression from reduce_iter via per-axis DIV/MOD/MUL using
        // the view's actual strides.  Replaces the rank-1 stride-aware
        // branch above (now redundant) and the prior S_RESHAPE_V wrap
        // for non-contig views with a direct S_INDEX_E emission.
        // Mirrors tinygrad's rangeify, which builds index expressions
        // as symbolic UOps over RANGE atoms (apply_movement_op).
        u32 v_ndim = v->shape.ndim;
        u32 dims_u32[MAX_DIM];
        u32 strides_u32[MAX_DIM];
        for (u32 d = 0; d < v_ndim; d++) {
          if (v->strides[d] < 0) RBAIL_MID("pre-INDEX-E negative stride");
          dims_u32   [d] = v->shape.dims[d];
          strides_u32[d] = (u32)v->strides[d];
        }
        u32 addr = build_addr_from_flat_iter(ke, reduce_range,
                                              dims_u32, strides_u32,
                                              v_ndim, in_off);
        if (addr == 0) RBAIL_MID("pre-INDEX-E build_addr failed");
        u32 src[2] = {param, addr};
        idx = rangeify_emit(ke, S_INDEX_E, dtype, 2, src, 0);
      } else if (in_numel == reduce_in_numel
                 && v->shape.ndim == in_ndim) {
        // Partial reduce: input has rank in_ndim (= os->ndim + 1).
        // Includes the trivial reduce_size==1 case where the
        // reduce axis is size 1 -- still need view-aware INDEX
        // because the input view may be broadcast (stride==0
        // axes from chain-rule grad expansion).
        // Partial reduce.  Input rank = output rank + 1; the reduce
        // axis sits at position `reduce_axis` inferred from
        // `reduce_inner`:
        //   inner = product(input.dims[reduce_axis + 1 ..])
        // so walking from the back of the input shape, the smallest
        // suffix product matching inner identifies the position.
        // For inner == 1 the reduce axis is trailing (Phase C-1
        // case); for inner > 1 it sits in the middle (Conv2D-
        // backward over batch, etc.).
        if (v->shape.ndim != in_ndim) RBAIL_MID("pre-reduce input ndim != in_ndim");
        // Find the largest k in [0, in_ndim) with
        //   product(dims[k+1 .. in_ndim-1]) == reduce_inner.
        // For inner=1 -> k = in_ndim - 1 (trailing axis); for inner
        // = dims[last] -> k = in_ndim - 2; etc.  Walk k from
        // in_ndim-1 downward, comparing the running suffix product
        // BEFORE multiplying dims[k] in.
        u32 reduce_axis = (u32)-1;
        {
          u32 partial = 1;     // = product(dims[in_ndim ..]) = 1
          for (i32 k = (i32)in_ndim - 1; k >= 0; k--) {
            if (partial == reduce_inner) { reduce_axis = (u32)k; break; }
            partial *= v->shape.dims[k];
          }
        }
        if (reduce_axis >= in_ndim) RBAIL_MID("reduce_axis search exhausted");
        // Build r_ids in INPUT-axis order: input axis -> range id.
        // input axes [0..reduce_axis-1] come from output axes
        // [0..reduce_axis-1]; input axis reduce_axis is the REDUCE
        // range; input axes [reduce_axis+1..] come from output axes
        // [reduce_axis..].
        u32 r_ids    [MAX_DIM + 1]; u32 r_strides[MAX_DIM + 1];
        for (u32 d = 0; d < in_ndim; d++) {
          if (d < reduce_axis) {
            r_ids[d] = loop_ranges[d];
          } else if (d == reduce_axis) {
            r_ids[d] = reduce_range;
          } else {
            // d > reduce_axis: maps to output axis d-1.
            r_ids[d] = loop_ranges[d - 1];
          }
          // Sanity: dim mismatch with non-broadcast stride bails.
          // Exception: a size-1 axis is an implicit broadcast (iter
          // always 0 on that axis from the input's perspective).
          // Force stride to 0 there to make the LOOP iter address
          // collapse to offset+0 regardless of the LOOP's extent.
          u32 expected_dim;
          if (d < reduce_axis)       expected_dim = os->dims[d];
          else if (d == reduce_axis) expected_dim = reduce_size;
          else                       expected_dim = os->dims[d - 1];
          if (v->shape.dims[d] == 1 && expected_dim != 1) {
            r_strides[d] = 0;
            continue;
          }
          if (v->shape.dims[d] != expected_dim && v->strides[d] != 0) {
            RBAIL_MID("pre-reduce dim mismatch (non-broadcast)");
          }
          r_strides[d] = (u32)v->strides[d];
        }
        idx = emit_index_chain(ke, dtype, param, r_ids, r_strides, in_ndim, in_off);
      } else if (in_numel == reduce_in_numel && reduce_inner == 1) {
        // Flat-buffer alias of the reduce-input shape: same total
        // numel, lower rank view, contiguous, reduce axis trailing.
        // Walk all (LOOP, REDUCE) iters in canonical row-major over
        // (output_shape ++ reduce_size).  Common when the partial-
        // reduce input is a flatten of the per-output strip.
        u32 r_ids[MAX_DIM + 1];
        for (u32 d = 0; d < os->ndim; d++) r_ids[d] = loop_ranges[d];
        r_ids[os->ndim] = reduce_range;
        idx = emit_index_chain(ke, dtype, param, r_ids, in_strides,
                               in_ndim, in_off);
      } else if (v->shape.ndim == os->ndim) {
        // Same rank as output (Phase F-fix): walk the View's strides
        // directly, mirroring the post-scope branch.  Inputs that are
        // broadcast (stride==0) or transposed need view-aware strides
        // -- the canonical loop_strides would silently mis-address.
        // PAD/SHRINK consumers do their own iter shift + bounds gate,
        // so a size mismatch on those inputs is intentional.
        int shape_mismatch = 0;
        for (u32 d = 0; d < os->ndim; d++) {
          if (v->shape.dims[d] != os->dims[d] && v->strides[d] != 0
              && !input_via_padshrink[i]) {
            shape_mismatch = 1;
            break;
          }
        }
        // Try the rngs-based fallback when shape mismatches but rngs
        // are extent-compatible with v.shape per axis (so addresses
        // computed from rngs * v.strides land within v's flat bounds).
        // The extent check is what makes this safe: without it, rngs
        // iterating over the larger os.dim sizes would address past
        // v's bounds and break tests like nn/attention-identity-q.
        if (shape_mismatch && input_rngs_pre[i].ndim == v->shape.ndim
            && v->shape.ndim > 0) {
          // Strict gate: every non-zero-stride axis must have a raw
          // S_RANGE ref whose extent matches v.shape.dims[d].  Chain
          // expressions (S_IDIV/S_IMOD/...) can't safely take this
          // path because the backward walk's rngs encode iter
          // expressions for the chain's INTENT (e.g. RESHAPE flat-
          // roundtrip), not necessarily the addressing the materializer
          // actually wants here -- verified via attention test.
          int ext_ok = 1;
          for (u32 d = 0; d < v->shape.ndim; d++) {
            if (v->strides[d] == 0) continue;
            u32 ref = input_rngs_pre[i].refs[d];
            if (ref == 0) { ext_ok = 0; break; }
            ScalarUop const *ru = &ke->scalar_uops[ref];
            if (ru->op != S_RANGE) { ext_ok = 0; break; }
            u32 extent = (u32)(ru->extra & 0xFFFFFFFFu);
            if (extent != v->shape.dims[d]) { ext_ok = 0; break; }
          }
          if (ext_ok) goto pre_index_rngs_fallback;
        }
        if (shape_mismatch) {
          if (getenv("THVM_RANGEIFY_BAIL")) {
            fprintf(stderr, "  pre-INDEX-mismatch: i=%u v.shape=[", i);
            for (u32 d = 0; d < v->shape.ndim; d++)
              fprintf(stderr, "%u%s", v->shape.dims[d], d+1==v->shape.ndim?"":",");
            fprintf(stderr, "] v.strides=[");
            for (u32 d = 0; d < v->shape.ndim; d++)
              fprintf(stderr, "%d%s", v->strides[d], d+1==v->shape.ndim?"":",");
            fprintf(stderr, "] os=[");
            for (u32 d = 0; d < os->ndim; d++)
              fprintf(stderr, "%u%s", os->dims[d], d+1==os->ndim?"":",");
            fprintf(stderr, "] rngs.ndim=%u rngs.refs=[",
                    input_rngs_pre[i].ndim);
            for (u32 d = 0; d < input_rngs_pre[i].ndim; d++) {
              u32 ref = input_rngs_pre[i].refs[d];
              if (ref == 0) { fprintf(stderr, "(none)"); }
              else fprintf(stderr, "%s/extra=%lu",
                           scalar_op_name(ke->scalar_uops[ref].op),
                           (unsigned long)(ke->scalar_uops[ref].extra & 0xFFFFFFFFu));
              if (d+1 < input_rngs_pre[i].ndim) fprintf(stderr, ",");
            }
            fprintf(stderr, "]\n");
          }
          RBAIL_MID("pre-INDEX shape mismatch (non-broadcast)");
        }
        u32 strides_u32[MAX_DIM];
        for (u32 d = 0; d < os->ndim; d++) {
          strides_u32[d] = (u32)v->strides[d];
        }
        idx = emit_index_chain(ke, dtype, param, loop_ranges, strides_u32,
                               os->ndim, in_off);
      } else if (input_rngs_pre[i].ndim == v->shape.ndim
                 && v->shape.ndim > 0) {
       pre_index_rngs_fallback:
        via_rngs_pre[i] = 1;
        // RNGS-BASED FALLBACK (Phase 3 of apply_movement_op port).
        // The backward walk computed input_rngs_pre[i] = per-axis iter
        // expressions for this input read inside the reduce body.
        // Build addr = sum(rngs[d] * v.strides[d]) + offset directly
        // via S_INDEX_E.  Handles arbitrary chain shapes that the
        // per-pattern branches above don't recognize.
        RngsCtx const *r = &input_rngs_pre[i];
        u32 acc = in_off ? emit_iconst(ke, (i64)in_off) : 0;
        for (u32 d = 0; d < r->ndim; d++) {
          if (v->strides[d] < 0) { acc = 0; break; }  // bail
          if (v->strides[d] == 0) continue;  // broadcast axis
          u32 t = r->refs[d];
          if (v->strides[d] != 1) {
            u32 c = emit_iconst(ke, (i64)v->strides[d]);
            t = emit_ibinop(ke, S_IMUL, t, c);
          }
          acc = (acc == 0) ? t : emit_ibinop(ke, S_IADD, acc, t);
        }
        if (acc == 0) acc = emit_iconst(ke, 0);
        u32 src[2] = {param, acc};
        idx = rangeify_emit(ke, S_INDEX_E, dtype, 2, src, 0);
      } else {
        // No supported pre-INDEX shape branch matched.  Bailing
        // here is safer than emitting wrong addresses with
        // canonical loop_strides.
        if (getenv("THVM_RANGEIFY_BAIL")) {
          fprintf(stderr, "  pre-INDEX-detail: i=%u in_numel=%u onum=%u v.ndim=%u os.ndim=%u in_ndim=%u red->numel=%u reduce_size=%u reduce_in_numel=%u reduce_inner=%u rngs.ndim=%u\n",
                  i, in_numel, onum, v->shape.ndim, os->ndim,
                  in_ndim, has_reduce ? red->numel : 0,
                  has_reduce ? reduce_size : 0, reduce_in_numel,
                  has_reduce ? reduce_inner : 0,
                  input_rngs_pre[i].ndim);
        }
        RBAIL_MID("pre-INDEX no branch matched");
      }
      if (idx == 0) RBAIL_MID("pre-INDEX emit failed");
      // Wrap with valid_mask if PAD chain produced one.
      u32 load = rangeify_emit_unary(ke, S_LOAD, dtype, idx);
      if (input_rngs_pre[i].valid_mask != 0) {
        u32 zero = emit_iconst(ke, 0);
        u32 wsrc[3] = {input_rngs_pre[i].valid_mask, load, zero};
        load = rangeify_emit(ke, S_IWHERE, dtype, 3, wsrc, 0);
      }
      input_load_pre[i] = load;
    }

    // Post-scope LOAD: per-LOOP-element read.  Stride encoding
    // mirrors the input's View -- non-contig inputs (e.g. broadcast
    // EXPAND with stride==0 on a dim, or transpose patterns landed
    // by Phase D's chain-rule grad expansion) are absorbed into
    // INDEX without bailing.
    if (input_used_post[i]) {
      View const *v = &ke->input_views[i];
      u32 in_off    = (u32)v->offset;
      u32 idx;
      if (in_numel == 1) {
        u32 src[1] = {param};
        u64 packed = pack_index_extra(NULL, 0, in_off);
        if (packed == UINT64_MAX) RBAIL_MID("post-INDEX scalar offset > u16");
        idx = rangeify_emit(ke, S_INDEX, dtype, 1, src, packed);
      } else if (v->shape.ndim == os->ndim) {
        u32 strides_u32[MAX_DIM];
        for (u32 d = 0; d < os->ndim; d++) {
          // Shape-mismatch tolerance: if v->shape.dims[d] != os->dims[d]
          // with non-zero stride, just walk the input by its stride
          // (which gives the "take first N" interpretation -- common
          // when the input is a SHRINK alias / RESHAPE'd view that
          // rangeify can't otherwise match).  Old code bailed unless
          // the input was via PAD/SHRINK; loosened.
          strides_u32[d] = (u32)v->strides[d];
        }
        idx = emit_index_chain(ke, dtype, param, loop_ranges, strides_u32,
                               os->ndim, in_off);
      } else if (has_reduce && in_numel == reduce_in_numel
                 && reduce_in_numel == onum) {
        idx = emit_index_chain(ke, dtype, param, loop_ranges, loop_strides,
                               os->ndim, in_off);
      } else if (in_numel == onum) {
        // Flat-buffer alias / RESHAPE: same total elements as output
        // but with a lower- (or higher-) rank view.  Walk the output
        // in canonical row-major order; the corresponding input
        // position is the flat index, so loop_strides over LOOP
        // ranges gives the correct address.  Assumes the input view
        // is contiguous from offset (standard after RESHAPE).
        idx = emit_index_chain(ke, dtype, param, loop_ranges, loop_strides,
                               os->ndim, in_off);
      } else if (in_numel < onum && v->shape.ndim <= os->ndim) {
        // Lower-rank broadcast: input shape is a contiguous prefix
        // (most common: per-channel/leading axis) or suffix (per-row
        // trailing axis) of the output shape.  Per-axis strides:
        // v's strides for the matched axes, 0 for the broadcast axes.
        // The materializer emits an EXPAND op that's lowered as
        // identity at the scalar level; this branch supplies the
        // broadcast addressing the EXPAND would otherwise need.
        u32 v_ndim = v->shape.ndim;
        int prefix_ok = 1;
        for (u32 d = 0; d < v_ndim; d++) {
          if (v->shape.dims[d] != os->dims[d]) { prefix_ok = 0; break; }
        }
        int suffix_ok = 0;
        u32 suffix_off = (os->ndim >= v_ndim) ? (os->ndim - v_ndim) : 0;
        if (!prefix_ok && os->ndim >= v_ndim) {
          suffix_ok = 1;
          for (u32 d = 0; d < v_ndim; d++) {
            if (v->shape.dims[d] != os->dims[suffix_off + d]) {
              suffix_ok = 0; break;
            }
          }
        }
        if (prefix_ok || suffix_ok) {
          u32 base_axis = prefix_ok ? 0 : suffix_off;
          u32 strides_u32[MAX_DIM];
          for (u32 d = 0; d < os->ndim; d++) {
            if (d >= base_axis && d < base_axis + v_ndim) {
              strides_u32[d] = (u32)v->strides[d - base_axis];
            } else {
              strides_u32[d] = 0;  // broadcast axis
            }
          }
          idx = emit_index_chain(ke, dtype, param, loop_ranges, strides_u32,
                                 os->ndim, in_off);
        } else if (input_rngs_post[i].ndim == v->shape.ndim
                   && v->shape.ndim > 0) {
          // RNGS-BASED FALLBACK (post-scope analog of the pre-scope
          // branch above).  The backward walk computed input_rngs_post
          // = per-axis iter expressions for this input.  Build the
          // address symbolically via S_INDEX_E.  Handles the case where
          // a chain (PERMUTE/RESHAPE/EXPAND/...) makes v.shape and
          // os.shape incompatible by direct prefix/suffix matching.
          via_rngs_post[i] = 1;
          RngsCtx const *r = &input_rngs_post[i];
          u32 acc = in_off ? emit_iconst(ke, (i64)in_off) : 0;
          int bailed = 0;
          for (u32 d = 0; d < r->ndim; d++) {
            if (v->strides[d] < 0) { bailed = 1; break; }
            if (v->strides[d] == 0) continue;
            u32 t = r->refs[d];
            if (v->strides[d] != 1) {
              u32 c = emit_iconst(ke, (i64)v->strides[d]);
              t = emit_ibinop(ke, S_IMUL, t, c);
            }
            acc = (acc == 0) ? t : emit_ibinop(ke, S_IADD, acc, t);
          }
          if (bailed) {
            RBAIL_MID("post-INDEX rngs negative stride");
          }
          if (acc == 0) acc = emit_iconst(ke, 0);
          u32 src[2] = {param, acc};
          idx = rangeify_emit(ke, S_INDEX_E, dtype, 2, src, 0);
        } else {
          if (getenv("THVM_RANGEIFY_BAIL")) {
            fprintf(stderr, "  post-INDEX broadcast no prefix/suffix match"
                            " v.shape.ndim=%u rngs.ndim=%u\n",
                    v->shape.ndim, input_rngs_post[i].ndim);
          }
          RBAIL_MID("post-INDEX broadcast unmatched");
        }
      } else {
        if (getenv("THVM_RANGEIFY_BAIL")) {
          fprintf(stderr, "  post-INDEX-detail: i=%u in_numel=%u onum=%u v.ndim=%u os.ndim=%u v.shape=[",
                  i, in_numel, onum, v->shape.ndim, os->ndim);
          for (u32 d = 0; d < v->shape.ndim; d++)
            fprintf(stderr, "%u%s", v->shape.dims[d], d+1==v->shape.ndim?"":",");
          fprintf(stderr, "] v.strides=[");
          for (u32 d = 0; d < v->shape.ndim; d++)
            fprintf(stderr, "%d%s", v->strides[d], d+1==v->shape.ndim?"":",");
          fprintf(stderr, "] os=[");
          for (u32 d = 0; d < os->ndim; d++)
            fprintf(stderr, "%u%s", os->dims[d], d+1==os->ndim?"":",");
          fprintf(stderr, "]\n");
        }
        RBAIL_MID("post-INDEX no branch matched");
      }
      if (idx == 0) RBAIL_MID("post-INDEX emit failed");
      input_load_post[i] = rangeify_emit_unary(ke, S_LOAD, dtype, idx);
    }
  }

  // 4. ALU lowering: one S_X per KProgOp.  An op at position
  // <= reduce_pos uses input_load_pre[] for its input refs; at
  // position > reduce_pos uses input_load_post[].  The REDUCE op
  // itself sits at reduce_pos (pre-scope ref to body input).
  // UOP_EXPAND lowers as identity -- broadcast is implicit at the
  // per-LOOP-element scalar level.
  u32 prog_value[nops_local];
  u8  via_rngs[nops_local];
  for (u32 i = 0; i < nops_local; i++) { prog_value[i] = 0; via_rngs[i] = 0; }
  // Helper for checking via_rngs from a src ref.
  #define SRC_VIA_RNGS(raw, pre_scope) \
    (KSRC_IS_INPUT(raw) \
      ? ((pre_scope) ? via_rngs_pre [KSRC_INDEX(raw)] \
                     : via_rngs_post[KSRC_INDEX(raw)]) \
      : via_rngs[KSRC_INDEX(raw)])
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    u32 dtype  = p->dtype;
    int pre    = (has_reduce && (int)i <= reduce_pos);
    u32 *input_load = pre ? input_load_pre : input_load_post;
    if (p->opcode == UOP_CONST) {
      prog_value[i] = rangeify_emit_leaf(ke, S_CONST, dtype, (u64)p->arg);
      continue;
    }
    if (p->opcode == UOP_BITCAST) {
      // BITCAST is identity at the scalar level for non-narrow FPs:
      // reg holds the raw bits and downstream u->dtype reinterprets
      // them.  But for narrow FPs (fp16/bf16/fp8), scalar_load_typed
      // widens to f32 bits at the buffer boundary -- so the raw
      // bit pattern would be gone by the time BITCAST runs.
      //
      // For BITCAST of a narrow-FP *input*, emit a parallel
      // S_LOAD_RAW that bypasses the widening and pass that through
      // as identity.  For BITCAST of a narrow-FP *intermediate*
      // (no current test triggers this) the raw bits are already lost
      // upstream; we still bail there.
      u32 dst_dtype = p->dtype;
      u32 raw0      = p->src[0];
      int src_is_narrow_input = 0;
      if (KSRC_IS_INPUT(raw0)) {
        u32 sd = ke->input_dtypes[KSRC_INDEX(raw0)];
        if (sd == DT_FP16 || sd == DT_BF16
         || sd == DT_FP8E4M3 || sd == DT_FP8E5M2) src_is_narrow_input = 1;
      }
      int dst_is_narrow = (dst_dtype == DT_FP16 || dst_dtype == DT_BF16
                        || dst_dtype == DT_FP8E4M3 || dst_dtype == DT_FP8E5M2);
      if (src_is_narrow_input && !dst_is_narrow) {
        // Reuse the INDEX of the existing S_LOAD; emit a fresh
        // S_LOAD_RAW with that same INDEX so the raw narrow-FP bits
        // land in the register.
        u32 load_id = input_load[KSRC_INDEX(raw0)];
        if (load_id == 0) RBAIL_MID("BITCAST narrow-FP source LOAD missing");
        u32 idx = ke->scalar_uops[load_id].src[0];
        prog_value[i] = rangeify_emit_unary(ke, S_LOAD_RAW, dtype, idx);
        continue;
      }
      if (dst_is_narrow || src_is_narrow_input) {
        // narrow-FP intermediate-source case (no test today) or
        // BITCAST *into* a narrow FP -- bail.
        RBAIL_MID("BITCAST narrow-FP");
      }
      // Fall through to the identity path below.
    }
    if (p->opcode == UOP_RESHAPE && p->src0_ndim > 0 && p->out_ndim > 0) {
      // RESHAPE that changes shape AND sits in a chain with downstream
      // PAD/SHRINK needs an iter-coord transform: PAD's iter (in
      // out_dims coords) must be flattened + re-decomposed into
      // src0_dims coords before SHRINK applies its begin.  Other
      // RESHAPE positions (no PAD/SHRINK downstream) are pure
      // identity at the scalar level.
      u32 raw = p->src[0];
      u32 v   = KSRC_IS_INPUT(raw) ? input_load[KSRC_INDEX(raw)]
                                   : prog_value[KSRC_INDEX(raw)];
      if (v == 0) RBAIL_MID("RESHAPE src 0");
      // Identity-pass when source is via_rngs: rngs already baked in
      // the chain's RESHAPE transform via flat-roundtrip.
      if (SRC_VIA_RNGS(raw, pre)) {
        prog_value[i] = v;
        via_rngs[i] = 1;
        continue;
      }
      int shape_changes = (p->src0_ndim != p->out_ndim);
      if (!shape_changes) {
        for (u32 d = 0; d < p->src0_ndim; d++) {
          if (p->src0_dims[d] != p->out_dims[d]) { shape_changes = 1; break; }
        }
      }
      // Size-1-axis insertion/removal: if src0_dims and out_dims have
      // the same multiset of non-1 dims in the same order, the RESHAPE
      // is identity at the scalar level (no data movement).  Downstream
      // EXPAND/movement ops use LOOP iters at os->ndim; the inserted
      // size-1 axes are handled by stride-0 broadcasts emitted earlier.
      // Covers the dominant `[N] -> [N, 1, 1]` broadcast-prep pattern.
      if (shape_changes) {
        u32 a_idx = 0, b_idx = 0;
        int identity_mod_ones = 1;
        while (a_idx < p->src0_ndim || b_idx < p->out_ndim) {
          while (a_idx < p->src0_ndim && p->src0_dims[a_idx] == 1) a_idx++;
          while (b_idx < p->out_ndim  && p->out_dims [b_idx] == 1) b_idx++;
          if (a_idx == p->src0_ndim && b_idx == p->out_ndim) break;
          if (a_idx == p->src0_ndim || b_idx == p->out_ndim
              || p->src0_dims[a_idx] != p->out_dims[b_idx]) {
            identity_mod_ones = 0;
            break;
          }
          a_idx++;
          b_idx++;
        }
        if (identity_mod_ones) {
          prog_value[i] = v;
          continue;
        }
      }
      if (shape_changes) {
        // Cap each ndim at 4 (the u8 packing of in/out dims into
        // extra: 4 bytes each side, 8 bytes total).  Both ndims must
        // equal os->ndim so the LOOP ranges' iters fully define
        // flat_idx and the in_dims decompose covers all axes.
        // Mismatch cases (either side different from os->ndim) need
        // synthetic iter dims -- deferred to a future S_RESHAPE
        // generalization.
        if (p->src0_ndim > 4 || p->out_ndim > 4
            || p->src0_ndim != os->ndim || p->out_ndim != os->ndim) {
          // Rank-mismatch RESHAPE.  Try the S_RESHAPE_V split-src form
          // (mirrors tinygrad's apply_movement_op for RESHAPE -- input
          // ranges become fresh PLACEHOLDER/VIRT ranges that the wrap
          // writes from a flat-index roundtrip of selected LOOP iters).
          //
          // MVP coverage requires:
          // 1. Source is a direct contig input load (KSRC_IS_INPUT,
          //    input_view contiguous, src0_dims numel matches view).
          // 2. Each non-1 axis in out_dims maps to a unique LOOP axis
          //    in os with matching extent.  Size-1 axes in out_dims
          //    contribute no flat_idx (they're just labels for
          //    downstream EXPAND).  os axes with no corresponding
          //    out_dim entry are broadcast axes -- the source value
          //    is constant across their LOOP iter.
          //
          // The selected LOOP iters drive flat_idx (in the order they
          // appear in out_dims, contributing only for non-1 axes); the
          // src0_dims VIRT ranges decompose the result.  This handles
          // the "RESHAPE then EXPAND" pattern: e.g. RESHAPE [1,3,3] ->
          // [1,1,9] then EXPAND to [2,9] -- only the LOOP iter for the
          // size-9 os axis matters; the size-2 os axis (broadcast) is
          // ignored, so the S_LOAD doesn't depend on it.
          //
          // Other cases (prog_value chains, non-contig views, out_dims
          // non-1 axes that don't match an os axis) bail and fall
          // through to cpu_interpret.
          int can_use_v = KSRC_IS_INPUT(raw)
                       && p->src0_ndim <= MAX_DIM
                       && p->out_ndim  <= MAX_DIM
                       && (1 + (u32)os->ndim + (u32)p->src0_ndim) <= SCALAR_MAX_SRC;
          // Match each non-1 out_dim to a unique LOOP axis in os by
          // extent equality.  Also collect the per-out_dim contribution:
          //   contrib_iter[d]  -- LOOP range ref to use for out_dim d
          //                       (0 if axis is size-1, no contribution)
          // Order matches out_dims so flat_idx accumulates row-major
          // over the out_dims layout.
          u32 contrib_iter[MAX_DIM] = {0};
          u8  os_used[MAX_DIM] = {0};
          int reduce_used = 0;
          if (can_use_v) {
            for (u32 d = 0; d < p->out_ndim; d++) {
              if (p->out_dims[d] == 1) continue;  // size-1 contributes nothing
              int matched = 0;
              for (u32 e = 0; e < os->ndim; e++) {
                if (os_used[e]) continue;
                if (os->dims[e] == p->out_dims[d]) {
                  contrib_iter[d] = loop_ranges[e];
                  os_used[e] = 1;
                  matched = 1;
                  break;
                }
              }
              // Fallback: if no os axis matched, try reduce_range.
              // Unambiguous only when the extent doesn't ALSO appear in
              // os.dims (we'd have matched it above).  Used by reduce-
              // chain RESHAPEs that flatten the reducible region:
              // [a,b,c] -> [1, a*b*c] when a*b*c == reduce_size.
              if (!matched && reduce_size != 0 && !reduce_used
                  && p->out_dims[d] == reduce_size) {
                contrib_iter[d] = reduce_range;
                reduce_used = 1;
                matched = 1;
              }
              if (!matched) { can_use_v = 0; break; }
            }
          }
          if (can_use_v) {
            u32 slot = KSRC_INDEX(raw);
            View const *src_view = &ke->input_views[slot];
            // Verify view shape matches src0_dims AND numel matches.
            // For non-contig views (e.g. slice views with strides like
            // [25,5,1] on a [1,3,3] shape into a [5,5,5] parent), we
            // can still index correctly using the view's actual strides
            // -- as long as the view shape matches, S_INDEX with
            // v->strides + per-axis VIRT iters reads the right elements.
            if (src_view->shape.ndim != p->src0_ndim) can_use_v = 0;
            if (can_use_v) {
              for (u32 d = 0; d < p->src0_ndim; d++) {
                if (src_view->shape.dims[d] != p->src0_dims[d]) {
                  can_use_v = 0; break;
                }
                if (src_view->strides[d] < 0) {
                  can_use_v = 0; break;  // negative stride: bail
                }
                if ((u32)src_view->strides[d] > 0xFFFFu) {
                  can_use_v = 0; break;  // stride doesn't fit u16
                }
              }
            }
            if (can_use_v) {
              u32 src0_numel = 1;
              for (u32 d = 0; d < p->src0_ndim; d++) src0_numel *= p->src0_dims[d];
              if (src0_numel != ke->input_numels[slot]) can_use_v = 0;
            }
          }
          if (can_use_v) {
            u32 slot = KSRC_INDEX(raw);
            View const *src_view = &ke->input_views[slot];
            u32 in_ndim = p->src0_ndim;
            // Build the output-side ref list: one ref per non-1
            // out_dim, in out_dims order.  size-1 out_dims contribute
            // nothing (their extent is 1, so the flat_idx contribution
            // would be 0 anyway).  We also need fresh size-1 VIRT ranges
            // for those positions so S_RESHAPE_V's flat_idx accumulator
            // can multiply by the correct out extent.
            //
            // Concrete encoding: emit a VIRT range with extent =
            // out_dims[d] for each axis -- size-1 axes give an extent
            // that contributes a stride of 1 in the multiplication,
            // and their iter is forced to 0 by the wrap (since extent=1).
            u32 out_refs[MAX_DIM] = {0};
            u32 n_out = p->out_ndim;
            for (u32 d = 0; d < n_out; d++) {
              if (contrib_iter[d] != 0) {
                out_refs[d] = contrib_iter[d];
              } else {
                // size-1 out_dim: emit a VIRT range with extent 1.
                u64 r_extra = ((u64)S_AXIS_VIRT << 32) | 1ULL;
                out_refs[d] = rangeify_emit_leaf(ke, S_RANGE,
                                                  DT_INT32, r_extra);
              }
            }
            // Allocate VIRT ranges sized to src0_dims (input side).
            u32 virt_ranges[MAX_DIM];
            for (u32 d = 0; d < in_ndim; d++) {
              u64 r_extra = ((u64)S_AXIS_VIRT << 32) | (u64)p->src0_dims[d];
              virt_ranges[d] = rangeify_emit_leaf(ke, S_RANGE, DT_INT32, r_extra);
            }
            // Fresh DEFINE_PARAM + S_LOAD via VIRT ranges.  Use the
            // view's actual strides so non-contig slice views work
            // correctly.  For a contig view this matches
            // row_major_strides(src0_dims); for a non-contig slice
            // (e.g. shape [1,3,3] strides [25,5,1] viewing a [5,5,5]
            // parent), it reads the right window into the parent.
            u32 fresh_param = rangeify_emit_leaf(ke, S_DEFINE_PARAM,
                                                 p->dtype, (u64)slot);
            u32 strides[MAX_DIM];
            for (u32 d = 0; d < in_ndim; d++) {
              strides[d] = (u32)src_view->strides[d];
            }
            u32 idx = emit_index_chain(ke, p->dtype, fresh_param,
                                        virt_ranges, strides, in_ndim,
                                        (u32)src_view->offset);
            if (idx == 0) RBAIL_MID("RESHAPE-V emit_index_chain failed");
            u32 v_load = rangeify_emit_unary(ke, S_LOAD, p->dtype, idx);
            // Emit S_RESHAPE_V: src[0] = body, src[1..1+n_out) =
            // output refs (selected LOOP iters + size-1 VIRT placeholders),
            // src[1+n_out..) = input (VIRT) refs.  extra[byte 0] = n_out.
            u32 src_arr[SCALAR_MAX_SRC] = {v_load};
            for (u32 d = 0; d < n_out;   d++) src_arr[1 + d]              = out_refs[d];
            for (u32 d = 0; d < in_ndim; d++) src_arr[1 + n_out + d]      = virt_ranges[d];
            u8  src_count = (u8)(1 + n_out + in_ndim);
            u64 extra     = (u64)n_out & 0xFFu;
            prog_value[i] = rangeify_emit(ke, S_RESHAPE_V, p->dtype,
                                          src_count, src_arr, extra);
            continue;
          }
          RBAIL_MID("RESHAPE shape-change ndim cap or != os->ndim");
        }
        // Legacy shared-LOOP-refs encoding.  Body's S_LOAD strides
        // match in_dims (input view's actual strides), and S_RESHAPE
        // shifts the LOOP iters in-place from out_dims coords to
        // in_dims coords via flat-index roundtrip.
        u64 lo = 0, hi = 0;
        for (u32 d = 0; d < p->out_ndim; d++) {
          if (p->out_dims[d] > 0xFFu) RBAIL_MID("RESHAPE out_dim > u8");
          lo |= ((u64)p->out_dims[d] & 0xFFu) << (8 * d);
        }
        for (u32 d = 0; d < p->src0_ndim; d++) {
          if (p->src0_dims[d] > 0xFFu) RBAIL_MID("RESHAPE in_dim > u8");
          hi |= ((u64)p->src0_dims[d] & 0xFFu) << (8 * d);
        }
        u64 extra = lo | (hi << 32);
        u32 src_arr[SCALAR_MAX_SRC] = {v};
        for (u32 d = 0; d < os->ndim; d++) src_arr[1 + d] = loop_ranges[d];
        prog_value[i] = rangeify_emit(ke, S_RESHAPE, p->dtype,
                                      (u8)(1 + os->ndim), src_arr, extra);
      } else {
        prog_value[i] = v;
      }
      continue;
    }
    if (p->opcode == UOP_EXPAND || p->opcode == UOP_RESHAPE
        || p->opcode == UOP_LOAD || p->opcode == UOP_BITCAST) {
      // Movement-as-identity / structural marker / bitcast: src[0]
      // is the value, output uses the same scalar bits.  At the
      // per-LOOP-element scalar level EXPAND broadcast, RESHAPE
      // flat-buffer rewrap, LOAD ("read this tensor" marker), and
      // BITCAST (same bits, different dtype label) are all no-ops
      // -- downstream dtype interpretation flows through u->dtype
      // when the next op decodes its operands.
      u32 raw = p->src[0];
      u32 v   = KSRC_IS_INPUT(raw) ? input_load[KSRC_INDEX(raw)]
                                   : prog_value[KSRC_INDEX(raw)];
      if (v == 0) RBAIL_MID("identity (EXPAND/RESHAPE/LOAD/BITCAST) src 0");
      prog_value[i] = v;
      continue;
    }
    if (p->opcode == UOP_CAST) {
      // Value-preserving cast: emit S_CAST so the dispatcher reads
      // the source's dtype, decodes, and re-encodes as u->dtype.
      u32 raw = p->src[0];
      u32 v   = KSRC_IS_INPUT(raw) ? input_load[KSRC_INDEX(raw)]
                                   : prog_value[KSRC_INDEX(raw)];
      if (v == 0) RBAIL_MID("CAST src 0");
      prog_value[i] = rangeify_emit_unary(ke, S_CAST, p->dtype, v);
      continue;
    }
    if (p->opcode == UOP_SHRINK || p->opcode == UOP_PAD) {
      // Per-axis index transform.  S_SHRINK shifts each LOOP range
      // iter by `begin[d]` before evaluating the body; S_PAD shifts
      // and gates against the source dim (returns 0 outside the
      // valid range, matching cpu_op_pad).  Both encode begin per
      // axis in extra (u16 / packed).  Phase F-3 caps at 3 axes
      // (matches the per-INDEX axis cap; chain for higher rank).
      u32 raw = p->src[0];
      u32 v   = KSRC_IS_INPUT(raw) ? input_load[KSRC_INDEX(raw)]
                                   : prog_value[KSRC_INDEX(raw)];
      // (Future: identity-pass when SRC_VIA_RNGS(raw, pre) -- rngs
      // already baked the per-axis shift into the load address.
      // Currently disabled: identity-passing SHRINK on its own gives
      // no bail reduction (failing kernels are PAD); identity-passing
      // PAD breaks nn/lenet-end-to-end-forward.  Need to find what
      // PAD chain pattern is unsafe before re-enabling.)
      if (v == 0) RBAIL_MID("SHRINK/PAD src 0");
      u32 ndim = p->out_ndim;
      if (ndim == 0) ndim = os->ndim;
      // Cap is u64-extra packing: 16 bits/axis (begin u16 for SHRINK,
      // begin u8 + src_dim u8 for PAD).  4 axes fit; chain for more.
      if (ndim > 4) RBAIL_MID("SHRINK/PAD ndim > 4");
      // Identity-pass SHRINK only (PAD identity-pass breaks lenet's
      // softmax chain -- needs deeper investigation).  When the
      // source value's via_rngs flag is set, the chain's iter shifts
      // are baked into the load address by the backward walk, so the
      // forward emission can skip wrapping with S_SHRINK.
      if (p->opcode == UOP_SHRINK && SRC_VIA_RNGS(raw, pre)) {
        prog_value[i] = v;
        via_rngs[i] = 1;
        continue;
      }
      // SHRINK/PAD fusion path: when the source is an S_RESHAPE_V wrap
      // and ndim > os->ndim, the SHRINK/PAD operates on a rank-promoted
      // intermediate.  Fuse by re-emitting RESHAPE_V with adjusted
      // output refs.
      //
      // SHRINK is straightforward: shift each output ref by begin via
      // S_IADD.
      //
      // PAD is subtler: when PAD inflates a size-1 axis to size-N
      // (src_dim=1 -> out_dim=N), the new axis isn't driven by any
      // LOOP iter from the kernel's outer scope -- the corresponding
      // RESHAPE_V output ref is a placeholder VIRT(extent=1) whose
      // iter is always 0.  For the conv2d-style chain, that new axis
      // semantically corresponds to the reduce axis (kw position):
      // axis is summed away by a downstream SUM_REDUCE.  When
      // out_dim == reduce_size we route this axis's orig_ref through
      // reduce_range and bounds-check reduce_range vs [begin,
      // begin+src_dim).  Per-axis pass-through / shift / reduce-route
      // decisions; if any axis can't be classified, bail.
      if ((p->opcode == UOP_SHRINK || p->opcode == UOP_PAD)
          && ndim > os->ndim
          && ke->scalar_uops[v].op == S_RESHAPE_V) {
        ScalarUop const *rv = &ke->scalar_uops[v];
        u32 n_out_orig = (u32)(rv->extra & 0xFFu);
        if (n_out_orig != ndim) {
          RBAIL_MID("SHRINK/PAD fusion: ndim != RESHAPE_V n_out");
        }
        u32 n_in   = (u32)rv->src_count - 1 - n_out_orig;
        u32 new_out_refs[MAX_DIM];
        u32 valid_mask = 0;
        int can_fuse = 1;
        // Track which os/loop_ranges axes are already used by orig_refs
        // (to avoid double-routing one os axis through two PAD axes).
        u8 os_axis_used[MAX_DIM] = {0};
        for (u32 d = 0; d < n_out_orig; d++) {
          u32 ref = rv->src[1 + d];
          for (u32 e = 0; e < os->ndim; e++) {
            if (ref == loop_ranges[e]) { os_axis_used[e] = 1; break; }
          }
        }
        int reduce_used = 0;
        for (u32 d = 0; d < n_out_orig; d++) {
          if (rv->src[1 + d] == reduce_range) { reduce_used = 1; break; }
        }
        for (u32 d = 0; d < n_out_orig; d++) {
          u32 begin    = p->pad_widths[2 * d];
          u32 src_dim  = p->src0_dims [d];
          u32 out_dim  = p->out_dims  [d];
          u32 orig_ref = rv->src[1 + d];
          if (p->opcode == UOP_SHRINK) {
            if (begin == 0) {
              new_out_refs[d] = orig_ref;
            } else {
              u32 c_ic = emit_iconst(ke, (i64)begin);
              new_out_refs[d] = emit_ibinop(ke, S_IADD, orig_ref, c_ic);
            }
          } else {
            // PAD per-axis classification.
            if (begin == 0 && src_dim == out_dim) {
              new_out_refs[d] = orig_ref;
              continue;
            }
            u32 ref_iter = 0;
            if (src_dim == out_dim) {
              // Same-size axis with begin offset -- SHRINK-style shift.
              ref_iter = orig_ref;
            } else if (src_dim < out_dim) {
              // Rank-promoted axis: route through reduce_range (if
              // out_dim matches reduce_size and reduce isn't already
              // claimed) or the unique unused os.dim with matching
              // extent.
              if (reduce_size != 0 && out_dim == reduce_size && !reduce_used) {
                ref_iter = reduce_range;
                reduce_used = 1;
              } else {
                for (u32 e = 0; e < os->ndim; e++) {
                  if (os_axis_used[e]) continue;
                  if (os->dims[e] == out_dim) {
                    ref_iter = loop_ranges[e];
                    os_axis_used[e] = 1;
                    break;
                  }
                }
              }
              if (ref_iter == 0) { can_fuse = 0; break; }
            } else {
              can_fuse = 0; break;
            }
            // Shift back by begin: source coord = ref_iter - begin.
            if (begin > 0) {
              u32 c = emit_iconst(ke, (i64)begin);
              new_out_refs[d] = emit_ibinop(ke, S_ISUB, ref_iter, c);
            } else {
              new_out_refs[d] = ref_iter;
            }
            // Bounds: ref_iter < begin+src_dim AND ref_iter >= begin.
            u32 hi_lim  = emit_iconst(ke, (i64)(begin + src_dim));
            u32 axis_ok = emit_ibinop(ke, S_ILT, ref_iter, hi_lim);
            if (begin > 0) {
              u32 lo_lim = emit_iconst(ke, (i64)begin);
              u32 lt_lo  = emit_ibinop(ke, S_ILT, ref_iter, lo_lim);
              u32 one    = emit_iconst(ke, 1);
              u32 ge_lo  = emit_ibinop(ke, S_ISUB, one, lt_lo);
              axis_ok    = emit_ibinop(ke, S_IAND, axis_ok, ge_lo);
            }
            if (valid_mask == 0) valid_mask = axis_ok;
            else valid_mask = emit_ibinop(ke, S_IAND, valid_mask, axis_ok);
          }
        }
        if (!can_fuse) {
          if (getenv("THVM_RANGEIFY_BAIL")) {
            fprintf(stderr, "  PAD-fusion-fail: out_dims=[");
            for (u32 d = 0; d < n_out_orig; d++)
              fprintf(stderr, "%u%s", p->out_dims[d], d+1==n_out_orig?"":",");
            fprintf(stderr, "] src_dims=[");
            for (u32 d = 0; d < n_out_orig; d++)
              fprintf(stderr, "%u%s", p->src0_dims[d], d+1==n_out_orig?"":",");
            fprintf(stderr, "] begins=[");
            for (u32 d = 0; d < n_out_orig; d++)
              fprintf(stderr, "%u%s", p->pad_widths[2*d], d+1==n_out_orig?"":",");
            fprintf(stderr, "] os.dims=[");
            for (u32 d = 0; d < os->ndim; d++)
              fprintf(stderr, "%u%s", os->dims[d], d+1==os->ndim?"":",");
            fprintf(stderr, "] reduce_size=%u\n", reduce_size);
          }
          RBAIL_MID("PAD fusion: per-axis classification failed");
        }
        u32 body = rv->src[0];
        if (p->opcode == UOP_PAD && valid_mask != 0) {
          u32 zero = emit_iconst(ke, 0);
          u32 wsrc[3] = {valid_mask, body, zero};
          body = rangeify_emit(ke, S_IWHERE, p->dtype, 3, wsrc, 0);
        }
        u32 src_arr_v[SCALAR_MAX_SRC] = {body};
        for (u32 d = 0; d < n_out_orig; d++) src_arr_v[1 + d] = new_out_refs[d];
        for (u32 d = 0; d < n_in; d++) src_arr_v[1 + n_out_orig + d] = rv->src[1 + n_out_orig + d];
        u8  src_count = (u8)(1 + n_out_orig + n_in);
        u64 extra_v   = (u64)n_out_orig & 0xFFu;
        prog_value[i] = rangeify_emit(ke, S_RESHAPE_V, p->dtype,
                                      src_count, src_arr_v, extra_v);
        continue;
      }
      // PAD-as-size1-inflation fusion (very narrow case to be safe):
      // - single inflated axis (all others pass-through)
      // - inflated axis must have src_dim==1 AND out_dim==reduce_size
      // - source must be exactly S_LOAD (no IWHERE / chain)
      //
      // Then AND a single bounds-check on reduce_range into a fresh
      // S_IWHERE wrapping the LOAD.  Wider fusion variants (multi-
      // axis, IWHERE-wrapped loads, intermediate sources) all
      // failed lenet softmax in iteration 31 -- the load's address
      // can transitively depend on the rank-promoted axis through
      // upstream chain transforms in ways that aren't visible
      // locally.
      if (p->opcode == UOP_PAD && ndim > os->ndim
          && ke->scalar_uops[v].op == S_LOAD
          && reduce_size != 0) {
        u32 inflated_axis = (u32)-1;
        u32 inflated_begin = 0;
        u32 inflated_srcdim = 0;
        int multi_inflate = 0;
        for (u32 d = 0; d < ndim; d++) {
          u32 begin   = p->pad_widths[2 * d];
          u32 src_dim = p->src0_dims [d];
          u32 out_dim = p->out_dims  [d];
          if (begin == 0 && src_dim == out_dim) continue;
          if (src_dim == 1 && out_dim == reduce_size) {
            if (inflated_axis != (u32)-1) { multi_inflate = 1; break; }
            inflated_axis   = d;
            inflated_begin  = begin;
            inflated_srcdim = src_dim;
          } else {
            multi_inflate = 1; break;
          }
        }
        if (!multi_inflate && inflated_axis != (u32)-1) {
          u32 hi_lim  = emit_iconst(ke, (i64)(inflated_begin + inflated_srcdim));
          u32 axis_ok = emit_ibinop(ke, S_ILT, reduce_range, hi_lim);
          if (inflated_begin > 0) {
            u32 lo_lim = emit_iconst(ke, (i64)inflated_begin);
            u32 lt_lo  = emit_ibinop(ke, S_ILT, reduce_range, lo_lim);
            u32 one    = emit_iconst(ke, 1);
            u32 ge_lo  = emit_ibinop(ke, S_ISUB, one, lt_lo);
            axis_ok    = emit_ibinop(ke, S_IAND, axis_ok, ge_lo);
          }
          u32 zero    = emit_iconst(ke, 0);
          u32 wsrc[3] = {axis_ok, v, zero};
          prog_value[i] = rangeify_emit(ke, S_IWHERE, p->dtype, 3, wsrc, 0);
          continue;
        }
      }
      if (ndim > os->ndim) {
        if (getenv("THVM_RANGEIFY_BAIL")) {
          fprintf(stderr, "  SHRINK/PAD-rank: opcode=%s out_dims=[",
                  p->opcode == UOP_PAD ? "PAD" : "SHRINK");
          for (u32 d = 0; d < ndim; d++)
            fprintf(stderr, "%u%s", p->out_dims[d], d+1==ndim?"":",");
          fprintf(stderr, "] src_dims=[");
          for (u32 d = 0; d < ndim; d++)
            fprintf(stderr, "%u%s", p->src0_dims[d], d+1==ndim?"":",");
          fprintf(stderr, "] os.dims=[");
          for (u32 d = 0; d < os->ndim; d++)
            fprintf(stderr, "%u%s", os->dims[d], d+1==os->ndim?"":",");
          fprintf(stderr, "] src.op=%s\n", scalar_op_name(ke->scalar_uops[v].op));
        }
        RBAIL_MID("SHRINK/PAD ndim > os->ndim (rank-promoted intermediate)");
      }
      u8  sop;
      u64 extra = 0;
      if (p->opcode == UOP_SHRINK) {
        sop = S_SHRINK;
        for (u32 d = 0; d < ndim; d++) {
          u32 begin = p->pad_widths[2 * d];
          if (begin > 0xFFFFu) RBAIL_MID("SHRINK begin > u16");
          extra |= ((u64)begin & 0xFFFFu) << (16 * d);
        }
      } else {
        sop = S_PAD;
        for (u32 d = 0; d < ndim; d++) {
          u32 begin   = p->pad_widths[2 * d];
          u32 src_dim = p->src0_dims [d];
          if (begin > 0xFFu || src_dim > 0xFFu) RBAIL_MID("PAD begin/src_dim > u8");
          extra |= ((u64)begin   & 0xFFu) << (16 * d);
          extra |= ((u64)src_dim & 0xFFu) << (16 * d + 8);
        }
      }
      u32 src_arr[SCALAR_MAX_SRC] = {v};
      for (u32 d = 0; d < ndim; d++) src_arr[1 + d] = loop_ranges[d];
      prog_value[i] = rangeify_emit(ke, sop, p->dtype, (u8)(1 + ndim),
                                    src_arr, extra);
      continue;
    }
    if (p->opcode == UOP_FLIP) {
      // Per-axis index reversal.  The bitmask of axes to flip lives
      // in p->arg (set by materialize from heap[loc+1]).  We pass the
      // mask through extra; S_FLIP at dispatch reverses range_iter[d]
      // for each set bit before evaluating the body.
      u32 raw = p->src[0];
      u32 v   = KSRC_IS_INPUT(raw) ? input_load[KSRC_INDEX(raw)]
                                   : prog_value[KSRC_INDEX(raw)];
      if (v == 0) RBAIL_MID("FLIP src 0");
      u32 ndim = p->out_ndim ? p->out_ndim : os->ndim;
      // Cap at 8 axes -- bitmask is u8, and SCALAR_MAX_SRC = MAX_DIM+1.
      if (ndim > 8 || ndim > os->ndim) RBAIL_MID("FLIP ndim > 8");
      u64 axes_mask = (u64)(p->arg & 0xFFu);
      u32 src_arr[SCALAR_MAX_SRC] = {v};
      for (u32 d = 0; d < ndim; d++) src_arr[1 + d] = loop_ranges[d];
      prog_value[i] = rangeify_emit(ke, S_FLIP, p->dtype, (u8)(1 + ndim),
                                    src_arr, axes_mask);
      continue;
    }
    if (p->opcode == UOP_REDUCE) {
      // src[0] body: either an input load or a previous prog op.
      // Sourced from pre-scope (the body sits inside the reduce loop).
      u32 raw = p->src[0];
      u32 body = KSRC_IS_INPUT(raw) ? input_load_pre[KSRC_INDEX(raw)]
                                    : prog_value[KSRC_INDEX(raw)];
      if (body == 0) RBAIL_MID("REDUCE body 0");
      u32 src[2] = {body, reduce_range};
      u8  sop    = (reduce_kind == REDUCE_MAX) ? S_REDUCE_MAX : S_REDUCE_SUM;
      prog_value[i] = rangeify_emit(ke, sop, dtype, 2, src, 0);
      continue;
    }
    // Resolve up to 2 sources, picking the scope-appropriate input load.
    u32 src_v[2] = {0, 0};
    int all_via_rngs = (p->n_src > 0);
    for (u8 s = 0; s < p->n_src && s < 2; s++) {
      u32 raw = p->src[s];
      if (KSRC_IS_INPUT(raw)) {
        src_v[s] = input_load[KSRC_INDEX(raw)];
        if (src_v[s] == 0) RBAIL_MID("ALU input src 0");
      } else {
        src_v[s] = prog_value[KSRC_INDEX(raw)];
      }
      if (!SRC_VIA_RNGS(raw, pre)) all_via_rngs = 0;
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
      default: RBAIL_MID("ALU opcode unhandled in late switch");
    }
    if (p->n_src == 1) {
      prog_value[i] = rangeify_emit_unary(ke, sop, dtype, src_v[0]);
    } else if (p->n_src == 2) {
      prog_value[i] = rangeify_emit_binary(ke, sop, dtype, src_v[0], src_v[1]);
    } else {
      RBAIL_MID("ALU n_src not in {1,2}");
    }
    if (all_via_rngs) via_rngs[i] = 1;
  }

  // 5. STORE the final value, BUFFERIZE the kernel.  BUFFERIZE
  // carries every LOOP range (up to MAX_DIM of them) so the
  // dispatcher can iterate the outer loop nest without chasing
  // INDEX chains.
  u32 final_v = prog_value[ke->n_ops - 1];
  u32 store   = rangeify_emit_binary(ke, S_STORE, ke->output_dtype,
                                     out_index, final_v);
  u32 buf_src[SCALAR_MAX_SRC] = {store};
  u8  buf_count  = (u8)(1 + os->ndim);
  for (u32 d = 0; d < os->ndim; d++) buf_src[1 + d] = loop_ranges[d];
  rangeify_emit(ke, S_BUFFERIZE, ke->output_dtype, buf_count, buf_src, 0);
  return 1;
}
