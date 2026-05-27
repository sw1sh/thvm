// tests/test_aot_e2e.c
//
// Phase 2 iter 7: end-to-end pipeline.  Demonstrates that
// thvm_aot_emit_program produces a C source string that:
//
//   1. Compiles standalone (via clang) when wrapped with a
//      `#include "src/thvm.c"` and a generated `main()`.
//   2. Runs to a result equivalent to what the interpreter (or
//      the hand-coded test_bend_tree_sum) would produce.
//
// The compile path here uses a self-contained binary, not a dylib.
// Reasoning: a dlopen'd dylib that #include's thvm.c would have its
// own copy of every global (CURRENT_CTX, HEAP, DEFS, ...), and the
// host couldn't share heap state with it.  A separate-process
// binary sidesteps the issue: the dylib path with a real
// runtime-ops indirection lands in Phase 4 once the WL bridge needs
// it.  For Phase 2 iter 7 this is enough to prove the emitter
// produces correct, compilable, runnable code.
//
// Test program: a TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>,
//                                    TLam[ig, TNum[99]]][x]].
// Call with x = NUM(0)  -> 42
// Call with x = NUM(1)  -> 43
// Call with x = NUM(99) -> 99 (default arm)

#include "../src/thvm.c"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Repo root used in the generated wrapper's #include path.  Must be
// an absolute path; we get it via the worktree's git rev-parse.
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

// Build the trivial TMatChain def in BOOK_HEAP at slot 1.  Returns
// the slot id.
static u32 build_trivial_def(void) {
  // TLam[x, TMatChain[<|0 -> TNum[42], 1 -> TNum[43]|>, TLam[ig, TNum[99]]][x]]
  Term n42 = term_new(0, TAG_NUM, DT_INT32, 42);
  Term n43 = term_new(0, TAG_NUM, DT_INT32, 43);
  Term n99 = term_new(0, TAG_NUM, DT_INT32, 99);

  // Default arm: TLam[ig, NUM(99)]
  u64 lam_ig_loc = book_alloc(1);
  book_set(lam_ig_loc, n99);
  Term def_lam = term_new(0, TAG_LAM, 0, lam_ig_loc);

  // Inner MAT: 1 -> NUM(43), default = def_lam
  u64 mat_inner_loc = book_alloc(2);
  book_set(mat_inner_loc + 0, n43);
  book_set(mat_inner_loc + 1, def_lam);
  Term mat_inner = term_new(0, TAG_MAT, 1, mat_inner_loc);

  // Outer MAT: 0 -> NUM(42), default = mat_inner
  u64 mat_outer_loc = book_alloc(2);
  book_set(mat_outer_loc + 0, n42);
  book_set(mat_outer_loc + 1, mat_inner);
  Term mat_outer = term_new(0, TAG_MAT, 0, mat_outer_loc);

  // Outer App: App(mat_outer, TVar(x))
  u64 lam_x_loc = book_alloc(1);
  Term var_x   = term_new(0, TAG_VAR, 0, lam_x_loc);
  u64 app_loc  = book_alloc(2);
  book_set(app_loc + 0, mat_outer);
  book_set(app_loc + 1, var_x);
  Term app     = term_new(0, TAG_APP, 0, app_loc);
  book_set(lam_x_loc, app);
  Term lam_x   = term_new(0, TAG_LAM, 0, lam_x_loc);

  DEFS[1] = lam_x;
  return 1;
}

