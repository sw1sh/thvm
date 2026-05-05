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

struct KAxisCacheSlot {
  u64        key;
  ScalarUop *scalar_uops;
  u32        n_scalar_uops;
  u32        n_inputs;
  u32       *input_dtypes;
  u32       *input_numels;
  u32        output_dtype;
  u32        output_numel;
  Shape      output_shape;
  u8         source_tag;
  u32        source_ext;
  KernelAxes axes;
};
typedef struct KAxisCacheSlot KAxisCacheSlot;

static KpCacheSlot KP_CACHE[KP_CACHE_CAP];
static KAxisCacheSlot KAXIS_CACHE[KP_CACHE_CAP];

fn void kernel_program_cache_reset(void) {
  for (u32 i = 0; i < KP_CACHE_CAP; i++) {
    if (KP_CACHE[i].program != NULL) free(KP_CACHE[i].program);
    KP_CACHE[i].key     = 0;
    KP_CACHE[i].program = NULL;
    KP_CACHE[i].n_ops   = 0;
    if (KAXIS_CACHE[i].scalar_uops != NULL) free(KAXIS_CACHE[i].scalar_uops);
    if (KAXIS_CACHE[i].input_dtypes != NULL) free(KAXIS_CACHE[i].input_dtypes);
    if (KAXIS_CACHE[i].input_numels != NULL) free(KAXIS_CACHE[i].input_numels);
    memset(&KAXIS_CACHE[i], 0, sizeof(KAXIS_CACHE[i]));
  }
}

static u64 kp_hash_bytes(u64 h, void const *ptr, size_t n) {
  u8 const *bytes = (u8 const *)ptr;
  for (size_t i = 0; i < n; i++) {
    h ^= (u64)bytes[i]; h *= 0x100000001b3ULL;
  }
  return h;
}

static u64 kp_hash_u64(u64 h, u64 x) {
  return kp_hash_bytes(h, &x, sizeof(x));
}

// Hash the bytes of every populated KProgOp.  We hash the entire
// struct: KProgOp is plain data with no pointers, so memcmp /
// memcpy are correct, and bytes capture every relevant field
// (opcode, dtype, n_src, src[], arg, numel, out_ndim, out_dims,
// pad_widths, axis_perm).  Using the struct size keeps us
// future-proof against new fields.
static u64 kp_program_hash(KProgOp const *prog, u32 n_ops) {
  u64 h = 0xcbf29ce484222325ULL;
  h = kp_hash_u64(h, (u64)n_ops);
  h = kp_hash_bytes(h, prog, (size_t)n_ops * sizeof(KProgOp));
  return h | (1ULL << 63);   // never zero
}

fn u64 kernel_program_key(KProgOp const *prog, u32 n_ops) {
  if (prog == NULL || n_ops == 0) {
    return 0;
  }
  return kp_program_hash(prog, n_ops);
}

