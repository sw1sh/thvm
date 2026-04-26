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

  // REDUCE only allowed when it IS the root (tail-fuse).  Movement
  // ops + non-tail REDUCE bail (deferred to g2c / g2d).
  if (op == UOP_REDUCE && loc == root_loc) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    if (ke->n_ops >= KPROG_MAX_OPS) return VISIT_BAIL;
    u32 kind = (u32)term_val(heap_read(loc + 1));
    u32 axis = (u32)term_val(heap_read(loc + 2));
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode = UOP_REDUCE;
    p->dtype  = src_dtype(ke, src_idx);
    p->arg    = (kind << 24) | axis;
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

// Stub: g2c will replace with the movement-op view-rewrite path.
fn Term materialize_uop_in_env(Term t, u32 env_id) { (void)env_id; return t; }

fn Term thvm_materialize(Term term) {
  if (term_tag(term) != TAG_UOP)        return term;
  if (term_ext(term) == UOP_KERNEL)     return term;
  realize_classify(term);
  topo_sort_boundaries(term);
  // Reset emit map for this realize.
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    BOUNDARY_TID [i] = 0;
    BOUNDARY_TERM[i] = 0;
  }
  // Emit kernels in topo order.  The sink is the boundary whose loc
  // matches the input root; if any emit bails, return the input
  // unchanged so callers get a visible no-op rather than a partial
  // kernel graph.
  Term sink_kernel = 0;
  for (u32 i = 0; i < BOUNDARY_ORDER_LEN; i++) {
    Term k = emit_kernel_for_boundary(i);
    if (k == 0) return term;
    if (BOUNDARY_ORDER[i] == term_val(term)) sink_kernel = k;
  }
  return sink_kernel != 0 ? sink_kernel : term;
}
