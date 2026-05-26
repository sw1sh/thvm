// alo/realize.c - walk one layer of a static book template into the
// dynamic heap.
//
// Mirrors HVM4's wnf_alo_* / TinyHVM's thvm_alo_realize.  Strategy:
//
//   - leaves (TAG_NUM, TAG_TEN, TAG_REF, TAG_ERA): pass through.
//   - TAG_VAR: rebind via the substitution chain (ALO-VAR).
//   - TAG_LAM: allocate one fresh dyn cell, push a binding
//     (book_lam_loc -> new_lam_loc), wrap the body in a fresh ALO
//     under the extended state (ALO-LAM).
//   - TAG_APP / TAG_UOP / TAG_SUP / TAG_DUP and other fixed-arity
//     nodes: allocate fresh dyn cells, wrap each child in ALO under
//     the same state (ALO-NOD).
//
// Children get wrapped in ALOs rather than realised eagerly so a
// non-strict consumer (e.g., a recursive REF in a never-fired
// branch) doesn't expand its body.  alo_force fires one layer when
// wnf later enters that ALO.

// Returns the dyn arity for a fixed-arity tag/opcode (matches the
// table used in book/from_dynamic.c).  UOP_GRAD's tail is variable
// (heap = [y, gy, NUM(n), x_1..x_n]) so we read n from the book
// cell at val+2.
static u32 alo_node_arity(u8 tag, u32 ext, u64 val) {
  switch (tag) {
    case TAG_APP: return 2;
    case TAG_SUP: return 2;
    case TAG_DUP: return 1;
    case TAG_OP2: return 2;
    case TAG_MAT: return 2;
    case TAG_EQL: return 2;
    case TAG_AND: return 2;
    case TAG_OR:  return 2;
    case TAG_WHEN: return 2;
    case TAG_DP0: case TAG_DP1: {
      // grad-flavored projections wrap a 3-cell grad cell
      // [y, gy, target_or_zero]; plain DUP projections wrap a
      // 1-cell dup body.  Realize the cells so each lambda
      // instance gets its own grad cell -- otherwise the body's
      // TVAR target inside cell+2 would alias the template's
      // bound var and resolve to the same LAM across instances.
      if (ext & DUP_GRAD_FLAG) return 3;
      return 1;
    }
    case TAG_UOP: {
      switch (ext) {
        case UOP_CONST:                                    return 1;
        case UOP_ADD: case UOP_MUL: case UOP_CMPLT:        return 2;
        case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
        case UOP_LOG2: case UOP_SQRT:                      return 1;
        // UOP_REDUCE: book heap = [src, kind, n_axes, axis_0, ...].
        // n_axes lives at val+2 (a TAG_NUM cell in the book); total
        // cells = 3 + n_axes.
        case UOP_REDUCE: {
          Term n_cell = book_read(val + 2);
          u32 n_axes = (term_tag(n_cell) == TAG_NUM)
                       ? (u32)term_val(n_cell) : 1;
          if (n_axes > MAX_DIM) n_axes = MAX_DIM;
          return 3 + n_axes;
        }
        case UOP_LOAD:                                     return 1;
        case UOP_KERNEL:                                   return 2;
        default:                                           return 0;
      }
    }
    default: return 0;
  }
}

// DUP-share remap: when a parent realize encounters DP0 and DP1 of
// the same book dup, both projections must end up pointing at the
// SAME freshly-allocated dyn dup loc -- otherwise each fires its own
// DUP-NOD interaction (HVM4 fires only one per pair, since
// heap_subst_cop substitutes the other side via SUB).  The lookup
// key is (state_id, book_dup_loc); state_id is the binding scope
// shared by both projections at suspend time, so subsequent realizes
// of either projection find the same entry.
//
// Open-addressed hash table.  Generation counter (DUP_SHARE_GEN)
// invalidates all entries on reset without re-zeroing 24 MiB.
// Probe limit 8: on overflow we fall back to the un-shared path
// (still correct, just one extra DUP-NOD per affected pair).

#define DUP_SHARE_BITS  20
#define DUP_SHARE_SLOTS (1u << DUP_SHARE_BITS)
#define DUP_SHARE_MASK  (DUP_SHARE_SLOTS - 1)
#define DUP_SHARE_PROBE 8

typedef struct { u32 gen; u32 state_id; u64 book_loc; u64 dyn_loc; } DupShareEntry;
static DupShareEntry DUP_SHARE[DUP_SHARE_SLOTS];
static u32 DUP_SHARE_GEN = 1;

fn void alo_dup_share_reset(void) {
  DUP_SHARE_GEN++;
  if (DUP_SHARE_GEN == 0) {
    // wraparound after 2^32 resets -- zero the table once and start over.
    memset(DUP_SHARE, 0, sizeof(DUP_SHARE));
    DUP_SHARE_GEN = 1;
  }
}

static inline u32 dup_share_hash(u32 state, u64 book) {
  u64 h = (u64)state * 2654435761ull + book * 11400714785074694791ull;
  return (u32)((h >> 32) ^ h) & DUP_SHARE_MASK;
}

static int dup_share_lookup(u32 state, u64 book, u64 *out_dyn) {
  u32 h = dup_share_hash(state, book);
  for (u32 i = 0; i < DUP_SHARE_PROBE; i++) {
    u32 slot = (h + i) & DUP_SHARE_MASK;
    DupShareEntry *e = &DUP_SHARE[slot];
    if (e->gen != DUP_SHARE_GEN) return 0;
    if (e->state_id == state && e->book_loc == book) {
      *out_dyn = e->dyn_loc;
      return 1;
    }
  }
  return 0;
}

