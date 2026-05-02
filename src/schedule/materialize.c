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

#define BOUNDARY_ORDER_CAP 16384
static u64  BOUNDARY_ORDER[BOUNDARY_ORDER_CAP];
static u32  BOUNDARY_TID  [BOUNDARY_ORDER_CAP];   // emitted output TenDesc id
static Term BOUNDARY_TERM [BOUNDARY_ORDER_CAP];   // emitted UOP_KERNEL term
static u32  BOUNDARY_ORDER_LEN = 0;

// Open-addressed loc -> BOUNDARY_ORDER index hash.  Without it,
// boundary_index_for_loc was an O(BOUNDARY_ORDER_LEN) scan per
// visited UOP child, called from every emit_kernel_for_boundary's
// visit() recursion -- the dominant cost above n~12 in the bound-w
// SGD pattern (O(N^3) materialize).
#define BOUNDARY_HASH_CAP   (1u << 16)        // 64K slots, BOUNDARY_ORDER_CAP = 16384
#define BOUNDARY_HASH_EMPTY 0xFFFFFFFFu
static u32 BOUNDARY_HASH[BOUNDARY_HASH_CAP];

static inline u32 boundary_hash_of(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (BOUNDARY_HASH_CAP - 1);
}

static void boundary_hash_clear(void) {
  for (u32 i = 0; i < BOUNDARY_HASH_CAP; i++) BOUNDARY_HASH[i] = BOUNDARY_HASH_EMPTY;
}

static void boundary_hash_insert(u64 loc, u32 idx) {
  u32 h = boundary_hash_of(loc);
  for (u32 probe = 0; probe < BOUNDARY_HASH_CAP; probe++) {
    u32 i = (h + probe) & (BOUNDARY_HASH_CAP - 1);
    if (BOUNDARY_HASH[i] == BOUNDARY_HASH_EMPTY) {
      BOUNDARY_HASH[i] = idx;
      return;
    }
  }
}

#define BOUNDARY_DEPTH_INVALID 0xFFFFFFFFu
static u32 BOUNDARY_DEPTH    [REALIZE_INFO_CAP];
// Per-boundary maximum-consumer depth.  Filled after topo by
// boundary_compute_last_use; consumed by the depth-aware mem planner
// to recycle output bufs once their LAST consumer has emitted.  0 =
// "no consumer is itself a realize boundary"; that includes the
// realize root (the caller reads it; never recycle) and any orphan
// preserved tensor.
static u32 BOUNDARY_LAST_USE [REALIZE_INFO_CAP];

#define VISIT_BAIL 0xDEADBEEFu

// === per-realize memory planner =====================================
//
// Phase 8 of the tinygrad-parity arc.  After topo_sort + last_use
// computation, the emit loop walks boundaries in alloc-depth order;
// before each kernel allocates its output buf, the planner pushes
// any earlier kernel's output buf whose last_use_depth has already
// passed onto the backend's freelist.  cpu_buf_alloc then pops a
// same-nbytes match instead of growing CPU_BUFS_NEXT.  Today this is
// CPU-only: Metal has command-buffer batches plus deferred decrefs, so
// speculative planner reuse needs a matching Metal drain/proof first.

#define MEM_PLAN_CAP BOUNDARY_ORDER_CAP
typedef struct {
  u32 buf_id;
  u32 last_use_depth;
  u8  backend_id;       // 1 = CPU, 2 = Metal
  u8  pushed;
} MemPlanEntry;

static MemPlanEntry MEM_PLAN[MEM_PLAN_CAP];
static u32          MEM_PLAN_LEN = 0;

// Default-off opt-in via THVM_REUSE_BUFS=1.  Within-pass reuse is
// safe for forward-only flat graphs; the chain-rule + Phase-3
// fusion-relaxation cases need DUP/SUP-aware lifetime tracking
// that's a Phase-9 follow-up.  Until then, the planner ships
// gated -- opt in for the bench numbers, training graphs stick
// with the eager allocator.
static int mem_plan_enabled(void) {
  static int known = 0, enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_REUSE_BUFS");
    enabled       = (e && e[0] == '1');
    known         = 1;
  }
  return enabled;
}

static void mem_plan_reset(void) { MEM_PLAN_LEN = 0; }

// Pop every entry the planner left on CPU_FREELIST that hasn't been
// re-issued by an in-pass cpu_buf_alloc.  thvm_realize loops
// materialize+wnf to fixed-point; if a planner push from pass N
// survived into pass N+1's freelist, pass N+1's allocations could
// reuse a buf whose original TenDesc is still referenced by the
// chain rule's freshly-emitted UOPs, corrupting the read.  Drain
// at end-of-pass so the planner's freelist scope stays strictly
// per-pass; within-pass reuse (alloc-then-pop within the same emit
// loop) still works.
static void mem_plan_drain_freelist(void) {
  if (!mem_plan_enabled()) return;
  for (u32 i = 0; i < MEM_PLAN_LEN; i++) {
    MemPlanEntry *e = &MEM_PLAN[i];
    if (!e->pushed)         continue;
    if (e->backend_id != 1) continue;       // CPU only for now
    // Walk CPU_FREELIST looking for this buf_id.  If still there,
    // pop it (without reissuing) so it returns to the original
    // owner's "live" state.  Drop refcount stays 0 until either a
    // future TenDesc grabs it or end-of-realize rollback frees it.
    for (u32 k = 0; k < CPU_FREELIST_LEN; k++) {
      if (CPU_FREELIST[k] != e->buf_id)     continue;
      // Swap-with-last + shrink: remove from freelist without
      // changing CPU_BUFS[e->buf_id].refcount or contents.  The
      // buf goes back to refcount=1 so the existing extern-pin /
      // preserve walk handles it correctly.
      CPU_FREELIST[k] = CPU_FREELIST[CPU_FREELIST_LEN - 1];
      CPU_FREELIST_LEN--;
      CPU_BUFS[e->buf_id].refcount = 1;
      break;
    }
  }
}

static void mem_plan_record(u32 buf_id, u32 last_use_depth, Backend *b) {
  if (b == NULL || buf_id == 0)         return;
  if (MEM_PLAN_LEN >= MEM_PLAN_CAP)     return;
  MemPlanEntry *e = &MEM_PLAN[MEM_PLAN_LEN++];
  e->buf_id         = buf_id;
  e->last_use_depth = last_use_depth;
  e->backend_id     = (u8)b->id;
  e->pushed         = 0;
}

