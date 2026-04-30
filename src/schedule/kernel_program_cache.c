// schedule/kernel_program_cache.c - hash-cons cache for KProgOp[]
// arrays.
//
// Two `KernelEntry` boundaries with bit-for-bit identical
// `program[]` (opcode + dtype + n_src + arg + numel + src[] + the
// shape/perm/pad metadata) describe the same compute kernel
// modulo I/O.  The recursive-lambda training loop hits this case
// every iteration: `w_K` differs across iters but the program
// structure is invariant (the abstract `KSRC_AS_INPUT(slot)` /
// `KSRC_INDEX(j)` patterns don't reference concrete tids).
//
// This cache lets every emitted kernel share the underlying
// `KProgOp[]` array.  Saves program memory; the
// `metal_pipeline_for` / `cpu_op_*` dispatcher cache was already
// keyed on opcode at fire time, so compiled-shader sharing was
// already working -- this just removes the redundant in-memory
// copies and gives us a structural signature on each kernel that
// later passes (loop-pattern detection, kid sharing) can key off.
//
// Cleared from `thvm_init` via `kernel_program_cache_reset`; entries
// own the program arrays for their lifetime.

#define KP_CACHE_CAP (1u << 14)            // 16K slots

struct KpCacheSlot {
  u64        key;            // FNV-1a hash; 0 = empty slot
  KProgOp   *program;        // cache-owned, freed on reset
  u32        n_ops;
  KernelAxes axes;           // shared scheduling plan: every kid that
                             // hits this slot points at &axes through
                             // KernelEntry.axes (Phase 16: per-
                             // program-shape opt sharing).  Mutated
                             // by axes_apply_opt; read by cg_emit and
                             // cpu_jit_hash so the same opt automatically
                             // applies to every kernel with this
                             // program shape.
};

static KpCacheSlot KP_CACHE[KP_CACHE_CAP];

fn void kernel_program_cache_reset(void) {
  for (u32 i = 0; i < KP_CACHE_CAP; i++) {
    if (KP_CACHE[i].program != NULL) free(KP_CACHE[i].program);
    KP_CACHE[i].key     = 0;
    KP_CACHE[i].program = NULL;
    KP_CACHE[i].n_ops   = 0;
  }
}

// Hash the bytes of every populated KProgOp.  We hash the entire
// struct: KProgOp is plain data with no pointers, so memcmp /
// memcpy are correct, and bytes capture every relevant field
// (opcode, dtype, n_src, src[], arg, numel, out_ndim, out_dims,
// pad_widths, axis_perm).  Using the struct size keeps us
// future-proof against new fields.
static u64 kp_program_hash(KProgOp const *prog, u32 n_ops) {
  u64 h = 0xcbf29ce484222325ULL;
  h ^= (u64)n_ops; h *= 0x100000001b3ULL;
  u8 const *bytes = (u8 const *)prog;
  size_t total = (size_t)n_ops * sizeof(KProgOp);
  for (size_t i = 0; i < total; i++) {
    h ^= (u64)bytes[i]; h *= 0x100000001b3ULL;
  }
  return h | (1ULL << 63);   // never zero
}

static int kp_program_equal(KProgOp const *a, u32 a_n,
                             KProgOp const *b, u32 b_n) {
  if (a_n != b_n) return 0;
  return memcmp(a, b, (size_t)a_n * sizeof(KProgOp)) == 0;
}

// Look up; on hit, sets *out_n_ops and returns the cached pointer.
// On miss, returns NULL.  The returned pointer is cache-owned --
// the caller must mark its KernelEntry as program_shared.
fn KProgOp *kernel_program_cache_lookup(KProgOp const *prog,
                                        u32 n_ops,
                                        u32 *out_n_ops) {
  if (n_ops == 0) return NULL;
  u64 key = kp_program_hash(prog, n_ops);
  u32 mask = KP_CACHE_CAP - 1;
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < KP_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    KpCacheSlot *s = &KP_CACHE[i];
    if (s->key == 0) return NULL;
    if (s->key == key &&
        kp_program_equal(prog, n_ops, s->program, s->n_ops)) {
      *out_n_ops = s->n_ops;
      return s->program;
    }
  }
  return NULL;
}

// Slot-bearing variant: returns the cache slot whose program equals
// `prog`, NULL on miss.  Used when the caller needs not just the
// interned program pointer but also the shared `axes` so kids
// sharing this program inherit any opts already applied (and any
// future opts apply to all of them).  Phase 16 per-program-shape
// opt sharing.
fn KpCacheSlot *kernel_program_cache_lookup_slot(KProgOp const *prog,
                                                  u32 n_ops) {
  if (n_ops == 0) return NULL;
  u64 key = kp_program_hash(prog, n_ops);
  u32 mask = KP_CACHE_CAP - 1;
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < KP_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    KpCacheSlot *s = &KP_CACHE[i];
    if (s->key == 0) return NULL;
    if (s->key == key &&
        kp_program_equal(prog, n_ops, s->program, s->n_ops)) {
      return s;
    }
  }
  return NULL;
}

// Slot-bearing insert: same as kernel_program_cache_insert but
// returns the slot pointer (so caller can grab &slot->axes).
fn KpCacheSlot *kernel_program_cache_insert_slot(KProgOp const *prog, u32 n_ops) {
  if (n_ops == 0) return NULL;
  u64 key = kp_program_hash(prog, n_ops);
  u32 mask = KP_CACHE_CAP - 1;
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < KP_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    KpCacheSlot *s = &KP_CACHE[i];
    if (s->key == 0) {
      KProgOp *owned = (KProgOp *)malloc((size_t)n_ops * sizeof(KProgOp));
      memcpy(owned, prog, (size_t)n_ops * sizeof(KProgOp));
      s->key     = key;
      s->program = owned;
      s->n_ops   = n_ops;
      memset(&s->axes, 0, sizeof(KernelAxes));    // axes default-init
                                                  // happens at materialize-
                                                  // time via axes_default_for.
      return s;
    }
  }
  return NULL;
}

// Insert: copies the program bytes into a tight cache-owned
// buffer (so we don't depend on the caller's potentially-
// oversized realloc'd buffer).  Returns the cache pointer; the
// caller frees its own buffer and adopts the cache pointer with
// program_shared=1.  No-op + returns NULL if the cache is full.
fn KProgOp *kernel_program_cache_insert(KProgOp const *prog, u32 n_ops) {
  if (n_ops == 0) return NULL;
  u64 key = kp_program_hash(prog, n_ops);
  u32 mask = KP_CACHE_CAP - 1;
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < KP_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    KpCacheSlot *s = &KP_CACHE[i];
    if (s->key == 0) {
      KProgOp *owned = (KProgOp *)malloc((size_t)n_ops * sizeof(KProgOp));
      memcpy(owned, prog, (size_t)n_ops * sizeof(KProgOp));
      s->key = key;
      s->program = owned;
      s->n_ops = n_ops;
      return owned;
    }
    // (Hash collisions: we only insert on empty slots; a duplicate
    //  hash that doesn't match equality will just probe past.)
  }
  return NULL;   // full -- caller keeps its own buffer
}

// Stats for tests / introspection.
fn u32 kernel_program_cache_size(void) {
  u32 n = 0;
  for (u32 i = 0; i < KP_CACHE_CAP; i++) {
    if (KP_CACHE[i].key != 0) n++;
  }
  return n;
}
