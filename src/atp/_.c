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
#endif

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
  if (nc == NULL || nt == NULL || np == NULL || nq == NULL) {
    fprintf(stderr, "atp_ensure_cp_cap: realloc to %u CPs failed\n", cap);
    exit(1);
  }
  s->cp_packed = nc; s->cp_trace = nt;
  s->cp_pri = np; s->cp_seq = nq;
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
// this is unreachable in practice but keeps unpack total).
static void acp_pack_term(Term t, u8 **pp) {
  switch (term_tag(t)) {
    case TAG_CTR: {
      u32 n = term_ctr_n(t);
      *(*pp)++ = (u8)'C';
      acp_put_varint(pp, term_ext(t));
      acp_put_varint(pp, n);
      for (u32 i = 0; i < n; i++) acp_pack_term(term_ctr_at(t, i), pp);
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
// `*out_len` receives its length.  The two terms pack back to back --
// the preorder records are self-delimiting via arity, so acp_unpack
// reads lhs then rhs with no separator.
static u8 *acp_pack(Term lhs, Term rhs, u32 *out_len) {
  u32 bound = acp_packed_bound(lhs) + acp_packed_bound(rhs);
  u8 *buf = (u8 *)malloc(bound);
  if (buf == NULL) { fprintf(stderr, "acp_pack: OOM\n"); exit(1); }
  u8 *p = buf;
  acp_pack_term(lhs, &p);
  acp_pack_term(rhs, &p);
  if (out_len != NULL) *out_len = (u32)(p - buf);
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
  u8  *packed = acp_pack(lhs, rhs, &len);
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
  s->cp_packed[i] = acp_pack(lhs, rhs, NULL);
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
      s->cp_packed[w] = acp_pack(l, r, NULL);
      touched = 1;
    } else if (w != i) {
      // Unchanged -- compact the existing buffer down to slot w.
      s->cp_packed[w] = s->cp_packed[i];
      s->cp_packed[i] = NULL;
    }
    s->cp_trace[w] = s->cp_trace[i];
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
};
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
  u32 rlen  = atp_symbol_count(repl);            // repl's preorder length
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
  // Flatten repl into the freed [redex_pos, redex_pos+rlen) region.
  u32 p = redex_pos;
  atp_ri_flatten(repl, flat, subsz, flatsym, folded, cap, &p);
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

#endif // ATP_RULE_INDEX

fn AtpState *thvm_atp_init(const KboConfig *cfg, u32 step_cap) {
  AtpState *s = (AtpState *)calloc(1, sizeof(AtpState));
  if (s == NULL) return NULL;
  s->kbo      = cfg;
  s->step_cap = step_cap;
  atp_register_primitives();
  acp_selftest();   // verify the Stringterms pack/unpack round-trip
  // Allocate the growable rule / CP arrays at their initial
  // capacity.  ensure_*_cap fills the trace slots with
  // ATP_TRACE_NONE (0 is a valid trace index, so explicit fill
  // is required); a fresh array starts with r_cap == 0 so the
  // helper treats the whole span as new.
  atp_ensure_rule_cap(s, ATP_INIT_RULES);
  atp_ensure_cp_cap(s, ATP_INIT_CPS);
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
  s->rule_index_dirty = 1u;
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
  // Each cp_packed[] slot is a malloc'd byte string the queue owns;
  // free every non-NULL slot (free(NULL) is a no-op) then the array.
  if (s->cp_packed != NULL) {
    for (u32 i = 0; i < s->cp_cap; i++) free(s->cp_packed[i]);
    free(s->cp_packed);
  }
  free(s->cp_trace);
  free(s->cp_pri);
  free(s->cp_seq);
#ifdef ATP_FV_INDEX
  atp_fv_index_free(s->fv_index);
#endif
#ifdef ATP_RULE_INDEX
  atp_ri_free(s->rule_index);
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
  u32 n_roots = 2u * s->n_rules + 2u /* goal */
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

// Push a trace entry as a TAG_CTR with label = reason and children
// [NUM(parent_a), NUM(parent_b), lhs, rhs].  Returns the entry's
// index in s->trace, or ATP_TRACE_NONE if the buffer is full.
//
// 6.1b/c will wire this into add_equation / orient_and_add /
// generate_cps; for 6.1a the helper just exists, and the storage is
// init'd to zero by thvm_atp_init's calloc.
static u32 atp_trace_push(AtpState *s, u32 reason, u32 p_a, u32 p_b,
                          Term lhs, Term rhs) {
  if (s == NULL || s->n_trace >= ATP_MAX_TRACE) return ATP_TRACE_NONE;
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

// Push an axiom / pending equation onto the CP queue.  The
// saturation loop's orient + generate machinery processes it
// uniformly with later-derived CPs.  Also records a TRACE_AXIOM
// entry so the proof trace (stage 6.1) can identify this CP's
// origin downstream.  The CP queue is growable, so this never
// rejects for being full; returns 1 on success, 0 only on NULL
// state or a sort-check rejection.
fn u8 thvm_atp_add_equation(AtpState *s, Term lhs, Term rhs) {
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
  // 7c: an axiom enters the engine as a queued CP.  Canonicalize its
  // variables here so the very first CP, like every later-derived
  // one, carries a dense [0, k) variable set.
  thvm_normalize_vars(&lhs, &rhs);
#endif
  u32 trace_idx = atp_trace_push(s, TRACE_AXIOM,
                                 ATP_TRACE_NONE, ATP_TRACE_NONE,
                                 lhs, rhs);
  atp_cp_heap_push(s, lhs, rhs, trace_idx);
  return 1;
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
      // oriented rule -- forward only, no order check, no waste.
      RewriteSubst subst = {{0}};
      if (thvm_match(lhs[i], t, &subst)) {
        *fired = 1;
        return thvm_subst_apply(rhs[i], &subst);
      }
      continue;
    }
    // unorientable equation -- both directions, variable-safe + order-gated.
    if (atp_vars_contained(rhs[i], lhs[i])) {       // l -> r
      RewriteSubst subst = {{0}};
      if (thvm_match(lhs[i], t, &subst)) {
        Term repl = thvm_subst_apply(rhs[i], &subst);
        if (atp_compare(s, t, repl) == KBO_GT) { *fired = 1; return repl; }
      }
    }
    if (atp_vars_contained(lhs[i], rhs[i])) {       // r -> l
      RewriteSubst subst = {{0}};
      if (thvm_match(rhs[i], t, &subst)) {
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
  if (s->n_unorient == 0u && lhs == s->lhs && rhs == s->rhs &&
      n_rules == s->n_rules) {
    return atp_rewrite_normalize_indexed(s, t, step_cap);
  }
#endif
  for (u32 i = 0; i < step_cap; i++) {
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

#ifdef ATP_GOAL_HEURISTIC
// === Waldmeister lever 1: goal-directed CP selection ================
//
// Waldmeister's CPinGoal / GoalinCP heuristics (Clas_CP_Goal.c) weight
// a critical pair by its structural relationship to the conjecture: a
// CP whose subterms match the goal is selected first, a CP unrelated
// to the goal is pushed far down the queue.  Without this the engine
// saturates blindly -- cpl1 / subl2 / thm trace identical trajectories
// because the goal only gates the goal-check, never CP selection.
//
// This is a port of Waldmeister's CPinGoal classifier (Clas_CP_Goal.c).
// A CP (cl,cr) is weighted by its structural match to the conjecture,
// graded into three levels:
//
//   Doppelmatch  -- one CP side generalises a subterm of one goal side
//                   AND the other CP side generalises a subterm of the
//                   other goal side, under one consistent substitution.
//                   Weight = the goal's RESIDUAL mass: phi(goal) minus
//                   what the match covered.  Small when the CP closely
//                   resembles the goal.
//   Einfachmatch -- only one CP side matches.  Weight = residual mass
//                   x SINGLE_FACTOR.
//   Nullmatch    -- neither matches.  Weight = the CP's own mass
//                   x NONE_FACTOR.
//
// The key (vs the earlier additive penalty): a matched CP is scored by
// the goal residual, NOT its own size -- so a large goal-resembling CP
// still scores small and is selected early, while a flat multiplicative
// factor on a binary relate/not signal let large CPs leapfrog.
// SINGLE_FACTOR / NONE_FACTOR are Waldmeister's CIGICInit defaults.
#define ATP_GOAL_MIN_STRUCT    4u
#define ATP_GOAL_SINGLE_FACTOR 5u
#define ATP_GOAL_NONE_FACTOR   50u

// Match pattern `pat` against `subj` or any subterm of it, extending
// `sub` (a later match against the same sub stays consistent -- the
// Sigma-matching of CPinGoal).  Returns the symbol count of the matched
// subterm, 0 if none.  Patterns below MIN_STRUCT are ignored: a bare
// nand(x,y) shell matches everything and carries no signal.
static u32 atp_goal_match_into(Term pat, Term subj, RewriteSubst *sub) {
  if (atp_symbol_count(pat) >= ATP_GOAL_MIN_STRUCT) {
    RewriteSubst save = *sub;
    if (thvm_match(pat, subj, sub)) return atp_symbol_count(subj);
    *sub = save;
  }
  if (term_tag(subj) == TAG_CTR) {
    u32 n = term_ctr_n(subj);
    for (u32 i = 0; i < n; i++) {
      u32 c = atp_goal_match_into(pat, term_ctr_at(subj, i), sub);
      if (c) return c;
    }
  }
  return 0;
}

// CPinGoal weight of CP (cl,cr) against the conjecture.  No goal set
// (completion mode) -> 0.  See the block comment above.
static u32 atp_goal_weight(const AtpState *s, Term cl, Term cr) {
  if (s == NULL || s->goal_lhs == 0) return 0u;
  Term gl = s->goal_lhs, gr = s->goal_rhs;
  u32  phi_g = atp_symbol_count(gl) + atp_symbol_count(gr);
  u32  phi_c = atp_symbol_count(cl) + atp_symbol_count(cr);
  u32  best  = phi_c * ATP_GOAL_NONE_FACTOR;       // Nullmatch fallback
  // Two pairings: (cl into gl, cr into gr) and (cl into gr, cr into gl).
  for (u32 swap = 0; swap < 2u; swap++) {
    Term ga = swap ? gr : gl;
    Term gb = swap ? gl : gr;
    RewriteSubst sub = {{0}};
    u32 ca = atp_goal_match_into(cl, ga, &sub);
    u32 cb = ca ? atp_goal_match_into(cr, gb, &sub) : 0u;
    if (ca && cb) {                                // Doppelmatch
      u32 cov = ca + cb;
      u32 res = (cov < phi_g) ? phi_g - cov : 0u;
      if (res < best) best = res;
    }
    if (ca) {                                      // Einfachmatch via cl
      u32 res = (ca < phi_g) ? phi_g - ca : 0u;
      u32 w   = res * ATP_GOAL_SINGLE_FACTOR;
      if (w < best) best = w;
    }
    RewriteSubst s2 = {{0}};                       // Einfachmatch via cr
    u32 cc = atp_goal_match_into(cr, gb, &s2);
    if (cc) {
      u32 res = (cc < phi_g) ? phi_g - cc : 0u;
      u32 w   = res * ATP_GOAL_SINGLE_FACTOR;
      if (w < best) best = w;
    }
  }
  return best;
}
#endif /* ATP_GOAL_HEURISTIC */

// 8.8: priority weight for a CP.  Default `--add` heuristic is
// the symbol-count sum.  When `s->use_mix_heuristic` is set, add
// a penalty for CPs that fail to orient cleanly (KBO_UN or
// KBO_EQ) -- mirrors Waldmeister's `--mix` heuristic in
// `ClasHeuristics.c`.  The penalty (`MIX_UNORIENTED_PENALTY`)
// is conservative; experiments may want to tune it.  Under
// -DATP_GOAL_HEURISTIC a bounded goal-directed penalty is then
// added (Waldmeister lever 1, above).
#define MIX_UNORIENTED_PENALTY 4u
static u32 atp_cp_priority(AtpState *s, Term lhs, Term rhs) {
#ifdef ATP_GOAL_HEURISTIC
  // Goal-directed run: Waldmeister uses CPinGoal as THE classifier --
  // the weight is the CP's graded structural distance to the goal, not
  // its size.  Completion-mode runs (no goal) fall through to the
  // size/mix heuristic below.
  if (s != NULL && s->goal_lhs != 0) return atp_goal_weight(s, lhs, rhs);
#endif
  u32 base = atp_symbol_count(lhs) + atp_symbol_count(rhs);
  if (s != NULL && s->use_mix_heuristic) {
    KboCmp c = atp_compare(s, lhs, rhs);
    if (c != KBO_GT && c != KBO_LT) {
      // KBO_EQ / KBO_UN -- penalize.
      base += MIX_UNORIENTED_PENALTY;
    }
  }
  return base;
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
static void atp_cp_swap(AtpState *s, u32 i, u32 j) {
  u8  *tc = s->cp_packed[i];s->cp_packed[i]= s->cp_packed[j];s->cp_packed[j]= tc;
  u32  tt = s->cp_trace[i]; s->cp_trace[i] = s->cp_trace[j]; s->cp_trace[j] = tt;
  u32  tp = s->cp_pri[i];   s->cp_pri[i]   = s->cp_pri[j];   s->cp_pri[j]   = tp;
  u32  tq = s->cp_seq[i];   s->cp_seq[i]   = s->cp_seq[j];   s->cp_seq[j]   = tq;
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

// Push one CP onto the heap.  Computes its priority once (the cost
// the old select_cp paid n times per step) and sifts up.  O(log n).
static void atp_cp_heap_push(AtpState *s, Term lhs, Term rhs, u32 trace) {
  atp_ensure_cp_cap(s, s->n_cps + 1);
  u32 i = s->n_cps;
  // Pack the CP into a byte string outside the managed heap.  Slot i
  // (== n_cps) is always NULL here -- atp_ensure_cp_cap NULL-inits new
  // slots and every pop / drop NULLs the slot it vacates.  `packed`
  // is the buffer's identity; a later sift only moves the pointer
  // between slots, so it stays valid for the index borrow below.
  u8  *packed    = acp_pack(lhs, rhs, NULL);
  s->cp_packed[i]= packed;
  s->cp_trace[i] = trace;
  s->cp_pri[i]   = atp_cp_priority(s, lhs, rhs);
  u32 seq        = s->cp_seq_next++;
  s->cp_seq[i]   = seq;
  s->n_cps++;
  atp_cp_sift_up(s, i);
  // 8a: keep cp_graph in lockstep with the array mirror.
  atp_cp_graph_sync(s);
#ifdef ATP_FV_INDEX
  // 7d: index the new CP under its (GC-stable) seq id.  The record
  // borrows `packed`; the trie keys on the FV, not the heap slot, so
  // the sift-up above does not touch the index.
  atp_fv_index_insert(s->fv_index, lhs, rhs, packed, seq);
#endif
}

// Pop the cheapest CP from the queue (lowest (cp_pri, cp_seq) --
// the `--add` heuristic, ties by insertion order).
//
// 7c': the CP queue is a binary min-heap (atp_cp_heap_push keeps it
// so).  Selection is heap pop-min: take the root, move the last
// element into the root slot, sift down.  O(log n) per call --
// replacing the old per-step rebuild of an n-leaf INC-SUP tree +
// thvm_collapse_ordered, which was O(n) per step => O(n^2) over a
// run and the dominant cost past ~24 steps on hard problems.
//
// Returns 1 on success (out-params populated), 0 on empty queue.
fn u8 thvm_atp_select_cp(AtpState *s, Term *lhs_out, Term *rhs_out) {
  if (s == NULL || s->n_cps == 0) return 0;
  // Unpack the cheapest CP (heap root) from its byte string into two
  // fresh heap Terms for the caller to normalize.
  acp_unpack(s->cp_packed[0], lhs_out, rhs_out);
  s->last_popped_trace = s->cp_trace[0];
#ifdef ATP_FV_INDEX
  // 7d: the popped CP leaves the queue -- drop it from the index so a
  // later subsumption query never matches a stale, no-longer-queued
  // CP (which would diverge from the array-scan verdict).  Mark the
  // record dead BEFORE freeing the byte string it borrows: a dead
  // record is never dereferenced, so the borrow stays sound.
  atp_fv_index_remove(s->fv_index, s->cp_seq[0]);
#endif
  free(s->cp_packed[0]);
  s->cp_packed[0] = NULL;
  s->n_cps--;
  if (s->n_cps > 0) {
    u32 last = s->n_cps;
    s->cp_packed[0]    = s->cp_packed[last];
    s->cp_packed[last] = NULL;          // vacated slot: leave it empty
    s->cp_trace[0] = s->cp_trace[last];
    s->cp_pri[0]   = s->cp_pri[last];
    s->cp_seq[0]   = s->cp_seq[last];
    atp_cp_sift_down(s, 0);
  }
  // 8a: the pop shrank / reordered the mirror -- resync cp_graph.
  atp_cp_graph_sync(s);
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
    s->cp_seq[i] = s->cp_seq_next++;
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
fn AtpStatus thvm_atp_step(AtpState *s) {
  if (s == NULL) return ATP_QUEUE_EMPTY;

  AtpStatus goal = thvm_atp_goal_check(s);
  if (goal != ATP_RUNNING) return goal;

  if (s->step >= s->step_cap) return ATP_TIMEOUT;

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
  Term l = atp_rewrite_normalize(s, cp_lhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);
  Term r = atp_rewrite_normalize(s, cp_rhs, s->lhs, s->rhs, s->n_rules, NORM_CAP);

  if (kbo_eq(l, r)) {
    thvm_atp_heap_reset(hcp_norm);
    s->step++;
    return ATP_RUNNING;
  }

  u32 src_trace = s->last_popped_trace;
  AtpAddedRange added = thvm_atp_orient_and_add(s, l, r);
  if (added.count == 0) {
    // R full, or some other refusal.  Count the work and continue.
    s->step++;
    return ATP_RUNNING;
  }

  // Trace each newly-added rule with its source CP as parent_a.
  // For unfailing 2-way fallback both directions get separate
  // entries so PCL output can identify each rule individually.
  // Stash the trace index in r_trace[] so generate_cps can
  // record TRACE_CP parents for any CP born from this rule.
  for (u32 k = 0; k < added.count; k++) {
    Term rl = s->lhs[added.first + k];
    Term rr = s->rhs[added.first + k];
    u32  t  = atp_trace_push(s, TRACE_ORIENT, src_trace,
                             ATP_TRACE_NONE, rl, rr);
    s->r_trace[added.first + k] = t;
  }

  // Interreduce shifts new-rule indices down by `dropped`.
  u32 dropped = thvm_atp_interreduce(s, added);
  AtpAddedRange post = added;
  post.first = (dropped > post.first) ? 0 : (post.first - dropped);

  thvm_atp_generate_cps(s, post);

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
      case TRACE_AXIOM:  type_str = "axiom";  break;
      case TRACE_ORIENT: type_str = "orient"; break;
      case TRACE_CP:     type_str = "cp";     break;
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
// Backward "anti" steps (r->l) are OFF by default -- see MNF_MAX_ANTI.

#define MNF_RED        0u
#define MNF_GREEN      1u
// Backward "anti" steps (r->l) are OFF by default: Waldmeister's MNF
// defaults to noAnti -- forward normalisation only -- and relies on
// completion to grow R until the fronts' forward reducts coincide.  A
// non-zero cap opts into variable-safe backward steps, capped per
// lineage (Waldmeister's antiWOVar).  Overridable with -DMNF_MAX_ANTI=N.
#ifndef MNF_MAX_ANTI
#define MNF_MAX_ANTI   0u
#endif
#define MNF_MAX_NODES  400000u
#define MNF_SUCC_CAP   2048u
#define MNF_BUDGET     192u
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
#ifdef ATP_MNF_DIAG
  u32      meet_a;           // existing table node the join collided with
  u32      meet_b_parent;    // parent of the colliding (uncreated) node
  Term     meet_term;        // the term both fronts reached
  u8       meet_b_col;       // colour of the colliding (uncreated) node
#endif
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
#ifdef ATP_MNF_DIAG
      m->meet_a        = idx;
      m->meet_b_parent = parent;
      m->meet_term     = t;
      m->meet_b_col    = col;
#endif
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

// One-step rewrites of `t` (all positions, rules [rule_lo, rule_hi),
// forward + -- when allow_anti -- variable-safe backward) collected
// into mnf_succ_buf / mnf_succ_anti.
static Term mnf_succ_buf[MNF_SUCC_CAP];
static u8   mnf_succ_anti[MNF_SUCC_CAP];

static void mnf_successors(AtpState *s, Term t, u8 allow_anti,
                           u32 rule_lo, u32 rule_hi, u32 *n) {
  for (u32 j = rule_lo; j < rule_hi; j++) {
    if (*n >= MNF_SUCC_CAP) return;
    {
      RewriteSubst sub = {{0}};
      if (thvm_match(s->lhs[j], t, &sub)) {
        mnf_succ_buf[*n]  = thvm_subst_apply(s->rhs[j], &sub);
        mnf_succ_anti[*n] = 0u;
        (*n)++;
        if (*n >= MNF_SUCC_CAP) return;
      }
    }
    if (allow_anti && atp_vars_contained(s->lhs[j], s->rhs[j])) {
      RewriteSubst sb = {{0}};
      if (thvm_match(s->rhs[j], t, &sb)) {
        mnf_succ_buf[*n]  = thvm_subst_apply(s->lhs[j], &sb);
        mnf_succ_anti[*n] = 1u;
        (*n)++;
        if (*n >= MNF_SUCC_CAP) return;
      }
    }
  }
  if (term_tag(t) == TAG_CTR) {
    u32 m = term_ctr_n(t);
    if (m > REWRITE_MAX_ARITY) return;
    for (u32 i = 0; i < m; i++) {
      u32 base = *n;
      mnf_successors(s, term_ctr_at(t, i), allow_anti, rule_lo, rule_hi, n);
      for (u32 k = base; k < *n; k++) {
        Term ch[REWRITE_MAX_ARITY];
        for (u32 c = 0; c < m; c++) {
          ch[c] = (c == i) ? mnf_succ_buf[k] : term_ctr_at(t, c);
        }
        mnf_succ_buf[k] = term_new_ctr(term_ext(t), ch, m);
      }
    }
  }
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
  mnf_successors(s, t, (u8)(anti < MNF_MAX_ANTI), rule_lo, rule_hi, &n);
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
static int mnf_step(AtpState *s, AtpMnf *m, u32 budget) {
  if (m->joined) return 1;
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
      if (m->nodes[ni].expanded) mnf_expand_node(s, m, ni, lo, hi);
    }
    m->n_rules_seen = hi;
  }
  // (b) expand the two fronts in alternation -- one node each per
  // round -- taking each colour's node by the irred-adaptive deque
  // policy (mnf_pop).  A node's `irred` is settled by mnf_expand_node;
  // it feeds the next pop of the same colour.
  for (u32 b = 0; b < budget && !m->joined; b++) {
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
#endif /* ATP_MNF */

fn AtpStatus thvm_atp_goal_check(AtpState *s) {
  if (s == NULL || s->goal_lhs == 0) return ATP_RUNNING;
  const u32 NORM_CAP = 64;

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

#ifdef ATP_MNF
  // Milestone 10: drive the MNF bidirectional search instead of the
  // single-normal-form check.  Created lazily on the first call; each
  // call feeds in completion's new rules and expands a budget of
  // nodes.  An opposite-colour collision (the fronts met) is the proof.
  if (s->mnf == NULL) s->mnf = mnf_create(s);
  if (s->mnf != NULL) {
    return mnf_step(s, s->mnf, MNF_BUDGET) ? ATP_PROVED : ATP_RUNNING;
  }
  // mnf allocation failed -- fall through to the single-NF check.
#endif
  Term l = atp_rewrite_normalize(s, s->goal_lhs, s->lhs, s->rhs,
                                 s->n_rules, NORM_CAP);
  Term r = atp_rewrite_normalize(s, s->goal_rhs, s->lhs, s->rhs,
                                 s->n_rules, NORM_CAP);
  return kbo_eq(l, r) ? ATP_PROVED : ATP_RUNNING;
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
    }
    w++;
  }
  if (w != s->n_cps) {
    s->n_cps = w;
    thvm_atp_cp_reheapify(s);   // rebuilds heap + FV index + cp_graph
  }
}
#endif /* ATP_ORPHAN_KILL */

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
  u32  n_new = added.count;
  if (n_new > 2) n_new = 2;
  for (u32 k = 0; k < n_new; k++) {
    new_lhs[k] = s->lhs[added.first + k];
    new_rhs[k] = s->rhs[added.first + k];
  }

  u32 dropped = 0;
  u32 i       = 0;
  while (i < added.first - dropped) {
    Term old_lhs = s->lhs[i];
    Term old_rhs = s->rhs[i];
    Term reduced = atp_rewrite_normalize(s, old_lhs, new_lhs, new_rhs, n_new, 16);
    if (!kbo_eq(reduced, old_lhs)) {
      // The older rule's LHS simplified -- drop it and requeue
      // (reduced, old_rhs) for re-orientation.
      thvm_atp_add_equation(s, reduced, old_rhs);
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
      i++;
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
#ifndef ATP_CP_DIAG
  (void)rule_a;
  (void)rule_b;
#endif
  for (u32 i = 0; i < ncps; i++) {
    Term cp_lhs = cps[i].lhs;
    Term cp_rhs = cps[i].rhs;
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
    if (joinable) {
      s->n_cps_dropped_joinable++;
      continue;
    }
    if (q_subsmd) {
      s->n_cps_dropped_queue_subsumed++;
      continue;
    }
    u32 t = atp_trace_push(s, TRACE_CP, parent_a, parent_b,
                           cp_lhs, cp_rhs);
    atp_cp_heap_push(s, cp_lhs, cp_rhs, t);
    pushed++;
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
static u32 thvm_atp_generate_cps_c(AtpState *s, AtpAddedRange added) {
  u32 first = added.first;
  u32 last  = added.first + added.count;
  u32 n     = s->n_rules;
  if (last > n) last = n;
  if (first > last) return 0;

  CriticalPair buf[ATP_CP_BATCH];
  u32 pushed = 0;

  // (new x all_R): the new rule is i (outer), j ranges over all
  // existing rules (including the new ones for new x new self-overlap).
  for (u32 i = first; i < last; i++) {
    for (u32 j = 0; j < n; j++) {
      u32 nbuf = thvm_critical_pairs_range(s->lhs, s->rhs, n,
                                           i, i + 1, j, j + 1,
                                           buf, ATP_CP_BATCH);
      pushed += atp_push_cps_traced(s, buf, nbuf,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
      // A single saturation step can out-allocate a whole GC
      // semi-space in raw critical-pair + normalisation scratch.
      // Collect between overlap pairs -- `buf` is fully processed
      // here, so no in-flight CP needs rooting -- to bound the
      // transient working set instead of crashing mid-step.
      if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);
    }
  }

  // (old x new): old rule on the outside, new rule fed as inner.
  for (u32 i = 0; i < first; i++) {
    for (u32 j = first; j < last; j++) {
      u32 nbuf = thvm_critical_pairs_range(s->lhs, s->rhs, n,
                                           i, i + 1, j, j + 1,
                                           buf, ATP_CP_BATCH);
      pushed += atp_push_cps_traced(s, buf, nbuf,
                                    s->r_trace[i], s->r_trace[j],
                                    i, j);
      if (atp_heap_under_pressure()) thvm_atp_gc_collect(s);   // see above
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

  ctx->out[ctx->count].lhs = cp_lhs;
  ctx->out[ctx->count].rhs = cp_rhs;
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
