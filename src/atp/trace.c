// === stage 6.2: PCL-shaped trace serializer ===========================
//
// Walks the trace[] array and emits human-readable text in the shape
// of Waldmeister's PCL ("Proof Construction Language") output.  Each
// line:
//
//   <idx> (<reason> [from <p_a>[, <p_b>]]): <lhs> = <rhs>
//
// Term printer handles TAG_CTR (as "C<lab>(args...)"), TAG_FVR (as
// "x_<id>"), TAG_NUM (as "#<val>"), TAG_ERA (as "ERA"), with a
// "?T<tag>" fallback for any other tag.  Truncates silently on
// buffer overflow.  Each open CTR level keeps the locals a recursive
// printer would -- its own buffer window (buf, cap), written length w
// and child index -- on an explicit stack, and a finished child's
// length is added to its parent's w as a recursive return would be.

typedef struct {
  u64   base;
  char *buf;
  u32   cap;
  u32   w;
  u32   i;
  u32   n;
} AtpPrettyFrame;

static u32 atp_pretty_term(Term t, char *buf, u32 cap) {
  enum { PRETTY_INL = 64 };
  AtpPrettyFrame  fr_inl[PRETTY_INL];
  AtpPrettyFrame *fr  = fr_inl;
  u32             nfr = 0u;
  u32             cfr = PRETTY_INL;
  u32             ret = 0u;
  for (;;) {
    u8 opened = 0u;
    if (cap == 0) {
      ret = 0u;
    } else {
      switch (term_tag(t)) {
        case TAG_FVR: ret = (u32)snprintf(buf, cap, "x_%u", term_ext(t)); break;
        case TAG_NUM: ret = (u32)snprintf(buf, cap, "#%u", (u32)term_val(t)); break;
        case TAG_ERA: ret = (u32)snprintf(buf, cap, "ERA"); break;
        case TAG_CTR: {
          if (cap <= 1) {
            ret = 0u;
            break;
          }
          u32 lab = term_ext(t);
          u32 n   = term_ctr_n(t);
          int n_w = snprintf(buf, cap, "C%u", lab);
          u32 w   = (u32)n_w;
          if (n_w < 0) {
            ret = 0u;
          } else if (w >= cap) {
            ret = cap - 1;
          } else if (n == 0 || w + 1 >= cap) {
            ret = w;
          } else {
            w += (u32)snprintf(buf + w, cap - w, "(");
            if (nfr == cfr) {
              fr = (AtpPrettyFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
            }
            fr[nfr].base = term_val(t);
            fr[nfr].buf  = buf;
            fr[nfr].cap  = cap;
            fr[nfr].w    = w;
            fr[nfr].i    = 0u;
            fr[nfr].n    = n;
            nfr++;
            opened = 1u;
          }
          break;
        }
        default: ret = (u32)snprintf(buf, cap, "?T%u", term_tag(t)); break;
      }
    }
    // Run open levels until one starts a child.  `have` = a child just
    // returned `ret` to the innermost level.
    u8 have = (u8)!opened;
    for (;;) {
      if (nfr == 0u) {
        term_stack_release(fr, fr_inl);
        return ret;
      }
      AtpPrettyFrame *f = &fr[nfr - 1u];
      if (have) {
        have = 0u;
        f->w += ret;
        if (f->w >= f->cap) {
          ret = f->cap - 1;
          nfr--;
          have = 1u;
          continue;
        }
        f->i++;
      }
      if (f->i < f->n && f->w + 2 < f->cap) {
        if (f->i > 0) f->w += (u32)snprintf(f->buf + f->w, f->cap - f->w, ", ");
        if (f->w >= f->cap) {
          ret = f->cap - 1;
          nfr--;
          have = 1u;
          continue;
        }
        t   = heap_read(f->base + 1u + (u64)f->i);
        buf = f->buf + f->w;
        cap = f->cap - f->w;
        break;
      }
      if (f->w + 1 < f->cap) f->w += (u32)snprintf(f->buf + f->w, f->cap - f->w, ")");
      ret = f->w;
      nfr--;
      have = 1u;
    }
  }
}

fn u32 thvm_atp_trace_serialize(const AtpState *s, char *buf, u32 cap) {
  if (s == NULL || buf == NULL || cap == 0) return 0;
  buf[0] = '\0';
  u32 w = 0;
  for (u32 i = 0; i < s->n_trace; i++) {
    if (w + 1 >= cap) break;
    Term entry  = s->trace[i];
    u32  reason = term_ext(entry);
    u32  p_a    = (u32)term_val(term_ctr_at(entry, 0));
    u32  p_b    = (u32)term_val(term_ctr_at(entry, 1));
    Term lhs, rhs;
    if (!atp_trace_eq_load((AtpState *)s, i, &lhs, &rhs)) {
      lhs = term_ctr_at(entry, 2);
      rhs = term_ctr_at(entry, 3);
    }

    const char *type_str = "?";
    switch (reason) {
      case TRACE_AXIOM:    type_str = "axiom";    break;
      case TRACE_ORIENT:   type_str = "orient";   break;
      case TRACE_CP:       type_str = "cp";       break;
      case TRACE_SIMPLIFY: type_str = "simplify"; break;
      case TRACE_FVI:      type_str = "fvi";      break;
    }

    int n;
    if (p_a == ATP_TRACE_NONE) {
      n = snprintf(buf + w, cap - w, "%u (%s): ", i, type_str);
    } else if (p_b == ATP_TRACE_NONE) {
      n = snprintf(buf + w, cap - w, "%u (%s from %u): ", i, type_str, p_a);
    } else {
      n = snprintf(buf + w, cap - w, "%u (%s from %u, %u): ", i, type_str,
                   p_a, p_b);
    }
    if (n < 0) break;
    w += (u32)n;
    if (w + 1 >= cap) break;

    w += atp_pretty_term(lhs, buf + w, cap - w);
    if (w + 4 >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, " = ");

    w += atp_pretty_term(rhs, buf + w, cap - w);
    if (w + 1 >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, "\n");
  }
  if (w >= cap) w = cap - 1;
  buf[w] = '\0';
  return w;
}

// === proof extraction =================================================
//
// Reconstruct the equational rewrite chain that closes a single-NF
// goal.  thvm_atp_trace_serialize (above) emits the COMPLETION trace
// -- the CP/rule derivation DAG; this emits the orthogonal object: the
// chain of rewrites taking each conjecture side to the shared normal
// form.
//
// The recording rewriter mirrors the WL-driven engine's normalizer so
// the recorded chain reproduces goal_check's single-NF result:
// leftmost-outermost, lowest-index rule.  An oriented rule fires
// forward (lhs->rhs); under ordered rewriting an unorientable equation
// fires whichever direction strictly decreases the redex -- so the
// recorded chain descends a well-founded order and cannot bounce
// between the two faces of a symmetric equation.

// One leftmost-outermost rewrite of `t`, recording the redex path and
// the rule index.  `pos` is caller-owned scratch holding the path so
// far in pos[0..depth); on a hit the full path is left in
// pos[0..*out_pos_len).  *out_fwd is 1 when the rule fired lhs->rhs, 0
// when an unorientable equation fired rhs->lhs.  Returns the rewritten
// term; *fired = 1 on a hit, 0 at a fixpoint.
static Term atp_proof_rewrite_step(AtpState *s, Term t, u8 *pos, u8 depth,
                                   u32 *out_rule, u8 *out_pos_len,
                                   u8 *out_fwd, u8 *fired) {
  for (u32 i = 0; i < s->n_rules; i++) {
#ifdef ATP_ORDERED_REWRITE
    // An unorientable equation (r_orient[i] == 0) is stored once and
    // rewrites in whichever direction is order-decreasing for the
    // redex at hand -- the same both-directions, order-gated rule as
    // atp_ordered_try_top, so the recorded step matches the normalizer.
    if (!s->r_orient[i]) {
      if (g_atp_proof_oriented_only) continue;   // oriented-fixpoint phase
      {                                                     // l -> r
        RewriteSubst sub = {{0}};
        if (thvm_match(s->lhs[i], t, &sub)) {
          Term tmpl = atp_unorient_template(s, s->lhs[i], s->rhs[i]);
          if (tmpl != 0) {
            Term repl = thvm_subst_apply(tmpl, &sub);
            if (atp_compare(s, t, repl) == KBO_GT) {
              *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
              return repl;
            }
          }
        }
      }
      {                                                     // r -> l
        RewriteSubst sub = {{0}};
        if (thvm_match(s->rhs[i], t, &sub)) {
          Term tmpl = atp_unorient_template(s, s->rhs[i], s->lhs[i]);
          if (tmpl != 0) {
            Term repl = thvm_subst_apply(tmpl, &sub);
            if (atp_compare(s, t, repl) == KBO_GT) {
              *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 0;
              return repl;
            }
          }
        }
      }
      continue;
    }
#endif
    RewriteSubst sub = {{0}};
    if (thvm_match(s->lhs[i], t, &sub)) {
      *fired = 1;
      *out_rule = i;
      *out_pos_len = depth;
      *out_fwd = 1;
      return thvm_subst_apply(s->rhs[i], &sub);
    }
  }
  if (term_tag(t) == TAG_CTR && depth < ATP_PROOF_MAX_DEPTH) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    for (u32 i = 0; i < n; i++) {
      pos[depth] = (u8)i;
      u8 cf = 0;
      Term nch = atp_proof_rewrite_step(s, term_ctr_at(t, i), pos,
                                        (u8)(depth + 1u), out_rule,
                                        out_pos_len, out_fwd, &cf);
      if (cf) {
        Term children[REWRITE_MAX_ARITY];
        for (u32 j = 0; j < n; j++) {
          children[j] = (j == i) ? nch : term_ctr_at(t, j);
        }
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  *fired = 0;
  return t;
}

// 1 iff pattern p is a CTR whose head cannot match at a node with the
// given head (thvm_match fails on the head before anything else).
static inline u8 atp_head_misses(Term p, u8 t_ctr, u32 t_lab) {
  return (u8)(term_tag(p) == TAG_CTR && (!t_ctr || term_ext(p) != t_lab));
}

// Slice-aware port of atp_proof_rewrite_step above: same outermost-
// leftmost rule pick (with the ORDERED_REWRITE both-directions/order-
// gated branch for unorientable equations), but the candidate rule
// list is the (lhs_arr / rhs_arr / n) slice rather than s->lhs.  The
// per-rule cached orientation s->r_orient does not apply to a custom
// slice, so the unorientable / orientable split falls back to a per-
// rule atp_compare here -- in the interreduce caller's case n <= 2 so
// the cost is trivial.
static Term atp_proof_rewrite_step_slice(AtpState *s, Term t,
                                         u8 *pos, u8 depth,
                                         const Term *lhs_arr,
                                         const Term *rhs_arr,
                                         u32 n,
                                         u32 *out_rule, u8 *out_pos_len,
                                         u8 *out_fwd, u8 *fired) {
  // WM per-position redex priority (NF/NFBildung.c:503-531): oriented
  // rules first at this position, unorientable equations only when no
  // oriented rule matched here -- the same two-pass order as
  // atp_ordered_try_top / find_redex_ft, so the recorded chain replays
  // the slice normalizers' redex picks exactly.  A rule whose pattern
  // head differs from the node's is skipped before its orientation
  // lookup and substitution setup: it could not match here.
  u8  t_ctr = (u8)(term_tag(t) == TAG_CTR);
  u32 t_lab = t_ctr ? term_ext(t) : 0u;
  for (u32 i = 0; i < n; i++) {
    if (atp_head_misses(lhs_arr[i], t_ctr, t_lab)) {
      continue;
    }
#ifdef ATP_ORDERED_REWRITE
    if (!atp_orient_memo_gt(s, lhs_arr[i], rhs_arr[i])) continue;
#endif
    RewriteSubst sub = {{0}};
    if (thvm_match(lhs_arr[i], t, &sub)) {
      *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
      return thvm_subst_apply(rhs_arr[i], &sub);
    }
  }
#ifdef ATP_ORDERED_REWRITE
  for (u32 i = 0; i < n; i++) {
    u8 l_miss = atp_head_misses(lhs_arr[i], t_ctr, t_lab);
    u8 r_miss = atp_head_misses(rhs_arr[i], t_ctr, t_lab);
    if (l_miss && r_miss) {
      continue;
    }
    if (atp_orient_memo_gt(s, lhs_arr[i], rhs_arr[i])) continue;
    if (!l_miss) {                                            // l -> r
      RewriteSubst sub = {{0}};
      if (thvm_match(lhs_arr[i], t, &sub)) {
        Term tmpl = atp_unorient_template(s, lhs_arr[i], rhs_arr[i]);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &sub);
          if (atp_compare(s, t, repl) == KBO_GT) {
            *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
            return repl;
          }
        }
      }
    }
    if (!r_miss) {                                            // r -> l
      RewriteSubst sub = {{0}};
      if (thvm_match(rhs_arr[i], t, &sub)) {
        Term tmpl = atp_unorient_template(s, rhs_arr[i], lhs_arr[i]);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &sub);
          if (atp_compare(s, t, repl) == KBO_GT) {
            *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 0;
            return repl;
          }
        }
      }
    }
  }
