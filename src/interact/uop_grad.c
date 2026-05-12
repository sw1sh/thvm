// interact/uop_grad.c - dup-like, gy-threaded chain rule for the BWD
// projection of a grad cell (TAG_DP1 + DUP_GRAD_FLAG).
//
// A grad cell is a regular dup-cell with two slots:
//
//     cell[0] = y    (the value being differentiated)
//     cell[1] = gy   (the cotangent at y's shape -- ones for scalar
//                     loss seed; transformed per-operator down the
//                     chain so it always reaches a sub-cell at the
//                     sub-cell's child shape)
//
// Two aux ports share the cell:
//
//     TAG_DP0 + DUP_GRAD_FLAG  =  FWD = passthrough cell[0] = y
//     TAG_DP1 + DUP_GRAD_FLAG  =  BWD = chain rule (this file)
//
// Each chain-rule fire walks one structural step on y's outermost
// UOp.  For each compute child a_i, the rule:
//
//   1. computes the per-op adjoint  gy_for_a_i  (cotangent-at-a_i.shape)
//      using gy from cell[1] AND any forward sub-values it needs
//      (e.g. b for the MUL adjoint  ∂(a*b)/∂a = b * gy);
//   2. allocates a fresh sub-cell  [a_i, gy_for_a_i]  via
//      grad_cell_alloc / heap_set, with FWD/BWD projections of the
//      sub-cell carrying DUP_GRAD_FLAG;
//   3. emits an outer expression that combines the children's BWD
//      projections (sum over differentiable children).
//
// At a TEN leaf the rule emits  SUP^{tid}(zero_at_S, gy)  where S =
// leaf.shape and gy is the cotangent that has been threaded down to
// the leaf.  The outer DUP^t at the WL surface then projects:
//   - match  side (target == t)        => gy   (= per-element grad)
//   - mismatch side (target != t)      => zero (no contribution)
//
// Adjoint table (one row per UOP child slot):
//
//   ADD(a, b)        gy_for_a = gy            ; gy_for_b = gy
//   MUL(a, b)        gy_for_a = MUL(b_fwd,gy) ; gy_for_b = MUL(a_fwd,gy)
//   NEG(a)           gy_for_a = NEG(gy)
//   RECIP(a)         gy_for_a = MUL(gy, NEG(MUL(RECIP(a), RECIP(a))))
//   EXP2(a)          gy_for_a = MUL(gy, MUL(EXP2(a), CONST(ln 2)))
//   LOG2(a)          gy_for_a = MUL(gy, MUL(RECIP(a), CONST(1/ln2)))
//   SQRT(a)          gy_for_a = MUL(gy, MUL(CONST(0.5), RECIP(SQRT(a))))
//   REDUCE_SUM(a, x) gy_for_a = EXPAND(reshape_keepdim(gy, x), a.shape)
//   REDUCE_MAX(a, x) gy_for_a = MUL(mask, EXPAND(reshape_keepdim(gy, x), a.shape))
//   EXPAND(a, S)     gy_for_a = REDUCE_SUM_along(expanded_axes, gy)
//   RESHAPE(a, S)    gy_for_a = RESHAPE(gy, a.shape)
//   PERMUTE(a, p)    gy_for_a = PERMUTE(gy, inv_perm)
//   FLIP(a, m)       gy_for_a = FLIP(gy, m)
//   PAD(a, b/e)      gy_for_a = SHRINK(gy, [b_i, b_i + a.dim_i])
//   SHRINK(a, b/e)   gy_for_a = PAD(gy, [b_i, src.dim_i - e_i])
//   CONST/LOAD/CMP   no children to differentiate (leaf cotangent dies)

// Lift `src` to `target_term`'s shape via UOP_EXPAND.  Works for
// both TAG_TEN (look up TENS[tid].view.shape) and TAG_UOP (use
// term_shape_in to compute the result shape statically).  No-op
// when shape can't be determined or the rank is zero.
fn Term expand_to_target(Term src, Term target_term) {
  Shape s;
  if (!term_shape_in(target_term, 0, &s) || s.ndim == 0) return src;
  return uop_expand(src, s.ndim, s.dims);
}

// Forward decl: grad_target_dtype reads the target stack defined
// alongside grad_current_target below.
fn u32 grad_target_dtype(void);

// Scalar zero for a non-differentiable branch (CMPLT/CMPEQ,
// CONST/LOAD/ASSIGN, unknown KERNEL).  Returns numel-1 CONST(0)
// rather than EXPAND'd-to-y's-shape zero -- the chain rule's outer
// ADD combiner relies on numel-1 broadcasting to combine sibling
// branches whose gradients land at the TARGET's shape (typically
// much smaller than y's shape).  Returning at y's shape forces the
// outer ADD to broadcast the OTHER (target-shaped) branch up to y's
// shape, producing a wrong-shaped gradient at the leaf.  Mirrors
// the scalar-zero convention used at non-target TEN leaves
// (grad_leaf_sup) and at the dispatch's TEN-leaf mismatch path.
// Dtype tracks the target so the zero buffer's itemsize matches the
// kernel that ADDs / MULs it.
fn Term grad_zero_at(Term y) {
  (void)y;
  return uop_const(grad_target_dtype(), 0);
}

// Allocate a 3-slot grad cell with cell[0] = child, cell[1] = 0
// (placeholder), cell[2] = target.  Caller MUST heap_set(loc + 1,
// gy_for_child) before the BWD projection is forced.  Returning the
// loc lets us split the fwd/bwd construction across the gy-build
// (caller may need the FWD of one sibling to construct the other
// sibling's gy, e.g. MUL).
//
// Sub-cells inherit `target` from a thread-static set up by
// interact_grad on entry: the OUTER fire reads cell[2] and pushes
// it on a small stack so sub-cells allocated during the chain-rule
// rewrite carry the same target.  When target != 0, the leaf rule
// does direct tid-equality match against target (returning gy on
// match, scalar zero on mismatch) -- no SUP/DUP scaffolding needed.
// When target == 0, the leaf falls back to the SUP/DUP path so the
// outer DUP^t.tid nest at the WL surface can do per-target
// projection (used by TGradMany and concrete-target single TGrad).
#define GRAD_TARGET_STACK_CAP 32
static Term GRAD_TARGET_STACK[GRAD_TARGET_STACK_CAP];
static u32  GRAD_TARGET_TOP = 0;
static inline Term grad_current_target(void) {
  return GRAD_TARGET_TOP > 0
       ? GRAD_TARGET_STACK[GRAD_TARGET_TOP - 1]
       : 0;
}

