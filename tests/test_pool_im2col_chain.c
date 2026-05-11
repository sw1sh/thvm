// test_pool_im2col_chain.c -- repro for the conv `_pool` im2col view
// chain over a non-trivial-cIn input.  Builds the same movement-op
// chain TConv2DIm2ColBatchedPool uses (Reshape->Expand->Reshape->
// Shrink->Reshape->Shrink->Permute->Reshape), resolves it, and
// checks tendesc_strided_index against the textbook im2col formula.
//
// Background: at BS=512 a real beautiful_mnist forward refuses a
// 3.8 GB buffer (947912704 = 512^2 * 32 * 113 floats) -- the per-op
// fallback materializes conv2's im2col view and tendesc_strided_index
// returns indices ~100x the underlying buffer size, a B^2*cIn blowup.
// conv1's chain (cIn=1) composes fine; conv2's (cIn=32) doesn't.
// This test repros with B=2, cIn=3, h=w=6, k=3 (the same structure,
// tiny dims) so any blowup shows up against a 216-element buffer.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("pool-im2col/cIn3-chain-indices");
  // input {B=2, cIn=3, h=6, w=6} -- contiguous, numel 216.
  const u32 B = 2, cIn = 3, h = 6, w = 6, kh = 3, kw = 3;
  const u32 hOut = h - kh + 1, wOut = w - kw + 1;  // 4, 4
  // rh = ceil(kh*(h+1)/h) = ceil(3*7/6) = ceil(3.5) = 4
  const u32 rh = (kh * (h + 1) + h - 1) / h;        // 4
  const u32 rw = (kw * (w + 1) + w - 1) / w;        // 4
  Shape s_in = {0}; s_in.ndim = 4;
  s_in.dims[0] = B; s_in.dims[1] = cIn; s_in.dims[2] = h; s_in.dims[3] = w;
  u32 in_tid = tensor_alloc(CURRENT_BACKEND, s_in, DT_FP32);
  // Fill with flat index values so we can spot wrong gathers.
  {
    u32 n = B * cIn * h * w;
    f32 *buf = (f32 *)malloc(sizeof(f32) * n);
    for (u32 i = 0; i < n; i++) buf[i] = (f32)i;
    CURRENT_BACKEND->buf_write(TENS[in_tid].buf_id, buf, sizeof(f32) * n);
    free(buf);
  }
  Term in = term_new(0, TAG_TEN, DT_FP32, in_tid);

  // x0 = Reshape(in, {B,cIn,1,h,1,w})
  u32 d0[6] = { B, cIn, 1, h, 1, w };
  Term x0 = uop_reshape(in, 6, d0);
  // x1a = Expand(x0, {B,cIn,rh,h,rw,w})
  u32 d1[6] = { B, cIn, rh, h, rw, w };
  Term x1a = uop_expand(x0, 6, d1);
  // x1 = Reshape(x1a, {B,cIn,rh*h,rw*w})
  u32 d2[4] = { B, cIn, rh * h, rw * w };
  Term x1 = uop_reshape(x1a, 4, d2);
  // x2 = Shrink(x1, {{0,B},{0,cIn},{0,kh*(h+1)},{0,kw*(w+1)}})
  u32 sh2[8] = { 0, B, 0, cIn, 0, kh * (h + 1), 0, kw * (w + 1) };
  Term x2 = uop_shrink(x1, 4, sh2);
  // x3 = Reshape(x2, {B,cIn,kh,h+1,kw,w+1})
  u32 d3[6] = { B, cIn, kh, h + 1, kw, w + 1 };
  Term x3 = uop_reshape(x2, 6, d3);
  // x4 = Shrink(x3, {{0,B},{0,cIn},{0,kh},{0,hOut},{0,kw},{0,wOut}})
  u32 sh4[12] = { 0, B, 0, cIn, 0, kh, 0, hOut, 0, kw, 0, wOut };
  Term x4 = uop_shrink(x3, 6, sh4);
  // xCol6 = Permute(x4, {1,2,4,0,3,5}) -> {cIn,kh,kw,B,hOut,wOut}
  u32 perm[6] = { 1, 2, 4, 0, 3, 5 };
  Term xCol6 = uop_permute(x4, 6, perm);
  // xCol = Reshape(xCol6, {cIn*kh*kw, B*hOut*wOut})
  u32 d5[2] = { cIn * kh * kw, B * hOut * wOut };
  Term xCol = uop_reshape(xCol6, 2, d5);

  // Resolve the chain.  thvm_materialize + wnf turns the movement-op
  // chain into a TAG_TEN (strided alias or materialized copy).
  Term done = wnf(thvm_materialize(xCol));
  CHECK(term_tag(done) == TAG_TEN);
  u32 td = (u32)term_val(done);
  CHECK(td != 0 && td < TENS_NEXT);

  TenDesc *t = &TENS[td];
  const u32 R = cIn * kh * kw;       // 27
  const u32 C = B * hOut * wOut;     // 32
  CHECK_EQ(t->view.numel, R * C);    // 864
  const u32 buf_n = B * cIn * h * w; // 216

  // Whether `done` is a strided alias of the original buffer or a
  // materialized contiguous copy, check the textbook im2col mapping.
  // If `done` aliases (buf_id == in's buf_id) we check
  // tendesc_strided_index directly; if it was materialized into a
  // fresh buffer we read the buffer contents (which equal the
  // gathered source values == the source flat index by construction).
  int is_alias = (t->buf_id == TENS[in_tid].buf_id);
  f32 *matbuf = NULL;
  if (!is_alias) {
    matbuf = (f32 *)malloc(sizeof(f32) * R * C);
    CURRENT_BACKEND->buf_read(t->buf_id, matbuf, sizeof(f32) * R * C);
  }

  u32 bad = 0, max_idx_seen = 0;
  for (u32 r = 0; r < R; r++) {
    u32 ci = r / (kh * kw);
    u32 ki = (r / kw) % kh;
    u32 kj = r % kw;
    for (u32 c = 0; c < C; c++) {
      u32 b  = c / (hOut * wOut);
      u32 oi = (c / wOut) % hOut;
      u32 oj = c % wOut;
      u32 expect = ((b * cIn + ci) * h + (oi + ki)) * w + (oj + kj);
      u32 k = r * C + c;
      u32 got;
      if (is_alias) {
        got = tendesc_strided_index(t, k);
        if (got > max_idx_seen) max_idx_seen = got;
      } else {
        got = (u32)matbuf[k];   // gathered value == source flat idx
      }
      if (got != expect) {
        if (bad < 6) {
          fprintf(stderr,
            "  MISMATCH r=%u c=%u (ci=%u ki=%u kj=%u b=%u oi=%u oj=%u)"
            " k=%u: got=%u expect=%u\n",
            r, c, ci, ki, kj, b, oi, oj, k, got, expect);
        }
        bad++;
      }
    }
  }
  if (is_alias) {
    fprintf(stderr, "  [alias path] max underlying index seen=%u (buffer is %u elems)\n",
            max_idx_seen, buf_n);
    CHECK(max_idx_seen < buf_n);   // index must stay inside the buffer
  } else {
    fprintf(stderr, "  [materialized path] %u/%u entries checked\n", R * C - bad, R * C);
  }
  CHECK_EQ(bad, 0u);
  if (matbuf) free(matbuf);

  // --- conv2-shaped chain at several batch sizes: B in {4, 64, 256}.
  // The real beautiful_mnist conv2 structure (cIn=32, h=w=24, k5).
  // At BS=512 a real run refuses a 947912704-elem buffer; bisect for
  // the breakpoint.  Inspect view.numel / nviews / max underlying idx.
  // (Skip the full gather check at large B -- just numel + sampling.)
  for (u32 B2 = 4; B2 <= 256; B2 *= 8) {
    char tname[64]; snprintf(tname, sizeof(tname), "pool-im2col/conv2-shaped-B%u", B2);
    TEST_BEGIN(tname);
    const u32 cIn2 = 32, h2 = 24, w2 = 24, kh2 = 5, kw2 = 5;
    const u32 hOut2 = h2 - kh2 + 1, wOut2 = w2 - kw2 + 1;   // 20,20
    const u32 rh2 = (kh2 * (h2 + 1) + h2 - 1) / h2;          // ceil(5*25/24)=6
    const u32 rw2 = (kw2 * (w2 + 1) + w2 - 1) / w2;          // 6
    Shape s2 = {0}; s2.ndim = 4;
    s2.dims[0]=B2; s2.dims[1]=cIn2; s2.dims[2]=h2; s2.dims[3]=w2;
    u32 in2_tid = tensor_alloc(CURRENT_BACKEND, s2, DT_FP32);
    Term in2 = term_new(0, TAG_TEN, DT_FP32, in2_tid);
    u32 a0[6]={B2,cIn2,1,h2,1,w2};               Term y0=uop_reshape(in2,6,a0);
    u32 a1[6]={B2,cIn2,rh2,h2,rw2,w2};           Term y1=uop_expand(y0,6,a1);
    u32 a2[4]={B2,cIn2,rh2*h2,rw2*w2};           Term y2=uop_reshape(y1,4,a2);
    u32 b2_[8]={0,B2,0,cIn2,0,kh2*(h2+1),0,kw2*(w2+1)}; Term y3=uop_shrink(y2,4,b2_);
    u32 a3[6]={B2,cIn2,kh2,h2+1,kw2,w2+1};       Term y4=uop_reshape(y3,6,a3);
    u32 b4[12]={0,B2,0,cIn2,0,kh2,0,hOut2,0,kw2,0,wOut2}; Term y5=uop_shrink(y4,6,b4);
    u32 pm[6]={1,2,4,0,3,5};                     Term y6=uop_permute(y5,6,pm);
    u32 a5[2]={cIn2*kh2*kw2, B2*hOut2*wOut2};    Term y7=uop_reshape(y6,2,a5);
    Term done2 = wnf(thvm_materialize(y7));
    CHECK(term_tag(done2) == TAG_TEN);
    u32 td2 = (u32)term_val(done2);
    TenDesc *t2 = &TENS[td2];
    u64 expect_numel = (u64)cIn2*kh2*kw2 * (u64)B2*hOut2*wOut2;   // 800*1600 = 1,280,000
    u64 buf2_n = (u64)B2*cIn2*h2*w2;                              // 73,728
    fprintf(stderr,
      "  conv2-shaped: view.numel=%u (expect %llu)  nviews=%u  buf_id_eq_in=%d\n",
      t2->view.numel, (unsigned long long)expect_numel, t2->nviews,
      t2->buf_id == TENS[in2_tid].buf_id);
    CHECK_EQ((u64)t2->view.numel, expect_numel);
    // sample some indices; none should exceed the underlying buffer
    int is_alias2 = (t2->buf_id == TENS[in2_tid].buf_id);
    if (is_alias2) {
      u32 worst = 0;
      for (u32 k = 0; k < t2->view.numel; k += 997) {   // sample
        u32 g = tendesc_strided_index(t2, k);
        if (g > worst) worst = g;
      }
      fprintf(stderr, "  conv2-shaped [alias]: sampled max idx=%u (buf %llu elems)\n",
              worst, (unsigned long long)buf2_n);
      CHECK(worst < buf2_n);
    } else {
      fprintf(stderr, "  conv2-shaped [materialized]: %u-elem buffer\n", t2->view.numel);
      // materialized buffer numel must be the im2col size, not inflated
      CHECK((u64)t2->view.numel == expect_numel);
    }
  }

  thvm_free();
  TEST_REPORT();
}
