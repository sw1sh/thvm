// test_nn_conv.c -- nn_pool, nn_conv2d and nn_conv_transpose2d (src/nn/) against
// direct loops.
//
// The inputs are small multiples of 1/8 and 1/4, so every product and partial
// sum is exact in f32 and the realized values compare bit for bit, whatever
// order the kernel sums in.
//
//   1. nn_pool's shape: (2, 3, 5, 5) by 2x2 windows of stride 2;
//   2. conv2d, groups 1, with a bias;
//   3. stride 2 with padding 1;
//   4. dilation 2;
//   5. groups 2 with a bias;
//   6. depthwise (groups == C_in), stride (2, 1), padding (1, 0);
//   7. per-side padding (1, 0) on H and (2, 1) on W, and a negative side, which
//      shrinks;
//   8. a 1-D conv (one spatial axis), stride 2, padding 1, with a bias;
//   9. conv_transpose2d: stride 2, padding 1, output_padding 1, a bias; groups 2
//      with dilation 2;
//  10. groups * cin != C_in fails with t == 0.
#include "../src/thvm.c"
#include "test.h"

static Tensor leaf(u32 ndim, u32 const *dims, f32 const *data) {
  Shape s = {0};
  s.ndim = ndim;
  memcpy(s.dims, dims, ndim * sizeof dims[0]);
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data,
                        (u64)TENS[tid].view.numel * sizeof(f32));
  Tensor t = {term_new(0, TAG_TEN, DT_FP32, tid), DT_FP32, s};
  return t;
}

static f32 *fill(u32 n, u32 mul, u32 mod, f32 scale) {
  f32 *v = malloc(n * sizeof *v);
  for (u32 i = 0; i < n; i++) {
    v[i] = (f32)((i32)((i * mul) % mod) - (i32)(mod / 2)) * scale;
  }
  return v;
}

// Realizes t and compares its n values with want bit for bit.
static void check_exact(Tensor t, f32 const *want, u32 n) {
  CHECK(t.t != 0);
  CHECK_EQ(shape_numel(t.shape), n);
  Term done = t.t != 0 ? term_resolve(thvm_realize(t.t)) : 0;
  CHECK_EQ(term_tag(done), TAG_TEN);
  if (term_tag(done) != TAG_TEN) {
    return;
  }
  f32 *got = calloc(n, sizeof *got);
  CPU_BACKEND.buf_read(TENS[term_val(done)].buf_id, got, (u64)n * sizeof(f32));
  u32 shown = 0;
  for (u32 i = 0; i < n && shown < 6; i++) {
    if (got[i] != want[i]) {
      fprintf(stderr, "  %s: out[%u] got=%.9g want=%.9g\n", test_name, i, got[i], want[i]);
      shown++;
    }
  }
  CHECK(memcmp(got, want, n * sizeof(f32)) == 0);
  free(got);
}

// A 2-D case; pad is (begin, end) on H then W.  A 1-D case has H = kH = 1.
typedef struct {
  u32 B, C_in, H, W, C_out, kH, kW, groups;
  u32 stride[2], dilation[2];
  i32 pad[4];
  int bias, one_d;
} Case;

