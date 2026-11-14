// The walk former's treated critical pairs, treated in parallel and
// pushed in formation order.
//
// The walk former (step.c) formed each critical pair and pushed it at
// once: the treatment (doR normal forms of both sides, about half of an
// OrAssociativity run), the joined drop, the gates, the trace record and
// the queue insert.  Under atp_push_batch_on the treated FT route instead
// hands the formed pair here, and atp_push_batch_flush first treats the
// chunk, split into contiguous runs over the worker threads, then runs
// the push tail per pair in formation order on the calling thread.  The
// treatment reads only the rule set, its mirrors and its discrimination
// tree, which no push changes, and records no trace, so the trace and the
// queue come out byte-identical at any thread count.  A route that pushes
// outside the batch flushes it first (atp_push_cps_traced refuses a push
// while one is pending).
//
// A worker normalizes with its own arena (g_ft_arena_tl), its own
// subterm-NF memo and index scratch (thread-local in ft_norm.c), and
// never polls the host abort hook.  The knobs the treatment caches on
// first read are atomic, since a worker may be the first to read one.
// Cells move between arenas, so with workers every arena frees loosely
// (AtpFt.loose).

// A caller's thread request for the next runs, 0 for the default (the
// THVMLink bridge sets it per run: the completion handle that Simplify
// calls inside the kernel asks for the kernel's thread setting, at most
// 4; TFindProof leaves the default).
static u32 g_atp_push_threads_req = 0u;
static void thvm_atp_set_push_threads(u32 n) { g_atp_push_threads_req = n; }

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
#include <pthread.h>
#ifdef __linux__
#include <sched.h>
#endif

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

// Normalizing threads, the caller included: THVM_ATP_PUSH_THREADS, else
// the caller's request (thvm_atp_set_push_threads), else the CPUs this
// process may run on (sched_getaffinity, so a cpuset is honoured), at
// most 8.
static u32 atp_push_threads(void) {
  static long env = -1;
  static u32  cpus = 0u;
  if (env < 0) {
    const char *e = getenv("THVM_ATP_PUSH_THREADS");
    long v = (e != NULL) ? strtol(e, NULL, 10) : 0;
    env = (v > 0) ? v : 0;
  }
  if (env > 0) return (u32)env;
  if (g_atp_push_threads_req != 0u) return g_atp_push_threads_req;
  if (cpus == 0u) {
#if defined(__linux__) && defined(CPU_COUNT)
    cpu_set_t cs;
    CPU_ZERO(&cs);
    cpus = (sched_getaffinity(0, sizeof cs, &cs) == 0) ? (u32)CPU_COUNT(&cs) : 1u;
#else
    long c = sysconf(_SC_NPROCESSORS_ONLN);
    cpus = (c > 0) ? (u32)c : 1u;
#endif
    if (cpus == 0u) cpus = 1u;
    if (cpus > 8u) cpus = 8u;
  }
  return cpus;
}

// Threads from which batching is on by default.  With two the batch
// overhead (the raw emit of pairs that join, colder cells, the spill)
// outweighs the split: OrAssociativity 13.66 -> 13.79 s and Sheffer
// 94.3 -> 98.0 s, against 11.87 s and 90.4 s at four.
enum { ATP_PUSH_BATCH_MIN_THREADS = 4 };

// Pairs per flush (THVM_ATP_PUSH_BATCH_CHUNK).  A chunk keeps its cells
// in cache between formation, treatment and the pack; with one thread a
// chunk of 8 costs OrAssociativity 3%, 128 costs 9%, and whole-fact
// batches (thousands of pairs) cost 41%, with identical work counts.
static u32 atp_push_batch_chunk(void) {
  static long env = -1;
  if (env < 0) {
    const char *e = getenv("THVM_ATP_PUSH_BATCH_CHUNK");
    long v = (e != NULL) ? strtol(e, NULL, 10) : 0;
    env = (v > 0) ? v : 0;
  }
  if (env > 0) return (u32)env;
  return atp_push_threads() >= ATP_PUSH_BATCH_MIN_THREADS ? 2048u : 8u;
}