// Compile a standalone binary that includes thvm.c + the emitted
// source + a generated main.  Returns 0 on success.
static int compile_e2e_binary(const char *emit_src,
                              const char *prog_name,
                              u32 input_val,
                              const char *out_bin_path) {
  char src_path[512];
  snprintf(src_path, sizeof src_path, "/tmp/aot_e2e_%s.c", prog_name);

  FILE *f = fopen(src_path, "w");
  if (!f) return -1;

  // Include thvm.c via absolute path so relative #includes inside
  // it (e.g., aot/_.c -> halloc.h) resolve.
  fprintf(f, "#include \"%s/src/thvm.c\"\n\n", REPO_ROOT);
  fputs(emit_src, f);

  // Generated main: register the program, run with input_val,
  // print the result Term's tag/val.
  fprintf(f,
    "\n"
    "#include <stdio.h>\n"
    "int main(void) {\n"
    "  thvm_init();\n"
    "  AotProgram p;\n"
    "  aot_program_%s_register(&p);\n"
    "  Term in = term_new(0, TAG_NUM, DT_INT32, %uu);\n"
    "  AotTask root = aot_make_task(FN_%s, AOT_RET_ROOT,\n"
    "                                in, 0, 0, 0);\n"
    "  Term r = aot_run_serial(&p, root);\n"
    "  printf(\"result tag=%%u val=%%llu\\n\",\n"
    "         (unsigned)term_tag(r), (unsigned long long)term_val(r));\n"
    "  return 0;\n"
    "}\n",
    prog_name, input_val, prog_name);
  fclose(f);

  char cmd[2048];
  snprintf(cmd, sizeof cmd,
    "clang -O0 -std=c11 -Wno-everything -DACCELERATE_NEW_LAPACK "
    "-DATP_RULE_INDEX -DATP_FV_INDEX -DATP_VAR_NORM "
    "-DATP_ORDERED_REWRITE -DATP_ORPHAN_KILL -DATP_CP_GROUND_JOIN "
    "-framework Accelerate "
    "-o '%s' '%s' 2>&1",
    out_bin_path, src_path);

  FILE *p = popen(cmd, "r");
  if (!p) return -1;
  char line[1024];
  while (fgets(line, sizeof line, p) != NULL) {
    // Surface the first error/warning so failures are diagnosable.
    if (strstr(line, "error:") != NULL) {
      fprintf(stderr, "clang: %s", line);
    }
  }
  return pclose(p);
}

// Run the compiled binary, capture stdout's `val=N` integer.  Returns
// the parsed val or UINT64_MAX on any failure.
static u64 run_e2e_binary(const char *bin_path) {
  FILE *p = popen(bin_path, "r");
  if (!p) return UINT64_MAX;
  char buf[256];
  u64 val = UINT64_MAX;
  while (fgets(buf, sizeof buf, p) != NULL) {
    char *eq = strstr(buf, "val=");
    if (eq) {
      val = strtoull(eq + 4, NULL, 10);
      break;
    }
  }
  pclose(p);
  return val;
}

