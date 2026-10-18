// schedule/realize_waves.c - order the roots of a realize_many bundle by
// their buffer read/write hazards.
//
// tinygrad's create_schedule (tinygrad/schedule/__init__.py:32-83) links
// kernels by buffer STATE.  An assign's write supersedes the state its
// target held (`writes` is keyed by the superseded state, :44).  A reader
// of a state runs after the kernels that produced it (RAW, :51-56) and
// before every kernel whose write supersedes it (WAR, :57-65), unless that
// writer is already in the reader's backward slice.  A cycle raises
// "cycle detected in assign graph" (:82).
//
// realize_many cannot order single kernels: wnf(root) fires every kernel
// and ASSIGN the root reaches, on demand and in dependency order.  So the
// edges link ROOTS.  Per root r the walk records the states r reads (a
// TAG_TEN leaf, the buffer's prior state, or an ASSIGN node, its
// post-write state, reached other than as an ASSIGN dst) and the ASSIGN
// nodes r reaches, each of which fires inside wnf(r) unless an earlier
// root fired it.  WAR j -> i when root j reads a state that an ASSIGN
// reached by root i supersedes and root j does not reach that ASSIGN (the
// backward-slice exemption).  A WAR edge puts i in a later wave than j.
// RAW edges need no barrier, because a root fires the ASSIGNs it reads on
// demand; they only order the roots inside a wave.
//
// Root edges cannot reorder a prior-state read and a write that sit under
// the SAME root; tinygrad orders those per kernel.

// A root set is a bitset over the bundle's roots, RW_W words, kept in one
// word pool and named by its offset there; a bundle of any size gets the
// whole analysis (tinygrad's create_schedule has no root cap).
typedef u32 RwSet;

enum { RW_EDGE_ROOT = 0, RW_EDGE_READ = 1, RW_EDGE_DST = 2 };

// A state key is an ASSIGN node's heap loc (its post-write state) or a
// buffer's prior state, tagged in the high bits.
#define RW_KEY_BUF (1ULL << 63)
#define RW_KEY_TID (1ULL << 62)

typedef struct {
  u64   key;
  RwSet readers;    // roots that read this state
  RwSet firers;     // ASSIGN states: roots that reach the ASSIGN
  u32   sup;        // ASSIGN states: 1 + index of the superseded state
  u8    sup_done;
} RwState;

typedef struct { Term t; u32 edge; } RwItem;

typedef struct { u64 loc; u8 used; RwSet roots; } RwRootLoc;

// A visited-set slot: a heap loc, or a kernel id tagged RW_SEEN_KERNEL.
typedef struct { u64 key; u32 stamp; } RwSeen;   // stamp: epoch << 2 | edge bits

#define RW_SEEN_KERNEL (1ULL << 63)

static RwState  *RW_STATES     = NULL;
static u32       RW_STATES_LEN = 0;
static u32       RW_STATES_CAP = 0;
static u32      *RW_INDEX      = NULL;   // 1 + state index, 0 = empty slot
static u32       RW_INDEX_CAP  = 0;      // power of two
static RwItem   *RW_STACK      = NULL;
static u32       RW_STACK_LEN  = 0;
static u32       RW_STACK_CAP  = 0;
static RwSeen   *RW_SEEN       = NULL;
static u64       RW_SEEN_CAP   = 0;      // power of two
static u64       RW_SEEN_LEN   = 0;      // slots stamped in the current epoch
static u32       RW_EPOCH      = 0;
static u64       *RW_POOL        = NULL;
static u32        RW_POOL_LEN    = 0;    // words handed out this call
static u32        RW_POOL_CAP    = 0;
static u32        RW_W           = 0;    // words per set: (n + 63) / 64
static RwRootLoc *RW_ROOTLOC     = NULL;
static u32        RW_ROOTLOC_CAP = 0;    // power of two, >= 2 * n
static RwSet     *RW_WAR_SUCC    = NULL; // per root
static RwSet     *RW_RAW_PRED    = NULL; // per root
static u32        RW_ROOTS_CAP   = 0;