static void check_conv(Case c) {
  u32 cin = c.C_in / c.groups, rcout = c.C_out / c.groups;
  u32 nx = c.B * c.C_in * c.H * c.W, nw = c.C_out * cin * c.kH * c.kW;
  f32 *xs = fill(nx, 7, 11, 0.125f), *ws = fill(nw, 5, 7, 0.25f), *bs = fill(c.C_out, 3, 5, 0.5f);
  u32 Ho = (u32)((i32)c.H + c.pad[0] + c.pad[1] - (i32)(c.dilation[0] * (c.kH - 1)) - 1)
           / c.stride[0] + 1;
  u32 Wo = (u32)((i32)c.W + c.pad[2] + c.pad[3] - (i32)(c.dilation[1] * (c.kW - 1)) - 1)
           / c.stride[1] + 1;
  u32 n = c.B * c.C_out * Ho * Wo;
  f32 *want = malloc(n * sizeof *want);
  for (u32 b = 0; b < c.B; b++) {
    for (u32 co = 0; co < c.C_out; co++) {
      for (u32 ho = 0; ho < Ho; ho++) {
        for (u32 wo = 0; wo < Wo; wo++) {
          f32 acc = 0.0f;
          for (u32 ci = 0; ci < cin; ci++) {
            for (u32 kh = 0; kh < c.kH; kh++) {
              for (u32 kw = 0; kw < c.kW; kw++) {
                i64 h = (i64)(ho * c.stride[0] + kh * c.dilation[0]) - c.pad[0];
                i64 v = (i64)(wo * c.stride[1] + kw * c.dilation[1]) - c.pad[2];
                if (h < 0 || h >= c.H || v < 0 || v >= c.W) {
                  continue;
                }
                u32 xc = (co / rcout) * cin + ci;
                acc += xs[((b * c.C_in + xc) * c.H + (u32)h) * c.W + (u32)v]
                     * ws[((co * cin + ci) * c.kH + kh) * c.kW + kw];
              }
            }
          }
          want[((b * c.C_out + co) * Ho + ho) * Wo + wo] = acc + (c.bias ? bs[co] : 0.0f);
        }
      }
    }
  }
  Tensor none = {0};
  Tensor b = c.bias ? leaf(1, &c.C_out, bs) : none, out;
  if (c.one_d) {
    u32 xd[3] = {c.B, c.C_in, c.W}, wd[3] = {c.C_out, cin, c.kW};
    out = nn_conv2d(leaf(3, xd, xs), leaf(3, wd, ws), b, c.groups, &c.stride[1], &c.dilation[1],
                    &c.pad[2]);
    CHECK_EQ(out.shape.ndim, 3);
  } else {
    u32 xd[4] = {c.B, c.C_in, c.H, c.W}, wd[4] = {c.C_out, cin, c.kH, c.kW};
    out = nn_conv2d(leaf(4, xd, xs), leaf(4, wd, ws), b, c.groups, c.stride, c.dilation, c.pad);
    CHECK_EQ(out.shape.ndim, 4);
    CHECK_EQ(out.shape.dims[2], Ho);
  }
  CHECK_EQ(out.shape.dims[1], c.C_out);
  CHECK_EQ(out.shape.dims[out.shape.ndim - 1], Wo);
  check_exact(out, want, n);
  free(xs);
  free(ws);
  free(bs);
  free(want);
}

