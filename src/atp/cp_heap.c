// === 7c': CP-queue binary min-heap ==================================
//
// The CP queue (cp_packed/cp_trace/cp_pri/cp_seq) is kept as a
// binary min-heap ordered by (cp_pri, cp_seq): cheapest priority
// first, insertion order breaking ties.  This reproduces the old
// `--add` selection order (collapse_ordered sorted by INC depth,
// ties by queue index) but at O(log n) per push/pop instead of
// rebuilding an n-leaf INC-SUP tree + collapse on every step.

// Ordering predicate: does the CP at heap POSITION i sort strictly
// before the one at position j?  The sift-hot keys (pri / seq /
// ultimate) are read from the position's packed node -- exact caches
// of the stationary-row payload (AtpCpHeapNode refresh invariant,
// thvm.h), so every verdict is provably identical to the historical
// row loads while a compare touches ONE cache line per node.  Only the
// mode-gated extra keys (collapse-defer / tie-rhs packed-byte walks)
// fall through to row loads via node->row.
static int atp_cp_before(const AtpState *s, u32 i, u32 j) {
  const AtpCpHeapNode *na = &s->cp_heap[i], *nb = &s->cp_heap[j];
  // Waldmeister history-driven Act_ultimate: a CP tagged "ultimate"
  // ranks strictly before every non-ultimate CP, regardless of
  // heuristic weight (NewClassification.c sets w1 = minimalWeight()
  // = INT32_MIN for the `initial = ultimate` action; we encode it as
  // an out-of-band bit so reheapify cannot scramble the order).  The
  // flag is OFF by default -- engine byte-identical.  aux caches
  // cp_ultimate[row] (the WM preset turns this gate ON via the intake
  // canonicalize, so it IS on the default-bench compare path).
  if ((s->use_initial_ultimate || s->use_database_ultimate)
      && s->cp_ultimate != NULL) {
    u32 ui = na->aux, uj = nb->aux;
    if (ui != uj) return ui > uj;
    // Both ultimate: WM stamps EVERY Act_ultimate CP w1 =
    // minimalWeight() = INT32_MIN (NewClassification.c:330), so the
    // (w1, w2) heap key degenerates to w2 alone -- pure FIFO within
    // the ultimate class.  The computed cp_pri is deliberately
    // ignored here; for axioms the FIFO stamps carry the canonical
    // loader-sort order (atp_wm_intake_canonicalize).
    if (ui) return na->seq < nb->seq;
  }
  // Collapser deferral (opt-in, THVM_ATP_COLLAPSE_DEFER=<penalty>): add a
  // priority penalty to bare-variable-rhs CPs (collapsers), deferring their
  // selection past the compound CPs WM forms first.  thvm forms rule x axiom
  // collapser overlaps EARLIER than WM's DSBaum emission order, so its
  // collapser-heavy trajectory over-explores (CP-queue explosion) where WM
  // stays lean.  Penalizing collapsers approximates WM's later timing without
  // reconstructing the emission age.  Off by default (< 0 sentinel).
  static int cdef = -2;
  if (cdef == -2) {
    const char *e = getenv("THVM_ATP_COLLAPSE_DEFER");
    cdef = e ? (int)strtoul(e, NULL, 10) : -1;
  }
  if (cdef >= 0) {
    // Mode-gated (off by default): the rhs-node count lives in the
    // packed bytes -- a row load through node->row.
    u32 pi = na->pri
             + (acp_rhs_nodes(s->cp_packed[na->row]) == 1u ? (u32)cdef : 0u);
    u32 pj = nb->pri
             + (acp_rhs_nodes(s->cp_packed[nb->row]) == 1u ? (u32)cdef : 0u);
    if (pi != pj) return pi < pj;
  } else {
    if (na->pri != nb->pri) return na->pri < nb->pri;
  }
  // Weight-tie tiebreak (opt-in, THVM_ATP_TIE_RHS): among CPs sharing the
  // WMQ weight bucket (S*S+S-1 depends only on total size, so a compound
  // and a collapser of equal total size tie), prefer the LARGER-rhs CP.
  // WM's -auto age order picks the shallow-overlap compound (large rhs)
  // before the deep-overlap collapser (bare-variable rhs) of the same
  // weight; thvm's raw cp_seq does the reverse (the collapser's overlap is
  // formed earlier).  Preferring the larger rhs approximates WM's pick
  // without reconstructing its full DSBaum emission age.  Off by default
  // -> engine/dylib byte-identical unless the flag is set.
  static int tie_rhs = -1;
  if (tie_rhs < 0) tie_rhs = getenv("THVM_ATP_TIE_RHS") ? 1 : 0;
  if (tie_rhs) {
    // Mode-gated (off by default): packed-byte walk via node->row.
    u32 ri = acp_rhs_nodes(s->cp_packed[na->row]);
    u32 rj = acp_rhs_nodes(s->cp_packed[nb->row]);
    if (ri != rj) return ri > rj;   // larger rhs (compound) selected first
  }
  // Twee-faithful passive tie-break (opt-in, THVM_ATP_TWEE_TIEBREAK):
  // Twee's `instance Ord Passive` (Twee.hs:260-266) breaks a score tie by
  // (max parent-rule-id, min parent-rule-id) on the two overlap rules --
  // preferring the CP whose later-born parent is earliest.  thvm's default
  // is cp_seq (FIFO formation order); the two diverge, cascading (via the
  // completion) into different rule sets, so thvm reaches Twee's goal-joining
  // configuration on a different -- and, on the twee-only cluster, later --
  // schedule.  Sound: a passive tie-break only reorders selection, never
  // the equational theory.  Off by default -> byte-identical (WM preset).
  static int twee_tb = -1;
  if (twee_tb < 0) twee_tb = getenv("THVM_ATP_TWEE_TIEBREAK") ? 1 : 0;
  if (twee_tb && s->cp_uid_a != NULL && s->cp_uid_b != NULL) {
    u32 ai = s->cp_uid_a[na->row], bi = s->cp_uid_b[na->row];
    u32 aj = s->cp_uid_a[nb->row], bj = s->cp_uid_b[nb->row];
    u32 mxi = ai > bi ? ai : bi, mni = ai < bi ? ai : bi;
    u32 mxj = aj > bj ? aj : bj, mnj = aj < bj ? aj : bj;
    if (mxi != mxj) return mxi < mxj;
    if (mni != mnj) return mni < mnj;
  }
  return na->seq < nb->seq;
}

// Swap the CPs at heap positions i, j.  Only the position mirror moves
// (cp_heap: pos -> packed node, cp_pos: row -> pos); the ~14 parallel
// payload arrays stay stationary at their rows -- the historical
// whole-payload swap sifted every array on every hop, ~14 scattered
// stores per swap.  Whole 16-byte nodes swap, so the key caches ride
// along with their row -- the refresh invariant is preserved by
// construction.  A subsumption-index record borrowing cp_packed bytes
// stays valid a fortiori (the pointer never moves between slots).
static u32 atp_goal_weight(const AtpState *s, Term cl, Term cr);

static void atp_cp_swap(AtpState *s, u32 i, u32 j) {
  AtpCpHeapNode ni = s->cp_heap[i], nj = s->cp_heap[j];
  s->cp_heap[i] = nj;
  s->cp_heap[j] = ni;
  s->cp_pos[ni.row] = j;
  s->cp_pos[nj.row] = i;
}

// Move payload row `src` into (vacated) row `dst`: the pop-backfill /
// sweep-densify companion of the stationary-row discipline.  Copies
// every per-CP array the historical backfill copied -- cp_deprio is
// deliberately NOT moved (the historical backfill never moved it
// either: it is a position-stamped array, see thvm.h).  The caller
// rewires cp_heap/cp_pos for the moved row.
static inline void atp_cp_row_move(AtpState *s, u32 dst, u32 src) {
  s->cp_packed[dst] = s->cp_packed[src];
  s->cp_packed[src] = NULL;           // vacated row: leave it empty
  atp_cp_stash_move(s, dst, src);     // peak / raw stashes ride along
  s->cp_trace[dst]  = s->cp_trace[src];
  s->cp_kind[dst]   = s->cp_kind[src];
  s->cp_pri[dst]    = s->cp_pri[src];
  s->cp_seq[dst]    = s->cp_seq[src];
  if (s->cp_depth != NULL) s->cp_depth[dst] = s->cp_depth[src];
  if (s->cp_gd != NULL) s->cp_gd[dst] = s->cp_gd[src];
  s->cp_goal[dst]   = s->cp_goal[src];
  s->cp_sine[dst]   = s->cp_sine[src];
  s->cp_pri2[dst]   = s->cp_pri2[src];
  s->cp_last_norm_r_revision[dst] = s->cp_last_norm_r_revision[src];
  if (s->cp_ultimate != NULL) s->cp_ultimate[dst] = s->cp_ultimate[src];
  if (s->cp_uid_a != NULL) {
    s->cp_uid_a[dst] = s->cp_uid_a[src];
    s->cp_uid_b[dst] = s->cp_uid_b[src];
  }
  atp_cp_implicit_move(s, dst, src);
#ifdef THVM_ATPFT_CPQ
  // Move the (still-owned) FT entry alongside; zeroes the vacated src
  // so a later destroy / clear does not double-free the spans.
  atp_cp_ft_move(s, dst, src);
#endif
}

// After a position-order sweep compaction (cp_heap[0..w) holds the
// survivor rows in the sweep's visit order; s->n_cps is still the
// pre-sweep count), densify the payload rows to [0, w): every survivor
// row >= w moves into a freed row < w and the mirror rewires.  Which
// freed row a survivor lands in is NOT observable -- every consumer
// reads payload through cp_heap/cp_pos, positions are untouched here,
// and cp_deprio (the one position-raw array) never moves anyway.
// Rebuilds cp_pos for the surviving positions and commits n_cps = w.
static void atp_cp_rows_densify(AtpState *s, u32 w) {
  u32 old_n = s->n_cps;
  if (w != old_n) {
    u8 *live = (u8 *)calloc((size_t)(old_n ? old_n : 1u), 1u);
    if (live == NULL) {
      fprintf(stderr, "atp_cp_rows_densify: calloc %u failed\n", old_n);
      exit(1);
    }
    for (u32 q = 0; q < w; q++) live[s->cp_heap[q].row] = 1u;
    u32 f = 0;   // next freed row < w (ascending; enough exist: |live| == w)
    for (u32 q = 0; q < w; q++) {
      u32 r = s->cp_heap[q].row;
      if (r < w) continue;
      while (live[f]) f++;
      atp_cp_row_move(s, /*dst=*/f, /*src=*/r);
      live[f] = 1u;
      // Row move: the keys were copied verbatim to row f, so the
      // node's pri/seq/aux caches stay exact -- rewire only .row.
      s->cp_heap[q].row = f;
      f++;
    }
    free(live);
  }
  for (u32 q = 0; q < w; q++) s->cp_pos[s->cp_heap[q].row] = q;
  s->n_cps = w;
}

static void atp_cp_sift_up(AtpState *s, u32 i) {
  while (i > 0) {
    u32 parent = (i - 1) / 2;
    if (!atp_cp_before(s, i, parent)) break;
    atp_cp_swap(s, i, parent);
    i = parent;
  }
}

static void atp_cp_sift_down(AtpState *s, u32 i) {
  for (;;) {
    u32 l = 2 * i + 1, r = 2 * i + 2, m = i;
    if (l < s->n_cps && atp_cp_before(s, l, m)) m = l;
    if (r < s->n_cps && atp_cp_before(s, r, m)) m = r;
    if (m == i) break;
    atp_cp_swap(s, i, m);
    i = m;
  }
}

// ===== Waldmeister 2-D KD-heap port (opt-in THVM_ATP_KDHEAP) =============
// Faithful port of KDHeap.c (k=2) as thvm's CP-queue discipline, reproducing
// itl(mi) selection exactly: w1 = cp_pri (WMQ2 weight), w2 = cp_seq (age).
// KD-heap nodes are 1-indexed (node n <-> array slot n-1); every swap routes
// through atp_cp_swap so all parallel CP arrays stay consistent.  Ref:
// ~/src/wolfram/waldmeister/sources/BASIC/KDHeap.c (level/find_min/normalize_*).
static int atp_kdheap_on(void) {
  static int v = -1;
  if (v < 0) v = getenv("THVM_ATP_KDHEAP") ? 1 : 0;
  return v;
}
// Diagnostic: the CPdimension d of the most recent KD-heap selection
// (0 = weight root, 1 = FIFO/age jump); surfaced in the CPSEL trace.
static u32 g_kdh_last_d = 99u;
// EL_GREATER(d): d==0 -> (w1,then w2); d==1 -> w2 alone (KDHeapElements.h:63).
// na/nb are 1-indexed heap NODES; both keys come from the packed node
// caches (exact copies of cp_pri[row] / cp_seq[row], see the
// AtpCpHeapNode refresh invariant) -- one cache line per compare side.
static int atp_kdh_gt(const AtpState *s, unsigned d, u32 na, u32 nb) {
  const AtpCpHeapNode *a = &s->cp_heap[na - 1u], *b = &s->cp_heap[nb - 1u];
  if (d == 0u) {
    if (a->pri != b->pri) return a->pri > b->pri;
    return a->seq > b->seq;
  }
  return a->seq > b->seq;
}
static unsigned atp_kdh_level(u32 n) {           // floor(log2(n))
  unsigned l = 1u;
  while (n >= (1u << l)) l++;
  return l - 1u;
}
static unsigned atp_kdh_dmodk(unsigned d) { return (d >= 2u) ? d - 2u : d; }
// Min in dimension d among the top `levels`+1 levels of the subheap at `index`.
static void atp_kdh_find_min(const AtpState *s, u32 index, unsigned d,
                             unsigned levels, u32 *min, unsigned *min_level) {
  u32 n_nodes = s->n_cps;
  *min = index; *min_level = 0u;
  for (unsigned ld = 1u; ld <= levels; ld++) {
    for (u32 delta = 0u; delta < (1u << ld); delta++) {
      u32 j = (index << ld) + delta;
      if (j > n_nodes) return;
      if (atp_kdh_gt(s, d, *min, j)) { *min = j; *min_level = ld; }
    }
  }
}
static void atp_kdh_normalize_up(AtpState *s, u32 index, unsigned level_diff,
                                 unsigned d) {
  while (level_diff > 0u) {
    d = atp_kdh_dmodk(d);
    u32 anc = index >> level_diff;
    if (atp_kdh_gt(s, d, anc, index)) atp_cp_swap(s, anc - 1u, index - 1u);
    level_diff--; d++;
  }
}
static void atp_kdh_normalize_down(AtpState *s, u32 index, unsigned level_index) {
  u32 min; unsigned d, min_level;
  d = (level_index < 2u) ? level_index : (level_index % 2u);
  atp_kdh_find_min(s, index, d, 2u, &min, &min_level);
  while (min != index) {
    atp_cp_swap(s, min - 1u, index - 1u);
    atp_kdh_normalize_up(s, min, min_level - 1u, d + 1u);
    index = min;
    d = atp_kdh_dmodk(d + min_level);
    atp_kdh_find_min(s, index, d, 2u, &min, &min_level);
  }
}
// KD-heap insert: the fresh node is slot `slot` (0-indexed) == node slot+1.
static void atp_kdh_insert(AtpState *s, u32 slot) {
  u32 node = slot + 1u;
  atp_kdh_normalize_up(s, node, atp_kdh_level(node), 0u);
}
// KD-heap delete of slot `slot` (0-indexed): the caller has already
// backfilled slot with the ex-last element and decremented n_cps.  Repair
// via normalize_up then normalize_down (deleteElement, KDHeap.c:512-515).
static void atp_kdh_delete_repair(AtpState *s, u32 slot) {
  u32 node = slot + 1u;
  unsigned lvl = atp_kdh_level(node);
  atp_kdh_normalize_up(s, node, lvl, 0u);
  atp_kdh_normalize_down(s, node, lvl);
}
// Rebuild the KD-heap in place after a bulk mutation (interreduce re-weights
// every queued CP).  Sequential normalize_up over nodes 1..n_cps == building
// the heap by repeated insertion; replaces the binary Floyd build-heap.
static void atp_kdh_build(AtpState *s) {
  for (u32 node = 1u; node <= s->n_cps; node++) {
    atp_kdh_normalize_up(s, node, atp_kdh_level(node), 0u);
  }
}

