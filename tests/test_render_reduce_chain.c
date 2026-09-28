// test_render_reduce_chain.c -- a STORE whose value is a REDUCE of a
// REDUCE of a different kind renders the same values the CPU UOp walker
// computes, whatever the axis count of each link.
//
//   A  out[i] = max_j sum_{k,l} x[i,j,k,l]    MAX[j] of SUM[k,l]
//   B  out[i] = sum_{j,k} max_l x[i,j,k,l]    SUM[j,k] of MAX[l]
//   C  out[i] = max_j sum_k y[i,j,k]          single-axis links
//   D  A with the SUM's l axis fully UNROLLed (split-inner half)
//
// Each is rendered through the production C entry
// (cg_render_uop_kernel_c_root), compiled with clang and run, and walked
// by cpu_uop_walk; both must match the reference.
#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

#define NI 2
#define NJ 3
#define NK 4
#define NL 2
#define NX (NI * NJ * NK * NL)

static Term x_index(Term i, Term j, Term k, Term l) {
  Term a = uop_int_binary(UOP_IMUL, i, uop_const(DT_INT32, NJ * NK * NL));
  Term b = uop_int_binary(UOP_IMUL, j, uop_const(DT_INT32, NK * NL));
  Term c = uop_int_binary(UOP_IMUL, k, uop_const(DT_INT32, NL));
  return uop_int_binary(UOP_IADD,
                        uop_int_binary(UOP_IADD, uop_int_binary(UOP_IADD, a, b), c), l);
}

// Render `root` through the production C entry, compile it with clang and
// run it.  Returns 1 on a completed run.
static int run_rendered(Term root, f32 const *x, f32 *res) {
  static char src[65536];
  FILE *fp = fmemopen(src, sizeof src, "w");
  if (fp == NULL) {
    return 0;
  }
  cg_render_uop_kernel_c_root(root, "k_chain", fp);
  fclose(fp);
  char const *tmpdir = getenv("TMPDIR");
  char path[512];
  snprintf(path, sizeof path, "%s/thvm_reduce_chain_XXXXXX.c",
           (tmpdir != NULL && tmpdir[0] != '\0') ? tmpdir : ".");
  int fd = mkstemps(path, 2);
  if (fd < 0) {
    return 0;
  }
  FILE *cf = fdopen(fd, "w");
  fputs(src, cf);
  fclose(cf);
  char so[600];
  snprintf(so, sizeof so, "%s.dylib", path);
  char cmd[1400];
  snprintf(cmd, sizeof cmd, "clang -O1 -shared -fPIC -o '%s' '%s' 2>/dev/null", so, path);
  void *h = (system(cmd) == 0) ? dlopen(so, RTLD_NOW) : NULL;
  typedef void (*kfn)(void *, const void *const *, unsigned,
                      const unsigned *, const unsigned *);
  kfn kf = (h != NULL) ? (kfn)dlsym(h, "k_chain") : NULL;
  if (kf != NULL) {
    const void *ins[1]   = { x };
    unsigned    numel[1] = { NX };
    kf(res, ins, NI, numel, NULL);
  } else {
    fprintf(stderr, "  render/compile failed:\n%s\n", src);
  }
  if (h != NULL) {
    dlclose(h);
  }
  unlink(path);
  unlink(so);
  return kf != NULL;
}

static u32 mk_f32(u32 n, f32 const *data) {
  Shape s = {0};
  s.ndim = 1;
  s.dims[0] = n;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data, (u64)n * sizeof(f32));
  }
  return tid;
}

// Run `root` through the CPU UOp walker.  Returns 1 when the walker ran it.
static int run_walker(Term root, Term out, Term x_buf, f32 const *x, f32 *res) {
  KernelEntry ke;
  memset(&ke, 0, sizeof ke);
  u32 tids[1] = {0};                      // raw buffer pointer, no view bias
  u32 in_buf_ids[1] = { TENS[mk_f32(NX, x)].buf_id };
  ke.cached_lift.in_bufs[0] = x_buf;
  u32 tout = mk_f32(NI, NULL);
  ke.n_inputs               = 1;
  ke.input_tids             = tids;
  ke.output_dtype           = DT_FP32;
  ke.cached_lift.store_root = root;
  ke.cached_lift.out_buf    = out;
  ke.cached_lift.n_inputs   = 1;
  if (!cpu_uop_walk(&ke, in_buf_ids, TENS[tout].buf_id)) {
    return 0;
  }
  CPU_BACKEND.buf_read(TENS[tout].buf_id, res, NI * sizeof(f32));
  return 1;
}

static void check_close(f32 const *got, f32 const *want, char const *what) {
  for (u32 r = 0; r < NI; r++) {
    if (fabsf(got[r] - want[r]) > 1e-4f) {
      fprintf(stderr, "  %s: out[%u] got=%g want=%g\n", what, r, got[r], want[r]);
    }
    CHECK(fabsf(got[r] - want[r]) <= 1e-4f);
  }
}

static void check_both(Term root, Term out, Term x_buf, f32 const *x,
                       f32 const *want, char const *what) {
  f32 got[NI];
  char label[64];
  CHECK(run_rendered(root, x, got));
  snprintf(label, sizeof label, "%s rendered", what);
  check_close(got, want, label);
  CHECK(run_walker(root, out, x_buf, x, got));
  snprintf(label, sizeof label, "%s walker", what);
  check_close(got, want, label);
}

static void fill_x(f32 *x) {
  for (u32 n = 0; n < NX; n++) {
    x[n] = 0.25f * (f32)((n * 7u) % 11u) - 1.0f + 0.03125f * (f32)(n % 5u);
  }
}