#endif
  if (term_tag(t) == TAG_CTR && depth < ATP_PROOF_MAX_DEPTH) {
    u32 n_ctr = term_ctr_n(t);
    if (n_ctr > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    for (u32 i = 0; i < n_ctr; i++) {
      pos[depth] = (u8)i;
      u8 cf = 0;
      Term nch = atp_proof_rewrite_step_slice(s, term_ctr_at(t, i), pos,
                                              (u8)(depth + 1u),
                                              lhs_arr, rhs_arr, n,
                                              out_rule, out_pos_len,
                                              out_fwd, &cf);
      if (cf) {
        Term children[REWRITE_MAX_ARITY];
        for (u32 j = 0; j < n_ctr; j++) {
          children[j] = (j == i) ? nch : term_ctr_at(t, j);
        }
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n_ctr);
      }
    }
  }
  *fired = 0;
  return t;
}

// Append one recorded rewrite of `t` to out[*n) and return the result;
// `*progressed` is set when a step fired.  `oriented_only` selects the
// orientable-fixpoint phase (skip unorientable equations).
static Term atp_proof_record_one(AtpState *s, Term t, u32 side,
                                 u8 oriented_only,
                                 AtpProofStep *out, u32 cap, u32 *n,
                                 u8 *progressed) {
  u8  pos[ATP_PROOF_MAX_DEPTH];
  u32 rule = 0;
  u8  pos_len = 0, fwd = 1u, fired = 0;
  g_atp_proof_oriented_only = oriented_only;
  Term t2 = atp_proof_rewrite_step(s, t, pos, 0u, &rule, &pos_len,
                                   &fwd, &fired);
  g_atp_proof_oriented_only = 0u;
  if (!fired || kbo_eq(t, t2)) { *progressed = 0u; return t; }
  if (*n < cap) {
    AtpProofStep *st = &out[*n];
    st->side    = side;
    st->rule    = rule;
    st->fwd     = fwd;
    st->pos_len = pos_len;
    for (u8 k = 0; k < pos_len; k++) st->pos[k] = pos[k];
    st->before  = t;
    st->after   = t2;
    (*n)++;
  }
  *progressed = 1u;
  return t2;
}

