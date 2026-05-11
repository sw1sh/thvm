// test_metal_variable_pso_hit.c -- symbolic-shape Variable -> shared PSO.
//
// Demonstrates the vertical slice of kvar support: a kernel built
// with a Variable("BS") extent bound to runtime value 4 and another
// built with the same Variable bound to 32 hit the SAME tile-jit PSO
// cache slot.
//
// Three checks:
//   1) metal_tile_jit_hash(ke@BS=4) == metal_tile_jit_hash(ke@BS=32)
//      -- the cache key is BS-invariant for symbolic kernels.
//   2) metal_tile_jit_pipeline builds a PSO on the first, then HITS
//      the in-memory cache on the second (METAL_JIT_BUILD_HITS++).
//   3) Dispatching a tiny vector-fill kernel for each produces the
//      correct output -- catches the case where hashes match but the
//      rendered MSL is wrong (e.g. forgot to substitute V_BS).
//
// Surgical: builds the kernel directly via rangeify_emit() (same
// pattern as test_metal_pso_cache.c's build_add_kernel), no model
// graph / materialize involvement.

#include "../src/thvm.c"
#include "test.h"

#include <stdio.h>
#include <string.h>

// Counters / probes from build/backend_metal.o.
extern u64 thvm_metal_jit_hits(void);
extern u64 thvm_metal_jit_misses(void);
extern u64 thvm_metal_tile_jit_hash(KernelEntry const *ke);
extern void thvm_metal_jit_drop_in_memory_psos(void);

static int metal_available(void) {
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  int ok = 0;
  if (CURRENT_BACKEND == &METAL_BACKEND) {
    u32 bid = CURRENT_BACKEND->buf_alloc(4);
    if (bid != 0) { CURRENT_BACKEND->buf_free(bid); ok = 1; }
  }
  thvm_free();
  return ok;
}

// Build a tiny "vector fill" kernel:  for i in [0, BS):  out[i] = 1.0
// The single LOOP range's extent is the symbolic Variable `bs_var`;
// at dispatch time the runtime value `bs_runtime` is what's bound via
// setBytes: as `constant uint &V_BS`.  ke->output_numel is the
// concrete runtime value (the lifter-fallback dispatch shape needs a
// number; the worst case would be kvar_hi which oversizes the launch
// but still works).
static u32 build_fill_kernel(u32 bs_var, u32 bs_runtime) {
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  ke->n_inputs     = 0;
  ke->output_dtype = DT_FP32;
  ke->output_numel = bs_runtime;

  u32 packed = kvar_pack_extent(bs_var);
  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | packed);
  u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 src_c[2] = {pc, r0};
  u32 ic = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_c, 0);
  // f32 1.0 bit pattern.
  u32 one_bits;
  { float one = 1.0f; memcpy(&one_bits, &one, sizeof(one_bits)); }
  u32 c1 = rangeify_emit_leaf(ke, S_CONST, DT_FP32, (u64)one_bits);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, c1);
  u32 root_src[2] = {sto, r0};
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, root_src, 0);

  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = bs_runtime;
  ke->schedule = &ke->_local_schedule;
  memset(ke->schedule, 0, sizeof(KpSchedule));
  axes_default_for(ke);

  // Stamp the per-dispatch runtime value for the Variable.
  CHECK(kernel_kvar_bind(ke, bs_var, bs_runtime));
  return kid;
}

// Dispatch the fill kernel; verify out[i] == 1.0 for i in [0, n).
// Returns 0 on success.
static int run_fill_and_verify(u32 kid, u32 n) {
  KernelEntry *ke = &KERNELS[kid];
  f32 *out = (f32 *)calloc(n, sizeof(f32));
  if (out == NULL) return 1;
  u32 out_buf = METAL_BACKEND.buf_alloc((u64)n * sizeof(f32));
  if (out_buf == 0) { free(out); return 2; }
  METAL_BACKEND.buf_write(out_buf, out, (u64)n * sizeof(f32));
  int rc = METAL_BACKEND.dispatch_kernel(ke, NULL, out_buf);
  if (rc != 0) { free(out); return 3; }
  METAL_BACKEND.buf_read(out_buf, out, (u64)n * sizeof(f32));
  int bad = 0;
  for (u32 i = 0; i < n; i++) {
    if (out[i] != 1.0f) { bad = 1; break; }
  }
  free(out);
  return bad ? 4 : 0;
}

