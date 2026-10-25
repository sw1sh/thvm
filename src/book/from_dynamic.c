// Snapshots a dynamic term into the book heap: a template that survives heap
// resets and collections, which a TAG_REF unfolds through ALO.  Every node is
// copied, so a book cell never holds a dyn loc.

// Linear remap (dyn loc -> book loc): defs are small enough for O(N) lookup.
typedef struct {
  u64 dyn_loc;
  u64 book_loc;
} BookRemap;

#define BOOK_REMAP_CAP 1024

static int remap_find(BookRemap const *map, u32 map_pos, u64 dyn_loc, u64 *book_loc) {
  for (u32 i = 0; i < map_pos; i++) {
    if (map[i].dyn_loc == dyn_loc) {
      *book_loc = map[i].book_loc;
      return 1;
    }
  }
  return 0;
}

static u64 remap_add(BookRemap *map, u32 *map_pos, u64 dyn_loc, u64 size) {
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

// A node reached twice is copied once, so the book keeps the term's sharing.
static Term clone_to_book_rec(Term t, BookRemap *map, u32 *map_pos) {
  // A VAR or DP whose cell took a substitution stands for that value, and an
  // ALO for its unfolded node.
  t = term_resolve(t);
  u8  tag = term_tag(t);
  u32 ext = term_ext(t);
  u64 val = term_val(t);
  u64 b;

  switch (tag) {
    case TAG_VAR: {
      // The binder LAM was mapped before its body was walked; a free VAR
      // (binder outside the snapshot) becomes VAR(0).
      if (!remap_find(map, *map_pos, val, &b)) b = 0;
      return term_new(0, TAG_VAR, ext, b);
    }

    case TAG_DP0:
    case TAG_DP1: {
      // A grad projection's [y, gy, target] cell is copied whole below;
      // alo_realize gives each instance its own copy.
      if (ext & DUP_GRAD_FLAG) break;
      // A plain projection becomes BJ0/BJ1, Levy-opaque under wnf and cnf;
      // alo_realize turns it back into a DP per instance, its dup cell shared
      // by both sides (HVM4's DP/BJ split, HVM4 clang/cnf/_.c).
      u8 bj_tag = (tag == TAG_DP0) ? TAG_BJ0 : TAG_BJ1;
      if (remap_find(map, *map_pos, val, &b)) return term_new(0, bj_tag, ext, b);
      b = remap_add(map, map_pos, val, 1);
      book_set(b, clone_to_book_rec(heap_read(val), map, map_pos));
      return term_new(0, bj_tag, ext, b);
    }

    case TAG_LAM: {
      // Re-seal LAM_ERA_MASK from the dyn body, for a LAM built with ext 0 (say
      // by a test that bypasses lam_seal_ext); the other ext bits survive.
      u32 sealed_ext = lam_seal_ext(val, ext & ~LAM_ERA_MASK);
      if (remap_find(map, *map_pos, val, &b)) return term_new(0, TAG_LAM, sealed_ext, b);
      b = remap_add(map, map_pos, val, 1);
      book_set(b, clone_to_book_rec(heap_read(val), map, map_pos));
      // The book LAM takes the shape; alo_realize copies it onto each instance.
      Shape s;
      if (lam_shape_lookup(val, &s)) lam_shape_set_book(b, &s);
      return term_new(0, TAG_LAM, sealed_ext, b);
    }

    default:
      break;
  }

  u32 ar = term_cells(t, HEAP);
  if (ar == 0) return t;
  if (remap_find(map, *map_pos, val, &b)) return term_new(0, tag, ext, b);
  b = remap_add(map, map_pos, val, ar);
  for (u32 i = 0; i < ar; i++) {
    book_set(b + i, clone_to_book_rec(heap_read(val + i), map, map_pos));
  }
  return term_new(0, tag, ext, b);
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
