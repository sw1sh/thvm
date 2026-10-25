// interact/uop_grad.c - the chain rule.  A grad cell holds [y, gy, target]
// with gy the cotangent of y.  Its DP0 + DUP_GRAD_FLAG projection is y; its
// DP1 + DUP_GRAD_FLAG projection fires interact_grad, which steps through y's
// outer UOp and sums, over the differentiable children, the BWD of a cell
// holding each child's adjoint (the leaf rule at a TEN).  Three walks: the
// target walk (cell[2] set), the single walk (no target, leaves marked
// requires_grad) and the SUP/DUP walk (no target, none marked).
//
// Adjoint table (the cotangent each child slot receives):
//
//   ADD(a, b)        gy_for_a = gy            ; gy_for_b = gy
//   MUL(a, b)        gy_for_a = MUL(b_fwd,gy) ; gy_for_b = MUL(a_fwd,gy)
//   POW(b, e)        gy_for_b = gy * (e == 0 ? e : e * b ** (e - 1)),
//                    gy_for_e = gy * (b == 0 ? (e < 0 ? -inf : 0) : y * log2(b) * ln 2)
//   IWHERE(c, t, e)  gy_for_t = where(c, gy, 0) ; gy_for_e = where(c, 0, gy)
//                    (c gets none)
//   NEG(a)           gy_for_a = NEG(gy)
//   RECIP(a)         gy_for_a = MUL(MUL(NEG(gy), RECIP(a)), RECIP(a))
//   EXP2(a)          gy_for_a = MUL(MUL(EXP2(a), gy), CONST(ln 2))
//   LOG2(a)          gy_for_a = MUL(gy, MUL(RECIP(a), CONST(1/ln2)))
//   SQRT(a)          gy_for_a = MUL(gy, MUL(CONST(0.5), RECIP(SQRT(a))))
//   REDUCE_SUM(a, x) gy_for_a = up(gy),
//                    up(g) = EXPAND(reshape_keepdim(g, x), a.shape)
//   REDUCE_MAX(a, x) gy_for_a = MUL(MUL(m, RECIP(up(SUM(m, x)))), up(gy)),
//                    m = (a == up(y)): tied maxima share gy
//   REDUCE_PROD(a,x) gy_for_a = up(gy) * (a == 0 ? (one zero ? up(PROD(s, x)) : 0)
//                                        : up(y) / a), s = (a == 0 ? 1 : a)
//   EXPAND(a, S)     gy_for_a = REDUCE_SUM_along(expanded_axes, gy)
//   RESHAPE(a, S)    gy_for_a = RESHAPE(gy, a.shape)
//   PERMUTE(a, p)    gy_for_a = PERMUTE(gy, inv_perm)
//   FLIP(a, m)       gy_for_a = FLIP(gy, m)
//   PAD(a, b/e)      gy_for_a = SHRINK(gy, [b_i, b_i + a.dim_i])
//   SHRINK(a, b/e)   gy_for_a = PAD(gy, [b_i, a.dim_i - e_i])
//   GATHER(s, i, d)  gy_for_s = SCATTER(SUM, zeros(s.shape), i, gy, d, 1)
//                    (i gets none)
//   SCATTER(b, i, s, d, SUM, self)
//                    gy_for_b = gy if self, else
//                               SCATTER(REPLACE, gy, i, zeros(i.shape), d)
//                    gy_for_s = GATHER(gy, i, d)            (i gets none)
//   SCATTER(b, i, s, d, MAX, self)   (MIN mirrors it)
//                    M = SCATTER(MAX, full(I), i, s, d, 1); T = b if self,
//                    else SCATTER(REPLACE, b, i, full(I), d)
//                    g_M = gy where M > T, gy/2 where M == T, else 0;
//                    g_T likewise with M < T
//                    gy_for_b = g_T, written positions zeroed unless self
//                    gy_for_s = (s == G(M)) * G(1/cnt) * G(g_M), cnt the
//                               ties of M among the K one-hot entries
//                               (grad_scatter_maxmin)       (i gets none)
//   CAST(a)          gy_for_a = CAST(gy, a.dtype)
//   COPY(a)          gy_for_a = gy
//   KERNEL           the rule runs on the kernel's source_uop
//   CMPLT, CMPEQ, BITCAST, DETACH, CONST, LOAD, ASSIGN: none (a scalar zero)

fn u32 grad_target_dtype(void);

// A one-element zero, not one at y's shape: the ADD summing sibling branches
// would broadcast a target-shaped sibling up to y's shape.
fn Term grad_zero_at(Term y) {
  (void)y;
  return uop_const(grad_target_dtype(), 0);
}

// interact_grad pushes each fire's target (cell[2]) here, so the cells the
// fire allocates inherit it.
#define GRAD_TARGET_STACK_CAP 32
static Term GRAD_TARGET_STACK[GRAD_TARGET_STACK_CAP];
static u32  GRAD_TARGET_TOP = 0;
static inline Term grad_current_target(void) {
  return GRAD_TARGET_TOP > 0
       ? GRAD_TARGET_STACK[GRAD_TARGET_TOP - 1]
       : 0;
}

// Leaves marked requires_grad: TGrad[y], TGrad over several targets and py's
// backward() mark y's float leaves for one walk and unmark them after
// (thvm_wl_tensor_set_requires_grad, py_ten_set_grad_leaf).
u32 GRAD_REQ_NCOUNT = 0;
fn void grad_req_ncount_inc(void) { GRAD_REQ_NCOUNT++; }
fn void grad_req_ncount_dec(void) {
  if (GRAD_REQ_NCOUNT > 0) GRAD_REQ_NCOUNT--;
}

// Chain-rule scalars (zeros, ln 2, 0.5) take the target's dtype, else f32, so
// their buffer's itemsize matches the kernel that reads them.
fn u32 grad_target_dtype(void) {
  Term target = grad_current_target();
  u32  dt     = DT_FP32;
  if (target != 0) term_dtype_in(target, 0, &dt);
  return dt;
}
static u64 grad_cell_alloc(Term child) {
  u64 c = heap_alloc(3);
  heap_set(c + 0, child);
  heap_set(c + 1, 0);                     // placeholder; caller fills gy
  heap_set(c + 2, grad_current_target()); // inherit
  return c;
}
static Term grad_fwd_of(u64 cell) {
  // A child's forward value: the SUP/DUP walk needs a linear DP0 copy; the
  // other walks share the node, so it is realized once for every reader.
  if (grad_current_target() != 0 || GRAD_REQ_NCOUNT > 0)
    return term_resolve(heap_read(cell + 0));
  return term_new(0, TAG_DP0, DUP_GRAD_FLAG, cell);
}
static Term grad_bwd_of(u64 cell) {
  return term_new(0, TAG_DP1, DUP_GRAD_FLAG, cell);
}

// A TEN child serves as its own forward value: it is atomic, so sharing it
// needs no DP0 wrapper.
static Term grad_fwd_inline(u64 cell, Term child) {
  if (term_tag(term_resolve(child)) == TAG_TEN) return child;
  return grad_fwd_of(cell);
}

// Follows ALOs only, not VAR/DP substitutions, so two references to one
// lambda-bound variable compare equal whatever APP-LAM substituted for it.
static Term grad_alo_resolve(Term t) {
  while (term_tag(t) == TAG_ALO) t = alo_force(t);
  return t;
}

// The cotangent a TEN leaf returns.  Target walk: gy at the target, else
// zero.  Single walk: zero, having added gy to the leaf's .grad if marked.
// SUP/DUP walk: SUP^tid(0, gy), for a DUP nest labelled by tid to project.
static Term grad_leaf_sup(Term ten, Term gy_for_leaf) {
  // Callers pass `ten` resolved or raw, and a raw VAR's term_val is its loc,
  // not the tid.
  Term ten_r  = term_resolve(ten);
  if (term_tag(ten_r) != TAG_TEN) ten_r = ten;   // best effort
  Term target = grad_current_target();
  if (target != 0) {
    // The target is this leaf as the same bound variable or the same tid.
    Term ty = grad_alo_resolve(target);
    Term tn = grad_alo_resolve(ten);
    int matched = (ty == tn);
    Term tr = term_resolve(target);
    if (!matched && term_tag(tr) == TAG_TEN
        && term_val(tr) == term_val(ten_r)) {
      matched = 1;
    }
    if (matched) {
      // tinygrad mixin/op.py:450 Tensor.gradient: a named target gets gy
      // whether or not it is marked; marks only pick the single walk's leaves.
      return gy_for_leaf;
    }
    return uop_const(grad_target_dtype(), 0);
  }
  u32 tid = (u32)term_val(ten_r);
  u32 leaf_dt = DT_FP32;
  term_dtype_in(ten_r, 0, &leaf_dt);
  if (GRAD_REQ_NCOUNT > 0 && tid > 0 && tid < TENS_NEXT) {
    if (TENS[tid].requires_grad == 0) {
      return uop_const(leaf_dt, 0);
    }
    Term prev = TENS[tid].grad;
    TENS[tid].grad = (prev == 0)
                   ? gy_for_leaf
                   : uop_binary(UOP_ADD, prev, gy_for_leaf);
    return uop_const(leaf_dt, 0);
  }
  u64 sloc = heap_alloc(2);
  heap_set(sloc + 0, uop_const(leaf_dt, 0));
  heap_set(sloc + 1, gy_for_leaf);
  return term_new(0, TAG_SUP, tid, sloc);
}

