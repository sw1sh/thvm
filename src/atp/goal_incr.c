// atp/goal_incr.c: incremental goal check (thvm_atp_goal_check, the
// single universal goal on the FT normal-form path).
//
// The goal check normalizes the ORIGINAL goal sides from scratch whenever
// the rule set changes, as Waldmeister's `rnf` goal module does ("tries
// to reduce initial goal", GOALS/NormaleZiele.c, "normal goals", :269):
// on each new fact NZ_ZielErledigtMitNeuemFaktum ("goal done with new
// fact", :188-212) restores the original sides (InitElemente, "init
// elements", :111-122) and normalizes them against the whole R and E
// (NF_NormalformRE2, "normal form over R and E").  Waldmeister's default
// module is `snf` ("keeps goals always reduced", :268; the default case
// of the goal option, GOALS/Zielverwaltung.c, "goal management",
// :756-759): it keeps the reduced sides and hands each new fact to
// NF_NormalformstRE (NF/NFBildung.c, "normal-form building", :715-724),
// which normalizes a side with R and E only when that fact applies
// somewhere in it (ObjektAngewendet, "object applied", :686-712).  On a
// mid-completion R that is not confluent the two modules reach different
// normal forms, and thvm's search depends on the `rnf` one (proof
// detection, the goal-similarity heuristics), so this keeps that normal
// form exactly and skips the work that cannot change it.
//
// A full normalization records its walk per side: each firing's rule
// (by uid), direction, pass (oriented rule or unorientable equation) and
// redex position.  At the next check the stored normal forms stand when
// the same walk would run again:
//   * every rule that fired is still in R with the same sides and
//     orientation,
//   * no unorientable equation was rewritten in place (both of its faces
//     are patterns and its ordering gate reads both sides; outside the
//     WM preset the RHS compose of interreduction reaches equations too,
//     counted in s->unorient_rhs_edits), and
//   * no rule added since fires at any position the walk visits.
// An oriented rule's right side is no pattern and enters no winner
// choice, so its in-place compose matters only when the rule fired.
// Rules leave R anywhere but join it only at the end of the slot order,
// and compaction keeps the order, so at every visit the candidates under
// the current R are the recorded candidates minus deleted ones plus added
// ones.  With the fired rule still present and no added rule firing, the
// winner is the same rule (the first match in slot order, or under the
// mixmost strategy the pattern-order minimum with the newest slot taking
// ties), and a visit where nothing fired still fires nothing.  With no
// rule added at all the first condition suffices.  Otherwise the walk is
// replayed from the original goal side with the recorded firings forced
// at their positions and only the added rules asked at each visit
// (ft_norm.c ft_mm_goal_replay / ft_goal_replay_outer); any added rule
// firing ends the replay and the check normalizes in full.  The replay
// costs the traversal and the splices, not the candidate search over all
// of R and its ordering gates.  A kept check moves the watermark up: the
// walks hold under the current R, so the next check asks only the rules
// added after it.  A walk that ran out of its step budget
// (a rewrite cycle, or a normal form further away than the budget) is
// kept and replayed the same way: the recorded firings exhaust the same
// budget, so a full normalization stops on the same term.
//
// Unorientable equations apply under ordered rewriting: the ordering gate
// of an added equation runs exactly as in the full walk, through the same
// per-position rule selection (ft_cell_try_rules over the added slots).
// Deleting a rule cannot make a normal form reducible, and when it did
// not fire on the walk it changes no visit's outcome.
//
// THVM_ATP_GOAL_INCR=0 turns the skip off (every check normalizes in
// full); -DTHVM_ATP_GOAL_INCR_SELFCHECK re-normalizes every skipped check
// from scratch off the work clock and stops the run if the result differs
// from the stored normal forms.

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)

typedef struct {
  u32              uid;
  const AtpFtCell *lhs;     // the rule's sides when its walk was recorded
  const AtpFtCell *rhs;
  u64              hash;    // content of both sides at that time
  u8               orient;
  u32              slot;    // its slot at the current check
} AtpGoalRule;