// Normalize one conjecture side, appending each rewrite to out[*n).
// `side` tags every recorded step; returns the side's normal form.
//
// Mirrors atp_rewrite_normalize_ordered (the goal-check normalizer):
// run the orientable rules to a fixpoint, then take ONE unorientable
// (both-directions, order-gated) step, and repeat to a joint fixpoint.
// A single interleaved pass reaches a DIFFERENT normal form on a
// non-confluent rule set (completion stopped at the goal-join), so the
// recorded chain would not meet the other side -- thvm_atp_proof_extract
// would then return 0 and the ProofObject reconstruction fail even
// though the engine proved the goal.
static Term atp_proof_record_side(AtpState *s, Term t, u32 side,
                                  AtpProofStep *out, u32 cap, u32 *n) {
  for (u32 it = 0; it < ATP_PROOF_MAX_STEPS; it++) {
    // Wall-deadline / host-abort poll.  This routine runs AFTER the
    // saturation loop returns (called from thvm_wl_atp_run_proof's
    // post-engine proof-extraction step), so without a poll here a
    // timed-out saturation followed by a long proof-extract rewrite
    // ignores TimeConstraint and hangs the kernel past wall_deadline.
    if ((it & 0x7Fu) == 0u) {
      if (s->wall_deadline_us != 0u) {
        u64 now = atp_now_us();
        if (now != 0u && now >= s->wall_deadline_us) return t;
      }
      if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return t;
    }
    u8 progressed = 0u;
    // Orientable indexed-fixpoint phase: drain every oriented rewrite.
    for (u32 j = 0; j < ATP_PROOF_MAX_STEPS; j++) {
      u8 ofired = 0u;
      t = atp_proof_record_one(s, t, side, 1u, out, cap, n, &ofired);
      if (!ofired) break;
      progressed = 1u;
      if ((j & 0x7Fu) == 0u) {
        if (s->wall_deadline_us != 0u) {
          u64 now = atp_now_us();
          if (now != 0u && now >= s->wall_deadline_us) return t;
        }
        if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return t;
      }
    }
    // One unorientable step (skipped when n_unorient == 0).
    u8 ufired = 0u;
    t = atp_proof_record_one(s, t, side, 0u, out, cap, n, &ufired);
    if (ufired) progressed = 1u;
    if (!progressed) return t;            // joint fixpoint
  }
  return t;
}