// grad_bwd_for_child's memo: its result is a pure function of (child, gy,
// target), so a shared sub-DAG reuses one cell instead of re-firing per parent.
#define GRAD_MEMO_CAP (1u << 14)
typedef struct { u32 gen; Term child, gy, target, result; } GradMemoEntry;
static GradMemoEntry GRAD_MEMO[GRAD_MEMO_CAP];
static u32 GRAD_MEMO_GEN = 1;

static inline u32 grad_memo_hash(Term child, Term gy, Term target) {
  u64 h = (u64)child;
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
  h ^= (u64)gy;     h *= 0xff51afd7ed558ccdULL;
  h ^= (u64)target; h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  return (u32)h & (GRAD_MEMO_CAP - 1);
}

static int grad_memo_lookup(Term child, Term gy, Term target, Term *out) {
  u32 base = grad_memo_hash(child, gy, target);
  for (u32 probe = 0; probe < GRAD_MEMO_CAP; probe++) {
    GradMemoEntry *e = &GRAD_MEMO[(base + probe) & (GRAD_MEMO_CAP - 1)];
    if (e->gen != GRAD_MEMO_GEN) { return 0; }
    if (e->child == child && e->gy == gy && e->target == target) {
      *out = e->result;
      return 1;
    }
  }
  return 0;
}

static void grad_memo_insert(Term child, Term gy, Term target, Term result) {
  u32 base = grad_memo_hash(child, gy, target);
  for (u32 probe = 0; probe < GRAD_MEMO_CAP; probe++) {
    GradMemoEntry *e = &GRAD_MEMO[(base + probe) & (GRAD_MEMO_CAP - 1)];
    if (e->gen != GRAD_MEMO_GEN
        || (e->child == child && e->gy == gy && e->target == target)) {
      e->gen    = GRAD_MEMO_GEN;
      e->child  = child;
      e->gy     = gy;
      e->target = target;
      e->result = result;
      return;
    }
  }
  // Full: drop the entry; the memo only saves work.
}

#define GRAD_DEP_CAP (1u << 15)
typedef struct {
  u32  gen;
  u64  loc;
  Term target;
  u8   result;
  u8   visiting;
} GradDepEntry;
static GradDepEntry GRAD_DEP[GRAD_DEP_CAP];

// A forward node's cotangent sum in the single walk, per (loc, target): the
// pre-walk counts its arrivals and the last one fires one cell with the sum,
// as tinygrad mixin/gradient.py:119-139 sums grads[k] before k's rule runs.
#define GRAD_SLOT_CAP (1u << 15)
typedef struct {
  u32  gen;
  u64  loc;         // forward-node heap loc (dedup key)
  Term target;      // dedup key
  Term pending_gy;  // accumulated cotangent across early arrivals (0 = none)
  u32  n_arrived;   // bwd arrivals seen so far
  u32  n_expected;  // bwd arrivals the pre-walk counted (0 = uncounted)
  u64  cell;        // the cell holding the node's gy (0 = none yet)
  u8   fired;       // 1 once `cell`'s chain rule has fired
} GradSlotEntry;
static GradSlotEntry GRAD_SLOT[GRAD_SLOT_CAP];

// Arrivals past their slot's pre-walk count (an under-count), reset by
// grad_memo_begin_realize; the grad tests check it with grad_slot_stuck_get.
static u64 GRAD_SLOT_EXCESS  = 0;
fn u64 grad_slot_excess_get (void) { return GRAD_SLOT_EXCESS;  }

// Slots whose last arrival never came (the pre-walk over-counted), so their
// cotangent was dropped.
fn u64 grad_slot_stuck_get(void) {
  u64 stuck = 0;
  for (u32 i = 0; i < GRAD_SLOT_CAP; i++) {
    GradSlotEntry *e = &GRAD_SLOT[i];
    if (e->gen == GRAD_MEMO_GEN && e->n_expected > 0
        && e->n_arrived < e->n_expected) stuck++;
  }
  return stuck;
}

fn void grad_memo_begin_realize(void) {
  GRAD_MEMO_GEN++;
  if (GRAD_MEMO_GEN == 0) {
    GRAD_MEMO_GEN = 1;
    memset(GRAD_MEMO, 0, sizeof(GRAD_MEMO));
    memset(GRAD_DEP,  0, sizeof(GRAD_DEP));
    memset(GRAD_SLOT, 0, sizeof(GRAD_SLOT));
  }
  GRAD_SLOT_EXCESS  = 0;
}

static inline u32 grad_dep_hash(u64 loc, Term target) {
  u64 h = loc;
  h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
  h ^= (u64)target; h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return (u32)h & (GRAD_DEP_CAP - 1);
}

static int grad_term_matches_target(Term t, Term target) {
  if (target == 0) {
    return 0;
  }
  // The same bound variable, the same tid, or the same substituted graph.
  Term ty = grad_alo_resolve(target);
  Term tt = grad_alo_resolve(t);
  if (ty == tt) {
    return 1;
  }
  Term tr = term_resolve(target);
  Term rr = term_resolve(t);
  if (term_tag(tr) == TAG_TEN && term_tag(rr) == TAG_TEN
      && term_val(tr) == term_val(rr)) {
    return 1;
  }
  // A recursive loop's `w` past the first iteration: grad_alo_resolve stops at
  // its DP1 wrapper, term_resolve reaches the shared graph and fires nothing.
  return tr == rr;
}

static int grad_depends_on_target(Term t, Term target) {
  if (target == 0) {
    return 1;
  }
  if (grad_term_matches_target(t, target)) {
    return 1;
  }
  Term r = term_resolve(t);
  u8 tag = term_tag(r);
  if (tag == TAG_TEN || tag == TAG_NUM) {
    return 0;
  }
  if (tag != TAG_UOP) {
    return 1;
  }
  u32 op = term_ext(r);
  if (op == UOP_KERNEL) {
    u64 loc = term_val(r);
    u32 kid = (u32)term_val(heap_read(loc + 1));
    if (kid != 0 && kid < KERNELS_NEXT && KERNELS[kid].source_uop != 0) {
      KernelEntry *ke = &KERNELS[kid];
      for (u32 i = 0; i < ke->n_inputs; i++) {
        Term in = ke->input_terms[i];
        if (in == 0) {
          u32 tid = ke->input_tids[i];
          if (tid != 0 && tid < TENS_NEXT) {
            in = term_new(0, TAG_TEN, TENS[tid].dtype, tid);
          }
        }
        if (in != 0 && grad_depends_on_target(in, target)) {
          return 1;
        }
      }
      return grad_depends_on_target(KERNELS[kid].source_uop, target);
    }
    return 1;
  }

  u64 loc = term_val(r);
  u32 idx = grad_dep_hash(loc, target);
  for (u32 probe = 0; probe < GRAD_DEP_CAP; probe++) {
    GradDepEntry *e = &GRAD_DEP[(idx + probe) & (GRAD_DEP_CAP - 1)];
    if (e->gen == GRAD_MEMO_GEN && e->loc == loc && e->target == target) {
      return e->visiting ? 0 : e->result;
    }
    if (e->gen != GRAD_MEMO_GEN) {
      e->gen      = GRAD_MEMO_GEN;
      e->loc      = loc;
      e->target   = target;
      e->visiting = 1;
      e->result   = 1;
      u8 ar = uop_arity((u8)op);
      u8 res = 0;
      for (u8 i = 0; i < ar; i++) {
        if (grad_depends_on_target(heap_read(loc + i), target)) {
          res = 1;
          break;
        }
      }
      e->visiting = 0;
      e->result   = res;
      return res;
    }
  }
  return 1;
}

