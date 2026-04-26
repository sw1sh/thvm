// schedule/materialize.c - tinygrad-style scheduler.
//
// g2a: realize_classify + topo_sort_boundaries populate
// BOUNDARY_ORDER (kernel emit order).
// g2b: build_kernel emits one KernelEntry per boundary by visiting
//      its UOp subgraph and inlining every non-boundary upstream
//      elementwise / CONST / LOAD / REDUCE-as-tail op into the
//      kernel's program[].  Movement ops (g2c) and GRAD (g2d) are
//      not yet supported -- visit() returns 0xDEADBEEF on those,
//      which makes the boundary's emit bail and thvm_materialize
//      fall back to returning the input unchanged.

#define BOUNDARY_ORDER_CAP 1024
static u64  BOUNDARY_ORDER[BOUNDARY_ORDER_CAP];
static u32  BOUNDARY_TID  [BOUNDARY_ORDER_CAP];   // emitted output TenDesc id
static Term BOUNDARY_TERM [BOUNDARY_ORDER_CAP];   // emitted UOP_KERNEL term
static u32  BOUNDARY_ORDER_LEN = 0;

#define BOUNDARY_DEPTH_INVALID 0xFFFFFFFFu
static u32 BOUNDARY_DEPTH[REALIZE_INFO_CAP];

#define VISIT_BAIL 0xDEADBEEFu

// === topo-sort over realize boundaries (g2a) ===

static u32 boundary_depth_rec(u64 loc) {
  u32 idx = realize_info_find(loc);
  if (idx == 0xFFFFFFFFu) return 0;
  if (BOUNDARY_DEPTH[idx] != BOUNDARY_DEPTH_INVALID) return BOUNDARY_DEPTH[idx];
  BOUNDARY_DEPTH[idx] = 0;            // cycle guard

  u32 max_up = 0;
  u8  ar     = uop_arity(REALIZE_INFO[idx].op);
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) != TAG_UOP)        continue;
    if (term_ext(child) == UOP_KERNEL)     continue;
    u64 cloc = term_val(child);
    u8  dup  = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;
    u32 cd = boundary_depth_rec(cloc);
    if (cd > max_up) max_up = cd;
  }
  u32 d = REALIZE_INFO[idx].realized ? max_up + 1 : max_up;
  BOUNDARY_DEPTH[idx] = d;
  return d;
}

static void topo_sort_boundaries(Term root) {
  BOUNDARY_ORDER_LEN = 0;
  for (u32 i = 0; i < REALIZE_INFO_CAP; i++)
    BOUNDARY_DEPTH[i] = BOUNDARY_DEPTH_INVALID;
  boundary_depth_rec(term_val(root));

  struct { u64 loc; u32 depth; } items[BOUNDARY_ORDER_CAP];
  u32 n = 0;
  for (u32 i = 0; i < REALIZE_INFO_LEN && n < BOUNDARY_ORDER_CAP; i++) {
    if (!REALIZE_INFO[i].realized) continue;
    items[n].loc   = REALIZE_INFO[i].loc;
    items[n].depth = BOUNDARY_DEPTH[i];
    n++;
  }
  for (u32 i = 1; i < n; i++) {
    for (u32 j = i; j > 0; j--) {
      u8 swap = (items[j].depth <  items[j-1].depth)
            || (items[j].depth == items[j-1].depth && items[j].loc < items[j-1].loc);
      if (!swap) break;
      u64 lt = items[j].loc;   items[j].loc   = items[j-1].loc;   items[j-1].loc   = lt;
      u32 dt = items[j].depth; items[j].depth = items[j-1].depth; items[j-1].depth = dt;
    }
  }
  for (u32 i = 0; i < n; i++) BOUNDARY_ORDER[BOUNDARY_ORDER_LEN++] = items[i].loc;
}

fn u32 materialize_boundary_count(void)         { return BOUNDARY_ORDER_LEN; }
fn u64 materialize_boundary_at(u32 i)           { return i < BOUNDARY_ORDER_LEN ? BOUNDARY_ORDER[i] : 0; }

