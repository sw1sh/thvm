// test_render_c_slot_hole.c -- the C99 kernel renderer and input-slot holes.
//
// cg_render_uop_kernel_c_root numbers a kernel's inputs by UOP_BUFFER
// instance.  A slot no BUFFER claims (a hole) is an input the kernel does
// not read: it renders with no declaration.  Declining it instead pushed
// whole conv+BatchNorm kernels onto the CPU walker.  A slot holding
// something other than a real buffer (dtype 0) still declines: it would
// render as `unsigned char` while the body reads floats.
#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

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

typedef void (*KFn)(void *, const void *const *, unsigned, const unsigned *,
                    const unsigned *);

static int compile_run(char const *src, const void *const *ins, f32 *out) {
  char c_path[] = "/tmp/thvm_slot_hole_XXXXXX.c";
  int fd = mkstemps(c_path, 2);
  if (fd < 0) {
    return 0;
  }
  FILE *f = fdopen(fd, "w");
  fputs(src, f);
  fclose(f);
  char so_path[sizeof c_path + 8];
  snprintf(so_path, sizeof so_path, "%s.so", c_path);
  char cmd[512];
  snprintf(cmd, sizeof cmd, "clang -O1 -fPIC -shared -o '%s' '%s' 2>/dev/null",
           so_path, c_path);
  int ok = system(cmd) == 0;
  void *h = ok ? dlopen(so_path, RTLD_NOW | RTLD_LOCAL) : NULL;
  KFn k = h ? (KFn)dlsym(h, "k_hole") : NULL;
  if (k != NULL) {
    unsigned numels[3] = { N, N, N };
    unsigned kv[1] = { 0 };
    k(out, ins, N, numels, kv);
  }
  if (h != NULL) {
    dlclose(h);
  }
  unlink(c_path);
  unlink(so_path);
  return k != NULL;
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
  const void *ins[3] = { xv, hv, zv };
  CHECK(compile_run(src, ins, got));
  for (u32 r = 0; r < N; r++) {
    CHECK(got[r] == xv[r] + zv[r]);
  }
}

// A dtype-0 slot is not a real buffer: decline (render nothing).
static void test_untyped_slot_declines(void) {
  u32 d[1] = { N };
  Term out = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, d, 0);
  Term x   = uop_buffer_inst(UOP_SCOPE_GLOBAL, 0,       1, d, 1);
  Term i   = uop_range(0, KAX_LOOP, N);
  Term root = uop_store(out, i, uop_index_e(x, i));
  static char src[16384];
  CHECK(render_c(root, src, sizeof src) == 0);
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
  TEST_BEGIN("render-c-slot-hole/names-ident");
  test_names_ident();
  TEST_BEGIN("render-c-slot-hole/named-hole-declines");
  test_named_hole_declines();
  thvm_free();
  TEST_REPORT();
}
