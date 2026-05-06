// tests/test_aot_e2e_bench.c
//
// Phase 2 iter 9: bench AOT-emitted vs hand-coded `count`.
//
// `count(SUC^D{ZER}) = 2^D` is the simplest workload that exercises
// the SPLIT/cont path -- each level does one OP2 fold cont over two
// recursive count calls.  Identical work both ways:
//
//   hand-coded: dispatch fn switches on FN_count_h / CONT_count_h_0
//               directly via a switch in this TU
//   emit:       same shape produced by thvm_aot_emit_program; runs
//               in a separately compiled standalone binary that
//               #includes thvm.c and the emit
//
// Reports per-D timings.  Asserts only on values (not timings) --
// timings are for human inspection.  The point is to know how
// close the emit is to hand-coded, NOT to gate CI on it.

#include "../src/thvm.c"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

static char REPO_ROOT[1024] = {0};

static void compute_repo_root(void) {
  FILE *p = popen("git rev-parse --show-toplevel", "r");
  if (!p) { strcpy(REPO_ROOT, "."); return; }
  if (fgets(REPO_ROOT, sizeof REPO_ROOT, p) != NULL) {
    size_t n = strlen(REPO_ROOT);
    if (n > 0 && REPO_ROOT[n - 1] == '\n') REPO_ROOT[n - 1] = '\0';
  }
  pclose(p);
}

static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// === Hand-coded count =================================================

#define FN_COUNT_H        100u
#define CONT_COUNT_H_0    101u

static AotResult par_count_h_entry(AotProgram *p, AotTask *t);
static AotResult par_count_h_cont_0(AotProgram *p, AotTask *t);

static AotResult par_count_h_cont_0(AotProgram *p, AotTask *t) {
  (void)p;
  u32 lv = (u32)term_val(t->args[0]);
  u32 rv = (u32)term_val(t->args[1]);
  return aot_make_value(
      term_new(0, TAG_NUM, term_ext(t->args[0]), lv + rv));
}

static AotResult par_count_h_entry(AotProgram *p, AotTask *t) {
  (void)p;
  Term n = t->args[0];
  if (term_tag(n) == TAG_CTR && term_ext(n) == 0) {
    // ZER -> NUM(1)
    return aot_make_value(term_new(0, TAG_NUM, DT_INT32, 1));
  }
  if (term_tag(n) == TAG_CTR && term_ext(n) == 1) {
    // SUC{p} -> count(p) + count(p)
    Term p_child = term_ctr_at(n, 0);
    u64 dp = aot_alloc_cont(CONT_COUNT_H_0, 0, t->ret);
    return aot_make_split(
        aot_make_task(FN_COUNT_H, aot_enc_ret((u32)dp, 0),
                      p_child, 0, 0, 0),
        aot_make_task(FN_COUNT_H, aot_enc_ret((u32)dp, 1),
                      p_child, 0, 0, 0));
  }
  // Default: ERA
  return aot_make_value(term_new(0, TAG_ERA, 0, 0));
}

static AotResult count_h_dispatch(AotProgram *p, AotTask *t) {
  switch (t->fn_id) {
    case FN_COUNT_H:        return par_count_h_entry(p, t);
    case CONT_COUNT_H_0:    return par_count_h_cont_0(p, t);
    default:                return aot_make_value(0);
  }
}

static Term build_nat(u32 d) {
  Term cur = term_new_ctr(0, NULL, 0);   // ZER
  for (u32 i = 0; i < d; i++) {
    cur = term_new_ctr(1, &cur, 1);      // SUC{cur}
  }
  return cur;
}

// === Build an AOT-emitted binary for count at slot 2 ================

