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
    case TAG_UOP: {
      switch (ext) {
        case UOP_CONST:                                    return 1;
        case UOP_ADD: case UOP_MUL: case UOP_CMPLT:
        case UOP_CMPEQ:                                    return 2;
        case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
        case UOP_LOG2: case UOP_SQRT:                      return 1;
        case UOP_LOAD:                                     return 1;
        case UOP_REDUCE:                                   return 3;
        // k0b: UOP_GRAD heap is [y, gy, NUM(n), x_1..x_n] -- variable
        // tail.  Read NUM(n) at val+2 to compute 3+n; recursive
        // optimizer lambdas (TOptim) embed UOP_GRAD in their body
        // template, which book_from_dynamic clones into BOOK_HEAP.
        case UOP_GRAD: {
          Term n_cell = heap_read(val + 2);
          u32  n = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 1;
          return 3 + n;
        }
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

    case TAG_LAM: {
      u64 b = remap_lookup_or_alloc(map, map_pos, val, 1);
      Term body = heap_read(val);
      Term cb   = clone_to_book_rec(body, map, map_pos);
      book_set(b, cb);
      return term_new(0, TAG_LAM, ext, b);
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