// A new empty set into *out; 0 on OOM.  The pool may move, so a set is
// only ever addressed through its offset.
static int rw_set_new(RwSet *out) {
  if (RW_POOL_LEN + RW_W > RW_POOL_CAP) {
    u32  cap   = (RW_POOL_CAP != 0) ? RW_POOL_CAP * 2 : 4096;
    while (cap < RW_POOL_LEN + RW_W) cap *= 2;
    u64 *grown = (u64 *)realloc(RW_POOL, (size_t)cap * sizeof(u64));
    if (grown == NULL) return 0;
    RW_POOL     = grown;
    RW_POOL_CAP = cap;
  }
  *out = RW_POOL_LEN;
  memset(&RW_POOL[RW_POOL_LEN], 0, (size_t)RW_W * sizeof(u64));
  RW_POOL_LEN += RW_W;
  return 1;
}

fn void rw_set_add(RwSet s, u32 i) {
  RW_POOL[s + (i >> 6)] |= 1ULL << (i & 63);
}

fn void rw_set_del(RwSet s, u32 i) {
  RW_POOL[s + (i >> 6)] &= ~(1ULL << (i & 63));
}

fn int rw_set_has(RwSet s, u32 i) {
  return (int)((RW_POOL[s + (i >> 6)] >> (i & 63)) & 1ULL);
}

fn void rw_set_or(RwSet dst, RwSet src) {
  for (u32 x = 0; x < RW_W; x++) RW_POOL[dst + x] |= RW_POOL[src + x];
}

fn u64 rw_hash(u64 k) {
  k ^= k >> 33; k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ULL;
  k ^= k >> 33;
  return k;
}

// The visited set is sized to the largest walk, not to the heap: the walk
// runs on every multi-root realize, and a table indexed by heap loc would
// keep every page any walk touched resident for the life of the process.
// A slot stamped in an older epoch counts as empty, so a new walk needs no
// clear.
static void rw_epoch_next(void) {
  RW_SEEN_LEN = 0;
  if (++RW_EPOCH >= (1u << 30)) {
    if (RW_SEEN != NULL) {
      memset(RW_SEEN, 0, RW_SEEN_CAP * sizeof(RwSeen));
    }
    RW_EPOCH = 1;
  }
}

static int rw_seen_grow(void) {
  u64     cap  = (RW_SEEN_CAP != 0) ? RW_SEEN_CAP * 2 : 4096;
  RwSeen *seen = (RwSeen *)calloc(cap, sizeof(RwSeen));
  if (seen == NULL) {
    return 0;
  }
  for (u64 s = 0; s < RW_SEEN_CAP; s++) {
    if ((RW_SEEN[s].stamp >> 2) != RW_EPOCH) {
      continue;
    }
    u64 h = rw_hash(RW_SEEN[s].key) & (cap - 1);
    while ((seen[h].stamp >> 2) == RW_EPOCH) {
      h = (h + 1) & (cap - 1);
    }
    seen[h] = RW_SEEN[s];
  }
  free(RW_SEEN);
  RW_SEEN     = seen;
  RW_SEEN_CAP = cap;
  return 1;
}

// 1 the first time `key` gets edge bit `bit` in the current epoch, 0 when
// it already has it, -1 on OOM.
fn int rw_mark(u64 key, u32 bit) {
  if ((RW_SEEN_LEN + 1) * 2 > RW_SEEN_CAP && !rw_seen_grow()) {
    return -1;
  }
  u64 mask = RW_SEEN_CAP - 1;
  u64 h    = rw_hash(key) & mask;
  while ((RW_SEEN[h].stamp >> 2) == RW_EPOCH) {
    if (RW_SEEN[h].key == key) {
      if (RW_SEEN[h].stamp & bit) {
        return 0;
      }
      RW_SEEN[h].stamp |= bit;
      return 1;
    }
    h = (h + 1) & mask;
  }
  RW_SEEN[h].key   = key;
  RW_SEEN[h].stamp = (RW_EPOCH << 2) | bit;
  RW_SEEN_LEN++;
  return 1;
}

// rw_mark for a heap node reached through an edge of this kind.  READ and
// ROOT share a bit; a DST reach is separate because a view reached as an
// ASSIGN dst passes the dst role down.
fn int rw_first_visit(u64 loc, u32 edge) {
  return rw_mark(loc, (edge == RW_EDGE_DST) ? 2u : 1u);
}

// rw_mark for a kernel; 0 for an id that names no kernel.
fn int rw_first_kernel(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) {
    return 0;
  }
  return rw_mark(RW_SEEN_KERNEL | kid, 1u);
}

