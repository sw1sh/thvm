// test_cpu_jit_warmup.c - THVM_CPU_JIT_WARMUP governs every CPU JIT path.
//
// A kernel runs compiled only from the fire at which its source has fired
// THVM_CPU_JIT_WARMUP times; the UOp walker runs the fires before it.  The
// batched precompile at realize, the in-memory compile cache and the dylibs
// on disk under /tmp all stand behind that gate, so a threshold no run
// reaches (1000000000, which walker-mode test runs set) sends the kernel to
// the walker even when its dylib is on disk.
//
// One elementwise kernel, out = a * b + salt, is realized through
// thvm_realize in five child processes, one per phase, since the JIT reads
// its knobs once per process.  The salt is drawn per run, so the kernel's
// source, its JIT key and every cached artifact of it are new to the run:
//   warm-precompile   warmup 0, precompile on: the combined cold-start
//                     dylib compiles the kernel, no per-kernel dylib;
//   gated-precompile  warmup 1000000000, precompile on, that combined dylib
//                     on disk: three realizes walk;
//   threshold         warmup 3, precompile on: fires 1 and 2 walk, fires 3
//                     and 4 run compiled;
//   warm-lazy         warmup 0, precompile off: the dispatch compiles the
//                     per-kernel dylib;
//   gated-lazy        warmup 1000000000, precompile off, that per-kernel
//                     dylib on disk: three realizes walk.
// The route of each fire is the dispatch kind the CPU ladder records.
#include "../src/thvm.c"
#include "test.h"
#include <sys/wait.h>
#include <time.h>

#define WG_N 64

static f32 WG_SALT;

static u32 wg_bits(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

static Term wg_leaf(f32 const *data) {
  Shape s   = {0};
  s.ndim    = 1;
  s.dims[0] = WG_N;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, WG_N * sizeof(f32));
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

// Realize out = a * b + salt, check its values, and return the kid of the
// kernel that wrote it (0 when none did).
static u32 wg_realize(void) {
  f32 a[WG_N], b[WG_N];
  for (u32 i = 0; i < WG_N; i++) {
    a[i] = (f32)i;
    b[i] = 3.0f;
  }
  u32 const one[1] = { 1 };
  u32 const n[1]   = { WG_N };
  Term salt = uop_expand(uop_reshape(uop_const(DT_FP32, wg_bits(WG_SALT)), 1, one), 1, n);
  Term expr = uop_binary(UOP_ADD, uop_binary(UOP_MUL, wg_leaf(a), wg_leaf(b)), salt);
  u32  lo   = KERNELS_NEXT;
  Term done = term_resolve(thvm_realize(expr));
  CHECK(term_tag(done) == TAG_TEN);
  if (term_tag(done) != TAG_TEN) {
    return 0;
  }
  u32 tid = (u32)term_val(done);
  f32 got[WG_N];
  TENS[tid].backend->buf_read(TENS[tid].buf_id, got, sizeof got);
  int exact = 1;
  for (u32 i = 0; i < WG_N; i++) {
    if (got[i] != 3.0f * (f32)i + WG_SALT) {
      exact = 0;
    }
  }
  CHECK(exact);
  u32 kid = 0;
  for (u32 k = lo; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].output_tid == tid) {
      kid = k;
    }
  }
  CHECK(kid != 0);
  return kid;
}

static u64 wg_key(u32 kid) {
  char *src = cpu_jit_render_canon(&KERNELS[kid]);
  if (src == NULL) {
    return 0;
  }
  u64 key = cpu_src_hash(src);
  free(src);
  return key;
}

// The combined dylib a precompile batch of this one kernel builds.
static void wg_combined_path(char *out, size_t cap, u64 key) {
  CpuJitJob job;
  memset(&job, 0, sizeof job);
  job.key = key;
  cpu_jit_dylib_path(out, cap, "comb_", cpu_jit_batch_key(&job, 1));
}

