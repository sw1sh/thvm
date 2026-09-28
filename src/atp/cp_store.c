// === Deferred-CP (`implicit_pair`) per-slot tag-bit bookkeeping =====
//
// The invariant is `cp_is_implicit[i] set <=> cp_implicit[i] live AND
// cp_packed[i] == NULL` (thvm.h AtpCpImplicit header).  Every slot
// drop / move / swap site below maintains it through these helpers;
// all are NULL-safe so the flag-OFF engine (both arrays NULL) pays a
// single predictable branch.

static inline u8 atp_cp_slot_implicit(const AtpState *s, u32 i) {
  return s->cp_is_implicit != NULL
      && ((s->cp_is_implicit[i >> 3] >> (i & 7u)) & 1u);
}

static inline void atp_cp_implicit_set(AtpState *s, u32 i) {
  s->cp_is_implicit[i >> 3] |= (u8)(1u << (i & 7u));
}

static inline void atp_cp_implicit_clear(AtpState *s, u32 i) {
  if (s->cp_is_implicit != NULL) {
    s->cp_is_implicit[i >> 3] &= (u8)~(1u << (i & 7u));
  }
}

// Move the descriptor + tag bit from slot `src` into slot `dst`,
// vacating `src` (compaction / backfill).  No-op when dst == src.
static inline void atp_cp_implicit_move(AtpState *s, u32 dst, u32 src) {
  if (s->cp_is_implicit == NULL || dst == src) return;
  if (s->cp_implicit != NULL) s->cp_implicit[dst] = s->cp_implicit[src];
  if (atp_cp_slot_implicit(s, src)) atp_cp_implicit_set(s, dst);
  else                              atp_cp_implicit_clear(s, dst);
  atp_cp_implicit_clear(s, src);
}

// (A heap sift no longer touches the descriptors: sifts swap only the
// cp_heap/cp_pos position mirror, so a slot's descriptor + tag bit stay
// put with its stationary payload row.)

// === Raw-descriptor lane (use_raw_desc / use_raw_desc_cert) =========
//
// The WM preset's raw >= 50 KPBehandelt class is pushed through the
// walk former's FT-direct emit (sw_form_push_ft_try -> vcp_ft_emit_
// fused): the packed byte string is serialized straight from the two
// parent face spans + the unifier, no heap Terms.  This lane stores,
// for exactly that class, a 20-byte descriptor (two interned face-pair
// refs + the overlap subterm's preorder index) instead of the malloc'd
// byte string, and re-runs the identical rename/unify/emit pipeline at
// pop to recover byte-exact packed bytes.  Soundness of the pointer
// capture: rule-face mirror spans are content-frozen for the state's
// lifetime -- a drop moves them to the r_dead_*_save_ft slots, an RHS
// compose abandons the old span in Arena A ("freed wholesale at
// ft_destroy time"), and nothing else writes a live face's cells -- so
// the refs re-derive the BIRTH-time pair even after parent mutation.
#define ATP_RDESC_NONE 0xFFFFFFFFu
typedef struct {
  u8  armed;      // consume-once: armed by sw_form_push_ft_try beside g_vcp_emit
  u32 out_face;   // interned (lo, ro) of the outer parent, as consumed
  u32 in_face;    // interned (lin, rin) of the inner parent, pre-rename
  u32 sub_pre;    // preorder cell index of the overlap subterm in lo
  u32 len;        // packed byte length of the emit (cert-mode memcmp bound)
} AtpRdescPush;
static AtpRdescPush g_rdesc_push;   // armed at formation (walk former)
static AtpRdescPush g_rdesc_store;  // handed to atp_cp_heap_insert_packed
static u32 atp_rdesc_intern(AtpState *s, const struct AtpFtCell *lo,
                            const struct AtpFtCell *ro);
static u8 *atp_rdesc_materialize(AtpState *s, const AtpCpImplicit *d,
                                 u32 *len_out, u32 *nodes_out,
                                 u32 *lhs_nodes_out);
static void atp_cp_rdesc_pop_fix(AtpState *s, u32 v);
static void atp_cp_implicit_arrays_ensure(AtpState *s);

// 7c': push one CP onto the binary min-heap CP queue.  Defined
// below (after atp_cp_priority); forward-declared here so the
// earlier add_equation push site can call it.  `is_ultimate`=1
// tags the CP for Waldmeister Act_ultimate (initial-axiom front)
// ranking; effective only when s->use_initial_ultimate is on.
// `raw_untreated`=1 marks the WM KPBehandelt >=50 raw class, which
// bypasses the auto-MaxWeight stash (see the definition).
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace,
                             u8 is_ultimate, u8 raw_untreated);

// Periodic full-rule-set CP-queue interreduction (Waldmeister
// KPV_KPMengeInterreduzieren).  Defined far below (it needs the
// normalizer + reheapify); forward-declared for the thvm_atp_step call.
// The period (run every Nth rule addition) lives here so the call site
// can name it.
#define ATP_CP_SET_IR_PERIOD 16u
static void atp_cp_set_interreduce(AtpState *s);
// Twee task discipline for the sweep (cp_set_ir_budget > 0): frequency,
// budget and sampled-dead-fraction gates (Twee Task.hs taskDue +
// Twee.hs:978 newTask 10 / shouldSimplifyQueue), timed in process CPU
// microseconds like Twee's getCPUTime.  Defined next to the sweep;
// forward-declared for the thvm_atp_step call site.
#define ATP_CP_SET_IR_FREQ_US    10000000ull  // Twee newTask freq = 10 CPU s
#define ATP_CP_SET_IR_THRESHOLD  20u  // Twee cfg_renormalise_threshold %
#define ATP_CP_SET_IR_SAMPLE     100u // Twee cfg_cp_sample_size
static u8 atp_cp_set_ir_due(AtpState *s);
// WM doR-only Term-side normalize (defined with the interreduce block
// far below); forward-declared for the use_pop_rules_only pop arm.
static Term atp_rules_only_normalize(AtpState *s, Term t, u32 cap);

// === Waldmeister Stringterms: packed byte-string critical pairs =====
//
// A direct port of Waldmeister's `Stringterms` module (sources/TPR/
// Stringterms.c).  Waldmeister keeps its set of unselected equations
// not as heap term-graphs but as PACKED PREORDER BYTE STRINGS -- one
// `PTermpaarT = byte *` per critical pair.  A 30-symbol CP is a
// ~30-byte string in plain malloc memory: it never enters the IC heap,
// so the copying collector never touches it.  thvm's CP queue stores
// CPs as IC heap terms instead; at `thm` step ~230 that queue is a
// ~62M-cell live set the GC re-copies every collection (the late-game
// wall the loop hit at iters 16-17).
//
// thvm has no global symbol table (Waldmeister's SO_Stelligkeit gives
// a symbol's arity), so each CTR node packs its own arity -- otherwise
// this is Waldmeister's technique verbatim: walk the pair in preorder,
// emit one self-delimiting record per node, rebuild from arity.
//
// `acp_pack` returns a malloc'd buffer (caller frees); `acp_unpack`
// rebuilds the two heap Terms.  The CP queue (`cp_packed[]`) and the
// subsumption-index records hold these byte strings directly; the
// collector roots neither, which is what frees the late-game heap.

// LEB128 varint -- 7 bits/byte, high bit = "more".
static void acp_put_varint(u8 **pp, u64 v) {
  while (v >= 0x80u) { *(*pp)++ = (u8)(v | 0x80u); v >>= 7; }
  *(*pp)++ = (u8)v;
}
static u64 acp_get_varint(const u8 **pp) {
  u64 v = 0; u32 shift = 0; u8 b;
  do { b = *(*pp)++; v |= (u64)(b & 0x7Fu) << shift; shift += 7u; }
  while (b & 0x80u);
  return v;
}

