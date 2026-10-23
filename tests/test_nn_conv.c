// test_nn_conv.c -- nn_pool and nn_conv2d (src/nn/) against a direct loop.
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
//   7. groups * cin != C_in fails with t == 0.
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

typedef struct {
  u32 B, C_in, H, W, C_out, kH, kW, groups;
  u32 stride[2], dilation[2], pad[2];
  int bias;
} Case;

// out[b][co][ho][wo] = bias[co] + sum over the group's (ci, kh, kw).
static void conv_ref(Case const *c, f32 const *x, f32 const *w, f32 const *bias, u32 Ho, u32 Wo,
                     f32 *out) {
  u32 cin = c->C_in / c->groups, rcout = c->C_out / c->groups;
  for (u32 b = 0; b < c->B; b++) {
    for (u32 co = 0; co < c->C_out; co++) {
      for (u32 ho = 0; ho < Ho; ho++) {
        for (u32 wo = 0; wo < Wo; wo++) {
          f32 acc = 0.0f;
          for (u32 ci = 0; ci < cin; ci++) {
            for (u32 kh = 0; kh < c->kH; kh++) {
              for (u32 kw = 0; kw < c->kW; kw++) {
                i64 h = (i64)(ho * c->stride[0] + kh * c->dilation[0]) - c->pad[0];
                i64 v = (i64)(wo * c->stride[1] + kw * c->dilation[1]) - c->pad[1];
                if (h < 0 || h >= c->H || v < 0 || v >= c->W) {
                  continue;
                }
                u32 xc = (co / rcout) * cin + ci;
                acc += x[((b * c->C_in + xc) * c->H + (u32)h) * c->W + (u32)v]
                     * w[((co * cin + ci) * c->kH + kh) * c->kW + kw];
              }
            }
          }
          out[((b * c->C_out + co) * Ho + ho) * Wo + wo] = acc + (c->bias ? bias[co] : 0.0f);
        }
      }
    }
  }
}

static void check_conv(Case c) {
  u32 cin = c.C_in / c.groups;
  u32 xd[4] = {c.B, c.C_in, c.H, c.W}, wd[4] = {c.C_out, cin, c.kH, c.kW}, bd[1] = {c.C_out};
  u32 nx = c.B * c.C_in * c.H * c.W, nw = c.C_out * cin * c.kH * c.kW;
  f32 *xs = fill(nx, 7, 11, 0.125f), *ws = fill(nw, 5, 7, 0.25f), *bs = fill(c.C_out, 3, 5, 0.5f);
  Tensor b = {0};
  if (c.bias) {
    b = leaf(1, bd, bs);
  }
  Tensor out = nn_conv2d(leaf(4, xd, xs), leaf(4, wd, ws), b, c.groups, c.stride, c.dilation,
                         c.pad);
  u32 Ho = (c.H + 2 * c.pad[0] - c.dilation[0] * (c.kH - 1) - 1) / c.stride[0] + 1;
  u32 Wo = (c.W + 2 * c.pad[1] - c.dilation[1] * (c.kW - 1) - 1) / c.stride[1] + 1;
  CHECK(out.t != 0);
  CHECK_EQ(out.shape.ndim, 4);
  CHECK_EQ(out.shape.dims[0], c.B);
  CHECK_EQ(out.shape.dims[1], c.C_out);
  CHECK_EQ(out.shape.dims[2], Ho);
  CHECK_EQ(out.shape.dims[3], Wo);
  u32 n = c.B * c.C_out * Ho * Wo;
  f32 *want = malloc(n * sizeof *want), *got = calloc(n, sizeof *got);
  conv_ref(&c, xs, ws, bs, Ho, Wo, want);
  Term done = out.t != 0 ? term_resolve(thvm_realize(out.t)) : 0;
  CHECK_EQ(term_tag(done), TAG_TEN);
  if (term_tag(done) == TAG_TEN) {
    CHECK_EQ(TENS[term_val(done)].view.numel, n);
    CPU_BACKEND.buf_read(TENS[term_val(done)].buf_id, got, (u64)n * sizeof(f32));
    u32 shown = 0;
    for (u32 i = 0; i < n && shown < 6; i++) {
      if (got[i] != want[i]) {
        fprintf(stderr, "  %s: out[%u] got=%.9g want=%.9g\n", test_name, i, got[i], want[i]);
        shown++;
      }
    }
    CHECK(memcmp(got, want, n * sizeof(f32)) == 0);
  }
  free(xs);
  free(ws);
  free(bs);
  free(want);
  free(got);
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
  check_conv((Case){2, 3, 5, 5, 4, 3, 3, 1, {1, 1}, {1, 1}, {0, 0}, 1});

  TEST_BEGIN("nn/conv2d-stride2-pad1");
  check_conv((Case){1, 2, 6, 7, 3, 3, 3, 1, {2, 2}, {1, 1}, {1, 1}, 0});

  TEST_BEGIN("nn/conv2d-dilation2");
  check_conv((Case){1, 2, 7, 7, 2, 3, 3, 1, {1, 1}, {2, 2}, {0, 0}, 0});

  TEST_BEGIN("nn/conv2d-groups2-bias");
  check_conv((Case){1, 4, 5, 5, 6, 3, 3, 2, {1, 1}, {1, 1}, {0, 0}, 1});

  TEST_BEGIN("nn/conv2d-depthwise");
  check_conv((Case){2, 3, 6, 5, 3, 3, 2, 3, {2, 1}, {1, 1}, {1, 0}, 0});

  TEST_BEGIN("nn/conv2d-groups-mismatch");
  {
    u32 xd[4] = {1, 4, 5, 5}, wd[4] = {2, 3, 3, 3};
    u32 one[2] = {1, 1}, zero[2] = {0, 0};
    f32 *xs = fill(100, 7, 11, 0.125f), *ws = fill(54, 5, 7, 0.25f);
    Tensor none = {0};
    Tensor out = nn_conv2d(leaf(4, xd, xs), leaf(4, wd, ws), none, 2, one, one, zero);
    CHECK_EQ(out.t, 0);
    free(xs);
    free(ws);
  }

  TEST_REPORT();
}