// --- Raw-descriptor lane bodies (see the block comment at the top
// helpers).  atp_rdesc_intern: dedupe one (lo, ro) face-pair into
// s->rdesc_faces, returning its u32 ref.  Open addressing over pointer
// pairs; the table only ever grows (face pairs are never retired --
// their spans are immortal within the state).
static void atp_rdesc_hash_grow(AtpState *s) {
  u32 ncap = s->rdesc_hash_cap ? s->rdesc_hash_cap * 2u : 4096u;
  u32 *nh = (u32 *)malloc((size_t)ncap * sizeof(u32));
  if (nh == NULL) thvm_fatal("atp_rdesc_hash_grow: OOM");
  for (u32 i = 0; i < ncap; i++) nh[i] = ATP_RDESC_NONE;
  for (u32 r = 0; r < s->rdesc_n; r++) {
    u64 h = ((u64)(uintptr_t)s->rdesc_faces[r].lo
             ^ ((u64)(uintptr_t)s->rdesc_faces[r].ro << 17))
            * 0x9e3779b97f4a7c15ull;
    u32 j = (u32)(h >> 32) & (ncap - 1u);
    while (nh[j] != ATP_RDESC_NONE) j = (j + 1u) & (ncap - 1u);
    nh[j] = r;
  }
  free(s->rdesc_hash);
  s->rdesc_hash     = nh;
  s->rdesc_hash_cap = ncap;
}

static u32 atp_rdesc_intern(AtpState *s, const struct AtpFtCell *lo,
                            const struct AtpFtCell *ro) {
  if (s->rdesc_hash == NULL ||
      s->rdesc_n * 4u >= s->rdesc_hash_cap * 3u) {
    atp_rdesc_hash_grow(s);
  }
  u64 h = ((u64)(uintptr_t)lo ^ ((u64)(uintptr_t)ro << 17))
          * 0x9e3779b97f4a7c15ull;
  u32 j = (u32)(h >> 32) & (s->rdesc_hash_cap - 1u);
  while (s->rdesc_hash[j] != ATP_RDESC_NONE) {
    u32 r = s->rdesc_hash[j];
    if (s->rdesc_faces[r].lo == lo && s->rdesc_faces[r].ro == ro) return r;
    j = (j + 1u) & (s->rdesc_hash_cap - 1u);
  }
  if (s->rdesc_n == s->rdesc_cap) {
    u32 ncap = s->rdesc_cap ? s->rdesc_cap * 2u : 1024u;
    AtpRdescFace *nf = (AtpRdescFace *)realloc(
        s->rdesc_faces, (size_t)ncap * sizeof(AtpRdescFace));
    if (nf == NULL) thvm_fatal("atp_rdesc_intern: OOM");
    s->rdesc_faces = nf;
    s->rdesc_cap   = ncap;
  }
  u32 r = s->rdesc_n++;
  s->rdesc_faces[r].lo = lo;
  s->rdesc_faces[r].ro = ro;
  s->rdesc_hash[j] = r;
  return r;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Re-derive a raw-desc slot's packed byte string from its parents:
// the exact push-time pipeline -- rename the inner face by
// CP_RENAME_OFFSET (== sw_rename_ft's clone content), re-address the
// overlap subterm by preorder index (spans are ->next preorder
// chains), ft_unify, then vcp_ft_emit_fused into g_acp_scratch.  Every
// step is a pure function of the frozen face spans, so the bytes are
// identical to the push-time emit (certified by use_raw_desc_cert).
// Returns a malloc'd copy of the bytes; clears the transient fused
// weigh / emit state it armed.  thvm_fatal on any failure: the push
// side only stored a descriptor when this exact pipeline SUCCEEDED on
// these exact inputs, so a failure here is a broken invariant.
static u8 *atp_rdesc_materialize(AtpState *s, const AtpCpImplicit *d,
                                 u32 *len_out, u32 *nodes_out,
                                 u32 *lhs_nodes_out) {
  if (d->out_face >= s->rdesc_n || d->in_face >= s->rdesc_n) {
    thvm_fatal("atp_rdesc_materialize: face ref out of range");
  }
  const AtpRdescFace *fo = &s->rdesc_faces[d->out_face];
  const AtpRdescFace *fi = &s->rdesc_faces[d->in_face];
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *lin_ft = ft_rename_span(a, fi->lo, CP_RENAME_OFFSET, 0);
  AtpFtCell *rin_ft = ft_rename_span(a, fi->ro, CP_RENAME_OFFSET, 0);
  const AtpFtCell *sub = fo->lo;
  for (u32 k = 0; k < d->sub_pre; k++) {
    if (sub == NULL) thvm_fatal("atp_rdesc_materialize: sub_pre walked "
                                "off the outer face span");
    sub = sub->next;
  }
  static AtpFtSubst rsubst;
  ft_subst_reset(&rsubst);
  if (!ft_unify(sub, lin_ft, &rsubst)) {
    fprintf(stderr,
            "rdesc materialize: ft_unify FAILED out_face=%u in_face=%u "
            "sub_pre=%u weight=%u\n",
            d->out_face, d->in_face, d->sub_pre, d->weight);
    thvm_fatal("atp_rdesc_materialize: re-unification failed");
  }
  if (!vcp_ft_emit_fused(s->kbo, fo->lo, sub, rin_ft, fo->ro, &rsubst)) {
    fprintf(stderr,
            "rdesc materialize: emit ANOMALY out_face=%u in_face=%u "
            "sub_pre=%u weight=%u\n",
            d->out_face, d->in_face, d->sub_pre, d->weight);
    thvm_fatal("atp_rdesc_materialize: re-emit anomaly");
  }
  u32 flen = g_vcp_emit.len;
  u8 *bytes = (u8 *)malloc(flen ? flen : 1u);
  if (bytes == NULL) thvm_fatal("atp_rdesc_materialize: OOM");
  memcpy(bytes, g_acp_scratch, flen);
  *len_out       = flen;
  *nodes_out     = g_vcp_emit.nodes;
  *lhs_nodes_out = g_vcp_emit.lhs_nodes;
  // The emit armed the consume-once fused weigh state; nothing at pop
  // consumes it, so disarm both (a stale valid=1 would feed this CP's
  // balances into the NEXT push's weigh compare).
  g_acp_fuse.valid = 0u;
  g_vcp_emit.armed = 0u;
  ft_free_span(a, lin_ft, lin_ft->end);
  ft_free_span(a, rin_ft, rin_ft->end);
  s->n_rawdesc_mat++;
  return bytes;
}
#else
static u8 *atp_rdesc_materialize(AtpState *s, const AtpCpImplicit *d,
                                 u32 *len_out, u32 *nodes_out,
                                 u32 *lhs_nodes_out) {
  (void)s; (void)d; (void)len_out; (void)nodes_out; (void)lhs_nodes_out;
  thvm_fatal("atp_rdesc_materialize: raw-desc lane requires the "
             "THVM_ATPFT_NORM build (descriptors can never be pushed "
             "without it)");
  return NULL;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// Cert-mode verify of one eager slot that carries a descriptor:
// re-derive and memcmp against the stored bytes; thvm_fatal with a
// full dump on any mismatch.  Shared by the pop-time check and the
// whole-queue sweep (thvm_atp_rawdesc_cert_sweep).
static void atp_rdesc_cert_verify(AtpState *s, u32 v) {
  const AtpCpImplicit *d = &s->cp_implicit[v];
  u32 len = 0u, nodes = 0u, lhs_nodes = 0u;
  u8 *bytes = atp_rdesc_materialize(s, d, &len, &nodes, &lhs_nodes);
  u32 elen = d->weight;   // cert mode: push-time byte length
  if (len != elen || memcmp(bytes, s->cp_packed[v], len) != 0) {
    u32 firstdiff = 0u;
    u32 lim = len < elen ? len : elen;
    while (firstdiff < lim &&
           bytes[firstdiff] == s->cp_packed[v][firstdiff]) {
      firstdiff++;
    }
    fprintf(stderr,
            "RAW_DESC CERT MISMATCH seq=%u len=%u/%u firstdiff=%u "
            "out_face=%u(lo=%p ro=%p) in_face=%u(lo=%p ro=%p) "
            "sub_pre=%u pri=%u\n",
            s->cp_seq[v], len, elen, firstdiff,
            d->out_face, (const void *)s->rdesc_faces[d->out_face].lo,
            (const void *)s->rdesc_faces[d->out_face].ro,
            d->in_face, (const void *)s->rdesc_faces[d->in_face].lo,
            (const void *)s->rdesc_faces[d->in_face].ro,
            d->sub_pre, d->priority);
    for (u32 k = 0; k < lim && k < 512u; k++) {
      if (k % 32u == 0u) fprintf(stderr, "\n  eager/desc[%4u]:", k);
      fprintf(stderr, " %02x/%02x", s->cp_packed[v][k], bytes[k]);
    }
    fputc('\n', stderr);
    thvm_fatal("RAW_DESC certifier: materialized pair differs from "
               "the eager packed bytes");
  }
  free(bytes);
}

// Whole-queue certifier sweep (cert mode only): verify EVERY live
// queued slot that carries a descriptor, not just the popped ones --
// pop coverage is thin exactly because the raw class floods the queue
// rather than the selections.  Returns the number of slots verified;
// fatal on the first mismatch.  Payload rows are dense in [0, n_cps).
fn u64 thvm_atp_rawdesc_cert_sweep(AtpState *s) {
  if (s == NULL || s->cp_implicit == NULL || s->use_raw_desc_cert == 0u) {
    return 0ull;
  }
  u64 n = 0ull;
  for (u32 v = 0; v < s->n_cps; v++) {
    if (atp_cp_slot_implicit(s, v)) continue;
    if (s->cp_implicit[v].out_face == ATP_RDESC_NONE) continue;
    if (s->cp_packed[v] == NULL) continue;
    atp_rdesc_cert_verify(s, v);
    n++;
  }
  return n;
}

// Pop-time slot fix for the raw-desc lane, called on the selected
// (non-orphan) slot BEFORE the unpack:
// - desc mode: materialize the packed bytes into cp_packed[v] and
//   clear the tag bit -- the slot becomes a plain eager slot and every
//   downstream consumer (unpack, mixmost byte->FT pop stash, the
//   free-on-pop) runs the eager code path bit-for-bit.
// - cert mode: the slot IS eager (bytes stored at push); re-derive
//   from the descriptor and memcmp -- thvm_fatal with a full dump on
//   any mismatch.  desc.weight carries the push-time byte length, so
//   the compare is exact-length, never an over-read.
// No-op when the lane is off (cp_implicit NULL or no descriptor).
static void atp_cp_rdesc_pop_fix(AtpState *s, u32 v) {
  if (s->cp_implicit == NULL) return;
  if (s->use_raw_desc != 0u && atp_cp_slot_implicit(s, v)) {
    u32 len = 0u, nodes = 0u, lhs_nodes = 0u;
    u8 *bytes = atp_rdesc_materialize(s, &s->cp_implicit[v],
                                      &len, &nodes, &lhs_nodes);
    s->cp_packed[v] = bytes;
    atp_cp_implicit_clear(s, v);
  } else if (s->use_raw_desc_cert != 0u && !atp_cp_slot_implicit(s, v)
             && s->cp_implicit[v].out_face != ATP_RDESC_NONE
             && s->cp_packed[v] != NULL) {
    atp_rdesc_cert_verify(s, v);
  }
}

// Insert an already-packed CP byte string onto the heap.  Takes
// ownership of `packed` (the queue frees it on pop/drop).  `lhs`/`rhs`
// are the live Terms (for the FV index + goal weight); `cp_nodes` is
// the precomputed node count.  `is_ultimate`=1 forces the CP to the
// heap front (Waldmeister Act_ultimate) -- effective only when
// s->use_initial_ultimate is set; otherwise the bit is recorded but
// atp_cp_before ignores it.  Shared by atp_cp_heap_push (fresh CP)
// and the auto-MaxWeight stash drain (re-admitting a deferred CP).
// `lhs_nodes` is the LHS-only node count from acp_pack_ex (0xffffffff
// = unknown -> the weight modes re-walk the sides as before).
static void atp_cp_heap_insert_packed(AtpState *s, u8 *packed, u32 cp_nodes,
                                      Term lhs, Term rhs, u32 trace,
                                      u8 is_ultimate, u32 lhs_nodes,
                                      u8 kind) {
  atp_ensure_cp_cap(s, s->n_cps + 1);
  u32 i = s->n_cps;
  // Fresh CP appends at row i == heap position i (identical placement
  // to the historical layout; the sift below then moves only the
  // position mirror).  The packed node is stamped by atp_cp_heap_set
  // just before the sift, AFTER every key (pri / seq / ultimate) is
  // written to the row.
  s->cp_pos[i]  = i;
  // Eager packed slot: make sure no stale deferred-CP tag aliases the
  // packed bytes as a descriptor (every drop/move site clears its bit,
  // so this is a backstop that keeps the invariant local).
  atp_cp_implicit_clear(s, i);
  s->cp_packed[i]= packed;
  // Simpler-join pop-peak stash: attach the armed peak bytes to the
  // slot -- ONLY when the slot stores packed bytes (the peak was
  // renumbered through the pair-pack's dense map, so byte-less slots
  // -- CPQ mode-8 adoption, skip-pack -- would read it against a
  // different var space; they degrade peakless instead).  Consume-once
  // either way so a dropped pending can never leak onto a later CP.
  atp_cp_stash_clear(s, i);  // backstop, mirrors the implicit clear
  if (g_sj_peak_pending != NULL) {
    if (packed != NULL) {
      atp_cp_peak_arrays_ensure(s);
      s->cp_peak[i] = g_sj_peak_pending;
      g_sj_peak_pending = NULL;
    } else {
      atp_sj_peak_pending_drop();
    }
  }
  // Raw-overlap stash: self-contained bytes, attached to any slot.
  if (g_cp_raw_pending != NULL) {
    atp_cp_raw_arrays_ensure(s);
    s->cp_raw[i] = g_cp_raw_pending;
    g_cp_raw_pending = NULL;
  }
  s->cp_trace[i] = trace;
  s->cp_kind[i]  = kind;
  // Fresh CP: no IR-side normalize witness yet -- force the IR sweep to
  // normalize this CP at least once before the cookie can short-circuit.
  s->cp_last_norm_r_revision[i] = ATP_CP_NORM_COOKIE_NONE;
  s->cp_deprio[i] = g_cp_push_deprio;
  u64 _dkb_t0 = g_atp_phase_detail ? atp_now_us() : 0;
  s->cp_pri[i]   = s->cp_deprio[i]
                     ? ATP_CP_PRI_NEVER
                     : atp_cp_priority_sized_ex(s, lhs, rhs, cp_nodes,
                                                lhs_nodes);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // ifeq guard + tgt goal-targeted disciplines, FT-routed-push arm:
  // the treated route's Term args are zero sentinels, so the
  // in-priority Term counts above saw 0 guards / 0 symbols -- count on
  // the treatment's NF spans instead (the exact content this slot
  // stores).  tgt first, then ifeq penalty, then ifeq guard-DECIDING
  // boost, matching the Term-route order in atp_cp_priority_sized_ex.
  // The raw >= 50 emit route (lazy mode only) has no live spans here
  // and stays unscaled: its giant raw weight already buries it, and
  // the mechanisms are selection bias, not gates.
  if (s->cp_pri[i] < ATP_CP_PRI_NEVER &&
      g_vcp_treat.armed && g_vcp_treat.fl != NULL &&
      g_vcp_treat.fr != NULL &&
      (lhs == 0 || term_tag(lhs) == TAG_NUM ||
       rhs == 0 || term_tag(rhs) == TAG_NUM)) {
    if (atp_tgt_armed(s)) {
      s->cp_pri[i] = atp_tgt_weight_scale_ft(s, g_vcp_treat.fl,
                                             g_vcp_treat.fr, s->cp_pri[i]);
    }
    if (s->use_ifeq_guard) {
      s->cp_pri[i] = atp_ifeq_weight_scale_ft(s, g_vcp_treat.fl,
                                              g_vcp_treat.fr, s->cp_pri[i]);
    }
    if (s->use_ifeq_decide) {
      s->cp_pri[i] = atp_ifeq_decide_scale_ft(s, g_vcp_treat.fl,
                                              g_vcp_treat.fr, s->cp_pri[i]);
    }
  }
#endif
  // spb=goal / nwc derivation-level bias (route-independent: reads only
  // the formation-time bit, never the term content).  Before the FT
  // mirror adopt below so both queues see the same priority.
  s->cp_pri[i] = atp_spb_scale(s->cp_pri[i], g_cp_gd);
  s->cp_gd[i] = g_cp_gd;
  s->cp_depth[i] = g_cp_formation_depth;   // Twee getDepth (see g_cp_formation_depth)
  if (g_atp_phase_detail) g_atp_cpg_us_kbo += atp_now_us() - _dkb_t0;
  if (s->cp_ultimate != NULL) {
    s->cp_ultimate[i] = is_ultimate;
    if (is_ultimate) s->n_cps_ultimate++;
  }
  s->cp_goal[i]  = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                     ? atp_goal_weight(s, lhs, rhs) : 0u;
  s->cp_sine[i]  = atp_cp_sine_level(s, lhs, rhs);
#ifdef THVM_ATPFT_CPQ
  // Mirror the CP into Arena-A FT cells.  When the generation-time fuse
  // is armed, its NF spans ARE this CP (lhs/rhs are ft_to_term of them),
  // so ADOPT them and skip the ft_from_term rebuild -- ownership moves
  // to the slot, and we NULL the fuse handles so the treat cleanup
  // (which runs after this push) does not double-free.  Otherwise
  // (emit-class or non-fused push) rebuild from the Terms.
  if (g_vcp_treat.armed && g_vcp_treat.fl != NULL && g_vcp_treat.fr != NULL) {
    atp_cp_ft_adopt(s, i, g_vcp_treat.fl, g_vcp_treat.fr,
                    s->cp_pri[i], /*origin=*/0xffffu);
    g_vcp_treat.fl = NULL;
    g_vcp_treat.fr = NULL;
  } else if (s->cp_packed[i] != NULL) {
    // Routed/fused push: the Term args are zeros or ride-along
    // sentinels by the routed-push contract -- the slot's packed
    // bytes are the authoritative content.  Decode the mirror from
    // them (byte->FT); converting the Term args fabricated degenerate
    // spans.
    const u8 *pp = s->cp_packed[i];
    AtpFt *fa = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *dfl = acp_unpack_term_ft(fa, &pp, 0);
    AtpFtCell *dfr = acp_unpack_term_ft(fa, &pp, 0);
    atp_cp_ft_adopt(s, i, dfl, dfr, s->cp_pri[i], /*origin=*/0xffffu);
  } else {
    atp_cp_ft_set(s, i, lhs, rhs, s->cp_pri[i], /*origin=*/0xffffu);
  }
#endif
  // K-D Heap secondary dimension: compute cp_pri2 with the alternate
  // weight mode (w2_mode, set via thvm_atp_set_w2).  Cheap -- ~one
  // symbol-count walk per CP -- and only filled when w2_modulo > 0.  In
  // GNN coop the GNN owns cp_pri2, so a fresh CP gets the neutral band
  // value until the next re-rank scores it (see ATP_GNN_COOP_NEUTRAL_PRI).
  _dkb_t0 = g_atp_phase_detail ? atp_now_us() : 0;
  s->cp_pri2[i]  = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                 : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, lhs, rhs, s->w2_mode)
                 :                        0u;
  if (g_atp_phase_detail) g_atp_cpg_us_kbo += atp_now_us() - _dkb_t0;
  u32 seq        = s->cp_seq_next++;
  s->cp_seq[i]   = seq;
  if (s->otter_rec) atp_otter_record(s, i, seq);
  if (s->cp_uid_a != NULL) { s->cp_uid_a[i] = g_cp_uid_a; s->cp_uid_b[i] = g_cp_uid_b; }
  // Raw-descriptor lane: stamp the descriptor for the FT-direct raw
  // emit class (g_rdesc_store armed by atp_cp_heap_push), or the
  // "no descriptor" sentinel for every other insert -- the pop-time
  // certifier / converter keys on out_face.  desc mode (packed == NULL)
  // sets the tag bit so the slot reads as deferred; cert mode keeps the
  // eager bytes + a clear bit and records the byte length in .weight
  // for the exact-length memcmp.  Both flags off: zero-cost skip.
  if ((s->use_raw_desc | s->use_raw_desc_cert) != 0u) {
    atp_cp_implicit_arrays_ensure(s);
    if (g_rdesc_store.armed != 0u) {
      s->cp_implicit[i].out_face = g_rdesc_store.out_face;
      s->cp_implicit[i].in_face  = g_rdesc_store.in_face;
      s->cp_implicit[i].sub_pre  = g_rdesc_store.sub_pre;
      s->cp_implicit[i].weight   = (packed == NULL) ? cp_nodes
                                                    : g_rdesc_store.len;
      s->cp_implicit[i].priority = s->cp_pri[i];
      if (packed == NULL) atp_cp_implicit_set(s, i);
      s->n_cps_rawdesc++;
      g_rdesc_store.armed = 0u;
    } else {
      s->cp_implicit[i].out_face = ATP_RDESC_NONE;
    }
  }
  // Fuse-neutral insert stream (THVM_ATP_INSERT_TRACE): one line per
  // engine insert with the comparator keys.  Both formation arms funnel
  // through this function, and the env is NOT in atp_cp_push_fuse_ok's
  // list, so the streams are directly diffable across arms -- the probe
  // that pins arm-divergence bugs (the lazy+classic collapse).
  {
    static int instr = -1;
    if (instr < 0) instr = atp_env_on("THVM_ATP_INSERT_TRACE");
    if (instr) {
      fprintf(stderr, "INSTR seq=%u pri=%u nodes=%u kind=%u\n",
              seq, s->cp_pri[i], cp_nodes, kind);
    }
  }
  atp_cp_form_trace(seq, cp_nodes, lhs, rhs);
  atp_hx_push(s, i, packed, cp_nodes);
  atp_topk_on_push(s, i);
  // All comparator keys for row i are final -- stamp the packed node
  // (refresh invariant) before the sift compares against it.
  atp_cp_heap_set(s, i, i);
  s->n_cps++;
  if (atp_kdheap_on()) atp_kdh_insert(s, i);
  else atp_cp_sift_up(s, i);
#ifdef ATP_FV_INDEX
  // Only build the CP-subsumption index when the query will actually run.
  // The WM presets disable use_queue_subsume, so the index would be built
  // (102M+ nodes on OrAssociativity) and never queried -- pure waste that
  // exhausts the GC heap.  Byte-identical when skipped: nothing reads it.
  if (s->use_queue_subsume)
    atp_fv_index_insert(s->fv_index, lhs, rhs, packed, seq);
#endif
}

// The LIVE auto-MaxWeight bound: base + slope * (deepest rule-LHS
// weight).  Recomputed against the current rule set so the bound grows
// as completion deepens R -- a CP deferred now becomes admissible once
// the rules it would interact with have themselves entered R.  Returns
// 0 (== unbounded) when the auto bound is disabled.
// Recompute the deepest CURRENT rule LHS and cache the auto bound.
// Called once per step (at the drain), NOT per CP push -- rescanning R
// on every push was a measured throughput sink.  The bound tracks the
// live rule set (it can fall as deep rules are interreduced away),
// which keeps it tight; completeness does NOT rest on bound
// monotonicity but on the overflow stash + force-drain in select_cp,
// so a tighter, fluctuating bound is still complete.
static void atp_auto_maxw_recompute(AtpState *s) {
  if (s->auto_max_cp_weight_base == 0u) { s->auto_max_cp_weight_cur = 0u; return; }
  // The deepest rule LHS only changes when R changes (add/interreduce).
  // Re-scanning all rules (atp_symbol_count over s->lhs[]) every step is
  // O(n_rules * steps) -- a hidden ~10% of wall (runtime profiling).  Skip
  // the scan when r_revision is unchanged since the last recompute.
  if (s->auto_maxw_rev == s->r_revision + 1u) return;
  u32 deepest = 0u;
  for (u32 k = 0; k < s->n_rules; k++) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    u32 w = atp_rule_symbol_count(s, k, 0u);
#else
    u32 w = atp_symbol_count(s->lhs[k]);
#endif
    if (w > deepest) deepest = w;
  }
  s->auto_maxw_rev = s->r_revision + 1u;
  s->max_rule_lhs_weight = deepest;
  u32 slope = s->auto_max_cp_weight_slope ? s->auto_max_cp_weight_slope : 2u;
  s->auto_max_cp_weight_cur = s->auto_max_cp_weight_base + slope * deepest;
}