// Persistent pack scratch: the pack walk emits into this geometrically
// grown buffer, then acp_pack copies the EXACT length into the returned
// malloc.  vs the historical acp_packed_bound pre-walk + 21-bytes/node
// over-allocation this saves one full tree walk per CP AND right-sizes
// the queue's resident byte strings (~3 bytes/node typical vs 21 -- the
// multi-million-CP queue was holding ~7x dead malloc slack).
// Single-threaded saturation, so the shared scratch is safe.
static u8 *g_acp_scratch     = NULL;
static u32 g_acp_scratch_cap = 0u;

// Ensure `extra` more bytes fit at scratch position `pos`.
static void acp_scratch_ensure(u32 pos, u32 extra) {
  if (pos + extra <= g_acp_scratch_cap) return;
  u32 ncap = g_acp_scratch_cap ? g_acp_scratch_cap : 4096u;
  while (pos + extra > ncap) ncap *= 2u;
  u8 *nb = (u8 *)realloc(g_acp_scratch, ncap);
  if (nb == NULL) thvm_fatal("acp_scratch_ensure: OOM");
  g_acp_scratch = nb;
  g_acp_scratch_cap = ncap;
}

// Append `t`'s preorder packing to the scratch at `*pos`.  CTR: 'C',
// label, arity, then children; FVR: 'V', var id; NUM: 'N', dtype, raw
// value; anything else: 'E' (era placeholder -- ATP terms are
// first-order, so this is unreachable in practice but keeps unpack
// total).  Each node reserves its own worst case (1 discriminator + 2
// varints <= 21 bytes) before writing.  `*nodes` is bumped once per
// node visited -- the pack walks every node anyway, so the CP's symbol
// count (its selection weight) falls out for free, sparing
// atp_cp_priority a second full traversal.
static void acp_pack_term(Term t, u32 *pos, u32 *nodes) {
  (*nodes)++;
  acp_scratch_ensure(*pos, 21u);
  u8 *p = g_acp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      *p++ = (u8)'C';
      acp_put_varint(&p, term_ext(t));
      acp_put_varint(&p, n);
      *pos = (u32)(p - g_acp_scratch);
      for (u32 i = 0; i < n; i++) acp_pack_term(term_ctr_at(t, i), pos, nodes);
      return;
    }
    case TAG_NUM:
      *p++ = (u8)'N';
      acp_put_varint(&p, term_ext(t));
      acp_put_varint(&p, term_val(t));
      break;
    case TAG_FVR:
      *p++ = (u8)'V';
      acp_put_varint(&p, term_ext(t));
      break;
    default:
      *p++ = (u8)'E';
      break;
  }
  *pos = (u32)(p - g_acp_scratch);
}

// Rebuild one preorder-packed term, advancing `*pp` past it.
static Term acp_unpack_term(const u8 **pp) {
  if (g_atp_phase_detail) { extern u64 g_acp_unterm_nodes; g_acp_unterm_nodes++; }
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n > REWRITE_MAX_ARITY) n = REWRITE_MAX_ARITY;
      Term kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) kids[i] = acp_unpack_term(pp);
      return term_new_ctr(label, kids, n);
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      return term_new(0, TAG_NUM, dtype, val);
    }
    case 'V':
    default:
      return term_new_fvr((u32)acp_get_varint(pp));
  }
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span sibling of acp_unpack_term: build the packed preorder record
// directly as ftnew_* cells -- no Term hop, so reading a packed trace
// pair into the span world costs zero interchange.  Total: the NUM
// case builds the ATPFT_NUM_MARKER pair exactly as ft_from_term
// would (never present in a rule pair, but the format allows it).
static AtpFtCell *acp_unpack_ft(AtpFt *a, const u8 **pp);
static AtpFtCell *acp_unpack_ft(AtpFt *a, const u8 **pp) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n > REWRITE_MAX_ARITY) n = REWRITE_MAX_ARITY;
      if (n == 0u) return ftnew_const(a, label, 0);
      AtpFtCell *kids[REWRITE_MAX_ARITY];
      for (u32 i = 0; i < n; i++) kids[i] = acp_unpack_ft(a, pp);
      return ftnew_ctr(a, label, (u16)n, kids, 0);
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      AtpFtCell *kids[2] = {
        ftnew_const(a, dtype & ~WF_VAR_BIT, 0),
        ftnew_const(a, (u32)val & ~WF_VAR_BIT, 0),
      };
      return ftnew_ctr(a, ATPFT_NUM_MARKER, 2u, kids, 0);
    }
    case 'V':
    default:
      return ftnew_var(a, (u32)acp_get_varint(pp), 0);
  }
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// Pack a critical pair (lhs, rhs) into a fresh malloc'd byte string;
// `*out_len` receives its length, `*out_nodes` the total symbol count
// of lhs+rhs (the CP's selection weight).  Either out-param may be
// NULL.  The two terms pack back to back -- the preorder records are
// self-delimiting via arity, so acp_unpack reads lhs then rhs with no
// separator.
// `out_lhs_nodes` (optional) receives the LHS-only node count -- the
// per-side symbol counts the CP-weight modes (MAX/GT/MIX/MIX2/UNIF)
// would otherwise re-walk both terms for (identical values: both count
// one per node).
static u8 *acp_pack_ex(Term lhs, Term rhs, u32 *out_len, u32 *out_nodes,
                       u32 *out_lhs_nodes) {
  u32 pos = 0u, nodes = 0u;
  acp_pack_term(lhs, &pos, &nodes);
  if (out_lhs_nodes != NULL) *out_lhs_nodes = nodes;
  acp_pack_term(rhs, &pos, &nodes);
  u8 *buf = (u8 *)malloc(pos ? pos : 1u);
  if (buf == NULL) thvm_fatal("acp_pack: OOM");
  memcpy(buf, g_acp_scratch, pos);
  if (out_len   != NULL) *out_len   = pos;
  if (out_nodes != NULL) *out_nodes = nodes;
  return buf;
}

static u8 *acp_pack(Term lhs, Term rhs, u32 *out_len, u32 *out_nodes) {
  return acp_pack_ex(lhs, rhs, out_len, out_nodes, NULL);
}

// Preorder node count of one term -- the exact tally acp_pack_term
// accrues (one ++ per node: CTR head + every child, NUM, FVR), but
// without touching the scratch buffer.  Used by the byte-queue-
// retirement push path to recover cp_nodes / lhs_nodes when the byte
// pack is skipped (nothing reads cp_packed under that config).
static u32 acp_count_term(Term t) {
  u32 n = 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 k = term_ctr_n(t);
    for (u32 i = 0; i < k; i++) n += acp_count_term(term_ctr_at(t, i));
  }
  return n;
}

#ifdef THVM_ATPFT_RULES
// Decode one packed preorder term STRAIGHT into AtpFt cells -- the
// byte-side sibling of ft_from_term(acp_unpack_term(pp)) with the
// heap-Term intermediate removed.  Builds through the same ftnew_*
// constructors ft_from_term_rec uses, so every cell (sym / arity /
// flags / next / end threading, NUM-marker encoding, GROUND folding)
// is bit-identical to the two-hop route; only the heap Terms are
// never materialised.  The pop-time treatment uses this to source
// the mixmost normalize input from the queue's byte string with zero
// ft_from_term.
static AtpFtCell *acp_unpack_term_ft(AtpFt *a, const u8 **pp, int scratch) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (n == 0u) return ftnew_const(a, label, scratch);
      enum { KIDS_STACK = 16 };
      AtpFtCell *kids_stack[KIDS_STACK];
      AtpFtCell **kids      = kids_stack;
      AtpFtCell **kids_heap = NULL;
      if (n > KIDS_STACK) {
        kids_heap = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)n);
        if (kids_heap == NULL) thvm_fatal("acp_unpack_term_ft: kids OOM");
        kids = kids_heap;
      }
      for (u32 i = 0; i < n; i++) kids[i] = acp_unpack_term_ft(a, pp, scratch);
      AtpFtCell *c = ftnew_ctr(a, label, (u16)n, kids, scratch);
      free(kids_heap);
      return c;
    }
    case 'N': {
      u32 dtype = (u32)acp_get_varint(pp);
      u64 val   = acp_get_varint(pp);
      AtpFtCell *dt_cell  = ftnew_const(a, dtype & ~WF_VAR_BIT, scratch);
      AtpFtCell *raw_cell = ftnew_const(a, ((u32)val) & ~WF_VAR_BIT, scratch);
      AtpFtCell *kids[2]  = { dt_cell, raw_cell };
      return ftnew_ctr(a, ATPFT_NUM_MARKER, 2u, kids, scratch);
    }
    case 'V':
    default:
      return ftnew_var(a, (u32)acp_get_varint(pp), scratch);
  }
}

