// test_nn_ops.c -- src/nn's elementwise, reduce, softmax, norm, select and dot ops
// against host loops, the way a C or FFI caller builds them (no py layer).
//
//   1. nn_elementwise: exp, log, sigmoid, tanh, relu, gelu (both), erf, rsqrt,
//      square, sign, abs, within f32 rounding of libm;
//   2. nn_reduce / nn_mean with keepdim, nn_softmax and its log form;
//   3. nn_layernorm and nn_batchnorm with weight and bias;
//   4. nn_maximum / nn_minimum on a broadcast pair with ties, nn_where;
//   5. nn_dot at 2-D, N-D and a vector, nn_linear with a 1-D weight (a multiply);
//   6. shape errors give t == 0.
#include "../src/thvm.c"
#include "test.h"
#include <math.h>

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

// Realizes t and compares its n values with want, each within tol * (1 + |want|).
static void check(Tensor t, f32 const *want, u32 n, double tol, char const *what) {
  CHECK(t.t != 0);
  CHECK_EQ(shape_numel(t.shape), n);
  if (t.t == 0) {
    return;
  }
  Term done = term_resolve(thvm_realize(t.t));
  CHECK_EQ(term_tag(done), TAG_TEN);
  if (term_tag(done) != TAG_TEN) {
    return;
  }
  f32 got[64] = {0};
  CPU_BACKEND.buf_read(TENS[term_val(done)].buf_id, got, (u64)n * sizeof(f32));
  u32 bad = 0;
  for (u32 i = 0; i < n; i++) {
    if (!(fabs((double)got[i] - want[i]) <= tol * (1.0 + fabs(want[i])))) {
      if (bad++ < 4) {
        fprintf(stderr, "  %s: [%u] got=%.9g want=%.9g\n", what, i, got[i], want[i]);
      }
    }
  }
  CHECK_EQ(bad, 0);
}

static f32 const xs[6] = {-2.0f, -0.5f, 0.0f, 0.25f, 1.0f, 3.0f};
static u32 const d6[1] = {6}, d23[2] = {2, 3};

static double gelu_tanh(double x) {
  return 0.5 * x * (1.0 + tanh(0.7978845608028654 * (x + 0.044715 * x * x * x)));
}

