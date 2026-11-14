// The walk former's treated critical pairs, pushed per generation step.
//
// The walk former (step.c) formed each critical pair and pushed it at
// once: the treatment (doR normal forms of both sides, about half of an
// OrAssociativity run), the joined drop, the gates, the trace record and
// the queue insert.  Under atp_push_batch_on the treated FT route instead
// hands the formed pair here, and atp_push_batch_flush first treats the
// whole batch, then runs the push tail per pair in formation order.  The
// treatment reads only the rule set and its mirrors, which no push
// changes, and records no trace, so the trace and the queue come out
// byte-identical.  A route that pushes outside the batch flushes it first
// (atp_push_cps_traced refuses a push while one is pending).

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
typedef struct {
  CriticalPair cp;
  AtpFtCell   *fl, *fr;      // raw pair cells, then their normal forms
  u64          raw_off;      // raw pair bytes for the trace store
  u32          raw_len;
  u32          t_out, t_in, i_out, i_in;
  u32          rawsz_hint;
  // the overlap geometry the CPFORM diagnostic reads at the push
  u32  g_i, g_j, g_itr, g_jtr;
  Term g_ilhs, g_jlhs;
  u8   g_combo, g_i_or, g_j_or, g_pos_len;
  u8   g_pos[CP_MAX_DEPTH];
} AtpPushBatchEnt;

static AtpPushBatchEnt *g_pb           = NULL;
static u32              g_pb_cap       = 0u;
static u8              *g_pb_bytes     = NULL;
static u64              g_pb_bytes_n   = 0u;
static u64              g_pb_bytes_cap = 0u;
// pairs pushed by flushes inside the walk, collected per generation step
static u32              g_pb_mid       = 0u;

// Pairs per flush (THVM_ATP_PUSH_BATCH_CHUNK, default 8).  A small chunk
// keeps its cells in cache between formation, treatment and the pack:
// on OrAssociativity a chunk of 8 costs 3%, 128 costs 9%, and whole-fact
// batches (thousands of pairs) cost 41%, with identical work counts.
static u32 atp_push_batch_chunk(void) {
  static u32 n = 0u;
  if (n == 0u) {
    const char *e = getenv("THVM_ATP_PUSH_BATCH_CHUNK");
    long v = (e != NULL) ? strtol(e, NULL, 10) : 0;
    n = (v > 0) ? (u32)v : 8u;
  }
  return n;
}