static u32 atp_auto_maxw_bound(AtpState *s) {
  if (s->auto_max_cp_weight_base == 0u) return 0u;
  // Return the cached bound (refreshed once per step at the drain).
  return s->auto_max_cp_weight_cur;
}

// Park an over-bound CP on the overflow stash (auto-MaxWeight).  Takes
// ownership of `packed`.  Drained back into the heap by
// atp_auto_maxw_drain once the bound grows past its weight, so the CP
// is deferred, NEVER discarded -- completeness preserved.
static void atp_cp_stash_push(AtpState *s, u8 *packed, u32 cp_nodes,
                              u32 trace, u8 is_ultimate, u8 kind) {
  if (s->n_cp_stash >= s->cp_stash_cap) {
    u32 ncap = s->cp_stash_cap ? s->cp_stash_cap * 2u : 256u;
    u8 **np  = (u8 **)realloc(s->cp_stash_packed, ncap * sizeof(u8 *));
    u32 *nt  = (u32 *)realloc(s->cp_stash_trace,  ncap * sizeof(u32));
    u32 *nn  = (u32 *)realloc(s->cp_stash_nodes,  ncap * sizeof(u32));
    u8  *nk  = (u8  *)realloc(s->cp_stash_kind,   ncap * sizeof(u8));
    // Lazy-grow cp_stash_ultimate only when the flag is on; engine
    // byte-identical when off.
    u8  *nu  = s->use_initial_ultimate
                 ? (u8 *)realloc(s->cp_stash_ultimate, ncap * sizeof(u8))
                 : NULL;
    if (np == NULL || nt == NULL || nn == NULL || nk == NULL
        || (s->use_initial_ultimate && nu == NULL)) {
      // Allocation failure: rather than leak or lose the CP, admit it
      // directly (slow path, but sound -- never drops a proof CP).
      free(np); free(nt); free(nn); free(nk); free(nu);
      Term l = 0, r = 0;
      acp_unpack(packed, &l, &r);
      atp_cp_heap_insert_packed(s, packed, cp_nodes, l, r, trace,
                                is_ultimate, 0xffffffffu, kind);
      return;
    }
    s->cp_stash_packed = np; s->cp_stash_trace = nt; s->cp_stash_nodes = nn;
    s->cp_stash_kind = nk;
    if (s->use_initial_ultimate) s->cp_stash_ultimate = nu;
    s->cp_stash_cap = ncap;
  }
  s->cp_stash_packed[s->n_cp_stash]  = packed;
  s->cp_stash_trace[s->n_cp_stash]   = trace;
  s->cp_stash_nodes[s->n_cp_stash]   = cp_nodes;
  s->cp_stash_kind[s->n_cp_stash]    = kind;
  if (s->cp_stash_ultimate != NULL) {
    s->cp_stash_ultimate[s->n_cp_stash] = is_ultimate;
  }
  s->n_cp_stash++;
}

// Re-admit every stashed CP now within the (recomputed, possibly
// grown) bound.  Compacts the stash in place.  Called after a rule is
// added (the bound may have grown) and whenever the live queue empties
// (force=1 admits the lightest stashed CP regardless, so selection
// never starves while CPs remain).
static void atp_auto_maxw_drain(AtpState *s, u8 force) {
  if (s->auto_max_cp_weight_base == 0u || s->n_cp_stash == 0u) return;
  u32 bound = atp_auto_maxw_bound(s);
  // When forced and nothing is within bound, raise the working bound to
  // the lightest stashed CP so at least one re-enters -- a monotone
  // growth that guarantees every stashed CP is eventually selected.
  if (force && s->n_cps == 0u) {
    u32 lightest = 0xffffffffu;
    for (u32 k = 0; k < s->n_cp_stash; k++) {
      if (s->cp_stash_nodes[k] < lightest) lightest = s->cp_stash_nodes[k];
    }
    if (lightest != 0xffffffffu && lightest > bound) bound = lightest;
  }
  u32 w = 0;
  for (u32 r = 0; r < s->n_cp_stash; r++) {
    if (s->cp_stash_nodes[r] <= bound) {
      Term l = 0, rr = 0;
      acp_unpack(s->cp_stash_packed[r], &l, &rr);
      u8 ult = (s->cp_stash_ultimate != NULL) ? s->cp_stash_ultimate[r] : 0u;
      atp_cp_heap_insert_packed(s, s->cp_stash_packed[r],
                                s->cp_stash_nodes[r], l, rr,
                                s->cp_stash_trace[r], ult, 0xffffffffu,
                                s->cp_stash_kind[r]);
    } else {
      s->cp_stash_packed[w]   = s->cp_stash_packed[r];
      s->cp_stash_trace[w]    = s->cp_stash_trace[r];
      s->cp_stash_nodes[w]    = s->cp_stash_nodes[r];
      s->cp_stash_kind[w]     = s->cp_stash_kind[r];
      if (s->cp_stash_ultimate != NULL) {
        s->cp_stash_ultimate[w] = s->cp_stash_ultimate[r];
      }
      w++;
    }
  }
  s->n_cp_stash = w;
}

#ifdef THVM_ATPFT_NORM
// THVM_ATP_FT_EMIT_CHECK for the treated survivor's queue pack: the
// FT-side fused pack must byte-match the classic decode-then-pack
// (ft_to_term x2 -> thvm_normalize_vars -> acp_pack_ex) and the fused
// weigh must verdict-match the classic memo-free KBO -- the exact
// FUSE_CHECK discipline, sourced from the NF cells.
static void vcp_treat_pack_certify(const AtpState *s, const u8 *packed,
                                   u32 flen, u32 cp_nodes, u32 lhs_nodes) {
  Term cl = ft_to_term(g_vcp_treat.fl);
  Term cr = ft_to_term(g_vcp_treat.fr);
  KboCmp cf = acp_fuse_kbo(s->kbo);
  thvm_normalize_vars(&cl, &cr);
  u32 rlen = 0u, rn = 0u, rl = 0u;
  u8 *ref = acp_pack_ex(cl, cr, &rlen, &rn, &rl);
  KboCmp cc = thvm_kbo_nomemo(cl, cr, s->kbo);
  if (rlen != flen || rn != cp_nodes || rl != lhs_nodes ||
      memcmp(ref, packed, rlen) != 0 || cf != cc) {
    fprintf(stderr,
            "ATP_FT_EMIT_CHECK TREATPACK MISMATCH len=%u/%u nodes=%u/%u "
            "lhs=%u/%u bytes=%s kbo=%d/%d\n",
            flen, rlen, cp_nodes, rn, lhs_nodes, rl,
            (rlen == flen && memcmp(ref, packed, rlen) == 0) ? "EQ" : "DIFF",
            (int)cf, (int)cc);
    abort();
  }
  free(ref);
}
#endif // THVM_ATPFT_NORM

