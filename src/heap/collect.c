// heap/collect.c -- Cheney copying GC over two semi-spaces of HEAP.  After a
// realize under heap or device pressure (thvm_realize_maybe_gc) the roots and
// every side table holding Terms are evacuated; tables keyed by loc re-key
// through the forward pointers (an evacuated cell's loc[0], tagged GC_FWD_TAG,
// past TAG_COUNT), and the hash-cons and upload caches are cleared.

#define GC_FWD_TAG 0x7E
#define GC_MARGIN  1024

static int GC_ENABLED    = 0;
static u64 GC_SPACE_SZ   = 0;
static u64 GC_FROM_START = 0;
static u64 GC_FROM_END   = 0;
static u64 GC_TO_START   = 0;
static u64 GC_TO_END     = 0;
static u64 GC_COUNT      = 0;
static u64 HEAP_PERM_USED = 0;   // permanent-tail cells handed out

// THVM_HEAP_CELLS, else HEAP_CAP; an unparseable or out-of-range value falls
// back to HEAP_CAP.
fn u64 thvm_heap_cells(void) {
  static u64 cells = 0;
  if (cells == 0) {
    cells = HEAP_CAP;
    const char *e = getenv("THVM_HEAP_CELLS");
    if (e != NULL && *e != '\0') {
      char *end = NULL;
      unsigned long long v = strtoull(e, &end, 0);
      // The 38-bit VAL field bounds an addressable loc; keep a margin below 2^38.
      if (end != NULL && *end == '\0' && v >= (1ULL << 20) && v <= (1ULL << 37)) {
        cells = (u64)v;
      }
    }
  }
  return cells;
}

// THVM_BOOK_CELLS, else BOOK_CAP.  The book heap is mmap-backed, so the large
// default costs no RSS until written.
fn u64 thvm_book_cells(void) {
  static u64 cells = 0;
  if (cells == 0) {
    cells = BOOK_CAP;
    const char *e = getenv("THVM_BOOK_CELLS");
    if (e != NULL && *e != '\0') {
      char *end = NULL;
      unsigned long long v = strtoull(e, &end, 0);
      if (end != NULL && *end == '\0' && v >= (1ULL << 16) && v <= (1ULL << 37)) {
        cells = (u64)v;
      }
    }
  }
  return cells;
}

// Permanent cells past the semi-spaces (THVM_HEAP_PERM_CELLS, default 0).  The
// collector never scans outside from-space, so append-only atom-only nodes a
// run keeps to its end (the --proof trace) cost it nothing here.
static u64 HEAP_PERM_REQ = 0;
// An in-process request instead of setenv, which would slow every uncached
// getenv; it counts only before the heap is first mapped.
fn void thvm_heap_request_perm_cells(u64 cells) { HEAP_PERM_REQ = cells; }

fn u64 thvm_heap_perm_cells(void) {
  static u64 cells = ~0ull;
  if (cells == ~0ull) {
    cells = (HEAP_PERM_REQ <= (1ULL << 30)) ? HEAP_PERM_REQ : 0;
    const char *e = getenv("THVM_HEAP_PERM_CELLS");
    if (e != NULL && *e != '\0') {
      char *end = NULL;
      unsigned long long v = strtoull(e, &end, 0);
      if (end != NULL && *end == '\0' && v <= (1ULL << 30)) cells = (u64)v;
    }
  }
  return cells;
}

fn u64 thvm_heap_map_cells(void) {
  return thvm_heap_cells() + thvm_heap_perm_cells();
}

// Bump-allocate n permanent cells; 0 when the tail is absent or full
// (a real loc is never 0: the tail starts past both semi-spaces).
fn u64 heap_perm_alloc(u64 n) {
  if (n == 0 || HEAP_PERM_USED + n > thvm_heap_perm_cells()) return 0;
  u64 loc = thvm_heap_cells() + HEAP_PERM_USED;
  HEAP_PERM_USED += n;
  return loc;
}


fn void gc_reset(void) {
  GC_ENABLED    = 0;
  GC_SPACE_SZ   = 0;
  GC_FROM_START = 0;
  GC_FROM_END   = 0;
  GC_TO_START   = 0;
  GC_TO_END     = 0;
  GC_COUNT      = 0;
}

