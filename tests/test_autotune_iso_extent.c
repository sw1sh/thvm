// test_autotune_iso_extent.c -- the isolated BEAM search gives a composed
// input a scratch buffer that holds every address its kernel reads.
//
// BEAM, AUTOTUNE and JITBEAM time each candidate on fresh scratch buffers,
// one per input slot (codegen/autotune.c kautotune_iso_begin), as tinygrad
// times beam candidates on buffers sized to each PARAM, the whole buffer
// under the kernel's views (codegen/opt/postrange.py:256-258 args_from_ast,
// schedule/rangeify.py:284 debuf).  A slot marked input_chain_composed is
// bound as its raw buffer and read at addresses that compose its view chain
// over that buffer: row t of W through a SHRINK reads W[t*D .. t*D+D-1].  A
// scratch sized to the view's numel was read past its end, a heap overflow
// that faults once the offset is large; kautotune_iso_begin now gives such a
// slot its whole buffer.
//
// The scratch of every input slot of a row-sum kernel and of a
// permute-reshape chain kernel is checked against the largest index
// tendesc_strided_index reaches, and BEAM=2 realizes rows at the end of a
// 200000-row table.  Run as is (C JIT) and with
// THVM_CPU_JIT_WARMUP=1000000000 (the UOp walker).
#include "../src/thvm.c"
#include "test.h"

enum { V = 64, D = 16, R = 7, BIG_V = 200000 };

static u64 reach_of(TenDesc const *t) {
  u64 hi = 0;
  for (u32 k = 0; k < t->view.numel; k++) {
    u64 x = (u64)tendesc_strided_index(t, k) + 1;
    if (x > hi) {
      hi = x;
    }
  }
  return hi;
}

static u32 mk(u32 ndim, u32 const *dims, f32 const *data) {
  Shape s = {0};
  s.ndim = ndim;
  u64 n = 1;
  for (u32 d = 0; d < ndim; d++) {
    s.dims[d] = dims[d];
    n *= dims[d];
  }
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data, n * 4);
  return tid;
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static Term bcast_scalar(f32 const *s, u32 ndim, u32 const *dims) {
  u32 sd[1] = { 1 }, ones[MAX_DIM] = { 1, 1, 1, 1, 1, 1, 1, 1 };
  return uop_expand(uop_reshape(ten(mk(1, sd, s)), ndim, ones), ndim, dims);
}

static u32 realized_tid(Term y) {
  Term r = term_resolve(thvm_realize(y));
  CHECK_EQ(term_tag(r), TAG_TEN);
  return term_tag(r) == TAG_TEN ? (u32)term_val(r) : 0;
}

// Every input slot's scratch holds what the kernel reads there: a composed
// slot's reach over its storage, any other slot's numel.  Counts the
// composed slots whose reach passes their numel.
static u32 check_iso_sizes(char const *name, u32 kid) {
  KernelEntry *ke = &KERNELS[kid];
  int ok = kautotune_iso_begin(kid);
  CHECK(ok);
  if (!ok) {
    return 0;
  }
  u32 past = 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 in   = ke->input_tids[i];
    u64 need = ke->input_numels[i];
    if (ke->input_chain_composed[i] && in != 0 && in < TENS_NEXT) {
      u64 reach = reach_of(&TENS[in]);
      if (reach > need) {
        need = reach;
        past++;
      }
    }
    u64 have = CPU_BUFS[ISO.in_buf_ids[i]].nbytes;
    u64 want = dtype_storage_bytes(ke->input_dtypes[i], need);
    if (have < want) {
      fprintf(stderr, "  %s: slot %u scratch %llu bytes, reads reach %llu\n",
              name, i, (unsigned long long)have, (unsigned long long)want);
    }
    CHECK(have >= want);
  }
  kautotune_iso_end();
  return past;
}

static u32 kernel_of(u32 k0, u32 out_tid) {
  for (u32 k = k0; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].output_tid == out_tid) {
      return k;
    }
  }
  return 0;
}