// Push one CP onto the heap.  Computes its priority once (the cost
// the old select_cp paid n times per step) and sifts up.  O(log n).
// `is_ultimate`=1 propagates the Waldmeister Act_ultimate front-rank
// to the CP slot (effective only when s->use_initial_ultimate is on).
// `raw_untreated`=1 marks the WM KPBehandelt >=50 raw class (lazy push,
// no generation-time treatment): it bypasses the auto-MaxWeight
// deferral stash -- WM has no such lane, and the stash has no FIFO
// dimension, so parking the raw class there would swallow it where WM
// buries it IN the heap at raw weight (recentCPinsert, reachable by
// the periodic FIFO pick).  The lossy caps (max_cp_queue and the WM
// -mw analog max_cp_weight) still apply on the raw weight, exactly as
// WM's hard cap would.
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace,
                             u8 is_ultimate, u8 raw_untreated) {
  g_twee_dbg_ctx = raw_untreated ? 2u : 1u;
  // Hard queue-size cap (memory leash): drop before packing (saves the
  // acp_pack malloc) once the live queue is full.  Lossy; the periodic
  // FIFO/priority selection still fires over the kept CPs.
  if (s->max_cp_queue > 0u && s->n_cps >= s->max_cp_queue) {
    s->n_cps_dropped_qcap++;
    atp_sj_peak_pending_drop();
    return;
  }
  // Pack the CP into a byte string outside the managed heap.  The pack
  // walk counts per-side nodes as it goes; the weigh below reuses them.
  // Fused push loop (g_cp_push_fused): lhs/rhs arrive UN-renumbered;
  // acp_pack_fused renumbers during the byte emit and arms the flat
  // weigh state (consumed by atp_weigh_compare, cleared on every exit
  // below).  An anomaly falls back to the classic pipeline, starting
  // with the treated renumber the fused caller skipped.
  u32  cp_nodes  = 0u;
  u32  lhs_nodes = 0u;
  u8  *packed    = NULL;
  u8   rdesc     = 0u;   // raw-desc slot: no bytes stored, descriptor instead
  if (g_cp_push_fused && g_vcp_emit.armed) {
    // FT-direct push: consume the virtual emit --
    // bytes from the scratch, counts from the stash, the fused weigh
    // state already armed by vcp_emit_fused.  The FUSE_CHECK certifier
    // below has no Terms to rebuild from here; the FT route's own
    // per-CP certifier is THVM_ATP_FT_EMIT_CHECK (sw_form_push).
    u32 flen = g_vcp_emit.len;
    if (g_rdesc_push.armed != 0u && s->use_raw_desc != 0u) {
      // Raw-desc store: the emit's gates/counts/weigh state are
      // consumed exactly as the eager arm consumes them; only the
      // byte string is NOT copied out -- the descriptor re-derives it
      // at pop.  (Certifier mode takes the eager arm below and stamps
      // the descriptor alongside the bytes.)
      rdesc = 1u;
    } else {
      packed = (u8 *)malloc(flen ? flen : 1u);
      if (packed == NULL) thvm_fatal("atp_cp_heap_push: OOM");
      memcpy(packed, g_acp_scratch, flen);
    }
    if (g_rdesc_push.armed != 0u &&
        (s->use_raw_desc | s->use_raw_desc_cert) != 0u) {
      g_rdesc_store       = g_rdesc_push;
      g_rdesc_store.len   = flen;
      g_rdesc_store.armed = 1u;
    }
    g_rdesc_push.armed = 0u;   // consume-once, mirroring g_vcp_emit
    cp_nodes  = g_vcp_emit.nodes;
    lhs_nodes = g_vcp_emit.lhs_nodes;
    g_acp_fuse_n_ok++;
#ifdef THVM_ATPFT_NORM
  } else if (g_cp_push_fused && g_vcp_treat.armed) {
    // Treated FT-direct push (phase 2b): pack the
    // survivor's NF cells straight to bytes + the armed fused weigh
    // (ftp_pack_fused).  An anomaly (marker syms etc.) decodes the
    // cells and rejoins the classic fused pipeline below -- exactly
    // the pair the classic route would have packed.
    static int treat_check = -1;
    if (treat_check < 0) treat_check = atp_env_on("THVM_ATP_FT_EMIT_CHECK");
    // Twee-score byte-queue retirement: under the exact conditions of
    // the classic skip-pack below (CPQ mirror live, subsumption off, no
    // packed-byte consumer compiled in) plus no weight-cap/stash reader
    // of the bytes, the adopted FT spans ARE the queue payload and the
    // malloc'd byte string is write-only -- classic mode 8 never
    // allocated it (its pushes took the skip-pack), so skipping keeps
    // the fused arm's RSS at the classic arm's level (the 3M-CP-queue
    // regime runs against the memory leash).  Confined to mode 8: the
    // long-certified default arms keep their byte behavior.
    u8 twee_skip_buf = 0u;
#ifdef THVM_ATPFT_CPQ
#if !defined(THVM_ATP_COLLAPSE_DEFER) && !defined(THVM_ATP_TIE_RHS)
    twee_skip_buf = (s->cp_weight_mode == ATP_CP_WEIGHT_TWEE &&
                     !s->use_queue_subsume &&
                     s->auto_max_cp_weight_base == 0u &&
                     s->max_cp_weight == 0u && !treat_check) ? 1u : 0u;
#endif
#endif
    u32 flen = 0u;
    packed = ftp_pack_fused(s->kbo, g_vcp_treat.fl, g_vcp_treat.fr,
                            &flen, &cp_nodes, &lhs_nodes,
                            (u8)!twee_skip_buf);
    if (packed != NULL) {
      g_acp_fuse_n_ok++;
      if (twee_skip_buf) {
        packed = NULL;   // scratch sentinel -- not owned, nothing reads it
      } else if (treat_check) {
        vcp_treat_pack_certify(s, packed, flen, cp_nodes, lhs_nodes);
      }
    } else {
      g_vcp_treat_n_pkfb++;
      lhs = ft_to_term(g_vcp_treat.fl);
      rhs = ft_to_term(g_vcp_treat.fr);
      packed = acp_pack_fused(s->kbo, lhs, rhs, &flen, &cp_nodes,
                              &lhs_nodes);
      if (packed == NULL) {
        g_acp_fuse_n_fb++;
        thvm_normalize_vars(&lhs, &rhs);
      } else {
        g_acp_fuse_n_ok++;
      }
    }
#endif
  } else if (g_cp_push_fused) {
    u32 flen = 0u;
    packed = acp_pack_fused(s->kbo, lhs, rhs, &flen, &cp_nodes, &lhs_nodes);
    if (packed == NULL) {
      g_acp_fuse_n_fb++;
      thvm_normalize_vars(&lhs, &rhs);
    } else {
      g_acp_fuse_n_ok++;
      // THVM_ATP_FUSE_CHECK=1: per-CP differential certifier -- the
      // classic normalize+pack must byte-match the fused emit and the
      // classic memo-free KBO must verdict-match the fused weigh.
      static int fuse_check = -1;
      if (fuse_check < 0) fuse_check = atp_env_on("THVM_ATP_FUSE_CHECK");
      if (fuse_check) {
        Term cl = lhs, cr = rhs;
        thvm_normalize_vars(&cl, &cr);
        u32 rlen = 0u, rn = 0u, rl = 0u;
        u8 *ref = acp_pack_ex(cl, cr, &rlen, &rn, &rl);
        KboCmp cf = acp_fuse_kbo(s->kbo);
        KboCmp cc = thvm_kbo_nomemo(cl, cr, s->kbo);
        if (rlen != flen || rn != cp_nodes || rl != lhs_nodes ||
            memcmp(ref, packed, rlen) != 0 || cf != cc) {
          fprintf(stderr,
                  "ATP_FUSE_CHECK MISMATCH len=%u/%u nodes=%u/%u "
                  "lhs_nodes=%u/%u bytes=%s kbo=%d/%d\n",
                  flen, rlen, cp_nodes, rn, lhs_nodes, rl,
                  (rlen == flen && memcmp(ref, packed, rlen) == 0)
                    ? "EQ" : "DIFF",
                  (int)cf, (int)cc);
          abort();
        }
        free(ref);
      }
    }
  }
  if (packed == NULL && !rdesc) {
    u8 skip_pack = 0u;
#ifdef THVM_ATPFT_CPQ
#if !defined(THVM_ATP_COLLAPSE_DEFER) && !defined(THVM_ATP_TIE_RHS)
    // Byte-queue retirement: when subsumption is off (the WM presets set
    // use_queue_subsume=0, so neither the FV index nor the non-FV scan
    // is built) and no packed-byte heap-compare is compiled in, NOTHING
    // reads cp_packed -- slot_read, the peeks, and the priority rebuild
    // all source the FT queue (atp_cp_ft_set stores the pair at insert).
    // Skip the malloc+pack; recover the node counts it would have
    // returned via the identical preorder tally.  cp_packed[i] stays
    // NULL on a non-implicit slot, inert here (the cp_packed==NULL <=>
    // implicit reliance also sits behind use_queue_subsume).
    if (!s->use_queue_subsume) {
      lhs_nodes = acp_count_term(lhs);
      cp_nodes  = lhs_nodes + acp_count_term(rhs);
      skip_pack = 1u;
    }
#endif
#endif
    if (!skip_pack)
      packed = acp_pack_ex(lhs, rhs, NULL, &cp_nodes, &lhs_nodes);
  }
  // Waldmeister NEVER weight-caps an ultimate CP -- an initial axiom is
  // the spec (Act_ultimate, w1=INT32_MIN) and must always enter the
  // queue.  Deferring a heavy axiom to the auto-MaxWeight stash (or
  // dropping it under the hard cap) delays/loses it and, since the
  // intake canonicalize only tags CPs already in the queue, leaves the
  // heavy axiom un-ultimate -- it then pops hundreds of picks late at
  // its computed weight (the ShefferAxioms__Commutativity firstdiv-3
  // divergence: a 3rd axiom of >20 nodes stashed past the base-20 bound).
  if (!is_ultimate) {
    // Waldmeister MaxWeight (hard cap): drop an over-weight critical pair
    // before it enters the queue.  0 = unbounded.  This is the LOSSY
    // bound (flag-gated by the caller setting max_cp_weight); the default
    // engine leaves it 0.  The weight includes the LADR penalties when
    // they are on (Prover9 max_weight bounds the penalized clause weight).
    if (s->max_cp_weight > 0u &&
        cp_nodes + atp_ladr_penalty(s, lhs, rhs) > s->max_cp_weight) {
      free(packed);
      g_acp_fuse.valid = 0u;
      g_rdesc_store.armed = 0u;
      atp_sj_peak_pending_drop();
      return;
    }
    // Twee --max-term-size (per-side cap): drop the CP when EITHER side
    // exceeds the bound, i.e. max(|lhs|,|rhs|) > cap.  Twee's dequeue
    // applies cfg_accept_term (checkSize n t = KBO.size t <= n) to BOTH
    // overlap sides (Twee.hs:423-424) -- with default weights the raw node
    // count.  Unlike the sum cap above this separates a one-huge-side
    // monster (the assoc-lifted H7xAC rules) from the both-sides-moderate
    // CPs the proof needs; the sum cap floods on LAT138-1 because no total
    // threshold cleanly splits 30+8 from 20+20.  Same lossy drop.
    if (s->max_cp_side > 0u &&
        (lhs_nodes > s->max_cp_side ||
         (cp_nodes - lhs_nodes) > s->max_cp_side)) {
      free(packed);
      g_acp_fuse.valid = 0u;
      g_rdesc_store.armed = 0u;
      atp_sj_peak_pending_drop();
      return;
    }
    // Auto-MaxWeight (completeness-preserving): defer an over-bound CP to
    // the overflow stash rather than dropping it.  Disabled when base==0.
    // The WM raw>=50 class never defers (see the header comment) -- but that
    // class is exactly what explodes the CP queue on WolframAxioms, and the
    // stash is completeness-preserving (re-admitted as the bound grows), so
    // opt-in THVM_ATP_STASH_RAW lets it cover the raw class too: the queue
    // bounds WITHOUT dropping, unlike the lossy hard cap.  Off by default ->
    // Sheffer/faithful paths keep the raw-never-defer behavior.
    static int stash_raw = -1;
    if (stash_raw < 0) stash_raw = getenv("THVM_ATP_STASH_RAW") ? 1 : 0;
    if (s->auto_max_cp_weight_base > 0u && (!raw_untreated || stash_raw)) {
      u32 bound = atp_auto_maxw_bound(s);
      if (bound > 0u && cp_nodes > bound) {
        if (rdesc) {
          // The overflow stash stores packed bytes only -- reify the
          // deferred emit from the still-live scratch (identical bytes
          // to the eager arm's copy) so the stash verdict matches.
          packed = (u8 *)malloc(g_rdesc_store.len ? g_rdesc_store.len : 1u);
          if (packed == NULL) thvm_fatal("atp_cp_heap_push: OOM");
          memcpy(packed, g_acp_scratch, g_rdesc_store.len);
        }
        atp_cp_stash_push(s, packed, cp_nodes, trace, is_ultimate,
                          g_cp_push_kind);
        g_acp_fuse.valid = 0u;
        g_rdesc_store.armed = 0u;
        // The overflow stash is packed-only; the deferred CP re-enters
        // via insert_packed later with no peak (degrade-to-peakless).
        atp_sj_peak_pending_drop();
        return;
      }
    }
  }
  if (g_atp_phase_detail) {
    if (raw_untreated) {
      g_acp_pack_nodes_raw += cp_nodes;
      g_acp_pack_n_raw++;
    } else {
      g_acp_pack_nodes_treated += cp_nodes;
      g_acp_pack_n_treated++;
    }
  }
  // Normalized-size CP scoring under lazy-normalize (THVM_ATP_LAZY_
  // SCORE_NF, doc block at atp_ifeq_strip_env): the raw class weighs
  // on a step-capped-NF per-side size estimate instead of its raw
  // tower size.  Score-only -- the queued bytes/spans and every
  // weight-cap verdict above keep the true raw counts.  (The TWEE
  // weight mode ignores the count base -- atp_cp_twee_fused_base --
  // so the override is inert there.)
  u32 score_nodes = cp_nodes, score_lhs = lhs_nodes;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (raw_untreated && s->use_lazy_normalize) {
    u32 nf_cap = atp_lazy_scorenf_cap();
    if (nf_cap > 0u) {
      u32 nl = 0u, nr = 0u;
      // Raw-desc slot: no malloc'd bytes, but the emit's bytes are
      // still live in the scratch -- probe those (identical content).
      if (atp_cp_lazy_scorenf_probe(s, rdesc ? g_acp_scratch : packed,
                                    lhs, rhs, nf_cap,
                                    &nl, &nr)) {
        g_scorenf_n++;
        if (nl + nr < cp_nodes) g_scorenf_shrunk++;
        score_nodes = nl + nr;
        score_lhs   = nl;
      }
    }
  }
#endif
  if (atp_hx_on(s)) g_hx.raw_pending = raw_untreated;
  atp_cp_heap_insert_packed(s, packed, score_nodes, lhs, rhs, trace,
                            is_ultimate, score_lhs, g_cp_push_kind);
  g_acp_fuse.valid = 0u;   // the fused weigh state is consume-once
  g_twee_dbg_ctx = 0u;
}

// Lazily allocate the deferred-CP descriptor array + tag bitset to the
// current cp_cap (the caller has already run atp_ensure_cp_cap, which
// keeps both in lockstep with cp_packed once they exist).  First-push-
// only; under use_implicit_cp == 0 neither is ever allocated and every
// helper above stays a single-branch no-op.
static void atp_cp_implicit_arrays_ensure(AtpState *s) {
  if (s->cp_is_implicit != NULL) return;
  u32 cap = s->cp_cap;
  s->cp_implicit = (AtpCpImplicit *)malloc((size_t)cap
                                           * sizeof(AtpCpImplicit));
  s->cp_is_implicit = (u8 *)calloc((size_t)(cap + 7u) / 8u, 1u);
  if (s->cp_implicit == NULL || s->cp_is_implicit == NULL) {
    thvm_fatal("atp_cp_implicit_arrays_ensure: OOM");
  }
  for (u32 i = 0; i < cap; i++) {
    s->cp_implicit[i].parent_a_trace_id = ATP_TRACE_NONE;
    s->cp_implicit[i].parent_b_trace_id = ATP_TRACE_NONE;
    s->cp_implicit[i].overlap_position  = 0;
    s->cp_implicit[i].weight            = 0u;
    s->cp_implicit[i].priority          = 0u;
  }
}