// FT sibling of thvm_normalize_vars: canonically renumber the pair's
// variables IN PLACE to a dense [0, k) set shared across both sides,
// first-occurrence order over the lhs-then-rhs preorder -- which on a
// flatterm is simply the linear span order, so one pass over each span
// suffices.  Callers own the spans (clones); adoption proves the
// result against the Term-side canonical pair with ft_eq_term, so an
// edge divergence (e.g. > REWRITE_MAX_VAR distinct vars, where the
// Term side keeps original ids) can only cause a fallback, never a
// wrong mirror.
static void ft_normalize_vars_pair(AtpFtCell *l, AtpFtCell *r) {
  u32 old_id[REWRITE_MAX_VAR];
  u32 n = 0u;
  AtpFtCell *span[2] = { l, r };
  for (u32 t = 0; t < 2u; t++) {
    AtpFtCell *e = (span[t]->end != NULL) ? span[t]->end->next : NULL;
    for (AtpFtCell *p = span[t]; p != e; p = p->next) {
      if ((p->sym & WF_VAR_BIT) == 0u) continue;
      u32 id = p->sym & ~WF_VAR_BIT;
      u32 k = 0u;
      for (; k < n; k++) {
        if (old_id[k] == id) break;
      }
      if (k == n) {
        if (n >= REWRITE_MAX_VAR) return;   // overflow: leave the rest
        old_id[n++] = id;
      }
      p->sym = WF_VAR_BIT | k;
    }
  }
}

// Distinct-variable count across a span pair (capped at
// REWRITE_MAX_VAR -- callers only test the cliff).
static u32 ft_distinct_var_count(const AtpFtCell *l, const AtpFtCell *r) {
  u32 ids[REWRITE_MAX_VAR];
  u32 n = 0u;
  const AtpFtCell *span[2] = { l, r };
  for (u32 t = 0; t < 2u; t++) {
    const AtpFtCell *e = (span[t]->end != NULL) ? span[t]->end->next : NULL;
    for (const AtpFtCell *p = span[t]; p != e; p = p->next) {
      if ((p->sym & WF_VAR_BIT) == 0u) continue;
      u32 id = p->sym & ~WF_VAR_BIT;
      u32 k = 0u;
      for (; k < n; k++) {
        if (ids[k] == id) break;
      }
      if (k == n) {
        if (n >= REWRITE_MAX_VAR) return REWRITE_MAX_VAR;
        ids[n++] = id;
      }
    }
  }
  return n;
}

// Pop-time FT stash: thvm_atp_select_cp decodes the selected packed
// CP's byte string into two fresh FT spans (bytes -> cells, no Term
// hop) and parks them here for the step function's mixmost pop
// treatment.  NULL when the pop had no byte string (implicit slot,
// orphan, CPQ skip-pack) -- the consumer falls back to ft_from_term
// of the unpacked Terms.  Single-threaded engine; consumed (and
// reset) by exactly the next pop treatment.
static AtpFtCell *g_pop_ft_l = NULL;
static AtpFtCell *g_pop_ft_r = NULL;

// Survivor NF spans: the step function parks the popped CP's flat
// normal forms here around the thvm_atp_orient_and_add call so
// atp_push_rule can ADOPT them (clone, after an exact ft_eq_term
// proof) as the new rule's FT mirror instead of re-encoding the
// canonical Terms via ft_from_term.  NULL outside that bracket --
// every other rule-add path (axiom intake, interreduce requeue)
// falls back to the conversion.
static AtpFtCell *g_rule_ft_l = NULL;
static AtpFtCell *g_rule_ft_r = NULL;

// THVM_ATP_LAZY_RULE_TERMS (requires THVM_ATP_TRACE_LEAN_RULES): the
// pop survivor is committed WITHOUT decoding its NF spans -- orient
// runs on per-side NUM sentinel handles with this bracket carrying the
// spans; atp_compare / normalize_vars / the dup scan / the mirror
// build / the trace pack all intercept on it.  The Term slots hold the
// sentinels; any residual content reader walking a NUM where a CTR
// was expected is a divergence the parity suite flushes out (the
// certification grind).  Armed only across one orient call.
typedef struct {
  const AtpFtCell *span_l;   // pop NF spans, pre-var-norm, (l, r) order
  const AtpFtCell *span_r;
  Term sent_l;               // the sentinel handles orient receives
  Term sent_r;
  u8   armed;
} AtpLazyRule;
static AtpLazyRule g_lazy_rule = {NULL, NULL, 0, 0, 0u};
static int atp_lazy_rules_on(void) {
  static int on = -1;
  if (on < 0) {
    // Default ON (with the lean rule traces below) -- certified: the
    // full parity suites pass and the solve trajectories are
    // byte-identical with the pair active.  "0" on either env is the
    // one-round kill switch; lazy additionally requires lean (the
    // sentinel commit needs the off-heap trace pack).
    const char *lz = getenv("THVM_ATP_LAZY_RULE_TERMS");
    const char *ln = getenv("THVM_ATP_TRACE_LEAN_RULES");
    int lazy_ok = (lz == NULL || lz[0] != '0');
    int lean_ok = (ln == NULL || ln[0] != '0');
    on = lazy_ok && lean_ok;
  }
  return on;
}
// Map one of the two armed sentinels to its span (NULL = not a
// sentinel of the current bracket).
static const AtpFtCell *atp_lazy_span_of(Term t) {
  if (!g_lazy_rule.armed) return NULL;
  if (t == g_lazy_rule.sent_l) return g_lazy_rule.span_l;
  if (t == g_lazy_rule.sent_r) return g_lazy_rule.span_r;
  return NULL;
}

// Span sibling of wald_term_sort: identical sort inference off the
// mirror cells (vars by id from the spec table; constructors by label
// with arity + argument-sort checks; the TAG_NUM marker maps to the
// non-FOL-tag verdict).
static u32 wald_term_sort_ft(const WaldSpec *spec, const AtpFtCell *c) {
  if (spec == NULL || spec->n_sorts == 0) return 0;
  if ((c->sym & WF_VAR_BIT) != 0u) {
    u32 id = c->sym & ~WF_VAR_BIT;
    for (u32 i = 0; i < spec->n_vars; i++) {
      if (spec->vars[i].var_id == id) return spec->vars[i].sort;
    }
    return WALD_MAX_SORTS;
  }
  u32 lab = c->sym;
  if (lab == ATPFT_NUM_MARKER) return WALD_MAX_SORTS;   // non-FOL tag
  u32 n = (u32)c->arity;
  const WaldSym *sym = NULL;
  for (u32 i = 0; i < spec->n_symbols; i++) {
    if (spec->symbols[i].label == lab) {
      sym = &spec->symbols[i];
      break;
    }
  }
  if (sym == NULL)     return WALD_MAX_SORTS;
  if (sym->arity != n) return WALD_MAX_SORTS;
  const AtpFtCell *kid = c->next;
  for (u32 i = 0; i < n; i++) {
    u32 child_sort = wald_term_sort_ft(spec, kid);
    if (child_sort != sym->arg_sorts[i]) return WALD_MAX_SORTS;
    kid = kid->end->next;
  }
  return sym->result_sort;
}

