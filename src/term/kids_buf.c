// Child scratch for a CTR rebuild: `stk` (REWRITE_MAX_ARITY slots)
// when n fits, a heap block otherwise; release with term_kids_release.
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
