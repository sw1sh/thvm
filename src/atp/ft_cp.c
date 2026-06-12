// ft_cp.c - AtpFt-native critical-pairs visitor.
//
// Parallel cp_visit / thvm_critical_pairs_pair working directly on
// AtpFtCell trees -- the engine's rule mirrors `s->lhs_ft[]` /
// `s->rhs_ft[]` are already in this form, so the per-overlap-pair
// cost of converting back and forth is paid only at the boundary:
// ft_to_term on the THREE outputs of an accepted CP (cp_lhs,
// cp_rhs, cp_peak) so the CriticalPair slot stays Term-typed for
// the downstream queue.
//
// Cost model:
//   * Per (i, j) overlap pair:
//       - rename j's vars by REWRITE_MAX_VAR/2 (one O(|lj|+|rj|)
//         scratch walk allocating a fresh FT copy).
//       - walk every non-var position of li_ft; ft_subterm_at lifts
//         out the cell pointer in O(depth) via the next/end chain.
//       - ft_unify each subterm vs lj_renamed_ft, emitting a
//         binding into an AtpFtSubst (reset between positions).
//   * Per ACCEPTED CP:
//       - cp_replace_at on FT (O(li_ft size)) producing a fresh
//         tree in scratch.
//       - ft_unify_apply on three FT trees.
//       - ft_to_term on the three results.
//
// Downstream filter still sees byte-identical CPs because:
//   * ft_unify is structurally the same Robinson algorithm as
//     thvm_unify (FVR-walks the same way, same occurs check).
//   * ft_unify_apply walks the same recursion; the resulting Term
//     after ft_to_term hash-collides with thvm_unify_apply's
//     output by induction.
//
// Gated on THVM_ATPFT_UNIFY (default off; flipped on after the
// 5-run andassoc bench beats baseline).
//
// Included from src/atp/_.c after ft_unify.c so the ft_unify /
// ft_unify_apply prototypes are in scope.

#ifdef THVM_ATPFT_UNIFY

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"   // WF_VAR_BIT

// Forward decls -- defined in sibling FT TUs.
extern AtpFtCell *ftnew_var  (AtpFt *a, u32 var_id, int scratch);
extern AtpFtCell *ftnew_const(AtpFt *a, u32 sym, int scratch);
extern AtpFtCell *ftnew_ctr  (AtpFt *a, u32 sym, u16 arity,
                              AtpFtCell *const *kids, int scratch);
extern AtpFtCell *ft_from_term(AtpFt *a, Term t, int scratch);
extern Term       ft_to_term  (const AtpFtCell *x);

// AtpFtSubst forward (from ft_match.c, same TU).  We don't redefine.
extern int        ft_unify       (const AtpFtCell *s, const AtpFtCell *t,
                                  AtpFtSubst *subst);
extern AtpFtCell *ft_unify_apply (AtpFt *a, const AtpFtCell *t,
                                  const AtpFtSubst *s, int scratch);
extern void       ft_subst_reset (AtpFtSubst *s);

// --- Var-bit helpers (file-local copies) -----------------------------
static inline int  ft_cp_is_var (const AtpFtCell *c) {
  return (c->sym & WF_VAR_BIT) != 0u;
}
static inline u32  ft_cp_var_id (const AtpFtCell *c) {
  return c->sym & ~WF_VAR_BIT;
}

// --- Rename: bump every FVR id by `offset`, building a fresh tree --
//
// Scratch-arena output by convention -- the rename is one of the
// per-(i,j)-pair scratch lifetimes.  Constant subtrees (no FVRs)
// return a fresh deep-copy anyway since the scratch arena's reset
// would otherwise invalidate the original.
#define FT_CP_KIDS_STACK 16u