// The (loc, target) slot, or an empty entry for the caller to grad_slot_init;
// NULL when the table is full.
static GradSlotEntry *grad_slot_get(u64 loc, Term target) {
  u32 idx = grad_dep_hash(loc, target);
  for (u32 probe = 0; probe < GRAD_SLOT_CAP; probe++) {
    GradSlotEntry *e = &GRAD_SLOT[(idx + probe) & (GRAD_SLOT_CAP - 1)];
    if (e->gen != GRAD_MEMO_GEN) return e;
    if (e->loc == loc && e->target == target) return e;
  }
  return NULL;
}

static void grad_slot_init(GradSlotEntry *e, u64 loc, Term target) {
  e->gen        = GRAD_MEMO_GEN;
  e->loc        = loc;
  e->target     = target;
  e->pending_gy = 0;
  e->n_arrived  = 0;
  e->n_expected = 0;
  e->cell       = 0;
  e->fired      = 0;
}

// The pre-walk, after tinygrad's _deepwalk (mixin/gradient.py:109): count the
// BWD arrivals each UOP node under y will get, descending into each node once.
static void grad_prewalk_count(Term y);

// 1 if the pre-walk already descended into loc (GRAD_DEP's `visiting` flag
// marks it); grad_prewalk_credit_and_descend credits each arrival regardless.
static int grad_prewalk_visited(u64 loc, Term target) {
  u32 idx = grad_dep_hash(loc, target);
  for (u32 probe = 0; probe < GRAD_DEP_CAP; probe++) {
    GradDepEntry *e = &GRAD_DEP[(idx + probe) & (GRAD_DEP_CAP - 1)];
    if (e->gen == GRAD_MEMO_GEN && e->loc == loc && e->target == target) {
      if (e->visiting) return 1;
      e->visiting = 1;
      return 0;
    }
    if (e->gen != GRAD_MEMO_GEN) {
      e->gen      = GRAD_MEMO_GEN;
      e->loc      = loc;
      e->target   = target;
      e->visiting = 1;
      e->result   = 0;
      return 0;
    }
  }
  return 1;   // table full: the nodes below stay uncounted
}

// Ops whose rule returns zero without visiting a child; this must match
// interact_grad_dispatch, or a sink's children are over-counted and never
// fire.  DETACH ends the walk (tinygrad mixin/gradient.py:113-114).
static int grad_op_is_diff_sink(u8 op) {
  return op == UOP_CMPLT  || op == UOP_CMPEQ
      || op == UOP_BITCAST || op == UOP_DETACH
      || op == UOP_CONST  || op == UOP_LOAD || op == UOP_ASSIGN || op == UOP_THREEFRY;
}

static void grad_prewalk_credit_and_descend(Term child, Term target) {
  Term cr = term_resolve(child);
  if (term_tag(cr) != TAG_UOP) return;     // TEN leaf / NUM: no slot
  u64 loc = term_val(cr);
  GradSlotEntry *e = grad_slot_get(loc, target);
  if (e == NULL) return;                   // table full: defensive
  if (e->gen != GRAD_MEMO_GEN) {
    grad_slot_init(e, loc, target);
  }
  e->n_expected++;
  grad_prewalk_count(cr);
}

// Bit i: the rule sends a cotangent to child slot i.  None goes to a sink's
// slots, IWHERE's condition (tinygrad mixin/gradient.py:81) or an index.
static u32 grad_op_diff_slot_mask(u8 op) {
  if (grad_op_is_diff_sink(op)) {
    return 0;
  }
  switch (op) {
    case UOP_IWHERE:  return 0x6u;   // then, else
    case UOP_GATHER:  return 0x1u;   // src
    case UOP_SCATTER: return 0x5u;   // base, src
    default:          return (1u << uop_arity(op)) - 1u;
  }
}

// A KERNEL's one arrival goes to its source_uop, where the rule sends it.
static void grad_prewalk_descend_children(u8 op, u64 loc, Term target) {
  if (op == UOP_KERNEL) {
    u32 kid = (u32)term_val(heap_read(loc + 1));
    if (kid != 0 && kid < KERNELS_NEXT) {
      Term src = KERNELS[kid].source_uop;
      if (src != 0) grad_prewalk_credit_and_descend(src, target);
    }
    return;
  }
  u32 mask = grad_op_diff_slot_mask(op);
  u8  ar   = uop_arity(op);
  for (u8 i = 0; i < ar; i++) {
    if (mask & (1u << i)) {
      grad_prewalk_credit_and_descend(heap_read(loc + i), target);
    }
  }
}

static void grad_prewalk_count(Term y) {
  Term r = term_resolve(y);
  if (term_tag(r) != TAG_UOP) return;
  u64 loc = term_val(r);
  u8  op  = (u8)term_ext(r);
  Term target = grad_current_target();
  if (grad_prewalk_visited(loc, target)) return;
  grad_prewalk_descend_children(op, loc, target);
}

// uop_grad runs it on each seed before wnf fires the rule; only the single walk
// uses the counts.
fn void grad_prewalk_fanin(Term y) {
  if (!(GRAD_REQ_NCOUNT > 0 && grad_current_target() == 0)) return;
  grad_prewalk_count(y);
}

// The single walk folds a node's arrivals through its slot; the other walks,
// which the pre-walk does not count, give every arrival its own cell.
static Term grad_bwd_emit_uop(Term child, Term gy) {
  Term cr = term_resolve(child);
  if (term_tag(cr) == TAG_TEN) return grad_leaf_sup(cr, gy);
  int shared = (GRAD_REQ_NCOUNT > 0 && grad_current_target() == 0);
  if (!shared || term_tag(cr) != TAG_UOP) {
    u64 c = grad_cell_alloc(child);
    heap_set(c + 1, gy);
    return grad_bwd_of(c);
  }
  u64           loc    = term_val(cr);
  Term          target = grad_current_target();
  u32           dt     = grad_target_dtype();
  GradSlotEntry *e     = grad_slot_get(loc, target);
  if (e == NULL) {
    // Table full: a cell per arrival, still correct.
    u64 c = grad_cell_alloc(child);
    heap_set(c + 1, gy);
    return grad_bwd_of(c);
  }
  if (e->gen != GRAD_MEMO_GEN) grad_slot_init(e, loc, target);

  // Uncounted (the pre-walk did not reach this node): arrivals fold into one
  // cell's gy until it fires, and an arrival after that opens a new cell.
  if (e->n_expected == 0) {
    if (e->fired) {
      u64 c = grad_cell_alloc(child);
      heap_set(c + 1, gy);
      e->cell  = c;
      e->fired = 0;
      return grad_bwd_of(c);
    }
    if (e->cell != 0) {
      u64 c = e->cell;
      heap_set(c + 1, uop_binary(UOP_ADD, heap_read(c + 1), gy));
      return uop_const(dt, 0);
    }
    u64 c = grad_cell_alloc(child);
    heap_set(c + 1, gy);
    e->cell = c;
    return grad_bwd_of(c);
  }

  e->n_arrived++;
  if (e->n_arrived < e->n_expected) {
    // Early arrival: fold and defer (tinygrad mixin/gradient.py:138).
    e->pending_gy = (e->pending_gy == 0)
                  ? gy
                  : uop_binary(UOP_ADD, e->pending_gy, gy);
    return uop_const(dt, 0);
  }
  if (e->n_arrived == e->n_expected) {
    // Last arrival: one cell with the whole sum, so the subtree below is
    // walked once.
    Term full_gy = (e->pending_gy == 0) ? gy : uop_binary(UOP_ADD, e->pending_gy, gy);
    u64 c = grad_cell_alloc(child);
    heap_set(c + 1, full_gy);
    e->cell  = c;
    e->fired = 0;
    e->pending_gy = 0;
    return grad_bwd_of(c);
  }
  // Excess arrival (the pre-walk under-counted): a cell with just this gy adds
  // the missing part without re-adding the earlier ones.
  GRAD_SLOT_EXCESS++;
  u64 c = grad_cell_alloc(child);
  heap_set(c + 1, gy);
  e->cell  = c;
  e->fired = 0;
  return grad_bwd_of(c);
}