static int rw_push(Term t, u32 edge) {
  if (RW_STACK_LEN == RW_STACK_CAP) {
    u32 cap = (RW_STACK_CAP != 0) ? RW_STACK_CAP * 2 : 1024;
    RwItem *grown = (RwItem *)realloc(RW_STACK, (size_t)cap * sizeof(RwItem));
    if (grown == NULL) return 0;
    RW_STACK     = grown;
    RW_STACK_CAP = cap;
  }
  RW_STACK[RW_STACK_LEN].t    = t;
  RW_STACK[RW_STACK_LEN].edge = edge;
  RW_STACK_LEN++;
  return 1;
}

// Follow substituted VARs and fired DUP projections without forcing ALOs:
// the walk runs before materialize and must not mutate the heap.
static Term rw_resolve(Term t) {
  for (int hops = 0; hops < 256; hops++) {
    u8 tag = term_tag(t);
    if (tag != TAG_VAR && tag != TAG_DP0 && tag != TAG_DP1) return t;
    Term cell = heap_read(term_val(t));
    if (!term_sub_get(cell)) return t;
    t = term_sub_set(cell, 0);
  }
  return t;
}

// Tags whose term_val is a heap loc the walk descends into.
static int rw_is_node(u8 tag) {
  switch (tag) {
    case TAG_UOP: case TAG_DP0: case TAG_DP1: case TAG_LAM: case TAG_DUP:
    case TAG_APP: case TAG_SUP: case TAG_OP2: case TAG_MAT: case TAG_CTR:
      return 1;
    default:
      return 0;
  }
}

static u64 rw_base_key(u32 tid) {
  TenDesc const *d = &TENS[tid];
  if (d->buf_id == 0) return RW_KEY_TID | tid;
  u64 backend_id = (d->backend != NULL) ? d->backend->id : 0;
  return RW_KEY_BUF | (backend_id << 32) | d->buf_id;
}

static int rw_index_grow(void) {
  u32 cap = (RW_INDEX_CAP != 0) ? RW_INDEX_CAP * 2 : 1024;
  u32 *index = (u32 *)calloc(cap, sizeof(u32));
  if (index == NULL) return 0;
  for (u32 s = 0; s < RW_STATES_LEN; s++) {
    u32 h = (u32)rw_hash(RW_STATES[s].key) & (cap - 1);
    while (index[h] != 0) h = (h + 1) & (cap - 1);
    index[h] = s + 1;
  }
  free(RW_INDEX);
  RW_INDEX     = index;
  RW_INDEX_CAP = cap;
  return 1;
}

// 1 + the index of the state for `key` (inserted if new), 0 on OOM.
static u32 rw_state(u64 key) {
  if ((RW_STATES_LEN + 1) * 2 > RW_INDEX_CAP && !rw_index_grow()) return 0;
  u32 mask = RW_INDEX_CAP - 1;
  u32 h    = (u32)rw_hash(key) & mask;
  while (RW_INDEX[h] != 0) {
    if (RW_STATES[RW_INDEX[h] - 1].key == key) return RW_INDEX[h];
    h = (h + 1) & mask;
  }
  if (RW_STATES_LEN == RW_STATES_CAP) {
    u32 cap = (RW_STATES_CAP != 0) ? RW_STATES_CAP * 2 : 256;
    RwState *grown = (RwState *)realloc(RW_STATES, (size_t)cap * sizeof(RwState));
    if (grown == NULL) return 0;
    RW_STATES     = grown;
    RW_STATES_CAP = cap;
  }
  RwState *st = &RW_STATES[RW_STATES_LEN];
  memset(st, 0, sizeof *st);
  st->key = key;
  if (!rw_set_new(&st->readers) || !rw_set_new(&st->firers)) return 0;
  RW_INDEX[h] = ++RW_STATES_LEN;
  return RW_STATES_LEN;
}

// The state an ASSIGN supersedes: its dst operand resolved through view
// movement ops (the chain materialize aliases to the target buffer).
static int rw_sup_key(Term dst, u64 *key) {
  Term t = rw_resolve(dst);
  for (int hops = 0; hops < 256; hops++) {
    if (term_tag(t) != TAG_UOP || !op_is_view_movement((u8)term_ext(t))) break;
    t = rw_resolve(heap_read(term_val(t) + 0));
  }
  if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_ASSIGN) {
    *key = term_val(t);
    return 1;
  }
  if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_KERNEL) t = heap_read(term_val(t) + 0);
  if (term_tag(t) != TAG_TEN) return 0;
  u32 tid = (u32)term_val(t);
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  *key = rw_base_key(tid);
  return 1;
}

