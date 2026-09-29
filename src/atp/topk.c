// === Selection-time top-K re-ranker (neural clause selection, 0d) ====
//
// Opt-in: THVM_ATP_RERANK_TOPK=<K> with THVM_ATP_RERANK_MODEL=<file>
// (FindProof --rerank-topk / --rerank-model).  At every weight-dimension
// pick -- the heap-root branch of thvm_atp_select_cp, root not ultimate
// -- the engine's K best queued CPs under its own heap order are scored
// by a small head over what the selector knows at that moment, and the
// argmax is selected instead of the root.  The other K-1 stay queued
// untouched: the top K is found by a best-first walk of the binary heap
// and the chosen position leaves through the arbitrary-position
// extraction the FIFO dimension uses.
//
// Inputs per candidate (tools/atp_enigma/topk_net.py, x0..x17):
//   log1p(rank), log1p(n_cps), log1p(cp_pri),
//   the 14 thvm_atp_cp_features columns as the history export's 'F'
//   record computed them at the CP's push (age = seq; the goal columns
//   7 and 11 against the goal normal form as it stood at that push),
//   log1p(selections since the push).
// score = -kappa log1p(rank) + w2 . relu(W1 z + b1) + b2, z = (x - mu) / sd.
//
// Push-time state is reproduced from two logs instead of per-CP storage:
// cp_seq_next at every pick (push step of seq q = #{picks with
// cp_seq_next <= q}, as tools/atp_enigma/np0_extract.py) and the goal
// normal form keyed by the first seq pushed under it.  The candidates'
// pairs are decoded into heap scratch that is popped after the scoring.
// A CP repacked in place by the CP-set interreduction is scored on its
// current content.
//
// Unarmed, s->topk stays NULL and every hook is one predictable branch.

#define ATP_TOPK_MAX  64u
#define ATP_TOPK_NX   18u
#define ATP_TOPK_HMAX 128u

typedef struct {
  u32    loaded;
  u32    k;
  u32    hid;
  double mu[ATP_TOPK_NX];
  double sd[ATP_TOPK_NX];
  double w1[ATP_TOPK_HMAX * ATP_TOPK_NX];
  double b1[ATP_TOPK_HMAX];
  double w2[ATP_TOPK_HMAX];
  double b2;
  double kappa;
} AtpTopkModel;

typedef struct {
  u32   *bounds;
  u32    n_bounds;
  u32    cap_bounds;
  u32   *gv_seq;
  Term  *gv_l;
  Term  *gv_r;
  u32    n_gv;
  u32    cap_gv;
  u32    n_cache;
  u32    cache_ir;
  u32    c_seq[ATP_TOPK_MAX];
  float  c_feat[ATP_TOPK_MAX][ATP_CP_FEATURE_DIM];
  u64    n_pick;
  u64    n_changed;
  u64    n_feat;
  u64    t_us;
  FILE  *dump;
  u32    dump_left;
} AtpTopk;

static AtpTopkModel g_atp_topk_model;

// Defined further down the translation unit.
static int  atp_cp_before(const AtpState *s, u32 i, u32 j);
static void atp_cp_slot_read(const AtpState *s, u32 i, Term *lhs, Term *rhs);
static int  atp_st_read(FILE *f, const char *json, u64 data_start,
                        const char *name, u32 count, double *dst);
static void atp_cp_features_vs(const AtpState *s, Term lhs, Term rhs, u32 age,
                               Term gl, Term gr, float *out);

static u32 atp_topk_meta(const char *json, const char *key) {
  char pat[48];
  snprintf(pat, sizeof pat, "\"%s\":\"", key);
  const char *p = strstr(json, pat);
  if (p == NULL) {
    return 0u;
  }
  return (u32)strtoul(p + strlen(pat), NULL, 10);
}

