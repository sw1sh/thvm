// The one entry to the nn ops for the hosts (py, THVMLink, an FFI): op is an
// NN_OP_*, t[nt] its tensors (t == 0 for an absent bias or weight), ia[ni] its
// integer and ra[nr] its real arguments.  Arguments are checked here, at the
// boundary; t == 0 on a bad count, rank or axis.
//
//   MOVE         t: x; ia: a UOP_RESHAPE / PERMUTE / EXPAND / PAD / SHRINK / FLIP,
//                then its arg (dims, perm, (begin, end) per axis, a bitmask)
//   CAST         t: x; ia: dtype
//   FULL         ia: dtype, dims...; ra: value
//   BINOP        t: a, b; ia: a binary UOp
//   ELEMENTWISE  t: x; ia: NN_EXP .. NN_SQUARE
//   REDUCE       t: x; ia: REDUCE_SUM or REDUCE_MAX, keepdim, mean, axes...
//   WHERE        t: cond, x, y
//   MAXIMUM      t: a, b; ia: [1 for the minimum]
//   SOFTMAX      t: x; ia: axis, [1 for log_softmax]
//   LAYERNORM    t: x; ia: axes...; ra: eps
//   BATCHNORM    t: x, weight, bias, mean, invstd
//   DOT, LINEAR  t: x, w[, b]
//   ARANGE       ia: dtype; ra: start, stop, step
//   TRI          t: x; ia: diagonal, lower
//   SPARSE_CE    t: logits, int labels; ia: axis
//   SDPA         t: q, k, v, mask; ia: causal
//   POOL, MAX_POOL2D  t: x; ia: kernel[nk], stride[nk], dilation[nk]
//   CONV2D       t: x, w, b; ia: groups, stride[n], dilation[n], (begin, end)[n]
//   CONV_TRANSPOSE2D  the same, then output_padding[n]
// with n = rank(w) - 2 spatial axes; a negative axis counts from the end.

// The NN_OP_* ids by name, so a host need not hard-code them.
static char const *const NN_OP_NAMES[NN_OP_COUNT] = {
  "move", "cast", "full", "binop", "elementwise", "reduce", "where", "maximum", "softmax",
  "layernorm", "batchnorm", "dot", "linear", "arange", "tri", "sparse_categorical_crossentropy",
  "scaled_dot_product_attention", "pool", "max_pool2d", "conv2d", "conv_transpose2d",
};

// name's NN_OP_* id, or NN_OP_COUNT for none.
fn u32 nn_op_id(char const *name) {
  for (u32 i = 0; i < NN_OP_COUNT; i++) {
    if (strcmp(NN_OP_NAMES[i], name) == 0) {
      return i;
    }
  }
  return NN_OP_COUNT;
}

// ia[0..n) as distinct in-range axes of x into out; 0 when one is not.
static int nn_call_axes(Tensor const *x, u32 n, i32 const *ia, u32 *out) {
  u32 seen = 0;
  if (n > MAX_DIM) {
    return 0;
  }
  for (u32 i = 0; i < n; i++) {
    i32 a = ia[i] < 0 ? ia[i] + (i32)x->shape.ndim : ia[i];
    if (a < 0 || a >= (i32)x->shape.ndim || (seen >> a & 1)) {
      return 0;
    }
    seen |= 1u << a;
    out[i] = (u32)a;
  }
  return 1;
}

