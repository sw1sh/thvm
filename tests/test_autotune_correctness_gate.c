// test_autotune_correctness_gate.c -- BEAM autotune NaN/parity gate.
//
// The BEAM kernel-opt search (codegen/autotune.c) selects tile/opt
// variants by SPEED only.  Without a correctness check a fast-but-wrong
// variant (NaN/Inf output, or a numerically divergent reordering) could
// win the search and be cached.  These tests pin the gate that rejects
// any candidate whose output diverges from the no-opt baseline:
//
//   (1) kautotune_output_stat reads the output buffer to host and
//       computes a finite, hand-checkable {all_finite, sum_abs} stat.
//   (2) kautotune_stat_accept rejects NaN/Inf and gross divergence
//       while accepting legitimate (within-tol) reorder noise.
//   (3) kautotune_bench_seq returns 0 (variant skipped) when a fired
//       candidate's output is NaN, even though it never times it.
//   (4) end-to-end kernel_autotune on a real CPU elementwise kernel
//       still produces a correct (finite, baseline-matching) output.
//   (5) the dtype-keyed tolerance is loose for bf16, tight for f32.
//   (6) KAUTOTUNE_CACHE_VERSION was bumped past the gate-less version.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

// Fill a TAG_TEN tensor's backing buffer with f32 values v[0..n).
static void fill_f32(Term t, f32 const *v, u32 n) {
  u32 tid   = (u32)term_val(t);
  u32 buf   = TENS[tid].buf_id;
  Backend *b = TENS[tid].backend;
  b->buf_write(buf, v, (u64)n * sizeof(f32));
}

