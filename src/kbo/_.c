// thvm_kbo - Knuth-Bendix ordering on first-order terms.
//
// Compares two terms over a signature of TAG_CTR (function symbols)
// and TAG_FVR (variables) under a KBO config (per-symbol weights +
// total precedence + scalar variable weight w0).  Stage 2 of the
// IC-native ATP roadmap (docs/plans/waldmeister_ic_atp.md).
//
// Returns:
//   KBO_EQ : s and t are structurally identical
//   KBO_GT : s > t under KBO
//   KBO_LT : s < t under KBO
//   KBO_UN : incomparable
//
// Algorithm (Baader-Nipkow):
//   1. Domination check: for each variable y, count(s,y) and count(t,y).
//      If neither side dominates, UN.
//   2. Compare total weights.
//   3. On weight tie, compare top symbols by precedence; on precedence
//      tie, recurse lexicographically on arguments.
//   4. Variables compare equal only to themselves; UN against anything
//      else of equal weight.
//
// Two implementations live here:
//   * thvm_kbo_naive: the original Baader-Nipkow recursion.  Each
//     lexicographic level re-runs a full weight/var-balance traversal
//     and zeroes a fixed array -- O(n^2) on a left/right spine.  Kept
//     as the reference oracle for differential testing.
//   * thvm_kbo (production): Loechner's linear-time comparison
//     ("Things to Know when Implementing KBO", JAR 36(4):289-310,
//     2006), ported from Waldmeister's sources/ORD/KBO.c
//     (Vortest / KBOEntscheidungRek / KBOEntscheidungLex).  Weight
//     balance (phidiff) and variable balance are threaded through a
//     single combined traversal; the lexicographic tie-break reuses
//     the already-computed balances and only inspects the first
//     differing argument.  Touched variable ids are tracked so the
//     balance table is cleared in O(#touched) rather than memset.
//
// Both use fixed-size arrays sized by KBO_MAX_VAR.  No heap
// allocation, no IC reduction.

#define KBO_MAX_VAR 64

// === structural equality ============================================

static u8 kbo_eq(Term s, Term t) {
  if (s == t) return 1;   // pointer-identical -- rewriting/subst_apply
                          // share subterm cells, so this fires often
  if (term_tag(s) != term_tag(t)) return 0;
  if (term_ext(s) != term_ext(t)) return 0;
  switch (term_tag(s)) {
    case TAG_FVR: return 1;  // same id (ext) already checked
    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return 0;
      for (u32 i = 0; i < ns; i++) {
        if (!kbo_eq(term_ctr_at(s, i), term_ctr_at(t, i))) return 0;
      }
      return 1;
    }
    default: return term_val(s) == term_val(t);
  }
}

// === naive reference oracle =========================================

// Add term `t` entirely to one side of the balance: weight*sign into
// *wb, per-variable occurrence counts*sign into bal[].  kbo_diff uses
// this for the subtrees where the two compared terms diverge.
static void kbo_addto(Term t, int sign, const KboConfig *cfg,
                      long long *wb, int *bal) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < KBO_MAX_VAR) bal[id] += sign;
      *wb += (long long)sign * (long long)cfg->var_weight;
      return;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      *wb += (long long)sign *
             (long long)((lab < cfg->n_labels) ? cfg->weights[lab] : 0);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        kbo_addto(term_ctr_at(t, i), sign, cfg, wb, bal);
      return;
    }
    default: return;
  }
}

// Simultaneous diff-traversal of `s` and `t` -- Waldmeister's Vortest
// (ORD/KBO.c).  One pass accumulates weight(s)-weight(t) into *wb and
// count(s,v)-count(t,v) into bal[]: matching nodes contribute nothing
// and cancel exactly; only the subtrees where s and t diverge are
// summed.  Returns 1 iff s and t are structurally identical.
static int kbo_diff(Term s, Term t, const KboConfig *cfg,
                    long long *wb, int *bal) {
  if (s == t) return 1;                              // identical subtree
  u32 tg = term_tag(s);
  if (tg == term_tag(t) && term_ext(s) == term_ext(t)) {
    if (tg == TAG_FVR) return 1;                     // same variable
    if (tg == TAG_CTR) {
      u32 ns = term_ctr_n(s), nt = term_ctr_n(t);
      if (ns == nt) {
        int ident = 1;
        for (u32 i = 0; i < ns; i++)
          ident &= kbo_diff(term_ctr_at(s, i), term_ctr_at(t, i),
                            cfg, wb, bal);
        return ident;
      }
    } else if (term_val(s) == term_val(t)) {
      return 1;                                      // equal atom (NUM, ...)
    }
  }
  // diverge: s wholly on the + side, t wholly on the - side.
  kbo_addto(s, +1, cfg, wb, bal);
  kbo_addto(t, -1, cfg, wb, bal);
  return 0;
}

static KboCmp kbo_rec(Term s, Term t, const KboConfig *cfg);

// Naive Baader-Nipkow comparator (reference oracle).
static KboCmp thvm_kbo_naive(Term s, Term t, const KboConfig *cfg) {
  return kbo_rec(s, t, cfg);
}

static KboCmp kbo_rec(Term s, Term t, const KboConfig *cfg) {
  long long wb = 0;                       // weight(s) - weight(t)
  int bal[KBO_MAX_VAR] = {0};              // count(s,v) - count(t,v)
  if (kbo_diff(s, t, cfg, &wb, bal)) return KBO_EQ;

  u8 s_dom = 1, t_dom = 1;                 // s/t dominates: balance never -/+
  for (u32 i = 0; i < KBO_MAX_VAR; i++) {
    if (bal[i] < 0) s_dom = 0;
    if (bal[i] > 0) t_dom = 0;
  }
  if (!s_dom && !t_dom) return KBO_UN;

  if (wb > 0) return s_dom ? KBO_GT : KBO_UN;
  if (wb < 0) return t_dom ? KBO_LT : KBO_UN;

  // weights equal: compare top symbols
  u8 s_is_fvr = (term_tag(s) == TAG_FVR);
  u8 t_is_fvr = (term_tag(t) == TAG_FVR);
  if (s_is_fvr || t_is_fvr) return KBO_UN;
  if (term_tag(s) != TAG_CTR || term_tag(t) != TAG_CTR) return KBO_UN;

  u32 lab_s = term_ext(s);
  u32 lab_t = term_ext(t);
  u32 prec_s = (lab_s < cfg->n_labels) ? cfg->precedence[lab_s] : 0;
  u32 prec_t = (lab_t < cfg->n_labels) ? cfg->precedence[lab_t] : 0;
  if (prec_s > prec_t) return s_dom ? KBO_GT : KBO_UN;
  if (prec_s < prec_t) return t_dom ? KBO_LT : KBO_UN;

  // same top symbol: lex comparison of args
  u32 ns = term_ctr_n(s);
  u32 nt = term_ctr_n(t);
  if (ns != nt) return KBO_UN;
  for (u32 i = 0; i < ns; i++) {
    KboCmp c = kbo_rec(term_ctr_at(s, i), term_ctr_at(t, i), cfg);
    if (c == KBO_EQ) continue;
    if (c == KBO_GT) return s_dom ? KBO_GT : KBO_UN;
    if (c == KBO_LT) return t_dom ? KBO_LT : KBO_UN;
    return KBO_UN;
  }
  return KBO_EQ;
}

