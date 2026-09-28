// test_autotune_iso_scratch.c - the BEAM isolated bench's scratch buffers
// hold every element the kernel can read.
//
// kautotune_iso_begin benches a kernel on fresh scratch buffers instead of
// the live graph buffers.  An input slot flagged input_chain_composed binds
// the raw buffer under its view: the kernel's loads compose the movement
// chain into the address, so a shrunk view is read up to the extent its
// base spans, past the view's own element count.  tinygrad sizes each beam
// scratch to its PARAM's whole buffer (schedule/rangeify.py:284,
// codegen/opt/postrange.py:256-258).  A view-sized scratch made BEAM=2 read
// past it on x.shrink(...) * 2 and on x.shrink(...).reshape(...).sum(1).
//
// For each kernel the test checks that the shrunk slot is chain-composed,
// that its scratch holds the base extent, that the isolated bench computes
// the kernel from the scratch contents, and that a BEAM=2 search leaves the
// live result correct.

#include "../src/thvm.c"
#include "test.h"

static Term leaf_f32(u32 ndim, u32 const *dims) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  u32 n   = TENS[tid].view.numel;
  f32 *v  = (f32 *)malloc((size_t)n * sizeof(f32));
  for (u32 i = 0; i < n; i++) {
    v[i] = (f32)(i % 23) - 11.0f;
  }
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, v, (u64)n * sizeof(f32));
  free(v);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static Term full_f32(u32 bits, u32 ndim, u32 const *dims) {
  u32 ones[MAX_DIM];
  for (u32 i = 0; i < ndim; i++) {
    ones[i] = 1;
  }
  return uop_expand(uop_reshape(uop_const(DT_FP32, bits), ndim, ones),
                    ndim, dims);
}

// The realize detaches producer_kid from a committed output, so find the
// kernel by the tensor it wrote.
static u32 kernel_writing(Term realized) {
  Term r = term_resolve(realized);
  if (term_tag(r) != TAG_TEN) {
    return 0;
  }
  u32 tid = (u32)term_val(r);
  for (u32 kid = KERNELS_NEXT; kid-- > 1;) {
    if (KERNELS[kid].output_tid == tid) {
      return kid;
    }
  }
  return 0;
}

static u32 composed_slot(KernelEntry const *ke) {
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_chain_composed[i]) {
      return i;
    }
  }
  return ke->n_inputs;
}

static u64 cpu_buf_bytes(u32 buf) {
  return (buf != 0 && buf < CPU_BUFS_NEXT) ? CPU_BUFS[buf].nbytes : 0;
}

// out[r * 64 + c] = 2 * in[r * 128 + 64 + c]
static u32 shrink_mul_mismatches(f32 const *in, f32 const *out) {
  u32 bad = 0;
  for (u32 r = 0; r < 2; r++) {
    for (u32 c = 0; c < 64; c++) {
      if (out[r * 64 + c] != 2.0f * in[r * 128 + 64 + c]) {
        bad++;
      }
    }
  }
  return bad;
}

// out[r] = sum of in[r * 32 + j * 8 + 4 + k] over j, k < 4
static u32 merge_sum_mismatches(f32 const *in, f32 const *out) {
  u32 bad = 0;
  for (u32 r = 0; r < 2; r++) {
    f64 want = 0.0;
    for (u32 j = 0; j < 4; j++) {
      for (u32 k = 0; k < 4; k++) {
        want += (f64)in[r * 32 + j * 8 + 4 + k];
      }
    }
    if (fabs((f64)out[r] - want) > 1e-4 * (1.0 + fabs(want))) {
      bad++;
    }
  }
  return bad;
}

typedef u32 (*MismatchFn)(f32 const *in, f32 const *out);

