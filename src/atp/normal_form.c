// The normal form of `t` under the current rules and unorientable
// equations: ordered rewriting with the strategy and step budget the goal
// check normalizes with (atp_goal_pair_joined).  When completion has
// saturated, R and E form a ground-convergent system, so equal terms over
// the axioms have the same normal form; before that, the result is only
// some term equal to `t`.
fn Term thvm_atp_normal_form(AtpState *s, Term t) {
#ifdef THVM_ATP_AC
  if (thvm_atp_get_ac_mask() != 0ull) {
    return atp_rewrite_normalize(s, t, s->lhs, s->rhs, s->n_rules,
                                 ATP_GOAL_NORM_CAP);
  }
#endif
  atp_ft_mirror_ensure(s);
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *nf = atp_rewrite_normalize_ft(s, ft_from_term(a, t, 0),
                                           ATP_GOAL_NORM_CAP);
  Term out = ft_to_term(nf);
  // A kept system answers many queries out of one arena.
  ft_free_span(a, nf, nf->end);
  return out;
}