// === view-only path for movement ops (g2c1) ===
//
// RESHAPE / EXPAND / PERMUTE / SHRINK / FLIP rewrite a TenDesc's
// View instead of allocating a kernel.  Each helper computes the
// target View from the source View; view_resolve walks a movement-
// op chain rooted at a TAG_TEN, allocating an alias TenDesc per
// layer via tensor_view_of (which inherits buf_id, dtype, backend,
// producer_kid).  PAD intentionally falls through (g2c2) -- a
// view-only PAD would have to read bytes outside the alloc.

static int view_apply_reshape(View const *src, u64 expr_loc, View *out) {
  if (!src->contiguous) return 0;
  u32 t_ndim  = (u32)term_val(heap_read(expr_loc + 1));
  Shape ts = {0}; ts.ndim = t_ndim;
  u32 t_numel = 1;
  for (u32 i = 0; i < t_ndim; i++) {
    u32 d = (u32)term_val(heap_read(expr_loc + 2 + i));
    ts.dims[i] = d;
    t_numel *= d;
  }
  if (t_numel != src->numel) return 0;
  *out = view_create(ts);
  return 1;
}

static int view_apply_expand(View const *src, u64 expr_loc, View *out) {
  u32 t_ndim = (u32)term_val(heap_read(expr_loc + 1));
  if (t_ndim < src->shape.ndim) return 0;       // can't drop axes via EXPAND
  Shape ts = {0}; ts.ndim = t_ndim;
  u32 t_numel = 1;
  for (u32 i = 0; i < t_ndim; i++) {
    u32 td = (u32)term_val(heap_read(expr_loc + 2 + i));
    ts.dims[i] = td;
    t_numel  *= td;
    // Existing axis: must match exactly or be 1 (broadcast).
    if (i < src->shape.ndim
        && src->shape.dims[i] != td && src->shape.dims[i] != 1) return 0;
  }
  out->shape      = ts;
  out->numel      = t_numel;
  out->offset     = src->offset;
  out->contiguous = (t_numel == src->numel) ? src->contiguous : 0;
  for (u32 i = 0; i < t_ndim; i++) {
    if (i >= src->shape.ndim) out->strides[i] = 0;     // new trailing broadcast axis
    else out->strides[i] = (src->shape.dims[i] == ts.dims[i]) ? src->strides[i] : 0;
  }
  for (u32 i = t_ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  return 1;
}

static int view_apply_permute(View const *src, u64 expr_loc, View *out) {
  if (!src->contiguous) return 0;       // simple-source-only for v1
  Shape ts = {0}; ts.ndim = src->shape.ndim;
  out->offset = src->offset;
  u8 used[MAX_DIM] = {0};
  u8 identity = 1;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    u32 p = (u32)term_val(heap_read(expr_loc + 1 + i));
    if (p >= src->shape.ndim || used[p]) return 0;
    used[p] = 1;
    ts.dims[i]      = src->shape.dims[p];
    out->strides[i] = src->strides[p];
    if (p != i) identity = 0;
  }
  for (u32 i = src->shape.ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  out->shape      = ts;
  out->numel      = src->numel;
  out->contiguous = identity ? src->contiguous : 0;
  return 1;
}

static int view_apply_shrink(View const *src, u64 expr_loc, View *out) {
  if (!src->contiguous) return 0;
  Shape ts = {0}; ts.ndim = src->shape.ndim;
  i32 add_off = 0;
  u32 t_numel = 1;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    u32 b = (u32)term_val(heap_read(expr_loc + 1 + 2 * i));
    u32 e = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
    if (e <= b || e > src->shape.dims[i]) return 0;
    ts.dims[i] = e - b;
    t_numel  *= (e - b);
    add_off  += (i32)b * src->strides[i];
  }
  out->shape  = ts;
  out->numel  = t_numel;
  out->offset = src->offset + add_off;
  for (u32 i = 0; i < src->shape.ndim; i++) out->strides[i] = src->strides[i];
  for (u32 i = src->shape.ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  out->contiguous = (t_numel == src->numel) ? 1 : 0;
  return 1;
}

static int view_apply_flip(View const *src, u64 expr_loc, View *out) {
  if (!src->contiguous) return 0;
  u32 mask = (u32)term_val(heap_read(expr_loc + 1));
  out->shape  = src->shape;
  out->numel  = src->numel;
  out->offset = src->offset;
  u8 any = 0;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    if (mask & (1u << i)) {
      out->strides[i] = -src->strides[i];
      out->offset += (i32)(src->shape.dims[i] - 1) * src->strides[i];
      any = 1;
    } else {
      out->strides[i] = src->strides[i];
    }
  }
  for (u32 i = src->shape.ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  out->contiguous = any ? 0 : src->contiguous;
  return 1;
}

// Materialize a UOP_CONST to a 1-element TenDesc filled with the
// const value.  Used by view_resolve when a movement-op chain
// (typically EXPAND from interact_grad's expand_to_target leaf
// rule) bottoms out at a CONST instead of a TenDesc.
static u32 const_to_tendesc(u64 const_loc) {
  Term num   = heap_read(const_loc);
  u32  dtype = term_ext(num);
  u32  bits  = (u32)term_val(num);
  Shape s = {0}; s.ndim = 1; s.dims[0] = 1;
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, dtype);
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, &bits, 4);
  return tid;
}