// Marks y's slot fired once its cell `cell_loc` reads gy, so a later uncounted
// arrival opens a new cell instead of folding into a spent one.
static void grad_slot_mark_fired(u64 cell_loc, Term y, Term target) {
  if (!(GRAD_REQ_NCOUNT > 0 && target == 0)) return;
  if (term_tag(y) != TAG_UOP) return;
  u64 loc = term_val(y);
  u32 idx = grad_dep_hash(loc, target);
  for (u32 probe = 0; probe < GRAD_SLOT_CAP; probe++) {
    GradSlotEntry *e = &GRAD_SLOT[(idx + probe) & (GRAD_SLOT_CAP - 1)];
    if (e->gen == GRAD_MEMO_GEN && e->loc == loc && e->target == target) {
      if (e->cell == cell_loc) e->fired = 1;
      return;
    }
    if (e->gen != GRAD_MEMO_GEN) return;
  }
}

static Term grad_bwd_for_child(Term child, Term gy_for_child) {
  Term target = grad_current_target();
  if (target != 0 && !grad_depends_on_target(child, target)) {
    return uop_const(grad_target_dtype(), 0);
  }
  // The single walk's emit is stateful (an early arrival folds and returns
  // zero), so the memo, which assumes a pure function of its keys, skips it.
  int shared = (GRAD_REQ_NCOUNT > 0 && target == 0);
  Term cached;
  if (!shared && grad_memo_lookup(child, gy_for_child, target, &cached)) {
    return cached;
  }

  Term result;
  if (target != 0 && grad_term_matches_target(child, target)) {
    // The child is the target: descending would differentiate the target's
    // own expression, whose leaves are not the target, to zero.
    result = gy_for_child;
    goto done;
  }
  if (term_tag(term_resolve(child)) == TAG_TEN) {
    result = grad_leaf_sup(child, gy_for_child);
    goto done;
  }
  result = grad_bwd_emit_uop(child, gy_for_child);
done:
  if (!shared) grad_memo_insert(child, gy_for_child, target, result);
  return result;
}

// ln 2, 1/ln 2 and 0.5 for the EXP2, LOG2 and SQRT rules, as f32 bits that
// const_to_tendesc converts to the target's dtype when it materializes them.
fn Term grad_ln2_const(void) {
  f32 ln2 = 0.6931471805599453f;
  u32 bits;
  memcpy(&bits, &ln2, sizeof bits);
  return uop_const(grad_target_dtype(), bits);
}
fn Term grad_inv_ln2_const(void) {
  f32 inv = 1.4426950408889634f;
  u32 bits;
  memcpy(&bits, &inv, sizeof bits);
  return uop_const(grad_target_dtype(), bits);
}
fn Term grad_half_const(void) {
  f32 half = 0.5f;
  u32 bits;
  memcpy(&bits, &half, sizeof bits);
  return uop_const(grad_target_dtype(), bits);
}

// The first node since the last grad_err_take that the rule cannot handle
// fires to zero, so the walk finishes; py's backward() raises its message.
static char GRAD_ERR[256];
static char GRAD_ERR_TAKEN[256];

fn void grad_err_set(char const *msg) {
  if (GRAD_ERR[0] != 0) {
    return;
  }
  snprintf(GRAD_ERR, sizeof GRAD_ERR, "%s", msg);
  fprintf(stderr, "thvm: %s\n", GRAD_ERR);
}

// The pending grad error, cleared by the call; NULL when there is none.
fn char const *grad_err_take(void) {
  if (GRAD_ERR[0] == 0) {
    return NULL;
  }
  memcpy(GRAD_ERR_TAKEN, GRAD_ERR, sizeof GRAD_ERR);
  GRAD_ERR[0] = 0;
  return GRAD_ERR_TAKEN;
}

static int grad_shape_eq(Shape const *a, Shape const *b) {
  if (a->ndim != b->ndim) {
    return 0;
  }
  for (u32 d = 0; d < a->ndim; d++) {
    if (a->dims[d] != b->dims[d]) {
      return 0;
    }
  }
  return 1;
}

// A tensor of shape `s` holding the CONST payload `bits` of `dtype`.
static Term grad_full(u32 dtype, Shape const *s, u32 bits) {
  u32 ones[MAX_DIM];
  for (u32 d = 0; d < s->ndim; d++) {
    ones[d] = 1;
  }
  return uop_expand(uop_reshape(uop_const(dtype, bits), s->ndim, ones),
                    s->ndim, s->dims);
}

// gy at y's shape: gy itself, or a broadcast of a one-element gy (the
// scalar-zero convention of grad_zero_at).  0 when neither fits.
static Term grad_gy_at(Term gy, Term y) {
  Shape ys, gs;
  if (!term_shape_in(y, 0, &ys) || !term_shape_in(gy, 0, &gs)) {
    return 0;
  }
  if (grad_shape_eq(&gs, &ys)) {
    return gy;
  }
  if (shape_numel(gs) != 1) {
    return 0;
  }
  u32 ones[MAX_DIM];
  for (u32 d = 0; d < ys.ndim; d++) {
    ones[d] = 1;
  }
  return uop_expand(uop_reshape(gy, ys.ndim, ones), ys.ndim, ys.dims);
}

// tinygrad's sum_acc_dtype for a float cotangent (dtype.py:220): the 16- and
// 8-bit floats accumulate in float32, the others in their own dtype.
static u32 grad_acc_dtype(u32 dtype) {
  if (dtype_is_float(dtype) && dtype_itemsize(dtype) < 4) {
    return DT_FP32;
  }
  return dtype;
}

// gy summed into zeros of src's shape where idx reads, in sum_acc_dtype and
// cast back as tinygrad mixin/gradient.py:133 does; 0 when gy does not fit.
static Term grad_gather_adjoint(Term y, Term src, Term idx, Term gy, u32 dim) {
  Shape ss;
  Term  g = grad_gy_at(gy, y);
  if (g == 0 || !term_shape_in(src, 0, &ss)) {
    return 0;
  }
  u32 dt = DT_FP32;
  term_dtype_in(g, 0, &dt);
  u32 acc = grad_acc_dtype(dt);
  if (acc != dt) {
    g = uop_cast(g, acc);
  }
  Term out = uop_scatter(grad_full(acc, &ss, 0), idx, g, dim, SCATTER_SUM, 1);
  if (out != 0 && acc != dt) {
    out = uop_cast(out, dt);
  }
  return out;
}

// g with every position idx writes along dim set to zero: tinygrad's
// _inv_mask(g, 0) (mixin/op.py:1126) as a REPLACE scatter of zeros.
static Term grad_zero_written(Term g, Term idx, u32 dim) {
  Shape is;
  u32   dt = DT_FP32;
  if (!term_shape_in(idx, 0, &is)) {
    return 0;
  }
  term_dtype_in(g, 0, &dt);
  return uop_scatter(g, idx, grad_full(dt, &is, 0), dim, SCATTER_REPLACE, 0);
}