// Pick the dtype to materialize chain-rule scalars at.  Returns the
// current target's dtype (term_dtype_in walks the graph) so scalar
// zeros and bookkeeping consts (ln2, half, ...) line up with the
// kernel that consumes them.  Defaulting to DT_FP32 here used to
// bit-misalign under f64 / f16 / bf16: cpu_op_add on a numel-1
// broadcast f32 zero against an f64 src reads 8 bytes from a 4-byte
// const buffer.
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
  return term_new(0, TAG_DP0, DUP_GRAD_FLAG, cell);
}
static Term grad_bwd_of(u64 cell) {
  return term_new(0, TAG_DP1, DUP_GRAD_FLAG, cell);
}

// FWD reference for a sibling's chain-rule cell, used in cross-
// references inside gy_for_other (e.g. MUL's gy_for_a needs b's
// forward value).  When the child is already an atomic TEN we
// inline it directly: the DP0+grad_flag wrapper would resolve to
// the same TEN via term_resolve, but the wrapper costs one Term
// per reference, and these multiply across nested TGrad rounds.
// Skipping the wrap for TEN children is safe -- TEN is atomic, no
// re-evaluation cost from sharing.
static Term grad_fwd_inline(u64 cell, Term child) {
  if (term_tag(term_resolve(child)) == TAG_TEN) return child;
  return grad_fwd_of(cell);
}

// Emit the leaf result for a TEN child.
//
// Two paths:
//   (a) target-bearing fire (grad_current_target() != 0): resolve
//       target to a concrete TEN; if its tid matches the leaf's,
//       return gy_for_leaf directly; otherwise return a scalar zero.
//       This is what makes TGrad work inside TLam bodies -- the WL
//       constructor can pass the (still-symbolic) bound variable as
//       target, and after APP-LAM beta the chain rule resolves it
//       to the substituted TEN here.
//   (b) plain fire (target == 0): emit SUP^{tid}(CONST(0), gy).
//       The mismatch slot is a scalar zero (shape {1}); cpu_op_add /
//       cpu_op_mul broadcast numel==1 operands, so any outer combiner
//       ADD(matched_at_S, scalar_zero) resolves to matched_at_S
//       regardless of S.  The match slot keeps gy at its natural
//       (= leaf) shape so the surrounding DUP^t.tid match returns
//       the correct per-element VJP.  Used when the WL surface
//       wraps with the per-leaf DUP nest (concrete-target TGrad
//       and TGradMany).
// Resolve through ALO chains but DO NOT follow VAR/DP-SUB.  Used
// to compute "variable identity": for a lambda-bound `w` whose
// template loc is buried under one or more ALOs, we want the
// realized TVAR (the dyn loc) -- not whatever value APP-LAM beta
// substituted at that loc.  That way two terms referring to the
// same bound variable compare equal even when SUB has substituted
// it to a yet-unmaterialized UOP graph (typical iter-2+ of a
// recursive sgd loop).
static Term grad_alo_resolve(Term t) {
  while (term_tag(t) == TAG_ALO) t = alo_force(t);
  return t;
}

static Term grad_leaf_sup(Term ten, Term gy_for_leaf) {
  // Resolve `ten` defensively: callers either pass an already-
  // resolved Term (MUL/RECIP/etc.) or the raw heap_read child
  // (grad_bwd_for_child).  Reading term_val on an unresolved VAR
  // gives the var-loc, not the substituted TEN's tid -- a tid
  // comparison against `target` would silently mismatch.
  Term ten_r  = term_resolve(ten);
  if (term_tag(ten_r) != TAG_TEN) ten_r = ten;   // best effort
  Term target = grad_current_target();
  if (target != 0) {
    // Variable-identity match first: target's alo-resolved form is
    // TVAR(dyn_loc).  If the leaf-as-passed (ten) alo-resolves to
    // the same TVAR, we're at the bound variable and gy is the
    // gradient regardless of what SUB has substituted.
    Term ty = grad_alo_resolve(target);
    Term tn = grad_alo_resolve(ten);
    if (ty == tn) return gy_for_leaf;
    // Concrete-tid match: target resolved through SUB to a TEN
    // (iter-1 case where w0 is concrete).
    Term tr = term_resolve(target);
    if (term_tag(tr) == TAG_TEN && term_val(tr) == term_val(ten_r)) {
      return gy_for_leaf;
    }
    return uop_const(grad_target_dtype(), 0);   // mismatch -> scalar zero
  }
  u32 tid = (u32)term_val(ten_r);
  u32 leaf_dt = DT_FP32;
  term_dtype_in(ten_r, 0, &leaf_dt);
  u64 sloc = heap_alloc(2);
  heap_set(sloc + 0, uop_const(leaf_dt, 0));
  heap_set(sloc + 1, gy_for_leaf);
  return term_new(0, TAG_SUP, tid, sloc);
}

