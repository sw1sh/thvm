// wnf/redex.c - redexes as packed Terms (tag = the eliminator, val = its slot)
// for nf and the WL stepper.  is_redex tests one; redex_fire fires it through
// its interact_* (0 when it is none) and patches every heap cell holding it to
// the result; redex_enumerate collects them under roots and over the heap.  A
// step session indexes the heap so that patching costs O(uses).

fn u8 is_redex(Term t) {
  HOT_IS_REDEX_CALLS++;
  u8  tag = term_tag(t);
  u64 val = term_val(t);
  switch (tag) {
    case TAG_APP: {
      Term fun = term_resolve(heap_read(val));
      u8 ft = term_tag(fun);
      return ft == TAG_LAM || ft == TAG_ERA || ft == TAG_MAT;
    }
    case TAG_DP0:
    case TAG_DP1: {
      // Grad projections are always eligible; a DP1 whose y matches no rule
      // fires to 0.
      if (term_ext(t) & DUP_GRAD_FLAG) return 1;
      Term cell = heap_read(val);
      // A SUB cell is a sibling's finished fire, not an active pair.
      if (term_sub_get(cell)) return 0;
      Term body = term_resolve(cell);
      u8 bt = term_tag(body);
      if (bt == TAG_SUP || bt == TAG_LAM || bt == TAG_ERA ||
          bt == TAG_NUM || bt == TAG_TEN || bt == TAG_CTR ||
          bt == TAG_ANY || bt == TAG_BRI) return 1;
      // DUP-NOD bodies, as in wnf/_.c; never APP (it would copy a beta redex).
      if (bt == TAG_OP2  || bt == TAG_MAT  || bt == TAG_EQL ||
          bt == TAG_AND  || bt == TAG_OR   || bt == TAG_WHEN ||
          bt == TAG_ANN  || bt == TAG_DSU  || bt == TAG_DDU  ||
          bt == TAG_INC) return 1;
      // DUP-UOP shares a passive UOP; a KERNEL or ASSIGN fires first.
      if (bt == TAG_UOP) {
        u8 op = term_ext(body);
        return op != UOP_KERNEL && op != UOP_ASSIGN;
      }
      return 0;
    }
    case TAG_REF: return 1;
    case TAG_ALO: return 1;
    case TAG_OP2: {
      // Acquire: pairs with heap_replace's release store of a patched operand.
      Term x = term_resolve(heap_read_acq(val));
      Term y = term_resolve(heap_read_acq(val + 1));
      return term_tag(x) == TAG_NUM && term_tag(y) == TAG_NUM;
    }
    case TAG_UOP: {
      u8 op = term_ext(t);
      if (op == UOP_KERNEL) return 1;
      if (op == UOP_ASSIGN) {
        Term dst = term_resolve(heap_read(val + 0));
        Term src = term_resolve(heap_read(val + 1));
        return term_tag(dst) == TAG_TEN && term_tag(src) == TAG_TEN;
      }
      // UOP-SUP: an elementwise UOP over a SUP operand (see redex_fire).
      if (uop_is_binary_elementwise(op)) {
        Term a = term_resolve(heap_read(val + 0));
        Term b = term_resolve(heap_read(val + 1));
        if (term_tag(a) == TAG_SUP || term_tag(b) == TAG_SUP) return 1;
      }
      if (uop_is_unary_elementwise(op)) {
        Term a = term_resolve(heap_read(val + 0));
        if (term_tag(a) == TAG_SUP) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// Cells a compound term owns, for the DFS and the parent map; 0 for an atom
// or a tag not listed here.
static u32 term_arity(Term t) {
  u8 tag = term_tag(t);
  switch (tag) {
    case TAG_LAM: return 1;
    case TAG_APP: return 2;
    case TAG_SUP: return 2;
    case TAG_DUP: return 1;
    case TAG_OP2: return 2;
    case TAG_MAT: return 2;
    case TAG_ALO: return 2;
    case TAG_DP0: case TAG_DP1: return 1;
    case TAG_UOP: {
      u8 op = term_ext(t);
      if (op == UOP_KERNEL) return 2;
      if (op == UOP_ASSIGN) return 2;
      return uop_arity(op);
    }
    default: return 0;
  }
}

#define STEP_NIL ((u64)-1)

// nf's pool while attached (redex_worklist_attach_pool): step_fresh_push then
// pushes to a worker's bag instead of the fresh list.
static struct WnfPool *NF_POOL     = 0;
static u32             NF_POOL_TID = 0;

// Step session tables.  A Term's use chain starts at STEP_USE_HEAD (keyed by
// STEP_USE_KEY) and runs through STEP_USE_NEXT[loc]; STEP_PARENT[loc] is the
// Term owning slot loc (0: none); STEP_PRE holds the redexes the caller saw.
static u8     STEP_ACTIVE        = 0;
static Term  *STEP_USE_HEAD      = 0;   // chain heads, STEP_NIL = empty
static Term  *STEP_USE_KEY       = 0;   // bucket keys (Term values)
static u32    STEP_USE_HEAD_CAP  = 0;   // power of two
static u64   *STEP_USE_NEXT      = 0;
static Term  *STEP_PARENT        = 0;
static u64    STEP_SIDE_CAP      = 0;
static Term  *STEP_PRE           = 0;
static u32    STEP_PRE_CAP       = 0;
static Term  *STEP_FRESH_BUF     = 0;
static u32    STEP_FRESH_N       = 0;
static u32    STEP_FRESH_CAP     = 0;
static Term  *STEP_FRESH_SEEN    = 0;   // dedup hash for pushes this session

static inline u32 step_hash(Term t, u32 cap_mask) {
  u64 h = (u64)t;
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return (u32)h & cap_mask;
}

// Slot of Term t in the use-head table, inserted if new; STEP_USE_HEAD_FULL
// when full.  Keys are claimed by CAS and never removed; HEADs are NIL from
// attach, and a CAS winner leaves its HEAD alone (a write could wipe a push).
#define STEP_USE_HEAD_FULL ((u32)-1)
static u32 step_use_head_slot(Term t) {
  u32 mask = STEP_USE_HEAD_CAP - 1;
  u32 h = step_hash(t, mask);
  for (u32 probe = 0; probe < STEP_USE_HEAD_CAP; probe++) {
    u32 i = (h + probe) & mask;
    Term cur = (Term)__atomic_load_n((u64 *)&STEP_USE_KEY[i],
                                     __ATOMIC_ACQUIRE);
    if (cur == t) return i;
    if (cur == 0) {
      Term expected = 0;
      if (__atomic_compare_exchange_n((u64 *)&STEP_USE_KEY[i],
                                      (u64 *)&expected, (u64)t,
                                      0,
                                      __ATOMIC_ACQ_REL,
                                      __ATOMIC_ACQUIRE)) {
        return i;
      }
      // Lost the race: the slot is ours only if the winner inserted t.
      if (expected == t) return i;
    }
  }
  return STEP_USE_HEAD_FULL;
}

// Treiber-stack push of loc onto the chain at STEP_USE_HEAD[slot].
static inline void step_use_head_push_atomic(u32 slot, u64 loc) {
  u64 prev;
  do {
    prev = __atomic_load_n((u64 *)&STEP_USE_HEAD[slot], __ATOMIC_ACQUIRE);
    STEP_USE_NEXT[loc] = prev;
  } while (!__atomic_compare_exchange_n(
      (u64 *)&STEP_USE_HEAD[slot], &prev, (u64)loc,
      0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
}

static u8 step_pre_contains(Term t) {
  if (STEP_PRE_CAP == 0) return 0;
  u32 mask = STEP_PRE_CAP - 1;
  u32 h = step_hash(t, mask);
  for (u32 probe = 0; probe < STEP_PRE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    if (STEP_PRE[i] == 0) return 0;
    if (STEP_PRE[i] == t) return 1;
  }
  return 0;
}

static void step_pre_insert(Term t) {
  if (STEP_PRE_CAP == 0) return;
  u32 mask = STEP_PRE_CAP - 1;
  u32 h = step_hash(t, mask);
  for (u32 probe = 0; probe < STEP_PRE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    if (STEP_PRE[i] == 0) { STEP_PRE[i] = t; return; }
    if (STEP_PRE[i] == t) return;
  }
}

// Queue a redex on the fresh list once, unless the caller saw it (STEP_PRE).
static void step_fresh_push(Term t) {
  if (!STEP_ACTIVE || t == 0) return;
  if (!is_redex(t)) return;
  // Under nf, push to the calling worker's own bag (a WsDeque has one
  // producer); with no worker bound, as in a T=1 drain, to NF_POOL_TID's.
  if (NF_POOL != 0) {
    u32 tid = CURRENT_WORKER ? CURRENT_WORKER->id : NF_POOL_TID;
    wnf_pool_push(NF_POOL, tid, t);
    return;
  }
  if (step_pre_contains(t)) return;
  u32 mask = STEP_FRESH_CAP - 1;
  u32 h = step_hash(t, mask);
  for (u32 probe = 0; probe < STEP_FRESH_CAP; probe++) {
    u32 i = (h + probe) & mask;
    if (STEP_FRESH_SEEN[i] == 0) {
      STEP_FRESH_SEEN[i] = t;
      if (STEP_FRESH_N < STEP_FRESH_CAP) STEP_FRESH_BUF[STEP_FRESH_N++] = t;
      return;
    }
    if (STEP_FRESH_SEEN[i] == t) return;
  }
}

// Make STEP_PARENT / STEP_USE_NEXT valid below new_cap.
static void step_side_grow(u64 new_cap) {
  if (new_cap <= STEP_SIDE_CAP) return;
  u64 cap = STEP_SIDE_CAP ? STEP_SIDE_CAP * 2 : 4096;
  while (cap < new_cap) cap *= 2;
  STEP_USE_NEXT = (u64 *) realloc(STEP_USE_NEXT, cap * sizeof(u64));
  STEP_PARENT   = (Term *)realloc(STEP_PARENT,   cap * sizeof(Term));
  for (u64 i = STEP_SIDE_CAP; i < cap; i++) {
    STEP_USE_NEXT[i] = STEP_NIL;
    STEP_PARENT[i]   = 0;
  }
  STEP_SIDE_CAP = cap;
}

// Put loc on Term t's use chain.  A loc has one chain node
// (STEP_USE_NEXT[loc]), so it can be on one chain at a time.
static void step_use_insert(u64 loc, Term t) {
  if (!STEP_ACTIVE || t == 0) return;
  if (loc >= STEP_SIDE_CAP) step_side_grow(loc + 1);
  u32 i = step_use_head_slot(t);
  if (i == STEP_USE_HEAD_FULL) return;   // full: heap_replace scans for t
  step_use_head_push_atomic(i, loc);
}

static inline void step_parent_set(u64 loc, Term parent) {
  if (!STEP_ACTIVE) return;
  if (loc >= STEP_SIDE_CAP) step_side_grow(loc + 1);
  STEP_PARENT[loc] = parent;
}

// Whether cell `cur` is a UOP_ASSIGN's src slot (loc+1).  heap_replace keeps a
// kernel there: a training loop re-fires it each pass, and a baked TEN would
// stop training after one step.  Only a session checks it (STEP_PARENT).
static int cell_is_assign_src(u64 cur) {
  if (cur >= STEP_SIDE_CAP) return 0;
  Term parent = STEP_PARENT[cur];
  return parent != 0 && term_tag(parent) == TAG_UOP
      && term_ext(parent) == UOP_ASSIGN && term_val(parent) + 1 == cur;
}

// Patch every heap cell holding the fired redex `old` to `new_`.  In a step
// session old's use chain finds them (else a heap scan) and each patched cell's
// parent is pushed for promotion; without a session, a plain heap scan.
static void heap_replace(Term old, Term new_) {
  if (old == new_) return;
  HOT_HEAP_REPLACE_CALLS++;
  int old_is_kernel = (term_tag(old) == TAG_UOP && term_ext(old) == UOP_KERNEL);
  if (STEP_ACTIVE) {
    u32 mask = STEP_USE_HEAD_CAP - 1;
    u32 h = step_hash(old, mask);
    u32 slot = (u32)-1;
    for (u32 probe = 0; probe < STEP_USE_HEAD_CAP; probe++) {
      u32 i = (h + probe) & mask;
      if (STEP_USE_KEY[i] == 0) break;
      if (STEP_USE_KEY[i] == old) { slot = i; break; }
    }
    if (slot != (u32)-1) {
      // Claim old's chain by swapping its head to NIL, then move each
      // patched cell onto new_'s chain.
      u64 cur = (u64)__atomic_exchange_n(
          (u64 *)&STEP_USE_HEAD[slot], (u64)STEP_NIL,
          __ATOMIC_ACQ_REL);
      u32 new_slot = step_use_head_slot(new_);
      while (cur != STEP_NIL) {
        u64 next = STEP_USE_NEXT[cur];
        if (heap_read(cur) == old && !(old_is_kernel && cell_is_assign_src(cur))) {
          // Release: pairs with is_redex's acquire load.
          heap_set_rel(cur, new_);
          if (new_slot != STEP_USE_HEAD_FULL) {
            step_use_head_push_atomic(new_slot, cur);
          }
          if (cur < STEP_SIDE_CAP) {
            Term parent = STEP_PARENT[cur];
            if (parent != 0) step_fresh_push(parent);
          }
        }
        // else: stale chain entry from a prior swap; skip.
        HOT_HEAP_REPLACE_CELLS++;
        cur = next;
      }
      return;
    }
    // old is not indexed (no cell holds it, or the table was full at insert):
    // scan the heap, still pushing the patched cells' parents.
    HOT_HEAP_REPLACE_CELLS += HEAP_NEXT;
    for (u64 i = 0; i < HEAP_NEXT; i++) {
      if (heap_read(i) == old && !(old_is_kernel && cell_is_assign_src(i))) {
        heap_set(i, new_);
        if (i < STEP_SIDE_CAP) {
          Term parent = STEP_PARENT[i];
          if (parent != 0) step_fresh_push(parent);
        }
      }
    }
    return;
  }
  HOT_HEAP_REPLACE_CELLS += HEAP_NEXT;
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    if (heap_read(i) == old) heap_set(i, new_);
  }
}

// nf's pool and seed tid, for step_fresh_push; without them a session keeps
// its own fresh list.
fn void redex_worklist_attach_pool(WnfPool *p, u32 tid) {
  NF_POOL         = p;
  NF_POOL_TID     = tid;
}

fn void redex_worklist_detach_pool(void) {
  NF_POOL = 0;
}

fn Term redex_fire(Term redex) {
  if (!is_redex(redex)) return 0;
  u8  tag = term_tag(redex);
  u64 val = term_val(redex);
  Term result = redex;
  u64 hb_at_fire = HEAP_NEXT;

  switch (tag) {
    case TAG_APP: {
      Term fun = term_resolve(heap_read(val));
      Term arg = heap_read(val + 1);
      switch (term_tag(fun)) {
        case TAG_LAM: result = interact_app_lam(fun, arg); break;
        case TAG_ERA: result = interact_app_era();         break;
        case TAG_PRI: result = interact_app_pri(fun, arg); break;
        case TAG_SUP: result = interact_app_sup(fun, arg); break;
        case TAG_MAT: {
          // Forced as wnf/_.c's APP-MAT forces it.
          Term arg_w = wnf(arg);
          if (term_tag(arg_w) == TAG_DP0 || term_tag(arg_w) == TAG_DP1) {
            arg_w = cnf(arg_w);
          }
          result = interact_app_mat(fun, arg_w);
          break;
        }
        default: return 0;  // validation race: not a redex anymore
      }
      break;
    }
    case TAG_DP0:
    case TAG_DP1: {
      u64  loc  = val;
      // A grad projection leaves the cell alone: the sibling still reads y.
      if (term_ext(redex) & DUP_GRAD_FLAG) {
        if (tag == TAG_DP0) {
          ITRS++;
          result = heap_read(loc);
          break;
        }
        Term g = interact_grad(redex);
        if (g == redex) return 0;   // stuck (y can't pattern-match yet)
        ITRS++;
        result = g;
        break;
      }
      // Take the body atomically: of racing workers only one reads it non-zero.
      Term cell = heap_take_atomic(loc);
      if (cell == 0) return 0;       // claimed by another worker
      Term body = term_resolve(cell);
      u8 side = (tag == TAG_DP0) ? 0 : 1;
      u32 lab = term_ext(redex);
      switch (term_tag(body)) {
        case TAG_SUP: result = interact_dup_sup(lab, loc, side, body); break;
        case TAG_ERA: result = interact_dup_era(side, loc, body);      break;
        case TAG_LAM: result = interact_dup_lam(lab, loc, side, body); break;
        case TAG_NUM: result = interact_dup_num(side, loc, body);      break;
        case TAG_TEN: result = interact_dup_ten(side, loc, body);      break;
        case TAG_CTR: result = interact_dup_ctr(lab, loc, side, body); break;
        case TAG_BRI: result = interact_dup_bri(lab, loc, side, body); break;
        case TAG_ANY: result = interact_dup_any(side, loc, body);      break;
        case TAG_UOP: {
          Term r = interact_dup_uop(lab, loc, side, body);
          if (r == 0) {
            heap_set(loc, cell);   // active UOP: restore, stay stuck
            return 0;
          }
          result = r;
          break;
        }
        // DUP-NOD, as in wnf/_.c (never APP).
        case TAG_OP2: case TAG_MAT: case TAG_EQL:
        case TAG_AND: case TAG_OR:  case TAG_WHEN:
        case TAG_ANN: case TAG_DSU: case TAG_DDU:
        case TAG_INC:
          result = interact_dup_nod(lab, loc, side, body);
          break;
        default:
          heap_set(loc, cell);  // restore: not a redex any more
          return 0;
      }
      break;
    }
    case TAG_REF: {
      u32  name = term_ext(redex);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0) return 0;
      // No ITRS: an unfold is no interaction (wnf/_.c's TAG_REF).
      result = alo_realize(book, 0);
      break;
    }
    case TAG_ALO: {
      // No ITRS, as for REF.
      result = alo_force(redex);
      break;
    }
    case TAG_OP2: {
      Term x = term_resolve(heap_read(val + 0));
      Term y = term_resolve(heap_read(val + 1));
      if (term_tag(x) != TAG_NUM || term_tag(y) != TAG_NUM) return 0;
      result = interact_op2_num_num(term_ext(redex), term_ext(x), (u32)term_val(x),
                                    (u32)term_val(y), val);
      break;
    }
    case TAG_UOP: {
      u8 op = term_ext(redex);
      if (op == UOP_KERNEL) {
        result = interact_kernel(redex);   // ITRS++ inside
      } else if (op == UOP_ASSIGN) {
        result = interact_assign(redex);   // ITRS++ inside
        if (result == redex) return 0;     // stuck: not a redex yet
      } else if (uop_is_binary_elementwise(op)) {
        // UOP_op(&L{a0, a1}, c)
        // ---------------------- UOP-SUP (as OP2-SUP; mirrored for a right SUP)
        // ! &L{c0, c1} = c
        // &L{UOP_op(a0, c0), UOP_op(a1, c1)}
        // The DUP matters: a same-label SUP c annihilates against it, where
        // sharing c would nest SUP^L inside SUP^L (ADD(x, x) of a SUP x).
        Term a = term_resolve(heap_read(val + 0));
        Term b = term_resolve(heap_read(val + 1));
        if (term_tag(a) == TAG_SUP) {
          u32  lab  = term_ext(a);
          u64  sloc = term_val(a);
          Term a0   = heap_read(sloc + 0);
          Term a1   = heap_read(sloc + 1);
          u64  dup  = heap_alloc(1);
          heap_set(dup, b);
          Term c0   = term_new(0, TAG_DP0, lab, dup);
          Term c1   = term_new(0, TAG_DP1, lab, dup);
          Term op0  = uop_binary(op, a0, c0);
          Term op1  = uop_binary(op, a1, c1);
          u64  ns   = heap_alloc(2);
          heap_set(ns + 0, op0);
          heap_set(ns + 1, op1);
          ITRS++;
          result = term_new(0, TAG_SUP, lab, ns);
        } else if (term_tag(b) == TAG_SUP) {
          u32  lab  = term_ext(b);
          u64  sloc = term_val(b);
          Term b0   = heap_read(sloc + 0);
          Term b1   = heap_read(sloc + 1);
          u64  dup  = heap_alloc(1);
          heap_set(dup, a);
          Term c0   = term_new(0, TAG_DP0, lab, dup);
          Term c1   = term_new(0, TAG_DP1, lab, dup);
          Term op0  = uop_binary(op, c0, b0);
          Term op1  = uop_binary(op, c1, b1);
          u64  ns   = heap_alloc(2);
          heap_set(ns + 0, op0);
          heap_set(ns + 1, op1);
          ITRS++;
          result = term_new(0, TAG_SUP, lab, ns);
        } else {
          return 0;
        }
      } else if (uop_is_unary_elementwise(op)) {
        // UOP-SUP (unary):  UOP_op(SUP^L(a, b)) -> SUP^L(UOP_op(a), UOP_op(b))
        Term a = term_resolve(heap_read(val + 0));
        if (term_tag(a) == TAG_SUP) {
          u32 lab  = term_ext(a);
          u64 sloc = term_val(a);
          Term a0  = heap_read(sloc + 0);
          Term a1  = heap_read(sloc + 1);
          Term op0 = uop_unary(op, a0);
          Term op1 = uop_unary(op, a1);
          u64 ns   = heap_alloc(2);
          heap_set(ns + 0, op0);
          heap_set(ns + 1, op1);
          ITRS++;
          result = term_new(0, TAG_SUP, lab, ns);
        } else {
          return 0;
        }
      } else {
        return 0;
      }
      break;
    }
    default: return 0;
  }

  // Patch the parents, except an ASSIGN's src kernel (cell_is_assign_src).
  heap_replace(redex, result);
  // Session: index the cells this fire allocated, record the result and those
  // cells as parents of their slots, and push the redexes among them.
  if (STEP_ACTIVE) {
    if (result != 0) step_fresh_push(result);
    for (u64 i = hb_at_fire; i < HEAP_NEXT; i++) {
      step_use_insert(i, heap_read(i));
    }
    if (result != 0) {
      u32 ar = term_arity(result);
      if (ar > 0) {
        u64 base = term_val(result);
        for (u32 k = 0; k < ar; k++) step_parent_set(base + k, result);
      }
    }
    for (u64 i = hb_at_fire; i < HEAP_NEXT; i++) {
      Term t = heap_read(i);
      u32 ar = term_arity(t);
      if (ar == 0) continue;
      u64 base = term_val(t);
      for (u32 k = 0; k < ar; k++) step_parent_set(base + k, t);
    }
    for (u64 i = hb_at_fire; i < HEAP_NEXT; i++) {
      step_fresh_push(heap_read(i));
    }
  }
  return result;
}

// redex_enumerate's visited set: open addressing on the packed Term, sized at
// each call to 4x the Terms the walk can meet (load <= 0.25).
static Term *REDEX_DEDUP     = NULL;
static u32   REDEX_DEDUP_CAP = 0;

static inline u32 redex_dedup_hash(Term t) {
  u64 h = (u64)t;
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return (u32)h & (REDEX_DEDUP_CAP - 1);
}

static void redex_dedup_resize(u64 want) {
  u32 cap = 32768;
  while ((u64)cap < want * 4) {
    cap <<= 1;
    if (cap == 0) { cap = (u32)1 << 28; break; }   // u32 overflow
  }
  if (cap != REDEX_DEDUP_CAP) {
    REDEX_DEDUP = (Term *)realloc(REDEX_DEDUP, (size_t)cap * sizeof(Term));
    REDEX_DEDUP_CAP = cap;
  }
  // Each enumerate starts from an empty set.
  memset(REDEX_DEDUP, 0, (size_t)REDEX_DEDUP_CAP * sizeof(Term));
}

// 1 when t is new (and now inserted), 0 when already seen.
static inline u8 redex_dedup_insert(Term t) {
  u32 h = redex_dedup_hash(t);
  for (u32 probe = 0; probe < REDEX_DEDUP_CAP; probe++) {
    u32 i = (h + probe) & (REDEX_DEDUP_CAP - 1);
    if (REDEX_DEDUP[i] == t) return 0;
    if (REDEX_DEDUP[i] == 0) {
      REDEX_DEDUP[i] = t;
      return 1;
    }
  }
  return 1;   // full: never, at 4x the Terms a walk can meet
}

static void redex_collect_one(Term t, Term *out, u32 cap, u32 *count) {
  if (!is_redex(t)) return;
  if (!redex_dedup_insert(t)) return;
  if (*count < cap) out[*count] = t;
  (*count)++;
}

// The redexes under the roots, then on the whole heap, deduped by Term, into
// out[0..cap); returns the full count, which may exceed cap.
fn u32 redex_enumerate(Term *roots, u32 n_roots, Term *out, u32 cap) {
  HOT_REDEX_ENUM_CALLS++;
  HOT_REDEX_ENUM_CELLS += HEAP_NEXT;
  redex_dedup_resize((u64)HEAP_NEXT + (u64)n_roots);

  u32 count = 0;

  // DFS from the roots, which may live off the heap (a Term the WL side holds).
  static Term *DFS_STACK = NULL;
  static u32   DFS_STACK_CAP = 0;
  if (DFS_STACK_CAP == 0) {
    DFS_STACK_CAP = 4096;
    DFS_STACK = (Term *)malloc(DFS_STACK_CAP * sizeof(Term));
  }
  u32 s_pos = 0;
  for (u32 i = 0; i < n_roots; i++) {
    if (s_pos >= DFS_STACK_CAP) {
      DFS_STACK_CAP *= 2;
      DFS_STACK = (Term *)realloc(DFS_STACK, DFS_STACK_CAP * sizeof(Term));
    }
    DFS_STACK[s_pos++] = roots[i];
  }
  while (s_pos > 0) {
    Term t = DFS_STACK[--s_pos];
    if (!redex_dedup_insert(t)) continue;
    if (is_redex(t)) {
      if (count < cap) out[count] = t;
      count++;
    }
    u32 ar = term_arity(t);
    if (ar == 0) continue;
    u64 base = term_val(t);
    for (u32 i = 0; i < ar; i++) {
      if (s_pos >= DFS_STACK_CAP) {
        DFS_STACK_CAP *= 2;
        DFS_STACK = (Term *)realloc(DFS_STACK, DFS_STACK_CAP * sizeof(Term));
      }
      DFS_STACK[s_pos++] = heap_read(base + i);
    }
  }

  // Then every heap cell, for redexes off the root tree: a grad cell whose
  // parent heap_replace patched away may still owe the outer chain its BWD.
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    redex_collect_one(heap_read(i), out, cap, &count);
  }
  return count;
}

// === Step session API ===

#define STEP_HEAD_CAP_DEFAULT (1u << 14)
#define STEP_PRE_CAP_DEFAULT  (1u << 14)
#define STEP_FRESH_CAP_DEFAULT (1u << 12)

// attach builds the use chains and parent map from the roots and heap, seeds
// STEP_PRE and returns the redex count; drain_fresh returns the redexes new
// since the last drain.  gc_collect moves locs, so none may run in a session.
fn u32 redex_step_attach(Term *roots, u32 n_roots) {
  if (STEP_ACTIVE) return 0;
  STEP_ACTIVE = 1;
  u64 base_cap = HEAP_NEXT > 4096 ? HEAP_NEXT : 4096;
  STEP_SIDE_CAP = base_cap;
  STEP_USE_NEXT = (u64 *) malloc(base_cap * sizeof(u64));
  STEP_PARENT   = (Term *)calloc(base_cap, sizeof(Term));
  for (u64 i = 0; i < base_cap; i++) STEP_USE_NEXT[i] = STEP_NIL;

  // Up to HEAP_NEXT distinct Terms: size the head table 4x (load <= 0.25).
  u32 desired = STEP_HEAD_CAP_DEFAULT;
  while ((u64)desired < (u64)HEAP_NEXT * 4 && desired < (1u << 28)) {
    desired <<= 1;
  }
  STEP_USE_HEAD_CAP = desired;
  STEP_USE_HEAD = (Term *)malloc(STEP_USE_HEAD_CAP * sizeof(Term));
  STEP_USE_KEY  = (Term *)calloc(STEP_USE_HEAD_CAP, sizeof(Term));
  for (u32 i = 0; i < STEP_USE_HEAD_CAP; i++) STEP_USE_HEAD[i] = STEP_NIL;

  STEP_PRE_CAP = STEP_PRE_CAP_DEFAULT;
  STEP_PRE = (Term *)calloc(STEP_PRE_CAP, sizeof(Term));

  STEP_FRESH_CAP   = STEP_FRESH_CAP_DEFAULT;
  STEP_FRESH_BUF   = (Term *)malloc(STEP_FRESH_CAP * sizeof(Term));
  STEP_FRESH_SEEN  = (Term *)calloc(STEP_FRESH_CAP, sizeof(Term));
  STEP_FRESH_N     = 0;

  for (u64 i = 0; i < HEAP_NEXT; i++) {
    Term t = heap_read(i);
    if (t == 0) continue;
    step_use_insert(i, t);
    u32 ar = term_arity(t);
    if (ar == 0) continue;
    u64 base = term_val(t);
    for (u32 k = 0; k < ar; k++) step_parent_set(base + k, t);
  }

  // An off-heap root also parents its slots, so a patch there promotes it.
  for (u32 r = 0; r < n_roots; r++) {
    Term t = roots[r];
    u32 ar = term_arity(t);
    if (ar == 0) continue;
    u64 base = term_val(t);
    for (u32 k = 0; k < ar; k++) {
      // A parent the heap walk recorded is kept.
      if (base + k < STEP_SIDE_CAP && STEP_PARENT[base + k] == 0) {
        step_parent_set(base + k, t);
      } else if (base + k >= STEP_SIDE_CAP) {
        step_parent_set(base + k, t);
      }
    }
  }

  // redex_enumerate returns the full count, past the buffer; redexes beyond it
  // miss STEP_PRE, so a later push can report one as fresh.
  enum { SEED_BUF_CAP = 8192 };
  static Term seed_buf[SEED_BUF_CAP];
  u32 n_total = redex_enumerate(roots, n_roots, seed_buf, SEED_BUF_CAP);
  u32 n_pre   = n_total < SEED_BUF_CAP ? n_total : SEED_BUF_CAP;
  for (u32 i = 0; i < n_pre; i++) step_pre_insert(seed_buf[i]);
  return n_total;
}

fn Term redex_step_fire(Term redex) {
  if (!STEP_ACTIVE) return redex_fire(redex);
  // The caller has seen what it fires, even a redex attach missed.
  step_pre_insert(redex);
  Term r = redex_fire(redex);
  return r;
}

fn u32 redex_step_drain_fresh(Term *out, u32 cap) {
  if (!STEP_ACTIVE) return 0;
  u32 n = STEP_FRESH_N;
  if (n > cap) n = cap;
  for (u32 i = 0; i < n; i++) out[i] = STEP_FRESH_BUF[i];
  // The whole fresh list, past cap too, now counts as seen.
  for (u32 i = 0; i < STEP_FRESH_N; i++) step_pre_insert(STEP_FRESH_BUF[i]);
  STEP_FRESH_N = 0;
  memset(STEP_FRESH_SEEN, 0, STEP_FRESH_CAP * sizeof(Term));
  return n;
}

fn void redex_step_detach(void) {
  if (!STEP_ACTIVE) return;
  STEP_ACTIVE = 0;
  free(STEP_USE_HEAD);   STEP_USE_HEAD = 0;
  free(STEP_USE_KEY);    STEP_USE_KEY  = 0;
  free(STEP_USE_NEXT);   STEP_USE_NEXT = 0;
  free(STEP_PARENT);     STEP_PARENT   = 0;
  free(STEP_PRE);        STEP_PRE      = 0;
  free(STEP_FRESH_BUF);  STEP_FRESH_BUF = 0;
  free(STEP_FRESH_SEEN); STEP_FRESH_SEEN = 0;
  STEP_USE_HEAD_CAP = 0;
  STEP_SIDE_CAP     = 0;
  STEP_PRE_CAP      = 0;
  STEP_FRESH_CAP    = 0;
  STEP_FRESH_N      = 0;
}