// The SCATTER MAX / MIN row of the table: tinygrad's amax M.maximum(T)
// (mixin/op.py:1129) through its MAX, REDUCE MAX and WHERE rules
// (mixin/gradient.py:78-79, :10-13, :81).  0 when an operand does not fit.
static int grad_scatter_maxmin(Term y, Term base, Term idx, Term src, Term g,
                               Term *g_base, Term *g_src) {
  u32   kind = uop_scatter_kind(y), dim = uop_scatter_dim(y);
  u32   dt, ident;
  f32   half = 0.5f;
  u32   half_bits;
  Shape bs, is;
  if (!term_dtype_in(y, 0, &dt) || !term_shape_in(y, 0, &bs)
      || !term_shape_in(idx, 0, &is) || !scatter_identity_bits(kind, dt, &ident)) {
    return 0;
  }
  memcpy(&half_bits, &half, sizeof half_bits);
  u32  acc  = grad_acc_dtype(dt);
  Term idn  = grad_full(dt, &bs, ident);
  Term m    = uop_scatter(idn, idx, src, dim, kind, 1);
  Term t    = uop_scatter_include_self(y)
            ? base
            : uop_scatter(base, idx, grad_full(dt, &is, ident), dim, SCATTER_REPLACE, 0);
  if (m == 0 || t == 0) {
    return 0;
  }
  Term m_wins = (kind == SCATTER_MAX) ? uop_binary(UOP_CMPLT, t, m)
                                      : uop_binary(UOP_CMPLT, m, t);
  Term t_wins = (kind == SCATTER_MAX) ? uop_binary(UOP_CMPLT, m, t)
                                      : uop_binary(UOP_CMPLT, t, m);
  Term tie    = uop_binary(UOP_CMPEQ, m, t);
  // A losing branch is +0 for MAX and -0 for MIN: tinygrad's amin is
  // NEG(MAX(NEG R, NEG T)), so its losing branch is -1 * +0.
  Term zero   = uop_const(dt, (kind == SCATTER_MIN && dtype_is_float(dt)) ? 0x80000000u : 0u);
  Term split  = uop_where(tie, uop_binary(UOP_MUL, g, uop_const(dt, half_bits)), zero);
  Term g_m    = uop_where(m_wins, g, split);
  Term g_t    = uop_where(t_wins, g, split);
  *g_base = uop_scatter_include_self(y) ? g_t : grad_zero_written(g_t, idx, dim);
  f32  k_f = (f32)is.dims[dim];
  u32  k_bits;
  memcpy(&k_bits, &k_f, sizeof k_bits);
  // cnt counts the K one-hot entries equal to M: the hits with src == M and,
  // when M is I, the non-hits, which hold I.
  Term e   = uop_cast(uop_binary(UOP_CMPEQ, m, idn), acc);
  Term gm  = uop_gather(m, idx, dim);
  Term ge  = uop_gather(e, idx, dim);
  if (gm == 0 || ge == 0) {
    return 0;
  }
  Term c   = uop_cast(uop_binary(UOP_CMPEQ, src, gm), acc);
  Term sa  = uop_scatter(grad_full(acc, &bs, 0), idx,
                         uop_binary(UOP_ADD, c, uop_unary(UOP_NEG, ge)), dim, SCATTER_SUM, 1);
  if (sa == 0) {
    return 0;
  }
  Term cnt = uop_binary(UOP_ADD, uop_binary(UOP_MUL, e, uop_const(acc, k_bits)), sa);
  Term gr  = uop_gather(uop_unary(UOP_RECIP, cnt), idx, dim);
  Term ggm = uop_gather(g_m, idx, dim);
  if (gr == 0 || ggm == 0 || *g_base == 0) {
    return 0;
  }
  *g_src = uop_binary(UOP_MUL, uop_cast(uop_binary(UOP_MUL, c, gr), dt), ggm);
  return 1;
}

static char const *const GRAD_SCATTER_KIND_NAME[] = {
  [SCATTER_SUM] = "sum", [SCATTER_PROD] = "prod", [SCATTER_MAX] = "amax",
  [SCATTER_MIN] = "amin", [SCATTER_REPLACE] = "replace",
};

static Term grad_kernel_term_from_ten(Term ten) {
  Term tr = term_resolve(ten);
  if (term_tag(tr) != TAG_TEN) return 0;
  u32 tid = (u32)term_val(tr);
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  u32 kid = TENS[tid].producer_kid;
  if (kid == 0 || kid >= KERNELS_NEXT) return 0;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, tr);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, kid));
  return term_new(0, TAG_UOP, UOP_KERNEL, loc);
}