// Decode-on-demand for a lazy rule slot: a TAG_NUM handle in a rule
// slot is always a lazy sentinel (rule terms are CTR/FVR only), so
// materialize the Term from the mirror ONCE, replace the slot, and
// return it.  Every site that PERSISTS rule-Term values into
// Term-typed storage (r_dead saves, subsumer shapes, the irv victim
// buffer, the MNF pending queue) calls this so no sentinel ever
// escapes the slot arrays.
static Term atp_rule_term(AtpState *s, u32 i, u8 side) {
  Term t = side ? s->rhs[i] : s->lhs[i];
  if (term_tag(t) != TAG_NUM) return t;
  const AtpFtCell *m = side ? s->rhs_ft[i] : s->lhs_ft[i];
  if (m == NULL) return t;   // defensive: nothing to decode from
  Term real = ft_to_term(m);
  if (side) s->rhs[i] = real;
  else      s->lhs[i] = real;
  return real;
}

// Mirror-aware rule-side symbol count: a span's cell count equals the
// Term's node count (the TAG_NUM marker subtree counts as the ONE
// node its Term is).  Reads the mirror for lazy slots so weight scans
// never see sentinel sizes.
static u32 ft_symbol_count_span(const AtpFtCell *c) {
  u32 n = 0u;
  const AtpFtCell *e = (c->end != NULL) ? c->end->next : NULL;
  for (const AtpFtCell *p = c; p != e; p = p->next) {
    if ((p->sym & WF_VAR_BIT) == 0u && p->sym == ATPFT_NUM_MARKER) {
      n++;
      p = p->end;   // marker + 2 field kids = one Term node
      continue;
    }
    n++;
  }
  return n;
}
// Span var-containment: every var of `a` occurs in `b` (the exact
// sibling of atp_vars_contained on mirror cells).
static u8 ft_span_has_var(const AtpFtCell *b, u32 id) {
  const AtpFtCell *e = (b->end != NULL) ? b->end->next : NULL;
  for (const AtpFtCell *q = b; q != e; q = q->next) {
    if ((q->sym & WF_VAR_BIT) != 0u && (q->sym & ~WF_VAR_BIT) == id) {
      return 1u;
    }
  }
  return 0u;
}
static u8 ft_vars_contained_span(const AtpFtCell *a, const AtpFtCell *b) {
  const AtpFtCell *e = (a->end != NULL) ? a->end->next : NULL;
  for (const AtpFtCell *q = a; q != e; q = q->next) {
    if ((q->sym & WF_VAR_BIT) != 0u &&
        !ft_span_has_var(b, q->sym & ~WF_VAR_BIT)) {
      return 0u;
    }
  }
  return 1u;
}

static u32 atp_rule_symbol_count(AtpState *s, u32 i, u8 side) {
  Term t = side ? s->rhs[i] : s->lhs[i];
  if (term_tag(t) == TAG_NUM) {
    const AtpFtCell *m = side ? s->rhs_ft[i] : s->lhs_ft[i];
    if (m != NULL) return ft_symbol_count_span(m);
  }
  return atp_symbol_count(t);
}

// Keep-site staging for the MNF pend spans (set right before
// mnf_notify_fact; consumed by it).
static const AtpFtCell *g_mnf_pend_ft_l = NULL;
static const AtpFtCell *g_mnf_pend_ft_r = NULL;

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
static u8 atp_order_is_plain_kbo(const AtpState *s);
#endif

// Requeue bracket: the demote drain parks the victim's (possibly NF)
// spans here around atp_add_equation_simplified so the enqueue can
// canonicalize / trace / fused-pack straight from them -- the queued
// CP is bytes + trace, so no Term ever materializes on the requeue.
// Read-only for the enqueue (it clones internally); cleared by it.
static const AtpFtCell *g_enq_ft_l = NULL;
static const AtpFtCell *g_enq_ft_r = NULL;

// Trace-pack span bracket: the ORIENT/FVI trace loop arms this with
// the new rule's mirrors so atp_trace_push's lean arm packs the pair
// from spans (atp_trace_eq_store_ft) instead of walking the (possibly
// sentinel) Terms.  Cleared by the push.
static const AtpFtCell *g_trace_eq_ft_l = NULL;
static const AtpFtCell *g_trace_eq_ft_r = NULL;
#endif // THVM_ATPFT_RULES

// Inverse of acp_pack: rebuild both heap Terms from the byte string.
static void acp_unpack(const u8 *buf, Term *lhs, Term *rhs) {
  const u8 *p = buf;
  Term l = acp_unpack_term(&p);
  Term r = acp_unpack_term(&p);
  if (lhs != NULL) *lhs = l;
  if (rhs != NULL) *rhs = r;
}

// === Fused varnorm + pack + KBO-weigh (one walk per CP) =============
//
// The CP push pipeline historically walked every surviving CP three
// times: thvm_normalize_vars rebuilt the pair with dense var ids (one
// walk + a full Term-cell rebuild), acp_pack walked the rebuilt pair
// to serialize it, and the GT/MIX weigh (thvm_kbo_nomemo via
// kbo_lin_addto) walked it a third time for the weight/variable
// balances.  The three are fused here into ONE preorder pass over the
// PRE-renumber pair that simultaneously
//   (a) assigns canonically dense var ids by first occurrence -- the
//       exact thvm_normalize_vars numbering (lhs fully, then rhs; see
//       unify/_.c nv_rewrite) -- WITHOUT materializing renumbered
//       Term cells,
//   (b) emits the packed byte string of the renumbered pair into
//       g_acp_scratch -- byte-identical to normalize-then-acp_pack,
//   (c) encodes each side as a flat preorder node array (dense ids)
//       and accumulates the whole-pair weight balance (phidiff) and
//       per-variable occurrence balance -- the exact sums
//       thvm_kbo_nomemo's Vortest computes for the top-level compare
//       (matching subtrees cancel there, so the net sums are equal;
//       kbo_lin_decide_clear's sign classification is accumulation-
//       order-invariant).
// The weigh (acp_fuse_kbo) then decides GT/LT/UN/EQ from the flat
// form: identity is a memcmp, the weight/var verdict comes from the
// accumulated balances, and only a genuine phi tie descends -- via a
// mirror of kbo_lin_rek over the flat arrays (kbo_flat_rek's decision
// core with the Term path's arity guard restored, since a flat node
// carries its arity here).  The renumber's KBO-cap property is what
// makes this sound where weighing the RAW pair is not: raw overlap
// terms carry j-side ids >= CP_RENAME_OFFSET, above the KBO balance
// caps (see atp_cp_order_gate); the dense ids stay inside them.
//
// Anomalies -- a NUM/ERA atom, a non-NUM arity cell, arity >
// REWRITE_MAX_ARITY, or > REWRITE_MAX_VAR distinct variables (the
// classes where nv_rewrite bails or the flat encoding cannot mirror
// thvm_kbo) -- abort the fused walk; the caller falls back to the
// classic normalize + pack + Term-weigh pipeline, so the fused path
// never has to replicate the bail semantics.  Unreachable on ATP
// first-order signatures; the fallback counter should stay 0.
//
// Kill switch: THVM_ATP_NO_FUSE_PACK=1 (same-binary A/B).  Certifier:
// THVM_ATP_FUSE_CHECK=1 re-runs the classic pipeline per CP and
// asserts byte-equal packing + verdict-equal weighing.