static void mem_plan_push_dead(u32 current_depth) {
  if (!mem_plan_enabled()) return;
  for (u32 i = 0; i < MEM_PLAN_LEN; i++) {
    MemPlanEntry *e = &MEM_PLAN[i];
    if (e->pushed)                              continue;
    if (e->last_use_depth >= current_depth)     continue;
    if (e->buf_id == 0)                         continue;
    if (e->backend_id == 1) {
      // Refcount > 1 means another TenDesc aliases this buf
      // (typically a view-only RESHAPE / EXPAND chain).  Recycling
      // would yank the bytes from the alias too, so skip.  The
      // existing post-realize preserve walk + rollback releases
      // these via the refcount-driven path.
      if (CPU_BUFS[e->buf_id].refcount > 1) { e->pushed = 1; continue; }
      // External / WL-shared bufs (NumericArray imports) own no
      // backing storage; don't push them onto the freelist (the
      // freelist owns the malloc'd region).
      if (!CPU_BUFS[e->buf_id].owns_data)   { e->pushed = 1; continue; }
      cpu_buf_freelist_push(e->buf_id);
    }
    e->pushed = 1;
  }
}


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
    // term_resolve to follow VAR/ALO chains -- match realize_walk_rec
    // and visit() so the topo-sort sees the same boundary set.
    Term child = term_resolve(heap_read(loc + i));
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

// Walk DOWN from `from_loc` through non-realized intermediates.  For
// each realized boundary B encountered along the way, set
// BOUNDARY_LAST_USE[B] = max(BOUNDARY_LAST_USE[B], visiting_depth).
// `visited` is a bitmap sized to HEAP_NEXT to dedup the recursion.
//
// The walk has to descend through non-realized UOps because the
// emit loop INLINES them into the parent's program (visit() in
// emit_kernel_for_boundary recurses through them as KProgOp slots);
// the boundary that the program eventually reads is the realized
// kid, so its true last_use_depth is the realized PARENT'S depth,
// not the non-realized intermediate's "depth" (which equals the
// child's, see boundary_depth_rec where non-realized just inherits).
static void boundary_last_use_descend(u64 from_loc, u32 visiting_depth,
                                      u8 *visited) {
  if (from_loc == 0 || from_loc >= HEAP_NEXT) return;
  if (visited[from_loc]) return;
  visited[from_loc] = 1;
  u32 idx = realize_info_find(from_loc);
  if (idx == 0xFFFFFFFFu) return;
  if (REALIZE_INFO[idx].realized) {
    if (visiting_depth > BOUNDARY_LAST_USE[idx]) {
      BOUNDARY_LAST_USE[idx] = visiting_depth;
    }
    return;     // stop at the boundary -- its OWN children get
                // handled when boundary_compute_last_use walks
                // them as realized parents.
  }
  // Non-realized intermediate: recurse through its UOp children.
  u8 ar = uop_arity(REALIZE_INFO[idx].op);
  u64 seen[MAX_UOP_SRC] = {0};
  u8  n_seen = 0;
  for (u8 c = 0; c < ar; c++) {
    Term child = term_resolve(heap_read(from_loc + c));
    if (term_tag(child) != TAG_UOP)         continue;
    if (term_ext(child) == UOP_KERNEL)      continue;
    u64 cloc = term_val(child);
    u8  dup  = 0;
    for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
    if (dup) continue;
    seen[n_seen++] = cloc;
    boundary_last_use_descend(cloc, visiting_depth, visited);
  }
}

// For each realized parent at depth D, walk its UOp subtree (through
// any non-realized intermediates) and bump BOUNDARY_LAST_USE on every
// realized child it reaches to D.  After this, the planner can
// safely freelist-push a buf at depth = last_use + 1 because every
// realized parent that consumes it has already emitted by then.
static void boundary_compute_last_use(void) {
  for (u32 i = 0; i < REALIZE_INFO_CAP; i++) BOUNDARY_LAST_USE[i] = 0;
  if (HEAP_NEXT == 0) return;
  u8 *visited = (u8 *)calloc(HEAP_NEXT, 1);
  if (visited == NULL) return;
  for (u32 i = 0; i < REALIZE_INFO_LEN; i++) {
    UOpInfo *p = &REALIZE_INFO[i];
    if (!p->realized)                              continue;
    u32 p_depth = BOUNDARY_DEPTH[i];
    if (p_depth == BOUNDARY_DEPTH_INVALID)         continue;
    u8 ar = uop_arity(p->op);
    u64 seen[MAX_UOP_SRC] = {0};
    u8  n_seen = 0;
    // Reset the visited bitmap per-parent so the walk doesn't
    // collapse across parents (each parent independently roots
    // its own consumer-depth update).
    memset(visited, 0, HEAP_NEXT);
    for (u8 c = 0; c < ar; c++) {
      Term child = term_resolve(heap_read(p->loc + c));
      if (term_tag(child) != TAG_UOP)         continue;
      if (term_ext(child) == UOP_KERNEL)      continue;
      u64 cloc = term_val(child);
      u8  dup  = 0;
      for (u8 j = 0; j < n_seen; j++) if (seen[j] == cloc) { dup = 1; break; }
      if (dup) continue;
      seen[n_seen++] = cloc;
      boundary_last_use_descend(cloc, p_depth, visited);
    }
  }
  free(visited);
}

