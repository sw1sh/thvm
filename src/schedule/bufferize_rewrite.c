// schedule/bufferize_rewrite.c - rewrite-stats stubs kept for test
// compatibility.  The actual rewrite harness (bufferize_rewrite_apply +
// the RealizeRewriteRule type) retired when the last named rule moved
// inline pre-seed (commit 099cbbea); these accessors now return 0
// (or the cleared length) so test_bufferize_classify.c still links.

#define REALIZE_REWRITE_STATS_CAP 64

typedef struct {
  char const *name;
  u32         hits;
} RealizeRewriteStat;

static RealizeRewriteStat REALIZE_REWRITE_STATS[REALIZE_REWRITE_STATS_CAP];
static u32                REALIZE_REWRITE_STATS_LEN = 0;

fn void bufferize_rewrite_stats_clear(void) {
  REALIZE_REWRITE_STATS_LEN = 0;
}

fn u32 bufferize_rewrite_stats_len(void) {
  return REALIZE_REWRITE_STATS_LEN;
}

fn u32 bufferize_rewrite_stat_hits(char const *name) {
  (void)name;
  return 0;
}