// Dispatcher: walk a movement-op chain, allocating one alias
// TenDesc per layer; return the final tid (0 on bail).  The
// source must resolve to a TenDesc (TAG_TEN, UOP_KERNEL, UOP_CONST
// materialized to a 1-element TenDesc, or a recursive view chain
// rooted at one).  Backend must be view-aware -- otherwise returns
// 0 so caller falls through.
static u32 view_resolve(Term t) {
  if (CURRENT_BACKEND == NULL || !CURRENT_BACKEND->view_aware) return 0;
  u8 tag = term_tag(t);
  if (tag == TAG_TEN) return (u32)term_val(t);
  if (tag != TAG_UOP) return 0;

  u8  op  = term_ext(t);
  u64 loc = term_val(t);

  if (op == UOP_KERNEL) {
    Term outbuf = heap_read(loc);
    if (term_tag(outbuf) != TAG_TEN) return 0;
    return (u32)term_val(outbuf);
  }

  // CONST source: materialize to a fresh 1-element TenDesc.
  // interact_grad's expand_to_target leaf rule produces
  // EXPAND(CONST(0|1)) -> target.shape; without this branch the
  // grad chain stalls as a UOP that never fires.
  if (op == UOP_CONST) return const_to_tendesc(loc);

  // Source recurses (could be another movement op chain or CONST).
  u32 src_tid = view_resolve(heap_read(loc));
  if (src_tid == 0) return 0;
  View const *src_view = &TENS[src_tid].view;

  View nv = {0};
  int  ok = 0;
  switch (op) {
    case UOP_RESHAPE: ok = view_apply_reshape(src_view, loc, &nv); break;
    case UOP_EXPAND:  ok = view_apply_expand (src_view, loc, &nv); break;
    case UOP_PERMUTE: ok = view_apply_permute(src_view, loc, &nv); break;
    case UOP_SHRINK:  ok = view_apply_shrink (src_view, loc, &nv); break;
    case UOP_FLIP:    ok = view_apply_flip   (src_view, loc, &nv); break;
    default: return 0;                      // PAD + non-movement ops bail
  }
  if (!ok) return 0;
  return tensor_view_of(src_tid, nv);
}

// True when a UOp opcode is one of the 5 view-only-path movement ops.
static u8 op_is_view_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_EXPAND || op == UOP_PERMUTE
      || op == UOP_SHRINK  || op == UOP_FLIP;
}