typedef struct AtpGoalIncr {
  u8               valid;
  u8               mixmost;   // strategy of the recorded walks
  u8               lex_dep;   // lex-dependent demodulation on
  u8               joined;    // verdict of the stored normal forms
  const AtpFtCell *mir_l;     // goal mirrors the walks started from
  const AtpFtCell *mir_r;
  AtpFtCell       *nf_l;      // the normal forms (own clones)
  AtpFtCell       *nf_r;
  u32              uid_wm;    // every rule with a larger uid came later
  u32              e_edits;   // s->unorient_rhs_edits the walks hold for
  AtpGoalWalk      walk[2];
  AtpGoalRule     *rules;     // the distinct rules that fired
  u32              n_rules, cap_rules;
} AtpGoalIncr;

// Profile counters (printed with the THVM_ATP_PROFILE phase block).
u64 g_atp_goal_incr_full    = 0u;   // full normalizations
u64 g_atp_goal_incr_nonew   = 0u;   // skips with no rule added
u64 g_atp_goal_incr_replay  = 0u;   // skips after a replay
u64 g_atp_goal_incr_diverge = 0u;   // replays an added rule ended
u64 g_atp_goal_incr_changed = 0u;   // a fired rule left or changed
u64 g_atp_goal_incr_miss    = 0u;   // clean replays off the stored form

static int atp_goal_incr_on(void) {
  static int on = -1;
  if (on < 0) {
    const char *e = getenv("THVM_ATP_GOAL_INCR");
    on = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  return on;
}

static void atp_goal_incr_free(struct AtpGoalIncr *g) {
  if (g == NULL) {
    return;
  }
  for (u32 k = 0; k < 2u; k++) {
    free(g->walk[k].fire);
    free(g->walk[k].pos);
  }
  free(g->rules);
  free(g);
}

static u64 atp_goal_rule_hash(const AtpFtCell *l, const AtpFtCell *r) {
  u64 h = 1469598103934665603ull;
  const AtpFtCell *side[2] = { l, r };
  for (u32 k = 0; k < 2u; k++) {
    const AtpFtCell *e = (side[k]->end != NULL) ? side[k]->end->next : NULL;
    for (const AtpFtCell *c = side[k]; c != e; c = c->next) {
      h ^= (u64)c->sym;
      h *= 1099511628211ull;
      h ^= (u64)c->arity;
      h *= 1099511628211ull;
    }
    h ^= 0xFFFFFFFFull;
    h *= 1099511628211ull;
  }
  return h;
}

// Slot of the live rule with this uid, or 0xFFFFFFFF.  Read-only: the
// uid_idx probe is verified against r_uid, and a miss scans.
static u32 atp_goal_rule_slot(const AtpState *s, u32 uid) {
  if (uid != 0u && uid < s->uid_idx_cap && s->uid_idx != NULL) {
    u32 h = s->uid_idx[uid];
    if (h < s->n_rules && s->r_uid[h] == uid) {
      return h;
    }
  }
  for (u32 i = 0; i < s->n_rules; i++) {
    if (s->r_uid[i] == uid) {
      return i;
    }
  }
  return 0xFFFFFFFFu;
}

static int atp_goal_u32_cmp(const void *a, const void *b) {
  u32 x = *(const u32 *)a, y = *(const u32 *)b;
  return (x > y) - (x < y);
}

// Keep a full normalization for the next check: the normal forms, the
// distinct rules the two walks fired (with their content), and the uid
// watermark.  A walk that ran out of its step budget is kept like one
// that reached a normal form: the same firings exhaust the same budget,
// so a full normalization stops at the same term.  A walk the NF cap or
// the deadline stopped, or one not fully recorded, leaves nothing.
static void atp_goal_incr_store(AtpState *s, AtpGoalIncr *g,
                                const AtpFtCell *mir_l, const AtpFtCell *mir_r,
                                const AtpFtCell *fl, const AtpFtCell *fr,
                                u8 joined) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  if (g->nf_l != NULL) {
    ft_free_span(a, g->nf_l, g->nf_l->end);
  }
  if (g->nf_r != NULL) {
    ft_free_span(a, g->nf_r, g->nf_r->end);
  }
  g->nf_l  = NULL;
  g->nf_r  = NULL;
  g->valid = 0u;
  if (g->walk[0].cut || g->walk[0].unusable ||
      g->walk[1].cut || g->walk[1].unusable) {
    return;
  }
  u32 n_fire = g->walk[0].n_fire + g->walk[1].n_fire;
  u32 *uids = NULL;
  u32 n_uid = 0u;
  if (n_fire > 0u) {
    uids = (u32 *)malloc((size_t)n_fire * sizeof(u32));
    if (uids == NULL) {
      return;
    }
    for (u32 k = 0; k < 2u; k++) {
      for (u32 i = 0; i < g->walk[k].n_fire; i++) {
        uids[n_uid++] = g->walk[k].fire[i].uid;
      }
    }
    qsort(uids, n_uid, sizeof(u32), atp_goal_u32_cmp);
    u32 w = 0u;
    for (u32 i = 0; i < n_uid; i++) {
      if (w == 0u || uids[w - 1u] != uids[i]) {
        uids[w++] = uids[i];
      }
    }
    n_uid = w;
  }
  if (n_uid > g->cap_rules) {
    AtpGoalRule *nr = (AtpGoalRule *)realloc(g->rules, (size_t)n_uid * sizeof *nr);
    if (nr == NULL) {
      free(uids);
      return;
    }
    g->rules     = nr;
    g->cap_rules = n_uid;
  }
  for (u32 i = 0; i < n_uid; i++) {
    u32 slot = atp_goal_rule_slot(s, uids[i]);
    if (slot == 0xFFFFFFFFu) {
      free(uids);
      return;
    }
    AtpGoalRule *gr = &g->rules[i];
    gr->uid    = uids[i];
    gr->lhs    = s->lhs_ft[slot];
    gr->rhs    = s->rhs_ft[slot];
    gr->hash   = atp_goal_rule_hash(gr->lhs, gr->rhs);
    gr->orient = s->r_orient[slot];
    gr->slot   = slot;
  }
  g->n_rules = n_uid;
  for (u32 k = 0; k < 2u; k++) {
    for (u32 i = 0; i < g->walk[k].n_fire; i++) {
      AtpGoalFire *f = &g->walk[k].fire[i];
      u32 *hit = (u32 *)bsearch(&f->uid, uids, n_uid, sizeof(u32),
                                atp_goal_u32_cmp);
      f->ri = (u32)(hit - uids);
    }
  }
  free(uids);
  g->nf_l    = ft_clone_span(a, fl, 0);
  g->nf_r    = ft_clone_span(a, fr, 0);
  g->mir_l   = mir_l;
  g->mir_r   = mir_r;
  g->uid_wm  = s->next_rule_uid;
  g->e_edits = s->unorient_rhs_edits;
  g->joined  = joined;
  g->mixmost = s->use_mixmost_nf;
  g->lex_dep = (u8)(s->lex_dep_lim != 0u);
  g->valid   = 1u;
}

