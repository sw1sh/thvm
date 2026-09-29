// test_nested_reduce_unroll.c -- an UNROLL split of one reduce's axis, in a
// kernel that nests reduces, folds into exactly that reduce.
//
// uop_dag_apply_split rewrites the split axis to
// IADD(IMUL(outer, k), OPT(inner, UNROLL, k)) while REDUCE.axes keeps naming
// only `outer`, so the unrolled `inner` half lies inside the body of every
// reduce that encloses the split one.  Two shapes, each checked for
// ownership (uop_reduce_owns_split_axis), for the rendered C (compiled with the JIT's C compiler + run)
// and for the CPU UOp walker:
//   A  out[i] = 0.5 * sum_j (x[i,j] + sum_k x[i,k])   UNROLL k: the INNER
//      reduce owns it.  Unowned, the outer loop re-summed the inner reduce
//      once per lane (the 9x nested-reduce broadcast bug).
//   B  out[i] = sum_j y[i,j] * sum_k z[i,j,k]         UNROLL j: the OUTER
//      reduce owns it while the inner body reads it.  Unowned, the inner
//      reduce summed z over every j.
#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

#define NI 2
#define NJ 3
#define NK 3

// The REDUCE among a binary node's two operands, or 0.
static Term reduce_operand(Term bin) {
  if (term_tag(bin) != TAG_UOP) {
    return 0;
  }
  for (u32 s = 0; s < 2; s++) {
    Term c = heap_read(term_val(bin) + s);
    if (term_tag(c) == TAG_UOP && term_ext(c) == UOP_REDUCE) {
      return c;
    }
  }
  return 0;
}

static Term build_a(Term out, Term x) {
  Term i     = uop_range(0, KAX_LOOP,   NI);
  Term j     = uop_range(1, KAX_REDUCE, NJ);
  Term k     = uop_range(2, KAX_REDUCE, NK);
  Term row   = uop_int_binary(UOP_IMUL, i, uop_const(DT_INT32, NJ));
  Term inner = uop_reduce(REDUCE_SUM, 2,
                          uop_index_e(x, uop_int_binary(UOP_IADD, row, k)));
  Term body  = uop_binary(UOP_ADD,
                          uop_index_e(x, uop_int_binary(UOP_IADD, row, j)), inner);
  Term outer = uop_reduce(REDUCE_SUM, 1, body);
  Term half  = uop_const(DT_FP32, f32_bits(0.5f));
  return uop_store(out, i, uop_binary(UOP_MUL, outer, half));
}

static Term build_b(Term out, Term y, Term z) {
  Term i     = uop_range(0, KAX_LOOP,   NI);
  Term j     = uop_range(1, KAX_REDUCE, NJ);
  Term k     = uop_range(2, KAX_REDUCE, NK);
  Term ij    = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, i, uop_const(DT_INT32, NJ)), j);
  Term ijk   = uop_int_binary(UOP_IADD,
                              uop_int_binary(UOP_IMUL, ij, uop_const(DT_INT32, NK)), k);
  Term inner = uop_reduce(REDUCE_SUM, 2, uop_index_e(z, ijk));
  Term outer = uop_reduce(REDUCE_SUM, 1,
                          uop_binary(UOP_MUL, uop_index_e(y, ij), inner));
  return uop_store(out, i, outer);
}