// Bench `kid` on the isolated scratch.  The shrunk slot's view has `nviews`
// prior views, and `base_n` is the element count of its base buffer, which
// the kernel reads up to its last element.
static void check_iso_scratch(char const *name, u32 kid, u32 nviews,
                              u32 base_n, MismatchFn mismatches) {
  char         label[128];
  KernelEntry *ke   = &KERNELS[kid];
  u32          slot = composed_slot(ke);

  snprintf(label, sizeof(label), "autotune-iso/%s/slot-chain-composed", name);
  TEST_BEGIN(label);
  CHECK(slot < ke->n_inputs);
  if (slot < ke->n_inputs) {
    CHECK_EQ(TENS[ke->input_tids[slot]].nviews, nviews);
  }

  snprintf(label, sizeof(label), "autotune-iso/%s/bench-isolates", name);
  TEST_BEGIN(label);
  int iso = kautotune_iso_begin(kid);
  CHECK_EQ(iso, 1);
  if (!iso || slot >= ke->n_inputs) {
    return;
  }

  snprintf(label, sizeof(label), "autotune-iso/%s/scratch-holds-base", name);
  TEST_BEGIN(label);
  u64 have = cpu_buf_bytes(ISO.in_buf_ids[slot]);
  CHECK(have >= (u64)base_n * sizeof(f32));

  // Fire only on a scratch that covers the kernel's reads: an undersized
  // one would make the test itself read past it.
  snprintf(label, sizeof(label), "autotune-iso/%s/bench-reads-scratch", name);
  TEST_BEGIN(label);
  if (have >= (u64)base_n * sizeof(f32)) {
    kernel_bench_fire(kid);
    KautotuneOutStat st = kautotune_output_stat(ke);
    CHECK(st.valid && st.all_finite);
    f32 *in  = (f32 *)malloc((size_t)base_n * sizeof(f32));
    f32 *out = (f32 *)malloc((size_t)ke->output_numel * sizeof(f32));
    ISO.backend->buf_read(ISO.in_buf_ids[slot], in, (u64)base_n * sizeof(f32));
    ISO.backend->buf_read(ISO.out_buf_id, out,
                          (u64)ke->output_numel * sizeof(f32));
    CHECK_EQ(mismatches(in, out), 0);
    free(in);
    free(out);
  }
  kautotune_iso_end();
}

// A BEAM=2 search over `kid`, then a live fire: the result must still be the
// kernel's value over the live base buffer `x`.
static void check_beam_live(char const *name, u32 kid, Term x, u32 base_n,
                            MismatchFn mismatches) {
  char         label[128];
  KernelEntry *ke = &KERNELS[kid];
  snprintf(label, sizeof(label), "autotune-iso/%s/beam2-live-result", name);
  TEST_BEGIN(label);
  setenv("BEAM", "2", 1);
  setenv("AUTOTUNE_CACHE", "0", 1);
  ke->schedule->autotuned = 0;
  kernel_autotune(kid);
  unsetenv("BEAM");
  kernel_bench_fire(kid);
  u32 xt = (u32)term_val(x);
  f32 *in  = (f32 *)malloc((size_t)base_n * sizeof(f32));
  f32 *out = (f32 *)malloc((size_t)ke->output_numel * sizeof(f32));
  TENS[xt].backend->buf_read(TENS[xt].buf_id, in, (u64)base_n * sizeof(f32));
  TENS[ke->output_tid].backend->buf_read(TENS[ke->output_tid].buf_id, out,
                                         (u64)ke->output_numel * sizeof(f32));
  CHECK_EQ(mismatches(in, out), 0);
  free(in);
  free(out);
}

int main(void) {
  unsetenv("BEAM");
  unsetenv("JITBEAM");
  unsetenv("AUTOTUNE");
  thvm_init();

  // x.shrink(((0,2),(64,128))) * 2 over x of shape (2,128): the view is
  // (2,64) at offset 64 with row stride 128, so the kernel reads x up to
  // element 1*128 + 64 + 63 = 255.
  u32  xd[2] = {2, 128};
  u32  be[4] = {0, 2, 64, 128};
  u32  yd[2] = {2, 64};
  Term x     = leaf_f32(2, xd);
  Term two   = full_f32(0x40000000u, 2, yd);
  Term y     = thvm_realize(uop_binary(UOP_MUL, uop_shrink(x, 2, be), two));
  u32  kmul  = kernel_writing(y);
  TEST_BEGIN("autotune-iso/shrink-mul/kernel");
  CHECK(kmul != 0 && kmul < KERNELS_NEXT);
  if (kmul != 0 && kmul < KERNELS_NEXT) {
    check_iso_scratch("shrink-mul", kmul, 0, 256, shrink_mul_mismatches);
    check_beam_live("shrink-mul", kmul, x, 256, shrink_mul_mismatches);
  }

  // x.shrink(((0,2),(0,4),(4,8))).reshape(2,16).sum(1) over x of shape
  // (2,4,8): the reshape merges the shrunk axis with its outer neighbour,
  // a two-view chain whose address reaches 32 + 3*8 + 4 + 3 = 63.
  u32  zd[3]  = {2, 4, 8};
  u32  zbe[6] = {0, 2, 0, 4, 4, 8};
  u32  md[2]  = {2, 16};
  Term z      = leaf_f32(3, zd);
  Term merged = uop_reshape(uop_shrink(z, 3, zbe), 2, md);
  Term s      = thvm_realize(uop_reduce(REDUCE_SUM, 1, merged));
  u32  ksum   = kernel_writing(s);
  TEST_BEGIN("autotune-iso/merge-sum/kernel");
  CHECK(ksum != 0 && ksum < KERNELS_NEXT);
  if (ksum != 0 && ksum < KERNELS_NEXT) {
    check_iso_scratch("merge-sum", ksum, 1, 64, merge_sum_mismatches);
    check_beam_live("merge-sum", ksum, z, 64, merge_sum_mismatches);
  }

  thvm_free();
  TEST_REPORT();
}
