// heap/collect.c -- Cheney-style copying GC for the dyn heap.
//
// Two semi-spaces of equal size carved out of HEAP at gc_init time.
// The active region is from-space; heap_alloc bumps within it.  When
// the realize loop finishes one realize call and notices HEAP_NEXT
// has crossed the high-water mark, it calls gc_collect with the live
// root set; the collector evacuates every reachable cell to to-space
// and swaps the spaces.
//
// Forward pointers: an evacuated cell's loc[0] is overwritten with
// term_new(0, GC_FWD_TAG, 0, new_loc).  GC_FWD_TAG is a tag value
// that no real Term ever uses (TAG_PRI is the highest at 25; we pick
// 0x7E which is well past any real tag and below the 7-bit max).
//
// Side tables that store heap locs/Terms:
//   - lam_shape table: re-keyed via the forward pointers BEFORE the
//     space swap.
//   - uop_const_cache, uop_mov_cache: cleared (rebuildable; the
//     stored Terms would otherwise dangle into the now-unused
//     to-space).
//   - extern_pinned_terms, EXTERN_PIN_HANDLES, DEFS, WNF_LAST_STACK,
//     KernelEntry.source_uop / input_terms[]: re-evacuated as part
//     of the root set so their stored Terms get the new locs.
//   - kernel_program_cache: KProgOp[] holds opcodes / numels / slot
//     indices, no heap locs, so it survives across GC unchanged.

#define GC_FWD_TAG 0x7E
#define GC_MARGIN  1024

static int GC_ENABLED    = 0;
static u64 GC_SPACE_SZ   = 0;
static u64 GC_FROM_START = 0;
static u64 GC_FROM_END   = 0;
static u64 GC_TO_START   = 0;
static u64 GC_TO_END     = 0;
static u64 GC_COUNT      = 0;

fn void gc_reset(void) {
  GC_ENABLED    = 0;
  GC_SPACE_SZ   = 0;
  GC_FROM_START = 0;
  GC_FROM_END   = 0;
  GC_TO_START   = 0;
  GC_TO_END     = 0;
  GC_COUNT      = 0;
}

// space_words: half the desired total heap allowance.  Each semi-
// space gets `space_words` cells; total addressable = 2*space_words
// (must be <= HEAP_CAP).
fn void gc_init(u64 space_words) {
  if (2 * space_words > HEAP_CAP) {
    fprintf(stderr, "gc_init: %llu cells x 2 exceeds HEAP_CAP=%llu\n",
            (unsigned long long)space_words, (unsigned long long)HEAP_CAP);
    exit(1);
  }
  GC_ENABLED    = 1;
  GC_SPACE_SZ   = space_words;
  GC_FROM_START = 0;
  GC_FROM_END   = space_words;
  GC_TO_START   = space_words;
  GC_TO_END     = 2 * space_words;
  GC_COUNT      = 0;
  // Loc 0 is used as the "no binder yet" sentinel in book/clone, so
  // bump the alloc cursor past it.  thvm pre-GC behaviour reserved
  // loc 0 implicitly by initializing HEAP_NEXT to 0 + first alloc.
  HEAP_NEXT = GC_FROM_START;
}

fn u64 gc_count(void)        { return GC_COUNT; }
fn int gc_enabled(void)      { return GC_ENABLED; }
fn u64 gc_from_start(void)   { return GC_FROM_START; }
fn u64 gc_from_end(void)     { return GC_FROM_END; }

