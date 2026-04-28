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

fn Term grad_zero_at(Term y) {
  return expand_to_target(uop_const(DT_F32, 0), y);
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
typedef struct { Term fwd; Term bwd; u64 loc; } GradPair;
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
static Term grad_leaf_sup(Term ten, Term gy_for_leaf) {
  // Resolve `ten` defensively: callers either pass an already-
  // resolved Term (MUL/RECIP/etc.) or the raw heap_read child
  // (grad_bwd_for_child).  Reading term_val on an unresolved VAR
  // gives the var-loc, not the substituted TEN's tid -- a tid
  // comparison against `target` would silently mismatch.
  Term ten_r  = term_resolve(ten);
  if (term_tag(ten_r) != TAG_TEN) ten_r = ten;   // best effort; caller guarantees TEN
  Term target = grad_current_target();
  if (target != 0) {
    // Drive target to WHNF before comparing: in a recursive lambda
    // (sgd_loop) the bound variable is substituted to step(w0) --
    // a UOP graph that hasn't materialized yet.  term_resolve only
    // peels VAR/ALO indirection; we need wnf (or interact_kernel)
    // to drive any pending compute so the target becomes a TEN.
    Term tr = term_resolve(target);
    if (term_tag(tr) != TAG_TEN) tr = term_resolve(wnf(tr));
    if (term_tag(tr) == TAG_TEN && term_val(tr) == term_val(ten_r)) {
      return gy_for_leaf;
    }
    return uop_const(DT_F32, 0);          // mismatch -> scalar zero
  }
  u32 tid = (u32)term_val(ten_r);
  u64 sloc = heap_alloc(2);
  heap_set(sloc + 0, uop_const(DT_F32, 0));
  heap_set(sloc + 1, gy_for_leaf);
  return term_new(0, TAG_SUP, tid, sloc);
}

// Build the BWD reference for a child:
//   - if child is TEN, short-circuit: emit SUP^tid(0, gy_for_child)
//     directly (no grad cell needed, no BWD trigger needed).
//   - else allocate a grad cell [child, gy_for_child] and return
//     its DP1+grad_flag BWD projection (chain rule will recurse).
static Term grad_bwd_for_child(Term child, Term gy_for_child) {
  if (term_tag(term_resolve(child)) == TAG_TEN) {
    return grad_leaf_sup(child, gy_for_child);
  }
  u64 c = grad_cell_alloc(child);
  heap_set(c + 1, gy_for_child);
  return grad_bwd_of(c);
}

// One-shot helper: allocate cell, write gy, return {fwd, bwd, loc}.
static GradPair grad_pair_with_gy(Term child, Term gy_for_child) {
  u64 c = grad_cell_alloc(child);
  heap_set(c + 1, gy_for_child);
  GradPair p = { grad_fwd_of(c), grad_bwd_of(c), c };
  return p;
}

// Helper: "ones at scalar" cotangent (used by RECIP/EXP2/etc. when
// they need to thread gy through a chain that doesn't depend on it).
// Note: in practice gy should always be passed in; this is a
// fallback for cases where the chain rule needs a fresh constant.
fn Term grad_ln2_const(void) {
  f32 ln2 = 0.6931471805599453f;
  u32 bits;
  memcpy(&bits, &ln2, sizeof bits);
  return uop_const(DT_F32, bits);
}
fn Term grad_inv_ln2_const(void) {
  f32 inv = 1.4426950408889634f;
  u32 bits;
  memcpy(&bits, &inv, sizeof bits);
  return uop_const(DT_F32, bits);
}
fn Term grad_half_const(void) {
  f32 half = 0.5f;
  u32 bits;
  memcpy(&bits, &half, sizeof bits);
  return uop_const(DT_F32, bits);
}

// Inner dispatch: runs the actual chain-rule rewrite.  The outer
// `interact_grad` wraps this with the push/pop of the target stack
// so grad_cell_alloc / grad_leaf_sup see the right current target
// during sub-cell allocation.
static Term interact_grad_dispatch(Term grad_term) {
  u64  cell_orig = term_val(grad_term);
  Term y         = term_resolve(heap_read(cell_orig + 0));
  Term gy        = heap_read(cell_orig + 1);
  Term target    = heap_read(cell_orig + 2);

  u8 y_tag = term_tag(y);

  // === LEAF: y is a TEN ===
  // Same two-path treatment as grad_leaf_sup: when target is
  // present, do direct tid-match (gy on match, scalar zero on
  // mismatch); when target == 0, emit SUP^{y.tid}(CONST(0), gy)
  // for the surrounding WL DUP nest to project.
  if (y_tag == TAG_TEN) {
    if (target != 0) {
      Term tr = term_resolve(target);
      if (term_tag(tr) != TAG_TEN) tr = term_resolve(wnf(tr));
      if (term_tag(tr) == TAG_TEN && term_val(tr) == term_val(y)) {
        return gy;
      }
      return uop_const(DT_F32, 0);
    }
    u32 tid = (u32)term_val(y);
    u64 sloc = heap_alloc(2);
    heap_set(sloc + 0, uop_const(DT_F32, 0));   // scalar 0, broadcasts in any ADD/MUL
    heap_set(sloc + 1, gy);
    return term_new(0, TAG_SUP, tid, sloc);
  }

  // NUM: constant input -- no leaf contribution; cotangent dies.
  if (y_tag == TAG_NUM) {
    return uop_const(DT_F32, 0);
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
        perm[i] = (u32)term_val(heap_read(y_loc + 1 + i));
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
        ranges[2 * i + 0] = (u32)term_val(heap_read(y_loc + 1 + 2 * i));
        ranges[2 * i + 1] = (u32)term_val(heap_read(y_loc + 2 + 2 * i));
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
      Term src = KERNELS[kid].source_uop;
      if (src == 0) {
        return grad_zero_at(y);
      }
      return grad_bwd_for_child(src, gy);
    }

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
fn Term interact_grad(Term grad_term) {
  Term target = heap_read(term_val(grad_term) + 2);
  u8 pushed = 0;
  if (GRAD_TARGET_TOP < GRAD_TARGET_STACK_CAP) {
    GRAD_TARGET_STACK[GRAD_TARGET_TOP++] = target;
    pushed = 1;
  }
  Term r = interact_grad_dispatch(grad_term);
  if (pushed) GRAD_TARGET_TOP--;
  return r;
}
