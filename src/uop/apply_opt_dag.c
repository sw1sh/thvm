// uop/apply_opt_dag.c -- DAG-side apply_opt, the Phase E counterpart
// to apply_opt.c's legacy KpSchedule.applied_opts[] mutation.  Each
// helper takes a UOp DAG root + opt parameters and returns a new
// root with the equivalent transformation applied.  Operates purely
// on the UOp DAG -- no KpSchedule, no tile_uops side-channel.
//
// Initial slice covers the two opts that matter for the matmul
// autotune loop: KOP_TC (tile-size selection for the simdgroup_matrix
// template) and KOP_GLOBAL (axis-type swap to drop the
// `if(sgi==0u && tg==0u)` guard in render_uop's TC emission).
// KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP/SWAP land in subsequent
// slices.
//
// Wired into kernel_apply_opt's outer dispatcher: when
// `ke->cached_lift.store_root != 0` we take the DAG path here;
// otherwise we fall through to the legacy schedule path.

// ---------- KOP_TC ---------------------------------------------------
// Wrap (or update) the OPT(_, TC, factor) marker around the inner
// REDUCE that lives in STORE.value.  Three layouts to handle:
//   (a) STORE(buf, addr, REDUCE(...))        -- bare matmul, no OPT.
//                                               Wrap REDUCE with OPT.
//   (b) STORE(buf, addr, OPT(REDUCE, TC, _)) -- already TC-marked.
//                                               Replace factor.
//   (c) anything else                        -- not a matmul we
//                                               recognise; bail.
//
// Returns the new STORE root, or 0 on bail.

fn Term uop_dag_apply_tc(Term root, u32 factor) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  u64 sloc = term_val(root);
  Term buf   = heap_read(sloc + 0);
  Term addr  = heap_read(sloc + 1);
  Term value = heap_read(sloc + 2);

  Term new_value = 0;
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_REDUCE) {
    // Layout (a): bare REDUCE.
    new_value = uop_opt(value, UOP_OPT_TC, factor);
  } else if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
             && uop_opt_kind(value) == UOP_OPT_TC) {
    // Layout (b): already wrapped; rebuild with new factor.
    Term inner = uop_opt_target(value);
    if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_REDUCE) return 0;
    new_value = uop_opt(inner, UOP_OPT_TC, factor);
  } else {
    return 0;
  }

  if (new_value == value) return root;
  return uop_store(buf, addr, new_value);
}

// ---------- KOP_GLOBAL ----------------------------------------------
// Find every UOP_RANGE leaf with `axis_id == target_axis_id` and
// replace its axis_type with `KAX_GLOBAL` (5).  Hash-cons makes the
// new range a fresh term distinct from the old one; uop_graph_rewrite
// then walks the DAG bottom-up, rebuilding parents whose children
// changed (memoised so each parent rebuilds at most once).
//
// Validity gates:
//   - target axis must currently be axis_type == KAX_LOOP (0).  Don't
//     overwrite REDUCE/UPCAST/UNROLL/LOCAL/GROUP/etc. -- those carry
//     semantic meaning and a GLOBAL re-stamp would break the kernel.
//   - The renderer's TC emission with both m & n bound to KAX_GLOBAL
//     drops the sgi==0 guard (see render_uop.c rmu_emit_matmul_tc
//     parallel_tc branch); applying GLOBAL to ONE axis emits the
//     half-bound form (one for-loop survives).
//
// Returns the new STORE root, or `root` unchanged if no RANGE matched.

typedef struct {
  u32 target_axis_id;
} ApplyOptDagGlobalCtx;

static Term apply_opt_dag_global_rewrite(Term t, void *user) {
  ApplyOptDagGlobalCtx const *ctx = (ApplyOptDagGlobalCtx const *)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return t;
  u64 loc = term_val(t);
  u32 axis_id   = (u32)term_val(heap_read(loc + 0));
  u32 axis_type = (u32)term_val(heap_read(loc + 1));
  u32 extent    = (u32)term_val(heap_read(loc + 2));
  if (axis_id != ctx->target_axis_id) return t;
  if (axis_type != KAX_LOOP) return t;
  return uop_range(axis_id, KAX_GLOBAL, extent);
}

