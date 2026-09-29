// test_kernel_width.c - CPU kernels with more than 64 inputs.
//
// tinygrad's CPU has no per-kernel buffer cap: DEVICE_MAX_BUFS lists only
// WEBGPU (schedule/rangeify.py:170).  A left-deep sum of N distinct leaves
// is one elementwise kernel with N inputs, and it must compute its result
// however wide it is: through the CPU JIT on the default path, and through
// the UOp walker when the JIT does not run.  A kernel whose dispatch ran no
// rung bumps cpu_dispatch_no_rung_count, which must stay 0.
//
// The unified rewrite can add an input slot after visit() declared the
// kernel's inputs (unified_rewrite_buffer_for_tid_extend).  Such a slot
// past the 64th must reach the lift's input table, or the walker reads
// the leaf as 0.0.  A shrunk leaf read at a slot past the 64th must keep
// its input_chain_composed flag, or the walker applies the view offset a
// second time.

#include "../src/thvm.c"
#include "test.h"

#define KW_ELEMS 16

// Leaf i holds (i + 1) + 1024 * j at element j: every leaf differs, and the
// sum of up to 300 leaves stays an integer below 2^24, so f32 is exact.
static Term kw_leaf(u32 i) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = KW_ELEMS;
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  f32 data[KW_ELEMS];
  for (u32 j = 0; j < KW_ELEMS; j++) {
    data[j] = (f32)(i + 1) + 1024.0f * (f32)j;
  }
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, data, sizeof(data));
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static int kw_exact(u32 buf_id, u32 n) {
  f32 got[KW_ELEMS];
  CURRENT_BACKEND->buf_read(buf_id, got, sizeof(got));
  f32 leaf_sum = (f32)(n * (n + 1) / 2);
  for (u32 j = 0; j < KW_ELEMS; j++) {
    if (got[j] != leaf_sum + 1024.0f * (f32)n * (f32)j) {
      return 0;
    }
  }
  return 1;
}

typedef struct {
  u32 kernels;
  u32 widest;       // kid of the kernel with the most inputs
  u32 max_inputs;
  u32 out_buf;
  int exact;
} KwRun;

static KwRun kw_run_sum(u32 n) {
  KwRun r = {0, 0, 0, 0, 0};
  Term acc = kw_leaf(0);
  for (u32 i = 1; i < n; i++) {
    acc = uop_binary(UOP_ADD, acc, kw_leaf(i));
  }
  u32  before = KERNELS_NEXT;
  Term out    = wnf(thvm_materialize(acc));
  r.kernels = KERNELS_NEXT - before;
  for (u32 k = before; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].n_inputs > r.max_inputs) {
      r.max_inputs = KERNELS[k].n_inputs;
      r.widest     = k;
    }
  }
  if (term_tag(out) != TAG_TEN) {
    return r;
  }
  r.out_buf = TENS[(u32)term_val(out)].buf_id;
  r.exact   = kw_exact(r.out_buf, n);
  return r;
}

// Run kernel `kid` through the UOp walker alone, into a zeroed output,
// and read the result back into `got`.
static int kw_walk(u32 kid, u32 out_buf, f32 *got) {
  KernelEntry *ke = &KERNELS[kid];
  f32 zeros[KW_ELEMS] = {0};
  CURRENT_BACKEND->buf_write(out_buf, zeros, sizeof(zeros));
  u32 *ids = (u32 *)calloc(ke->n_inputs, sizeof(u32));
  for (u32 i = 0; i < ke->n_inputs; i++) {
    ids[i] = TENS[ke->input_tids[i]].buf_id;
  }
  int ran = cpu_uop_walk(ke, ids, out_buf);
  free(ids);
  CURRENT_BACKEND->buf_read(out_buf, got, KW_ELEMS * sizeof(f32));
  return ran;
}

static int kw_walk_exact(u32 kid, u32 out_buf, u32 n) {
  f32 got[KW_ELEMS];
  return kw_walk(kid, out_buf, got) && kw_exact(out_buf, n);
}

