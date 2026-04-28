// (lam x.body) arg
// ---------------- APP-LAM
// x <- arg
// body
//
// Read the lambda body, install `arg` as the substitution at the
// binder's heap cell (so a future VAR enter on that loc picks it up
// and clears the SUB flag), then continue reducing the body.
//
// JIT-style: when the lambda body is a UOP graph (compute) and
// the argument carries a shape (TEN or shape-inferable UOP),
// register the bound var's shape and materialize the body into
// a UOP_KERNEL before the standard beta.  The kernel's TVAR
// input slot resolves through SUB to `arg` at fire time.
//
// Why this is safe to do for every lambda (no opt-in flag):
//   - Bodies that aren't TAG_UOP (TLam, TApp, TMat, ...) are
//     skipped -- materialize on a structural lambda is a no-op
//     anyway.
//   - Already-compiled bodies (TAG_UOP + UOP_KERNEL) skip the
//     re-materialize step.
//   - When `arg` carries no shape (TLam, TNum, TEra, ...) we
//     fall through to plain beta unchanged.
//   - The kernel-program hash-cons cache (c83c29b) deduplicates
//     identical programs across iters of a recursive lambda, so
//     the per-instance materialize cost is bounded.
fn Term interact_app_lam(Term lam, Term arg) {
  ITRS++;
  u64  loc  = term_val(lam);
  Term body = heap_read(loc);
  if (term_tag(body) == TAG_UOP && term_ext(body) != UOP_KERNEL) {
    Shape s;
    if (term_shape_in(arg, 0, &s) && s.ndim > 0) {
      lam_shape_set(loc, &s);
      body = thvm_materialize(body);
    }
  }
  heap_subst_var(loc, arg);
  return body;
}