static Term interact_grad_dispatch(Term grad_term) {
  u64  cell_orig = term_val(grad_term);
  Term y         = term_resolve(heap_read(cell_orig + 0));
  // Resolved like y: each lambda iteration hands gy over in a fresh ALO
  // (alo_suspend_child).
  Term gy        = term_resolve(heap_read(cell_orig + 1));
  Term target    = heap_read(cell_orig + 2);

  grad_slot_mark_fired(cell_orig, y, target);

  u8 y_tag = term_tag(y);

  // === LEAF: y is a TEN ===
  // y is the target itself, as the same bound variable.
  if (target != 0) {
    Term ty = grad_alo_resolve(target);
    Term yr = grad_alo_resolve(heap_read(cell_orig + 0));
    if (ty == yr) return gy;
  }

  if (y_tag == TAG_TEN) {
    if (target != 0) {
      Term tr = term_resolve(target);
      if (term_tag(tr) == TAG_TEN && term_val(tr) == term_val(y)) {
        return gy;
      }
      Term kterm = grad_kernel_term_from_ten(y);
      if (kterm != 0) {
        u32 tid = (u32)term_val(y);
        u32 kid = tid < TENS_NEXT ? TENS[tid].producer_kid : 0;
        if (kid != 0 && kid < KERNELS_NEXT) {
          // A kernel's output: differentiate through its source_uop.
          Term src = KERNELS[kid].source_uop;
          if (src != 0 && term_tag(src) == TAG_UOP) {
            return grad_bwd_for_child(src, gy);
          }
        }
      }
      return uop_const(grad_target_dtype(), 0);
    }
    // y is itself a leaf (x.sum() of a 0-D x): the seed is its gradient.
    return grad_leaf_sup(y, gy);
  }

  if (y_tag == TAG_NUM) {
    return uop_const(grad_target_dtype(), 0);
  }

  if (y_tag != TAG_UOP) return grad_term;

  u8  y_op  = term_ext(y);
  u64 y_loc = term_val(y);

  switch (y_op) {

    // === Elementwise binary ===
    case UOP_ADD: {
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term a_bwd = grad_bwd_for_child(a, gy);
      Term b_bwd = grad_bwd_for_child(b, gy);
      return uop_binary(UOP_ADD, a_bwd, b_bwd);
    }

    // Each adjoint needs the other child's forward value; a non-TEN child gets
    // a cell only for its FWD projection.
    case UOP_MUL: {
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term a_resolved = term_resolve(a);
      Term b_resolved = term_resolve(b);
      u8   a_is_ten   = term_tag(a_resolved) == TAG_TEN;
      u8   b_is_ten   = term_tag(b_resolved) == TAG_TEN;
      u64 ca = 0, cb = 0;
      Term a_fwd = a_resolved, b_fwd = b_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      if (!b_is_ten) { cb = grad_cell_alloc(b); b_fwd = grad_fwd_of(cb); }
      Term gy_a = uop_binary(UOP_MUL, b_fwd, gy);
      Term gy_b = uop_binary(UOP_MUL, a_fwd, gy);
      (void)ca; (void)cb;
      Term a_bwd = grad_bwd_emit_uop(a_resolved, gy_a);
      Term b_bwd = grad_bwd_emit_uop(b_resolved, gy_b);
      return uop_binary(UOP_ADD, a_bwd, b_bwd);
    }

    // tinygrad mixin/gradient.py:75-76: b gets e * b ** (e - 1), e where e is 0;
    // e gets y * log2(b) * ln 2, where b is 0 instead 0, or -inf under e < 0.
    case UOP_POW: {
      Term b = heap_read(y_loc + 0);
      Term e = heap_read(y_loc + 1);
      Term b_resolved = term_resolve(b);
      Term e_resolved = term_resolve(e);
      u64  cb = 0, ce = 0;
      Term b_fwd = b_resolved, e_fwd = e_resolved;
      if (term_tag(b_resolved) != TAG_TEN) { cb = grad_cell_alloc(b); b_fwd = grad_fwd_of(cb); }
      if (term_tag(e_resolved) != TAG_TEN) { ce = grad_cell_alloc(e); e_fwd = grad_fwd_of(ce); }
      u32  dt = grad_target_dtype();
      f32  fv[2] = { 1.0f, -INFINITY };
      u32  bits[2];
      memcpy(bits, fv, sizeof bits);
      Term zero = uop_const(dt, 0), one = uop_const(dt, bits[0]), ninf = uop_const(dt, bits[1]);
      Term pw   = uop_binary(UOP_POW, b_fwd, e_fwd);
      Term db   = uop_where(uop_binary(UOP_CMPEQ, e_fwd, zero), e_fwd,
                            uop_binary(UOP_MUL, e_fwd, uop_binary(UOP_POW, b_fwd,
                                uop_binary(UOP_ADD, e_fwd, uop_unary(UOP_NEG, one)))));
      Term de   = uop_where(uop_binary(UOP_CMPEQ, b_fwd, zero),
                            uop_where(uop_binary(UOP_CMPLT, e_fwd, zero), ninf, zero),
                            uop_binary(UOP_MUL, uop_binary(UOP_MUL, pw, uop_unary(UOP_LOG2, b_fwd)),
                                       grad_ln2_const()));
      (void)cb; (void)ce;
      Term b_bwd = grad_bwd_emit_uop(b_resolved, uop_binary(UOP_MUL, gy, db));
      Term e_bwd = grad_bwd_emit_uop(e_resolved, uop_binary(UOP_MUL, gy, de));
      return uop_binary(UOP_ADD, b_bwd, e_bwd);
    }

    case UOP_CMPLT: case UOP_CMPEQ: {
      return grad_zero_at(y);
    }

    // tinygrad mixin/gradient.py:81.  The masks are value selects, so
    // uop_where (no index-layer folds).
    case UOP_IWHERE: {
      Term c     = heap_read(y_loc + 0);
      Term t     = heap_read(y_loc + 1);
      Term e     = heap_read(y_loc + 2);
      u64  cc    = grad_cell_alloc(c);
      Term c_fw  = grad_fwd_inline(cc, c);
      Term zero  = grad_zero_at(y);
      Term t_bwd = grad_bwd_for_child(t, uop_where(c_fw, gy, zero));
      Term e_bwd = grad_bwd_for_child(e, uop_where(c_fw, zero, gy));
      return uop_binary(UOP_ADD, t_bwd, e_bwd);
    }

    // === Elementwise unary ===
    case UOP_NEG: {
      Term a = heap_read(y_loc + 0);
      Term gy_a = uop_unary(UOP_NEG, gy);
      return grad_bwd_for_child(a, gy_a);
    }

    case UOP_LOG2: {
      Term a = heap_read(y_loc + 0);
      Term a_resolved = term_resolve(a);
      u8 a_is_ten = term_tag(a_resolved) == TAG_TEN;
      u64 ca = 0;
      Term a_fwd = a_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      Term ra     = uop_unary(UOP_RECIP, a_fwd);
      Term k      = grad_inv_ln2_const();
      Term factor = uop_binary(UOP_MUL, ra, k);
      Term gy_a   = uop_binary(UOP_MUL, gy, factor);
      (void)ca;
      return grad_bwd_emit_uop(a_resolved, gy_a);
    }

    case UOP_EXP2: {
      Term a = heap_read(y_loc + 0);
      Term a_resolved = term_resolve(a);
      u8 a_is_ten = term_tag(a_resolved) == TAG_TEN;
      u64 ca = 0;
      Term a_fwd = a_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      // tinygrad's ret * ctx * ln2 (mixin/gradient.py:71), which uop_sym's
      // x * (d * y) reads under a sigmoid.
      Term ea   = uop_unary(UOP_EXP2, a_fwd);
      Term gy_a = uop_binary(UOP_MUL, uop_binary(UOP_MUL, ea, gy), grad_ln2_const());
      (void)ca;
      return grad_bwd_emit_uop(a_resolved, gy_a);
    }

    case UOP_SIN: {
      // d sin(a) = sin(pi/2 - a) (tinygrad mixin/gradient.py SIN rule).
      Term a = heap_read(y_loc + 0);
      Term a_resolved = term_resolve(a);
      u8 a_is_ten = term_tag(a_resolved) == TAG_TEN;
      u64 ca = 0;
      Term a_fwd = a_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      f32  half_pi = 1.5707963267948966f;
      u32  bits;
      memcpy(&bits, &half_pi, sizeof bits);
      Term shifted = uop_binary(UOP_ADD, uop_unary(UOP_NEG, a_fwd),
                                uop_const(grad_target_dtype(), bits));
      Term gy_a = uop_binary(UOP_MUL, gy, uop_unary(UOP_SIN, shifted));
      (void)ca;
      return grad_bwd_emit_uop(a_resolved, gy_a);
    }

    case UOP_RECIP: {
      Term a = heap_read(y_loc + 0);
      Term a_resolved = term_resolve(a);
      u8 a_is_ten = term_tag(a_resolved) == TAG_TEN;
      u64 ca = 0;
      Term a_fwd = a_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      // tinygrad's -ctx * ret * ret (mixin/gradient.py:68).
      Term r    = uop_unary(UOP_RECIP, a_fwd);
      Term gy_a = uop_binary(UOP_MUL, uop_binary(UOP_MUL, uop_unary(UOP_NEG, gy), r), r);
      (void)ca;
      return grad_bwd_emit_uop(a_resolved, gy_a);
    }

    case UOP_SQRT: {
      Term a = heap_read(y_loc + 0);
      Term a_resolved = term_resolve(a);
      u8 a_is_ten = term_tag(a_resolved) == TAG_TEN;
      u64 ca = 0;
      Term a_fwd = a_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      Term sa     = uop_unary(UOP_SQRT, a_fwd);
      Term inv_sa = uop_unary(UOP_RECIP, sa);
      Term k      = grad_half_const();
      Term factor = uop_binary(UOP_MUL, k, inv_sa);
      Term gy_a   = uop_binary(UOP_MUL, gy, factor);
      (void)ca;
      return grad_bwd_emit_uop(a_resolved, gy_a);
    }

    // === REDUCE ===
    case UOP_REDUCE: {
      u32  kind   = uop_reduce_kind(y);
      u32  n_axes = uop_reduce_n_axes(y);
      u32  axes[MAX_DIM];
      for (u32 i = 0; i < n_axes; i++) axes[i] = uop_reduce_axis(y, i);
      Term a    = uop_reduce_src(y);

      Shape a_shape;
      if (!term_shape_in(a, 0, &a_shape) || a_shape.ndim == 0) {
        return grad_term;
      }

      u32 keep_dims[MAX_DIM] = {0};
      if (a_shape.ndim == 1) {
        keep_dims[0] = 1;
      } else {
        u8 is_reduce_axis[MAX_DIM] = {0};
        for (u32 i = 0; i < n_axes; i++) {
          if (axes[i] < MAX_DIM) is_reduce_axis[axes[i]] = 1;
        }
        for (u32 i = 0; i < a_shape.ndim; i++)
          keep_dims[i] = is_reduce_axis[i] ? 1u : a_shape.dims[i];
      }
      Term gy_keepdim = uop_reshape(gy, a_shape.ndim, keep_dims);
      Term gy_lifted  = uop_expand(gy_keepdim, a_shape.ndim, a_shape.dims);

      Term gy_a;
      u64  ca = grad_cell_alloc(a);
      Term a_fwd = grad_fwd_inline(ca, a);
      if (kind == REDUCE_SUM) {
        gy_a = gy_lifted;
      } else if (kind == REDUCE_MAX) {
        // tinygrad mixin/gradient.py:10-13, against the forward y: a max
        // recomputed here may round differently, leaving a zero tie count.
        Term mx_keep   = uop_reshape(y, a_shape.ndim, keep_dims);
        Term mx_lifted = uop_expand(mx_keep, a_shape.ndim, a_shape.dims);
        Term mask      = uop_binary(UOP_CMPEQ, a_fwd, mx_lifted);
        Term cnt        = uop_reduce_multi(REDUCE_SUM, n_axes, axes, mask);
        Term cnt_keep   = uop_reshape(cnt, a_shape.ndim, keep_dims);
        Term cnt_lifted = uop_expand(cnt_keep, a_shape.ndim, a_shape.dims);
        Term inv_cnt    = uop_unary(UOP_RECIP, cnt_lifted);
        Term mask_norm  = uop_binary(UOP_MUL, mask, inv_cnt);
        gy_a            = uop_binary(UOP_MUL, mask_norm, gy_lifted);
      } else if (kind == REDUCE_PROD) {
        // tinygrad mixin/gradient.py:14-18: y / x_j where x_j != 0; at the
        // only zero, the product of the others; past one zero, 0.
        u32  dt      = DT_FP32;
        (void)term_dtype_in(a, 0, &dt);
        Term zero    = ru_const_of(dt, 0);
        Term is_zero = uop_binary(UOP_CMPEQ, a_fwd, zero);
        Term safe_x  = uop_where(is_zero, ru_const_of(dt, 1), a_fwd);
        Term n_zero  = uop_reduce_multi(REDUCE_SUM, n_axes, axes, uop_cast(is_zero, DT_INT32));
        Term others  = uop_reduce_multi(REDUCE_PROD, n_axes, axes, safe_x);
        Term one_zero = uop_binary(UOP_CMPEQ, n_zero, uop_const(DT_INT32, 1));
        Term at_zero = uop_where(one_zero, others, uop_const(dt, 0));
        Term y_keep  = uop_expand(uop_reshape(y, a_shape.ndim, keep_dims), a_shape.ndim, a_shape.dims);
        Term ratio   = uop_binary(UOP_MUL, y_keep, uop_unary(UOP_RECIP, safe_x));
        Term az_up   = uop_expand(uop_reshape(at_zero, a_shape.ndim, keep_dims), a_shape.ndim, a_shape.dims);
        gy_a = uop_binary(UOP_MUL, gy_lifted, uop_where(is_zero, az_up, ratio));
      } else {
        // Unknown reduce kind: pass gy through unchanged (best effort).
        gy_a = gy_lifted;
      }
      (void)ca;
      return grad_bwd_emit_uop(a, gy_a);
    }

    // === Indexed ops ===
    // The gradient of tinygrad's one-hot where + sum (mixin/op.py:1041).
    case UOP_GATHER: {
      Term src = uop_gather_src(y);
      Term idx = uop_gather_idx(y);
      u64  ci  = grad_cell_alloc(idx);
      Term g_src = grad_gather_adjoint(y, src, grad_fwd_inline(ci, idx), gy,
                                       uop_gather_dim(y));
      if (g_src == 0) {
        grad_err_set("grad: the cotangent of a gather does not fit its shape");
        return grad_zero_at(y);
      }
      return grad_bwd_for_child(src, g_src);
    }

    // SUM differentiates tinygrad's mask.where(src, 0).sum(-1).add(self or
    // _inv_mask(self, 0)) (mixin/op.py:1127).  PROD and REPLACE have no rule
    // yet: they fire to zero and set the error slot.
    case UOP_SCATTER: {
      u32 kind = uop_scatter_kind(y);
      if (kind != SCATTER_SUM && kind != SCATTER_MAX && kind != SCATTER_MIN) {
        char msg[96];
        snprintf(msg, sizeof msg, "grad: no gradient rule yet for a '%s' scatter",
                 kind <= SCATTER_REPLACE ? GRAD_SCATTER_KIND_NAME[kind] : "?");
        grad_err_set(msg);
        return grad_zero_at(y);
      }
      Term base   = uop_scatter_base(y);
      Term idx    = uop_scatter_idx(y);
      Term src    = uop_scatter_src(y);
      u32  dim    = uop_scatter_dim(y);
      u64  ci     = grad_cell_alloc(idx);
      Term idx_fw = grad_fwd_inline(ci, idx);
      Term g      = grad_gy_at(gy, y);
      Term g_src  = 0;
      Term g_base = 0;
      if (g != 0 && kind == SCATTER_SUM) {
        g_src  = uop_gather(g, idx_fw, dim);
        g_base = uop_scatter_include_self(y) ? g : grad_zero_written(g, idx_fw, dim);
      } else if (g != 0) {
        u64 cb = grad_cell_alloc(base);
        u64 cs = grad_cell_alloc(src);
        if (!grad_scatter_maxmin(y, grad_fwd_inline(cb, base), idx_fw,
                                 grad_fwd_inline(cs, src), g, &g_base, &g_src)) {
          g_src = 0;
        }
      }
      if (g_src == 0 || g_base == 0) {
        grad_err_set("grad: the cotangent of a scatter does not fit its shape");
        return grad_zero_at(y);
      }
      Term base_bwd = grad_bwd_for_child(base, g_base);
      Term src_bwd  = grad_bwd_for_child(src, g_src);
      return uop_binary(UOP_ADD, base_bwd, src_bwd);
    }

    // === Movement ops ===
    case UOP_RESHAPE: {
      Term a = heap_read(y_loc + 0);
      Shape a_shape;
      Term gy_a;
      if (term_shape_in(a, 0, &a_shape) && a_shape.ndim > 0) {
        gy_a = uop_reshape(gy, a_shape.ndim, a_shape.dims);
      } else {
        gy_a = gy;   // best effort: passthrough
      }
      return grad_bwd_for_child(a, gy_a);
    }

    case UOP_EXPAND: {
      Term a    = heap_read(y_loc + 0);
      u32  ndim = (u32)term_val(heap_read(y_loc + 1));   // out ndim
      u32  out_dims[MAX_DIM];
      for (u32 i = 0; i < ndim; i++)
        out_dims[i] = (u32)term_val(heap_read(y_loc + 2 + i));

      Shape a_shape;
      Term gy_a;
      if (term_shape_in(a, 0, &a_shape) && ndim >= a_shape.ndim) {
        // Leading 1s up to out's rank, so a rank increase reads as broadcast.
        u32 pad = ndim - a_shape.ndim;
        u32 a_padded[MAX_DIM] = {0};
        for (u32 i = 0; i < pad; i++) a_padded[i] = 1;
        for (u32 i = 0; i < a_shape.ndim; i++) a_padded[pad + i] = a_shape.dims[i];

        // Sum every axis whose size changed, 1 -> 0 included, in one REDUCE (the
        // per-axis reduces merge), as tinygrad does (mixin/gradient.py:131-133);
        // reduce_chain_collect makes it one kernel only over contiguous trailing
        // axes, so they move last.
        u32 bcast_axes[MAX_DIM]; u32 n_bcast = 0;
        for (u32 i = 0; i < ndim; i++) {
          if (a_padded[i] != out_dims[i]) bcast_axes[n_bcast++] = i;
        }
        Term cur = gy;
        int already_trailing = (n_bcast == 0)
            || (bcast_axes[n_bcast - 1] == ndim - 1
                && bcast_axes[0] == ndim - n_bcast);
        if (n_bcast >= 2 && !already_trailing) {
          u32 perm[MAX_DIM]; u32 pn = 0;
          u8  is_bcast[MAX_DIM] = {0};
          for (u32 i = 0; i < n_bcast; i++) is_bcast[bcast_axes[i]] = 1;
          for (u32 i = 0; i < ndim; i++) if (!is_bcast[i]) perm[pn++] = i;
          for (u32 i = 0; i < n_bcast; i++) perm[pn++] = bcast_axes[i];
          cur = uop_permute(cur, ndim, perm);
          for (i32 axis = (i32)ndim - 1; axis >= (i32)(ndim - n_bcast); axis--)
            cur = uop_reduce(REDUCE_SUM, (u32)axis, cur);
        } else {
          for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
            if (a_padded[axis] != out_dims[axis]) {
              cur = uop_reduce(REDUCE_SUM, (u32)axis, cur);
            }
          }
        }
        // The reduces drop the broadcast axes; reshaping restores a's own 1s
        // (a 0-D a is the full reduce itself).
        gy_a = a_shape.ndim == 0 ? cur : uop_reshape(cur, a_shape.ndim, a_shape.dims);
      } else {
        gy_a = gy;
      }
      return grad_bwd_for_child(a, gy_a);
    }

    case UOP_FLIP: {
      Term a    = heap_read(y_loc + 0);
      u32  mask = (u32)term_val(heap_read(y_loc + 1));
      Term gy_a = uop_flip(gy, mask);
      return grad_bwd_for_child(a, gy_a);
    }

    case UOP_PERMUTE: {
      Term a = heap_read(y_loc + 0);
      Shape src_shape;
      if (!term_shape_in(a, 0, &src_shape) || src_shape.ndim == 0) {
        return grad_term;
      }
      u32 ndim = src_shape.ndim;
      u32 perm[MAX_DIM], inv_perm[MAX_DIM];
      for (u32 i = 0; i < ndim; i++)
        perm[i] = (u32)term_val(heap_read(y_loc + 2 + i));
      for (u32 i = 0; i < ndim; i++)
        inv_perm[perm[i]] = i;
      Term gy_a = uop_permute(gy, ndim, inv_perm);
      return grad_bwd_for_child(a, gy_a);
    }

    case UOP_PAD: case UOP_SHRINK: {
      Term a = heap_read(y_loc + 0);
      Shape a_shape;
      if (!term_shape_in(a, 0, &a_shape) || a_shape.ndim == 0) {
        return grad_term;
      }
      u32 ndim = a_shape.ndim;
      u32 ranges[2 * MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        ranges[2 * i + 0] = (u32)term_val(heap_read(y_loc + 2 + 2 * i));
        ranges[2 * i + 1] = (u32)term_val(heap_read(y_loc + 3 + 2 * i));
      }

      // A PAD or SHRINK that changes nothing passes gy through, as tinygrad's
      // returns self (mixin/movement.py:186-212): its inverse as a zero-width
      // PAD left the scheduler a reduce it could not inline.
      int noop = 1;
      for (u32 i = 0; i < ndim; i++) {
        noop &= y_op == UOP_PAD ? ranges[2 * i] == 0 && ranges[2 * i + 1] == 0
                                : ranges[2 * i] == 0 && ranges[2 * i + 1] == a_shape.dims[i];
      }
      if (noop) {
        return grad_bwd_for_child(a, gy);
      }
      Term gy_a;
      if (y_op == UOP_PAD) {
        // ranges are the pad widths (b_i, e_i).
        u32 sranges[2 * MAX_DIM];
        for (u32 i = 0; i < ndim; i++) {
          sranges[2 * i + 0] = ranges[2 * i + 0];
          sranges[2 * i + 1] = ranges[2 * i + 0] + a_shape.dims[i];
        }
        gy_a = uop_shrink(gy, ndim, sranges);
      } else {
        // ranges are the kept bounds [b_i, e_i).
        u32 widths[2 * MAX_DIM];
        for (u32 i = 0; i < ndim; i++) {
          widths[2 * i + 0] = ranges[2 * i + 0];
          widths[2 * i + 1] = (a_shape.dims[i] > ranges[2 * i + 1])
                                ? (a_shape.dims[i] - ranges[2 * i + 1]) : 0;
        }
        gy_a = uop_pad(gy, ndim, widths);
      }
      return grad_bwd_for_child(a, gy_a);
    }

    case UOP_KERNEL: {
      // The rule runs on the pre-kernelize source_uop, not the program.
      u32 kid = (u32)term_val(heap_read(y_loc + 1));
      if (kid == 0 || kid >= KERNELS_NEXT) {
        return grad_zero_at(y);
      }
      Term src = KERNELS[kid].source_uop;
      if (src != 0 && term_tag(src) == TAG_UOP) {
        return grad_bwd_for_child(src, gy);
      }
      return grad_zero_at(y);
    }

    case UOP_COPY:
      // tinygrad copies gy back to src's device (mixin/gradient.py:92); a
      // realize runs on one backend, so gy passes straight through.
      return grad_bwd_for_child(heap_read(y_loc), gy);

    case UOP_CAST: {
      // tinygrad mixin/gradient.py:67.
      Term src = heap_read(y_loc);
      u32 src_dtype = DT_FP32;
      term_dtype_in(src, 0, &src_dtype);
      Term gy_in = uop_cast(gy, src_dtype);
      return grad_bwd_for_child(src, gy_in);
    }
    case UOP_BITCAST:
      // tinygrad mixin/gradient.py:105-106: a bitcast has no gradient.
      return grad_zero_at(y);

    case UOP_CONST:
    case UOP_LOAD:
    case UOP_ASSIGN:
    case UOP_THREEFRY:
      return grad_zero_at(y);

    case UOP_DETACH:
      // Stop-gradient: the child is a constant to the backward.
      return grad_zero_at(y);

    default:
      return grad_term;
  }
}

