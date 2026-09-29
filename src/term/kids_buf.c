// term/kids_buf.c - child scratch for rebuilding one CTR node.
//
// The rewrite/unify/KBO walkers rebuild a node into a stack array of
// REWRITE_MAX_ARITY children.  A wider node used to make them bail out
// and return the subterm UNCHANGED -- a substitution or a renaming-apart
// that silently skips one subterm, i.e. a clause whose variables no
// longer denote what the rest of the clause says (FOF input reaches the
// clausal engine with arity-30 predicates and wide Skolem terms; the
// UEQ engine never exceeds arity 8).  Wide nodes now get a heap block:
// the common case keeps its stack array and pays one compare.

fn Term *term_kids_buf(Term *stk, u32 n) {
  if (n <= REWRITE_MAX_ARITY) {
    return stk;
  }
  Term *h = (Term *)malloc((size_t)n * sizeof(Term));
  if (h == NULL) {
    fprintf(stderr, "thvm: out of memory rebuilding an arity-%u term\n", n);
    abort();
  }
  return h;
}

fn void term_kids_release(Term *kids, Term *stk) {
  if (kids != stk) {
    free(kids);
  }
}