// Load the head from a safetensors file (tools/atp_enigma/topk_net.py
// export): f32 tensors mu, sd [18], w1 [hid*18] (row-major), b1, w2
// [hid], b2, kappa [1]; K / Hidden / Inputs in __metadata__.  Returns 1
// on success.
fn int thvm_atp_topk_load(const char *path) {
  AtpTopkModel *m = &g_atp_topk_model;
  memset(m, 0, sizeof *m);
  FILE *f = (path != NULL) ? fopen(path, "rb") : NULL;
  if (f == NULL) {
    return 0;
  }
  u64 hlen = 0u;
  char *json = NULL;
  int ok = fread(&hlen, 8u, 1u, f) == 1u && hlen > 0u && hlen < (1u << 20);
  if (ok) {
    json = (char *)malloc((size_t)hlen + 1u);
    ok = json != NULL && fread(json, 1u, (size_t)hlen, f) == (size_t)hlen;
  }
  if (ok) {
    json[hlen] = '\0';
    m->k   = atp_topk_meta(json, "K");
    m->hid = atp_topk_meta(json, "Hidden");
    ok = m->k >= 1u && m->k <= ATP_TOPK_MAX && m->hid >= 1u && m->hid <= ATP_TOPK_HMAX
         && atp_topk_meta(json, "Inputs") == ATP_TOPK_NX;
  }
  if (ok) {
    u64 d0 = 8u + hlen;
    u32 h = m->hid;
    ok = atp_st_read(f, json, d0, "mu", ATP_TOPK_NX, m->mu)
         && atp_st_read(f, json, d0, "sd", ATP_TOPK_NX, m->sd)
         && atp_st_read(f, json, d0, "w1", h * ATP_TOPK_NX, m->w1)
         && atp_st_read(f, json, d0, "b1", h, m->b1)
         && atp_st_read(f, json, d0, "w2", h, m->w2)
         && atp_st_read(f, json, d0, "b2", 1u, &m->b2)
         && atp_st_read(f, json, d0, "kappa", 1u, &m->kappa);
  }
  free(json);
  fclose(f);
  m->loaded = ok ? 1u : 0u;
  return ok;
}

// Armed by the environment: K, and the model loaded once per process.
static u32 atp_topk_env_k(void) {
  static int k = -1;
  if (k >= 0) {
    return (u32)k;
  }
  k = 0;
  const char *e = getenv("THVM_ATP_RERANK_TOPK");
  const char *mp = getenv("THVM_ATP_RERANK_MODEL");
  if (e == NULL || e[0] == '\0' || atoi(e) <= 0) {
    return 0u;
  }
  if (!thvm_atp_topk_load(mp)) {
    fprintf(stderr, "topk-rerank: cannot load model '%s'; re-ranker off\n",
            mp != NULL ? mp : "");
    return 0u;
  }
  u32 want = (u32)atoi(e);
  if (want > ATP_TOPK_MAX) {
    want = ATP_TOPK_MAX;
  }
  if (want != g_atp_topk_model.k) {
    fprintf(stderr, "topk-rerank: --rerank-topk=%u but the model was trained at K=%u\n",
            want, g_atp_topk_model.k);
  }
  k = (int)want;
  return (u32)k;
}

static AtpTopk *atp_topk_state(AtpState *s) {
  if (s->topk != NULL) {
    return (AtpTopk *)s->topk;
  }
  if (atp_topk_env_k() == 0u) {
    return NULL;
  }
  AtpTopk *tk = (AtpTopk *)calloc(1, sizeof(AtpTopk));
  if (tk == NULL) {
    return NULL;
  }
  const char *dp = getenv("THVM_ATP_RERANK_DUMP");
  if (dp != NULL && dp[0] != '\0') {
    tk->dump = fopen(dp, "a");
    const char *dm = getenv("THVM_ATP_RERANK_DUMP_MAX");
    tk->dump_left = (dm != NULL && dm[0] != '\0') ? (u32)atoi(dm) : 400u;
  }
  s->topk = tk;
  return tk;
}