// Render `root` to C, compile it with the JIT's C compiler and run it.  Returns 1 on a
// completed run.
static int run_rendered(Term root, Term out, Term const *ins, u32 n_in,
                        const void *const *in_data, unsigned const *numels,
                        f32 *res) {
  static char src[32768];
  FILE *fp = fmemopen(src, sizeof src, "w");
  if (fp == NULL) {
    return 0;
  }
  cg_render_uop_kernel_c(root, "k_nested", out, ins, n_in, fp);
  fclose(fp);
  char path[] = "/tmp/thvm_nested_unroll_XXXXXX.c";
  int fd = mkstemps(path, 2);
  if (fd < 0) {
    return 0;
  }
  FILE *cf = fdopen(fd, "w");
  fputs(src, cf);
  fclose(cf);
  char so[300];
  snprintf(so, sizeof so, "%s.dylib", path);
  char cmd[800];
  snprintf(cmd, sizeof cmd, "%s -O1 -shared -fPIC -o %s %s 2>/dev/null", cpu_cc(), so, path);
  void *h = (system(cmd) == 0) ? dlopen(so, RTLD_NOW) : NULL;
  typedef void (*kfn)(void *, const void *const *, unsigned,
                      const unsigned *, const unsigned *);
  kfn kf = (h != NULL) ? (kfn)dlsym(h, "k_nested") : NULL;
  if (kf != NULL) {
    kf(res, in_data, NI, numels, NULL);
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
static int run_walker(Term root, Term out, Term const *ins, u32 n_in,
                      f32 const *const *in_data, unsigned const *numels,
                      f32 *res) {
  KernelEntry ke;
  memset(&ke, 0, sizeof ke);
  u32 tids[2] = {0, 0};                   // raw buffer pointers, no view bias
  u32 in_buf_ids[2];
  Term lift_ins[2] = {0, 0};
  for (u32 s = 0; s < n_in; s++) {
    in_buf_ids[s] = TENS[mk_f32(numels[s], in_data[s])].buf_id;
    lift_ins[s]   = ins[s];
  }
  ke.cached_lift.in_bufs = lift_ins;
  ke.cached_lift.in_cap  = 2;
  u32 tout = mk_f32(NI, NULL);
  ke.n_inputs               = n_in;
  ke.input_tids             = tids;
  ke.output_dtype           = DT_FP32;
  ke.cached_lift.store_root = root;
  ke.cached_lift.out_buf    = out;
  ke.cached_lift.n_inputs   = n_in;
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

static void test_inner_reduce_owns_its_unroll(void) {
  u32 xdims[1] = { NI * NJ };
  u32 odims[1] = { NI };
  Term x   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, xdims, 1);
  Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims, 0);
  KOpt opt = { KOP_UNROLL, 2, NK };
  Term root  = uop_dag_apply_kopt(build_a(out, x), opt);
  Term outer = (root != 0) ? reduce_operand(heap_read(term_val(root) + 2)) : 0;
  Term inner = (outer != 0) ? reduce_operand(uop_reduce_src(outer)) : 0;
  CHECK(inner != 0);
  if (inner == 0) {
    return;
  }
  CHECK(uop_reduce_owns_split_axis(inner, 3) == 1);   // k's unrolled half
  CHECK(uop_reduce_owns_split_axis(outer, 3) == 0);

  f32 xv[NI * NJ], want[NI], got[NI];
  for (u32 r = 0; r < NI; r++) {
    f32 row = 0.0f;
    for (u32 c = 0; c < NJ; c++) {
      xv[r * NJ + c] = 0.25f * (f32)(r * NJ + c) - 0.5f;
      row += xv[r * NJ + c];
    }
    want[r] = 0.5f * (row + (f32)NJ * row);
  }
  const void *in_data[1] = { xv };
  f32 const  *in_f32 [1] = { xv };
  unsigned    numels [1] = { NI * NJ };
  CHECK(run_rendered(root, out, &x, 1, in_data, numels, got));
  check_close(got, want, "A rendered");
  CHECK(run_walker(root, out, &x, 1, in_f32, numels, got));
  check_close(got, want, "A walker");
}

static void test_outer_reduce_owns_unroll_read_by_inner(void) {
  u32 ydims[1] = { NI * NJ };
  u32 zdims[1] = { NI * NJ * NK };
  u32 odims[1] = { NI };
  Term y   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, ydims, 1);
  Term z   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, zdims, 2);
  Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, odims, 0);
  KOpt opt = { KOP_UNROLL, 1, NJ };
  Term root  = uop_dag_apply_kopt(build_b(out, y, z), opt);
  Term outer = (root != 0) ? heap_read(term_val(root) + 2) : 0;
  Term inner = (outer != 0) ? reduce_operand(uop_reduce_src(outer)) : 0;
  CHECK(inner != 0);
  if (inner == 0) {
    return;
  }
  CHECK(uop_reduce_owns_split_axis(outer, 2) == 1);   // j's unrolled half
  CHECK(uop_reduce_owns_split_axis(inner, 2) == 0);

  f32 yv[NI * NJ], zv[NI * NJ * NK], want[NI], got[NI];
  for (u32 n = 0; n < NI * NJ; n++) {
    yv[n] = 0.5f + 0.125f * (f32)n;
  }
  for (u32 n = 0; n < NI * NJ * NK; n++) {
    zv[n] = 0.1f * (f32)n - 1.0f;
  }
  for (u32 r = 0; r < NI; r++) {
    want[r] = 0.0f;
    for (u32 c = 0; c < NJ; c++) {
      f32 zs = 0.0f;
      for (u32 q = 0; q < NK; q++) {
        zs += zv[(r * NJ + c) * NK + q];
      }
      want[r] += yv[r * NJ + c] * zs;
    }
  }
  Term ins[2] = { y, z };
  const void *in_data[2] = { yv, zv };
  f32 const  *in_f32 [2] = { yv, zv };
  unsigned    numels [2] = { NI * NJ, NI * NJ * NK };
  CHECK(run_rendered(root, out, ins, 2, in_data, numels, got));
  check_close(got, want, "B rendered");
  CHECK(run_walker(root, out, ins, 2, in_f32, numels, got));
  check_close(got, want, "B walker");
}

int main(void) {
  thvm_init();
  TEST_BEGIN("nested-reduce-unroll/inner-reduce-owns-its-unroll");
  test_inner_reduce_owns_its_unroll();
  TEST_BEGIN("nested-reduce-unroll/outer-reduce-owns-unroll-read-by-inner");
  test_outer_reduce_owns_unroll_read_by_inner();
  thvm_free();
  TEST_REPORT();
}