int main(void) {
  thvm_init();
  f32 want[64];

  TEST_BEGIN("nn/elementwise");
  {
    struct { u32 op; double (*f)(double); double tol; } const ops[] = {
      {NN_EXP, exp, 1e-6},      {NN_SIGMOID, NULL, 1e-6}, {NN_TANH, tanh, 1e-6},
      {NN_GELU, gelu_tanh, 1e-6}, {NN_ERF, erf, 2e-6},    {NN_GELU_ERF, NULL, 2e-6},
      {NN_SQUARE, NULL, 0},     {NN_RELU, NULL, 0},        {NN_SIGN, NULL, 0},
      {NN_ABS, fabs, 0},
    };
    for (u32 k = 0; k < sizeof ops / sizeof ops[0]; k++) {
      for (u32 i = 0; i < 6; i++) {
        double x = xs[i];
        switch (ops[k].op) {
          case NN_SIGMOID:  want[i] = (f32)(1.0 / (1.0 + exp(-x))); break;
          case NN_GELU_ERF: want[i] = (f32)(0.5 * x * (1.0 + erf(x / sqrt(2.0)))); break;
          case NN_SQUARE:   want[i] = (f32)(x * x); break;
          case NN_RELU:     want[i] = (f32)(x > 0 ? x : 0); break;
          case NN_SIGN:     want[i] = (f32)(x > 0 ? 1 : x < 0 ? -1 : 0); break;
          default:          want[i] = (f32)ops[k].f(x); break;
        }
      }
      check(nn_elementwise(ops[k].op, leaf(1, d6, xs)), want, 6, ops[k].tol, "elementwise");
    }
    f32 const pos[3] = {0.25f, 1.0f, 4.0f};
    u32 const d3[1] = {3};
    f32 const logs[3] = {(f32)log(0.25), 0.0f, (f32)log(4.0)}, rs[3] = {2.0f, 1.0f, 0.5f};
    check(nn_elementwise(NN_LOG, leaf(1, d3, pos)), logs, 3, 1e-6, "log");
    check(nn_elementwise(NN_RSQRT, leaf(1, d3, pos)), rs, 3, 1e-6, "rsqrt");
    Tensor bad = nn_elementwise(NN_ELEMENTWISE_COUNT, leaf(1, d3, pos));
    CHECK_EQ(bad.t, 0);
  }

  TEST_BEGIN("nn/reduce-softmax");
  {
    u32 ax1[1] = {1};
    Tensor m = nn_mean(leaf(2, d23, xs), 1, ax1, 1);
    CHECK_EQ(m.shape.ndim, 2);
    CHECK_EQ(m.shape.dims[1], 1);
    f32 const means[2] = {-2.5f / 3, 4.25f / 3};
    check(m, means, 2, 1e-6, "mean keepdim");
    f32 sm[6], lsm[6];
    for (u32 r = 0; r < 2; r++) {
      double mx = -INFINITY, s = 0;
      for (u32 c = 0; c < 3; c++) {
        mx = fmax(mx, xs[r * 3 + c]);
      }
      for (u32 c = 0; c < 3; c++) {
        s += exp(xs[r * 3 + c] - mx);
      }
      for (u32 c = 0; c < 3; c++) {
        sm[r * 3 + c]  = (f32)(exp(xs[r * 3 + c] - mx) / s);
        lsm[r * 3 + c] = (f32)(xs[r * 3 + c] - mx - log(s));
      }
    }
    check(nn_softmax(leaf(2, d23, xs), -1, 0), sm, 6, 1e-6, "softmax");
    check(nn_softmax(leaf(2, d23, xs), 1, 1), lsm, 6, 1e-6, "log_softmax");
  }

  TEST_BEGIN("nn/norms");
  {
    u32 ax1[1] = {1};
    f32 ln[6], bn[6];
    for (u32 r = 0; r < 2; r++) {
      double mu = 0, var = 0;
      for (u32 c = 0; c < 3; c++) {
        mu += xs[r * 3 + c] / 3.0;
      }
      for (u32 c = 0; c < 3; c++) {
        var += (xs[r * 3 + c] - mu) * (xs[r * 3 + c] - mu) / 3.0;
      }
      for (u32 c = 0; c < 3; c++) {
        ln[r * 3 + c] = (f32)((xs[r * 3 + c] - mu) / sqrt(var + 1e-5));
      }
    }
    check(nn_layernorm(leaf(2, d23, xs), 1, ax1, 1e-5), ln, 6, 1e-5, "layernorm");
    f32 const mean[3] = {0.5f, -1.0f, 2.0f}, inv[3] = {2.0f, 0.5f, 1.0f};
    f32 const wt[3] = {1.5f, -1.0f, 0.25f}, bs[3] = {0.0f, 1.0f, -2.0f};
    u32 const d3[1] = {3};
    for (u32 i = 0; i < 6; i++) {
      u32 c = i % 3;
      bn[i] = (xs[i] - mean[c]) * inv[c] * wt[c] + bs[c];
    }
    check(nn_batchnorm(leaf(2, d23, xs), leaf(1, d3, wt), leaf(1, d3, bs), leaf(1, d3, mean),
                       leaf(1, d3, inv)), bn, 6, 0, "batchnorm");
  }

  TEST_BEGIN("nn/select");
  {
    f32 const row[3] = {0.0f, 1.0f, 5.0f};   // xs[4] == row[1]: a tie
    u32 const d3[1] = {3};
    f32 mx[6], mn[6], wh[6];
    for (u32 i = 0; i < 6; i++) {
      mx[i] = xs[i] > row[i % 3] ? xs[i] : row[i % 3];
      mn[i] = xs[i] < row[i % 3] ? xs[i] : row[i % 3];
      wh[i] = row[i % 3] != 0 ? xs[i] : -xs[i];
    }
    check(nn_maximum(leaf(2, d23, xs), leaf(1, d3, row)), mx, 6, 0, "maximum");
    check(nn_minimum(leaf(2, d23, xs), leaf(1, d3, row)), mn, 6, 0, "minimum");
    Tensor x = leaf(2, d23, xs);
    check(nn_where(leaf(1, d3, row), x, nn_unary(UOP_NEG, x)), wh, 6, 0, "where");
    u32 const d2[1] = {2};
    Tensor bad = nn_maximum(leaf(2, d23, xs), leaf(1, d2, row));
    CHECK_EQ(bad.t, 0);
  }

  TEST_BEGIN("nn/dot");
  {
    f32 w[12];
    for (u32 i = 0; i < 12; i++) {
      w[i] = (f32)((i32)(i % 5) - 2) * 0.5f;
    }
    u32 const d34[2] = {3, 4}, d223[3] = {2, 2, 3}, d3[1] = {3};
    f32 xx[12];
    for (u32 i = 0; i < 12; i++) {
      xx[i] = (f32)((i32)(i * 7 % 9) - 4) * 0.25f;
    }
    // (2, 3) @ (3, 4) and (2, 2, 3) @ (3, 4), batch rows of xx.
    for (u32 r = 0; r < 4; r++) {
      for (u32 c = 0; c < 4; c++) {
        f32 acc = 0;
        for (u32 k = 0; k < 3; k++) {
          acc += xx[r * 3 + k] * w[k * 4 + c];
        }
        want[r * 4 + c] = acc;
      }
    }
    check(nn_dot(leaf(2, d23, xx), leaf(2, d34, w)), want, 8, 0, "dot 2-D");
    Tensor nd = nn_dot(leaf(3, d223, xx), leaf(2, d34, w));
    CHECK_EQ(nd.shape.ndim, 3);
    check(nd, want, 16, 0, "dot N-D");
    check(nn_dot(leaf(1, d3, xx), leaf(2, d34, w)), want, 4, 0, "dot vector");
    f32 const wv[3] = {2.0f, -1.0f, 0.5f};
    f32 mul[6];
    for (u32 i = 0; i < 6; i++) {
      mul[i] = xx[i] * wv[i % 3];
    }
    Tensor none = {0};
    check(nn_linear(leaf(2, d23, xx), leaf(1, d3, wv), none), mul, 6, 0, "linear 1-D");
    Tensor bad = nn_dot(leaf(2, d23, xx), leaf(2, d23, xx));
    CHECK_EQ(bad.t, 0);
  }

  TEST_BEGIN("nn/tri-attention-loss-pool");
  {
    // tril / triu of (2, 3).
    f32 lo[6], up[6];
    for (u32 i = 0; i < 6; i++) {
      u32 r = i / 3, c = i % 3;
      lo[i] = c <= r ? xs[i] : 0.0f;
      up[i] = c >= r ? xs[i] : 0.0f;
    }
    check(nn_tri(leaf(2, d23, xs), 0, 1), lo, 6, 0, "tril");
    check(nn_tri(leaf(2, d23, xs), 0, 0), up, 6, 0, "triu");
    // Causal attention, q = k = v = xs as (2, 3): row i attends to rows <= i.
    f32 att[6];
    for (u32 i = 0; i < 2; i++) {
      double s[2], m = -INFINITY, z = 0;
      for (u32 j = 0; j <= i; j++) {
        s[j] = 0;
        for (u32 c = 0; c < 3; c++) {
          s[j] += (double)xs[i * 3 + c] * xs[j * 3 + c] / sqrt(3.0);
        }
        m = fmax(m, s[j]);
      }
      for (u32 j = 0; j <= i; j++) {
        z += exp(s[j] - m);
      }
      for (u32 c = 0; c < 3; c++) {
        double a = 0;
        for (u32 j = 0; j <= i; j++) {
          a += exp(s[j] - m) / z * xs[j * 3 + c];
        }
        att[i * 3 + c] = (f32)a;
      }
    }
    Tensor none = {0};
    check(nn_scaled_dot_product_attention(leaf(2, d23, xs), leaf(2, d23, xs), leaf(2, d23, xs),
                                          none, 1), att, 6, 1e-5, "causal attention");
    // Sparse cross-entropy of (2, 3) logits at labels {2, 0}.
    u32 lab_tid = tensor_alloc(&CPU_BACKEND, (Shape){1, {2}}, DT_INT32);
    i32 const labels[2] = {2, 0};
    CPU_BACKEND.buf_write(TENS[lab_tid].buf_id, (void *)labels, sizeof labels);
    Tensor y = {term_new(0, TAG_TEN, DT_INT32, lab_tid), DT_INT32, {1, {2}}};
    double nll = 0;
    for (u32 r = 0; r < 2; r++) {
      double mx = fmax(fmax(xs[r * 3], xs[r * 3 + 1]), xs[r * 3 + 2]), z = 0;
      for (u32 c = 0; c < 3; c++) {
        z += exp(xs[r * 3 + c] - mx);
      }
      nll -= (xs[r * 3 + (u32)labels[r]] - mx - log(z)) / 2.0;
    }
    f32 const ce[1] = {(f32)nll};
    check(nn_sparse_categorical_crossentropy(leaf(2, d23, xs), y, -1), ce, 1, 1e-6, "sparse ce");
    // max_pool2d of (1, 4, 4): 2x2 tiles (the reshape path) and 3x3 stride 1 (the unfold).
    f32 img[16];
    for (u32 i = 0; i < 16; i++) {
      img[i] = (f32)((i * 5) % 16);
    }
    u32 const d144[3] = {1, 4, 4}, k2[2] = {2, 2}, k3[2] = {3, 3}, one[2] = {1, 1};
    f32 p2[4], p3[4];
    for (u32 oh = 0; oh < 2; oh++) {
      for (u32 ow = 0; ow < 2; ow++) {
        f32 a = -INFINITY, b = -INFINITY;
        for (u32 kh = 0; kh < 3; kh++) {
          for (u32 kw = 0; kw < 3; kw++) {
            if (kh < 2 && kw < 2) {
              a = fmaxf(a, img[(oh * 2 + kh) * 4 + ow * 2 + kw]);
            }
            b = fmaxf(b, img[(oh + kh) * 4 + ow + kw]);
          }
        }
        p2[oh * 2 + ow] = a;
        p3[oh * 2 + ow] = b;
      }
    }
    check(nn_max_pool2d(leaf(3, d144, img), 2, k2, k2, one), p2, 4, 0, "max_pool2d tiles");
    check(nn_max_pool2d(leaf(3, d144, img), 2, k3, one, one), p3, 4, 0, "max_pool2d unfold");
  }

  TEST_BEGIN("nn/call");
  {
    Tensor x = leaf(2, d23, xs);
    i32 const sm[2] = {-1, 0}, pad_short[3] = {UOP_PAD, 1, 1}, perm_dup[3] = {UOP_PERMUTE, 0, 0};
    i32 const bad_axis[2] = {2, 0}, conv_short[3] = {1, 1, 1};
    Tensor r = nn_call(NN_OP_SOFTMAX, &x, 1, sm, 2, NULL, 0);
    CHECK(r.t != 0);
    CHECK_EQ(r.shape.ndim, 2);
    CHECK_EQ(nn_call(NN_OP_COUNT + NN_ELEMENTWISE_COUNT, &x, 1, sm, 2, NULL, 0).t, 0);
    CHECK_EQ(nn_call(NN_OP_MOVE, &x, 1, pad_short, 3, NULL, 0).t, 0);
    CHECK_EQ(nn_call(NN_OP_MOVE, &x, 1, perm_dup, 3, NULL, 0).t, 0);
    CHECK_EQ(nn_call(NN_OP_SOFTMAX, &x, 1, bad_axis, 2, NULL, 0).t, 0);
    CHECK_EQ(nn_call(NN_OP_DOT, &x, 1, NULL, 0, NULL, 0).t, 0);
    Tensor xw[3] = {leaf(4, (u32[4]){1, 1, 3, 3}, xs), leaf(4, (u32[4]){1, 1, 2, 2}, xs), {0}};
    CHECK_EQ(nn_call(NN_OP_CONV2D, xw, 3, conv_short, 3, NULL, 0).t, 0);
    CHECK_EQ(nn_op_id("conv_transpose2d"), NN_OP_CONV_TRANSPOSE2D);
    // tinygrad's least_upper_dtype.
    CHECK_EQ(nn_promote(DT_FP32, DT_FP64), DT_FP64);
    CHECK_EQ(nn_promote(DT_INT32, DT_FP32), DT_FP32);
    CHECK_EQ(nn_promote(DT_INT32, DT_FP16), DT_FP16);
    CHECK_EQ(nn_promote(DT_FP16, DT_BF16), DT_FP32);
    CHECK_EQ(nn_promote(DT_UINT8, DT_INT8), DT_INT16);
    CHECK_EQ(nn_promote(DT_BOOL, DT_INT32), DT_INT32);
    CHECK_EQ(nn_promote(DT_INT64, DT_UINT64), DT_FP32);
    CHECK_EQ(nn_op_id("nope"), UINT32_MAX);
    i32 const none_i[1] = {0};
    check(nn_call(nn_op_id("square"), &x, 1, none_i, 0, NULL, 0),
          (f32[6]){4.0f, 0.25f, 0.0f, 0.0625f, 1.0f, 9.0f}, 6, 0, "square by name");
  }

  TEST_REPORT();
}