// The term reached on `side` after keeping `k` of that side's recorded
// rewrites: k == 0 is the side's seed (the first step's `before`, or the
// seed `seed` when the side recorded no steps), otherwise the k-th step's
// `after`.  `lo`/`hi` bound the side's [lo, hi) slice in out[].
static Term atp_chain_term(const AtpProofStep *out, u32 lo, u32 hi,
                           Term seed, u32 k) {
  if (lo >= hi) return seed;             // no steps recorded this side
  if (k == 0u) return out[lo].before;
  return out[lo + (k - 1u)].after;
}

// Goal-chain meet trim (Option B): the two sides are each recorded to
// their INDEPENDENT joint fixpoint, but FindEquationalProof stops the
// moment the sides coincide.  On a non-confluent completion that join is
// reached BEFORE either fixpoint (e.g. the unorientable a.a == b.b rule,
// grounded to a reserved min-const, fires once more on each side after
// they already met -- a 2-step out-and-back that nets to a no-op).  Find
// the earliest (i, j), minimizing i + j, where keeping i lhs steps and j
// rhs steps lands both sides on kbo-equal terms, then drop every step
// past that meet on either side.  The kept chain still closes the goal:
// lhs walks to the meet, rhs walks to the same meet, meet == meet.
static u32 atp_proof_trim_to_meet(const AtpProofStep *out, u32 n0, u32 n,
                                  Term seed_lhs, Term seed_rhs,
                                  AtpProofStep *trimmed) {
  u32 best_i = n0, best_j = n - n0, best_sum = (n0) + (n - n0);
  u8  found = 0u;
  for (u32 i = 0; i <= n0; i++) {
    Term tl = atp_chain_term(out, 0u, n0, seed_lhs, i);
    for (u32 j = 0; j <= n - n0; j++) {
      if (i + j >= best_sum) continue;   // can't beat the current best
      Term tr = atp_chain_term(out, n0, n, seed_rhs, j);
      if (kbo_eq(tl, tr)) {
        best_i = i; best_j = j; best_sum = i + j; found = 1u;
      }
    }
  }
  if (!found) {                           // sides never coincide on a prefix
    for (u32 k = 0; k < n; k++) trimmed[k] = out[k];
    return n;
  }
  // Compact: keep out[0, best_i) (lhs) then out[n0, n0+best_j) (rhs).
  u32 w = 0u;
  for (u32 k = 0; k < best_i; k++)            trimmed[w++] = out[k];
  for (u32 k = 0; k < best_j; k++)            trimmed[w++] = out[n0 + k];
  return w;
}