// Mutually recursive with uop_child_is_active.
static u8 uop_has_active_descendant_memo(Term t);

// Memo for the transitive active scan, a generation per outermost call: a
// shared sub-DAG (Adam's gT in gT*gT and 0.1*gT) would walk exponentially.
#define UOP_ACT_MEMO_CAP (1u << 14)
typedef struct { u32 gen; u64 valid_loc; u8 result; u8 visiting; } UopActMemoEntry;
static UopActMemoEntry UOP_ACT_MEMO[UOP_ACT_MEMO_CAP];
static u32 UOP_ACT_MEMO_GEN = 0;
static u32 UOP_ACT_MEMO_DEPTH = 0;     // re-entrant guard
static u32 UOP_ACT_SCAN_DEPTH = 0;

static inline u32 uop_act_memo_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (UOP_ACT_MEMO_CAP - 1);
}

static inline void uop_act_memo_next_gen(void) {
  UOP_ACT_MEMO_GEN++;
  if (UOP_ACT_MEMO_GEN == 0) {
    UOP_ACT_MEMO_GEN = 1;
    memset(UOP_ACT_MEMO, 0, sizeof(UOP_ACT_MEMO));
  }
}

// Whether wnf must descend into UOP child t: a DP0/DP1, KERNEL or ASSIGN has
// a rule to fire, and a passive UOP may hide one below.
fn u8 uop_child_is_active(Term t) {
  // Checked before term_resolve, which unwraps a grad DP0 to cell[0]: wnf must
  // still descend into it, or the DP0 survives and materialize bails on it.
  u8 raw_tag = term_tag(t);
  if (raw_tag == TAG_DP0 || raw_tag == TAG_DP1) return 1;
  Term r = term_resolve(t);
  u8   tag = term_tag(r);
  if (tag == TAG_DP0 || tag == TAG_DP1) return 1;
  if (tag == TAG_UOP) {
    u32 op = term_ext(r);
    if (op == UOP_KERNEL || op == UOP_ASSIGN) return 1;
    return uop_has_active_descendant_memo(r);
  }
  return 0;
}

