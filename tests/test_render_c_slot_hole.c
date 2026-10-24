// test_render_c_slot_hole.c -- the C99 kernel renderer and input-slot holes.
//
// cg_render_uop_kernel_c_root numbers a kernel's inputs by UOP_BUFFER
// instance.  A slot no BUFFER claims (a hole) is an input the kernel does
// not read: it renders with no declaration.  Declining it instead pushed
// whole conv+BatchNorm kernels onto the CPU walker.  A slot holding a bare
// TAG_TEN leaf, no UOP_BUFFER, still declines: it has no dtype to declare.  A
// BUFFER of dtype 0 is a bool one (DT_BOOL is 0) and renders as unsigned char;
// declining it sent every kernel reading a bool mask to the walker.
#include "../src/thvm.c"
#include "test.h"

#define N 4

static u32 render_c(Term root, char *src, size_t cap) {
  FILE *fp = fmemopen(src, cap, "w");
  if (fp == NULL) {
    return 0;
  }
  cg_render_uop_kernel_c_root(root, "k_hole", fp);
  long n = ftell(fp);
  fclose(fp);
  src[n > 0 ? n : 0] = '\0';
  return (u32)(n > 0 ? n : 0);
}

// out[i] = x[i] + z[i]; x is input slot 1 and z slot 3, so slot 2 is a hole.
static void test_unread_hole_renders(void) {
  u32 d[1] = { N };
  Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 0);
  Term x   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 1);
  Term z   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 3);
  Term i   = uop_range(0, KAX_LOOP, N);
  Term root = uop_store(out, i, uop_binary(UOP_ADD, uop_index_e(x, i),
                                           uop_index_e(z, i)));
  static char src[16384];
  CHECK(render_c(root, src, sizeof src) > 0);
  CHECK(strstr(src, "*in0 =") != NULL);
  CHECK(strstr(src, "*in2 =") != NULL);
  CHECK(strstr(src, "*in1 =") == NULL);
  f32 xv[N] = { 1, 2, 3, 4 }, hv[N] = { 100, 100, 100, 100 }, zv[N] = { 10, 20, 30, 40 };
  f32 got[N] = { 0 };
  const void *ins[3]    = { xv, hv, zv };
  unsigned    numels[3] = { N, N, N };
  CHECK(cpu_jit_run_root(root, got, ins, N, numels) == 0);
  for (u32 r = 0; r < N; r++) {
    CHECK(got[r] == xv[r] + zv[r]);
  }
}

// A bare TAG_TEN leaf in a slot is no typed buffer: decline (render nothing).
static void test_untyped_slot_declines(void) {
  u32 d[1] = { N };
  Shape s = {1, { N }};
  Term out  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 0);
  Term leaf = term_new(0, TAG_TEN, DT_FP32, tensor_alloc(&CPU_BACKEND, s, DT_FP32));
  Term i    = uop_range(0, KAX_LOOP, N);
  Term root = uop_store(out, i, uop_index_e(leaf, i));
  static char src[16384];
  CHECK(render_c(root, src, sizeof src) == 0);
}

// A bool (dtype 0) slot renders and reads its bytes as 0 / 1.
static void test_bool_slot_renders(void) {
  u32 d[1] = { N };
  Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 0);
  Term m   = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_BOOL, 1, d, 1);
  Term i   = uop_range(0, KAX_LOOP, N);
  Term root = uop_store(out, i, uop_cast(uop_index_e(m, i), DT_FP32));
  static char src[16384];
  CHECK(render_c(root, src, sizeof src) > 0);
  CHECK(strstr(src, "const unsigned char *in0") != NULL);
  unsigned char mv[N] = { 1, 0, 0, 1 };
  f32 got[N] = { 0 };
  const void *ins[1]    = { mv };
  unsigned    numels[1] = { N };
  CHECK(cpu_jit_run_root(root, got, ins, N, numels) == 0);
  for (u32 r = 0; r < N; r++) {
    CHECK(got[r] == (f32)mv[r]);
  }
}

// The body-reference check matches `in<k>` as a whole identifier only.
static void test_names_ident(void) {
  CHECK(rmu_src_names_input("x = in1[i];", 1) == 1);
  CHECK(rmu_src_names_input("x = in10[i];", 1) == 0);
  CHECK(rmu_src_names_input("x = min1[i];", 1) == 0);
  CHECK(rmu_src_names_input("f(in1)", 1) == 1);
  CHECK(rmu_src_names_input("in1", 1) == 1);
  CHECK(rmu_src_names_input("x = in2[i];", 1) == 0);
}

// A body that names a hole means discovery missed a buffer: decline.
static void test_named_hole_declines(void) {
  char const *body = "  out[i] = in0[i] + in1[i];\n";
  Term slots[3] = { 1, 1, 0 };
  CHECK(rmu_src_names_hole(body, slots, 2) == 1);
  Term full[3] = { 1, 1, 1 };
  CHECK(rmu_src_names_hole(body, full, 2) == 0);
}

int main(void) {
  thvm_init();
  TEST_BEGIN("render-c-slot-hole/unread-hole-renders");
  test_unread_hole_renders();
  TEST_BEGIN("render-c-slot-hole/untyped-slot-declines");
  test_untyped_slot_declines();
  TEST_BEGIN("render-c-slot-hole/bool-slot-renders");
  test_bool_slot_renders();
  TEST_BEGIN("render-c-slot-hole/names-ident");
  test_names_ident();
  TEST_BEGIN("render-c-slot-hole/named-hole-declines");
  test_named_hole_declines();
  thvm_free();
  TEST_REPORT();
}