#ifdef THVM_ATP_GOAL_INCR_SELFCHECK
typedef struct {
  u32 uid, step;
  const AtpFtCell *lhs, *rhs;
  u64 hash;
  u8 orient, dead;
} AtpGoalAsked;
static AtpGoalAsked *g_goal_asked = NULL;
static u32 g_goal_asked_n = 0u, g_goal_asked_cap = 0u;
static void atp_goal_asked_log(const AtpState *s, u32 r) {
  if (g_goal_asked_n == g_goal_asked_cap) {
    u32 cap = g_goal_asked_cap ? 2u * g_goal_asked_cap : 4096u;
    AtpGoalAsked *n = (AtpGoalAsked *)realloc(g_goal_asked, (size_t)cap * sizeof *n);
    if (n == NULL) {
      return;
    }
    g_goal_asked = n;
    g_goal_asked_cap = cap;
  }
  AtpGoalAsked *a = &g_goal_asked[g_goal_asked_n++];
  a->uid = s->r_uid[r];
  a->step = s->step;
  a->lhs = s->lhs_ft[r];
  a->rhs = s->rhs_ft[r];
  a->hash = (a->lhs != NULL && a->rhs != NULL) ? atp_goal_rule_hash(a->lhs, a->rhs) : 0u;
  a->orient = s->r_orient[r];
  a->dead = (s->r_dead != NULL) ? s->r_dead[r] : 0u;
}
#endif

