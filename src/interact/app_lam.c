// (lam x.body) arg
// ---------------- APP-LAM
// x <- arg
// body
//
// Read the lambda body, install `arg` as the substitution at the
// binder's heap cell (so a future VAR enter on that loc picks it up
// and clears the SUB flag), then continue reducing the body.
//
// LAM_JIT_FLAG case: the body is currently a UOP graph with TVARs
// of unknown shape.  Now that we have `arg`, we know the bound
// var's shape -- register it in the lam_shape table, materialize
// the body into a UOP_KERNEL, and then proceed with the standard
// beta.  The kernel references TVAR(loc) as its input slot; the
// SUB substitution we install routes subsequent reads through
// `arg`.  Idempotent: if the body's already a UOP_KERNEL (e.g. on
// a re-entry path), we skip the JIT step.
fn Term interact_app_lam(Term lam, Term arg) {
  ITRS++;
  u64  loc  = term_val(lam);
  Term body = heap_read(loc);
  if ((term_ext(lam) & LAM_JIT_FLAG) &&
      !(term_tag(body) == TAG_UOP && term_ext(body) == UOP_KERNEL)) {
    Shape s;
    if (term_shape_in(arg, 0, &s) && s.ndim > 0) {
      lam_shape_set(loc, &s);
      Term mat = thvm_materialize(body);
      // Re-read in case materialize ran during the call: heap[loc]
      // might still hold the original body Term (it usually does;
      // materialize writes the kernel into a fresh cell and returns
      // it).  We replace heap[loc] below via heap_subst_var.
      body = mat;
    }
  }
  heap_subst_var(loc, arg);
  return body;
}