static void topo_sort_boundaries(Term root) {
  BOUNDARY_ORDER_LEN = 0;
  boundary_hash_clear();
  for (u32 i = 0; i < REALIZE_INFO_CAP; i++)
    BOUNDARY_DEPTH[i] = BOUNDARY_DEPTH_INVALID;
  boundary_depth_rec(term_val(root));
  boundary_compute_last_use();

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
  for (u32 i = 0; i < n; i++) {
    u32 idx = BOUNDARY_ORDER_LEN++;
    BOUNDARY_ORDER[idx] = items[i].loc;
    boundary_hash_insert(items[i].loc, idx);
  }
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

// _merge_dims: collapse runs of stride-compatible axes in `src` into
// (merged_dim, stride, expand_real_dim) triples.  Mirrors tinygrad's
// View._merge_dims at tinygrad/shape/view.py:19.  Two adjacent axes
// (size s_a, stride st_a) and (size s_b, stride st_b) are
// stride-compatible iff st_a == s_b * st_b -- the outer axis steps
// over exactly one inner block, so logically the pair acts as a
// single axis of size s_a*s_b with the inner stride.
//
// Unit axes (size 1) are treated as "merging" placeholders and join
// the next axis without affecting the merged stride.  Stride-0
// (broadcast) axes form their own block; expand_real_dim is set to 0
// for them so the reshape fitter knows that block doesn't carry
// memory width.  No-mask version: thvm's View has no `mask` field.
//
// Output count <= src->shape.ndim; caller pre-allocates MAX_DIM slots.
typedef struct {
  u32 merged_dim;          // logical size of merged block
  i32 stride;              // stride of the inner-most axis in the block
  u32 expand_real_dim;     // merged_dim if stride!=0 else 0; tracks
                           //   how much memory width the block spans
} MergedDim;

static u32 view_merge_dims(View const *v, MergedDim *out) {
  if (v->shape.ndim == 0) return 0;
  out[0].merged_dim      = v->shape.dims[0];
  out[0].stride          = v->strides[0];
  out[0].expand_real_dim = v->strides[0] ? v->shape.dims[0] : 0;
  u32 n = 1;
  u8 merging = (v->shape.dims[0] == 1);
  for (u32 i = 1; i < v->shape.ndim; i++) {
    u32 s  = v->shape.dims[i];
    i32 st = v->strides[i];
    if (s == 1) continue;                                  // unit axes always merge
    MergedDim *last = &out[n - 1];
    if (merging || last->stride == (i32)s * st) {
      last->merged_dim     *= s;
      last->stride          = st;
      last->expand_real_dim = st ? (merging ? s : last->expand_real_dim * s) : 0;
    } else {
      out[n].merged_dim      = s;
      out[n].stride          = st;
      out[n].expand_real_dim = st ? s : 0;
      n++;
    }
    merging = (s == 1);
  }
  return n;
}

// Tinygrad-faithful reshape that absorbs into a single (possibly non-
// contig) view via _merge_dims when the new shape's axis decomposition
// aligns with the source's contig sub-blocks.  Mirrors
// View.reshape (tinygrad/shape/view.py:267).  Returns 1 on success,
// 0 when no single-view absorb exists (caller chains views).
static int view_apply_reshape(View const *src, u64 expr_loc, View *out) {
  u32 t_ndim  = (u32)term_val(heap_read(expr_loc + 1));
  Shape ts = {0}; ts.ndim = t_ndim;
  u32 t_numel = 1;
  for (u32 i = 0; i < t_ndim; i++) {
    u32 d = (u32)term_val(heap_read(expr_loc + 2 + i));
    ts.dims[i] = d;
    t_numel *= d;
  }
  if (t_numel != src->numel) return 0;
  if (t_ndim > MAX_DIM) return 0;

  // Fast path: contig source -- canonical strides for new shape.
  if (src->contiguous) {
    *out = view_create(ts);
    return 1;
  }

  // Non-contig source: try to express the reshape as new strides
  // walking through src's merged contig sub-blocks, in REVERSE
  // (tinygrad walks from the trailing axis inward).
  MergedDim merged[MAX_DIM];
  u32 n_merged = view_merge_dims(src, merged);

  i32 strides_rev[MAX_DIM];   // collected back-to-front
  u32 strides_n = 0;
  i32 r_idx = (i32)t_ndim - 1;

  for (i32 mi = (i32)n_merged - 1; mi >= 0 && r_idx >= 0; mi--) {
    u32 acc        = 1;
    i32 new_stride = merged[mi].stride;
    u32 real_dim   = merged[mi].expand_real_dim;
    while (acc < merged[mi].merged_dim
        && acc != merged[mi].merged_dim
        && r_idx >= 0) {
      u32 new_dim = ts.dims[r_idx];
      r_idx--;
      strides_rev[strides_n++] = new_stride;
      if (new_dim != 1) {
        acc        *= new_dim;
        new_stride *= (acc < real_dim) ? (i32)new_dim : 0;
      }
    }
    if (acc != merged[mi].merged_dim) return 0;   // mismatch -- caller falls back
  }
  // Pad any remaining outer axes with stride 0 (leading 1-dims).
  while ((u32)strides_n < t_ndim) strides_rev[strides_n++] = 0;
  if (strides_n != t_ndim) return 0;

  out->shape = ts;
  out->numel = t_numel;
  out->offset = src->offset;
  for (u32 i = 0; i < t_ndim; i++) out->strides[i] = strides_rev[t_ndim - 1 - i];
  for (u32 i = t_ndim; i < MAX_DIM; i++) out->strides[i] = 0;
  // Contig iff the resulting strides are canonical row-major.
  out->contiguous = 1;
  i32 cs = 1;
  for (i32 i = (i32)t_ndim - 1; i >= 0; i--) {
    if (out->strides[i] != cs) { out->contiguous = 0; break; }
    cs *= (i32)ts.dims[i];
  }
  if (out->offset != 0) out->contiguous = 0;
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
  // Permute on a non-contig source is mathematically fine: just
  // reorder strides + dims to match the new axis order.  The output
  // is contig only if (a) src was contig AND (b) the permutation is
  // identity; the existing `contiguous = identity ? src->contiguous : 0`
  // assignment below already reflects that.
  Shape ts = {0}; ts.ndim = src->shape.ndim;
  out->offset = src->offset;
  u8 used[MAX_DIM] = {0};
  u8 identity = 1;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    u32 p = (u32)term_val(heap_read(expr_loc + 2 + i));
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
  // Shrink on a non-contig source is mathematically fine: bump
  // offset by `b * src->strides[i]` per axis, keep src strides.
  // The output is contig only if (a) src was contig AND (b) the
  // shrink doesn't actually drop any element (numel preserved).
  Shape ts = {0}; ts.ndim = src->shape.ndim;
  i32 add_off = 0;
  u32 t_numel = 1;
  for (u32 i = 0; i < src->shape.ndim; i++) {
    u32 b = (u32)term_val(heap_read(expr_loc + 2 + 2 * i));
    u32 e = (u32)term_val(heap_read(expr_loc + 3 + 2 * i));
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
  // Flip on a non-contig source is mathematically fine: negate
  // the per-axis stride and bump offset by (dim-1)*src_stride.
  // The output is contig only if no axis was actually flipped AND
  // src was contig.  `out->contiguous = any ? 0 : src->contiguous`
  // below already captures that.
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
  // Write the dtype-correctly-sized scalar.  For F32 the bits field
  // already carries the IEEE-754 layout; integer dtypes interpret
  // `bits` as an i32 (sign-extended at the WL bridge) and pack down
  // to the narrow width.
  u8  buf8;  u16 buf16;  u32 buf32;  u64 buf64;
  void *src = NULL;  u64 nbytes = 0;
  switch (dtype) {
    case DT_BOOL:   buf8  = (u8)(bits & 1);                     src = &buf8;  nbytes = 1; break;
    case DT_INT8:   buf8  = (u8)(i8)(int32_t)bits;              src = &buf8;  nbytes = 1; break;
    case DT_UINT8:  buf8  = (u8)bits;                           src = &buf8;  nbytes = 1; break;
    case DT_INT16:  buf16 = (u16)(i16)(int32_t)bits;            src = &buf16; nbytes = 2; break;
    case DT_UINT16: buf16 = (u16)bits;                          src = &buf16; nbytes = 2; break;
    case DT_INT32:
    case DT_UINT32:
    case DT_FP32:   buf32 = bits;                               src = &buf32; nbytes = 4; break;
    case DT_INT64:  buf64 = (u64)(i64)(int32_t)bits;            src = &buf64; nbytes = 8; break;
    case DT_UINT64: buf64 = (u64)bits;                          src = &buf64; nbytes = 8; break;
    case DT_FP16:
    case DT_BF16:
    case DT_FP64:
    case DT_FP8E4M3:
    case DT_FP8E5M2: {
      // Promote f32 bits -> target float (lossy for 64-bit beyond
      // f32 precision; precise enough for the common 0.0 / 1.0 /
      // log(2) literals the grad chain rule emits).
      f32 v; memcpy(&v, &bits, sizeof(v));
      static u8 buf_bytes[8];
      from_fp32_lane(buf_bytes, dtype, &v, 1);
      src = buf_bytes; nbytes = dtype_storage_bytes(dtype, 1);
      break;
    }
    case DT_INT4: {
      static u8 nibble_byte;
      i8 v8 = (i8)((i32)bits);
      pack_int4(&nibble_byte, &v8, 1);
      src = &nibble_byte; nbytes = 1;
      break;
    }
    case DT_UINT4: {
      static u8 nibble_byte;
      u8 v8 = (u8)(bits & 0xFu);
      pack_uint4(&nibble_byte, &v8, 1);
      src = &nibble_byte; nbytes = 1;
      break;
    }
    default:
      // Larger / packed dtypes need a 64-bit payload (Phase D/F).
      // Fall back to a zero-fill so we don't write garbage past the
      // buffer end; caller will see all-zeros and fail visibly.
      buf64 = 0; src = &buf64; nbytes = dtype_storage_bytes(dtype, 1);
      break;
  }
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src, nbytes);
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
  if (tag == TAG_TEN) {
    u32 tid = (u32)term_val(t);
    // Packed nibble dtypes can't ride the view-only path: every
    // gather step in materialize_root_alias and the cpu_interpret
    // pre-mat loop is byte-aligned (1/2/4/8) and packed itemsize
    // is 0.  Force a kernel emit so cpu_op_run_via_i8 handles the
    // unpack/repack for movement ops.
    if (tid != 0 && tid < TENS_NEXT && dtype_is_packed(TENS[tid].dtype)) return 0;
    return tid;
  }
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
  if (ok) return tensor_view_of(src_tid, nv);
  // Single-view absorb failed.  Caller falls back to emitting a
  // kernel op (cpu_op_reshape memcpy, etc.) -- that's strictly
  // faster than chain-appending and forcing per-kernel
  // tendesc_strided_index pre-mat.  Auto chain-append regressed
  // LeNet badly because every EXPAND->RESHAPE in the gy lift
  // chain produced a multi-view that pre-mat had to walk per
  // element.  The chain-append API
  // (tensor_view_chain_append) stays available for code that
  // EXPLICITLY needs a stride-trick view no kernel-op-emit path
  // can produce -- e.g. tinygrad-style im2col -- where the
  // upfront chain cost is paid back by collapsing kh*kw
  // partial-sum kernels into one sgemm.
  return 0;
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
  // Skip the gather only when EVERYTHING is contig: public view is
  // contig with no offset AND no ShapeTracker chain (which would
  // map a contig outer view through non-contig inner views).
  if (d->view.contiguous && d->view.offset == 0 && d->nviews == 0) return t;

  u32 dst_tid = tensor_alloc(d->backend, d->view.shape, d->dtype);
  if (dst_tid == 0) return t;

  // Bytes to read = max element index reachable + 1.  When there's
  // no chain we can compute it from strides analytically (cheap).
  // With a chain we'd need the full per-element walk -- defer that
  // to the gather loop below by allocating enough to cover the
  // underlying buffer's true size if the backend reports it.
  u32 max_idx = 0;
  if (d->nviews == 0) {
    i32 m = d->view.offset;
    for (u32 i = 0; i < d->view.shape.ndim; i++) {
      if (d->view.shape.dims[i] > 1 && d->view.strides[i] > 0)
        m += (i32)(d->view.shape.dims[i] - 1) * d->view.strides[i];
    }
    max_idx = (u32)m;
  } else {
    for (u32 k = 0; k < d->view.numel; k++) {
      u32 bidx = tendesc_strided_index(d, k);
      if (bidx > max_idx) max_idx = bidx;
    }
  }
  size_t src_bytes = (size_t)dtype_storage_bytes(d->dtype, (u64)(max_idx + 1));
  void  *raw       = malloc(src_bytes);
  d->backend->buf_read(d->buf_id, raw, src_bytes);
  size_t dst_bytes = (size_t)dtype_storage_bytes(d->dtype, d->view.numel);
  void *dst_host = malloc(dst_bytes);
  if (d->dtype == DT_FP32) {
    f32 *o = (f32 *)dst_host; f32 *s = (f32 *)raw;
    for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)];
  } else {
    switch (dtype_itemsize(d->dtype)) {
      case 1: { u8  *o = (u8  *)dst_host, *s = (u8  *)raw;
                for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)]; break; }
      case 2: { u16 *o = (u16 *)dst_host, *s = (u16 *)raw;
                for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)]; break; }
      case 4: { u32 *o = (u32 *)dst_host, *s = (u32 *)raw;
                for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)]; break; }
      case 8: { u64 *o = (u64 *)dst_host, *s = (u64 *)raw;
                for (u32 k = 0; k < d->view.numel; k++) o[k] = s[tendesc_strided_index(d, k)]; break; }
      default:
        free(raw); free(dst_host); tensor_release(dst_tid); return t;
    }
  }
  d->backend->buf_write(TENS[dst_tid].buf_id, dst_host, dst_bytes);
  free(raw);
  free(dst_host);
  return term_new(0, TAG_TEN, d->dtype, dst_tid);
}