// The treated route batches unless a push stage reads formation state
// the batch does not carry: the simpler-join peak (built from the walk
// operands), the definitional-constant keep lane (clones the raw cells
// before the treatment) and the guard strip (re-forms the raw pair).
// THVM_ATP_PUSH_BATCH=1 turns batching on; off, every pair is pushed at
// once, as before.
static int atp_push_batch_on(const AtpState *s) {
  static int env = -1;
  if (env < 0) {
    const char *e = getenv("THVM_ATP_PUSH_BATCH");
    env = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  return env && !s->use_simpler_join && !atp_drc_armed(s) &&
         !atp_ifeq_strip_armed(s);
}

// Takes the formed treated pair out of g_vcp_treat, whose operands the
// next formation overwrites: the raw bytes now, the cells by ownership.
// 0 when the raw emit fails; the caller then pushes the pair at once.
static u8 atp_push_batch_add(AtpState *s, const CriticalPair *cp,
                             u32 t_out, u32 t_in, u32 i_out, u32 i_in,
                             u32 rawsz_hint) {
  u32 len = vcp_ft_treat_emit_raw(s);
  if (len == 0u) return 0u;
  if (g_pb_n == g_pb_cap) {
    u32 ncap = g_pb_cap ? 2u * g_pb_cap : 1024u;
    AtpPushBatchEnt *nb = (AtpPushBatchEnt *)realloc(g_pb, (size_t)ncap * sizeof *nb);
    if (nb == NULL) thvm_fatal("atp_push_batch_add: OOM");
    g_pb = nb;
    g_pb_cap = ncap;
  }
  if (g_pb_bytes_n + len > g_pb_bytes_cap) {
    u64 ncap = g_pb_bytes_cap ? g_pb_bytes_cap : 65536u;
    while (g_pb_bytes_n + len > ncap) ncap *= 2u;
    u8 *nb = (u8 *)realloc(g_pb_bytes, (size_t)ncap);
    if (nb == NULL) thvm_fatal("atp_push_batch_add: OOM");
    g_pb_bytes = nb;
    g_pb_bytes_cap = ncap;
  }
  AtpPushBatchEnt *e = &g_pb[g_pb_n];
  memcpy(g_pb_bytes + g_pb_bytes_n, g_acp_scratch, len);
  e->raw_off = g_pb_bytes_n;
  e->raw_len = len;
  g_pb_bytes_n += len;
  e->cp = *cp;
  e->fl = g_vcp_treat.fl;
  e->fr = g_vcp_treat.fr;
  e->t_out = t_out;
  e->t_in = t_in;
  e->i_out = i_out;
  e->i_in = i_in;
  e->rawsz_hint = rawsz_hint;
  e->g_i = g_cpform_i;
  e->g_j = g_cpform_j;
  e->g_itr = g_cpform_itr;
  e->g_jtr = g_cpform_jtr;
  e->g_ilhs = g_cpform_ilhs;
  e->g_jlhs = g_cpform_jlhs;
  e->g_combo = g_cpform_combo;
  e->g_i_or = g_cpform_i_or;
  e->g_j_or = g_cpform_j_or;
  e->g_pos_len = g_cpform_pos_len;
  memcpy(e->g_pos, g_cpform_pos, sizeof e->g_pos);
  g_vcp_treat.fl = g_vcp_treat.fr = NULL;
  g_vcp_treat.lo = g_vcp_treat.sub = g_vcp_treat.rin = g_vcp_treat.ro = NULL;
  g_vcp_treat.subst = NULL;
  g_vcp_treat.raw_len = 0u;
  g_vcp_treat.armed = 0u;
  g_pb_n++;
  return 1u;
}

// Treats every batched pair, then pushes them in formation order.
// Returns the number queued.
static u32 atp_push_batch_flush(AtpState *s) {
  if (g_pb_n == 0u) return 0u;
  u64 t0 = g_atp_phase_enabled ? atp_now_us() : 0u;
  atp_ft_mirror_ensure(s);
  for (u32 k = 0; k < g_pb_n; k++) {
    g_pb[k].fl = atp_rules_only_normalize_ft(s, g_pb[k].fl, 64u);
    g_pb[k].fr = atp_rules_only_normalize_ft(s, g_pb[k].fr, 64u);
  }
  if (g_atp_phase_enabled) g_atp_phase_us_push_normalize += atp_now_us() - t0;
  u32 pushed = 0u;
  g_pb_flushing = 1u;
  for (u32 k = 0; k < g_pb_n; k++) {
    AtpPushBatchEnt *e = &g_pb[k];
    acp_scratch_ensure(0u, e->raw_len);
    memcpy(g_acp_scratch, g_pb_bytes + e->raw_off, e->raw_len);
    g_vcp_treat.fl = e->fl;
    g_vcp_treat.fr = e->fr;
    g_vcp_treat.raw_len = e->raw_len;
    g_vcp_treat.lo = g_vcp_treat.sub = g_vcp_treat.rin = g_vcp_treat.ro = NULL;
    g_vcp_treat.subst = NULL;
    g_vcp_treat.peak = NULL;
    g_vcp_treat.prenorm = 1u;
    g_vcp_treat.armed = 1u;
    g_cp_rawsz_hint = e->rawsz_hint;
    g_cpform_i = e->g_i;
    g_cpform_j = e->g_j;
    g_cpform_itr = e->g_itr;
    g_cpform_jtr = e->g_jtr;
    g_cpform_ilhs = e->g_ilhs;
    g_cpform_jlhs = e->g_jlhs;
    g_cpform_combo = e->g_combo;
    g_cpform_i_or = e->g_i_or;
    g_cpform_j_or = e->g_j_or;
    g_cpform_pos_len = e->g_pos_len;
    memcpy(g_cpform_pos, e->g_pos, sizeof g_cpform_pos);
    pushed += atp_push_cps_traced(s, &e->cp, 1u, e->t_out, e->t_in, e->i_out, e->i_in);
    vcp_treat_release(s);
    g_vcp_treat.prenorm = 0u;
    g_acp_fuse.valid = 0u;
  }
  g_pb_flushing = 0u;
  g_pb_n = 0u;
  g_pb_bytes_n = 0u;
  return pushed;
}

// The flush for a pair already formed but not batchable (its raw emit
// failed): its stash, geometry and size hint survive the flush for its
// own push right after.
static u32 atp_push_batch_flush_around(AtpState *s) {
  if (g_pb_n == 0u) return 0u;
  VcpTreat keep = g_vcp_treat;
  u32 hint = g_cp_rawsz_hint;
  u32 gi = g_cpform_i, gj = g_cpform_j, gitr = g_cpform_itr, gjtr = g_cpform_jtr;
  Term gil = g_cpform_ilhs, gjl = g_cpform_jlhs;
  u8 gc = g_cpform_combo, gio = g_cpform_i_or, gjo = g_cpform_j_or, gpl = g_cpform_pos_len;
  u8 gpos[CP_MAX_DEPTH];
  memcpy(gpos, g_cpform_pos, sizeof gpos);
  g_vcp_treat.fl = g_vcp_treat.fr = NULL;
  g_vcp_treat.peak = NULL;
  u32 pushed = atp_push_batch_flush(s);
  g_vcp_treat = keep;
  g_cp_rawsz_hint = hint;
  g_cpform_i = gi;
  g_cpform_j = gj;
  g_cpform_itr = gitr;
  g_cpform_jtr = gjtr;
  g_cpform_ilhs = gil;
  g_cpform_jlhs = gjl;
  g_cpform_combo = gc;
  g_cpform_i_or = gio;
  g_cpform_j_or = gjo;
  g_cpform_pos_len = gpl;
  memcpy(g_cpform_pos, gpos, sizeof gpos);
  return pushed;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES
