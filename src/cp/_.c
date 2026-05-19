// thvm_critical_pairs - enumerate critical pairs of an oriented
// rule set on TAG_CTR + TAG_FVR terms.  Stage 4 of
// docs/plans/waldmeister_ic_atp.md.
//
// A critical pair (CP) arises when two rules  l_i -> r_i  and
// l_j -> r_j  overlap: there exists a non-variable position p in
// l_i  such that  l_i|p  unifies with l_j  (after variable
// renaming).  Letting σ = mgu(l_i|p, l_j_renamed), the CP is
//
//    ( σ(l_i[p ← r_j_renamed]) , σ(r_i) )
//
// Both sides should reduce to the same normal form for the system
// to be confluent; CP generation is the engine that drives KB
// completion.
//
// This implementation:
//   - Assumes per-rule variable ids fit in [0, REWRITE_MAX_VAR/2).
//     rule_j gets renamed by REWRITE_MAX_VAR/2 so its ids land in
//     the upper half, disjoint from rule_i.
//   - Walks all non-variable positions of rule_i.lhs (top + every
//     CTR-internal position).
//   - Tries unification at each (i, j, p) triple.  Successful
//     unifiers produce one CP each, written into `out`.
//
// No SUP-based encoding yet -- that's stage 4.5 (optional).

#define CP_RENAME_OFFSET (REWRITE_MAX_VAR / 2)

// CriticalPair is declared in src/thvm.h so external callers can hold
// the buffer.

// Replace the sub-term at position `p` of `t` with `repl`.  `p` is
// a path of child indices: p[0] is the index at depth 0, p[1] at
// depth 1, etc.  `p_len == 0` means top.  Returns the rebuilt term.
static Term cp_replace_at(Term t, const u32 *p, u32 p_len, Term repl) {
  if (p_len == 0) return repl;
  if (term_tag(t) != TAG_CTR) return t;
  u32 n = term_ctr_n(t);
  if (p[0] >= n || n > REWRITE_MAX_ARITY) return t;
  Term children[REWRITE_MAX_ARITY];
  for (u32 i = 0; i < n; i++) {
    children[i] = (i == p[0])
      ? cp_replace_at(term_ctr_at(t, i), p + 1, p_len - 1, repl)
      : term_ctr_at(t, i);
  }
  return term_new_ctr(term_ext(t), children, n);
}

// Read the sub-term at position `p`.
static Term cp_subterm_at(Term t, const u32 *p, u32 p_len) {
  for (u32 d = 0; d < p_len; d++) {
    if (term_tag(t) != TAG_CTR) return 0;
    u32 n = term_ctr_n(t);
    if (p[d] >= n) return 0;
    t = term_ctr_at(t, p[d]);
  }
  return t;
}

// Recursively visit every non-variable position of `t` (including
// the top), invoking `visit` with the position path.  `visit` may
// append CPs into the output buffer; it returns the new count.
typedef u32 (*CpVisitor)(const u32 *p, u32 p_len, void *ctx);

static u32 cp_walk_positions(Term t, u32 *path, u32 depth, u32 max_depth,
                             CpVisitor visit, void *ctx, u32 count) {
  if (term_tag(t) != TAG_CTR) return count;  // skip variables (FVR)
  count = visit(path, depth, ctx);
  if (depth >= max_depth) return count;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) {
    path[depth] = i;
    count = cp_walk_positions(term_ctr_at(t, i), path, depth + 1, max_depth,
                              visit, ctx, count);
  }
  return count;
}

// CP_MAX_DEPTH is declared in src/thvm.h alongside CriticalPair --
// cp_walk_positions caps the path depth at it, so a recorded
// CriticalPair.pos always fits.

// Visitor closure: at the current position p, try unifying
// rule_i.lhs|p with rule_j_renamed.lhs.  On success, push the CP
// into out_buf.
typedef struct {
  Term         li, ri;        // rule i (read as-is)
  Term         lj, rj;        // rule j (already renamed, ready to unify)
  CriticalPair *out;
  u32           cap;
  u32           count;
} CpCtx;

static u32 cp_visit(const u32 *p, u32 p_len, void *raw) {
  CpCtx *ctx = (CpCtx *)raw;
  if (ctx->count >= ctx->cap) return ctx->count;

  Term sub = cp_subterm_at(ctx->li, p, p_len);
  if (sub == 0) return ctx->count;
  if (term_tag(sub) == TAG_FVR) return ctx->count;  // skip vars (defensive)

  RewriteSubst subst = {{0}};
  if (!thvm_unify(sub, ctx->lj, &subst)) return ctx->count;

  // CP = (σ(l_i[p ← r_j]), σ(r_i))
  Term replaced = cp_replace_at(ctx->li, p, p_len, ctx->rj);
  Term cp_lhs   = thvm_unify_apply(replaced, &subst);
  Term cp_rhs   = thvm_unify_apply(ctx->ri,  &subst);

  CriticalPair *slot = &ctx->out[ctx->count];
  slot->lhs = cp_lhs;
  slot->rhs = cp_rhs;
  // Record the superposition position -- the path into rule i's lhs
  // where rule j overlapped.  cp_walk_positions caps depth at
  // CP_MAX_DEPTH, so p_len never exceeds the pos[] array.
  slot->pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len; d++) slot->pos[d] = (u8)p[d];
  ctx->count++;
  return ctx->count;
}

// Enumerate CPs over the (i, j) sub-rectangle  [start_i, end_i)
// x [start_j, end_j)  of the rule set, with j's variables renamed
// apart by CP_RENAME_OFFSET.  Saturation uses this to compute only
// the freshly-required CPs after a rule add: (new x all_R) and
// (old x new), avoiding the redundant (old x old) work.
fn u32 thvm_critical_pairs_range(const Term *lhs, const Term *rhs, u32 n_rules,
                                 u32 start_i, u32 end_i,
                                 u32 start_j, u32 end_j,
                                 CriticalPair *out, u32 cap) {
  if (end_i > n_rules) end_i = n_rules;
  if (end_j > n_rules) end_j = n_rules;
  CpCtx ctx;
  ctx.out   = out;
  ctx.cap   = cap;
  ctx.count = 0;
  u32 path[CP_MAX_DEPTH];
  for (u32 i = start_i; i < end_i; i++) {
    for (u32 j = start_j; j < end_j; j++) {
      ctx.li = lhs[i];
      ctx.ri = rhs[i];
      ctx.lj = thvm_rename_vars(lhs[j], CP_RENAME_OFFSET);
      ctx.rj = thvm_rename_vars(rhs[j], CP_RENAME_OFFSET);
      ctx.count = cp_walk_positions(ctx.li, path, 0, CP_MAX_DEPTH,
                                    cp_visit, &ctx, ctx.count);
    }
  }
  return ctx.count;
}

// Enumerate all CPs across the (i, j) cross-product of the rule set.
// Thin wrapper around the range version with the full extents.
fn u32 thvm_critical_pairs(const Term *lhs, const Term *rhs, u32 n_rules,
                           CriticalPair *out, u32 cap) {
  return thvm_critical_pairs_range(lhs, rhs, n_rules,
                                   0, n_rules, 0, n_rules,
                                   out, cap);
}