fn u64 kernel_rangeified_key(KernelEntry const *ke) {
  if (ke == NULL) {
    return 0;
  }
  u64 h = 0xcbf29ce484222325ULL ^ 0x52414E4745584B41ULL;
  if (ke->scalar_uops != NULL && ke->n_scalar_uops > 0) {
    h = kp_hash_u64(h, 1);
    h = kp_hash_u64(h, (u64)ke->n_scalar_uops);
    h = kp_hash_bytes(h, ke->scalar_uops,
                      (size_t)ke->n_scalar_uops * sizeof(ScalarUop));
  } else if (ke->axes != NULL && ke->axes->n_axes > 0) {
    h = kp_hash_u64(h, 2);
    h = kp_hash_u64(h, (u64)term_tag(ke->source_uop));
    h = kp_hash_u64(h, (u64)term_ext(ke->source_uop));
    // Phase E: hash axis info via tile_anno's shared helper.
    h = tile_anno_hash_axes(ke, h);
  } else {
    return 0;
  }
  h = kp_hash_u64(h, (u64)ke->n_inputs);
  for (u32 i = 0; i < ke->n_inputs; i++) {
    h = kp_hash_u64(h, (u64)ke->input_dtypes[i]);
    h = kp_hash_u64(h, (u64)ke->input_numels[i]);
  }
  h = kp_hash_u64(h, (u64)ke->output_dtype);
  h = kp_hash_u64(h, (u64)ke->output_numel);
  h = kp_hash_u64(h, (u64)ke->output_shape.ndim);
  h = kp_hash_bytes(h, ke->output_shape.dims,
                    (size_t)ke->output_shape.ndim * sizeof(u32));
  return (h & 0x3FFFFFFFFFFFFFFFULL) | (1ULL << 62);
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

static int kaxis_slot_equal(KAxisCacheSlot const *s, KernelEntry const *ke) {
  if (s->n_scalar_uops != ke->n_scalar_uops) {
    return 0;
  }
  if (memcmp(s->scalar_uops, ke->scalar_uops,
             (size_t)ke->n_scalar_uops * sizeof(ScalarUop)) != 0) {
    return 0;
  }
  if (s->n_inputs != ke->n_inputs
      || s->output_dtype != ke->output_dtype
      || s->output_numel != ke->output_numel
      || s->output_shape.ndim != ke->output_shape.ndim
      || s->source_tag != term_tag(ke->source_uop)
      || s->source_ext != term_ext(ke->source_uop)
      || memcmp(s->output_shape.dims, ke->output_shape.dims,
                (size_t)s->output_shape.ndim * sizeof(u32)) != 0) {
    return 0;
  }
  if (s->n_scalar_uops == 0) {
    if (ke->scalar_uops != NULL && ke->n_scalar_uops > 0) {
      return 0;
    }
    if (ke->axes == NULL) return 0;
    // Phase E: compare via tile_anno on the live side; stored side
    // s->axes still holds the byte arrays.  Symmetric with the hash
    // above (only kax_type + extent participate).
    u32 n_axes_c = tile_anno_axis_count_or_kernelaxes(ke);
    if (s->axes.n_axes != n_axes_c) return 0;
    for (u32 i = 0; i < n_axes_c; i++) {
      TileAxisInfo info;
      if (!tile_anno_axis_or_kernelaxes(ke, i, &info)) return 0;
      if ((u32)s->axes.axis_types[i] != info.kax_type) return 0;
      if (s->axes.full_shape [i] != info.extent)       return 0;
    }
  }
  if (s->n_inputs > 0) {
    if (memcmp(s->input_dtypes, ke->input_dtypes,
               (size_t)s->n_inputs * sizeof(u32)) != 0) {
      return 0;
    }
    if (memcmp(s->input_numels, ke->input_numels,
               (size_t)s->n_inputs * sizeof(u32)) != 0) {
      return 0;
    }
  }
  return 1;
}

fn KernelAxes *kernel_rangeified_axes_cache_lookup_or_insert(KernelEntry const *ke) {
  u64 key = kernel_rangeified_key(ke);
  if (key == 0) {
    return NULL;
  }
  u32 mask = KP_CACHE_CAP - 1;
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  for (u32 probe = 0; probe < KP_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    KAxisCacheSlot *s = &KAXIS_CACHE[i];
    if (s->key == 0) {
      memset(s, 0, sizeof(*s));
      if (ke->scalar_uops != NULL && ke->n_scalar_uops > 0) {
        s->scalar_uops = (ScalarUop *)malloc((size_t)ke->n_scalar_uops
                                             * sizeof(ScalarUop));
        if (s->scalar_uops == NULL) {
          return NULL;
        }
        memcpy(s->scalar_uops, ke->scalar_uops,
               (size_t)ke->n_scalar_uops * sizeof(ScalarUop));
      }
      if (ke->n_inputs > 0) {
        s->input_dtypes = (u32 *)malloc((size_t)ke->n_inputs * sizeof(u32));
        s->input_numels = (u32 *)malloc((size_t)ke->n_inputs * sizeof(u32));
        if (s->input_dtypes == NULL || s->input_numels == NULL) {
          free(s->scalar_uops);
          free(s->input_dtypes);
          free(s->input_numels);
          memset(s, 0, sizeof(*s));
          return NULL;
        }
        memcpy(s->input_dtypes, ke->input_dtypes,
               (size_t)ke->n_inputs * sizeof(u32));
        memcpy(s->input_numels, ke->input_numels,
               (size_t)ke->n_inputs * sizeof(u32));
      }
      s->key           = key;
      s->n_scalar_uops = ke->n_scalar_uops;
      s->n_inputs      = ke->n_inputs;
      s->output_dtype  = ke->output_dtype;
      s->output_numel  = ke->output_numel;
      s->output_shape  = ke->output_shape;
      s->source_tag    = term_tag(ke->source_uop);
      s->source_ext    = term_ext(ke->source_uop);
      if (ke->axes != NULL) {
        s->axes = *ke->axes;
      }
      return &s->axes;
    }
    if (s->key == key && kaxis_slot_equal(s, ke)) {
      return &s->axes;
    }
  }
  return NULL;
}

// Stats for tests / introspection.
fn u32 kernel_program_cache_size(void) {
  u32 n = 0;
  for (u32 i = 0; i < KP_CACHE_CAP; i++) {
    if (KP_CACHE[i].key != 0) n++;
  }
  return n;
}