// Build the BWD reference for a child:
//   - if child IS the target variable (alo-only resolve match),
//     short-circuit to gy_for_child directly: this child is the
//     differentiation target leaf, regardless of what SUB has
//     substituted it to (catches recursive lambda iter-2+ where
//     the bound w substitutes to a UOP graph not yet materialized).
//   - if child is TEN, short-circuit: emit SUP^tid(0, gy_for_child)
//     (or gy_for_child directly when target is set) via the leaf
//     rule.
//   - else allocate a grad cell [child, gy_for_child] and return
//     its DP1+grad_flag BWD projection (chain rule will recurse).
//
// Per-realize memoization on (child, gy, target):
// `interact_grad(uop, gy, target)` is deterministic in those three
// keys, but the chain rule's straight recursion re-walks shared
// sub-DAGs once per parent (visible in HotCounters as
// GradFires=54025 on LeNet's 5 grads = 10K interact_grad calls per
// target on a 65-node forward DAG).  This open-addressed gen-keyed
// table caches the BWD result so subsequent visits to the same
// (child, gy, target) triple share the existing grad cell instead
// of allocating a fresh one + re-firing the chain rule from scratch.
// Generation bumps once at thvm_realize entry so entries survive
// across the child BWD fires that make up one chain-rule walk, but
// cannot stale across separate user TGrad calls.
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

static u64 GRAD_MEMO_HITS  = 0;
static u64 GRAD_MEMO_MISSES = 0;