// === build_kernel: visit() recursion (g2b) ===

static u32 boundary_index_for_loc(u64 loc) {
  u32 h = boundary_hash_of(loc);
  for (u32 probe = 0; probe < BOUNDARY_HASH_CAP; probe++) {
    u32 i = (h + probe) & (BOUNDARY_HASH_CAP - 1);
    u32 idx = BOUNDARY_HASH[i];
    if (idx == BOUNDARY_HASH_EMPTY) return 0xFFFFFFFFu;
    if (idx < BOUNDARY_ORDER_LEN && BOUNDARY_ORDER[idx] == loc) return idx;
  }
  return 0xFFFFFFFFu;
}

static u32 input_slot_dedup(KernelEntry *ke, u32 tid, Term term) {
  for (u32 i = 0; i < ke->n_inputs; i++)
    if (ke->input_tids[i] == tid && ke->input_terms[i] == term) return i;
  kernel_inputs_reserve(ke, ke->n_inputs + 1);
  u32 slot = ke->n_inputs++;
  ke->input_tids   [slot] = tid;
  ke->input_dtypes [slot] = TENS[tid].dtype;
  ke->input_numels [slot] = TENS[tid].view.numel;
  ke->input_terms  [slot] = term;
  ke->input_views  [slot] = TENS[tid].view;     // codegen consumes for strided reads
  return slot;
}

