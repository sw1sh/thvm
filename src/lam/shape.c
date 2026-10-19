// Shapes of TLam-bound vars, keyed by the binding LAM's loc, so materialize can
// compile a body before APP-LAM beta binds its var (term_shape_in reads them).
//
// A side table rather than a 2-cell LAM: every consumer assumes heap[lam_loc]
// is the body.
//
// Key: bit 63 marks a book loc (book and dyn locs overlap), bits 56..59 the
// context slot (contexts share this table and reuse locs), bits 0..37 the loc.

#define LAM_SHAPE_INIT_CAP   (1u << 12)
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
// It doubles at half load: each unfold of a shaped def records its instance's
// shape, so a long recursive loop outgrows any fixed size between collections.
static LamShapeSlot *LAM_SHAPE_TABLE = NULL;
static u32           LAM_SHAPE_CAP   = 0;    // a power of two, or 0
static u32           LAM_SHAPE_LEN   = 0;

fn void lam_shape_reset(void) {
  if (LAM_SHAPE_TABLE != NULL) {
    memset(LAM_SHAPE_TABLE, 0, (size_t)LAM_SHAPE_CAP * sizeof *LAM_SHAPE_TABLE);
  }
  LAM_SHAPE_LEN = 0;
}

static inline u32 lam_shape_hash(u64 key) {
  // MurmurHash3's fmix64, so dense small locs and the high book and context
  // bits all reach the masked low bits.
  key ^= key >> 33; key *= 0xff51afd7ed558ccdULL;
  key ^= key >> 33; key *= 0xc4ceb9fe1a85ec53ULL;
  key ^= key >> 33;
  return (u32)key;
}

static LamShapeSlot *lam_shape_slot(LamShapeSlot *table, u32 cap, u64 key) {
  u32 i = lam_shape_hash(key) & (cap - 1);
  while (table[i].occupied && table[i].key != key) {
    i = (i + 1) & (cap - 1);
  }
  return &table[i];
}

static void lam_shape_grow(void) {
  u32           cap   = LAM_SHAPE_CAP ? 2 * LAM_SHAPE_CAP : LAM_SHAPE_INIT_CAP;
  LamShapeSlot *table = (LamShapeSlot *)calloc(cap, sizeof *table);
  if (table == NULL) {
    thvm_fatal("lam_shape: out of memory");
  }
  for (u32 i = 0; i < LAM_SHAPE_CAP; i++) {
    if (LAM_SHAPE_TABLE[i].occupied) {
      *lam_shape_slot(table, cap, LAM_SHAPE_TABLE[i].key) = LAM_SHAPE_TABLE[i];
    }
  }
  free(LAM_SHAPE_TABLE);
  LAM_SHAPE_TABLE = table;
  LAM_SHAPE_CAP   = cap;
}

static void lam_shape_set_keyed(u64 key, Shape const *shape) {
  if (2 * (LAM_SHAPE_LEN + 1) > LAM_SHAPE_CAP) {
    lam_shape_grow();
  }
  LamShapeSlot *s = lam_shape_slot(LAM_SHAPE_TABLE, LAM_SHAPE_CAP, key);
  if (!s->occupied) {
    s->occupied = 1;
    s->key      = key;
    LAM_SHAPE_LEN++;
  }
  s->shape = *shape;
}

static int lam_shape_lookup_keyed(u64 key, Shape *out) {
  if (LAM_SHAPE_CAP == 0) return 0;
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