static void atp_topk_goal(const AtpState *s, Term *gl, Term *gr) {
  *gl = 0;
  *gr = 0;
  if (s->goal_lhs != 0) {
    *gl = s->goal_lhs_nf ? s->goal_lhs_nf : s->goal_lhs;
    *gr = s->goal_rhs_nf ? s->goal_rhs_nf : s->goal_rhs;
  }
}

static void atp_topk_gv_append(AtpTopk *tk, u32 seq, Term gl, Term gr) {
  if (tk->n_gv == tk->cap_gv) {
    u32 nc = tk->cap_gv ? 2u * tk->cap_gv : 16u;
    u32 *ns = (u32 *)realloc(tk->gv_seq, nc * sizeof(u32));
    Term *nl = (Term *)realloc(tk->gv_l, nc * sizeof(Term));
    Term *nr = (Term *)realloc(tk->gv_r, nc * sizeof(Term));
    if (ns != NULL) {
      tk->gv_seq = ns;
    }
    if (nl != NULL) {
      tk->gv_l = nl;
    }
    if (nr != NULL) {
      tk->gv_r = nr;
    }
    if (ns == NULL || nl == NULL || nr == NULL) {
      return;
    }
    tk->cap_gv = nc;
  }
  tk->gv_seq[tk->n_gv] = seq;
  tk->gv_l[tk->n_gv] = gl;
  tk->gv_r[tk->n_gv] = gr;
  tk->n_gv++;
}

// Queue insert of row `row` (seq stamped): log the goal normal form when
// it changed since the last push.
static void atp_topk_on_push(AtpState *s, u32 row) {
  if (s->topk == NULL && atp_topk_env_k() == 0u) {
    return;
  }
  AtpTopk *tk = atp_topk_state(s);
  if (tk == NULL) {
    return;
  }
  Term gl, gr;
  atp_topk_goal(s, &gl, &gr);
  if (tk->n_gv > 0u && tk->gv_l[tk->n_gv - 1u] == gl && tk->gv_r[tk->n_gv - 1u] == gr) {
    return;
  }
  atp_topk_gv_append(tk, s->cp_seq[row], gl, gr);
}

// The WM intake re-stamps every queued row before the first pick (the
// export re-emits their 'F' records then): one goal version from seq 0.
static void atp_topk_on_intake(AtpState *s) {
  AtpTopk *tk = (AtpTopk *)s->topk;
  if (tk == NULL) {
    return;
  }
  Term gl, gr;
  atp_topk_goal(s, &gl, &gr);
  tk->n_gv = 0u;
  atp_topk_gv_append(tk, 0u, gl, gr);
}

// Non-orphan pick: cp_seq_next at the pick, the 'S' record's aux.
static void atp_topk_on_select(AtpState *s) {
  AtpTopk *tk = (AtpTopk *)s->topk;
  if (tk == NULL) {
    return;
  }
  if (tk->n_bounds == tk->cap_bounds) {
    u32 nc = tk->cap_bounds ? 2u * tk->cap_bounds : 1024u;
    u32 *nb = (u32 *)realloc(tk->bounds, nc * sizeof(u32));
    if (nb == NULL) {
      return;
    }
    tk->bounds = nb;
    tk->cap_bounds = nc;
  }
  tk->bounds[tk->n_bounds++] = s->cp_seq_next;
}

// #{picks whose cp_seq_next <= seq}: the push step of seq.
static u32 atp_topk_push_step(const AtpTopk *tk, u32 seq) {
  u32 lo = 0u, hi = tk->n_bounds;
  while (lo < hi) {
    u32 mid = lo + (hi - lo) / 2u;
    if (tk->bounds[mid] <= seq) {
      lo = mid + 1u;
    } else {
      hi = mid;
    }
  }
  return lo;
}

