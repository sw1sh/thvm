// scalar/simplify.c -- harness for the scalar-UOp graph simplification
// pass.  Mirrors src/uop/graph_rewrite.c but operates on the per-kernel
// ScalarUop[] arena instead of the heap-resident TAG_UOP DAG.
//
// Phase 2 of the tinygrad symbolic/index rule port: this file ships the
// driver only.  No rewrite rules live here yet -- subsequent phases drop
// rule tables in (divandmod, symbolic, ...).
//
// The driver walks the arena bottom-up (slot ids are emitted in
// post-order by rangeify), follows remap chains so a parent always sees
// the latest rewritten child, then offers each rule a chance to fire on
// the freshly-resolved node.  When a rule returns nonzero, that becomes
// the new id for the slot via remap[]; the next round picks it up when
// any parent revisits the slot.  We iterate to fixpoint with a hard cap
// on outer rounds to bound pathological cycles.
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

fn u32 scalar_simplify_apply(ScalarUop *uops, u32 *n_uops_inout,
                              u32 root_id, ScalarSimplifyRule const *rules,
                              u32 n_rules, void *user) {
  scalar_simplify_stats_clear();
  if (uops == NULL || n_uops_inout == NULL || root_id == 0) {
    return root_id;
  }
  u32 n_uops = *n_uops_inout;
  if (root_id >= n_uops || rules == NULL || n_rules == 0) {
    return root_id;
  }

  u32 *remap = (u32 *)malloc((size_t)n_uops * sizeof(u32));
  if (remap == NULL) {
    return root_id;
  }
  for (u32 i = 0; i < n_uops; i++) {
    remap[i] = i;
  }

  for (u32 round = 0; round < SCALAR_SIMPLIFY_MAX_ROUNDS; round++) {
    int any_fired = 0;
    // Re-read the count -- a future rule that allocates new slots via
    // *n_uops_inout would have grown the live count.
    n_uops = *n_uops_inout;
    // Children are emitted before parents in the rangeify arena, so a
    // forward sweep visits each node after its sources -- equivalent to
    // post-order DFS without an explicit stack.
    for (u32 id = 1; id < n_uops; id++) {
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
        u32 next = rules[r].apply(uops, n_uops, id, user);
        if (next == 0 || next == id) continue;
        // n_uops_inout may have grown if the rule allocated; rebound.
        n_uops = *n_uops_inout;
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