// === Loechner linear comparator =====================================
//
// State for one comparison.  `bal[v]` is count(s,v) - count(t,v) for
// the *current* Vortest scope; `touched[0..n_touched)` lists the var
// ids with a nonzero entry so the table clears in O(#touched).  A var
// id is pushed onto `touched` the first time its balance leaves 0.

typedef struct {
  const KboConfig *cfg;
  int  bal[KBO_MAX_VAR];
  u32  touched[KBO_MAX_VAR];
  u32  n_touched;
} KboLin;

static inline u32 kbo_weight(const KboConfig *cfg, Term t) {
  u32 lab = term_ext(t);
  return (lab < cfg->n_labels) ? cfg->weights[lab] : 0;
}
static inline u32 kbo_prec(const KboConfig *cfg, Term t) {
  u32 lab = term_ext(t);
  return (lab < cfg->n_labels) ? cfg->precedence[lab] : 0;
}

// Read a CTR node's child count + child-cell base in ONE pair of heap
// reads, so the per-child traversal in the KBO hot loops indexes the
// contiguous child cells directly (heap_read(base + i)) instead of
// re-deriving the count through term_ctr_n on every term_ctr_at call.
// thvm's CTR stores [count, child0, child1, ...] contiguously, so the
// children base is term_val(t)+1.  KBO is ~half the saturation wall on
// the single-symbol Sheffer signature and its recursion node count
// equals the term size, so dropping one redundant header read per child
// access is a measurable inner-loop win.
static inline u32 kbo_ctr_children(Term t, u64 *base) {
  u64 loc = term_val(t);
  *base = loc + 1u;
  return (u32)term_val(heap_read(loc));
}

// Bump variable `id`'s balance by `delta`, recording first touch.
static inline void kbo_bump(KboLin *st, u32 id, int delta) {
  if (id >= KBO_MAX_VAR) return;
  int prev = st->bal[id];
  if (prev == 0) st->touched[st->n_touched++] = id;
  st->bal[id] = prev + delta;
}

// === per-term weight + variable-profile memo ========================
//
// kbo_lin_addto re-walks a whole divergent subtree on EVERY compare to
// accumulate its total KBO weight (Σ symbol-weight) into phidiff and its
// per-variable occurrence counts into bal[].  The profile puts that walk
// at ~half the comparator self-time on the single-symbol Sheffer
// signature, where the same subterm cells recur across millions of
// compares.  Memoize, per CTR subtree, BOTH its total weight AND a compact
// (var_id, count) profile, so applying the subtree to a balance scope is
// O(#distinct vars) -- a handful on Sheffer terms -- instead of
// O(|subtree|).  A ground subtree (no in-range variable) is a single
// phidiff add.  The profile is what makes the memo a net win: the terms
// here are variable-bearing, so caching ONLY the weight (and still walking
// for the var bumps) is no faster than the original single combined pass.
//
// Direct-mapped, epoch-stamped exactly like the LPO (s,t)->verdict memo:
// the ATP engine bumps the epoch (thvm_kbo_invalidate) on every GC, so
// stale entries from before a cell relocation are simply not matched -- no
// clearing.  Term cells are pointer-stable between GCs, so the cell value
// is a sound key.  Weight + var profile depend only on the term structure
// (and cfg->weights / var_weight, fixed within a run; a config change
// re-bumps the epoch).  Collisions overwrite, paying a recompute, never a
// wrong value.  A subtree with more than KBO_VPROF_CAP distinct in-range
// variables is NOT profile-cached (vc == KBO_VPROF_OVF); its weight is
// still cached and the var bumps fall back to a recursive walk.
#define KBO_WMEMO_BITS 16
#define KBO_WMEMO_SIZE (1u << KBO_WMEMO_BITS)
#define KBO_WMEMO_MASK (KBO_WMEMO_SIZE - 1u)
#define KBO_VPROF_CAP  6u           // distinct vars cached inline per node
#define KBO_VPROF_OVF  0xFFu        // sentinel: too many distinct vars
// Per-CTR memo entry keyed by STRUCTURAL hash of the subtree (not by
// raw Term ID).  thvm doesn't hash-cons cells, so two structurally-
// equal CTR terms get distinct Term IDs -- under the previous
// Term-ID keying the memo missed every such pair, capping hit rate
// around 35% on saturating workloads (project_atp_wm_sheffer_lpo).
// Keying by struct hash lets all structurally-equal subterms share
// the entry.  Hash collisions (two structurally distinct subterms
// hashing identically) are ~2^-64 per pair -- negligible.
typedef struct {
  u64       hash;                   // structural hash of the subtree
  u32       epoch;
  long long w;                      // total KBO weight of the subtree
  u8        vc;                     // #distinct in-range vars, or _OVF
  u8        vid[KBO_VPROF_CAP];     // variable ids
  i16       vcnt[KBO_VPROF_CAP];    // occurrence counts (signed for bump)
} KboWMemoEnt;
static KboWMemoEnt g_kbo_wmemo[KBO_WMEMO_SIZE];
static u32 g_kbo_epoch = 1;
static const KboConfig *g_kbo_last_cfg = NULL;

// Per-Term-ID cache: Term ID -> (struct hash, wmemo slot index).
// First visit of a Term ID pays the bottom-up walk (which fills both
// this cache + g_kbo_wmemo); subsequent visits of the SAME Term ID
// are O(1) (read hash, jump to slot).  Sized identically to wmemo.
#define KBO_THASH_BITS 16
#define KBO_THASH_SIZE (1u << KBO_THASH_BITS)
#define KBO_THASH_MASK (KBO_THASH_SIZE - 1u)
typedef struct {
  Term t;
  u64  hash;
  u32  epoch;
  u32  wmemo_idx;
} KboTHashEnt;
static KboTHashEnt g_kbo_thash[KBO_THASH_SIZE];

static inline u32 kbo_thash_slot(Term t) {
  u64 h = (u64)t * 0x9E3779B97F4A7C15ull;
  h ^= h >> 29;
  return (u32)h & KBO_THASH_MASK;
}

// Slot a structural hash into the wmemo table.
static inline u32 kbo_wmemo_slot(u64 sh) {
  return (u32)(sh ^ (sh >> 29)) & KBO_WMEMO_MASK;
}

