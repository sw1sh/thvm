// uop/grad.c - construct dup-flavored grad-cell projections.
//
// A grad cell is a regular dup-style cell holding [y, gy].  Its two
// aux ports are TAG_DP0 (forward projection: passthrough to y) and
// TAG_DP1 (backward projection: chain-rule rewrite via interact_grad).
// We share the IC's DUP/DP0/DP1 machinery; the DUP_GRAD_FLAG bit on
// the projection's ext (label) tells redex_fire / wnf to dispatch
// the grad-flavored interaction instead of the regular DUP-X
// dispatch.
//
// Mirrors a 3-port combinator with one principal port (cell.y) and
// two aux ports (FWD, BWD), exactly the dup-like structure HVM4
// gives DUP -- just with grad semantics on the aux ports and a
// cotangent (gy) threaded alongside y for the BWD chain rule.

// Cell layout: [y, gy, target_or_zero].
//
// `target` is optional (0 = use the SUP/DUP scaffolding, the WL
// surface wraps with TDup^t.tid to project per-target).  When the
// caller passes a non-zero `target` Term (typically a TVAR captured
// inside a TLam body), the chain rule's leaf-handler does a direct
// tid-equality match against target -- emitting gy at matching
// leaves, scalar zero elsewhere -- without needing the WL DUP nest.
// This makes TGrad work inside TLam bodies where leafTids can't be
// discovered statically pre-substitution.
fn u64 uop_grad_cell(Term y, Term gy, Term target) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, y);
  heap_set(loc + 1, gy);
  heap_set(loc + 2, target);
  return loc;
}

fn Term uop_grad(Term y, Term gy) {
  return term_new(0, TAG_DP1, DUP_GRAD_FLAG, uop_grad_cell(y, gy, 0));
}

fn Term uop_fwd(Term y, Term gy) {
  return term_new(0, TAG_DP0, DUP_GRAD_FLAG, uop_grad_cell(y, gy, 0));
}

fn Term uop_grad_with_target(Term y, Term gy, Term target) {
  return term_new(0, TAG_DP1, DUP_GRAD_FLAG, uop_grad_cell(y, gy, target));
}