// Symbolic input slot: a TVAR whose binding LAM has a shape
// annotation but no concrete TEN yet (pre APP-LAM beta).
// `tid = 0` flags the slot as needing fire-time resolution
// (interact_kernel sees tid==0 + term!=0 and term_resolves).
static u32 input_slot_dedup_var(KernelEntry *ke, Term var_term,
                                 u32 dtype, u32 numel) {
  for (u32 i = 0; i < ke->n_inputs; i++)
    if (ke->input_tids[i] == 0 && ke->input_terms[i] == var_term) return i;
  kernel_inputs_reserve(ke, ke->n_inputs + 1);
  u32 slot = ke->n_inputs++;
  ke->input_tids   [slot] = 0;          // resolved at fire time
  ke->input_dtypes [slot] = dtype;
  ke->input_numels [slot] = numel;
  ke->input_terms  [slot] = var_term;
  // Symbolic input: shape-annotated but no concrete strides until
  // fire time.  Synthesize a canonical contig view at the annotated
  // shape so codegen doesn't try to emit strided reads against
  // garbage stride bytes.
  Shape s = {0}; s.ndim = 1; s.dims[0] = numel;
  ke->input_views  [slot] = view_create(s);
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
  // Resolve VAR (SUB-bit) + ALO (one-layer force) chains so a body
  // post-APP-LAM-beta exposes its bound argument's TEN/UOP rather
  // than the bare VAR cell that visit() would otherwise bail on.
  // term_resolve is a pure pointer hop -- no allocation, no firing.
  t = term_resolve(t);
  u8 tag = term_tag(t);

  // DP1_GRAD projections are driven by wnf's uop_drive_inner_actives
  // before materialize ever sees them.  If one survives into visit
  // (e.g. shape inference happening too early), fall through to the
  // VISIT_BAIL at line 641 -- the realize loop will iterate, wnf
  // will fire it next pass, and re-enter materialize.  Materialize
  // is graph -> kernel compile, NEVER fires interactions.

  if (tag == TAG_TEN) {
    u32 tid  = (u32)term_val(t);
    u32 slot = input_slot_dedup(ke, tid, t);
    if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
    return KSRC_AS_INPUT(slot);
  }
  // Shape-annotated TVAR: bound by a TLamShape whose annotation
  // sits in the lam_shape side table.  Treat as a symbolic input
  // slot: the kernel program references KSRC_AS_INPUT(slot), and
  // at fire time interact_kernel resolves input_terms[slot]
  // (the VAR Term) through SUB to whatever APP-LAM beta has
  // bound it to -- typically the recursive-loop iter's current
  // weight tensor.  Lets a lambda body materialize ONCE without
  // waiting for substitution; the kernel-program cache then
  // dedups this kernel against future structurally identical
  // emissions from re-instantiations of the same body.
  if (tag == TAG_VAR) {
    Shape s;
    if (lam_shape_lookup(term_val(t), &s)) {
      u32 numel = 1;
      for (u32 i = 0; i < s.ndim; i++) numel *= s.dims[i];
      u32 slot = input_slot_dedup_var(ke, t, DT_FP32, numel);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      return KSRC_AS_INPUT(slot);
    }
    return VISIT_BAIL;          // no shape annotation -- can't compile
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
    kernel_program_reserve(ke, ke->n_ops + 1);
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
    kernel_program_reserve(ke, ke->n_ops + 1);
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode = (u8)op;
    p->dtype  = src_dtype(ke, src_idx);
    p->numel  = src_numel(ke, src_idx);
    p->n_src  = 1;
    p->src[0] = src_idx;
    return ke->n_ops - 1;
  }

  if (op == UOP_CAST || op == UOP_BITCAST) {
    u32 src_idx = visit(heap_read(loc), ke, root_loc);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    Term num = heap_read(loc + 1);
    if (term_tag(num) != TAG_NUM) return VISIT_BAIL;
    u32 dst_dtype = (u32)term_val(num);
    kernel_program_reserve(ke, ke->n_ops + 1);
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode = (u8)op;
    p->dtype  = dst_dtype;
    // arg carries the source dtype so the kernel can route through
    // to_fp32_lane / from_fp32_lane (see backend/cpu/op/cast.c).
    p->arg    = src_dtype(ke, src_idx);
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
    kernel_program_reserve(ke, ke->n_ops + 1);
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
  // (UOP_GRAD/UOP_FWD moved to TAG_DP{0,1}+DUP_GRAD_FLAG -- the
  // visit-time term_resolve at the top of this function already
  // bails on TAG_DP* via the TAG_UOP-only filter.)

  // Movement ops as a child of the kernel: try view-only resolve
  // first.  If the source isn't a contig TenDesc-resolvable chain
  // (e.g., EXPAND wrapping a MUL from interact_grad), fall through
  // to kernel-op emit with the appropriate metadata.
  if (op_is_view_movement(op)) {
    u32 alias_tid = view_resolve(t);
    if (alias_tid != 0) {
      u32 slot = input_slot_dedup(ke, alias_tid, t);
      if (slot == 0xFFFFFFFFu) return VISIT_BAIL;
      return KSRC_AS_INPUT(slot);
    }
    // Fallback: emit as a kernel op.  Recurse into source, look up
    // shapes, populate the metadata cpu_op_<op> + Metal shaders need.
    u32 src_idx = visit(heap_read(loc), ke, root_loc);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    kernel_program_reserve(ke, ke->n_ops + 1);
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
        u32 pi = (u32)term_val(heap_read(loc + 2 + i));
        p->axis_perm[i] = (u8)(pi & 0xFF);
      }
    }
    if (op == UOP_SHRINK) {
      for (u32 i = 0; i < src_shape.ndim; i++) {
        u32 b = (u32)term_val(heap_read(loc + 2 + 2 * i));
        u32 e = (u32)term_val(heap_read(loc + 3 + 2 * i));
        p->pad_widths[2 * i + 0] = (u8)(b & 0xFF);
        p->pad_widths[2 * i + 1] = (u8)(e & 0xFF);
      }
    }
    if (op == UOP_FLIP) {
      // axes_mask sits in the wrapping UOP cell's ext field
      // (uop_flip stuffs it via term_new's ext).  Wait -- actually
      // uop_flip puts the mask in heap[loc+1] as a NUM cell.  Check
      // the constructor.
      Term mask_num = heap_read(loc + 1);
      if (term_tag(mask_num) == TAG_NUM) p->arg = (u32)term_val(mask_num);
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
    kernel_program_reserve(ke, ke->n_ops + 1);
    // Source shape: from the PAD's source term (TenDesc lookup).
    Shape src_shape = {0};
    if (!term_shape_in(heap_read(loc), 0, &src_shape)) return VISIT_BAIL;
    // Output shape: src.dim[i] + b_i + e_i per axis.
    Shape out_shape = src_shape;
    u32   out_numel = 1;
    for (u32 i = 0; i < src_shape.ndim; i++) {
      u32 b = (u32)term_val(heap_read(loc + 2 + 2 * i));
      u32 e = (u32)term_val(heap_read(loc + 3 + 2 * i));
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
      u32 b = (u32)term_val(heap_read(loc + 2 + 2 * i));
      u32 e = (u32)term_val(heap_read(loc + 3 + 2 * i));
      p->pad_widths[2 * i + 0] = (u8)(b & 0xFF);
      p->pad_widths[2 * i + 1] = (u8)(e & 0xFF);
    }
    return ke->n_ops - 1;
  }

  // REDUCE -- as the kernel root (tail-fuse) or as an intermediate
  // op whose result is consumed elementwise (broadcast) by later
  // program ops.  The "at most one REDUCE per kernel" invariant
  // (used by cg_emit's reduce-tail / reduce-broadcast modes and by
  // cpu_op_reduce's per-output indexing) is enforced by counting
  // REDUCEs already in the program.
  if (op == UOP_REDUCE) {
    ReduceChainInfo rc;
    if (reduce_chain_collect(t, &rc)) {
      int chain_inlined = 1;
      for (u32 j = 1; j < rc.n_reduces; j++) {
        u32 cidx = realize_info_find(rc.locs[j]);
        if (cidx != 0xFFFFFFFFu && REALIZE_INFO[cidx].realized) {
          chain_inlined = 0;
          break;
        }
      }
      if (!chain_inlined) goto single_reduce_emit;
      if (loc != root_loc) {
        for (u32 i = 0; i < ke->n_ops; i++) {
          if (ke->program[i].opcode == UOP_REDUCE) return VISIT_BAIL;
        }
      }
      u32 src_idx = visit(rc.src, ke, root_loc);
      if (src_idx == VISIT_BAIL) return VISIT_BAIL;
      for (u32 i = 0; i < ke->n_ops; i++) {
        if (ke->program[i].opcode == UOP_REDUCE) return VISIT_BAIL;
      }
      kernel_program_reserve(ke, ke->n_ops + 1);
      KProgOp *p = &ke->program[ke->n_ops++];
      memset(p, 0, sizeof(*p));
      p->opcode = UOP_REDUCE;
      p->dtype  = src_dtype(ke, src_idx);
      p->arg    = (rc.kind << 24) | (rc.inner & 0x00FFFFFFu);
      p->numel  = rc.out_numel;
      p->n_src  = 1;
      p->src[0] = src_idx;
      return ke->n_ops - 1;
    }

  single_reduce_emit:
    if (loc != root_loc) {
      // Non-root REDUCE: only allow ONE per kernel.
      for (u32 i = 0; i < ke->n_ops; i++) {
        if (ke->program[i].opcode == UOP_REDUCE) return VISIT_BAIL;
      }
    }
    u32 src_idx = visit(heap_read(loc), ke, root_loc);
    if (src_idx == VISIT_BAIL) return VISIT_BAIL;
    kernel_program_reserve(ke, ke->n_ops + 1);
    u32 kind = (u32)term_val(heap_read(loc + 1));
    u32 axis = (u32)term_val(heap_read(loc + 2));
    // arg encoding (cpu_op_reduce / Metal reduce shader):
    //   bits 24..31 = kind, bits 0..23 = inner = prod(dims[axis+1..]).
    // Packing axis here (the round-1 bug) gave inner = 0 -> fallback
    // to 1 -> axis treated as innermost, which is correct only when
    // axis IS innermost.  Compute inner from the source shape.
    Shape src_shape = {0};
    u32 inner = 1;
    u32 src_numel_total = src_numel(ke, src_idx);
    if (term_shape_in(heap_read(loc), 0, &src_shape)) {
      for (u32 i = axis + 1; i < src_shape.ndim; i++) inner *= src_shape.dims[i];
    }
    // Output numel of THIS REDUCE op (not necessarily the kernel's
    // output): src_numel / axis_size.  axis_size = src_shape.dims[axis]
    // when shape is known; otherwise fall back to the kernel's
    // output_numel (root-REDUCE case).
    u32 reduce_numel = ke->output_numel;
    if (src_shape.ndim > axis) {
      u32 axis_size = src_shape.dims[axis] ? src_shape.dims[axis] : 1;
      reduce_numel = src_numel_total / axis_size;
    }
    KProgOp *p = &ke->program[ke->n_ops++];
    memset(p, 0, sizeof(*p));
    p->opcode = UOP_REDUCE;
    p->dtype  = src_dtype(ke, src_idx);
    p->arg    = (kind << 24) | (inner & 0x00FFFFFFu);
    p->numel  = reduce_numel;
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
  u32 out_dtype = DT_FP32;
  term_dtype_in(root_term, 0, &out_dtype);

  // Memory planner: push any earlier kernel's output buf onto the
  // backend freelist if its last consumer (in alloc-depth terms)
  // has already emitted.  cpu_buf_alloc / metal_buf_alloc inside
  // tensor_alloc below then pop a same-nbytes match instead of
  // growing the buf table.
  u32 this_depth = BOUNDARY_DEPTH[idx];
  mem_plan_push_dead(this_depth);

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

  // Degenerate case: visit() consumed the whole boundary subgraph as
  // a single input slot (n_ops == 0, result == KSRC_AS_INPUT).  This
  // happens when the boundary's root is a movement-op chain whose
  // view_resolve found a direct alias TenDesc -- no compute to do.
  // Without this branch the kernel commits with an empty program;
  // cpu_interpret runs nothing; the alloc'd output buffer stays
  // zero-initialized, silently zeroing whatever signal was supposed
  // to flow through (the gy=CONST(1.0) seed in MSE backward, etc).
  // Skip kernel emission and alias the boundary's output to the
  // input tid directly.
  if (ke->n_ops == 0 && KSRC_IS_INPUT(result)) {
    u32 alias_tid = ke->input_tids[KSRC_INDEX(result)];
    if (alias_tid != 0 && alias_tid < TENS_NEXT) {
      // Release the unused output_tid we speculatively allocated.
      tensor_release(out_tid);
      kernel_dealloc_last(kid);
      Term alias_term = term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
      BOUNDARY_TID [bi] = alias_tid;
      BOUNDARY_TERM[bi] = alias_term;
      return alias_term;
    }
  }

  // Hash-cons the KProgOp[] against the kernel-program cache.
  // Two boundaries with bit-for-bit identical programs (opcode +
  // dtype + n_src + arg + numel + src[] + shape/perm/pad bytes)
  // share the underlying program array.  Memory savings on
  // recursive lambda loops where each iter emits a structurally
  // identical step kernel; correctness is unchanged because
  // input_tids[] / output_tid stay per-kernel.
  // Phase 16: kernel-program-cache slot owns the shared `KernelAxes`
  // so every kid with the same KProgOp[] sees the same opts.  Apply
  // once -> propagates to all sharing kids.  For kernels that don't
  // make it into the cache (n_ops == 0 OR cache full), fall back to
  // KernelEntry._local_axes.
  KpCacheSlot *slot = NULL;
  if (ke->n_ops > 0) {
    slot = kernel_program_cache_lookup_slot(ke->program, ke->n_ops);
    if (slot != NULL) {
      free(ke->program);
      ke->program        = slot->program;
      ke->n_ops          = slot->n_ops;
      ke->ops_cap        = slot->n_ops;
      ke->program_shared = 1;
    } else {
      slot = kernel_program_cache_insert_slot(ke->program, ke->n_ops);
      if (slot != NULL) {
        free(ke->program);
        ke->program        = slot->program;
        ke->ops_cap        = ke->n_ops;
        ke->program_shared = 1;
      }
      // (cache full -- silently keep the kernel-owned copy; axes
      //  fall back to _local_axes below)
    }
  }
  ke->axes = (slot != NULL) ? &slot->axes : &ke->_local_axes;

  // Default-init the axis-typed scheduling plan now that the program
  // and output_shape are finalized.  Idempotent: a cached slot whose
  // axes were already populated by an earlier kid sharing this
  // program is a no-op, so opts already applied to the program shape
  // survive across new kid emissions.
  axes_default_for(ke);

  // Rangeify lowering: produce a parallel scalar-UOp form alongside
  // the legacy KProgOp[].  When the lowering succeeds, seed the
  // TileUop schedule plan above it; opt-in tile dispatch can consume
  // that plan while default dispatch still routes through the existing
  // scalar/KProgOp paths.  When rangeify bails (op not yet supported,
  // broadcast pattern not handled, etc.), the legacy KProgOp[]
  // dispatch runs and scalar_uops/tile_uops stay empty.  DEFAULT-ON
  // as of Phase E -- THVM_RANGEIFY=0 disables and reverts to the
  // legacy emit path for every kernel.
  // Reads getenv per emit (cheap; ~1us) so test harnesses can flip
  // the flag mid-session without restarting the runtime.
  {
    const char *e = getenv("THVM_RANGEIFY");
    int rangeify_on = (e == NULL) ? 1 : (e[0] != '0');
    int lowered = rangeify_on && rangeify_try_lower_elementwise(ke);
    if (lowered) {
      rangeify_cse(ke);
      rangeify_dce(ke);
      axes_ensure_scalar_reduce(ke);
    }
    if (!ke->program_shared) {
      KernelAxes *shared_axes = kernel_rangeified_axes_cache_lookup_or_insert(ke);
      if (shared_axes != NULL) {
        ke->axes = shared_axes;
      }
    }
    if (lowered) {
      axes_ensure_scalar_reduce(ke);
      tile_sync_from_scalar(ke);
    }
  }

  u64 kloc = heap_alloc(2);
  heap_set(kloc + 0, term_new(0, TAG_TEN, out_dtype, out_tid));
  heap_set(kloc + 1, term_new(0, TAG_NUM, DT_INT32, kid));
  Term kernel_term = term_new(0, TAG_UOP, UOP_KERNEL, kloc);

  // Pin a heap cell carrying the UOP_KERNEL Term itself so heap-walk
  // discovery (e.g. THeapDiagram, gc_mark) sees every emitted kernel,
  // not only the sink that gets returned to the WL handle.  Without
  // this pin, only the sink kernel_term is reachable (via the WL
  // surface Term); the non-sink kernels' Terms live in BOUNDARY_TERM[]
  // C-side scratch and never become heap-resident.  Cost: one Term
  // (~8B) per emitted kernel; the pinned cell is read-only (the Term
  // it holds is identical to BOUNDARY_TERM[bi], no aliasing concern).
  u64 pin = heap_alloc(1);
  heap_set(pin, kernel_term);

  BOUNDARY_TID [bi] = out_tid;
  BOUNDARY_TERM[bi] = kernel_term;

  // Record this output buf so a later-depth emit can recycle it.
  // last_use_depth = 0 means "no consumer is itself a realize
  // boundary" -- typically the realize root + any preserved orphan;
  // those never get pushed (the threshold mem_plan_push_dead checks
  // is `last_use < current_depth`, which 0 satisfies for any
  // current_depth >= 1, so we'd freelist-push the root and the
  // caller would read freed bytes).  Skip recording in that case.
  // Recycle only single-consumer outputs.  Multi-consumer ones may
  // be aliased through DUP/SUP / read by interactions outside the
  // realize-info-tracked DAG (e.g. UOP_ASSIGN in optimizer loops),
  // and their true last_use isn't always equal to BOUNDARY_LAST_USE.
  // The Phase-3 fusion relaxation also lets some non-realized
  // intermediates feed multiple consumers without a shared buf;
  // restricting recycling here keeps those cases safe.  Phase 9
  // can lift this guard once the planner has explicit ASSIGN +
  // DUP-aware lifetime tracking.
  if (BOUNDARY_LAST_USE[idx] > 0
      && REALIZE_INFO[idx].consumer_count == 1) {
    mem_plan_record(TENS[out_tid].buf_id,
                    BOUNDARY_LAST_USE[idx],
                    CURRENT_BACKEND);
  }

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

// Recursive descent: walk a UOP DAG looking for UOP_ASSIGN nodes
// at any depth.  Each ASSIGN's src subgraph is materialized in
// place (heap_set on cell+1) so the surrounding kernel-emission
// pass sees a kernel chain producing a TEN, not a raw UOP graph.
// Bottoms out at non-UOP tags and at UOP_KERNEL (already materialized).
// Idempotent across re-entries via the early returns inside
// thvm_materialize.
static void materialize_inner_assigns(Term term) {
  if (term_tag(term) != TAG_UOP) return;
  u32 op = term_ext(term);
  if (op == UOP_KERNEL) return;
  u8 ar = uop_arity((u8)op);
  if (ar == 0) return;
  u64 loc = term_val(term);
  for (u8 i = 0; i < ar; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) != TAG_UOP) continue;
    if (term_ext(child) == UOP_ASSIGN) {
      u64  cloc     = term_val(child);
      Term csrc     = heap_read(cloc + 1);
      Term csrc_mat = thvm_materialize(csrc);
      if (csrc_mat != csrc) heap_set(cloc + 1, csrc_mat);
    } else {
      materialize_inner_assigns(child);
    }
  }
}