// Compute a leaf's contribution to the structural hash.  Inlined so
// the iterative combine pass can mix child hashes without an O(|t|)
// recursive walk per child -- children's hashes come from their
// already-filled g_kbo_thash entry (when CTR) or from inline compute
// (when FVR/atom).
static inline u64 kbo_leaf_hash(Term t) {
  u32 tg = term_tag(t);
  if (tg == TAG_FVR) {
    return 0xfacefacefaceull ^ ((u64)term_ext(t) * 0x100000001b3ull);
  }
  if (tg == TAG_CTR) {
    // A CTR child's hash MUST come from g_kbo_thash (filled by an
    // earlier post-order combine step).  Caller invariant: every
    // CTR child has been visited before its parent's combine fires.
    KboTHashEnt *ce = &g_kbo_thash[kbo_thash_slot(t)];
    if (ce->epoch == g_kbo_epoch && ce->t == t) return ce->hash;
    // Slot evicted under collision; fall back to inline recurse.
    // (Rare -- only on a same-walk thash collision.)
    u32 lab = term_ext(t);
    u64 base; u32 n = kbo_ctr_children(t, &base);
    u64 h = 0xcbf29ce484222325ull ^ ((u64)lab * 0x100000001b3ull);
    for (u32 i = 0; i < n; i++) {
      h ^= kbo_leaf_hash(heap_read(base + i));
      h *= 0x100000001b3ull;
    }
    h ^= (u64)n * 0x9e3779b97f4a7c15ull;
    return h;
  }
  return 0xdeadbeefcafebabeull ^ ((u64)tg * 0x100000001b3ull);
}

// Invalidate the whole per-term memo in O(1) by bumping the epoch.
fn void thvm_kbo_invalidate(void) {
  if (++g_kbo_epoch == 0u) {
    for (u32 i = 0; i < KBO_WMEMO_SIZE; i++) g_kbo_wmemo[i].epoch = 0u;
    for (u32 i = 0; i < KBO_THASH_SIZE; i++) g_kbo_thash[i].epoch = 0u;
    g_kbo_epoch = 1u;
  }
}

// Accumulate var id `id`'s count by `delta` into a small (id,count) list
// being built for the memo.  Returns 1 on success, 0 if the list would
// exceed KBO_VPROF_CAP distinct ids (caller marks the node overflowed).
static inline int kbo_vprof_add(u8 *vid, i16 *vcnt, u8 *vc, u32 id, int delta) {
  for (u8 k = 0; k < *vc; k++) {
    if (vid[k] == (u8)id) {
      int sum = (int)vcnt[k] + delta;
      if (sum > 32767 || sum < -32768) return 0;   // i16 range guard
      vcnt[k] = (i16)sum;
      return 1;
    }
  }
  if (*vc >= KBO_VPROF_CAP) return 0;
  if (delta > 32767 || delta < -32768) return 0;
  vid[*vc] = (u8)id; vcnt[*vc] = (i16)delta; (*vc)++;
  return 1;
}

static KboWMemoEnt *kbo_subtree_memo(const KboConfig *cfg, Term t);

// Combine one CTR node's own weight + the profiles of its children
// into the memo slot for `t`.  Computes the node's structural hash
// from its children's hashes (read from g_kbo_thash, filled by
// earlier post-order steps), looks up g_kbo_wmemo by that hash:
//   - HIT: structurally-equal term already cached -- reuse weight +
//     profile, only need to register `t` in g_kbo_thash.
//   - MISS: compute weight + profile, fill the wmemo slot, register
//     t->(hash, slot) in g_kbo_thash.
// This is the actual perf lever -- the bench's saturator generates
// many fresh Term IDs for the same structural shape, so the wmemo
// hit rate jumps from ~35% (Term-ID keyed) toward ~70% (struct-hash
// keyed).
static KboWMemoEnt *kbo_memo_combine(const KboConfig *cfg, Term t) {
  // Compute this node's structural hash from children (children's
  // hashes are in g_kbo_thash since post-order put them there first).
  u32 lab = term_ext(t);
  u64 sh = 0xcbf29ce484222325ull ^ ((u64)lab * 0x100000001b3ull);
  u64 base; u32 n = kbo_ctr_children(t, &base);
  for (u32 i = 0; i < n; i++) {
    sh ^= kbo_leaf_hash(heap_read(base + i));
    sh *= 0x100000001b3ull;
  }
  sh ^= (u64)n * 0x9e3779b97f4a7c15ull;

  // Register this Term ID in the per-Term-ID cache.
  u32 thidx = kbo_thash_slot(t);
  KboTHashEnt *th = &g_kbo_thash[thidx];
  u32 widx = kbo_wmemo_slot(sh);
  th->t = t; th->hash = sh; th->epoch = g_kbo_epoch; th->wmemo_idx = widx;

  // Wmemo hit?  Reuse precomputed weight + profile.
  KboWMemoEnt *e = &g_kbo_wmemo[widx];
  if (e->epoch == g_kbo_epoch && e->hash == sh) return e;

  // Wmemo miss: compute weight + profile.
  long long w = (long long)kbo_weight(cfg, t);
  u8 vc = 0u;
  u8 vid[KBO_VPROF_CAP];
  i16 vcnt[KBO_VPROF_CAP];
  int ovf = 0;
  for (u32 i = 0; i < n; i++) {
    Term c = heap_read(base + i);
    if (term_tag(c) == TAG_FVR) {
      w += (long long)cfg->var_weight;
      u32 id = term_ext(c);
      if (id < KBO_MAX_VAR && !ovf) {
        if (!kbo_vprof_add(vid, vcnt, &vc, id, +1)) ovf = 1;
      }
    } else if (term_tag(c) == TAG_CTR) {
      // Child must have a g_kbo_thash entry (post-order); from that,
      // read the wmemo slot.  If the slot still holds the child's
      // entry, use it; else recompute via kbo_subtree_memo.
      KboTHashEnt *cth = &g_kbo_thash[kbo_thash_slot(c)];
      KboWMemoEnt *ce;
      if (cth->epoch == g_kbo_epoch && cth->t == c) {
        ce = &g_kbo_wmemo[cth->wmemo_idx];
        if (!(ce->epoch == g_kbo_epoch && ce->hash == cth->hash)) {
          ce = kbo_subtree_memo(cfg, c);
        }
      } else {
        ce = kbo_subtree_memo(cfg, c);
      }
      w += ce->w;
      if (ce->vc == KBO_VPROF_OVF) ovf = 1;
      else if (!ovf) {
        for (u8 k = 0; k < ce->vc; k++) {
          if (!kbo_vprof_add(vid, vcnt, &vc, ce->vid[k], ce->vcnt[k])) {
            ovf = 1; break;
          }
        }
      }
    }
    // a non-CTR/non-FVR child contributes 0 weight, no vars.
  }
  e->hash = sh; e->epoch = g_kbo_epoch; e->w = w;
  if (ovf) {
    e->vc = KBO_VPROF_OVF;
  } else {
    e->vc = vc;
    for (u8 k = 0; k < vc; k++) { e->vid[k] = vid[k]; e->vcnt[k] = vcnt[k]; }
  }
  return e;
}