// Dense-id map: old id -> position == dense id, first-occurrence
// ordered (the exact nv_rewrite assignment).
typedef struct {
  u32 old_id[REWRITE_MAX_VAR];
  u32 n;
} AcpNvMap;

// Flat preorder node.  sym >= 0 is a CTR label, sym < 0 encodes a
// (dense) variable id as -(id+1); w is the node's own KBO weight; sz
// the subtree span in nodes; ar the CTR arity (0 for a variable) --
// the field kbo/_.c's KboFlatNode lacks, restored here so the lex
// descent keeps thvm_kbo's ns != nt incomparability guard.
typedef struct {
  i32 sym;
  u32 w;
  u32 sz;
  u32 ar;
} AcpFlatNode;

// Per-CP flat pool: lhs at [0, fl_n), rhs at [fl_n, fl_n + fr_n).
// Grown geometrically, persistent across CPs (single-threaded
// saturation, like g_acp_scratch).
static AcpFlatNode *g_acpf_pool = NULL;
static u32          g_acpf_cap  = 0u;

static void acpf_ensure(u32 need) {
  if (need <= g_acpf_cap) return;
  u32 ncap = g_acpf_cap ? g_acpf_cap : 1024u;
  while (need > ncap) ncap *= 2u;
  AcpFlatNode *np = (AcpFlatNode *)realloc(g_acpf_pool,
                                           (size_t)ncap * sizeof *np);
  if (np == NULL) thvm_fatal("acpf_ensure: OOM");
  g_acpf_pool = np;
  g_acpf_cap  = ncap;
}

// Fused-walk state for the CP being packed.  `valid` marks the flat
// encoding + balances live for exactly one weigh (atp_weigh_compare
// consumes it; atp_cp_heap_push clears it on every exit path).
typedef struct {
  AcpNvMap  map;
  long long phidiff;                 // w(lhs) - w(rhs)
  int       bal[REWRITE_MAX_VAR];    // count(lhs,v) - count(rhs,v), dense v
  u32       fl_n, fr_n;              // per-side flat node counts
  u8        valid;
  // ATP_CP_WEIGHT_TWEE per-side scores, accumulated by ftp_pack_fused
  // for the treated FT-direct push.  Exact because the treated
  // survivor's classic Term image is ft_to_term(NF) -- fresh unshared
  // cells, so atp_twee_term_score's Term-identity dedup can never fire
  // and the score is the plain weighted count 7*|side| - #vars(side),
  // provided the side stays under the classic walk's 256-node stack
  // cap (twee_valid=0 above it; the consumer then materializes and
  // scores classically).  Consumed by atp_cp_priority_sized_ex.
  u32       twee_l, twee_r;
  u8        twee_valid;
} AcpFuse;
static AcpFuse g_acp_fuse;

// Fused-pipeline shape counters (PROFILE=2 printout; fallbacks should
// be 0 on ATP signatures).
u64 g_acp_fuse_n_ok = 0;
u64 g_acp_fuse_n_fb = 0;

// One preorder step of the fused walk: pack `t` (renumbered on emit)
// at scratch position *pos, bump *nodes once per node, and -- when
// `fz` is non-NULL -- append the flat node at *fpos and accumulate the
// weight/var balances with `sign` (+1 lhs, -1 rhs).  Returns 0 on an
// anomaly (see the header comment); the caller falls back whole.
// Reference recursive form -- the default entry is the explicit-stack
// iteration below (acp_fuse_walk), which keeps pos/nodes/fpos in
// registers instead of chasing three out-pointers per node and pays no
// call bracket per child.  THVM_ATP_NO_FUSE_ITER=1 routes back here
// (same-binary kill switch); both emit the identical preorder bytes /
// flat nodes / balances.
static u8 acp_fuse_walk_rec(Term t, const KboConfig *cfg, AcpNvMap *m,
                            u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                            u32 *fpos) {
  (*nodes)++;
  acp_scratch_ensure(*pos, 21u);
  u8 *p = g_acp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_CTR: {
      // nv_rewrite's defensive guards (non-NUM arity cell, over-arity
      // node) leave the subtree un-renumbered there; abort to the
      // classic pipeline instead of mirroring the bail.
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (term_tag(n_cell) != TAG_NUM) return 0;
      u32 n = (u32)term_val(n_cell);
      if (n > REWRITE_MAX_ARITY) return 0;
      u32 lab = term_ext(t);
      *p++ = (u8)'C';
      acp_put_varint(&p, lab);
      acp_put_varint(&p, n);
      *pos = (u32)(p - g_acp_scratch);
      u32 here = 0u;
      if (fz != NULL) {
        acpf_ensure(*fpos + 1u);
        here = (*fpos)++;
        u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
        g_acpf_pool[here].sym = (i32)lab;
        g_acpf_pool[here].w   = w;
        g_acpf_pool[here].ar  = n;
        fz->phidiff += (long long)sign * (long long)w;
      }
      for (u32 i = 0; i < n; i++) {
        if (!acp_fuse_walk_rec(heap_read(base + 1u + (u64)i), cfg, m, pos,
                               nodes, fz, sign, fpos)) return 0;
      }
      if (fz != NULL) g_acpf_pool[here].sz = *fpos - here;
      return 1;
    }
    case TAG_FVR: {
      u32 old = term_ext(t);
      u32 d = 0u;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;   // nv keeps ids past a
                                                 // full map; fall back
        m->old_id[m->n++] = old;
        if (fz != NULL) fz->bal[d] = 0;
      }
      *p++ = (u8)'V';
      acp_put_varint(&p, d);
      *pos = (u32)(p - g_acp_scratch);
      if (fz != NULL) {
        acpf_ensure(*fpos + 1u);
        g_acpf_pool[*fpos].sym = -(i32)(d + 1u);
        g_acpf_pool[*fpos].w   = cfg->var_weight;
        g_acpf_pool[*fpos].sz  = 1u;
        g_acpf_pool[*fpos].ar  = 0u;
        (*fpos)++;
        fz->phidiff += (long long)sign * (long long)cfg->var_weight;
        fz->bal[d]  += sign;
      }
      return 1;
    }
    default:
      return 0;   // NUM / ERA atom: the flat compare cannot mirror
                  // thvm_kbo's term_val identity; classic path handles
  }
}