// Deferred-CP (`implicit_pair`) push: record the critical pair as a
// 20-byte descriptor instead of an `acp_pack`'d byte string -- the WM
// passive-set discipline (KPVerwaltung.c stores implicit term pairs and
// materializes only on treatment via TPR_TP2ParIntermed).  The slot is
// TRACE-BACKED: its raw unified terms already live unconditionally in
// the TRACE_CP entry at `cp_trace[i]` (children 2/3, pushed by
// atp_trace_push_cp for every queued CP), so materialization at
// selection reads them back for the pop-time normalize that already
// runs unconditionally -- no re-unification, no extra storage.  The
// parent trace ids are cached in the descriptor for the dead-parent /
// orphan check without a trace-entry walk.
//
// All weight bookkeeping runs HERE, while the unified terms are still
// live (WM C_Classify-then-discard): cp_pri / cp_goal / cp_pri2 are
// computed exactly as atp_cp_heap_insert_packed computes them, then the
// terms are dropped (the trace keeps the raw forms alive).  Implicit
// slots are NEVER inserted into the FV subsumption index -- WM has no
// queue-vs-queue subsumption, so the implicit passive set does not
// participate (see atp_fv_index_rebuild).
//
// Returns 1 when the CP was consumed (queued as a descriptor, or
// dropped by the MaxWeight hard cap -- identical verdict to
// atp_cp_heap_push's), 0 when the caller must take the eager packed
// path instead (auto-MaxWeight over-bound CPs go to the packed overflow
// stash, which has no implicit lane).
static u8 atp_cp_implicit_push(AtpState *s, Term lhs, Term rhs,
                               u32 parent_a_trace_id, u32 parent_b_trace_id,
                               u32 cp_trace_id, u8 is_ultimate) {
  // Same structural weight acp_pack counts (one tick per preorder node)
  // -- computed without the pack walk or its malloc.
  u32 cp_lhs_nodes = atp_symbol_count(lhs);
  u32 cp_rhs_nodes = atp_symbol_count(rhs);
  u32 cp_nodes = cp_lhs_nodes + cp_rhs_nodes;
  // Hard queue-size cap (memory leash): same lossy drop as atp_cp_heap_push.
  if (s->max_cp_queue > 0u && s->n_cps >= s->max_cp_queue) {
    s->n_cps_dropped_qcap++;
    return 1u;   // consumed (dropped) -- identical verdict to the MaxWeight cap
  }
  // Waldmeister MaxWeight hard cap: same lossy drop as atp_cp_heap_push.
  if (s->max_cp_weight > 0u &&
      cp_nodes + atp_ladr_penalty(s, lhs, rhs) > s->max_cp_weight) return 1u;
  // Twee --max-term-size per-side cap (see atp_cp_heap_push): drop when
  // EITHER side's node count exceeds the bound.  Same lossy consumed verdict.
  if (s->max_cp_side > 0u &&
      (cp_lhs_nodes > s->max_cp_side || cp_rhs_nodes > s->max_cp_side))
    return 1u;
  // Auto-MaxWeight: the overflow stash stores packed byte strings, so an
  // over-bound CP falls back to the eager path (which re-tests the bound
  // and stashes).  Not in the WM preset; deferred-by-construction.
  if (s->auto_max_cp_weight_base > 0u) {
    u32 bound = atp_auto_maxw_bound(s);
    if (bound > 0u && cp_nodes > bound) return 0u;
  }
  atp_ensure_cp_cap(s, s->n_cps + 1);
  atp_cp_implicit_arrays_ensure(s);
  u32 i = s->n_cps;
  // Append at row i == heap position i (see atp_cp_heap_insert_packed;
  // the packed node is stamped pre-sift, after all key writes).
  s->cp_pos[i]  = i;
  s->cp_packed[i] = NULL;          // deferred slot marker
  atp_cp_stash_clear(s, i);        // backstop: no stash backs a descriptor
  s->cp_trace[i]  = cp_trace_id;
  s->cp_kind[i]   = g_cp_push_kind;
  s->cp_last_norm_r_revision[i] = ATP_CP_NORM_COOKIE_NONE;
  s->cp_deprio[i] = g_cp_push_deprio;
  s->cp_pri[i]    = s->cp_deprio[i]
                      ? ATP_CP_PRI_NEVER
                      : atp_cp_priority_sized(s, lhs, rhs, cp_nodes);
  s->cp_pri[i]    = atp_spb_scale(s->cp_pri[i], g_cp_gd);
  s->cp_gd[i]     = g_cp_gd;
  s->cp_depth[i] = g_cp_formation_depth;   // Twee getDepth (see g_cp_formation_depth)
  if (s->cp_ultimate != NULL) {
    s->cp_ultimate[i] = is_ultimate;
    if (is_ultimate) s->n_cps_ultimate++;
  }
  s->cp_goal[i]   = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                      ? atp_goal_weight(s, lhs, rhs) : 0u;
  s->cp_sine[i]   = atp_cp_sine_level(s, lhs, rhs);
  s->cp_pri2[i]   = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                  : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, lhs, rhs,
                                                              s->w2_mode)
                  :                        0u;
  s->cp_seq[i]    = s->cp_seq_next++;
  if (s->cp_uid_a != NULL) { s->cp_uid_a[i] = g_cp_uid_a; s->cp_uid_b[i] = g_cp_uid_b; }
  atp_cp_form_trace(s->cp_seq[i], cp_nodes, lhs, rhs);
  // No THVM_ATPFT_CPQ mirror: the FT queue's slot-occupied invariant
  // tracks cp_packed[i], and a deferred slot is exactly the NULL case
  // (atp_cp_ft_clear / _move are NULL-idempotent).
  s->cp_implicit[i].parent_a_trace_id = parent_a_trace_id;
  s->cp_implicit[i].parent_b_trace_id = parent_b_trace_id;
  s->cp_implicit[i].overlap_position  = 0;
  s->cp_implicit[i].weight            = cp_nodes;
  s->cp_implicit[i].priority          = s->cp_pri[i];
  atp_cp_implicit_set(s, i);
  // All comparator keys for row i are final -- stamp the packed node
  // (refresh invariant) before the sift compares against it.
  atp_cp_heap_set(s, i, i);
  s->n_cps++;
  s->n_cps_implicit++;
  atp_hx_push(s, i, NULL, cp_nodes);
  atp_topk_on_push(s, i);
  if (atp_kdheap_on()) atp_kdh_insert(s, i);
  else atp_cp_sift_up(s, i);
  return 1u;
}

// Waldmeister CP-queue interleaving (a port of KPVerwaltung.c's
// `CPdimension`).  Waldmeister keeps the set of unselected equations
// in a K-D heap with TWO keys -- a weight key and a FIFO insertion
// key -- and `CPdimension` returns the FIFO dimension for `thresholdCP`
// of every `moduloCP` selections, the weight dimension otherwise.
//
// WM derives (moduloCP, thresholdCP) from the `-pq interleave=f.h`
// token: `moduloCP = f + h`, `thresholdCP = f` (KPVerwaltung.c:1484-
// 1485); CPdimension() (KPVerwaltung.c:846-848) returns the FIFO
// dimension when `AnzAktivierterRE % moduloCP < thresholdCP`.
//
// TWO reference configurations exist and they DIFFER here:
// - PLAIN `wmcli` (bare `-pq`, no interleave token): thresholdCP 0,
//   FIFO never fires -- a pure smallest-weight heap.  The historical
//   wm_align_matrix baseline rows were recorded against this config
//   (and a hardcoded threshold 1 forked CombinatorAxioms SKIToBCKW at
//   selection 303, which is why the preset once pinned threshold 0).
// - `wmcli -auto` (= FindEquationalProof = Method ->
//   "WaldmeisterProcess"): Automodus Orkus/StdS carries `itl(mi)` =
//   `-pq interleave=1.50` -> thresholdCP 1, moduloCP 51.  THIS is the
//   config the Waldmeister preset targets; the default below follows
//   it.  THVM_ATP_FIFO_THRESHOLD=0 restores the plain-wmcli mode.
//
// The legacy (non-WM) path keeps the raw-selection-count end-of-window
// behaviour the weight-order unit tests pin.
#define ATP_CP_FIFO_MODULO     11u
#define ATP_CP_FIFO_THRESHOLD   1u

// WM `CPdimension()` under the faithful WM preset (use_intake_order):
// the parity reference is `wmcli -auto` (= FindEquationalProof =
// Method -> "WaldmeisterProcess"), whose Orkus/StdS strategy carries
// `itl(mi)` = `-pq interleave=1.50` (ANA/YFiles.c Schrittweiten[mi] =
// {1, 50}) -> thresholdCP = 1, moduloCP = 51 (KPVerwaltung.c:1484-1485):
// ONE oldest-CP (FIFO) selection whenever AnzAktivierterRE % 51 < 1,
// including the run-in at count 0 before the first activation (the
// pinned WolframAxioms__DoubleNegation divergence: WM sel 117 FIFO-pops
// the old rule1xrule1 CP while queued 929-weight duplicates wait; a
// weight pop takes the duplicate instead -> firstdiv 117).
// THVM_ATP_FIFO_THRESHOLD=0 restores the PLAIN-wmcli behaviour (bare
// `-pq` carries no interleave token -> thresholdCP 0, FIFO never
// fires) that the wm_align_matrix baseline rows were recorded against.
// Outside the preset thvm keeps the legacy raw-selection-count
// interleave the weight-order unit tests rely on.
static inline u8 atp_cp_fifo_dimension(const AtpState *s) {
  u32 modulo = s->fifo_modulo ? s->fifo_modulo : ATP_CP_FIFO_MODULO;
  // Vampire AWPassiveClauseContainer balance: age pick first, then the
  // weight picks, counted in activations.
  if (s->use_age_depth) {
    u32 ap = s->fifo_age_part ? s->fifo_age_part : 1u;
    return (s->n_activated_re % modulo) < ap;
  }
  if (s->use_intake_order) {
    static int thr = -2;
    if (thr == -2) {
      const char *e = getenv("THVM_ATP_FIFO_THRESHOLD");
      thr = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 1;
    }
    if (thr <= 0) return 0u;
    return (s->n_activated_re % modulo) < (u32)thr;
  }
  u32 thr = s->fifo_age_part ? s->fifo_age_part : ATP_CP_FIFO_THRESHOLD;
  return (modulo - thr) <= (s->cp_select_count % modulo);
}

// Select and remove one CP from the queue.  Most calls take the heap
// min (lowest (cp_pri, cp_seq) -- the weight heuristic); ATP_CP_FIFO_-
// THRESHOLD of every ATP_CP_FIFO_MODULO calls instead take the OLDEST
// queued CP (lowest cp_seq) -- Waldmeister's FIFO dimension.
// Extraction works at an arbitrary slot j: backfill from the last
// slot, then sift the backfilled element (one of sift-up / sift-down
// is a no-op).
//
// Returns 1 on success (out-params populated), 0 on empty queue.
// ENIGMA training-data recorder (defined with the feature block below);
// forward-declared here for the gated hook at the end of this function.
static void atp_cp_feat_record(AtpState *s, Term lhs, Term rhs,
                               u32 trace_id);
// Lazy orphan murder: defined with the orphan-murder setters below.
static int atp_cp_is_orphan(const AtpState *s, u32 cp_trace);
static int atp_trace_is_dead(const AtpState *s, u32 trace_id);
// WM `dokgS` test (1-step join via a single existing rule); defined
// alongside the per-CP-push subsume drop a few thousand lines down.
static u8 atp_cp_rule_subsumed(AtpState *s, Term lhs, Term rhs);
// WM -ks "s" pop-time E-subsumption test; defined next to
// atp_cp_rule_subsumed.
static u8 atp_pop_eq_subsumed(AtpState *s, Term lhs, Term rhs);
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// FT siblings (defined next to the Term versions): the pop treatment
// decides the -ks "s" drop on the flat NFs so a subsumed pop never
// materializes heap Terms.  Returns 0/1 verdict; 2 = cannot decide
// (missing rule mirror) -- caller falls back to the Term route.
static int atp_pop_eq_subsumed_ft(AtpState *s, const AtpFtCell *lhs,
                                  const AtpFtCell *rhs);
static u8 atp_order_is_plain_kbo(const AtpState *s);
static u8 ft_sym_multiset_eq(const AtpFtCell *a, const AtpFtCell *b);
#ifdef THVM_ATP_AC
static u8 atp_cp_perm_subsumed_ft(const AtpFtCell *lhs,
                                  const AtpFtCell *rhs);
#endif
#endif
// On-demand FT-mirror population for any caller of atp_rewrite_normalize_ft
// (definition with atp_cp_trivially_joinable, far below).
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static inline void atp_ft_mirror_ensure(AtpState *s);
#endif

// THVM_ATP_CLOCK_TRACE line for one LRS horizon recompute.
static void atp_lrs_trace(const AtpState *s, u64 elapsed, u64 remaining,
                          u64 predicted, u32 cps0) {
  FILE *tf = atp_clock_trace();
  if (tf == NULL) {
    return;
  }
  fprintf(tf, "lrs sel=%u step=%u el=%llu rem=%llu pred=%llu horizon=%u "
          "cps0=%u cps1=%u", s->cp_select_count, s->step,
          (unsigned long long)elapsed, (unsigned long long)remaining,
          (unsigned long long)predicted, s->lrs_horizon, cps0, s->n_cps);
  atp_clock_trace_end(tf);
}

// Age key of queued row `vv` for the LRS age limit: the FIFO order
// (seq), or Vampire's AgeQueue order (depth, then weight) under
// --age-depth.  Smaller = reached earlier by the age picks.
static inline u64 atp_lrs_age_key(const AtpState *s, u32 pos, u32 vv) {
  if (s->use_age_depth && s->cp_depth != NULL)
    return ((u64)s->cp_depth[vv] << 32) | (u64)s->cp_heap[pos].pri;
  return (u64)s->cp_heap[pos].seq;
}

// k-th smallest (0-indexed) of a[0..n) by quickselect; permutes a.
static u64 atp_u64_select(u64 *a, u32 n, u32 k) {
  u32 lo = 0, hi = n - 1u;
  while (lo < hi) {
    u64 pivot = a[(lo + hi) >> 1];
    u32 i = lo, j = hi;
    for (;;) {
      while (a[i] < pivot) i++;
      while (a[j] > pivot) j--;
      if (i >= j) break;
      u64 t = a[i]; a[i] = a[j]; a[j] = t;
      i++; if (j == 0u) break; j--;
    }
    if (j < k) lo = j + 1u;
    else       hi = j;
  }
  return a[k];
}