fn Tensor nn_call(u32 op, Tensor const *t, u32 nt, i32 const *ia, u32 ni, double const *ra,
                  u32 nr) {
  static u32 const need_t[NN_OP_COUNT] = {1, 1, 0, 2, 1, 1, 3, 2, 1, 1, 5, 2, 3, 0, 1, 2, 4,
                                          1, 1, 3, 3};
  Tensor fail = {0, DT_FP32, {0, {0}}};
  if (op >= NN_OP_COUNT || nt < need_t[op] || (need_t[op] > 0 && t[0].t == 0)) {
    return fail;
  }
  u32 ax[MAX_DIM];
  switch (op) {
    case NN_OP_MOVE: {
      u32 arg[2 * MAX_DIM], n = ni - 1, nd = t[0].shape.ndim, mv = ni >= 1 ? (u32)ia[0] : 0;
      if (ni < 1 || n > 2 * MAX_DIM) {
        return fail;
      }
      u64 numel = 1, seen = 0;
      for (u32 i = 0; i < n; i++) {
        arg[i] = (u32)ia[1 + i];
        numel *= arg[i];
        seen |= arg[i] < 64 ? 1ull << arg[i] : 0;
      }
      int ok = 0;
      switch (mv) {
        case UOP_RESHAPE: ok = n <= MAX_DIM && numel == shape_numel(t[0].shape); break;
        case UOP_EXPAND:  ok = n == nd; break;
        case UOP_PERMUTE: ok = n == nd && seen == (nd < 64 ? (1ull << nd) - 1 : 0); break;
        case UOP_PAD:
        case UOP_SHRINK:  ok = n == 2 * nd; break;
        case UOP_FLIP:    ok = n == 1; break;
        default:          ok = 0; break;
      }
      for (u32 i = 0; mv == UOP_EXPAND && ok && i < nd; i++) {
        ok = t[0].shape.dims[i] == arg[i] || t[0].shape.dims[i] == 1;
      }
      for (u32 i = 0; mv == UOP_SHRINK && ok && i < nd; i++) {
        ok = arg[2 * i] <= arg[2 * i + 1] && arg[2 * i + 1] <= t[0].shape.dims[i];
      }
      return ok ? nn_move(mv, t[0], n, arg) : fail;
    }
    case NN_OP_CAST: {
      return ni >= 1 && (u32)ia[0] < DT_COUNT ? nn_cast(t[0], (u32)ia[0]) : fail;
    }
    case NN_OP_FULL: {
      if (ni < 1 || ni - 1 > MAX_DIM || nr < 1 || (u32)ia[0] >= DT_COUNT) {
        return fail;
      }
      Shape s = {ni - 1, {0}};
      for (u32 i = 1; i < ni; i++) {
        s.dims[i - 1] = (u32)ia[i];
      }
      return nn_full(ra[0], (u32)ia[0], s);
    }
    case NN_OP_BINOP: {
      return ni >= 1 && t[1].t != 0 ? nn_binop((u32)ia[0], t[0], t[1]) : fail;
    }
    case NN_OP_ELEMENTWISE: {
      return ni >= 1 ? nn_elementwise((u32)ia[0], t[0]) : fail;
    }
    case NN_OP_REDUCE: {
      if (ni < 3 || ((u32)ia[0] != REDUCE_SUM && (u32)ia[0] != REDUCE_MAX)
          || !nn_call_axes(&t[0], ni - 3, ia + 3, ax)) {
        return fail;
      }
      return ia[2] ? nn_mean(t[0], ni - 3, ax, ia[1]) : nn_reduce((u32)ia[0], t[0], ni - 3, ax, ia[1]);
    }
    case NN_OP_WHERE: {
      return t[1].t != 0 && t[2].t != 0 ? nn_where(t[0], t[1], t[2]) : fail;
    }
    case NN_OP_MAXIMUM: {
      if (t[1].t == 0) {
        return fail;
      }
      return ni >= 1 && ia[0] ? nn_minimum(t[0], t[1]) : nn_maximum(t[0], t[1]);
    }
    case NN_OP_SOFTMAX: {
      if (ni < 1 || !nn_call_axes(&t[0], 1, ia, ax)) {
        return fail;
      }
      return nn_softmax(t[0], (i32)ax[0], ni >= 2 && ia[1]);
    }
    case NN_OP_LAYERNORM: {
      if (nr < 1 || !nn_call_axes(&t[0], ni, ia, ax)) {
        return fail;
      }
      return nn_layernorm(t[0], ni, ax, ra[0]);
    }
    case NN_OP_BATCHNORM: {
      return t[3].t != 0 && t[4].t != 0 ? nn_batchnorm(t[0], t[1], t[2], t[3], t[4]) : fail;
    }
    case NN_OP_DOT: {
      return t[1].t != 0 ? nn_dot(t[0], t[1]) : fail;
    }
    case NN_OP_LINEAR: {
      return t[1].t != 0 ? nn_linear(t[0], t[1], t[2]) : fail;
    }
    case NN_OP_ARANGE: {
      return ni >= 1 && nr >= 3 && (u32)ia[0] < DT_COUNT
               ? nn_arange(ra[0], ra[1], ra[2], (u32)ia[0]) : fail;
    }
    case NN_OP_TRI: {
      return ni >= 2 ? nn_tri(t[0], ia[0], ia[1]) : fail;
    }
    case NN_OP_SPARSE_CE: {
      if (t[1].t == 0 || ni < 1 || !nn_call_axes(&t[0], 1, ia, ax)) {
        return fail;
      }
      return nn_sparse_categorical_crossentropy(t[0], t[1], (i32)ax[0]);
    }
    case NN_OP_SDPA: {
      if (t[1].t == 0 || t[2].t == 0 || ni < 1) {
        return fail;
      }
      return nn_scaled_dot_product_attention(t[0], t[1], t[2], t[3], ia[0]);
    }
    case NN_OP_POOL:
    case NN_OP_MAX_POOL2D: {
      u32 k[3 * MAX_DIM], nk = ni / 3;
      if (ni == 0 || ni % 3 != 0 || nk > MAX_DIM) {
        return fail;
      }
      for (u32 i = 0; i < ni; i++) {
        k[i] = (u32)ia[i];
      }
      return op == NN_OP_POOL ? nn_pool(t[0], nk, k, k + nk, k + 2 * nk)
                              : nn_max_pool2d(t[0], nk, k, k + nk, k + 2 * nk);
    }
    case NN_OP_CONV2D:
    case NN_OP_CONV_TRANSPOSE2D: {
      if (t[1].t == 0 || t[1].shape.ndim < 3) {
        return fail;
      }
      u32 n = t[1].shape.ndim - 2, tr = op == NN_OP_CONV_TRANSPOSE2D;
      u32 stride[MAX_DIM], dilation[MAX_DIM], out_pad[MAX_DIM];
      i32 pad[2 * MAX_DIM];
      if (ni != 1 + 4 * n + (tr ? n : 0) || ia[0] <= 0) {
        return fail;
      }
      for (u32 j = 0; j < n; j++) {
        stride[j]      = (u32)ia[1 + j];
        dilation[j]    = (u32)ia[1 + n + j];
        pad[2 * j]     = ia[1 + 2 * n + 2 * j];
        pad[2 * j + 1] = ia[2 + 2 * n + 2 * j];
        out_pad[j]     = tr ? (u32)ia[1 + 4 * n + j] : 0;
        if (stride[j] == 0 || dilation[j] == 0) {
          return fail;
        }
      }
      return tr ? nn_conv_transpose2d(t[0], t[1], t[2], (u32)ia[0], stride, dilation, pad, out_pad)
                : nn_conv2d(t[0], t[1], t[2], (u32)ia[0], stride, dilation, pad);
    }
    default: {
      return fail;
    }
  }
}