// Explicit-stack form of the fused walk -- the default entry.  Same
// preorder, same emissions, same anomaly classes as the recursive
// reference above; pos/nodes/fpos live in locals across the whole
// tree (the recursive form dereferences three out-pointers at every
// node) and a child costs one frame push instead of an 8-argument
// call bracket.  A tree deeper than the frame stack returns 0 -- the
// caller's classic fallback emits the identical bytes, so the cap is
// a performance edge, not a semantic one (OA's deepest CP sides run
// ~1000 frames; the cap is 4x that).
enum { ACP_FUSE_MAX_DEPTH = 4096 };
typedef struct { u64 base; u32 n; u32 i; u32 here; } AcpFuseFrame;
static AcpFuseFrame g_acp_fuse_stk[ACP_FUSE_MAX_DEPTH];
static u8 acp_fuse_walk_iter(Term t, const KboConfig *cfg, AcpNvMap *m,
                             u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                             u32 *fpos) {
  u32 lpos = *pos, lnodes = *nodes, lfpos = *fpos;
  u32 sp = 0u;
  Term cur = t;
  for (;;) {
    lnodes++;
    acp_scratch_ensure(lpos, 21u);
    u8 *p = g_acp_scratch + lpos;
    switch (term_tag(cur)) {
      case TAG_CTR: {
        u64 base = term_val(cur);
        Term n_cell = heap_read(base);
        if (term_tag(n_cell) != TAG_NUM) return 0;
        u32 n = (u32)term_val(n_cell);
        if (n > REWRITE_MAX_ARITY) return 0;
        u32 lab = term_ext(cur);
        *p++ = (u8)'C';
        acp_put_varint(&p, lab);
        acp_put_varint(&p, n);
        lpos = (u32)(p - g_acp_scratch);
        u32 here = 0u;
        if (fz != NULL) {
          acpf_ensure(lfpos + 1u);
          here = lfpos++;
          u32 w = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
          g_acpf_pool[here].sym = (i32)lab;
          g_acpf_pool[here].w   = w;
          g_acpf_pool[here].ar  = n;
          fz->phidiff += (long long)sign * (long long)w;
        }
        if (n > 0u) {
          if (sp >= (u32)ACP_FUSE_MAX_DEPTH) return 0;   // classic fallback
          g_acp_fuse_stk[sp].base = base;
          g_acp_fuse_stk[sp].n    = n;
          g_acp_fuse_stk[sp].i    = 0u;
          g_acp_fuse_stk[sp].here = here;
          sp++;
          cur = heap_read(base + 1u);
          continue;
        }
        if (fz != NULL) g_acpf_pool[here].sz = lfpos - here;
        break;
      }
      case TAG_FVR: {
        u32 old = term_ext(cur);
        u32 d = 0u;
        for (; d < m->n; d++) {
          if (m->old_id[d] == old) break;
        }
        if (d == m->n) {
          if (m->n >= REWRITE_MAX_VAR) return 0;   // full map; fall back
          m->old_id[m->n++] = old;
          if (fz != NULL) fz->bal[d] = 0;
        }
        *p++ = (u8)'V';
        acp_put_varint(&p, d);
        lpos = (u32)(p - g_acp_scratch);
        if (fz != NULL) {
          acpf_ensure(lfpos + 1u);
          g_acpf_pool[lfpos].sym = -(i32)(d + 1u);
          g_acpf_pool[lfpos].w   = cfg->var_weight;
          g_acpf_pool[lfpos].sz  = 1u;
          g_acpf_pool[lfpos].ar  = 0u;
          lfpos++;
          fz->phidiff += (long long)sign * (long long)cfg->var_weight;
          fz->bal[d]  += sign;
        }
        break;
      }
      default:
        return 0;   // NUM / ERA atom (see the recursive reference)
    }
    // Ascend: advance to the next pending sibling, fixing up each
    // completed CTR's subtree span on the way out.
    for (;;) {
      if (sp == 0u) {
        *pos = lpos; *nodes = lnodes; *fpos = lfpos;
        return 1;
      }
      AcpFuseFrame *f = &g_acp_fuse_stk[sp - 1u];
      if (++f->i < f->n) {
        cur = heap_read(f->base + 1u + (u64)f->i);
        break;
      }
      if (fz != NULL) g_acpf_pool[f->here].sz = lfpos - f->here;
      sp--;
    }
  }
}

// Default dispatcher: the iterative walk unless THVM_ATP_NO_FUSE_ITER=1
// pins the recursive reference (kill switch; both are byte-identical).
static u8 acp_fuse_walk(Term t, const KboConfig *cfg, AcpNvMap *m,
                        u32 *pos, u32 *nodes, AcpFuse *fz, int sign,
                        u32 *fpos) {
  static int no_iter = -1;
  if (no_iter < 0) {
    const char *e = getenv("THVM_ATP_NO_FUSE_ITER");
    no_iter = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  if (no_iter) return acp_fuse_walk_rec(t, cfg, m, pos, nodes, fz, sign, fpos);
  return acp_fuse_walk_iter(t, cfg, m, pos, nodes, fz, sign, fpos);
}

// Fused renumber+pack of a raw pair into the scratch (no flat encode,
// no balances): bytes identical to thvm_normalize_vars-then-
// acp_pack_term x2.  Serves the off-heap trace store.  Returns 0 on
// an anomaly (caller renumbers + packs classically).
static u8 acp_trace_pack_fused(Term lhs, Term rhs, u32 *pos) {
  AcpNvMap m;
  m.n = 0u;
  u32 nodes = 0u, fpos = 0u;
  if (!acp_fuse_walk(lhs, NULL, &m, pos, &nodes, NULL, +1, &fpos)) return 0;
  if (!acp_fuse_walk(rhs, NULL, &m, pos, &nodes, NULL, -1, &fpos)) return 0;
  return 1;
}

// Fused counterpart of thvm_normalize_vars + acp_pack_ex: packs the
// renumbered pair into a fresh exact-size malloc and leaves the flat
// encoding + balances armed in g_acp_fuse for the weigh.  NULL on an
// anomaly (nothing armed; caller runs the classic pipeline).
static u8 *acp_pack_fused(const KboConfig *cfg, Term lhs, Term rhs,
                          u32 *out_len, u32 *out_nodes,
                          u32 *out_lhs_nodes) {
  AcpFuse *fz = &g_acp_fuse;
  fz->valid   = 0u;
  fz->twee_valid = 0u;
  fz->map.n   = 0u;
  fz->phidiff = 0;
  u32 pos = 0u, nodes = 0u, fpos = 0u;
  if (!acp_fuse_walk(lhs, cfg, &fz->map, &pos, &nodes, fz, +1, &fpos)) {
    return NULL;
  }
  u32 lhs_nodes = nodes, fl = fpos;
  if (!acp_fuse_walk(rhs, cfg, &fz->map, &pos, &nodes, fz, -1, &fpos)) {
    return NULL;
  }
  fz->fl_n  = fl;
  fz->fr_n  = fpos - fl;
  fz->valid = 1u;
  u8 *buf = (u8 *)malloc(pos ? pos : 1u);
  if (buf == NULL) thvm_fatal("acp_pack_fused: OOM");
  memcpy(buf, g_acp_scratch, pos);
  if (out_len != NULL) *out_len = pos;
  *out_nodes     = nodes;
  *out_lhs_nodes = lhs_nodes;
  return buf;
}

// Add the flat subtree at a[pos] to one side of the current balance
// scope (mirror of kbo_lin_addto_walk / kbo_flat_addto): one linear
// sweep over the contiguous slice.
static void acpf_addto(KboLin *st, const AcpFlatNode *a, u32 pos, int sign,
                       long long *phidiff) {
  u32 end = pos + a[pos].sz;
  for (u32 i = pos; i < end; i++) {
    *phidiff += (long long)sign * (long long)a[i].w;
    if (a[i].sym < 0) kbo_bump(st, (u32)(-a[i].sym) - 1u, sign);
  }
}

// Combined Vortest over two flat slices (mirror of kbo_vortest):
// matching subtrees cancel and are skipped, divergent subtrees go
// wholly onto + / -.  Returns 1 iff structurally identical.  The
// arity test mirrors kbo_vortest's ns == nt gate exactly.
static int acpf_vortest(KboLin *st, const AcpFlatNode *a, u32 pa,
                        const AcpFlatNode *b, u32 pb, long long *phidiff) {
  if (a[pa].sym == b[pb].sym) {
    if (a[pa].sym < 0) return 1;                     // same variable
    if (a[pa].ar == b[pb].ar) {
      // Identical-slice fast path: byte-equal flat slices are
      // structurally equal subtrees -- no divergence, no contribution.
      if (a[pa].sz == b[pb].sz &&
          memcmp(&a[pa], &b[pb],
                 (size_t)a[pa].sz * sizeof(AcpFlatNode)) == 0) {
        return 1;
      }
      u32 ca = pa + 1u, cb = pb + 1u;
      int ident = 1;
      for (u32 i = 0; i < a[pa].ar; i++) {
        ident &= acpf_vortest(st, a, ca, b, cb, phidiff);
        ca += a[ca].sz;
        cb += b[cb].sz;
      }
      return ident;
    }
  }
  // diverge: a[pa] wholly on +, b[pb] wholly on -.
  acpf_addto(st, a, pa, +1, phidiff);
  acpf_addto(st, b, pb, -1, phidiff);
  return 0;
}

// Decision after Vortest -- mirror of kbo_lin_rek's per-level logic
// (KBOEntscheidungRek + KBOEntscheidungLex) over the flat slices,
// including the Term path's arity guard.  Precondition: the subtrees
// are not identical and the balance table is cleared.
static KboCmp acpf_rek(KboLin *st, const AcpFlatNode *a, u32 pa,
                       const AcpFlatNode *b, u32 pb,
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
    default:
      return KBO_UN;
  }
  // phi tie: compare top symbols by precedence.
  if (a[pa].sym < 0 || b[pb].sym < 0) return KBO_UN;
  const KboConfig *cfg = st->cfg;
  u32 la = (u32)a[pa].sym, lb = (u32)b[pb].sym;
  u32 ps = (la < cfg->n_labels) ? cfg->precedence[la] : 0;
  u32 pt = (lb < cfg->n_labels) ? cfg->precedence[lb] : 0;
  if (ps > pt) return (varcmp == KBO_LT) ? KBO_UN : KBO_GT;
  if (ps < pt) return (varcmp == KBO_GT) ? KBO_UN : KBO_LT;
  if (la != lb) return KBO_UN;
  if (a[pa].ar != b[pb].ar) return KBO_UN;   // kbo_lin_rek's ns != nt
  // KBOEntscheidungLex: first non-identical child pair decides; the
  // deeper result maps back through this level's varcmp.
  u32 ca = pa + 1u, cb = pb + 1u;
  for (u32 i = 0; i < a[pa].ar; i++) {
    long long phi = 0;
    int ident = acpf_vortest(st, a, ca, b, cb, &phi);
    if (ident) {
      kbo_lin_clear(st);
      ca += a[ca].sz;
      cb += b[cb].sz;
      continue;
    }
    KboCmp cvar  = kbo_lin_decide_clear(st);
    KboCmp inner = acpf_rek(st, a, ca, b, cb, phi, cvar);
    return kbo_map_through(varcmp, inner);
  }
  return KBO_EQ;
}

