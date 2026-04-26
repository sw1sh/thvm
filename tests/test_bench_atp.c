// test_bench_atp.c -- stage 7.4c bench harness for the IC-native ATP.
//
// Walks `tests/data/atp/*.pr`, runs our ATP on each (with a fixed
// step budget), records 8 metrics per run into `build/bench-atp.csv`,
// and soft-checks each result against the matching `.expect`:
// only the final ATP status (PROVED / TIMEOUT / QUEUE_EMPTY) is
// asserted; step/rule counts are written to the CSV but not gated
// (drift is a measurement, not a regression).

#include "../src/thvm.c"
#include "test.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// 8.3e-iii: budget reduced from 256 to 32 because IC-routed
// rewrite (use_ic_rewrite=1) allocates ~6 heap cells per APP-PRI
// chain, and a 256-step saturation on the harder fixtures
// (group_commutative_inverse.pr) blows past the 16M-cell HEAP_CAP.
// 32 keeps all 4 modes within heap bounds and is enough to
// observe the per-step latency.
#define BENCH_STEP_BUDGET 32
#define BENCH_MAX_FILES   64
#define BENCH_PATH_LEN    256

static const char *atp_status_str(AtpStatus st) {
  switch (st) {
    case ATP_RUNNING:     return "RUNNING";
    case ATP_PROVED:      return "PROVED";
    case ATP_REFUTED:     return "REFUTED";
    case ATP_TIMEOUT:     return "TIMEOUT";
    case ATP_QUEUE_EMPTY: return "QUEUE_EMPTY";
  }
  return "?";
}

// Read a `.expect` file and copy `status=<value>` into `status_out`.
// Lines starting with `%` are comments (matching `.pr` lexer); blank
// lines are skipped.  Returns 0 on success, -1 on file-open failure.
static int read_expect_status(const char *path, char *status_out, u32 cap) {
  status_out[0] = 0;
  FILE *f = fopen(path, "r");
  if (f == NULL) return -1;
  char line[256];
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '%' || line[0] == '\n' || line[0] == '\r') continue;
    if (strncmp(line, "status=", 7) == 0) {
      const char *src = line + 7;
      u32 i = 0;
      while (i + 1 < cap && *src && *src != '\n' && *src != '\r' && *src != ' ') {
        status_out[i++] = *src++;
      }
      status_out[i] = 0;
    }
  }
  fclose(f);
  return 0;
}

// Run the ATP on a single .pr file and return the metrics.  Caller
// supplies the spec and ATP state; we just plumb the call.
//
// `use_ic_cp_gen` toggles 8.1e-i's flag on the AtpState before
// adding axioms / running -- selects between the C-direct and
// IC-routed CP enumerators.  `use_ic_rewrite` toggles 8.3e-i's
// flag selecting between the C-direct and IC-routed rewrite
// normalize paths.
static AtpStatus run_one(const char *pr_path,
                        u8 use_ic_cp_gen, u8 use_ic_rewrite,
                        AtpState **out_atp,
                        WaldSpec **out_spec, double *out_wall_ms) {
  WaldSpec *spec = wald_init();
  WaldErr e = wald_parse_file(pr_path, spec);
  if (e != WALD_OK) {
    wald_free(spec);
    *out_atp = NULL;
    *out_spec = NULL;
    return ATP_RUNNING;   // sentinel for "parse failed"
  }

  // Build KboConfig from parsed precedences, +1 shift so rank 0
  // is distinguishable from "unset".
  static u32 weights[64];
  static u32 prec[64];
  for (u32 i = 0; i < 64; i++) { weights[i] = 0; prec[i] = 0; }
  u32 max_label = 0;
  for (u32 i = 0; i < spec->n_symbols; i++) {
    if (spec->symbols[i].label > max_label) max_label = spec->symbols[i].label;
  }
  for (u32 i = 0; i < spec->n_symbols; i++) {
    weights[spec->symbols[i].label] = 1;
    prec[spec->symbols[i].label]    = spec->symbols[i].prec_rank + 1;
  }
  static KboConfig cfg;
  cfg.weights    = weights;
  cfg.precedence = prec;
  cfg.n_labels   = max_label + 1;
  cfg.var_weight = 1;

  // 8.5d: when the .pr file declared `ORDERING LPO`, also build
  // an LpoConfig from the precedences and attach it -- AtpState's
  // dispatcher (`atp_compare`) prefers LPO when both are set.
  static LpoConfig lpo_cfg;
  lpo_cfg.precedence = prec;
  lpo_cfg.n_labels   = max_label + 1;

  AtpState *atp = thvm_atp_init(&cfg, BENCH_STEP_BUDGET);
  atp->use_ic_cp_gen  = use_ic_cp_gen;
  atp->use_ic_rewrite = use_ic_rewrite;
  if (spec->ordering_kind == WALD_ORDER_LPO) {
    thvm_atp_set_lpo(atp, &lpo_cfg);
  }
  for (u32 i = 0; i < spec->n_eqns; i++) {
    thvm_atp_add_equation(atp, spec->eqn_lhs[i], spec->eqn_rhs[i]);
  }
  if (spec->n_existential > 0) {
    // 8.9d: spec declared an EXISTS section -- run in narrow mode.
    thvm_atp_set_goal_existential(atp, spec->goal_lhs, spec->goal_rhs);
  } else {
    thvm_atp_set_goal(atp, spec->goal_lhs, spec->goal_rhs);
  }

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  AtpStatus st = thvm_atp_run(atp);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  *out_wall_ms = (t1.tv_sec - t0.tv_sec) * 1e3
               + (t1.tv_nsec - t0.tv_nsec) / 1e6;
  *out_atp  = atp;
  *out_spec = spec;
  return st;
}