static u8 uop_has_active_descendant_memo(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_KERNEL || op == UOP_ASSIGN) return 0;
  u8 ar = uop_arity((u8)op);
  u64 loc = term_val(t);
  // One generation per uop_next_active_child scan, whose sibling slots share
  // descendants; a standalone probe gets its own.
  u8 outer = (UOP_ACT_MEMO_DEPTH == 0 && UOP_ACT_SCAN_DEPTH == 0);
  if (outer) uop_act_memo_next_gen();
  UOP_ACT_MEMO_DEPTH++;
  // `visiting` breaks cycles.
  u32 idx = uop_act_memo_hash(loc);
  for (u32 probe = 0; probe < UOP_ACT_MEMO_CAP; probe++) {
    u32 i = (idx + probe) & (UOP_ACT_MEMO_CAP - 1);
    UopActMemoEntry *e = &UOP_ACT_MEMO[i];
    if (e->gen == UOP_ACT_MEMO_GEN && e->valid_loc == loc) {
      UOP_ACT_MEMO_DEPTH--;
      if (outer) UOP_ACT_MEMO_DEPTH = 0;
      return e->visiting ? 0 : e->result;
    }
    if (e->gen != UOP_ACT_MEMO_GEN) {
      e->gen       = UOP_ACT_MEMO_GEN;
      e->valid_loc = loc;
      e->visiting  = 1;
      e->result    = 0;
      u8 res = 0;
      for (u8 c = 0; c < ar; c++) {
        if (uop_child_is_active(heap_read(loc + c))) { res = 1; break; }
      }
      e->visiting = 0;
      e->result   = res;
      UOP_ACT_MEMO_DEPTH--;
      if (outer) UOP_ACT_MEMO_DEPTH = 0;
      return res;
    }
  }
  UOP_ACT_MEMO_DEPTH--;
  if (outer) UOP_ACT_MEMO_DEPTH = 0;
  return 0;
}

// First active-or-active-bearing child slot index in a UOP at or
// after `start`, or `ar` if none.
fn u8 uop_next_active_child(u64 uop_loc, u8 ar, u8 start) {
  u8 outer = (UOP_ACT_SCAN_DEPTH == 0);
  if (outer) uop_act_memo_next_gen();
  UOP_ACT_SCAN_DEPTH++;
  for (u8 i = start; i < ar; i++) {
    if (uop_child_is_active(heap_read(uop_loc + i))) {
      UOP_ACT_SCAN_DEPTH--;
      if (outer) UOP_ACT_SCAN_DEPTH = 0;
      return i;
    }
  }
  UOP_ACT_SCAN_DEPTH--;
  if (outer) UOP_ACT_SCAN_DEPTH = 0;
  return ar;
}

fn Term interact_grad(Term grad_term) {
  HOT_GRAD_FIRES++;
  Term target = heap_read(term_val(grad_term) + 2);
  u8 pushed = 0;
  if (GRAD_TARGET_TOP < GRAD_TARGET_STACK_CAP) {
    GRAD_TARGET_STACK[GRAD_TARGET_TOP++] = target;
    pushed = 1;
  }
  Term r = interact_grad_dispatch(grad_term);
  if (pushed) GRAD_TARGET_TOP--;

  // Pad a confirmed mismatch to the target's shape: a result can be one
  // element.  Fires never nest, so the stack test passes at every fire.
  if (target != 0 && pushed && GRAD_TARGET_TOP == 0) {
    Term tr = term_resolve(target);
    if (term_tag(tr) == TAG_TEN) {
      Shape tshape, rshape;
      // An unknown result shape is taken as the target's: an unconditional
      // wrap would cost a kernel per fire.
      if (term_shape_in(tr, 0, &tshape) && tshape.ndim > 0
          && term_shape_in(r,  0, &rshape) && rshape.ndim > 0) {
        u8 mismatch = (rshape.ndim != tshape.ndim);
        if (!mismatch) {
          for (u32 i = 0; i < tshape.ndim; i++) {
            if (rshape.dims[i] != tshape.dims[i]) { mismatch = 1; break; }
          }
        }
        if (mismatch) {
          u32 dt = DT_FP32;
          term_dtype_in(target, 0, &dt);
          Term zero    = uop_const(dt, 0);
          Term zero_lf = uop_expand(zero, tshape.ndim, tshape.dims);
          r = uop_binary(UOP_ADD, r, zero_lf);
        }
      }
    }
  }
  return r;
}