// First allocatable cell of a semi-space.  Loc 0 reads as "none" (an unbound
// book binder, assign_fire_claim, heap_perm_alloc), so no node lives there.
static u64 gc_space_base(u64 start) {
  return start == 0 ? 1 : start;
}

fn void gc_init(u64 space_words) {
  if (2 * space_words > thvm_heap_cells()) {
    fprintf(stderr, "gc_init: %llu cells x 2 exceeds heap=%llu\n",
            (unsigned long long)space_words,
            (unsigned long long)thvm_heap_cells());
    exit(1);
  }
  GC_ENABLED    = 1;
  GC_SPACE_SZ   = space_words;
  GC_FROM_START = 0;
  GC_FROM_END   = space_words;
  GC_TO_START   = space_words;
  GC_TO_END     = 2 * space_words;
  GC_COUNT      = 0;
  HEAP_PERM_USED = 0;
  HEAP_NEXT = gc_space_base(GC_FROM_START);
}

fn u64 gc_count(void)        { return GC_COUNT; }
fn int gc_enabled(void)      { return GC_ENABLED; }
fn u64 gc_from_start(void)   { return GC_FROM_START; }
fn u64 gc_from_end(void)     { return GC_FROM_END; }

static Term gc_evacuate(Term t, u64 *alloc);

// States of the unforced ALOs this collection moves: their chain entries name
// dyn LAM cells (ALO_STATES) and dup cells (DUP_SHARE) that must move too.
static u32 *GC_ALO_WORK     = NULL;
static u32  GC_ALO_WORK_LEN = 0;
static u32  GC_ALO_WORK_CAP = 0;

static void gc_alo_note(u32 state) {
  if (GC_ALO_WORK_LEN == GC_ALO_WORK_CAP) {
    u32  cap = GC_ALO_WORK_CAP ? 2 * GC_ALO_WORK_CAP : 1024;
    u32 *w   = (u32 *)realloc(GC_ALO_WORK, (size_t)cap * sizeof *w);
    if (w == NULL) thvm_fatal("gc: out of memory for the ALO state worklist");
    GC_ALO_WORK     = w;
    GC_ALO_WORK_CAP = cap;
  }
  GC_ALO_WORK[GC_ALO_WORK_LEN++] = state;
}

// Cells below `first` move as they are.
static u64 gc_evacuate_cells(u64 loc, u32 size, u32 first, u64 *alloc) {
  u64 dst = *alloc;
  *alloc += size;
  for (u32 i = 0; i < size; i++) {
    HEAP[dst + i] = HEAP[loc + i];
  }
  HEAP[loc] = term_new(0, GC_FWD_TAG, 0, dst);
  for (u32 i = first; i < size; i++) {
    HEAP[dst + i] = gc_evacuate(HEAP[dst + i], alloc);
  }
  return dst;
}

static Term gc_evacuate(Term t, u64 *alloc) {
  if (t == 0) return 0;
  u8  tag = term_tag(t);
  u64 loc = term_val(t);

  if (tag == TAG_NUM || tag == TAG_TEN || tag == TAG_REF ||
      tag == TAG_ERA || tag == TAG_ANY || tag == TAG_FVR ||
      tag == GC_FWD_TAG) {
    return t;
  }
  // A 0-arg PRI (val 0) owns no cells.
  if (tag == TAG_PRI && loc == 0) return t;

  // Permanent-tail and to-space locs never move.
  if (loc < GC_FROM_START || loc >= GC_FROM_END) return t;

  Term cell0 = HEAP[loc];
  if (term_tag(cell0) == GC_FWD_TAG) {
    u64 new_loc = term_val(cell0);
    return term_new(term_sub_get(t), tag, term_ext(t), new_loc);
  }

  u32 size = term_cells(t, HEAP);
  if (size == 0) return t;

  // An unforced ALO's cell 0 is a book term, its NUM state in cell 1; alo_force
  // leaves the dyn result and an ERA.
  u32 first = (tag == TAG_ALO && term_tag(HEAP[loc + 1]) == TAG_NUM) ? 1 : 0;
  if (first) gc_alo_note((u32)term_val(HEAP[loc + 1]));
  u64 new_loc = gc_evacuate_cells(loc, size, first, alloc);
  return term_new(term_sub_get(t), tag, term_ext(t), new_loc);
}

