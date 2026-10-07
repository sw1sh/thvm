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
//     TenDesc.grad, KernelEntry.source_uop / input_terms[]: re-evacuated
//     as part of the root set so their stored Terms get the new locs.

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

// Runtime heap allowance (cells).  Resolved once from THVM_HEAP_CELLS,
// falling back to the HEAP_CAP compile-time default so an unset env is
// byte-identical to the historical fixed heap.  A bad/zero/oversize
// value falls back to HEAP_CAP.
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

// Static def-template (book) heap allowance in cells.  Mirrors
// thvm_heap_cells: resolved once from THVM_BOOK_CELLS, default BOOK_CAP.
// Now that the book heap is mmap-backed (lazily zero-faulted), the large
// default costs no RSS until cells are actually written -- but a workload
// that wants a smaller virtual reservation can still shrink it here.
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

// Permanent cells past the two semi-spaces, reserved only when
// THVM_HEAP_PERM_CELLS asks (default 0 keeps the historical mapping).
// The collector never evacuates or scans a loc outside from-space, so
// a node built here stays put and costs the copying pass nothing: the
// home for append-only atom-only nodes a run keeps to its end (the
// --proof trace entries past the default trace cap, which would
// otherwise grow the live set every collection copies).
static u64 HEAP_PERM_REQ = 0;
// In-process request (a caller that must not grow the environment:
// every uncached getenv in a hot path would then cost more work).
// Takes effect only before the heap is first mapped.
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

// Cells the heap mapping covers: both semi-spaces plus the permanent tail.
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