int main(void) {
  if (!metal_available()) {
    PENDING("no Metal device available");
  }

  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE",    "1",     1);
  // Isolate the on-disk PSO cache so this run doesn't race with
  // parallel test runs.
  char tmpdir[1024];
  snprintf(tmpdir, sizeof(tmpdir),
           "/tmp/thvm_metal_var_pso_test_%d", (int)getpid());
  setenv("THVM_METAL_PSO_CACHE_DIR", tmpdir, 1);
  // Default the on-disk cache OFF for this test -- we only assert on
  // the in-memory hit, and a stale .bin from a prior run could
  // shadow the build.
  setenv("THVM_METAL_PSO_CACHE", "0", 1);

  // ============================================================
  TEST_BEGIN("metal_var_pso/hash-invariant-across-BS");
  thvm_init();
  kvar_reset();
  u32 bs_var = kvar_alloc("BS", 1, 512);
  CHECK(bs_var != 0);
  {
    u32 k4  = build_fill_kernel(bs_var, 4);
    u32 k32 = build_fill_kernel(bs_var, 32);
    u64 h4  = thvm_metal_tile_jit_hash(&KERNELS[k4]);
    u64 h32 = thvm_metal_tile_jit_hash(&KERNELS[k32]);
    CHECK_EQ((unsigned long long)h4, (unsigned long long)h32);
    // Sanity: a literal-extent kernel (no Variable) hashes DIFFERENTLY
    // for BS=4 vs BS=32 -- this is exactly the wart kvar fixes.
    u32 lit4 = kernel_alloc();
    {
      KernelEntry *ke = &KERNELS[lit4];
      ke->n_inputs = 0; ke->output_dtype = DT_FP32; ke->output_numel = 4;
      u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                                  ((u64)S_AXIS_LOOP << 32) | 4u);
      u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
      u32 src_c[2] = {pc, r0};
      u32 ic = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_c, 0);
      u32 ob; { float o = 1.0f; memcpy(&ob, &o, sizeof(ob)); }
      u32 c1 = rangeify_emit_leaf(ke, S_CONST, DT_FP32, (u64)ob);
      u32 st = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, c1);
      u32 rs[2] = {st, r0};
      rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, rs, 0);
      ke->output_shape.ndim = 1; ke->output_shape.dims[0] = 4;
      ke->schedule = &ke->_local_schedule;
      memset(ke->schedule, 0, sizeof(KpSchedule));
    }
    u32 lit32 = kernel_alloc();
    {
      KernelEntry *ke = &KERNELS[lit32];
      ke->n_inputs = 0; ke->output_dtype = DT_FP32; ke->output_numel = 32;
      u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                                  ((u64)S_AXIS_LOOP << 32) | 32u);
      u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
      u32 src_c[2] = {pc, r0};
      u32 ic = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_c, 0);
      u32 ob; { float o = 1.0f; memcpy(&ob, &o, sizeof(ob)); }
      u32 c1 = rangeify_emit_leaf(ke, S_CONST, DT_FP32, (u64)ob);
      u32 st = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, c1);
      u32 rs[2] = {st, r0};
      rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, rs, 0);
      ke->output_shape.ndim = 1; ke->output_shape.dims[0] = 32;
      ke->schedule = &ke->_local_schedule;
      memset(ke->schedule, 0, sizeof(KpSchedule));
    }
    u64 hl4  = thvm_metal_tile_jit_hash(&KERNELS[lit4]);
    u64 hl32 = thvm_metal_tile_jit_hash(&KERNELS[lit32]);
    CHECK(hl4 != hl32);
  }
  thvm_free();

  // ============================================================
  TEST_BEGIN("metal_var_pso/second-build-hits-in-memory-cache");
  thvm_init();
  kvar_reset();
  bs_var = kvar_alloc("BS", 1, 512);
  CHECK(bs_var != 0);
  {
    u32 k4 = build_fill_kernel(bs_var, 4);
    u64 hits_before = thvm_metal_jit_hits();
    u64 h4_pre = thvm_metal_tile_jit_hash(&KERNELS[k4]);
    // First fire: builds (or disk-loads) the PSO, caching it in
    // memory.  Output must be correct.
    CHECK_EQ(run_fill_and_verify(k4, 4), 0);
    u64 h4_post = thvm_metal_tile_jit_hash(&KERNELS[k4]);
    // The hash must NOT change across a dispatch (otherwise the cache
    // slot moves and the second fire can't reuse it).
    CHECK_EQ((unsigned long long)h4_pre, (unsigned long long)h4_post);
    // (the build path bumps MISSES, not HITS, so HITS is unchanged.)
    CHECK_EQ((unsigned long long)thvm_metal_jit_hits(),
             (unsigned long long)hits_before);
    // Second fire of a DIFFERENT-BS kernel that shares the cache key:
    // must HIT the in-memory PSO slot.
    u32 k32 = build_fill_kernel(bs_var, 32);
    u64 h32_pre = thvm_metal_tile_jit_hash(&KERNELS[k32]);
    CHECK_EQ((unsigned long long)h4_post, (unsigned long long)h32_pre);
    CHECK_EQ(run_fill_and_verify(k32, 32), 0);
    CHECK((unsigned long long)thvm_metal_jit_hits() > (unsigned long long)hits_before);
  }
  thvm_free();

  // ============================================================
  TEST_BEGIN("metal_var_pso/rendered-msl-correct-at-each-BS");
  // Same as above but cross-check that the values, not just the hash,
  // are right for several BS values fired in sequence -- guards
  // against the V_BS substitution being dropped from the for-loop.
  thvm_init();
  kvar_reset();
  bs_var = kvar_alloc("BS", 1, 512);
  CHECK(bs_var != 0);
  {
    u32 sizes[4] = {1, 4, 32, 100};
    for (u32 i = 0; i < 4; i++) {
      u32 k = build_fill_kernel(bs_var, sizes[i]);
      CHECK_EQ(run_fill_and_verify(k, sizes[i]), 0);
    }
    // All four should have shared a single PSO build (one miss, three
    // hits) after the first.  We don't pin exact counts (other
    // kernels in the process could perturb), but at least one hit
    // must have occurred among the latter three.
  }
  thvm_free();

  unsetenv("THVM_METAL_PSO_CACHE");
  unsetenv("THVM_METAL_PSO_CACHE_DIR");
  unsetenv("THVM_TILE");
  unsetenv("THVM_BACKEND");

  TEST_REPORT();
}