int main(void) {
  thvm_init();

  // === (0) Cache version bumped past the gate-less version (2) =======
  TEST_BEGIN("autotune-gate/cache-version-bumped");
  CHECK(KAUTOTUNE_CACHE_VERSION >= 3);

  // === (1) baseline stat is finite + matches a hand-computed sum =====
  // Build c = a + b for a small 1D kernel with known inputs, fire it,
  // and read the output stat.  sum_abs must equal sum(|a_i + b_i|).
  TEST_BEGIN("autotune-gate/baseline-stat-finite-and-exact");
  u32 dims[1] = {16};
  Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32(dims, 1));
  Term b = term_new(0, TAG_TEN, DT_FP32, alloc_f32(dims, 1));
  f32 av[16], bv[16];
  double expect_sum_abs = 0.0;
  for (u32 i = 0; i < 16; i++) {
    av[i] = (f32)(i + 1);          // 1..16
    bv[i] = (f32)(i % 3) - 1.0f;   // -1,0,1 cycling
    expect_sum_abs += fabs((double)av[i] + (double)bv[i]);
  }
  fill_f32(a, av, 16);
  fill_f32(b, bv, 16);

  Term add  = uop_binary(UOP_ADD, a, b);
  thvm_materialize(add);
  u32 kadd = KERNELS_NEXT - 1;
  kernel_compute_consumer_counts();
  KernelEntry *Kadd = &KERNELS[kadd];

  axes_reset_to_default(Kadd);
  kernel_bench_fire(kadd);
  KautotuneOutStat base = kautotune_output_stat(Kadd);
  CHECK(base.valid);
  CHECK(base.all_finite);
  CHECK(fabs(base.sum_abs - expect_sum_abs) < 1e-4);

  // === (2) stat_accept: NaN candidate rejected, close one accepted ===
  TEST_BEGIN("autotune-gate/stat-accept-rejects-nan");
  KautotuneOutStat nan_cand = { .valid = 1, .all_finite = 0,
                                .sum_abs = base.sum_abs, .wsum = base.wsum };
  CHECK_EQ(kautotune_stat_accept(&base, &nan_cand,
                                 kautotune_stat_tol(DT_FP32)), 0);

  TEST_BEGIN("autotune-gate/stat-accept-rejects-gross-divergence");
  // A finite candidate whose magnitude doubled -- a real miscompile.
  KautotuneOutStat far_cand = { .valid = 1, .all_finite = 1,
                                .sum_abs = base.sum_abs * 2.0 + 1.0,
                                .wsum = base.wsum * 2.0 + 1.0 };
  CHECK_EQ(kautotune_stat_accept(&base, &far_cand,
                                 kautotune_stat_tol(DT_FP32)), 0);

  TEST_BEGIN("autotune-gate/stat-accept-allows-within-tol");
  // f32 reorder noise: a tiny relative perturbation must pass (both the
  // magnitude budget AND the position-weighted sum within tol).
  KautotuneOutStat near_cand = { .valid = 1, .all_finite = 1,
                                 .sum_abs = base.sum_abs * (1.0 + 1e-5),
                                 .wsum    = base.wsum * (1.0 + 1e-5) };
  CHECK_EQ(kautotune_stat_accept(&base, &near_cand,
                                 kautotune_stat_tol(DT_FP32)), 1);

  TEST_BEGIN("autotune-gate/stat-accept-rejects-position-scramble");
  // A tile that PRESERVES the magnitude budget (sum_abs matches) but scrambles
  // element positions diverges in the position-weighted sum -> rejected.  This
  // is the DiT-matmul mis-tile that NaN'd the warm FLUX replay yet slipped a
  // pure sum_abs gate on synthetic scratch.
  KautotuneOutStat scramble_cand = { .valid = 1, .all_finite = 1,
                                     .sum_abs = base.sum_abs,
                                     .wsum    = base.wsum * 1.5 + 1.0 };
  CHECK_EQ(kautotune_stat_accept(&base, &scramble_cand,
                                 kautotune_stat_tol(DT_FP32)), 0);

  TEST_BEGIN("autotune-gate/stat-accept-unmeasured-candidate-passes");
  // valid=0 (gate could not read this backend) -> never block.
  KautotuneOutStat unmeasured = { .valid = 0, .all_finite = 1, .sum_abs = 0 };
  CHECK_EQ(kautotune_stat_accept(&base, &unmeasured,
                                 kautotune_stat_tol(DT_FP32)), 1);

  TEST_BEGIN("autotune-gate/stat-accept-no-baseline-finiteness-only");
  // No trustworthy baseline -> finite candidate passes, NaN still fails.
  KautotuneOutStat no_base = { .valid = 0, .all_finite = 1, .sum_abs = 0 };
  CHECK_EQ(kautotune_stat_accept(&no_base, &far_cand,
                                 kautotune_stat_tol(DT_FP32)), 1);
  CHECK_EQ(kautotune_stat_accept(&no_base, &nan_cand,
                                 kautotune_stat_tol(DT_FP32)), 0);

  // === (3) bench_seq rejects a candidate whose fired output is NaN ===
  // Inject a NaN into the output buffer *after* applying the variant but
  // we cannot easily hook between fire and stat; instead, simulate the
  // rejection path directly: corrupt the output to NaN, build a stat,
  // and confirm bench_seq's gate (stat_accept) would reject it.  This
  // pins that a non-finite fired output yields rejection (return 0),
  // which is what the in-loop `if (!kautotune_bench_seq(...)) continue;`
  // relies on.
  TEST_BEGIN("autotune-gate/nan-output-fails-the-gate");
  {
    u32 otid = Kadd->output_tid;
    u32 obuf = TENS[otid].buf_id;
    Backend *ob = TENS[otid].backend;
    // Overwrite output[0] with NaN.
    f32 host[16];
    ob->buf_read(obuf, host, sizeof(host));
    host[0] = (f32)NAN;
    ob->buf_write(obuf, host, sizeof(host));
    KautotuneOutStat corrupt = kautotune_output_stat(Kadd);
    CHECK(corrupt.valid);
    CHECK_EQ(corrupt.all_finite, 0);
    CHECK_EQ(kautotune_stat_accept(&base, &corrupt,
                                   kautotune_stat_tol(DT_FP32)), 0);
    // Restore so the kernel buffer is clean for the e2e test below.
    kernel_bench_fire(kadd);
  }

  // === (3b) bench_seq itself rejects vs a divergent baseline =========
  // Drive kautotune_bench_seq end-to-end with the no-opt sequence.  With
  // the TRUE baseline the candidate output matches -> accept (return 1).
  // With a deliberately-wrong baseline (half the magnitude) the same
  // correct fired output now "diverges" -> the gate rejects (return 0).
  // This pins that the rejection is wired through bench_seq's own fire +
  // stat path, not just the helper.
  TEST_BEGIN("autotune-gate/bench-seq-accepts-against-true-baseline");
  {
    KOptSeq noop = {0};  // n == 0 -> baseline sequence
    u64 us = 0;
    int ok = kautotune_bench_seq(kadd, Kadd, &noop, 1, &base, 0, &us);
    CHECK_EQ(ok, 1);
  }

  TEST_BEGIN("autotune-gate/bench-seq-rejects-against-wrong-baseline");
  {
    KautotuneOutStat wrong = { .valid = 1, .all_finite = 1,
                               .sum_abs = base.sum_abs * 0.5 };
    KOptSeq noop = {0};
    u64 us = 0;
    int ok = kautotune_bench_seq(kadd, Kadd, &noop, 1, &wrong, 0, &us);
    CHECK_EQ(ok, 0);
  }

  // === (4) end-to-end: kernel_autotune leaves a CORRECT output =======
  // With BEAM on, the CPU UPCAST proposer fires candidates.  The gate
  // must let the search proceed (correct variants pass) and the final
  // applied kernel must still compute a + b.  We verify the post-tune
  // output stat still matches the baseline.
  TEST_BEGIN("autotune-gate/e2e-autotune-keeps-correct-output");
  setenv("BEAM", "2", 1);
  setenv("AUTOTUNE_DISABLE", "0", 1);
  setenv("AUTOTUNE_CACHE", "0", 1);   // force a fresh search, no disk cache
  // Re-fire baseline to refresh the output, capture the reference sum.
  axes_reset_to_default(Kadd);
  kernel_bench_fire(kadd);
  KautotuneOutStat ref = kautotune_output_stat(Kadd);
  CHECK(ref.valid && ref.all_finite);

  Kadd->schedule->autotuned = 0;       // allow the search to run
  int n_cand = kernel_opts_propose(Kadd, (KOpt[16]){0}, 16);
  // CPU proposer should offer at least one UPCAST candidate on a 16-wide
  // splittable axis; if not, the e2e tune is a documented no-op.
  kernel_autotune(kadd);
  kernel_bench_fire(kadd);             // fire the applied (winning) variant
  KautotuneOutStat tuned = kautotune_output_stat(Kadd);
  CHECK(tuned.valid);
  CHECK(tuned.all_finite);
  CHECK(fabs(tuned.sum_abs - ref.sum_abs)
        <= kautotune_stat_tol(DT_FP32) * (fabs(ref.sum_abs) + 1e-9));
  (void)n_cand;
  unsetenv("BEAM");
  unsetenv("AUTOTUNE_DISABLE");
  unsetenv("AUTOTUNE_CACHE");

  // === (5) dtype-keyed tolerance: bf16 loose, f32 tight =============
  TEST_BEGIN("autotune-gate/tol-keyed-by-dtype");
  CHECK(kautotune_stat_tol(DT_BF16) > kautotune_stat_tol(DT_FP32));
  CHECK(kautotune_stat_tol(DT_FP16) > kautotune_stat_tol(DT_FP32));
  // A bf16-scale reorder (2% drift) that f32 would reject, bf16 accepts.
  KautotuneOutStat bf_base = { .valid = 1, .all_finite = 1, .sum_abs = 100.0 };
  KautotuneOutStat bf_cand = { .valid = 1, .all_finite = 1, .sum_abs = 102.0 };
  CHECK_EQ(kautotune_stat_accept(&bf_base, &bf_cand,
                                 kautotune_stat_tol(DT_BF16)), 1);
  CHECK_EQ(kautotune_stat_accept(&bf_base, &bf_cand,
                                 kautotune_stat_tol(DT_FP32)), 0);

  thvm_free();
  TEST_REPORT();
}
