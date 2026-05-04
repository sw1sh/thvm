// schedule/bufferize_rewrite.c - tinygrad-style rewrite harness for
// realization-boundary decisions.
//
// The first scheduling rewrite surface is the realize map:
// `bufferize_classify` seeds conservative BUFFERIZE-like boundaries,
// then named rules rewrite those boundary bits under explicit
// legality/cost guards.  This keeps fusion policy inspectable and
// gives later scalar/tile graph rewrites the same rule-table shape.

#define REALIZE_REWRITE_STATS_CAP 64

typedef u32 (*RealizeRewriteFn)(Term root);

typedef struct {
  char const       *name;
  RealizeRewriteFn apply;
} RealizeRewriteRule;

typedef struct {
  char const *name;
  u32         hits;
} RealizeRewriteStat;

static RealizeRewriteStat REALIZE_REWRITE_STATS[REALIZE_REWRITE_STATS_CAP];
static u32                REALIZE_REWRITE_STATS_LEN = 0;

fn void bufferize_rewrite_stats_clear(void) {
  REALIZE_REWRITE_STATS_LEN = 0;
}

static void bufferize_rewrite_stats_record(char const *name, u32 hits) {
  if (name == NULL) return;
  for (u32 i = 0; i < REALIZE_REWRITE_STATS_LEN; i++) {
    if (strcmp(REALIZE_REWRITE_STATS[i].name, name) == 0) {
      REALIZE_REWRITE_STATS[i].hits += hits;
      return;
    }
  }
  if (REALIZE_REWRITE_STATS_LEN >= REALIZE_REWRITE_STATS_CAP) return;
  REALIZE_REWRITE_STATS[REALIZE_REWRITE_STATS_LEN].name = name;
  REALIZE_REWRITE_STATS[REALIZE_REWRITE_STATS_LEN].hits = hits;
  REALIZE_REWRITE_STATS_LEN++;
}

fn u32 bufferize_rewrite_stats_len(void) {
  return REALIZE_REWRITE_STATS_LEN;
}

fn char const *bufferize_rewrite_stat_name(u32 i) {
  return i < REALIZE_REWRITE_STATS_LEN ? REALIZE_REWRITE_STATS[i].name : "";
}

fn u32 bufferize_rewrite_stat_hits_at(u32 i) {
  return i < REALIZE_REWRITE_STATS_LEN ? REALIZE_REWRITE_STATS[i].hits : 0;
}

fn u32 bufferize_rewrite_stat_hits(char const *name) {
  if (name == NULL) return 0;
  for (u32 i = 0; i < REALIZE_REWRITE_STATS_LEN; i++) {
    if (strcmp(REALIZE_REWRITE_STATS[i].name, name) == 0) {
      return REALIZE_REWRITE_STATS[i].hits;
    }
  }
  return 0;
}

static int bufferize_rewrite_dump_enabled(void) {
  char const *e = getenv("DUMP_REWRITE");
  if (e != NULL && e[0] == '1') return 1;
  e = getenv("DUMP_FUSION_REWRITE");
  return e != NULL && e[0] == '1';
}

static void bufferize_rewrite_stats_dump(void) {
  if (!bufferize_rewrite_dump_enabled()) return;
  fprintf(stderr, "bufferize_rewrite_summary rules=%u\n",
          REALIZE_REWRITE_STATS_LEN);
  for (u32 i = 0; i < REALIZE_REWRITE_STATS_LEN; i++) {
    fprintf(stderr, "  %s hits=%u\n",
            REALIZE_REWRITE_STATS[i].name,
            REALIZE_REWRITE_STATS[i].hits);
  }
}

static void bufferize_rewrite_apply(Term root,
                                  RealizeRewriteRule const *rules,
                                  u32 n_rules) {
  for (u32 i = 0; i < n_rules; i++) {
    // Stamp the current rule name so bufferize_node_mark / bufferize_node_unmark
    // can attribute their bufferize-graph effects to it.  Phase 1 of
    // docs/plans/bufferize.md - the rule's hit count and the
    // removed_by/added_by stamps must agree.
    bufferize_set_current_rule(rules[i].name);
    u32 hits = rules[i].apply != NULL ? rules[i].apply(root) : 0;
    bufferize_set_current_rule(NULL);
    bufferize_rewrite_stats_record(rules[i].name, hits);
  }
}