fn Term thvm_materialize(Term term) {
  HOT_MATERIALIZE_CALLS++;
  // REF / ALO transparency: jump (don't unfold) into the body cell.
  // term_resolve walks VAR-SUB and ALO chains -- pure pointer hops,
  // no heap allocation.  TAG_REF jumps directly to DEFS[name], the
  // book-heap pointer registered by TDef -- still no allocation, no
  // rewriting of the original term, just reading where the body
  // lives.  Cap at 8 hops as a safety net against degenerate
  // self-referencing REF chains; in practice 1-2 suffices.
  for (int hops = 0; hops < 8; hops++) {
    Term resolved = term_resolve(term);
    if (term_tag(resolved) == TAG_REF) {
      u32 name = term_ext(resolved);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0) break;
      term = book;
      continue;
    }
    if (resolved == term) break;
    term = resolved;
  }

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
  // Compound IC nodes (APP/LAM/SUP/DUP/OP2/MAT/ALO): walk children
  // in-place so a single TMaterialize call on a full recursive
  // training term -- e.g. TPri[loss, ASSIGN(...)] = APP(APP(APP(PRI),
  // loss), step) at root, or TLam[k, body] for the loop wrapper --
  // descends to find the embedded UOP graphs and compiles each one.
  // Children are materialized in place (heap_set on the original
  // cell) so the surrounding structure stays intact for wnf to drive
  // at fire time.  Atoms (NUM/TEN/REF/ERA/VAR) are leaves -- nothing
  // to materialize, recursion bottoms out via the early returns above.
  {
    u8 t = term_tag(term);
    u32 ar = 0;
    switch (t) {
      case TAG_APP: case TAG_SUP: case TAG_OP2: case TAG_MAT:
      case TAG_ALO:
        ar = 2; break;
      case TAG_LAM: case TAG_DUP:
        ar = 1; break;
      default: ar = 0; break;
    }
    if (ar > 0) {
      u64 loc = term_val(term);
      for (u32 i = 0; i < ar; i++) {
        Term child = heap_read(loc + i);
        Term child_mat = thvm_materialize(child);
        if (child_mat != child) heap_set(loc + i, child_mat);
      }
      return term;
    }
  }
  if (term_tag(term) != TAG_UOP)        return term;
  if (term_ext(term) == UOP_KERNEL)     return term;
  // GRAD is a stop point in materialize -- wnf fires interact_grad,
  // then thvm_realize loops back here to compile the unrolled UOps.
  // ASSIGN is a wnf-fired primitive (interact_assign) -- not a kernel.
  // Materialize the SRC subgraph so its kernels are compiled, then
  // re-wrap as ASSIGN(dst, materialized_src).  Wnf later fires the
  // src kernels, lands a TEN in heap[loc+1], and interact_assign
  // memcpys it into dst.buf.
  if (term_ext(term) == UOP_ASSIGN) {
    u64  loc        = term_val(term);
    Term dst_cell   = heap_read(loc + 0);
    Term src_cell   = heap_read(loc + 1);
    Term src_mat    = thvm_materialize(src_cell);
    if (src_mat != src_cell) heap_set(loc + 1, src_mat);
    (void)dst_cell;
    return term;
  }
  // Pre-walk: recursively scan the UOP DAG for NESTED ASSIGNs and
  // materialize each one's src subgraph in place.  Without this, an
  // ASSIGN buried inside the src of an outer ASSIGN (or several
  // levels deep in a compound UOP -- e.g. Adam's
  // `w - lr * mAfter / denom`) keeps a raw UOP src that wnf can't
  // reduce to a TEN, so the ASSIGN never fires.
  materialize_inner_assigns(term);

  Term simplified = uop_graph_simplify_materialize(term, 0);
  if (simplified != term) {
    return thvm_materialize(simplified);
  }

  if (term_ext(term) == UOP_CONST) {
    u32 tid = const_to_tendesc(term_val(term));
    return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
  }

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
  mem_plan_reset();
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
      // Rewind KERNELS_NEXT, freeing per-kernel heap arrays for the
      // kernels emitted in this attempt (each kernel_alloc grew the
      // input/program arrays via realloc; without this loop they'd
      // leak).
      for (u32 r = kernels_at_start; r < KERNELS_NEXT; r++)
        kernel_free_arrays(&KERNELS[r]);
      KERNELS_NEXT = kernels_at_start;
      TENS_NEXT    = tens_at_start;
      mem_plan_drain_freelist();
      return term;
    }
    if (BOUNDARY_ORDER[i] == term_val(term)) sink_kernel = k;
  }
  // End-of-pass: pop any planner-pushed bufs still on CPU_FREELIST so
  // a subsequent thvm_realize -> materialize iteration doesn't pull
  // from them (the chain rule's freshly-emitted UOPs may still
  // reference those tids).
  mem_plan_drain_freelist();
  return sink_kernel != 0 ? sink_kernel : term;
}