fn Term uop_dag_apply_global(Term root, u32 axis_id) {
  ApplyOptDagGlobalCtx ctx = { axis_id };
  UOpGraphRewriteRule rules[] = {
    { "apply-opt-dag-global", apply_opt_dag_global_rewrite },
  };
  return uop_graph_rewrite(root, rules, 1, &ctx);
}

// ---------- KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP (split) ----------
// (KOP_SWAP defined at end of file; reuses split's substitution-map
//  scaffolding.)
// Split the UOP_RANGE leaf at `target_axis` into outer (axis=target,
// LOOP, extent=E/k) + inner (axis=target+1, inner_kax, extent=k).
// All leaves at axis_id > target shift right by 1.  Substitute the
// linear index `IADD(IMUL(outer, k), inner_wrapped)` for any reference
// to the pre-split leaf via Term identity (hash-cons dedups all
// references to the same leaf).
//
// inner_kax is one of KAX_UPCAST/UNROLL/LOCAL/GROUP_REDUCE.
// opt_kind   is UOP_OPT_UPCAST/UNROLL/GROUP_REDUCE (or 0xFF for LOCAL,
// which doesn't wrap inner in UOP_OPT).
//
// Validity: `target_axis` must currently be a single bare UOP_RANGE
// leaf in the DAG with extent % k == 0.  Returns 0 on bail.

// Substitution map: keys are pre-existing Term IDs, values are their
// post-rewrite replacements.  The map only contains ORIGINAL terms;
// the rule never re-fires on its own outputs because the outputs are
// not keys.  Cap is comfortable for typical kernels (one origin per
// axis + one shift per existing axis).
#define APPLY_OPT_DAG_SPLIT_MAP_CAP 64

typedef struct {
  Term key;
  Term val;
} ApplyOptDagSplitMapEntry;

typedef struct {
  ApplyOptDagSplitMapEntry entries[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n;
} ApplyOptDagSplitCtx;

static Term apply_opt_dag_split_rewrite(Term t, void *user) {
  ApplyOptDagSplitCtx const *ctx = (ApplyOptDagSplitCtx const *)user;
  for (u32 i = 0; i < ctx->n; i++) {
    if (ctx->entries[i].key == t) return ctx->entries[i].val;
  }
  return t;
}

// Manual single-pass substitution walker.  Avoids uop_graph_rewrite's
// recurse-on-rule-output semantics which causes infinite recursion when
// substitution targets are bidirectional (e.g. SWAP swaps A and B,
// where mapped(A) == B and mapped(B) == A; the rewriter would loop).
//
// Each pre-existing Term is visited at most once via a Term→Term memo.
// A mapped value is returned as-is (NOT recursed into) since by
// construction the values are pre-existing Terms unrelated to the
// substitution shape.
#define APPLY_OPT_DAG_SUB_MEMO_CAP 1024

typedef struct {
  Term key;
  Term val;
} ApplyOptDagSubMemoEntry;

typedef struct {
  ApplyOptDagSubMemoEntry memo[APPLY_OPT_DAG_SUB_MEMO_CAP];
  u32 memo_n;
  ApplyOptDagSplitCtx const *map;
} ApplyOptDagSubState;

static int apply_opt_dag_sub_memo_lookup(ApplyOptDagSubState *st, Term t,
                                         Term *out) {
  for (u32 i = 0; i < st->memo_n; i++) {
    if (st->memo[i].key == t) { *out = st->memo[i].val; return 1; }
  }
  return 0;
}

static void apply_opt_dag_sub_memo_insert(ApplyOptDagSubState *st, Term k,
                                          Term v) {
  if (st->memo_n < APPLY_OPT_DAG_SUB_MEMO_CAP) {
    st->memo[st->memo_n].key = k;
    st->memo[st->memo_n].val = v;
    st->memo_n++;
  }
}

static Term apply_opt_dag_substitute(Term t, ApplyOptDagSubState *st);

static Term apply_opt_dag_sub_uncached(Term t, ApplyOptDagSubState *st) {
  // Direct map hit: return mapped value AS-IS (no recursion).
  for (u32 i = 0; i < st->map->n; i++) {
    if (st->map->entries[i].key == t) return st->map->entries[i].val;
  }
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  // Leaf opcodes: no children to recurse into.
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return t;
  if (op == UOP_OPT) {
    Term tgt = uop_opt_target(t);
    Term new_tgt = apply_opt_dag_substitute(tgt, st);
    if (new_tgt == tgt) return t;
    return uop_opt(new_tgt, uop_opt_kind(t), uop_opt_factor(t));
  }
  u8 ar = uop_arity(op);
  if (ar == 0) return t;
  Term srcs[MAX_UOP_SRC];
  int changed = 0;
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old = heap_read(loc + i);
    Term ne  = (term_tag(old) == TAG_UOP)
               ? apply_opt_dag_substitute(old, st)
               : old;
    srcs[i] = ne;
    if (ne != old) changed = 1;
  }
  if (!changed) return t;
  return uop_graph_rebuild_with_srcs(t, srcs);
}