// Flatten a non-contig TenDesc into a fresh contiguous copy via
// view_strided_index.  Used by thvm_materialize when the root is a
// movement-op chain that resolves to a view alias the caller will
// read through (wnf expects flat-buffer reads).
static Term materialize_root_alias(Term t) {
  if (term_tag(t) != TAG_TEN) return t;
  u32 tid = (u32)term_val(t);
  if (tid == 0 || tid >= TENS_NEXT) return t;
  TenDesc *d = &TENS[tid];
  if (d->view.contiguous && d->view.offset == 0) return t;

  u32 dst_tid = tensor_alloc(d->backend, d->view.shape, d->dtype);
  if (dst_tid == 0) return t;

  // Bytes to read = max element index reachable + 1.  Negative
  // strides (FLIP) start the offset at the high end and walk
  // downward, so they're already covered by `offset`.
  i32 max_idx = d->view.offset;
  for (u32 i = 0; i < d->view.shape.ndim; i++) {
    if (d->view.shape.dims[i] > 1 && d->view.strides[i] > 0)
      max_idx += (i32)(d->view.shape.dims[i] - 1) * d->view.strides[i];
  }
  size_t src_bytes = (size_t)(max_idx + 1) * 4;
  void  *raw       = malloc(src_bytes);
  d->backend->buf_read(d->buf_id, raw, src_bytes);
  void *dst_host = malloc((size_t)d->view.numel * 4);
  if (d->dtype == DT_F32) {
    f32 *o = (f32 *)dst_host; f32 *s = (f32 *)raw;
    for (u32 k = 0; k < d->view.numel; k++) o[k] = s[view_strided_index(&d->view, k)];
  } else {
    i32 *o = (i32 *)dst_host; i32 *s = (i32 *)raw;
    for (u32 k = 0; k < d->view.numel; k++) o[k] = s[view_strided_index(&d->view, k)];
  }
  d->backend->buf_write(TENS[dst_tid].buf_id, dst_host, (size_t)d->view.numel * 4);
  free(raw);
  free(dst_host);
  return term_new(0, TAG_TEN, d->dtype, dst_tid);
}

// === build_kernel: visit() recursion (g2b) ===

static u32 boundary_index_for_loc(u64 loc) {
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++)
    if (BOUNDARY_ORDER[i] == loc) return i;
  return 0xFFFFFFFFu;
}

static u32 input_slot_dedup(KernelEntry *ke, u32 tid, Term term) {
  for (u32 i = 0; i < ke->n_inputs; i++)
    if (ke->input_tids[i] == tid && ke->input_terms[i] == term) return i;
  if (ke->n_inputs >= KERNEL_MAX_INPUT) return 0xFFFFFFFFu;
  u32 slot = ke->n_inputs++;
  ke->input_tids   [slot] = tid;
  ke->input_dtypes [slot] = TENS[tid].dtype;
  ke->input_numels [slot] = TENS[tid].view.numel;
  ke->input_terms  [slot] = term;
  return slot;
}

static u32 src_dtype(KernelEntry *ke, u32 src_idx) {
  return KSRC_IS_INPUT(src_idx) ? ke->input_dtypes[KSRC_INDEX(src_idx)]
                                 : ke->program[src_idx].dtype;
}
static u32 src_numel(KernelEntry *ke, u32 src_idx) {
  return KSRC_IS_INPUT(src_idx) ? ke->input_numels[KSRC_INDEX(src_idx)]
                                 : ke->program[src_idx].numel;
}