// Explicit-stack post-order traversal used to fill the per-subtree memo
// without native C recursion, so an arbitrarily deep term cannot overflow
// the stack.  Each frame holds the CTR node and a cursor over its children;
// a child CTR not yet memoized this epoch is pushed and processing of the
// parent suspends until it returns.  When a node's children are exhausted it
// is combined (kbo_memo_combine) into its slot.  The recursive version this
// replaces was the diagnosed SIGSEGV site on the deeper WM-FPA normal forms.
typedef struct {
  Term t;
  u64  base;
  u32  n;
  u32  i;
} KboMemoFrame;
// A worklist stack of CTR frames.  Starts in a generous static buffer and,
// only if a term is deeper than that, grows onto the heap -- so depth is
// unbounded yet the common case never allocates.
#define KBO_MEMO_STACK_CAP 8192
static KboMemoFrame g_kbo_memo_stack[KBO_MEMO_STACK_CAP];
// Re-entrancy guard: kbo_memo_combine recomputes a child whose memo slot a
// sibling's subtree evicted via a hash collision by calling back into
// kbo_subtree_memo (matching the original recursion's per-child lookup).  The
// outer call still owns g_kbo_memo_stack, so the nested call must NOT reuse
// it -- it allocates its own heap worklist instead.  Collisions are rare, so
// the static buffer is the common (non-reentrant) case.
static u8 g_kbo_memo_active = 0u;

// Compute (and cache) the weight + variable profile of subtree `t`.
// Memoizes every CTR node, so a node already seen this epoch is O(1).
// Variable ids >= KBO_MAX_VAR are out of balance range and are counted only
// toward the weight (matching kbo_bump's id guard).  Iterative post-order
// over an explicit worklist stack so an arbitrarily deep term cannot
// overflow the C stack (the diagnosed SIGSEGV site under WM-FPA).
static KboWMemoEnt *kbo_subtree_memo(const KboConfig *cfg, Term t) {
  // Fast Term-ID path: same Term ID seen this epoch -> O(1) jump to
  // its wmemo slot (verified by struct-hash equality to catch any
  // wmemo slot eviction since).
  u32 thidx = kbo_thash_slot(t);
  KboTHashEnt *th = &g_kbo_thash[thidx];
  if (th->epoch == g_kbo_epoch && th->t == t) {
    KboWMemoEnt *e = &g_kbo_wmemo[th->wmemo_idx];
    if (e->epoch == g_kbo_epoch && e->hash == th->hash) return e;
    // Slot evicted under collision; fall through to re-fill.
  }

  u8 reentrant = g_kbo_memo_active;
  g_kbo_memo_active = 1u;
  KboMemoFrame *heap_stack = NULL;
  KboMemoFrame *stack;
  u32 cap = KBO_MEMO_STACK_CAP;
  if (reentrant) {
    heap_stack = (KboMemoFrame *)malloc((size_t)cap * sizeof *heap_stack);
    stack = heap_stack;
  } else {
    stack = g_kbo_memo_stack;
  }
  u32 sp = 0u;
  KboMemoFrame *f = &stack[sp++];
  f->t = t; f->n = kbo_ctr_children(t, &f->base); f->i = 0u;
  while (sp > 0u) {
    f = &stack[sp - 1u];
    // Advance over children that are leaves or already memoized; push the
    // first CTR child still needing computation.
    int descended = 0;
    while (f->i < f->n) {
      Term c = heap_read(f->base + f->i);
      f->i++;
      if (term_tag(c) != TAG_CTR) continue;          // FVR/atom: combined later
      KboTHashEnt *cth = &g_kbo_thash[kbo_thash_slot(c)];
      if (cth->epoch == g_kbo_epoch && cth->t == c) continue;  // already memoized this walk
      if (sp >= cap) {
        u32 ncap = cap * 2u;
        KboMemoFrame *nh = (KboMemoFrame *)malloc((size_t)ncap * sizeof *nh);
        for (u32 k = 0; k < sp; k++) nh[k] = stack[k];
        if (heap_stack) free(heap_stack);
        heap_stack = nh; stack = nh; cap = ncap;
        f = &stack[sp - 1u];
      }
      KboMemoFrame *nf = &stack[sp++];
      nf->t = c; nf->n = kbo_ctr_children(c, &nf->base); nf->i = 0u;
      descended = 1;
      break;
    }
    if (descended) continue;
    // children exhausted: combine this node, pop.
    kbo_memo_combine(cfg, f->t);
    sp--;
  }
  if (heap_stack) free(heap_stack);
  // Read t's thash entry (filled by the combine above); from it, locate
  // the wmemo slot.  A late wmemo collision may have evicted it -- in
  // that case re-combine.
  th = &g_kbo_thash[thidx];
  KboWMemoEnt *e;
  if (th->epoch == g_kbo_epoch && th->t == t) {
    e = &g_kbo_wmemo[th->wmemo_idx];
    if (!(e->epoch == g_kbo_epoch && e->hash == th->hash)) e = kbo_memo_combine(cfg, t);
  } else {
    e = kbo_memo_combine(cfg, t);
  }
  if (!reentrant) g_kbo_memo_active = 0u;
  return e;
}

// Total KBO weight of a single term (Σ symbol-weight), served from the
// per-term memo.  The ATP CP-weight heuristics (CH_Ord/Gt/Mix) weigh both
// faces of EVERY critical pair, a full-term re-traversal each; routing
// them through this memo makes a repeat O(1).  Same epoch lifecycle as the
// comparator memo (invalidated on GC / config change).  FVR / atom roots
// are O(1) directly; a CTR root reuses or fills the memo.
fn long long thvm_kbo_term_weight(const KboConfig *cfg, Term t) {
  switch (term_tag(t)) {
    case TAG_FVR: return (long long)cfg->var_weight;
    case TAG_CTR: return kbo_subtree_memo(cfg, t)->w;
    default:      return (long long)cfg->var_weight;
  }
}

// Fallback: apply ALL variable bumps of subtree `t` by a full walk (used
// when the node's distinct-var count overflowed the inline profile).
// Iterative pre-order over an explicit worklist of child cells, growing onto
// the heap only if a term is deeper than the static buffer, so a deep OVF
// subtree cannot overflow the C stack.
static Term g_kbo_addvars_stack[KBO_MEMO_STACK_CAP];
static void kbo_addvars_walk(KboLin *st, Term t, int sign) {
  Term *stack = g_kbo_addvars_stack;
  u32 cap = KBO_MEMO_STACK_CAP;
  Term *heap_stack = NULL;
  u32 sp = 0u;
  stack[sp++] = t;
  while (sp > 0u) {
    Term cur = stack[--sp];
    if (term_tag(cur) == TAG_FVR) {
      kbo_bump(st, term_ext(cur), sign);
    } else if (term_tag(cur) == TAG_CTR) {
      u64 base; u32 n = kbo_ctr_children(cur, &base);
      if (sp + n > cap) {
        u32 ncap = cap * 2u;
        while (sp + n > ncap) ncap *= 2u;
        Term *nh = (Term *)malloc((size_t)ncap * sizeof *nh);
        for (u32 k = 0; k < sp; k++) nh[k] = stack[k];
        if (heap_stack) free(heap_stack);
        heap_stack = nh; stack = nh; cap = ncap;
      }
      for (u32 i = 0; i < n; i++) stack[sp++] = heap_read(base + i);
    }
    // a non-CTR/non-FVR cell contributes no var bumps.
  }
  if (heap_stack) free(heap_stack);
}