// y = sum_j (W[t, j] + s): W[t] a SHRINK view, s a broadcast scalar.
static void row_sum_case(u32 w_tid, f32 const *w, u32 t, int beam) {
  f32 s1[1] = { 3.0f };
  u32 row[2] = { 1, D }, be[4] = { t, t + 1, 0, D };
  Term tok = uop_shrink(ten(w_tid), 2, be);
  Term y   = uop_reduce(REDUCE_SUM, 1,
                        uop_binary(UOP_ADD, tok, bcast_scalar(s1, 2, row)));
  f32 want = 0.0f;
  for (u32 j = 0; j < D; j++) {
    want += w[(u64)t * D + j] + s1[0];
  }
  u32 k0  = KERNELS_NEXT;
  u32 tid = realized_tid(y);
  if (tid == 0) {
    return;
  }
  f32 got = 0.0f;
  TENS[tid].backend->buf_read(TENS[tid].buf_id, &got, 4);
  if (got != want) {
    fprintf(stderr, "  row %u: got=%g want=%g\n", t, got, want);
  }
  CHECK(got == want);
  u32 kid = kernel_of(k0, tid);
  CHECK(kid != 0);
  if (kid == 0) {
    return;
  }
  if (beam) {
    KernelEntry const *ke = &KERNELS[kid];
    CHECK(ke->schedule != NULL && ke->schedule->autotuned);
  }
  CHECK(check_iso_sizes("row-sum", kid) >= 1);
}

// y[k] = W[a + k % R, k / R] + s: rows a .. a+R-1 of W, permuted, then
// reshaped flat (a reshape of a strided view extends the view chain).
static void permuted_block_case(u32 w_tid, f32 const *w, u32 a) {
  f32 s1[1] = { 0.5f };
  u32 be[4] = { a, a + R, 0, D }, perm[2] = { 1, 0 }, flat[1] = { D * R };
  Term x = uop_reshape(uop_permute(uop_shrink(ten(w_tid), 2, be), 2, perm),
                       1, flat);
  Term y = uop_binary(UOP_ADD, x, bcast_scalar(s1, 1, flat));
  u32 k0  = KERNELS_NEXT;
  u32 tid = realized_tid(y);
  if (tid == 0) {
    return;
  }
  f32 got[D * R];
  TENS[tid].backend->buf_read(TENS[tid].buf_id, got, sizeof(got));
  u32 bad = 0;
  for (u32 k = 0; k < D * R; k++) {
    f32 want = w[(a + k % R) * D + k / R] + s1[0];
    if (got[k] != want && bad++ < 4) {
      fprintf(stderr, "  block k=%u: got=%g want=%g\n", k, got[k], want);
    }
  }
  CHECK_EQ(bad, 0u);
  u32 kid = kernel_of(k0, tid);
  CHECK(kid != 0);
  if (kid != 0) {
    CHECK(check_iso_sizes("permuted-block", kid) >= 1);
  }
}

int main(void) {
  thvm_init();

  f32 w[V * D];
  for (u32 q = 0; q < V * D; q++) {
    w[q] = (f32)((i32)(q % 11u) - 5);
  }
  u32 vd[2] = { V, D };
  u32 w_tid = mk(2, vd, w);
  TEST_BEGIN("iso-extent/row-sum");
  for (u32 t = 1; t < V; t += 9) {
    row_sum_case(w_tid, w, t, 0);
  }
  TEST_BEGIN("iso-extent/permuted-block");
  for (u32 a = 1; a + R <= V; a += 19) {
    permuted_block_case(w_tid, w, a);
  }

  // BEAM=2 searches each row-sum kernel on its scratch buffers.  A scratch
  // sized to the row (64 bytes) while the kernel reads ~12.8 MB into it
  // faults here.
  TEST_BEGIN("iso-extent/beam-rows-at-table-end");
  setenv("BEAM", "2", 1);
  setenv("AUTOTUNE_CACHE", "0", 1);
  f32 *big = (f32 *)malloc((size_t)BIG_V * D * 4);
  for (u64 q = 0; q < (u64)BIG_V * D; q++) {
    big[q] = (f32)((i32)(q % 11u) - 5);
  }
  u32 bd[2] = { BIG_V, D };
  u32 big_tid = mk(2, bd, big);
  for (u32 t = BIG_V - 3; t < BIG_V; t++) {
    row_sum_case(big_tid, big, t, 1);
  }
  unsetenv("BEAM");
  free(big);
  thvm_free();
  TEST_REPORT();
}
