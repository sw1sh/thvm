// scalar/simplify.c -- harness for the scalar-UOp graph simplification
// pass + the divandmod rule table.  Mirrors src/uop/graph_rewrite.c but
// operates on the per-kernel ScalarUop[] arena instead of the heap-
// resident TAG_UOP DAG.
//
// Phase 3 of the tinygrad symbolic/index rule port: ports the
// divandmod.py rules that tinygrad runs on integer index expressions.
// The high-leverage rules (nested-div-mod, nested-div-mod-mod, add-div-
// split, factor-remainder) collapse cascaded MOD/IDIV expressions that
// arise from movement-op composition (reshape o stride o pad).
//
// The driver walks the arena bottom-up (slot ids are emitted in
// post-order by rangeify), follows remap chains so a parent always sees
// the latest rewritten child, then offers each rule a chance to fire on
// the freshly-resolved node.  When a rule returns nonzero, that becomes
// the new id for the slot via remap[]; the next round picks it up when
// any parent revisits the slot.  We iterate to fixpoint with a hard cap
// on outer rounds to bound pathological cycles.
//
// Rules may allocate new arena nodes via rangeify_emit_* on the
// KernelEntry passed through `user`; the driver re-reads the arena head
// after each rule call to follow the realloc.
//
// Stats live in a flat table keyed by rule name, mirroring the layout
// in src/uop/graph_rewrite.c so DUMP_SCALAR_SIMPLIFY=1 produces the
// same shape of output as DUMP_UOP_REWRITE.

#define SCALAR_SIMPLIFY_STATS_CAP 128
#define SCALAR_SIMPLIFY_MAX_ROUNDS 64

typedef struct {
  char const *name;
  u32         hits;
} ScalarSimplifyStat;

static ScalarSimplifyStat SCALAR_SIMPLIFY_STATS[SCALAR_SIMPLIFY_STATS_CAP];
static u32                SCALAR_SIMPLIFY_STATS_LEN = 0;

fn void scalar_simplify_stats_clear(void) {
  SCALAR_SIMPLIFY_STATS_LEN = 0;
}

static void scalar_simplify_stats_record(char const *name, u32 hits) {
  if (name == NULL) {
    return;
  }
  for (u32 i = 0; i < SCALAR_SIMPLIFY_STATS_LEN; i++) {
    if (strcmp(SCALAR_SIMPLIFY_STATS[i].name, name) == 0) {
      SCALAR_SIMPLIFY_STATS[i].hits += hits;
      return;
    }
  }
  if (SCALAR_SIMPLIFY_STATS_LEN >= SCALAR_SIMPLIFY_STATS_CAP) {
    return;
  }
  SCALAR_SIMPLIFY_STATS[SCALAR_SIMPLIFY_STATS_LEN].name = name;
  SCALAR_SIMPLIFY_STATS[SCALAR_SIMPLIFY_STATS_LEN].hits = hits;
  SCALAR_SIMPLIFY_STATS_LEN++;
}

fn u32 scalar_simplify_stats_len(void) {
  return SCALAR_SIMPLIFY_STATS_LEN;
}

fn char const *scalar_simplify_stat_name(u32 i) {
  return i < SCALAR_SIMPLIFY_STATS_LEN
       ? SCALAR_SIMPLIFY_STATS[i].name
       : "";
}

fn u32 scalar_simplify_stat_hits_at(u32 i) {
  return i < SCALAR_SIMPLIFY_STATS_LEN
       ? SCALAR_SIMPLIFY_STATS[i].hits
       : 0;
}

fn u32 scalar_simplify_stat_hits(char const *name) {
  if (name == NULL) {
    return 0;
  }
  for (u32 i = 0; i < SCALAR_SIMPLIFY_STATS_LEN; i++) {
    if (strcmp(SCALAR_SIMPLIFY_STATS[i].name, name) == 0) {
      return SCALAR_SIMPLIFY_STATS[i].hits;
    }
  }
  return 0;
}

static int scalar_simplify_dump_enabled(void) {
  char const *e = getenv("DUMP_SCALAR_SIMPLIFY");
  return e != NULL && e[0] == '1';
}

fn void scalar_simplify_stats_print(void) {
  fprintf(stderr, "scalar_simplify_summary rules=%u\n",
          SCALAR_SIMPLIFY_STATS_LEN);
  for (u32 i = 0; i < SCALAR_SIMPLIFY_STATS_LEN; i++) {
    fprintf(stderr, "  %s hits=%u\n",
            SCALAR_SIMPLIFY_STATS[i].name,
            SCALAR_SIMPLIFY_STATS[i].hits);
  }
}