fn u32 thvm_atp_proof_extract(AtpState *s, AtpProofStep *out, u32 cap) {
  if (s == NULL || out == NULL || cap == 0u) return 0;
  // Single-NF extraction only: no goal, or an existential (narrowing)
  // goal, has no two-sided rewrite chain to reconstruct here.
  if (s->goal_lhs == 0 || s->goal_existential) return 0;

  // Record the goal_lhs chain (side 0) then the goal_rhs chain
  // (side 1), both forward, appended into out[].  The assembled
  // proof rewrites the equation L == R: first L down to its normal
  // form, then R down to the same normal form, ending at NF == NF.
  u32  n = 0;
  Term nf_lhs, nf_rhs;
  u32  n0;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (s->use_mixmost_nf) {
    // Order-faithful extraction: replay the goal reduction in the SAME
    // FT-mixmost (WM innermost) order the search proved with, recording
    // AtpProofSteps.  The Term-side outermost record below reaches
    // DIFFERENT (non-joining) normal forms on the non-confluent proof-
    // time R -- nf_lhs != nf_rhs -> return 0 -> empty-goal-chain.
    atp_ft_mirror_ensure(s);
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *lf = atp_rewrite_normalize_ft_proofsteps(
        s, ft_from_term(a, s->goal_lhs, 0), ATP_PROOF_MAX_STEPS, 0u, out, &n, cap);
    nf_lhs = ft_to_term(lf);
    n0 = n;                               // boundary: side-0 steps in [0, n0)
    AtpFtCell *rf = atp_rewrite_normalize_ft_proofsteps(
        s, ft_from_term(a, s->goal_rhs, 0), ATP_PROOF_MAX_STEPS, 1u, out, &n, cap);
    nf_rhs = ft_to_term(rf);
  } else
#endif
  {
    nf_lhs = atp_proof_record_side(s, s->goal_lhs, 0u, out, cap, &n);
    n0 = n;                               // boundary: side-0 steps in [0, n0)
    nf_rhs = atp_proof_record_side(s, s->goal_rhs, 1u, out, cap, &n);
  }

  if (getenv("THVM_ATP_GOAL_NF_DEBUG")) {
    char lb[512], rb[512];
    atp_pretty_term(nf_lhs, lb, sizeof lb);
    atp_pretty_term(nf_rhs, rb, sizeof rb);
    fprintf(stderr, "[extract] n_steps=%u n0=%u mixmost=%u nf_lhs=%s nf_rhs=%s kbo_eq=%d\n",
            n, n0, s->use_mixmost_nf, lb, rb, kbo_eq(nf_lhs, nf_rhs));
  }
  // Not single-NF provable -- the two sides never meet under R.  A
  // symmetric goal closed only by the MNF search lands here.
  if (!kbo_eq(nf_lhs, nf_rhs)) return 0;

  // Trim both chains back to the EARLIEST cross-side meet so the
  // reconstructed proof stops where FindEquationalProof stops, instead
  // of trailing the post-meet round-trip to each side's fixpoint.
  static AtpProofStep trimmed[ATP_PROOF_MAX_STEPS * 2u];
  u32 cmin = cap < (u32)(ATP_PROOF_MAX_STEPS * 2u)
                 ? cap : (u32)(ATP_PROOF_MAX_STEPS * 2u);
  if (n <= cmin) {
    u32 m = atp_proof_trim_to_meet(out, n0, n, s->goal_lhs, s->goal_rhs,
                                   trimmed);
    for (u32 k = 0; k < m; k++) out[k] = trimmed[k];
    n = m;
  }

  return n;
}