// Recursive visit.  Returns a program-index (0..n_ops-1) or
// VISIT_BAIL on any unsupported op.
static u32 visit(Term t, KernelEntry *ke, u64 root_loc) {
  u8 tag = term_tag(t);

  if (tag == TAG_TEN) {
    u32 tid  = (u32)term_val(t);
    u32 slot = input_slot_dedup(ke, tid, t);
    if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
    return KSRC_AS_INPUT(slot);
  }
  if (tag != TAG_UOP) return VISIT_BAIL;

  u8  op  = term_ext(t);
  u64 loc = term_val(t);

  if (op == UOP_KERNEL) {
    Term outbuf = heap_read(loc);
    if (term_tag(outbuf) != TAG_TEN) return VISIT_BAIL;
    u32 tid  = (u32)term_val(outbuf);
    u32 slot = input_slot_dedup(ke, tid, t);
    if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
    return KSRC_AS_INPUT(slot);
  }

  // Boundary that isn't this kernel's root: become an input.  The
  // upstream boundary was emitted earlier in topo order, so its
  // BOUNDARY_TID slot is populated.
  if (loc != root_loc) {
    u32 bi = boundary_index_for_loc(loc);
    if (bi != 0xFFFFFFFFu) {
      u32 tid = BOUNDARY_TID[bi];
      Term boundary_term = BOUNDARY_TERM[bi];
      if (tid == 0) return VISIT_BAIL;
      u32 slot = input_slot_dedup(ke, tid, boundary_term);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      return KSRC_AS_INPUT(slot);
    }
  }

  if (op == UOP_CONST) {
    if (ke->n_ops >= KPROG_MAX_OPS) return VISIT_BAIL;
    Term num = heap_read(loc);
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode = UOP_CONST;
    p->dtype  = term_ext(num);            // dtype on the NUM cell
    p->arg    = (u32)term_val(num);
    p->n_src  = 0;
    p->numel  = 1;
    return ke->n_ops - 1;
  }

  if (uop_is_unary_elementwise(op) || op == UOP_LOAD) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    if (ke->n_ops >= KPROG_MAX_OPS) return VISIT_BAIL;
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode = (u8)op;
    p->dtype  = src_dtype(ke, src_idx);
    p->numel  = src_numel(ke, src_idx);
    p->n_src  = 1;
    p->src[0] = src_idx;
    return ke->n_ops - 1;
  }

  if (uop_is_binary_elementwise(op)) {
    u32 li = visit(heap_read(loc + 0), ke, root_loc);
    if (li == VISIT_BAIL) return VISIT_BAIL;
    u32 ri = visit(heap_read(loc + 1), ke, root_loc);
    if (ri == VISIT_BAIL) return VISIT_BAIL;
    if (ke->n_ops >= KPROG_MAX_OPS) return VISIT_BAIL;
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode = (u8)op;
    u32 ln = src_numel(ke, li), rn = src_numel(ke, ri);
    p->numel  = (ln >= rn) ? ln : rn;
    p->dtype  = src_dtype(ke, li);
    p->n_src  = 2;
    p->src[0] = li;
    p->src[1] = ri;
    return ke->n_ops - 1;
  }

  // GRAD is a stop point in materialize -- the architecture is
  // wnf-fires-grad + materialize-compiles-uops, with thvm_realize
  // looping the pair until no fresh kernels are emitted.  Bailing
  // here makes the enclosing kernel emission abort so the caller
  // (thvm_realize) loops back through wnf to fire interact_grad.
  if (op == UOP_GRAD) return VISIT_BAIL;

  // Movement ops as a child of the kernel: try view-only resolve
  // first.  If the source isn't a contig TenDesc-resolvable chain
  // (e.g., EXPAND wrapping a MUL from interact_grad), fall through
  // to kernel-op emit with the appropriate metadata.
  if (op_is_view_movement(op)) {
    u32 alias_tid = view_resolve(t);
    if (alias_tid != 0) {
      Term alias_term = term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
      u32 slot = input_slot_dedup(ke, alias_tid, alias_term);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      return KSRC_AS_INPUT(slot);
    }
    // Fallback: emit as a kernel op.  Recurse into source, look up
    // shapes, populate the metadata cpu_op_<op> + Metal shaders need.
    u32 src_idx = visit(heap_read(loc), ke, root_loc);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    if (ke->n_ops >= KPROG_MAX_OPS) return VISIT_BAIL;
    Shape src_shape = {0};
    if (!term_shape_in(heap_read(loc), 0, &src_shape)) return VISIT_BAIL;
    Shape out_shape = {0};
    if (!term_shape_in(t, 0, &out_shape)) return VISIT_BAIL;
    u32 out_numel = 1;
    for (u32 i = 0; i < out_shape.ndim; i++) out_numel *= out_shape.dims[i];
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode    = op;
    p->dtype     = src_dtype(ke, src_idx);
    p->numel     = out_numel;
    p->n_src     = 1;
    p->src[0]    = src_idx;
    p->src0_ndim = (u8)(src_shape.ndim & 0xFF);
    p->out_ndim  = (u8)(out_shape.ndim & 0xFF);
    for (u32 i = 0; i < src_shape.ndim; i++) p->src0_dims[i] = src_shape.dims[i];
    for (u32 i = 0; i < out_shape.ndim; i++) p->out_dims [i] = out_shape.dims[i];
    if (op == UOP_PERMUTE) {
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 pi = (u32)term_val(heap_read(loc + 1 + i));
        p->axis_perm[i] = (u8)(pi & 0xFF);
      }
    }
    if (op == UOP_SHRINK) {
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 b = (u32)term_val(heap_read(loc + 1 + 2 * i));
        u32 e = (u32)term_val(heap_read(loc + 2 + 2 * i));
        p->pad_widths[2 * i + 0] = (u8)(b & 0xFF);
        p->pad_widths[2 * i + 1] = (u8)(e & 0xFF);
      }
    }
    return ke->n_ops - 1;
  }

  // PAD as a kernel emit (g2c2): allocate a fresh buf, run
  // cpu_op_pad / metal pad shader.  Unlike SHRINK/PERMUTE/etc, PAD
  // can't be a view-only alias because reading bytes outside the
  // alloc is UB even when calloc'd.
  if (op == UOP_PAD) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    if (ke->n_ops >= KPROG_MAX_OPS) return VISIT_BAIL;
    // Source shape: from the PAD's source term (TenDesc lookup).
    Shape src_shape = {0};
    if (!term_shape_in(heap_read(loc), 0, &src_shape)) return VISIT_BAIL;
    // Output shape: src.dim[i] + b_i + e_i per axis.
    Shape out_shape = src_shape;
    u32   out_numel = 1;
    for (u32 i = 0; i < src_shape.ndim; i++) {
      u32 b = (u32)term_val(heap_read(loc + 1 + 2 * i));
      u32 e = (u32)term_val(heap_read(loc + 2 + 2 * i));
      out_shape.dims[i] = src_shape.dims[i] + b + e;
      out_numel        *= out_shape.dims[i];
    }
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode    = UOP_PAD;
    p->dtype     = src_dtype(ke, src_idx);
    p->numel     = out_numel;
    p->n_src     = 1;
    p->src[0]    = src_idx;
    p->src0_ndim = (u8)(src_shape.ndim & 0xFF);
    p->out_ndim  = (u8)(out_shape.ndim & 0xFF);
    for (u32 i = 0; i < src_shape.ndim; i++) {
      p->src0_dims[i] = src_shape.dims[i];
      p->out_dims [i] = out_shape.dims[i];
      u32 b = (u32)term_val(heap_read(loc + 1 + 2 * i));
      u32 e = (u32)term_val(heap_read(loc + 2 + 2 * i));
      p->pad_widths[2 * i + 0] = (u8)(b & 0xFF);
      p->pad_widths[2 * i + 1] = (u8)(e & 0xFF);
    }
    return ke->n_ops - 1;
  }

  // REDUCE only allowed when it IS the root (tail-fuse).  Movement
  // ops + non-tail REDUCE bail (deferred to g2c / g2d).
  if (op == UOP_REDUCE && loc == root_loc) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    if (ke->n_ops >= KPROG_MAX_OPS) return VISIT_BAIL;
    u32 kind = (u32)term_val(heap_read(loc + 1));
    u32 axis = (u32)term_val(heap_read(loc + 2));
    // arg encoding (cpu_op_reduce / Metal reduce shader):
    //   bits 24..31 = kind, bits 0..23 = inner = prod(dims[axis+1..]).
    // Packing axis here (the round-1 bug) gave inner = 0 -> fallback
    // to 1 -> axis treated as innermost, which is correct only when
    // axis IS innermost.  Compute inner from the source shape.
    Shape src_shape = {0};
    u32 inner = 1;
    if (term_shape_in(heap_read(loc), 0, &src_shape)) {
      for (u32 i = axis + 1; i < src_shape.ndim; i++) inner *= src_shape.dims[i];
    }
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode = UOP_REDUCE;
    p->dtype  = src_dtype(ke, src_idx);
    p->arg    = (kind << 24) | (inner & 0x00FFFFFFu);
    p->numel  = ke->output_numel;
    p->n_src  = 1;
    p->src[0] = src_idx;
    return ke->n_ops - 1;
  }

  return VISIT_BAIL;
}