// Keep and re-point what the live ALO states name: each chain entry's LAM cell
// and the dup cell DUP_SHARE holds for a live state (state 0 included, the REF
// unfold's).  Moving a cell can reach more unforced ALOs, so this runs until no
// state turns live and no entry moves.
static void gc_evacuate_alo_states(u64 *alloc) {
  if (GC_ALO_WORK_LEN == 0) return;
  u32 n    = ALO_STATES_NEXT > 0 ? ALO_STATES_NEXT : 1;
  u8 *live = (u8 *)calloc(n, 1);
  if (live == NULL) thvm_fatal("gc: out of memory for the ALO state marks");
  for (;;) {
    while (GC_ALO_WORK_LEN > 0) {
      u32 st = GC_ALO_WORK[--GC_ALO_WORK_LEN];
      if (st >= n || live[st]) continue;
      live[st] = 1;
      if (st != 0) {
        Term v = gc_evacuate(term_new(0, TAG_VAR, 0, ALO_STATES[st].new_loc), alloc);
        ALO_STATES[st].new_loc = term_val(v);
        gc_alo_note(ALO_STATES[st].parent);
      }
    }
    u32 moved = 0;
    for (u32 i = 0; i < DUP_SHARE_SLOTS; i++) {
      DupShareEntry *e = &DUP_SHARE[i];
      if (e->gen != DUP_SHARE_GEN || e->state_id >= n || !live[e->state_id]) continue;
      Term v = gc_evacuate(term_new(0, TAG_VAR, 0, e->dyn_loc), alloc);
      if (term_val(v) != e->dyn_loc) {
        e->dyn_loc = term_val(v);
        moved++;
      }
    }
    if (GC_ALO_WORK_LEN == 0 && moved == 0) break;
  }
  free(live);
}

// Re-key the lam-shape table through the forward pointers, before the swap.
// Book keys, other contexts' keys (LAM_SHAPE_CTX_MASK) and locs outside
// from-space keep their entries; a current-context LAM that died loses its.
static void gc_remap_lam_shape(void) {
  u32 cap = LAM_SHAPE_CAP;
  u64 cur_ctx = ((u64)thvm_context_current() & 0xF) << LAM_SHAPE_CTX_SHIFT;
  typedef struct { u64 key; Shape shape; } Pair;
  Pair *snap = (Pair *)malloc(sizeof(Pair) * cap);
  if (!snap) return;
  u32 n_snap = 0;
  for (u32 i = 0; i < cap; i++) {
    if (LAM_SHAPE_TABLE[i].occupied) {
      snap[n_snap].key   = LAM_SHAPE_TABLE[i].key;
      snap[n_snap].shape = LAM_SHAPE_TABLE[i].shape;
      n_snap++;
    }
  }
  lam_shape_reset();
  for (u32 i = 0; i < n_snap; i++) {
    u64 key = snap[i].key;
    if (key & LAM_SHAPE_BOOK_BIT) {
      lam_shape_set_keyed(key, &snap[i].shape);
      continue;
    }
    if ((key & LAM_SHAPE_CTX_MASK) != cur_ctx) {
      lam_shape_set_keyed(key, &snap[i].shape);
      continue;
    }
    u64 loc = key & ~LAM_SHAPE_CTX_MASK;
    if (loc < GC_FROM_START || loc >= GC_FROM_END) {
      lam_shape_set_keyed(key, &snap[i].shape);
      continue;
    }
    Term cell = HEAP[loc];
    if (term_tag(cell) == GC_FWD_TAG) {
      lam_shape_set_keyed(term_val(cell) | cur_ctx, &snap[i].shape);
    }
  }
  free(snap);
}