// Walk a remap chain to its terminal id.  Self-referential entries
// (remap[i] == i) terminate; otherwise we follow the chain.  No path
// compression -- the iteration count is bounded by the cap on outer
// rounds, so chains stay short in practice.
static u32 scalar_simplify_resolve(u32 const *remap, u32 n_uops, u32 id) {
  u32 cur = id;
  for (u32 hops = 0; hops < n_uops; hops++) {
    if (cur >= n_uops) {
      return cur;
    }
    u32 next = remap[cur];
    if (next == cur) {
      return cur;
    }
    cur = next;
  }
  return cur;
}

// Grow the remap[] table to cover slots up to `needed`.  New entries are
// initialised to identity (remap[i] = i) so freshly-allocated nodes pass
// through resolve() unchanged.
static u32 *scalar_simplify_remap_grow(u32 *remap, u32 *cap_inout, u32 needed) {
  u32 cap = *cap_inout;
  if (needed <= cap) return remap;
  u32 new_cap = cap == 0 ? 16 : cap;
  while (new_cap < needed) new_cap *= 2;
  u32 *grown = (u32 *)realloc(remap, (size_t)new_cap * sizeof(u32));
  if (grown == NULL) return remap;
  for (u32 i = cap; i < new_cap; i++) grown[i] = i;
  *cap_inout = new_cap;
  return grown;
}

fn u32 scalar_simplify_apply(ScalarUop **uops_inout, u32 *n_uops_inout,
                              u32 root_id, ScalarSimplifyRule const *rules,
                              u32 n_rules, void *user) {
  scalar_simplify_stats_clear();
  if (uops_inout == NULL || n_uops_inout == NULL || root_id == 0) {
    return root_id;
  }
  u32 n_uops = *n_uops_inout;
  if (root_id >= n_uops || rules == NULL || n_rules == 0) {
    return root_id;
  }

  u32 remap_cap = 0;
  u32 *remap = scalar_simplify_remap_grow(NULL, &remap_cap, n_uops);
  if (remap == NULL) {
    return root_id;
  }

  for (u32 round = 0; round < SCALAR_SIMPLIFY_MAX_ROUNDS; round++) {
    int any_fired = 0;
    // Re-read the count -- a rule that allocated new slots will have
    // grown the live count.
    n_uops = *n_uops_inout;
    remap = scalar_simplify_remap_grow(remap, &remap_cap, n_uops);
    // Children are emitted before parents in the rangeify arena, so a
    // forward sweep visits each node after its sources -- equivalent to
    // post-order DFS without an explicit stack.
    for (u32 id = 1; id < n_uops; id++) {
      ScalarUop *uops = *uops_inout;
      ScalarUop *u = &uops[id];
      // Resolve any source remaps so rules see the latest child ids.
      for (u8 s = 0; s < u->src_count; s++) {
        u32 raw = u->src[s];
        if (raw == 0) continue;
        u32 res = scalar_simplify_resolve(remap, n_uops, raw);
        if (res != raw) {
          u->src[s] = res;
        }
      }
      // Don't re-fire on slots already remapped this round.
      if (remap[id] != id) continue;

      for (u32 r = 0; r < n_rules; r++) {
        if (rules[r].apply == NULL) continue;
        u32 next = rules[r].apply(uops_inout, n_uops_inout, id, user);
        if (next == 0 || next == id) continue;
        // n_uops_inout may have grown if the rule allocated; rebound.
        n_uops = *n_uops_inout;
        remap = scalar_simplify_remap_grow(remap, &remap_cap, n_uops);
        if (next >= n_uops) continue;
        remap[id] = next;
        scalar_simplify_stats_record(rules[r].name, 1);
        any_fired = 1;
        break;
      }
    }
    if (!any_fired) break;
  }

  u32 new_root = scalar_simplify_resolve(remap, n_uops, root_id);
  free(remap);

  if (scalar_simplify_dump_enabled()) {
    scalar_simplify_stats_print();
  }
  return new_root;
}

