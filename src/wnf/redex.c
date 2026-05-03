// wnf/redex.c -- redex enumeration + single-redex firing for the
// debugger / step interface.
//
// A "redex" is a Term value whose principal port currently faces an
// interaction partner.  Identification is by the packed Term value
// itself: the val field uniquely picks out the redex's data slot,
// the tag picks out the eliminator role.
//
// The core trio:
//   is_redex(t)       -- predicate; safe to call on any Term.
//   redex_fire(t)     -- dispatches the matching interact_*; returns
//                        the rewrite result (or 0 on validation
//                        failure -- the input wasn't a redex).  Also
//                        rewrites every heap cell still holding the
//                        old redex term to point at the result, so
//                        nested redexes get their parent slots
//                        patched without the caller's help.
//   redex_enumerate   -- scans the live heap for redex Terms,
//                        deduped by packed Term value.  Used both
//                        for TRedexes[t] reporting and for the
//                        pre/post diff that powers TInteract's
//                        "fresh" return.
//
// Cost is O(HEAP_NEXT) per scan -- linear over the live heap.  For
// the legacy direct-fire path this is fine.
//
// === step session (incremental WL stepper) =======================
// The WL stepper opens a `redex_step_*` session that holds a
// persistent inverse index (Term -> heap-locs) plus a per-loc
// parent map.  Inside a session:
//   * heap_replace patches via the inverse index in O(uses-of-old)
//     instead of O(HEAP_NEXT).
//   * Parent-promotion redexes (a parent slot was patched, the
//     parent term's redex status flipped) are surfaced via the
//     parent map without re-enumerating the heap.
//   * The "fresh" diff TInteract returns is computed locally as
//     the worklist contents minus the pre-set, dropping the two
//     full enumerates per step.
// Sessionless callers (nf, direct redex_fire) keep the linear scan.

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
      // Grad-flavored projections (DUP_GRAD_FLAG on ext): always
      // eligible.  DP0 = FWD passthrough; DP1 = chain-rule fire (may
      // bail back to non-redex if y can't pattern-match yet).
      if (term_ext(t) & DUP_GRAD_FLAG) return 1;
      Term cell = heap_read(val);
      // Skip if the dup slot already holds a SUB (a previous fire's
      // residue) -- that's a substitution shortcut, not a fresh
      // active pair.
      if (term_sub_get(cell)) return 0;
      Term body = term_resolve(cell);
      u8 bt = term_tag(body);
      if (bt == TAG_SUP || bt == TAG_LAM || bt == TAG_ERA ||
          bt == TAG_NUM || bt == TAG_TEN || bt == TAG_CTR) return 1;
      // DUP-UOP commute: a lazy-compute UOP (CONST/ADD/MUL/NEG/REDUCE/
      // EXPAND/RESHAPE/FLIP) is eligible.  Active UOPs (KERNEL/ASSIGN)
      // are NOT -- they need to fire their own interaction first.
      if (bt == TAG_UOP) {
        u8 op = term_ext(body);
        return op != UOP_KERNEL && op != UOP_ASSIGN;
      }
      return 0;
    }
    case TAG_REF: return 1;
    case TAG_ALO: return 1;
    case TAG_OP2: {
      Term x = term_resolve(heap_read(val));
      Term y = term_resolve(heap_read(val + 1));
      return term_tag(x) == TAG_NUM && term_tag(y) == TAG_NUM;
    }
    case TAG_UOP: {
      u8 op = term_ext(t);
      // KERNEL always reducible (the dispatcher is idempotent for
      // inspection -- with `fired` removed, every entry re-runs).
      // GRAD always *eligible* -- the fire is a no-op when y isn't
      // structurally pattern-matchable.
      // ASSIGN reducible only when both children resolve to TAG_TEN;
      // otherwise wnf walks src's producer first to materialize the
      // value being assigned.
      if (op == UOP_KERNEL) return 1;
      if (op == UOP_ASSIGN) {
        Term dst = term_resolve(heap_read(val + 0));
        Term src = term_resolve(heap_read(val + 1));
        return term_tag(dst) == TAG_TEN && term_tag(src) == TAG_TEN;
      }
      // UOP-SUP commutation: a binary or unary elementwise UOP whose
      // input contains a SUP commutes the SUP outward (HVM4-style).
      // Tensor sharing is by heap-loc identity, so the "other" operand
      // doesn't need a DUP -- both new UOPs reference its cell.
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

// How many heap cells does a compound term own?  Atoms and known-
// non-compound tags return 0 (no recursion).  Mirrors the layout
// arities used by alo/realize.c and book/from_dynamic.c, but
// includes UOP variants.
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

// Step session: persistent across calls so the WL stepper doesn't
// pay an O(HEAP_NEXT) scan on every TInteract.  The session owns
// three side tables, all indexed by loc 0..STEP_SIDE_CAP:
//
//   STEP_USE_NEXT[L]  = next loc holding the same Term as L, or
//                       STEP_NIL.  Buckets keyed by Term in
//                       STEP_USE_HEAD give the first loc.
//   STEP_PARENT[L]    = the Term whose val owns slot L (so we can
//                       check is_redex on it after heap_replace
//                       patches L).  0 if no parent recorded.
//   STEP_PRE[T]       = open-addressed hash set of "redexes the
//                       caller already saw" -- the pre-fire active
//                       set.  Drained-fresh = pushed - pre.
//
// All built once on attach by walking roots+heap; heap_replace
// (when a session is attached) does O(uses-of-old) work via the
// use-list and pushes parent terms whose redex-status flips.
//
// Sessionless callers (legacy nf, direct TInteract without
// step_attach) keep the linear scan -- zero overhead when off.

#define STEP_NIL ((u64)-1)

static u8     STEP_ACTIVE        = 0;
static Term  *STEP_USE_HEAD      = 0;   // hash-bucket head loc per Term, STEP_NIL = empty
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

// Lookup-or-insert in the use-head table.  Returns the slot index
// (caller reads STEP_USE_HEAD[i] for the current head, writes back).
static u32 step_use_head_slot(Term t) {
  u32 mask = STEP_USE_HEAD_CAP - 1;
  u32 h = step_hash(t, mask);
  for (u32 probe = 0; probe < STEP_USE_HEAD_CAP; probe++) {
    u32 i = (h + probe) & mask;
    if (STEP_USE_KEY[i] == 0) {
      STEP_USE_KEY[i]  = t;
      STEP_USE_HEAD[i] = STEP_NIL;
      return i;
    }
    if (STEP_USE_KEY[i] == t) return i;
  }
  return 0;   // table saturated -- caller falls back to linear scan
}

// Pre-set membership.
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

// Push to fresh worklist with dedup.  Filters out terms already in
// the pre-set so callers see only the symmetric diff.
static void step_fresh_push(Term t) {
  if (!STEP_ACTIVE || t == 0) return;
  if (!is_redex(t)) return;
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

// Grow the per-loc side arrays so STEP_PARENT[loc] / STEP_USE_NEXT[loc]
// are valid for `loc < new_cap`.
static void step_side_grow(u64 new_cap) {
  if (new_cap <= STEP_SIDE_CAP) return;
  // Round up to a power of two and double from current to amortise.
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

// Record that cell at loc holds Term t.  Inserts into the use-list
// at the head (LIFO).  Idempotent within a session: callers that
// re-insert a (loc, t) pair link an extra node, which is harmless --
// heap_replace will overwrite the cell on the first match and the
// stale chain entry becomes a no-op.
static void step_use_insert(u64 loc, Term t) {
  if (!STEP_ACTIVE || t == 0) return;
  if (loc >= STEP_SIDE_CAP) step_side_grow(loc + 1);
  u32 i = step_use_head_slot(t);
  STEP_USE_NEXT[loc] = STEP_USE_HEAD[i];
  STEP_USE_HEAD[i]   = (Term)loc;
}

static inline void step_parent_set(u64 loc, Term parent) {
  if (!STEP_ACTIVE) return;
  if (loc >= STEP_SIDE_CAP) step_side_grow(loc + 1);
  STEP_PARENT[loc] = parent;
}

// Patch every heap cell that still holds `old` to hold `new`.  The
// IC interactions logically substitute the redex's incoming wire
// with the result; in heap terms that means any parent slot whose
// content equals the redex term gets replaced by the result.
//
// Two paths:
//   * Step session attached -> O(uses-of-old) via the use-list.
//     Each patched cell's parent term is checked for redex
//     promotion and pushed to the fresh worklist.
//   * No session -> linear scan over HEAP_NEXT (legacy semantics
//     the inspector + nf rely on for parent-slot patching when
//     no inverse index exists).
static void heap_replace(Term old, Term new_) {
  if (old == new_) return;
  HOT_HEAP_REPLACE_CALLS++;
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
      u64 cur = STEP_USE_HEAD[slot];
      // Walk + patch + chain transfer onto `new_`'s bucket.
      u32 new_slot = step_use_head_slot(new_);
      while (cur != STEP_NIL) {
        u64 next = STEP_USE_NEXT[cur];
        if (heap_read(cur) == old) {
          heap_set(cur, new_);
          // Transfer this loc onto new_'s use-list so future
          // heap_replace(new_, ...) calls find it.
          STEP_USE_NEXT[cur] = STEP_USE_HEAD[new_slot];
          STEP_USE_HEAD[new_slot] = (Term)cur;
          // Parent-promotion: if the owning term is now a redex,
          // surface it.
          if (cur < STEP_SIDE_CAP) {
            Term parent = STEP_PARENT[cur];
            if (parent != 0) step_fresh_push(parent);
          }
        }
        // else: stale chain entry from a prior swap; skip.
        HOT_HEAP_REPLACE_CELLS++;
        cur = next;
      }
      STEP_USE_HEAD[slot] = STEP_NIL;
    }
    return;
  }
  HOT_HEAP_REPLACE_CELLS += HEAP_NEXT;
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    if (heap_read(i) == old) heap_set(i, new_);
  }
}