// The fused CP weigh: verdict-identical to
// thvm_kbo_nomemo(lhs', rhs', cfg) on the renumbered pair the fused
// walk encoded.  Identity = flat memcmp (== Vortest's ident); the
// top-level phidiff/var balances come pre-accumulated from the pack
// walk; only a phi tie descends via the flat Rek/Lex mirror above.
// `_at` takes an explicit fuse state + flat pool base so a caller with
// a LOCAL encoding (the FT-emit virtual order gate) can weigh without
// touching the armed global; the bare entry reads the globals.
static KboCmp acp_fuse_kbo_at(const KboConfig *cfg, const AcpFuse *fz,
                              const AcpFlatNode *pool) {
  const AcpFlatNode *a = pool;
  const AcpFlatNode *b = pool + fz->fl_n;
  if (fz->fl_n == fz->fr_n &&
      memcmp(a, b, (size_t)fz->fl_n * sizeof(AcpFlatNode)) == 0) {
    return KBO_EQ;
  }
  // Variable-domination verdict from the accumulated balances --
  // kbo_lin_decide_clear's sign classification over the dense ids.
  KboCmp varcmp = KBO_EQ;
  for (u32 k = 0; k < fz->map.n; k++) {
    int v = fz->bal[k];
    if (v == 0) continue;
    switch (varcmp) {
      case KBO_EQ: varcmp = (v < 0) ? KBO_LT : KBO_GT; break;
      case KBO_LT: if (v > 0) varcmp = KBO_UN; break;
      case KBO_GT: if (v < 0) varcmp = KBO_UN; break;
      default: break;
    }
  }
  KboLin *st = &g_kbo_st;
  st->cfg       = cfg;
  st->n_touched = 0;
  return acpf_rek(st, a, 0u, b, 0u, fz->phidiff, varcmp);
}

static KboCmp acp_fuse_kbo(const KboConfig *cfg) {
  return acp_fuse_kbo_at(cfg, &g_acp_fuse, g_acpf_pool);
}

// Set around atp_trace_push_cp by the fused push loop: the off-heap
// trace store renumbers the raw pair during its own byte emit.
static u8 g_trace_eq_nv = 0u;
// Set around atp_cp_heap_push by the fused push loop: the heap push
// takes the fused pack (its caller skipped the treated renumber).
static u8 g_cp_push_fused = 0u;

// === Simpler-join pop-time peak stash (AtpState.cp_peak) =============
//
// Twee joins a critical pair at SELECTION with the then-current rule
// set, and its checkOrder/step3 legs (Join.hs:81-120) read the
// overlap's `top` -- sigma(l_outer), the peak both CP sides descend
// from -- which Twee recovers by re-deriving the overlap from its
// parents at pop.  thvm queues the (treated) pair itself, so the peak
// used to exist only at generation time and the pop/interreduce
// simpler-join re-checks degraded to the peakless step1+2 verdict
// (the documented residual divergence in ft_norm.c; on GRP706-1 the
// missing pop-time below-peak joining kept a flood Twee prunes).
//
// The stash closes that gap: at push, the formation peak is packed
// (acp preorder records, 'C'/'V' only) RENUMBERED THROUGH THE SAME
// dense first-occurrence var map as the queued pair -- shared
// variables keep shared ids, peak-only variables extend the map past
// the pair's k -- and stored per slot in s->cp_peak[].  At pop /
// interreduce re-checks the bytes decode to an Arena-A span and arm
// the g_ftsj peak for the SAME faithful allSteps pass the treat runs.
// Var-space soundness: the queued bytes are the pack-time renumbered
// pair, pop-side rewriting only instantiates from redex matches (an
// ordered step never introduces variables -- KBO decrease requires
// var containment), so the pop NFs stay inside the pair's dense id
// set and every peak-vs-side compare sees consistent identities.
//
// Degrade-to-peakless (slot stays NULL, exactly the pre-stash
// behaviour): requeued interreduce victims (no overlap, no peak --
// Twee has no such requeue), the raw-emit FT route + RAW_DESC
// descriptor lane (the re-derivation reconstructs the overlap at pop;
// peak wiring there is future work, see the report), record-mode
// replay (the pair there is the trace RAW pack, a different var
// space), drc-kept CPs (the kept content is re-renumbered), pack
// anomalies (NUM/ERA atoms, over-arity, > REWRITE_MAX_VAR vars --
// unreachable on ATP signatures), and CPQ builds whose slot stores no
// bytes (the adopted spans keep raw ids).  Over-keeping is the only
// possible effect of a missing peak; soundness is untouched (proofs
// replay recorded engine steps).
static u8 *g_sj_peak_pending = NULL;   // consume-once: armed by the push
                                       // loop, attached by insert_packed