// LRS horizon recomputation (Riazanov & Voronkov, JSC 36, 2003).  Given
// the observed selection rate and the remaining budget, predict how
// many MORE CPs the saturator will pop, then take the `k`-th smallest
// cp_pri in the queue as the weight horizon: any CP with cp_pri >
// horizon is provably unreachable in budget (the saturator will pop
// `k` CPs and those `k` are the lightest, by the heap order) and is
// pruned.  O(n_queue) selection + O(n_queue) sweep; the period gate in
// the caller amortizes this across thousands of selections.
//
// Rate and remaining budget are read on the work clock (atp_work_now_us:
// PROCESS CPU time by default) against a simulated budget: the wall span
// from the LRS epoch to the deadline, as if the prover owned one CPU for
// all of it.  Vampire's
// LRS::estimatedReachableCount (Saturation/LRS.cpp) takes the min of a
// time estimate on Timer::elapsedMilliseconds -- a steady_clock WALL
// reading (Lib/Timer.cpp) -- and, when an instruction limit is set, an
// estimate on a perf_event user-space instruction count against `sil`,
// the simulated instruction limit.  In the CASC UEQ schedule
// (CASC/Schedules.cpp) 58 of the 59 lrs strategies set sil and run
// under an `i=` instruction kill with the time limit the whole remaining
// run, so their binding estimate counts work done and is blind to load;
// the one without (the first champion, run for the whole time)
// estimates on the wall.  The lanes here take the load-blind form: the
// schedule's slices share one machine, and a wall estimate turns their
// contention into trajectory changes.  Work done is measured in CPU
// time, not instructions: an instruction estimate needs an instruction
// BUDGET per lane (Vampire tunes sil per strategy offline), and turning
// the slice's wall budget into one needs an instructions-per-second
// constant (Vampire's own conversion assumes 2 GHz,
// CASC/PortfolioMode.cpp) that mis-scales every horizon by the host's
// real rate; the counter is also Linux-only (perf_event, gated by
// kernel.perf_event_paranoid).  CPU time needs neither.  An unloaded
// slice, where CPU tracks wall, gets the wall estimate's horizon; a
// descheduled slice gets that same horizon at the same selection index
// instead of pruning harder.  The kill stays on the wall: the deadline
// guard reads the wall clock.
//
// Soundness: a discarded CP is not in the reach window; the proof, if it
// exists in budget, does not depend on it (same "incomplete in principle,
// complete in budget" tradeoff Vampire ships).  When the deadline has
// already passed, or the simulated budget is spent, leave the horizon
// untouched -- the next deadline check in atp_norm_deadline_fired will
// trip ATP_TIMEOUT.
static void atp_lrs_recompute_horizon(AtpState *s) {
  if (!s->use_lrs || s->wall_deadline_us == 0u || s->n_cps == 0u) {
    return;
  }
  u64 wall = atp_now_us();
  u64 now  = atp_work_now_us();
  if (wall == 0u || now == 0u) {
    return;
  }
  if (s->lrs_start_us == 0u) {
    // First call: latch the epoch on both clocks, then warm up.
    s->lrs_start_us      = now;
    s->lrs_start_wall_us = wall;
    return;
  }
  if (wall >= s->wall_deadline_us || now <= s->lrs_start_us ||
      s->wall_deadline_us <= s->lrs_start_wall_us) {
    return;
  }
  if (s->cp_select_count < s->lrs_warmup_selections) {
    return;
  }
  u64 budget  = s->wall_deadline_us - s->lrs_start_wall_us;
  u64 elapsed = now - s->lrs_start_us;
  if (elapsed >= budget) {
    return;
  }
  u64 remaining = budget - elapsed;
  // predicted_remaining = remaining * (selections_done / elapsed)
  // computed as (sel * remaining) / elapsed to keep the divide last.
  u64 predicted = ((u64)s->cp_select_count * remaining) / elapsed;
  if (predicted == 0u) predicted = 1u;
  // If the predicted-remaining reach window already covers the whole
  // queue, no CP is unreachable yet -- clear the horizon (no prune).
  if (predicted >= (u64)s->n_cps) {
    s->lrs_horizon = 0u;
    s->lrs_last_recompute_at = s->cp_select_count;
    s->n_lrs_recomputes++;
    atp_lrs_trace(s, elapsed, remaining, predicted, s->n_cps);
    return;
  }
  // Find the k-th smallest cp_pri.  k == predicted (1-indexed) -- the
  // saturator will pop `predicted` more CPs and those are the lightest
  // `predicted` in the queue under the heap order.  Quickselect over a
  // scratch copy of cp_pri keeps it O(n_queue) average without touching
  // the live heap.
  u32 n = s->n_cps;
  // Vampire LRS (--lrs-age): the age picks reach `ka` of the predicted
  // selections, the weight picks the rest; a CP survives when EITHER
  // queue reaches it (AWPassiveClauseContainer::exceedsAllLimits).
  u8  two = (u8)(s->use_lrs_age && s->fifo_modulo > 1u);
  u64 kw = predicted;
  s->lrs_age_horizon = UINT64_MAX;
  if (two) {
    u64 ka = (predicted + s->fifo_modulo - 1u) / s->fifo_modulo;
    kw = (predicted > ka) ? predicted - ka : 1u;
    u64 *ak = (u64 *)malloc((size_t)n * sizeof(u64));
    if (ak == NULL) return;     // out of memory: skip the recompute (sound)
    for (u32 i = 0; i < n; i++) ak[i] = atp_lrs_age_key(s, i, s->cp_heap[i].row);
    s->lrs_age_horizon = atp_u64_select(ak, n, (u32)(ka - 1u));
    free(ak);
  }
  u32 *scratch = (u32 *)malloc((size_t)n * sizeof(u32));
  if (scratch == NULL) return;  // out of memory: skip the recompute (sound)
  // node.pri == cp_pri[node.row] (refresh invariant) -- the packed
  // read spares the dependent payload load per entry.
  for (u32 i = 0; i < n; i++) scratch[i] = s->cp_heap[i].pri;
  u32 k = (u32)kw - 1u;         // 0-indexed k-th smallest
  // Hoare partition / quickselect loop.
  u32 lo = 0, hi = n - 1u;
  while (lo < hi) {
    u32 pivot = scratch[(lo + hi) >> 1];
    u32 i = lo, j = hi;
    for (;;) {
      while (scratch[i] < pivot) i++;
      while (scratch[j] > pivot) j--;
      if (i >= j) break;
      u32 t = scratch[i]; scratch[i] = scratch[j]; scratch[j] = t;
      i++; if (j == 0u) break; j--;
    }
    if (j < k) lo = j + 1u;
    else       hi = j;
  }
  u32 horizon = scratch[k];
  free(scratch);
  s->lrs_horizon = horizon;
  s->lrs_last_recompute_at = s->cp_select_count;
  s->n_lrs_recomputes++;
  // Prune the queue of CPs above the horizon.  Walk from the tail and
  // backfill from the live last slot; defer sifting until the end so the
  // prune is O(n_queue) instead of O(n_queue * log n_queue).  After
  // pruning, Floyd-build the remaining slots into a heap in one pass.
  u32 cps0 = s->n_cps;
  if (horizon == UINT32_MAX) {        // sentinel: never prune everything
    atp_lrs_trace(s, elapsed, remaining, predicted, cps0);
    return;
  }
  u32 i = s->n_cps;
  while (i > 0u) {
    i--;
    // i is a heap POSITION; the CP's payload row is vv.  The pri test
    // reads the packed cache (== cp_pri[vv], refresh invariant).
    u32 vv = s->cp_heap[i].row;
    if (s->cp_heap[i].pri <= horizon) continue;
    if (two && atp_lrs_age_key(s, i, vv) <= s->lrs_age_horizon) continue;
    atp_hx_del(s, s->cp_seq[vv], ATP_HX_DEL_LRS);
    // Vampire LRS discards the clause from passive, which also takes it
    // out of the Otter simplification set (Otter::onPassiveRemoved).
    if (s->use_otter) {
      atp_otter_popkill(s, s->cp_seq[vv], ATP_HX_OT_RETIRE_LRS);
    }
    free(s->cp_packed[vv]);
    s->cp_packed[vv] = NULL;
    atp_cp_stash_clear(s, vv);
    atp_cp_implicit_clear(s, vv);
#ifdef THVM_ATPFT_CPQ
    atp_cp_ft_clear(s, vv);
#endif
#ifdef ATP_FV_INDEX
    // No-op for deferred slots: their seq was never inserted.
    atp_fv_index_remove(s->fv_index, s->cp_seq[vv]);
#endif
    // Heap-entry backfill from the last position, then row densify
    // (move the last row into the vacated row vv).  The heap shrink
    // MUST precede the cp_pos[last-row] read: the last row may be the
    // very CP that sat at the popped last position.
    u32 last = s->n_cps - 1u;
    if (i != last) {
      // Whole-node move: the last position's key caches ride along
      // with their row (keys untouched -- invariant preserved).
      AtpCpHeapNode moved = s->cp_heap[last];
      s->cp_heap[i] = moved;
      s->cp_pos[moved.row] = i;
    }
    s->n_cps--;
    u32 lr = s->n_cps;             // last (to-densify) payload row
    if (vv != lr) {
      atp_cp_row_move(s, /*dst=*/vv, /*src=*/lr);
      u32 p = s->cp_pos[lr];
      // Row move: keys copied verbatim to row vv -- rewire only .row.
      s->cp_heap[p].row = vv;
      s->cp_pos[vv] = p;
    }
    s->n_cps_dropped_lrs++;
    // The backfilled element at slot i might itself be above horizon;
    // re-check it on the next iteration (compensate the i-- at the
    // top of the loop).  Bound by i < s->n_cps to skip the no-op
    // tail-drop case.
    if (i < s->n_cps) i++;
  }
  // Re-establish the heap order via Floyd build-heap: O(n_queue) sift-
  // downs over internal nodes, last to first.
  if (s->n_cps > 1u) {
    if (atp_kdheap_on()) atp_kdh_build(s);
    else for (u32 k = s->n_cps / 2u; k > 0u; ) {
      k--;
      atp_cp_sift_down(s, k);
    }
  }
  atp_lrs_trace(s, elapsed, remaining, predicted, cps0);
}

// Deferred-CP (`implicit_pair`) materialization at selection -- the WM
// TPR_TP2ParIntermed analog (KPVerwaltung.c:975, the read-back before
// NF_Normalform2).  An implicit slot has no packed byte string; its raw
// unified pair lives in the TRACE_CP entry at `cp_trace[slot_idx]` as
// children 2/3 (atp_trace_push_cp), so the pop reads them straight off
// the trace -- no re-unification (WM re-unifies via U1_KPRekonstruieren
// only because it has no always-on trace), no copy, no malloc.
//
// Liveness + aliasing: the trace array is a GC root set (thvm_atp_gc_-
// collect roots every entry and Cheney forwarding preserves sharing),
// so the returned Terms are alive and current across collections; the
// per-step heap reset can never pop them (the entry predates the step's
// checkpoint).  The pop path treats the popped pair as read-only --
// atp_rewrite_normalize / _record build fresh terms and kbo_eq just
// walks -- and the record_norm_steps path already feeds these exact
// trace children into the same chain, so handing out shared trace
// terms is the established discipline, not a new one.
//
// Form: children 2/3 are the RAW overlap (pre queue-reduction), the
// same form the lazy-push flow queues; the unconditional pop-time
// normalize takes it to a normal form just the same.  The descriptor's
// cached weight/priority were computed on the push-time reduced form,
// which only ordered the heap -- ordering is consumed by the time the
// slot is popped.
//
// Returns 1 always: the push side guarantees a live TRACE_CP entry for
// every implicit slot (trace-capped CPs stay eager), so a miss here is
// a broken invariant, not a recoverable state.  Dead-parent (orphan)
// slots are still materializable -- the trace entry outlives its
// parents -- and are discarded by the orphan check in the caller.
static int atp_cp_implicit_materialize(const AtpState *s, u32 slot_idx,
                                        Term *out_lhs, Term *out_rhs) {
  u32 t = s->cp_trace[slot_idx];
  if (t == ATP_TRACE_NONE || t >= s->n_trace) {
    thvm_fatal("atp_cp_implicit_materialize: implicit slot has no live "
               "trace entry (push-side invariant broken)");
  }
  Term te = s->trace[t];
  if (term_tag(te) != TAG_CTR || term_ext(te) != TRACE_CP
      || term_ctr_n(te) < 4u) {
    thvm_fatal("atp_cp_implicit_materialize: trace entry is not a "
               "TRACE_CP (push-side invariant broken)");
  }
  if (!atp_trace_eq_load((AtpState *)s, t, out_lhs, out_rhs)) {
    *out_lhs = term_ctr_at(te, 2);
    *out_rhs = term_ctr_at(te, 3);
  }
  return 1;
}

// Slot-aware CP read shared by every surface that walks the live queue
// (selection plus the read-only diagnostics thvm_atp_cp_get /
// thvm_atp_queued_cps / thvm_atp_cp_size_stats / thvm_atp_peek_top_k):
// a packed slot unpacks fresh transient heap Terms; an implicit slot
// returns the trace-resident raw pair zero-copy.  Callers treat the
// pair as read-only either way.
static void atp_cp_slot_read(const AtpState *s, u32 i,
                             Term *lhs, Term *rhs) {
  if (atp_cp_slot_implicit(s, i)) {
    if (s->use_raw_desc != 0u) {
      // Raw-desc slot outside the pop path (peeks / dumps / sweeps):
      // materialize transiently, unpack the identical Terms an eager
      // slot would yield, release the bytes.  (The pop path converts
      // the slot in place instead -- atp_cp_rdesc_pop_fix -- so the
      // downstream free/mixmost consumers run the eager code.)
      u32 len = 0u, nodes = 0u, lhs_nodes = 0u;
      u8 *bytes = atp_rdesc_materialize((AtpState *)s, &s->cp_implicit[i],
                                        &len, &nodes, &lhs_nodes);
      acp_unpack(bytes, lhs, rhs);
      free(bytes);
    } else {
      atp_cp_implicit_materialize(s, i, lhs, rhs);
    }
  } else {
#ifdef THVM_ATPFT_CPQ
    // Byte-queue retirement: a packed slot sources its term pair from
    // the Arena-A FT image (cp_packed_ft), not the legacy byte string.
    // ft_to_term rebuilds the identical heap Terms acp_unpack would --
    // test_ft_cpq T1 pins the round-trip byte-for-byte on every slot.
    AtpFtCell *fl, *fr;
    atp_cp_ft_slot_get(s, i, &fl, &fr);
    *lhs = ft_to_term(fl);
    *rhs = ft_to_term(fr);
#else
    acp_unpack(s->cp_packed[i], lhs, rhs);
#endif
  }
}