// conv_transpose2d: w is (C_in, C_out / groups, kH, kW); each input element
// scatters x * w into out at ih * s - pad_begin + kh * d.
static void check_conv_transpose(Case c, u32 const out_pad[2]) {
  u32 cing = c.C_in / c.groups, coutg = c.C_out / c.groups;
  u32 nx = c.B * c.C_in * c.H * c.W, nw = c.C_in * coutg * c.kH * c.kW;
  f32 *xs = fill(nx, 7, 11, 0.125f), *ws = fill(nw, 5, 7, 0.25f), *bs = fill(c.C_out, 3, 5, 0.5f);
  u32 Ho = (c.H - 1) * c.stride[0] - (u32)(c.pad[0] + c.pad[1]) + c.dilation[0] * (c.kH - 1)
           + out_pad[0] + 1;
  u32 Wo = (c.W - 1) * c.stride[1] - (u32)(c.pad[2] + c.pad[3]) + c.dilation[1] * (c.kW - 1)
           + out_pad[1] + 1;
  u32 n = c.B * c.C_out * Ho * Wo;
  f32 *want = calloc(n, sizeof *want);
  for (u32 b = 0; b < c.B; b++) {
    for (u32 ci = 0; ci < c.C_in; ci++) {
      for (u32 ih = 0; ih < c.H; ih++) {
        for (u32 iw = 0; iw < c.W; iw++) {
          for (u32 cl = 0; cl < coutg; cl++) {
            for (u32 kh = 0; kh < c.kH; kh++) {
              for (u32 kw = 0; kw < c.kW; kw++) {
                i64 oh = (i64)(ih * c.stride[0] + kh * c.dilation[0]) - c.pad[0];
                i64 ow = (i64)(iw * c.stride[1] + kw * c.dilation[1]) - c.pad[2];
                if (oh < 0 || oh >= Ho || ow < 0 || ow >= Wo) {
                  continue;
                }
                u32 co = (ci / cing) * coutg + cl;
                want[((b * c.C_out + co) * Ho + (u32)oh) * Wo + (u32)ow] +=
                    xs[((b * c.C_in + ci) * c.H + ih) * c.W + iw]
                    * ws[((ci * coutg + cl) * c.kH + kh) * c.kW + kw];
              }
            }
          }
        }
      }
    }
  }
  if (c.bias) {
    for (u32 i = 0; i < n; i++) {
      want[i] += bs[(i / (Ho * Wo)) % c.C_out];
    }
  }
  Tensor none = {0};
  u32 xd[4] = {c.B, c.C_in, c.H, c.W}, wd[4] = {c.C_in, coutg, c.kH, c.kW};
  Tensor out = nn_conv_transpose2d(leaf(4, xd, xs), leaf(4, wd, ws),
                                   c.bias ? leaf(1, &c.C_out, bs) : none, c.groups, c.stride,
                                   c.dilation, c.pad, out_pad);
  CHECK_EQ(out.shape.dims[1], c.C_out);
  CHECK_EQ(out.shape.dims[2], Ho);
  CHECK_EQ(out.shape.dims[3], Wo);
  check_exact(out, want, n);
  free(xs);
  free(ws);
  free(bs);
  free(want);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("nn/pool-shape");
  {
    u32 xd[4] = {2, 3, 5, 5}, k[2] = {2, 2}, s[2] = {2, 2}, d[2] = {1, 1};
    f32 *xs = fill(150, 7, 11, 0.125f);
    Tensor p = nn_pool(leaf(4, xd, xs), 2, k, s, d);
    u32 want[6] = {2, 3, 2, 2, 2, 2};
    CHECK(p.t != 0);
    CHECK_EQ(p.shape.ndim, 6);
    CHECK(memcmp(p.shape.dims, want, sizeof want) == 0);
    free(xs);
  }

  TEST_BEGIN("nn/conv2d-bias");
  check_conv((Case){2, 3, 5, 5, 4, 3, 3, 1, {1, 1}, {1, 1}, {0, 0, 0, 0}, 1, 0});

  TEST_BEGIN("nn/conv2d-stride2-pad1");
  check_conv((Case){1, 2, 6, 7, 3, 3, 3, 1, {2, 2}, {1, 1}, {1, 1, 1, 1}, 0, 0});

  TEST_BEGIN("nn/conv2d-dilation2");
  check_conv((Case){1, 2, 7, 7, 2, 3, 3, 1, {1, 1}, {2, 2}, {0, 0, 0, 0}, 0, 0});

  TEST_BEGIN("nn/conv2d-groups2-bias");
  check_conv((Case){1, 4, 5, 5, 6, 3, 3, 2, {1, 1}, {1, 1}, {0, 0, 0, 0}, 1, 0});

  TEST_BEGIN("nn/conv2d-depthwise");
  check_conv((Case){2, 3, 6, 5, 3, 3, 2, 3, {2, 1}, {1, 1}, {1, 1, 0, 0}, 0, 0});

  TEST_BEGIN("nn/conv2d-per-side-pad");
  check_conv((Case){1, 2, 4, 5, 3, 2, 3, 1, {1, 1}, {1, 1}, {1, 0, 2, 1}, 1, 0});
  check_conv((Case){1, 2, 6, 6, 2, 3, 3, 1, {1, 1}, {1, 1}, {-1, 1, 0, -2}, 0, 0});

  TEST_BEGIN("nn/conv1d");
  check_conv((Case){2, 3, 1, 7, 4, 1, 3, 1, {1, 2}, {1, 1}, {0, 0, 1, 1}, 1, 1});

  TEST_BEGIN("nn/conv-transpose2d");
  {
    u32 const op1[2] = {1, 1}, op0[2] = {0, 0};
    check_conv_transpose((Case){1, 4, 3, 3, 3, 3, 3, 1, {2, 2}, {1, 1}, {1, 1, 1, 1}, 1, 0}, op1);
    check_conv_transpose((Case){2, 4, 3, 4, 2, 2, 2, 2, {1, 1}, {2, 2}, {0, 0, 0, 0}, 0, 0}, op0);
  }

  TEST_BEGIN("nn/conv2d-groups-mismatch");
  {
    u32 xd[4] = {1, 4, 5, 5}, wd[4] = {2, 3, 3, 3};
    u32 one[2] = {1, 1};
    i32 zero[4] = {0, 0, 0, 0};
    f32 *xs = fill(100, 7, 11, 0.125f), *ws = fill(54, 5, 7, 0.25f);
    Tensor none = {0};
    Tensor out = nn_conv2d(leaf(4, xd, xs), leaf(4, wd, ws), none, 2, one, one, zero);
    CHECK_EQ(out.t, 0);
    free(xs);
    free(ws);
  }

  TEST_REPORT();
}
