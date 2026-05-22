// thvm_atp_* - saturation loop state (stage 5.1).
//
// Heap-allocated AtpState plus init / free / add_equation / set_goal
// helpers.  The actual saturation step (thvm_atp_step) lands in 5.2;
// the priority-aware CP selection in 5.3; the recursive-descent
// rewriter feeding step 4 of the algorithm in 5.4.  This file just
// gives the loop a place to live.
//
// See docs/plans/saturation_loop.md for the algorithm.

// === 8.1c: ATP primitives registered into the TAG_PRI table ========
//
// `prim_unify_apply` is the first primitive: takes two terms (s, t),
// tries `thvm_unify`, and on success returns `thvm_unify_apply(s,
// &subst)` -- the unified term that both s and t collapse to under
// σ.  On failure, returns ERA so the surrounding APP-PRI structure
// short-circuits via APP-ERA when consumed by SUP-encoded CP
// enumeration in 8.1d.
//
// Linear matching: thvm_unify uses a stack-allocated RewriteSubst,
// so the primitive is reentrant and stateless w.r.t. the caller.
static Term prim_unify_apply(Term *args) {
  Term s = args[0];
  Term t = args[1];
  RewriteSubst subst = {{0}};
  if (!thvm_unify(s, t, &subst)) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  return thvm_unify_apply(s, &subst);
}

// 8.1e-ii: arity-3 variant.  Takes (s, t, target); returns
// `thvm_unify_apply(target, &subst)` where σ = mgu(s, t), or ERA
// if (s, t) fails to unify.  Lets the IC-routed CP enumerator
// build sigma from one pair and apply it to a different term --
// the workflow `thvm_critical_pairs_range` performs internally.
static Term prim_unify_apply3(Term *args) {
  Term s      = args[0];
  Term t      = args[1];
  Term target = args[2];
  RewriteSubst subst = {{0}};
  if (!thvm_unify(s, t, &subst)) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  return thvm_unify_apply(target, &subst);
}

// 8.2b: process-global KboConfig registry.  Pointers don't fit
// cleanly in a Term's `val` field, so IC code invokes the KBO
// comparator with a NUM-encoded cfg_id and the registry resolves
// the actual `KboConfig *` at fire time.
//
// `kbo_cfg_register` is idempotent for tests; saturation code
// typically calls it once during setup.  `kbo_cfg_get` returns
// NULL for unregistered ids so `prim_kbo` can return ERA
// defensively.
static const KboConfig *KBO_CFG_TABLE[KBO_CFG_TABLE_CAP];

fn u32 kbo_cfg_register(u32 cfg_id, const KboConfig *cfg) {
  if (cfg_id >= KBO_CFG_TABLE_CAP) return 0;
  KBO_CFG_TABLE[cfg_id] = cfg;
  return cfg_id;
}

fn const KboConfig *kbo_cfg_get(u32 cfg_id) {
  if (cfg_id >= KBO_CFG_TABLE_CAP) return NULL;
  return KBO_CFG_TABLE[cfg_id];
}

// 8.2b: arity-3 KBO primitive.  Takes (s, t, cfg_id_NUM); returns
// NUM(KboCmp) -- the 4-valued comparison result -- or ERA if
// cfg_id is bogus / no config registered.  Lets IC code invoke
// the KBO comparator from inside an APP-PRI chain.
static Term prim_kbo(Term *args) {
  Term s   = args[0];
  Term t   = args[1];
  Term cid = args[2];
  if (term_tag(cid) != TAG_NUM) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  const KboConfig *cfg = kbo_cfg_get((u32)term_val(cid));
  if (cfg == NULL) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  KboCmp r = thvm_kbo(s, t, cfg);
  return term_new(0, TAG_NUM, DT_INT32, (u64)r);
}

// 8.2c: pure-IC structural-equality on terms.  The C body handles
// the leaf cases (tag mismatch, FVR with same id, NUM with same
// val); for CTR with arity n it BUILDS an AND chain of n
// self-recursive APP-PRI calls and returns the unfired chain --
// the wnf reducer then evaluates the AND, firing each child
// comparison through APP-PRI saturation, short-circuiting on the
// first NUM(0).  This is "IC-driven control flow with C base
// cases" -- closer to the design memo's option (2) than option
// (3), but a real proof point that recursive structural code
// runs through our reducer end-to-end.
//
// Build a single recursive call: APP(APP(PRI(id), child_s), child_t).
static Term kbo_eq_build_call(Term cs, Term ct) {
  u64 l1 = heap_alloc(2);
  heap_set(l1 + 0, term_new_pri(ATP_PRIM_KBO_EQ_IC));
  heap_set(l1 + 1, cs);
  Term step1 = term_new(0, TAG_APP, 0, l1);

  u64 l2 = heap_alloc(2);
  heap_set(l2 + 0, step1);
  heap_set(l2 + 1, ct);
  return term_new(0, TAG_APP, 0, l2);
}

static Term prim_kbo_eq_ic(Term *args) {
  Term s = args[0];
  Term t = args[1];

  if (term_tag(s) != term_tag(t)) return term_new(0, TAG_NUM, DT_INT32, 0);
  if (term_ext(s) != term_ext(t)) return term_new(0, TAG_NUM, DT_INT32, 0);

  switch (term_tag(s)) {
    case TAG_FVR:
      // Same ext means same FVR id; equality follows.
      return term_new(0, TAG_NUM, DT_INT32, 1);

    case TAG_CTR: {
      u32 ns = term_ctr_n(s);
      u32 nt = term_ctr_n(t);
      if (ns != nt) return term_new(0, TAG_NUM, DT_INT32, 0);
      if (ns == 0) return term_new(0, TAG_NUM, DT_INT32, 1);

      // Build AND(c_0, AND(c_1, ..., c_{n-1})).  Right-fold so the
      // last child is the innermost; AND is right-strict so this
      // evaluates left-to-right from the reducer's perspective.
      Term chain = kbo_eq_build_call(term_ctr_at(s, ns - 1),
                                     term_ctr_at(t, ns - 1));
      for (u32 j = ns - 1; j > 0; j--) {
        u32 idx = j - 1;
        Term call_idx = kbo_eq_build_call(term_ctr_at(s, idx),
                                          term_ctr_at(t, idx));
        chain = term_new_and(call_idx, chain);
      }
      return chain;
    }

    default:
      return term_new(0, TAG_NUM, DT_INT32,
                      (term_val(s) == term_val(t)) ? 1 : 0);
  }
}

// 8.3b: IC-native rule dispatch primitive.  Takes `(lhs, rhs,
// target)`; runs `thvm_match` to bind LHS variables against the
// target; on success returns `thvm_subst_apply(rhs, &subst)` --
// the rewritten term -- on failure returns ERA.
//
// Equivalent to one step of the C-side `thvm_rewrite_step` at
// the top position.  Combined with APP-SUP fan-out (8.3c) it
// lets a SUP of partial-PRI rules dispatch in parallel.
static Term prim_rewrite_step(Term *args) {
  Term lhs    = args[0];
  Term rhs    = args[1];
  Term target = args[2];
  RewriteSubst subst = {{0}};
  if (!thvm_match(lhs, target, &subst)) {
    return term_new(0, TAG_ERA, 0, 0);
  }
  return thvm_subst_apply(rhs, &subst);
}

// Idempotent: tests / saturation init both call this; the registry
// just overwrites with the same function pointer.
static void atp_register_primitives(void) {
  prim_register(ATP_PRIM_UNIFY_APPLY,  prim_unify_apply,  2);
  prim_register(ATP_PRIM_UNIFY_APPLY3, prim_unify_apply3, 3);
  prim_register(ATP_PRIM_KBO,          prim_kbo,          3);
  prim_register(ATP_PRIM_KBO_EQ_IC,    prim_kbo_eq_ic,    2);
  prim_register(ATP_PRIM_REWRITE_STEP, prim_rewrite_step, 3);
}

// 8.3e-ii: IC-routed top-only rewrite try.  For each rule, builds
// APP(APP(APP(PRI(REWRITE_STEP), lhs_i), rhs_i), t) and reduces
// via wnf.  Returns the rewritten term on first non-ERA result;
// sets *fired = 1.  Otherwise *fired = 0 and returns t.
//
// Mirrors the C-side `rewrite_try_top` (in `src/rewrite/_.c`)
// with the per-rule matching going through APP-PRI evaluation
// instead of direct `thvm_match` calls.
static Term atp_ic_rewrite_try_top(Term t, const Term *lhs, const Term *rhs,
                               u32 n_rules, u8 *fired) {
  for (u32 i = 0; i < n_rules; i++) {
    u64 l1 = heap_alloc(2);
    heap_set(l1 + 0, term_new_pri(ATP_PRIM_REWRITE_STEP));
    heap_set(l1 + 1, lhs[i]);
    Term step1 = term_new(0, TAG_APP, 0, l1);

    u64 l2 = heap_alloc(2);
    heap_set(l2 + 0, step1);
    heap_set(l2 + 1, rhs[i]);
    Term step2 = term_new(0, TAG_APP, 0, l2);

    u64 l3 = heap_alloc(2);
    heap_set(l3 + 0, step2);
    heap_set(l3 + 1, t);
    Term step3 = term_new(0, TAG_APP, 0, l3);

    Term result = wnf(step3);
    if (term_tag(result) != TAG_ERA) {
      *fired = 1;
      return result;
    }
  }
  *fired = 0;
  return t;
}

// 8.3e-ii: IC-routed analog of `thvm_rewrite_step`.  Same outermost-
// leftmost strategy (try top, else descend into CTR children
// left-to-right); per-rule matching dispatches through APP-PRI
// evaluation via `prim_rewrite_step`.  Same outputs as the C path
// (parity-tested in `tests/test_atp.c`).
static Term atp_ic_rewrite_step(Term t, const Term *lhs, const Term *rhs,
                            u32 n_rules) {
  u8 fired = 0;
  Term r = atp_ic_rewrite_try_top(t, lhs, rhs, n_rules, &fired);
  if (fired) return r;

  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) return t;
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      Term original = children[i];
      Term rewritten = atp_ic_rewrite_step(original, lhs, rhs, n_rules);
      if (!kbo_eq(rewritten, original)) {
        children[i] = rewritten;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }

  return t;
}

// 8.3e-ii: IC-routed rewrite normalization.  Iterates
// `atp_ic_rewrite_step` until fixpoint or step_cap exhausted -- same
// shape as `thvm_rewrite_normalize` but with the per-step
// matching routed through APP-PRI.
static Term atp_rewrite_normalize_ic(Term t,
                                     const Term *lhs, const Term *rhs,
                                     u32 n_rules, u32 step_cap) {
  for (u32 i = 0; i < step_cap; i++) {
    Term t2 = atp_ic_rewrite_step(t, lhs, rhs, n_rules);
    if (kbo_eq(t, t2)) return t;
    t = t2;
  }
  return t;
}

#ifdef ATP_RULE_INDEX
// 7e lever 2: forward declaration -- the rule-LHS redex index and its
// indexed normalizer are defined further down (after the FV-index
// block, where the discrimination-tree skeleton lives); the shim
// below dispatches to it.
static Term atp_rewrite_normalize_indexed(AtpState *s, Term t, u32 step_cap);
// Preorder node count -- defined after atp_compare; the indexed
// normalizer needs it up here to size an incremental-flatten splice.
static u32 atp_symbol_count(Term t);
#ifdef ATP_ORDERED_REWRITE
// Opt-in flatterm fast-path for the mixed normalize loop; defined after
// the indexed normalizer + ordered helpers it builds on.
static Term atp_rewrite_normalize_flatterm_mixed(AtpState *s, Term t,
                                                 u32 step_cap);
#ifdef ATP_FLATTERM_SELFCHECK
static Term atp_rewrite_normalize_flatterm_selfcheck_tree(AtpState *s, Term t,
                                                          u32 step_cap);
#endif
#endif
#endif

// Throttled wall-deadline / host-abort poll for the inner rewrite
// loops.  goal-check normalizes with a 65536 step cap, and the mixed
// ordered path nests an indexed normalize inside its own 65536-step
// loop, so a single normalize can run far past MaxWallSeconds (or a
// host Abort[] / TimeConstrained[]) before thvm_atp_step's per-step
// check is ever reached again.  Defined after the wall-deadline
// machinery; forward-declared here for the normalizers above.  On a
// fire the caller returns the partial term and the next thvm_atp_step
// turns it into ATP_TIMEOUT / ATP_ABORTED.
static int atp_norm_deadline_fired(AtpState *s);

#ifdef ATP_ORDERED_REWRITE
// 9c-foundation: forward declaration -- the ordered normalizer is
// defined after atp_compare (it needs the reduction-order compare).
static Term atp_rewrite_normalize_ordered(AtpState *s, Term t,
                                          const Term *lhs, const Term *rhs,
                                          u32 n_rules, u32 step_cap);
#endif

// 8.3e-i: AtpState-aware shim.  Dispatches between the C-direct
// and IC-routed normalize paths based on s->use_ic_rewrite.
// Replaces direct `thvm_rewrite_normalize` calls in
// AtpState-internal callers (saturation step, goal-check,
// interreduce, joinability/connectedness filters).
//
// 7e lever 2: under -DATP_RULE_INDEX, a normalize call against the
// FULL current rule set (lhs == s->lhs && n_rules == s->n_rules --
// the hot `atp_cp_trivially_joinable` / saturation-step / goal-check
// path) routes to the rule-LHS discrimination index instead of
// `rewrite_try_top`'s linear scan.  Calls against any OTHER rule
// array (interreduce's 1-2 rule slice; the diag connectedness
// filter's filtered set) keep the linear scan -- the index reflects
// s->lhs[] only.  IC-routed rewriting (use_ic_rewrite) takes
// precedence, unchanged.
static Term atp_rewrite_normalize(AtpState *s, Term t,
                                  const Term *lhs, const Term *rhs,
                                  u32 n_rules, u32 step_cap) {
#ifdef ATP_ORDERED_REWRITE
  // 9c-foundation: proper unfailing-completion rewriting.  Supersedes
  // the indexed / IC / linear paths below (which all assume rules are
  // pre-oriented).  Needs s for the reduction-order comparison.
  if (s != NULL) {
    return atp_rewrite_normalize_ordered(s, t, lhs, rhs, n_rules, step_cap);
  }
#endif
  if (s != NULL && s->use_ic_rewrite) {
    return atp_rewrite_normalize_ic(t, lhs, rhs, n_rules, step_cap);
  }
#ifdef ATP_RULE_INDEX
  if (s != NULL && lhs == s->lhs && rhs == s->rhs && n_rules == s->n_rules) {
    return atp_rewrite_normalize_indexed(s, t, step_cap);
  }
#endif
  return thvm_rewrite_normalize(t, lhs, rhs, n_rules, step_cap);
}

// Grow the rule arrays (lhs / rhs / r_trace) to hold at least
// `need` entries.  No-op when capacity already suffices.  Doubles
// from the current capacity so amortized push cost stays O(1).
// New r_trace slots are filled with ATP_TRACE_NONE so a manually
// written rule that bypasses orient_and_add still has a defined
// trace index.  Aborts the process on OOM (matches heap_alloc's
// fatal policy -- there is no meaningful recovery for the caller).
static void atp_ensure_rule_cap(AtpState *s, u32 need) {
  if (need <= s->r_cap) return;
  u32 cap = s->r_cap ? s->r_cap : ATP_INIT_RULES;
  while (cap < need) cap *= 2;
  Term *nl = (Term *)realloc(s->lhs,      cap * sizeof(Term));
  Term *nr = (Term *)realloc(s->rhs,      cap * sizeof(Term));
  u32  *nt = (u32  *)realloc(s->r_trace,  cap * sizeof(u32));
  u8   *no = (u8   *)realloc(s->r_orient, cap * sizeof(u8));
  if (nl == NULL || nr == NULL || nt == NULL || no == NULL) {
    fprintf(stderr, "atp_ensure_rule_cap: realloc to %u rules failed\n",
            cap);
    exit(1);
  }
  s->lhs = nl; s->rhs = nr; s->r_trace = nt; s->r_orient = no;
  for (u32 i = s->r_cap; i < cap; i++) s->r_trace[i] = ATP_TRACE_NONE;
  s->r_cap = cap;
}

// Grow the CP arrays (cp_packed / cp_trace / cp_pri / cp_seq) to hold
// at least `need` entries.  Same doubling discipline as
// atp_ensure_rule_cap.  New cp_packed slots are NULL-initialised so
// thvm_atp_cp_set / thvm_atp_free can tell an unused slot apart from a
// live packed buffer.
static void atp_ensure_cp_cap(AtpState *s, u32 need) {
  if (need <= s->cp_cap) return;
  u32 cap = s->cp_cap ? s->cp_cap : ATP_INIT_CPS;
  while (cap < need) cap *= 2;
  u8  **nc = (u8 **)realloc(s->cp_packed, cap * sizeof(u8 *));
  u32  *nt = (u32  *)realloc(s->cp_trace, cap * sizeof(u32));
  u32  *np = (u32  *)realloc(s->cp_pri,   cap * sizeof(u32));
  u32  *nq = (u32  *)realloc(s->cp_seq,   cap * sizeof(u32));
  u32  *ng = (u32  *)realloc(s->cp_goal,  cap * sizeof(u32));
  if (nc == NULL || nt == NULL || np == NULL || nq == NULL || ng == NULL) {
    fprintf(stderr, "atp_ensure_cp_cap: realloc to %u CPs failed\n", cap);
    exit(1);
  }
  s->cp_packed = nc; s->cp_trace = nt;
  s->cp_pri = np; s->cp_seq = nq; s->cp_goal = ng;
  for (u32 i = s->cp_cap; i < cap; i++) {
    s->cp_packed[i] = NULL;
    s->cp_trace[i]  = ATP_TRACE_NONE;
  }
  s->cp_cap = cap;
}

// 7c': push one CP onto the binary min-heap CP queue.  Defined
// below (after atp_cp_priority); forward-declared here so the
// earlier add_equation push site can call it.
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace);

// Periodic full-rule-set CP-queue interreduction (Waldmeister
// KPV_KPMengeInterreduzieren).  Defined far below (it needs the
// normalizer + reheapify); forward-declared for the thvm_atp_step call.
// The period (run every Nth rule addition) lives here so the call site
// can name it.
#define ATP_CP_SET_IR_PERIOD 16u
static void atp_cp_set_interreduce(AtpState *s);

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

// Worst-case packed bytes for term `t`: 1 discriminator + 2 varints,
// each varint <= 10 bytes -> 21 bytes per node is a safe bound.
static u32 acp_packed_bound(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t), c = 21u;
      for (u32 i = 0; i < n; i++) c += acp_packed_bound(term_ctr_at(t, i));
      return c;
    }
    default: return 21u;
  }
}

// Append `t`'s preorder packing to `*pp`.  CTR: 'C', label, arity,
// then children; FVR: 'V', var id; NUM: 'N', dtype, raw value;
// anything else: 'E' (era placeholder -- ATP terms are first-order, so
// this is unreachable in practice but keeps unpack total).  `*nodes`
// is bumped once per node visited -- the pack walks every node anyway,
// so the CP's symbol count (its selection weight) falls out for free,
// sparing atp_cp_priority a second full traversal.
static void acp_pack_term(Term t, u8 **pp, u32 *nodes) {
  (*nodes)++;
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      *(*pp)++ = (u8)'C';
      acp_put_varint(pp, term_ext(t));
      acp_put_varint(pp, n);
      for (u32 i = 0; i < n; i++) acp_pack_term(term_ctr_at(t, i), pp, nodes);
      return;
    }
    case TAG_NUM:
      *(*pp)++ = (u8)'N';
      acp_put_varint(pp, term_ext(t));
      acp_put_varint(pp, term_val(t));
      return;
    case TAG_FVR:
      *(*pp)++ = (u8)'V';
      acp_put_varint(pp, term_ext(t));
      return;
    default:
      *(*pp)++ = (u8)'E';
      return;
  }
}

// Rebuild one preorder-packed term, advancing `*pp` past it.
static Term acp_unpack_term(const u8 **pp) {
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

// Pack a critical pair (lhs, rhs) into a fresh malloc'd byte string;
// `*out_len` receives its length, `*out_nodes` the total symbol count
// of lhs+rhs (the CP's selection weight).  Either out-param may be
// NULL.  The two terms pack back to back -- the preorder records are
// self-delimiting via arity, so acp_unpack reads lhs then rhs with no
// separator.
static u8 *acp_pack(Term lhs, Term rhs, u32 *out_len, u32 *out_nodes) {
  u32 bound = acp_packed_bound(lhs) + acp_packed_bound(rhs);
  u8 *buf = (u8 *)malloc(bound);
  if (buf == NULL) { fprintf(stderr, "acp_pack: OOM\n"); exit(1); }
  u8 *p = buf;
  u32 nodes = 0u;
  acp_pack_term(lhs, &p, &nodes);
  acp_pack_term(rhs, &p, &nodes);
  if (out_len   != NULL) *out_len   = (u32)(p - buf);
  if (out_nodes != NULL) *out_nodes = nodes;
  return buf;
}

// Inverse of acp_pack: rebuild both heap Terms from the byte string.
static void acp_unpack(const u8 *buf, Term *lhs, Term *rhs) {
  const u8 *p = buf;
  Term l = acp_unpack_term(&p);
  Term r = acp_unpack_term(&p);
  if (lhs != NULL) *lhs = l;
  if (rhs != NULL) *rhs = r;
}

// One-way match of a PACKED pattern term against a heap Term subject --
// the Stringterms counterpart of thvm_match (rewrite/_.c:27), and the
// Waldmeister technique of matching on the packed representation
// directly.  The pattern is never unpacked to a heap tree: the matcher
// walks the preorder byte string and the subject term in lockstep, so
// a head-symbol mismatch fast-fails after one discriminator byte with
// zero allocation.  This is what keeps the subsumption index off the
// per-candidate acp_unpack that an unpack-then-thvm_match would pay.
//
// `*pp` advances past the pattern term on a full match; on a mismatch
// it is left mid-record (the caller aborts the whole match, so the
// stale cursor is never used).  Verdict is bit-identical to
// thvm_match: a NUM / ERA pattern matches nothing (thvm_match's
// default branch returns 0); a variable id past the REWRITE_MAX_VAR
// matcher cliff fails; a repeated variable is confirmed with kbo_eq.
static u8 acp_match_term(const u8 **pp, Term subj, RewriteSubst *sub) {
  u8 disc = *(*pp)++;
  switch (disc) {
    case 'C': {
      u32 label = (u32)acp_get_varint(pp);
      u32 n     = (u32)acp_get_varint(pp);
      if (term_tag(subj) != TAG_CTR) return 0;
      if (term_ext(subj) != label)   return 0;
      if (term_ctr_n(subj) != n)     return 0;
      for (u32 i = 0; i < n; i++) {
        if (!acp_match_term(pp, term_ctr_at(subj, i), sub)) return 0;
      }
      return 1;
    }
    case 'V': {
      u32 id = (u32)acp_get_varint(pp);
      if (id >= REWRITE_MAX_VAR) return 0;
      if (sub->bindings[id] == 0) { sub->bindings[id] = subj; return 1; }
      return kbo_eq(sub->bindings[id], subj);
    }
    default:         // 'N' (NUM) / 'E' -- thvm_match's default: no match
      return 0;
  }
}

// Two-sided one-way match: does the packed CP `pack(plhs)++pack(prhs)`
// match (qlhs, qrhs) under a single shared substitution?  Equivalent
// to `thvm_match(plhs,qlhs,&s) && thvm_match(prhs,qrhs,&s)` with the
// pattern kept packed.  The lhs match advances the cursor exactly past
// `pack(plhs)` (preorder is self-delimiting), so the rhs match resumes
// at `pack(prhs)`.
static u8 acp_match_pair(const u8 *packed, Term qlhs, Term qrhs,
                         RewriteSubst *sub) {
  const u8 *p = packed;
  if (!acp_match_term(&p, qlhs, sub)) return 0;
  return acp_match_term(&p, qrhs, sub);
}

// One-time port self-check, run at engine init: a hand-built pair --
// nested CTRs, repeated variable, a NUM -- must survive a pack/unpack
// round-trip structurally intact, so a broken Stringterms port fails
// loudly and immediately rather than corrupting the CP queue later.
static void acp_selftest(void) {
  static u8 done = 0;
  if (done) return;
  done = 1;
  Term v0 = term_new_fvr(0u), v1 = term_new_fvr(1u);
  Term inner[2]  = { v0, v1 };
  Term f         = term_new_ctr(7u, inner, 2u);          // f(v0, v1)
  Term spine[2]  = { f, v0 };                            // var v0 repeats
  Term lhs       = term_new_ctr(7u, spine, 2u);          // f(f(v0,v1), v0)
  Term rhs       = term_new(0, TAG_NUM, 0u, 42u);        // NUM 42
  u32  len = 0;
  u8  *packed = acp_pack(lhs, rhs, &len, NULL);
  Term ul = 0, ur = 0;
  acp_unpack(packed, &ul, &ur);
  free(packed);
  if (!kbo_eq(lhs, ul) || !kbo_eq(rhs, ur)) {
    fprintf(stderr, "acp_selftest: Stringterms round-trip FAILED\n");
    exit(1);
  }
}

// Pack (lhs, rhs) into CP queue slot i, freeing any byte string
// already there.  Callers that build the queue directly -- chiefly
// tests -- use this instead of writing the (now packed) queue slots
// by hand.  The slot's priority / seq are filled by a subsequent
// thvm_atp_cp_reheapify, exactly as before the port.
fn void thvm_atp_cp_set(AtpState *s, u32 i, Term lhs, Term rhs) {
  if (s == NULL) return;
  atp_ensure_cp_cap(s, i + 1u);
  free(s->cp_packed[i]);                 // free(NULL) is a no-op
  s->cp_packed[i] = acp_pack(lhs, rhs, NULL, NULL);
}

// Unpack CP queue slot i back into two fresh transient heap Terms --
// the read counterpart of thvm_atp_cp_set.
fn void thvm_atp_cp_get(const AtpState *s, u32 i, Term *lhs, Term *rhs) {
  acp_unpack(s->cp_packed[i], lhs, rhs);
}

// === 8a: IC-native CP-set graph (-DATP_CP_GRAPH) ====================
//
// Under the flag the CP queue is ALSO held as one shared Term:
// `cp_graph = CpSet[Cp[l0,r0], Cp[l1,r1], ...]`, the leaves in the
// same slot order as the cp_packed[] queue.  Every
// CP mutation (heap push, select pop, heap reorder, reheapify)
// rebuilds cp_graph from the arrays so the two stay in lockstep; a
// debug assertion checks decode(cp_graph) equals the mirror.  This
// is a pure representation swap -- selection, priority, and search
// are the unchanged milestone-7 array engine.  8b makes cp_graph
// the thing reductions act on.
#ifdef ATP_CP_GRAPH

// Encode one CP as a 2-child `Cp[lhs,rhs]` CTR leaf.
static Term atp_cp_encode_leaf(Term lhs, Term rhs) {
  Term children[2] = { lhs, rhs };
  return term_new_ctr(ATP_CP_LABEL, children, 2);
}

// Decode a `Cp[lhs,rhs]` leaf back into its two terms.  Returns 1
// on a well-formed leaf, 0 otherwise.
static int atp_cp_decode_leaf(Term leaf, Term *lhs_out, Term *rhs_out) {
  if (term_tag(leaf) != TAG_CTR) return 0;
  if (term_ext(leaf) != ATP_CP_LABEL) return 0;
  if (term_ctr_n(leaf) != 2) return 0;
  *lhs_out = term_ctr_at(leaf, 0);
  *rhs_out = term_ctr_at(leaf, 1);
  return 1;
}

// Rebuild s->cp_graph from the cp_packed[] queue (each slot unpacked).
// Called at the end of every CP mutation so the graph stays in
// lockstep.  The container is a fresh CTR each rebuild, but the
// Cp[] leaves carry the already-hash-consed lhs/rhs cells, so two
// CPs sharing a subterm still share its heap cells.
static void atp_cp_graph_rebuild(AtpState *s) {
  if (s == NULL) return;
  if (s->n_cps == 0) {
    s->cp_graph = term_new_ctr(ATP_CPSET_LABEL, NULL, 0);
    return;
  }
  Term *leaves = (Term *)malloc((size_t)s->n_cps * sizeof(Term));
  if (leaves == NULL) {
    fprintf(stderr, "atp_cp_graph_rebuild: malloc for %u leaves failed\n",
            s->n_cps);
    exit(1);
  }
  for (u32 i = 0; i < s->n_cps; i++) {
    Term l = 0, r = 0;
    acp_unpack(s->cp_packed[i], &l, &r);
    leaves[i] = atp_cp_encode_leaf(l, r);
  }
  s->cp_graph = term_new_ctr(ATP_CPSET_LABEL, leaves, s->n_cps);
  free(leaves);
}

// Debug assertion: decode(cp_graph) must equal the cp_packed[] queue
// (unpacked), slot for slot.  Run after every mutation so a drift
// between the two representations aborts immediately rather than
// corrupting a proof silently.
static void atp_cp_graph_assert(const AtpState *s) {
  if (s == NULL) return;
  assert(term_tag(s->cp_graph) == TAG_CTR
         && "cp_graph must be a CTR");
  assert(term_ext(s->cp_graph) == ATP_CPSET_LABEL
         && "cp_graph must carry the CpSet label");
  assert(term_ctr_n(s->cp_graph) == s->n_cps
         && "cp_graph leaf count must equal n_cps");
  for (u32 i = 0; i < s->n_cps; i++) {
    Term gl = 0, gr = 0;
    int ok = atp_cp_decode_leaf(term_ctr_at(s->cp_graph, i), &gl, &gr);
    assert(ok && "cp_graph child must be a well-formed Cp[lhs,rhs] leaf");
    Term cl = 0, cr = 0;
    acp_unpack(s->cp_packed[i], &cl, &cr);
    assert(kbo_eq(gl, cl)
           && "cp_graph leaf lhs must equal cp_packed[] mirror");
    assert(kbo_eq(gr, cr)
           && "cp_graph leaf rhs must equal cp_packed[] mirror");
  }
}

// Maintain cp_graph after a CP mutation: rebuild + assert lockstep.
static void atp_cp_graph_sync(AtpState *s) {
  atp_cp_graph_rebuild(s);
  atp_cp_graph_assert(s);
}

// === 8b: shared whole-graph CP normalization ========================
//
// Today (8a) normalization is lazy: thvm_atp_step rewrites only the
// CP it pops.  8b adds atp_normalize_graph -- when a rule is oriented
// it normalizes EVERY CP term in cp_graph in ONE sweep that threads a
// single `input cell -> normal-form cell` memo across all CPs.
//
// thvm hash-conses every cell, so a subterm shared by k CPs is the
// SAME Term value in all k.  The memo is keyed by that Term value, so
// the shared subterm's normal form is computed once total instead of
// once per CP -- this cross-CP memo IS the optimal-sharing win.  Per
// the spec target, per-step normalization cost drops from
// O(n_cps * |term|) toward O(distinct redexes).
//
// This is a deliberate SEMANTIC change (not bit-identical to 8a):
// eagerly normalizing queued CPs changes which become trivially
// joined when.  The memoized normalizer is bottom-up + top-fixpoint
// (innermost then the existing outermost-leftmost thvm_rewrite_step
// loop) -- it reaches a normal form under R just as the lazy path
// does; for the confluent rule sets KB completion drives toward, the
// normal form is the same one.

// Open-addressing Term -> Term memo for one normalization sweep.
// 0 is not a valid Term, so a 0 key marks an empty slot.
typedef struct {
  Term *keys;
  Term *vals;
  u32   cap;     // power of two
  u32   count;
} AtpNormMemo;

static void atp_norm_memo_init(AtpNormMemo *m, u32 hint) {
  u32 cap = 64;
  while (cap < hint * 2u) cap *= 2u;
  m->keys  = (Term *)calloc(cap, sizeof(Term));
  m->vals  = (Term *)calloc(cap, sizeof(Term));
  m->cap   = cap;
  m->count = 0;
  if (m->keys == NULL || m->vals == NULL) {
    fprintf(stderr, "atp_norm_memo_init: calloc for %u slots failed\n", cap);
    exit(1);
  }
}

static void atp_norm_memo_free(AtpNormMemo *m) {
  free(m->keys);
  free(m->vals);
  m->keys = NULL;
  m->vals = NULL;
}

// 64-bit mix (splitmix64 finalizer) -- a Term is a packed u64, so
// hashing the whole word spreads tag/ext/val bits across the table.
static u64 atp_term_hash(Term t) {
  u64 x = (u64)t;
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27; x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return x;
}

static void atp_norm_memo_grow(AtpNormMemo *m);

// Look up t; returns 1 and sets *out if present.
static int atp_norm_memo_get(const AtpNormMemo *m, Term t, Term *out) {
  u32 mask = m->cap - 1u;
  u32 i = (u32)atp_term_hash(t) & mask;
  for (;;) {
    Term k = m->keys[i];
    if (k == 0) return 0;
    if (k == t) { *out = m->vals[i]; return 1; }
    i = (i + 1u) & mask;
  }
}

static void atp_norm_memo_put(AtpNormMemo *m, Term t, Term v) {
  if ((m->count + 1u) * 4u >= m->cap * 3u) atp_norm_memo_grow(m);
  u32 mask = m->cap - 1u;
  u32 i = (u32)atp_term_hash(t) & mask;
  for (;;) {
    Term k = m->keys[i];
    if (k == 0) { m->keys[i] = t; m->vals[i] = v; m->count++; return; }
    if (k == t) { m->vals[i] = v; return; }
    i = (i + 1u) & mask;
  }
}

static void atp_norm_memo_grow(AtpNormMemo *m) {
  u32   old_cap  = m->cap;
  Term *old_keys = m->keys;
  Term *old_vals = m->vals;
  m->cap  *= 2u;
  m->keys  = (Term *)calloc(m->cap, sizeof(Term));
  m->vals  = (Term *)calloc(m->cap, sizeof(Term));
  m->count = 0;
  if (m->keys == NULL || m->vals == NULL) {
    fprintf(stderr, "atp_norm_memo_grow: calloc for %u slots failed\n", m->cap);
    exit(1);
  }
  for (u32 i = 0; i < old_cap; i++) {
    if (old_keys[i] != 0) atp_norm_memo_put(m, old_keys[i], old_vals[i]);
  }
  free(old_keys);
  free(old_vals);
}

#ifdef ATP_NORM_STATS
// 8b: instrumentation -- memo hits vs misses, summed over a run.  A
// hit means a subterm shared by an already-visited CP: that node's
// normal form was reused instead of recomputed.  hits/(hits+misses)
// is the optimal-sharing ratio.  g_atp_norm_secs accumulates the
// wall time spent inside atp_normalize_graph so the sweep cost can be
// reported as a fraction of total runtime.
#include <time.h>
static u64    g_atp_norm_hits   = 0;
static u64    g_atp_norm_misses = 0;
static double g_atp_norm_secs   = 0.0;
#endif

// Memoized normal form of t under (lhs, rhs).  Bottom-up: each child
// is normalized through the SAME memo first (so a shared subterm is
// done once), the children-normalized term is rebuilt, then the
// existing outermost-leftmost thvm_rewrite_step loop runs to fixpoint
// at the top.  The memo is keyed by the input cell so every CP that
// carries that cell reuses the result for free.
static Term atp_norm_memo(AtpNormMemo *m, Term t,
                          const Term *lhs, const Term *rhs,
                          u32 n_rules, u32 step_cap) {
  Term cached = 0;
  if (atp_norm_memo_get(m, t, &cached)) {
#ifdef ATP_NORM_STATS
    g_atp_norm_hits++;
#endif
    return cached;
  }
#ifdef ATP_NORM_STATS
  g_atp_norm_misses++;
#endif

  Term cur = t;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n <= REWRITE_MAX_ARITY) {
      Term children[REWRITE_MAX_ARITY];
      int changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term ci = term_ctr_at(t, i);
        Term ni = atp_norm_memo(m, ci, lhs, rhs, n_rules, step_cap);
        children[i] = ni;
        if (ni != ci) changed = 1;
      }
      if (changed) cur = term_new_ctr(term_ext(t), children, n);
    }
  }

  // Outermost-leftmost fixpoint at the top -- identical loop shape to
  // thvm_rewrite_normalize, just applied after the children settled.
  for (u32 i = 0; i < step_cap; i++) {
    Term t2 = thvm_rewrite_step(cur, lhs, rhs, n_rules);
    if (kbo_eq(cur, t2)) break;
    cur = t2;
  }

  atp_norm_memo_put(m, t, cur);
  return cur;
}

// 8b: simplify every CP term in cp_graph in one shared sweep.  Called
// from thvm_atp_step once a rule is oriented, with `added` = the
// just-oriented rule range.
//
// Why only the new rule(s), not full R: every queued CP was already
// normalized under R BEFORE this step -- old CPs were normalized by an
// earlier sweep, fresh CPs are trivial-join-filtered against full R in
// atp_push_cps_traced.  Orienting one rule changes R by exactly that
// rule, so the queue only needs that rule (the unfailing fallback may
// add two) applied to reach normal form under R-new.  Re-running all
// of R against all n_cps CPs every step is the O(n_cps*|term|*n_rules)
// trap; restricting to the new rules makes the per-step sweep
// O(n_cps*|term|*n_new), n_new <= 2.
//
// The shared memo is the optimal-sharing win WITHIN the sweep: a
// subterm common to many CPs is the SAME hash-consed Term, so its
// rewrite under the new rule is computed once and reused across every
// CP carrying it -- cost is O(distinct subterm cells), not
// O(sum of CP sizes).  A CP untouched by the new rule memo-resolves to
// itself after one traversal and stays put.
//
// Trivially-joined CPs (both sides converge under the new rule) drop
// out here -- 8c will route that through an Eql[x,x] -> ERA
// reflexivity rule during the sweep; for 8b the kbo_eq check does the
// same pruning.  Repacks the cp_packed[] queue, then reheapify
// recomputes priorities and rebuilds cp_graph + the heap.
static void atp_normalize_graph(AtpState *s, AtpAddedRange added) {
  if (s == NULL || s->n_cps == 0 || added.count == 0) return;
  const u32 NORM_CAP = 64;
#ifdef ATP_NORM_STATS
  clock_t g_t0 = clock();
#endif

  // The newly-oriented rules.  added.first/.count index s->lhs/s->rhs;
  // copy by value so a later compaction can't move them under us.
  u32 n_new = added.count;
  if (n_new > 2) n_new = 2;
  Term new_lhs[2], new_rhs[2];
  for (u32 k = 0; k < n_new; k++) {
    new_lhs[k] = s->lhs[added.first + k];
    new_rhs[k] = s->rhs[added.first + k];
  }

  AtpNormMemo memo;
  atp_norm_memo_init(&memo, s->n_cps * 4u);

  u32 w = 0;
  int touched = 0;   // any CP term rewritten or dropped this sweep?
  for (u32 i = 0; i < s->n_cps; i++) {
    Term ol = 0, orr = 0;
    acp_unpack(s->cp_packed[i], &ol, &orr);
    Term l = atp_norm_memo(&memo, ol,  new_lhs, new_rhs, n_new, NORM_CAP);
    Term r = atp_norm_memo(&memo, orr, new_lhs, new_rhs, n_new, NORM_CAP);
    // Trivially-joined: both sides converged.  Drop the CP -- it adds
    // no equational consequence.
    if (kbo_eq(l, r)) {
      s->n_cps_dropped_joinable++;
      free(s->cp_packed[i]);
      s->cp_packed[i] = NULL;
      touched = 1;
      continue;
    }
    if (l != ol || r != orr) {
      // CP rewritten -- repack into a fresh byte string at slot w.
      free(s->cp_packed[i]);
      s->cp_packed[i] = NULL;
      s->cp_packed[w] = acp_pack(l, r, NULL, NULL);
      touched = 1;
    } else if (w != i) {
      // Unchanged -- compact the existing buffer down to slot w.
      s->cp_packed[w] = s->cp_packed[i];
      s->cp_packed[i] = NULL;
    }
    s->cp_trace[w] = s->cp_trace[i];
    // Carry the insertion age down to the compacted slot so a
    // cp_fifo_tiebreak reheapify can preserve it (Waldmeister w1=fifo);
    // harmless otherwise (reheapify overwrites cp_seq when the flag
    // is off).
    s->cp_seq[w] = s->cp_seq[i];
    w++;
  }
  s->n_cps = w;

  atp_norm_memo_free(&memo);

  // The common case: the new rule(s) reduced no queued CP -- the queue
  // was already in normal form under R-new (newly-pushed CPs are
  // trivial-join-filtered against full R in atp_push_cps_traced, old
  // CPs were swept under every earlier rule).  Then cp_pri / heap
  // order / cp_graph are all still valid; skip the O(n_cps) rebuild.
  // Only when a CP actually changed or dropped does reheapify run --
  // it recomputes cp_pri/cp_seq and rebuilds cp_graph from the mirror.
  if (touched) {
    thvm_atp_cp_reheapify(s);
  }
#ifdef ATP_NORM_STATS
  g_atp_norm_secs += (double)(clock() - g_t0) / CLOCKS_PER_SEC;
#endif
}

#ifdef ATP_NORM_STATS
// 8b instrumentation accessor: total memoized-normalize node visits
// that hit vs missed the shared memo, summed over the run.  Let a
// bench print the optimal-sharing ratio.
fn void thvm_atp_norm_stats(u64 *hits, u64 *misses, double *secs) {
  if (hits   != NULL) *hits   = g_atp_norm_hits;
  if (misses != NULL) *misses = g_atp_norm_misses;
  if (secs   != NULL) *secs   = g_atp_norm_secs;
}
#endif

// === 8e: shared-traversal multi-match (the 91%-killer) ==============
//
// Milestone 7 located the wall: atp_cp_queue_subsumed scans EVERY
// queued CP (~64k) calling thvm_match per leaf -- 7b measured ~16M
// match calls / step, 91% of runtime.  The scan asks, for a freshly
// generated candidate CP (lhs, rhs): is there a queued CP (qs, qt)
// and a substitution sigma with (lhs, rhs) = (sigma qs, sigma qt)?
// The QUEUED CP is the PATTERN (it carries the FVR variables); the
// CANDIDATE is the SUBJECT.  This is FORWARD subsumption -- the new
// candidate is subsumed by an existing queued CP and dropped before
// it ever reaches the queue.
//
// 8e routes that scan through ONE thvm_match_multi traversal of
// cp_graph instead of an explicit O(n_cps) loop.  cp_graph is the
// flat CTR `CpSet[Cp[qs0,qt0], Cp[qs1,qt1], ...]`; the CpSet
// container is the fan-out point -- thvm_match_multi forks over its
// n leaves against the one shared (lhs, rhs) subject.
//
// THE THESIS-FAILING RESULT -- documented honestly, per the plan's
// "8e's payoff depends on CPs actually sharing subterms ... report
// it honestly" instruction.
//
// 8e's intended win was a (pattern_cell, subject_cell) -> match
// memo: a subterm shared by many CPs would be matched against a
// given subject subterm once.  That memo was BUILT, INSTRUMENTED
// (-DATP_MATCH_STATS) and MEASURED.  Verdict on the Wolfram axiom
// (cpl1): 0.0% memo hit rate -- 0 hits over 7.7M subterm-pair
// lookups, three independent measurements.  Diagnosis:
//
//  - thvm has a BUMP allocator, NOT a hash-cons table.  term_new_ctr
//    (src/term/new_ctr.c) always heap_allocs a fresh cell, so two
//    structurally-equal CTR subterms are the SAME Term value ONLY
//    when the literal same cell is reused.  The plan's premise
//    "thvm hash-conses every cell" is factually wrong.
//  - The memo key is the (pattern, subject) cell PAIR.  A hit needs
//    BOTH cells to recur together.  On the flat CpSet every CP leaf
//    has a DISTINCT top cell, so thvm_match fails fast at the leaf
//    root -- the discrimination happens at the root, before the
//    traversal ever descends to any deep subterm CPs might share.
//  - A discrimination tree shares term PREFIXES (the root-anchored
//    test).  A flat hash-consed DAG of disjoint-headed CPs shares
//    term SUBTERMS but no prefix.  The flat CpSet provides zero
//    prefix sharing for a root-anchored match -- so the fan-out
//    re-traverses the subject in full per leaf.  Real prefix
//    sharing needs the SUP fan-out CONTAINER of workstream 8f, not
//    the flat 8a container.
//
// The memo, with its per-node hash + probe + partial-subst
// bookkeeping, was pure overhead at 0% hits -- it ran cpl1 ~16x
// SLOWER than 8b.  Per the plan's "degrades gracefully to the
// per-CP scan -- no worse than today" requirement, 8e ships WITHOUT
// the memo: thvm_match_multi is the fan-out over plain thvm_match,
// which IS the per-CP scan -- behavior-identical and 8b-cost.  8e
// is the milestone go/no-go signal and the signal is: the
// flat-CpSet shared-traversal does not beat the scan; revisit at 8f
// with the SUP container that actually shares prefixes.

#ifdef ATP_MATCH_STATS
#include <time.h>
static u64    g_atp_mm_calls       = 0;  // thvm_match_multi invocations
static u64    g_atp_mm_node_visits = 0;  // CP leaves walked
static u64    g_atp_mm_memo_hits   = 0;  // structurally 0 -- no memo
static u64    g_atp_mm_memo_miss   = 0;  // thvm_match calls issued
static double g_atp_mm_secs        = 0.0;
#endif

// 8e: forward subsumption of candidate (lhs, rhs) against the whole
// queued CP set in ONE traversal of cp_graph -- the replacement for
// atp_cp_queue_subsumed's explicit O(n_cps) thvm_match loop.
//
// `graph` is cp_graph: `CpSet[Cp[qs,qt], ...]`.  The CpSet container
// is the fan-out point: thvm_match_multi forks over its n Cp[] leaves
// against the one shared (lhs, rhs) subject.  Each leaf runs the same
// two-sided match the per-CP scan did -- forward (sigma qs = lhs AND
// sigma qt = rhs, one sigma threaded through both) then symmetric --
// via plain thvm_match.  Returns 1 on the first subsuming leaf.
//
// The verdict is IDENTICAL to the per-CP loop, leaf for leaf: 8e is
// a routing change, not a semantic one.  The (P,S) memo that was
// meant to share per-subterm work is absent BY MEASUREMENT, not
// oversight -- see the block comment above.
static u8 thvm_match_multi(Term graph, Term lhs, Term rhs) {
#ifdef ATP_MATCH_STATS
  g_atp_mm_calls++;
#endif
  if (term_tag(graph) != TAG_CTR) return 0;
  if (term_ext(graph) != ATP_CPSET_LABEL) return 0;
  u32 n = term_ctr_n(graph);
  for (u32 k = 0; k < n; k++) {
#ifdef ATP_MATCH_STATS
    g_atp_mm_node_visits++;
#endif
    Term qs = 0, qt = 0;
    if (!atp_cp_decode_leaf(term_ctr_at(graph, k), &qs, &qt)) continue;
    // Forward: sigma qs = lhs AND sigma qt = rhs, one sigma threaded
    // through both matches (equational subsumption).
    {
      RewriteSubst subst = {{0}};
#ifdef ATP_MATCH_STATS
      g_atp_mm_memo_miss += 2u;
#endif
      if (thvm_match(qs, lhs, &subst) &&
          thvm_match(qt, rhs, &subst)) {
        return 1;
      }
    }
    // Symmetric: sigma qs = rhs AND sigma qt = lhs.
    {
      RewriteSubst subst = {{0}};
#ifdef ATP_MATCH_STATS
      g_atp_mm_memo_miss += 2u;
#endif
      if (thvm_match(qs, rhs, &subst) &&
          thvm_match(qt, lhs, &subst)) {
        return 1;
      }
    }
  }
  return 0;
}

#ifdef ATP_MATCH_STATS
// 8e instrumentation accessor: thvm_match_multi calls, CP leaves
// walked, and thvm_match calls issued.  memo_hits is structurally 0
// -- 8e ships memo-free (the (P,S) memo measured 0% on the flat
// CpSet; see the 8e block comment).  Kept so the bench can report
// the scan size and confirm thvm_match is still the hot spot.
fn void thvm_atp_match_stats(u64 *calls, u64 *node_visits,
                             u64 *memo_hits, u64 *memo_miss,
                             double *secs) {
  if (calls       != NULL) *calls       = g_atp_mm_calls;
  if (node_visits != NULL) *node_visits = g_atp_mm_node_visits;
  if (memo_hits   != NULL) *memo_hits   = g_atp_mm_memo_hits;
  if (memo_miss   != NULL) *memo_miss   = g_atp_mm_memo_miss;
  if (secs        != NULL) *secs        = g_atp_mm_secs;
}
#endif

#else  // !ATP_CP_GRAPH -- the milestone-7 array engine, byte-for-byte.

// No-op so mutation sites carry one unconditional call site instead
// of #ifdef'd blocks; the compiler elides it with the flag off.
static inline void atp_cp_graph_sync(AtpState *s) { (void)s; }

#endif // ATP_CP_GRAPH

// === 7d: CP-queue subsumption index (-DATP_FV_INDEX) ===============
//
// THE WALL.  7b profiling pinned ~91% of completion runtime on
// `thvm_match`, every call under `atp_push_cps_traced` ->
// `atp_cp_queue_subsumed`.  That function asks, for each freshly
// generated candidate CP (lhs, rhs): is there a queued CP (qs, qt)
// and a substitution sigma with (lhs, rhs) = (sigma qs, sigma qt)
// (forward) or = (sigma qt, sigma qs) (symmetric)?  The milestone-7
// engine answers it with a flat O(n_cps) loop -- on the deep Wolfram
// axiom, n_cps climbs to ~64k and the loop issues ~16M recursive
// `thvm_match` calls per step.  The query almost always answers "no"
// (over a 200-step cpl1 run only ONE CP is ever queue-subsumed), so
// the cost is entirely in proving the negative -- ruling out every
// queued CP.
//
// Milestone 8 bet thvm's structural sharing would let the CP set act
// as a free discrimination tree.  8e REFUTED that: thvm has a bump
// allocator (src/heap/alloc.c), NOT hash-consing -- a fresh
// subsumption query shares no cells with stored CPs, and the match is
// root-anchored, so the flat-CpSet shared traversal is exactly the
// per-CP scan.  7d is the proven fix every serious completion prover
// uses: a real term index.
//
// WHY A DISCRIMINATION TREE, NOT A FEATURE VECTOR.  7d was first
// built as a feature-vector (FV) index -- the structure the plan
// recommended -- on cheap monotone integer features (symbol count,
// per-depth CTR profile, term depth) where a more-general term is
// componentwise <=.  It was sound, GC-trivial, and MEASURED: on the
// single-symbol Wolfram nand axiom it plateaued at ~47% false-
// positive survival (18.8k of 40.2k queued CPs surviving the filter
// per query) and adding depth-profile features did not move it.  The
// reason is structural: a CP whose one side is a bare variable -- a
// large fraction of the queue -- has the size profile of its other
// side alone, so its FV dominates almost every larger CP's FV.  A
// size-based FV simply cannot exclude a small term that "could
// generalize" a large one by shape but does not.
//
// Excluding by SHAPE needs a position-keyed symbol test, and that is
// exactly a discrimination tree.  The plan permitted the deviation
// "with a strong reason, justified against the GC-stability point".
// The reason: the measured FV plateau.  The GC point still holds --
// the 8b worry was MOVING CELL POINTERS in the index.  This tree is
// keyed entirely on integer LABEL ids (a CTR's label, or a wildcard
// marker for a variable); label ids are not heap addresses and do
// not move under the Cheney collector.  The only Term-valued storage
// is each leaf record's (lhs, rhs) mirror, rooted in
// thvm_atp_gc_collect.  So the index is as GC-trivial as the FV trie
// was -- the flag is still spelled -DATP_FV_INDEX.
//
// THE STRUCTURE -- a PERFECT discrimination tree.  The tree spans
// the PREORDER traversal of the CP viewed as one synthetic term
// `Cp(lhs, rhs)` (a binary node `Cp` so one tree covers both sides).
// A plain discrimination tree treats every variable as one wildcard
// `*`; that was MEASURED and plateaued at ~47% retrieval, because
// the deep nand-trees have many REPEATED variables and a one-`*`
// tree cannot tell `nand(x,x)` from `nand(x,y)`.  This is the
// PERFECT variant: it numbers a pattern's variables by first-
// appearance order, so `nand(x,x)` flattens to `nand *0 *0` and
// `nand(x,y)` to `nand *0 *1`.  Each tree edge is keyed on a flat
// symbol:
//   ATP_DT_NUM             a TAG_NUM atom
//   ATP_DT_STAR_BASE + k   the k-th DISTINCT pattern variable
//   ATP_DT_CTR_BASE  + lab a TAG_CTR with label `lab`
//
// INSERT renumbers the stored CP's variables (first occurrence of a
// var -> the next free k) and walks the renumbered preorder string,
// descending / creating an edge per symbol; the node reached after
// the whole string gets a leaf record.
//
// RETRIEVAL ("find every stored pattern that one-way MATCHES subject
// T") flattens T to a preorder subterm array and walks it in
// lockstep with the tree, carrying a binding array
// `star_bind[k] -> subject subterm`.  At the tree node for the
// current subject subterm `t`:
//   - a CTR edge equal to `t`'s own label -- follow it; `t`'s
//     children are the next preorder positions.
//   - a STAR_BASE+k edge -- the k-th pattern variable:
//       * k unbound  -> bind star_bind[k] := t, descend past t's
//                       whole subtree, unbind on backtrack;
//       * k bound    -> follow only if kbo_eq(star_bind[k], t)
//                       (the variable's earlier occurrence pinned
//                       a value; a repeat must equal it), descend
//                       past t's subtree.
// Every other edge is pruned.  This folds full one-way matching --
// structure AND variable consistency -- into the descent: a stored
// CP reaches a leaf IFF it matches T.  The leaf still runs the SAME
// two-sided thvm_match for byte-identical verdicts (and as a guard),
// but it now essentially always confirms.
//
// SOUNDNESS (never misses a subsumer).  thvm_match(pattern, subject)
// succeeds iff at every preorder position the pattern has a CTR
// equal to the subject's there, or a variable whose every occurrence
// binds a kbo_eq subterm.  The descent above follows exactly the
// edge that case takes -- CTR-equal, first-var-bind, or repeat-var-
// kbo_eq -- so it reaches a subsuming CP's leaf and never prunes it.
// The symmetric orientation is covered by a second retrieval over
// `Cp(rhs,lhs)`.  The tree's verdict is therefore identical to the
// array scan, CP for CP: same drops, same proof, same step/CP
// counts.

#ifdef ATP_FV_INDEX

// Flat-symbol alphabet for a perfect-discrimination-tree edge.
// Ordering matters: NUM < every STAR(k) < every CTR(lab), so the
// sym-ascending child list lets a descent stop scanning early.
#define ATP_DT_NUM        0u                 // TAG_NUM atom
#define ATP_DT_MAXVARS    64u                // distinct vars per CP
#define ATP_DT_STAR_BASE  1u                 // STAR(k) = BASE + k
#define ATP_DT_CTR_BASE   (ATP_DT_STAR_BASE + ATP_DT_MAXVARS)
#define ATP_DT_NIL        0xFFFFFFFFu

// Preorder-flattened subject cap.  Sized to hold a deep-saturation
// critical pair: at thm step ~230 a ~1% tail of queued CPs flattens
// past the old 4096, and each such query spilled to the O(n_recs)
// full scan -- ~500M thvm_match calls, the late-game wall.  32768
// keeps that tail on the perfect-tree descent; the descent's STAR
// recursion is then bounded by half the cap (atp_dt_descend loops the
// CTR spine), well within the stack.  A still-bigger term aborts to
// the full scan (correct, never a silent under-retrieval).
#define ATP_DT_FLAT_CAP   32768u

// Per-term variable renumbering: maps a raw TAG_FVR id to its
// first-appearance index 0,1,2,...  `slot[id]` holds (index+1), 0 =
// not yet seen.  Reset per CP at insert and per orientation at
// retrieval.
typedef struct { u32 slot[REWRITE_MAX_VAR]; u32 n; u8 folded; } AtpDtVarMap;

static void atp_dt_varmap_reset(AtpDtVarMap *vm) {
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) vm->slot[i] = 0;
  vm->n = 0;
  vm->folded = 0;
}

// First-appearance index of variable id `vid`.  Ids >= REWRITE_MAX_VAR,
// or more than ATP_DT_MAXVARS distinct vars, fold onto the last slot --
// sound (folding only coarsens the tree, never drops a candidate) but
// it makes the descent inexact: `folded` records that so the leaf
// keeps the confirming thvm_match.  folded == 0 -> the descent is an
// exact subsumption proof.
static u32 atp_dt_var_index(AtpDtVarMap *vm, u32 vid) {
  if (vid >= REWRITE_MAX_VAR) { vid = REWRITE_MAX_VAR - 1u; vm->folded = 1u; }
  if (vm->slot[vid] == 0) {
    u32 idx;
    if (vm->n < ATP_DT_MAXVARS) { idx = vm->n; vm->n++; }
    else                       { idx = ATP_DT_MAXVARS - 1u; vm->folded = 1u; }
    vm->slot[vid] = idx + 1u;
  }
  return vm->slot[vid] - 1u;
}

// flatsym of one term node under variable renumbering `vm`.
static u32 atp_dt_flatsym(Term t, AtpDtVarMap *vm) {
  switch (term_tag(t)) {
    case TAG_CTR: return ATP_DT_CTR_BASE + term_ext(t);
    case TAG_NUM: return ATP_DT_NUM;
    case TAG_FVR:
    default:      return ATP_DT_STAR_BASE + atp_dt_var_index(vm, term_ext(t));
  }
}

// A discrimination-tree node: a left-child / right-sibling tree in a
// flat realloc-grown pool addressed by u32 INDEX (no pointers, so a
// pool realloc never invalidates the structure).  `sym` is the flat
// symbol of the edge INTO this node.  `rec_head` is the head of this
// node's leaf record list.
typedef struct {
  u32 sym;        // flat symbol of the in-edge
  u32 child;      // first child node index, or ATP_DT_NIL
  u32 sibling;    // next sibling node index, or ATP_DT_NIL
  u32 rec_head;   // first record index, or ATP_DT_NIL
} AtpDtNode;

// One indexed CP.  `packed` BORROWS the queue's cp_packed[] byte
// string for this CP -- the queue owns and frees it; the index only
// reads it (and only while `live`).  Storing the packed pointer, not a
// pair of heap Terms, keeps the collector out of the index entirely.
// `live` is cleared when the CP is popped / dropped -- a dead record
// is skipped by retrieval (its `packed` may by then dangle, but a dead
// record is never dereferenced) and reclaimed by the next index
// rebuild.  `seq` is the CP's stable id (the seq->record map key);
// `next` links the leaf list.
typedef struct {
  u8  *packed;
  u32  seq;
  u32  next;
  u8   live;
  u8   folded;    // this CP folded a var -> descent inexact for it
} AtpDtRec;

// seq -> record-index open-addressing hash entry (NIL = empty).
typedef struct { u32 seq; u32 rec; } AtpDtSeqEnt;

// The index.  Named `struct AtpFvIndex` -- the flag and the opaque
// thvm.h forward declaration are spelled that way; the structure
// inside is the discrimination tree the measurement settled on.
struct AtpFvIndex {
  AtpDtNode   *nodes;
  u32          n_nodes, cap_nodes;
  AtpDtRec    *recs;
  u32          n_recs, cap_recs;        // n_recs == GC-rooted span
  u32          n_live;                  // live record count (== n_cps)
  AtpDtSeqEnt *seqmap;                   // seq -> rec index
  u32          seqmap_cap;               // power of two
  u32          root;                     // tree root node index
  // Instrumentation (cheap counters, always compiled).
  u64 q_calls;            // atp_cp_queue_subsumed queries
  u64 q_candidates;       // leaf records reached by retrieval
  u64 q_matchcalls;       // thvm_match calls issued on candidates
  u64 q_nodevisits;       // discrimination-tree nodes touched
};
typedef struct AtpFvIndex AtpFvIndex;

static u32 atp_dt_node_new(AtpFvIndex *ix, u32 sym) {
  if (ix->n_nodes == ix->cap_nodes) {
    u32 cap = ix->cap_nodes ? ix->cap_nodes * 2u : 1024u;
    AtpDtNode *p = (AtpDtNode *)realloc(ix->nodes, cap * sizeof(AtpDtNode));
    if (p == NULL) { fprintf(stderr, "atp_dt: node pool OOM\n"); exit(1); }
    ix->nodes = p;
    ix->cap_nodes = cap;
  }
  u32 i = ix->n_nodes++;
  ix->nodes[i].sym      = sym;
  ix->nodes[i].child    = ATP_DT_NIL;
  ix->nodes[i].sibling  = ATP_DT_NIL;
  ix->nodes[i].rec_head = ATP_DT_NIL;
  return i;
}

static u32 atp_dt_rec_new(AtpFvIndex *ix) {
  if (ix->n_recs == ix->cap_recs) {
    u32 cap = ix->cap_recs ? ix->cap_recs * 2u : 1024u;
    AtpDtRec *p = (AtpDtRec *)realloc(ix->recs, cap * sizeof(AtpDtRec));
    if (p == NULL) { fprintf(stderr, "atp_dt: rec pool OOM\n"); exit(1); }
    ix->recs = p;
    ix->cap_recs = cap;
  }
  return ix->n_recs++;
}

// Find `parent`'s child reached by edge `sym`, creating it absent.
// Children kept in ascending-sym order -- deterministic across runs.
static u32 atp_dt_child(AtpFvIndex *ix, u32 parent, u32 sym) {
  u32 prev = ATP_DT_NIL;
  u32 cur  = ix->nodes[parent].child;
  while (cur != ATP_DT_NIL && ix->nodes[cur].sym < sym) {
    prev = cur;
    cur  = ix->nodes[cur].sibling;
  }
  if (cur != ATP_DT_NIL && ix->nodes[cur].sym == sym) return cur;
  u32 nn = atp_dt_node_new(ix, sym);          // may realloc the pool
  ix->nodes[nn].sibling = cur;
  if (prev == ATP_DT_NIL) ix->nodes[parent].child = nn;
  else                    ix->nodes[prev].sibling = nn;
  return nn;
}

// --- seq -> record map (open addressing, linear probe) -------------

static void atp_dt_seqmap_init(AtpFvIndex *ix, u32 cap) {
  ix->seqmap_cap = cap;
  ix->seqmap = (AtpDtSeqEnt *)malloc(cap * sizeof(AtpDtSeqEnt));
  if (ix->seqmap == NULL) { fprintf(stderr, "atp_dt: seqmap OOM\n"); exit(1); }
  for (u32 i = 0; i < cap; i++) ix->seqmap[i].seq = ATP_DT_NIL;
}

static void atp_dt_seqmap_put(AtpFvIndex *ix, u32 seq, u32 rec);

static void atp_dt_seqmap_grow(AtpFvIndex *ix) {
  u32 old_cap = ix->seqmap_cap;
  AtpDtSeqEnt *old = ix->seqmap;
  atp_dt_seqmap_init(ix, old_cap * 2u);
  for (u32 i = 0; i < old_cap; i++) {
    if (old[i].seq != ATP_DT_NIL) atp_dt_seqmap_put(ix, old[i].seq, old[i].rec);
  }
  free(old);
}

static void atp_dt_seqmap_put(AtpFvIndex *ix, u32 seq, u32 rec) {
  if ((ix->n_live + 1u) * 2u > ix->seqmap_cap) atp_dt_seqmap_grow(ix);
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DT_NIL) h = (h + 1u) & mask;
  ix->seqmap[h].seq = seq;
  ix->seqmap[h].rec = rec;
}

static u32 atp_dt_seqmap_get(const AtpFvIndex *ix, u32 seq) {
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DT_NIL) {
    if (ix->seqmap[h].seq == seq) return ix->seqmap[h].rec;
    h = (h + 1u) & mask;
  }
  return ATP_DT_NIL;
}

// Delete `seq` (Knuth back-shift, keeps probe chains intact).
static void atp_dt_seqmap_del(AtpFvIndex *ix, u32 seq) {
  u32 mask = ix->seqmap_cap - 1u;
  u32 h = (seq * 2654435761u) & mask;
  while (ix->seqmap[h].seq != ATP_DT_NIL && ix->seqmap[h].seq != seq) {
    h = (h + 1u) & mask;
  }
  if (ix->seqmap[h].seq == ATP_DT_NIL) return;
  u32 j = h;
  for (;;) {
    ix->seqmap[h].seq = ATP_DT_NIL;
    u32 k;
    do {
      j = (j + 1u) & mask;
      if (ix->seqmap[j].seq == ATP_DT_NIL) return;
      k = (ix->seqmap[j].seq * 2654435761u) & mask;
    } while ((h <= j) ? (h < k && k <= j) : (h < k || k <= j));
    ix->seqmap[h] = ix->seqmap[j];
    h = j;
  }
}

// --- index lifecycle -----------------------------------------------

static AtpFvIndex *atp_fv_index_new(void) {
  AtpFvIndex *ix = (AtpFvIndex *)calloc(1, sizeof(AtpFvIndex));
  if (ix == NULL) { fprintf(stderr, "atp_dt: index OOM\n"); exit(1); }
  atp_dt_seqmap_init(ix, 1024u);
  ix->root = atp_dt_node_new(ix, ATP_DT_NIL);  // root edge unused
  return ix;
}

static void atp_fv_index_free(AtpFvIndex *ix) {
  if (ix == NULL) return;
  free(ix->nodes);
  free(ix->recs);
  free(ix->seqmap);
  free(ix);
}

// --- insert --------------------------------------------------------
//
// Walk term `t` in preorder, descending the tree by flatsym per
// node, creating edges as needed.  `vm` renumbers variables by
// first appearance.  Returns the node reached after `t`'s whole
// preorder string.  A TAG_CTR's children extend the string in
// left-to-right order; a TAG_FVR / TAG_NUM is one symbol.
static u32 atp_dt_insert_term(AtpFvIndex *ix, u32 node, Term t,
                              AtpDtVarMap *vm) {
  node = atp_dt_child(ix, node, atp_dt_flatsym(t, vm));
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      node = atp_dt_insert_term(ix, node, term_ctr_at(t, i), vm);
    }
  }
  return node;
}

// Insert CP (lhs, rhs) with stable id `seq`.  The CP is indexed as
// the synthetic term `Cp(lhs, rhs)` so a single tree spans both
// sides; ATP_CP_LABEL is the `Cp` head used elsewhere in the engine.
// One variable renumbering spans the WHOLE CP -- a variable shared
// between lhs and rhs (the common case for an equation) keeps one
// star index across both sides.
// `packed` is the queue's cp_packed[] byte string for this CP; the
// record borrows it (queue owns and frees).  `lhs`/`rhs` are the
// unpacked terms, needed only to descend the discrimination tree.
static void atp_fv_index_insert(AtpFvIndex *ix, Term lhs, Term rhs,
                                u8 *packed, u32 seq) {
  AtpDtVarMap vm;
  atp_dt_varmap_reset(&vm);
  u32 node = ix->root;
  node = atp_dt_child(ix, node, ATP_DT_CTR_BASE + ATP_CP_LABEL);  // Cp head
  node = atp_dt_insert_term(ix, node, lhs, &vm);
  node = atp_dt_insert_term(ix, node, rhs, &vm);
  u32 rec = atp_dt_rec_new(ix);
  ix->recs[rec].packed = packed;
  ix->recs[rec].seq    = seq;
  ix->recs[rec].live   = 1u;
  ix->recs[rec].folded = vm.folded;
  ix->recs[rec].next = ix->nodes[node].rec_head;
  ix->nodes[node].rec_head = rec;
  atp_dt_seqmap_put(ix, seq, rec);
  ix->n_live++;
}

// Drop CP `seq`: clear the record's live flag, unhook from the seq
// map.  The dead record stays in its leaf list (cheap); a rebuild
// reclaims the pool slot.
static void atp_fv_index_remove(AtpFvIndex *ix, u32 seq) {
  u32 rec = atp_dt_seqmap_get(ix, seq);
  if (rec == ATP_DT_NIL) return;
  if (ix->recs[rec].live) {
    ix->recs[rec].live = 0u;
    ix->n_live--;
  }
  atp_dt_seqmap_del(ix, seq);
}

// Discard every record / node and rebuild the tree from the live CP
// arrays.  Used when a wholesale CP-set mutation (reheapify after an
// atp_normalize_graph compaction; a test populating the arrays
// directly) reshuffles seqs out from under the incremental path.
static void atp_fv_index_rebuild(AtpState *s) {
  AtpFvIndex *ix = s->fv_index;
  if (ix == NULL) return;
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->n_live  = 0;
  ix->root    = atp_dt_node_new(ix, ATP_DT_NIL);
  for (u32 i = 0; i < ix->seqmap_cap; i++) ix->seqmap[i].seq = ATP_DT_NIL;
  for (u32 i = 0; i < s->n_cps; i++) {
    Term l = 0, r = 0;
    acp_unpack(s->cp_packed[i], &l, &r);
    atp_fv_index_insert(ix, l, r, s->cp_packed[i], s->cp_seq[i]);
  }
}

// --- retrieval -----------------------------------------------------

// Preorder-flatten `t` from index `*pos`, recording per position the
// SUBTREE SIZE (preorder-position span) in `subsz[]` and the flat
// SYMBOL CODE in `flatsym[]` -- CTR_BASE+lab / NUM / STAR+idx under
// the variable renumbering `vm`.  The flatsym string drives the
// perfect-tree descent: a CTR/NUM symbol matches exactly, a repeat
// pattern variable is confirmed by a flatsym-slice memcmp.  Returns 1
// on success, 0 if the cap is hit (caller falls back to the full scan
// -- never silently under-retrieves).
static u8 atp_dt_flatten(Term t, u32 *subsz, u32 *flatsym,
                         AtpDtVarMap *vm, u32 cap, u32 *pos) {
  u32 here = *pos;
  if (here >= cap) return 0;
  flatsym[here] = atp_dt_flatsym(t, vm);
  *pos = here + 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!atp_dt_flatten(term_ctr_at(t, i), subsz, flatsym, vm, cap, pos))
        return 0;
    }
  }
  subsz[here] = *pos - here;          // positions covered by t's subtree
  return 1;
}

// One retrieval's immutable parameters, threaded through the
// recursion without a wide signature.  The saturation engine is
// single-threaded (one AtpState per run), so file-static query
// scratch is safe and keeps the hot descent lean.  `g_atp_dt_star`
// is the descent's per-path binding array: star[k] is the PREORDER
// POSITION the k-th pattern variable was first bound to (0 = unbound;
// real positions are >= 1 since flat[0] is the synthetic Cp head).
static AtpFvIndex *g_atp_dt_ix      = NULL;
static const u32  *g_atp_dt_subsz   = NULL;
static const u32  *g_atp_dt_flatsym = NULL;  // per-position flat-symbol code
static u32         g_atp_dt_flatlen = 0;
static Term        g_atp_dt_qlhs    = 0;
static Term        g_atp_dt_qrhs    = 0;
static u32         g_atp_dt_star[ATP_DT_MAXVARS];
static u8          g_atp_dt_query_folded = 0;  // subject flatten folded a var

// A leaf reached by the descent: report whether a live CP sits here.
// When neither THIS CP nor the subject folded a variable (the common
// case after thvm_normalize_vars), the flatsym descent has already
// proved both sides exactly -- CTR/NUM symbols matched exactly,
// variable consistency by flatsym-slice memcmp -- so the live record
// IS the subsumer and the two-sided thvm_match is skipped.  A fold
// makes that record's path coarser; it alone re-runs the SAME
// thvm_match the array scan used as the authoritative guard -- one
// oversized CP no longer poisons the fast path for its leaf-mates.
static u8 atp_dt_leaf_match(u32 node) {
  AtpFvIndex *ix = g_atp_dt_ix;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_DT_NIL;
       r = ix->recs[r].next) {
    AtpDtRec *rc = &ix->recs[r];
    if (!rc->live) continue;
    ix->q_candidates++;
    if (!rc->folded && !g_atp_dt_query_folded) return 1;  // exact descent
    // Match the stored CP straight off its packed byte string -- no
    // per-candidate acp_unpack (the late-game FV-index hot path).
    RewriteSubst subst = {{0}};
    ix->q_matchcalls += 2u;
    if (acp_match_pair(rc->packed, g_atp_dt_qlhs, g_atp_dt_qrhs, &subst)) {
      return 1;
    }
  }
  return 0;
}

// Walk the flattened subject from preorder index `pos` in lockstep
// with tree node `node`, threading the per-path variable bindings in
// g_atp_dt_star.  Returns 1 as soon as a reachable leaf yields a
// genuine subsumer.
//
// A CTR/NUM subject head matches at most ONE child, so that branch is
// a tail continuation followed by LOOPING (advance node/pos in place)
// rather than recursing -- only the STAR branches recurse.  This both
// drops call overhead on the CTR spine and bounds the recursion depth
// to the path's STAR-edge count, so a deep (ATP_DT_FLAT_CAP-long)
// subject cannot overflow the stack.
static u8 atp_dt_descend(u32 node, u32 pos) {
  AtpFvIndex *ix = g_atp_dt_ix;
  for (;;) {
    ix->q_nodevisits++;
    if (pos == g_atp_dt_flatlen) {
      // Whole subject consumed -- this node's records are leaves.
      return atp_dt_leaf_match(node);
    }
    u32  sz         = g_atp_dt_subsz[pos];   // preorder span of t's subtree
    u32  csym_exact = g_atp_dt_flatsym[pos]; // CTR_BASE+lab / NUM / STAR+idx
    u32  ctr_next   = ATP_DT_NIL;            // the lone CTR/NUM-match child
    for (u32 c = ix->nodes[node].child; c != ATP_DT_NIL;
         c = ix->nodes[c].sibling) {
      u32 csym = ix->nodes[c].sym;
      if (csym >= ATP_DT_STAR_BASE && csym < ATP_DT_CTR_BASE) {
        // Stored variable, the (csym-STAR_BASE)-th distinct pattern
        // var.  One-way match: the FIRST occurrence binds it to this
        // subterm's preorder position; a REPEAT applies only if the two
        // subterms' flatsym slices are byte-identical.
        u32 k = csym - ATP_DT_STAR_BASE;
        u32 bound = g_atp_dt_star[k];
        if (bound == 0) {
          g_atp_dt_star[k] = pos;             // first occurrence: bind
          u8 hit = atp_dt_descend(c, pos + sz);
          g_atp_dt_star[k] = 0;               // unbind on backtrack
          if (hit) return 1;
        } else if (g_atp_dt_subsz[bound] == sz &&
                   memcmp(&g_atp_dt_flatsym[bound], &g_atp_dt_flatsym[pos],
                          (size_t)sz * sizeof(u32)) == 0) {
          if (atp_dt_descend(c, pos + sz)) return 1;
        }
      } else if (csym == csym_exact) {
        // Stored CTR/NUM equal to t's own symbol -- consume t's head;
        // t's children are the next preorder positions.
        ctr_next = c;
      }
    }
    if (ctr_next == ATP_DT_NIL) return 0;     // no CTR continuation: done
    node = ctr_next;                          // tail-continue without a call
    pos  = pos + 1u;
  }
}

// Retrieve over one orientation: descend the tree for the synthetic
// subject `Cp(o_lhs, o_rhs)`.  Returns 1 if a queued CP subsumes the
// candidate in this orientation.
static u8 atp_dt_query_orient(AtpFvIndex *ix, Term o_lhs, Term o_rhs) {
  // Static (not stack): ATP_DT_FLAT_CAP is large enough that a 512 KB
  // pair of arrays would overflow the frame.  The saturation engine is
  // single-threaded and atp_dt_query_orient does not recurse, so one
  // shared scratch pair is safe -- each call refills it before use.
  static u32 subsz_s[ATP_DT_FLAT_CAP];
  static u32 flatsym_s[ATP_DT_FLAT_CAP];
  // One variable renumbering spans the whole synthetic Cp(o_lhs,o_rhs)
  // -- exactly as atp_fv_index_insert renumbers the stored CP.
  AtpDtVarMap vm;
  atp_dt_varmap_reset(&vm);
  // Reserve position 0 for the synthetic `Cp` head: it spans the whole
  // subject, so subsz[0] = total positions.
  u32 pos = 1u;
  u8  ok  = atp_dt_flatten(o_lhs, subsz_s, flatsym_s, &vm, ATP_DT_FLAT_CAP, &pos)
         && atp_dt_flatten(o_rhs, subsz_s, flatsym_s, &vm, ATP_DT_FLAT_CAP, &pos);
  if (!ok) {
    // Cap hit -- fall back to a full scan so a deep CP can never be
    // silently under-retrieved (which would drop a real subsumer).
    for (u32 r = 0; r < ix->n_recs; r++) {
      if (!ix->recs[r].live) continue;
      ix->q_candidates++;
      RewriteSubst subst = {{0}};
      ix->q_matchcalls += 2u;
      if (acp_match_pair(ix->recs[r].packed, o_lhs, o_rhs, &subst)) {
        return 1;
      }
    }
    return 0;
  }
  flatsym_s[0] = ATP_DT_CTR_BASE + ATP_CP_LABEL;  // synthetic Cp head
  subsz_s[0]   = pos;                             // whole subject span
  g_atp_dt_ix           = ix;
  g_atp_dt_subsz        = subsz_s;
  g_atp_dt_flatsym      = flatsym_s;
  g_atp_dt_flatlen      = pos;
  g_atp_dt_qlhs         = o_lhs;
  g_atp_dt_qrhs         = o_rhs;
  g_atp_dt_query_folded = vm.folded;
  for (u32 i = 0; i < ATP_DT_MAXVARS; i++) g_atp_dt_star[i] = 0;
  // Descend from the root: its single real edge is the `Cp` head,
  // which lines up with flat[0] (also a Cp-head symbol).
  u32 cp_sym = ATP_DT_CTR_BASE + ATP_CP_LABEL;
  for (u32 c = ix->nodes[ix->root].child; c != ATP_DT_NIL;
       c = ix->nodes[c].sibling) {
    if (ix->nodes[c].sym == cp_sym) return atp_dt_descend(c, 1u);
  }
  return 0;
}

// 7d: forward subsumption of candidate (lhs, rhs) against the queued
// CP set via the discrimination tree.  Behaviour-identical to the
// array scan: the tree is a sound over-approximation, so any CP it
// surfaces is then confirmed by the SAME thvm_match the scan used,
// and any CP it prunes provably could not have matched.  Two
// orientations: forward Cp(lhs,rhs), symmetric Cp(rhs,lhs).
static u8 atp_fv_index_query(AtpFvIndex *ix, Term lhs, Term rhs) {
  ix->q_calls++;
  if (atp_dt_query_orient(ix, lhs, rhs)) return 1;
  if (atp_dt_query_orient(ix, rhs, lhs)) return 1;
  return 0;
}

// 7d instrumentation accessor: per-run retrieval stats so a bench can
// confirm the O(n_cps) scan collapsed.  `calls` is the number of
// atp_cp_queue_subsumed queries; `node_visits` the discrimination-
// tree nodes touched; `candidates` the leaf records reached; and
// `matchcalls` the thvm_match calls those candidates triggered.
// candidates / calls is the false-positive volume; the array scan
// would have touched n_cps records per query.
fn void thvm_atp_fv_stats(const AtpState *s, u64 *calls, u64 *node_visits,
                          u64 *candidates, u64 *matchcalls, u32 *nodes) {
  AtpFvIndex *ix = (s != NULL) ? s->fv_index : NULL;
  if (calls       != NULL) *calls       = (ix != NULL) ? ix->q_calls : 0;
  if (node_visits != NULL) *node_visits = (ix != NULL) ? ix->q_nodevisits : 0;
  if (candidates  != NULL) *candidates  = (ix != NULL) ? ix->q_candidates : 0;
  if (matchcalls  != NULL) *matchcalls  = (ix != NULL) ? ix->q_matchcalls : 0;
  if (nodes       != NULL) *nodes       = (ix != NULL) ? ix->n_nodes : 0;
}

#endif // ATP_FV_INDEX

// === 7e lever 2: rule-LHS redex index (-DATP_RULE_INDEX) ============
//
// THE OTHER WALL.  Once 7d collapsed the CP-queue subsumption scan,
// the diagnosis re-profiled and pinned the remaining normalization
// wall on `thvm_rewrite_step` -- specifically `rewrite_try_top`'s
// O(n_rules) linear LHS scan.  `atp_cp_trivially_joinable` runs two
// full `atp_rewrite_normalize` calls per CP candidate; each is up to
// NORM_CAP=64 `thvm_rewrite_step`s; each step calls `rewrite_try_top`
// at every preorder position of the term; and `rewrite_try_top` tries
// every rule LHS at that position.  As R climbs past 250 rules the
// linear scan dominates.
//
// THE FIX -- the DUAL of 7d.  7d indexes `Cp(lhs, rhs)` PAIRS and, for
// a subject CP, retrieves a stored pattern that one-way matches it
// (subsumption retrieval).  Here we index single rule-LHS TERMS and,
// for a subject subterm, retrieve which rule LHS one-way matches it
// (redex retrieval) -- the same matching direction (stored pattern has
// variables, subject is concrete), so 7d's perfect-discrimination-tree
// descent is reused VERBATIM: CTR-exact edge / first-var-bind STAR /
// repeat-var-kbo_eq STAR, the STAR/CTR flat alphabet, the preorder
// flatten with subtree spans.  Only the insert key (one term, no `Cp`
// wrapper) and the leaf action (collect a rule index, not return-on-
// first-hit) are rewritten.
//
// BEHAVIOR-IDENTITY -- the lowest-rule-index rule.  `rewrite_try_top`
// tries rules in index order and the FIRST match wins; mid-completion
// R is not confluent, so which rule fires changes the normal form.
// The tree returns leaves in tree order, not index order.  So the
// descent does NOT stop on first hit: it visits EVERY reachable leaf
// and tracks the minimum rule index.  The leaf still runs the SAME
// one-way `thvm_match` the linear scan ran, as the authoritative
// guard, so a stored LHS reaches "winner" status iff `thvm_match`
// confirms it -- byte-identical to the linear scan picking that rule.
// `thvm_rewrite_step`'s redex-selection order (top, then children
// left-to-right) is untouched -- only the per-position rule choice is
// indexed.  This block is independent of -DATP_FV_INDEX (it carries
// its own copy of the discrimination-tree skeleton).

#ifdef ATP_RULE_INDEX

// The unorientable-faces index inserts a face only when its replacement
// side's variables are contained in the matched side (else the face can
// never fire); declared here, defined with the ordered-rewrite helpers.
static int atp_vars_contained(Term a, Term b);

// Flat-symbol alphabet -- same scheme as 7d's atp_dt_*: NUM < every
// STAR(k) < every CTR(lab) so a sym-ascending child list lets the
// descent stop scanning early.
#define ATP_RI_NUM        0u
#define ATP_RI_MAXVARS    64u
#define ATP_RI_STAR_BASE  1u
#define ATP_RI_CTR_BASE   (ATP_RI_STAR_BASE + ATP_RI_MAXVARS)
#define ATP_RI_NIL        0xFFFFFFFFu
// Preorder-flatten capacity for the indexed normalizer.  Sized to
// hold a raw (un-reduced) critical-pair side -- the deep overlap of
// two rules can run to tens of thousands of nodes before it is
// normalized down; an over-deep subject still works (the normalizer
// takes linear steps and re-flattens once rewriting shrinks it back
// under the cap) but pays the linear rate while it does.
#define ATP_RI_FLAT_CAP   65536u

// Per-term variable renumbering by first appearance (see 7d's
// AtpDtVarMap).  `slot[id]` holds (index+1); 0 = not yet seen.
typedef struct { u32 slot[REWRITE_MAX_VAR]; u32 n; u8 folded; } AtpRiVarMap;

static void atp_ri_varmap_reset(AtpRiVarMap *vm) {
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) vm->slot[i] = 0;
  vm->n = 0;
  vm->folded = 0;
}

// `folded` records whether any variable hit an imperfect case -- an id
// >= REWRITE_MAX_VAR, or more than ATP_RI_MAXVARS distinct vars -- where
// two distinct variables collapse onto one star slot.  A flatten with
// folded == 0 distinguishes every variable, so a discrimination-tree
// descent that reaches a leaf is then an exact match proof.
static u32 atp_ri_var_index(AtpRiVarMap *vm, u32 vid) {
  if (vid >= REWRITE_MAX_VAR) { vid = REWRITE_MAX_VAR - 1u; vm->folded = 1u; }
  if (vm->slot[vid] == 0) {
    u32 idx;
    if (vm->n < ATP_RI_MAXVARS) { idx = vm->n; vm->n++; }
    else                       { idx = ATP_RI_MAXVARS - 1u; vm->folded = 1u; }
    vm->slot[vid] = idx + 1u;
  }
  return vm->slot[vid] - 1u;
}

static u32 atp_ri_flatsym(Term t, AtpRiVarMap *vm) {
  switch (term_tag(t)) {
    case TAG_CTR: return ATP_RI_CTR_BASE + term_ext(t);
    case TAG_NUM: return ATP_RI_NUM;
    case TAG_FVR:
    default:      return ATP_RI_STAR_BASE + atp_ri_var_index(vm, term_ext(t));
  }
}

// Flat symbol under RAW variable ids -- no first-appearance renumbering.
// The subject-side flatten uses this (rule-LHS inserts keep the
// first-appearance scheme above).  Raw ids make a flatsym position
// independent of the rest of the term, so an incremental re-flatten can
// SPLICE a rewritten subtree without re-deriving the whole string.  For
// a var-normalised subject (dense [0,k) ids, first appearance == id) it
// is bit-identical to atp_ri_flatsym; the variable-consistency relation
// the descent's repeat-var memcmp depends on is preserved either way
// (both are consistent global encodings).  `*folded` is raised if a raw
// id crosses ATP_RI_MAXVARS (then the descent re-confirms via thvm_match).
static u32 atp_ri_flatsym_raw(Term t, u8 *folded) {
  switch (term_tag(t)) {
    case TAG_CTR: return ATP_RI_CTR_BASE + term_ext(t);
    case TAG_NUM: return ATP_RI_NUM;
    case TAG_FVR:
    default: {
      u32 id = term_ext(t);
      if (id >= ATP_RI_MAXVARS) { *folded = 1u; id = ATP_RI_MAXVARS - 1u; }
      return ATP_RI_STAR_BASE + id;
    }
  }
}

// A discrimination-tree node: left-child / right-sibling in a flat
// realloc-grown pool addressed by u32 index (a realloc never
// invalidates the structure).  `rec_head` heads the leaf record list.
typedef struct {
  u32 sym;
  u32 child;
  u32 sibling;
  u32 rec_head;
} AtpRiNode;

// One indexed rule: `rule` is the index into s->lhs[]/s->rhs[].
// `next` links the leaf list.  No Term mirror -- the index is rebuilt
// from s->lhs[] whenever R mutates, so it never outlives a GC move.
typedef struct {
  u32 rule;
  u32 next;
} AtpRiRec;

struct AtpRuleIndex {
  AtpRiNode *nodes;
  u32        n_nodes, cap_nodes;
  AtpRiRec  *recs;
  u32        n_recs, cap_recs;
  u32        root;
  u32        n_rules_built;     // R size the tree currently reflects
  u8         any_folded;        // some rule LHS folded a var -> imperfect
  // 1 for the unorientable-faces index: a leaf rec's `rule` field then
  // carries the direction in its high bit (ATP_RI_DIR_BIT) -- bit set =
  // r->l (matched face is rhs[i], replacement is lhs[i]); clear = l->r.
  u8         is_unorient;
};
// Direction bit packed into a leaf rec's `rule` field for the
// unorientable index.  Rule indices never approach 2^31 in completion,
// so the top bit is free as a per-face direction tag.
#define ATP_RI_DIR_BIT  0x80000000u
typedef struct AtpRuleIndex AtpRuleIndex;

static u32 atp_ri_node_new(AtpRuleIndex *ix, u32 sym) {
  if (ix->n_nodes == ix->cap_nodes) {
    u32 cap = ix->cap_nodes ? ix->cap_nodes * 2u : 1024u;
    AtpRiNode *p = (AtpRiNode *)realloc(ix->nodes, cap * sizeof(AtpRiNode));
    if (p == NULL) { fprintf(stderr, "atp_ri: node pool OOM\n"); exit(1); }
    ix->nodes = p;
    ix->cap_nodes = cap;
  }
  u32 i = ix->n_nodes++;
  ix->nodes[i].sym      = sym;
  ix->nodes[i].child    = ATP_RI_NIL;
  ix->nodes[i].sibling  = ATP_RI_NIL;
  ix->nodes[i].rec_head = ATP_RI_NIL;
  return i;
}

static u32 atp_ri_rec_new(AtpRuleIndex *ix) {
  if (ix->n_recs == ix->cap_recs) {
    u32 cap = ix->cap_recs ? ix->cap_recs * 2u : 1024u;
    AtpRiRec *p = (AtpRiRec *)realloc(ix->recs, cap * sizeof(AtpRiRec));
    if (p == NULL) { fprintf(stderr, "atp_ri: rec pool OOM\n"); exit(1); }
    ix->recs = p;
    ix->cap_recs = cap;
  }
  return ix->n_recs++;
}

// Find `parent`'s child reached by edge `sym`, creating it absent.
// Children kept in ascending-sym order.
static u32 atp_ri_child(AtpRuleIndex *ix, u32 parent, u32 sym) {
  u32 prev = ATP_RI_NIL;
  u32 cur  = ix->nodes[parent].child;
  while (cur != ATP_RI_NIL && ix->nodes[cur].sym < sym) {
    prev = cur;
    cur  = ix->nodes[cur].sibling;
  }
  if (cur != ATP_RI_NIL && ix->nodes[cur].sym == sym) return cur;
  u32 nn = atp_ri_node_new(ix, sym);          // may realloc the pool
  ix->nodes[nn].sibling = cur;
  if (prev == ATP_RI_NIL) ix->nodes[parent].child = nn;
  else                    ix->nodes[prev].sibling = nn;
  return nn;
}

static AtpRuleIndex *atp_ri_new(void) {
  AtpRuleIndex *ix = (AtpRuleIndex *)calloc(1, sizeof(AtpRuleIndex));
  if (ix == NULL) { fprintf(stderr, "atp_ri: index OOM\n"); exit(1); }
  ix->root = atp_ri_node_new(ix, ATP_RI_NIL);
  return ix;
}

static void atp_ri_free(AtpRuleIndex *ix) {
  if (ix == NULL) return;
  free(ix->nodes);
  free(ix->recs);
  free(ix);
}

// Walk rule LHS `t` in preorder, descending the tree by flatsym per
// node.  Returns the node reached after `t`'s whole preorder string.
static u32 atp_ri_insert_term(AtpRuleIndex *ix, u32 node, Term t,
                              AtpRiVarMap *vm) {
  node = atp_ri_child(ix, node, atp_ri_flatsym(t, vm));
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      node = atp_ri_insert_term(ix, node, term_ctr_at(t, i), vm);
    }
  }
  return node;
}

// Discard every node / record and rebuild the tree from s->lhs[0..
// n_rules).  Rules are inserted in ascending index order -- the leaf
// list at a node is then index-descending (push-front), which the
// retrieval's min-tracking does not depend on but keeps deterministic.
static void atp_ri_rebuild(AtpState *s) {
  AtpRuleIndex *ix = s->rule_index;
  if (ix == NULL) return;
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->any_folded = 0;
  ix->root    = atp_ri_node_new(ix, ATP_RI_NIL);
  for (u32 i = 0; i < s->n_rules; i++) {
    // When unorientable equations are present, index only the
    // orientable rules (always forward-decreasing, applied without an
    // order check); the few unorientable ones go to the linear
    // KBO-gated pass.  When n_unorient == 0 every rule is orientable, so
    // index all -- and r_orient[] may be a stale default for rules a
    // caller installed by hand, which is fine in that all-orientable
    // regime.
    if (s->n_unorient > 0u && !s->r_orient[i]) continue;
    AtpRiVarMap vm;
    atp_ri_varmap_reset(&vm);
    u32 node = atp_ri_insert_term(ix, ix->root, s->lhs[i], &vm);
    if (vm.folded) ix->any_folded = 1u;
    u32 rec  = atp_ri_rec_new(ix);
    ix->recs[rec].rule = i;
    ix->recs[rec].next = ix->nodes[node].rec_head;
    ix->nodes[node].rec_head = rec;
  }
  ix->n_rules_built = s->n_rules;

  // Companion index over the UNORIENTABLE equations' faces.  For each
  // unorientable rule i, the matched side may be either face; index a
  // face only when its replacement side's variables are contained in it
  // (else the face can never produce a well-formed instance and the
  // linear scan's atp_vars_contained guard would reject it).  The leaf
  // rec's `rule` field carries the direction in ATP_RI_DIR_BIT: clear =
  // l->r (match lhs[i], replace by rhs[i]); set = r->l (match rhs[i],
  // replace by lhs[i]).  Faces are inserted in (rule asc, l->r before
  // r->l) order so the retrieval's candidate list is built deterministic.
  AtpRuleIndex *ux = s->unorient_index;
  if (ux != NULL) {
    ux->n_nodes = 0;
    ux->n_recs  = 0;
    ux->any_folded = 0;
    ux->is_unorient = 1u;
    ux->root    = atp_ri_node_new(ux, ATP_RI_NIL);
    if (s->n_unorient > 0u) {
      for (u32 i = 0; i < s->n_rules; i++) {
        if (s->r_orient[i]) continue;                 // oriented: rule_index
        // l->r face: match lhs[i], replace by rhs[i].
        if (atp_vars_contained(s->rhs[i], s->lhs[i])) {
          AtpRiVarMap vm;
          atp_ri_varmap_reset(&vm);
          u32 node = atp_ri_insert_term(ux, ux->root, s->lhs[i], &vm);
          if (vm.folded) ux->any_folded = 1u;
          u32 rec  = atp_ri_rec_new(ux);
          ux->recs[rec].rule = i;                     // dir bit clear
          ux->recs[rec].next = ux->nodes[node].rec_head;
          ux->nodes[node].rec_head = rec;
        }
        // r->l face: match rhs[i], replace by lhs[i].
        if (atp_vars_contained(s->lhs[i], s->rhs[i])) {
          AtpRiVarMap vm;
          atp_ri_varmap_reset(&vm);
          u32 node = atp_ri_insert_term(ux, ux->root, s->rhs[i], &vm);
          if (vm.folded) ux->any_folded = 1u;
          u32 rec  = atp_ri_rec_new(ux);
          ux->recs[rec].rule = i | ATP_RI_DIR_BIT;
          ux->recs[rec].next = ux->nodes[node].rec_head;
          ux->nodes[node].rec_head = rec;
        }
      }
    }
    ux->n_rules_built = s->n_rules;
  }

  s->rule_index_dirty = 0u;
}

// --- retrieval -----------------------------------------------------

// Preorder-flatten `t` into `flat[]` from `*pos`, recording each
// position's subtree span in `subsz[]` and its raw flat-symbol in
// `flatsym[]`.  Returns 1 on success, 0 on cap.  Used both for the
// whole-subject flatten at the start of a normalize and -- with `*pos`
// set to a redex position -- to splice a rewritten subtree in place.
static u8 atp_ri_flatten(Term t, Term *flat, u32 *subsz, u32 *flatsym,
                         u8 *folded, u32 cap, u32 *pos) {
  u32 here = *pos;
  if (here >= cap) return 0;
  flat[here]    = t;
  flatsym[here] = atp_ri_flatsym_raw(t, folded);
  *pos = here + 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      if (!atp_ri_flatten(term_ctr_at(t, i), flat, subsz, flatsym, folded,
                          cap, pos)) return 0;
    }
  }
  subsz[here] = *pos - here;
  return 1;
}

// One retrieval's immutable parameters (single-threaded saturation, so
// file-static query scratch is safe).  `g_atp_ri_flat`/`subsz` span
// the WHOLE subject of the current `atp_ri_rewrite_step`; a query at
// preorder position `p` walks the slice flat[p .. p+subsz[p]).
// `g_atp_ri_best` is the lowest rule index a reachable leaf has
// confirmed so far -- ATP_RI_NIL = none.
static AtpRuleIndex *g_atp_ri_ix      = NULL;
static const Term   *g_atp_ri_flat    = NULL;
static const u32    *g_atp_ri_subsz   = NULL;
static const u32    *g_atp_ri_flatsym = NULL;  // per-position flat-symbol code
static const Term   *g_atp_ri_lhs     = NULL;  // s->lhs[] for the leaf guard
static const Term   *g_atp_ri_rhs     = NULL;  // s->rhs[] for r->l face guards
static u32           g_atp_ri_qend    = 0;     // end of the queried slice
static u32           g_atp_ri_star[ATP_RI_MAXVARS];  // first-bind positions
static u32           g_atp_ri_best    = ATP_RI_NIL;
static u32           g_atp_ri_best_star[ATP_RI_MAXVARS]; // star[] at the win
static Term          g_atp_ri_qsubj   = 0;     // subject at the query position
static u8            g_atp_ri_query_folded = 0; // subject flatten folded a var

// At a leaf node: update g_atp_ri_best with the minimum rule index
// whose LHS genuinely one-way matches the query subject.  The
// perfect-tree descent proves structure + variable consistency for
// the var ids the tree distinguishes, but `atp_ri_var_index` folds
// ids >= REWRITE_MAX_VAR onto one slot (coarser tree), so the leaf
// re-runs `thvm_match` -- the authoritative guard, exactly the test
// `rewrite_try_top`'s linear scan applies.
static void atp_ri_leaf_collect(u32 node) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  // When neither the rule LHSs nor this subject folded a variable (the
  // common case after thvm_normalize_vars), the flatsym descent has
  // already proved the match exactly -- CTR symbols matched exactly,
  // variable consistency by flatsym memcmp -- so reaching this leaf IS
  // the proof and thvm_match is skipped.  A fold makes the tree coarser
  // and the leaf re-runs thvm_match as the authoritative guard.
  u8 perfect = !ix->any_folded && !g_atp_ri_query_folded;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_RI_NIL;
       r = ix->recs[r].next) {
    u32 rule = ix->recs[r].rule;
    if (rule >= g_atp_ri_best) continue;          // cannot lower the min
    if (perfect) {
      g_atp_ri_best = rule;
      // Snapshot the path's variable bindings: with a perfect descent
      // these ARE the match substitution, so atp_ri_rewrite_step reads
      // them off directly instead of re-running thvm_match.
      for (u32 k = 0; k < ATP_RI_MAXVARS; k++) {
        g_atp_ri_best_star[k] = g_atp_ri_star[k];
      }
      continue;
    }
    RewriteSubst subst = {{0}};
    if (thvm_match(g_atp_ri_lhs[rule], g_atp_ri_qsubj, &subst)) {
      g_atp_ri_best = rule;
    }
  }
}

// Walk the flattened subject slice from preorder index `pos` in
// lockstep with tree node `node`, threading per-path variable
// bindings in g_atp_ri_star.  Visits EVERY reachable leaf (does not
// stop early) so g_atp_ri_best ends at the global minimum matching
// rule index for this position.
//
// At most ONE child can match a CTR/NUM subject head (the children
// carry distinct symbols), so that branch is a tail continuation: it
// is followed by LOOPING -- advancing `node`/`pos` in place -- rather
// than recursing.  Only the STAR (rule-variable) branches, which can
// fan out, recurse.  For the typical CTR spine of a rule LHS the whole
// descent is then one loop with no call overhead.
static void atp_ri_descend(u32 node, u32 pos) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  for (;;) {
    if (pos == g_atp_ri_qend) {
      atp_ri_leaf_collect(node);
      return;
    }
    u32 sz         = g_atp_ri_subsz[pos];
    u32 csym_exact = g_atp_ri_flatsym[pos];   // CTR_BASE+lab / NUM / STAR+idx
    u32 ctr_next   = ATP_RI_NIL;              // the lone CTR-match child
    for (u32 c = ix->nodes[node].child; c != ATP_RI_NIL;
         c = ix->nodes[c].sibling) {
      u32 csym = ix->nodes[c].sym;
      if (csym >= ATP_RI_STAR_BASE && csym < ATP_RI_CTR_BASE) {
        // Stored rule variable: FIRST occurrence binds it to this
        // subterm's preorder position; a REPEAT applies only if the two
        // subterms' flatsym slices are byte-identical (one-way matching).
        u32 k = csym - ATP_RI_STAR_BASE;
        u32 bound = g_atp_ri_star[k];
        if (bound == ATP_RI_NIL) {
          g_atp_ri_star[k] = pos;
          atp_ri_descend(c, pos + sz);
          g_atp_ri_star[k] = ATP_RI_NIL;
        } else if (g_atp_ri_subsz[bound] == sz &&
                   memcmp(&g_atp_ri_flatsym[bound], &g_atp_ri_flatsym[pos],
                          (size_t)sz * sizeof(u32)) == 0) {
          atp_ri_descend(c, pos + sz);
        }
      } else if (csym == csym_exact) {
        // Stored CTR/NUM equal to the subject's head: consume the head;
        // the subject's children are the next preorder positions.
        ctr_next = c;
      }
    }
    if (ctr_next == ATP_RI_NIL) return;       // no CTR continuation: done
    node = ctr_next;                          // tail-continue without a call
    pos  = pos + 1u;
  }
}

// Retrieve the lowest rule index whose LHS one-way matches the subject
// subterm at preorder position `qpos` of the shared flat array.
// Returns ATP_RI_NIL if no rule LHS matches there.
static u32 atp_ri_query_pos(u32 qpos) {
  g_atp_ri_qend  = qpos + g_atp_ri_subsz[qpos];
  g_atp_ri_qsubj = g_atp_ri_flat[qpos];
  g_atp_ri_best  = ATP_RI_NIL;
  // g_atp_ri_star is NOT reset here: atp_ri_descend pairs every
  // `star[k] = pos` with a `star[k] = NIL` on backtrack and never
  // returns early, so star[] is all-NIL on entry and exit of every
  // descent.  The one reset is done once per normalize.
  atp_ri_descend(g_atp_ri_ix->root, qpos);
  return g_atp_ri_best;
}

// --- unorientable-faces retrieval (candidate collection) -----------
//
// The orientable retrieval above tracks a single MINIMUM rule index: the
// linear scan it replaces always fires the lowest-index rule with no
// order check, so the structural match alone names the winner.  The
// unorientable pass cannot collapse to a min: at a position, the linear
// scan tries each unorientable equation (rule asc; l->r then r->l) and
// the FIRST direction that is strictly order-DECREASING for the matched
// instance wins -- the structurally-lowest face may be order-rejected.
// So this descent COLLECTS every structurally matching face (rule, dir)
// at the position; the caller applies the LPO gate in (rule asc, l->r
// before r->l) order and fires the first that passes -- identical to the
// linear scan, but the LPO compare runs only on candidates.
// Candidate buffer cap.  At a position, the matching unorientable faces
// are bounded by the count of indexed faces (<= 2 * n_unorient); a deep
// completion's unorientable subset runs to a few hundred, and a shallow
// face (e.g. a commutativity-style nand(x,y)=nand(y,x)) matches every
// nand-headed subterm, so size generously to keep the indexed path live
// (overflow only forces the exact linear fallback at that position --
// correct, just slow).
#define ATP_RI_MAXCAND 8192u
static u32 g_atp_ri_cand[ATP_RI_MAXCAND];   // rule | dir-bit, structural hits
static u32 g_atp_ri_ncand = 0;

// Leaf action for the unorientable index: append each face whose stored
// pattern one-way matches the query subject.  Same match proof as the
// orientable leaf (perfect descent OR thvm_match on a fold) -- only the
// stored side (lhs[i] for l->r, rhs[i] for r->l) is the match pattern.
static void atp_ri_leaf_collect_unorient(u32 node) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  u8 perfect = !ix->any_folded && !g_atp_ri_query_folded;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_RI_NIL;
       r = ix->recs[r].next) {
    u32 packed = ix->recs[r].rule;
    if (g_atp_ri_ncand >= ATP_RI_MAXCAND) return;   // buffer full: caller bails
    if (perfect) {
      g_atp_ri_cand[g_atp_ri_ncand++] = packed;
      continue;
    }
    u32 rule = packed & ~ATP_RI_DIR_BIT;
    Term pat = (packed & ATP_RI_DIR_BIT) ? g_atp_ri_rhs[rule]
                                         : g_atp_ri_lhs[rule];
    RewriteSubst subst = {{0}};
    if (thvm_match(pat, g_atp_ri_qsubj, &subst)) {
      g_atp_ri_cand[g_atp_ri_ncand++] = packed;
    }
  }
}

// Candidate-collecting twin of atp_ri_descend.  Identical traversal,
// different leaf action (collect every face, not the min).  Kept
// separate so the hot orientable descent stays a tight single-callback
// loop with no per-node branch on index kind.
static void atp_ri_descend_unorient(u32 node, u32 pos) {
  AtpRuleIndex *ix = g_atp_ri_ix;
  for (;;) {
    if (pos == g_atp_ri_qend) {
      atp_ri_leaf_collect_unorient(node);
      return;
    }
    u32 sz         = g_atp_ri_subsz[pos];
    u32 csym_exact = g_atp_ri_flatsym[pos];
    u32 ctr_next   = ATP_RI_NIL;
    for (u32 c = ix->nodes[node].child; c != ATP_RI_NIL;
         c = ix->nodes[c].sibling) {
      u32 csym = ix->nodes[c].sym;
      if (csym >= ATP_RI_STAR_BASE && csym < ATP_RI_CTR_BASE) {
        u32 k = csym - ATP_RI_STAR_BASE;
        u32 bound = g_atp_ri_star[k];
        if (bound == ATP_RI_NIL) {
          g_atp_ri_star[k] = pos;
          atp_ri_descend_unorient(c, pos + sz);
          g_atp_ri_star[k] = ATP_RI_NIL;
        } else if (g_atp_ri_subsz[bound] == sz &&
                   memcmp(&g_atp_ri_flatsym[bound], &g_atp_ri_flatsym[pos],
                          (size_t)sz * sizeof(u32)) == 0) {
          atp_ri_descend_unorient(c, pos + sz);
        }
      } else if (csym == csym_exact) {
        ctr_next = c;
      }
    }
    if (ctr_next == ATP_RI_NIL) return;
    node = ctr_next;
    pos  = pos + 1u;
  }
}

// Collect into g_atp_ri_cand[] every unorientable face whose stored
// pattern one-way matches the subject subterm at preorder position
// `qpos`.  Returns the candidate count (g_atp_ri_ncand).  Order within
// the buffer is leaf-list / tree order -- NOT priority -- so the caller
// must apply the (rule asc, l->r before r->l) priority itself.
static u32 atp_ri_query_pos_unorient(u32 qpos) {
  g_atp_ri_qend  = qpos + g_atp_ri_subsz[qpos];
  g_atp_ri_qsubj = g_atp_ri_flat[qpos];
  g_atp_ri_ncand = 0;
  atp_ri_descend_unorient(g_atp_ri_ix->root, qpos);
  return g_atp_ri_ncand;
}

// Find the FIRST preorder position of the shared subject that is a
// redex (some rule LHS matches there).  Preorder = outermost-leftmost
// = exactly `thvm_rewrite_step`'s "try top, then children left-to-
// right" order.  `clean_before` is the previous step's rewrite
// position: every position strictly before it that is NOT one of its
// ancestors (q + subsz[q] <= clean_before) was unchanged by that
// rewrite and was already found non-redex, so it is skipped -- the
// search re-examines only the ancestors and the changed subtree.  This
// returns the identical redex a full scan would (incremental
// outermost normalisation, Waldmeister's ascend-from-last-redex).
static u8 atp_ri_find_redex(u32 flatlen, u32 clean_before,
                            u32 *redex_pos, u32 *redex_rule) {
  for (u32 p = 0; p < flatlen; p++) {
    if (p < clean_before && p + g_atp_ri_subsz[p] <= clean_before) {
      continue;                              // unchanged, known non-redex
    }
    u32 m = atp_ri_query_pos(p);
    if (m != ATP_RI_NIL) {
      *redex_pos  = p;
      *redex_rule = m;
      return 1;
    }
  }
  return 0;
}

// Materialise a tree Term from the flat arrays at preorder position
// `pos`.  Called ONCE per normalize (at fixpoint) -- the per-step
// rewrites only SPLICE the flat arrays, never the tree.  `flatsym`
// distinguishes a CTR (>= CTR_BASE) from a leaf; `flat[pos]` carries
// each node's pre-splice Term, so a subtree no splice touched rebuilds
// child-for-child equal to its original cell and is returned as-is --
// untouched subtrees cost zero allocation, only the union of the
// rewrite paths gets fresh CTR blocks.
static Term atp_ri_build(const Term *flat, const u32 *subsz,
                         const u32 *flatsym, u32 pos) {
  if (flatsym[pos] < ATP_RI_CTR_BASE) return flat[pos];  // NUM / variable leaf
  Term src = flat[pos];
  u32  n   = term_ctr_n(src);
  Term children[REWRITE_MAX_ARITY];
  u8   changed = 0u;
  u32  c = pos + 1u;
  for (u32 i = 0; i < n; i++) {
    children[i] = atp_ri_build(flat, subsz, flatsym, c);
    if (children[i] != term_ctr_at(src, i)) changed = 1u;
    c += subsz[c];
  }
  if (!changed) return src;                       // subtree untouched -- share
  return term_new_ctr(term_ext(src), children, n);
}

// Splice scratch: `repl` is flattened here first so its preorder
// length is the advanced cursor -- no separate atp_symbol_count walk.
static Term g_atp_ri_repl_flat   [ATP_RI_FLAT_CAP];
static u32  g_atp_ri_repl_subsz  [ATP_RI_FLAT_CAP];
static u32  g_atp_ri_repl_flatsym[ATP_RI_FLAT_CAP];

// Splice a rewrite into the persistent flat arrays.  A normalize step
// rewrote preorder position `redex_pos` (old subtree span
// `subsz[redex_pos]`) into `repl`.  Rather than re-flatten the whole
// new term, replace the redex region in place: shift the tail, write
// `repl`'s flattening at `redex_pos`, and fan the size delta into the
// ancestors' subtree spans.  Raw-id flat symbols make every untouched
// position's flatsym splice-stable, so this is exact.  Returns 1 on
// success, 0 if the spliced length would overrun `cap` (caller then
// re-flattens from scratch).
static u8 atp_ri_splice(Term *flat, u32 *subsz, u32 *flatsym, u32 *flatlen,
                        u8 *folded, u32 redex_pos, Term repl, u32 cap) {
  u32 oldsz = subsz[redex_pos];
  // Flatten repl into the scratch arrays: its preorder length `rlen`
  // is the cursor the flatten advances, so the old separate
  // atp_symbol_count(repl) walk (one per rewrite step) is gone.
  u32 rlen  = 0u;
  if (!atp_ri_flatten(repl, g_atp_ri_repl_flat, g_atp_ri_repl_subsz,
                      g_atp_ri_repl_flatsym, folded, cap, &rlen)) {
    return 0;                                    // repl alone overruns
  }
  u32 tail  = *flatlen - redex_pos - oldsz;      // positions after the redex
  if (redex_pos + rlen + tail > cap) return 0;   // would overrun
  // Fan the size delta into every ancestor of redex_pos.  Their flatsym
  // is unchanged (the rewrite replaces a subtree, not an ancestor head)
  // but their span grows/shrinks by rlen-oldsz.  Walk the path with the
  // OLD spans; modular u32 arithmetic carries a negative delta exactly.
  u32 a = 0u;
  while (a != redex_pos) {
    u32 c = a + 1u;                              // first child position
    while (c + subsz[c] <= redex_pos) c += subsz[c];
    subsz[a] = subsz[a] + rlen - oldsz;
    a = c;
  }
  // Shift the tail [redex_pos+oldsz, flatlen) to [redex_pos+rlen, ...).
  if (tail > 0u && rlen != oldsz) {
    memmove(&flat[redex_pos + rlen],    &flat[redex_pos + oldsz],
            (size_t)tail * sizeof(Term));
    memmove(&subsz[redex_pos + rlen],   &subsz[redex_pos + oldsz],
            (size_t)tail * sizeof(u32));
    memmove(&flatsym[redex_pos + rlen], &flatsym[redex_pos + oldsz],
            (size_t)tail * sizeof(u32));
  }
  // Place the pre-flattened repl into the freed [redex_pos,
  // redex_pos+rlen) region.  flat / subsz / flatsym entries are
  // position-independent (Term cells, self-relative spans, raw
  // symbols), so a contiguous copy is exact -- and far cheaper than
  // re-walking repl's tree.
  memcpy(&flat[redex_pos],    g_atp_ri_repl_flat,
         (size_t)rlen * sizeof(Term));
  memcpy(&subsz[redex_pos],   g_atp_ri_repl_subsz,
         (size_t)rlen * sizeof(u32));
  memcpy(&flatsym[redex_pos], g_atp_ri_repl_flatsym,
         (size_t)rlen * sizeof(u32));
  *flatlen = redex_pos + rlen + tail;
  return 1;
}

// Indexed analog of `thvm_rewrite_normalize`: rewrite `t` to fixpoint
// (or step_cap) via the rule-LHS discrimination index.  The subject is
// flattened ONCE; each step SPLICES the rewrite into the persistent
// flat arrays (atp_ri_splice) -- no per-step tree rebuild, no
// re-flatten.  The tree Term is materialised ONCE, at fixpoint, by
// atp_ri_build.  A normalize is thus O(subject + sum repl + final tree)
// rather than O(steps * subject).  The incremental redex search resumes
// from the last redex (atp_ri_find_redex's `clean_before`).  Behavior-
// identical to the linear ordered scan: same outermost-leftmost redex,
// same lowest-index rule, same substitution.
static Term atp_rewrite_normalize_indexed(AtpState *s, Term t, u32 step_cap) {
  if (s->rule_index == NULL) s->rule_index = atp_ri_new();
  if (s->rule_index_dirty || s->rule_index->n_rules_built != s->n_rules) {
    atp_ri_rebuild(s);
  }
  static Term flat[ATP_RI_FLAT_CAP];
  static u32  subsz[ATP_RI_FLAT_CAP];
  static u32  flatsym[ATP_RI_FLAT_CAP];
  g_atp_ri_ix      = s->rule_index;
  g_atp_ri_flat    = flat;
  g_atp_ri_subsz   = subsz;
  g_atp_ri_flatsym = flatsym;
  g_atp_ri_lhs     = s->lhs;
  // Reset the descent's variable-binding array ONCE per normalize.  Every
  // atp_ri_descend leaves it all-NIL (each bind is unwound on backtrack),
  // so the per-query reset that used to live in atp_ri_query_pos was
  // redundant -- a 64-store loop on every one of millions of queries.
  for (u32 k = 0; k < ATP_RI_MAXVARS; k++) g_atp_ri_star[k] = ATP_RI_NIL;

  u32 flatlen = 0u;
  u8  folded  = 0u;
  // Flatten the subject into the persistent arrays.  An over-deep term
  // (> ATP_RI_FLAT_CAP nodes -- a raw critical-pair side can be) cannot
  // be flattened: the loop then takes a single linear rewrite step and
  // RE-FLATTENS, so the fast splice path resumes the instant rewriting
  // shrinks the term back under the cap (a whole-normalize linear
  // fallback would never resume).
  u8  flattened   = atp_ri_flatten(t, flat, subsz, flatsym, &folded,
                                   ATP_RI_FLAT_CAP, &flatlen);
  u32 prev_redex  = 0u;                 // 0 on the first step -> full scan
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) return t;
    if (!flattened) {
      // Over-deep: one linear rewrite, then retry the flatten.
      Term t2 = thvm_rewrite_step(t, s->lhs, s->rhs, s->n_rules);
      if (kbo_eq(t, t2)) return t;       // fixpoint
      t = t2;
      flatlen = 0u; folded = 0u;
      flattened = atp_ri_flatten(t, flat, subsz, flatsym, &folded,
                                 ATP_RI_FLAT_CAP, &flatlen);
      prev_redex = 0u;
      continue;
    }
    g_atp_ri_query_folded = folded;
    u32 redex_pos = 0u, redex_rule = 0u;
    if (!atp_ri_find_redex(flatlen, prev_redex, &redex_pos, &redex_rule)) {
      return atp_ri_build(flat, subsz, flatsym, 0u);   // no redex: fixpoint
    }
    // Build the chosen rule's substitution.  A perfect descent already
    // bound every rule variable -- g_atp_ri_best_star[k] is the preorder
    // position of the subterm bound to rule variable k -- so read it
    // straight off flat[].  thvm_match runs only when folding made the
    // descent inexact.  Rule LHS vars are dense [0,k) after
    // thvm_normalize_vars, so the star index IS the variable id.
    RewriteSubst subst = {{0}};
    if (!g_atp_ri_ix->any_folded && !folded) {
      for (u32 k = 0; k < ATP_RI_MAXVARS; k++) {
        if (g_atp_ri_best_star[k] != ATP_RI_NIL) {
          subst.bindings[k] = flat[g_atp_ri_best_star[k]];
        }
      }
    } else if (!thvm_match(s->lhs[redex_rule], flat[redex_pos], &subst)) {
      return atp_ri_build(flat, subsz, flatsym, 0u);   // unreachable: confirmed
    }
    Term repl = thvm_subst_apply(s->rhs[redex_rule], &subst);
    if (atp_ri_splice(flat, subsz, flatsym, &flatlen, &folded,
                      redex_pos, repl, ATP_RI_FLAT_CAP)) {
      prev_redex = redex_pos;
    } else {
      // The rewrite would grow the term past the cap.  Materialise the
      // current (pre-rewrite) tree -- atp_ri_splice left the flat arrays
      // untouched on overrun -- and drop to the linear branch, which
      // re-finds and applies this same redex, then re-flattens once the
      // term shrinks back under the cap.
      t = atp_ri_build(flat, subsz, flatsym, 0u);
      flattened = 0u;
    }
  }
  return flattened ? atp_ri_build(flat, subsz, flatsym, 0u) : t;
}

#ifdef ATP_ORDERED_REWRITE
// The flatterm mixed path builds on the ordered-rewrite helpers + the
// reduction-order compare, all defined further down (after atp_compare);
// forward-declare them here so the flat fast-path can call them.
static int    atp_vars_contained(Term a, Term b);
static KboCmp atp_compare(AtpState *s, Term lhs, Term rhs);
static u8     g_atp_skip_oriented;   // tentative def; initialised below
static u32    atp_pretty_term(Term t, char *buf, u32 cap);

// Env-gated derivation trace (THVM_ATP_RULE_TRACE=1).  Probes the env
// once; default builds are silent and behaviorally byte-identical.
static int atp_rule_trace_on(void) {
  static int trace_on = -1;
  if (trace_on < 0) {
    const char *e = getenv("THVM_ATP_RULE_TRACE");
    trace_on = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  }
  return trace_on;
}
static Term   atp_ordered_rewrite_step(AtpState *s, Term t,
                                       const Term *lhs, const Term *rhs,
                                       u32 n_rules, u8 *fired);
// === flatterm fast-path for the MIXED (orientable + unorientable)
// normalize loop (opt-in, THVM_ATP_FLATTERM=1, default OFF) ===========
//
// The default mixed path (atp_rewrite_normalize_ordered) alternates a
// flat indexed fixpoint (orientable rules) with ONE pointer-tree
// outermost-leftmost unorientable step (atp_ordered_rewrite_step),
// MATERIALISING a tree and RE-FLATTENING the whole subject on every
// unorientable rewrite.  On a self-overlapping axiom with ~10% of R
// unorientable, the per-step tree walk + re-flatten dominates (profiled
// at ~70% of the completion wall in atp_ordered_rewrite_step/_try_top).
//
// This variant keeps the subject in the SAME flat arrays across BOTH
// passes.  The orientable redexes are found+spliced by the existing
// discrimination-tree descent (atp_ri_find_redex / atp_ri_splice); the
// unorientable redex is found by a LINEAR PREORDER SCAN of the flat
// array (flatterm.TO_Schwanz-style next-pointer walk via subsz[]),
// rebuilding the subterm at each CTR position (atp_ri_build at pos) for
// the order-gated thvm_match.  The replacement is spliced in place --
// no per-step re-flatten of the whole subject.  The full tree is built
// ONCE, at the global fixpoint.
//
// Semantics are byte-identical to the default mixed loop:
//   * orientable fixpoint first (lowest-index rule, outermost-leftmost),
//   * then ONE unorientable step (lowest-index equation, outermost-
//     leftmost position, both directions order-gated + variable-safe),
//   * repeat to a joint fixpoint.
// The differential test (tests/test_atp.c, ATP_FLATTERM_DIFF) asserts
// equality against the tree path over random terms + rule sets.

// Run the orientable indexed fixpoint over the ALREADY-FLATTENED shared
// arrays (g_atp_ri_*), leaving the subject flat.  Mirrors the inner loop
// of atp_rewrite_normalize_indexed but takes/returns the flat state by
// reference so the unorientable pass can resume on the same array.
// `*flatlen` / `*folded` are updated in place.  Returns 1 if any
// orientable rewrite fired.
static u8 atp_ft_indexed_fixpoint(AtpState *s, Term *flat, u32 *subsz,
                                  u32 *flatsym, u32 *flatlen, u8 *folded,
                                  u32 step_cap) {
  u8  any = 0u;
  u32 prev_redex = 0u;
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) return any;
    g_atp_ri_query_folded = *folded;
    u32 redex_pos = 0u, redex_rule = 0u;
    if (!atp_ri_find_redex(*flatlen, prev_redex, &redex_pos, &redex_rule)) {
      return any;                                   // orientable fixpoint
    }
    RewriteSubst subst = {{0}};
    if (!g_atp_ri_ix->any_folded && !*folded) {
      for (u32 k = 0; k < ATP_RI_MAXVARS; k++) {
        if (g_atp_ri_best_star[k] != ATP_RI_NIL) {
          subst.bindings[k] = flat[g_atp_ri_best_star[k]];
        }
      }
    } else if (!thvm_match(s->lhs[redex_rule], flat[redex_pos], &subst)) {
      return any;                                   // unreachable: confirmed
    }
    Term repl = thvm_subst_apply(s->rhs[redex_rule], &subst);
    if (atp_ri_splice(flat, subsz, flatsym, flatlen, folded,
                      redex_pos, repl, ATP_RI_FLAT_CAP)) {
      prev_redex = redex_pos;
      any = 1u;
    } else {
      return any;                                   // overrun: caller bails
    }
  }
  return any;
}

// Linear-scan unorientable step at a single rebuilt subterm `sub` at
// preorder position `p`: tries each unorientable equation (rule asc,
// l->r then r->l), fires the first strictly order-decreasing instance.
// Returns 2 = fired (spliced), 1 = matched-firable but splice overran
// (caller bails), 0 = nothing fired here.  This is the exact fallback
// the indexed path defers to when the candidate buffer overflows or the
// index is unavailable -- byte-identical verdicts to atp_ordered_try_top.
static u8 atp_ft_unorient_at_linear(AtpState *s, Term *flat, u32 *subsz,
                                    u32 *flatsym, u32 *flatlen, u8 *folded,
                                    u32 p, Term sub) {
  for (u32 i = 0; i < s->n_rules; i++) {
    if (s->r_orient[i]) continue;                 // oriented: indexed pass
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->lhs[i], sub, &subst) &&
          atp_vars_contained(s->rhs[i], s->lhs[i])) {
        Term repl = thvm_subst_apply(s->rhs[i], &subst);
        if (atp_compare(s, sub, repl) == KBO_GT) {
          return atp_ri_splice(flat, subsz, flatsym, flatlen, folded,
                               p, repl, ATP_RI_FLAT_CAP) ? 2u : 1u;
        }
      }
    }
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->rhs[i], sub, &subst) &&
          atp_vars_contained(s->lhs[i], s->rhs[i])) {
        Term repl = thvm_subst_apply(s->lhs[i], &subst);
        if (atp_compare(s, sub, repl) == KBO_GT) {
          return atp_ri_splice(flat, subsz, flatsym, flatlen, folded,
                               p, repl, ATP_RI_FLAT_CAP) ? 2u : 1u;
        }
      }
    }
  }
  return 0u;
}

// Scan the shared flat array preorder for the leftmost-outermost
// position where some UNORIENTABLE equation fires (either direction,
// variable-safe + strictly order-decreasing for the instance).  At each
// CTR position the candidate faces are retrieved from the unorientable
// discrimination index (atp_ri_query_pos_unorient) instead of an
// O(n_rules) linear scan; the LPO order-gate (atp_compare) then runs
// ONLY on those structurally-matching candidates, in the linear scan's
// own priority (rule asc, l->r before r->l).  The first order-decreasing
// face fires -- byte-identical redex/rule/direction to the linear scan.
// On a hit, splices the replacement in place and returns 1.
//
// The candidate buffer (g_atp_ri_cand) is bounded; if a position has
// more matching faces than it holds, OR the index is absent, this falls
// back to the exact linear scan at that position (atp_ft_unorient_at_
// linear) so the verdict is never weakened.
static u8 atp_ft_unorient_step(AtpState *s, Term *flat, u32 *subsz,
                               u32 *flatsym, u32 *flatlen, u8 *folded) {
  AtpRuleIndex *ux = s->unorient_index;
  AtpRuleIndex *saved_ix = g_atp_ri_ix;
  for (u32 p = 0; p < *flatlen; p++) {
    // A leaf position (variable / NUM -- flatsym below CTR_BASE) can
    // never head a rule LHS that is a non-trivial term, so the only
    // unorientable equation that could fire is a bare `x = y`-style one,
    // which is never order-decreasing (and the orient check would have
    // dropped it).  Skip on every leaf -- O(1) flat lookup.
    if (flatsym[p] < ATP_RI_CTR_BASE) continue;
    // Materialise the subterm at preorder position p from the flat
    // arrays.  flat[p] alone is STALE once a descendant was spliced (a
    // splice rewrites the child cells + subsz/flatsym, never the
    // ancestor's flat[] tree cell), so the match/order check must run
    // against the rebuilt subtree.  An untouched subtree rebuilds to its
    // shared original at zero allocation.
    Term sub = atp_ri_build(flat, subsz, flatsym, p);
    if (ux == NULL) {
      u8 r = atp_ft_unorient_at_linear(s, flat, subsz, flatsym, flatlen,
                                       folded, p, sub);
      if (r == 2u) return 1u;
      if (r == 1u) return 0u;                       // overrun: caller bails
      continue;
    }
    g_atp_ri_ix = ux;
    g_atp_ri_query_folded = *folded;  // leaf-collect's perfect-match guard
    u32 ncand = atp_ri_query_pos_unorient(p);
    g_atp_ri_ix = saved_ix;
    if (ncand >= ATP_RI_MAXCAND) {
      // Buffer saturated -- the index dropped faces; redo this position
      // with the exact linear scan so no candidate is missed.
      u8 r = atp_ft_unorient_at_linear(s, flat, subsz, flatsym, flatlen,
                                       folded, p, sub);
      if (r == 2u) return 1u;
      if (r == 1u) return 0u;
      continue;
    }
    if (ncand == 0u) continue;                      // no firable face here
    // Sort the candidates into the linear scan's priority order.  The
    // linear scan tries each rule ascending, l->r before r->l, and fires
    // the first order-decreasing instance, so the priority key is
    // (rule << 1) | dir (dir: l->r = 0, r->l = 1) -- ascending in this
    // key reproduces (rule asc, l->r first) EXACTLY.  (The leaf rec packs
    // dir in the high bit, which is a fine encoding but the WRONG sort
    // order across rules -- r->l of rule i must precede l->r of rule i+1,
    // which the high-bit packing inverts -- so re-key here.)  N is tiny
    // (faces matching one subterm); insertion sort, stable on equal keys
    // (duplicates cannot occur -- one rec per (rule, dir)).
    static u32 cand[ATP_RI_MAXCAND];
    for (u32 k = 0; k < ncand; k++) {
      u32 packed = g_atp_ri_cand[k];
      u32 rule   = packed & ~ATP_RI_DIR_BIT;
      u32 rl     = (packed & ATP_RI_DIR_BIT) ? 1u : 0u;
      cand[k]    = (rule << 1) | rl;
    }
    for (u32 a = 1u; a < ncand; a++) {
      u32 v = cand[a];
      u32 b = a;
      while (b > 0u && cand[b - 1u] > v) { cand[b] = cand[b - 1u]; b--; }
      cand[b] = v;
    }
    for (u32 k = 0; k < ncand; k++) {
      u32 key    = cand[k];
      u32 rule   = key >> 1;
      u8  rl     = (u8)(key & 1u);
      Term pat   = rl ? s->rhs[rule] : s->lhs[rule];
      Term other = rl ? s->lhs[rule] : s->rhs[rule];
      RewriteSubst subst = {{0}};
      if (!thvm_match(pat, sub, &subst)) continue;  // index over-approx
      Term repl = thvm_subst_apply(other, &subst);
      if (atp_compare(s, sub, repl) == KBO_GT) {
        if (atp_ri_splice(flat, subsz, flatsym, flatlen, folded,
                          p, repl, ATP_RI_FLAT_CAP)) return 1u;
        return 0u;                                  // overrun: caller bails
      }
    }
  }
  return 0u;
}

// Flatterm mixed normalizer (opt-in).  Keeps the subject flat across the
// orientable indexed fixpoint AND the unorientable pass, splicing every
// rewrite in place; the tree is built ONCE at the joint fixpoint.
// Equivalent to atp_rewrite_normalize_ordered's mixed branch.  Returns
// the SAME normal form (asserted by ATP_FLATTERM_DIFF).
static Term atp_rewrite_normalize_flatterm_mixed(AtpState *s, Term t,
                                                 u32 step_cap) {
  if (s->rule_index == NULL) s->rule_index = atp_ri_new();
  // The unorientable-faces index is needed only on this (flatterm) path;
  // allocate it lazily here so the default engine never carries it.
  // atp_ri_rebuild populates it when it (re)builds rule_index.
  if (s->unorient_index == NULL) s->unorient_index = atp_ri_new();
  if (s->rule_index_dirty || s->rule_index->n_rules_built != s->n_rules ||
      s->unorient_index->n_rules_built != s->n_rules) {
    atp_ri_rebuild(s);
  }
  static Term flat[ATP_RI_FLAT_CAP];
  static u32  subsz[ATP_RI_FLAT_CAP];
  static u32  flatsym[ATP_RI_FLAT_CAP];
  g_atp_ri_ix      = s->rule_index;
  g_atp_ri_flat    = flat;
  g_atp_ri_subsz   = subsz;
  g_atp_ri_flatsym = flatsym;
  g_atp_ri_lhs     = s->lhs;
  g_atp_ri_rhs     = s->rhs;
  for (u32 k = 0; k < ATP_RI_MAXVARS; k++) g_atp_ri_star[k] = ATP_RI_NIL;

  u32 flatlen = 0u;
  u8  folded  = 0u;
  if (!atp_ri_flatten(t, flat, subsz, flatsym, &folded,
                      ATP_RI_FLAT_CAP, &flatlen)) {
    // Over-deep subject: cannot flatten -- fall back to the proven tree
    // mixed loop for this call (no semantic difference, just slower).
    for (u32 i = 0; i < step_cap; i++) {
      if (atp_norm_deadline_fired(s)) return t;
      t = atp_rewrite_normalize_indexed(s, t, step_cap);
      u8 fired = 0;
      g_atp_skip_oriented = 1u;
      Term t2 = atp_ordered_rewrite_step(s, t, s->lhs, s->rhs,
                                         s->n_rules, &fired);
      g_atp_skip_oriented = 0u;
      if (!fired) break;
      t = t2;
    }
    return t;
  }

  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) break;
    // The indexed fixpoint may overrun on a splice; on overrun it stops
    // having done partial work, the unorientable pass would too, so we
    // materialise and hand the rest to the tree loop.  Detect overrun by
    // re-checking after: a clean fixpoint leaves no orientable redex.
    atp_ft_indexed_fixpoint(s, flat, subsz, flatsym, &flatlen, &folded,
                            step_cap);
    u8 fired = atp_ft_unorient_step(s, flat, subsz, flatsym, &flatlen,
                                    &folded);
    if (!fired) break;          // joint fixpoint (or splice overrun: rare)
  }
  return atp_ri_build(flat, subsz, flatsym, 0u);
}

#ifdef ATP_FLATTERM_SELFCHECK
// The exact tree mixed loop (a copy of atp_rewrite_normalize_ordered's
// mixed branch), used only by the build-time self-check to confirm the
// flatterm path's normal form matches.  Defeats the speedup; never
// compiled into a release build.
static Term atp_rewrite_normalize_flatterm_selfcheck_tree(AtpState *s, Term t,
                                                          u32 step_cap) {
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) return t;
    t = atp_rewrite_normalize_indexed(s, t, step_cap);
    u8 fired = 0;
    g_atp_skip_oriented = 1u;
    Term t2 = atp_ordered_rewrite_step(s, t, s->lhs, s->rhs,
                                       s->n_rules, &fired);
    g_atp_skip_oriented = 0u;
    if (!fired) break;
    t = t2;
  }
  return t;
}
#endif
#endif // ATP_ORDERED_REWRITE

// === CP-generation overlap-partner index (WM U1_KPsBildenZuRegel) ===
//
// THE CP-GEN CROSS-PRODUCT.  thvm_atp_generate_cps_c overlaps a new rule
// i into every existing rule j -- for j over ALL n_rules it calls
// atp_overlap_ij -> thvm_critical_pairs_pair, which walks the positions
// of li and thvm_unify's rule-j's lhs lj there.  A CP is emitted only
// when lj unifies with a non-variable subterm of li, but the scan PAYS
// for every j whether or not any overlap exists -- an O(n_rules)-per-rule
// scan.  Waldmeister avoids it via the discrimination tree: for a new
// rule it forms overlaps only against the rules the tree retrieves as
// unifiable (sources/INF/Unifikation1.c:1480 U1_KPsBildenZuRegel ->
// TermMitDSBaumUnifizieren / TermMitDSBaumTeiltermenUnifizieren on
// RE_Regelbaum).
//
// MEASURED SCOPE -- single-symbol theories defeat the filter.  On the
// Sheffer-stroke axiom set (one binary symbol `nand`; AndAssociativity /
// DoubleNegation over WolframAxioms) EVERY rule LHS is nand-headed and
// every overlap subterm is nand-headed, so the discrimination tree cannot
// discriminate at the shallow positions -- the candidate set equals the
// full rule set (cand == n, measured) and the filter saves nothing.  The
// profiled cost on that problem is NOT the cross-product scan but the
// per-generated-CP work: CP reduction (atp_rewrite_normalize) plus the
// KBO atp_compare in the ordered-rewrite order-gate, both proportional to
// CPs GENERATED, which an overlap filter cannot reduce while preserving
// the CP set.  The index is therefore a no-op-cost, CP-set-identical
// speedup ONLY for MULTI-symbol theories, where distinct LHS heads let
// the tree prune the cross-product.  Kept gated off; reported honestly.
//
// THE PORT -- a candidate FILTER, CP-set-preserving.  cp_index is a
// discrimination tree over the WHOLE rule-LHS terms (reusing atp_ri_*'s
// node/rec pools, atp_ri_flatsym first-appearance scheme, atp_ri_child
// insert).  For the new rule i, the partners j the cross-product can
// emit a CP for are exactly { j : lj unifies with some non-var subterm
// of li }.  atp_cp_index_collect descends cp_index in UNIFICATION mode
// against a query subterm and gathers every rule whose stored LHS could
// unify -- a SUPERSET of the true partners (a discrimination tree on the
// flat symbol string cannot decide unifiability exactly, only filter).
// thvm_atp_generate_cps_c then runs the EXACT atp_overlap_ij on each
// candidate; cp_visit's thvm_unify is the authoritative gate, so the CP
// set is byte-identical to the unindexed scan -- the index only skips
// the j's that provably cannot overlap.
//
// UNIFICATION DESCENT (vs atp_ri_descend's one-way match).  Both the
// stored LHS and the query subterm carry variables, so at each position:
//   - stored STAR (rule var): unifies with the whole query subterm ->
//     descend the STAR child, skipping the query subterm.
//   - query STAR (subject var): unifies with anything stored -> the
//     query consumed one position; the stored side may be ANY subtree,
//     so descend EVERY child (CTR children with their subtree skipped,
//     and the STAR child).
//   - stored CTR == query CTR: descend (head consumed on both sides).
// No variable-consistency check is applied (that would need an occurs-
// /binding-aware unify; the filter stays a sound superset without it).

#define ATP_CP_MAXCAND 65536u
static u32 g_atp_cp_cand[ATP_CP_MAXCAND];
static u32 g_atp_cp_ncand     = 0;
static u8  g_atp_cp_overflow  = 0;     // candidate buffer overflowed -> full scan
// De-dup: a rule reached via several positions/branches is collected once.
// g_atp_cp_seen[rule] == g_atp_cp_epoch marks "already in g_atp_cp_cand".
static u32 *g_atp_cp_seen   = NULL;
static u32  g_atp_cp_seencap = 0;
static u32  g_atp_cp_epoch  = 0;

static AtpRuleIndex *g_atp_cp_ix      = NULL;
static const Term   *g_atp_cp_qflat   = NULL;
static const u32    *g_atp_cp_qsubsz  = NULL;
static const u32    *g_atp_cp_qflatsym = NULL;

static void atp_cp_cand_add(u32 rule) {
  if (rule < g_atp_cp_seencap && g_atp_cp_seen[rule] == g_atp_cp_epoch) return;
  if (rule < g_atp_cp_seencap) g_atp_cp_seen[rule] = g_atp_cp_epoch;
  if (g_atp_cp_ncand >= ATP_CP_MAXCAND) { g_atp_cp_overflow = 1u; return; }
  g_atp_cp_cand[g_atp_cp_ncand++] = rule;
}

static int atp_cp_cand_cmp(const void *a, const void *b) {
  u32 x = *(const u32 *)a, y = *(const u32 *)b;
  return (x > y) - (x < y);
}

// Sort the collected candidates ascending so the indexed generator
// processes overlap pairs in the SAME (j ascending) order the unindexed
// n_rules scan did -- a CP's FIFO trace tiebreak then matches, keeping
// the derived-rule sequence byte-identical.
static void atp_cp_cand_sort(void) {
  qsort(g_atp_cp_cand, g_atp_cp_ncand, sizeof(u32), atp_cp_cand_cmp);
}

static void atp_cp_index_leaf(u32 node) {
  AtpRuleIndex *ix = g_atp_cp_ix;
  for (u32 r = ix->nodes[node].rec_head; r != ATP_RI_NIL;
       r = ix->recs[r].next) {
    atp_cp_cand_add(ix->recs[r].rule);
  }
}

// Collect EVERY rule in the whole subtree rooted at `node` -- the action
// when a query variable consumes the corresponding stored subtree (a var
// unifies with anything, so any stored continuation is a candidate).
static void atp_cp_index_collect_subtree(u32 node) {
  AtpRuleIndex *ix = g_atp_cp_ix;
  atp_cp_index_leaf(node);
  for (u32 c = ix->nodes[node].child; c != ATP_RI_NIL;
       c = ix->nodes[c].sibling) {
    if (g_atp_cp_overflow) return;
    atp_cp_index_collect_subtree(c);
  }
}

// Descend cp_index from `node` against the flat query subject slice
// starting at preorder position `pos` (which spans the query subterm).
// Collects every reachable leaf's rules (unification-compatible filter).
static void atp_cp_index_descend(u32 node, u32 pos, u32 qend) {
  AtpRuleIndex *ix = g_atp_cp_ix;
  if (pos == qend) { atp_cp_index_leaf(node); return; }
  u32 qsym = g_atp_cp_qflatsym[pos];
  u8  q_is_star = (qsym >= ATP_RI_STAR_BASE && qsym < ATP_RI_CTR_BASE);
  u32 sz   = g_atp_cp_qsubsz[pos];
  for (u32 c = ix->nodes[node].child; c != ATP_RI_NIL;
       c = ix->nodes[c].sibling) {
    if (g_atp_cp_overflow) return;
    u32 csym = ix->nodes[c].sym;
    if (csym >= ATP_RI_STAR_BASE && csym < ATP_RI_CTR_BASE) {
      // Stored rule var unifies with the whole query subterm at `pos`:
      // the stored side consumed one tree edge (the STAR), the query
      // consumed its whole subterm (sz positions).
      atp_cp_index_descend(c, pos + sz, qend);
    } else if (csym == qsym) {
      // Stored CTR/NUM head equals the query head: consume both heads
      // (one tree edge, one query position).
      atp_cp_index_descend(c, pos + 1u, qend);
    }
    // A non-matching stored CTR vs a concrete query CTR cannot unify --
    // pruned (the discrimination-tree win).
  }
  if (q_is_star) {
    // Query var unifies with ANY stored subtree at this node: collect
    // every rule reachable below `node` (var-vs-anything).  The query
    // already advanced past the var (a single position) at the caller's
    // continuation; here we simply harvest the whole stored remainder.
    atp_cp_index_collect_subtree(node);
  }
}

// Build cp_index from s->lhs[0..n_rules): one whole-LHS term per rule,
// keyed by the atp_ri_flatsym first-appearance symbol string.  Rebuilt
// (like rule_index) whenever R mutates.
static void atp_cp_index_rebuild(AtpState *s) {
  AtpRuleIndex *ix = s->cp_index;
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->root    = atp_ri_node_new(ix, ATP_RI_NIL);
  AtpRiVarMap vm;
  for (u32 i = 0; i < s->n_rules; i++) {
    atp_ri_varmap_reset(&vm);
    u32 node = atp_ri_insert_term(ix, ix->root, s->lhs[i], &vm);
    u32 rec  = atp_ri_rec_new(ix);
    ix->recs[rec].rule    = i;
    ix->recs[rec].next    = ix->nodes[node].rec_head;
    ix->nodes[node].rec_head = rec;
  }
  ix->n_rules_built = s->n_rules;
  if (g_atp_cp_seencap < s->n_rules) {
    u32 cap = g_atp_cp_seencap ? g_atp_cp_seencap : 1024u;
    while (cap < s->n_rules) cap *= 2u;
    u32 *p = (u32 *)realloc(g_atp_cp_seen, (size_t)cap * sizeof(u32));
    if (p == NULL) { fprintf(stderr, "atp_cp_index: seen OOM\n"); exit(1); }
    g_atp_cp_seen   = p;
    for (u32 k = g_atp_cp_seencap; k < cap; k++) g_atp_cp_seen[k] = 0u;
    g_atp_cp_seencap = cap;
  }
}

// Collect the candidate partner rules for overlapping a new rule whose
// LHS is `li`: every rule j with lj unifiable with a non-var subterm of
// li.  `li` is flattened once; each non-var position queries cp_index.
// Returns the count (in g_atp_cp_cand) or sets g_atp_cp_overflow.
static u32 atp_cp_index_collect(AtpState *s, Term li) {
  static Term qflat[ATP_RI_FLAT_CAP];
  static u32  qsubsz[ATP_RI_FLAT_CAP];
  static u32  qflatsym[ATP_RI_FLAT_CAP];
  u8  folded = 0;
  u32 pos    = 0;
  if (!atp_ri_flatten(li, qflat, qsubsz, qflatsym, &folded,
                      ATP_RI_FLAT_CAP, &pos)) {
    g_atp_cp_overflow = 1u;            // subject too deep -> exact scan
    return 0;
  }
  g_atp_cp_ix       = s->cp_index;
  g_atp_cp_qflat    = qflat;
  g_atp_cp_qsubsz   = qsubsz;
  g_atp_cp_qflatsym = qflatsym;
  g_atp_cp_ncand    = 0;
  g_atp_cp_overflow = 0;
  if (++g_atp_cp_epoch == 0u) {        // wrapped: clear so stale != epoch
    for (u32 k = 0; k < g_atp_cp_seencap; k++) g_atp_cp_seen[k] = 0u;
    g_atp_cp_epoch = 1u;
  }
  // Every non-variable subterm position of li is a candidate overlap
  // site -- cp_visit walks exactly these (it skips TAG_FVR).  Querying
  // each gathers the rules whose lhs could unify there.
  for (u32 p = 0; p < pos; p++) {
    u32 qsym = qflatsym[p];
    if (qsym >= ATP_RI_STAR_BASE && qsym < ATP_RI_CTR_BASE) continue; // var
    u32 qend = p + qsubsz[p];
    atp_cp_index_descend(s->cp_index->root, p, qend);
    if (g_atp_cp_overflow) return 0;
  }
  return g_atp_cp_ncand;
}

// Insert every NON-VAR subterm position of `t` (under one shared
// first-appearance varmap, so a rule's repeated var is consistent across
// its subterms) as a separate tree path keyed to `rule`.  A var-headed
// subterm is skipped (cp_visit never overlaps at a variable position).
static void atp_cp_subindex_insert(AtpRuleIndex *ix, Term t, u32 rule,
                                   AtpRiVarMap *vm) {
  if (term_tag(t) == TAG_CTR) {
    u32 node = atp_ri_insert_term(ix, ix->root, t, vm);
    u32 rec  = atp_ri_rec_new(ix);
    ix->recs[rec].rule       = rule;
    ix->recs[rec].next       = ix->nodes[node].rec_head;
    ix->nodes[node].rec_head = rec;
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) {
      atp_cp_subindex_insert(ix, term_ctr_at(t, i), rule, vm);
    }
  }
}

// Build cp_subindex: every non-var subterm of every rule LHS -> rule.
static void atp_cp_subindex_rebuild(AtpState *s) {
  AtpRuleIndex *ix = s->cp_subindex;
  ix->n_nodes = 0;
  ix->n_recs  = 0;
  ix->root    = atp_ri_node_new(ix, ATP_RI_NIL);
  AtpRiVarMap vm;
  for (u32 i = 0; i < s->n_rules; i++) {
    atp_ri_varmap_reset(&vm);
    atp_cp_subindex_insert(ix, s->lhs[i], i, &vm);
  }
  ix->n_rules_built = s->n_rules;
}

// Collect candidate partner rules for the (old i x new j) direction:
// every rule i whose LHS li has a NON-VAR subterm unifiable with the new
// rule's whole LHS `lj`.  Query the subterm index once with `lj`.
static u32 atp_cp_subindex_collect(AtpState *s, Term lj) {
  static Term qflat[ATP_RI_FLAT_CAP];
  static u32  qsubsz[ATP_RI_FLAT_CAP];
  static u32  qflatsym[ATP_RI_FLAT_CAP];
  u8  folded = 0;
  u32 pos    = 0;
  if (!atp_ri_flatten(lj, qflat, qsubsz, qflatsym, &folded,
                      ATP_RI_FLAT_CAP, &pos)) {
    g_atp_cp_overflow = 1u;
    return 0;
  }
  g_atp_cp_ix       = s->cp_subindex;
  g_atp_cp_qflat    = qflat;
  g_atp_cp_qsubsz   = qsubsz;
  g_atp_cp_qflatsym = qflatsym;
  g_atp_cp_ncand    = 0;
  g_atp_cp_overflow = 0;
  if (++g_atp_cp_epoch == 0u) {
    for (u32 k = 0; k < g_atp_cp_seencap; k++) g_atp_cp_seen[k] = 0u;
    g_atp_cp_epoch = 1u;
  }
  atp_cp_index_descend(s->cp_subindex->root, 0u, pos);
  if (g_atp_cp_overflow) return 0;
  return g_atp_cp_ncand;
}

#endif // ATP_RULE_INDEX

fn AtpState *thvm_atp_init(const KboConfig *cfg, u32 step_cap) {
  AtpState *s = (AtpState *)calloc(1, sizeof(AtpState));
  if (s == NULL) return NULL;
  // Persistent LPO memo: a completion compares the same subterm pairs
  // millions of times.  Opt in, and drop any entries from a prior run
  // (a static LpoConfig pointer may be reused with new precedence).
  thvm_lpo_set_persist(1u);
  thvm_lpo_invalidate();
  s->kbo      = cfg;
  s->step_cap = step_cap;
  // CP-priority weight: the ordering-directed GT heuristic is the
  // engine default -- it cuts the corpus saturation step count and
  // proves the distance-1 CriticalPairLemma cpl1 in a single step
  // (the symbol-count ADD heuristic took 21).  ADD stays reachable
  // via thvm_atp_set_cp_weight_mode for callers that want the bare
  // symbol-count sum.
  s->cp_weight_mode = ATP_CP_WEIGHT_GT;
  // Right-reduction (DISCOUNT-loop composition) is on by default:
  // interreduce keeps surviving rules' RHSs in normal form so the
  // CPs born from them stay small.  calloc zeroed it; set explicitly.
  s->right_reduce = 1u;
  atp_register_primitives();
  acp_selftest();   // verify the Stringterms pack/unpack round-trip
  // Allocate the growable rule / CP arrays at their initial
  // capacity.  ensure_*_cap fills the trace slots with
  // ATP_TRACE_NONE (0 is a valid trace index, so explicit fill
  // is required); a fresh array starts with r_cap == 0 so the
  // helper treats the whole span as new.
  atp_ensure_rule_cap(s, ATP_INIT_RULES);
  atp_ensure_cp_cap(s, ATP_INIT_CPS);
  // Proof-trace soft cap.  Default ATP_MAX_TRACE keeps the drop
  // behavior byte-identical; THVM_ATP_TRACE_MAX raises it so a deep
  // (1601-rule) completion can still record every rule's lineage.
  s->t_max = ATP_MAX_TRACE;
  {
    const char *tm = getenv("THVM_ATP_TRACE_MAX");
    if (tm != NULL && tm[0] != '\0') {
      long v = strtol(tm, NULL, 10);
      if (v > 0) s->t_max = (u32)v;
    }
  }
#ifdef ATP_CP_GRAPH
  // 8a: start cp_graph as the empty CpSet[] -- n_cps is 0 here, so
  // the array mirror and the graph agree from the first instant.
  atp_cp_graph_sync(s);
#endif
#ifdef ATP_FV_INDEX
  // 7d: an empty FV subsumption index; CPs enter it on enqueue.
  s->fv_index = atp_fv_index_new();
#endif
#ifdef ATP_RULE_INDEX
  // 7e lever 2: the rule-LHS redex index is built lazily on the first
  // indexed normalize -- start it NULL, dirty so the first build
  // fires.  (calloc already zeroed both, the explicit set documents
  // intent.)
  s->rule_index       = NULL;
  s->unorient_index   = NULL;     // lazily built on the flatterm path
  s->cp_index         = NULL;     // lazily built on the first indexed CP gen
  s->cp_subindex      = NULL;     // companion subterm index (old i x new j)
  s->rule_index_dirty = 1u;
  // Opt-in CP-generation overlap-partner index.  OFF unless
  // THVM_ATP_CP_INDEX is set non-"0"; the default engine scans all
  // n_rules per new rule (byte-identical CP set either way).
  {
    const char *ci = getenv("THVM_ATP_CP_INDEX");
    s->use_cp_index = (ci != NULL && ci[0] != '\0' && ci[0] != '0') ? 1u : 0u;
  }
  // Opt-in flatterm fast-path for the mixed normalize loop.  OFF unless
  // THVM_ATP_FLATTERM is set to a non-"0" value -- the default engine
  // stays byte-identical (the tree mixed loop).
  {
    const char *ft = getenv("THVM_ATP_FLATTERM");
    s->use_flatterm = (ft != NULL && ft[0] != '\0' && ft[0] != '0') ? 1u : 0u;
  }
#endif
  return s;
}

#ifdef ATP_MNF
static void mnf_destroy(struct AtpMnf *m);   // defined with the MNF module below
// GC support: the MNF coloured nodes hold reached Terms on the heap, so
// they are collector roots.  Defined with the MNF module; declared here
// for thvm_atp_gc_collect (which precedes the module).
static u32  mnf_gc_count(struct AtpMnf *m);
static void mnf_gc_gather(struct AtpMnf *m, Term *roots, u32 *w);
static void mnf_gc_writeback(struct AtpMnf *m, const Term *roots, u32 base);
#endif

fn void thvm_atp_free(AtpState *s) {
  if (s == NULL) return;
  free(s->lhs);
  free(s->rhs);
  free(s->r_trace);
  free(s->r_orient);
  free(s->trace);
  // Each cp_packed[] slot is a malloc'd byte string the queue owns;
  // free every non-NULL slot (free(NULL) is a no-op) then the array.
  if (s->cp_packed != NULL) {
    for (u32 i = 0; i < s->cp_cap; i++) free(s->cp_packed[i]);
    free(s->cp_packed);
  }
  free(s->cp_trace);
  free(s->cp_pri);
  free(s->cp_goal);
  free(s->cp_seq);
  // Auto-MaxWeight overflow stash: each packed byte string is owned
  // here too (free(NULL) is a no-op for slots already drained).
  if (s->cp_stash_packed != NULL) {
    for (u32 i = 0; i < s->n_cp_stash; i++) free(s->cp_stash_packed[i]);
    free(s->cp_stash_packed);
  }
  free(s->cp_stash_trace);
  free(s->cp_stash_nodes);
  // ENIGMA training-data arrays (NULL unless recording was enabled).
  free(s->cp_feat_rows);
  free(s->cp_feat_trace);
  free(s->cp_feat_label);
#ifdef ATP_FV_INDEX
  atp_fv_index_free(s->fv_index);
#endif
#ifdef ATP_RULE_INDEX
  atp_ri_free(s->rule_index);
  atp_ri_free(s->unorient_index);
  atp_ri_free(s->cp_index);
  atp_ri_free(s->cp_subindex);
#endif
#ifdef ATP_MNF
  mnf_destroy(s->mnf);
#endif
  free(s);
}

// === 9.3: heap checkpoint/reset =====================================
fn u64 thvm_atp_heap_checkpoint(void) {
  return HEAP_NEXT;
}

fn void thvm_atp_heap_reset(u64 checkpoint) {
  // Only allow popping back; never advance (callers should use
  // term_new_* for that).  Silent no-op on out-of-range to make
  // the API safe to sprinkle in step paths.
  if (checkpoint <= HEAP_NEXT) HEAP_NEXT = checkpoint;
}

// === 7a: in-loop GC for the saturation engine ======================
//
// thvm_atp_heap_checkpoint/reset only reclaims the per-step
// normalization scratch when a CP is trivially joined.  Every
// rule / CP / trace Term that survives a step is a heap-resident
// cell that lives forever, so a long completion run otherwise
// bumps HEAP_NEXT until heap_alloc reports "from-space exhausted".
//
// The fix: gather every live Term reachable from the AtpState into
// a root array and hand it to the Cheney collector (gc_collect).
// The collector evacuates each root to to-space, rewrites the root
// slot with the moved location, and swaps semi-spaces -- exactly
// the discipline `thvm_realize` uses for the WL session.  Writing
// the moved Terms back into the AtpState arrays keeps the engine
// pointing at the live copies.
//
// Live Term fields rooted here:
//   - lhs[0..n_rules), rhs[0..n_rules)        -- the rule set R
//   - goal_lhs, goal_rhs                      -- the conjecture
//   - trace[0..n_trace)                       -- TAG_CTR entries
//                                                (each holds lhs/rhs)
//   - witness_subst.bindings[0..REWRITE_MAX_VAR) -- narrowing σ
//
// The CP queue is NOT rooted: after the Waldmeister Stringterms port
// each queued CP is a packed byte string in plain malloc memory
// (cp_packed[]), outside the managed heap, so the collector never
// touches it.  This is the structural fix for the late-game GC wall.
// The subsumption-index records borrow those byte strings (u8 *, not
// Term), so the index needs no rooting either.
//
// Returns 1 if a collection ran, 0 if GC is disabled / no state.
fn u8 thvm_atp_gc_collect(AtpState *s) {
  if (s == NULL || !gc_enabled()) return 0;

  // Count the root slots so we can size the array exactly.
  u32 n_roots = 2u * s->n_rules + 4u /* goal + goal_nf */
              + s->n_trace + REWRITE_MAX_VAR;
#ifdef ATP_CP_GRAPH
  // 8b: cp_graph is now a thing reductions act on (atp_normalize_graph
  // rewrites it), so its CTR cells must be relocated by the collector,
  // not rebuilt afterward.  One extra root slot for the CpSet term.
  n_roots += 1u;
#endif
#ifdef ATP_MNF
  // Milestone 10: every MNF coloured node holds a reached Term.  Root
  // them so the collector relocates them; the hash table (structural
  // hashes, node indices) is GC-invariant and needs no fixup.
  n_roots += mnf_gc_count(s->mnf);
#endif
  Term *roots = (Term *)malloc((size_t)n_roots * sizeof(Term));
  if (roots == NULL) return 0;

  u32 w = 0;
  for (u32 i = 0; i < s->n_rules; i++) {
    roots[w++] = s->lhs[i];
    roots[w++] = s->rhs[i];
  }
  roots[w++] = s->goal_lhs;
  roots[w++] = s->goal_rhs;
  roots[w++] = s->goal_lhs_nf;
  roots[w++] = s->goal_rhs_nf;
  for (u32 i = 0; i < s->n_trace; i++) roots[w++] = s->trace[i];
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
    roots[w++] = s->witness_subst.bindings[i];
  }
#ifdef ATP_CP_GRAPH
  u32 cp_graph_root = w;
  roots[w++] = s->cp_graph;
#endif
#ifdef ATP_MNF
  u32 mnf_node_root = w;
  mnf_gc_gather(s->mnf, roots, &w);
#endif

  gc_collect(roots, w);

  // Write the relocated Terms back into the AtpState in the same
  // order they were gathered.
  w = 0;
  for (u32 i = 0; i < s->n_rules; i++) {
    s->lhs[i] = roots[w++];
    s->rhs[i] = roots[w++];
  }
  s->goal_lhs = roots[w++];
  s->goal_rhs = roots[w++];
  s->goal_lhs_nf = roots[w++];
  s->goal_rhs_nf = roots[w++];
  for (u32 i = 0; i < s->n_trace; i++) s->trace[i] = roots[w++];
  for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
    s->witness_subst.bindings[i] = roots[w++];
  }
#ifdef ATP_CP_GRAPH
  // 8b: cp_graph was rooted, so the collector relocated its CpSet +
  // Cp[] cells in place.  Write the moved CpSet back; a debug
  // assertion confirms it still decodes to the cp_packed[] queue
  // (unpacked) -- no rebuild-everything pass.
  s->cp_graph = roots[cp_graph_root];
  atp_cp_graph_assert(s);
#endif
#ifdef ATP_MNF
  // Write the relocated reached-Terms back into the MNF nodes.  No
  // hash-table fixup: mnf_hash is structural, so a relocated term
  // keeps its hash and stays in its bucket.
  mnf_gc_writeback(s->mnf, roots, mnf_node_root);
#endif

  free(roots);

  // Cells moved: the persistent LPO (s,t)->verdict memo is keyed on cell
  // addresses, now stale -- drop it.
  thvm_lpo_invalidate();
  return 1;
}

// Trigger threshold: collect when the from-space bump cursor has
// crossed this fraction of the live semi-space.  Half-full mirrors
// the default trigger in `thvm_realize` (realize.c).
static u8 atp_heap_under_pressure(void) {
  if (!gc_enabled()) return 0;
  u64 lo   = gc_from_start();
  u64 hi   = gc_from_end();
  u64 half = lo + (hi - lo) / 2;
  return HEAP_NEXT > half;
}

// Grow the heap-allocated trace[] to hold at least one more entry,
// honoring the s->t_max soft cap.  Returns 1 if there is room for the
// next push, 0 if the cap is hit (caller returns ATP_TRACE_NONE).
static int atp_trace_ensure(AtpState *s) {
  if (s->n_trace >= s->t_max) return 0;
  if (s->n_trace < s->t_cap) return 1;
  u32 cap = s->t_cap ? s->t_cap * 2u : 4096u;
  if (cap > s->t_max) cap = s->t_max;
  Term *nt = (Term *)realloc(s->trace, (size_t)cap * sizeof(Term));
  if (nt == NULL) return 0;
  s->trace = nt;
  s->t_cap = cap;
  return 1;
}

// Push a trace entry as a TAG_CTR with label = reason and children
// [NUM(parent_a), NUM(parent_b), lhs, rhs].  Returns the entry's
// index in s->trace, or ATP_TRACE_NONE if the buffer is full.
//
// 6.1b/c will wire this into add_equation / orient_and_add /
// generate_cps; for 6.1a the helper just exists, and the storage is
// init'd to zero by thvm_atp_init's calloc.
static u32 atp_trace_push(AtpState *s, u32 reason, u32 p_a, u32 p_b,
                          Term lhs, Term rhs) {
  if (s == NULL || !atp_trace_ensure(s)) return ATP_TRACE_NONE;
  Term children[4] = {
    term_new(0, TAG_NUM, 0, p_a),
    term_new(0, TAG_NUM, 0, p_b),
    lhs,
    rhs,
  };
  s->trace[s->n_trace] = term_new_ctr(reason, children, 4);
  u32 idx = s->n_trace;
  s->n_trace++;
  return idx;
}

// Push a TRACE_CP entry that also carries the superposition position
// -- pos[0..pos_len) is the path into parent_a's rule lhs where
// parent_b's rule lhs overlapped (CriticalPair.pos).  The entry is a
// TAG_CTR(TRACE_CP) with children
//   [NUM(p_a), NUM(p_b), lhs, rhs, NUM(pos_len), NUM(pos_0), ...].
// Children 0..3 match a plain atp_trace_push entry, so the trace
// serializer, GC root walk, and orphan-kill scan -- all of which
// touch only the first four children -- are unaffected; the proof
// DAG reads the overlap geometry off children 4+.
static u32 atp_trace_push_cp(AtpState *s, u32 p_a, u32 p_b,
                             Term lhs, Term rhs,
                             const u8 *pos, u8 pos_len) {
  if (s == NULL || !atp_trace_ensure(s)) return ATP_TRACE_NONE;
  Term children[5 + CP_MAX_DEPTH];
  children[0] = term_new(0, TAG_NUM, 0, p_a);
  children[1] = term_new(0, TAG_NUM, 0, p_b);
  children[2] = lhs;
  children[3] = rhs;
  children[4] = term_new(0, TAG_NUM, 0, pos_len);
  for (u8 k = 0; k < pos_len; k++) {
    children[5 + k] = term_new(0, TAG_NUM, 0, pos[k]);
  }
  s->trace[s->n_trace] = term_new_ctr(TRACE_CP, children, 5u + pos_len);
  u32 idx = s->n_trace;
  s->n_trace++;
  return idx;
}

// Push an axiom / pending equation onto the CP queue.  The
// saturation loop's orient + generate machinery processes it
// uniformly with later-derived CPs.  Also records a TRACE_AXIOM
// entry so the proof trace (stage 6.1) can identify this CP's
// origin downstream.  The CP queue is growable, so this never
// rejects for being full; returns 1 on success, 0 only on NULL
// state or a sort-check rejection.
// Shared body: sort-check, var-normalize, push a trace entry with
// the given `reason` / `parent_a`, and enqueue the equation as a CP.
// thvm_atp_add_equation enqueues an input axiom (TRACE_AXIOM, no
// parent); the interreduce re-queue path uses TRACE_SIMPLIFY with
// the dropped rule's trace index so the proof DAG stays connected.
static u8 atp_enqueue_equation(AtpState *s, Term lhs, Term rhs,
                               u32 reason, u32 parent_a) {
  if (s == NULL) return 0;
  // 8.4d: when a WaldSpec is attached, reject ill-sorted inputs
  // before mutating state.  Each side must be well-sorted AND
  // both sides must share the same sort (an equation l = r in
  // a sorted signature requires `sort(l) == sort(r)`).
  // Homogeneous-mode (NULL spec or n_sorts == 0) returns sort 0
  // from wald_term_sort unconditionally so the gate is a no-op.
  if (s->spec != NULL) {
    u32 sl = wald_term_sort(s->spec, lhs);
    u32 sr = wald_term_sort(s->spec, rhs);
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
#ifdef ATP_VAR_NORM
  // 7c: an equation enters the engine as a queued CP.  Canonicalize
  // its variables here so the very first CP, like every later-
  // derived one, carries a dense [0, k) variable set.
  thvm_normalize_vars(&lhs, &rhs);
#endif
  u32 trace_idx = atp_trace_push(s, reason, parent_a,
                                 ATP_TRACE_NONE, lhs, rhs);
  atp_cp_heap_push(s, lhs, rhs, trace_idx);
  return 1;
}

fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
  return atp_enqueue_equation(s, lhs, rhs, TRACE_AXIOM, ATP_TRACE_NONE);
}

// Re-queue a simplified older rule (interreduce path).  Records a
// TRACE_SIMPLIFY entry whose parent_a is the dropped rule's trace
// index, so a proof consumer can replay the reduction chain instead
// of treating the equation as an unjustified axiom.
static u8 atp_add_equation_simplified(AtpState *s, Term lhs, Term rhs,
                                      u32 parent_trace) {
  return atp_enqueue_equation(s, lhs, rhs, TRACE_SIMPLIFY, parent_trace);
}

// Set the conjecture (single equation goal_lhs == goal_rhs).
// Calling with goal_lhs == 0 clears the goal (completion mode).
// Returns 1 on success, 0 if 8.4d's sort-check rejected the goal.
fn u8 thvm_atp_set_goal(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  // Clearing the goal: lhs == 0 means "completion mode", always
  // accepted regardless of sort-check.
  if (lhs == 0) {
    s->goal_lhs = 0;
    s->goal_rhs = 0;
    return 1;
  }
  // 8.4d: gate on sort-check when a spec is attached -- both
  // sides must be well-sorted AND share the same sort.
  if (s->spec != NULL) {
    u32 sl = wald_term_sort(s->spec, lhs);
    u32 sr = wald_term_sort(s->spec, rhs);
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
  s->goal_lhs = lhs;
  s->goal_rhs = rhs;
  return 1;
}

// 8.4d: attach a WaldSpec for sort-check gating.
fn void thvm_atp_set_spec(AtpState *s, const struct WaldSpec *spec) {
  if (s == NULL) return;
  s->spec = spec;
}

// 8.5c: attach an LpoConfig.  When non-NULL, orient_and_add
// dispatches to thvm_lpo instead of thvm_kbo.
fn void thvm_atp_set_lpo(AtpState *s, const LpoConfig *lpo) {
  if (s == NULL) return;
  s->lpo = lpo;
}

// Milestone 10: runtime gate for the MNF goal-directed front search.
// No-op effect unless the dylib was compiled with -DATP_MNF -- the
// field is always present, but goal_check only reads it inside its
// `#ifdef ATP_MNF` block.
fn void thvm_atp_set_use_mnf(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_mnf = on ? 1u : 0u;
}

fn void thvm_atp_set_use_ground_join(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_ground_join = on ? 1u : 0u;
}

// Bachmair-Dershowitz connectedness CP deletion (Twee section 6.2).
// Default OFF -> the engine is byte-identical; the WL surface flips it
// for Method -> {... "Connectedness" -> True} and the Waldmeister preset.
fn void thvm_atp_set_use_connectedness(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_connectedness = on ? 1u : 0u;
}

// Waldmeister-faithful RHS interreduction (Interreduktion.c
// RMRechtsInterred).  See the AtpState.use_rhs_interreduce comment.
fn void thvm_atp_set_use_rhs_interreduce(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_rhs_interreduce = on ? 1u : 0u;
}

// Unfailing-completion both-faces superposition.  See the
// AtpState.use_unfailing_cp comment.
fn void thvm_atp_set_use_unfailing_cp(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->use_unfailing_cp = on ? 1u : 0u;
}

fn void thvm_atp_set_use_flatterm(AtpState *s, u8 on) {
  if (s == NULL) return;
#ifdef ATP_RULE_INDEX
  s->use_flatterm = on ? 1u : 0u;
#else
  (void)on;   // no flat machinery without the rule index
#endif
}

fn void thvm_atp_set_use_cp_index(AtpState *s, u8 on) {
  if (s == NULL) return;
#ifdef ATP_RULE_INDEX
  s->use_cp_index = on ? 1u : 0u;
#else
  (void)on;   // no index machinery without the rule index
#endif
}

fn void thvm_atp_set_selection_ratio(AtpState *s, u32 modulo) {
  if (s == NULL) return;
  s->fifo_modulo = modulo;   // 0 -> default (11) at selection time
}

fn void thvm_atp_set_cp_fifo_tiebreak(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->cp_fifo_tiebreak = on ? 1u : 0u;
}

// Select the CP-priority weight mode (an `AtpCpWeightMode` value).
// Out-of-range values clamp to ATP_CP_WEIGHT_ADD (0) so a garbage
// mode falls back to the bare symbol-count heuristic.
fn void thvm_atp_set_cp_weight_mode(AtpState *s, u32 mode) {
  if (s == NULL) return;
  s->cp_weight_mode = (mode < ATP_CP_WEIGHT_LAST)
                    ? (u8)mode
                    : (u8)ATP_CP_WEIGHT_ADD;
}

fn void thvm_atp_set_max_cp_weight(AtpState *s, u32 w) {
  if (s != NULL) s->max_cp_weight = w;
}

// Enable the automatic, completeness-preserving growing CP-weight
// bound.  `base` seeds the bound; slope (default 2) scales it by the
// deepest current rule LHS so the bound tracks how complex the rule
// set has become.  base==0 disables (the historical unbounded engine).
fn void thvm_atp_set_auto_max_cp_weight(AtpState *s, u32 base) {
  if (s == NULL) return;
  s->auto_max_cp_weight_base  = base;
  s->auto_max_cp_weight_slope = 2u;
  s->auto_max_cp_weight_cur   = base;   // grown lazily as rules deepen
}

fn void thvm_atp_set_goal_interleave(AtpState *s, u32 ratio) {
  if (s != NULL) s->use_goal_interleave = ratio;
}

// 8.5c: order-aware compare.  Picks LPO (if attached) or KBO,
// returning a unified KboCmp-shaped result.  The two enums share
// numeric values (EQ=0, GT=1, LT=-1, UN=2), so the cast is safe.
static KboCmp atp_compare(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return KBO_UN;
  if (s->lpo != NULL) {
    return (KboCmp)thvm_lpo(lhs, rhs, s->lpo);
  }
  return thvm_kbo(lhs, rhs, s->kbo);
}

#if defined(ATP_ORDERED_REWRITE) || defined(ATP_MNF)
// === variable-occurrence helpers ====================================
// Shared by 9c ordered rewriting (variable-safe rewrite directions)
// and the Milestone-10 MNF search (variable-safe backward steps).

// Does variable id `id` occur in `t`?
static int atp_term_has_var(Term t, u32 id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (atp_term_has_var(term_ctr_at(t, i), id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}
// Every variable of `a` also occurs in `b`?  A rewrite direction is
// variable-safe only when the result side's variables are contained
// in the matched side's -- else the rewrite would introduce variables.
static int atp_vars_contained(Term a, Term b) {
  switch (term_tag(a)) {
    case TAG_FVR: return atp_term_has_var(b, term_ext(a));
    case TAG_CTR: {
      u32 n = term_ctr_n(a);
      for (u32 i = 0; i < n; i++) {
        if (!atp_vars_contained(term_ctr_at(a, i), b)) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}
#endif /* ATP_ORDERED_REWRITE || ATP_MNF */

#ifdef ATP_ORDERED_REWRITE
// === 9c-foundation: ordered rewriting ===============================
//
// Proper unfailing-completion rewriting.  The KBO_UN both-ways hack
// stored an unorientable equation u=v as two looping rules u->v and
// v->u.  Here an equation is stored once and the rewrite step tries
// every rule in BOTH directions, applying a direction only when the
// result strictly decreases the redex in the reduction order.  An
// oriented rule l->r (l > r, hence l.sigma > r.sigma for every sigma)
// fires forward only; an unorientable equation fires whichever
// direction is decreasing for the instance at hand.  Every rewrite
// strictly descends a well-founded order, so normalization terminates.

// One ordered rewrite at the top of `t`.
//
// An oriented rule (lhs[i] > rhs[i]) is decreasing for every instance,
// so it fires forward with NO order check and NO discarded `repl` --
// the same cost as the plain rewriter.  Only an unorientable equation
// pays the both-directions order-gated path: each direction is tried
// only when variable-safe and applied only when it strictly decreases
// `t`.  This fast path matters: without it, building a `repl` per
// failed order-check on every rule churns the heap catastrophically.
// Set by the mixed normalize loop: after the indexed (orientable)
// fixpoint, oriented rules cannot fire, so the linear step skips them
// and only tries the unorientable equations.
static u8 g_atp_skip_oriented = 0u;

static Term atp_ordered_try_top(AtpState *s, Term t,
                                const Term *lhs, const Term *rhs,
                                u32 n_rules, u8 *fired) {
  // A rule's orientation is fixed at creation; for the live rule set it
  // is cached in s->r_orient.  Recomputing it here -- a full KBO compare
  // per rule, per rewrite position -- was ~70% of the completion wall.
  // A custom rule array (interreduction's 2-rule set) is not the live
  // set, so it falls back to the direct compare.
  const u8 *orient = (lhs == s->lhs && rhs == s->rhs) ? s->r_orient : NULL;
  for (u32 i = 0; i < n_rules; i++) {
    u8 oriented = orient ? orient[i]
                         : (u8)(atp_compare(s, lhs[i], rhs[i]) == KBO_GT);
    if (oriented) {
      if (g_atp_skip_oriented) continue;   // already at indexed fixpoint
      // oriented rule -- forward only, no order check, no waste.
      RewriteSubst subst = {{0}};
      if (thvm_match(lhs[i], t, &subst)) {
        *fired = 1;
        return thvm_subst_apply(rhs[i], &subst);
      }
      continue;
    }
    // Unorientable equation -- both directions, variable-safe + order-
    // gated.  Match FIRST, then check the variable-containment guard:
    // at most positions the rule has no redex, so the cheap fail-fast
    // thvm_match avoids the full-term atp_vars_contained walk (a
    // measured hot leaf).  Behaviour-identical -- both the match and
    // the guard must hold for the rule to fire, and the guard does not
    // depend on the redex.
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(lhs[i], t, &subst) &&                 // l -> r
          atp_vars_contained(rhs[i], lhs[i])) {
        Term repl = thvm_subst_apply(rhs[i], &subst);
        if (atp_compare(s, t, repl) == KBO_GT) { *fired = 1; return repl; }
      }
    }
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(rhs[i], t, &subst) &&                 // r -> l
          atp_vars_contained(lhs[i], rhs[i])) {
        Term repl = thvm_subst_apply(lhs[i], &subst);
        if (atp_compare(s, t, repl) == KBO_GT) { *fired = 1; return repl; }
      }
    }
  }
  *fired = 0;
  return t;
}

// One outermost-leftmost ordered rewrite anywhere in `t`: tries the
// top, else descends into TAG_CTR children left-to-right.
static Term atp_ordered_rewrite_step(AtpState *s, Term t,
                                     const Term *lhs, const Term *rhs,
                                     u32 n_rules, u8 *fired) {
  Term top = atp_ordered_try_top(s, t, lhs, rhs, n_rules, fired);
  if (*fired) return top;
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    for (u32 i = 0; i < n; i++) {
      u8 cf = 0;
      Term nch = atp_ordered_rewrite_step(s, term_ctr_at(t, i),
                                          lhs, rhs, n_rules, &cf);
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

// Ordered normalization to fixpoint.  step_cap is a safety bound only
// -- ordered rewriting genuinely terminates.
static Term atp_rewrite_normalize_ordered(AtpState *s, Term t,
                                          const Term *lhs, const Term *rhs,
                                          u32 n_rules, u32 step_cap) {
#ifdef ATP_RULE_INDEX
  // When EVERY rule in R is KBO-oriented (lhs > rhs), every forward
  // rewrite is order-decreasing, so the discrimination-tree normalizer
  // is an exact equivalent of the linear ordered scan (same outermost-
  // leftmost redex, same lowest-index rule) and replaces the per-
  // position O(n_rules) thvm_match scan with a tree descent.  An
  // unorientable equation, while present in R, drops to the linear
  // path (where both rewrite directions are order-gated).  `n_unorient`
  // is a live COUNT, not a sticky flag: a transient unorientable rule
  // -- interreduced away or re-oriented after reduction -- restores the
  // indexed path the moment R is orientable again.
  if (lhs == s->lhs && rhs == s->rhs && n_rules == s->n_rules) {
    if (s->n_unorient == 0u) {
      return atp_rewrite_normalize_indexed(s, t, step_cap);
    }
    // Opt-in flatterm fast-path: keep the subject flat across both the
    // orientable indexed fixpoint and the unorientable pass (no per-step
    // re-flatten / tree rebuild).  Same normal form as the tree loop
    // below; default OFF so the engine is byte-identical.
    if (s->use_flatterm) {
#ifdef ATP_FLATTERM_SELFCHECK
      // In-engine differential: also run the tree path and abort on any
      // mismatch.  Build-only (defeats the speedup) -- proves the
      // flatterm normal form equals the tree one on the LIVE saturation
      // workload, not just the offline random test.
      {
        Term ref = atp_rewrite_normalize_flatterm_selfcheck_tree(s, t, step_cap);
        Term got = atp_rewrite_normalize_flatterm_mixed(s, t, step_cap);
        if (!kbo_eq(ref, got)) {
          char ib[2048], rb[2048], gb[2048];
          atp_pretty_term(t, ib, sizeof ib);
          atp_pretty_term(ref, rb, sizeof rb);
          atp_pretty_term(got, gb, sizeof gb);
          fprintf(stderr, "FLATTERM SELFCHECK MISMATCH\n in=%s\n tree=%s\n flat=%s\n",
                  ib, rb, gb);
          fprintf(stderr, " n_rules=%u n_unorient=%u\n", s->n_rules, s->n_unorient);
          for (u32 ri = 0; ri < s->n_rules; ri++) {
            char la[1024], ra[1024];
            atp_pretty_term(s->lhs[ri], la, sizeof la);
            atp_pretty_term(s->rhs[ri], ra, sizeof ra);
            fprintf(stderr, "  R%u%s: %s = %s\n", ri,
                    s->r_orient[ri] ? "" : "(un)", la, ra);
          }
          abort();   // build-time invariant: flatterm NF == tree NF
        }
        return got;
      }
#endif
      return atp_rewrite_normalize_flatterm_mixed(s, t, step_cap);
    }
    // Mixed rule set: the discrimination tree (orientable rules only)
    // normalizes every orientable rewrite in one fast descent; the few
    // unorientable equations are then applied one outermost, KBO-gated
    // step at a time, re-running the indexed pass between.  After an
    // indexed fixpoint no orientable rule fires, so the linear step
    // finds an unorientable rewrite (or none = done).  Replaces the
    // O(n_rules) linear scan per rewrite that dominated completion on a
    // self-overlapping axiom.
    for (u32 i = 0; i < step_cap; i++) {
      if (atp_norm_deadline_fired(s)) return t;
      t = atp_rewrite_normalize_indexed(s, t, step_cap);
      u8 fired = 0;
      g_atp_skip_oriented = 1u;
      Term t2 = atp_ordered_rewrite_step(s, t, lhs, rhs, n_rules, &fired);
      g_atp_skip_oriented = 0u;
      if (!fired) break;
      t = t2;
    }
    return t;
  }
#endif
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) return t;
    u8 fired = 0;
    Term t2 = atp_ordered_rewrite_step(s, t, lhs, rhs, n_rules, &fired);
    if (!fired) break;
    t = t2;
  }
  return t;
}
#endif /* ATP_ORDERED_REWRITE */

// Total symbol count: TAG_FVR / atoms count as 1; TAG_CTR counts
// itself + the symbols of its children.  This is the "size" used
// by Waldmeister's `--add` heuristic in `ClasHeuristics.c`
// ("classification heuristics") -- the simplest CP-priority
// function: cheapest-by-size wins.
static u32 atp_symbol_count(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      u32 c = 1;
      for (u32 i = 0; i < n; i++) {
        c += atp_symbol_count(term_ctr_at(t, i));
      }
      return c;
    }
    default: return 1;   // FVR / atoms / NUM / etc.
  }
}

// === ClasHeuristics: advanced CP-weight term measures ===============
// Helpers feeding the non-default `AtpCpWeightMode` weight modes --
// ports of measures from Waldmeister's `ClasHeuristics`,
// `ClasFunctions` and `Unifikation1` ("unification 1") modules.

// Term depth: a leaf (FVR / atom / nullary CTR) has depth 0; a
// CTR has 1 + max child depth.  Used by the unification-measure
// weight, mirroring Waldmeister's `TO_Termtiefe` ("term depth").
static u32 atp_term_depth(Term t) {
  if (term_tag(t) != TAG_CTR) return 0;
  u32 n = term_ctr_n(t);
  u32 d = 0;
  for (u32 i = 0; i < n; i++) {
    u32 cd = atp_term_depth(term_ctr_at(t, i));
    if (cd > d) d = cd;
  }
  return n == 0 ? 0 : d + 1;
}

// Does variable `var_id` (a TAG_FVR id) occur anywhere in `t`?
// The occur-check used by the unification-measure weight.
static int atp_var_occurs(Term t, u32 var_id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == var_id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        if (atp_var_occurs(term_ctr_at(t, i), var_id)) return 1;
      }
      return 0;
    }
    default: return 0;
  }
}

// Depth-weighted term-disagreement count -- a port of Waldmeister's
// `U1_Unifikationsmass` ("unification measure"; sources/INF/
// Unifikation1.c).  Walks `a` and `b` in parallel: equal top
// symbols recurse into children; a variable on either side that
// fails the occur-check binds for free (cost 0) and otherwise costs
// `2*d`; a function-symbol clash costs `d`.  `d` is the working
// depth, clamped at 1 as the recursion descends -- mirroring
// Waldmeister's `if (!(--d)) d = 1;` step-down.  Result is 0 iff
// the terms are syntactically unifiable.
static u32 atp_unif_measure_rec(Term a, Term b, u32 d) {
  u8 ta = term_tag(a), tb = term_tag(b);
  if (ta == TAG_CTR && tb == TAG_CTR
      && term_ext(a) == term_ext(b)
      && term_ctr_n(a) == term_ctr_n(b)) {
    u32 nd = d > 1 ? d - 1 : 1;
    u32 n = term_ctr_n(a);
    u32 mass = 0;
    for (u32 i = 0; i < n; i++) {
      mass += atp_unif_measure_rec(term_ctr_at(a, i),
                                   term_ctr_at(b, i), nd);
    }
    return mass;
  }
  if (ta == TAG_FVR) {
    return atp_var_occurs(b, term_ext(a)) ? 2u * d : 0u;
  }
  if (tb == TAG_FVR) {
    return atp_var_occurs(a, term_ext(b)) ? 2u * d : 0u;
  }
  // Function-symbol (or non-CTR) clash.
  return d;
}

static u32 atp_unif_measure(Term lhs, Term rhs) {
  u32 dl = atp_term_depth(lhs);
  u32 dr = atp_term_depth(rhs);
  u32 d  = dl > dr ? dl : dr;
  if (d == 0) d = 1;
  return atp_unif_measure_rec(lhs, rhs, d);
}

// Per-term KBO weight feeding the ORD / GT / MIX modes.  Ports
// Waldmeister's `CF_Phi_KBO` ("KBO weight"; sources/CLAS/
// ClasFunctions.c): sum the active signature's per-symbol weights
// (`cfg->weights[label]` for a function symbol, `cfg->var_weight`
// for a variable).  m8's KBO module only exposes the single-pass
// differential balance, so the standalone single-term walk is
// inlined here.  With no KboConfig attached the raw symbol count
// is the fallback.
static u32 atp_kbo_weight(AtpState *s, Term t) {
  const KboConfig *cfg = (s != NULL) ? s->kbo : NULL;
  if (cfg == NULL) return atp_symbol_count(t);
  switch (term_tag(t)) {
    case TAG_FVR: return cfg->var_weight;
    case TAG_CTR: {
      u32 lab = term_ext(t);
      u32 w   = (lab < cfg->n_labels) ? cfg->weights[lab] : 0u;
      u32 n   = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        w += atp_kbo_weight(s, term_ctr_at(t, i));
      }
      return w;
    }
    default: return cfg->var_weight;
  }
}

// CP-weight base for one weight mode -- ports of the `CH_*Weight`
// functions in Waldmeister's `ClasHeuristics` module.  Mode
// ATP_CP_WEIGHT_ADD reproduces the pre-port symbol-count sum.
static u32 atp_cp_weight_base(AtpState *s, Term lhs, Term rhs, u32 mode) {
  switch (mode) {
    case ATP_CP_WEIGHT_MAX: {
      // CH_MaxWeight: max(|lhs|, |rhs|).
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      return wl > wr ? wl : wr;
    }
    case ATP_CP_WEIGHT_ORD: {
      // CH_OrdWeight: KBO-weight sum (CF_Phi_KBO over both sides).
      return atp_kbo_weight(s, lhs) + atp_kbo_weight(s, rhs);
    }
    case ATP_CP_WEIGHT_GT: {
      // CH_GtWeight: ordering-directed -- the greater side's
      // weight when the CP orients, the sum otherwise.
      u32 wl = atp_kbo_weight(s, lhs), wr = atp_kbo_weight(s, rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      return (c == KBO_GT) ? wl
           : (c == KBO_LT) ? wr
           : wl + wr;
    }
    case ATP_CP_WEIGHT_MIX: {
      // CH_MixWeight: (wl+wr)*g + g + (wl+wr), g = GtWeight value.
      u32 wl = atp_kbo_weight(s, lhs), wr = atp_kbo_weight(s, rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      u32 sum = wl + wr;
      return sum * g + g + sum;
    }
    case ATP_CP_WEIGHT_MIX2: {
      // CH_MixWeight2: g*10 + (wl+wr).
      u32 wl = atp_kbo_weight(s, lhs), wr = atp_kbo_weight(s, rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      return g * 10u + (wl + wr);
    }
    case ATP_CP_WEIGHT_UNIF: {
      // CH_Unifikationsmass: (wl+wr) * unification-measure.
      u32 wl = atp_kbo_weight(s, lhs), wr = atp_kbo_weight(s, rhs);
      return (wl + wr) * atp_unif_measure(lhs, rhs);
    }
    case ATP_CP_WEIGHT_ADD:
    default:
      // CH_AddWeight: symbol-count sum -- the pre-port default.
      return atp_symbol_count(lhs) + atp_symbol_count(rhs);
  }
}

// === Waldmeister goal-directed CP selection (Clas_CP_Goal.c) ========
// Weight a critical pair by how its two sides structurally match the
// goal (the CPinGoal classifier).  A CP side that is a generalization
// of -- i.e. matches into -- a goal subterm "covers" that subterm.
//   Doppelmatch  : both CP sides match goal subterms under ONE
//                  consistent substitution.  Weight = the goal residual
//                  phi(goal) - covered (minimized over match positions).
//   Einfachmatch : one CP side matches.  Weight = residual x 5.
//   Nullmatch    : neither.  Weight = the CP's own size x 50.
// A goal-resembling CP thus scores small and is selected first,
// steering completion at the goal -- Waldmeister's key to fast proofs.
#define ATP_GOAL_SINGLE_FACTOR 5u
#define ATP_GOAL_NONE_FACTOR   50u

// Best coverage of `pat` matched into `subj` or any subterm, extending
// the (consistent) substitution `base`: returns the largest matched
// subterm's symbol count, 0 if none.  `pat` must be a CTR -- a bare
// variable generalizes everything and carries no goal signal (the
// MinStruct spirit of Clas_CP_Goal).
static u32 atp_goal_match_cov(Term pat, Term subj, RewriteSubst base) {
  if (term_tag(pat) != TAG_CTR) return 0u;
  u32 best = 0u;
  RewriteSubst t = base;
  if (thvm_match(pat, subj, &t)) {
    u32 c = atp_symbol_count(subj);
    if (c > best) best = c;
  }
  if (term_tag(subj) == TAG_CTR) {
    u32 n = term_ctr_n(subj);
    for (u32 i = 0; i < n; i++) {
      u32 c = atp_goal_match_cov(pat, term_ctr_at(subj, i), base);
      if (c > best) best = c;
    }
  }
  return best;
}

// Best (cov_cl + cov_cr) for a Doppelmatch: cl matches a subterm of sx
// (substitution sigma), cr matches a subterm of gy extending sigma.
// Walks every cl-match position so the cr-match is gated by a
// consistent sigma, maximizing total coverage (= minimal residual).
static u32 atp_goal_doppel(Term cl, Term cr, Term sx, Term gy,
                           RewriteSubst base) {
  u32 best = 0u;
  if (term_tag(cl) == TAG_CTR) {
    RewriteSubst t1 = base;
    if (thvm_match(cl, sx, &t1)) {
      u32 cov_a = atp_symbol_count(sx);
      u32 cov_b = atp_goal_match_cov(cr, gy, t1);
      if (cov_b && cov_a + cov_b > best) best = cov_a + cov_b;
    }
  }
  if (term_tag(sx) == TAG_CTR) {
    u32 n = term_ctr_n(sx);
    for (u32 i = 0; i < n; i++) {
      u32 c = atp_goal_doppel(cl, cr, term_ctr_at(sx, i), gy, base);
      if (c > best) best = c;
    }
  }
  return best;
}

// CPinGoal weight of CP (cl, cr) against the conjecture.  No goal
// (completion mode) -> 0.  See the block comment above.
static u32 atp_goal_weight(const AtpState *s, Term cl, Term cr) {
  if (s == NULL || s->goal_lhs == 0) return 0u;
  Term gl = s->goal_lhs_nf ? s->goal_lhs_nf : s->goal_lhs;
  Term gr = s->goal_rhs_nf ? s->goal_rhs_nf : s->goal_rhs;
  u32 phi_g = atp_symbol_count(gl) + atp_symbol_count(gr);
  RewriteSubst e = {{0}};
  // Doppelmatch over both pairings (cl->gl & cr->gr, cl->gr & cr->gl).
  u32 d1 = atp_goal_doppel(cl, cr, gl, gr, e);
  u32 d2 = atp_goal_doppel(cl, cr, gr, gl, e);
  u32 dcov = d1 > d2 ? d1 : d2;
  if (dcov) return (dcov < phi_g) ? (phi_g - dcov) : 0u;
  // Einfachmatch: best single coverage of either CP side into either
  // goal side.
  u32 ec = atp_goal_match_cov(cl, gl, e);
  { u32 x = atp_goal_match_cov(cl, gr, e); if (x > ec) ec = x; }
  { u32 x = atp_goal_match_cov(cr, gl, e); if (x > ec) ec = x; }
  { u32 x = atp_goal_match_cov(cr, gr, e); if (x > ec) ec = x; }
  if (ec) {
    u32 res = (ec < phi_g) ? (phi_g - ec) : 0u;
    return res * ATP_GOAL_SINGLE_FACTOR;
  }
  // Nullmatch.
  return (atp_symbol_count(cl) + atp_symbol_count(cr)) * ATP_GOAL_NONE_FACTOR;
}
// 8.8: priority weight for a CP.  Default `--add` heuristic is
// the symbol-count sum.  When `s->use_mix_heuristic` is set, add
// a penalty for CPs that fail to orient cleanly (KBO_UN or
// KBO_EQ) -- mirrors Waldmeister's `--mix` heuristic in
// `ClasHeuristics.c`.  The penalty (`MIX_UNORIENTED_PENALTY`)
// is conservative; experiments may want to tune it.  Under
// -DATP_GOAL_HEURISTIC a bounded goal-directed penalty is then
// added (Waldmeister lever 1, above).
//
// `s->cp_weight_mode` selects among the ported `ClasHeuristics`
// weight functions (see the `AtpCpWeightMode` enum); the engine
// default is ATP_CP_WEIGHT_GT.  Selecting ATP_CP_WEIGHT_ADD with
// use_mix_heuristic unset and no goal makes this function the
// bare symbol-count sum.
#define MIX_UNORIENTED_PENALTY 4u
// Priority weight for a CP whose symbol-count sum is already known
// (`base`) -- e.g. counted for free during acp_pack.  Identical
// verdict to atp_cp_priority; only the redundant size walk is
// skipped.  The precomputed `base` is the ADD-mode value; a
// non-default cp_weight_mode recomputes the base from the terms.
static u32 atp_cp_priority_sized(AtpState *s, Term lhs, Term rhs, u32 base) {
  // Goal-directed mode (Waldmeister CPinGoal): weight by structural
  // match to the goal.  Opt-in via cp_weight_mode so completion-mode
  // and the other weights are unaffected.
  if (s != NULL && s->cp_weight_mode == ATP_CP_WEIGHT_GOAL &&
      s->goal_lhs != 0) {
    return atp_goal_weight(s, lhs, rhs);
  }
  u32 mode = (s != NULL) ? s->cp_weight_mode : ATP_CP_WEIGHT_ADD;
  if (mode != ATP_CP_WEIGHT_ADD) {
    // The caller's `base` is the ADD-mode symbol-count sum; for any
    // other mode it must be recomputed from the CP terms.
    base = atp_cp_weight_base(s, lhs, rhs, mode);
  }
  if (s != NULL && s->use_mix_heuristic) {
    KboCmp c = atp_compare(s, lhs, rhs);
    if (c != KBO_GT && c != KBO_LT) {
      // KBO_EQ / KBO_UN -- penalize.
      base += MIX_UNORIENTED_PENALTY;
    }
  }
  return base;
}
// Learned CP-selection scorer (ENIGMA-style).  Logistic-regression
// weights trained on the labelled corpus exported by THVM_ATP_CP_DATASET
// (per-selected-CP features, labelled by trace-DAG reachability from the
// goal-closing step over the 83 provable AxiomaticTheory notable
// theorems; held-out test AUC ~0.85).  score = W.features + B, in the
// RAW feature space (the standardization is folded into W,B).  A higher
// score means more proof-relevant, so it maps to a LOWER heap priority
// (selected sooner).  Completeness is preserved by select_cp's periodic
// FIFO (CPdimension) pick, which fires regardless of the weight mode.
static const float ATP_LEARNED_W[ATP_CP_FEATURE_DIM] = {
  0.003216f, -0.271657f, 0.460614f, -0.096104f, 0.003216f, 0.042247f,
  0.003216f, -0.000402f, -0.005740f, -0.156174f, -0.023514f, 1.121586f,
  1.999360f, -0.012683f};
static const float ATP_LEARNED_B = -1.598045f;

static u32 atp_cp_learned_priority(AtpState *s, Term lhs, Term rhs) {
  float feat[ATP_CP_FEATURE_DIM];
  thvm_atp_cp_features(s, lhs, rhs, s->cp_seq_next, feat);
  float score = ATP_LEARNED_B;
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) score += ATP_LEARNED_W[i] * feat[i];
  // Map score (typically ~[-6, 4]) to a u32 priority, higher score ->
  // lower priority.  Clamp into a safe positive band.
  float pr = 1.0e6f - 1.0e4f * score;
  if (pr < 0.0f) pr = 0.0f;
  if (pr > 2.0e9f) pr = 2.0e9f;
  return (u32)pr;
}

static u32 atp_cp_priority(AtpState *s, Term lhs, Term rhs) {
  if (s->cp_weight_mode == ATP_CP_WEIGHT_LEARNED) {
    return atp_cp_learned_priority(s, lhs, rhs);
  }
  return atp_cp_priority_sized(s, lhs, rhs,
                               atp_symbol_count(lhs) + atp_symbol_count(rhs));
}

// === 7c': CP-queue binary min-heap ==================================
//
// The CP queue (cp_packed/cp_trace/cp_pri/cp_seq) is kept as a
// binary min-heap ordered by (cp_pri, cp_seq): cheapest priority
// first, insertion order breaking ties.  This reproduces the old
// `--add` selection order (collapse_ordered sorted by INC depth,
// ties by queue index) but at O(log n) per push/pop instead of
// rebuilding an n-leaf INC-SUP tree + collapse on every step.

// Ordering predicate: does queue slot i sort strictly before j?
static int atp_cp_before(const AtpState *s, u32 i, u32 j) {
  if (s->cp_pri[i] != s->cp_pri[j]) return s->cp_pri[i] < s->cp_pri[j];
  return s->cp_seq[i] < s->cp_seq[j];
}

// Swap all four parallel CP arrays at slots i, j.  cp_packed swaps the
// pointer only -- the byte string itself does not move, so a
// subsumption-index record still borrows a valid buffer after a sift.
static u32 atp_goal_weight(const AtpState *s, Term cl, Term cr);

static void atp_cp_swap(AtpState *s, u32 i, u32 j) {
  u8  *tc = s->cp_packed[i];s->cp_packed[i]= s->cp_packed[j];s->cp_packed[j]= tc;
  u32  tt = s->cp_trace[i]; s->cp_trace[i] = s->cp_trace[j]; s->cp_trace[j] = tt;
  u32  tp = s->cp_pri[i];   s->cp_pri[i]   = s->cp_pri[j];   s->cp_pri[j]   = tp;
  u32  tq = s->cp_seq[i];   s->cp_seq[i]   = s->cp_seq[j];   s->cp_seq[j]   = tq;
  u32  tg = s->cp_goal[i];  s->cp_goal[i]  = s->cp_goal[j];  s->cp_goal[j]  = tg;
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

// Insert an already-packed CP byte string onto the heap.  Takes
// ownership of `packed` (the queue frees it on pop/drop).  `lhs`/`rhs`
// are the live Terms (for the FV index + goal weight); `cp_nodes` is
// the precomputed node count.  Shared by atp_cp_heap_push (fresh CP)
// and the auto-MaxWeight stash drain (re-admitting a deferred CP).
static void atp_cp_heap_insert_packed(AtpState *s, u8 *packed, u32 cp_nodes,
                                      Term lhs, Term rhs, u32 trace) {
  atp_ensure_cp_cap(s, s->n_cps + 1);
  u32 i = s->n_cps;
  s->cp_packed[i]= packed;
  s->cp_trace[i] = trace;
  s->cp_pri[i]   = atp_cp_priority_sized(s, lhs, rhs, cp_nodes);
  s->cp_goal[i]  = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                     ? atp_goal_weight(s, lhs, rhs) : 0u;
  u32 seq        = s->cp_seq_next++;
  s->cp_seq[i]   = seq;
  s->n_cps++;
  atp_cp_sift_up(s, i);
  atp_cp_graph_sync(s);
#ifdef ATP_FV_INDEX
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
  u32 deepest = 0u;
  for (u32 k = 0; k < s->n_rules; k++) {
    u32 w = atp_symbol_count(s->lhs[k]);
    if (w > deepest) deepest = w;
  }
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
                              u32 trace) {
  if (s->n_cp_stash >= s->cp_stash_cap) {
    u32 ncap = s->cp_stash_cap ? s->cp_stash_cap * 2u : 256u;
    u8 **np  = (u8 **)realloc(s->cp_stash_packed, ncap * sizeof(u8 *));
    u32 *nt  = (u32 *)realloc(s->cp_stash_trace,  ncap * sizeof(u32));
    u32 *nn  = (u32 *)realloc(s->cp_stash_nodes,  ncap * sizeof(u32));
    if (np == NULL || nt == NULL || nn == NULL) {
      // Allocation failure: rather than leak or lose the CP, admit it
      // directly (slow path, but sound -- never drops a proof CP).
      free(np); free(nt); free(nn);
      Term l = 0, r = 0;
      acp_unpack(packed, &l, &r);
      atp_cp_heap_insert_packed(s, packed, cp_nodes, l, r, trace);
      return;
    }
    s->cp_stash_packed = np; s->cp_stash_trace = nt; s->cp_stash_nodes = nn;
    s->cp_stash_cap = ncap;
  }
  s->cp_stash_packed[s->n_cp_stash] = packed;
  s->cp_stash_trace[s->n_cp_stash]  = trace;
  s->cp_stash_nodes[s->n_cp_stash]  = cp_nodes;
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
      atp_cp_heap_insert_packed(s, s->cp_stash_packed[r],
                                s->cp_stash_nodes[r], l, rr,
                                s->cp_stash_trace[r]);
    } else {
      s->cp_stash_packed[w] = s->cp_stash_packed[r];
      s->cp_stash_trace[w]  = s->cp_stash_trace[r];
      s->cp_stash_nodes[w]  = s->cp_stash_nodes[r];
      w++;
    }
  }
  s->n_cp_stash = w;
}

// Push one CP onto the heap.  Computes its priority once (the cost
// the old select_cp paid n times per step) and sifts up.  O(log n).
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace) {
  // Pack the CP into a byte string outside the managed heap.
  u32  cp_nodes  = 0u;
  u8  *packed    = acp_pack(lhs, rhs, NULL, &cp_nodes);
  // Waldmeister MaxWeight (hard cap): drop an over-weight critical pair
  // before it enters the queue.  0 = unbounded.  This is the LOSSY
  // bound (flag-gated by the caller setting max_cp_weight); the default
  // engine leaves it 0.
  if (s->max_cp_weight > 0u && cp_nodes > s->max_cp_weight) {
    free(packed);
    return;
  }
  // Auto-MaxWeight (completeness-preserving): defer an over-bound CP to
  // the overflow stash rather than dropping it.  Disabled when base==0.
  if (s->auto_max_cp_weight_base > 0u) {
    u32 bound = atp_auto_maxw_bound(s);
    if (bound > 0u && cp_nodes > bound) {
      atp_cp_stash_push(s, packed, cp_nodes, trace);
      return;
    }
  }
  atp_cp_heap_insert_packed(s, packed, cp_nodes, lhs, rhs, trace);
}

// Waldmeister CP-queue interleaving (a port of KPVerwaltung.c's
// `CPdimension`).  Waldmeister keeps the set of unselected equations
// in a K-D heap with TWO keys -- a weight key and a FIFO insertion
// key -- and `CPdimension` returns the FIFO dimension for `thresholdCP`
// of every `moduloCP` selections, the weight dimension otherwise.
// Waldmeister's problem analysis picks the ratio from {1:10, 1:50,
// 1:100, 1:200} (YFiles.c `Schrittweiten`); this is the most-fair
// setting, 1 FIFO pick per 11 selections.
//
// A pure smallest-weight heap can starve -- it keeps picking light
// CPs while a heavier CP sits unselected -- so the periodic FIFO pick
// is the fairness lever.  Waldmeister places the FIFO pick at the
// START of each modulo window; placing it at the END instead is the
// same ratio (the phase is immaterial over a completion run) and
// keeps a queue selected fewer than MODULO-THRESHOLD times on a pure
// weight order, which the weight-order unit tests rely on.
#define ATP_CP_FIFO_MODULO     11u
#define ATP_CP_FIFO_THRESHOLD   1u

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

  // CPdimension: FIFO pick on the last THRESHOLD of every MODULO
  // selections, weight pick (heap root) otherwise.
  u32 j = 0;
  if (s->use_goal_interleave > 0u &&
      (s->cp_select_count % s->use_goal_interleave) == 0u) {
    // Goal-directed pick: the most goal-relevant queued CP (min
    // cp_goal).  E-style ratio -- the other picks are weight-based,
    // building the system; this steers toward the goal.
    u32 best = 0;
    for (u32 i = 1; i < s->n_cps; i++) {
      if (s->cp_goal[i] < s->cp_goal[best]) best = i;
    }
    j = best;
  } else if (((s->fifo_modulo ? s->fifo_modulo : ATP_CP_FIFO_MODULO)
              - ATP_CP_FIFO_THRESHOLD)
             <= s->cp_select_count
                  % (s->fifo_modulo ? s->fifo_modulo : ATP_CP_FIFO_MODULO)) {
    // FIFO dimension: the oldest queued CP is the lowest cp_seq.
    // O(n_cps) scan, but only 1 call in `fifo_modulo` takes this branch.
    u32 best = 0;
    for (u32 i = 1; i < s->n_cps; i++) {
      if (s->cp_seq[i] < s->cp_seq[best]) best = i;
    }
    j = best;
  }
  s->cp_select_count++;

  // Unpack the chosen CP from its byte string into two fresh heap
  // Terms for the caller to normalize.
  acp_unpack(s->cp_packed[j], lhs_out, rhs_out);
  s->last_popped_trace = s->cp_trace[j];
#ifdef ATP_FV_INDEX
  // 7d: the popped CP leaves the queue -- drop it from the index so a
  // later subsumption query never matches a stale, no-longer-queued
  // CP.  Mark the record dead BEFORE freeing the byte string it
  // borrows: a dead record is never dereferenced, so the borrow stays
  // sound.
  atp_fv_index_remove(s->fv_index, s->cp_seq[j]);
#endif
  free(s->cp_packed[j]);
  s->cp_packed[j] = NULL;
  s->n_cps--;
  if (j != s->n_cps) {
    // Backfill slot j from the (ex-)last slot, then repair the heap.
    u32 last = s->n_cps;
    s->cp_packed[j]    = s->cp_packed[last];
    s->cp_packed[last] = NULL;          // vacated slot: leave it empty
    s->cp_trace[j] = s->cp_trace[last];
    s->cp_pri[j]   = s->cp_pri[last];
    s->cp_seq[j]   = s->cp_seq[last];
    s->cp_goal[j]  = s->cp_goal[last];
    atp_cp_sift_up(s, j);
    atp_cp_sift_down(s, j);
  }
  // 8a: the pop shrank / reordered the mirror -- resync cp_graph.
  atp_cp_graph_sync(s);
  // ENIGMA training-data hook: record the processed CP's feature
  // vector + trace id.  Gated -- with the flag off (the default) this
  // is a single predictable-branch test and the engine is byte-
  // identical to the untracked run.
  if (s->record_cp_features) {
    atp_cp_feat_record(s, *lhs_out, *rhs_out, s->last_popped_trace);
  }
  return 1;
}

// 7c': re-establish the CP-queue heap invariant over cp_packed[0..n_cps).
// The normal path keeps the queue a heap via atp_cp_heap_push, but a
// caller (chiefly tests) that populates cp_packed / n_cps directly
// (via thvm_atp_cp_set) must call this so cp_pri / cp_seq are filled
// and the array satisfies the heap order before select / peek.
fn void thvm_atp_cp_reheapify(AtpState *s) {
  if (s == NULL || s->n_cps == 0) return;
  atp_ensure_cp_cap(s, s->n_cps);
  for (u32 i = 0; i < s->n_cps; i++) {
    Term l = 0, r = 0;
    acp_unpack(s->cp_packed[i], &l, &r);
    s->cp_pri[i] = atp_cp_priority(s, l, r);
    s->cp_goal[i] = (s->use_goal_interleave > 0u && s->goal_lhs != 0)
                      ? atp_goal_weight(s, l, r) : 0u;
    // Waldmeister `-:w1=fifo`: keep each surviving CP's original
    // insertion age so equal-weight ties stay oldest-first run-wide.
    // The post-orient compaction (atp_normalize_graph) already carried
    // cp_seq[] down to its packed slot, so leave it untouched here.
    // Default: reassign a fresh monotone seq (engine byte-identical).
    if (!s->cp_fifo_tiebreak) s->cp_seq[i] = s->cp_seq_next++;
  }
  // Floyd build-heap: sift down every internal node, last to first.
  for (u32 i = s->n_cps / 2; i > 0; ) {
    i--;
    atp_cp_sift_down(s, i);
  }
  // 8a: a caller populated the arrays directly -- resync cp_graph.
  atp_cp_graph_sync(s);
#ifdef ATP_FV_INDEX
  // 7d: reheapify reassigned every cp_seq[] (the index's stable key)
  // and a normalize-graph compaction may have dropped CPs.  The
  // incremental insert/remove path can no longer track the set, so
  // rebuild the index wholesale from the live CP arrays.
  atp_fv_index_rebuild(s);
#endif
}

// Measurement-only: walk the live CP queue, reporting min/max/mean
// node count (the acp_pack symbol count) and a coarse size histogram
// into the caller's `bins` array (bins[k] counts CPs with node-count
// in [k*bucket, (k+1)*bucket); the last bin is the overflow tail).
// `nbins`/`bucket` are caller-chosen.  Pure read of cp_packed; no
// engine state mutated.  Returns the queue length.
fn u32 thvm_atp_cp_size_stats(const AtpState *s, u32 *min_out, u32 *max_out,
                              double *mean_out, u32 *bins, u32 nbins,
                              u32 bucket) {
  if (bins != NULL && nbins > 0) {
    for (u32 k = 0; k < nbins; k++) bins[k] = 0u;
  }
  if (s == NULL || s->n_cps == 0) {
    if (min_out)  *min_out  = 0u;
    if (max_out)  *max_out  = 0u;
    if (mean_out) *mean_out = 0.0;
    return 0u;
  }
  u32 mn = 0xffffffffu, mx = 0u;
  u64 sum = 0u;
  for (u32 i = 0; i < s->n_cps; i++) {
    Term l = 0, r = 0;
    acp_unpack(s->cp_packed[i], &l, &r);
    u32 nodes = atp_symbol_count(l) + atp_symbol_count(r);
    if (nodes < mn) mn = nodes;
    if (nodes > mx) mx = nodes;
    sum += nodes;
    if (bins != NULL && nbins > 0 && bucket > 0) {
      u32 b = nodes / bucket;
      if (b >= nbins) b = nbins - 1u;
      bins[b]++;
    }
  }
  if (min_out)  *min_out  = mn;
  if (max_out)  *max_out  = mx;
  if (mean_out) *mean_out = (double)sum / (double)s->n_cps;
  return s->n_cps;
}

// Push one rule onto R; the rule array is growable, so this always
// succeeds (returns 1) unless the state pointer is NULL.
//
// 7c: under -DATP_VAR_NORM the rule's variables are canonically
// renumbered before storage (dense [0, k), shared across both
// sides) -- alpha-renaming that keeps every stored variable below
// the REWRITE_MAX_VAR matcher cliff -- and an identical rule
// already in R (both sides `kbo_eq`) is rejected (returns 0,
// nothing stored).  The renumbering makes alpha-equivalent rules
// byte-identical, so the duplicate guard catches the "add the same
// rule 300x" pathology that interreduction's subsumption misses
// while the matcher is dead on out-of-range variables.
static u8 atp_push_rule(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
#ifdef ATP_VAR_NORM
  thvm_normalize_vars(&lhs, &rhs);
  for (u32 i = 0; i < s->n_rules; i++) {
    if (kbo_eq(s->lhs[i], lhs) && kbo_eq(s->rhs[i], rhs)) {
      return 0;  // duplicate rule -- already in R
    }
  }
#endif
  atp_ensure_rule_cap(s, s->n_rules + 1);
  s->lhs[s->n_rules] = lhs;
  s->rhs[s->n_rules] = rhs;
  // Cache the rule's orientation once -- atp_ordered_try_top reads this
  // instead of recomputing a full KBO compare per rewrite position.
  s->r_orient[s->n_rules] = (u8)(atp_compare(s, lhs, rhs) == KBO_GT);
  if (!s->r_orient[s->n_rules]) s->n_unorient++;
  s->n_rules++;
  // Env-gated derivation trace.  Prints each rule at orientation time
  // in derivation order, mirroring Waldmeister's `-a 4` "... added as
  // new rule N:" output.
  if (atp_rule_trace_on()) {
    char la[2048], ra[2048];
    atp_pretty_term(lhs, la, sizeof la);
    atp_pretty_term(rhs, ra, sizeof ra);
    fprintf(stderr, "RULE %u: %s -> %s%s\n", s->n_rules - 1u, la, ra,
            s->r_orient[s->n_rules - 1u] ? "" : "  (unorientable)");
  }
#ifdef ATP_RULE_INDEX
  // 7e lever 2: R grew -- the rule-LHS index no longer reflects it.
  s->rule_index_dirty = 1u;
#endif
  return 1;
}

// One full saturation step.  See docs/plans/saturation_loop.md
// sec.2 for the algorithm.  Order:
//   1. goal_check   -- cheap; may close if a prior step proved
//   2. step_cap     -- TIMEOUT if exceeded
//   3. select_cp    -- QUEUE_EMPTY if exhausted
//   4. normalize    -- both sides under current R (NORM_CAP = 64)
//   5. trivialize   -- skip if sides become kbo_eq
//   6. orient + add -- KBO + unfailing fallback
//   7. interreduce  -- drop subsumed older rules
//   8. generate_cps -- (new x R) + (old x new), adjusted for
//                      dropped old rules
//   9. goal_check   -- may close after new rule(s) integrated
//  10. step++       -- only on a "real" step that didn't close
//
// Returns one of: ATP_RUNNING (continue), ATP_PROVED (goal hit),
// ATP_TIMEOUT (step cap), ATP_QUEUE_EMPTY (saturation reached
// without proving the goal).

fn void thvm_atp_set_record_norm_steps(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->record_norm_steps = on ? 1u : 0u;
}

// Toggle interreduction right-reduction (RHS composition).  On by
// default (see thvm_atp_init); set 0 to recover left-reduction-only
// interreduction for A/B measurement or proof-extraction fallback.
fn void thvm_atp_set_right_reduce(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->right_reduce = on ? 1u : 0u;
}

// Toggle periodic critical-pair-set interreduction (a port of
// Waldmeister KPV_KPMengeInterreduzieren, KPVerwaltung.c:1032, whose
// per-CP AP_generic callback re-normalizes / joinable-deletes / reweights
// each queued CP).  Default OFF; flipped on by Method->"Waldmeister".
fn void thvm_atp_set_cp_set_interreduce(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->cp_set_interreduce = on ? 1u : 0u;
}

// Read wall-clock microseconds from CLOCK_REALTIME -- portable
// across linux / macOS / freebsd and good enough for a >=1 second
// deadline budget.
// Host abort hook (see thvm.h): NULL unless a host (e.g. the WL glue)
// installs a poll into Abort[] / TimeConstrained[].
int (*thvm_atp_abort_hook)(void) = NULL;

static u64 atp_now_us(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
  return (u64)ts.tv_sec * 1000000ull + (u64)(ts.tv_nsec / 1000);
}

// Throttled poll (every 256 calls) of the wall deadline + host abort,
// for the inner rewrite loops.  atp_now_us is a clock_gettime syscall,
// so it is gated behind the tick mask to stay off the hot path.  Only
// fires once the deadline has actually passed or a host Abort[] is
// pending, so a normal (non-aborting) normalize is never cut short.
static int atp_norm_deadline_fired(AtpState *s) {
  static u32 tick = 0u;
  if ((++tick & 0xFFu) != 0u) return 0;
  if (s->wall_deadline_us != 0u) {
    u64 now = atp_now_us();
    if (now != 0u && now >= s->wall_deadline_us) return 1;
  }
  if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return 1;
  return 0;
}

fn void thvm_atp_set_wall_deadline(AtpState *s, double seconds_from_now) {
  if (s == NULL) return;
  if (seconds_from_now <= 0.0) { s->wall_deadline_us = 0u; return; }
  u64 now = atp_now_us();
  if (now == 0u) return;  // clock_gettime failed; leave deadline off
  s->wall_deadline_us = now + (u64)(seconds_from_now * 1e6);
}

// Forward decl: defined after the proof-extract machinery further
// down; atp_rewrite_normalize_record below reuses it as the one-step
// metadata-recording rewriter.
static Term atp_proof_rewrite_step(AtpState *s, Term t, u8 *pos, u8 depth,
                                   u32 *out_rule, u8 *out_pos_len,
                                   u8 *out_fwd, u8 *fired);

// Slice-aware companion: same one-step metadata-recording shape as
// atp_proof_rewrite_step but the rule set is a passed-in slice
// (lhs_arr / rhs_arr / n) rather than the live R.  Used by the
// interreduce path so the NORM_STEPs it records cite the (slice-)
// local rule that fired -- the dropped rule's normalization through
// the just-added rules.  `out_rule` is the slice index; the caller
// maps it to a TRACE_ORIENT trace id via its own trace-id array.
static Term atp_proof_rewrite_step_slice(AtpState *s, Term t,
                                         u8 *pos, u8 depth,
                                         const Term *lhs_arr,
                                         const Term *rhs_arr,
                                         u32 n,
                                         u32 *out_rule, u8 *out_pos_len,
                                         u8 *out_fwd, u8 *fired);

// Push a TRACE_NORM_STEP entry recording one rewrite step the CP-
// normalize loop just applied.  Children layout (see thvm.h for the
// schema mirrored by the WL decoder):
//   [NUM(parent_a), NUM(rule_idx), lhs (after step), rhs (after step),
//    NUM(pos_len), NUM(pos_0), ..., NUM(side), NUM(fwd)]
static u32 atp_trace_push_norm_step(AtpState *s, u32 p_a, u32 rule_idx,
                                    Term lhs, Term rhs,
                                    u8 side, u8 fwd,
                                    const u8 *pos, u8 pos_len) {
  if (s == NULL || !atp_trace_ensure(s)) return ATP_TRACE_NONE;
  Term children[7 + ATP_PROOF_MAX_DEPTH];
  children[0] = term_new(0, TAG_NUM, 0, p_a);
  children[1] = term_new(0, TAG_NUM, 0, rule_idx);
  children[2] = lhs;
  children[3] = rhs;
  children[4] = term_new(0, TAG_NUM, 0, pos_len);
  for (u8 k = 0; k < pos_len; k++) {
    children[5u + k] = term_new(0, TAG_NUM, 0, pos[k]);
  }
  children[5u + pos_len]      = term_new(0, TAG_NUM, 0, side);
  children[5u + pos_len + 1u] = term_new(0, TAG_NUM, 0, fwd);
  s->trace[s->n_trace] = term_new_ctr(TRACE_NORM_STEP, children,
                                      7u + pos_len);
  u32 idx = s->n_trace;
  s->n_trace++;
  return idx;
}

// CP-normalize chain recorder: iterate one rewrite step at a time
// via the proof-extracter (atp_proof_rewrite_step), pushing a
// TRACE_NORM_STEP per fire so the WL extractor walks the chain
// linearly.  `eq_other` is the equation's other side (unchanged
// during this side's normalization) -- recorded in each step's
// (lhs, rhs) tuple per `side` (0 lhs / 1 rhs).  Returns the final
// term and updates *chain_tail to the last pushed step's trace
// index (or leaves it at the caller's prev_trace if no step fires).
static Term atp_rewrite_normalize_record(AtpState *s,
                                         Term t, Term eq_other, u8 side,
                                         u32 *chain_tail, u32 step_cap) {
  for (u32 it = 0; it < step_cap; it++) {
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0;
    u8  pos_len = 0, fwd = 1u, fired = 0;
    Term t2 = atp_proof_rewrite_step(s, t, pos, 0u, &rule, &pos_len,
                                     &fwd, &fired);
    if (!fired) break;
    Term step_lhs = (side == 0u) ? t2 : eq_other;
    Term step_rhs = (side == 0u) ? eq_other : t2;
    // Store the rule's TRACE_ORIENT trace index (not mainRules
    // position): mainRules shifts as interreduction drops rules, but
    // each TRACE_ORIENT entry stays at a fixed trace index for the
    // life of the run, so WL can read the rule's stored sides off
    // trace[] regardless of later interreduction.
    u32 rule_trace = (rule < s->n_rules) ? s->r_trace[rule]
                                         : ATP_TRACE_NONE;
    u32 ti = atp_trace_push_norm_step(s, *chain_tail, rule_trace,
                                      step_lhs, step_rhs, side, fwd,
                                      pos, pos_len);
    if (ti != ATP_TRACE_NONE) *chain_tail = ti;
    t = t2;
  }
  return t;
}

// Slice-based companion to atp_rewrite_normalize_record: rewrites
// `t` against the (lhs_arr / rhs_arr / n) slice, pushing one
// TRACE_NORM_STEP per fire.  `trace_arr[i]` is the TRACE_ORIENT
// trace id for slice rule i (so WL can locate the rule entry
// regardless of later compaction).  Used by interreduce so the
// dropped rule's normalization through the just-added rules is
// recorded as a chain of NORM_STEPs -- the resulting TRACE_SIMPLIFY
// parents on the last NORM_STEP, and WL's chain extractor emits
// each step as a SubstitutionLemma directly (no emitNorm BFS).
static Term atp_rewrite_normalize_slice_record(AtpState *s, Term t,
                                               const Term *lhs_arr,
                                               const Term *rhs_arr,
                                               const u32  *trace_arr,
                                               u32 n,
                                               u32 step_cap,
                                               u32 *chain_tail,
                                               u8 side, Term eq_other) {
  for (u32 it = 0; it < step_cap; it++) {
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0;
    u8  pos_len = 0, fwd = 1u, fired = 0;
    Term t2 = atp_proof_rewrite_step_slice(s, t, pos, 0u,
                                           lhs_arr, rhs_arr, n,
                                           &rule, &pos_len, &fwd, &fired);
    if (!fired) break;
    Term step_lhs = (side == 0u) ? t2 : eq_other;
    Term step_rhs = (side == 0u) ? eq_other : t2;
    u32 rule_trace = (rule < n) ? trace_arr[rule] : ATP_TRACE_NONE;
    u32 ti = atp_trace_push_norm_step(s, *chain_tail, rule_trace,
                                      step_lhs, step_rhs, side, fwd,
                                      pos, pos_len);
    if (ti != ATP_TRACE_NONE) *chain_tail = ti;
    t = t2;
  }
  return t;
}

fn AtpStatus thvm_atp_step(AtpState *s) {
  if (s == NULL) return ATP_QUEUE_EMPTY;

  AtpStatus goal = thvm_atp_goal_check(s);
  if (goal != ATP_RUNNING) return goal;

  if (s->step >= s->step_cap) return ATP_TIMEOUT;

  // Wall-clock deadline check.  Polled per outer step; fine grain
  // enough to defend against runaway recursive-axiom expansions
  // (Y combinator's `Y x == x (Y x)` saturates with unbounded CP
  // fan-out) without showing up in the hot loop's profile.
  if (s->wall_deadline_us != 0u) {
    u64 now = atp_now_us();
    if (now != 0u && now >= s->wall_deadline_us) return ATP_TIMEOUT;
  }
  // Host abort (WL Abort[] / TimeConstrained[]): polled per outer step.
  if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return ATP_ABORTED;

  // 7a: in-loop GC.  When the dyn heap has crossed the half-full
  // mark, run a Cheney collection BEFORE allocating this step's
  // normalization / CP-enumeration scratch.  Done here -- before
  // select_cp pops -- so every live Term is still parked in the
  // AtpState arrays and gets rooted by thvm_atp_gc_collect.  A
  // long completion run otherwise exhausts from-space; with this
  // the heap floats around the live working set instead of
  // climbing monotonically.
  if (atp_heap_under_pressure()) {
    thvm_atp_gc_collect(s);
  }

  // Auto-MaxWeight: refresh the bound against the current rule set at
  // the top of the step so this step's first pushes (and the empty-
  // queue force-drain inside select_cp) see a bound consistent with R.
  if (s->auto_max_cp_weight_base > 0u) atp_auto_maxw_recompute(s);

  Term cp_lhs = 0, cp_rhs = 0;
  if (!thvm_atp_select_cp(s, &cp_lhs, &cp_rhs)) {
    return ATP_QUEUE_EMPTY;
  }

  // 9.3: snapshot the heap before the (potentially heavy) IC-routed
  // rewrite cells are allocated.  When the CP is trivially joined
  // (kbo_eq(l, r) below), neither l nor r is referenced downstream
  // and the entire normalize block is dead -- pop back.
  u64 hcp_norm = thvm_atp_heap_checkpoint();

  const u32 NORM_CAP = 64;
  u32 src_trace  = s->last_popped_trace;
  u32 chain_tail = src_trace;
  Term l, r;
  if (s->record_norm_steps) {
    // Record each CP-normalize rewrite as a TRACE_NORM_STEP, chained
    // from the CP -- WL walks the chain linearly when extracting the
    // ProofObject.  Start from the trace entry's RAW (un-reduced)
    // CP form, not select_cp's already-queue-reduced form: queue-
    // time reduction (atp_cp_trivially_joinable) was previously off-
    // trace, so the resulting NORM_STEPs reach from the literal CP
    // the verifier expects all the way to the form orient sees --
    // no chain gap.
    Term raw_lhs = cp_lhs;
    Term raw_rhs = cp_rhs;
    if (src_trace != ATP_TRACE_NONE && src_trace < s->n_trace) {
      Term cp_te = s->trace[src_trace];
      if (term_tag(cp_te) == TAG_CTR && term_ext(cp_te) == TRACE_CP &&
          term_ctr_n(cp_te) >= 4u) {
        raw_lhs = term_ctr_at(cp_te, 2);
        raw_rhs = term_ctr_at(cp_te, 3);
      }
    }
    l = atp_rewrite_normalize_record(s, raw_lhs, raw_rhs, 0u,
                                     &chain_tail, NORM_CAP);
    r = atp_rewrite_normalize_record(s, raw_rhs, l, 1u,
                                     &chain_tail, NORM_CAP);
  } else {
    l = atp_rewrite_normalize(s, cp_lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
    r = atp_rewrite_normalize(s, cp_rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  }

  if (kbo_eq(l, r)) {
    // Skipping the heap reset when norm-step recording is on: the
    // TRACE_NORM_STEP entries just pushed reference the intermediate
    // Terms allocated during the normalize, and rewinding the heap
    // would leave their children dangling.
    if (!s->record_norm_steps) {
      thvm_atp_heap_reset(hcp_norm);
    }
    s->step++;
    return ATP_RUNNING;
  }

  AtpAddedRange added = thvm_atp_orient_and_add(s, l, r);
  if (added.count == 0) {
    // R full, or some other refusal.  Count the work and continue.
    s->step++;
    return ATP_RUNNING;
  }

  // Trace each newly-added rule with its source CP as parent_a (or
  // the chain tail when norm-step recording is on: the chain ends at
  // the last NORM_STEP, which the ORIENT inherits from).  For
  // unfailing 2-way fallback both directions get separate entries so
  // PCL output can identify each rule individually.  Stash the trace
  // index in r_trace[] so generate_cps can record TRACE_CP parents
  // for any CP born from this rule.
  for (u32 k = 0; k < added.count; k++) {
    Term rl = s->lhs[added.first + k];
    Term rr = s->rhs[added.first + k];
    u32  t  = atp_trace_push(s, TRACE_ORIENT, chain_tail,
                             ATP_TRACE_NONE, rl, rr);
    s->r_trace[added.first + k] = t;
  }

  // Interreduce shifts new-rule indices down by `dropped`.
  u32 dropped = thvm_atp_interreduce(s, added);
  AtpAddedRange post = added;
  post.first = (dropped > post.first) ? 0 : (post.first - dropped);

  // Auto-MaxWeight: refresh the bound against the now-current rule set
  // before this step's CPs are generated/pushed (the just-oriented rule
  // may have deepened or, via interreduce, shrunk R).
  if (s->auto_max_cp_weight_base > 0u) atp_auto_maxw_recompute(s);

  thvm_atp_generate_cps(s, post);

  // Re-admit any stashed CP now within the refreshed bound.
  if (s->auto_max_cp_weight_base > 0u) atp_auto_maxw_drain(s, 0u);

#ifdef ATP_CP_GRAPH
  // 8b: a rule was oriented (R changed), so every queued CP may now
  // simplify under it.  Sweep the WHOLE cp_graph once, applying the
  // newly-oriented rule(s) with a memo shared across all CPs -- a
  // subterm common to many CPs is rewritten once.  Trivially-joined
  // CPs collapse and drop out here.  The lazy per-CP normalize at the
  // top of the next step still runs (full R, catching any cascaded
  // redex); 8b makes the popped CP cheaper because it is already
  // simplified under every rule oriented since it was queued.
  atp_normalize_graph(s, post);
#endif

  // Periodic full-rule-set CP-queue interreduction (Waldmeister
  // KPV_KPMengeInterreduzieren).  Gated behind cp_set_interreduce and run
  // every cp_set_ir_period-th rule addition so the per-CP full-R sweep's
  // cost is amortized.  Default off -> the call is skipped and the engine
  // is byte-identical.
  if (s->cp_set_interreduce && added.count > 0u) {
    u32 period = s->cp_set_ir_period ? s->cp_set_ir_period
                                     : ATP_CP_SET_IR_PERIOD;
    if (s->n_rules % period == 0u) {
      atp_cp_set_interreduce(s);
    }
  }

  goal = thvm_atp_goal_check(s);
  if (goal != ATP_RUNNING) return goal;

  s->step++;
  return ATP_RUNNING;
}

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
// buffer overflow.

static u32 atp_pretty_term(Term t, char *buf, u32 cap);

static u32 atp_pretty_ctr(Term t, char *buf, u32 cap) {
  if (cap <= 1) return 0;
  u32 lab = term_ext(t);
  u32 n   = term_ctr_n(t);
  int n_w = snprintf(buf, cap, "C%u", lab);
  if (n_w < 0) return 0;
  u32 w = (u32)n_w;
  if (w >= cap) return cap - 1;
  if (n == 0) return w;
  if (w + 1 >= cap) return w;
  w += (u32)snprintf(buf + w, cap - w, "(");
  for (u32 i = 0; i < n; i++) {
    if (w + 2 >= cap) break;
    if (i > 0) w += (u32)snprintf(buf + w, cap - w, ", ");
    if (w >= cap) return cap - 1;
    w += atp_pretty_term(term_ctr_at(t, i), buf + w, cap - w);
    if (w >= cap) return cap - 1;
  }
  if (w + 1 < cap) w += (u32)snprintf(buf + w, cap - w, ")");
  return w;
}

static u32 atp_pretty_term(Term t, char *buf, u32 cap) {
  if (cap == 0) return 0;
  switch (term_tag(t)) {
    case TAG_FVR: return (u32)snprintf(buf, cap, "x_%u", term_ext(t));
    case TAG_NUM: return (u32)snprintf(buf, cap, "#%u", (u32)term_val(t));
    case TAG_ERA: return (u32)snprintf(buf, cap, "ERA");
    case TAG_CTR: return atp_pretty_ctr(t, buf, cap);
    default:      return (u32)snprintf(buf, cap, "?T%u", term_tag(t));
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
    Term lhs    = term_ctr_at(entry, 2);
    Term rhs    = term_ctr_at(entry, 3);

    const char *type_str = "?";
    switch (reason) {
      case TRACE_AXIOM:    type_str = "axiom";    break;
      case TRACE_ORIENT:   type_str = "orient";   break;
      case TRACE_CP:       type_str = "cp";       break;
      case TRACE_SIMPLIFY: type_str = "simplify"; break;
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
      if (atp_vars_contained(s->rhs[i], s->lhs[i])) {       // l -> r
        RewriteSubst sub = {{0}};
        if (thvm_match(s->lhs[i], t, &sub)) {
          Term repl = thvm_subst_apply(s->rhs[i], &sub);
          if (atp_compare(s, t, repl) == KBO_GT) {
            *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
            return repl;
          }
        }
      }
      if (atp_vars_contained(s->lhs[i], s->rhs[i])) {       // r -> l
        RewriteSubst sub = {{0}};
        if (thvm_match(s->rhs[i], t, &sub)) {
          Term repl = thvm_subst_apply(s->lhs[i], &sub);
          if (atp_compare(s, t, repl) == KBO_GT) {
            *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 0;
            return repl;
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
  for (u32 i = 0; i < n; i++) {
#ifdef ATP_ORDERED_REWRITE
    u8 oriented = (u8)(atp_compare(s, lhs_arr[i], rhs_arr[i]) == KBO_GT);
    if (!oriented) {
      if (atp_vars_contained(rhs_arr[i], lhs_arr[i])) {       // l -> r
        RewriteSubst sub = {{0}};
        if (thvm_match(lhs_arr[i], t, &sub)) {
          Term repl = thvm_subst_apply(rhs_arr[i], &sub);
          if (atp_compare(s, t, repl) == KBO_GT) {
            *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
            return repl;
          }
        }
      }
      if (atp_vars_contained(lhs_arr[i], rhs_arr[i])) {       // r -> l
        RewriteSubst sub = {{0}};
        if (thvm_match(rhs_arr[i], t, &sub)) {
          Term repl = thvm_subst_apply(lhs_arr[i], &sub);
          if (atp_compare(s, t, repl) == KBO_GT) {
            *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 0;
            return repl;
          }
        }
      }
      continue;
    }
#endif
    RewriteSubst sub = {{0}};
    if (thvm_match(lhs_arr[i], t, &sub)) {
      *fired = 1; *out_rule = i; *out_pos_len = depth; *out_fwd = 1;
      return thvm_subst_apply(rhs_arr[i], &sub);
    }
  }
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

// Normalize one conjecture side, appending each rewrite to out[*n).
// `side` tags every recorded step; returns the side's normal form.
static Term atp_proof_record_side(AtpState *s, Term t, u32 side,
                                  AtpProofStep *out, u32 cap, u32 *n) {
  for (u32 it = 0; it < ATP_PROOF_MAX_STEPS; it++) {
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0;
    u8  pos_len = 0, fwd = 1u, fired = 0;
    Term t2 = atp_proof_rewrite_step(s, t, pos, 0u, &rule, &pos_len,
                                     &fwd, &fired);
    if (!fired || kbo_eq(t, t2)) return t;
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
    t = t2;
  }
  return t;
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
  Term nf_lhs = atp_proof_record_side(s, s->goal_lhs, 0u, out, cap, &n);
  Term nf_rhs = atp_proof_record_side(s, s->goal_rhs, 1u, out, cap, &n);

  // Not single-NF provable -- the two sides never meet under R.  A
  // symmetric goal closed only by the MNF search lands here.
  if (!kbo_eq(nf_lhs, nf_rhs)) return 0;

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

// === ENIGMA-style CP feature extraction + labelled dataset ==========
// Data-foundation step for a LEARNED critical-pair selector.  See
// thvm.h for the feature schema; training (logistic regression / GBDT)
// and the resulting fast C scorer in select_cp are a later step.

// Count distinct FVR ids and total FVR occurrences across (l, r) in a
// single pair of walks.  `seen` is a small membership bitset keyed on
// the var id modulo its width (var ids are dense [0,k) after
// ATP_VAR_NORM, so collisions don't happen on the canonical CP forms
// this records; the count is exact for k <= 64).
static void atp_cp_var_stats_rec(Term t, u64 *seen, u32 *distinct,
                                 u32 *occ) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      (*occ)++;
      u32 bit = id & 63u;
      if (!((*seen >> bit) & 1u)) {
        *seen |= (1ull << bit);
        (*distinct)++;
      }
      return;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        atp_cp_var_stats_rec(term_ctr_at(t, i), seen, distinct, occ);
      }
      return;
    }
    default: return;
  }
}

// Top CTR label of a term (the feature's "top symbol"); 0 for a
// variable / atom / non-CTR root.
static u32 atp_cp_top_symbol(Term t) {
  return (term_tag(t) == TAG_CTR) ? term_ext(t) : 0u;
}

// Does any top-symbol-headed subterm of `t` also occur (by top symbol)
// as a subterm of the goal term `g`?  A cheap structural-overlap proxy
// for ENIGMA's goal-distance feature: 1 if the CP touches a function
// symbol the goal uses at some position, else 0.  Variables carry no
// goal signal (a var generalizes everything), mirroring the MinStruct
// spirit of the CPinGoal classifier.
static int atp_term_label_set(Term t, u64 *labels) {
  if (term_tag(t) == TAG_CTR) {
    u32 lab = term_ext(t);
    *labels |= (1ull << (lab & 63u));
    u32 n = term_ctr_n(t);
    for (u32 i = 0; i < n; i++) atp_term_label_set(term_ctr_at(t, i), labels);
  }
  return 0;
}
static int atp_cp_shares_goal_symbol(const AtpState *s, Term l, Term r) {
  if (s == NULL || s->goal_lhs == 0) return 0;
  Term gl = s->goal_lhs_nf ? s->goal_lhs_nf : s->goal_lhs;
  Term gr = s->goal_rhs_nf ? s->goal_rhs_nf : s->goal_rhs;
  u64 goal_labels = 0u;
  atp_term_label_set(gl, &goal_labels);
  atp_term_label_set(gr, &goal_labels);
  u64 cp_labels = 0u;
  atp_term_label_set(l, &cp_labels);
  atp_term_label_set(r, &cp_labels);
  return (goal_labels & cp_labels) != 0u;
}

fn void thvm_atp_cp_features(const AtpState *s, Term lhs, Term rhs,
                             u32 age, float *out) {
  if (out == NULL) return;
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) out[i] = 0.0f;

  u32 sl = atp_symbol_count(lhs), sr = atp_symbol_count(rhs);
  u32 dl = atp_term_depth(lhs),   dr = atp_term_depth(rhs);
  u64 seen = 0u; u32 distinct = 0u, occ = 0u;
  atp_cp_var_stats_rec(lhs, &seen, &distinct, &occ);
  atp_cp_var_stats_rec(rhs, &seen, &distinct, &occ);

  // `s` is logically const for feature reads but the weight helpers
  // take a non-const AtpState (they only read the rule set / config).
  AtpState *sm = (AtpState *)s;

  out[0]  = (float)(sl + sr);
  out[1]  = (float)(dl > dr ? dl : dr);
  out[2]  = (float)distinct;
  out[3]  = (float)occ;
  out[4]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_ADD);
  out[5]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_GT);
  out[6]  = (float)atp_cp_weight_base(sm, lhs, rhs, ATP_CP_WEIGHT_MIX2);
  out[7]  = (float)atp_goal_weight(s, lhs, rhs);
  out[8]  = (float)age;
  out[9]  = (float)atp_cp_top_symbol(lhs);
  out[10] = (float)atp_cp_top_symbol(rhs);
  out[11] = (float)atp_cp_shares_goal_symbol(s, lhs, rhs);
  KboCmp c = atp_compare(sm, lhs, rhs);
  out[12] = (float)((c == KBO_GT || c == KBO_LT) ? 1 : 0);
  out[13] = (float)atp_unif_measure(lhs, rhs);
}

fn void thvm_atp_set_record_cp_features(AtpState *s, u8 on) {
  if (s == NULL) return;
  s->record_cp_features = on ? 1u : 0u;
}

// Grow the recording arrays to hold at least `need` rows.
static void atp_cp_feat_ensure(AtpState *s, u32 need) {
  if (need <= s->cp_feat_cap) return;
  u32 cap = s->cp_feat_cap ? s->cp_feat_cap * 2u : 256u;
  while (cap < need) cap *= 2u;
  s->cp_feat_rows  = (float *)realloc(s->cp_feat_rows,
                                      (size_t)cap * ATP_CP_FEATURE_DIM
                                        * sizeof(float));
  s->cp_feat_trace = (u32 *)realloc(s->cp_feat_trace, cap * sizeof(u32));
  s->cp_feat_label = (u8 *)realloc(s->cp_feat_label, cap * sizeof(u8));
  s->cp_feat_cap   = cap;
}

// Record one PROCESSED CP (called from thvm_atp_select_cp under the
// flag).  `trace_id` is the popped CP's trace-entry index.
static void atp_cp_feat_record(AtpState *s, Term lhs, Term rhs,
                               u32 trace_id) {
  atp_cp_feat_ensure(s, s->n_cp_feat + 1u);
  float *row = s->cp_feat_rows + (size_t)s->n_cp_feat * ATP_CP_FEATURE_DIM;
  thvm_atp_cp_features(s, lhs, rhs, s->n_cp_feat, row);
  s->cp_feat_trace[s->n_cp_feat] = trace_id;
  s->cp_feat_label[s->n_cp_feat] = 0u;
  s->n_cp_feat++;
}

// Parents (in the trace DAG) of trace entry `ti`: for the 4-child
// reasons (AXIOM/ORIENT/CP/SIMPLIFY) children 0 and 1 are parent trace
// ids; for TRACE_NORM_STEP only child 0 is a parent trace id (child 1
// is a RULE index, not a trace id).  Writes up to 2 parents into
// out[0..2) and returns the count.
static u32 atp_trace_parents(const AtpState *s, u32 ti, u32 *out) {
  if (ti == ATP_TRACE_NONE || ti >= s->n_trace) return 0u;
  Term e = s->trace[ti];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 2u) return 0u;
  u32 reason = term_ext(e);
  u32 n = 0u;
  u32 p0 = term_val(term_ctr_at(e, 0));
  if (p0 != ATP_TRACE_NONE) out[n++] = p0;
  if (reason != TRACE_NORM_STEP) {
    u32 p1 = term_val(term_ctr_at(e, 1));
    if (p1 != ATP_TRACE_NONE) out[n++] = p1;
  }
  return n;
}

fn u32 thvm_atp_cp_label(AtpState *s) {
  if (s == NULL || s->n_cp_feat == 0u) return 0u;

  // Proof set seed: the RULES that join the goal, via the existing
  // single-NF proof extractor.  Each fired rule's r_trace[] is its
  // TRACE_ORIENT entry; the proof-relevant CPs are the transitive
  // trace-DAG ancestors of those entries.
  static AtpProofStep steps[ATP_PROOF_MAX_STEPS * 2u];
  u32 n_steps = thvm_atp_proof_extract(s, steps,
                                       ATP_PROOF_MAX_STEPS * 2u);
  if (n_steps == 0u) return 0u;   // not single-NF extractable

  // Reachable-trace bitset over [0, n_trace).  Marked entries are the
  // proof set: ancestors of every fired rule's TRACE_ORIENT.
  u8 *reach = (u8 *)calloc(s->n_trace, 1u);
  if (reach == NULL) return 0u;

  // Worklist DFS from each proof rule's TRACE_ORIENT trace id.
  u32 *stack = (u32 *)malloc((size_t)s->n_trace * sizeof(u32));
  if (stack == NULL) { free(reach); return 0u; }
  u32 sp = 0u;
  for (u32 k = 0; k < n_steps; k++) {
    u32 ru = steps[k].rule;
    if (ru >= s->n_rules) continue;
    u32 ti = s->r_trace[ru];
    if (ti != ATP_TRACE_NONE && ti < s->n_trace && !reach[ti]) {
      reach[ti] = 1u;
      stack[sp++] = ti;
    }
  }
  while (sp > 0u) {
    u32 ti = stack[--sp];
    u32 par[2];
    u32 np = atp_trace_parents(s, ti, par);
    for (u32 i = 0; i < np; i++) {
      u32 p = par[i];
      if (p < s->n_trace && !reach[p]) {
        reach[p] = 1u;
        stack[sp++] = p;
      }
    }
  }

  // Label each recorded selected-CP: 1 iff its trace id is in the
  // proof set.  Count distinct proof-relevant selected CPs.
  u32 n_pos = 0u;
  for (u32 i = 0; i < s->n_cp_feat; i++) {
    u32 ti = s->cp_feat_trace[i];
    u8 lab = (ti != ATP_TRACE_NONE && ti < s->n_trace && reach[ti]) ? 1u : 0u;
    s->cp_feat_label[i] = lab;
    if (lab) n_pos++;
  }

  free(stack);
  free(reach);
  return n_pos;
}

fn u32 thvm_atp_cp_dataset_append(const AtpState *s, const char *path,
                                  u8 header) {
  if (s == NULL || path == NULL || s->n_cp_feat == 0u) return 0u;
  FILE *f = fopen(path, "a");
  if (f == NULL) return 0u;
  if (header) {
    fprintf(f, "label");
    static const char *names[ATP_CP_FEATURE_DIM] = {
      "size_sum", "max_depth", "n_distinct_vars", "n_var_occ",
      "weight_add", "weight_gt", "weight_mix2", "goal_weight",
      "age", "top_symbol_l", "top_symbol_r", "shares_goal_sub",
      "orientable", "unif_measure",
    };
    for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) fprintf(f, "\t%s", names[j]);
    fprintf(f, "\n");
  }
  for (u32 i = 0; i < s->n_cp_feat; i++) {
    const float *row = s->cp_feat_rows + (size_t)i * ATP_CP_FEATURE_DIM;
    fprintf(f, "%u", (unsigned)s->cp_feat_label[i]);
    for (u32 j = 0; j < ATP_CP_FEATURE_DIM; j++) fprintf(f, "\t%.6g", row[j]);
    fprintf(f, "\n");
  }
  fclose(f);
  return s->n_cp_feat;
}

// Drive thvm_atp_step until it returns a non-RUNNING status.
fn AtpStatus thvm_atp_run(AtpState *s) {
  AtpStatus st;
  do {
    st = thvm_atp_step(s);
  } while (st == ATP_RUNNING);
  return st;
}

// Goal check: normalize both sides of the conjecture under the
// current R; if they're now structurally equal, the goal is
// proved.  Returns ATP_PROVED on a hit, ATP_RUNNING otherwise.
// Skips cleanly (returns ATP_RUNNING) when no goal is set
// (goal_lhs == 0) -- the completion-mode case.
//
// Top-only rewriting today via thvm_rewrite_normalize; stage 5.4's
// recursive descent will widen coverage to sub-positions.
//
// Step cap NORM_CAP = 64 bounds the normalization (matches the
// ballpark used in tests/test_rewrite.c's headline demo); tune
// once we have benchmark data.
// 8.9c: per-iteration narrow budget.  Each goal_check call tries
// up to this many narrow_step iterations before giving up; the
// outer saturation loop calls goal_check again next iteration
// with the (potentially larger) rule set.
#define ATP_NARROW_BUDGET 8

#ifdef ATP_MNF
// === Milestone 10: MNF -- the Multiple-Normal-Forms goal search =====
//
// A port of Waldmeister's MNF module.  The conjecture stops being a
// passive single-normal-form check and becomes a goal search: goal_lhs
// seeds a GREEN front, goal_rhs a RED one.  Each front *normalises* its
// term forward (l->r) with the current rule set R; because R is not
// confluent a term has many forward reducts, so each front is a set,
// held in a hash table.  When a reduct collides with a term already in
// the table of the OPPOSITE colour the fronts have met: goal_lhs and
// goal_rhs share a rewrite path, so the goal is proved.
//
// The set is fed incrementally -- each rule completion derives is
// applied to the already-reached terms -- so progress comes from
// completion growing R until the fronts' forward reducts coincide.
//
// Search order is Waldmeister's irreducible-adaptive deque: expand
// depth-first (newest node) while reductions stay productive, switch
// to breadth-first (oldest node) the moment the last node expanded was
// irreducible -- a normal form, a dead end (see mnf_pop / mnf_step).
//
// Backward "anti" steps (r->l): see MNF_MAX_ANTI.

#define MNF_RED        0u
#define MNF_GREEN      1u
// Backward "anti" steps (r->l) -- a port of Waldmeister's antiWOVar:
// variable-safe backward rewriting through unorientable equations,
// capped at MNF_MAX_ANTI per lineage.  Forward-only fronts (anti=0)
// cannot close a symmetric goal and rely on completion alone, which
// for NAND commutativity from the single Wolfram axiom diverges.
// anti=2 is the minimum that closes wolfram.pr (anti=1 does not;
// anti>=2 does), and is the default.  Overridable with
// -DMNF_MAX_ANTI=N.
#ifndef MNF_MAX_ANTI
#define MNF_MAX_ANTI   2u
#endif
#define MNF_MAX_NODES  400000u
#define MNF_SUCC_CAP   2048u
// First-expansion nodes per goal_check.  The collision step is gated
// by completion (the fronts cannot join before completion derives the
// enabling rule), NOT by this budget: an A/B sweep 8..384 on wolfram
// moved the proving step only 363..377 while wall time scaled
// linearly with the budget.  A large budget is therefore pure wasted
// MNF work -- it just grows the front (and the cost of re-expanding
// it against every new rule) faster than completion can use it.  16
// keeps the front growth proportional to completion's pace; wolfram
// 15.2 s -> 1.8 s.
#define MNF_BUDGET     16u
#define MNF_N_BUCKETS  (1u << 21)        // 2097152, power of two
#define MNF_ROOT_PARENT 0xFFFFFFFFu      // mnf_insert: this term is a seed

typedef struct {
  Term term;
  u32  hash;
  u32  parent;     // node this term was first reached from (root: self)
  u8   colour;     // MNF_RED / MNF_GREEN
  u8   anti;       // backward steps used on the path to this term
  u8   expanded;   // successors already generated against [0, n_rules_seen)
  u8   irred;      // 1 until a forward (size-reducing) successor is found
} MnfNode;

typedef struct AtpMnf {
  MnfNode *nodes;
  u32      n_nodes;
  u32      cap_nodes;
  // Each front is a deque of pending node indices.  Successors are
  // pushed on the left; mnf_pop takes the left (newest, depth-first)
  // or right (oldest, breadth-first) end per the irred-adaptive policy.
  u32     *qred;
  u32     *qgreen;
  u32      qred_head, qred_tail;       // live deque span is q[head, tail)
  u32      qgreen_head, qgreen_tail;
  u32      q_cap;
  u8       red_last_irred;             // last RED node expanded was a NF
  u8       green_last_irred;
  u32     *buckets;          // open addressing: bucket -> node_idx + 1
  u32      n_buckets;
  u32      n_rules_seen;     // rules already fed in
  Term     seed_red;         // goal_rhs -- the RED front's origin
  Term     seed_green;       // goal_lhs -- the GREEN front's origin
  u32      n_red, n_green;   // nodes reached per colour
  u32      n_anti;           // nodes reached via a backward (r->l) step
  u32      n_dup;            // same-colour duplicate terms (dropped)
  u32      n_trunc;          // node expansions that hit MNF_SUCC_CAP
  u8       full;             // node table reached MNF_MAX_NODES
  u8       joined;
  // The join witness (also used by the proof extractor, so always
  // present, not behind ATP_MNF_DIAG).  Side A is the existing table
  // node a fresh reduct collided with; side B is that fresh reduct --
  // never created as a node, so it is identified by its parent and the
  // colliding term.  meet_term == nodes[meet_a].term == the reduct.
  u32      meet_a;           // existing table node the join collided with
  u32      meet_b_parent;    // parent of the colliding (uncreated) node
  Term     meet_term;        // the term both fronts reached
  u8       meet_b_col;       // colour of the colliding (uncreated) node
} AtpMnf;

// Structural hash of a term (FNV-ish mix over the preorder).
static u32 mnf_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 h = 0x811c9dc5u ^ (term_ext(t) * 0x01000193u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h = (h ^ mnf_hash(term_ctr_at(t, i))) * 0x01000193u;
      }
      return h ^ (n + 0x9e3779b9u);
    }
    case TAG_FVR:
      return (0x2545f491u ^ term_ext(t)) * 0x01000193u;
    default:
      return (0xdeadbeefu ^ (u32)term_tag(t)) * 0x01000193u;
  }
}

// Find a node whose term is kbo_eq to `t` (hash `h`); MNF_MAX_NODES if
// absent.  The table holds one node per distinct term -- the first
// colour to reach it.
static u32 mnf_lookup(const AtpMnf *m, Term t, u32 h) {
  u32 mask = m->n_buckets - 1u;
  for (u32 probe = h & mask; ; probe = (probe + 1u) & mask) {
    u32 slot = m->buckets[probe];
    if (slot == 0u) return MNF_MAX_NODES;
    u32 idx = slot - 1u;
    if (m->nodes[idx].hash == h && kbo_eq(m->nodes[idx].term, t)) return idx;
  }
}

// Insert `t` (colour `col`, lineage anti-count `anti`, reached from node
// `parent` -- MNF_ROOT_PARENT for a seed).  Returns 1 if this insertion
// JOINED the fronts (an opposite-colour node already held `t`).  Same-
// colour duplicate -> dropped.  Fresh term -> added, indexed, enqueued.
static int mnf_insert(AtpMnf *m, Term t, u8 col, u8 anti, u32 parent) {
  u32 h   = mnf_hash(t);
  u32 idx = mnf_lookup(m, t, h);
  if (idx != MNF_MAX_NODES) {
    if (m->nodes[idx].colour != col) {
      m->meet_a        = idx;
      m->meet_b_parent = parent;
      m->meet_term     = t;
      m->meet_b_col    = col;
      m->joined = 1u;
      return 1;
    }
    m->n_dup++;
    return 0;
  }
  if (m->n_nodes >= MNF_MAX_NODES) { m->full = 1u; return 0; }   // set full
  u32 ni = m->n_nodes++;
  m->nodes[ni].term     = t;
  m->nodes[ni].hash     = h;
  m->nodes[ni].parent   = (parent == MNF_ROOT_PARENT) ? ni : parent;
  m->nodes[ni].colour   = col;
  m->nodes[ni].anti     = anti;
  m->nodes[ni].expanded = 0u;
  m->nodes[ni].irred    = 1u;   // until a forward successor proves otherwise
  if (col == MNF_RED) m->n_red++; else m->n_green++;
  if (anti > 0u) m->n_anti++;
  u32 mask = m->n_buckets - 1u;
  u32 probe = h & mask;
  while (m->buckets[probe] != 0u) probe = (probe + 1u) & mask;
  m->buckets[probe] = ni + 1u;
  // Push left onto the colour's deque (each node enqueued exactly once).
  if (col == MNF_RED) m->qred[--m->qred_head]     = ni;
  else                m->qgreen[--m->qgreen_head] = ni;
  return 0;
}

// Per-rule caches, refreshed once per mnf_step over the whole rule
// set so the successor recursion reads them instead of recomputing:
//   g_mnf_vc   -- vars(lhs) subset of vars(rhs) (variable-safe backward)
//   g_mnf_ln   -- nodes(lhs[j]) -- the forward-match size pre-filter
//   g_mnf_rn   -- nodes(rhs[j]) -- the backward-match size pre-filter
// A one-way match needs nodes(pattern) <= nodes(subject), so a rule
// whose matched side outsizes the term cannot fire -- the integer
// compare skips the thvm_match entirely.
static u8  *g_mnf_vc     = NULL;
static u32 *g_mnf_ln     = NULL;
static u32 *g_mnf_rn     = NULL;
static u32  g_mnf_vc_cap = 0u;

// One-step rewrites of `t` (all positions, rules [rule_lo, rule_hi),
// forward + -- when allow_anti -- variable-safe backward) collected
// into mnf_succ_buf / mnf_succ_anti.
static Term mnf_succ_buf[MNF_SUCC_CAP];
static u8   mnf_succ_anti[MNF_SUCC_CAP];

// Returns nodes(t).  Children are expanded first so the size is known
// when rules are tried at t -- the size pre-filter then skips a rule
// whose matched side has more nodes than t (a one-way match needs
// nodes(pattern) <= nodes(subject), so such a rule provably fails).
// Successor order in the buffer differs from a rules-first walk, but
// every successor is still inserted, so the MNF set is unchanged.
static u32 mnf_successors(AtpState *s, Term t, u8 allow_anti,
                          u32 rule_lo, u32 rule_hi, u32 *n) {
  u32 size = 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 m = term_ctr_n(t);
    if (m > REWRITE_MAX_ARITY) return 0xFFFFFFu;  // unreachable; disable filter
    for (u32 i = 0; i < m; i++) {
      u32 base = *n;
      size += mnf_successors(s, term_ctr_at(t, i), allow_anti,
                             rule_lo, rule_hi, n);
      for (u32 k = base; k < *n; k++) {
        Term ch[REWRITE_MAX_ARITY];
        for (u32 c = 0; c < m; c++) {
          ch[c] = (c == i) ? mnf_succ_buf[k] : term_ctr_at(t, c);
        }
        mnf_succ_buf[k] = term_new_ctr(term_ext(t), ch, m);
      }
    }
  }
  // `size` is now nodes(t) -- try every rule at t, size-filtered.
  for (u32 j = rule_lo; j < rule_hi && *n < MNF_SUCC_CAP; j++) {
    if (g_mnf_ln[j] <= size) {
      RewriteSubst sub = {{0}};
      if (thvm_match(s->lhs[j], t, &sub)) {
        mnf_succ_buf[*n]  = thvm_subst_apply(s->rhs[j], &sub);
        mnf_succ_anti[*n] = 0u;
        (*n)++;
        if (*n >= MNF_SUCC_CAP) return size;
      }
    }
    if (allow_anti && g_mnf_vc[j] && g_mnf_rn[j] <= size) {
      RewriteSubst sb = {{0}};
      if (thvm_match(s->rhs[j], t, &sb)) {
        mnf_succ_buf[*n]  = thvm_subst_apply(s->lhs[j], &sb);
        mnf_succ_anti[*n] = 1u;
        (*n)++;
      }
    }
  }
  return size;
}

// Generate node `ni`'s successors against rules [rule_lo, rule_hi) and
// insert each (same colour, anti-count bumped for a backward step).  A
// node that yields at least one forward (size-reducing) rewrite is
// reducible -- its `irred` flag is cleared; one that yields none is a
// normal form and stays irreducible.
static void mnf_expand_node(AtpState *s, AtpMnf *m, u32 ni,
                            u32 rule_lo, u32 rule_hi) {
  u8   col  = m->nodes[ni].colour;
  u8   anti = m->nodes[ni].anti;
  Term t    = m->nodes[ni].term;
  u32  n    = 0u;
  (void)mnf_successors(s, t, (u8)(anti < MNF_MAX_ANTI), rule_lo, rule_hi, &n);
  if (n >= MNF_SUCC_CAP) m->n_trunc++;   // successor buffer overflowed
  for (u32 k = 0; k < n && !m->joined; k++) {
    if (mnf_succ_anti[k] == 0u) m->nodes[ni].irred = 0u;   // forward redex
    mnf_insert(m, mnf_succ_buf[k], col, (u8)(anti + mnf_succ_anti[k]), ni);
  }
}

// Create the MNF set for the current goal: goal_lhs -> GREEN front,
// goal_rhs -> RED front.  An immediate collision (goal_lhs kbo_eq
// goal_rhs) already joins.
static AtpMnf *mnf_create(AtpState *s) {
  AtpMnf *m = (AtpMnf *)calloc(1, sizeof(AtpMnf));
  if (m == NULL) return NULL;
  m->cap_nodes = MNF_MAX_NODES;
  m->q_cap     = MNF_MAX_NODES;
  m->n_buckets = MNF_N_BUCKETS;
  m->nodes   = (MnfNode *)calloc(m->cap_nodes, sizeof(MnfNode));
  m->qred    = (u32 *)calloc(m->q_cap, sizeof(u32));
  m->qgreen  = (u32 *)calloc(m->q_cap, sizeof(u32));
  m->buckets = (u32 *)calloc(m->n_buckets, sizeof(u32));
  if (m->nodes == NULL || m->qred == NULL || m->qgreen == NULL ||
      m->buckets == NULL) {
    free(m->nodes); free(m->qred); free(m->qgreen);
    free(m->buckets); free(m);
    return NULL;
  }
  // Deques start empty at the right end -- mnf_insert pushes left.
  m->qred_head   = m->qred_tail   = m->q_cap;
  m->qgreen_head = m->qgreen_tail = m->q_cap;
  m->seed_green = s->goal_lhs;     // the GREEN front's origin
  m->seed_red   = s->goal_rhs;     // the RED front's origin
  mnf_insert(m, s->goal_lhs, MNF_GREEN, 0u, MNF_ROOT_PARENT);
  mnf_insert(m, s->goal_rhs, MNF_RED,   0u, MNF_ROOT_PARENT);
  return m;
}

static void mnf_destroy(struct AtpMnf *m) {
  if (m == NULL) return;
  free(m->nodes); free(m->qred); free(m->qgreen); free(m->buckets); free(m);
}

// GC support: the coloured nodes' terms are collector roots.  Gather
// them for thvm_atp_gc_collect and write the relocated terms back; the
// hash table (structural hashes, node indices) is GC-invariant.
static u32 mnf_gc_count(struct AtpMnf *m) {
  return (m == NULL) ? 0u : m->n_nodes;
}
static void mnf_gc_gather(struct AtpMnf *m, Term *roots, u32 *w) {
  if (m == NULL) return;
  for (u32 i = 0; i < m->n_nodes; i++) roots[(*w)++] = m->nodes[i].term;
}
static void mnf_gc_writeback(struct AtpMnf *m, const Term *roots, u32 base) {
  if (m == NULL) return;
  for (u32 i = 0; i < m->n_nodes; i++) m->nodes[i].term = roots[base + i];
}

// Take a node from a colour's deque.  Waldmeister's irreducible-
// adaptive policy: if the last node expanded for this colour was
// irreducible (a normal form -- a dead end), pop the OLDEST node
// (right end, breadth-first -- go try elsewhere); otherwise pop the
// NEWEST (left end, depth-first -- keep driving the reduction down).
static u32 mnf_pop(u32 *q, u32 *head, u32 *tail, u8 last_irred) {
  if (last_irred) return q[--(*tail)];   // FIFO end: oldest
  return q[(*head)++];                   // LIFO end: newest
}

#ifdef ATP_MNF_DIAG
// Independent join verifier.  A join is sound because every MNF node is,
// by construction, a one-step equational rewrite of its parent (forward
// l->r or variable-safe backward r->l) -- so goal_lhs ->* meet <-* goal_rhs
// is a closed equational chain.  This re-checks that claim from the parent
// pointers: walk `ni` up to its root and confirm each child term is a
// genuine one-step rewrite of its parent under the FINAL rule set.  Steps
// taken with a rule that interreduction has since retired will not replay
// (still equationally valid -- just no longer in R); `*replayed` counts
// the ones that do.  Returns the root node index.
static u32 mnf_verify_chain(AtpState *s, AtpMnf *m, u32 ni,
                            u32 *len, u32 *replayed) {
  u32 guard = 0u;
  while (m->nodes[ni].parent != ni && guard++ < m->n_nodes) {
    u32 p = m->nodes[ni].parent;
    u32 n = 0u;
    mnf_successors(s, m->nodes[p].term, 1u, 0u, s->n_rules, &n);
    for (u32 k = 0; k < n; k++) {
      if (kbo_eq(mnf_succ_buf[k], m->nodes[ni].term)) { (*replayed)++; break; }
    }
    (*len)++;
    ni = p;
  }
  return ni;
}

// Re-check the join captured by mnf_insert and report to stderr.
static void mnf_verify(AtpState *s, AtpMnf *m) {
  u32 lenA = 0u, repA = 0u;
  u32 rootA = mnf_verify_chain(s, m, m->meet_a, &lenA, &repA);
  // Side B's node was never created -- meet_term is a one-step rewrite of
  // meet_b_parent; verify that link, then walk meet_b_parent to its root.
  u32 nB = 0u, lenB = 1u, repB = 0u;
  mnf_successors(s, m->nodes[m->meet_b_parent].term, 1u, 0u, s->n_rules, &nB);
  for (u32 k = 0; k < nB; k++) {
    if (kbo_eq(mnf_succ_buf[k], m->meet_term)) { repB++; break; }
  }
  u32 rootB = mnf_verify_chain(s, m, m->meet_b_parent, &lenB, &repB);
  u8   colA      = m->nodes[m->meet_a].colour;
  Term grootA    = m->nodes[rootA].term;
  Term grootB    = m->nodes[rootB].term;
  // colA owns side A; meet_b_col (opposite) owns side B.
  int roots_ok = (colA == MNF_GREEN)
      ? (kbo_eq(grootA, m->seed_green) && kbo_eq(grootB, m->seed_red))
      : (kbo_eq(grootA, m->seed_red)   && kbo_eq(grootB, m->seed_green));
  u32 green_len = (colA == MNF_GREEN) ? lenA : lenB;
  u32 red_len   = (colA == MNF_GREEN) ? lenB : lenA;
  u32 green_rep = (colA == MNF_GREEN) ? repA : repB;
  u32 red_rep   = (colA == MNF_GREEN) ? repB : repA;
  fprintf(stderr,
      "[mnf] JOIN: meet has %u symbols; green-side chain %u step(s) "
      "(%u replay under final R), red-side chain %u step(s) (%u replay); "
      "chain roots == goal: %s\n",
      atp_symbol_count(m->meet_term),
      green_len, green_rep, red_len, red_rep,
      roots_ok ? "YES" : "NO -- BUG");
}
#endif /* ATP_MNF_DIAG */

// One MNF advance: (a) feed any rules completion derived since the last
// call to every already-expanded node; (b) expand up to `budget` queued
// (first-expansion) nodes against the full current R.  Returns 1 once
// the fronts have joined.
// Per-node guard for the MNF expansion loops.  Returns 1 when the loop
// must stop, for either of two reasons:
//
//  - Wall deadline.  A single mnf_step can re-expand a large node table
//    against many newly-derived rules without ever returning to
//    thvm_atp_step's per-step wall check, so a hard goal can run far
//    past MaxWallSeconds inside one mnf_step.  Poll the deadline here
//    (throttled to every 256th call, since clock_gettime per node over
//    a 400k-node table is pure overhead) and bail; goal_check returns
//    and the next step iteration reports ATP_TIMEOUT.
//  - Heap pressure.  Between node expansions every live MNF term is
//    parked in m->nodes (rooted by thvm_atp_gc_collect) and no
//    expand-local term is allocated yet, so a Cheney collection here is
//    safe.  If heap is still under pressure after a collection -- the
//    live working set alone exceeds the half-space -- the front must
//    stop growing or it would exhaust from-space and abort the process;
//    set m->full so the caller stops.  Completion keeps running and the
//    goal still closes by single-NF or a later collision.
static int mnf_loop_guard(AtpState *s, AtpMnf *m) {
  static u32 tick = 0u;
  if ((++tick & 0xFFu) == 0u) {
    if (s->wall_deadline_us != 0u) {
      u64 now = atp_now_us();
      if (now != 0u && now >= s->wall_deadline_us) return 1;
    }
    if (thvm_atp_abort_hook != NULL && thvm_atp_abort_hook()) return 1;
  }
  if (!atp_heap_under_pressure()) return 0;
  thvm_atp_gc_collect(s);
  if (atp_heap_under_pressure()) { m->full = 1u; return 1; }
  return 0;
}

static int mnf_step(AtpState *s, AtpMnf *m, u32 budget) {
  if (m->joined) return 1;
  // Refresh the per-rule caches (vars-contained flag + lhs/rhs node
  // counts) for the whole current rule set -- mnf_successors reads
  // them instead of recomputing at every node it expands this call.
  if (s->n_rules > g_mnf_vc_cap) {
    u32 cap = g_mnf_vc_cap ? g_mnf_vc_cap : 256u;
    while (cap < s->n_rules) cap *= 2u;
    u8  *nv = (u8  *)realloc(g_mnf_vc, cap);
    u32 *nl = (u32 *)realloc(g_mnf_ln, cap * sizeof(u32));
    u32 *nr = (u32 *)realloc(g_mnf_rn, cap * sizeof(u32));
    if (nv == NULL || nl == NULL || nr == NULL) {
      fprintf(stderr, "mnf_step: rule cache OOM\n"); exit(1);
    }
    g_mnf_vc = nv; g_mnf_ln = nl; g_mnf_rn = nr; g_mnf_vc_cap = cap;
  }
  for (u32 j = 0; j < s->n_rules; j++) {
    g_mnf_vc[j] = (u8)atp_vars_contained(s->lhs[j], s->rhs[j]);
    g_mnf_ln[j] = atp_symbol_count(s->lhs[j]);
    g_mnf_rn[j] = atp_symbol_count(s->rhs[j]);
  }
#ifdef ATP_MNF_DIAG
  { static u32 c = 0;
    if (c++ % 16u == 0u) {
      fprintf(stderr,
        "[mnf] call=%-6u nodes=%-7u red=%-7u green=%-7u "
        "queue(r=%u g=%u) anti=%u dup=%u trunc=%u full=%u rules=%u/%u\n",
        c, m->n_nodes, m->n_red, m->n_green,
        m->qred_tail - m->qred_head, m->qgreen_tail - m->qgreen_head,
        m->n_anti, m->n_dup, m->n_trunc, m->full,
        m->n_rules_seen, s->n_rules);
    } }
#endif
  if (s->n_rules > m->n_rules_seen) {
    u32 lo = m->n_rules_seen, hi = s->n_rules, upto = m->n_nodes;
    for (u32 ni = 0; ni < upto && !m->joined; ni++) {
      if (m->nodes[ni].expanded) {
        if (mnf_loop_guard(s, m)) break;
        mnf_expand_node(s, m, ni, lo, hi);
      }
    }
    m->n_rules_seen = hi;
  }
  // (b) expand the two fronts in alternation -- one node each per
  // round -- taking each colour's node by the irred-adaptive deque
  // policy (mnf_pop).  A node's `irred` is settled by mnf_expand_node;
  // it feeds the next pop of the same colour.
  for (u32 b = 0; b < budget && !m->joined; b++) {
    if (mnf_loop_guard(s, m)) break;
    int did = 0;
    if (m->qred_head < m->qred_tail) {
      u32 ni = mnf_pop(m->qred, &m->qred_head, &m->qred_tail,
                       m->red_last_irred);
      if (!m->nodes[ni].expanded) {
        mnf_expand_node(s, m, ni, 0u, s->n_rules);
        m->nodes[ni].expanded = 1u;
      }
      m->red_last_irred = m->nodes[ni].irred;
      did = 1;
    }
    if (!m->joined && m->qgreen_head < m->qgreen_tail) {
      u32 ni = mnf_pop(m->qgreen, &m->qgreen_head, &m->qgreen_tail,
                       m->green_last_irred);
      if (!m->nodes[ni].expanded) {
        mnf_expand_node(s, m, ni, 0u, s->n_rules);
        m->nodes[ni].expanded = 1u;
      }
      m->green_last_irred = m->nodes[ni].irred;
      did = 1;
    }
    if (!did) break;
  }
#ifdef ATP_MNF_DIAG
  if (m->joined) mnf_verify(s, m);
#endif
  return m->joined;
}

// === MNF proof extraction ============================================
//
// A join is goal_lhs ->* meet <-* goal_rhs: every MNF node is a one-step
// equational rewrite of its parent (forward l->r, or variable-safe
// backward r->l), so walking the two parent chains up from `meet` to the
// two seeds reconstructs a closed equational chain.  This is the
// goal-directed analog of atp_proof_record_side: it fills AtpProofStep[]
// with the same {side, rule, fwd, pos, before, after} shape the WL
// ProofObject builder already consumes, so the symmetric goal joins the
// same dataset machinery as a single-NF proof.
//
// GREEN side terms are emitted as side 0 (the goal_lhs chain), RED as
// side 1 (the goal_rhs chain); both chains run goal -> meet, so the
// assembled equation reaches `meet == meet` -- exactly the NF == NF
// tautology the single-NF path reaches, with `meet` as the common form.

// Find the one-step rewrite of `before` that yields `after` using the
// rule slice (rl[j] -> rr[j], j in [0,nr)): try every rule (forward
// l->r, and -- gated by vars-contained -- backward r->l) at every
// position, returning the redex path / rule / direction of the first
// match.  Mirrors mnf_successors' search but stops at the edge whose
// result is kbo_eq to `after`.  `out_rule` indexes into the slice.
// Returns 1 on success.
static int mnf_edge_step(Term before, Term after,
                         const Term *rl, const Term *rr, u32 nr,
                         u8 *pos, u8 depth,
                         u32 *out_rule, u8 *out_pos_len, u8 *out_fwd) {
  for (u32 j = 0; j < nr; j++) {
    RewriteSubst sub = {{0}};
    if (thvm_match(rl[j], before, &sub)) {                     // forward l->r
      Term repl = thvm_subst_apply(rr[j], &sub);
      if (kbo_eq(repl, after)) {
        *out_rule = j; *out_pos_len = depth; *out_fwd = 1u; return 1;
      }
    }
    if (atp_vars_contained(rl[j], rr[j])) {                    // backward r->l
      RewriteSubst sb = {{0}};
      if (thvm_match(rr[j], before, &sb)) {
        Term repl = thvm_subst_apply(rl[j], &sb);
        if (kbo_eq(repl, after)) {
          *out_rule = j; *out_pos_len = depth; *out_fwd = 0u; return 1;
        }
      }
    }
  }
  // Descend: the rewrite was below the top.  `before` and `after` agree
  // everywhere except the one child holding the redex.
  if (term_tag(before) == TAG_CTR && term_tag(after) == TAG_CTR &&
      term_ext(before) == term_ext(after) &&
      term_ctr_n(before) == term_ctr_n(after) &&
      depth < ATP_PROOF_MAX_DEPTH) {
    u32 nc = term_ctr_n(before);
    for (u32 i = 0; i < nc; i++) {
      Term cb = term_ctr_at(before, i);
      Term ca = term_ctr_at(after, i);
      if (kbo_eq(cb, ca)) continue;     // unchanged child: not the redex
      pos[depth] = (u8)i;
      if (mnf_edge_step(cb, ca, rl, rr, nr, pos, (u8)(depth + 1u),
                        out_rule, out_pos_len, out_fwd)) {
        return 1;
      }
    }
  }
  return 0;
}

// Build the HISTORICAL rule set: every equation that ever entered R,
// not just the rules surviving in s->lhs/s->rhs after interreduction.
// An MNF path step taken with a rule interreduction later retired only
// replays against this superset.  Sources: the live rules plus every
// TRACE_ORIENT / TRACE_AXIOM / TRACE_SIMPLIFY trace entry's (lhs, rhs).
// Caller-owned out_l / out_r arrays of capacity `cap`; returns count.
// Build the historical rule set (live rules + every ORIENT/AXIOM/
// SIMPLIFY equation that ever entered R via the trace).  out_trace[i]
// records the TRACE index that resolves slot i, so the WL extractor can
// resolve an MNF step's cited rule through resolveTrace exactly as it
// resolves a completion MainStep: a live derived rule lands on its
// critical-pair lemma, a retired rule on its own trace node.
static u32 mnf_historical_rules(AtpState *s, Term *out_l, Term *out_r,
                                u32 *out_trace, u32 cap) {
  u32 n = 0u;
  for (u32 i = 0; i < s->n_rules && n < cap; i++) {
    out_l[n] = s->lhs[i]; out_r[n] = s->rhs[i];
    out_trace[n] = s->r_trace[i];
    n++;
  }
  for (u32 i = 0; i < s->n_trace && n < cap; i++) {
    Term e = s->trace[i];
    u32  reason = term_ext(e);
    if (reason == TRACE_ORIENT || reason == TRACE_AXIOM ||
        reason == TRACE_SIMPLIFY) {
      out_l[n] = term_ctr_at(e, 2);
      out_r[n] = term_ctr_at(e, 3);
      out_trace[n] = i;
      n++;
    }
  }
  return n;
}

// Walk node `ni` up to its seed, collecting the chain of terms
// root..ni into buf[0..*len) (root first).  Returns the root index.
static u32 mnf_collect_lineage(AtpMnf *m, u32 ni, u32 *buf, u32 *len,
                               u32 cap) {
  u32 stack[ATP_PROOF_MAX_STEPS + 2u];
  u32 sp = 0u;
  u32 guard = 0u;
  while (sp < cap && sp < (ATP_PROOF_MAX_STEPS + 2u) &&
         guard++ < m->n_nodes + 1u) {
    stack[sp++] = ni;
    if (m->nodes[ni].parent == ni) break;
    ni = m->nodes[ni].parent;
  }
  // stack holds ni..root (leaf first); reverse into buf (root first).
  for (u32 i = 0; i < sp; i++) buf[i] = stack[sp - 1u - i];
  *len = sp;
  return ni;
}

// Emit the steps along a lineage (terms[0..n_terms), seed/root first)
// into out[], tagged with `side`.  Each consecutive pair is one
// rewrite, reconstructed by mnf_edge_step against the (rl, rr, nr)
// rule slice.  Both fronts are emitted seed -> meet, so the GREEN and
// RED chains meet at the join term.  Returns 1 if every edge replayed.
static int mnf_emit_lineage(AtpMnf *m, const u32 *terms,
                            u32 n_terms, u32 side,
                            const Term *rl, const Term *rr,
                            const u32 *rtr, u32 nr,
                            AtpProofStep *out, u32 cap, u32 *n) {
  for (u32 k = 0; k + 1u < n_terms; k++) {
    Term before = m->nodes[terms[k]].term;
    Term after  = m->nodes[terms[k + 1u]].term;
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u32 rule = 0u; u8 pos_len = 0u, fwd = 1u;
    if (!mnf_edge_step(before, after, rl, rr, nr,
                       pos, 0u, &rule, &pos_len, &fwd)) {
      return 0;   // an edge replays against no historical rule
    }
    if (*n >= cap) return 0;
    AtpProofStep *st = &out[*n];
    st->side    = side;
    // rtr[rule] is the TRACE index of the cited historical rule, so WL
    // resolves it via resolveTrace just like a completion MainStep.
    st->rule    = rtr[rule];
    st->fwd     = fwd;
    st->pos_len = pos_len;
    for (u8 p = 0; p < pos_len; p++) st->pos[p] = pos[p];
    st->before  = before;
    st->after   = after;
    (*n)++;
  }
  return 1;
}

// Reconstruct the MNF join as a two-sided AtpProofStep chain.  Side A is
// the existing table node meet_a; side B is the fresh reduct (meet_term)
// of meet_b_parent that collided with it.  Both A and B equal meet_term,
// so each chain runs seed -> meet.  GREEN -> side 0, RED -> side 1.
// Returns the step count, 0 if no join / not reconstructable.
fn u32 thvm_atp_mnf_proof_extract(AtpState *s, AtpProofStep *out, u32 cap) {
  if (s == NULL || out == NULL || cap == 0u) return 0;
  AtpMnf *m = s->mnf;
  if (m == NULL || !m->joined) return 0;

  // Lineage A: meet_a up to its seed.
  static u32 lin_a[ATP_PROOF_MAX_STEPS + 2u];
  static u32 lin_b[ATP_PROOF_MAX_STEPS + 2u];
  u32 len_a = 0u, len_b = 0u;
  mnf_collect_lineage(m, m->meet_a, lin_a, &len_a, ATP_PROOF_MAX_STEPS + 2u);

  // Lineage B: the colliding reduct was never created as a node.  Walk
  // its PARENT to the seed, then append a synthetic final node holding
  // meet_term.  We reuse meet_b_parent's node slot terms; the final
  // edge (meet_b_parent.term -> meet_term) is emitted explicitly.
  mnf_collect_lineage(m, m->meet_b_parent, lin_b, &len_b,
                      ATP_PROOF_MAX_STEPS + 1u);

  u8 col_a = m->nodes[m->meet_a].colour;     // colour that owns lineage A
  // meet_b_col is the opposite colour (the fresh reduct's colour).
  u32 side_a = (col_a == MNF_GREEN) ? 0u : 1u;
  u32 side_b = (m->meet_b_col == MNF_GREEN) ? 0u : 1u;

  // Reconstruct each edge against the HISTORICAL rule set (live rules +
  // every equation that ever entered R via the trace DAG), so an edge
  // taken with a since-retired rule still replays.
  u32 hist_cap = s->n_rules + s->n_trace;
  Term *hist_l = (Term *)malloc((size_t)hist_cap * sizeof(Term));
  Term *hist_r = (Term *)malloc((size_t)hist_cap * sizeof(Term));
  u32  *hist_t = (u32  *)malloc((size_t)hist_cap * sizeof(u32));
  if (hist_l == NULL || hist_r == NULL || hist_t == NULL) {
    free(hist_l); free(hist_r); free(hist_t); return 0; }
  u32 nr = mnf_historical_rules(s, hist_l, hist_r, hist_t, hist_cap);

  u32 n = 0u;
#ifdef ATP_MNF_DIAG
  fprintf(stderr, "[mnf-extract] meet_a=%u len_a=%u meet_b_parent=%u "
          "len_b=%u col_a=%u meet_b_col=%u hist_rules=%u\n",
          m->meet_a, len_a, m->meet_b_parent, len_b, col_a, m->meet_b_col,
          nr);
#endif
  // An edge that replays against no historical rule leaves the lineage
  // not reconstructable as a forward/backward chain.  Return 0 (no
  // extractable proof) rather than emit a partial, unsound chain.
  if (!mnf_emit_lineage(m, lin_a, len_a, side_a, hist_l, hist_r, hist_t, nr,
                        out, cap, &n)) {
#ifdef ATP_MNF_DIAG
    fprintf(stderr, "[mnf-extract] lineage A unreplayable at step %u/%u\n",
            n, len_a > 0u ? len_a - 1u : 0u);
#endif
    free(hist_l); free(hist_r); free(hist_t); return 0;
  }
  if (!mnf_emit_lineage(m, lin_b, len_b, side_b, hist_l, hist_r, hist_t, nr,
                        out, cap, &n)) {
#ifdef ATP_MNF_DIAG
    fprintf(stderr, "[mnf-extract] lineage B unreplayable at step %u\n", n);
#endif
    free(hist_l); free(hist_r); free(hist_t); return 0;
  }
  // Final B edge: meet_b_parent.term -> meet_term (the collision).
  {
    Term before = m->nodes[m->meet_b_parent].term;
    Term after  = m->meet_term;
    if (!kbo_eq(before, after)) {
      u8  pos[ATP_PROOF_MAX_DEPTH];
      u32 rule = 0u; u8 pos_len = 0u, fwd = 1u;
      if (!mnf_edge_step(before, after, hist_l, hist_r, nr,
                         pos, 0u, &rule, &pos_len, &fwd)) {
        free(hist_l); free(hist_r); free(hist_t); return 0;
      }
      if (n >= cap) return 0;
      AtpProofStep *st = &out[n];
      st->side    = side_b;
      st->rule    = hist_t[rule];   // TRACE index, see mnf_emit_lineage
      st->fwd     = fwd;
      st->pos_len = pos_len;
      for (u8 p = 0; p < pos_len; p++) st->pos[p] = pos[p];
      st->before  = before;
      st->after   = after;
      n++;
    }
  }
  free(hist_l); free(hist_r); free(hist_t);
  return n;
}
#endif /* ATP_MNF */

fn AtpStatus thvm_atp_goal_check(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0) return ATP_RUNNING;
  // Goal-side normalization cap.  Generous (not the per-step CP cap
  // of 64): a goal can close purely by normalization when the axioms
  // generate no critical pairs -- e.g. combinatory-logic identities
  // (B/C/W <-> S/K), whose rules are non-overlapping, so completion's
  // CP queue is empty and the ONLY way the goal joins is by reducing
  // both sides to a common normal form.  Deep combinator terms like
  // S(S(K(S(KS)K))S)(KK) x y z take many hundreds of rewrites to
  // normalize; a cap of 64 left them un-joined (reported QUEUE_EMPTY
  // with the goal actually provable).  A terminating rewrite system
  // reaches fixpoint and returns early regardless of the bound, so
  // the only risk is a non-terminating R (e.g. the Y axiom in scope),
  // which the wall deadline already bounds.
  const u32 NORM_CAP = 1u << 16;

  // 8.9c: existential goals use narrowing; universal goals stay
  // on the rewrite-and-compare path.
  if (s->goal_existential) {
    Term lhs = s->goal_lhs;
    Term rhs = s->goal_rhs;
    if (kbo_eq(lhs, rhs)) return ATP_PROVED;
    for (u32 i = 0; i < ATP_NARROW_BUDGET; i++) {
      Term new_lhs = 0, new_rhs = 0;
      u8 ok = thvm_atp_narrow_step(s, lhs, rhs,
                                   &new_lhs, &new_rhs,
                                   &s->witness_subst);
      if (!ok) return ATP_RUNNING;   // no more narrows; wait for new R
      lhs = new_lhs;
      rhs = new_rhs;
      if (kbo_eq(lhs, rhs)) {
        // Capture the converged terms back into the goal slots
        // so successive goal_check calls (after full saturation)
        // still report PROVED.
        s->goal_lhs = lhs;
        s->goal_rhs = rhs;
        return ATP_PROVED;
      }
    }
    // Budget exhausted; let the outer loop add more rules and retry.
    s->goal_lhs = lhs;
    s->goal_rhs = rhs;
    return ATP_RUNNING;
  }

  // Single-normal-form check: sound and cheap.  If both goal sides
  // rewrite to the same normal form under R the goal is proved.  This
  // catches every goal whose two sides share a normal form (e.g. thm,
  // whose deep lhs reduces to its rhs).
  Term l = atp_rewrite_normalize(s, s->goal_lhs, s->lhs, s->rhs,
                                 s->n_rules, NORM_CAP);
  Term r = atp_rewrite_normalize(s, s->goal_rhs, s->lhs, s->rhs,
                                 s->n_rules, NORM_CAP);
  // Cache the live (normalized) goal for the CPinGoal heuristic.
  s->goal_lhs_nf = l;
  s->goal_rhs_nf = r;
  if (kbo_eq(l, r)) return ATP_PROVED;
#ifdef ATP_MNF
  // Milestone 10: the MNF bidirectional search AUGMENTS the single-NF
  // check -- it does not replace it.  goal_lhs seeds a GREEN front,
  // goal_rhs a RED one; each rewrites with R; an opposite-colour
  // collision is the join.  This is the only detector that can prove a
  // goal whose two sides never share one normal form -- a symmetric
  // goal like commutativity nand(x,y)=nand(y,x), where neither side
  // rewrites to the other, so the single-NF check above structurally
  // cannot fire.  (An earlier revision had MNF REPLACE the single-NF
  // check; that regressed thm, which MNF's front search did not close
  // but the single-NF check does.  Both are sound -- run both.)
  //
  // Gated on s->use_mnf so the dylib can be COMPILED with -DATP_MNF
  // (MNF linked in) yet stay completion-only by default: the front
  // search runs only when the WL surface flips the flag for
  // Method -> "GoalDirected".  Off, this block is a single branch test.
  if (s->use_mnf) {
    if (s->mnf == NULL) s->mnf = mnf_create(s);
    if (s->mnf != NULL && mnf_step(s, s->mnf, MNF_BUDGET)) return ATP_PROVED;
  }
#endif
  return ATP_RUNNING;
}

// 8.9c: set an existential conjecture.  Mirrors thvm_atp_set_goal
// but flips s->goal_existential so the narrow path runs in
// goal_check.
fn u8 thvm_atp_set_goal_existential(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  if (lhs == 0) {
    s->goal_lhs = 0;
    s->goal_rhs = 0;
    s->goal_existential = 0;
    return 1;
  }
  if (s->spec != NULL) {
    u32 sl = wald_term_sort(s->spec, lhs);
    u32 sr = wald_term_sort(s->spec, rhs);
    if (sl == WALD_MAX_SORTS || sr == WALD_MAX_SORTS || sl != sr) {
      return 0;
    }
  }
  s->goal_lhs = lhs;
  s->goal_rhs = rhs;
  s->goal_existential = 1;
  return 1;
}

// Walk the older rules (indices [0, added.first)) and drop any
// whose LHS reduces under the freshly-added rule(s).  Each dropped
// rule's simplified equation goes back onto the CP queue so the
// saturation loop has a chance to re-orient it under the smaller R.
//
// Today this uses the top-position-only `thvm_rewrite_normalize`;
// stage 5.4's recursive-descent rewriter will automatically widen
// the coverage to sub-positions without changing this function.
//
// Returns the number of older rules that were dropped.
#ifdef ATP_ORPHAN_KILL
// 9b: orphan deletion (Waldmeister's "Waisenmord", orphan murder).
// When interreduce drops a rule, the queued CPs descended from it are
// redundant -- the re-queued reduced equation regenerates whatever
// they would contribute.  `dead` holds the trace ids of the dropped
// rules; a queued CP is an orphan iff its TRACE_CP entry names a dead
// rule as a parent.  Survivors are compacted down and the queue heap
// / FV index / cp_graph rebuilt via thvm_atp_cp_reheapify.
static void atp_cp_kill_orphans(AtpState *s, const u32 *dead, u32 n_dead) {
  if (s == NULL || n_dead == 0 || s->n_cps == 0) return;
  u32 w = 0;
  for (u32 i = 0; i < s->n_cps; i++) {
    int orphan = 0;
    u32 ti = s->cp_trace[i];
    if (ti != ATP_TRACE_NONE && ti < s->n_trace) {
      Term te = s->trace[ti];
      if (term_tag(te) == TAG_CTR && term_ext(te) == TRACE_CP) {
        u32 pa = (u32)term_val(term_ctr_at(te, 0));
        u32 pb = (u32)term_val(term_ctr_at(te, 1));
        for (u32 d = 0; d < n_dead; d++) {
          if (pa == dead[d] || pb == dead[d]) { orphan = 1; break; }
        }
      }
    }
    if (orphan) {
      // Drop the orphan -- free its byte string (the queue owns it).
      free(s->cp_packed[i]);
      s->cp_packed[i] = NULL;
      continue;
    }
    if (w != i) {
      s->cp_packed[w] = s->cp_packed[i];
      s->cp_packed[i] = NULL;
      s->cp_trace[w]  = s->cp_trace[i];
      s->cp_pri[w]    = s->cp_pri[i];
      s->cp_seq[w]    = s->cp_seq[i];
      s->cp_goal[w]   = s->cp_goal[i];
    }
    w++;
  }
  if (w != s->n_cps) {
    s->n_cps = w;
    thvm_atp_cp_reheapify(s);   // rebuilds heap + FV index + cp_graph
  }
}
#endif /* ATP_ORPHAN_KILL */

// Periodic critical-pair-set interreduction against the FULL rule set --
// a port of Waldmeister's KPV_KPMengeInterreduzieren (KPVerwaltung.c:1032)
// and its per-CP AP_generic callback (KPVerwaltung.c, the doR/doE branch).
// The per-step atp_normalize_graph sweep only applies the 1-2 just-added
// rules; a queued CP that became joinable through an OLDER rule (e.g. an
// interreduce cascade) stays on the heap until it is finally popped and
// dies in thvm_atp_step's pop-time normalize.  Until then it pollutes the
// heap-min selection -- the engine keeps picking light CPs that normalize
// to nothing while the heavier proof-relevant overlaps wait.  Waldmeister
// purges those dead CPs from the queue eagerly, so its heap-min always
// reflects a live, irreducible CP.  This pass reproduces that: walk the
// whole queue, normalize each CP against the full current rule set, DELETE
// the joinable ones, repack the reduced ones, then reheapify (which
// recomputes every priority -- the AP_generic C_ReClassify reweight).
// Default OFF; the engine is byte-identical unless cp_set_interreduce is
// set (Method->"Waldmeister").
static void atp_cp_set_interreduce(AtpState *s) {
  if (s == NULL || s->n_cps == 0u || s->n_rules == 0u) return;
  const u32 NORM_CAP = 64u;
  u32 w = 0u;
  int touched = 0;
  for (u32 i = 0; i < s->n_cps; i++) {
    // Per-CP heap checkpoint: the normalize allocates scratch cells; the
    // reduced terms are copied out by acp_pack, so the scratch is dead
    // after the (re)pack.  Reset each iteration so a long queue cannot
    // exhaust the dyn heap.
    u64 hcp = thvm_atp_heap_checkpoint();
    Term ol = 0, orr = 0;
    acp_unpack(s->cp_packed[i], &ol, &orr);
    Term l = atp_rewrite_normalize(s, ol,  s->lhs, s->rhs, s->n_rules, NORM_CAP);
    Term r = atp_rewrite_normalize(s, orr, s->lhs, s->rhs, s->n_rules, NORM_CAP);
    if (kbo_eq(l, r)) {
      // Joinable under R -- the CP adds no equational consequence.  Drop
      // it (WM AP_generic returns WTI_Delete).
      s->n_cps_dropped_joinable++;
      s->n_cp_set_ir_deleted++;
      free(s->cp_packed[i]);
      s->cp_packed[i] = NULL;
      touched = 1;
      thvm_atp_heap_reset(hcp);
      continue;
    }
    if (l != ol || r != orr) {
      free(s->cp_packed[i]);
      s->cp_packed[i] = NULL;
      s->cp_packed[w] = acp_pack(l, r, NULL, NULL);
      s->n_cp_set_ir_reweighted++;
      touched = 1;
    } else if (w != i) {
      s->cp_packed[w] = s->cp_packed[i];
      s->cp_packed[i] = NULL;
    }
    s->cp_trace[w] = s->cp_trace[i];
    w++;
    thvm_atp_heap_reset(hcp);
  }
  s->n_cps = w;
  if (touched) {
    s->n_cp_set_ir_passes++;
    thvm_atp_cp_reheapify(s);   // recompute every cp_pri + rebuild index
  }
}

fn u32 thvm_atp_interreduce(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0 || added.first == 0) return 0;
#ifdef ATP_ORPHAN_KILL
  // 9b: collect the trace ids of rules dropped below, then kill the
  // queued CPs descended from them once the compaction loop is done.
  u32 *atp_dead   = (u32 *)malloc(added.first * sizeof(u32));
  u32  atp_n_dead = 0;
#endif

  // Copy the new rules' Terms by value so we can safely compact the
  // R array beneath them.  Term is 64-bit; the heap cells they point
  // to don't move.
  Term new_lhs[2];
  Term new_rhs[2];
  u32  new_traces[2];
  u32  n_new = added.count;
  if (n_new > 2) n_new = 2;
  for (u32 k = 0; k < n_new; k++) {
    new_lhs[k] = s->lhs[added.first + k];
    new_rhs[k] = s->rhs[added.first + k];
    new_traces[k] = s->r_trace[added.first + k];
  }

  u32 dropped = 0;
  u32 i       = 0;
  while (i < added.first - dropped) {
    Term old_lhs = s->lhs[i];
    Term old_rhs = s->rhs[i];
    Term reduced;
    // When NORM_STEP recording is on (WL chain extraction needs it),
    // walk the LHS normalization step-by-step and push a TRACE_NORM_
    // STEP per fire chained from the dropped rule's trace.  The
    // TRACE_SIMPLIFY then parents on the chain tail, so its WL info
    // inherits from the last NORM_STEP whose recorded {lhs, rhs} is
    // exactly the simplified equation -- the cplEqSetQ check at the
    // ORIENT/SIMPLIFY resolveTrace branch passes directly and no
    // emitNorm BFS is needed to bridge the simplification gap.
    u32 simplify_parent = s->r_trace[i];
    if (s->record_norm_steps) {
      reduced = atp_rewrite_normalize_slice_record(
          s, old_lhs, new_lhs, new_rhs, new_traces, n_new, 16,
          &simplify_parent, 0u, old_rhs);
    } else {
      reduced = atp_rewrite_normalize(s, old_lhs, new_lhs, new_rhs,
                                      n_new, 16);
    }
    if (!kbo_eq(reduced, old_lhs)) {
      // The older rule's LHS simplified -- drop it and requeue
      // (reduced, old_rhs) for re-orientation.  Record the re-queue
      // as a TRACE_SIMPLIFY entry parented on the dropped rule's
      // trace index (or the NORM_STEP chain tail when recording is
      // on) so the proof DAG stays connected through interreduction
      // (a fresh TRACE_AXIOM would sever it).
      atp_add_equation_simplified(s, reduced, old_rhs, simplify_parent);
      if (atp_rule_trace_on()) {
        char la[2048];
        atp_pretty_term(s->lhs[i], la, sizeof la);
        fprintf(stderr, "  RETIRE rule (slot %u): LHS %s collapsed; "
                "re-queued for re-orientation\n", i, la);
      }
#ifdef ATP_ORPHAN_KILL
      // Capture the dropped rule's trace id before the shift below
      // overwrites r_trace[i].  Its descendant CPs are now orphans.
      if (atp_dead != NULL && s->r_trace[i] != ATP_TRACE_NONE) {
        atp_dead[atp_n_dead++] = s->r_trace[i];
      }
#endif
      // Keep the unorientable-rule count live: the dropped rule leaves
      // R here (it re-enters as a queued equation, re-counted only if
      // re-oriented unorientable at its next atp_push_rule).
      if (!s->r_orient[i]) s->n_unorient--;
      for (u32 j = i + 1; j < s->n_rules; j++) {
        s->lhs[j - 1]      = s->lhs[j];
        s->rhs[j - 1]      = s->rhs[j];
        s->r_trace[j - 1]  = s->r_trace[j];
        s->r_orient[j - 1] = s->r_orient[j];
      }
      s->n_rules--;
#ifdef ATP_RULE_INDEX
      // 7e lever 2: a rule was dropped and the array compacted -- the
      // rule-LHS index's index->LHS mapping is stale even if a later
      // re-add restores n_rules.  Force a rebuild on the next query.
      s->rule_index_dirty = 1u;
#endif
      dropped++;
      // Don't increment i; the next older rule shifted down to slot i.
    } else {
      // === RIGHT-REDUCTION (composition) ============================
      // The older rule i's LHS did NOT collapse, so the rule stays in
      // R as `l -> old_rhs`.  Now try to reduce its RHS against the
      // just-added rules: rewrite old_rhs to its normal form r'.  If it
      // changes (and l > r' still holds for the reduction order), update
      // s->rhs[i] in place -- l = r' is still an equational consequence
      // (l = old_rhs ->* r'), r' is no larger than old_rhs, and the CPs
      // born from rule i now use the smaller RHS.  This is the
      // DISCOUNT-loop right-reduction / composition step; without it the
      // RHSs (and every CP overlapping them) bloat across the run.
      if (s->right_reduce) {
        Term r_reduced;
        // Thread the proof DAG: record each RHS rewrite as a NORM_STEP
        // chained off rule i's own TRACE_ORIENT (parent), side = 1
        // (the RHS), with the LHS as the unchanged other side.  The
        // chain tail then carries the equation {l, r'} exactly, so a
        // new TRACE_ORIENT parented on it inherits directly under chain
        // extraction, and emitNorm bridges it under chain-off.
        u32 rr_parent = s->r_trace[i];
        if (s->record_norm_steps) {
          r_reduced = atp_rewrite_normalize_slice_record(
              s, old_rhs, new_lhs, new_rhs, new_traces, n_new, 16,
              &rr_parent, 1u, old_lhs);
        } else {
          r_reduced = atp_rewrite_normalize(s, old_rhs, new_lhs, new_rhs,
                                            n_new, 16);
        }
        if (!kbo_eq(r_reduced, old_rhs)) {
          // Orientation guard: l -> r' must remain a valid reduction
          // rule (l strictly greater than r').  r' is a reduct of
          // old_rhs <= l, so this holds in the standard case; verify it
          // and skip the in-place update if some pathological order
          // makes l NOT > r' (keep the rule as `l -> old_rhs`).
          // Size guard: a reduction order (KBO/LPO) guarantees r' is
          // smaller than old_rhs in the ORDER but not necessarily in
          // raw symbol count -- a rule a -> g(b,c) rewrites a constant
          // into a deeper term.  Since the point of right-reduction is
          // to keep RHSs (and the CPs overlapping them) SMALL, skip the
          // in-place update when r' has more symbols than old_rhs; the
          // rule keeps its compact RHS and the larger form never feeds
          // a critical pair.  (Soundness is unaffected either way.)
          if (atp_compare(s, old_lhs, r_reduced) == KBO_GT &&
              atp_symbol_count(r_reduced) <= atp_symbol_count(old_rhs)) {
            // Record the post-reduction rule as a fresh TRACE_ORIENT
            // parented on the NORM_STEP chain tail (or directly on the
            // old ORIENT when norm-step recording is off).  Repointing
            // r_trace[i] keeps resolveRule(i) -> the entry whose stored
            // (lhs, rhs) equals the live (l, r') pair.
            u32 new_t = atp_trace_push(s, TRACE_ORIENT, rr_parent,
                                       ATP_TRACE_NONE, old_lhs, r_reduced);
            // Retire the old `l -> old_rhs` ORIENT for the chain-off
            // aliveRulesAt model: a TRACE_SIMPLIFY whose ParentA names
            // the old trace marks it inactive from this point forward,
            // so emitNorm replays against `l -> r'`.  Push the marker
            // straight onto the trace (NOT atp_add_equation_simplified,
            // which would also enqueue a redundant CP -- the rule is
            // already live in R as l -> r').
            atp_trace_push(s, TRACE_SIMPLIFY, s->r_trace[i],
                           ATP_TRACE_NONE, old_lhs, r_reduced);
            s->rhs[i]     = r_reduced;
            s->r_trace[i] = new_t;
            s->n_right_reduced++;
            // RHS does not feed the LHS discrimination tree, so the
            // rule-LHS index stays valid; no dirty flag needed.
          }
        }
      }
      i++;
    }
  }

  // === Waldmeister IR_InterreduktionRechts (RMRechtsInterred,
  // Interreduktion.c:329): normalize the RIGHT-HAND side of every
  // surviving older rule against the freshly-added rules.  A rule whose
  // RHS reduces is dropped and re-queued as the simplified equation
  // (old_lhs, reduced_rhs) so the same TRACE_SIMPLIFY connected-DAG path
  // the LHS collapse uses justifies the RHS edit.  This keeps R reduced
  // -- the prior LHS-only interreduction left stale (non-normal) rule
  // RHSs in R, so the system never became canonical and the CP set
  // exploded (the Sheffer/Wolfram divergence).  Runtime-gated: the
  // default engine (use_rhs_interreduce == 0) skips this entirely.
  if (s->use_rhs_interreduce) {
    u32 j = 0;
    while (j < added.first - dropped) {
      Term old_lhs = s->lhs[j];
      Term old_rhs = s->rhs[j];
      // Normalize the RHS against only the new rule(s) -- the rest of R
      // already had its chance to reduce this RHS when those rules were
      // added.  (Matches Waldmeister: a new object reduces existing
      // rules' RHSs; full re-normalization is unnecessary.)
      Term reduced = atp_rewrite_normalize(s, old_rhs, new_lhs, new_rhs,
                                           n_new, 16);
      if (!kbo_eq(reduced, old_rhs)) {
        u32 simplify_parent = s->r_trace[j];
        // Drop the rule with the stale RHS and re-queue the simplified
        // equation; orient will re-admit it (its RHS now in normal form)
        // -- or join it away if it became trivial.
        atp_add_equation_simplified(s, old_lhs, reduced, simplify_parent);
#ifdef ATP_ORPHAN_KILL
        if (atp_dead != NULL && s->r_trace[j] != ATP_TRACE_NONE) {
          atp_dead[atp_n_dead++] = s->r_trace[j];
        }
#endif
        if (!s->r_orient[j]) s->n_unorient--;
        for (u32 k = j + 1; k < s->n_rules; k++) {
          s->lhs[k - 1]      = s->lhs[k];
          s->rhs[k - 1]      = s->rhs[k];
          s->r_trace[k - 1]  = s->r_trace[k];
          s->r_orient[k - 1] = s->r_orient[k];
        }
        s->n_rules--;
#ifdef ATP_RULE_INDEX
        s->rule_index_dirty = 1u;
#endif
        dropped++;
        // The next older rule shifted down to slot j; don't advance.
      } else {
        j++;
      }
    }
  }

#ifdef ATP_ORPHAN_KILL
  if (atp_dead != NULL) {
    if (atp_n_dead > 0) atp_cp_kill_orphans(s, atp_dead, atp_n_dead);
    free(atp_dead);
  }
#endif
  return dropped;
}

// Generate fresh CPs from the freshly-added rules `added` against
// the current rule set R, push survivors onto the CP queue.
// Drops overflow silently (queue cap or temp-buffer cap).  Returns
// the number of CPs successfully pushed.
//
// To avoid recomputing CPs already in the queue, the enumeration
// is restricted to (new x all_R) + (old x new), where the old x
// old slice is exactly the work we already did before the add.
//
// Temp buffer sized for one batch; large rule sets may produce
// more CPs than fit and silently drop them (matches Waldmeister's
// drop-on-overflow policy in `KPVerwaltung.c` -- *Kritische-Paare-
// Verwaltung*, "critical-pair management").

#define ATP_CP_BATCH 1024

// Stage 7.1: trivial-joinability check AND critical-pair reduction.
// Normalize both sides of a candidate CP under the current rule set R
// and write the reduced pair back through `*lhs`/`*rhs`: standard
// completion queues the NORMALIZED CP, not the raw overlap.  The
// un-normalized overlap of two deep rules blows past thousands of
// nodes -- dragging every later KBO compare / match / index descent
// and overrunning the retrieval flatten cap into a full O(n) scan.
// Returns 1 iff the two normal forms collapse to one term: the CP is
// joinable-by-R, adds no new consequence, and the caller discards it.
//
// This is the Waldmeister `Grundzusammenfuehrung` ("ground-merging")
// criterion at its weakest, equivalent to Twee's "joinable-by-current-
// R" pruning.  Stronger variants (ground-joinability over a sample
// of substitutions, AC-aware joinability) are deferred to 7.2+.
//
// Cost: two `thvm_rewrite_normalize` calls per CP candidate.  Worth
// it when the saturation produces many joinable CPs (group axioms
// generate ~hundreds of trivially-joinable overlaps per round).
static u8 atp_cp_trivially_joinable(AtpState *s, Term *lhs, Term *rhs) {
  const u32 NORM_CAP = 64;
  Term l = atp_rewrite_normalize(s, *lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  Term r = atp_rewrite_normalize(s, *rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  *lhs = l;
  *rhs = r;
  return kbo_eq(l, r);
}

// Stage 7.2b: source-rule-disjoint connectedness check.  Returns 1
// if (lhs, rhs) is joinable under R \ {rule_a, rule_b} -- the two
// rules that birthed this CP via overlap unification.  Bachmair-
// Dershowitz-Plaisted-style redundancy: if the join can be done
// without using either parent rule, the parent rules' interaction
// was redundant.
//
// Per the domination lemma in `docs/plans/connectedness_design.md`,
// this is strictly weaker pruning than 7.1's full-R joinability
// (since reducing the rule set cannot uncover joins that the full
// set misses).  We compute it for measurement: the resulting
// counter `n_cps_dropped_connected` is bounded above by
// `n_cps_dropped_joinable`, and the gap will become meaningful
// when AC matching or extended joinability lands in 7.4+.
//
// `rule_a`/`rule_b` are indices into `s->lhs[] / s->rhs[]`.  Pass
// ATP_RULE_NONE (or any value >= n_rules) to mean "no rule
// excluded" -- equivalent to running 7.1.  The filtered rule
// arrays are heap-allocated, sized to the live n_rules, since the
// rule set is unbounded.
static u8 atp_cp_source_disjoint_connected(AtpState *s, Term lhs, Term rhs,
                                           u32 rule_a, u32 rule_b) {
  const u32 NORM_CAP = 64;
  u32 n = s->n_rules;
  Term *filt_l = (n > 0) ? (Term *)malloc((size_t)n * sizeof(Term)) : NULL;
  Term *filt_r = (n > 0) ? (Term *)malloc((size_t)n * sizeof(Term)) : NULL;
  if (n > 0 && (filt_l == NULL || filt_r == NULL)) {
    free(filt_l); free(filt_r);
    return 0;
  }
  u32 n_filt = 0;
  for (u32 k = 0; k < n; k++) {
    if (k == rule_a || k == rule_b) continue;
    filt_l[n_filt] = s->lhs[k];
    filt_r[n_filt] = s->rhs[k];
    n_filt++;
  }
  Term l = atp_rewrite_normalize(s, lhs, filt_l, filt_r, n_filt, NORM_CAP);
  Term r = atp_rewrite_normalize(s, rhs, filt_l, filt_r, n_filt, NORM_CAP);
  free(filt_l);
  free(filt_r);
  return kbo_eq(l, r);
}

// Bachmair-Dershowitz connectedness (Twee section 6.2): a critical
// pair (s, t) born from the peak `p = sigma(li)` is REDUNDANT if s and
// t can be joined through a rewrite proof in which every term is
// STRICTLY BELOW the peak in the reduction order.  Soundness: such a
// proof witnesses that the local confluence of `p` follows from
// strictly smaller overlaps already (or to be) handled, so dropping
// the CP cannot lose a needed consequence -- this is the standard
// connectedness redundancy of unfailing completion.
//
// This is STRONGER than trivial-joinability (7.1): plain normalization
// only follows order-DECREASING steps to a normal form, so two sides
// with distinct normal forms are kept; connectedness additionally
// admits an unorientable equation applied in its non-reducing local
// direction, PROVIDED the result stays strictly below the peak.  Such a
// "detour below the peak" joins CPs that 7.1 misses, which is where the
// extra pruning comes from.
//
// Implementation: a bounded forward reachability below `peak`.  Seed a
// small hash set with s and t's ordered normal forms (always < peak
// since they are proper reducts).  Repeatedly expand a frontier term by
// every one-step rewrite -- both rule directions, variable-safe -- whose
// RESULT u satisfies peak >_C u; insert u, tagged by which seed it
// descends from (s-side / t-side / both).  A term reached from BOTH
// sides is the join point -> redundant.  Bounded by ATP_CONN_MAX_NODES
// (return 0 = KEEP on overflow, so the criterion never wrongly deletes).
#define ATP_CONN_MAX_NODES 256u
#define ATP_CONN_SIDE_S 1u
#define ATP_CONN_SIDE_T 2u

// Structural hash over the preorder (FNV-ish): structurally-equal terms
// with distinct heap cells hash identically.  Fast pre-filter before a
// kbo_eq in the connectedness reachability set.
static u32 atp_struct_hash(Term t) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 h = 0x811c9dc5u ^ (term_ext(t) * 0x01000193u);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) {
        h = (h ^ atp_struct_hash(term_ctr_at(t, i))) * 0x01000193u;
      }
      return h ^ (n + 0x9e3779b9u);
    }
    case TAG_FVR:
      return (0x2545f491u ^ term_ext(t)) * 0x01000193u;
    default:
      return (0xdeadbeefu ^ (u32)term_tag(t)) * 0x01000193u;
  }
}

// Collect every one-step rewrite of `t` (all positions, both
// variable-safe directions of each rule) whose result is strictly
// below `peak`, into out_buf[*n .. ).  Mirrors mnf_successors' shape
// but order-gated against the peak rather than the redex.
static u32 atp_conn_successors(AtpState *s, Term t, Term peak,
                               Term *out_buf, u32 *n, u32 cap) {
  u32 size = 1u;
  if (term_tag(t) == TAG_CTR) {
    u32 m = term_ctr_n(t);
    if (m > REWRITE_MAX_ARITY) return size;
    for (u32 i = 0; i < m; i++) {
      u32 base = *n;
      (void)atp_conn_successors(s, term_ctr_at(t, i), peak, out_buf, n, cap);
      for (u32 k = base; k < *n; k++) {
        Term ch[REWRITE_MAX_ARITY];
        for (u32 c = 0; c < m; c++) {
          ch[c] = (c == i) ? out_buf[k] : term_ctr_at(t, c);
        }
        out_buf[k] = term_new_ctr(term_ext(t), ch, m);
      }
    }
  }
  for (u32 j = 0; j < s->n_rules && *n < cap; j++) {
    RewriteSubst sub = {{0}};
    if (thvm_match(s->lhs[j], t, &sub)) {                 // l -> r
      Term repl = thvm_subst_apply(s->rhs[j], &sub);
      if (atp_compare(s, peak, repl) == KBO_GT) out_buf[(*n)++] = repl;
    }
    if (*n >= cap) break;
    if (atp_vars_contained(s->lhs[j], s->rhs[j])) {       // r -> l (var-safe)
      RewriteSubst sb = {{0}};
      if (thvm_match(s->rhs[j], t, &sb)) {
        Term repl = thvm_subst_apply(s->lhs[j], &sb);
        if (atp_compare(s, peak, repl) == KBO_GT) out_buf[(*n)++] = repl;
      }
    }
  }
  return size;
}

static u8 atp_cp_connected_below_peak(AtpState *s, Term lhs, Term rhs,
                                      Term peak) {
  if (s == NULL || peak == 0) return 0;
  // The ordered normal forms of both sides are proper reducts of the
  // peak, hence strictly below it; they are the natural seeds.
  Term l = atp_rewrite_normalize(s, lhs, s->lhs, s->rhs, s->n_rules, 64u);
  Term r = atp_rewrite_normalize(s, rhs, s->lhs, s->rhs, s->n_rules, 64u);
  if (kbo_eq(l, r)) return 1;                 // trivially joined below peak
  // A seed equal to the peak (no decreasing step taken) cannot stay
  // strictly below it -- bail (KEEP) to preserve soundness.
  if (atp_compare(s, peak, l) != KBO_GT || atp_compare(s, peak, r) != KBO_GT) {
    return 0;
  }
  Term  terms[ATP_CONN_MAX_NODES];
  u32   hash [ATP_CONN_MAX_NODES];
  u8    side [ATP_CONN_MAX_NODES];
  u32   n = 0u;
  terms[n] = l; hash[n] = atp_struct_hash(l); side[n] = ATP_CONN_SIDE_S; n++;
  terms[n] = r; hash[n] = atp_struct_hash(r); side[n] = ATP_CONN_SIDE_T; n++;
  Term succ[64];
  for (u32 cur = 0; cur < n; cur++) {
    u32 ns = 0u;
    (void)atp_conn_successors(s, terms[cur], peak, succ, &ns, 64u);
    for (u32 k = 0; k < ns; k++) {
      Term u  = succ[k];
      u32  hu = atp_struct_hash(u);
      u32  found = ATP_CONN_MAX_NODES;
      for (u32 i = 0; i < n; i++) {
        if (hash[i] == hu && kbo_eq(terms[i], u)) { found = i; break; }
      }
      if (found != ATP_CONN_MAX_NODES) {
        if ((side[found] | side[cur]) == (ATP_CONN_SIDE_S | ATP_CONN_SIDE_T)) {
          return 1;                          // join point reached from both
        }
        side[found] = (u8)(side[found] | side[cur]);
        continue;
      }
      if (n >= ATP_CONN_MAX_NODES) return 0;  // overflow -> KEEP (sound)
      terms[n] = u; hash[n] = hu; side[n] = side[cur]; n++;
    }
  }
  return 0;
}

// Stage 7.3a: rule subsumption check.  Returns 1 if there exist a
// rule `(l_k, r_k) ∈ R` and a substitution σ such that
// `(lhs, rhs) = (σ l_k, σ r_k)` (forward) or `(lhs, rhs) =
// (σ r_k, σ l_k)` (symmetric).  Equational subsumption: the σ must
// be CONSISTENT across both sides simultaneously, so we extend the
// same `RewriteSubst` across the two `thvm_match` calls.
//
// Per the domination lemma in `docs/plans/connectedness_design.md`:
// if (lhs, rhs) is rule-subsumed by (l_k, r_k), then rule
// (l_k, r_k) rewrites lhs to rhs in one step under σ, so
// `thvm_rewrite_normalize` collapses the pair too.  Hence
// `n_cps_dropped_rule_subsumed <= n_cps_dropped_joinable` always.
// We tick the counter for empirical measurement; the filtering
// itself stays in 7.1.
//
// 7.3b will add queue subsumption -- which IS orthogonal to 7.1.
static u8 atp_cp_rule_subsumed(AtpState *s, Term lhs, Term rhs) {
  for (u32 k = 0; k < s->n_rules; k++) {
    // Forward: σl_k = lhs AND σr_k = rhs (one σ extended through
    // both matches).
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->lhs[k], lhs, &subst) &&
          thvm_match(s->rhs[k], rhs, &subst)) {
        return 1;
      }
    }
    // Symmetric: σl_k = rhs AND σr_k = lhs.
    {
      RewriteSubst subst = {{0}};
      if (thvm_match(s->lhs[k], rhs, &subst) &&
          thvm_match(s->rhs[k], lhs, &subst)) {
        return 1;
      }
    }
  }
  return 0;
}

// Stage 7.3b: queue subsumption check.  Returns 1 if the candidate
// `(lhs, rhs)` is a substitution instance of some queued CP
// the CP unpacked from `s->cp_packed[k]` -- i.e., there is σ such
// that `(lhs, rhs) = (σ qs[k], σ qt[k])` (or symmetric).
//
// Genuinely orthogonal to 7.1: the queue does not participate in
// `thvm_rewrite_normalize`, so a CP can be queue-subsumed without
// being trivially-joinable (and vice versa).  Used as a real
// filter: the candidate is dropped before reaching the queue.
//
// Cost: O(|queue| * |term|) per candidate; cheap relative to a
// `thvm_rewrite_normalize` because matching has no fixed-point
// loop.
//
// 8e: with -DATP_CP_GRAPH the explicit per-CP loop is replaced by
// ONE thvm_match_multi traversal of cp_graph (the CpSet container is
// the fan-out point).  The verdict is IDENTICAL to the loop, leaf
// for leaf -- 8e is a routing change, not a semantic one.  It is
// memo-free: the (pattern_cell, subject_cell) memo measured 0%
// sharing on the flat CpSet, so shipping it would be pure overhead
// (see the 8e block comment at thvm_match_multi).
//
// 7d: with -DATP_FV_INDEX the O(n_cps) scan is replaced by an
// FV-index retrieval -- componentwise-dominated FVs are pulled from
// the trie and the SAME two-sided thvm_match runs only on those.
// The FV filter is a sound over-approximation, so the verdict is
// IDENTICAL to the scan, CP for CP.  The FV path takes priority over
// the -DATP_CP_GRAPH thvm_match_multi traversal (they are the same
// verdict; FV is the faster one).
#if defined(ATP_FV_INDEX)
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  if (s->n_cps == 0) return 0;
  return atp_fv_index_query(s->fv_index, lhs, rhs);
}
#elif defined(ATP_CP_GRAPH)
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  if (s->n_cps == 0) return 0;
  return thvm_match_multi(s->cp_graph, lhs, rhs);
}
#else
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  for (u32 k = 0; k < s->n_cps; k++) {
    // Match the queued CP straight off its packed byte string.
    // Forward: σqs = lhs AND σqt = rhs (one σ).
    RewriteSubst fwd = {{0}};
    if (acp_match_pair(s->cp_packed[k], lhs, rhs, &fwd)) return 1;
    // Symmetric.
    RewriteSubst sym = {{0}};
    if (acp_match_pair(s->cp_packed[k], rhs, lhs, &sym)) return 1;
  }
  return 0;
}
#endif

// === ATP_CP_CLASSIFY: Waldmeister critical-pair classification =====
//
// Ported from Waldmeister's `NewClassification` ("new classification")
// and `ClasFunctions` ("classification functions") modules
// (waldmeister/sources/CLAS/NewClassification.c, ClasFunctions.c).
//
// Waldmeister classifies every critical pair with a list of
// *Kriterien* ("criteria", killer predicates).  When a predicate
// fires, an *Aktion* ("action") rescales the CP's queue weight:
//   Act_ultimate -> minimal weight (pick first)
//   Act_never    -> maximal weight / discard  ("last" in -clas)
//   Act_double   -> weight * 2  (deprioritize)
//   Act_half     -> weight / 2  (prioritize)
//   Act_normal   -> no-op
// See C_Classify in NewClassification.c:313.
//
// The predicates ported here (ClasFunctions.c:107-176):
//   CF_KillerPraedikatR   -- CP, oriented, reduces an existing R/E LHS
//   CF_KillerPraedikatE   -- CP, unorientable, reduces an existing LHS
//   CF_KillerPraedikatRE  -- KillerR OR KillerE
//   CF_EChildPraedikat    -- a parent of the CP is an equation, not a
//                            rule (`!RE_IstRegel(parent)`)
//
// "Reduces an existing LHS" follows IR_TPRReduziertRE /
// IR_TPEReduziertRE (Interreduktion.c:413, "Interreduktion" =
// interreduction) -> TermpaarAnwendbar -> NF_TermpaarAnwendbar: the
// CP, viewed as a rewrite rule, matches some non-variable subterm of
// an existing rule's LHS.
#ifdef ATP_CP_CLASSIFY

// CriteriaEnumType (NewClassification.h:59).
typedef enum {
  ATP_CRIT_KILLER_R,
  ATP_CRIT_KILLER_RE,
  ATP_CRIT_KILLER_E,
  ATP_CRIT_ECHILD
} AtpCritKind;

// ActionEnumType (NewClassification.h:68).
typedef enum {
  ATP_ACT_ULTIMATE,
  ATP_ACT_NORMAL,
  ATP_ACT_DOUBLE,
  ATP_ACT_HALF,
  ATP_ACT_NEVER
} AtpCritAction;

// Does `pat`, used as a rewrite rule's LHS, match `term` or any of
// its non-variable subterms?  Mirrors NF_TermpaarAnwendbar
// (NFBildung.c:741): walk every subterm, test match.  The match
// makes the CP capable of rewriting that existing rule's LHS.
static u8 atp_clas_pat_reduces(Term pat, Term term) {
  if (term_tag(term) == TAG_CTR) {
    RewriteSubst subst = {{0}};
    if (thvm_match(pat, term, &subst)) return 1;
    u32 n = term_ctr_n(term);
    for (u32 i = 0; i < n; i++) {
      if (atp_clas_pat_reduces(pat, term_ctr_at(term, i))) return 1;
    }
  }
  return 0;
}

// Does the CP `(big -> small)`, read as a rewrite rule, reduce the
// LHS of any existing rule in R?  TermpaarAnwendbar
// (Interreduktion.c:398): scan all rules, test applicability.  A
// bare-variable `big` is not a well-formed rule LHS (it would
// rewrite every term), so it never counts as a killer -- this
// matches Waldmeister, where a rule LHS is always a non-variable.
static u8 atp_clas_reduces_some_rule(AtpState *s, Term big) {
  if (term_tag(big) != TAG_CTR) return 0;
  for (u32 k = 0; k < s->n_rules; k++) {
    if (atp_clas_pat_reduces(big, s->lhs[k])) return 1;
  }
  return 0;
}

// CF_KillerPraedikatR (ClasFunctions.c:107): the CP is orientable
// (KBO_GT / KBO_LT) and, oriented big->small, reduces an existing
// rule LHS.
static u8 atp_clas_killer_r(AtpState *s, Term lhs, Term rhs) {
  switch (atp_compare(s, lhs, rhs)) {
    case KBO_GT: return atp_clas_reduces_some_rule(s, lhs);
    case KBO_LT: return atp_clas_reduces_some_rule(s, rhs);
    default:     return 0;
  }
}

// CF_KillerPraedikatE (ClasFunctions.c:132): the CP is unorientable
// (KBO_UN / KBO_EQ) and reduces an existing rule LHS in either
// direction.
static u8 atp_clas_killer_e(AtpState *s, Term lhs, Term rhs) {
  KboCmp c = atp_compare(s, lhs, rhs);
  if (c == KBO_GT || c == KBO_LT) return 0;
  return atp_clas_reduces_some_rule(s, lhs) ||
         atp_clas_reduces_some_rule(s, rhs);
}

// CF_KillerPraedikatRE (ClasFunctions.c:147): KillerR OR KillerE.
static u8 atp_clas_killer_re(AtpState *s, Term lhs, Term rhs) {
  return atp_clas_killer_r(s, lhs, rhs) || atp_clas_killer_e(s, lhs, rhs);
}

// CF_EChildPraedikat (ClasFunctions.c:172): a parent of this CP is
// an equation rather than a rule.  thvm does not tag a stored rule
// as rule-vs-equation, so the closest analogue is "the parent rule
// is itself unorientable under the active reduction order" -- the
// non-rule case that orient_and_add routes through the unfailing
// fallback.  `rule_a` / `rule_b` index s->lhs[] / s->rhs[]; an
// index >= n_rules means "no parent".
static u8 atp_clas_echild(AtpState *s, u32 rule_a, u32 rule_b) {
  if (rule_a < s->n_rules) {
    if (atp_compare(s, s->lhs[rule_a], s->rhs[rule_a]) == KBO_UN) return 1;
  }
  if (rule_b < s->n_rules) {
    if (atp_compare(s, s->lhs[rule_b], s->rhs[rule_b]) == KBO_UN) return 1;
  }
  return 0;
}

// Evaluate one criterion against a CP.
static u8 atp_clas_eval(AtpState *s, AtpCritKind crit,
                        Term lhs, Term rhs, u32 rule_a, u32 rule_b) {
  switch (crit) {
    case ATP_CRIT_KILLER_R:  return atp_clas_killer_r(s, lhs, rhs);
    case ATP_CRIT_KILLER_RE: return atp_clas_killer_re(s, lhs, rhs);
    case ATP_CRIT_KILLER_E:  return atp_clas_killer_e(s, lhs, rhs);
    case ATP_CRIT_ECHILD:    return atp_clas_echild(s, rule_a, rule_b);
  }
  return 0;
}

// One classification rule: a criterion plus the action it triggers.
typedef struct {
  AtpCritKind   crit;
  AtpCritAction action;
} AtpCriterion;

// Default classification config.  Mirrors Waldmeister's ClasCriteria
// list (NewClassification.c:222): each criterion is tried in order,
// the first match decides the action.
//
//   EChild   -> double  : CPs descending from an unorientable parent
//                         tend to be unorientable themselves; defer
//                         them so oriented rules saturate first.
//   KillerRE -> never   : a killer CP would remove a rule via
//                         interreduction.  When that CP is also
//                         rule-subsumed (checked at the call site),
//                         dropping it is sound -- rule subsumption is
//                         a subset of joinability, so the dropped CP
//                         carries no equational consequence not
//                         already implied by R.
//
// The KillerRE/never row only discards when paired with the
// rule-subsumption guard in atp_classify_cp; the EChild/double row
// only rescales priority and never drops.
static const AtpCriterion ATP_CLAS_CRITERIA[] = {
  { ATP_CRIT_ECHILD,    ATP_ACT_DOUBLE },
  { ATP_CRIT_KILLER_RE, ATP_ACT_NEVER  },
};
#define ATP_CLAS_N_CRITERIA \
  (sizeof(ATP_CLAS_CRITERIA) / sizeof(ATP_CLAS_CRITERIA[0]))

// Classify one CP.  Returns 1 if the CP should be dropped (the
// firing action is Act_never AND the soundness guard holds);
// otherwise returns 0 and writes the rescaled priority into
// `*pri`.  Mirrors C_Classify (NewClassification.c:313): walk the
// criteria, first match wins, apply its action.
static u8 atp_classify_cp(AtpState *s, Term lhs, Term rhs,
                          u32 rule_a, u32 rule_b, u32 *pri) {
  for (u32 i = 0; i < ATP_CLAS_N_CRITERIA; i++) {
    if (!atp_clas_eval(s, ATP_CLAS_CRITERIA[i].crit, lhs, rhs,
                       rule_a, rule_b)) {
      continue;
    }
    switch (ATP_CLAS_CRITERIA[i].action) {
      case ATP_ACT_ULTIMATE:
        *pri = 0u;
        return 0;
      case ATP_ACT_NEVER:
        // Soundness guard: only discard when the CP is also
        // rule-subsumed (a subset of joinability -- see
        // atp_cp_rule_subsumed).  Otherwise treat as Act_double:
        // deprioritize without losing the CP.
        if (atp_cp_rule_subsumed(s, lhs, rhs)) {
          return 1;
        }
        if (*pri <= 0x7fffffffu) *pri *= 2u;
        return 0;
      case ATP_ACT_DOUBLE:
        if (*pri <= 0x7fffffffu) *pri *= 2u;
        return 0;
      case ATP_ACT_HALF:
        *pri /= 2u;
        return 0;
      case ATP_ACT_NORMAL:
        return 0;
    }
  }
  return 0;
}

// Push one CP onto the heap with a caller-supplied priority,
// bypassing atp_cp_priority.  The classifier computes a rescaled
// priority; this variant lets that value reach the queue.  Mirrors
// atp_cp_heap_push, but takes the priority instead of computing it
// from the packed node count.
static void atp_cp_heap_push_pri(AtpState *s, Term lhs, Term rhs,
                                 u32 trace, u32 pri) {
  atp_ensure_cp_cap(s, s->n_cps + 1);
  u32 i = s->n_cps;
  u8 *packed     = acp_pack(lhs, rhs, NULL, NULL);
  s->cp_packed[i]= packed;
  s->cp_trace[i] = trace;
  s->cp_pri[i]   = pri;
  u32 seq        = s->cp_seq_next++;
  s->cp_seq[i]   = seq;
  s->n_cps++;
  atp_cp_sift_up(s, i);
  atp_cp_graph_sync(s);
#ifdef ATP_FV_INDEX
  atp_fv_index_insert(s->fv_index, lhs, rhs, packed, seq);
#endif
}

#endif  // ATP_CP_CLASSIFY

// === Ground-joinability redundancy criterion ========================
// Martin-Nipkow (1990) / Twee (CADE 2021 sec 3.1) / Avenhaus-Hillen-
// brand-Loechner (2003).  A critical pair is GROUND-JOINABLE if every
// ground instance of it joins; such a CP is redundant and may be
// deleted without losing refutational completeness.  We implement
// Twee's SYMBOLIC order-parameterised version (gj_spec.md sections C+D):
// a constraint C is a total preorder on the CP's atoms (variables AND
// the relevant constants), represented as a Model that assigns each atom
// a (major,minor) rank.  Under C we rewrite SYMBOLICALLY: a rule l->r
// fires at a matched position iff lessIn(C, r-image, l-image) is defined
// -- i.e. (l-image) >=_C (r-image) holds for ALL grounding sigma that
// satisfy C -- and the two images are syntactically distinct.  lessIn
// (section D) decides >=_C for KBO by minimising the weight-difference
// linear form over all such sigma (the suffix-sum `minimumIn`), then a
// lexicographic comparison.  Crucially lessIn NEVER over-approximates
// >=_C (soundness condition P2): if it returns "defined" the inequality
// truly holds for every grounding, so no illegal rewrite ever fires.
//
// The groundJoin driver maintains a worklist of branches that together
// COVER every total preorder of the atoms.  Each branch is solved to a
// Model (or, when it forces ties, an equality substitution); the CP is
// normalised under the Model and the two normal forms compared.  On a
// successful join the Model is GENERALISED (weakenModel: drop an atom or
// merge two adjacent strict groups into a tie, NEVER merging two
// constants), and the complementary region is re-queued via diag().
// Equality branches unify the tied variables and recurse on the smaller
// CP.  Only when the worklist is fully exhausted (every ground ordering
// covered by a successful join -- soundness condition P3) do we DELETE.
// Any budget overflow, OOM, or unjoined branch -> KEEP (sound fallback).
//
// KBO is the supported order.  For LPO (s->lpo != NULL) we KEEP
// unconditionally: lessIn is implemented for KBO only, so GJ is a no-op
// (always sound) under LPO for now.
//
// Deletion is gated at the call site behind the runtime flag
// s->use_ground_join (Method -> {... "GroundJoin" -> True}); the
// n_cps_ground_joinable counter ticks regardless for measurement.  The
// whole region compiles out unless ATP_CP_GROUND_JOIN is defined (the
// shipped paclet defines it; see WL_ATP_DEFINES).
#ifdef ATP_CP_GROUND_JOIN

// Cap on distinct variables of the CP.  The branch worklist still
// terminates for larger counts, but small CPs are the norm and a tight
// cap bounds worst-case branching; over the cap -> KEEP (sound).
#define ATP_GJ_MAX_VARS 6
// Max atoms in a Model = CP variables + relevant constants.
#define ATP_GJ_MAX_ATOMS 16
// Per-side symbolic-rewrite step cap during a single join attempt.
#define ATP_GJ_NORM_CAP 64
// Cap on the number of branches processed by the groundJoin driver
// before bailing to KEEP (sound budget fallback, gj_spec.md C).
#define ATP_GJ_BRANCH_CAP 4096

// Collect distinct TAG_FVR var ids appearing in `t` into ids[] (size
// cap).  *n is the running count; returns 0 if it would overflow `cap`
// (caller treats overflow as "too many vars -> keep").
static u8 atp_gj_collect_vars(Term t, u32 *ids, u32 *n, u32 cap) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      for (u32 i = 0; i < *n; i++) {
        if (ids[i] == id) return 1;       // already seen
      }
      if (*n >= cap) return 0;            // too many distinct vars
      ids[*n] = id;
      (*n)++;
      return 1;
    }
    case TAG_CTR: {
      u32 k = term_ctr_n(t);
      for (u32 i = 0; i < k; i++) {
        if (!atp_gj_collect_vars(term_ctr_at(t, i), ids, n, cap)) return 0;
      }
      return 1;
    }
    default: return 1;
  }
}

// ── Atoms ───────────────────────────────────────────────────────────
// An atom is a variable (TAG_FVR id) or a nullary constant (TAG_CTR
// label, arity 0).  We compare atom SIZES under KBO: a constant's size
// is its fixed weight; a variable's size is an unknown integer >= the
// minimal admissible ground size.  The constraint C orders atoms by size.
typedef struct {
  u8  is_const;   // 1 = constant (TAG_CTR/0), 0 = variable
  u32 id;         // var id (is_const=0) or label (is_const=1)
} GjAtom;

static inline u8 gj_atom_eq(GjAtom a, GjAtom b) {
  return a.is_const == b.is_const && a.id == b.id;
}

// Size of a constant atom under the (real) KBO config.  Constants have
// no children, so their size is just their per-symbol weight.
static inline u32 gj_const_size(const KboConfig *cfg, u32 label) {
  return (label < cfg->n_labels) ? cfg->weights[label] : 0u;
}

// The minimal admissible ground-term size (KBO w0): any ground term has
// size >= var_weight (a single variable's image is at least one minimal
// symbol).  Used as the universal lower bound for an UNCONSTRAINED
// variable's size.
static inline u32 gj_min_size(const KboConfig *cfg) {
  // The smallest possible ground term is a single minimal-weight
  // constant; if every constant outweighs var_weight, a 1-arg term over
  // a 0-weight unary symbol could still be var_weight.  var_weight (w0)
  // is the safe, KBO-standard lower bound (P1: weights >= w0 > 0 except
  // possibly one unary symbol of weight 0, whose argument is >= w0).
  return cfg->var_weight;
}

// ── Model ───────────────────────────────────────────────────────────
// A Model is a total preorder on a fixed atom set: each atom carries a
// `major` size-rank.  a < b  iff  major(a) < major(b);  a == b (a tie,
// same SIZE) iff major(a) == major(b).  Atoms with the same major form
// one "tie group" (all forced to equal size).  (Twee carries a second
// `minor` field to make the preorder a strict total order on atoms for
// its solver; the size-reasoning here needs only `major`, so we drop it.)
typedef struct {
  GjAtom atom[ATP_GJ_MAX_ATOMS];
  u32    major[ATP_GJ_MAX_ATOMS];   // size-rank; equal major = equal size
  u32    n;
} GjModel;

// Find atom `a` in the model; return its index or -1.
static int gj_model_find(const GjModel *m, GjAtom a) {
  for (u32 i = 0; i < m->n; i++) if (gj_atom_eq(m->atom[i], a)) return (int)i;
  return -1;
}

// Compare two atoms' SIZES under the model: -1 (a<b), 0 (tie), 1 (a>b),
// or 2 = UNKNOWN (one or both atoms absent from the model -> C says
// nothing about them).
static int gj_model_cmp(const GjModel *m, GjAtom a, GjAtom b) {
  if (gj_atom_eq(a, b)) return 0;
  int ia = gj_model_find(m, a), ib = gj_model_find(m, b);
  if (ia < 0 || ib < 0) return 2;
  u32 ma = m->major[ia], mb = m->major[ib];
  if (ma < mb) return -1;
  if (ma > mb) return 1;
  return 0;
}

// ── KBO size as a linear form, minimised under C (gj_spec.md D) ──────
// We compare KBO sizes of t and u.  D = size(u) - size(t) is a linear
// form  c0 + sum_v coeff[v] * size(v)  where c0 is the net weight of
// FUNCTION-SYMBOL occurrences (variables excluded) and coeff[v] =
// count(u,v) - count(t,v).  size(v) is the unknown ground size of v.
//
// gj_size_form accumulates D's pieces by a diff-traversal of (t,u):
//   *c0          += net constant/funcsym weight  (u on +, t on -)
//   var_coeff[id] += net variable count          (u on +, t on -)
// Returns 0 if a variable id exceeds the table; caller treats that as
// "cannot decide" (Incomparable -> KEEP-safe).
static void gj_form_addto(Term t, int sign, const KboConfig *cfg,
                          long long *c0, long long *var_coeff) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (id < KBO_MAX_VAR) var_coeff[id] += sign;
      return;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      *c0 += (long long)sign *
             (long long)((lab < cfg->n_labels) ? cfg->weights[lab] : 0);
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++)
        gj_form_addto(term_ctr_at(t, i), sign, cfg, c0, var_coeff);
      return;
    }
    default: return;
  }
}

// minimumIn (gj_spec.md D, note "kbo under assumptions").  Compute the
// MINIMUM of D = c0 + sum_v coeff[v]*size(v) over every assignment of
// variable sizes consistent with the model `m` (and every grounding of
// unconstrained variables).  *finite is set 0 if the minimum is
// unbounded below (-infinity).  On *finite==0 the returned value is
// meaningless.  This is the load-bearing soundness primitive: an
// UNDER-estimate of the true minimum is the unsafe direction (it could
// claim D>0 strict when some sigma drives D<=0 -- an OVER-approximation
// of >=_C).  We therefore compute the EXACT minimum; any case we cannot
// bound exactly is reported as -infinity (KEEP-safe).
//
// Method: variables constrained by C lie in tie/order groups separated
// by constants along the size order.  We process the model's atoms in
// ascending size (major) order.  Constants act as fixed lower/upper
// pivots; the variables between two consecutive constants form a group
// with lower bound lo = size(lower constant) (or w0 if none) and upper
// bound hi = size(upper constant) (or none).  Within a group the
// variables are size-ordered by their major: x in the group satisfies
// lo <= size(x) and (if hi exists) size(x) <= hi, and the group's
// variables are nondecreasing in size.  Per spec D, with coeffs listed
// in ascending group order and sums = scanr1(+) (suffix sums):
//   all suffix sums >= 0:  min = (sum coeffs) * lo
//   some suffix sum < 0, no hi:  -infinity
//   some suffix sum < 0, hi exists: min = (sum coeffs)*lo
//                                      + (-min_suffix)*(lo - hi)
// Variables NOT in the model ("orphans") with coeff k: k<0 -> -infinity;
// else contribute 0 (size driven to its own minimum w0, but coeff>=0 so
// the minimal contribution toward D is taking size as small as possible;
// orphan min-contribution per spec = 0 for k>=0 since unconstrained
// orphan can be made equal to the global infimum which is folded into
// c0 already via... ) -- see note below.
//
// Returns the (finite) minimum; sets *finite.
static long long gj_minimum_in(const GjModel *m, const KboConfig *cfg,
                               const long long *var_coeff,
                               const u32 *cp_vars, u32 n_cp_vars,
                               long long c0, u8 *finite) {
  *finite = 1;
  long long total = c0;
  long long w0 = (long long)gj_min_size(cfg);

  // Orphan variables: present in the CP but absent from the model.
  // Their size is any value >= w0, unconstrained relative to others.
  //   coeff < 0  -> size can grow without bound -> -infinity.
  //   coeff > 0  -> minimised by size = w0 -> contributes coeff*w0.
  //   coeff == 0 -> contributes 0.
  for (u32 i = 0; i < n_cp_vars; i++) {
    u32 id = cp_vars[i];
    GjAtom va = { .is_const = 0, .id = id };
    if (gj_model_find(m, va) >= 0) continue;          // not an orphan
    long long k = (id < KBO_MAX_VAR) ? var_coeff[id] : 0;
    if (k < 0) { *finite = 0; return 0; }
    total += k * w0;
  }

  // Walk the model's atoms in ascending size order.  Group variables
  // between constants.  We sort indices by major (stable; minor breaks
  // ties but same-major atoms are one group regardless of minor).
  u32 order[ATP_GJ_MAX_ATOMS];
  for (u32 i = 0; i < m->n; i++) order[i] = i;
  for (u32 i = 1; i < m->n; i++) {                    // insertion sort
    u32 key = order[i];
    u32 km = m->major[key];
    int j = (int)i - 1;
    while (j >= 0 && m->major[order[j]] > km) { order[j + 1] = order[j]; j--; }
    order[j + 1] = key;
  }

  // Scan groups.  A group is a maximal run of consecutive (in size
  // order) VARIABLE atoms; the constant immediately before sets lo, the
  // constant immediately after sets hi.  Multiple variables tied with a
  // constant (same major as a constant) are pinned to that constant's
  // size -- handled by treating a same-major constant as both bound.
  u32 i = 0;
  long long cur_lo = w0;   // running lower bound (last constant size, or w0)
  while (i < m->n) {
    u32 ai = order[i];
    if (m->atom[ai].is_const) {
      cur_lo = (long long)gj_const_size(cfg, m->atom[ai].id);
      i++;
      continue;
    }
    // Start of a variable group at size-order position i.  Collect the
    // group: consecutive variables.  Note variables tied (same major)
    // with each other share a size; we still list each coeff (they all
    // share one unknown, but the suffix-sum derivation treats them as a
    // nondecreasing chain which a tie satisfies, so listing each is
    // correct -- ties just collapse y_i deltas to 0, never changing the
    // suffix-sum sign test).
    long long coeffs[ATP_GJ_MAX_ATOMS];
    u32 ng = 0;
    while (i < m->n && !m->atom[order[i]].is_const) {
      u32 vid = m->atom[order[i]].id;
      long long k = (vid < KBO_MAX_VAR) ? var_coeff[vid] : 0;
      coeffs[ng++] = k;
      i++;
    }
    // Upper bound: the next atom (if a constant) gives hi; if the next
    // atom is past the end, no upper bound.  (The next atom can only be
    // a constant here, since we consumed all consecutive variables.)
    u8 has_hi = 0;
    long long hi = 0;
    if (i < m->n && m->atom[order[i]].is_const) {
      has_hi = 1;
      hi = (long long)gj_const_size(cfg, m->atom[order[i]].id);
    }
    // suffix sums
    long long sum = 0;
    long long min_suffix = 0;       // minimum over suffix sums (incl. empty=+inf guard)
    u8 have_min = 0;
    long long suff[ATP_GJ_MAX_ATOMS];
    long long acc = 0;
    for (int g = (int)ng - 1; g >= 0; g--) { acc += coeffs[g]; suff[g] = acc; }
    for (u32 g = 0; g < ng; g++) {
      sum += coeffs[g];
      if (!have_min || suff[g] < min_suffix) { min_suffix = suff[g]; have_min = 1; }
    }
    // sum == total coeff of the group; min_suffix == min suffix sum.
    if (ng == 0) { cur_lo = has_hi ? hi : cur_lo; continue; }
    if (min_suffix >= 0) {
      total += sum * cur_lo;
    } else if (!has_hi) {
      *finite = 0; return 0;        // unbounded below
    } else {
      total += sum * cur_lo + (-min_suffix) * (cur_lo - hi);
    }
    cur_lo = has_hi ? hi : cur_lo;
  }
  return total;
}

// sizeLessIn: decide size(t) vs size(u) under C.
//   returns  -1 : size(t) <  size(u) for all sigma  (Strict on size)
//             0 : size(t) == size(u) for all sigma  (fall through to lex)
//             1 : not provable either way            (Incomparable)
// Per spec D: L = minimum of D = size(u)-size(t) over sigma|=C.
//   L >  0 -> Strict;  L == 0 -> Nonstrict (lex);  L < 0 / -inf -> Incomp.
static int gj_size_less_in(const GjModel *m, const KboConfig *cfg,
                           Term t, Term u,
                           const u32 *cp_vars, u32 n_cp_vars) {
  long long c0 = 0;
  long long var_coeff[KBO_MAX_VAR];
  for (u32 i = 0; i < KBO_MAX_VAR; i++) var_coeff[i] = 0;
  // D = size(u) - size(t): u on +, t on -.
  gj_form_addto(u, +1, cfg, &c0, var_coeff);
  gj_form_addto(t, -1, cfg, &c0, var_coeff);
  u8 finite = 0;
  long long L = gj_minimum_in(m, cfg, var_coeff, cp_vars, n_cp_vars, c0,
                              &finite);
  if (!finite) return 1;            // unbounded below -> Incomparable
  if (L > 0) return -1;             // size(t) < size(u) always
  if (L == 0) return 0;             // equal size for all sigma -> lex
  return 1;                         // L < 0: not provable -> Incomparable
}

// ── lexLessIn (gj_spec.md D step 2), reached only when sizes are
// FORCED equal.  Returns one of GJ_STRICT / GJ_NONSTRICT / GJ_INCOMP
// meaning t < u strictly / t <= u (and == for some sigma) / unknown.
enum { GJ_STRICT = 0, GJ_NONSTRICT = 1, GJ_INCOMP = 2 };

static GjAtom gj_term_atom(Term t, u8 *ok) {
  GjAtom a = {0};
  if (term_tag(t) == TAG_FVR) { a.is_const = 0; a.id = term_ext(t); *ok = 1; }
  else if (term_tag(t) == TAG_CTR && term_ctr_n(t) == 0) {
    a.is_const = 1; a.id = term_ext(t); *ok = 1;
  } else *ok = 0;
  return a;
}

// lessEqInModel for two atoms: is atom(t) <= atom(u) (size) under C?
// returns GJ_STRICT (t<u), GJ_NONSTRICT (t==u forced), or GJ_INCOMP.
static int gj_atom_less_eq(const GjModel *m, GjAtom at, GjAtom au) {
  if (gj_atom_eq(at, au)) return GJ_NONSTRICT;
  int c = gj_model_cmp(m, at, au);
  if (c == -1) return GJ_STRICT;
  if (c == 0)  return GJ_NONSTRICT;  // tie group: equal size
  return GJ_INCOMP;                  // 1 (a>b) or 2 (unknown)
}

static int gj_lex_less_in(const GjModel *m, const KboConfig *cfg,
                          Term t, Term u,
                          const u32 *cp_vars, u32 n_cp_vars, u32 depth);

// lessIn(cond, t, u): size-first then lex.  Returns the strictness of
// "t <= u under C" or GJ_INCOMP.
//   GJ_STRICT    : tσ <  uσ for ALL σ |= C
//   GJ_NONSTRICT : tσ <= uσ for ALL σ |= C
//   GJ_INCOMP    : not provable
static int gj_less_in(const GjModel *m, const KboConfig *cfg,
                      Term t, Term u, const u32 *cp_vars, u32 n_cp_vars,
                      u32 depth) {
  if (kbo_eq(t, u)) return GJ_NONSTRICT;
  int sz = gj_size_less_in(m, cfg, t, u, cp_vars, n_cp_vars);
  if (sz == -1) return GJ_STRICT;     // size(t) < size(u) strictly
  if (sz == 1)  return GJ_INCOMP;     // size not provably <=
  // sizes forced equal -> lexicographic
  return gj_lex_less_in(m, cfg, t, u, cp_vars, n_cp_vars, depth);
}

static int gj_lex_less_in(const GjModel *m, const KboConfig *cfg,
                          Term t, Term u,
                          const u32 *cp_vars, u32 n_cp_vars, u32 depth) {
  if (depth > 64) return GJ_INCOMP;   // recursion guard -> KEEP-safe
  if (kbo_eq(t, u)) return GJ_NONSTRICT;

  u8 t_atom = 0, u_atom = 0;
  GjAtom at = gj_term_atom(t, &t_atom);
  GjAtom au = gj_term_atom(u, &u_atom);

  // Both atoms: direct order from C.
  if (t_atom && u_atom) return gj_atom_less_eq(m, at, au);

  // t atom, u compound: if some atomic proper subterm v of u has t <= v
  // under C (strictly or tied), then t < u strictly (since |u| has the
  // same size and a larger structure dominating t).  Conservative:
  // require an atomic subterm v with gj_atom_less_eq(t,v) != INCOMP.
  if (t_atom && !u_atom && term_tag(u) == TAG_CTR) {
    u32 n = term_ctr_n(u);
    for (u32 i = 0; i < n; i++) {
      Term v = term_ctr_at(u, i);
      u8 vok = 0; GjAtom av = gj_term_atom(v, &vok);
      if (vok) {
        int r = gj_atom_less_eq(m, at, av);
        if (r != GJ_INCOMP) return GJ_STRICT;
      }
    }
    return GJ_INCOMP;
  }

  // t compound, u atom: symmetric -- t > u, so "t <= u" is not provable.
  if (!t_atom && u_atom) return GJ_INCOMP;

  // Both compound: compare heads.
  if (term_tag(t) == TAG_CTR && term_tag(u) == TAG_CTR) {
    u32 lt = term_ext(t), lu = term_ext(u);
    if (lt == lu) {
      u32 nt = term_ctr_n(t), nu = term_ctr_n(u);
      if (nt != nu) return GJ_INCOMP;
      // left-to-right; at the first differing pair recurse with lessIn.
      for (u32 i = 0; i < nt; i++) {
        Term ct = term_ctr_at(t, i), cu = term_ctr_at(u, i);
        if (kbo_eq(ct, cu)) continue;
        int r = gj_less_in(m, cfg, ct, cu, cp_vars, n_cp_vars, depth + 1);
        if (r == GJ_STRICT)    return GJ_STRICT;
        if (r == GJ_NONSTRICT) {
          // The spec unifies the tied arg pair and continues; we can
          // only continue safely if the remaining args make the verdict.
          // Conservatively, a Nonstrict (could be equal) at this arg
          // means we need the SAME pair to also satisfy the converse to
          // continue; without a unifier we report INCOMP unless this is
          // the last differing arg AND it is provably tied.  Since
          // kbo_eq already filtered equal args, a Nonstrict here that is
          // not Strict means "<= but maybe =", so continuing requires the
          // arg to be forced-equal.  We treat Nonstrict as "continue only
          // if no later arg differs"; otherwise INCOMP (KEEP-safe).
          u8 later_diff = 0;
          for (u32 j = i + 1; j < nt; j++) {
            if (!kbo_eq(term_ctr_at(t, j), term_ctr_at(u, j))) {
              later_diff = 1; break;
            }
          }
          if (later_diff) return GJ_INCOMP;
          return GJ_NONSTRICT;
        }
        return GJ_INCOMP;   // INCOMP at first differing arg
      }
      return GJ_NONSTRICT;  // all args equal (shouldn't reach: kbo_eq above)
    }
    // different heads, sizes equal: precedence decides.
    u32 pt = (lt < cfg->n_labels) ? cfg->precedence[lt] : 0;
    u32 pu = (lu < cfg->n_labels) ? cfg->precedence[lu] : 0;
    if (pt < pu) return GJ_STRICT;     // t's head precedes u's -> t < u
    return GJ_INCOMP;
  }

  return GJ_INCOMP;
}

// Does variable id `id` occur in `t`?  (Local copy so GJ does not depend
// on the ATP_ORDERED_REWRITE/ATP_MNF build flags.)
static int gj_has_var(Term t, u32 id) {
  switch (term_tag(t)) {
    case TAG_FVR: return term_ext(t) == id;
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      for (u32 i = 0; i < n; i++) if (gj_has_var(term_ctr_at(t, i), id)) return 1;
      return 0;
    }
    default: return 0;
  }
}

// Is every TAG_FVR variable of `sub` also a variable of `sup`?  Used to
// reject malformed (extra-variable) rule applications during GJ rewrite.
static u8 gj_vars_subset(Term sub, Term sup) {
  switch (term_tag(sub)) {
    case TAG_FVR: return gj_has_var(sup, term_ext(sub)) ? 1 : 0;
    case TAG_CTR: {
      u32 n = term_ctr_n(sub);
      for (u32 i = 0; i < n; i++)
        if (!gj_vars_subset(term_ctr_at(sub, i), sup)) return 0;
      return 1;
    }
    default: return 1;
  }
}

// ── C-parameterised rewriting (gj_spec.md C/D) ──────────────────────
// A rule l->r fires under model C at a matched position iff
// (l-image) >=_C (r-image) -- i.e. gj_less_in(C, r-image, l-image) is
// GJ_STRICT or GJ_NONSTRICT -- AND the two images are syntactically
// distinct.  Oriented rules (l > r unconditionally) also satisfy this,
// so the single test suffices for both.  Returns the rewritten term;
// sets *fired on success.
static Term gj_rewrite_step(AtpState *s, const GjModel *m, Term t,
                            const u32 *cp_vars, u32 n_cp_vars, u8 *fired) {
  const KboConfig *cfg = s->kbo;
  for (u32 i = 0; i < s->n_rules; i++) {
    // A well-formed rewrite rule never has a bare-variable LHS (it would
    // match and "rewrite" everything).  Skip such malformed rules so the
    // GJ test is robust even if handed an ill-formed rule set.
    if (term_tag(s->lhs[i]) == TAG_FVR) continue;
    RewriteSubst subst = {{0}};
    if (thvm_match(s->lhs[i], t, &subst)) {
      Term reduct = thvm_subst_apply(s->rhs[i], &subst);
      // Defense-in-depth: a well-formed rewrite rule has vars(rhs) subset
      // of vars(lhs), so a successful match binds every rhs variable and
      // `reduct` is variable-free over the rule's own variables.  If the
      // rule introduces an extra variable (malformed input), `reduct`
      // would carry an unbound variable and corrupt the join comparison.
      // Skip such a step: every variable of `reduct` must also occur in
      // `t` (the term we are rewriting).  No-op for well-formed rules.
      if (!gj_vars_subset(reduct, t)) continue;
      if (kbo_eq(reduct, t)) continue;     // no-op image
      // fire iff t (=l-image) >=_C reduct (=r-image), i.e.
      // gj_less_in(reduct, t) is defined (reduct <= t under C).
      int r = gj_less_in(m, cfg, reduct, t, cp_vars, n_cp_vars, 0);
      if (r == GJ_STRICT || r == GJ_NONSTRICT) {
        *fired = 1;
        return reduct;
      }
    }
  }
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > REWRITE_MAX_ARITY) { *fired = 0; return t; }
    Term children[REWRITE_MAX_ARITY];
    for (u32 i = 0; i < n; i++) children[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      u8 sub_fired = 0;
      Term rc = gj_rewrite_step(s, m, children[i], cp_vars, n_cp_vars,
                                &sub_fired);
      if (sub_fired) {
        children[i] = rc;
        *fired = 1;
        return term_new_ctr(term_ext(t), children, n);
      }
    }
  }
  *fired = 0;
  return t;
}

// Normalise `t` under model C with C-parameterised steps (step_cap).
static Term gj_normalize(AtpState *s, const GjModel *m, Term t,
                         const u32 *cp_vars, u32 n_cp_vars, u32 step_cap) {
  for (u32 k = 0; k < step_cap; k++) {
    u8 fired = 0;
    Term t2 = gj_rewrite_step(s, m, t, cp_vars, n_cp_vars, &fired);
    if (!fired) return t;
    t = t2;
  }
  return t;
}

// Join the two CP sides under model C: normalise both and compare.
static u8 gj_joins_under(AtpState *s, const GjModel *m, Term lhs, Term rhs,
                         const u32 *cp_vars, u32 n_cp_vars) {
  Term nl = gj_normalize(s, m, lhs, cp_vars, n_cp_vars, ATP_GJ_NORM_CAP);
  Term nr = gj_normalize(s, m, rhs, cp_vars, n_cp_vars, ATP_GJ_NORM_CAP);
  return kbo_eq(nl, nr);
}

// ── Coverage driver (gj_spec.md C) ──────────────────────────────────
// We must verify joinability under EVERY total preorder of the CP's
// atoms (variables + relevant constants).  Coverage (P3) is realised by
// EXHAUSTIVE enumeration, structured exactly as Twee's groundJoin:
//
//  * The variables are partitioned by a total preorder (ordered set
//    partition).  When two variables fall in the SAME class they are
//    forced EQUAL in SIZE; for KBO that is genuinely an `=` case, and
//    Twee step 5 handles it by UNIFYING the equal variables and
//    recursing on the smaller CP.  We do exactly that: collapse each
//    multi-variable class to a single representative (lowest id), apply
//    the var->representative substitution to both CP sides, and recurse
//    gj_cover on the reduced CP.  The recursion re-covers every sub-
//    ordering (incl. further ties and constant placements) of the
//    representatives, so the `=` region is fully covered.  Termination:
//    each recursion strictly drops the variable count.
//
//  * When all variables are in DISTINCT classes (a strict total order on
//    the variables), there is no `=` to discharge.  We then interleave
//    the relevant CONSTANTS into that strict variable chain in every
//    order (constants among themselves are pinned by their fixed
//    weights), build the resulting Model, and run the SYMBOLIC join:
//    gj_less_in reasons about ALL grounding sigma consistent with the
//    order (including the `<=` no-op steps), which is what lets the AC
//    pair join where a single ground representative could not (F.1).
//
// Soundness: gj_less_in never OVER-approximates >=_C (P2), so no illegal
// rewrite ever fires; the enumeration covers every ground ordering of
// vars (ties via unify-recurse, strict via models) and every constant
// placement (P3); any budget overflow / OOM / unjoined case returns 0
// (KEEP).  Constants of different weight are never asserted equal (the
// model rejects such an unsatisfiable preorder), so the unsound
// "merge two constants" never happens.

// Collect distinct nullary-constant labels in `t`.
static u8 gj_collect_consts(Term t, u32 *labs, u32 *n, u32 cap) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 k = term_ctr_n(t);
      if (k == 0) {
        u32 lab = term_ext(t);
        for (u32 i = 0; i < *n; i++) if (labs[i] == lab) return 1;
        if (*n >= cap) return 0;
        labs[(*n)++] = lab;
        return 1;
      }
      for (u32 i = 0; i < k; i++)
        if (!gj_collect_consts(term_ctr_at(t, i), labs, n, cap)) return 0;
      return 1;
    }
    default: return 1;
  }
}

// Build a Model from a STRICT order on variables interleaved with the
// CP's constants.  `atoms[0..n_atoms)` lists the model atoms; `cls[i]`
// is atom i's size-rank (class).  Distinct VARIABLES must have distinct
// classes here (the caller guarantees a strict variable order).
// Returns 0 (reject this preorder as unsatisfiable) if it ties two
// constants of different weight, or orders two constants against their
// fixed-weight order -- no ground sigma realises such a C, so skipping
// it loses no coverage (it vacuously joins).
static u8 gj_build_model(GjModel *m, const GjAtom *atoms, u32 n_atoms,
                         const u32 *cls, const KboConfig *cfg) {
  for (u32 i = 0; i < n_atoms; i++) {
    if (!atoms[i].is_const) continue;
    for (u32 j = 0; j < n_atoms; j++) {
      if (!atoms[j].is_const) continue;
      u32 si = gj_const_size(cfg, atoms[i].id);
      u32 sj = gj_const_size(cfg, atoms[j].id);
      if (cls[i] < cls[j] && si > sj) return 0;
      if (cls[i] > cls[j] && si < sj) return 0;
      if (cls[i] == cls[j] && si != sj) return 0;
    }
  }
  m->n = n_atoms;
  for (u32 i = 0; i < n_atoms; i++) {
    m->atom[i]  = atoms[i];
    m->major[i] = cls[i];
  }
  return 1;
}

// Given a STRICT total order on the variables (var at atom-index i has
// rank var_rank[i]), interleave the constants among the variables in
// every position and run the symbolic join under each resulting Model.
// Returns 1 iff all join.
//
// Coarse placement: variable at rank r gets coarse slot 2r+1.  Each
// constant is independently placed into a coarse slot in [0, 2*nvars]:
//   even slot 2k  = strictly between var-rank-(k-1) and var-rank-k
//                   (slot 0 = below all vars, 2*nvars = above all);
//   odd slot 2k+1 = tied (equal size) with the variable at rank k.
// This covers every position of a constant relative to the strict
// variable chain.  Two or more constants landing in the SAME even slot
// (the same gap) are then SUB-ORDERED by their fixed weights: distinct
// weights -> distinct majors in weight order; equal weights -> tied
// (the true ground fact).  This closes the coverage gap that a single
// shared even slot would otherwise create (two different-weight
// constants in one gap must still be tested in their forced order).
// Constants tied with a variable (odd slot) keep that exact major (a
// ground term genuinely can have a variable image's size).
// gj_build_model rejects placements that contradict the constants'
// fixed weight order (an unsatisfiable preorder -> skipped, vacuously
// joinable, no coverage lost).
//
// Final majors are produced by sorting all atoms on the key
// (coarse_slot, weight_if_const_in_even_slot) and assigning a dense
// rank that increments only when the key strictly increases, so equal
// keys share a major (a tie) and strict keys get distinct majors.
static u8 gj_cover_consts(AtpState *s, Term lhs, Term rhs,
                          const GjAtom *atoms, u32 n_atoms,
                          const u32 *var_rank,  // var_rank[i] for var atoms
                          u32 n_vars_model,
                          const u32 *cp_vars, u32 n_cp_vars, u32 *budget) {
  const KboConfig *cfg = s->kbo;
  u32 const_idx[ATP_GJ_MAX_ATOMS];
  u32 nc = 0;
  for (u32 i = 0; i < n_atoms; i++) if (atoms[i].is_const) const_idx[nc++] = i;

  u32 grid = 2u * n_vars_model + 1u;     // coarse const slot range [0, grid)
  u32 place[ATP_GJ_MAX_ATOMS];
  for (u32 c = 0; c < nc; c++) place[c] = 0;

  for (;;) {
    if (*budget == 0) return 0;
    (*budget)--;

    // coarse slot per atom
    u32 coarse[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++)
      if (!atoms[i].is_const) coarse[i] = 2u * var_rank[i] + 1u;
    for (u32 c = 0; c < nc; c++) coarse[const_idx[c]] = place[c];

    // sub-order key: for a constant in an EVEN coarse slot, break ties
    // between same-slot constants by weight; everything else keeps a 0
    // secondary key (vars, and constants tied with a var, are pinned to
    // their slot).
    long long key2[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++) {
      if (atoms[i].is_const && (coarse[i] % 2u) == 0u)
        key2[i] = (long long)gj_const_size(cfg, atoms[i].id);
      else
        key2[i] = 0;
    }

    // dense-rank by (coarse, key2): sort indices, assign majors.
    u32 ord[ATP_GJ_MAX_ATOMS];
    for (u32 i = 0; i < n_atoms; i++) ord[i] = i;
    for (u32 i = 1; i < n_atoms; i++) {
      u32 k = ord[i]; int j = (int)i - 1;
      while (j >= 0 &&
             (coarse[ord[j]] > coarse[k] ||
              (coarse[ord[j]] == coarse[k] && key2[ord[j]] > key2[k]))) {
        ord[j + 1] = ord[j]; j--;
      }
      ord[j + 1] = k;
    }
    u32 cls[ATP_GJ_MAX_ATOMS];
    u32 cur = 0;
    for (u32 r = 0; r < n_atoms; r++) {
      if (r > 0) {
        u32 a = ord[r - 1], b = ord[r];
        if (coarse[a] != coarse[b] || key2[a] != key2[b]) cur++;
      }
      cls[ord[r]] = cur;
    }

    GjModel m;
    if (gj_build_model(&m, atoms, n_atoms, cls, cfg)) {
      if (!gj_joins_under(s, &m, lhs, rhs, cp_vars, n_cp_vars)) return 0;
    }

    if (nc == 0) return 1;
    u32 c = 0;
    for (; c < nc; c++) {
      if (++place[c] < grid) break;
      place[c] = 0;
    }
    if (c == nc) return 1;
  }
}

// Recursively cover all total preorders of the CP's variables.  Strategy
// (gj_spec.md C steps 1-5):
//   * Enumerate ordered set partitions of the variables.
//   * A partition with two variables in one class is an `=` case: unify
//     (substitute each tied variable to the class representative) and
//     RECURSE on the smaller CP.
//   * A strict (all-singleton) partition: interleave constants and run
//     the symbolic join (gj_cover_consts).
// Returns 1 iff every case joins.
static u8 gj_cover(AtpState *s, Term lhs, Term rhs,
                   const u32 *var_ids, u32 n_vars, u32 *budget) {
  // Re-collect the constants of the (current, possibly substituted) CP.
  u32 const_labs[ATP_GJ_MAX_ATOMS];
  u32 n_consts = 0;
  if (!gj_collect_consts(lhs, const_labs, &n_consts, ATP_GJ_MAX_ATOMS)) return 0;
  if (!gj_collect_consts(rhs, const_labs, &n_consts, ATP_GJ_MAX_ATOMS)) return 0;

  if (n_vars == 0) {
    // Ground CP (after all unifications): build a model over constants
    // only (their order is fixed by weight) and join.  No variable ranks.
    GjAtom atoms[ATP_GJ_MAX_ATOMS];
    u32 n_atoms = 0;
    for (u32 i = 0; i < n_consts; i++) {
      if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
      atoms[n_atoms].is_const = 1; atoms[n_atoms].id = const_labs[i]; n_atoms++;
    }
    u32 dummy_rank[ATP_GJ_MAX_ATOMS] = {0};
    return gj_cover_consts(s, lhs, rhs, atoms, n_atoms, dummy_rank, 0,
                           var_ids, n_vars, budget);
  }

  // Enumerate ordered set partitions of the variables via restricted-
  // growth string cls[] + a permutation of the classes.
  u32 cls[ATP_GJ_MAX_VARS];
  u32 perm[ATP_GJ_MAX_VARS];
  for (u32 i = 0; i < n_vars; i++) cls[i] = 0;

  for (;;) {
    u32 n_cls = 0;
    for (u32 i = 0; i < n_vars; i++) if (cls[i] + 1 > n_cls) n_cls = cls[i] + 1;

    for (u32 i = 0; i < n_cls; i++) perm[i] = i;
    for (;;) {
      if (*budget == 0) return 0;
      u32 ocls[ATP_GJ_MAX_VARS];
      for (u32 i = 0; i < n_vars; i++) ocls[i] = perm[cls[i]];

      if (n_cls < n_vars) {
        // Some class holds >=2 variables -> `=` case.  Unify each class
        // to its representative (lowest-id var in the class) and recurse.
        (*budget)--;
        RewriteSubst sub = {{0}};
        u32 rep_of_class[ATP_GJ_MAX_VARS];
        for (u32 c = 0; c < n_cls; c++) rep_of_class[c] = (u32)-1;
        // Representative = the smallest var id in the class.
        for (u32 i = 0; i < n_vars; i++) {
          u32 c = ocls[i];
          if (rep_of_class[c] == (u32)-1 || var_ids[i] < rep_of_class[c])
            rep_of_class[c] = var_ids[i];
        }
        u32 new_vars[ATP_GJ_MAX_VARS];
        u32 n_new = 0;
        u8 sub_ok = 1;
        for (u32 i = 0; i < n_vars; i++) {
          u32 rep = rep_of_class[ocls[i]];
          if (var_ids[i] != rep) {
            if (var_ids[i] >= REWRITE_MAX_VAR) { sub_ok = 0; break; }
            sub.bindings[var_ids[i]] = term_new_fvr(rep);
          }
        }
        // The reduced variable set = the representatives (one per class).
        for (u32 c = 0; c < n_cls && sub_ok; c++) {
          u8 seen = 0;
          for (u32 k = 0; k < n_new; k++) if (new_vars[k] == rep_of_class[c]) seen = 1;
          if (!seen) new_vars[n_new++] = rep_of_class[c];
        }
        if (sub_ok) {
          Term sl = thvm_subst_apply(lhs, &sub);
          Term sr = thvm_subst_apply(rhs, &sub);
          if (!gj_cover(s, sl, sr, new_vars, n_new, budget)) return 0;
        } else {
          return 0;   // can't form the substitution -> KEEP (sound)
        }
      } else {
        // Strict order on the variables: var i sits at rank ocls[i].
        // Build the atom list (vars + consts) and interleave constants.
        GjAtom atoms[ATP_GJ_MAX_ATOMS];
        u32 var_rank[ATP_GJ_MAX_ATOMS];
        u32 n_atoms = 0;
        for (u32 i = 0; i < n_vars; i++) {
          if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
          atoms[n_atoms].is_const = 0; atoms[n_atoms].id = var_ids[i];
          var_rank[n_atoms] = ocls[i];
          n_atoms++;
        }
        for (u32 i = 0; i < n_consts; i++) {
          if (n_atoms >= ATP_GJ_MAX_ATOMS) return 0;
          atoms[n_atoms].is_const = 1; atoms[n_atoms].id = const_labs[i];
          var_rank[n_atoms] = 0;
          n_atoms++;
        }
        if (!gj_cover_consts(s, lhs, rhs, atoms, n_atoms, var_rank, n_vars,
                             var_ids, n_vars, budget))
          return 0;
      }

      // next permutation of classes
      if (n_cls < 2) break;
      u32 a = n_cls - 1;
      while (a > 0 && perm[a - 1] >= perm[a]) a--;
      if (a == 0) break;
      u32 b = n_cls - 1;
      while (perm[b] <= perm[a - 1]) b--;
      u32 tmp = perm[a - 1]; perm[a - 1] = perm[b]; perm[b] = tmp;
      u32 lo = a, hi = n_cls - 1;
      while (lo < hi) { tmp = perm[lo]; perm[lo] = perm[hi]; perm[hi] = tmp; lo++; hi--; }
    }

    // next restricted-growth partition
    u32 i = n_vars;
    while (i > 0) {
      i--;
      u32 max_prefix = 0;
      for (u32 j = 0; j < i; j++) if (cls[j] + 1 > max_prefix) max_prefix = cls[j] + 1;
      if (cls[i] < max_prefix) {
        cls[i]++;
        for (u32 j = i + 1; j < n_vars; j++) cls[j] = 0;
        break;
      }
      if (i == 0) return 1;              // exhausted -> all cases joined
      cls[i] = 0;
    }
  }
}

// Top-level: return 1 iff (lhs, rhs) is PROVABLY ground-joinable
// (would-DELETE), 0 otherwise (would-KEEP).  Sound (no false DELETE):
// gj_less_in never over-approximates >=_C (P2), the recursion + strict-
// order/constant-interleave enumeration covers every ground ordering
// (P3), and any uncertainty / budget / overflow returns 0 (KEEP).
static int atp_cp_ground_joinable(AtpState *s, Term lhs, Term rhs) {
  if (s == NULL) return 0;
  if (s->kbo == NULL && s->lpo == NULL) return 0;   // no ordering -> keep
  if (s->lpo != NULL) return 0;                     // LPO: GJ off (KBO-only)

  u32 var_ids[ATP_GJ_MAX_VARS];
  u32 n_vars = 0;
  if (!atp_gj_collect_vars(lhs, var_ids, &n_vars, ATP_GJ_MAX_VARS)) return 0;
  if (!atp_gj_collect_vars(rhs, var_ids, &n_vars, ATP_GJ_MAX_VARS)) return 0;

  u32 budget = ATP_GJ_BRANCH_CAP;
  return gj_cover(s, lhs, rhs, var_ids, n_vars, &budget) ? 1 : 0;
}

#endif  // ATP_CP_GROUND_JOIN

// Helper: push a batch of CPs onto the queue with TRACE_CP entries
// pointing at the two source rules' trace indices.  Drops overflow
// silently.  Filters and counters fire on each CP:
//   - 7.1:  trivially-joinable under R         -> drop, tick `n_cps_dropped_joinable`
//   - 7.2b: source-rule-disjoint connected     -> tick `n_cps_dropped_connected`
//                                                 (counter only, -DATP_CP_DIAG)
//   - 7.3a: rule-subsumed by some `(l, r) ∈ R` -> tick `n_cps_dropped_rule_subsumed`
//                                                 (counter only, -DATP_CP_DIAG)
//   - 7.3b: queue-subsumed by some queued CP   -> drop, tick `n_cps_dropped_queue_subsumed`
//                                                 (real filter, orthogonal to 7.1)
// When built with -DATP_CP_CLASSIFY, every CP is additionally run
// through the Waldmeister classifier (atp_classify_cp).  Its drop
// decision ticks `n_cps_dropped_classified` unconditionally for
// measurement.  As a filter the classifier is a strict subset of
// 7.1 (the soundness guard is rule subsumption, and rule subsumption
// implies joinability), so it never removes a CP that 7.1 keeps --
// the saturation status is identical with the flag on or off.  A
// surviving CP is pushed with the classifier's rescaled priority.
// `rule_a`/`rule_b` are the rule indices that birthed this CP batch
// (passed through to the connectedness check); `parent_a`/`parent_b`
// are their trace indices.  Returns count of CPs pushed.
//
// 7e lever 1: the 7.2b connectedness and 7.3a rule-subsumption checks
// are COUNTER-ONLY -- their verdicts feed `n_cps_dropped_connected` /
// `n_cps_dropped_rule_subsumed` and never drop a CP (only `joinable`
// and `q_subsmd` `continue`).  Each is two full `atp_rewrite_normalize`
// passes (plus a malloc) / an O(n_rules) two-sided match scan -- about
// half the per-CP filter cost.  They run only under -DATP_CP_DIAG;
// the default hot loop skips them.  Skipping is behavior-identical:
// same CPs queued, same proof, identical step/rule/cps trajectory.
static u32 atp_push_cps_traced(AtpState *s, const CriticalPair *cps,
                               u32 ncps, u32 parent_a, u32 parent_b,
                               u32 rule_a, u32 rule_b) {
  u32 pushed = 0;
#if defined(ATP_CP_GRAPH) && defined(ATP_MATCH_STATS)
  clock_t mm_t0 = clock();
#endif
#if !defined(ATP_CP_DIAG) && !defined(ATP_CP_CLASSIFY)
  (void)rule_a;
  (void)rule_b;
#endif
  for (u32 i = 0; i < ncps; i++) {
    Term cp_lhs = cps[i].lhs;
    Term cp_rhs = cps[i].rhs;
    // Snapshot the RAW superposition result -- the un-reduced
    // `(σ(l_i[p←r_j]), σ(r_i))` -- var-normalized for clean
    // alpha-canonical ids.  The trace records this (not the reduced
    // CP queued below): a Waldmeister-PCL CriticalPairLemma's
    // Statement is the raw overlap, which the proof verifier
    // re-derives from the two parent rules at the recorded position.
    Term raw_lhs = cp_lhs;
    Term raw_rhs = cp_rhs;
#ifdef ATP_VAR_NORM
    thvm_normalize_vars(&raw_lhs, &raw_rhs);
#endif
    // Reduce the CP w.r.t. R before it lands in the queue: standard
    // completion adds the NORMALIZED critical pair.  atp_cp_trivially_-
    // joinable writes the two normal forms back through cp_lhs/cp_rhs
    // and returns whether they collapsed (joinable -> drop).  A reduced
    // CP is dramatically smaller than the raw overlap, so the KBO
    // priority, the subsumption query, the index insert, and every
    // later retrieval against it all stay cheap.
    u8 joinable = atp_cp_trivially_joinable(s, &cp_lhs, &cp_rhs);
#ifdef ATP_VAR_NORM
    // 7c: canonically renumber the CP's variables -- AFTER reduction,
    // since rewriting can drop variables, so the dense [0, k) set is
    // computed on the form actually queued.  The CP enumerator bakes
    // CP_RENAME_OFFSET into the stored term, carrying ids past
    // REWRITE_MAX_VAR where the matcher goes dead; renumbering keeps
    // every stored var matchable AND makes alpha-equivalent CPs
    // byte-identical so the subsumption filters below actually fire.
    thvm_normalize_vars(&cp_lhs, &cp_rhs);
#endif
    // 8e: under -DATP_CP_GRAPH this runs ONE thvm_match_multi
    // traversal of cp_graph; off the flag it is the array scan.
    u8 q_subsmd    = atp_cp_queue_subsumed(s, cp_lhs, cp_rhs);
#ifdef ATP_CP_DIAG
    if (atp_cp_source_disjoint_connected(s, cp_lhs, cp_rhs,
                                         rule_a, rule_b)) {
      s->n_cps_dropped_connected++;
    }
    if (atp_cp_rule_subsumed(s, cp_lhs, cp_rhs)) {
      s->n_cps_dropped_rule_subsumed++;
    }
#endif
#ifdef ATP_CP_CLASSIFY
    u32 clas_pri = atp_cp_priority(s, cp_lhs, cp_rhs);
    u8  classified_drop = atp_classify_cp(s, cp_lhs, cp_rhs,
                                          rule_a, rule_b, &clas_pri);
    if (classified_drop) s->n_cps_dropped_classified++;
#endif
#ifdef ATP_CP_GROUND_JOIN
    // Ground-joinability redundancy.  The check is expensive (ordered-
    // set-partition enumeration + ground normalization per CP), so it
    // runs ONLY when the run opted in via s->use_ground_join -- a single
    // branch test otherwise, free for the default/shipped path.  When
    // opted in, a ground-joinable CP is counted and dropped; deletion is
    // sound (a ground-joinable CP is redundant -- Martin-Nipkow / Twee).
    if (s->use_ground_join && atp_cp_ground_joinable(s, cp_lhs, cp_rhs)) {
      s->n_cps_ground_joinable++;
      continue;
    }
#endif
    // Bachmair-Dershowitz connectedness (Twee section 6.2): drop a CP
    // whose sides join through terms strictly below the peak.  Gated on
    // use_connectedness so the default engine is byte-identical.  The
    // peak rides on the CriticalPair; the reduced cp_lhs/cp_rhs are the
    // CP sides.  Run before the trivial-join check so it can subsume it.
    if (s->use_connectedness && cps[i].peak != 0 &&
        atp_cp_connected_below_peak(s, cp_lhs, cp_rhs, cps[i].peak)) {
      s->n_cps_dropped_connected_below_peak++;
      continue;
    }
    if (joinable) {
      s->n_cps_dropped_joinable++;
      continue;
    }
    if (q_subsmd) {
      s->n_cps_dropped_queue_subsumed++;
      continue;
    }
#ifdef ATP_CP_CLASSIFY
    if (classified_drop) {
      continue;
    }
    u32 t = atp_trace_push_cp(s, parent_a, parent_b, raw_lhs, raw_rhs,
                              cps[i].pos, cps[i].pos_len);
    atp_cp_heap_push_pri(s, cp_lhs, cp_rhs, t, clas_pri);
    pushed++;
#else
    u32 t = atp_trace_push_cp(s, parent_a, parent_b, raw_lhs, raw_rhs,
                              cps[i].pos, cps[i].pos_len);
    atp_cp_heap_push(s, cp_lhs, cp_rhs, t);
    pushed++;
#endif
  }
#if defined(ATP_CP_GRAPH) && defined(ATP_MATCH_STATS)
  g_atp_mm_secs += (double)(clock() - mm_t0) / CLOCKS_PER_SEC;
#endif
  return pushed;
}

// 8.1e-i: C-direct critical-pair enumerator -- the path
// `thvm_atp_generate_cps` takes when `s->use_ic_cp_gen == 0`
// (the default).  Bulk of the work happens in
// `thvm_critical_pairs_range`; this function just plumbs the
// (i, j) iteration and trace bookkeeping.
// Overlap rule j into rule i, emitting every CP face the active
// completion variant requires.  Default (use_unfailing_cp == 0): the
// single (i-face = li, j = lj->rj) overlap -- byte-for-byte the prior
// thvm_critical_pairs_range(i,i+1,j,j+1) behaviour.  Unfailing
// (use_unfailing_cp == 1): when rule j is an unorientable equation,
// ALSO overlap its rhs-face (rj->lj); when rule i is unorientable, ALSO
// walk the positions of its rhs-face (so a redex inside ri is found,
// the CP's other component then being li).  Both-faces superposition is
// what unfailing completion needs to be COMPLETE on incomparable
// equations (Bachmair-Dershowitz-Plaisted; Waldmeister's default mode).
static u32 atp_overlap_ij(AtpState *s, u32 i, u32 j,
                          CriticalPair *buf, u32 cap) {
  u32 cnt = 0;
  // Variables of j must be renamed apart from i's -- the SAME offset
  // thvm_critical_pairs_range uses internally (REWRITE_MAX_VAR / 2).
  Term lj = thvm_rename_vars(s->lhs[j], REWRITE_MAX_VAR / 2);
  Term rj = thvm_rename_vars(s->rhs[j], REWRITE_MAX_VAR / 2);
  Term li = s->lhs[i], ri = s->rhs[i];

  // Face combinations.  i_face in {li (always), ri (if i unorientable)};
  // j as the from->to pair {(lj,rj) always, (rj,lj) if j unorientable}.
  u8 i_un = s->use_unfailing_cp && !s->r_orient[i];
  u8 j_un = s->use_unfailing_cp && !s->r_orient[j];

  // (i-face li) x (j: lj->rj)  -- the standard overlap.
  cnt = thvm_critical_pairs_pair(li, ri, lj, rj, buf, cap, cnt);
  if (j_un) {
    // (i-face li) x (j: rj->lj)
    cnt = thvm_critical_pairs_pair(li, ri, rj, lj, buf, cap, cnt);
  }
  if (i_un) {
    // (i-face ri) x (j: lj->rj)
    cnt = thvm_critical_pairs_pair(ri, li, lj, rj, buf, cap, cnt);
    if (j_un) {
      // (i-face ri) x (j: rj->lj)
      cnt = thvm_critical_pairs_pair(ri, li, rj, lj, buf, cap, cnt);
    }
  }
  return cnt;
}

// Run one overlap pair and push its CPs (the body shared by the indexed
// and unindexed generator loops).
static u32 atp_gen_one(AtpState *s, u32 i, u32 j, CriticalPair *buf) {
  u32 nbuf   = atp_overlap_ij(s, i, j, buf, ATP_CP_BATCH);
  u32 pushed = atp_push_cps_traced(s, buf, nbuf,
                                   s->r_trace[i], s->r_trace[j], i, j);
  // A single saturation step can out-allocate a whole GC semi-space in
  // raw critical-pair + normalisation scratch.  Collect between overlap
  // pairs -- `buf` is fully processed here, so no in-flight CP needs
  // rooting -- to bound the transient working set instead of crashing.
  if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);
  return pushed;
}

static u32 thvm_atp_generate_cps_c(AtpState *s, AtpAddedRange added) {
  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  CriticalPair buf[ATP_CP_BATCH];
  u32 pushed = 0;

#ifdef ATP_RULE_INDEX
  // Indexed overlap-partner retrieval (opt-in, byte-identical CP set).
  // Rebuild both overlap indices over the post-add rule set, then for
  // each new rule overlap only the candidate partners the index returns.
  // On a candidate-buffer / subject-depth OVERFLOW the call falls back to
  // the exact n_rules scan for that rule, so the CP set is preserved even
  // when the index over-runs its scratch.
  if (s->use_cp_index) {
    if (s->cp_index == NULL)    s->cp_index    = atp_ri_new();
    if (s->cp_subindex == NULL) s->cp_subindex = atp_ri_new();
    if (s->cp_index->n_rules_built != n) {
      atp_cp_index_rebuild(s);          // grows g_atp_cp_seen to cover n
      atp_cp_subindex_rebuild(s);
    }
    // (new x all_R): for each new rule i, candidates j are rules whose lj
    // unifies with a non-var subterm of li -- the whole-LHS index query.
    for (u32 i = first; i < last; i++) {
      u32 nc = atp_cp_index_collect(s, s->lhs[i]);
      if (g_atp_cp_overflow) {
        for (u32 j = 0; j < n; j++) pushed += atp_gen_one(s, i, j, buf);
      } else {
        atp_cp_cand_sort();
        for (u32 c = 0; c < nc; c++) {
          pushed += atp_gen_one(s, i, g_atp_cp_cand[c], buf);
        }
      }
    }
    // (old x new): for each new rule j, candidates i are OLD rules whose
    // li has a non-var subterm unifiable with the new lj -- the subterm
    // index query.  Only i < first (old) are this loop's partners.
    for (u32 j = first; j < last; j++) {
      u32 nc = atp_cp_subindex_collect(s, s->lhs[j]);
      if (g_atp_cp_overflow) {
        for (u32 i = 0; i < first; i++) pushed += atp_gen_one(s, i, j, buf);
      } else {
        atp_cp_cand_sort();
        for (u32 c = 0; c < nc; c++) {
          u32 i = g_atp_cp_cand[c];
          if (i < first) pushed += atp_gen_one(s, i, j, buf);
        }
      }
    }
    return pushed;
  }
#endif

  // (new x all_R): the new rule is i (outer), j ranges over all
  // existing rules (including the new ones for new x new self-overlap).
  for (u32 i = first; i < last; i++) {
    for (u32 j = 0; j < n; j++) {
      pushed += atp_gen_one(s, i, j, buf);
    }
  }

  // (old x new): old rule on the outside, new rule fed as inner.
  for (u32 i = 0; i < first; i++) {
    for (u32 j = first; j < last; j++) {
      pushed += atp_gen_one(s, i, j, buf);
    }
  }

  return pushed;
}

// 8.1e-ii: invoke `prim_unify_apply3` via APP-PRI evaluation.
// Builds the saturated chain APP(APP(APP(PRI(id), s), t), target)
// and reduces it via `wnf`.  Returns either σ(target) on success
// or ERA on unify failure -- the same shape as the underlying
// primitive returns.
static Term ic_unify_apply3(Term s, Term t, Term target) {
  u64 l1 = heap_alloc(2);
  heap_set(l1 + 0, term_new_pri(ATP_PRIM_UNIFY_APPLY3));
  heap_set(l1 + 1, s);
  Term step1 = term_new(0, TAG_APP, 0, l1);

  u64 l2 = heap_alloc(2);
  heap_set(l2 + 0, step1);
  heap_set(l2 + 1, t);
  Term step2 = term_new(0, TAG_APP, 0, l2);

  u64 l3 = heap_alloc(2);
  heap_set(l3 + 0, step2);
  heap_set(l3 + 1, target);
  Term step3 = term_new(0, TAG_APP, 0, l3);

  return wnf(step3);
}

// 8.1e-ii: per-position visitor that mirrors `cp_visit` (in
// `src/cp/_.c`) but routes the unify+apply step through the
// TAG_PRI machinery via `ic_unify_apply3`.  Same outputs as the
// C path; the IC contribution is the per-position unify call
// going through APP-PRI evaluation.
typedef struct {
  Term         li, ri;
  Term         lj, rj;
  CriticalPair *out;
  u32           cap;
  u32           count;
} CpCtxIc;

static u32 cp_visit_ic(const u32 *p, u32 p_len, void *raw) {
  CpCtxIc *ctx = (CpCtxIc *)raw;
  if (ctx->count >= ctx->cap) return ctx->count;

  Term sub = cp_subterm_at(ctx->li, p, p_len);
  if (sub == 0) return ctx->count;
  if (term_tag(sub) == TAG_FVR) return ctx->count;

  // Build replaced = li[p ← rj] (still in C; `cp_replace_at` is
  // a small pure helper).
  Term replaced = cp_replace_at(ctx->li, p, p_len, ctx->rj);

  // IC-routed unify+apply.  Two PRI calls because
  // prim_unify_apply3 takes a single target; recomputing σ each
  // time is wasteful but correct.  8.1e-iii will measure the
  // overhead.
  Term cp_lhs = ic_unify_apply3(sub, ctx->lj, replaced);
  if (term_tag(cp_lhs) == TAG_ERA) return ctx->count;
  Term cp_rhs = ic_unify_apply3(sub, ctx->lj, ctx->ri);
  if (term_tag(cp_rhs) == TAG_ERA) return ctx->count;
  Term cp_peak = ic_unify_apply3(sub, ctx->lj, ctx->li);
  if (term_tag(cp_peak) == TAG_ERA) return ctx->count;

  CriticalPair *slot = &ctx->out[ctx->count];
  slot->lhs = cp_lhs;
  slot->rhs = cp_rhs;
  slot->peak = cp_peak;
  slot->pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len; d++) slot->pos[d] = (u8)p[d];
  ctx->count++;
  return ctx->count;
}

// 8.1e-ii: IC-routed CP enumeration.  Same iteration pattern as
// the C path -- (i, j) cross-product over the new-vs-old rule
// rectangles -- but the per-position unify+apply flows through
// the TAG_PRI machinery via `cp_visit_ic` / `ic_unify_apply3`.
// Output CPs are structurally identical to the C path (verified
// by parity tests in `tests/test_atp.c`).
static u32 thvm_atp_generate_cps_ic(AtpState *s, AtpAddedRange added) {
  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  CriticalPair buf[ATP_CP_BATCH];
  u32 pushed = 0;
  CpCtxIc ctx;
  u32 path[CP_MAX_DEPTH];

  for (u32 i = first; i < last; i++) {
    for (u32 j = 0; j < n; j++) {
      ctx.li    = s->lhs[i];
      ctx.ri    = s->rhs[i];
      ctx.lj    = thvm_rename_vars(s->lhs[j], CP_RENAME_OFFSET);
      ctx.rj    = thvm_rename_vars(s->rhs[j], CP_RENAME_OFFSET);
      ctx.out   = buf;
      ctx.cap   = ATP_CP_BATCH;
      ctx.count = 0;
      (void)cp_walk_positions(ctx.li, path, 0, CP_MAX_DEPTH,
                              cp_visit_ic, &ctx, 0);
      pushed += atp_push_cps_traced(s, buf, ctx.count,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
      // Bound the per-step transient: a step can out-allocate a GC
      // semi-space in raw-CP + normalisation scratch.  `buf` is fully
      // processed here, so a collection between overlap pairs is safe.
      if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);
    }
  }

  for (u32 i = 0; i < first; i++) {
    for (u32 j = first; j < last; j++) {
      ctx.li    = s->lhs[i];
      ctx.ri    = s->rhs[i];
      ctx.lj    = thvm_rename_vars(s->lhs[j], CP_RENAME_OFFSET);
      ctx.rj    = thvm_rename_vars(s->rhs[j], CP_RENAME_OFFSET);
      ctx.out   = buf;
      ctx.cap   = ATP_CP_BATCH;
      ctx.count = 0;
      (void)cp_walk_positions(ctx.li, path, 0, CP_MAX_DEPTH,
                              cp_visit_ic, &ctx, 0);
      pushed += atp_push_cps_traced(s, buf, ctx.count,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
      if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);   // see above
    }
  }

  return pushed;
}

fn u32 thvm_atp_generate_cps(AtpState *s, AtpAddedRange added) {
  if (s == NULL || added.count == 0) return 0;
  if (s->use_ic_cp_gen) return thvm_atp_generate_cps_ic(s, added);
  return thvm_atp_generate_cps_c(s, added);
}

// Orient via KBO and push the rule(s).  See header comment for the
// dispatch table.  Atomic: if the unfailing fallback can't fit both
// orientations, neither is added.
fn AtpAddedRange thvm_atp_orient_and_add(AtpState *s, Term lhs, Term rhs) {
  AtpAddedRange r = {0, 0};
  if (s == NULL) return r;

  // 8.5c: dispatch between KBO and LPO based on which config is
  // attached.  See `atp_compare`.
  KboCmp c = atp_compare(s, lhs, rhs);
  switch (c) {
    case KBO_GT: {
      u32 idx = s->n_rules;
      if (atp_push_rule(s, lhs, rhs)) { r.first = idx; r.count = 1; }
      return r;
    }
    case KBO_LT: {
      u32 idx = s->n_rules;
      if (atp_push_rule(s, rhs, lhs)) { r.first = idx; r.count = 1; }
      return r;
    }
    case KBO_UN: {
#ifdef ATP_ORDERED_REWRITE
      // 9c-foundation: ordered rewriting drives an unorientable
      // equation in whichever direction decreases, so store it ONCE
      // (no looping u->v / v->u pair, no doubled CP generation).
      u32 idx = s->n_rules;
      if (atp_push_rule(s, lhs, rhs)) { r.first = idx; r.count = 1; }
      return r;
#else
      // Unfailing fallback: reserve 2 slots up front so the pair is
      // added atomically (the array is growable, so this can't fail).
      // 7c: atp_push_rule may reject either orientation as a
      // duplicate -- r must span exactly the rules actually stored,
      // so generate_cps overlaps only the freshly-added range.
      atp_ensure_rule_cap(s, s->n_rules + 2);
      u32 idx = s->n_rules;
      u32 added = 0;
      added += atp_push_rule(s, lhs, rhs) ? 1u : 0u;
      added += atp_push_rule(s, rhs, lhs) ? 1u : 0u;
      r.first = idx;
      r.count = added;
      return r;
#endif
    }
    case KBO_EQ:
    default:
      return r;
  }
}

// === 8.10b: top-K CP peek ==========================================
//
// Copies the top K cheapest CPs' (lhs, rhs) into the caller's
// buffers WITHOUT modifying the queue.  "Cheapest" is the same
// (cp_pri, cp_seq) order the heap pops in, so peek[0] always
// equals the next `thvm_atp_select_cp` result.
//
// Caller-side use case: see top-K candidates before deciding
// which (if any) to commit; useful for branching CP selectors,
// multi-CP batch heuristics, lookahead.

// (pri, seq, idx) triple sorted to realize the peek order.
typedef struct { u32 pri; u32 seq; u32 idx; } AtpPeekEnt;
static int atp_peek_cmp(const void *a, const void *b) {
  const AtpPeekEnt *x = (const AtpPeekEnt *)a;
  const AtpPeekEnt *y = (const AtpPeekEnt *)b;
  if (x->pri != y->pri) return (x->pri < y->pri) ? -1 : 1;
  if (x->seq != y->seq) return (x->seq < y->seq) ? -1 : 1;
  return 0;
}

fn u32 thvm_atp_peek_top_k(AtpState *s, u32 k,
                           Term *out_lhs, Term *out_rhs) {
  if (s == NULL || s->n_cps == 0) return 0;
  if (k > s->n_cps) k = s->n_cps;
  if (k == 0) return 0;

  // The heap array is ordered by the heap invariant, not fully
  // sorted -- so copy the (pri, seq, idx) triples and sort them
  // by the queue's selection key to read off the top K.
  AtpPeekEnt *ent = (AtpPeekEnt *)malloc((size_t)s->n_cps * sizeof(AtpPeekEnt));
  if (ent == NULL) return 0;
  for (u32 i = 0; i < s->n_cps; i++) {
    ent[i].pri = s->cp_pri[i];
    ent[i].seq = s->cp_seq[i];
    ent[i].idx = i;
  }
  qsort(ent, s->n_cps, sizeof(AtpPeekEnt), atp_peek_cmp);
  for (u32 i = 0; i < k; i++) {
    acp_unpack(s->cp_packed[ent[i].idx], &out_lhs[i], &out_rhs[i]);
  }
  free(ent);
  return k;
}

// === 8.9b: narrowing primitives ====================================

// One-shot narrow visitor: tries each rule at the current position,
// commits the first successful unification.  `success` flag stops
// the walk after a hit.
typedef struct {
  AtpState     *s;
  Term          side;        // currently being narrowed
  Term          other;       // the other side (sigma is applied here too)
  Term         *out_side;
  Term         *out_other;
  RewriteSubst *witness;
  u8            success;
} NarrowCtx;

static u32 narrow_visit(const u32 *p, u32 p_len, void *raw) {
  NarrowCtx *ctx = (NarrowCtx *)raw;
  if (ctx->success) return 0;

  Term sub = cp_subterm_at(ctx->side, p, p_len);
  if (sub == 0 || term_tag(sub) == TAG_FVR) return 0;

  for (u32 k = 0; k < ctx->s->n_rules; k++) {
    Term lj = thvm_rename_vars(ctx->s->lhs[k], CP_RENAME_OFFSET);
    Term rj = thvm_rename_vars(ctx->s->rhs[k], CP_RENAME_OFFSET);
    RewriteSubst subst = {{0}};
    if (!thvm_unify(sub, lj, &subst)) continue;

    // Narrow: replace the subterm at p with the rule's RHS,
    // then sigma-apply across both sides.
    Term replaced  = cp_replace_at(ctx->side, p, p_len, rj);
    *ctx->out_side  = thvm_unify_apply(replaced, &subst);
    *ctx->out_other = thvm_unify_apply(ctx->other, &subst);

    // Accumulate sigma into witness.  No composition step
    // needed: previous-step sigmas have already been applied
    // to side/other before this call, so each new binding lives
    // in the post-sigma universe.
    for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
      if (subst.bindings[i] != 0) {
        ctx->witness->bindings[i] = subst.bindings[i];
      }
    }
    ctx->success = 1;
    return 0;
  }
  return 0;
}

fn u8 thvm_atp_narrow_step(AtpState *s, Term lhs, Term rhs,
                           Term *out_lhs, Term *out_rhs,
                           RewriteSubst *witness) {
  if (s == NULL || s->n_rules == 0) return 0;
  if (out_lhs == NULL || out_rhs == NULL || witness == NULL) return 0;

  NarrowCtx ctx;
  ctx.s        = s;
  ctx.witness  = witness;
  ctx.success  = 0;

  u32 path[CP_MAX_DEPTH];

  // Try narrowing on lhs first.
  ctx.side       = lhs;
  ctx.other      = rhs;
  ctx.out_side   = out_lhs;
  ctx.out_other  = out_rhs;
  cp_walk_positions(lhs, path, 0, CP_MAX_DEPTH, narrow_visit, &ctx, 0);
  if (ctx.success) return 1;

  // Then rhs.
  ctx.side       = rhs;
  ctx.other      = lhs;
  ctx.out_side   = out_rhs;
  ctx.out_other  = out_lhs;
  cp_walk_positions(rhs, path, 0, CP_MAX_DEPTH, narrow_visit, &ctx, 0);
  return ctx.success;
}

fn Term thvm_atp_get_witness(const AtpState *s, u32 var_id) {
  if (s == NULL || var_id >= REWRITE_MAX_VAR) return 0;
  return s->witness_subst.bindings[var_id];
}

// === Stage 9.1b: bounded DFS multi-witness narrowing ================
// Enumerates up to N witnesses by recursively trying every (position,
// rule) choice at each node.  Stateless w.r.t. AtpState; populates
// the caller's RewriteSubst array directly.

typedef struct {
  AtpState     *s;
  RewriteSubst *witnesses;
  u32           max_witnesses;
  u32           found;
  u32           max_depth;
} NarrowAllCtx;

typedef struct {
  NarrowAllCtx *ctx;
  Term          side;       // currently being narrowed
  Term          other;      // the other side (sigma is applied here too)
  RewriteSubst  acc;        // accumulator at this DFS frame
  u32           depth;
  u8            narrowing_lhs;  // 1 = narrow side==lhs; 0 = narrow side==rhs
} NarrowAllVisitor;

static void narrow_all_dfs(NarrowAllCtx *ctx,
                           Term lhs, Term rhs,
                           const RewriteSubst *acc,
                           u32 depth);

static u32 narrow_all_visit(const u32 *p, u32 p_len, void *raw) {
  NarrowAllVisitor *v = (NarrowAllVisitor *)raw;
  if (v->ctx->found >= v->ctx->max_witnesses) return 0;

  Term sub = cp_subterm_at(v->side, p, p_len);
  if (sub == 0 || term_tag(sub) == TAG_FVR) return 0;

  for (u32 k = 0; k < v->ctx->s->n_rules; k++) {
    if (v->ctx->found >= v->ctx->max_witnesses) return 0;

    Term lj = thvm_rename_vars(v->ctx->s->lhs[k], CP_RENAME_OFFSET);
    Term rj = thvm_rename_vars(v->ctx->s->rhs[k], CP_RENAME_OFFSET);
    RewriteSubst subst = {{0}};
    if (!thvm_unify(sub, lj, &subst)) continue;

    Term replaced  = cp_replace_at(v->side, p, p_len, rj);
    Term new_side  = thvm_unify_apply(replaced,  &subst);
    Term new_other = thvm_unify_apply(v->other,  &subst);

    // Compose sigma into a fresh accumulator copy: each branch sees
    // its own bindings, siblings stay independent.
    RewriteSubst new_acc = v->acc;
    for (u32 i = 0; i < REWRITE_MAX_VAR; i++) {
      if (subst.bindings[i] != 0) new_acc.bindings[i] = subst.bindings[i];
    }

    if (v->narrowing_lhs)
      narrow_all_dfs(v->ctx, new_side, new_other, &new_acc, v->depth + 1);
    else
      narrow_all_dfs(v->ctx, new_other, new_side, &new_acc, v->depth + 1);
  }
  return 0;
}

static void narrow_all_dfs(NarrowAllCtx *ctx,
                           Term lhs, Term rhs,
                           const RewriteSubst *acc,
                           u32 depth) {
  if (ctx->found >= ctx->max_witnesses) return;
  if (kbo_eq(lhs, rhs)) {
    ctx->witnesses[ctx->found++] = *acc;
    return;
  }
  if (depth >= ctx->max_depth) return;

  u32 path[CP_MAX_DEPTH];
  NarrowAllVisitor v;
  v.ctx   = ctx;
  v.acc   = *acc;
  v.depth = depth;

  // Narrow on lhs first.
  v.side          = lhs;
  v.other         = rhs;
  v.narrowing_lhs = 1;
  cp_walk_positions(lhs, path, 0, CP_MAX_DEPTH, narrow_all_visit, &v, 0);
  if (ctx->found >= ctx->max_witnesses) return;

  // Then rhs.
  v.side          = rhs;
  v.other         = lhs;
  v.narrowing_lhs = 0;
  cp_walk_positions(rhs, path, 0, CP_MAX_DEPTH, narrow_all_visit, &v, 0);
}

fn u32 thvm_atp_narrow_all(AtpState *s,
                           Term lhs, Term rhs,
                           u32 max_depth, u32 max_witnesses,
                           RewriteSubst *witnesses) {
  if (s == NULL || witnesses == NULL || max_witnesses == 0) return 0;

  NarrowAllCtx ctx;
  ctx.s             = s;
  ctx.witnesses     = witnesses;
  ctx.max_witnesses = max_witnesses;
  ctx.found         = 0;
  ctx.max_depth     = max_depth;

  RewriteSubst empty = {{0}};
  narrow_all_dfs(&ctx, lhs, rhs, &empty, 0);
  return ctx.found;
}