// 1 when a full normalization of the goal sides would retrace the stored
// walks (see the file comment), so the stored normal forms stand.
static int atp_goal_incr_same(AtpState *s, AtpGoalIncr *g, u32 step_cap) {
  // An unorientable equation rewritten in place since the walks may now
  // fire where it did not: both of its faces are patterns.
  if (s->unorient_rhs_edits != g->e_edits) {
    g_atp_goal_incr_changed++;
    return 0;
  }
  for (u32 i = 0; i < g->n_rules; i++) {
    AtpGoalRule *gr = &g->rules[i];
    u32 slot = atp_goal_rule_slot(s, gr->uid);
    if (slot == 0xFFFFFFFFu || (s->r_dead != NULL && s->r_dead[slot]) ||
        s->lhs_ft[slot] != gr->lhs || s->rhs_ft[slot] != gr->rhs ||
        s->r_orient[slot] != gr->orient ||
        atp_goal_rule_hash(gr->lhs, gr->rhs) != gr->hash) {
      g_atp_goal_incr_changed++;
      return 0;
    }
    gr->slot = slot;
  }
  // The added rules: a suffix of the slot order.
  u32 k0 = s->n_rules;
  while (k0 > 0u && s->r_uid[k0 - 1u] > g->uid_wm) {
    k0--;
  }
  for (u32 i = 0; i < k0; i++) {
    if (s->r_uid[i] > g->uid_wm) {
      return 0;
    }
  }
  if (k0 == s->n_rules) {
    g->uid_wm = s->next_rule_uid;
    g_atp_goal_incr_nonew++;
    return 1;
  }
#ifdef THVM_ATP_GOAL_INCR_SELFCHECK
  for (u32 r = k0; r < s->n_rules; r++) {
    atp_goal_asked_log(s, r);
  }
#endif
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  for (u32 k = 0; k < 2u; k++) {
    AtpGoalWalk *w = &g->walk[k];
    for (u32 i = 0; i < w->n_fire; i++) {
      w->fire[i].slot = g->rules[w->fire[i].ri].slot;
    }
    AtpGoalReplay rp;
    memset(&rp, 0, sizeof rp);
    rp.w     = w;
    rp.k0    = k0;
    rp.try_u = (u8)(s->n_unorient > 0u);
    AtpFtCell *t = ft_clone_span(a, (k == 0u) ? g->mir_l : g->mir_r, 0);
    g_goal_replay_arm = &rp;
    AtpFtCell *res = atp_rewrite_normalize_ft(s, t, step_cap);
    g_goal_replay_arm = NULL;
    if (rp.diverged || rp.cur != w->n_fire || rp.starved != w->starved) {
      ft_free_span(a, res, res->end);
      g_atp_goal_incr_diverge++;
      return 0;
    }
    // A replay that forced every recorded firing and met no added rule
    // rebuilt the recorded walk, so it ends on the stored normal form;
    // were it not to, the check normalizes in full (counted as a miss).
    int kept = ft_eq(res, (k == 0u) ? g->nf_l : g->nf_r);
    ft_free_span(a, res, res->end);
    if (!kept) {
      g_atp_goal_incr_miss++;
#ifdef THVM_ATP_GOAL_INCR_SELFCHECK
      thvm_fatal("goal-incr: a clean replay missed the stored normal form");
#endif
      return 0;
    }
  }
  // The walks hold under the current R: later checks ask only the rules
  // added after this one.
  g->uid_wm = s->next_rule_uid;
  g_atp_goal_incr_replay++;
  return 1;
}

#ifdef THVM_ATP_GOAL_INCR_SELFCHECK
static u64 g_atp_goal_incr_sc = 0u;

static void atp_goal_incr_sc_report(void) {
  fprintf(stderr, "GOAL-INCR-SELFCHECK verified=%llu full=%llu nonew=%llu "
          "replay=%llu diverge=%llu changed=%llu miss=%llu\n",
          (unsigned long long)g_atp_goal_incr_sc,
          (unsigned long long)g_atp_goal_incr_full,
          (unsigned long long)g_atp_goal_incr_nonew,
          (unsigned long long)g_atp_goal_incr_replay,
          (unsigned long long)g_atp_goal_incr_diverge,
          (unsigned long long)g_atp_goal_incr_changed,
          (unsigned long long)g_atp_goal_incr_miss);
}