fn u32 thvm_atp_proof_serialize(const AtpProofStep *steps, u32 n_steps,
                                char *buf, u32 cap) {
  if (steps == NULL || buf == NULL || cap == 0u) return 0;
  buf[0] = '\0';
  u32 w = 0;
  for (u32 i = 0; i < n_steps; i++) {
    if (w + 1u >= cap) break;
    const AtpProofStep *st = &steps[i];
    int nw = snprintf(buf + w, cap - w, "%c rule %u %s @",
                      st->side == 0u ? 'L' : 'R', st->rule,
                      st->fwd ? "fwd" : "rev");
    if (nw < 0) break;
    w += (u32)nw;
    if (w + 1u >= cap) break;
    if (st->pos_len == 0u) {
      w += (u32)snprintf(buf + w, cap - w, "top");
    } else {
      for (u8 k = 0; k < st->pos_len && w + 2u < cap; k++) {
        w += (u32)snprintf(buf + w, cap - w, "%s%u",
                           k == 0 ? "" : ".", st->pos[k]);
      }
    }
    if (w + 3u >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, ": ");
    w += atp_pretty_term(st->before, buf + w, cap - w);
    if (w + 5u >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, " => ");
    w += atp_pretty_term(st->after, buf + w, cap - w);
    if (w + 1u >= cap) break;
    w += (u32)snprintf(buf + w, cap - w, "\n");
  }
  if (w >= cap) w = cap - 1;
  buf[w] = '\0';
  return w;
}