static void gc_evacuate_side_tables(u64 *alloc) {
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_LEN; i++) {
    EXTERN_PINNED_TERMS[i] = gc_evacuate(EXTERN_PINNED_TERMS[i], alloc);
  }
  if (EXTERN_PIN_HANDLES != NULL) {
    for (u64 i = 0; i < EXTERN_PIN_HANDLE_CAP; i++) {
      if (EXTERN_PIN_HANDLES[i] != 0) {
        EXTERN_PIN_HANDLES[i] = gc_evacuate(EXTERN_PIN_HANDLES[i], alloc);
      }
    }
  }
  // WNF_LAST_STACK: the frames a step-limited wnf_n left pending.
  for (u32 i = 0; i < WNF_LAST_STACK_LEN; i++) {
    WNF_LAST_STACK[i] = gc_evacuate(WNF_LAST_STACK[i], alloc);
  }
  // TenDesc.grad: a leaf's lazy gradient, read or summed onto after this realize.
  for (u32 t = 0; t < TENS_NEXT; t++) {
    if (TENS[t].grad != 0) {
      TENS[t].grad = gc_evacuate(TENS[t].grad, alloc);
    }
  }
  // KernelEntry Terms (output_tid and input_tids[] are TenDesc ids).  The
  // renderer names buffers by Term identity, so the lifted buffers move too.
  for (u32 k = 0; k < KERNELS_NEXT; k++) {
    KernelEntry *ke = &KERNELS[k];
    if (ke->source_uop != 0) {
      ke->source_uop = gc_evacuate(ke->source_uop, alloc);
    }
    if (ke->compute_bufferize != 0) {
      ke->compute_bufferize = gc_evacuate(ke->compute_bufferize, alloc);
    }
    if (ke->cached_lift.store_root != 0) {
      ke->cached_lift.store_root =
          gc_evacuate(ke->cached_lift.store_root, alloc);
    }
    if (ke->cached_lift_init_root != 0) {
      ke->cached_lift_init_root =
          gc_evacuate(ke->cached_lift_init_root, alloc);
    }
    if (ke->cached_lift.out_buf != 0) {
      ke->cached_lift.out_buf =
          gc_evacuate(ke->cached_lift.out_buf, alloc);
    }
    if (ke->cached_lift.in_bufs != NULL) {
      for (u32 i = 0; i < ke->cached_lift.n_inputs; i++) {
        if (ke->cached_lift.in_bufs[i] != 0) {
          ke->cached_lift.in_bufs[i] =
              gc_evacuate(ke->cached_lift.in_bufs[i], alloc);
        }
      }
    }
    if (ke->input_terms != NULL) {
      for (u32 i = 0; i < ke->n_inputs; i++) {
        if (ke->input_terms[i] != 0) {
          ke->input_terms[i] = gc_evacuate(ke->input_terms[i], alloc);
        }
      }
    }
  }
}

// Where a from-space node's base cell lives after evacuation; ~0 when the
// node died.  A loc outside from-space does not move.
fn u64 gc_forward_loc(u64 loc) {
  if (loc < GC_FROM_START || loc >= GC_FROM_END) {
    return loc;
  }
  Term c = HEAP[loc];
  return term_tag(c) == GC_FWD_TAG ? term_val(c) : ~0ull;
}

// Evacuates `roots` in place; HEAP_NEXT ends just past the live cells.
fn void gc_collect(Term *roots, u32 n_roots) {
  if (!GC_ENABLED) return;

  GC_COUNT++;
  GC_ALO_WORK_LEN = 0;
  u64 alloc = gc_space_base(GC_TO_START);
  materialized_loc_gc_before();

  for (u32 i = 0; i < n_roots; i++) {
    if (roots[i] != 0) roots[i] = gc_evacuate(roots[i], &alloc);
  }

  gc_evacuate_side_tables(&alloc);
  gc_evacuate_alo_states(&alloc);

  // After every evacuation and before the swap: its keys are from-space locs.
  gc_remap_lam_shape();

  // The hash-cons caches would return dangling Terms; they refill on demand.
  uop_const_cache_reset();
  uop_mov_cache_reset();
  // A stale loc would name an unrelated cell: the loc -> tid cache re-keys
  // through the forward pointers, the loc-keyed upload caches are cleared.
  materialized_loc_gc_after();
  copy_upload_cache_reset();
  weight_move_cache_reset();
  kernel_input_upload_reset();

  u64 swap_start = GC_FROM_START, swap_end = GC_FROM_END;
  GC_FROM_START = GC_TO_START;
  GC_FROM_END   = GC_TO_END;
  GC_TO_START   = swap_start;
  GC_TO_END     = swap_end;
  HEAP_NEXT = alloc;
}