static int wg_on_disk(char const *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static int wg_combined_cached(u64 key) {
  char path[256];
  wg_combined_path(path, sizeof path, key);
  return wg_on_disk(path);
}

static void wg_forget(u64 key) {
  char path[256];
  snprintf(path, sizeof path, "/tmp/thvm_jit_%016llx.c", (unsigned long long)key);
  unlink(path);
  snprintf(path, sizeof path, "/tmp/thvm_jit_comb_src_%016llx.c", (unsigned long long)key);
  unlink(path);
  cpu_jit_dylib_path(path, sizeof path, "", key);
  unlink(path);
  wg_combined_path(path, sizeof path, key);
  unlink(path);
  CpuJitJob job;
  memset(&job, 0, sizeof job);
  job.key = key;
  snprintf(path, sizeof path, "/tmp/thvm_jit_comb_%016llx.c",
           (unsigned long long)cpu_jit_batch_key(&job, 1));
  unlink(path);
}

static void wg_expect_route(u32 kid, u32 kind) {
  CHECK_EQ(cg_kernel_dispatch_kind(kid), kind);
}

static void wg_warm_precompile(void) {
  u32 kid = wg_realize();
  wg_expect_route(kid, KDISPATCH_JIT);
  u64 key = wg_key(kid);
  CHECK(wg_combined_cached(key));
  CHECK(!cpu_jit_dylib_cached(key));
}

static void wg_gated_precompile(void) {
  for (u32 r = 0; r < 3; r++) {
    u32 kid = wg_realize();
    wg_expect_route(kid, KDISPATCH_INTERPRETER);
    CHECK(wg_combined_cached(wg_key(kid)));
  }
}

static void wg_threshold(void) {
  u32 const want[4] = { KDISPATCH_INTERPRETER, KDISPATCH_INTERPRETER,
                        KDISPATCH_JIT, KDISPATCH_JIT };
  for (u32 r = 0; r < 4; r++) {
    wg_expect_route(wg_realize(), want[r]);
  }
}

static void wg_warm_lazy(void) {
  u32 kid = wg_realize();
  wg_expect_route(kid, KDISPATCH_JIT);
  CHECK(cpu_jit_dylib_cached(wg_key(kid)));
}

static void wg_gated_lazy(void) {
  u64 key = 0;
  for (u32 r = 0; r < 3; r++) {
    u32 kid = wg_realize();
    wg_expect_route(kid, KDISPATCH_INTERPRETER);
    key = wg_key(kid);
    CHECK(cpu_jit_dylib_cached(key));
  }
  wg_forget(key);
}

static void wg_phase(char const *name, char const *warmup, char const *parallel,
                     void (*body)(void)) {
  TEST_BEGIN(name);
  fflush(NULL);
  pid_t pid = fork();
  if (pid == 0) {
    setenv("THVM_CPU_JIT_WARMUP", warmup, 1);
    setenv("THVM_CPU_JIT_PARALLEL", parallel, 1);
    test_failures = 0;
    test_total    = 0;
    thvm_init();
    body();
    thvm_free();
    fflush(NULL);
    _exit(test_failures == 0 ? 0 : 1);
  }
  int status = 0;
  CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void) {
  WG_SALT = (f32)(1u + ((u32)getpid() * 2654435761u ^ (u32)time(NULL)) % 1000003u);

  wg_phase("cpu-jit-warmup/warm-precompile",  "0",          "1", wg_warm_precompile);
  wg_phase("cpu-jit-warmup/gated-precompile", "1000000000", "1", wg_gated_precompile);
  wg_phase("cpu-jit-warmup/threshold",        "3",          "1", wg_threshold);
  wg_phase("cpu-jit-warmup/warm-lazy",        "0",          "0", wg_warm_lazy);
  wg_phase("cpu-jit-warmup/gated-lazy",       "1000000000", "0", wg_gated_lazy);

  TEST_REPORT();
}