int main(void) {
  thvm_init();
  setvbuf(stdout, NULL, _IONBF, 0);

  compute_repo_root();
  CHECK(REPO_ROOT[0] != '\0');

  TEST_BEGIN("emit + compile + run: NUM(0) input -> NUM(42)");
  {
    u32 id = build_trivial_def();
    char *src = thvm_aot_emit_program(id, "trivial");
    CHECK(src != NULL);

    const char *bin = "/tmp/aot_e2e_trivial_0";
    int rc = compile_e2e_binary(src, "trivial", 0, bin);
    CHECK_EQ(rc, 0);

    u64 val = run_e2e_binary(bin);
    CHECK_EQ(val, 42ull);

    free(src);
  }

  TEST_BEGIN("emit + compile + run: NUM(1) input -> NUM(43)");
  {
    // Re-init so DEFS[1] doesn't carry over with shifted heap locs.
    thvm_free(); thvm_init();
    u32 id = build_trivial_def();
    char *src = thvm_aot_emit_program(id, "trivial");
    CHECK(src != NULL);

    const char *bin = "/tmp/aot_e2e_trivial_1";
    int rc = compile_e2e_binary(src, "trivial", 1, bin);
    CHECK_EQ(rc, 0);

    u64 val = run_e2e_binary(bin);
    CHECK_EQ(val, 43ull);

    free(src);
  }

  TEST_BEGIN("emit + compile + run: NUM(99) input hits default arm -> NUM(99)");
  {
    thvm_free(); thvm_init();
    u32 id = build_trivial_def();
    char *src = thvm_aot_emit_program(id, "trivial");
    CHECK(src != NULL);

    const char *bin = "/tmp/aot_e2e_trivial_99";
    int rc = compile_e2e_binary(src, "trivial", 99, bin);
    CHECK_EQ(rc, 0);

    u64 val = run_e2e_binary(bin);
    CHECK_EQ(val, 99ull);

    free(src);
  }

  // === SPLIT-path e2e: count(SUC^D{ZER}) -> 2^D =====================
  //
  // Exercises the entire SPLIT/cont pipeline at runtime, not just at
  // emit-string assertion time.  The def:
  //
  //   TLam[n, TMatChain[<|0 -> NUM(1),                  (* ZER -> 1 *)
  //                       1 -> TLam[p, OP2(+,            (* SUC{p} -> 2*count(p) *)
  //                              App(TRef[2], TVar(p)),
  //                              App(TRef[2], TVar(p)))] |>,
  //                     TLam[ig, TEra[]]                 (* default *)
  //                   ][n]]
  //
  // For input SUC^D{ZER}, count returns NUM(2^D).  Verifies:
  //   - CTR-arm dispatch + multi-LAM destructure + bound TVar use
  //   - Sibling-pair TOp2(+, self(p), self(p)) -> R_SPLIT + cont
  //   - The cont fires at runtime, sums two NUM(2^(D-1)) -> NUM(2^D)
  //   - aot_resolve walks the value all the way back to AOT_RET_ROOT

  TEST_BEGIN("emit + compile + run: count SUC^4{ZER} via SPLIT -> NUM(16)");
  {
    thvm_free(); thvm_init();

    // Build the count def at slot 2.
    Term n1   = term_new(0, TAG_NUM, DT_INT32, 1);
    Term era  = term_new(0, TAG_ERA, 0, 0);

    u64 lam_p_loc = book_alloc(1);
    Term var_p    = term_new(0, TAG_VAR, 0, lam_p_loc);

    // self(p): App(REF[2], TVar(p))
    Term ref      = term_new(0, TAG_REF, 2, 0);
    u64 c0_loc    = book_alloc(2);
    book_set(c0_loc + 0, ref);
    book_set(c0_loc + 1, var_p);
    Term call_p   = term_new(0, TAG_APP, 0, c0_loc);

    // OP2(+, self(p), self(p))
    u64 op_loc    = book_alloc(2);
    book_set(op_loc + 0, call_p);
    book_set(op_loc + 1, call_p);
    Term op2      = term_new(0, TAG_OP2, OP_ADD, op_loc);

    // SUC-arm handler: TLam[p, op2]
    book_set(lam_p_loc, op2);
    Term suc_handler = term_new(0, TAG_LAM, 0, lam_p_loc);

    // Default handler: TLam[ig, ERA]
    u64 lam_ig_loc = book_alloc(1);
    book_set(lam_ig_loc, era);
    Term def_handler = term_new(0, TAG_LAM, 0, lam_ig_loc);

    // Inner MAT: 1 (SUC) -> suc_handler, default = def_handler
    u64 inner_mat = book_alloc(2);
    book_set(inner_mat + 0, suc_handler);
    book_set(inner_mat + 1, def_handler);
    Term mat_inner = term_new(0, TAG_MAT, 1, inner_mat);

    // Outer MAT: 0 (ZER) -> NUM(1), default = mat_inner
    u64 outer_mat = book_alloc(2);
    book_set(outer_mat + 0, n1);
    book_set(outer_mat + 1, mat_inner);
    Term mat = term_new(0, TAG_MAT, 0, outer_mat);

    // Outer App: App(mat, TVar(n))
    u64 lam_n_loc = book_alloc(1);
    Term var_n    = term_new(0, TAG_VAR, 0, lam_n_loc);
    u64 app_loc   = book_alloc(2);
    book_set(app_loc + 0, mat);
    book_set(app_loc + 1, var_n);
    Term app      = term_new(0, TAG_APP, 0, app_loc);
    book_set(lam_n_loc, app);
    Term lam_n    = term_new(0, TAG_LAM, 0, lam_n_loc);

    DEFS[2] = lam_n;

    char *src = thvm_aot_emit_program(2, "count");
    CHECK(src != NULL);
    CHECK(strstr(src, "aot_make_split(") != NULL);    // SPLIT emitted
    CHECK(strstr(src, "lv + rv")          != NULL);    // OP_ADD fold

    // Generate a wrapper that builds SUC^4{ZER} and runs count.
    char src_path[512];
    snprintf(src_path, sizeof src_path, "/tmp/aot_e2e_count.c");
    FILE *f = fopen(src_path, "w");
    CHECK(f != NULL);
    fprintf(f, "#include \"%s/src/thvm.c\"\n\n", REPO_ROOT);
    fputs(src, f);
    fprintf(f,
      "\n"
      "#include <stdio.h>\n"
      "int main(void) {\n"
      "  thvm_init();\n"
      "  AotProgram p;\n"
      "  aot_program_count_register(&p);\n"
      "  /* Build SUC^4{ZER} */\n"
      "  Term in = term_new_ctr(0, NULL, 0);\n"
      "  for (int i = 0; i < 4; i++) {\n"
      "    in = term_new_ctr(1, &in, 1);\n"
      "  }\n"
      "  AotTask root = aot_make_task(FN_count, AOT_RET_ROOT,\n"
      "                                in, 0, 0, 0);\n"
      "  Term r = aot_run_serial(&p, root);\n"
      "  printf(\"result tag=%%u val=%%llu\\n\",\n"
      "         (unsigned)term_tag(r), (unsigned long long)term_val(r));\n"
      "  return 0;\n"
      "}\n");
    fclose(f);

    char cmd[2048];
    snprintf(cmd, sizeof cmd,
      "clang -O0 -std=c11 -Wno-everything -DACCELERATE_NEW_LAPACK "
    "-DATP_RULE_INDEX -DATP_FV_INDEX -DATP_VAR_NORM "
    "-DATP_ORDERED_REWRITE -DATP_ORPHAN_KILL -DATP_CP_GROUND_JOIN "
      "-framework Accelerate -o /tmp/aot_e2e_count /tmp/aot_e2e_count.c 2>&1");
    FILE *p = popen(cmd, "r");
    CHECK(p != NULL);
    char line[1024];
    while (fgets(line, sizeof line, p) != NULL) {
      if (strstr(line, "error:") != NULL) {
        fprintf(stderr, "clang: %s", line);
      }
    }
    int rc = pclose(p);
    CHECK_EQ(rc, 0);

    u64 val = run_e2e_binary("/tmp/aot_e2e_count");
    CHECK_EQ(val, 16ull);   // count(SUC^4{ZER}) = 2^4 = 16

    free(src);
  }

  // === Value-position TOp2 e2e: fib(N) over NUMs ====================
  //
  // Exercises the Phase 3 iter C TAG_OP2 emit (n-1, n-2 used as
  // recursive call args) AND the iter B wnf-fast-path (recursive
  // arg arrives as TAG_OP2, must wnf to NUM before tag-checking).
  // The def matches plain NUM(0)/NUM(1) and recurses on the default
  // arm with a binder k = dv.  The outer OP2(+, fib(n-1), fib(n-2))
  // still hits the SPLIT pattern; the INNER OP2s are the new path.
  //
  //   TLam[n, TMatChain[<|0 -> NUM(0), 1 -> NUM(1)|>,
  //     TLam[k, OP2(+, App(REF[3], OP2(-, k, NUM(1))),
  //                    App(REF[3], OP2(-, k, NUM(2))))]
  //   ][n]]
  //
  // For input NUM(8): expected fib(8) = 21.  Picked >=2 to ensure
  // the default arm fires and the value-position OP2 emit is on
  // the actual hot path.
  TEST_BEGIN("emit + compile + run: fib(8) via value-pos OP2 -> NUM(21)");
  {
    thvm_free(); thvm_init();

    Term n0 = term_new(0, TAG_NUM, DT_INT32, 0);
    Term n1 = term_new(0, TAG_NUM, DT_INT32, 1);
    Term n2 = term_new(0, TAG_NUM, DT_INT32, 2);

    // k binder + TVar(k).
    u64 lam_k_loc = book_alloc(1);
    Term var_k    = term_new(0, TAG_VAR, 0, lam_k_loc);

    // OP2(-, k, 1)
    u64 sub1_loc = book_alloc(2);
    book_set(sub1_loc + 0, var_k);
    book_set(sub1_loc + 1, n1);
    Term k_sub_1 = term_new(0, TAG_OP2, OP_SUB, sub1_loc);

    // OP2(-, k, 2)
    u64 sub2_loc = book_alloc(2);
    book_set(sub2_loc + 0, var_k);
    book_set(sub2_loc + 1, n2);
    Term k_sub_2 = term_new(0, TAG_OP2, OP_SUB, sub2_loc);

    // App(REF[3], k_sub_1) and App(REF[3], k_sub_2)
    Term self_ref = term_new(0, TAG_REF, 3, 0);
    u64 app1_loc  = book_alloc(2);
    book_set(app1_loc + 0, self_ref);
    book_set(app1_loc + 1, k_sub_1);
    Term call_n_1 = term_new(0, TAG_APP, 0, app1_loc);

    u64 app2_loc  = book_alloc(2);
    book_set(app2_loc + 0, self_ref);
    book_set(app2_loc + 1, k_sub_2);
    Term call_n_2 = term_new(0, TAG_APP, 0, app2_loc);

    // OP2(+, fib(n-1), fib(n-2))
    u64 add_loc = book_alloc(2);
    book_set(add_loc + 0, call_n_1);
    book_set(add_loc + 1, call_n_2);
    Term sum = term_new(0, TAG_OP2, OP_ADD, add_loc);

    // Default handler: TLam[k, sum]
    book_set(lam_k_loc, sum);
    Term def_handler = term_new(0, TAG_LAM, 0, lam_k_loc);

    // Inner MAT: 1 -> NUM(1), default = def_handler
    u64 inner_mat = book_alloc(2);
    book_set(inner_mat + 0, n1);
    book_set(inner_mat + 1, def_handler);
    Term mat_inner = term_new(0, TAG_MAT, 1, inner_mat);

    // Outer MAT: 0 -> NUM(0), default = mat_inner
    u64 outer_mat = book_alloc(2);
    book_set(outer_mat + 0, n0);
    book_set(outer_mat + 1, mat_inner);
    Term mat = term_new(0, TAG_MAT, 0, outer_mat);

    // Outer App: App(mat, TVar(n))
    u64 lam_n_loc = book_alloc(1);
    Term var_n    = term_new(0, TAG_VAR, 0, lam_n_loc);
    u64 app_loc   = book_alloc(2);
    book_set(app_loc + 0, mat);
    book_set(app_loc + 1, var_n);
    Term app      = term_new(0, TAG_APP, 0, app_loc);
    book_set(lam_n_loc, app);
    Term lam_n    = term_new(0, TAG_LAM, 0, lam_n_loc);

    DEFS[3] = lam_n;

    char *src = thvm_aot_emit_program(3, "fib");
    CHECK(src != NULL);
    // Iter C produced TAG_OP2 in value position (the n-1 / n-2
    // sub-exprs); ensure both forms appear in the emit.
    CHECK(strstr(src, "TAG_OP2, 1, ")  != NULL);  // OP_SUB = 1
    CHECK(strstr(src, "aot_make_split(") != NULL);

    // Wrapper that runs fib(8) and prints val=N.
    char src_path[512];
    snprintf(src_path, sizeof src_path, "/tmp/aot_e2e_fib.c");
    FILE *f = fopen(src_path, "w");
    CHECK(f != NULL);
    fprintf(f, "#include \"%s/src/thvm.c\"\n\n", REPO_ROOT);
    fputs(src, f);
    fprintf(f,
      "\n"
      "#include <stdio.h>\n"
      "int main(void) {\n"
      "  thvm_init();\n"
      "  AotProgram p;\n"
      "  aot_program_fib_register(&p);\n"
      "  Term in = term_new(0, TAG_NUM, DT_INT32, 8);\n"
      "  AotTask root = aot_make_task(FN_fib, AOT_RET_ROOT,\n"
      "                                in, 0, 0, 0);\n"
      "  Term r = aot_run_serial(&p, root);\n"
      "  printf(\"result tag=%%u val=%%llu\\n\",\n"
      "         (unsigned)term_tag(r), (unsigned long long)term_val(r));\n"
      "  return 0;\n"
      "}\n");
    fclose(f);

    char cmd[2048];
    snprintf(cmd, sizeof cmd,
      "clang -O0 -std=c11 -Wno-everything -DACCELERATE_NEW_LAPACK "
    "-DATP_RULE_INDEX -DATP_FV_INDEX -DATP_VAR_NORM "
    "-DATP_ORDERED_REWRITE -DATP_ORPHAN_KILL -DATP_CP_GROUND_JOIN "
      "-framework Accelerate -o /tmp/aot_e2e_fib /tmp/aot_e2e_fib.c 2>&1");
    FILE *p = popen(cmd, "r");
    CHECK(p != NULL);
    char line[1024];
    while (fgets(line, sizeof line, p) != NULL) {
      if (strstr(line, "error:") != NULL) {
        fprintf(stderr, "clang: %s", line);
      }
    }
    int rc = pclose(p);
    CHECK_EQ(rc, 0);

    u64 val = run_e2e_binary("/tmp/aot_e2e_fib");
    CHECK_EQ(val, 21ull);   // fib(8) = 21

    free(src);
  }

  TEST_REPORT();
}