// Cells owned by the term at term_val(t).  Returns 0 for atoms / 0
// for unrecognized -- caller treats unrecognized as atom (best
// effort; missing a tag costs correctness, so the switch covers the
// full live tag set).
static u32 gc_node_size(Term t) {
  u8  tag = term_tag(t);
  u64 loc = term_val(t);
  switch (tag) {
    case TAG_NUM: case TAG_TEN: case TAG_REF: case TAG_ERA:
    case TAG_ANY: case TAG_FVR:
      return 0;
    case TAG_LAM: case TAG_DUP: case TAG_INC: case TAG_BRI:
    case TAG_VAR:
      return 1;
    case TAG_DP0: case TAG_DP1:
      return (term_ext(t) & DUP_GRAD_FLAG) ? 3 : 1;
    case TAG_APP: case TAG_SUP: case TAG_OP2: case TAG_MAT:
    case TAG_ALO: case TAG_EQL: case TAG_AND: case TAG_OR:
    case TAG_WHEN: case TAG_ANN:
      return 2;
    case TAG_CTR: {
      Term n_cell = HEAP[loc];
      if (term_tag(n_cell) != TAG_NUM) return 0;
      return 1 + (u32)term_val(n_cell);
    }
    case TAG_PRI: {
      if (loc == 0) return 0;
      Term cnt = HEAP[loc];
      if (term_tag(cnt) != TAG_NUM) return 0;
      return 1 + (u32)term_val(cnt);
    }
    case TAG_UOP: {
      u32 op = term_ext(t);
      switch (op) {
        case UOP_CONST: return 1;
        case UOP_KERNEL: case UOP_ASSIGN: return 2;
        case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
        case UOP_LOG2: case UOP_SQRT: case UOP_LOAD:
          return 1;
        case UOP_ADD: case UOP_MUL:
        case UOP_CMPLT: case UOP_CMPEQ:
          return 2;
        case UOP_FLIP:   return 2;
        case UOP_REDUCE: return 3;
        case UOP_RESHAPE: case UOP_EXPAND: case UOP_PERMUTE: {
          Term ndim_cell = HEAP[loc + 1];
          if (term_tag(ndim_cell) != TAG_NUM) return 0;
          return 2 + (u32)term_val(ndim_cell);
        }
        case UOP_PAD: case UOP_SHRINK: {
          Term ndim_cell = HEAP[loc + 1];
          if (term_tag(ndim_cell) != TAG_NUM) return 0;
          return 2 + 2 * (u32)term_val(ndim_cell);
        }
        default: return 0;
      }
    }
    default: return 0;
  }
}

static Term gc_evacuate(Term t, u64 *alloc);

// Evacuate the cell sequence at `loc` of size `size`, install a fwd
// pointer, recursively evacuate its children's heap-loc-bearing
// Terms.  Returns the new heap loc (= the dst we allocated).
static u64 gc_evacuate_cells(u64 loc, u32 size, u64 *alloc) {
  u64 dst = *alloc;
  *alloc += size;
  for (u32 i = 0; i < size; i++) {
    HEAP[dst + i] = HEAP[loc + i];
  }
  HEAP[loc] = term_new(0, GC_FWD_TAG, 0, dst);
  for (u32 i = 0; i < size; i++) {
    HEAP[dst + i] = gc_evacuate(HEAP[dst + i], alloc);
  }
  return dst;
}

static Term gc_evacuate(Term t, u64 *alloc) {
  if (t == 0) return 0;
  u8  tag = term_tag(t);
  u64 loc = term_val(t);

  // Atom-only tags (no heap loc to chase).
  if (tag == TAG_NUM || tag == TAG_TEN || tag == TAG_REF ||
      tag == TAG_ERA || tag == TAG_ANY || tag == TAG_FVR ||
      tag == GC_FWD_TAG) {
    return t;
  }
  // Special: PRI with val=0 is a 0-arg atom (no accumulator yet).
  if (tag == TAG_PRI && loc == 0) return t;

  // Refs into the permanent region or to-space (already evacuated):
  // pass through unchanged.
  if (loc < GC_FROM_START || loc >= GC_FROM_END) return t;

  // Already evacuated: forward pointer cell.
  Term cell0 = HEAP[loc];
  if (term_tag(cell0) == GC_FWD_TAG) {
    u64 new_loc = term_val(cell0);
    return term_new(term_sub_get(t), tag, term_ext(t), new_loc);
  }

  u32 size = gc_node_size(t);
  if (size == 0) return t;

  u64 new_loc = gc_evacuate_cells(loc, size, alloc);
  return term_new(term_sub_get(t), tag, term_ext(t), new_loc);
}

// Re-key the LAM shape table via forward pointers.  Called BEFORE
// from/to space swap so the from-space cells still hold valid fwd
// pointers.  Book-loc-keyed entries (high-bit set) are untouched --
// book locs don't move.
static void gc_remap_lam_shape(void) {
  u32 cap = LAM_SHAPE_CAP;
  // Snapshot occupied entries; rebuild in place.
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
      // Book key: passes through untouched.
      lam_shape_set_keyed(key, &snap[i].shape);
      continue;
    }
    // Dyn loc: look up forward pointer.  Drop the entry if the LAM
    // didn't survive the trace -- it's unreachable, the shape isn't
    // needed.
    if (key < GC_FROM_START || key >= GC_FROM_END) {
      // Loc is outside from-space (permanent region or already in
      // to-space-as-from after a previous GC cycle).  Keep as-is.
      lam_shape_set_keyed(key, &snap[i].shape);
      continue;
    }
    Term cell = HEAP[key];
    if (term_tag(cell) == GC_FWD_TAG) {
      lam_shape_set_keyed(term_val(cell), &snap[i].shape);
    }
    // else: drop (no forward = unreachable).
  }
  free(snap);
}