// Iter 136 (workflow-research finding #1): WM-style single-pass walk
// over a divergent subtree, no memo lookup.  WM's Vortest (KBO.c:254-301)
// does one cell-by-cell preorder pass and never builds a per-subtree
// weight cache, because in completion the same subtree cell rarely
// recurs across compares -- the memo miss-rate is high enough that the
// lookup itself is overhead.  This entrypoint mirrors that: walk the
// subtree iteratively (no recursion / no memo), bumping each FVR
// variable's balance and accumulating the symbol-weight sum into
// phidiff in one pass.  Enable via THVM_KBO_VORTEST_LINEAR=1; default
// off so the engine stays byte-identical to the memo path.
static Term g_kbo_walk_stack[KBO_MEMO_STACK_CAP];
static int g_kbo_vortest_linear_gate = -1;
static void kbo_lin_addto_walk(KboLin *st, Term t, int sign,
                               long long *phidiff) {
  Term *stack = g_kbo_walk_stack;
  u32 cap = KBO_MEMO_STACK_CAP;
  Term *heap_stack = NULL;
  u32 sp = 0u;
  stack[sp++] = t;
  while (sp > 0u) {
    Term cur = stack[--sp];
    u32 tg = term_tag(cur);
    if (tg == TAG_FVR) {
      kbo_bump(st, term_ext(cur), sign);
      *phidiff += (long long)sign * (long long)st->cfg->var_weight;
    } else if (tg == TAG_CTR) {
      u32 lab = term_ext(cur);
      *phidiff += (long long)sign *
          (long long)(lab < st->cfg->n_labels ? st->cfg->weights[lab] : 0);
      u64 base; u32 n = kbo_ctr_children(cur, &base);
      if (sp + n > cap) {
        u32 ncap = cap * 2u;
        while (sp + n > ncap) ncap *= 2u;
        Term *nh = (Term *)malloc((size_t)ncap * sizeof *nh);
        for (u32 k = 0; k < sp; k++) nh[k] = stack[k];
        if (heap_stack) free(heap_stack);
        heap_stack = nh; stack = nh; cap = ncap;
      }
      for (u32 i = 0; i < n; i++) stack[sp++] = heap_read(base + i);
    }
    /* atoms contribute neither phidiff nor var balance under WM-style */
  }
  if (heap_stack) free(heap_stack);
}

