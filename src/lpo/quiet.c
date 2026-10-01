// thvm_lpo's flat verdict for an observer: the persistent (s,t) memo is
// neither read nor written (it is keyed on cell addresses, so an
// observer's scratch terms must stay out of it), and the statistics and
// the deadline poll are restored, so the engine's later comparisons run
// as if this one never happened.  Returns 0 when a side does not fit the
// flat encoding; the caller then needs thvm_lpo.
fn int thvm_lpo_quiet(Term s, Term t, const LpoConfig *cfg, LpoCmp *out) {
  u32 na = 0u, nb = 0u;
  if (!lpo_flat_encode_rec(s, cfg, g_lpo_flat_a, &na) ||
      !lpo_flat_encode_rec(t, cfg, g_lpo_flat_b, &nb)) {
    return 0;
  }
  u64 vs = 0u, vt = 0u;
  u8  in_range = 1u;
  for (u32 i = 0; i < na && in_range; i++) {
    if (g_lpo_flat_a[i].sym < 0) {
      u32 vid = (u32)(-g_lpo_flat_a[i].sym - 1);
      if (vid >= LPO_MAX_VAR) {
        in_range = 0u;
      } else {
        vs |= 1ull << vid;
      }
    }
  }
  for (u32 i = 0; i < nb && in_range; i++) {
    if (g_lpo_flat_b[i].sym < 0) {
      u32 vid = (u32)(-g_lpo_flat_b[i].sym - 1);
      if (vid >= LPO_MAX_VAR) {
        in_range = 0u;
      } else {
        vt |= 1ull << vid;
      }
    }
  }
  if (in_range && (vs & ~vt) != 0u && (vt & ~vs) != 0u) {
    *out = LPO_UN;
    return 1;
  }
  u64 rc = g_lpo_flat_rec_calls, mh = g_lpo_flat_memo_hits;
  u64 cc = g_lpo_flat_compute_calls, tc = g_lpo_top_calls;
  u64 fb = g_lpo_dag_fallbacks, pc = g_lpo_poll_ctr;
  int pd = g_lpo_past_deadline;
  *out = lpo_flat_cmp(na, nb, cfg);
  g_lpo_flat_rec_calls     = rc;
  g_lpo_flat_memo_hits     = mh;
  g_lpo_flat_compute_calls = cc;
  g_lpo_top_calls          = tc;
  g_lpo_dag_fallbacks      = fb;
  g_lpo_poll_ctr           = pc;
  g_lpo_past_deadline      = pd;
  return 1;
}