// Storage management -- compiled unconditionally (the array stays NULL
// and every helper is a single-branch no-op unless a pack site under
// the FT defines ever arms a pending peak).
static void atp_cp_peak_arrays_ensure(AtpState *s) {
  if (s->cp_peak != NULL) return;
  u32 cap = s->cp_cap ? s->cp_cap : 1u;
  s->cp_peak = (u8 **)calloc((size_t)cap, sizeof(u8 *));
  if (s->cp_peak == NULL) thvm_fatal("atp_cp_peak_arrays_ensure: OOM");
}
static inline void atp_cp_peak_clear(AtpState *s, u32 i) {
  if (s->cp_peak != NULL && s->cp_peak[i] != NULL) {
    free(s->cp_peak[i]);
    s->cp_peak[i] = NULL;
  }
}
// Move dst <- src, vacating src (row backfill / densify companion).
// dst is being overwritten wholesale, so any bytes still there are
// dead -- free them (free(NULL) no-op) rather than leak.
static inline void atp_cp_peak_move(AtpState *s, u32 dst, u32 src) {
  if (s->cp_peak == NULL) return;
  free(s->cp_peak[dst]);
  s->cp_peak[dst] = s->cp_peak[src];
  s->cp_peak[src] = NULL;
}
static void atp_sj_peak_pending_drop(void) {
  free(g_sj_peak_pending);
  g_sj_peak_pending = NULL;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Pop-time decoded peak span (Arena A), parked by select_cp for the
// step function's pop-treatment simpler-join; consumed (freed) there.
// At most one outstanding -- select_cp clears a leftover defensively.
// The owner arena rides along so a leftover from another state is
// dropped (its arena frees it wholesale at destroy) instead of being
// pushed onto the wrong free list.
static AtpFtCell *g_pop_sj_peak       = NULL;
static void      *g_pop_sj_peak_owner = NULL;
static void atp_pop_sj_peak_drop(const AtpState *s) {
  if (g_pop_sj_peak == NULL) return;
  if (s != NULL && s->ft_arena_ptr != NULL &&
      s->ft_arena_ptr == g_pop_sj_peak_owner) {
    ft_free_span((AtpFt *)s->ft_arena_ptr, g_pop_sj_peak,
                 g_pop_sj_peak->end);
  }
  g_pop_sj_peak       = NULL;
  g_pop_sj_peak_owner = NULL;
}

// Private emit scratch: the peak pack runs between the trace-store
// emit and the heap push, when g_acp_scratch may still hold live pair
// bytes (the g_vcp_emit route) -- a separate buffer sidesteps every
// aliasing question.
static u8 *g_sjp_scratch = NULL;
static u32 g_sjp_cap     = 0u;
static void sjp_ensure(u32 pos, u32 extra) {
  if (pos + extra <= g_sjp_cap) return;
  u32 ncap = g_sjp_cap ? g_sjp_cap : 1024u;
  while (pos + extra > ncap) ncap *= 2u;
  u8 *nb = (u8 *)realloc(g_sjp_scratch, ncap);
  if (nb == NULL) thvm_fatal("sjp_ensure: OOM");
  g_sjp_scratch = nb;
  g_sjp_cap = ncap;
}

// First-occurrence dense-map walk over a Term (no emit): the exact
// nv_rewrite / acp_fuse_walk numbering order.  0 on an anomaly.
static u8 sjp_map_walk_term(Term t, AcpNvMap *m) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return 0;
      for (u32 i = 0; i < n; i++) {
        if (!sjp_map_walk_term(term_ctr_at(t, i), m)) return 0;
      }
      return 1;
    }
    case TAG_FVR: {
      u32 old = term_ext(t);
      u32 d = 0;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;
        m->old_id[m->n++] = old;
      }
      return 1;
    }
    default:
      return 0;   // NUM/ERA atom: the sj compare never sees these
  }
}

// Emit `t` renumbered through (an extension of) `m` into the private
// scratch at *pos -- byte-format identical to acp_pack_term's 'C'/'V'
// records, so acp_unpack_term_ft decodes it.  0 on an anomaly.
static u8 sjp_emit_term(Term t, AcpNvMap *m, u32 *pos) {
  sjp_ensure(*pos, 21u);
  u8 *p = g_sjp_scratch + *pos;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      if (n > REWRITE_MAX_ARITY) return 0;
      *p++ = (u8)'C';
      acp_put_varint(&p, term_ext(t));
      acp_put_varint(&p, n);
      *pos = (u32)(p - g_sjp_scratch);
      for (u32 i = 0; i < n; i++) {
        if (!sjp_emit_term(term_ctr_at(t, i), m, pos)) return 0;
      }
      return 1;
    }
    case TAG_FVR: {
      u32 old = term_ext(t);
      u32 d = 0;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;
        m->old_id[m->n++] = old;
      }
      *p++ = (u8)'V';
      acp_put_varint(&p, d);
      *pos = (u32)(p - g_sjp_scratch);
      return 1;
    }
    default:
      return 0;
  }
}

// Span siblings: linear preorder over the ->next chain (a span's chain
// IS the preorder), arity from the cell -- the ftp_pack_fused record
// layout.  Marker syms (converted NUM / foreign tags) bail.
static u8 sjp_map_walk_span(const AtpFtCell *t, AcpNvMap *m) {
  const AtpFtCell *end_after = (t->end != NULL) ? t->end->next : NULL;
  for (const AtpFtCell *p = t; p != NULL && p != end_after; p = p->next) {
    if ((p->sym & WF_VAR_BIT) != 0u) {
      u32 old = p->sym & ~WF_VAR_BIT;
      u32 d = 0;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;
        m->old_id[m->n++] = old;
      }
    } else if (p->sym >= 0x20000000u || p->arity > REWRITE_MAX_ARITY) {
      return 0;
    }
  }
  return 1;
}
static u8 sjp_emit_span(const AtpFtCell *t, AcpNvMap *m, u32 *pos) {
  const AtpFtCell *end_after = (t->end != NULL) ? t->end->next : NULL;
  for (const AtpFtCell *c = t; c != NULL && c != end_after; c = c->next) {
    sjp_ensure(*pos, 21u);
    u8 *p = g_sjp_scratch + *pos;
    if ((c->sym & WF_VAR_BIT) != 0u) {
      u32 old = c->sym & ~WF_VAR_BIT;
      u32 d = 0;
      for (; d < m->n; d++) {
        if (m->old_id[d] == old) break;
      }
      if (d == m->n) {
        if (m->n >= REWRITE_MAX_VAR) return 0;
        m->old_id[m->n++] = old;
      }
      *p++ = (u8)'V';
      acp_put_varint(&p, d);
    } else {
      if (c->sym >= 0x20000000u || c->arity > REWRITE_MAX_ARITY) return 0;
      *p++ = (u8)'C';
      acp_put_varint(&p, c->sym);
      acp_put_varint(&p, c->arity);
    }
    *pos = (u32)(p - g_sjp_scratch);
  }
  return 1;
}

// Pack `peak` renumbered through the dense map of the pair the queue
// will store: the map is built by first-occurrence walk over base_l
// then base_r (the exact numbering thvm_normalize_vars /
// acp_pack_fused / ftp_pack_fused assign), then the peak emit extends
// a copy for peak-only vars.  Returns a malloc'd byte string (caller
// owns) or NULL on any anomaly (degrade: no stash).
static u8 *atp_sj_peak_pack_term(Term peak, Term base_l, Term base_r) {
  if (peak == 0 || base_l == 0 || base_r == 0) return NULL;
  AcpNvMap m;
  m.n = 0u;
  if (!sjp_map_walk_term(base_l, &m)) return NULL;
  if (!sjp_map_walk_term(base_r, &m)) return NULL;
  u32 pos = 0u;
  if (!sjp_emit_term(peak, &m, &pos)) return NULL;
  u8 *buf = (u8 *)malloc(pos ? pos : 1u);
  if (buf == NULL) thvm_fatal("atp_sj_peak_pack_term: OOM");
  memcpy(buf, g_sjp_scratch, pos);
  return buf;
}
static u8 *atp_sj_peak_pack_ft(const AtpFtCell *peak,
                               const AtpFtCell *base_l,
                               const AtpFtCell *base_r) {
  if (peak == NULL || base_l == NULL || base_r == NULL) return NULL;
  AcpNvMap m;
  m.n = 0u;
  if (!sjp_map_walk_span(base_l, &m)) return NULL;
  if (!sjp_map_walk_span(base_r, &m)) return NULL;
  u32 pos = 0u;
  if (!sjp_emit_span(peak, &m, &pos)) return NULL;
  u8 *buf = (u8 *)malloc(pos ? pos : 1u);
  if (buf == NULL) thvm_fatal("atp_sj_peak_pack_ft: OOM");
  memcpy(buf, g_sjp_scratch, pos);
  return buf;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