// A kernel reads its inputs when it fires, and a pending output fires its
// producer first (kernel_fire_by_id), so its reads belong to the root.
static int rw_push_kernel(u32 kid) {
  int first = rw_first_kernel(kid);
  if (first <= 0) {
    return first == 0;
  }
  KernelEntry const *ke = &KERNELS[kid];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = (ke->input_tids != NULL) ? ke->input_tids[i] : 0;
    if (tid != 0) {
      if (!rw_push(term_new(0, TAG_TEN, 0, tid), RW_EDGE_READ)) return 0;
    } else if (ke->input_terms != NULL && ke->input_terms[i] != 0) {
      if (!rw_push(ke->input_terms[i], RW_EDGE_READ)) return 0;
    }
  }
  return 1;
}

static int rw_read_ten(u32 tid, u32 r) {
  if (tid == 0 || tid >= TENS_NEXT) return 1;
  u32 s = rw_state(rw_base_key(tid));
  if (s == 0) return 0;
  rw_set_add(RW_STATES[s - 1].readers, r);
  return rw_push_kernel(TENS[tid].producer_kid);
}

static int rw_note_assign(u64 loc, u32 edge, u32 r) {
  u32 s = rw_state(loc);
  if (s == 0) return 0;
  if (!RW_STATES[s - 1].sup_done) {
    RW_STATES[s - 1].sup_done = 1;
    u64 key;
    if (rw_sup_key(heap_read(loc + 0), &key)) {
      u32 sup = rw_state(key);
      if (sup == 0) return 0;
      RW_STATES[s - 1].sup = sup;
    }
  }
  rw_set_add(RW_STATES[s - 1].firers, r);
  if (edge == RW_EDGE_READ) rw_set_add(RW_STATES[s - 1].readers, r);
  return 1;
}

// Push the heap children of node `t`.  Only a view movement op reached as
// an ASSIGN dst passes the dst role to its source.
static int rw_push_children(Term t, u32 edge) {
  u64 loc = term_val(t);
  switch (term_tag(t)) {
    case TAG_UOP: {
      u8 op = (u8)term_ext(t);
      if (op == UOP_ASSIGN) {
        return rw_push(heap_read(loc + 1), RW_EDGE_READ)
            && rw_push(heap_read(loc + 0), RW_EDGE_DST);
      }
      u32 child = (edge == RW_EDGE_DST && op_is_view_movement(op)) ? RW_EDGE_DST : RW_EDGE_READ;
      u8  ar    = uop_arity(op);
      for (u8 i = 0; i < ar; i++) {
        if (!rw_push(heap_read(loc + i), child)) return 0;
      }
      return 1;
    }
    case TAG_DP0:
    case TAG_DP1: {
      // A grad cell is [y, gy, target]; its chain rule reads y and gy.
      if (!rw_push(heap_read(loc), RW_EDGE_READ)) return 0;
      if (term_ext(t) & DUP_GRAD_FLAG) return rw_push(heap_read(loc + 1), RW_EDGE_READ);
      return 1;
    }
    case TAG_LAM:
    case TAG_DUP:
      return rw_push(heap_read(loc), RW_EDGE_READ);
    case TAG_APP:
    case TAG_SUP:
    case TAG_OP2:
    case TAG_MAT:
      return rw_push(heap_read(loc), RW_EDGE_READ)
          && rw_push(heap_read(loc + 1), RW_EDGE_READ);
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (!rw_push(heap_read(loc + 1 + i), RW_EDGE_READ)) return 0;
      }
      return 1;
    }
    default:
      return 1;
  }
}

// 1 when some root reaches a UOP_ASSIGN, 0 when none does, -1 on OOM.
// One epoch covers every root: this only asks whether an ASSIGN exists.
static int rw_any_assign(Term const *roots, u32 n) {
  rw_epoch_next();
  for (u32 i = 0; i < n; i++) {
    RW_STACK_LEN = 0;
    if (!rw_push(roots[i], RW_EDGE_ROOT)) return -1;
    while (RW_STACK_LEN > 0) {
      Term t   = rw_resolve(RW_STACK[--RW_STACK_LEN].t);
      u8   tag = term_tag(t);
      if (!rw_is_node(tag)) continue;
      if (tag == TAG_UOP && term_ext(t) == UOP_KERNEL) continue;
      int first = rw_first_visit(term_val(t), RW_EDGE_READ);
      if (first < 0) {
        return -1;
      }
      if (first == 0) {
        continue;
      }
      if (tag == TAG_UOP && term_ext(t) == UOP_ASSIGN) return 1;
      if (!rw_push_children(t, RW_EDGE_READ)) return -1;
    }
  }
  return 0;
}