static Term max_of_sum(Term out, Term x_buf, u32 unroll_l) {
  Term i = uop_range(0, KAX_LOOP,   NI);
  Term j = uop_range(1, KAX_REDUCE, NJ);
  Term k = uop_range(2, KAX_REDUCE, NK);
  Term l = uop_range(3, KAX_REDUCE, NL);
  u32 kl[2] = { 2, 3 };
  Term sum = uop_reduce_multi(REDUCE_SUM, 2, kl, uop_index_e(x_buf, x_index(i, j, k, l)));
  Term root = uop_store(out, i, uop_reduce(REDUCE_MAX, 1, sum));
  if (unroll_l) {
    KOpt opt = { KOP_UNROLL, 3, NL };
    root = uop_dag_apply_kopt(root, opt);
  }
  return root;
}

static void want_max_of_sum(f32 const *x, f32 *want) {
  for (u32 r = 0; r < NI; r++) {
    f32 m = -INFINITY;
    for (u32 jj = 0; jj < NJ; jj++) {
      f32 s = 0.0f;
      for (u32 q = 0; q < NK * NL; q++) {
        s += x[(r * NJ + jj) * NK * NL + q];
      }
      m = (s > m) ? s : m;
    }
    want[r] = m;
  }
}

static void test_max_over_one_of_sum_over_two(void) {
  u32 xdims[1] = { NX };
  u32 odims[1] = { NI };
  Term x_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, xdims, 1);
  Term out   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims, 0);
  f32 x[NX], want[NI];
  fill_x(x);
  want_max_of_sum(x, want);
  check_both(max_of_sum(out, x_buf, 0), out, x_buf, x, want, "A");
}

static void test_sum_over_two_of_max_over_one(void) {
  u32 xdims[1] = { NX };
  u32 odims[1] = { NI };
  Term x_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, xdims, 1);
  Term out   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims, 0);
  Term i = uop_range(0, KAX_LOOP,   NI);
  Term j = uop_range(1, KAX_REDUCE, NJ);
  Term k = uop_range(2, KAX_REDUCE, NK);
  Term l = uop_range(3, KAX_REDUCE, NL);
  Term mx = uop_reduce(REDUCE_MAX, 3, uop_index_e(x_buf, x_index(i, j, k, l)));
  u32 jk[2] = { 1, 2 };
  Term root = uop_store(out, i, uop_reduce_multi(REDUCE_SUM, 2, jk, mx));
  f32 x[NX], want[NI];
  fill_x(x);
  for (u32 r = 0; r < NI; r++) {
    want[r] = 0.0f;
    for (u32 q = 0; q < NJ * NK; q++) {
      f32 m = -INFINITY;
      for (u32 ll = 0; ll < NL; ll++) {
        f32 v = x[(r * NJ * NK + q) * NL + ll];
        m = (v > m) ? v : m;
      }
      want[r] += m;
    }
  }
  check_both(root, out, x_buf, x, want, "B");
}

static void test_single_axis_links(void) {
  u32 ydims[1] = { NI * NJ * NK };
  u32 odims[1] = { NI };
  Term y_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, ydims, 1);
  Term out   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims, 0);
  Term i = uop_range(0, KAX_LOOP,   NI);
  Term j = uop_range(1, KAX_REDUCE, NJ);
  Term k = uop_range(2, KAX_REDUCE, NK);
  Term ij = uop_int_binary(UOP_IADD,
                           uop_int_binary(UOP_IMUL, i, uop_const(DT_INT32, NJ)), j);
  Term addr = uop_int_binary(UOP_IADD,
                             uop_int_binary(UOP_IMUL, ij, uop_const(DT_INT32, NK)), k);
  Term sum = uop_reduce(REDUCE_SUM, 2, uop_index_e(y_buf, addr));
  Term root = uop_store(out, i, uop_reduce(REDUCE_MAX, 1, sum));
  f32 x[NX], want[NI];
  fill_x(x);
  for (u32 r = 0; r < NI; r++) {
    f32 m = -INFINITY;
    for (u32 jj = 0; jj < NJ; jj++) {
      f32 s = 0.0f;
      for (u32 kk = 0; kk < NK; kk++) {
        s += x[(r * NJ + jj) * NK + kk];
      }
      m = (s > m) ? s : m;
    }
    want[r] = m;
  }
  check_both(root, out, y_buf, x, want, "C");
}

static void test_max_of_sum_with_unrolled_link(void) {
  u32 xdims[1] = { NX };
  u32 odims[1] = { NI };
  Term x_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, xdims, 1);
  Term out   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims, 0);
  Term root  = max_of_sum(out, x_buf, 1);
  CHECK(root != 0);
  if (root == 0) {
    return;
  }
  f32 x[NX], want[NI];
  fill_x(x);
  want_max_of_sum(x, want);
  check_both(root, out, x_buf, x, want, "D");
}

int main(void) {
  thvm_init();
  TEST_BEGIN("render-reduce-chain/max-over-one-of-sum-over-two");
  test_max_over_one_of_sum_over_two();
  TEST_BEGIN("render-reduce-chain/sum-over-two-of-max-over-one");
  test_sum_over_two_of_max_over_one();
  TEST_BEGIN("render-reduce-chain/single-axis-links");
  test_single_axis_links();
  TEST_BEGIN("render-reduce-chain/max-of-sum-with-unrolled-link");
  test_max_of_sum_with_unrolled_link();
  thvm_free();
  TEST_REPORT();
}