// The goal version seq was pushed under: the last one starting at or
// before it.
static void atp_topk_goal_at(const AtpTopk *tk, u32 seq, Term *gl, Term *gr) {
  u32 lo = 0u, hi = tk->n_gv;
  while (lo < hi) {
    u32 mid = lo + (hi - lo) / 2u;
    if (tk->gv_seq[mid] <= seq) {
      lo = mid + 1u;
    } else {
      hi = mid;
    }
  }
  *gl = (lo > 0u) ? tk->gv_l[lo - 1u] : 0;
  *gr = (lo > 0u) ? tk->gv_r[lo - 1u] : 0;
}

// The k best heap positions in heap order: best-first over the binary
// heap (a node's children never precede it), frontier <= k + 1.
static u32 atp_topk_walk(const AtpState *s, u32 k, u32 *pos) {
  u32 fr[2u * ATP_TOPK_MAX + 2u];
  u32 nf = 0u, n = 0u;
  if (s->n_cps == 0u) {
    return 0u;
  }
  fr[nf++] = 0u;
  while (n < k && nf > 0u) {
    u32 b = 0u;
    for (u32 i = 1u; i < nf; i++) {
      if (atp_cp_before(s, fr[i], fr[b])) {
        b = i;
      }
    }
    u32 p = fr[b];
    fr[b] = fr[--nf];
    pos[n++] = p;
    u32 c = 2u * p + 1u;
    if (c < s->n_cps) {
      fr[nf++] = c;
    }
    if (c + 1u < s->n_cps) {
      fr[nf++] = c + 1u;
    }
  }
  return n;
}

static double atp_topk_score(const AtpTopkModel *m, const double *x) {
  double z[ATP_TOPK_NX];
  for (u32 j = 0u; j < ATP_TOPK_NX; j++) {
    z[j] = (x[j] - m->mu[j]) / m->sd[j];
  }
  double out = m->b2;
  for (u32 h = 0u; h < m->hid; h++) {
    const double *w = m->w1 + (size_t)h * ATP_TOPK_NX;
    double a = m->b1[h];
    for (u32 j = 0u; j < ATP_TOPK_NX; j++) {
      a += w[j] * z[j];
    }
    if (a > 0.0) {
      out += m->w2[h] * a;
    }
  }
  return out - m->kappa * x[0];
}