// Build one kernel rooted at the boundary at index bi.  Returns
// the emitted UOP_KERNEL term, or 0 on bail.
static Term emit_kernel_for_boundary(u32 bi) {
  u64 boundary_loc = BOUNDARY_ORDER[bi];
  u32 idx = realize_info_find(boundary_loc);
  if (idx == 0xFFFFFFFFu) return 0;

  u8   op        = REALIZE_INFO[idx].op;
  Term root_term = term_new(0, TAG_UOP, op, boundary_loc);

  Shape out_shape = {0};
  if (!term_shape_in(root_term, 0, &out_shape)) return 0;
  u32 out_dtype = DT_F32;
  term_dtype_in(root_term, 0, &out_dtype);

  u32 out_tid = tensor_alloc(CURRENT_BACKEND, out_shape, out_dtype);
  u32 kid     = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->output_tid    = out_tid;
  ke->output_dtype  = out_dtype;
  ke->output_shape  = out_shape;
  ke->output_numel  = TENS[out_tid].view.numel;
  ke->source_uop    = root_term;
  TENS[out_tid].producer_kid = kid;

  u32 result = visit(root_term, ke, boundary_loc);
  if (result == VISIT_BAIL) {
    kernel_dealloc_last(kid);
    TENS[out_tid].producer_kid = 0;
    return 0;
  }

  u64 kloc = heap_alloc(2);
  heap_set(kloc + 0, term_new(0, TAG_TEN, out_dtype, out_tid));
  heap_set(kloc + 1, term_new(0, TAG_NUM, DT_I32, kid));
  Term kernel_term = term_new(0, TAG_UOP, UOP_KERNEL, kloc);

  BOUNDARY_TID [bi] = out_tid;
  BOUNDARY_TERM[bi] = kernel_term;
  return kernel_term;
}

