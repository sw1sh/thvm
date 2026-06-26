// test_full_unroll_repro.c -- STAGE 0..3 reproducer for the
// "full-extent UNROLL" deviation.  Builds a lifted reduce STORE DAG
//   out[a0] = sum_{a1} (in0[a0*16 + a1])          (axis a1, extent 16)
// then applies KOP_UNROLL at the FULL extent (k == 16) via the exact
// production rewriter uop_dag_apply_kopt and renders C.  Confirms:
//  (1) the rewriter produces a live extent-1 reduce RANGE (not a dangling
//      REDUCE axis), and
//  (2) the renderer emits straight-line `#pragma unroll(16)` accumulation,
//      NOT a split-by-4 nest, and the C is numerically exact when run.
#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

static int contains(const char *h, const char *n) { return strstr(h, n) != NULL; }

// Build STORE(out, RANGE(0,LOOP,32), REDUCE(INDEX_E(in0, a0*16+a1), SUM, axis=1))
static Term build_reduce_root(Term out, Term in0) {
  Term a0   = uop_range(0, 0 /*LOOP*/,   32);
  Term a1   = uop_range(1, 1 /*REDUCE*/, 16);
  Term row  = uop_int_binary(UOP_IMUL, a0, uop_const(DT_INT32, 16));
  Term addr = uop_int_binary(UOP_IADD, row, a1);
  Term ld   = uop_index_e(in0, addr);
  Term red  = uop_reduce(REDUCE_SUM, /*axis=*/1, ld);
  return uop_store(out, a0, red);
}

// Render a root to C and return a malloc'd string (caller frees).
static char *render_c(Term root, const char *name, Term out, Term *ins, u32 ni) {
  static char buf[8192];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  cg_render_uop_kernel_c(root, name, out, ins, ni, fp);
  fclose(fp);
  return buf;
}

int main(void) {
  thvm_init();
  u32 odims[1] = { 32 };
  u32 idims[1] = { 32 * 16 };
  Term out = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims);
  Term in0 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, idims);
  Term ins[1] = { in0 };

  Term root = build_reduce_root(out, in0);

  // --- BEFORE: baseline reduce render (no opt) ---
  char before[8192];
  snprintf(before, sizeof before, "%s", render_c(root, "k_base", out, ins, 1));
  fprintf(stderr, "=== BEFORE (no UNROLL) ===\n%s\n", before);

  // --- Apply FULL-extent UNROLL on the reduce axis (axis_id=1, k=16) ---
  KOpt opt = { KOP_UNROLL, /*axis=*/1, /*k=*/16 };
  Term unrolled = uop_dag_apply_kopt(root, opt);
  TEST_BEGIN("full-unroll/apply-kopt-rewrites-root");
  CHECK(unrolled != 0 && unrolled != root);

  char after[8192];
  snprintf(after, sizeof after, "%s", render_c(unrolled, "k_unroll", out, ins, 1));
  fprintf(stderr, "=== AFTER (full UNROLL k=16) ===\n%s\n", after);

  // STAGE 1/3: the full-extent unroll must render straight-line over the
  // WHOLE extent (#pragma unroll(16) -> clang fully unrolls the 16-trip
  // inner loop), NOT a split-by-4 nest, and must keep a live reduce loop.
  TEST_BEGIN("full-unroll/renders-full-extent-not-split4");
  CHECK(contains(after, "unroll(16)") || contains(after, "unroll_count(16)"));
  CHECK(!contains(after, "< 4;"));         // no split-by-4 inner trip count
  CHECK(contains(after, "_acc"));          // accumulator survives

  // --- Numeric exactness: compile both kernels, run, compare ---
  TEST_BEGIN("full-unroll/numeric-exact");
  f32 in[32 * 16], exp[32];
  for (u32 r = 0; r < 32; r++) {
    exp[r] = 0.0f;
    for (u32 k = 0; k < 16; k++) {
      in[r * 16 + k] = (float)(r + 1) * 0.5f + (float)k * 0.25f;
      exp[r] += in[r * 16 + k];
    }
  }
  // Compile the unrolled kernel to a dylib and run it.
  char path[] = "/tmp/thvm_full_unroll_XXXXXX.c";
  int fd = mkstemps(path, 2);      // 2 = length of ".c" suffix to preserve
  CHECK(fd >= 0);
  FILE *cf = fdopen(fd, "w");
  fputs(after, cf);
  fclose(cf);
  char so[300]; snprintf(so, sizeof so, "%s.dylib", path);
  char cmd[800];
  snprintf(cmd, sizeof cmd,
           "clang -O3 -shared -fPIC -o %s %s 2>/dev/null", so, path);
  int rc = system(cmd);
  CHECK(rc == 0);
  void *h = dlopen(so, RTLD_NOW);
  CHECK(h != NULL);
  if (h) {
    typedef void (*kfn)(void *, const void *const *, unsigned,
                        const unsigned *, const unsigned *);
    kfn kf = (kfn)dlsym(h, "k_unroll");
    CHECK(kf != NULL);
    if (kf) {
      f32 res[32] = {0};
      const void *insp[1] = { in };
      unsigned numels[1] = { 32 * 16 };
      kf(res, insp, 1, numels, NULL);
      for (u32 r = 0; r < 32; r++) {
        if (fabsf(res[r] - exp[r]) > 1e-4f)
          fprintf(stderr, "  MISMATCH r=%u got=%f want=%f\n", r, res[r], exp[r]);
        CHECK(fabsf(res[r] - exp[r]) <= 1e-4f);
      }
    }
    dlclose(h);
  }
  unlink(path); unlink(so);
  TEST_REPORT();
}
