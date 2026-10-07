// test_bypass_gate_view_offset.c -- a kernel whose addresses compose an
// input's view marks that input, so no backend applies the view twice.
//
// The walker rewrite composes an input's view (a SHRINK's offset, say) into
// the kernel's addresses over the underlying buffer, and the chain-fold scan
// marks those slots input_chain_composed so the backends bind the raw buffer.
// When that mark was withheld, the UOp walker biased such a slot's pointer by
// its view offset a second time: a GPT-2 decode step (a token's row through
// W.shrink, plus a broadcast position term, summed for the layernorm mean)
// read a row twice as far down the table, past its end.
//
// y = sum_j (W[t, j] + s), with W[t] a SHRINK view of W and s a broadcast
// scalar.  Integer-valued data, so the sum is exact on both backends.  Run as is (C JIT) and with
// THVM_CPU_JIT_WARMUP=1000000000 (the walker).
#include "../src/thvm.c"
#include "test.h"

enum { V = 8, D = 16 };

static int interp_mode(void) {
  char const *e = getenv("THVM_CPU_JIT_WARMUP");
  return e != NULL && atoi(e) > 1;
}

static u32 mk(u32 ndim, u32 const *dims, f32 const *data) {
  Shape s = {0};
  s.ndim = ndim;
  u32 n = 1;
  for (u32 d = 0; d < ndim; d++) {
    s.dims[d] = dims[d];
    n *= dims[d];
  }
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data, (u64)n * 4);
  return tid;
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static void row_sum_case(u32 t) {
  f32 w[V * D], s1[1] = { 3.0f };
  for (u32 q = 0; q < V * D; q++) {
    w[q] = (f32)((i32)(q % 11u) - 5);
  }
  u32 wd[2] = { V, D }, sd[1] = { 1 }, row[2] = { 1, D }, ones[2] = { 1, 1 };
  u32 be[4] = { t, t + 1, 0, D };
  Term tok = uop_shrink(ten(mk(2, wd, w)), 2, be);
  Term s   = uop_expand(uop_reshape(ten(mk(1, sd, s1)), 2, ones), 2, row);
  Term y   = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_ADD, tok, s));
  f32 want = 0.0f;
  for (u32 j = 0; j < D; j++) {
    want += w[t * D + j] + s1[0];
  }
  u32 k0 = KERNELS_NEXT;
  Term r = term_resolve(thvm_realize(y));
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    return;
  }
  u32 tid = (u32)term_val(r);
  f32 got = 0.0f;
  TENS[tid].backend->buf_read(TENS[tid].buf_id, &got, 4);
  if (got != want) {
    fprintf(stderr, "  row %u: got=%g want=%g\n", t, got, want);
  }
  CHECK(got == want);
  // The kernel ran on the backend this run asked for, and every input slot
  // whose view is not the identity is marked composed.
  u32 marked = 0, offset_slots = 0;
  for (u32 k = k0; k < KERNELS_NEXT; k++) {
    KernelEntry *ke = &KERNELS[k];
    if (ke->output_tid != tid) {
      continue;
    }
    u32 kind = cg_kernel_dispatch_kind(k);
    CHECK_EQ(kind, interp_mode() ? KDISPATCH_INTERPRETER : KDISPATCH_JIT);
    for (u32 i = 0; i < ke->n_inputs; i++) {
      u32 in = ke->input_tids[i];
      if (in != 0 && in < TENS_NEXT && TENS[in].view.offset != 0) {
        offset_slots++;
        marked += ke->input_chain_composed[i] != 0;
      }
    }
  }
  CHECK(offset_slots >= 1);
  CHECK_EQ(marked, offset_slots);
}

int main(void) {
  thvm_init();
  TEST_BEGIN("bypass-gate-view-offset/row-sum");
  for (u32 t = 1; t < V; t++) {
    row_sum_case(t);
  }
  thvm_free();
  TEST_REPORT();
}