// Direct kernelize entry called by the surviving view tests:
// for the 5 view-only movement ops, return the alias TenDesc as a
// TAG_TEN; for everything else (PAD, elementwise, reduce, kernel)
// fall through to the normal kernel-emit path.
fn Term materialize_uop_in_env(Term t, u32 env_id) {
  (void)env_id;
  if (term_tag(t) == TAG_UOP) {
    u8 op = term_ext(t);
    if (op_is_view_movement(op)) {
      u32 alias_tid = view_resolve(t);
      if (alias_tid != 0)
        return term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
    }
  }
  return thvm_materialize(t);
}

fn Term thvm_materialize(Term term) {
  // TAG_CTR (multi-target grad bundle): materialize each child
  // independently and rebuild the CTR.  Pure structural recursion;
  // the children themselves are normal UOp graphs (or already TAG_TEN
  // for grad components that wnf already reduced).
  if (term_tag(term) == TAG_CTR) {
    u32 n = term_ctr_n(term);
    if (n > 256) return term;
    Term children[256];
    for (u32 i = 0; i < n; i++)
      children[i] = thvm_materialize(term_ctr_at(term, i));
    return term_new_ctr(term_ext(term), children, n);
  }
  if (term_tag(term) != TAG_UOP)        return term;
  if (term_ext(term) == UOP_KERNEL)     return term;
  // GRAD is a stop point in materialize -- wnf fires interact_grad,
  // then thvm_realize loops back here to compile the unrolled UOps.
  if (term_ext(term) == UOP_GRAD)       return term;

  // Movement-op root: resolve the chain to an alias TenDesc, then
  // flatten to a contig copy so wnf-side flat reads work.
  if (op_is_view_movement(term_ext(term))) {
    u32 alias_tid = view_resolve(term);
    if (alias_tid != 0) {
      Term alias_term = term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
      return materialize_root_alias(alias_term);
    }
  }

  realize_classify(term);
  topo_sort_boundaries(term);
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    BOUNDARY_TID [i] = 0;
    BOUNDARY_TERM[i] = 0;
  }
  // All-or-nothing emission: if any boundary fails to compile,
  // rewind KERNELS_NEXT and TENS_NEXT to their pre-call values
  // and return the input unchanged.  Without this rewind, partial
  // emission accumulates orphan kernels per call -- thvm_realize's
  // loop sees the same UOp graph each iteration and re-emits the
  // same successful boundaries, growing KERNELS_NEXT linearly with
  // iter count (the symptom that produced 1k+ kernels for a
  // softmax+CE backward).
  u32 kernels_at_start = KERNELS_NEXT;
  u32 tens_at_start    = TENS_NEXT;
  Term sink_kernel = 0;
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    Term k = emit_kernel_for_boundary(i);
    if (k == 0) {
      KERNELS_NEXT = kernels_at_start;
      TENS_NEXT    = tens_at_start;
      return term;
    }
    if (BOUNDARY_ORDER[i] == term_val(term)) sink_kernel = k;
  }
  return sink_kernel != 0 ? sink_kernel : term;
}
