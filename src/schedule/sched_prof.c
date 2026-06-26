// schedule/sched_prof.c - coarse per-phase wall-time profiler for the
// cold materialize/schedule/render pipeline.  All gated by
// THVM_SCHED_PROF=1; a default build never arms the atexit dump and the
// per-phase timers compile to a cached predicted branch.
#include <time.h>

double SCHED_PROF_SEC[SCHED_PROF_N];
u64    SCHED_PROF_CNT[SCHED_PROF_N];
int    SCHED_PROF_ENABLED = -1;

static const char *SCHED_PROF_NAME[SCHED_PROF_N] = {
  "bufferize_classify",
  "  rangeify_unified",
  "topo_sort_buffers",
  "arena_compute",
  "emit_kernels",
  "  render_uop",
  "  visit_walk",
  "heap_rooted_preserve",
  "gc_preserve",
  "kernel_gc_sweep",
  "thvm_materialize(total)",
  "thvm_realize(TOTAL)",
  "  wnf(compile+dispatch)",
  "  precompile_range",
  "  rollback+reclaim",
};

static void sched_prof_dump(void) {
  double total = 0.0;
  for (int i = 0; i < SCHED_PROF_N; i++) total += SCHED_PROF_SEC[i];
  fprintf(stderr, "\n=== THVM_SCHED_PROF (per-phase, accumulated) ===\n");
  for (int i = 0; i < SCHED_PROF_N; i++) {
    if (SCHED_PROF_CNT[i] == 0 && SCHED_PROF_SEC[i] == 0.0) continue;
    fprintf(stderr, "  %-26s %9.3f ms   (%llu calls)\n",
            SCHED_PROF_NAME[i], SCHED_PROF_SEC[i] * 1e3,
            (unsigned long long)SCHED_PROF_CNT[i]);
  }
  fprintf(stderr, "================================================\n");
}

int sched_prof_enabled(void) {
  if (SCHED_PROF_ENABLED < 0) {
    const char *e = getenv("THVM_SCHED_PROF");
    SCHED_PROF_ENABLED = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
    if (SCHED_PROF_ENABLED) atexit(sched_prof_dump);
  }
  return SCHED_PROF_ENABLED;
}

double sched_prof_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void sched_prof_add(int slot, double t0) {
  SCHED_PROF_SEC[slot] += sched_prof_now() - t0;
  SCHED_PROF_CNT[slot]++;
}
