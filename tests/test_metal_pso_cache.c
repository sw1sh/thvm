// test_metal_pso_cache.c -- persistent on-disk PSO cache.
//
// Covers the THVM_METAL_PSO_CACHE_DIR / pso-<key>.bin path that
// metal_tile_jit_build populates via MTLBinaryArchive.  Four
// scenarios:
//   1) In-memory hit on the second pipeline call.
//   2) Disk hit after dropping the in-memory PSO table.
//   3) Truncated .bin falls through to fresh build.
//   4) THVM_METAL_PSO_CACHE=0 writes no files.
//
// Surgical -- builds a tiny ADD kernel via the same path as
// test_metal_real's build_metal_tile_add_kernel, dispatches once,
// and verifies the disk-load path returns a valid PSO that still
// produces correct output.

#include "../src/thvm.c"
#include "test.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Counters / probes from build/backend_metal.o.
extern u64  thvm_metal_pso_cache_disk_hits(void);
extern u64  thvm_metal_pso_cache_disk_misses(void);
extern u64  thvm_metal_pso_cache_bytes_r(void);
extern u64  thvm_metal_pso_cache_bytes_w(void);
extern u64  thvm_metal_jit_hits(void);
extern u64  thvm_metal_jit_misses(void);
extern int  thvm_metal_pso_cache_enabled(void);
extern const char *thvm_metal_pso_cache_dir(void);
extern void thvm_metal_jit_drop_in_memory_psos(void);

// Same builder as test_metal_real.  Kept local-static so tests don't
// drift in lockstep with that file's harness changes.
static u32 build_add_kernel(u32 extent, u32 groups, u32 threads) {
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  kernel_inputs_reserve(ke, 2);
  ke->n_inputs        = 2;
  ke->input_tids[0]   = 0;
  ke->input_tids[1]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_dtypes[1] = DT_FP32;
  ke->input_numels[0] = extent;
  ke->input_numels[1] = extent;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = extent;

  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | extent);
  u32 pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pb = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 1);
  u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 src_a[2] = {pa, r0};
  u32 src_b[2] = {pb, r0};
  u32 src_c[2] = {pc, r0};
  u32 ia = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_a, 1);
  u32 ib = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_b, 1);
  u32 ic = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_c, 1);
  u32 la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 lb = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ib);
  u32 sum = rangeify_emit_binary(ke, S_ADD, DT_FP32, la, lb);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, sum);
  u32 root_src[2] = {sto, r0};
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, root_src, 0);

  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = extent;
  ke->schedule = &ke->_local_schedule;
  memset(ke->schedule, 0, sizeof(KpSchedule));
  KOpt local_op  = { .op = KOP_LOCAL,  .axis = 0, .arg = threads };
  KOpt global_op = { .op = KOP_GLOBAL, .axis = 0, .arg = groups  };
  CHECK(kernel_apply_opt(ke, local_op));
  CHECK(kernel_apply_opt(ke, global_op));
  return kid;
}

static int metal_available(void) {
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  int ok = 0;
  if (CURRENT_BACKEND == &METAL_BACKEND) {
    u32 bid = CURRENT_BACKEND->buf_alloc(4);
    if (bid != 0) {
      CURRENT_BACKEND->buf_free(bid);
      ok = 1;
    }
  }
  thvm_free();
  return ok;
}

// Returns count of regular files in `dir`.  -1 on opendir failure.
static int count_files_in_dir(const char *dir) {
  DIR *d = opendir(dir);
  if (d == NULL) return -1;
  int n = 0;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (ent->d_name[0] == '.') continue;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) n++;
  }
  closedir(d);
  return n;
}

// Remove every regular file under `dir` (one level deep; the
// runtime never creates subdirs).  Best-effort.
static void clear_dir(const char *dir) {
  DIR *d = opendir(dir);
  if (d == NULL) return;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (ent->d_name[0] == '.') continue;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
    (void)unlink(path);
  }
  closedir(d);
}