// Add the whole subtree `t` to one side of the current balance scope:
// weight*sign into *phidiff, var occurrences*sign into bal[].  A CTR
// subtree applies its memoized weight in O(1) and its variable profile in
// O(#distinct vars); only an overflowed profile falls back to a full walk.
static void kbo_lin_addto(KboLin *st, Term t, int sign,
                          long long *phidiff) {
  if (g_kbo_vortest_linear_gate < 0) {
    const char *e = getenv("THVM_KBO_VORTEST_LINEAR");
    g_kbo_vortest_linear_gate = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  if (g_kbo_vortest_linear_gate) {
    kbo_lin_addto_walk(st, t, sign, phidiff);
    return;
  }
  switch (term_tag(t)) {
    case TAG_FVR:
      kbo_bump(st, term_ext(t), sign);
      *phidiff += (long long)sign * (long long)st->cfg->var_weight;
      return;
    case TAG_CTR: {
      KboWMemoEnt *e = kbo_subtree_memo(st->cfg, t);
      *phidiff += (long long)sign * e->w;
      if (e->vc == KBO_VPROF_OVF) {
        kbo_addvars_walk(st, t, sign);
      } else {
        for (u8 k = 0; k < e->vc; k++)
          kbo_bump(st, e->vid[k], sign * (int)e->vcnt[k]);
      }
      return;
    }
    default: return;
  }
}

// Combined single-pass traversal of s,t (Waldmeister's Vortest).
// Accumulates weight(s)-weight(t) into *phidiff and the variable
// balance into st->bal (touched ids recorded).  Matching subtrees
// cancel and are skipped.  Returns 1 iff s and t are structurally
// identical.  The caller is responsible for clearing the balance
// (kbo_lin_clear) once it has read the decision out.
//
// Iterative over an explicit worklist of (s,t) pairs (growing onto the heap
// only past the static buffer), so two deeply nested terms cannot overflow
// the C stack.  The weight/var accumulation is commutative, so the order in
// which divergent subtrees are visited does not change phidiff or the
// balance -- the verdict is byte-identical to the recursive version.
typedef struct { Term s; Term t; } KboPair;
// Single-threaded saturation scratch for the iterative Vortest worklist (the
// whole KBO path already keeps its scope in the file-static g_kbo_st).  A
// stack-array would put a multi-KB frame on every one of millions of compares.
static KboPair g_kbo_vortest_stack[KBO_MEMO_STACK_CAP];
static int kbo_vortest(KboLin *st, Term s0, Term t0, long long *phidiff) {
  KboPair *stack = g_kbo_vortest_stack;
  u32 cap = KBO_MEMO_STACK_CAP;
  KboPair *heap_stack = NULL;
  u32 sp = 0u;
  int ident = 1;
  stack[sp].s = s0; stack[sp].t = t0; sp++;
  while (sp > 0u) {
    KboPair pr = stack[--sp];
    Term s = pr.s, t = pr.t;
    if (s == t) continue;                            // identical subtree
    u32 tg = term_tag(s);
    if (tg == term_tag(t) && term_ext(s) == term_ext(t)) {
      if (tg == TAG_FVR) continue;                   // same variable
      if (tg == TAG_CTR) {
        u64 sb, tb;
        u32 ns = kbo_ctr_children(s, &sb), nt = kbo_ctr_children(t, &tb);
        if (ns == nt) {
          if (sp + ns > cap) {
            u32 ncap = cap * 2u;
            while (sp + ns > ncap) ncap *= 2u;
            KboPair *nh = (KboPair *)malloc((size_t)ncap * sizeof *nh);
            for (u32 k = 0; k < sp; k++) nh[k] = stack[k];
            if (heap_stack) free(heap_stack);
            heap_stack = nh; stack = nh; cap = ncap;
          }
          for (u32 i = 0; i < ns; i++) {
            stack[sp].s = heap_read(sb + i);
            stack[sp].t = heap_read(tb + i);
            sp++;
          }
          continue;
        }
      } else if (term_val(s) == term_val(t)) {
        continue;                                    // equal atom
      }
    }
    // diverge: s wholly on +, t wholly on -.
    kbo_lin_addto(st, s, +1, phidiff);
    kbo_lin_addto(st, t, -1, phidiff);
    ident = 0;
  }
  if (heap_stack) free(heap_stack);
  return ident;
}

// Read the variable-balance verdict and clear the touched entries.
// Returns one of KBO_EQ/KBO_GT/KBO_LT/KBO_UN describing whether s
// dominates t variable-wise (Waldmeister's VarlisteLoeschenMitEntscheidung):
//   KBO_EQ : all balances zero
//   KBO_GT : some +, no -    (s dominates)
//   KBO_LT : some -, no +    (t dominates)
//   KBO_UN : both signs present
static KboCmp kbo_lin_decide_clear(KboLin *st) {
  KboCmp erg = KBO_EQ;
  for (u32 k = 0; k < st->n_touched; k++) {
    u32 id = st->touched[k];
    int v = st->bal[id];
    st->bal[id] = 0;
    if (v == 0) continue;
    switch (erg) {
      case KBO_EQ: erg = (v < 0) ? KBO_LT : KBO_GT; break;
      case KBO_LT: if (v > 0) erg = KBO_UN; break;
      case KBO_GT: if (v < 0) erg = KBO_UN; break;
      default: break;
    }
  }
  st->n_touched = 0;
  return erg;
}

// Just clear the touched balance entries (when s,t were identical, so
// the entries are already all zero, this is a no-op reset of n_touched).
static void kbo_lin_clear(KboLin *st) {
  for (u32 k = 0; k < st->n_touched; k++) st->bal[st->touched[k]] = 0;
  st->n_touched = 0;
}

// Map a deeper comparison result `inner` through one level's variable
// domination `varcmp` (Waldmeister's KBOEntscheidungRek result mapping).
// EQ stays EQ; a GT under a t-dominant level (or LT under an s-dominant
// level) collapses to UN; otherwise the direction is preserved.
static inline KboCmp kbo_map_through(KboCmp varcmp, KboCmp inner) {
  if (inner == KBO_EQ) return KBO_EQ;
  if (inner == KBO_GT) return (varcmp == KBO_LT) ? KBO_UN : KBO_GT;
  if (inner == KBO_LT) return (varcmp == KBO_GT) ? KBO_UN : KBO_LT;
  return KBO_UN;
}

// Decision after Vortest: given the weight balance phidiff and the
// variable-set verdict varcmp, decide s vs t (Waldmeister's
// KBOEntscheidungRek + KBOEntscheidungLex).  Precondition: s,t are not
// structurally identical and the balance table is already cleared.
//
// The recursive form descended a SINGLE spine: at each level, on a phi tie
// with equal top symbols, KBOEntscheidungLex finds the first non-identical
// child pair and recurses into KBOEntscheidungRek on it, mapping the deeper
// result back through this level's varcmp.  Only the first differing child
// ever recurses, so the recursion is a chain whose depth equals the term
// depth -- on the deep WM-FPA normal forms that overflowed the C stack.
// Made iterative here: descend the spine collecting each level's varcmp on
// an explicit stack, then fold the terminal verdict back up through them.
// Byte-identical to the recursive composition of kbo_map_through.
static KboCmp g_kbo_rek_vstack[KBO_MEMO_STACK_CAP];
static KboCmp kbo_lin_rek(KboLin *st, Term s, Term t, long long phidiff,
                          KboCmp varcmp) {
  KboCmp *vstack = g_kbo_rek_vstack;
  u32 cap = KBO_MEMO_STACK_CAP;
  KboCmp *heap_stack = NULL;
  u32 sp = 0u;
  KboCmp terminal;

  for (;;) {
    // --- one KBOEntscheidungRek level on (s,t,phidiff,varcmp) ---
    KboCmp here = KBO_UN;
    int descend = 0;
    int phi_tie = 0;            // weight balance ties at this level
    Term ds = 0, dt = 0;        // first differing child pair, if we descend
    switch (varcmp) {
      case KBO_GT:                                   // s dominates t
        if (phidiff < 0) here = KBO_UN;
        else if (phidiff > 0) here = KBO_GT;
        else phi_tie = 1;
        break;
      case KBO_LT:                                   // t dominates s
        if (phidiff < 0) here = KBO_LT;
        else if (phidiff > 0) here = KBO_UN;
        else phi_tie = 1;
        break;
      case KBO_EQ:                                   // same variables
        if (phidiff < 0) here = KBO_LT;
        else if (phidiff > 0) here = KBO_GT;
        else phi_tie = 1;
        break;
      default: here = KBO_UN; break;                 // incomparable var sets
    }
    if (phi_tie) {
      // phi tie: compare top symbols by precedence.
      u8 s_fvr = (term_tag(s) == TAG_FVR);
      u8 t_fvr = (term_tag(t) == TAG_FVR);
      if (s_fvr || t_fvr) { here = KBO_UN; }
      else if (term_tag(s) != TAG_CTR || term_tag(t) != TAG_CTR) {
        here = KBO_UN;
      } else {
        u32 ps = kbo_prec(st->cfg, s), pt = kbo_prec(st->cfg, t);
        if (ps > pt) here = (varcmp == KBO_LT) ? KBO_UN : KBO_GT;
        else if (ps < pt) here = (varcmp == KBO_GT) ? KBO_UN : KBO_LT;
        else if (term_ext(s) != term_ext(t)) here = KBO_UN;
        else {
          u32 ns = term_ctr_n(s), nt = term_ctr_n(t);
          if (ns != nt) here = KBO_UN;
          else {
            // KBOEntscheidungLex: first non-identical child pair decides.
            u64 sb, tb;
            u32 n = kbo_ctr_children(s, &sb);
            (void)kbo_ctr_children(t, &tb);
            KboCmp lexres = KBO_EQ;     // all children identical -> EQ
            int found = 0;
            for (u32 i = 0; i < n; i++) {
              Term cs = heap_read(sb + i), ct = heap_read(tb + i);
              long long phi = 0;
              int ident = kbo_vortest(st, cs, ct, &phi);
              if (ident) { kbo_lin_clear(st); continue; }
              // descend into this child pair next iteration.
              ds = cs; dt = ct;
              KboCmp cvar = kbo_lin_decide_clear(st);
              // push this level's varcmp; the deeper result is mapped
              // back through it on unwind.
              if (sp >= cap) {
                u32 ncap = cap * 2u;
                KboCmp *nh = (KboCmp *)malloc((size_t)ncap * sizeof *nh);
                for (u32 k = 0; k < sp; k++) nh[k] = vstack[k];
                if (heap_stack) free(heap_stack);
                heap_stack = nh; vstack = nh; cap = ncap;
              }
              vstack[sp++] = varcmp;
              phidiff = phi; varcmp = cvar; descend = 1; found = 1;
              break;
            }
            if (!found) here = lexres;  // KBO_EQ
          }
        }
      }
    }
    if (descend) { s = ds; t = dt; continue; }
    terminal = here;
    break;
  }

  // fold the terminal verdict up through the recorded varcmps.
  while (sp > 0u) terminal = kbo_map_through(vstack[--sp], terminal);
  if (heap_stack) free(heap_stack);
  return terminal;
}

// Production comparator: Loechner's linear-time KBO.
//
// bal[] is only ever written via kbo_bump (which records the id in the
// touched list) and cleared via that list before every return, so it is
// all-zero on entry AND exit of each call.  The scope is therefore kept
// in ONE file-static KboLin zeroed once (calloc-equivalent), instead of a
// fresh stack KboLin with a KBO_MAX_VAR-int zeroing loop on every call --
// KBO is in the saturation hot path (millions of calls), and the per-call
// 64-int memset dominated the comparator.  Single-threaded saturation, so
// the shared scope is safe.  Behavior-identical to the per-call version.
static KboLin g_kbo_st;   // bal[]/touched[] zero by static-storage init

// Persistence mode for the per-term weight memo, mirroring the LPO memo.
// OFF (default): every thvm_kbo call invalidates, so the memo is fresh per
// call -- always sound for arbitrary direct callers (tests) that may reuse
// cell addresses for a different term without telling us.  ON: the memo
// persists across calls (the whole point on a completion: the same
// subterms are weighed millions of times) and the caller PROMISES to
// invalidate on cell movement; the ATP engine turns this on at
// thvm_atp_init and invalidates on every GC.
static u8 g_kbo_persist = 0;
fn void thvm_kbo_set_persist(u8 on) { g_kbo_persist = on ? 1u : 0u; }

fn KboCmp thvm_kbo(Term s, Term t, const KboConfig *cfg) {
  if (g_kbo_persist) {
    // Persist across calls; a distinct cfg pointer means distinct weights,
    // so re-bump the epoch on a config change (the caller handles cell
    // moves via thvm_kbo_invalidate on GC).
    if (cfg != g_kbo_last_cfg) { g_kbo_last_cfg = cfg; thvm_kbo_invalidate(); }
  } else {
    thvm_kbo_invalidate();   // safe default: fresh memo every call
  }
  KboLin *st = &g_kbo_st;
  st->cfg = cfg;
  st->n_touched = 0;

  long long phidiff = 0;
  KboCmp erg;
  if (kbo_vortest(st, s, t, &phidiff)) {
    kbo_lin_clear(st);
    erg = KBO_EQ;
  } else {
    KboCmp varcmp = kbo_lin_decide_clear(st);
    erg = kbo_lin_rek(st, s, t, phidiff, varcmp);
  }
#ifdef KBO_WMEMO_SELFCHECK
  // Differential: the per-term weight memo must NEVER change the verdict.
  // thvm_kbo_naive is the memo-free Baader-Nipkow oracle (its own stack
  // arrays, no g_kbo_wmemo).  Run it on every live pair during bring-up
  // and abort on any divergence -- a wrong KBO verdict is a silent
  // soundness bug.  Defeats the speedup (runs both); compile OUT for the
  // measured runs.
  {
    KboCmp ref = thvm_kbo_naive(s, t, cfg);
    if (ref != erg) {
      fprintf(stderr, "KBO WMEMO SELFCHECK MISMATCH memo=%d naive=%d "
                      "(s=%llu t=%llu)\n", (int)erg, (int)ref,
                      (unsigned long long)s, (unsigned long long)t);
      abort();
    }
  }
#endif
  return erg;
}

// === flatterm comparator (thvm_kbo_flat) ============================
//
// Same Loechner linear-time decision as thvm_kbo, but each operand is
// flattened ONCE into a contiguous pre-order node array so the whole
// comparison reads cache-dense sequential memory instead of walking the
// IC term through term_ctr_at / term_tag / term_ext.
//
// Opt-in (THVM_ATP_KBO_FLAT=1 behind use_flatterm), NOT the default:
// thvm's CTR cells already store their children contiguously
// (term_ctr_at = heap_read(base + i)), so the IC traversal is itself
// cache-friendly and the flatten-both-operands pass measured net-
// negative against the in-place kbo_ctr_children traversal above on the
// Sheffer workload (the operand terms are shallow).  Kept for A/B and
// the differential self-check, and as the basis for a future path that
// reuses the subject array the flat normalizer already maintains
// (avoiding the per-call flatten).
//
// This is NOT a port: Waldmeister's KBO walks its own pointer term
// (sources/ORD/KBO.c, TermzellenZeigerT), so there is no flatterm KBO
// upstream.  The DECISION logic (Vortest / KBOEntscheidungRek /
// KBOEntscheidungLex) is identical to the kbo_lin_* path above -- the
// only change is the term-access layer.  A differential self-check
// (ATP_KBO_FLAT_SELFCHECK) asserts thvm_kbo_flat == thvm_kbo on every
// live pair during bring-up.
//
// Node encoding: sym >= 0 is a function label (CTR); sym < 0 encodes a
// variable id as -(id+1).  w is the node's OWN weight (cfg weight for a
// CTR, var_weight for a variable); sz is the subtree span in nodes
// (>= 1).  The lex tie-break compares matching-label children, so equal
// symbols recurse -- the array layout makes child i of a node at p start
// at the cursor advanced past its left siblings, exactly like preorder.

typedef struct KboFlatNode {
  i32 sym;   // >= 0 : CTR label;  < 0 : -(varid+1) for a variable
  i32 w;     // own weight (cfg->weights[label] or cfg->var_weight)
  u32 sz;    // subtree span in nodes (this node + all descendants)
} KboFlatNode;

#define KBO_FLAT_CAP 4096

static KboFlatNode g_kbo_flat_a[KBO_FLAT_CAP];
static KboFlatNode g_kbo_flat_b[KBO_FLAT_CAP];

// Pre-order flatten of `t` into `out` from `*pos`.  Returns 1 on
// success, 0 on cap overrun (caller falls back to thvm_kbo).  Mirrors
// atp_ri_flatten's structure but stores the KBO weight per node so the
// comparison never re-reads cfg in the inner loop.
static u8 kbo_flat_encode(Term t, const KboConfig *cfg, KboFlatNode *out,
                          u32 *pos) {
  u32 here = *pos;
  if (here >= KBO_FLAT_CAP) return 0;
  *pos = here + 1u;
  switch (term_tag(t)) {
    case TAG_FVR: {
      out[here].sym = -(i32)(term_ext(t) + 1u);
      out[here].w   = (i32)cfg->var_weight;
      out[here].sz  = 1u;
      return 1;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      out[here].sym = (i32)lab;
      out[here].w   = (lab < cfg->n_labels) ? (i32)cfg->weights[lab] : 0;
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (!kbo_flat_encode(term_ctr_at(t, i), cfg, out, pos)) return 0;
      }
      out[here].sz = *pos - here;
      return 1;
    }
    default:
      // A non-CTR / non-FVR head (NUM atom etc.) needs term_val identity
      // to compare correctly, which the flat node does not carry; bail
      // so thvm_kbo_flat falls back to the IC-term comparator.  The
      // single-symbol Sheffer ATP signature never hits this.
      return 0;
  }
}