static void dup_share_push(u32 state, u64 book, u64 dyn) {
  u32 h = dup_share_hash(state, book);
  for (u32 i = 0; i < DUP_SHARE_PROBE; i++) {
    u32 slot = (h + i) & DUP_SHARE_MASK;
    DupShareEntry *e = &DUP_SHARE[slot];
    if (e->gen != DUP_SHARE_GEN) {
      e->gen      = DUP_SHARE_GEN;
      e->state_id = state;
      e->book_loc = book;
      e->dyn_loc  = dyn;
      return;
    }
  }
  // probe limit reached: silently drop (fall back to no sharing)
}

// Wrap a book child in a fresh ALO bound to `state`.  Atoms that
// don't need allocation (NUM/TEN/REF/ERA) are passed through bare
// since wrapping them just generates a wnf step that would unwrap
// them again.
static Term alo_suspend_child(Term book_child, u32 state) {
  u8 tag = term_tag(book_child);
  if (tag == TAG_NUM || tag == TAG_TEN || tag == TAG_REF || tag == TAG_ERA)
    return book_child;
  return term_new_alo(book_child, state);
}

Term alo_realize(Term book_term, u32 state_id) {
  u8  tag = term_tag(book_term);
  u32 ext = term_ext(book_term);
  u64 val = term_val(book_term);

  switch (tag) {
    case TAG_NUM:
    case TAG_TEN:
    case TAG_REF:
    case TAG_ERA:
      return book_term;

    case TAG_VAR: {
      u64 dyn_loc;
      if (alo_state_lookup(state_id, val, &dyn_loc)) {
        return term_new(0, TAG_VAR, ext, dyn_loc);
      }
      // Free var (binder outside the snapshot) -- leave as-is.
      return book_term;
    }

    case TAG_DP0:
    case TAG_DP1: {
      // Grad-flavored projections wrap a 3-cell grad cell with
      // entirely different semantics; let the default node path
      // handle them (each instance gets its own grad cell, as today).
      if (ext & DUP_GRAD_FLAG) goto default_node;

      // Plain dup projection should not appear in book templates after
      // the BJ rewrite landed (clone_to_book_rec emits BJ for plain
      // DPs).  Defensive fall-through for back-compat with templates
      // that pre-date the rewrite: same dup_share semantics, emit DP.
      u64 new_loc;
      if (!dup_share_lookup(state_id, val, &new_loc)) {
        new_loc = heap_alloc(1);
        Term body = book_read(val);
        heap_set(new_loc, alo_suspend_child(body, state_id));
        dup_share_push(state_id, val, new_loc);
      }
      return term_new(0, tag, ext, new_loc);
    }

    case TAG_BJ0:
    case TAG_BJ1: {
      // Book-time projection -> fresh dyn DP.  Shared dup cell: both
      // BJ0 and BJ1 of the same book BJ map to the SAME freshly
      // allocated dyn dup-body so their DP0/DP1 siblings fire one
      // DUP-XXX between them (heap_subst_cop substitutes the dual).
      u64 new_loc;
      if (!dup_share_lookup(state_id, val, &new_loc)) {
        new_loc = heap_alloc(1);
        Term body = book_read(val);
        heap_set(new_loc, alo_suspend_child(body, state_id));
        dup_share_push(state_id, val, new_loc);
      }
      u8 dp_tag = (tag == TAG_BJ0) ? TAG_DP0 : TAG_DP1;
      return term_new(0, dp_tag, ext, new_loc);
    }

    case TAG_CTR: {
      // CTR layout: book[val] = NUM(arity), book[val+1..val+n] = children.
      // Allocate a fresh dyn layout, copy the arity NUM as-is, and
      // ALO-suspend each child under the same state so a downstream
      // VAR inside a child can rebind to dyn binders set up earlier
      // in this realization.
      Term n_cell = book_read(val);
      if (term_tag(n_cell) != TAG_NUM) return book_term;
      u32 n = (u32)term_val(n_cell);
      u64 new_loc = heap_alloc(1 + n);
      heap_set(new_loc, n_cell);
      for (u32 i = 0; i < n; i++) {
        Term child = book_read(val + 1 + i);
        heap_set(new_loc + 1 + i, alo_suspend_child(child, state_id));
      }
      return term_new(0, TAG_CTR, ext, new_loc);
    }

    case TAG_LAM: {
      u64 new_loc = heap_alloc(1);
      u32 body_state = alo_state_push(state_id, val, new_loc);
      Term body = book_read(val);
      heap_set(new_loc, alo_suspend_child(body, body_state));
      // Re-instate any shape annotation from the book LAM onto
      // the fresh dyn instance, so each iter's bound var is
      // shape-resolvable independently.
      Shape s;
      if (lam_shape_lookup_book(val, &s)) lam_shape_set(new_loc, &s);
      // LAM_ERA_MASK is already baked into `ext` from the book copy
      // (set by clone_to_book_rec when the dyn LAM was registered).
      // Don't re-seal here: the dyn body we're emitting has ALO-
      // wrapped children, which would hide a downstream VAR(new_loc)
      // and produce a false positive for the mask.
      return term_new(0, TAG_LAM, ext, new_loc);
    }

    default:
    default_node: {
      u32 ar = alo_node_arity(tag, ext, val);
      if (ar == 0) return book_term;       // unsupported tag: leave verbatim
      u64 new_loc = heap_alloc(ar);
      for (u32 i = 0; i < ar; i++) {
        Term child = book_read(val + i);
        heap_set(new_loc + i, alo_suspend_child(child, state_id));
      }
      return term_new(0, tag, ext, new_loc);
    }
  }
}