static AtpFtCell *ft_rename_vars(AtpFt *a, const AtpFtCell *t,
                                 u32 offset, int scratch) {
  if (t == NULL) return NULL;
  if (ft_cp_is_var(t)) {
    return ftnew_var(a, ft_cp_var_id(t) + offset, scratch);
  }
  u32 sym   = t->sym;
  u16 arity = t->arity;
  if (arity == 0u) {
    return ftnew_const(a, sym, scratch);
  }
  AtpFtCell *stack_kids[FT_CP_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > FT_CP_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_rename_vars: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = t->next;
  for (u16 i = 0; i < arity; i++) {
    kids[i] = ft_rename_vars(a, child, offset, scratch);
    child = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, scratch);
  free(heap_kids);
  return out;
}

// --- Subterm extraction at a position path ---------------------------
//
// Walk `t` along the path `p[0..p_len)` of child indices; return the
// referenced cell, or NULL on malformed path / index out of range.
// Sibling stride uses next/end.
static const AtpFtCell *ft_subterm_at(const AtpFtCell *t,
                                      const u32 *p, u32 p_len) {
  for (u32 d = 0; d < p_len; d++) {
    if (t == NULL) return NULL;
    if (ft_cp_is_var(t)) return NULL;
    if (p[d] >= t->arity) return NULL;
    const AtpFtCell *c = t->next;
    for (u32 i = 0; i < p[d]; i++) c = c->end->next;
    t = c;
  }
  return t;
}

// --- cp_replace_at on AtpFt -----------------------------------------
//
// Returns a fresh tree (scratch arena) with the subterm at position
// `p` replaced by `repl`.  Non-path siblings are deep-copied (the
// scratch reset between positions invalidates everything anyway).
// p_len == 0 => return repl directly (the caller still treats this
// path as a fresh subtree).
static AtpFtCell *ft_cp_replace_at(AtpFt *a, const AtpFtCell *t,
                                   const u32 *p, u32 p_len,
                                   const AtpFtCell *repl, int scratch);

// Fused replace_at + unify_apply: build a fresh tree from `t` with
// the subterm at position `p` set to `repl`, AND every variable in
// the resulting tree substituted via `subst`.  Saves one full
// recursive walk per CP vs calling ft_cp_replace_at + ft_unify_apply
// in sequence (the path-side branch substitutes ON the way down; off-
// path siblings substitute via the standard ft_unify_apply leaf).
static AtpFtCell *ft_cp_replace_subst(AtpFt *a, const AtpFtCell *t,
                                      const u32 *p, u32 p_len,
                                      const AtpFtCell *repl,
                                      const AtpFtSubst *subst, int scratch);

static AtpFtCell *ft_cp_deep_copy(AtpFt *a, const AtpFtCell *src, int scratch);

static AtpFtCell *ft_cp_deep_copy(AtpFt *a, const AtpFtCell *src, int scratch) {
  if (src == NULL) return NULL;
  if (ft_cp_is_var(src)) {
    return ftnew_var(a, ft_cp_var_id(src), scratch);
  }
  u32 sym   = src->sym;
  u16 arity = src->arity;
  if (arity == 0u) {
    return ftnew_const(a, sym, scratch);
  }
  AtpFtCell *stack_kids[FT_CP_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > FT_CP_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_cp_deep_copy: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = src->next;
  for (u16 i = 0; i < arity; i++) {
    kids[i] = ft_cp_deep_copy(a, child, scratch);
    child = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, scratch);
  free(heap_kids);
  return out;
}

static AtpFtCell *ft_cp_replace_at(AtpFt *a, const AtpFtCell *t,
                                   const u32 *p, u32 p_len,
                                   const AtpFtCell *repl, int scratch) {
  if (p_len == 0u) {
    return ft_cp_deep_copy(a, repl, scratch);
  }
  if (t == NULL || ft_cp_is_var(t)) return ft_cp_deep_copy(a, t, scratch);
  u32 sym   = t->sym;
  u16 arity = t->arity;
  if (p[0] >= arity) return ft_cp_deep_copy(a, t, scratch);
  AtpFtCell *stack_kids[FT_CP_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > FT_CP_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_cp_replace_at: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = t->next;
  for (u16 i = 0; i < arity; i++) {
    if (i == p[0]) {
      kids[i] = ft_cp_replace_at(a, child, p + 1, p_len - 1, repl, scratch);
    } else {
      kids[i] = ft_cp_deep_copy(a, child, scratch);
    }
    child = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, scratch);
  free(heap_kids);
  return out;
}

// Fused replace_at + unify_apply: walks `t` once.  Off-path: route
// through ft_unify_apply to substitute vars.  At the matched
// position: walk into `repl` substituting (via ft_unify_apply).
static AtpFtCell *ft_cp_replace_subst(AtpFt *a, const AtpFtCell *t,
                                      const u32 *p, u32 p_len,
                                      const AtpFtCell *repl,
                                      const AtpFtSubst *subst, int scratch) {
  if (p_len == 0u) {
    // The position to replace -- substitute into the replacement.
    return ft_unify_apply(a, repl, subst, scratch);
  }
  if (t == NULL) return NULL;
  if (ft_cp_is_var(t)) {
    // Off-path var: substitute as-is.
    return ft_unify_apply(a, t, subst, scratch);
  }
  u32 sym   = t->sym;
  u16 arity = t->arity;
  if (p[0] >= arity) {
    // Bad path; defensive deep-copy.
    return ft_unify_apply(a, t, subst, scratch);
  }
  AtpFtCell *stack_kids[FT_CP_KIDS_STACK];
  AtpFtCell **kids = stack_kids;
  AtpFtCell **heap_kids = NULL;
  if (arity > FT_CP_KIDS_STACK) {
    heap_kids = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (heap_kids == NULL) {
      fprintf(stderr, "ft_cp_replace_subst: kids OOM\n");
      exit(1);
    }
    kids = heap_kids;
  }
  const AtpFtCell *child = t->next;
  for (u16 i = 0; i < arity; i++) {
    if (i == p[0]) {
      kids[i] = ft_cp_replace_subst(a, child, p + 1, p_len - 1,
                                    repl, subst, scratch);
    } else {
      // Off-path sibling: substitute via ft_unify_apply.  (A deep_copy
      // would be WRONG -- the sibling may contain vars bound by the
      // unifier.)
      kids[i] = ft_unify_apply(a, child, subst, scratch);
    }
    if (kids[i] == NULL) { free(heap_kids); return NULL; }
    child = child->end->next;
  }
  AtpFtCell *out = ftnew_ctr(a, sym, arity, kids, scratch);
  free(heap_kids);
  return out;
}

// --- FT-native cp_visit ---------------------------------------------
//
// Mirrors src/cp/_.c cp_visit's recursion but feeds the FT trees.
// On success, populates the CriticalPair slot via ft_to_term on the
// three result trees.
typedef struct {
  const AtpFtCell *li_ft;          // rule i's lhs
  const AtpFtCell *ri_ft;          // rule i's rhs
  const AtpFtCell *lj_ft_renamed;  // rule j's lhs (renamed by REWRITE_MAX_VAR/2)
  const AtpFtCell *rj_ft_renamed;  // rule j's rhs (renamed)
  AtpFt           *arena;          // scratch arena for working FT cells
  CriticalPair    *out;
  u32              cap;
  u32              count;
  u8               need_peak;      // 1 = compute cp_peak; 0 = leave slot->peak = 0
  u8               skip_root;      // 1 = PROPER positions only (WM eTT)
  u32              scratch_mark_base; // scratch bump-pointer SNAPSHOT to reset
                                      // between positions (TBD: ft_alloc
                                      // does not currently expose mark/reset,
                                      // so we accept growth and rely on
                                      // saturation-step ft_scratch_reset)
} FtCpCtx;

static u32 ft_cp_visit(const u32 *p, u32 p_len, void *raw) {
  FtCpCtx *ctx = (FtCpCtx *)raw;
  if (ctx->skip_root && p_len == 0u) return ctx->count;
  if (ctx->count >= ctx->cap) return ctx->count;

  const AtpFtCell *sub = ft_subterm_at(ctx->li_ft, p, p_len);
  if (sub == NULL) return ctx->count;
  if (ft_cp_is_var(sub)) return ctx->count;     // skip var positions

  AtpFtSubst subst = {0};
  if (!ft_unify(sub, ctx->lj_ft_renamed, &subst)) {
    return ctx->count;
  }

  // Fused: build cp_lhs = subst(li[p<-rj]) in ONE walk (saves the
  // separate ft_cp_replace_at + ft_unify_apply over the spliced
  // tree).  cp_rhs and cp_peak each still need their own walk.
  AtpFtCell *cp_lhs_ft  = ft_cp_replace_subst(ctx->arena, ctx->li_ft, p, p_len,
                                              ctx->rj_ft_renamed, &subst, 1);
  AtpFtCell *cp_rhs_ft  = ft_unify_apply(ctx->arena, ctx->ri_ft, &subst, 1);
  AtpFtCell *cp_peak_ft = NULL;
  if (ctx->need_peak) {
    cp_peak_ft = ft_unify_apply(ctx->arena, ctx->li_ft, &subst, 1);
  }
  ft_subst_reset(&subst);
  if (cp_lhs_ft == NULL || cp_rhs_ft == NULL) {
    return ctx->count;
  }
  if (ctx->need_peak && cp_peak_ft == NULL) return ctx->count;

  // Decode back to Term for the CriticalPair slot.  The peak slot is
  // only populated when the connectedness gate is active (otherwise
  // downstream never reads it -- atp_push_cps_traced gates on
  // cps[i].peak != 0).
  Term cp_lhs  = ft_to_term(cp_lhs_ft);
  Term cp_rhs  = ft_to_term(cp_rhs_ft);
  Term cp_peak = ctx->need_peak ? ft_to_term(cp_peak_ft) : 0;

  CriticalPair *slot = &ctx->out[ctx->count];
  slot->lhs     = cp_lhs;
  slot->rhs     = cp_rhs;
  slot->peak    = cp_peak;
  slot->pos_len = (u8)p_len;
  for (u32 d = 0; d < p_len; d++) slot->pos[d] = (u8)p[d];
  ctx->count++;
  return ctx->count;
}

// --- Position walk on AtpFt -----------------------------------------
//
// Same shape as src/cp/_.c cp_walk_positions; calls ft_cp_visit at
// every non-var position (including top).  CP_MAX_DEPTH cap matches.
static u32 ft_cp_walk_positions(const AtpFtCell *t,
                                u32 *path, u32 depth, u32 max_depth,
                                u32 (*visit)(const u32 *, u32, void *),
                                void *ctx, u32 count) {
  if (t == NULL || ft_cp_is_var(t)) return count;
  count = visit(path, depth, ctx);
  if (depth >= max_depth) return count;
  u32 n = t->arity;
  const AtpFtCell *child = t->next;
  for (u32 i = 0; i < n; i++) {
    path[depth] = i;
    count = ft_cp_walk_positions(child, path, depth + 1, max_depth,
                                 visit, ctx, count);
    child = child->end->next;
  }
  return count;
}

// --- Public FT critical-pairs-pair ----------------------------------
//
// Mirrors thvm_critical_pairs_pair: rule j's vars must already be
// renamed apart from rule i's (the caller supplies pre-renamed
// FT trees).  Reads from `li_ft, ri_ft, lj_ft_r, rj_ft_r`, appends
// produced CPs into `out` starting at `count`.  Returns new count.
//
// `arena` is the working scratch arena -- ft_unify_apply, the
// replaced tree, and the renamed copies all live there.  Caller
// owns the lifecycle (call ft_scratch_reset between overlap pairs
// to bound growth).
//
// `skip_root` = 1 enumerates PROPER positions of li only (WM eTT;
// see thvm_critical_pairs_pair_noroot for the root-overlap
// ownership discipline).
u32 thvm_critical_pairs_pair_ft(const AtpFtCell *li_ft,
                                const AtpFtCell *ri_ft,
                                const AtpFtCell *lj_ft_r,
                                const AtpFtCell *rj_ft_r,
                                AtpFt           *arena,
                                u8               need_peak,
                                u8               skip_root,
                                CriticalPair    *out,
                                u32              cap,
                                u32              count) {
  FtCpCtx ctx;
  ctx.li_ft          = li_ft;
  ctx.ri_ft          = ri_ft;
  ctx.lj_ft_renamed  = lj_ft_r;
  ctx.rj_ft_renamed  = rj_ft_r;
  ctx.arena          = arena;
  ctx.out            = out;
  ctx.cap            = cap;
  ctx.count          = count;
  ctx.need_peak      = need_peak;
  ctx.skip_root      = skip_root;
  ctx.scratch_mark_base = 0u;
  u32 path[CP_MAX_DEPTH];
  return ft_cp_walk_positions(li_ft, path, 0, CP_MAX_DEPTH,
                              ft_cp_visit, &ctx, count);
}

// --- Public rename wrapper (so atp_overlap_ij can rename without
//     redefining ft_rename_vars in its own scope) ------------------
AtpFtCell *thvm_ft_rename_vars(AtpFt *a, const AtpFtCell *t,
                               u32 offset, int scratch) {
  return ft_rename_vars(a, t, offset, scratch);
}

#endif // THVM_ATPFT_UNIFY