// space_words: half the desired total heap allowance.  Each semi-
// space gets `space_words` cells; total addressable = 2*space_words
// (must be <= HEAP_CAP).
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
// A UOp opcode with no case here is never evacuated, and a live node of it
// reads garbage after the swap: every assigned opcode below UOP_COUNT has a
// case (44-50 are unassigned).
_Static_assert(UOP_COUNT == 55, "a new UOp opcode needs its heap size in gc_node_size");
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
        // UOP_COPY heap = [src, NUM(device+1)] -- src plus the trailing
        // device payload.  (Before the explicit-device COPY this had no
        // case and fell to `default: return 0`, leaving its src cell
        // unevacuated -- latent had a GC ever fired with a live COPY.)
        case UOP_COPY: return 2;
        case UOP_ADD: case UOP_MUL:
        case UOP_CMPLT: case UOP_CMPEQ:
          return 2;
        // 2-cell ops the evacuator MUST size correctly or it leaves the src
        // cell(s) in from-space (which becomes garbage to-space after the swap
        // -> shape/value corruption when a live node of this op survives a
        // collection).  CAST/BITCAST = [src, NUM(dst_dtype)]; this was the fp8
        // weight bug (a cached TUOpCast(fp8) corrupted after GC, the {128}
        // scale shape bled into the realized output).  INDEX_E/AFTER = 2 src
        // cells; the integer/bitwise ALU ops = [a, b] (index arithmetic).
        case UOP_CAST: case UOP_BITCAST:
        case UOP_INDEX_E: case UOP_AFTER:
        case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
        case UOP_IMOD: case UOP_ILT: case UOP_IAND: case UOP_IOR:
        case UOP_IXOR: case UOP_ISHR:
          return 2;
        case UOP_INVALID:     return 1;       // [NUM(0)]
        case UOP_DEFINE_VAR:  return 1;       // [NUM(packed_extent)]
        case UOP_DETACH:      return 1;       // [src]
        // 3-cell range / ternary / store / opt / gather.
        case UOP_RANGE:                       // [NUM(axis_id), NUM(axis_type), NUM(extent)]
        case UOP_IWHERE: case UOP_STORE:      // [cond,then,else] / [buf,addr,value]
        case UOP_OPT:                         // [target, NUM(kind), NUM(factor)]
        case UOP_GATHER:                      // [src, idx, NUM(dim)]
          return 3;
        // [base, idx, src, NUM(dim), NUM(kind), NUM(include_self)]
        case UOP_SCATTER:
          return 6;
        case UOP_FLIP:   return 2;
        // UOP_REDUCE: heap = [src, kind, n_axes, axis_0, ..., axis_{n-1}].
        // Total cells = 3 + n_axes (multi-axis port).
        case UOP_REDUCE: {
          Term n_cell = HEAP[loc + 2];
          if (term_tag(n_cell) != TAG_NUM) return 0;
          return 3 + (u32)term_val(n_cell);
        }
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
        // UOP_BUFFER = [NUM(scope), NUM(dtype), NUM(ndim), NUM(instance),
        //               NUM(d0), ..].
        case UOP_BUFFER: {
          Term ndim_cell = HEAP[loc + 2];
          if (term_tag(ndim_cell) != TAG_NUM) return 0;
          return 4 + (u32)term_val(ndim_cell);
        }
        // UOP_BUFFERIZE = [value, NUM(addrspace), NUM(removable), NUM(n),
        //                  range_0, .., range_{n-1}].
        case UOP_BUFFERIZE: {
          Term n_cell = HEAP[loc + 3];
          if (term_tag(n_cell) != TAG_NUM) return 0;
          return 4 + (u32)term_val(n_cell);
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
// pointers.  Book-loc-keyed entries (BOOK bit set) are untouched --
// book locs don't move.  The key carries the context slot id in
// LAM_SHAPE_CTX_MASK; GC only moves cells of the CURRENT context's
// heap, so entries keyed to a DIFFERENT context pass through
// untouched (their locs are not in this from-space).  For current-
// context dyn entries we strip the ctx bits, remap the bare loc, and
// re-apply the ctx (+ never the book) bits.
static void gc_remap_lam_shape(void) {
  u32 cap = LAM_SHAPE_CAP;
  u64 cur_ctx = ((u64)thvm_context_current() & 0xF) << LAM_SHAPE_CTX_SHIFT;
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
      // Book key: passes through untouched (book locs don't move).
      lam_shape_set_keyed(key, &snap[i].shape);
      continue;
    }
    if ((key & LAM_SHAPE_CTX_MASK) != cur_ctx) {
      // Entry belongs to another context's heap -- its loc is not in
      // this from-space, so leave it alone.
      lam_shape_set_keyed(key, &snap[i].shape);
      continue;
    }
    // Current-context dyn loc: strip the ctx bits, look up the
    // forward pointer, re-apply the ctx bits.  Drop the entry if the
    // LAM didn't survive the trace -- it's unreachable, shape unneeded.
    u64 loc = key & ~LAM_SHAPE_CTX_MASK;
    if (loc < GC_FROM_START || loc >= GC_FROM_END) {
      // Loc is outside from-space (permanent region or already in
      // to-space-as-from after a previous GC cycle).  Keep as-is.
      lam_shape_set_keyed(key, &snap[i].shape);
      continue;
    }
    Term cell = HEAP[loc];
    if (term_tag(cell) == GC_FWD_TAG) {
      lam_shape_set_keyed(term_val(cell) | cur_ctx, &snap[i].shape);
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
  // TenDesc.grad: the lazy gradient a backward accumulated onto a leaf,
  // read back by the host (and summed onto by the next backward) after
  // the realize that collects.
  for (u32 t = 0; t < TENS_NEXT; t++) {
    if (TENS[t].grad != 0) {
      TENS[t].grad = gc_evacuate(TENS[t].grad, alloc);
    }
  }
  // KernelEntry: source_uop + cached_lift + symbolic input_terms[].
  // output_tid and input_tids[] are TenDesc ids, not Terms.
  // cached_lift.store_root carries the post-lift UOp DAG root + the
  // heap-resident UOP_BUFFER / UOP_STORE / UOP_INDEX_E spine; out_buf
  // and in_bufs[0..n_inputs) are UOP_BUFFER cells (or UOP_NUM literals
  // when the lifter reads a const input).  Evacuating them keeps the
  // renderer's identity-based buffer-name resolution stable across
  // collections since rmu_buf_names_set compares by Term equality.
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

// Run a full collection.  Roots are evacuated in place; side tables
// (extern pins, DEFS, kernels, lam_shape, etc.) are remapped via the
// per-cell forward pointers.  After this call, HEAP_NEXT points
// just past the live region in the new from-space.
fn void gc_collect(Term *roots, u32 n_roots) {
  if (!GC_ENABLED) return;

  GC_COUNT++;
  u64 alloc = GC_TO_START;
  materialized_loc_gc_before();

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
  // The cross-realize loc -> tid cache (schedule/materialize.c) keys
  // off heap loc; after Cheney compacts the heap those locs name
  // entirely different cells (or fall outside HEAP_NEXT).  Re-key it
  // through the forward pointers so a stale loc never resolves to an
  // unrelated tid.
  materialized_loc_gc_after();
  // UOP_COPY device-upload cache (schedule/materialize.c) also keys off
  // the copy-node heap loc; clear it for the same reason.  The next
  // cross-backend realize re-uploads + repopulates.
  copy_upload_cache_reset();
  weight_move_cache_reset();
  kernel_input_upload_reset();

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