// Add the subtree at `a[pos]` to one side of the balance scope.  The
// flat layout means the whole subtree is the contiguous slice
// [pos, pos+a[pos].sz); one linear sweep, no recursion.
static void kbo_flat_addto(KboLin *st, const KboFlatNode *a, u32 pos,
                           int sign, long long *phidiff) {
  u32 end = pos + a[pos].sz;
  for (u32 i = pos; i < end; i++) {
    *phidiff += (long long)sign * (long long)a[i].w;
    if (a[i].sym < 0) {
      u32 id = (u32)(-a[i].sym) - 1u;
      kbo_bump(st, id, sign);
    }
  }
}

// Combined single-pass Vortest over the two flat arrays at positions
// pa, pb.  Identical decision logic to kbo_vortest -- matching subtrees
// cancel and are skipped, divergent subtrees go wholly onto + / -.
// Returns 1 iff the two subtrees are structurally identical.
static int kbo_flat_vortest(KboLin *st, const KboFlatNode *a, u32 pa,
                            const KboFlatNode *b, u32 pb,
                            long long *phidiff) {
  if (a[pa].sym == b[pb].sym) {
    if (a[pa].sym < 0) return 1;                       // same variable
    // same CTR label (children count is implied by the signature, and
    // equal labels share arity) -- recurse over the children slices.
    if (a[pa].sz == b[pb].sz) {
      // exact byte-identical subtree: a fast structural check avoids the
      // per-child recursion in the common "untouched subterm" case.
      // Children begin right after the head; advance both cursors in
      // lockstep over each child pair.
      u32 ca = pa + 1u, cb = pb + 1u;
      u32 ea = pa + a[pa].sz, eb = pb + b[pb].sz;
      int ident = 1;
      while (ca < ea && cb < eb) {
        ident &= kbo_flat_vortest(st, a, ca, b, cb, phidiff);
        ca += a[ca].sz;
        cb += b[cb].sz;
      }
      return ident;
    }
  }
  // diverge: a[pa] wholly on +, b[pb] wholly on -.
  kbo_flat_addto(st, a, pa, +1, phidiff);
  kbo_flat_addto(st, b, pb, -1, phidiff);
  return 0;
}