int main(void) {
  thvm_init();

  // Best-effort mkdir build/.  Existing dir is fine.
  mkdir("build", 0755);

  FILE *csv = fopen("build/bench-atp.csv", "w");
  CHECK(csv != NULL);
  if (csv == NULL) { thvm_free(); return 1; }
  fprintf(csv, "file,mode,status,wall_ms,step,n_rules,n_trace,"
               "drop_joinable,drop_connected,drop_rule_subsumed,"
               "drop_queue_subsumed\n");

  // Collect .pr file names from tests/data/atp/.
  DIR *dir = opendir("tests/data/atp");
  CHECK(dir != NULL);
  if (dir == NULL) { fclose(csv); thvm_free(); return 1; }

  char files[BENCH_MAX_FILES][BENCH_PATH_LEN];
  u32 n_files = 0;
  struct dirent *de;
  while ((de = readdir(dir)) != NULL) {
    const char *name = de->d_name;
    size_t len = strlen(name);
    if (len < 4) continue;
    if (strcmp(name + len - 3, ".pr") != 0) continue;
    if (n_files >= BENCH_MAX_FILES) continue;
    snprintf(files[n_files], BENCH_PATH_LEN, "tests/data/atp/%s", name);
    n_files++;
  }
  closedir(dir);

  // Sort lexicographically for deterministic CSV order.
  for (u32 i = 1; i < n_files; i++) {
    for (u32 j = i; j > 0 && strcmp(files[j - 1], files[j]) > 0; j--) {
      char tmp[BENCH_PATH_LEN];
      memcpy(tmp, files[j - 1], BENCH_PATH_LEN);
      memcpy(files[j - 1], files[j], BENCH_PATH_LEN);
      memcpy(files[j], tmp, BENCH_PATH_LEN);
    }
  }

  // Per-file: run under each (cp-gen, rewrite) mode in the
  // 2x2 cross product.  Mode label is two letters: first
  // character is cp-gen path ('c' or 'i'), second is rewrite
  // path ('c' or 'i').  Emits one CSV row per (file, mode);
  // soft-checks status against `.expect`.  All four modes must
  // produce the same status (parity confirmation from 8.1e-ii
  // and 8.3e-ii); their wall-clock numbers feed the bench
  // analysis.
  static const u8 cp_modes[2]              = {0u, 1u};
  static const u8 rw_modes[2]              = {0u, 1u};
  static const char *const path_names[]    = {"c", "i"};
  for (u32 i = 0; i < n_files; i++) {
    TEST_BEGIN(files[i]);

    char expect_path[BENCH_PATH_LEN];
    snprintf(expect_path, BENCH_PATH_LEN, "%.*s.expect",
             (int)(strlen(files[i]) - 3), files[i]);
    char expected[64];
    int er = read_expect_status(expect_path, expected, sizeof expected);

    for (u32 mc = 0; mc < 2; mc++) {
      for (u32 mr = 0; mr < 2; mr++) {
        // Reset heap between runs.  Each IC-routed saturation
        // allocates many APP/PRI cells per step, so 16 back-to-
        // back runs (4 files * 4 modes) overflow HEAP_CAP without
        // a reset.  thvm_free + thvm_init is the heaviest hammer
        // but it cleanly clears HEAP_NEXT.
        thvm_free();
        thvm_init();

        AtpState *atp;
        WaldSpec *spec;
        double wall_ms = 0.0;
        AtpStatus st = run_one(files[i],
                               cp_modes[mc], rw_modes[mr],
                               &atp, &spec, &wall_ms);
        CHECK(atp != NULL);
        if (atp == NULL) continue;

        const char *st_str = atp_status_str(st);
        char mode_label[4];
        mode_label[0] = path_names[mc][0];
        mode_label[1] = path_names[mr][0];
        mode_label[2] = 0;
        fprintf(csv, "%s,%s,%s,%.3f,%u,%u,%u,%u,%u,%u,%u\n",
                files[i], mode_label, st_str, wall_ms,
                atp->step, atp->n_rules, atp->n_trace,
                atp->n_cps_dropped_joinable,
                atp->n_cps_dropped_connected,
                atp->n_cps_dropped_rule_subsumed,
                atp->n_cps_dropped_queue_subsumed);

        if (er == 0 && expected[0] != 0) {
          CHECK_EQ((int)strcmp(st_str, expected), 0);
        }

        thvm_atp_free(atp);
        wald_free(spec);
      }
    }
  }

  fclose(csv);
  thvm_free();
  TEST_REPORT();
}