// Run the ADD kernel against [1..8] + [8..1] and verify output.
// Returns 0 on success, non-zero on failure.
static int run_add_and_verify(u32 kid) {
  KernelEntry *ke = &KERNELS[kid];
  f32 in0[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  f32 in1[8] = {8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};
  f32 out[8] = {0};
  u32 in0_buf = METAL_BACKEND.buf_alloc(sizeof(in0));
  u32 in1_buf = METAL_BACKEND.buf_alloc(sizeof(in1));
  u32 out_buf = METAL_BACKEND.buf_alloc(sizeof(out));
  if (in0_buf == 0 || in1_buf == 0 || out_buf == 0) return 1;
  METAL_BACKEND.buf_write(in0_buf, in0, sizeof(in0));
  METAL_BACKEND.buf_write(in1_buf, in1, sizeof(in1));
  METAL_BACKEND.buf_write(out_buf, out, sizeof(out));
  u32 in_bufs[2] = {in0_buf, in1_buf};
  if (METAL_BACKEND.dispatch_kernel(ke, in_bufs, out_buf) != 0) return 2;
  METAL_BACKEND.buf_read(out_buf, out, sizeof(out));
  for (u32 i = 0; i < 8; i++) {
    if (out[i] != in0[i] + in1[i]) return 3;
  }
  return 0;
}

int main(void) {
  if (!metal_available()) {
    PENDING("no Metal device available");
  }

  // Isolate the on-disk cache to a per-process temp dir so the test
  // doesn't pollute $HOME/.cache or race with parallel runs.
  char tmpdir[1024];
  snprintf(tmpdir, sizeof(tmpdir),
           "/tmp/thvm_metal_pso_cache_test_%d", (int)getpid());
  // Pre-clean (in case a prior run aborted before cleanup).
  clear_dir(tmpdir);
  (void)rmdir(tmpdir);

  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE",    "1",     1);
  setenv("THVM_METAL_PSO_CACHE_DIR", tmpdir, 1);
  unsetenv("THVM_METAL_PSO_CACHE");  // default on

  // ============================================================
  TEST_BEGIN("pso_cache/in-memory-hit-on-second-pipeline-call");
  thvm_init();
  CHECK(thvm_metal_pso_cache_enabled());
  CHECK_EQ(strcmp(thvm_metal_pso_cache_dir(), tmpdir), 0);
  {
    u32 kid = build_add_kernel(8, 2, 4);
    CHECK_EQ(run_add_and_verify(kid), 0);
    u64 hits_before = thvm_metal_jit_hits();
    // Second dispatch of the same kernel must hit the in-memory PSO
    // table.  The first dispatch above either built or loaded
    // from disk -- either way, the in-memory slot is populated, so
    // hits should increment.
    KernelEntry *ke = &KERNELS[kid];
    f32 a[8] = {0}, b[8] = {0}, out[8] = {0};
    u32 in0_buf = METAL_BACKEND.buf_alloc(sizeof(a));
    u32 in1_buf = METAL_BACKEND.buf_alloc(sizeof(b));
    u32 out_buf = METAL_BACKEND.buf_alloc(sizeof(out));
    METAL_BACKEND.buf_write(in0_buf, a, sizeof(a));
    METAL_BACKEND.buf_write(in1_buf, b, sizeof(b));
    METAL_BACKEND.buf_write(out_buf, out, sizeof(out));
    u32 in_bufs[2] = {in0_buf, in1_buf};
    CHECK_EQ(METAL_BACKEND.dispatch_kernel(ke, in_bufs, out_buf), 0);
    CHECK(thvm_metal_jit_hits() > hits_before);
  }
  thvm_free();

  // ============================================================
  TEST_BEGIN("pso_cache/disk-hit-after-dropping-in-memory");
  // Start from a clean dir so the assertions on bytes_w / file
  // count are deterministic (test 1's first dispatch may have
  // written to disk already).
  clear_dir(tmpdir);
  thvm_init();
  CHECK(thvm_metal_pso_cache_enabled());
  CHECK_EQ(thvm_metal_pso_cache_bytes_w(), 0ULL);
  {
    u32 kid = build_add_kernel(8, 2, 4);
    CHECK_EQ(run_add_and_verify(kid), 0);
    // metal_pso_cache_store must have written exactly one file.
    int n = count_files_in_dir(tmpdir);
    CHECK(n >= 1);
    CHECK(thvm_metal_pso_cache_bytes_w() > 0);
  }
  // Drop in-memory PSO table without nilling the device.  This
  // simulates a process restart for the purposes of this test --
  // the next dispatch must hit the on-disk archive (no MSL recompile
  // path through metal_pso_cache_try_load).
  u64 disk_hits_before = thvm_metal_pso_cache_disk_hits();
  thvm_metal_jit_drop_in_memory_psos();
  {
    u32 kid = build_add_kernel(8, 2, 4);
    CHECK_EQ(run_add_and_verify(kid), 0);
    CHECK_EQ(thvm_metal_pso_cache_disk_hits(), disk_hits_before + 1);
    CHECK(thvm_metal_pso_cache_bytes_r() > 0);
  }
  thvm_free();

  // ============================================================
  TEST_BEGIN("pso_cache/corrupt-file-falls-through-to-fresh-build");
  // Truncate the cached .bin to 16 bytes.  The archive load inside
  // metal_pso_cache_try_load must fail; metal_tile_jit_build then
  // takes the fresh-compile path and rewrites a valid entry.
  {
    DIR *d = opendir(tmpdir);
    CHECK(d != NULL);
    int truncated = 0;
    if (d != NULL) {
      struct dirent *ent;
      while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", tmpdir, ent->d_name);
        FILE *f = fopen(path, "wb");
        if (f != NULL) {
          char garbage[16] = {0};
          fwrite(garbage, 1, sizeof(garbage), f);
          fclose(f);
          truncated++;
        }
      }
      closedir(d);
    }
    CHECK(truncated >= 1);
  }
  thvm_init();
  CHECK(thvm_metal_pso_cache_enabled());
  {
    u64 misses_before = thvm_metal_pso_cache_disk_misses();
    u32 kid = build_add_kernel(8, 2, 4);
    CHECK_EQ(run_add_and_verify(kid), 0);
    // Disk-miss must have ticked at least once (load fail -> fresh
    // build path).  Output must still be correct.
    CHECK(thvm_metal_pso_cache_disk_misses() > misses_before);
  }
  thvm_free();

  // ============================================================
  TEST_BEGIN("pso_cache/env-off-suppresses-disk-writes");
  // Clear the dir, set the env, init, run, verify the dir is empty.
  clear_dir(tmpdir);
  setenv("THVM_METAL_PSO_CACHE", "0", 1);
  thvm_init();
  CHECK(!thvm_metal_pso_cache_enabled());
  {
    u32 kid = build_add_kernel(8, 2, 4);
    CHECK_EQ(run_add_and_verify(kid), 0);
    int n = count_files_in_dir(tmpdir);
    // Two valid states: dir doesn't exist (mkdir skipped) or dir is
    // empty.  Treat -1 (opendir fail) as 0 for the assertion.
    if (n < 0) n = 0;
    CHECK_EQ(n, 0);
  }
  thvm_free();
  unsetenv("THVM_METAL_PSO_CACHE");

  // ============================================================
  // Cleanup -- leave no garbage in /tmp.
  clear_dir(tmpdir);
  (void)rmdir(tmpdir);
  unsetenv("THVM_METAL_PSO_CACHE_DIR");
  unsetenv("THVM_TILE");
  unsetenv("THVM_BACKEND");

  TEST_REPORT();
}