static RwRootLoc *rw_rootloc_slot(u64 loc) {
  u32 h = (u32)rw_hash(loc) & (RW_ROOTLOC_CAP - 1);
  while (RW_ROOTLOC[h].used && RW_ROOTLOC[h].loc != loc) {
    h = (h + 1) & (RW_ROOTLOC_CAP - 1);
  }
  return &RW_ROOTLOC[h];
}

// Size the per-call tables for n roots; 0 on OOM.
static int rw_tables_for(u32 n) {
  RW_W        = (n + 63) / 64;
  RW_POOL_LEN = 0;
  if (n > RW_ROOTS_CAP) {
    RwSet *succ = (RwSet *)realloc(RW_WAR_SUCC, (size_t)n * sizeof(RwSet));
    if (succ == NULL) return 0;
    RW_WAR_SUCC = succ;
    RwSet *pred = (RwSet *)realloc(RW_RAW_PRED, (size_t)n * sizeof(RwSet));
    if (pred == NULL) return 0;
    RW_RAW_PRED  = pred;
    RW_ROOTS_CAP = n;
  }
  u32 cap = 512;
  while (cap < 2 * n) cap *= 2;
  if (cap > RW_ROOTLOC_CAP) {
    RwRootLoc *rl = (RwRootLoc *)realloc(RW_ROOTLOC, (size_t)cap * sizeof(RwRootLoc));
    if (rl == NULL) return 0;
    RW_ROOTLOC     = rl;
    RW_ROOTLOC_CAP = cap;
  }
  memset(RW_ROOTLOC, 0, (size_t)RW_ROOTLOC_CAP * sizeof(RwRootLoc));
  for (u32 i = 0; i < n; i++) {
    if (!rw_set_new(&RW_WAR_SUCC[i]) || !rw_set_new(&RW_RAW_PRED[i])) return 0;
  }
  return 1;
}

// Record the reads and ASSIGNs reached by root `r`.  0 on OOM.
static int rw_walk_root(Term root, u32 r) {
  rw_epoch_next();
  RW_STACK_LEN = 0;
  if (!rw_push(root, RW_EDGE_ROOT)) return 0;
  while (RW_STACK_LEN > 0) {
    RwItem it   = RW_STACK[--RW_STACK_LEN];
    Term   t    = rw_resolve(it.t);
    u32    edge = it.edge;
    u8     tag  = term_tag(t);
    if (tag == TAG_TEN) {
      if (edge == RW_EDGE_READ && !rw_read_ten((u32)term_val(t), r)) return 0;
      continue;
    }
    if (!rw_is_node(tag)) continue;
    u64 loc = term_val(t);
    if (tag == TAG_UOP && term_ext(t) == UOP_KERNEL) {
      Term out = heap_read(loc + 0);
      if (edge == RW_EDGE_READ && term_tag(out) == TAG_TEN) {
        if (!rw_read_ten((u32)term_val(out), r)) return 0;
      }
      if (!rw_push_kernel((u32)term_val(heap_read(loc + 1)))) return 0;
      continue;
    }
    int first = rw_first_visit(loc, edge);
    if (first < 0) {
      return 0;
    }
    if (first == 0) {
      continue;
    }
    if (edge != RW_EDGE_ROOT) {
      RwRootLoc *rl = rw_rootloc_slot(loc);
      if (rl->used) {
        rw_set_or(RW_RAW_PRED[r], rl->roots);
      }
    }
    if (tag == TAG_UOP && term_ext(t) == UOP_ASSIGN && !rw_note_assign(loc, edge, r)) return 0;
    if (!rw_push_children(t, edge)) return 0;
  }
  return 1;
}

