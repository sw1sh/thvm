// Snapshots a dynamic term into the book heap: a template that survives heap
// resets and that a TAG_REF unfolds through ALO.

// Linear remap (dyn loc -> book loc): defs are small enough for O(N) lookup.
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
      // A grad cell is [y, gy, target]; a plain dup body is one cell.
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
        // [src, NUM(dst dtype)]: clone both cells, as alo_node_arity does.
        case UOP_CAST: case UOP_BITCAST:                   return 2;
        case UOP_LOAD:                                     return 1;
        // A variable tail: clone_to_book_rec sizes REDUCE itself.
        case UOP_REDUCE:                                   return 0;
        case UOP_KERNEL:                                   return 2;
        case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
        case UOP_PAD:     case UOP_SHRINK:  case UOP_FLIP:
          return 0;
        // A select (uop_where / uop_iwhere): [cond, then, else].
        case UOP_IWHERE:                                   return 3;
        // Fixed-size indexed ops: clone every cell (NUM payload included,
        // matching alo_node_arity).
        case UOP_GATHER:                                   return 3;
        case UOP_SCATTER:                                  return 6;
        default:                                           return 0;
      }
    }
    default: return 0;
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
      // The binder LAM was mapped before its body was walked; a free VAR
      // (binder outside the snapshot) becomes VAR(0).
      u64 book_binder = 0;
      for (u32 i = 0; i < *map_pos; i++) {
        if (map[i].dyn_loc == val) { book_binder = map[i].book_loc; break; }
      }
      return term_new(0, TAG_VAR, ext, book_binder);
    }

    case TAG_CTR: {
      // [NUM(n), c_0, ..., c_{n-1}]: the whole 1+n run.
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
      // A grad projection's [y, gy, target] cell is cloned whole; alo_realize
      // gives each instance its own copy.
      if (ext & DUP_GRAD_FLAG) {
        u32 ar = dyn_arity(tag, ext, val);
        u64 b = remap_lookup_or_alloc(map, map_pos, val, ar);
        for (u32 i = 0; i < ar; i++) {
          Term child = heap_read(val + i);
          book_set(b + i, clone_to_book_rec(child, map, map_pos));
        }
        return term_new(0, tag, ext, b);
      }
      // A plain projection becomes BJ0/BJ1, Levy-opaque under wnf and cnf;
      // alo_realize turns it back into a DP per instance, its dup cell shared
      // by both sides (HVM4's DP/BJ split, TinyHVM/HVM4/clang/cnf/_.c).
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
      // The book LAM takes the shape; alo_realize copies it onto each instance.
      Shape s;
      if (lam_shape_lookup(val, &s)) lam_shape_set_book(b, &s);
      // Re-seal LAM_ERA_MASK from the dyn body, for a LAM built with ext 0 (say
      // by a test that bypasses lam_seal_ext); the other ext bits survive.
      u32 sealed_ext = lam_seal_ext(val, ext & ~LAM_ERA_MASK);
      return term_new(0, TAG_LAM, sealed_ext, b);
    }

    case TAG_UOP: {
      if (ext == UOP_REDUCE) {
        // [src, NUM(kind), NUM(n_axes), NUM(axis_0), ...], all cloned, as
        // alo_node_arity reads them from the book; passed through verbatim the
        // REDUCE would keep its dyn loc, and alo_realize would read garbage.
        Term n_cell = heap_read(val + 2);
        u32  n_axes = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 1;
        if (n_axes > MAX_DIM) n_axes = MAX_DIM;
        u32 ar = 3 + n_axes;
        u64 b = remap_lookup_or_alloc(map, map_pos, val, ar);
        for (u32 i = 0; i < ar; i++) {
          Term child = heap_read(val + i);
          book_set(b + i, clone_to_book_rec(child, map, map_pos));
        }
        return term_new(0, TAG_UOP, UOP_REDUCE, b);
      }
      goto generic_node;
    }

    default:
    generic_node: {
      u32 ar = dyn_arity(tag, ext, val);
      if (ar == 0) return t;     // not snapshotted: keeps its dyn loc
      u64 b = remap_lookup_or_alloc(map, map_pos, val, ar);
      for (u32 i = 0; i < ar; i++) {
        Term child = heap_read(val + i);
        book_set(b + i, clone_to_book_rec(child, map, map_pos));
      }
      return term_new(0, tag, ext, b);
    }
  }
}

// A book term: its val indexes BOOK_HEAP, not HEAP.
Term thvm_book_from_dynamic(Term body) {
  BookRemap map[BOOK_REMAP_CAP];
  u32 map_pos = 0;
  return clone_to_book_rec(body, map, &map_pos);
}

// The snapshot keeps later mutations of the dynamic graph out of the def.
void thvm_def_register(u32 name, Term body) {
  if (name >= DEFS_CAP) {
    fprintf(stderr, "thvm_def_register: name id %u out of range\n", name);
    exit(1);
  }
  DEFS[name] = thvm_book_from_dynamic(body);
}