static int grad_memo_lookup(Term child, Term gy, Term target, Term *out) {
  u32 base = grad_memo_hash(child, gy, target);
  for (u32 probe = 0; probe < GRAD_MEMO_CAP; probe++) {
    GradMemoEntry *e = &GRAD_MEMO[(base + probe) & (GRAD_MEMO_CAP - 1)];
    if (e->gen != GRAD_MEMO_GEN) { GRAD_MEMO_MISSES++; return 0; }
    if (e->child == child && e->gy == gy && e->target == target) {
      GRAD_MEMO_HITS++;
      *out = e->result;
      return 1;
    }
  }
  GRAD_MEMO_MISSES++;
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
  // table full -- silently drop (memo is an optimization, not
  // load-bearing for correctness).
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

fn void grad_memo_begin_realize(void) {
  GRAD_MEMO_GEN++;
  if (GRAD_MEMO_GEN == 0) {
    GRAD_MEMO_GEN = 1;
    memset(GRAD_MEMO, 0, sizeof(GRAD_MEMO));
    memset(GRAD_DEP,  0, sizeof(GRAD_DEP));
  }
  GRAD_MEMO_HITS   = 0;
  GRAD_MEMO_MISSES = 0;
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
  Term ty = grad_alo_resolve(target);
  Term tt = grad_alo_resolve(t);
  if (ty == tt) {
    return 1;
  }
  Term tr = term_resolve(target);
  Term rr = term_resolve(t);
  return term_tag(tr) == TAG_TEN && term_tag(rr) == TAG_TEN
      && term_val(tr) == term_val(rr);
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

static Term grad_bwd_for_child(Term child, Term gy_for_child) {
  Term target = grad_current_target();
  if (target != 0 && !grad_depends_on_target(child, target)) {
    return uop_const(grad_target_dtype(), 0);
  }
  Term cached;
  if (grad_memo_lookup(child, gy_for_child, target, &cached)) return cached;

  Term result;
  if (target != 0) {
    Term ty = grad_alo_resolve(target);
    Term tc = grad_alo_resolve(child);
    if (ty == tc) { result = gy_for_child; goto done; }
  }
  if (term_tag(term_resolve(child)) == TAG_TEN) {
    result = grad_leaf_sup(child, gy_for_child);
    goto done;
  }
  {
    u64 c = grad_cell_alloc(child);
    heap_set(c + 1, gy_for_child);
    result = grad_bwd_of(c);
  }
done:
  grad_memo_insert(child, gy_for_child, target, result);
  return result;
}

static Term grad_accum(Term acc, Term add) {
  if (add == 0) return acc;
  if (acc == 0) return add;
  return uop_binary(UOP_ADD, acc, add);
}

// Helper: "ones at scalar" cotangent (used by RECIP/EXP2/etc. when
// they need to thread gy through a chain that doesn't depend on it).
// Note: in practice gy should always be passed in; this is a
// fallback for cases where the chain rule needs a fresh constant.
//
// Dtype passes through grad_target_dtype() so the resulting CONST
// materializes at the dtype the kernel reads (const_to_tendesc
// promotes the f32 bits to the target float at materialize time).
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

static Term grad_kernel_input_term(KernelEntry *ke, u32 slot) {
  if (slot >= ke->n_inputs) return 0;
  if (ke->input_terms[slot] != 0) return ke->input_terms[slot];
  u32 tid = ke->input_tids[slot];
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
}

static Term grad_kernel_src_term(KernelEntry *ke, Term *vals, u32 raw) {
  if (KSRC_IS_INPUT(raw)) return grad_kernel_input_term(ke, KSRC_INDEX(raw));
  u32 idx = KSRC_INDEX(raw);
  return idx < ke->n_ops ? vals[idx] : 0;
}

static void grad_kernel_accum_src(KernelEntry *ke, Term *adjs, Term *input_adjs,
                                  u32 raw, Term g) {
  if (g == 0) return;
  if (KSRC_IS_INPUT(raw)) {
    u32 idx = KSRC_INDEX(raw);
    if (idx < ke->n_inputs) input_adjs[idx] = grad_accum(input_adjs[idx], g);
  } else {
    u32 idx = KSRC_INDEX(raw);
    if (idx < ke->n_ops) adjs[idx] = grad_accum(adjs[idx], g);
  }
}

static int grad_kernel_reduce_axis(Term src, KProgOp const *p, u32 *axis_out) {
  Shape s;
  if (!term_shape_in(src, 0, &s) || s.ndim == 0) return 0;
  u32 inner = p->arg & 0x00FFFFFFu;
  u32 total = 1;
  for (u32 i = 0; i < s.ndim; i++) total *= s.dims[i];
  for (u32 axis = 0; axis < s.ndim; axis++) {
    u32 axis_inner = 1;
    for (u32 i = axis + 1; i < s.ndim; i++) axis_inner *= s.dims[i];
    u32 axis_size = s.dims[axis] ? s.dims[axis] : 1;
    if (axis_inner == inner && total / axis_size == p->numel) {
      *axis_out = axis;
      return 1;
    }
  }
  return 0;
}

static void grad_kernel_copy_widths(KProgOp const *p, u32 ndim, u32 *out) {
  for (u32 i = 0; i < 2 * ndim && i < 2 * MAX_DIM; i++) {
    out[i] = p->pad_widths[i];
  }
}

static Term grad_kernel_forward_op(KernelEntry *ke, Term *vals, u32 idx) {
  KProgOp const *p = &ke->program[idx];
  Term a = p->n_src > 0 ? grad_kernel_src_term(ke, vals, p->src[0]) : 0;
  Term b = p->n_src > 1 ? grad_kernel_src_term(ke, vals, p->src[1]) : 0;
  switch (p->opcode) {
    case UOP_CONST:
      return uop_const(p->dtype, p->arg);
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
      return (a != 0 && b != 0) ? uop_binary(p->opcode, a, b) : 0;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
      return a != 0 ? uop_unary(p->opcode, a) : 0;
    case UOP_LOAD:
      return a != 0 ? uop_load(a) : 0;
    case UOP_REDUCE: {
      u32 axis = 0;
      if (a == 0 || !grad_kernel_reduce_axis(a, p, &axis)) return 0;
      return uop_reduce((p->arg >> 24) & 0xFFu, axis, a);
    }
    case UOP_RESHAPE:
      return (a != 0 && p->out_ndim > 0)
           ? uop_reshape(a, p->out_ndim, p->out_dims) : 0;
    case UOP_EXPAND:
      return (a != 0 && p->out_ndim > 0)
           ? uop_expand(a, p->out_ndim, p->out_dims) : 0;
    case UOP_PERMUTE:
      if (a != 0 && p->out_ndim > 0) {
        u32 perm[MAX_DIM] = {0};
        for (u32 d = 0; d < p->out_ndim; d++) perm[d] = p->axis_perm[d];
        return uop_permute(a, p->out_ndim, perm);
      }
      return 0;
    case UOP_PAD: {
      if (a == 0 || p->out_ndim == 0) return 0;
      u32 widths[2 * MAX_DIM] = {0};
      grad_kernel_copy_widths(p, p->out_ndim, widths);
      return uop_pad(a, p->out_ndim, widths);
    }
    case UOP_SHRINK: {
      if (a == 0 || p->src0_ndim == 0) return 0;
      u32 ranges[2 * MAX_DIM] = {0};
      grad_kernel_copy_widths(p, p->src0_ndim, ranges);
      return uop_shrink(a, p->src0_ndim, ranges);
    }
    case UOP_FLIP:
      return a != 0 ? uop_flip(a, p->arg) : 0;
    case UOP_CAST:
      return a != 0 ? uop_cast(a, p->dtype) : 0;
    case UOP_BITCAST:
      return a != 0 ? uop_bitcast(a, p->dtype) : 0;
    default:
      return 0;
  }
}

static Term grad_kernel_backprop(KernelEntry *ke, Term gy) {
  if (ke == NULL || ke->n_ops == 0) return 0;
  Term *vals       = (Term *)calloc(ke->n_ops, sizeof(Term));
  Term *adjs       = (Term *)calloc(ke->n_ops, sizeof(Term));
  Term *input_adjs = (Term *)calloc(ke->n_inputs ? ke->n_inputs : 1, sizeof(Term));
  if (vals == NULL || adjs == NULL || input_adjs == NULL) {
    free(vals); free(adjs); free(input_adjs);
    return 0;
  }

  for (u32 i = 0; i < ke->n_ops; i++) {
    vals[i] = grad_kernel_forward_op(ke, vals, i);
    if (vals[i] == 0) {
      free(vals); free(adjs); free(input_adjs);
      return 0;
    }
  }
  adjs[ke->n_ops - 1] = gy;

  for (i32 ii = (i32)ke->n_ops - 1; ii >= 0; ii--) {
    u32 i = (u32)ii;
    Term g = adjs[i];
    if (g == 0) continue;
    KProgOp const *p = &ke->program[i];
    Term a = p->n_src > 0 ? grad_kernel_src_term(ke, vals, p->src[0]) : 0;
    Term b = p->n_src > 1 ? grad_kernel_src_term(ke, vals, p->src[1]) : 0;

    switch (p->opcode) {
      case UOP_ADD:
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0], g);
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[1], g);
        break;
      case UOP_MUL:
        if (b != 0) grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                                          uop_binary(UOP_MUL, b, g));
        if (a != 0) grad_kernel_accum_src(ke, adjs, input_adjs, p->src[1],
                                          uop_binary(UOP_MUL, a, g));
        break;
      case UOP_NEG:
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                              uop_unary(UOP_NEG, g));
        break;
      case UOP_RECIP:
        if (a != 0) {
          Term r1 = uop_unary(UOP_RECIP, a);
          Term r2 = uop_unary(UOP_RECIP, a);
          Term sq = uop_binary(UOP_MUL, r1, r2);
          Term ns = uop_unary(UOP_NEG, sq);
          grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                                uop_binary(UOP_MUL, g, ns));
        }
        break;
      case UOP_EXP2:
        if (a != 0) {
          Term ea = uop_unary(UOP_EXP2, a);
          Term factor = uop_binary(UOP_MUL, ea, grad_ln2_const());
          grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                                uop_binary(UOP_MUL, g, factor));
        }
        break;
      case UOP_LOG2:
        if (a != 0) {
          Term ra = uop_unary(UOP_RECIP, a);
          Term factor = uop_binary(UOP_MUL, ra, grad_inv_ln2_const());
          grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                                uop_binary(UOP_MUL, g, factor));
        }
        break;
      case UOP_SQRT:
        if (a != 0) {
          Term sa = uop_unary(UOP_SQRT, a);
          Term inv_sa = uop_unary(UOP_RECIP, sa);
          Term factor = uop_binary(UOP_MUL, grad_half_const(), inv_sa);
          grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                                uop_binary(UOP_MUL, g, factor));
        }
        break;
      case UOP_REDUCE: {
        Shape a_shape;
        u32 axis = 0;
        if (a == 0 || !term_shape_in(a, 0, &a_shape) || a_shape.ndim == 0
            || !grad_kernel_reduce_axis(a, p, &axis)) break;
        u32 keep_dims[MAX_DIM] = {0};
        if (a_shape.ndim == 1) {
          keep_dims[0] = 1;
        } else {
          for (u32 d = 0; d < a_shape.ndim; d++)
            keep_dims[d] = (d == axis) ? 1u : a_shape.dims[d];
        }
        Term g_keep = uop_reshape(g, a_shape.ndim, keep_dims);
        Term g_lift = uop_expand(g_keep, a_shape.ndim, a_shape.dims);
        u32 kind = (p->arg >> 24) & 0xFFu;
        if (kind == REDUCE_MAX) {
          Term mx = uop_reduce(REDUCE_MAX, axis, a);
          Term mx_keep = uop_reshape(mx, a_shape.ndim, keep_dims);
          Term mx_lift = uop_expand(mx_keep, a_shape.ndim, a_shape.dims);
          Term mask = uop_binary(UOP_CMPEQ, a, mx_lift);
          g_lift = uop_binary(UOP_MUL, mask, g_lift);
        }
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0], g_lift);
        break;
      }
      case UOP_RESHAPE: {
        Shape src_shape;
        if (a != 0 && term_shape_in(a, 0, &src_shape) && src_shape.ndim > 0) {
          grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                                uop_reshape(g, src_shape.ndim, src_shape.dims));
        }
        break;
      }
      case UOP_EXPAND: {
        Shape a_shape;
        if (a == 0 || !term_shape_in(a, 0, &a_shape) || a_shape.ndim == 0
            || p->out_ndim < a_shape.ndim) break;
        u32 pad = p->out_ndim - a_shape.ndim;
        u32 a_padded[MAX_DIM] = {0};
        for (u32 d = 0; d < pad; d++) a_padded[d] = 1;
        for (u32 d = 0; d < a_shape.ndim; d++) a_padded[pad + d] = a_shape.dims[d];
        Term cur = g;
        for (i32 axis = (i32)p->out_ndim - 1; axis >= 0; axis--) {
          if (a_padded[axis] == 1 && p->out_dims[axis] > 1) {
            cur = uop_reduce(REDUCE_SUM, (u32)axis, cur);
          }
        }
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                              uop_reshape(cur, a_shape.ndim, a_shape.dims));
        break;
      }
      case UOP_PERMUTE: {
        if (p->out_ndim == 0) break;
        u32 inv[MAX_DIM] = {0};
        for (u32 d = 0; d < p->out_ndim; d++) inv[p->axis_perm[d]] = d;
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                              uop_permute(g, p->out_ndim, inv));
        break;
      }
      case UOP_PAD: {
        Shape a_shape;
        if (a == 0 || !term_shape_in(a, 0, &a_shape) || a_shape.ndim == 0) break;
        u32 ranges[2 * MAX_DIM] = {0};
        for (u32 d = 0; d < a_shape.ndim; d++) {
          ranges[2 * d + 0] = p->pad_widths[2 * d + 0];
          ranges[2 * d + 1] = p->pad_widths[2 * d + 0] + a_shape.dims[d];
        }
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                              uop_shrink(g, a_shape.ndim, ranges));
        break;
      }
      case UOP_SHRINK: {
        Shape a_shape;
        if (a == 0 || !term_shape_in(a, 0, &a_shape) || a_shape.ndim == 0) break;
        u32 widths[2 * MAX_DIM] = {0};
        for (u32 d = 0; d < a_shape.ndim; d++) {
          u32 b0 = p->pad_widths[2 * d + 0];
          u32 e0 = p->pad_widths[2 * d + 1];
          widths[2 * d + 0] = b0;
          widths[2 * d + 1] = a_shape.dims[d] > e0 ? a_shape.dims[d] - e0 : 0;
        }
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                              uop_pad(g, a_shape.ndim, widths));
        break;
      }
      case UOP_FLIP:
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                              uop_flip(g, p->arg));
        break;
      case UOP_LOAD:
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0], g);
        break;
      case UOP_CAST: {
        u32 src_dtype = DT_FP32;
        if (a != 0) term_dtype_in(a, 0, &src_dtype);
        grad_kernel_accum_src(ke, adjs, input_adjs, p->src[0],
                              uop_cast(g, src_dtype));
        break;
      }
      case UOP_CONST:
      case UOP_CMPLT:
      case UOP_CMPEQ:
      case UOP_BITCAST:
      default:
        break;
    }
  }

  Term total = 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (input_adjs[i] == 0) continue;
    Term in = grad_kernel_input_term(ke, i);
    if (in == 0) continue;
    total = grad_accum(total, grad_bwd_for_child(in, input_adjs[i]));
  }
  free(vals);
  free(adjs);
  free(input_adjs);
  return total != 0 ? total : uop_const(grad_target_dtype(), 0);
}

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