// The treated route batches unless a push stage reads formation state
// the batch does not carry: the simpler-join peak (built from the walk
// operands), the definitional-constant keep lane (clones the raw cells
// before the treatment) and the guard strip (re-forms the raw pair).
// THVM_ATP_PUSH_BATCH=0 / 1 forces it off / on; by default it is on from
// ATP_PUSH_BATCH_MIN_THREADS threads.
static int atp_push_batch_on(const AtpState *s) {
  static int env = -2;
  if (env == -2) {
    const char *e = getenv("THVM_ATP_PUSH_BATCH");
    env = (e != NULL && e[0] == '0') ? 0 : (e != NULL && e[0] == '1') ? 1 : -1;
  }
  int on = (env >= 0) ? env : (atp_push_threads() >= ATP_PUSH_BATCH_MIN_THREADS ? 1 : 0);
  return on && !s->use_simpler_join && !atp_drc_armed(s) &&
         !atp_ifeq_strip_armed(s);
}

// Whether a flush may treat on several threads: not with AC matching
// (it allocates in the arena's shared scratch region), the WMFPA mirror
// cache, the opt-in NF cap (it reads the arena's live count) or the
// detail profile (its per-node counters reallocate unguarded).
static int atp_push_batch_parallel_ok(const AtpState *s) {
  if (g_ft_normcore2 < 0) ft_normcore2_resolve();
#ifdef THVM_ATP_AC
  if (g_atp_ac_info.ac_mask != 0ull) return 0;
#endif
  return atp_push_threads() > 1u && !s->use_wmfpa && !g_ft_nfcap_on &&
         !g_atp_phase_detail;
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

// The doR normal forms of both sides of g_pb[lo, hi).
static void atp_push_batch_treat(AtpState *s, u32 lo, u32 hi) {
  for (u32 k = lo; k < hi; k++) {
    g_pb[k].fl = atp_rules_only_normalize_ft(s, g_pb[k].fl, 64u);
    g_pb[k].fr = atp_rules_only_normalize_ft(s, g_pb[k].fr, 64u);
  }
}

// --- the worker pool --------------------------------------------------

// A worker frees far more than it allocates (a redex outweighs its
// contractum, 36M cells freed against 10M allocated on OrAssociativity,
// and the caller frees the normal forms the worker built), so a worker
// arena keeps up to THVM_ATP_PUSH_REUSE_CAP freed cells for its own
// allocations and spills the rest, which the caller's arena takes after
// each parallel flush and uses once its own freed cells run out.  The cap
// trades speed for a bound on memory: the caller pays a cache miss per
// spilled cell it reuses (they were last touched on another core) where
// a fresh slab streams.  OrAssociativity at 8 threads: never spilling,
// 10.0 s and +66% peak RSS (the surplus grows with the run); 1M cells,
// 11.2 s and +20%; 16K cells, 11.9 s and +8%; serial, 13.7 s.
enum { ATP_PUSH_REUSE_CAP = 1048576 };


typedef struct AtpPushPool_s AtpPushPool;

typedef struct {
  AtpPushPool *pool;
  pthread_t    th;
  AtpFt        arena;
  u32          part;      // its run of each chunk: 1..n
} AtpPushWorker;

struct AtpPushPool_s {
  AtpState        *s;
  u32              n;        // worker threads; the caller treats part 0
  u32              n_arenas; // worker arenas initialized (>= n)
  AtpPushWorker   *w;
  pthread_mutex_t  mu;
  pthread_cond_t   go, done;
  u64              gen;      // job generation, bumped per flush
  u32              running;  // workers still treating the current chunk
  u32              n_ents;   // the current chunk: g_pb[0, n_ents)
  u8               quit;
};

// Part k of n_parts contiguous runs over [0, n): pairs formed in order
// stay together, so each thread's cells stay warm.
static void atp_push_part(u32 n, u32 n_parts, u32 k, u32 *lo, u32 *hi) {
  *lo = (u32)(((u64)n * k) / n_parts);
  *hi = (u32)(((u64)n * (k + 1u)) / n_parts);
}

static void *atp_push_worker_main(void *arg) {
  AtpPushWorker *w = (AtpPushWorker *)arg;
  AtpPushPool *p = w->pool;
  g_ft_arena_tl = &w->arena;
  g_atp_norm_worker = 1u;
  u64 seen = 0u;
  pthread_mutex_lock(&p->mu);
  for (;;) {
    while (!p->quit && p->gen == seen) pthread_cond_wait(&p->go, &p->mu);
    if (p->quit) break;
    seen = p->gen;
    u32 n = p->n_ents;
    pthread_mutex_unlock(&p->mu);
    u32 lo, hi;
    atp_push_part(n, p->n + 1u, w->part, &lo, &hi);
    atp_push_batch_treat(p->s, lo, hi);
    pthread_mutex_lock(&p->mu);
    if (--p->running == 0u) pthread_cond_signal(&p->done);
  }
  pthread_mutex_unlock(&p->mu);
  // this thread's memo and index scratch
  free(g_ftnfm);
  g_ftnfm = NULL;
  free(g_ftps.cand);
  free(g_ftps.cand_tmp);
  free(g_ftps.dtb_snap);
  free(g_ftps.dtb_rec);
  memset(&g_ftps, 0, sizeof g_ftps);
  return NULL;
}

// The state's pool, started on first use: atp_push_threads() - 1 workers,
// each with its own arena.  NULL if no thread could start.
static AtpPushPool *atp_push_pool(AtpState *s) {
  if (s->push_pool != NULL) return (AtpPushPool *)s->push_pool;
  u32 n = atp_push_threads() - 1u;
  AtpPushPool *p = (AtpPushPool *)calloc(1, sizeof *p);
  if (p != NULL) p->n_arenas = n;
  AtpPushWorker *w = (AtpPushWorker *)calloc(n, sizeof *w);
  if (p == NULL || w == NULL) thvm_fatal("atp_push_pool: OOM");
  p->s = s;
  p->w = w;
  pthread_mutex_init(&p->mu, NULL);
  pthread_cond_init(&p->go, NULL);
  pthread_cond_init(&p->done, NULL);
  ((AtpFt *)s->ft_arena_ptr)->loose = 1u;
  const char *rc = getenv("THVM_ATP_PUSH_REUSE_CAP");
  u64 reuse_cap = (rc != NULL) ? (u64)strtoull(rc, NULL, 10) : ATP_PUSH_REUSE_CAP;
  for (u32 k = 0; k < n; k++) {
    w[k].pool = p;
    w[k].part = k + 1u;
    ft_init(&w[k].arena);
    w[k].arena.loose = 1u;
    w[k].arena.reuse_cap = reuse_cap;
    if (pthread_create(&w[k].th, NULL, atp_push_worker_main, &w[k]) != 0) break;
    p->n = k + 1u;
  }
  s->push_pool = p;
  return p;
}

// Live cells over the pool's arenas (each may have wrapped; the sum with
// the state's own arena is exact).
static u64 atp_push_pool_live(const AtpState *s) {
  const AtpPushPool *p = (const AtpPushPool *)s->push_pool;
  if (p == NULL) return 0u;
  u64 n = 0u;
  for (u32 k = 0; k < p->n_arenas; k++) n += p->w[k].arena.n_persistent_alive;
  return n;
}

// Drops a pending batch whose state is going away: a run that ends on a
// longjmp (heap exhaustion in the push tail) can leave pairs pending, and
// their cells die with the state's arenas.
static void atp_push_batch_reset(void) {
  g_pb_n = 0u;
  g_pb_bytes_n = 0u;
  g_pb_mid = 0u;
  g_pb_flushing = 0u;
}

static void atp_push_pool_free(AtpState *s) {
  atp_push_batch_reset();
  AtpPushPool *p = (AtpPushPool *)s->push_pool;
  if (p == NULL) return;
  pthread_mutex_lock(&p->mu);
  p->quit = 1u;
  pthread_cond_broadcast(&p->go);
  pthread_mutex_unlock(&p->mu);
  for (u32 k = 0; k < p->n; k++) pthread_join(p->w[k].th, NULL);
  for (u32 k = 0; k < p->n_arenas; k++) {
    if (p->w[k].arena.session != 0u) ft_destroy(&p->w[k].arena);
  }
  pthread_cond_destroy(&p->done);
  pthread_cond_destroy(&p->go);
  pthread_mutex_destroy(&p->mu);
  free(p->w);
  free(p);
  s->push_pool = NULL;
}

static void atp_push_pool_rebalance(AtpPushPool *p, AtpFt *main_arena) {
  for (u32 k = 0; k < p->n_arenas; k++) {
    AtpFt *a = &p->w[k].arena;
    if (a->spill_head == NULL) continue;
    // onto the caller's spill pool, which it takes only after its own
    // freed cells (warm in its cache) run out
    a->spill_tail->next = main_arena->spill_head;
    if (main_arena->spill_head == NULL) main_arena->spill_tail = a->spill_tail;
    main_arena->spill_head = a->spill_head;
    a->spill_head = a->spill_tail = NULL;
  }
}

// Ends the pool's threads when the run ends, so a state kept for queries
// (TAtpSystem) holds no idle threads, and hands every freed cell the
// workers kept to the caller's arena.  The worker arenas stay until the
// state is freed, since their slabs hold live cells; a later run on the
// state treats on the caller.
static void atp_push_pool_stop(AtpState *s) {
  AtpPushPool *p = (AtpPushPool *)s->push_pool;
  if (p == NULL || p->n == 0u) return;
  pthread_mutex_lock(&p->mu);
  p->quit = 1u;
  pthread_cond_broadcast(&p->go);
  pthread_mutex_unlock(&p->mu);
  for (u32 k = 0; k < p->n; k++) pthread_join(p->w[k].th, NULL);
  p->n = 0u;
  AtpFt *main_arena = (AtpFt *)s->ft_arena_ptr;
  for (u32 k = 0; k < p->n_arenas; k++) {
    AtpFt *a = &p->w[k].arena;
    if (a->free_head == NULL) continue;
    AtpFtCell *tail = a->free_head;
    while (tail->next != NULL) tail = tail->next;
    tail->next = a->spill_head;
    if (a->spill_head == NULL) a->spill_tail = tail;
    a->spill_head = a->free_head;
    a->free_head = NULL;
    a->n_reuse = 0u;
  }
  atp_push_pool_rebalance(p, main_arena);
}

// Fewer pairs than this are treated on the caller: the wake-up costs more
// than the split saves (the lazy route's untreated pairs cut many flushes
// short).
enum { ATP_PUSH_PARALLEL_MIN = 64 };

// Treats g_pb[0, g_pb_n): on the pool when the chunk is large enough and
// may run in parallel, else here.  The mirrors and the tree are built
// here first, so the workers only read them.
static void atp_push_batch_treat_all(AtpState *s) {
  atp_ft_mirror_ensure(s);
  if (ftpi_applicable(s, 0u, s->n_rules)) ftpi_ensure(s);
  AtpPushPool *p = (g_pb_n >= ATP_PUSH_PARALLEL_MIN && atp_push_batch_parallel_ok(s))
                       ? atp_push_pool(s) : NULL;
  if (p == NULL || p->n == 0u) {
    atp_push_batch_treat(s, 0u, g_pb_n);
    return;
  }
  pthread_mutex_lock(&p->mu);
  p->n_ents = g_pb_n;
  p->running = p->n;
  p->gen++;
  pthread_cond_broadcast(&p->go);
  pthread_mutex_unlock(&p->mu);
  u32 lo, hi;
  atp_push_part(g_pb_n, p->n + 1u, 0u, &lo, &hi);
  atp_push_batch_treat(s, lo, hi);
  pthread_mutex_lock(&p->mu);
  while (p->running != 0u) pthread_cond_wait(&p->done, &p->mu);
  pthread_mutex_unlock(&p->mu);
  atp_push_pool_rebalance(p, (AtpFt *)s->ft_arena_ptr);
}

// Treats every batched pair, then pushes them in formation order.
// Returns the number queued.
static u32 atp_push_batch_flush(AtpState *s) {
  if (g_pb_n == 0u) return 0u;
  u64 t0 = g_atp_phase_enabled ? atp_now_us() : 0u;
  atp_push_batch_treat_all(s);
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
#elif defined(THVM_ATPFT_RULES)
static u64 atp_push_pool_live(const AtpState *s) { (void)s; return 0u; }
static void atp_push_pool_stop(AtpState *s) { (void)s; }
static void atp_push_pool_free(AtpState *s) { (void)s; }
#endif