fn u8 thvm_atp_select_cp(AtpState *s, Term *lhs_out, Term *rhs_out) {
  if (s == NULL) return 0;
  // Auto-MaxWeight: if the active queue is empty but CPs are deferred
  // on the overflow stash, force-drain the lightest back in -- the
  // search continues on the deferred CPs (raising the bound monotone),
  // so no proof CP is lost.  This is what makes the bound complete.
  if (s->n_cps == 0u && s->n_cp_stash > 0u && s->auto_max_cp_weight_base > 0u) {
    atp_auto_maxw_drain(s, 1u);
  }
  if (s->n_cps == 0) return 0;
  // LRS horizon: latch the start clock on the first call; recompute the
  // horizon + prune at most every lrs_recompute_period selections.
  // With use_lrs off, both branches predict false on the first test so
  // the engine is byte-identical to the historical path.
  if (s->use_lrs && s->wall_deadline_us != 0u) {
    if (s->lrs_start_us == 0u
        || (s->cp_select_count - s->lrs_last_recompute_at)
             >= (s->lrs_recompute_period ? s->lrs_recompute_period : 128u)) {
      atp_lrs_recompute_horizon(s);
      if (s->n_cps == 0u && s->n_cp_stash > 0u
          && s->auto_max_cp_weight_base > 0u) {
        atp_auto_maxw_drain(s, 1u);
      }
      if (s->n_cps == 0u) return 0;
    }
  }

  // WM selectNonOrphan (KPVerwaltung.c:535): loop deleteMin until a
  // non-orphan CP surfaces.  An orphan (a parent rule was interreduced
  // away) is extracted and discarded FOR FREE -- the simplified
  // equation that replaced the dead rule regenerates its contribution,
  // so dropping it preserves completeness.  With use_orphan_murder off
  // the body runs exactly once (the orphan test is a cheap predictable
  // branch) and the engine is byte-identical.
  for (;;) {
  // Orphan-murder above can drain the queue mid-loop (n_cps-- after each
  // discard); the upstream guard only fires before the for(;;) starts, so
  // the inner re-entry needs its own n_cps==0 check.  Without this the
  // FIFO/random branches would compute j over an empty array and the
  // subsequent acp_unpack would dereference a freed slot.
  if (s->n_cps == 0u) return 0;
  // CPdimension: FIFO pick on the last THRESHOLD of every MODULO
  // selections, weight pick (heap root) otherwise.
  u32 j = 0;
  u8  hx_pick = ATP_HX_PICK_WEIGHT;   // the history export's 'S' dimension
  // WM 2-D KD-heap selection (opt-in THVM_ATP_KDHEAP): the queue IS a faithful
  // KDHeap.c port, so selection is KDH_deleteMin(d) with d = CPdimension() =
  // (n_rules % 51 < 1) ? 1 : 0 (itl(mi) 1:50).  find_min(1,d,d) locates the
  // node: d=0 -> root (weight); d=1 -> min-w2 among the top 2 levels (the
  // KD-heap's age dimension, which surfaces WM's specific old CP).
  // WM-faithful itl(mi) interleave (opt-in, THVM_ATP_WM_ITL / THVM_ATP_KDHEAP).
  // Waldmeister StdS uses itl(mi) = Fifo:Heuristik 1:50 (YFiles.c Schrittweiten
  // [mi]), so moduloCP=51, thresholdCP=1, CPdimension = (AnzAktivierterRE % 51
  // < 1) ? d1 : d0 (KPVerwaltung.c:848,1484): AGE dim when the activated-RULE
  // count is a multiple of 51, weight otherwise.  KDHEAP is the faithful 2-D
  // KD-heap port (KDH_deleteMin(d), find_min top d+1 levels); WM_ITL is the
  // legacy binary-heap scan approximation.
  static int kdh = -1;
  if (kdh < 0) kdh = atp_kdheap_on();
  static int wm_itl = -1;
  if (wm_itl < 0) wm_itl = getenv("THVM_ATP_WM_ITL") ? 1 : 0;
  if (kdh) {
    // CPdimension (KPVerwaltung.c:848): d = AnzAktivierterRE % moduloCP <
    // thresholdCP.  WM's AnzAktivierterRE is the CUMULATIVE activation
    // counter (KPV_IncAktivierterRE, monotonic ++), NOT the current-alive
    // rule count -- so gate on s->n_activated_re, not s->n_rules (which
    // interreduction decrements, drifting the FIFO cadence off WM's).
    unsigned d = (s->n_activated_re % 51u < 1u) ? 1u : 0u;
    u32 mn = 1u; unsigned ml = 0u;
    atp_kdh_find_min(s, 1u, d, d, &mn, &ml);
    j = mn - 1u;
    g_kdh_last_d = d;
    if (d == 1u) {
      s->n_cps_w2_picks++;
      hx_pick = ATP_HX_PICK_KDH;
    }
  } else if (wm_itl && (s->n_rules % 51u) == 0u) {
    // WM KDH_deleteMin(d=1) searches only the TOP d+1 = 2 heap levels
    // (find_min, KDHeap.c:641 "in den obersten d+1 Ebenen suchen"), i.e.
    // the min-w2 (oldest) among the ~3 lightest CPs -- the binary-heap root
    // (slot 0) + its two children (slots 1,2) -- NOT the global oldest.
    static long topcap_env = -1;
    if (topcap_env < 0) {
      const char *e = getenv("THVM_ATP_WM_ITL_TOP");
      topcap_env = (e != NULL) ? (long)strtoul(e, NULL, 10) : 0;
    }
    u32 topcap = (u32)topcap_env;
    u32 top = (topcap == 0u || topcap > s->n_cps) ? s->n_cps : topcap;
    u32 best = 0;
    for (u32 i = 1; i < top; i++) {
      // node.seq == cp_seq[node.row] (refresh invariant).
      if (s->cp_heap[i].seq < s->cp_heap[best].seq) best = i;
    }
    j = best;
    s->n_cps_w2_picks++;
    hx_pick = ATP_HX_PICK_KDH;
  } else if (s->w2_modulo > 0u &&
      (s->cp_select_count % s->w2_modulo) == 0u) {
    // K-D Heap secondary dimension (WM CPdimension d=1): every
    // w2_modulo-th selection picks the min-cp_pri2 entry instead of
    // the primary heap root.  O(n_cps) scan over heap positions; the
    // period gates cost.  Surfaces structurally simple CPs buried
    // under primary weight.
    u32 best = 0;
    // cp_pri2 is NOT cached in the node (mode-gated key) -- row loads;
    // seq comes from the node cache (== cp_seq[row], invariant).
    u32 bp2 = s->cp_pri2[s->cp_heap[0].row], bsq = s->cp_heap[0].seq;
    for (u32 i = 1; i < s->n_cps; i++) {
      u32 vv = s->cp_heap[i].row;
      if (s->cp_pri2[vv] < bp2
          || (s->cp_pri2[vv] == bp2 && s->cp_heap[i].seq < bsq)) {
        best = i;
        bp2 = s->cp_pri2[vv];
        bsq = s->cp_heap[i].seq;
      }
    }
    j = best;
    s->n_cps_w2_picks++;
    hx_pick = ATP_HX_PICK_W2;
  } else if (s->use_goal_interleave > 0u &&
      (s->cp_select_count % s->use_goal_interleave) == 0u) {
    // Goal-directed pick: the most goal-relevant queued CP (min
    // cp_goal).  E-style ratio -- the other picks are weight-based,
    // building the system; this steers toward the goal.  Ties keep the
    // LOWEST heap position (strict <), so the scan must walk positions.
    u32 best = 0;
    for (u32 i = 1; i < s->n_cps; i++) {
      // cp_goal is NOT cached in the node (mode-gated key) -- row loads.
      if (s->cp_goal[s->cp_heap[i].row] < s->cp_goal[s->cp_heap[best].row]) {
        best = i;
      }
    }
    j = best;
    hx_pick = ATP_HX_PICK_GOAL;
  } else if (s->random_modulo > 0u &&
             (s->cp_select_count % s->random_modulo) == 0u) {
    // Vampire-style random pick: advance xorshift64 once, take the
    // result mod n_cps.  Deterministic given the seed; trajectory
    // differs from heap-min so the portfolio can sample paths the
    // weight-greedy walk misses (Vampire's distinctive McCune win).
    u64 x = s->rng_state ? s->rng_state : 0x9E3779B97F4A7C15ull;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    s->rng_state = x;
    j = (u32)(x % (u64)s->n_cps);
    hx_pick = ATP_HX_PICK_RANDOM;
  } else if (atp_cp_fifo_dimension(s)) {
    hx_pick = ATP_HX_PICK_AGE;
    // FIFO dimension: the oldest queued LIVE CP is the lowest cp_seq
    // among entries whose parents still live.  Skipping corpses inside
    // this one O(n_cps) scan replaces the old drain (pop + heap-repair
    // + slot-compaction per corpse: the OrAssociativity pick-2925
    // crossing popped ~765k dead entries over minutes) with a single
    // pass; the selected CP is identical -- the drain's sole outcome
    // was the first alive oldest.  Corpses stay queued and are
    // discarded lazily when the weight dimension reaches them, exactly
    // as before for non-FIFO picks.  Only the CHEAP no-unpack death
    // tests run here (trace orphan / uid).
    // Monotone dead-prefix floor (see AtpState.fifo_floor_seq): every
    // death test here is permanent (set-only death marks), so every
    // entry below the floor was proven dead by an earlier scan or has
    // been extracted.
    if (s->use_age_depth && s->cp_depth != NULL) {
      // Vampire AgeQueue::lessThan: age = derivation depth (max premise
      // age + 1, Kernel/Inference.cpp), then weight, then clause number.
      u32 best = 0xffffffffu, bd = 0u, bp = 0u, bq = 0u;
      for (u32 i = 0; i < s->n_cps; i++) {
        u32 vv = s->cp_heap[i].row;
        u32 d = s->cp_depth[vv], p = s->cp_heap[i].pri, q = s->cp_heap[i].seq;
        if (best != 0xffffffffu &&
            (d > bd || (d == bd && (p > bp || (p == bp && q >= bq))))) {
          continue;
        }
        if (s->use_orphan_murder) {
          if (atp_cp_is_orphan(s, s->cp_trace[vv])) continue;
          if (atp_cp_uid_orphan(s, vv)) continue;
        }
        best = i; bd = d; bp = p; bq = q;
      }
      j = (best != 0xffffffffu) ? best : 0u;
    } else if (s->s2a_coeff > 0u) {
      // s2a (Vampire sine_to_age): the age dimension orders by effective
      // age = cp_seq + s2a_coeff*cp_sine, so a goal-distant CP (high SInE
      // level) is made younger and the goal-NEAR oldest wins.  The dead-
      // prefix floor is bypassed: effective age is not monotone in the
      // scan order, so no prefix is provably dead.  O(n_cps) like the raw
      // scan below (the floor only ever let it skip a prefix).
      u64 best_eff = UINT64_MAX;
      u32 best = 0xffffffffu;
      for (u32 i = 0; i < s->n_cps; i++) {
        u32 vv = s->cp_heap[i].row;
        u64 eff = (u64)s->cp_heap[i].seq
                + (u64)s->s2a_coeff * (u64)s->cp_sine[vv];
        if (best != 0xffffffffu && eff >= best_eff) continue;
        if (s->use_orphan_murder) {
          if (atp_cp_is_orphan(s, s->cp_trace[vv])) continue;
          if (atp_cp_uid_orphan(s, vv)) continue;
        }
        best = i;
        best_eff = eff;
      }
      j = (best != 0xffffffffu) ? best : 0u;
    } else {
    const u32 floor_seq = s->fifo_floor_seq;
    u32 best = 0xffffffffu;
    u32 best_sq = 0xffffffffu;
    for (u32 i = 0; i < s->n_cps; i++) {
      // seq from the packed node (== cp_seq[row], refresh invariant):
      // the age filters run on the sequential node walk alone; only an
      // improving candidate pays the payload loads (trace / uid).
      u32 sq = s->cp_heap[i].seq;
      if (sq < floor_seq) continue;                 // proven dead prefix
      if (best != 0xffffffffu && sq >= best_sq) continue;
      if (s->use_orphan_murder) {
        u32 vv = s->cp_heap[i].row;
        if (atp_cp_is_orphan(s, s->cp_trace[vv])) continue;
        if (atp_cp_uid_orphan(s, vv)) continue;
      }
      best = i;
      best_sq = sq;
    }
    j = (best != 0xffffffffu) ? best : 0u;
    // The scanned minimum is extracted below on every path (selected,
    // or discarded as orphan and re-picked), and pushes
    // stamp monotonically growing seqs -- so everything at or below its
    // seq is dead or gone from here on.
    if (best != 0xffffffffu) s->fifo_floor_seq = best_sq + 1u;
    }
  } else if (s->topk != NULL) {
    // Weight dimension under the opt-in top-K re-ranker (src/atp/topk.c):
    // the head's pick among the root's K best, 0 when it keeps the root.
    j = atp_topk_pick(s);
    hx_pick = ATP_HX_PICK_TOPK;
  }

  // The chosen CP: j is its heap POSITION, v its stationary payload
  // row.  Every payload read/write below routes through v; j survives
  // only as the position (printed in the trace lines + heap repair).
  const u32 v = s->cp_heap[j].row;

  // Lazy orphan check.  If the chosen CP descends from an interreduced-
  // away rule, extract+discard it without counting a selection and
  // re-pick.  WM IncAnzKPEntfernt ticks per discard (KPVerwaltung.c:539).
  int orphan = s->use_orphan_murder && atp_cp_is_orphan(s, s->cp_trace[v]);
  // WM Waisenmord uid layer (see AtpState.r_uid): parent-identity death
  // test that works past the proof-trace soft cap AND for post-cap rules
  // whose r_trace is ATP_TRACE_NONE (where both tests above are blind) --
  // the WolframAxioms DoubleNegation firstdiv-796 extra pop.
  if (!orphan && s->use_orphan_murder && atp_cp_uid_orphan(s, v)) {
    orphan = 1;
  }
  // Gated forensic print for the silent at-pop orphan discard (the only
  // queue-removal path with no CPSEL line): seq/pri identify the victim
  // against CPFORM records.  Diagnostic only; zero cost unset.
  {
    static int otr = -1;
    if (otr < 0) otr = getenv("THVM_ATP_ORPHAN_TRACE") != NULL ? 1 : 0;
    if (otr && orphan) {
      // Decompose the verdict: which parent trace id is marked dead, and
      // whether the uid layer (not the trace layer) fired instead.
      u32 pa = 0xffffffffu, pb = 0xffffffffu;
      u8 pa_dead = 0u, pb_dead = 0u;
      u32 ct = s->cp_trace[v];
      if (ct != ATP_TRACE_NONE && ct < s->n_trace) {
        Term te = s->trace[ct];
        if (term_tag(te) == TAG_CTR && term_ext(te) == TRACE_CP) {
          pa = (u32)term_val(term_ctr_at(te, 0));
          pb = (u32)term_val(term_ctr_at(te, 1));
          pa_dead = (u8)atp_trace_is_dead(s, pa);
          pb_dead = (u8)atp_trace_is_dead(s, pb);
        }
      }
      fprintf(stderr,
              "ORPHDROP seq=%u pri=%u trace=%u pa=%u(dead=%u) pb=%u(dead=%u) "
              "uid_orphan=%u\n",
              s->cp_seq[v], s->cp_pri[v], s->cp_trace[v],
              pa, pa_dead, pb, pb_dead,
              (u8)atp_cp_uid_orphan(s, v));
    }
  }
  if (!orphan) s->cp_select_count++;
  else atp_hx_del(s, s->cp_seq[v], ATP_HX_DEL_ORPHAN_POP);

  // Unpack the chosen CP from its byte string into two fresh heap
  // Terms for the caller to normalize.  An implicit slot has no byte
  // string -- it materializes the raw pair off its TRACE_CP entry
  // instead (zero-copy; see atp_cp_implicit_materialize for the
  // liveness/aliasing argument).  The orphan check above already ran
  // on cp_trace[v], which the push side sets for implicit slots too,
  // so a dead-parent implicit CP is discarded below exactly like an
  // eager one (WM selectNonOrphan covers the implicit passive set).
  // Orphan pops are discarded without touching the terms (the loop
  // re-picks), so skip the unpack: a FIFO pop draining a long dead
  // prefix would otherwise allocate two fresh Terms per corpse and
  // exhaust the from-space mid-drain (OrAssociativity pick 2925
  // drains ~194k dead entries).  WM's selectNonOrphan likewise tests
  // parent liveness without building the termpair.
  if (orphan) {
    *lhs_out = 0;
    *rhs_out = 0;
  } else {
    // Raw-desc lane: convert a descriptor slot to a plain packed slot
    // (or run the cert-mode memcmp) BEFORE the unpack, so the whole
    // downstream pop path -- unpack, mixmost byte->FT stash, the
    // free-on-pop -- is the eager code path bit-for-bit.  Orphan pops
    // never materialize (the whole point of the descriptor).
    atp_cp_rdesc_pop_fix(s, v);
    // --otter-raw: the popped CP is its raw overlap; the pop normalize
    // derives its current form from there (the start its passive
    // demodulator was normalized from).
    const u8 *raw_b = (s->use_otter_raw && !s->record_norm_steps &&
                       s->cp_raw != NULL) ? s->cp_raw[v] : NULL;
    if (raw_b != NULL) acp_unpack(raw_b, lhs_out, rhs_out);
    else               atp_cp_slot_read(s, v, lhs_out, rhs_out);
    atp_hx_select(s, v, hx_pick);
    atp_topk_on_select(s);
    // ifeq guard-DECIDING telemetry: did the CP the main loop actually
    // picked decide a live guard (see atp_ifeq_decide_hit)?  Counts
    // distinct SELECTIONS, complementing g_ifeq_decide_n_hit's scoring-
    // event count -- see the bench stats print.
    if (s->use_ifeq_decide && atp_ifeq_decide_hit(s, *lhs_out, *rhs_out)) {
      g_ifeq_decide_n_popped++;
    }
#ifdef THVM_ATPFT_RULES
    // Byte->FT pop stash: decode the selected packed CP's byte string
    // straight into FT spans for the mixmost pop treatment (which
    // otherwise re-encodes the just-unpacked Terms via ft_from_term).
    // Only for eager packed slots on the mixmost/non-record path; the
    // consumer falls back to ft_from_term when the stash is empty
    // (implicit slot, non-mixmost preset, record_norm_steps replay).
    g_pop_ft_l = NULL;
    g_pop_ft_r = NULL;
    if (s->use_mixmost_nf && !s->record_norm_steps &&
        !atp_cp_slot_implicit(s, v) && s->cp_packed[v] != NULL) {
      AtpFt *pa = (AtpFt *)s->ft_arena_ptr;
      const u8 *pp = (raw_b != NULL) ? raw_b : s->cp_packed[v];
      g_pop_ft_l = acp_unpack_term_ft(pa, &pp, 0);
      g_pop_ft_r = acp_unpack_term_ft(pa, &pp, 0);
    }
#endif
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Simpler-join pop-peak: decode the slot's stashed peak (same
    // dense var space as the slot's packed bytes the pop treatment
    // consumes) for the pop-time allSteps checkOrder/step3 legs.
    // Record-mode replay keeps the peakless degrade -- its pair comes
    // from the trace raw pack, a different var space.  A leftover from
    // a step that exited before the sj site is dropped first.
    atp_pop_sj_peak_drop(s);
    if (s->use_simpler_join != 0u && !s->record_norm_steps &&
        raw_b == NULL &&   // the peak shares the queued pair's var space
        s->cp_peak != NULL && s->cp_peak[v] != NULL &&
        s->cp_packed[v] != NULL) {
      const u8 *pk = s->cp_peak[v];
      g_pop_sj_peak = acp_unpack_term_ft((AtpFt *)s->ft_arena_ptr,
                                         &pk, 0);
      g_pop_sj_peak_owner = s->ft_arena_ptr;
    }
    // Env-gated stash coverage tap (THVM_ATP_POP_PEAK_DEBUG): one line
    // per non-orphan pop -- did the popped slot carry a peak?
    {
      static int ppd = -1;
      if (ppd < 0) ppd = atp_env_on("THVM_ATP_POP_PEAK_DEBUG");
      if (ppd) {
        fprintf(stderr, "[poppeak] pick=%u peak=%s arr=%s packed=%s\n",
                s->cp_select_count,
                (g_pop_sj_peak != NULL) ? "yes" : "no",
                (s->cp_peak != NULL) ? "live" : "null",
                (s->cp_packed[v] != NULL) ? "yes" : "no");
      }
    }
#endif
  }
  s->last_popped_trace = s->cp_trace[v];
  s->last_popped_kind  = s->cp_kind[v];
  s->last_popped_depth = (s->cp_depth != NULL) ? s->cp_depth[v] : 0u;
  s->last_popped_gd    = (s->cp_gd != NULL) ? s->cp_gd[v] : 0u;
  s->last_popped_seq   = s->cp_seq[v];
  // Cleared per pop; the KBO_UN orient branch sets it iff the CP-formation
  // side swap physically fires, so the emission mirror never reads a stale
  // verdict from an earlier (e.g. orientable) selection.
  s->last_cp_wm_side_swapped = 0u;

  // Env-gated CP-selection trajectory dump for parity comparison vs
  // external provers (WaldmeisterProcess / VampireProcess).  Emits
  // one line per selected CP: pick number, queue index, sequence id,
  // priority, current rule count, then S-expr LHS/RHS.  Diff against
  // `wmcli -:l0 -P verbose` finds the algorithmic divergence point.
  // Env-gated CP-queue snapshot at a target pick (THVM_ATP_HEAPDUMP_AT):
  // dump every queued CP in the chosen CP's priority band with its seq +
  // terms, the thvm counterpart of Waldmeister WM_HEAPDUMP_AT.  Fires only
  // for the matching non-orphan pick; off (one cached probe) otherwise.
  if (!orphan) {
    // Pick-gated live-rule-set dump (THVM_ATP_DUMP_RULES_AT_PICK=N): the
    // rule-count-gated LIVERULE dump can't target a specific step; this one
    // prints the live set at a chosen selection.  Output-only.
    static int rd_at = -2;
    if (rd_at == -2) { const char *e = getenv("THVM_ATP_DUMP_RULES_AT_PICK"); rd_at = e ? atoi(e) : -1; }
    if (rd_at >= 0 && (u32)rd_at == s->cp_select_count) {
      for (u32 k = 0; k < s->n_rules; k++) {
        fprintf(stderr, "PICKRULE[%u] or=%u uid=%u lhs=", k, s->r_orient[k],
                s->r_uid ? s->r_uid[k] : 0u);
        atp_dbg_print_term(stderr, s->lhs[k]);
        fprintf(stderr, " rhs="); atp_dbg_print_term(stderr, s->rhs[k]);
        fputc('\n', stderr);
      }
    }
    static int hd_at = -2;
    static int hd_all = 0;
    if (hd_at == -2) {
      const char *e = getenv("THVM_ATP_HEAPDUMP_AT"); hd_at = e ? atoi(e) : -1;
      // THVM_ATP_HEAPDUMP_ALL: dump EVERY queued CP at the chosen pick,
      // not just the picked band -- localizes where a known-needed CP
      // (matched by seq or content) sits in the flood.  Output-only.
      hd_all = atp_env_on("THVM_ATP_HEAPDUMP_ALL");
    }
    if (hd_at >= 0 && (u32)hd_at == s->cp_select_count) {
      u32 band = s->cp_pri[v], n_band = 0;
      fprintf(stderr, "==THVMHEAP pick=%u band_pri=%u n_cps=%u==\n", s->cp_select_count, band, s->n_cps);
      for (u32 q = 0; q < s->n_cps; q++) {
        u32 qv = s->cp_heap[q].row;   // heap-position order, exactly as before
        if (!hd_all && s->cp_pri[qv] != band) continue;
        Term ql = 0, qr = 0;
        atp_cp_slot_read(s, qv, &ql, &qr);
        fprintf(stderr, "  HCP seq=%u pri=%u lhs=", s->cp_seq[qv], s->cp_pri[qv]);
        atp_dbg_print_term(stderr, ql);
        fprintf(stderr, " rhs=");
        atp_dbg_print_term(stderr, qr);
        fputc('\n', stderr);
        n_band++;
      }
      fprintf(stderr, "==THVMHEAPEND pick=%u n_band=%u==\n", s->cp_select_count, n_band);
    }
  }

  {
    static int cp_pick_trace = -1;
    if (cp_pick_trace < 0) cp_pick_trace = atp_env_on("THVM_ATP_CP_PICK_TRACE");
    if (cp_pick_trace && orphan) {
      // Discarded pop (orphan): the aligner skips these as
      // non-selections, so spare the term rendering -- a FIFO pop draining
      // a long dead prefix otherwise prints GBs and dominates the wall.
      // j stays the heap POSITION -- trace format byte-identical.
      fprintf(stderr, "CPSKIP pick=%u j=%u seq=%u pri=%u\n",
              s->cp_select_count, j, s->cp_seq[v], s->cp_pri[v]);
    } else if (cp_pick_trace) {
      fprintf(stderr, "CPSEL pick=%u j=%u seq=%u pri=%u rules=%u d=%u actre=%u dep=%u lhs=",
              s->cp_select_count, j, s->cp_seq[v], s->cp_pri[v], s->n_rules,
              g_kdh_last_d, s->n_activated_re,
              (s->cp_depth != NULL) ? s->cp_depth[v] : 0u);
      atp_dbg_print_term(stderr, *lhs_out);
      fputs(" rhs=", stderr);
      atp_dbg_print_term(stderr, *rhs_out);
      if (orphan && s->cp_trace[v] == ATP_TRACE_NONE) {
        // Uid-layer orphan past the proof-trace soft cap: the parents live
        // in the uid stamps, not the (absent) trace entry.  Skipped by the
        // aligner exactly like a genuine orphan (no counted selection).
        // (Tag + par_a field kept byte-identical to the historical print;
        // the cp_par_a side-array died with the wolf-dup detector, and in
        // the canonical config it was always absent here.)
        fprintf(stderr, " WOLFDUPORPHAN par_a=%u(%s) uid_a=%u(%s) uid_b=%u(%s)",
                ATP_TRACE_NONE, "live",
                (s->cp_uid_a != NULL) ? s->cp_uid_a[v] : 0u,
                (s->cp_uid_a != NULL && atp_uid_is_dead(s, s->cp_uid_a[v]))
                    ? "dead" : "live",
                (s->cp_uid_b != NULL) ? s->cp_uid_b[v] : 0u,
                (s->cp_uid_b != NULL && atp_uid_is_dead(s, s->cp_uid_b[v]))
                    ? "dead" : "live");
      } else if (orphan) {
        Term te = s->trace[s->cp_trace[v]];
        u32 pa = (u32)term_val(term_ctr_at(te, 0));
        u32 pb = (u32)term_val(term_ctr_at(te, 1));
        fprintf(stderr, " ORPHAN pa=%u(%s) pb=%u(%s)",
                pa, atp_trace_is_dead(s, pa) ? "dead" : "live",
                pb, atp_trace_is_dead(s, pb) ? "dead" : "live");
      }
      fputc('\n', stderr);
    }
  }
#ifdef ATP_FV_INDEX
  // 7d: the popped CP leaves the queue -- drop it from the index so a
  // later subsumption query never matches a stale, no-longer-queued
  // CP.  Mark the record dead BEFORE freeing the byte string it
  // borrows: a dead record is never dereferenced, so the borrow stays
  // sound.
  atp_fv_index_remove(s->fv_index, s->cp_seq[v]);
#endif
  free(s->cp_packed[v]);
  s->cp_packed[v] = NULL;
  atp_cp_stash_clear(s, v);  // stash bytes decoded above (or orphan drop)
  atp_cp_implicit_clear(s, v);
#ifdef THVM_ATPFT_CPQ
  // Stage 7: the popped row's FT spans return to Arena A's free list
  // BEFORE the row densify -- the row is about to be overwritten with
  // the last row's entry (still owned).  No-op when the entry was
  // never populated (NULL slots are idempotent on clear).
  atp_cp_ft_clear(s, v);
#endif
  // Extraction at heap position j + payload row v, two independent
  // backfills (the position-level one reproduces the historical
  // move-the-last-slot-into-j layout evolution byte-for-byte; the
  // row-level one keeps the payload rows dense in [0, n_cps)).  The
  // heap shrink MUST run before the cp_pos[last-row] read: the last
  // row may be the very CP that sat at the popped last position.
  s->n_cps--;
  u32 last_pos = s->n_cps;
  u8 need_repair = (j != last_pos);
  if (need_repair) {
    // Whole-node move: the key caches ride along with their row
    // (keys untouched -- invariant preserved).
    AtpCpHeapNode moved = s->cp_heap[last_pos];
    s->cp_heap[j] = moved;
    s->cp_pos[moved.row] = j;
  }
  u32 lr = s->n_cps;               // last (to-densify) payload row
  if (v != lr) {
    atp_cp_row_move(s, /*dst=*/v, /*src=*/lr);
    u32 p = s->cp_pos[lr];
    // Row move: keys copied verbatim to row v -- rewire only .row.
    s->cp_heap[p].row = v;
    s->cp_pos[v] = p;
  }
  if (need_repair) {
    if (atp_kdheap_on()) atp_kdh_delete_repair(s, j);
    else { atp_cp_sift_up(s, j); atp_cp_sift_down(s, j); }
  }

  if (orphan) {
    s->n_cps_dropped_orphan++;
    if (s->n_cps == 0u && s->n_cp_stash > 0u
        && s->auto_max_cp_weight_base > 0u) {
      atp_auto_maxw_drain(s, 1u);
    }
    if (s->n_cps == 0) return 0;  // queue drained to orphans only
    continue;                     // re-pick the next heap-min
  }
  // ENIGMA training-data hook: record the processed CP's feature
  // vector + trace id.  Gated -- with the flag off (the default) this
  // is a single predictable-branch test and the engine is byte-
  // identical to the untracked run.
  if (s->record_cp_features) {
    atp_cp_feat_record(s, *lhs_out, *rhs_out, s->last_popped_trace);
  }
  return 1;
  }  // end selectNonOrphan loop
}