// ============================================================
// divandmod rules (port of tinygrad/uop/divandmod.py).
// ============================================================
//
// Each rule is structural: it pattern-matches on the scalar-UOp graph
// shape and emits replacement nodes via rangeify_emit_*.  A rule
// returns the new node id, or 0 to indicate no match.  Conditions on
// constants (denominator > 0, divisibility) are checked literally;
// conditions on value ranges (numerator >= 0) are answered
// conservatively by `simplify_value_nonneg` -- a small structural
// estimator that recognises iter-arithmetic over S_RANGE iters and
// non-negative S_ICONSTs.

// Sign-extend a ScalarUop.extra payload as i64 (S_ICONST stores its
// literal there).
static i64 simplify_iconst_extra(ScalarUop const *u) {
  return (i64)u->extra;
}

// Conservative non-negative estimator over scalar integer ops.  Returns
// 1 only when the value is provably >= 0 by structure.  Mirrors the
// invariants under which tinygrad applies its truncating-divmod rules.
static int simplify_value_nonneg(ScalarUop const *uops, u32 n, u32 id) {
  if (id == 0 || id >= n) return 0;
  ScalarUop const *u = &uops[id];
  switch (u->op) {
    case S_RANGE:
      // All axis types are 0-based loop iterators in [0, extent).
      return 1;
    case S_ICONST:
      return simplify_iconst_extra(u) >= 0;
    case S_IADD:
    case S_IMUL:
      return simplify_value_nonneg(uops, n, u->src[0]) &&
             simplify_value_nonneg(uops, n, u->src[1]);
    case S_IDIV:
    case S_IMOD: {
      if (u->src[1] == 0 || u->src[1] >= n) return 0;
      ScalarUop const *d = &uops[u->src[1]];
      if (d->op != S_ICONST || simplify_iconst_extra(d) <= 0) return 0;
      return simplify_value_nonneg(uops, n, u->src[0]);
    }
    default:
      return 0;
  }
}

// Fetch a literal i64 constant from a child.  Returns 1 and writes *out
// on success, 0 if the child is not an S_ICONST.
static int simplify_match_iconst(ScalarUop const *uops, u32 n, u32 id, i64 *out) {
  if (id == 0 || id >= n) return 0;
  ScalarUop const *u = &uops[id];
  if (u->op != S_ICONST) return 0;
  *out = simplify_iconst_extra(u);
  return 1;
}

// (x % (k*c)) // c  ->  (x // c) % k        (kc % c == 0, c > 0, kc > 0)
// Mirrors divandmod.py:26-27 IDIV branch.
static u32 rule_nested_div_mod(ScalarUop **uops_inout, u32 *n_uops_inout,
                               u32 node_id, void *user) {
  ScalarUop *uops = *uops_inout;
  u32 n = *n_uops_inout;
  if (node_id >= n) return 0;
  ScalarUop const *div = &uops[node_id];
  if (div->op != S_IDIV || div->src_count != 2) return 0;

  i64 c;
  if (!simplify_match_iconst(uops, n, div->src[1], &c) || c <= 0) return 0;

  u32 mod_id = div->src[0];
  if (mod_id == 0 || mod_id >= n) return 0;
  ScalarUop const *mod = &uops[mod_id];
  if (mod->op != S_IMOD || mod->src_count != 2) return 0;

  i64 kc;
  if (!simplify_match_iconst(uops, n, mod->src[1], &kc)) return 0;
  if (kc <= 0 || c == 0 || kc % c != 0) return 0;
  i64 k = kc / c;

  KernelEntry *ke = (KernelEntry *)user;
  u32 x_id     = mod->src[0];
  u32 c_id     = div->src[1];
  u32 div_xc   = rangeify_emit_binary(ke, S_IDIV, div->dtype, x_id, c_id);
  u32 k_const  = rangeify_emit_leaf  (ke, S_ICONST, div->dtype, (u64)k);
  u32 mod_xck  = rangeify_emit_binary(ke, S_IMOD, div->dtype, div_xc, k_const);
  *uops_inout  = ke->scalar_uops;
  return mod_xck;
}