// Incremental nf worklist.  `nf` attaches a buffer + counter pointer
// before its main loop; redex_fire pushes locally-fresh redexes
// (result + cells the interaction allocated) directly so nf doesn't
// need to re-enumerate the global heap after every drain.  When no
// worklist is attached (e.g. WL `TInteract` direct fire), pushes
// are silently dropped.
static Term *NF_WORK_PTR    = 0;
static u32  *NF_WORK_N_PTR  = 0;
static u32   NF_WORK_CAP_VAL = 0;

fn void redex_worklist_attach(Term *buf, u32 *n_ptr, u32 cap) {
  NF_WORK_PTR     = buf;
  NF_WORK_N_PTR   = n_ptr;
  NF_WORK_CAP_VAL = cap;
}

fn void redex_worklist_detach(void) {
  NF_WORK_PTR     = 0;
  NF_WORK_N_PTR   = 0;
  NF_WORK_CAP_VAL = 0;
}

static inline void nf_work_push(Term t) {
  if (NF_WORK_PTR == 0) return;
  if (!is_redex(t)) return;
  u32 n = *NF_WORK_N_PTR;
  if (n >= NF_WORK_CAP_VAL) return;   // overflow: silently drop
  NF_WORK_PTR[n] = t;
  *NF_WORK_N_PTR = n + 1;
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
          u64 mat_loc = term_val(fun);
          u32 match   = term_ext(fun);
          Term arg_w  = wnf(arg);
          ITRS++;
          if (term_tag(arg_w) == TAG_NUM &&
              (u32)term_val(arg_w) == match) {
            result = heap_read(mat_loc);
          } else if (term_tag(arg_w) == TAG_CTR &&
                     term_ext(arg_w) == match) {
            // Destructure with cell reuse: APP #1 lives in the
            // consumed MAT cell, APP #2 in the consumed CTR's child
            // slots.  See wnf/_.c TAG_MAT for the layout argument.
            Term handler = heap_read(mat_loc);
            u32 n = term_ctr_n(arg_w);
            if (n == 0) { result = handler; break; }
            u64 ctr_loc = term_val(arg_w);
            Term child0 = heap_read(ctr_loc + 1);
            heap_set(mat_loc + 1, child0);
            Term res = term_new(0, TAG_APP, 0, mat_loc);
            if (n >= 2) {
              Term child1 = heap_read(ctr_loc + 2);
              heap_set(ctr_loc + 1, res);
              heap_set(ctr_loc + 2, child1);
              res = term_new(0, TAG_APP, 0, ctr_loc + 1);
            }
            if (n > 2) {
              u64 apps = heap_alloc(2 * (u64)(n - 2));
              for (u32 i = 2; i < n; i++) {
                u64 a = apps + 2 * (u64)(i - 2);
                heap_set(a + 0, res);
                heap_set(a + 1, term_ctr_at(arg_w, i));
                res = term_new(0, TAG_APP, 0, a);
              }
            }
            result = res;
          } else {
            // Miss: reuse the consumed MAT cell as APP(fallback, arg).
            heap_set(mat_loc + 0, heap_read(mat_loc + 1));
            heap_set(mat_loc + 1, arg_w);
            result = term_new(0, TAG_APP, 0, mat_loc);
          }
          break;
        }
        default: return 0;  // validation race: not a redex anymore
      }
      break;
    }
    case TAG_DP0:
    case TAG_DP1: {
      u64  loc  = val;
      // Grad-flavored projection: dispatch to grad-fwd / grad-bwd
      // BEFORE touching the cell (so both projections can read y).
      if (term_ext(redex) & DUP_GRAD_FLAG) {
        if (tag == TAG_DP0) {
          // FWD passthrough: cell holds y, return it.
          ITRS++;
          result = heap_read(loc);
          break;
        }
        // BWD: fire chain rule on y.
        Term g = interact_grad(redex);
        if (g == redex) return 0;   // stuck (y can't pattern-match yet)
        ITRS++;
        result = g;
        break;
      }
      Term cell = heap_take(loc);
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
        case TAG_UOP: {
          Term r = interact_dup_uop(lab, loc, side, body);
          if (r == 0) {
            heap_set(loc, cell);   // active UOP -- restore, stay stuck
            return 0;
          }
          result = r;
          break;
        }
        default:
          heap_set(loc, cell);  // restore -- not a redex anymore
          return 0;
      }
      break;
    }
    case TAG_REF: {
      u32  name = term_ext(redex);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0) return 0;
      // No ITRS bump: REF -> ALO is a structural unfolding step,
      // not an interaction.  Match wnf/_.c TAG_REF.
      result = alo_realize(book, 0);
      break;
    }
    case TAG_ALO: {
      // No ITRS bump: ALO is a structural deep-copy step, not
      // an interaction.  Match wnf/_.c TAG_ALO.
      result = alo_force(redex);
      break;
    }
    case TAG_OP2: {
      Term x = term_resolve(heap_read(val + 0));
      Term y = term_resolve(heap_read(val + 1));
      if (term_tag(x) != TAG_NUM || term_tag(y) != TAG_NUM) return 0;
      u32 op = term_ext(redex);
      u32 xv = (u32)term_val(x);
      u32 yv = (u32)term_val(y);
      u32 r;
      switch (op) {
        case OP_ADD: r = xv + yv; break;
        case OP_SUB: r = xv - yv; break;
        case OP_MUL: r = xv * yv; break;
        case OP_EQ:  r = (xv == yv) ? 1 : 0; break;
        case OP_LT:  r = (xv <  yv) ? 1 : 0; break;
        default:     r = 0; break;
      }
      ITRS++;
      result = term_new(0, TAG_NUM, term_ext(x), r);
      break;
    }
    case TAG_UOP: {
      u8 op = term_ext(redex);
      if (op == UOP_KERNEL) {
        result = interact_kernel(redex);   // ITRS++ inside
      } else if (op == UOP_ASSIGN) {
        result = interact_assign(redex);   // ITRS++ inside
        if (result == redex) return 0;     // stuck -- not a redex yet
      } else if (uop_is_binary_elementwise(op)) {
        // UOP-SUP commutation (HVM4 OP2-SUP shape):
        //   UOP_op(SUP^L(a, b), c) ->
        //     ! &L { c0, c1 } = c;
        //     &L { UOP_op(a, c0), UOP_op(b, c1) }
        //
        // The DUP on `c` is critical: when `c` is also SUP^L (same
        // label), DUP^L collapses against it via DUP-SUP, producing
        // the (a-side-of-c, b-side-of-c) pair.  Without it we'd get
        // SUP^L nested inside SUP^L on every same-label co-occurrence
        // (one per leaf-of-same-tid, which is exactly what blows up
        // for ADD(x, x)).  For non-SUP `c`, DUP-TEN / DUP-UOP / DUP-NUM
        // pass `c` through to both sides (sharing by heap-loc identity).
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

  heap_replace(redex, result);
  // Incremental worklist propagation: push the result (if it's a
  // fresh redex on its own) and every newly allocated cell whose
  // content is a redex.  Replaces nf.c's old `[hb, HEAP_NEXT)` scan
  // and the post-drain global re-enumerate.
  if (NF_WORK_PTR != 0) {
    if (result != 0) nf_work_push(result);
    for (u64 i = hb_at_fire; i < HEAP_NEXT; i++) {
      nf_work_push(heap_read(i));
    }
  }
  // Step session: index newly allocated cells (so future
  // heap_replace finds them) and surface any locally-fresh redex
  // to the fresh-worklist.  Parent recovery for new cells: for
  // every term `t` written into [hb, HEAP_NEXT), find which
  // owning Term in [hb, HEAP_NEXT) has val pointing here.  We
  // do this by sweeping the new cells once and recording each
  // arity-bearing term's child slots as parented by it.
  if (STEP_ACTIVE) {
    if (result != 0) step_fresh_push(result);
    for (u64 i = hb_at_fire; i < HEAP_NEXT; i++) {
      step_use_insert(i, heap_read(i));
    }
    // Second pass: any compound term that lives in [hb, HEAP_NEXT)
    // owns a slot range.  Walk the result + new cells' contents to
    // find such terms and stamp parent ownership.
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

// Hash-set side table for O(1) dedup during redex_enumerate.
// Plain `u8 seen[HEAP_CAP]` would be huge; an open-addressed hash
// keyed on the packed Term value keeps the lookup roughly cache-
// resident and grows lazily as enumeration runs.  Cleared at the
// top of redex_enumerate.  Cap matches NF_WORK_CAP * 4 so worst
// case (every redex unique) keeps load factor under 0.25.
#define REDEX_DEDUP_CAP (1u << 15)
static Term REDEX_DEDUP[REDEX_DEDUP_CAP];

static inline u32 redex_dedup_hash(Term t) {
  u64 h = (u64)t;
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return (u32)h & (REDEX_DEDUP_CAP - 1);
}

// Returns 1 if `t` was inserted (not previously present), 0 if seen.
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
  return 1;   // table full -- accept (rare; cap is 32K)
}

// Add `t` to `out` if it's a redex we haven't already recorded.
static void redex_collect_one(Term t, Term *out, u32 cap, u32 *count) {
  if (!is_redex(t)) return;
  if (!redex_dedup_insert(t)) return;
  if (*count < cap) out[*count] = t;
  (*count)++;
}

// DFS walk from each root, collecting redex Terms into `out`.  Also
// scans every live heap cell -- so redexes held inside a duplicated
// shared subgraph are caught even if the caller didn't pass the
// containing root.  Dedup is by packed Term value.
fn u32 redex_enumerate(Term *roots, u32 n_roots, Term *out, u32 cap) {
  HOT_REDEX_ENUM_CALLS++;
  HOT_REDEX_ENUM_CELLS += HEAP_NEXT;
  // Reset the dedup hash table.  memset is fine -- the table is
  // 32K * 8B = 256KB, well within L2.
  memset(REDEX_DEDUP, 0, sizeof(REDEX_DEDUP));

  u32 count = 0;

  // Worklist DFS for caller-provided roots (which may not live in
  // any heap cell -- e.g. a TTerm the user is holding directly).
  // Dedup-by-Term via REDEX_DEDUP so deeply shared subgraphs don't
  // explode the visit count quadratically.  No upper stack cap:
  // a deeply nested recursive lambda body can easily exceed 256
  // distinct Terms before reaching its leaves.  Use a heap-grown
  // stack via a static buffer that doubles on demand.
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

  // Plus the global heap scan: catches redexes off the root tree
  // (e.g. orphan grad sub-cells whose parent was heap_replaced
  // away, but whose BWD projection still has work to do that the
  // outer chain expects to consume).  With REDEX_DEDUP this is
  // O(HEAP_NEXT) not O(HEAP_NEXT * redex_count) -- the linear
  // scan inside redex_collect_one was the long-recursive-loop
  // killer.  Cells already inserted into REDEX_DEDUP via the DFS
  // are skipped in O(1).
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    redex_collect_one(heap_read(i), out, cap, &count);
  }
  return count;
}

// === step session API =============================================
// `redex_step_attach(roots, n_roots)` walks the heap+roots once,
// builds a Term -> heap-loc inverse index plus a loc -> parent-Term
// map, and seeds the pre-set with whatever is currently a redex.
// Subsequent `redex_step_fire(t)` calls do their parent-slot
// patching via the inverse index in O(uses-of-t) and surface any
// redex-status flips into a fresh-worklist drained by
// `redex_step_drain_fresh`.  Detach restores the legacy linear-
// scan paths.
//
// Memory: O(reachable cells) in three side arrays sized to the
// current HEAP_NEXT, plus two open-addressed hash tables for
// Term-keyed lookup.  All released on detach.
//
// Limitation: GC moves heap locs, so a session must not span a
// gc_collect call.  In practice the WL stepper holds a session
// only while inside one TInteract step, so this is fine.

#define STEP_HEAD_CAP_DEFAULT (1u << 14)
#define STEP_PRE_CAP_DEFAULT  (1u << 14)
#define STEP_FRESH_CAP_DEFAULT (1u << 12)

fn u32 redex_step_attach(Term *roots, u32 n_roots) {
  if (STEP_ACTIVE) return 0;
  STEP_ACTIVE = 1;
  // Allocate side tables up front; heap may grow during fires.
  u64 base_cap = HEAP_NEXT > 4096 ? HEAP_NEXT : 4096;
  STEP_SIDE_CAP = base_cap;
  STEP_USE_NEXT = (u64 *) malloc(base_cap * sizeof(u64));
  STEP_PARENT   = (Term *)calloc(base_cap, sizeof(Term));
  for (u64 i = 0; i < base_cap; i++) STEP_USE_NEXT[i] = STEP_NIL;

  STEP_USE_HEAD_CAP = STEP_HEAD_CAP_DEFAULT;
  STEP_USE_HEAD = (Term *)malloc(STEP_USE_HEAD_CAP * sizeof(Term));
  STEP_USE_KEY  = (Term *)calloc(STEP_USE_HEAD_CAP, sizeof(Term));
  for (u32 i = 0; i < STEP_USE_HEAD_CAP; i++) STEP_USE_HEAD[i] = STEP_NIL;

  STEP_PRE_CAP = STEP_PRE_CAP_DEFAULT;
  STEP_PRE = (Term *)calloc(STEP_PRE_CAP, sizeof(Term));

  STEP_FRESH_CAP   = STEP_FRESH_CAP_DEFAULT;
  STEP_FRESH_BUF   = (Term *)malloc(STEP_FRESH_CAP * sizeof(Term));
  STEP_FRESH_SEEN  = (Term *)calloc(STEP_FRESH_CAP, sizeof(Term));
  STEP_FRESH_N     = 0;

  // Walk every live heap cell once: index its content + record
  // parent ownership (if its content is a compound term whose
  // owned-slot range is also in-heap, those slots get parented).
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    Term t = heap_read(i);
    if (t == 0) continue;
    step_use_insert(i, t);
    u32 ar = term_arity(t);
    if (ar == 0) continue;
    u64 base = term_val(t);
    for (u32 k = 0; k < ar; k++) step_parent_set(base + k, t);
  }

  // Roots may be off-heap (caller holds the Term directly), so the
  // heap walk above won't have stamped them as parents of their
  // owned slots.  Do that here so a heap_replace patch on a root's
  // child slot can promote the root.
  for (u32 r = 0; r < n_roots; r++) {
    Term t = roots[r];
    u32 ar = term_arity(t);
    if (ar == 0) continue;
    u64 base = term_val(t);
    for (u32 k = 0; k < ar; k++) {
      // Don't clobber an existing parent: if this root's child slot
      // is also reachable through a heap-stored term, the heap
      // version is just as valid.
      if (base + k < STEP_SIDE_CAP && STEP_PARENT[base + k] == 0) {
        step_parent_set(base + k, t);
      } else if (base + k >= STEP_SIDE_CAP) {
        step_parent_set(base + k, t);
      }
    }
  }

  // Seed the pre-set + return the initial redex count via the
  // existing enumerate path (also walks caller roots that may not
  // live in any heap cell).
  static Term seed_buf[8192];
  u32 n = redex_enumerate(roots, n_roots, seed_buf, 8192);
  for (u32 i = 0; i < n; i++) step_pre_insert(seed_buf[i]);
  return n;
}

fn Term redex_step_fire(Term redex) {
  if (!STEP_ACTIVE) return redex_fire(redex);
  // Whatever the caller asks to fire is by definition something
  // they already saw -- belongs in the pre-set even if attach
  // missed it (e.g. a redex held off-heap in caller-land).
  step_pre_insert(redex);
  Term r = redex_fire(redex);
  return r;
}

fn u32 redex_step_drain_fresh(Term *out, u32 cap) {
  if (!STEP_ACTIVE) return 0;
  u32 n = STEP_FRESH_N;
  if (n > cap) n = cap;
  for (u32 i = 0; i < n; i++) out[i] = STEP_FRESH_BUF[i];
  // Whatever we're returning to the caller, they now know about,
  // so move it from "fresh" into "pre" before clearing.
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