static Term apply_opt_dag_substitute(Term t, ApplyOptDagSubState *st) {
  Term hit;
  if (apply_opt_dag_sub_memo_lookup(st, t, &hit)) return hit;
  Term out = apply_opt_dag_sub_uncached(t, st);
  apply_opt_dag_sub_memo_insert(st, t, out);
  return out;
}

// Walk DAG, find the unique UOP_RANGE leaf at target_axis.  Returns 0
// if none found or if multiple distinct leaves at the same axis_id
// exist (the lifter normally produces one).
static Term apply_opt_dag_find_range(Term root, u32 target_axis) {
  if (root == 0) return 0;
  Term stack[256];
  u32  sp = 0;
  stack[sp++] = root;
  Term found = 0;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      if (uop_range_axis_id(t) == target_axis) {
        if (found != 0 && found != t) return 0;  // multiple distinct
        found = t;
      }
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      Term tgt = uop_opt_target(t);
      if (term_tag(tgt) == TAG_UOP && sp < 256) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 256; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return found;
}

static u8 apply_opt_dag_inner_kax(u8 op) {
  if (op == KOP_UPCAST)   return (u8)KAX_UPCAST;
  if (op == KOP_UNROLL)   return (u8)KAX_UNROLL;
  if (op == KOP_LOCAL)    return (u8)KAX_LOCAL;
  if (op == KOP_GROUP)    return (u8)KAX_GROUP_REDUCE;
  if (op == KOP_GROUPTOP) return (u8)KAX_GROUP_REDUCE;
  return (u8)KAX_LOOP;
}

static u32 apply_opt_dag_inner_opt_kind(u8 op) {
  if (op == KOP_UPCAST)                              return UOP_OPT_UPCAST;
  if (op == KOP_UNROLL)                              return UOP_OPT_UNROLL;
  if (op == KOP_GROUP || op == KOP_GROUPTOP)         return UOP_OPT_GROUP_REDUCE;
  return 0xFFu;  // KOP_LOCAL: no UOP_OPT wrap
}

// Collect every UOP_RANGE leaf reachable from root into out[] (deduped
// by Term identity).  Returns the count.
static u32 apply_opt_dag_collect_ranges(Term root, Term *out, u32 cap) {
  if (root == 0) return 0;
  Term stack[256];
  u32  sp = 0;
  stack[sp++] = root;
  u32 n = 0;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      // dedupe
      int seen = 0;
      for (u32 i = 0; i < n; i++) { if (out[i] == t) { seen = 1; break; } }
      if (!seen && n < cap) out[n++] = t;
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      Term tgt = uop_opt_target(t);
      if (term_tag(tgt) == TAG_UOP && sp < 256) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 256; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return n;
}

