// (lam x.body) arg
// ---------------- APP-LAM
// x <- arg
// body
//
// Read the lambda body, install `arg` as the substitution at the
// binder's heap cell (so a future VAR enter on that loc picks it up
// and clears the SUB flag), then continue reducing the body.
//
// LAM_ERA_MASK fast-path (HVM4 wnf/app_lam.c:10):
//   When the bit is set on the LAM's ext, the body has no VAR(loc)
//   reference so heap_subst_var would write a substitute that nothing
//   reads.  We skip the write and return body directly.  The bit is
//   set at construction by lam_body_uses_var (src/lam/body_uses_var.c),
//   called from every LAM-construction site (DUP-LAM, APP-BRI,
//   ANN-LAM, alo_realize, book/from_dynamic, the WL bridge's
//   thvm_wl_term_new shim).
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
  u32  lam_ext = term_ext(lam);
  u64  loc     = term_val(lam);
  Term body    = heap_read(loc);
  if (lam_ext & LAM_ERA_MASK) {
    // Binder unused: skip both shape-annotation and heap_subst_var;
    // the former wouldn't be observed by any TVAR(loc), the latter
    // would dangle.  We still run the JIT-materialize step because
    // its job (compiling a UOP body to a KERNEL) is independent of
    // whether the binder is referenced.
    if (term_tag(body) == TAG_UOP && term_ext(body) != UOP_KERNEL) {
      body = thvm_materialize(body);
    }
    return body;
  }
  // Always shape-annotate the bound var when the argument
  // carries a shape -- even for curried / structural-bodied
  // lambdas.  The annotation propagates through nested LAMs:
  // outer's `App(λw.λn.body, w0)` annotates w_loc; inner's
  // `App(λn.body, NUM(N))` doesn't (NUM has no shape) but
  // the deeper compute body's TVAR(w_loc) has a known shape
  // by the time materialize gets to it.  Cheap insert; helps
  // every later visit() / chain-rule pass that walks the body.
  Shape s;
  if (term_shape_in(arg, 0, &s) && s.ndim > 0) {
    lam_shape_set(loc, &s);
  }
  // JIT: if the body is itself compute (a UOP graph), materialize
  // it now -- shape is in the side table, visit() can compile.
  // Bodies that aren't compute (curried lambdas, TIfZero, ...)
  // skip materialize -- it'd be a no-op anyway.
  if (term_tag(body) == TAG_UOP && term_ext(body) != UOP_KERNEL) {
    body = thvm_materialize(body);
  }
  heap_subst_var(loc, arg);
  return body;
}