// Sum of n - 1 leaves plus, last, base[KW_ELEMS + j] read through a
// SHRINK of a 2*KW_ELEMS leaf holding 3 * k at element k.  The shrunk
// view has offset KW_ELEMS and takes the kernel's last input slot.
static f32 kw_shrink_want(u32 n, u32 j) {
  return (f32)((n - 1) * n / 2) + 1024.0f * (f32)(n - 1) * (f32)j
       + 3.0f * (f32)(KW_ELEMS + j);
}

static KwRun kw_run_sum_with_shrink(u32 n) {
  KwRun r = {0, 0, 0, 0, 0};
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = 2 * KW_ELEMS;
  u32 base_tid = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  f32 base[2 * KW_ELEMS];
  for (u32 k = 0; k < 2 * KW_ELEMS; k++) {
    base[k] = 3.0f * (f32)k;
  }
  CURRENT_BACKEND->buf_write(TENS[base_tid].buf_id, base, sizeof(base));
  u32  be[2]  = { KW_ELEMS, 2 * KW_ELEMS };
  Term shrunk = uop_shrink(term_new(0, TAG_TEN, DT_FP32, base_tid), 1, be);
  Term acc    = kw_leaf(0);
  for (u32 i = 1; i + 1 < n; i++) {
    acc = uop_binary(UOP_ADD, acc, kw_leaf(i));
  }
  acc = uop_binary(UOP_ADD, acc, shrunk);
  u32  before = KERNELS_NEXT;
  Term out    = wnf(thvm_materialize(acc));
  r.kernels = KERNELS_NEXT - before;
  for (u32 k = before; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].n_inputs > r.max_inputs) {
      r.max_inputs = KERNELS[k].n_inputs;
      r.widest     = k;
    }
  }
  if (term_tag(out) != TAG_TEN) {
    return r;
  }
  r.out_buf = TENS[(u32)term_val(out)].buf_id;
  f32 got[KW_ELEMS];
  r.exact = kw_walk(r.widest, r.out_buf, got);
  for (u32 j = 0; j < KW_ELEMS; j++) {
    if (got[j] != kw_shrink_want(n, j)) {
      r.exact = 0;
    }
  }
  return r;
}

int main(void) {
  thvm_init();

  static char names[5][64];
  u32 widths[5] = { 63, 64, 65, 70, 130 };
  KwRun wide = {0, 0, 0, 0, 0};
  for (u32 w = 0; w < 5; w++) {
    snprintf(names[w], sizeof(names[w]), "kernel-width/sum-of-%u-leaves",
             widths[w]);
    TEST_BEGIN(names[w]);
    KwRun r = kw_run_sum(widths[w]);
    CHECK_EQ(r.kernels, 1);
    CHECK_EQ(r.max_inputs, widths[w]);
    CHECK(r.exact);
    wide = r;
  }

  TEST_BEGIN("kernel-width/walker-runs-130-inputs");
  CHECK(kw_walk_exact(wide.widest, wide.out_buf, 130));

  TEST_BEGIN("kernel-width/walker-shrunk-input-at-slot-69");
  {
    KwRun r = kw_run_sum_with_shrink(70);
    CHECK_EQ(r.kernels, 1);
    CHECK_EQ(r.max_inputs, 70);
    CHECK(KERNELS[r.widest].input_chain_composed[69]);
    CHECK(r.exact);
  }

  TEST_BEGIN("kernel-width/extend-past-64-declared-inputs");
  {
    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];
    for (u32 i = 0; i < 64; i++) {
      Term leaf = kw_leaf(1000 + i);
      input_slot_dedup(ke, (u32)term_val(leaf), leaf);
    }
    ke->cached_lift.n_inputs = ke->n_inputs;
    Term extra = kw_leaf(2000);
    Term buf   = unified_rewrite_buffer_for_tid_extend(ke, (u32)term_val(extra));
    CHECK(buf != 0);
    CHECK_EQ(uop_buffer_inst_get(buf), 65);
    CHECK_EQ(ke->n_inputs, 65);
    CHECK_EQ(ke->cached_lift.n_inputs, 65);
    CHECK(ke->cached_lift.in_bufs[64] == buf);
  }

  TEST_BEGIN("kernel-width/no-kernel-ran-without-a-rung");
  CHECK_EQ(cpu_dispatch_no_rung_count(), 0);

  thvm_free();
  TEST_REPORT();
}
