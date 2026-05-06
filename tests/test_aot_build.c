// tests/test_aot_build.c
//
// Phase 4 iter E1: smoke-test thvm_aot_compile_to_dylib.
//
// Builds a trivial def in BOOK_HEAP, calls compile_to_dylib, and
// checks the returned dylib path actually exists + is non-empty.
// Doesn't dlopen or invoke yet -- iter E2 adds that and verifies
// running it produces the expected NUM result.

#include "../src/thvm.c"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static u32 build_trivial_def(u32 slot) {
  // TLam[x, TMatChain[<|0 -> NUM(42), 1 -> NUM(43)|>,
  //                   TLam[ig, NUM(99)]][x]]
  // Same shape as test_aot_e2e's first test: input NUM(0)/NUM(1)/NUM(99)
  // -> NUM(42)/NUM(43)/NUM(99).

  Term n42 = term_new(0, TAG_NUM, DT_INT32, 42);
  Term n43 = term_new(0, TAG_NUM, DT_INT32, 43);
  Term n99 = term_new(0, TAG_NUM, DT_INT32, 99);

  u64 lam_ig_loc = book_alloc(1);
  book_set(lam_ig_loc, n99);
  Term def_lam = term_new(0, TAG_LAM, 0, lam_ig_loc);

  u64 mat_inner_loc = book_alloc(2);
  book_set(mat_inner_loc + 0, n43);
  book_set(mat_inner_loc + 1, def_lam);
  Term mat_inner = term_new(0, TAG_MAT, 1, mat_inner_loc);

  u64 mat_outer_loc = book_alloc(2);
  book_set(mat_outer_loc + 0, n42);
  book_set(mat_outer_loc + 1, mat_inner);
  Term mat_outer = term_new(0, TAG_MAT, 0, mat_outer_loc);

  u64 lam_x_loc = book_alloc(1);
  Term var_x   = term_new(0, TAG_VAR, 0, lam_x_loc);
  u64 app_loc  = book_alloc(2);
  book_set(app_loc + 0, mat_outer);
  book_set(app_loc + 1, var_x);
  Term app     = term_new(0, TAG_APP, 0, app_loc);
  book_set(lam_x_loc, app);
  Term lam_x   = term_new(0, TAG_LAM, 0, lam_x_loc);

  DEFS[slot] = lam_x;
  return slot;
}

int main(void) {
  thvm_init();
  setvbuf(stdout, NULL, _IONBF, 0);

  TEST_BEGIN("compile_to_dylib returns a non-NULL path");
  {
    u32 id = build_trivial_def(1);
    char *path = thvm_aot_compile_to_dylib(id, "trivial");
    CHECK(path != NULL);
    free(path);
  }

  TEST_BEGIN("compile_to_dylib output file exists + is non-empty");
  {
    thvm_free(); thvm_init();
    u32 id = build_trivial_def(1);
    char *path = thvm_aot_compile_to_dylib(id, "trivial");
    CHECK(path != NULL);
    struct stat st;
    int rc = stat(path, &st);
    CHECK_EQ(rc, 0);
    CHECK(st.st_size > 1024);   // arm64 dylib is at least a few KB
    free(path);
  }

  TEST_BEGIN("compile_to_dylib is content-addressed (cache hit)");
  {
    thvm_free(); thvm_init();
    u32 id = build_trivial_def(1);
    char *path1 = thvm_aot_compile_to_dylib(id, "trivial");
    CHECK(path1 != NULL);

    // Second call with same shape should produce same path (cache).
    thvm_free(); thvm_init();
    u32 id2 = build_trivial_def(1);
    char *path2 = thvm_aot_compile_to_dylib(id2, "trivial");
    CHECK(path2 != NULL);
    CHECK_EQ(strcmp(path1, path2), 0);

    free(path1);
    free(path2);
  }

  TEST_BEGIN("compile_to_dylib handles invalid def_id (returns NULL)");
  {
    char *path = thvm_aot_compile_to_dylib(DEFS_CAP + 7, "nope");
    CHECK_EQ((u64)path, (u64)NULL);
  }

  TEST_BEGIN("compile_to_dylib handles unbound def_slot (returns NULL)");
  {
    thvm_free(); thvm_init();
    char *path = thvm_aot_compile_to_dylib(99, "unbound");
    CHECK_EQ((u64)path, (u64)NULL);
  }

  // === Load + run: end-to-end verifies the dylib actually computes
  // the right NUM result ============================================
  TEST_BEGIN("load_and_run: NUM(0) input -> NUM(42)");
  {
    thvm_free(); thvm_init();
    u32 id = build_trivial_def(1);
    char *path = thvm_aot_compile_to_dylib(id, "trivial");
    CHECK(path != NULL);

    Term in   = term_new(0, TAG_NUM, DT_INT32, 0);
    u64  rt   = thvm_aot_load_and_run(path, "trivial", (u64)in);
    Term r    = (Term)rt;
    CHECK_EQ((u32)term_tag(r), (u32)TAG_NUM);
    CHECK_EQ((u32)term_val(r), 42u);
    free(path);
  }

  TEST_BEGIN("load_and_run: NUM(1) input -> NUM(43)");
  {
    thvm_free(); thvm_init();
    u32 id = build_trivial_def(1);
    char *path = thvm_aot_compile_to_dylib(id, "trivial");
    CHECK(path != NULL);

    Term in   = term_new(0, TAG_NUM, DT_INT32, 1);
    u64  rt   = thvm_aot_load_and_run(path, "trivial", (u64)in);
    Term r    = (Term)rt;
    CHECK_EQ((u32)term_tag(r), (u32)TAG_NUM);
    CHECK_EQ((u32)term_val(r), 43u);
    free(path);
  }

  TEST_BEGIN("load_and_run: NUM(99) input hits default arm -> NUM(99)");
  {
    thvm_free(); thvm_init();
    u32 id = build_trivial_def(1);
    char *path = thvm_aot_compile_to_dylib(id, "trivial");
    CHECK(path != NULL);

    Term in   = term_new(0, TAG_NUM, DT_INT32, 99);
    u64  rt   = thvm_aot_load_and_run(path, "trivial", (u64)in);
    Term r    = (Term)rt;
    CHECK_EQ((u32)term_tag(r), (u32)TAG_NUM);
    CHECK_EQ((u32)term_val(r), 99u);
    free(path);
  }

  TEST_BEGIN("load_and_run: bad path returns 0");
  {
    u64 rt = thvm_aot_load_and_run("/tmp/nonexistent_aot.dylib",
                                    "trivial", 0);
    CHECK_EQ(rt, 0ull);
  }

  TEST_BEGIN("load_and_run: bad symbol name returns 0");
  {
    thvm_free(); thvm_init();
    u32 id = build_trivial_def(1);
    char *path = thvm_aot_compile_to_dylib(id, "trivial");
    CHECK(path != NULL);
    u64 rt = thvm_aot_load_and_run(path, "nonexistent_sym", 0);
    CHECK_EQ(rt, 0ull);
    free(path);
  }

  TEST_REPORT();
}