fn Term uop_dag_apply_split(Term root, u8 op, u32 target_axis, u32 k) {
  if (k == 0) return 0;
  Term old_leaf = apply_opt_dag_find_range(root, target_axis);
  if (old_leaf == 0) return 0;
  u32 extent = uop_range_extent(old_leaf);
  if (extent % k != 0) return 0;
  u32 axis_type_outer = uop_range_axis_type(old_leaf);
  u8  inner_kax       = apply_opt_dag_inner_kax(op);
  u32 opt_kind        = apply_opt_dag_inner_opt_kind(op);

  Term outer = uop_range(target_axis,     axis_type_outer, extent / k);
  Term inner = uop_range(target_axis + 1, inner_kax,       k);
  Term inner_wrapped = (opt_kind != 0xFFu)
                       ? uop_opt(inner, opt_kind, k)
                       : inner;
  Term scaled = uop_int_binary(UOP_IMUL, outer, uop_const(DT_INT32, k));
  Term linear = uop_int_binary(UOP_IADD, scaled, inner_wrapped);

  // Build substitution map: pre-existing leaves → replacement.
  // - old_leaf at target_axis → linear (the IADD/IMUL composition)
  // - any leaf at axis_id > target_axis → same range with axis_id+1
  // The rule lookup is by Term identity; outputs are NOT keys, so the
  // rewriter's recursion on rule outputs naturally fixpoints.
  ApplyOptDagSplitCtx ctx;
  ctx.n = 0;
  ctx.entries[ctx.n].key = old_leaf;
  ctx.entries[ctx.n].val = linear;
  ctx.n++;

  Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n_ranges = apply_opt_dag_collect_ranges(root, ranges,
                                              APPLY_OPT_DAG_SPLIT_MAP_CAP);
  for (u32 i = 0; i < n_ranges && ctx.n < APPLY_OPT_DAG_SPLIT_MAP_CAP; i++) {
    Term r = ranges[i];
    if (r == old_leaf) continue;
    u32 a_id = uop_range_axis_id(r);
    if (a_id <= target_axis) continue;
    u32 a_type = uop_range_axis_type(r);
    u32 a_ext  = uop_range_extent(r);
    ctx.entries[ctx.n].key = r;
    ctx.entries[ctx.n].val = uop_range(a_id + 1, a_type, a_ext);
    ctx.n++;
  }

  ApplyOptDagSubState st;
  st.memo_n = 0;
  st.map    = &ctx;
  return apply_opt_dag_substitute(root, &st);
}

// ---------- KOP_SWAP ------------------------------------------------
// Swap axis_ids of every UOP_RANGE leaf at positions (a, b).  Both
// positions must already exist in the DAG (otherwise no-op).  Reuses
// the split-machinery's substitution-map idempotence: outputs are NOT
// keys, so the rewriter's recursion on rule outputs naturally fixpoints.
//
// Returns the new root, or `root` unchanged if no RANGE matched.

fn Term uop_dag_apply_swap(Term root, u32 axis_a, u32 axis_b) {
  if (axis_a == axis_b) return root;
  Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n_ranges = apply_opt_dag_collect_ranges(root, ranges,
                                              APPLY_OPT_DAG_SPLIT_MAP_CAP);
  ApplyOptDagSplitCtx ctx;
  ctx.n = 0;
  for (u32 i = 0; i < n_ranges && ctx.n < APPLY_OPT_DAG_SPLIT_MAP_CAP; i++) {
    Term r = ranges[i];
    u32 a_id = uop_range_axis_id(r);
    if (a_id != axis_a && a_id != axis_b) continue;
    u32 new_id = (a_id == axis_a) ? axis_b : axis_a;
    u32 a_type = uop_range_axis_type(r);
    u32 a_ext  = uop_range_extent(r);
    ctx.entries[ctx.n].key = r;
    ctx.entries[ctx.n].val = uop_range(new_id, a_type, a_ext);
    ctx.n++;
  }
  if (ctx.n == 0) return root;
  ApplyOptDagSubState st;
  st.memo_n = 0;
  st.map    = &ctx;
  return apply_opt_dag_substitute(root, &st);
}

// ---------- top-level dispatcher ------------------------------------

fn Term uop_dag_apply_kopt(Term root, KOpt opt) {
  switch (opt.op) {
    case KOP_TC:
      return uop_dag_apply_tc(root, opt.arg);
    case KOP_GLOBAL:
      return uop_dag_apply_global(root, opt.axis);
    case KOP_SWAP:
      return uop_dag_apply_swap(root, opt.axis, opt.arg);
    case KOP_UPCAST:
    case KOP_UNROLL:
    case KOP_LOCAL:
    case KOP_GROUP:
    case KOP_GROUPTOP:
      return uop_dag_apply_split(root, opt.op, opt.axis, opt.arg);
    default:
      return 0;  // unsupported (PADTO, NOLOCALS reserved)
  }
}