static KboCmp kbo_flat_rek(KboLin *st, const KboFlatNode *a, u32 pa,
                           const KboFlatNode *b, u32 pb,
                           long long phidiff, KboCmp varcmp);

// Lexicographic compare of the children of a[pa], b[pb] (same label,
// same arity).  Mirrors kbo_lin_lex over the flat slices.
static KboCmp kbo_flat_lex(KboLin *st, const KboFlatNode *a, u32 pa,
                           const KboFlatNode *b, u32 pb) {
  u32 ca = pa + 1u, cb = pb + 1u;
  u32 ea = pa + a[pa].sz, eb = pb + b[pb].sz;
  while (ca < ea && cb < eb) {
    long long phi = 0;
    int ident = kbo_flat_vortest(st, a, ca, b, cb, &phi);
    if (ident) {
      kbo_lin_clear(st);
      ca += a[ca].sz;
      cb += b[cb].sz;
      continue;
    }
    KboCmp varcmp = kbo_lin_decide_clear(st);
    return kbo_flat_rek(st, a, ca, b, cb, phi, varcmp);
  }
  return KBO_EQ;
}

// Decision after Vortest -- mirror of kbo_lin_rek.  Precondition: the
// subtrees are not identical and the balance table is cleared.
static KboCmp kbo_flat_rek(KboLin *st, const KboFlatNode *a, u32 pa,
                           const KboFlatNode *b, u32 pb,
                           long long phidiff, KboCmp varcmp) {
  switch (varcmp) {
    case KBO_GT:
      if (phidiff < 0) return KBO_UN;
      if (phidiff > 0) return KBO_GT;
      break;
    case KBO_LT:
      if (phidiff < 0) return KBO_LT;
      if (phidiff > 0) return KBO_UN;
      break;
    case KBO_EQ:
      if (phidiff < 0) return KBO_LT;
      if (phidiff > 0) return KBO_GT;
      break;
    default: return KBO_UN;
  }

  // phi tie: compare top symbols by precedence.
  u8 s_fvr = (a[pa].sym < 0);
  u8 t_fvr = (b[pb].sym < 0);
  if (s_fvr || t_fvr) return KBO_UN;

  const KboConfig *cfg = st->cfg;
  u32 la = (u32)a[pa].sym, lb = (u32)b[pb].sym;
  u32 ps = (la < cfg->n_labels) ? cfg->precedence[la] : 0;
  u32 pt = (lb < cfg->n_labels) ? cfg->precedence[lb] : 0;
  if (ps > pt) return (varcmp == KBO_LT) ? KBO_UN : KBO_GT;
  if (ps < pt) return (varcmp == KBO_GT) ? KBO_UN : KBO_LT;

  // equal precedence: only the same label (=> matching arity for a fixed
  // signature) recurses lexicographically; anything else is incomparable.
  if (la != lb) return KBO_UN;

  KboCmp lex = kbo_flat_lex(st, a, pa, b, pb);
  if (lex == KBO_EQ) return KBO_EQ;
  if (lex == KBO_GT) return (varcmp == KBO_LT) ? KBO_UN : KBO_GT;
  if (lex == KBO_LT) return (varcmp == KBO_GT) ? KBO_UN : KBO_LT;
  return KBO_UN;
}

// Flatterm production comparator.  Flattens s,t once each, then runs the
// linear-KBO decision over the contiguous node arrays.  On a flatten
// overrun (a term deeper than KBO_FLAT_CAP) it falls back to thvm_kbo so
// the verdict is never weakened.  Byte-identical verdict to thvm_kbo
// (asserted under ATP_KBO_FLAT_SELFCHECK).
// Iter 133 step 4: split the flatterm comparator into a "slice" core
// that takes pre-encoded operands, plus the historical entrypoint that
// encodes-then-slices.  Callers in the ATP hot path that already have
// one or both operands flat (e.g. the rule lhs/rhs cache built once at
// push time) bypass the per-call encode and feed the slice directly.
// Byte-identical verdict to thvm_kbo (asserted under
// ATP_KBO_FLAT_SELFCHECK).
fn KboCmp thvm_kbo_flat_slice(const KboFlatNode *a, u32 na,
                              const KboFlatNode *b, u32 nb,
                              const KboConfig *cfg) {
  (void)na; (void)nb;  /* lengths are encoded in each node's `sz` field;
                        * the decision walks via sz, so the slice does
                        * not need to know totals -- but accept them for
                        * callers that want to assert shape pre-flight. */
  KboLin *st = &g_kbo_st;
  st->cfg = cfg;
  st->n_touched = 0;

  long long phidiff = 0;
  if (kbo_flat_vortest(st, a, 0u, b, 0u, &phidiff)) {
    kbo_lin_clear(st);
    return KBO_EQ;
  }
  KboCmp varcmp = kbo_lin_decide_clear(st);
  return kbo_flat_rek(st, a, 0u, b, 0u, phidiff, varcmp);
}

fn KboCmp thvm_kbo_flat(Term s, Term t, const KboConfig *cfg) {
  u32 na = 0, nb = 0;
  if (!kbo_flat_encode(s, cfg, g_kbo_flat_a, &na) ||
      !kbo_flat_encode(t, cfg, g_kbo_flat_b, &nb)) {
    return thvm_kbo(s, t, cfg);
  }
  return thvm_kbo_flat_slice(g_kbo_flat_a, na, g_kbo_flat_b, nb, cfg);
}