static u32 build_count_def(void) {
  Term n1   = term_new(0, TAG_NUM, DT_INT32, 1);
  Term era  = term_new(0, TAG_ERA, 0, 0);

  u64 lam_p_loc = book_alloc(1);
  Term var_p    = term_new(0, TAG_VAR, 0, lam_p_loc);

  Term ref      = term_new(0, TAG_REF, 2, 0);
  u64 c0_loc    = book_alloc(2);
  book_set(c0_loc + 0, ref);
  book_set(c0_loc + 1, var_p);
  Term call_p   = term_new(0, TAG_APP, 0, c0_loc);

  u64 op_loc    = book_alloc(2);
  book_set(op_loc + 0, call_p);
  book_set(op_loc + 1, call_p);
  Term op2      = term_new(0, TAG_OP2, OP_ADD, op_loc);

  book_set(lam_p_loc, op2);
  Term suc_handler = term_new(0, TAG_LAM, 0, lam_p_loc);

  u64 lam_ig_loc = book_alloc(1);
  book_set(lam_ig_loc, era);
  Term def_handler = term_new(0, TAG_LAM, 0, lam_ig_loc);

  u64 inner_mat = book_alloc(2);
  book_set(inner_mat + 0, suc_handler);
  book_set(inner_mat + 1, def_handler);
  Term mat_inner = term_new(0, TAG_MAT, 1, inner_mat);

  u64 outer_mat = book_alloc(2);
  book_set(outer_mat + 0, n1);
  book_set(outer_mat + 1, mat_inner);
  Term mat = term_new(0, TAG_MAT, 0, outer_mat);

  u64 lam_n_loc = book_alloc(1);
  Term var_n    = term_new(0, TAG_VAR, 0, lam_n_loc);
  u64 app_loc   = book_alloc(2);
  book_set(app_loc + 0, mat);
  book_set(app_loc + 1, var_n);
  Term app      = term_new(0, TAG_APP, 0, app_loc);
  book_set(lam_n_loc, app);
  Term lam_n    = term_new(0, TAG_LAM, 0, lam_n_loc);

  DEFS[2] = lam_n;
  return 2;
}

// Compile an emitted binary that loops the count program over a
// range of depths and prints each (D, val, run_s).  Returns 0 on
// successful compile.  Caller runs the binary separately.
static int compile_emitted_count_bench(const char *out_bin) {
  u32 id = build_count_def();
  char *src = thvm_aot_emit_program(id, "count");
  if (!src) return -1;

  const char *src_path = "/tmp/aot_e2e_bench_count.c";
  FILE *f = fopen(src_path, "w");
  if (!f) { free(src); return -1; }

  fprintf(f, "#include \"%s/src/thvm.c\"\n\n", REPO_ROOT);
  fputs(src, f);
  free(src);

  fprintf(f,
    "\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <time.h>\n"
    "\n"
    "static double now_s(void) {\n"
    "  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);\n"
    "  return ts.tv_sec + ts.tv_nsec * 1e-9;\n"
    "}\n"
    "\n"
    "static Term build_nat(u32 d) {\n"
    "  Term cur = term_new_ctr(0, NULL, 0);\n"
    "  for (u32 i = 0; i < d; i++) cur = term_new_ctr(1, &cur, 1);\n"
    "  return cur;\n"
    "}\n"
    "\n"
    "int main(int argc, char **argv) {\n"
    "  thvm_init();\n"
    "  AotProgram p;\n"
    "  aot_program_count_register(&p);\n"
    "  /* Each line: D=<d> val=<v> run=<dt>s */\n"
    "  for (int i = 1; i < argc; i++) {\n"
    "    u32 d = (u32)atoi(argv[i]);\n"
    "    Term in = build_nat(d);\n"
    "    AotTask root = aot_make_task(FN_count, AOT_RET_ROOT,\n"
    "                                 in, 0, 0, 0);\n"
    "    double t0 = now_s();\n"
    "    Term r = aot_run_serial(&p, root);\n"
    "    double dt = now_s() - t0;\n"
    "    printf(\"D=%%u val=%%llu run=%%.6fs\\n\",\n"
    "           d, (unsigned long long)term_val(r), dt);\n"
    "    fflush(stdout);\n"
    "  }\n"
    "  return 0;\n"
    "}\n");
  fclose(f);

  char cmd[2048];
  snprintf(cmd, sizeof cmd,
    "clang -O2 -std=c11 -Wno-everything -DACCELERATE_NEW_LAPACK "
    "-framework Accelerate -o '%s' '%s' 2>&1",
    out_bin, src_path);

  FILE *pp = popen(cmd, "r");
  if (!pp) return -1;
  char line[1024];
  while (fgets(line, sizeof line, pp) != NULL) {
    if (strstr(line, "error:") != NULL) {
      fprintf(stderr, "clang: %s", line);
    }
  }
  return pclose(pp);
}