// The weight pick: the heap position to extract (0 = the engine's root).
static u32 atp_topk_pick(AtpState *s) {
  AtpTopk *tk = (AtpTopk *)s->topk;
  const AtpTopkModel *m = &g_atp_topk_model;
  if (!m->loaded || s->n_cps < 2u) {
    return 0u;
  }
  if ((s->use_initial_ultimate || s->use_database_ultimate) && s->cp_ultimate != NULL
      && s->cp_ultimate[s->cp_heap[0].row]) {
    return 0u;
  }
  u64 t0 = atp_now_us();
  u32 pos[ATP_TOPK_MAX];
  u32 k = atp_topk_walk(s, atp_topk_env_k(), pos);
  if (tk->cache_ir != s->n_cp_set_ir_reweighted) {
    tk->n_cache = 0u;
    tk->cache_ir = s->n_cp_set_ir_reweighted;
  }
  u32 seqs[ATP_TOPK_MAX];
  float feat[ATP_TOPK_MAX][ATP_CP_FEATURE_DIM];
  u64 hc = thvm_atp_heap_checkpoint();
  u8 decoded = 0u;
  for (u32 i = 0u; i < k; i++) {
    u32 row = s->cp_heap[pos[i]].row;
    u32 seq = s->cp_seq[row];
    seqs[i] = seq;
    u32 c = 0u;
    while (c < tk->n_cache && tk->c_seq[c] != seq) {
      c++;
    }
    if (c < tk->n_cache) {
      memcpy(feat[i], tk->c_feat[c], sizeof feat[i]);
      continue;
    }
    Term l = 0, r = 0, gl, gr;
    atp_cp_slot_read(s, row, &l, &r);
    atp_topk_goal_at(tk, seq, &gl, &gr);
    atp_cp_features_vs(s, l, r, seq, gl, gr, feat[i]);
    decoded = 1u;
    tk->n_feat++;
  }
  if (decoded) {
    // The decoded pairs are dead: pop them (the reset retires the KBO
    // memo entries of the recycled cells; the persistent LPO memo is
    // keyed on cell values too).
    thvm_atp_heap_reset(hc);
    thvm_lpo_invalidate();
  }
  tk->n_cache = k;
  memcpy(tk->c_seq, seqs, k * sizeof(u32));
  memcpy(tk->c_feat, feat, k * sizeof feat[0]);
  u32 t = s->cp_select_count + 1u;
  double best = 0.0;
  u32 bi = 0u;
  double x[ATP_TOPK_MAX][ATP_TOPK_NX];
  double sc[ATP_TOPK_MAX];
  for (u32 i = 0u; i < k; i++) {
    u32 row = s->cp_heap[pos[i]].row;
    x[i][0] = log1p((double)i);
    x[i][1] = log1p((double)s->n_cps);
    x[i][2] = log1p((double)s->cp_pri[row]);
    for (u32 j = 0u; j < ATP_CP_FEATURE_DIM; j++) {
      x[i][3u + j] = (double)feat[i][j];
    }
    x[i][ATP_TOPK_NX - 1u] = log1p((double)(t - atp_topk_push_step(tk, seqs[i])));
    sc[i] = atp_topk_score(m, x[i]);
    if (i == 0u || sc[i] > best) {
      best = sc[i];
      bi = i;
    }
  }
  if (tk->dump != NULL && tk->dump_left > 0u) {
    for (u32 i = 0u; i < k; i++) {
      fprintf(tk->dump, "%u\t%u\t%u\t%u", t, i, seqs[i], (u32)(i == bi));
      for (u32 j = 0u; j < ATP_TOPK_NX; j++) {
        fprintf(tk->dump, "\t%.17g", x[i][j]);
      }
      fprintf(tk->dump, "\t%.17g\n", sc[i]);
    }
    tk->dump_left--;
  }
  tk->n_pick++;
  if (bi != 0u) {
    tk->n_changed++;
  }
  tk->t_us += atp_now_us() - t0;
  return pos[bi];
}

// GC: the goal versions are Term handles.
static u32 atp_topk_gc_count(const AtpState *s) {
  const AtpTopk *tk = (const AtpTopk *)s->topk;
  return (tk != NULL) ? 2u * tk->n_gv : 0u;
}

static void atp_topk_gc_gather(const AtpState *s, Term *roots, u32 *w) {
  const AtpTopk *tk = (const AtpTopk *)s->topk;
  if (tk == NULL) {
    return;
  }
  for (u32 i = 0u; i < tk->n_gv; i++) {
    roots[(*w)++] = tk->gv_l[i];
    roots[(*w)++] = tk->gv_r[i];
  }
}

static void atp_topk_gc_writeback(AtpState *s, const Term *roots, u32 base) {
  AtpTopk *tk = (AtpTopk *)s->topk;
  if (tk == NULL) {
    return;
  }
  for (u32 i = 0u; i < tk->n_gv; i++) {
    tk->gv_l[i] = roots[base + 2u * i];
    tk->gv_r[i] = roots[base + 2u * i + 1u];
  }
}

static void atp_topk_free(AtpState *s) {
  AtpTopk *tk = (AtpTopk *)s->topk;
  if (tk == NULL) {
    return;
  }
  fprintf(stderr, "%% topk-rerank: K=%u picks=%llu changed=%llu feature_calls=%llu "
          "time=%.3fs per_pick=%.1fus\n", atp_topk_env_k(),
          (unsigned long long)tk->n_pick, (unsigned long long)tk->n_changed,
          (unsigned long long)tk->n_feat, (double)tk->t_us / 1e6,
          tk->n_pick ? (double)tk->t_us / (double)tk->n_pick : 0.0);
  if (tk->dump != NULL) {
    fclose(tk->dump);
  }
  free(tk->bounds);
  free(tk->gv_seq);
  free(tk->gv_l);
  free(tk->gv_r);
  free(tk);
  s->topk = NULL;
}
