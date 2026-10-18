// (lam{#K:h; m} #K)
// ----------------- APP-MAT-NUM
// h
//
// (lam{#K:h; m} #K{a, b, ...})
// ---------------------------- APP-MAT-CTR
// (h a b ...)
//
// (lam{#K:h; m} &L{a, b})
// ----------------------- APP-MAT-SUP (app_mat_sup.c)
// ! H &L = h
// ! M &L = m
// &L{(lam{#K:H0; M0} a), (lam{#K:H1; M1} b)}
//
// (lam{#K:h; m} x)
// ---------------- APP-MAT-MIS, for any other x
// (m x)
//
// `arg` comes forced (wnf, then cnf for a DP head).  The APPs take fresh cells,
// not the consumed MAT and CTR (HVM4's wnf_app_mat_ctr): nf's step index
// (wnf/redex.c) learns only the cells a fire allocates.
fn Term interact_app_mat(Term mat, Term arg) {
  u64 mat_loc = term_val(mat);
  u32 match   = term_ext(mat);
  ITRS++;
  multi_emit(RULE_MAT_DISPATCH, MULTI_TERM, mat_loc, 0, match);
  switch (term_tag(arg)) {
    case TAG_NUM: {
      if ((u32)term_val(arg) == match) {
        return heap_read(mat_loc + 0);
      }
      break;
    }
    case TAG_CTR: {
      if (term_ext(arg) != match) {
        break;
      }
      Term res = heap_read(mat_loc + 0);
      u32  n   = term_ctr_n(arg);
      if (n == 0) {
        return res;
      }
      u64 apps = heap_alloc(2 * (u64)n);
      for (u32 i = 0; i < n; i++) {
        u64 a = apps + 2 * (u64)i;
        heap_set(a + 0, res);
        heap_set(a + 1, term_ctr_at(arg, i));
        res = term_new(0, TAG_APP, 0, a);
      }
      return res;
    }
    case TAG_SUP: {
      return interact_app_mat_sup(mat, arg);
    }
    default: {
      break;
    }
  }
  u64 app = heap_alloc(2);
  heap_set(app + 0, heap_read(mat_loc + 1));
  heap_set(app + 1, arg);
  return term_new(0, TAG_APP, 0, app);
}
