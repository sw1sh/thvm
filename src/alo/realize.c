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
// (k0b: heap = [y, gy, NUM(n), x_1..x_n]) so we read n from the
// book cell at val+2.
static u32 alo_node_arity(u8 tag, u32 ext, u64 val) {
  switch (tag) {
    case TAG_APP: return 2;
    case TAG_SUP: return 2;
    case TAG_DUP: return 1;
    case TAG_OP2: return 2;
    case TAG_MAT: return 2;
    case TAG_UOP: {
      switch (ext) {
        case UOP_CONST:                                    return 1;
        case UOP_ADD: case UOP_MUL: case UOP_CMPLT:        return 2;
        case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
        case UOP_LOG2: case UOP_SQRT:                      return 1;
        case UOP_REDUCE:                                   return 3;
        case UOP_GRAD: {
          Term n_cell = book_read(val + 2);
          u32  n = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 1;
          return 3 + n;
        }
        case UOP_LOAD:                                     return 1;
        case UOP_KERNEL:                                   return 2;
        default:                                           return 0;
      }
    }
    default: return 0;
  }
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

    case TAG_LAM: {
      u64 new_loc = heap_alloc(1);
      u32 body_state = alo_state_push(state_id, val, new_loc);
      Term body = book_read(val);
      heap_set(new_loc, alo_suspend_child(body, body_state));
      return term_new(0, TAG_LAM, ext, new_loc);
    }

    default: {
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