// Order roots[0..n) for realize_many.  Returns the number of waves (>= 1)
// and fills order[] with root indices in execution order and wave_end[w]
// with the end of wave w in order[]; returns 0 when the roots' WAR edges
// form a cycle.  A bundle with no ASSIGN, or one the analysis cannot
// finish (OOM), is a single wave in the given order; *has_assign reports
// whether the analysis ran.
fn u32 realize_waves(Term const *roots, u32 n, u32 *order, u32 *wave_end, u8 *has_assign) {
  *has_assign = 0;
  for (u32 i = 0; i < n; i++) order[i] = i;
  wave_end[0] = n;
  if (n < 2) return 1;
  if (rw_any_assign(roots, n) != 1) return 1;
  *has_assign = 1;

  RW_STATES_LEN = 0;
  if (RW_INDEX != NULL) memset(RW_INDEX, 0, RW_INDEX_CAP * sizeof(u32));
  if (!rw_tables_for(n)) return 1;
  for (u32 i = 0; i < n; i++) {
    Term t = rw_resolve(roots[i]);
    if (!rw_is_node(term_tag(t))) continue;
    RwRootLoc *rl = rw_rootloc_slot(term_val(t));
    if (!rl->used) {
      if (!rw_set_new(&rl->roots)) return 1;
      rl->used = 1;
      rl->loc  = term_val(t);
    }
    rw_set_add(rl->roots, i);
  }
  for (u32 r = 0; r < n; r++) {
    if (!rw_walk_root(roots[r], r)) return 1;
  }

  for (u32 s = 0; s < RW_STATES_LEN; s++) {
    RwState const *a = &RW_STATES[s];
    if (a->sup == 0) continue;
    RwSet rd = RW_STATES[a->sup - 1].readers;
    for (u32 w = 0; w < RW_W; w++) {
      u64 src = RW_POOL[rd + w] & ~RW_POOL[a->firers + w];
      while (src != 0) {
        u32 j = (w << 6) + (u32)__builtin_ctzll(src);
        src &= src - 1;
        rw_set_or(RW_WAR_SUCC[j], a->firers);
      }
    }
  }

  // Kahn over the WAR edges, CTR-ordered queue; a WAR edge j -> i puts i
  // at least one wave after j.
  u32 *indeg = (u32 *)calloc(3 * (size_t)n, sizeof(u32));
  if (indeg == NULL) return 1;
  u32 *level = indeg + n;
  u32 *queue = indeg + 2 * (size_t)n;
  for (u32 j = 0; j < n; j++) {
    for (u32 i = 0; i < n; i++) {
      if (rw_set_has(RW_WAR_SUCC[j], i)) indeg[i]++;
    }
  }
  u32 head = 0, tail = 0;
  for (u32 i = 0; i < n; i++) {
    if (indeg[i] == 0) queue[tail++] = i;
  }
  u32 n_waves = 1;
  while (head < tail) {
    u32 j = queue[head++];
    for (u32 i = 0; i < n; i++) {
      if (!rw_set_has(RW_WAR_SUCC[j], i)) continue;
      if (level[i] < level[j] + 1) level[i] = level[j] + 1;
      if (level[i] + 1 > n_waves) n_waves = level[i] + 1;
      if (--indeg[i] == 0) queue[tail++] = i;
    }
  }
  if (tail < n || n_waves == 1) {
    free(indeg);
    return tail < n ? 0 : 1;
  }

  // Inside a wave, a root runs after the roots whose term it reaches;
  // ties keep CTR order.
  RwSet pending;
  if (!rw_set_new(&pending)) {
    free(indeg);
    return 1;
  }
  u32 pos = 0;
  for (u32 w = 0; w < n_waves; w++) {
    for (u32 i = 0; i < n; i++) {
      if (level[i] == w) rw_set_add(pending, i);
    }
    for (;;) {
      u32 pick = n, first = n;
      for (u32 i = 0; i < n && pick == n; i++) {
        if (!rw_set_has(pending, i)) continue;
        if (first == n) first = i;
        u64 waiting = 0;
        for (u32 x = 0; x < RW_W; x++) {
          u64 preds = RW_POOL[RW_RAW_PRED[i] + x];
          if (x == (i >> 6)) preds &= ~(1ULL << (i & 63));
          waiting |= preds & RW_POOL[pending + x];
        }
        if (waiting == 0) pick = i;
      }
      if (first == n) break;
      if (pick == n) pick = first;
      rw_set_del(pending, pick);
      order[pos++] = pick;
    }
    wave_end[w] = pos;
  }
  free(indeg);
  return n_waves;
}