// A skipped check, re-normalized from the original goal off the work
// clock, must reproduce the stored normal forms.
static void atp_goal_incr_selfcheck(AtpState *s, AtpGoalIncr *g,
                                    u32 step_cap) {
  static int armed = 0;
  if (!armed) {
    armed = 1;
    atexit(atp_goal_incr_sc_report);
  }
  atp_work_pause();
  static AtpGoalWalk fw[2];
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  atp_goal_walk_clear(&fw[0]);
  g_goal_rec_arm = &fw[0];
  AtpFtCell *l = atp_rewrite_normalize_ft(s, ft_clone_span(a, g->mir_l, 0),
                                          step_cap);
  atp_goal_walk_clear(&fw[1]);
  g_goal_rec_arm = &fw[1];
  AtpFtCell *r = atp_rewrite_normalize_ft(s, ft_clone_span(a, g->mir_r, 0),
                                          step_cap);
  g_goal_rec_arm = NULL;
  int same = ft_eq(l, g->nf_l) && ft_eq(r, g->nf_r);
  if (!same) {
    fprintf(stderr, "GOAL-INCR-SELFCHECK mismatch at step %u (n_rules=%u "
            "n_unorient=%u uid_wm=%u next_uid=%u mixmost=%u)\n",
            s->step, s->n_rules, s->n_unorient, g->uid_wm, s->next_rule_uid,
            s->use_mixmost_nf);
    for (u32 k = 0; k < 2u; k++) {
      const AtpGoalWalk *ow = &g->walk[k], *nw = &fw[k];
      u32 i = 0;
      while (i < ow->n_fire && i < nw->n_fire &&
             ow->fire[i].uid == nw->fire[i].uid &&
             ow->fire[i].dir == nw->fire[i].dir &&
             ow->fire[i].depth == nw->fire[i].depth &&
             (ow->fire[i].depth == 0u || nw->n_pos == 0u ||
              memcmp(ow->pos + ow->fire[i].pos_off, nw->pos + nw->fire[i].pos_off,
                     (size_t)ow->fire[i].depth * sizeof(u16)) == 0)) {
        i++;
      }
      fprintf(stderr, "  side %u: stored %u firings (starved %u), full %u "
              "(starved %u); first difference at firing %u\n", k,
              ow->n_fire, ow->starved, nw->n_fire, nw->starved, i);
      for (u32 w = 0; w < 2u; w++) {
        const AtpGoalWalk *x = (w == 0u) ? ow : nw;
        if (i >= x->n_fire) {
          fprintf(stderr, "    %s: (no firing)\n", w ? "full  " : "stored");
          continue;
        }
        const AtpGoalFire *f = &x->fire[i];
        u32 slot = atp_goal_rule_slot(s, f->uid);
        char lb[1024], rb[1024];
        lb[0] = rb[0] = '\0';
        if (slot != 0xFFFFFFFFu) {
          atp_pretty_term(ft_to_term(s->lhs_ft[slot]), lb, sizeof lb);
          atp_pretty_term(ft_to_term(s->rhs_ft[slot]), rb, sizeof rb);
        }
        fprintf(stderr, "    %s: uid=%u (%s) slot=%d dir=%u pass=%u depth=%u "
                "orient=%d dead=%d  %s = %s\n", w ? "full  " : "stored",
                f->uid, (f->uid > g->uid_wm) ? "added" : "old",
                (slot == 0xFFFFFFFFu) ? -1 : (int)slot, f->dir, f->pass,
                f->depth,
                (slot == 0xFFFFFFFFu) ? -1 : (int)s->r_orient[slot],
                (slot == 0xFFFFFFFFu || s->r_dead == NULL) ? -1 : (int)s->r_dead[slot],
                lb, rb);
        if (slot != 0xFFFFFFFFu) {
          fprintf(stderr, "      now: lhs=%p rhs=%p hash=%016llx\n",
                  (const void *)s->lhs_ft[slot], (const void *)s->rhs_ft[slot],
                  (unsigned long long)atp_goal_rule_hash(s->lhs_ft[slot], s->rhs_ft[slot]));
        }
        u32 shown = 0u;
        for (u32 q = 0; q < g_goal_asked_n && shown < 8u; q++) {
          const AtpGoalAsked *ak = &g_goal_asked[q];
          if (ak->uid != f->uid) {
            continue;
          }
          shown++;
          fprintf(stderr, "      asked at step %u: lhs=%p rhs=%p hash=%016llx "
                  "orient=%u dead=%u\n", ak->step, (const void *)ak->lhs,
                  (const void *)ak->rhs, (unsigned long long)ak->hash,
                  ak->orient, ak->dead);
        }
        if (shown == 0u) {
          fprintf(stderr, "      never asked by a replay\n");
        }
      }
    }
    thvm_fatal("goal-incr selfcheck: a skipped check differs from the full "
               "normalization");
  }
  ft_free_span(a, l, l->end);
  ft_free_span(a, r, r->end);
  atp_work_resume();
  g_atp_goal_incr_sc++;
}
#endif

#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES
