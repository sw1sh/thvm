// Shapes of TLam-bound vars, keyed by the binding LAM's loc, so materialize can
// compile a body before APP-LAM beta binds its var (term_shape_in reads them).
//
// A side table rather than a 2-cell LAM: every consumer assumes heap[lam_loc]
// is the body.
//
// Key: bit 63 marks a book loc (book and dyn locs overlap), bits 56..59 the
// context slot (contexts share this table and reuse locs), bits 0..37 the loc.

#define LAM_SHAPE_CAP        (1u << 12)
#define LAM_SHAPE_BOOK_BIT   (1ULL << 63)
#define LAM_SHAPE_CTX_SHIFT  56
#define LAM_SHAPE_CTX_MASK   (0xFULL << LAM_SHAPE_CTX_SHIFT)

static inline u64 lam_shape_ctx_key(u64 raw) {
  u64 ctx = (u64)thvm_context_current() & 0xF;
  return raw | (ctx << LAM_SHAPE_CTX_SHIFT);
}

typedef struct {
  u8    occupied;    // a flag, so that no key value has to mean empty
  u64   key;
  Shape shape;
} LamShapeSlot;
static LamShapeSlot LAM_SHAPE_TABLE[LAM_SHAPE_CAP];

fn void lam_shape_reset(void) {
  memset(LAM_SHAPE_TABLE, 0, sizeof(LAM_SHAPE_TABLE));
}

static inline u32 lam_shape_hash(u64 key) {
  // MurmurHash3's fmix64, so dense small locs and the high book and context
  // bits all reach the masked low bits.
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
  // table full: the annotation is dropped
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

// For the tests (lam_shape.wlt).
fn u32 lam_shape_count(void) {
  u32 n = 0;
  for (u32 i = 0; i < LAM_SHAPE_CAP; i++) {
    if (LAM_SHAPE_TABLE[i].occupied) n++;
  }
  return n;
}
