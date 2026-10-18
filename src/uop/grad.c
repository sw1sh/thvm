// A grad cell is a dup-style cell [y, gy, target]: its DP0 projection passes y
// through, its DP1 fires the chain rule (interact_grad), and DUP_GRAD_FLAG on
// the ext makes wnf and redex_fire dispatch them as grad projections.

// A nonzero `target` (a TVAR in a TLam body, say) runs the target walk: gy at
// the leaves it matches and zero elsewhere, so TGrad needs no leaf tids, which
// a TLam body lacks before substitution.
fn u64 uop_grad_cell(Term y, Term gy, Term target) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, y);
  heap_set(loc + 1, gy);
  heap_set(loc + 2, target);
  return loc;
}

// Defined in interact/uop_grad.c, which src/thvm.c includes after this file.
fn void grad_memo_begin_realize(void);
fn void grad_prewalk_fanin(Term y);

fn Term uop_grad(Term y, Term gy) {
  // Bump the slot generation, then count each node's BWD fan-in (the single
  // walk only) for wnf's chain rule, which fires a node once all parents
  // arrive (tinygrad compute_gradient, mixin/gradient.py:116).
  grad_memo_begin_realize();
  Term bwd = term_new(0, TAG_DP1, DUP_GRAD_FLAG, uop_grad_cell(y, gy, 0));
  grad_prewalk_fanin(y);
  return bwd;
}

fn Term uop_fwd(Term y, Term gy) {
  return term_new(0, TAG_DP0, DUP_GRAD_FLAG, uop_grad_cell(y, gy, 0));
}

fn Term uop_grad_with_target(Term y, Term gy, Term target) {
  return term_new(0, TAG_DP1, DUP_GRAD_FLAG, uop_grad_cell(y, gy, target));
}