// Inner dispatch: runs the actual chain-rule rewrite.  The outer
// `interact_grad` wraps this with the push/pop of the target stack
// so grad_cell_alloc / grad_leaf_sup see the right current target
// during sub-cell allocation.
static Term interact_grad_dispatch(Term grad_term) {
  u64  cell_orig = term_val(grad_term);
  Term y         = term_resolve(heap_read(cell_orig + 0));
  // Resolve gy through ALO chains: each lambda iter wraps the
  // grad cell's children in fresh ALOs (alo_suspend_child), so a
  // bare heap_read gives back distinct ALO Terms across iters
  // even when they all force to the same hash-consed CONST seed.
  // Resolving here lets uop_reshape / uop_expand hit the movement-
  // op cache, sharing the gy_lifted UOP graph across iters and
  // collapsing what would otherwise be N identical "ones-at-shape"
  // kernels into one.
  Term gy        = term_resolve(heap_read(cell_orig + 1));
  Term target    = heap_read(cell_orig + 2);

  u8 y_tag = term_tag(y);

  // === LEAF: y is a TEN ===
  // Same two-path treatment as grad_leaf_sup: when target is
  // present, do direct tid-match (gy on match, scalar zero on
  // mismatch); when target == 0, emit SUP^{y.tid}(CONST(0), gy)
  // for the surrounding WL DUP nest to project.
  // Variable-identity match: if y (raw, before term_resolve)
  // equals target's alo-resolved form, this whole sub-cell is
  // sitting at the bound variable -- gy is the gradient.  Catches
  // the recursive-lambda case where y is TVAR(dyn_w) before SUB
  // (which would point to a yet-unmaterialized UOP graph).
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
          Term kgrad = grad_kernel_backprop(&KERNELS[kid], gy);
          if (kgrad != 0) return kgrad;
        }
      }
      return uop_const(grad_target_dtype(), 0);
    }
    u32 tid = (u32)term_val(y);
    u32 leaf_dt = DT_FP32;
    term_dtype_in(y, 0, &leaf_dt);
    u64 sloc = heap_alloc(2);
    heap_set(sloc + 0, uop_const(leaf_dt, 0));   // scalar 0, broadcasts in any ADD/MUL
    heap_set(sloc + 1, gy);
    return term_new(0, TAG_SUP, tid, sloc);
  }

  // NUM: constant input -- no leaf contribution; cotangent dies.
  if (y_tag == TAG_NUM) {
    return uop_const(grad_target_dtype(), 0);
  }

  if (y_tag != TAG_UOP) return grad_term;

  u8  y_op  = term_ext(y);
  u64 y_loc = term_val(y);

  switch (y_op) {

    // === Elementwise binary ===
    // ADD(a, b): both children get gy unchanged.  We share gy across
    // both sub-cells by raw heap-loc reference -- materialize dedups
    // when both sides are realized.
    case UOP_ADD: {
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term a_bwd = grad_bwd_for_child(a, gy);
      Term b_bwd = grad_bwd_for_child(b, gy);
      return uop_binary(UOP_ADD, a_bwd, b_bwd);
    }

    // MUL(a, b): chicken-and-egg between the two adjoints (gy_for_a
    // needs b_fwd; gy_for_b needs a_fwd).  Allocate both cells with
    // placeholder gy first, build FWD references, compose adjoints,
    // then patch the gy slots.
    case UOP_MUL: {
      Term a = heap_read(y_loc + 0);
      Term b = heap_read(y_loc + 1);
      Term a_resolved = term_resolve(a);
      Term b_resolved = term_resolve(b);
      u8   a_is_ten   = term_tag(a_resolved) == TAG_TEN;
      u8   b_is_ten   = term_tag(b_resolved) == TAG_TEN;
      // Cross-references.  When the sibling is TEN, use it directly
      // (no DP0 wrapper needed).  Else we need its grad cell's FWD
      // projection -- allocated below with a placeholder gy, patched
      // after we build the gy expressions.
      u64 ca = 0, cb = 0;
      Term a_fwd = a_resolved, b_fwd = b_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      if (!b_is_ten) { cb = grad_cell_alloc(b); b_fwd = grad_fwd_of(cb); }
      Term gy_a = uop_binary(UOP_MUL, b_fwd, gy);
      Term gy_b = uop_binary(UOP_MUL, a_fwd, gy);
      // BWD references.  TEN children short-circuit to leaf SUPs;
      // others use the cells above (whose gy slot we patch now).
      Term a_bwd, b_bwd;
      if (a_is_ten) {
        a_bwd = grad_leaf_sup(a_resolved, gy_a);
      } else {
        heap_set(ca + 1, gy_a);
        a_bwd = grad_bwd_of(ca);
      }
      if (b_is_ten) {
        b_bwd = grad_leaf_sup(b_resolved, gy_b);
      } else {
        heap_set(cb + 1, gy_b);
        b_bwd = grad_bwd_of(cb);
      }
      return uop_binary(UOP_ADD, a_bwd, b_bwd);
    }

    // CMPLT/CMPEQ: non-differentiable; cotangent dies.
    case UOP_CMPLT: case UOP_CMPEQ: {
      return grad_zero_at(y);
    }

    // === Elementwise unary ===
    case UOP_NEG: {
      Term a = heap_read(y_loc + 0);
      Term gy_a = uop_unary(UOP_NEG, gy);
      return grad_bwd_for_child(a, gy_a);
    }

    case UOP_LOG2: {
      // d(log2 a)/da = 1/(a*ln2) ; gy_for_a = gy * RECIP(a) * (1/ln2)
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
      if (a_is_ten) return grad_leaf_sup(a_resolved, gy_a);
      heap_set(ca + 1, gy_a);
      return grad_bwd_of(ca);
    }

    case UOP_EXP2: {
      // d(2^a)/da = 2^a * ln2 ; gy_for_a = gy * EXP2(a) * ln2
      Term a = heap_read(y_loc + 0);
      Term a_resolved = term_resolve(a);
      u8 a_is_ten = term_tag(a_resolved) == TAG_TEN;
      u64 ca = 0;
      Term a_fwd = a_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      Term ea     = uop_unary(UOP_EXP2, a_fwd);
      Term k      = grad_ln2_const();
      Term factor = uop_binary(UOP_MUL, ea, k);
      Term gy_a   = uop_binary(UOP_MUL, gy, factor);
      if (a_is_ten) return grad_leaf_sup(a_resolved, gy_a);
      heap_set(ca + 1, gy_a);
      return grad_bwd_of(ca);
    }

    case UOP_RECIP: {
      // d(1/a)/da = -1/a^2 ; gy_for_a = gy * NEG(RECIP(a)*RECIP(a))
      Term a = heap_read(y_loc + 0);
      Term a_resolved = term_resolve(a);
      u8 a_is_ten = term_tag(a_resolved) == TAG_TEN;
      u64 ca = 0;
      Term a_fwd = a_resolved;
      if (!a_is_ten) { ca = grad_cell_alloc(a); a_fwd = grad_fwd_of(ca); }
      // Two independent RECIP nodes for diagram clarity (materialize
      // dedups by heap loc; structurally separate is intentional).
      Term r1 = uop_unary(UOP_RECIP, a_fwd);
      Term r2 = uop_unary(UOP_RECIP, a_fwd);
      Term sq = uop_binary(UOP_MUL, r1, r2);
      Term ns = uop_unary(UOP_NEG, sq);
      Term gy_a = uop_binary(UOP_MUL, gy, ns);
      if (a_is_ten) return grad_leaf_sup(a_resolved, gy_a);
      heap_set(ca + 1, gy_a);
      return grad_bwd_of(ca);
    }

    case UOP_SQRT: {
      // d(sqrt a)/da = 1/(2*sqrt a) ; gy_for_a = gy * 0.5 * RECIP(SQRT(a))
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
      if (a_is_ten) return grad_leaf_sup(a_resolved, gy_a);
      heap_set(ca + 1, gy_a);
      return grad_bwd_of(ca);
    }

    // === REDUCE ===
    // SUM: gy_for_a = EXPAND(reshape_keepdim(gy), a.shape).
    // MAX: gy_for_a = MUL(mask_at_argmax, EXPAND(reshape_keepdim(gy), a.shape)).
    case UOP_REDUCE: {
      Term a    = heap_read(y_loc + 0);
      u32  kind = (u32)term_val(heap_read(y_loc + 1));
      u32  axis = (u32)term_val(heap_read(y_loc + 2));

      Shape a_shape;
      if (!term_shape_in(a, 0, &a_shape) || a_shape.ndim == 0) {
        return grad_term;   // can't determine src shape; bail
      }

      // Build reshape-keepdim of gy: insert a 1 at `axis` so the
      // EXPAND back to a.shape is well-formed.  When a.ndim == 1,
      // the reduce output shape is {1} (per uop_meta), so a
      // RESHAPE to {1} is identity-ish; we still call it to be
      // explicit about the intermediate.
      u32 keep_dims[MAX_DIM] = {0};
      if (a_shape.ndim == 1) {
        // a.shape = {N}, y.shape = {1}, keepdim = {1}; expand back to {N}.
        keep_dims[0] = 1;
      } else {
        for (u32 i = 0; i < a_shape.ndim; i++)
          keep_dims[i] = (i == axis) ? 1u : a_shape.dims[i];
      }
      Term gy_keepdim = uop_reshape(gy, a_shape.ndim, keep_dims);
      Term gy_lifted  = uop_expand(gy_keepdim, a_shape.ndim, a_shape.dims);

      Term gy_a;
      u64  ca = grad_cell_alloc(a);
      Term a_fwd = grad_fwd_inline(ca, a);
      if (kind == REDUCE_SUM) {
        gy_a = gy_lifted;
      } else if (kind == REDUCE_MAX) {
        // mask = (a == lift(MAX(a, axis)))
        Term mx        = uop_reduce(REDUCE_MAX, axis, a_fwd);
        Term mx_keep   = uop_reshape(mx, a_shape.ndim, keep_dims);
        Term mx_lifted = uop_expand(mx_keep, a_shape.ndim, a_shape.dims);
        Term mask      = uop_binary(UOP_CMPEQ, a_fwd, mx_lifted);
        gy_a           = uop_binary(UOP_MUL, mask, gy_lifted);
      } else {
        // Unknown reduce kind: pass gy through unchanged (best effort).
        gy_a = gy_lifted;
      }
      heap_set(ca + 1, gy_a);
      return grad_bwd_of(ca);
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
      if (term_shape_in(a, 0, &a_shape) && a_shape.ndim > 0
          && ndim >= a_shape.ndim) {
        // Pad a's shape with leading 1s up to out's ndim so we can
        // detect implicit-broadcast axes (rank increase).  E.g.
        // EXPAND({4} -> {3, 4}) broadcasts axis 0 (implicit 1->3).
        u32 pad = ndim - a_shape.ndim;
        u32 a_padded[MAX_DIM] = {0};
        for (u32 i = 0; i < pad; i++) a_padded[i] = 1;
        for (u32 i = 0; i < a_shape.ndim; i++) a_padded[pad + i] = a_shape.dims[i];

        // Walk axes high-to-low; each REDUCE_SUM drops that axis.
        // After all reduces, cur has rank a_shape.ndim and matches
        // a.shape (no reshape needed -- reduces preserve non-broadcast
        // dims and drop broadcast ones).
        Term cur = gy;
        for (i32 axis = (i32)ndim - 1; axis >= 0; axis--) {
          if (a_padded[axis] == 1 && out_dims[axis] > 1) {
            cur = uop_reduce(REDUCE_SUM, (u32)axis, cur);
          }
        }
        // If a had explicit dim==1 axes that we DIDN'T reduce (because
        // out.dim was also 1), reduces leave them; rank still matches
        // a_shape.ndim.  Reshape to a_shape to be defensive about any
        // residual rank-1 collapse from the REDUCE-of-{N}->{1} case.
        if (a_shape.ndim == 1 && a_shape.dims[0] == 1) {
          // Special: a was {1}; cur is {1} after possible reduces.
          gy_a = uop_reshape(cur, 1, a_shape.dims);
        } else {
          gy_a = uop_reshape(cur, a_shape.ndim, a_shape.dims);
        }
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

      Term gy_a;
      if (y_op == UOP_PAD) {
        // PAD output dim_i = a.dim_i + b_i + e_i; gy has output shape.
        // gy_for_a = SHRINK(gy, [b_i, b_i + a.dim_i)).
        u32 sranges[2 * MAX_DIM];
        for (u32 i = 0; i < ndim; i++) {
          sranges[2 * i + 0] = ranges[2 * i + 0];
          sranges[2 * i + 1] = ranges[2 * i + 0] + a_shape.dims[i];
        }
        gy_a = uop_shrink(gy, ndim, sranges);
      } else {
        // SHRINK output dim_i = e_i - b_i; gy has output shape.
        // gy_for_a = PAD(gy, [b_i, a.dim_i - e_i]) restoring full extent.
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
      // KERNEL: chain rule should run on the pre-kernelize source UOp,
      // recovered from KernelEntry.source_uop.
      u32 kid = (u32)term_val(heap_read(y_loc + 1));
      if (kid == 0 || kid >= KERNELS_NEXT) {
        return grad_zero_at(y);
      }
      Term kgrad = grad_kernel_backprop(&KERNELS[kid], gy);
      if (kgrad != 0) {
        return kgrad;
      }
      Term src = KERNELS[kid].source_uop;
      if (src == 0) {
        return grad_zero_at(y);
      }
      return grad_bwd_for_child(src, gy);
    }

    case UOP_CAST: {
      // d/dx cast(x, dt) = cast(gy, x.dtype) -- value-preserving so
      // the chain rule passes the cotangent back through the source
      // dtype.  Mirrors tinygrad/gradient.py:17.
      Term src = heap_read(y_loc);
      u32 src_dtype = DT_FP32;
      term_dtype_in(src, 0, &src_dtype);
      Term gy_in = uop_cast(gy, src_dtype);
      return grad_bwd_for_child(src, gy_in);
    }
    case UOP_BITCAST:
      // BITCAST has no value-preserving gradient (the bit
      // reinterpretation isn't differentiable).  Return zero per
      // tinygrad/gradient.py:42.
      return grad_zero_at(y);

    case UOP_CONST:
    case UOP_LOAD:
    case UOP_ASSIGN:
      // Not differentiable -- bw is zero.
      return grad_zero_at(y);

    default:
      return grad_term;
  }
}

// Public entry: read this cell's target and push it on the per-fire
// stack so any sub-cell that grad_cell_alloc creates during the
// dispatch inherits it.  Pop on exit, even if dispatch bails (the
// caller may retry later, expecting a clean stack).
// Forward decl: transitive active check needs to recurse into
// UOP children, but is itself called from uop_child_is_active.
static u8 uop_has_active_descendant_memo(Term t);

// Per-call memoization for the transitive active scan: shared
// sub-DAGs (e.g. gT referenced by gT*gT and 0.1*gT in Adam) would
// otherwise walk exponentially.  Loc-keyed bit-bucket cleared by
// the outer entry point each time wnf re-enters a UOP head.
//
// Generation-counter scheme: outer call bumps gen; each cell stores
// (gen, value).  A stale gen means uncached.  Cap is power-of-two
// for AND-mask indexing.
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

// Is `t` a child slot worth driving when we hit a UOP-WHNF
// inflection?  Active forms are the ones whose own interaction has
// to fire to reach WHNF:
//
//   - DP0/DP1              chain-rule projection (interact_grad)
//                          or regular DUP commute (interact_dup_X)
//   - UOP_KERNEL           kernel dispatch (interact_kernel)
//   - UOP_ASSIGN           in-place buffer write (interact_assign_with)
//
// Plus TRANSITIVE actives: a passive UOP (ADD/MUL/REDUCE/...) whose
// subgraph contains any of the above.  Without this, a deeply-
// nested chain-rule grad like `ADD(MUL(x, GRAD), ...)` would never
// be reached: the immediate child is MUL (passive), but MUL hides
// an active GRAD descendant.
//
// Used by wnf's UOP enter/apply path (TAG_F_UOP_CHILD frames) to
// scan a UOP's children for the next slot worth descending into.
fn u8 uop_child_is_active(Term t) {
  // Pre-resolve check: term_resolve eagerly unwraps a TAG_DP0 with
  // DUP_GRAD_FLAG to its cell[0] body (so the chain rule can pattern-
  // match through the FWD wrapper).  We must NOT let that unwrap hide
  // the DP0 from this scan -- if the surrounding UOP has a DP0+grad
  // child, wnf needs to descend and fire it (`heap_read(loc)` indirect
  // dereference at wnf entry).  Without this, a MUL adjoint's
  // `gy_for_a = b_fwd * gy` where b_fwd = DP0(grad cell of b) is
  // reported inactive once interact_grad fills the cell with b, and
  // the DP0 wrapper survives unreduced in the result -- materialize
  // then bails on it.  Reproduces on TGrad through TBatchNormTrain
  // (BN backward through centered*centered triggers this MUL fanout).
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
  // A uop_next_active_child scan checks sibling slots that often
  // share deep descendants.  Keep one generation open for that scan
  // so those descendants are walked once; standalone probes still
  // get an isolated generation.
  u8 outer = (UOP_ACT_MEMO_DEPTH == 0 && UOP_ACT_SCAN_DEPTH == 0);
  if (outer) uop_act_memo_next_gen();
  UOP_ACT_MEMO_DEPTH++;
  // Open-addressed probe; visiting flag breaks self-cycles (rare
  // but possible via shared sub-DAGs).
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
  // Table full -- conservative answer.
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

  // Shape-pad to target.shape only at the OUTERMOST grad fire
  // (pushed && GRAD_TARGET_TOP == 0 after the pop above).  Inner
  // recursive sub-grads return per-leaf cotangent expressions
  // whose shape is the leaf's, not the user's target shape; we
  // only need padding on the user-visible result.  Without this
  // gate every recursive grad cell wraps, multiplying ADD-of-
  // EXPAND(0) kernels across the chain rule.
  if (target != 0 && pushed && GRAD_TARGET_TOP == 0) {
    Term tr = term_resolve(target);
    if (term_tag(tr) == TAG_TEN) {
      Shape tshape, rshape;
      // Pad ONLY on a confirmed mismatch: both shape inferences
      // succeed AND ndim/dims differ.  When shape inference fails on
      // the result (common for compound UOP graphs), we assume the
      // chain rule produced something at target.shape by construction
      // -- adding the EXPAND(0)+ADD wrap unconditionally would
      // introduce an extra kernel per outermost grad fire (visible
      // in fusion-count and bound-w training-loop tests).
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
