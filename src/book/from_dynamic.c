// book/from_dynamic.c - snapshot a dynamic term tree into the book
// heap so it can survive heap resets and act as a static template
// for thvm_def_register / TAG_REF unfolding.
//
// Two passes via a small per-call hash-style remapping table that
// turns dynamic heap locs (binder addresses + node addresses) into
// fresh book heap locs.  All children are copied recursively; atoms
// (TAG_NUM / TAG_TEN / TAG_REF / TAG_ERA / TAG_VAR-after-rebind) are
// the recursion base case.
//
// Limited tag coverage on purpose: LAM / APP / VAR / NUM / TEN /
// REF / ERA + the UOP family.  SUP / DUP and the IC interaction
// scaffolding don't appear in user-written defs (they arise during
// reduction); adding them here is a follow-up.

// Tiny linear remap table -- defs rarely exceed a few dozen cells, so
// O(N) lookup is fine and avoids dragging in a hashmap.
typedef struct {
  u64 dyn_loc;
  u64 book_loc;
} BookRemap;

#define BOOK_REMAP_CAP 1024

static u64 remap_lookup_or_alloc(BookRemap *map, u32 *map_pos,
                                 u64 dyn_loc, u64 size) {
  for (u32 i = 0; i < *map_pos; i++) {
    if (map[i].dyn_loc == dyn_loc) return map[i].book_loc;
  }
  if (*map_pos >= BOOK_REMAP_CAP) {
    fprintf(stderr, "book_from_dynamic: remap table overflow\n");
    exit(1);
  }
  u64 b = book_alloc(size);
  map[*map_pos].dyn_loc  = dyn_loc;
  map[*map_pos].book_loc = b;
  (*map_pos)++;
  return b;
}

static u32 dyn_arity(u8 tag, u32 ext, u64 val) {
  switch (tag) {
    case TAG_LAM: return 1;
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
      // grad-flavored projections share a 3-cell grad cell;
      // plain dup projections share a 1-cell dup body.
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
        case UOP_LOAD:                                     return 1;
        // UOP_REDUCE is now variable-arity (multi-axis): heap is
        // [src, kind, n_axes, axis_0, ..., axis_{n-1}].  Treat like
        // other variable-arity ops -- unsupported in book snapshot.
        case UOP_REDUCE:                                   return 0;
        case UOP_KERNEL:                                   return 2;
        case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
        case UOP_PAD:     case UOP_SHRINK:  case UOP_FLIP:
          return 0;  // variable-arity; unsupported in book snapshot for now
        default:                                           return 0;
      }
    }
    default: return 0;  // VAR / TEN / NUM / REF / ERA / ALO are atoms here
  }
}