// 7c': re-establish the CP-queue heap invariant over cp_packed[0..n_cps).
// The normal path keeps the queue a heap via atp_cp_heap_push, but a
// caller (chiefly tests) that populates cp_packed / n_cps directly
// (via thvm_atp_cp_set) must call this so cp_pri is filled and the
// array satisfies the heap order before select / peek.
//
// Split into two phases so the IR sweep can fold the per-CP priority
// recompute into its single survivor walk (it already holds the unpacked
// sides), and only run the FULL-array work (Floyd + FV-index rebuild)
// once at the end.  The public entry below stitches both back together
// for orphan-kill and direct cp_set callers.
static void atp_cp_rebuild_priorities(AtpState *s) {
  if (s == NULL || s->n_cps == 0) return;
  atp_ensure_cp_cap(s, s->n_cps);
  for (u32 i = 0; i < s->n_cps; i++) {
    // i is a heap POSITION; vv the CP's payload row.
    u32 vv = s->cp_heap[i].row;
    // Each packed CP unpacks two transient heap Terms below; on a deep
    // queue (~10^5 CPs) those overrun the from-space before the next
    // step-top GC.  Reclaim them in-loop: the unpacked Terms are consumed
    // within the iteration, so nothing live is carried across the boundary
    // (cp_packed bytes + rooted rule/trace arrays survive a Cheney pass).
    if (s->trace_pack && (HEAP_NEXT - gc_from_start()) > atp_gc_threshold_bytes()) {
      thvm_atp_gc_collect(s);
    }
    if (atp_cp_slot_implicit(s, vv)) {
      // Deferred slot: no packed bytes to unpack.  Reuse the push-time
      // cached priority (this is what the descriptor caches it for);
      // cp_goal / cp_pri2 keep their push-time values, which stay put
      // on the stationary row.
      s->cp_pri[vv] = s->cp_implicit[vv].priority;
      // cp_pri[vv] (re)written for a queued row: refresh the packed
      // node caches before the trailing Floyd compares against them.
      atp_cp_heap_set(s, i, vv);
      continue;
    }
    Term l = 0, r = 0;
#ifdef THVM_ATPFT_CPQ
    // Byte-queue retirement: source the term pair from the FT image
    // (packed slot, implicit already handled above).  Same terms
    // acp_unpack would rebuild -> same recomputed priority.
    { AtpFtCell *fl, *fr; atp_cp_ft_slot_get(s, vv, &fl, &fr);
      l = ft_to_term(fl); r = ft_to_term(fr); }
#else
    acp_unpack(s->cp_packed[vv], &l, &r);
#endif
    // cp_deprio stays POSITION-indexed (raw i): the historical sift /
    // backfill never moved it, so its byte at position i -- stamped by
    // whichever append last landed there -- is what the historical
    // rebuild read.  Preserves that quirk bit-exactly (GJ-keep configs).
    g_twee_dbg_ctx = 3u;
    u32 hx_old_pri = s->cp_pri[vv];
    s->cp_pri[vv] = s->cp_deprio[i] ? ATP_CP_PRI_NEVER
                                    : atp_cp_priority(s, l, r);
    g_twee_dbg_ctx = 0u;
    // spb=goal re-score from the row's stored derivation bit (the
    // commit_priorities discipline).
    if (s->cp_gd != NULL)
      s->cp_pri[vv] = atp_spb_scale(s->cp_pri[vv], s->cp_gd[vv]);
    s->cp_goal[vv] = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                      ? atp_goal_weight(s, l, r) : 0u;
    s->cp_sine[vv] = atp_cp_sine_level(s, l, r);
    s->cp_pri2[vv] = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                  : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, l, r, s->w2_mode)
                  :                        0u;
    atp_hx_reweight(s, vv, hx_old_pri);
    // cp_seq is NOT touched: insertion age is stamped once (heap push /
    // thvm_atp_cp_set) and survives every reweight, matching WM's
    // C_ReClassify which only recomputes w1 ("w2 wird nicht geaendert"
    // / w2 is not changed, CLAS/NewClassification.c:399-406).
    // cp_pri[vv] (re)written for a queued row: refresh the packed node
    // caches before the trailing Floyd compares against them.  (Also
    // heals the direct-build path: thvm_atp_cp_set leaves node.pri to
    // this rebuild, exactly like cp_pri itself.)
    atp_cp_heap_set(s, i, vv);
  }
}

static void atp_cp_floyd_only(AtpState *s) {
  if (s == NULL || s->n_cps == 0) return;
  // Floyd build-heap: sift down every internal node, last to first.
  if (atp_kdheap_on()) atp_kdh_build(s);
  else for (u32 i = s->n_cps / 2; i > 0; ) {
    i--;
    atp_cp_sift_down(s, i);
  }
#ifdef ATP_FV_INDEX
  // 7d: a sweep / compaction ahead of this reheapify may have dropped
  // CPs, so the incremental insert/remove path can no longer track the
  // set; rebuild the index wholesale from the live CP arrays (cp_seq,
  // the index's stable key, is preserved across the rebuild).
  atp_fv_index_rebuild(s);
#endif
}

// Per-survivor priority commit used by atp_cp_set_interreduce: at each
// w-commit site the IR sweep already has (l, r) in scope (either
// the pre-normalize (ol, orr) on the fast-path skip branches or the
// post-normalize (l, r) on the reweight branch).  Inlined here so the
// post-loop reheapify only has to do the FULL-array work (Floyd + FV).
// Soundness: the three weight functions are pure in (s, l, r) + state
// fields immutable during the IR sweep (cp_weight_mode, w2_*, gnn_coop,
// use_*, goal_*, sym_level[], conj_sym_mask, s->lhs/rhs); evaluating
// inside the loop yields bit-identical values to evaluating post-loop
// in atp_cp_rebuild_priorities.  `w` is the survivor's post-compaction
// heap POSITION (the raw cp_deprio index, matching the rebuild's
// position-quirk); `slot` its stationary payload row.  Payload-only
// writes: every caller stamps the packed node caches AFTERWARDS via
// atp_cp_heap_set(s, w, slot) (the IR sweep's w-commit sites), so the
// refresh invariant is re-established before the post-loop Floyd.
static inline void atp_cp_commit_priorities(AtpState *s, u32 w, u32 slot,
                                            Term l, Term r) {
  // Re-score with THIS CP's formation depth (Twee getDepth), not the last
  // push's stale g_cp_formation_depth -- the ATP_CP_WEIGHT_TWEE score reads
  // the global, so a dynamic re-score (cp_set_interreduce) must restore it
  // from the stored cp_depth or the depth term would be wrong.  A no-op for
  // the WM preset (mode 4 ignores the global).
  if (s->cp_depth != NULL) g_cp_formation_depth = s->cp_depth[slot];
  g_twee_dbg_ctx = 3u;
  u32 hx_old_pri = s->cp_pri[slot];
  s->cp_pri[slot]  = s->cp_deprio[w] ? ATP_CP_PRI_NEVER
                                     : atp_cp_priority(s, l, r);
  // spb=goal re-score: re-apply the derivation-bit scale from THIS
  // CP's stored bit (the g_cp_formation_depth restore discipline).
  if (s->cp_gd != NULL)
    s->cp_pri[slot] = atp_spb_scale(s->cp_pri[slot], s->cp_gd[slot]);
  s->cp_goal[slot] = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                    ? atp_goal_weight(s, l, r) : 0u;
  s->cp_sine[slot] = atp_cp_sine_level(s, l, r);
  s->cp_pri2[slot] = s->gnn_coop          ? ATP_GNN_COOP_NEUTRAL_PRI
                : (s->w2_modulo > 0u)  ? atp_cp_weight_base(s, l, r, s->w2_mode)
                :                        0u;
  g_twee_dbg_ctx = 0u;
  atp_hx_reweight(s, slot, hx_old_pri);
}

fn void thvm_atp_cp_reheapify(AtpState *s) {
  atp_cp_rebuild_priorities(s);
  atp_cp_floyd_only(s);
}

