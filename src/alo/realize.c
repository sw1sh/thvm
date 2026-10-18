// One layer of a book template into the dyn heap, after HVM4's wnf/alo_*.c: a
// VAR rebinds through the state chain, a LAM pushes its binding, a node copies.
//
// Children are suspended in fresh ALOs rather than realized, so a branch that
// never fires (a recursive REF, say) never expands.

// Cells per node, as book/from_dynamic.c laid them out (dyn_arity there).
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
      // A grad cell is [y, gy, target], a plain dup body one cell; each
      // instance gets its own, so a target VAR rebinds to that instance's LAM.
      if (ext & DUP_GRAD_FLAG) return 3;
      return 1;
    }
    case TAG_UOP: {
      switch (ext) {
        case UOP_CONST:                                    return 1;
        case UOP_ADD: case UOP_MUL: case UOP_CMPLT:
        case UOP_CMPEQ:                                    return 2;
        case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
        case UOP_LOG2: case UOP_SQRT:                      return 1;
        // [src, NUM(dst dtype)]: the NUM passes through bare.
        case UOP_CAST: case UOP_BITCAST:                   return 2;
        // [src, NUM(kind), NUM(n_axes), NUM(axis_0), ...]
        case UOP_REDUCE: {
          Term n_cell = book_read(val + 2);
          u32 n_axes = (term_tag(n_cell) == TAG_NUM)
                       ? (u32)term_val(n_cell) : 1;
          if (n_axes > MAX_DIM) n_axes = MAX_DIM;
          return 3 + n_axes;
        }
        case UOP_LOAD:                                     return 1;
        case UOP_KERNEL:                                   return 2;
        // A select (uop_where / uop_iwhere): [cond, then, else].
        case UOP_IWHERE:                                   return 3;
        // Every cell, the NUM payload included (NUMs pass through bare).
        case UOP_GATHER:                                   return 3;
        case UOP_SCATTER:                                  return 6;
        default:                                           return 0;
      }
    }
    default: return 0;
  }
}

// Both projections of a book dup must share one dyn dup cell, or each fires its
// own DUP; the key (state_id, book loc) is the same for both.

#define DUP_SHARE_BITS  20
#define DUP_SHARE_SLOTS (1u << DUP_SHARE_BITS)
#define DUP_SHARE_MASK  (DUP_SHARE_SLOTS - 1)
#define DUP_SHARE_PROBE 8

typedef struct { u32 gen; u32 state_id; u64 book_loc; u64 dyn_loc; } DupShareEntry;
static DupShareEntry DUP_SHARE[DUP_SHARE_SLOTS];
static u32 DUP_SHARE_GEN = 1;

// A generation bump empties the table without re-zeroing its 24 MiB.
fn void alo_dup_share_reset(void) {
  DUP_SHARE_GEN++;
  if (DUP_SHARE_GEN == 0) {
    // Wrapped: zero the table so no stale generation matches.
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
  // Probe limit hit: the pair goes unshared, which only costs a second DUP.
}

// NUM/TEN/REF/ERA pass bare: an ALO around them would only cost a wnf step.
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
      // A free var (binder outside the template) stays as is.
      return book_term;
    }

    case TAG_DP0:
    case TAG_DP1: {
      // A grad projection takes the node path: each instance gets its own cell.
      if (ext & DUP_GRAD_FLAG) goto default_node;

      // clone_to_book_rec writes plain projections as BJ0/BJ1; a book DP gets
      // the same shared dup cell.
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
      // BJ0 and BJ1 of one book BJ map to one fresh dup cell, so their DPs fire
      // one DUP between them (heap_subst_cop hands the other side a SUB).
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
      // [NUM(n), c_0, ..., c_{n-1}]
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
      // Each instance takes the book LAM's shape, so its own bound var has one.
      Shape s;
      if (lam_shape_lookup_book(val, &s)) lam_shape_set(new_loc, &s);
      // ext keeps the mask clone_to_book_rec sealed: under the ALO-wrapped
      // children a re-seal could not see whether VAR(new_loc) occurs.
      return term_new(0, TAG_LAM, ext, new_loc);
    }

    default:
    default_node: {
      u32 ar = alo_node_arity(tag, ext, val);
      if (ar == 0) return book_term;
      u64 new_loc = heap_alloc(ar);
      for (u32 i = 0; i < ar; i++) {
        Term child = book_read(val + i);
        heap_set(new_loc + i, alo_suspend_child(child, state_id));
      }
      return term_new(0, tag, ext, new_loc);
    }
  }
}