static Term clone_to_book_rec(Term t, BookRemap *map, u32 *map_pos) {
  u8  tag = term_tag(t);
  u32 ext = term_ext(t);
  u64 val = term_val(t);

  switch (tag) {
    case TAG_NUM:
    case TAG_TEN:
    case TAG_REF:
    case TAG_ERA:
      return t;

    case TAG_VAR: {
      // VAR refs the binder LAM's loc (val) -- remap to the new
      // book loc allocated for that LAM.  If we haven't seen the
      // binder yet, emit a placeholder (val=0) and let the LAM pass
      // patch it; in practice the LAM is always visited first since
      // we recurse top-down.
      u64 book_binder = 0;
      for (u32 i = 0; i < *map_pos; i++) {
        if (map[i].dyn_loc == val) { book_binder = map[i].book_loc; break; }
      }
      return term_new(0, TAG_VAR, ext, book_binder);
    }

    case TAG_CTR: {
      // CTR layout: heap[val] = NUM(arity), heap[val+1..val+n] = children.
      // Snapshot the whole 1+n run; recurse into each child so a VAR
      // inside resolves to its remapped book LAM binder.
      Term n_cell = heap_read(val);
      if (term_tag(n_cell) != TAG_NUM) return t;
      u32 n = (u32)term_val(n_cell);
      u64 b = remap_lookup_or_alloc(map, map_pos, val, 1 + n);
      book_set(b, n_cell);
      for (u32 i = 0; i < n; i++) {
        Term child = heap_read(val + 1 + i);
        book_set(b + 1 + i, clone_to_book_rec(child, map, map_pos));
      }
      return term_new(0, TAG_CTR, ext, b);
    }

    case TAG_DP0:
    case TAG_DP1: {
      // Grad-flavored projections share a 3-cell grad cell -- copy
      // verbatim through the default path so each lambda instance
      // gets a fresh grad cell with its own y/gy/target slots.
      if (ext & DUP_GRAD_FLAG) {
        u32 ar = dyn_arity(tag, ext, val);
        u64 b = remap_lookup_or_alloc(map, map_pos, val, ar);
        for (u32 i = 0; i < ar; i++) {
          Term child = heap_read(val + i);
          book_set(b + i, clone_to_book_rec(child, map, map_pos));
        }
        return term_new(0, tag, ext, b);
      }
      // Plain projection: rewrite DP -> BJ on the book copy so the
      // template is Levy-opaque under wnf and cnf.  alo_realize
      // unfolds BJ -> fresh DP per realize call (each book template
      // instance gets its own dyn dup cell shared across DP0/DP1
      // projections via alo_dup_share).  Mirrors HVM4's DP0/DP1 vs
      // BJ0/BJ1 split (TinyHVM/HVM4/clang/cnf/_.c).
      u64 b = remap_lookup_or_alloc(map, map_pos, val, 1);
      Term body = heap_read(val);
      book_set(b, clone_to_book_rec(body, map, map_pos));
      u8 bj_tag = (tag == TAG_DP0) ? TAG_BJ0 : TAG_BJ1;
      return term_new(0, bj_tag, ext, b);
    }

    case TAG_LAM: {
      u64 b = remap_lookup_or_alloc(map, map_pos, val, 1);
      Term body = heap_read(val);
      Term cb   = clone_to_book_rec(body, map, map_pos);
      book_set(b, cb);
      // Propagate any shape annotation on this LAM from dyn space
      // to book space.  TLamShape sets the dyn-side annotation;
      // here we mirror it on the book loc so alo_realize can
      // re-instate the annotation on each fresh dyn instance.
      Shape s;
      if (lam_shape_lookup(val, &s)) lam_shape_set_book(b, &s);
      // Re-seal LAM_ERA_MASK against the dyn body we just cloned
      // in.  This catches LAMs that were constructed with ext=0
      // (e.g. by tests that bypass lam_seal_ext) so the book copy
      // still gets an accurate mask.  We OR over `ext`'s remaining
      // bits so any other flags the dyn LAM carried survive.
      u32 sealed_ext = lam_seal_ext(val, ext & ~LAM_ERA_MASK);
      return term_new(0, TAG_LAM, sealed_ext, b);
    }

    default: {
      // Generic fixed-arity node (APP / UOP-fixed-arity).
      u32 ar = dyn_arity(tag, ext, val);
      if (ar == 0) return t;     // unsupported tag/opcode -- pass through verbatim
      u64 b = remap_lookup_or_alloc(map, map_pos, val, ar);
      for (u32 i = 0; i < ar; i++) {
        Term child = heap_read(val + i);
        book_set(b + i, clone_to_book_rec(child, map, map_pos));
      }
      return term_new(0, tag, ext, b);
    }
  }
}

Term thvm_book_from_dynamic(Term body) {
  BookRemap map[BOOK_REMAP_CAP];
  u32 map_pos = 0;
  return clone_to_book_rec(body, map, &map_pos);
}

void thvm_def_register(u32 name, Term body) {
  if (name >= DEFS_CAP) {
    fprintf(stderr, "thvm_def_register: name id %u out of range\n", name);
    exit(1);
  }
  DEFS[name] = thvm_book_from_dynamic(body);
}
