// lam/shape.c -- shape annotation side table for TLam-bound vars.
//
// The IC machinery (LAM / VAR / APP-LAM beta) is shape-agnostic
// by design.  But for materialize to compile a lambda body BEFORE
// substitution -- i.e. before APP-LAM beta resolves the bound var
// to a concrete tensor -- we need each TVAR(loc) to know what
// shape its eventual binding will have.
//
// We store this annotation in a side table keyed by the binding
// LAM's heap loc.  `TLamShape[shape, body]` at the WL surface
// records (lam_loc -> shape) here; `term_shape_in(TVAR(loc))`
// consults this table before falling through to its existing
// "shape unknown" path.
//
// Lifecycle: cleared on thvm_init (the heap is reset, all locs
// invalidated).  Annotations are propagated when LAMs are
// duplicated by clone_to_book_rec (dyn -> book) and alo_realize
// (book -> dyn) so each instantiation carries the shape.
//
// Why a side table rather than a 2-cell LAM layout: LAM is
// 1-cell across the entire IC (APP-LAM, ANN-LAM, GC, alo,
// book, materialize all assume `heap[lam_loc]` = body Term).
// Threading a 2-cell variant through every consumer is deeply
// invasive; the side table is opt-in (zero cost for unannotated
// LAMs) and stays out of the IC's reduction rules entirely.
//
// Capacity: 4K slots, linear-probed on collision.  A typical
// program has a handful of shape-annotated lambdas; for
// recursive loops the SAME lambda body is reused so even a 1000-
// iter loop only registers one annotation.
//
// Book and dyn heap locs live in separate namespaces (BOOK_HEAP
// is its own buffer), so a raw `u64` could collide between a
// book LAM and a dyn LAM at the same numeric loc.  We use the
// top bit (LAM_SHAPE_BOOK_BIT) to distinguish: book locs OR'd
// with this bit on insert/lookup; dyn locs use the raw value.
//
// Multi-context: HEAP and BOOK_HEAP are per-context (each context's
// heap_next / book_next restart at 0), but this table is a single
// file-static global shared across every context (see thvm.c
// thvm_context_destroy -- wiping it per-context would clobber other
// live contexts' entries).  So two contexts' lambdas can land at the
// SAME numeric loc and collide here: whichever inserted second would
// overwrite the first context's shape, and a lookup in the first
// context would then read the second's dims -- the FLUX
// cross-session bug (a 2nd weight-base context's velJit capture
// reused the 1st context's locs, read a stale/wrong shape, and
// emitted a malformed `{}[[2]]` tile-JIT kernel).  We fold the
// current context's slot id (0..15) into the key so cross-context
// locs never collide.  Layout of the u64 key:
//   bit  63       : BOOK flag (book loc vs dyn loc)
//   bits 56..59   : context slot id (CONTEXTS_CAP == 16 -> 4 bits)
//   bits 0..37    : raw loc (VAL_BITS == 38 bounds an addressable loc)

#define LAM_SHAPE_CAP        (1u << 12)
#define LAM_SHAPE_BOOK_BIT   (1ULL << 63)
#define LAM_SHAPE_CTX_SHIFT  56
#define LAM_SHAPE_CTX_MASK   (0xFULL << LAM_SHAPE_CTX_SHIFT)

// Fold the current context's slot id into a raw (dyn or book) loc.
static inline u64 lam_shape_ctx_key(u64 raw) {
  u64 ctx = (u64)thvm_context_current() & 0xF;
  return raw | (ctx << LAM_SHAPE_CTX_SHIFT);
}

typedef struct {
  u8    occupied;    // 0 = empty slot (key=0 / loc=0 are valid keys,
                     // so we can't use them as the "empty" sentinel)
  u64   key;         // raw dyn loc OR (book loc | BOOK_BIT)
  Shape shape;
} LamShapeSlot;
static LamShapeSlot LAM_SHAPE_TABLE[LAM_SHAPE_CAP];

fn void lam_shape_reset(void) {
  memset(LAM_SHAPE_TABLE, 0, sizeof(LAM_SHAPE_TABLE));
}

static inline u32 lam_shape_hash(u64 key) {
  // Mix bits; loc is dense small integers, so a simple Wang-style
  // mix avoids clustering.  BOOK_BIT lives high so it propagates
  // through the avalanche.
  key ^= key >> 33; key *= 0xff51afd7ed558ccdULL;
  key ^= key >> 33; key *= 0xc4ceb9fe1a85ec53ULL;
  key ^= key >> 33;
  return (u32)key & (LAM_SHAPE_CAP - 1);
}

static void lam_shape_set_keyed(u64 key, Shape const *shape) {
  u32 h = lam_shape_hash(key);
  for (u32 probe = 0; probe < LAM_SHAPE_CAP; probe++) {
    u32 i = (h + probe) & (LAM_SHAPE_CAP - 1);
    LamShapeSlot *s = &LAM_SHAPE_TABLE[i];
    if (!s->occupied || s->key == key) {
      s->occupied = 1;
      s->key      = key;
      s->shape    = *shape;
      return;
    }
  }
  // table full -- silently drop
}

static int lam_shape_lookup_keyed(u64 key, Shape *out) {
  u32 h = lam_shape_hash(key);
  for (u32 probe = 0; probe < LAM_SHAPE_CAP; probe++) {
    u32 i = (h + probe) & (LAM_SHAPE_CAP - 1);
    LamShapeSlot *s = &LAM_SHAPE_TABLE[i];
    if (!s->occupied) return 0;
    if (s->key == key) { *out = s->shape; return 1; }
  }
  return 0;
}

fn void lam_shape_set(u64 lam_loc, Shape const *shape) {
  lam_shape_set_keyed(lam_shape_ctx_key(lam_loc), shape);
}
fn int lam_shape_lookup(u64 lam_loc, Shape *out) {
  return lam_shape_lookup_keyed(lam_shape_ctx_key(lam_loc), out);
}

fn void lam_shape_set_book(u64 book_loc, Shape const *shape) {
  lam_shape_set_keyed(lam_shape_ctx_key(book_loc) | LAM_SHAPE_BOOK_BIT, shape);
}
fn int lam_shape_lookup_book(u64 book_loc, Shape *out) {
  return lam_shape_lookup_keyed(lam_shape_ctx_key(book_loc) | LAM_SHAPE_BOOK_BIT, out);
}

// Stats / introspection for tests.
fn u32 lam_shape_count(void) {
  u32 n = 0;
  for (u32 i = 0; i < LAM_SHAPE_CAP; i++) {
    if (LAM_SHAPE_TABLE[i].occupied) n++;
  }
  return n;
}