// (x % (k*c)) % c  ->  x % c                (kc % c == 0, c > 0, kc > 0)
// Mirrors divandmod.py:26-27 MOD branch.
static u32 rule_nested_div_mod_mod(ScalarUop **uops_inout, u32 *n_uops_inout,
                                   u32 node_id, void *user) {
  ScalarUop *uops = *uops_inout;
  u32 n = *n_uops_inout;
  if (node_id >= n) return 0;
  ScalarUop const *outer = &uops[node_id];
  if (outer->op != S_IMOD || outer->src_count != 2) return 0;

  i64 c;
  if (!simplify_match_iconst(uops, n, outer->src[1], &c) || c <= 0) return 0;

  u32 inner_id = outer->src[0];
  if (inner_id == 0 || inner_id >= n) return 0;
  ScalarUop const *inner = &uops[inner_id];
  if (inner->op != S_IMOD || inner->src_count != 2) return 0;

  i64 kc;
  if (!simplify_match_iconst(uops, n, inner->src[1], &kc)) return 0;
  if (kc <= 0 || kc % c != 0) return 0;

  KernelEntry *ke = (KernelEntry *)user;
  u32 x_id    = inner->src[0];
  u32 c_id    = outer->src[1];
  u32 mod_xc  = rangeify_emit_binary(ke, S_IMOD, outer->dtype, x_id, c_id);
  *uops_inout = ke->scalar_uops;
  return mod_xc;
}

// (x + c) // d  ->  (x + (c % d)) // d + (c // d)
// Mirrors divandmod.py:112-113.  Requires c >= 0, d > 0, x non-negative
// by structure, and c >= d (i.e. `c % d != c`).  Hoists the integer
// part of c out of the division so a downstream constant-folder can
// merge it with sibling constants.
static u32 rule_add_div_split(ScalarUop **uops_inout, u32 *n_uops_inout,
                              u32 node_id, void *user) {
  ScalarUop *uops = *uops_inout;
  u32 n = *n_uops_inout;
  if (node_id >= n) return 0;
  ScalarUop const *div = &uops[node_id];
  if (div->op != S_IDIV || div->src_count != 2) return 0;

  i64 d;
  if (!simplify_match_iconst(uops, n, div->src[1], &d) || d <= 0) return 0;

  u32 add_id = div->src[0];
  if (add_id == 0 || add_id >= n) return 0;
  ScalarUop const *add = &uops[add_id];
  if (add->op != S_IADD || add->src_count != 2) return 0;

  // Identify which side of the IADD is the constant.  We require it to
  // be a non-negative literal; the other side must be structurally
  // non-negative for the truncating-div identity to hold.
  u32 const_sid = 0, var_sid = 0;
  i64 c = 0;
  for (u8 i = 0; i < 2; i++) {
    i64 v;
    if (simplify_match_iconst(uops, n, add->src[i], &v) && v >= 0) {
      const_sid = add->src[i];
      var_sid   = add->src[i ^ 1];
      c = v;
      break;
    }
  }
  if (const_sid == 0 || var_sid == 0) return 0;
  if (!simplify_value_nonneg(uops, n, var_sid)) return 0;
  // c % d != c <=> c >= d for non-negative c.
  if (c < d) return 0;
  (void)const_sid;

  KernelEntry *ke   = (KernelEntry *)user;
  u32 dtype         = div->dtype;
  i64 c_mod_d       = c % d;
  i64 c_div_d       = c / d;
  u32 d_id          = div->src[1];
  u32 c_mod_const   = rangeify_emit_leaf  (ke, S_ICONST, dtype, (u64)c_mod_d);
  u32 new_add       = rangeify_emit_binary(ke, S_IADD,   dtype, var_sid, c_mod_const);
  u32 new_div       = rangeify_emit_binary(ke, S_IDIV,   dtype, new_add, d_id);
  u32 c_div_const   = rangeify_emit_leaf  (ke, S_ICONST, dtype, (u64)c_div_d);
  u32 result        = rangeify_emit_binary(ke, S_IADD,   dtype, new_div, c_div_const);
  *uops_inout       = ke->scalar_uops;
  return result;
}

static ScalarSimplifyRule SCALAR_SIMPLIFY_DIVANDMOD_RULES[] = {
  {"nested-div-mod",     rule_nested_div_mod},
  {"nested-div-mod-mod", rule_nested_div_mod_mod},
  {"add-div-split",      rule_add_div_split},
};

fn ScalarSimplifyRule const *scalar_simplify_divandmod_rules(u32 *n_out) {
  if (n_out != NULL) {
    *n_out = (u32)(sizeof(SCALAR_SIMPLIFY_DIVANDMOD_RULES) /
                   sizeof(SCALAR_SIMPLIFY_DIVANDMOD_RULES[0]));
  }
  return SCALAR_SIMPLIFY_DIVANDMOD_RULES;
}