// Evacuate every Term stored in C-side mutable structures so they
// point at to-space after the swap.
static void gc_evacuate_side_tables(u64 *alloc) {
  // EXTERN_PINNED_TERMS array.
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_LEN; i++) {
    EXTERN_PINNED_TERMS[i] = gc_evacuate(EXTERN_PINNED_TERMS[i], alloc);
  }
  // EXTERN_PIN_HANDLES (sparse array of Terms held by foreign
  // handles; nonzero entries are live).
  if (EXTERN_PIN_HANDLES != NULL) {
    for (u64 i = 0; i < EXTERN_PIN_HANDLE_CAP; i++) {
      if (EXTERN_PIN_HANDLES[i] != 0) {
        EXTERN_PIN_HANDLES[i] = gc_evacuate(EXTERN_PIN_HANDLES[i], alloc);
      }
    }
  }
  // DEFS holds book Terms (TAG_REF or pre-resolved book heap
  // references).  TAG_REF's val is a name id, not a heap loc, so
  // gc_evacuate is a no-op; book locs are outside from-space.
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] != 0) DEFS[i] = gc_evacuate(DEFS[i], alloc);
  }
  // WNF_LAST_STACK: pending eliminator frames from a prior wnf_n
  // bail.
  for (u32 i = 0; i < WNF_LAST_STACK_LEN; i++) {
    WNF_LAST_STACK[i] = gc_evacuate(WNF_LAST_STACK[i], alloc);
  }
  // KernelEntry: source_uop + compute_root + symbolic input_terms[].
  // output_tid and input_tids[] are TenDesc ids, not Terms.  The
  // Phase C compute_root field carries the post-lift UOp DAG; it
  // shares heap-resident UOP_BUFFER / UOP_STORE / UOP_INDEX_E cells
  // with source_uop's subgraph in the common case but also holds
  // freshly-allocated arithmetic spine cells (uop_int_binary etc.)
  // produced by the lifter.  Evacuating it here keeps those cells
  // alive across collections while the kernel is still in KERNELS[].
  for (u32 k = 0; k < KERNELS_NEXT; k++) {
    KernelEntry *ke = &KERNELS[k];
    if (ke->source_uop != 0) {
      ke->source_uop = gc_evacuate(ke->source_uop, alloc);
    }
    if (ke->compute_root != 0) {
      ke->compute_root = gc_evacuate(ke->compute_root, alloc);
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

// Run a full collection.  Roots are evacuated in place; side tables
// (extern pins, DEFS, kernels, lam_shape, etc.) are remapped via the
// per-cell forward pointers.  After this call, HEAP_NEXT points
// just past the live region in the new from-space.
fn void gc_collect(Term *roots, u32 n_roots) {
  if (!GC_ENABLED) return;

  GC_COUNT++;
  u64 alloc = GC_TO_START;

  // 1. Evacuate the explicit root list (caller's wnf result, etc.).
  for (u32 i = 0; i < n_roots; i++) {
    if (roots[i] != 0) roots[i] = gc_evacuate(roots[i], &alloc);
  }

  // 2. Evacuate side-table-stored Terms (pins, DEFS, KernelEntries,
  //    WNF_LAST_STACK).  These run AFTER root evac so any subgraph
  //    they share with the roots already has forward pointers.
  gc_evacuate_side_tables(&alloc);

  // 3. Remap lam_shape table (still uses from-space locs as keys at
  //    this point; remap via the forward pointers laid down in step 1
  //    + 2).
  gc_remap_lam_shape();

  // 4. Stale-cache cleanup: caches whose cached Terms point at
  //    from-space.  uop_const_cache + uop_mov_cache return Terms;
  //    after we swap spaces below, those Terms would dangle.  Cheap
  //    to rebuild on next allocation.
  uop_const_cache_reset();
  uop_mov_cache_reset();

  // 5. Swap from / to.
  u64 swap_start = GC_FROM_START, swap_end = GC_FROM_END;
  GC_FROM_START = GC_TO_START;
  GC_FROM_END   = GC_TO_END;
  GC_TO_START   = swap_start;
  GC_TO_END     = swap_end;

  // 6. Bump pointer = end of the evacuated content in the new
  //    from-space.
  HEAP_NEXT = alloc;

  // 7. Cheney semi-spaces: the old from-space becomes the next
  //    to-space.  No zeroing needed -- evacuation overwrites every
  //    live cell on the next collection, and forward pointers left
  //    behind are stale by definition.
}