// Run the emitted bench binary with a list of depths.  Returns 0 on
// success.  Caller parses stdout; we just stream + capture into a
// caller-supplied callback for test verification.
static int run_emitted_count_bench(const char *bin, const u32 *depths,
                                   u32 n_depths, u64 *out_vals,
                                   double *out_times) {
  char cmd[1024];
  int n = snprintf(cmd, sizeof cmd, "'%s'", bin);
  for (u32 i = 0; i < n_depths; i++) {
    n += snprintf(cmd + n, sizeof cmd - n, " %u", depths[i]);
  }
  FILE *pp = popen(cmd, "r");
  if (!pp) return -1;
  char line[1024];
  u32 i = 0;
  while (i < n_depths && fgets(line, sizeof line, pp) != NULL) {
    u32 d; u64 v; double dt;
    if (sscanf(line, "D=%u val=%llu run=%lfs",
               &d, (unsigned long long *)&v, &dt) == 3) {
      out_vals[i]  = v;
      out_times[i] = dt;
      i++;
    }
  }
  pclose(pp);
  return (i == n_depths) ? 0 : -1;
}

int main(void) {
  thvm_init();
  setvbuf(stdout, NULL, _IONBF, 0);

  compute_repo_root();
  CHECK(REPO_ROOT[0] != '\0');

  // Compile the emitted bench binary (one-time cost; not measured).
  TEST_BEGIN("compile AOT-emitted count bench binary");
  {
    int rc = compile_emitted_count_bench("/tmp/aot_count_bench");
    CHECK_EQ(rc, 0);
  }

  // === Run hand-coded count + AOT-emitted count at multiple D, T=1 ==
  TEST_BEGIN("hand-coded vs AOT-emitted count parity (T=1) at D=8/12/16");
  {
    AotProgram hand;
    hand.dispatch = count_h_dispatch;

    const u32 depths[]   = {8, 12, 16};
    const u32 n_depths   = sizeof depths / sizeof depths[0];
    u64    aot_vals[3];
    double aot_times[3];

    int rc = run_emitted_count_bench("/tmp/aot_count_bench",
                                     depths, n_depths,
                                     aot_vals, aot_times);
    CHECK_EQ(rc, 0);

    printf("\n  D    expected  hand-coded(s)  aot-emit(s)  ratio\n");
    for (u32 i = 0; i < n_depths; i++) {
      u32 d = depths[i];
      thvm_free(); thvm_init();   // fresh heap for each hand run
      Term in = build_nat(d);
      AotTask root = aot_make_task(FN_COUNT_H, AOT_RET_ROOT,
                                    in, 0, 0, 0);
      double t0 = now_s();
      Term r = aot_run_serial(&hand, root);
      double hand_dt = now_s() - t0;
      u64 hand_val = term_val(r);
      u64 expected = 1ull << d;

      printf("  %2u   %8llu  %12.6f  %11.6f  %.2fx\n",
             d, (unsigned long long)expected,
             hand_dt, aot_times[i], hand_dt / aot_times[i]);

      CHECK_EQ(hand_val, expected);
      CHECK_EQ(aot_vals[i], expected);
    }
  }

  TEST_REPORT();
}
