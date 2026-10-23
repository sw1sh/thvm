// A movement op on x and the shape it gives, as Tensor.reshape / permute /
// expand / pad / shrink compute it (py/thvm/tensor.py).  arg has n entries for
// RESHAPE, PERMUTE and EXPAND, and 2 * ndim (begin, end) pairs for PAD and SHRINK.
fn Tensor nn_move(u32 op, Tensor x, u32 n, u32 const *arg) {
  Tensor r = x;
  switch (op) {
    case UOP_RESHAPE:
    case UOP_EXPAND: {
      r.shape.ndim = n;
      for (u32 i = 0; i < n; i++) {
        r.shape.dims[i] = arg[i];
      }
      r.t = op == UOP_RESHAPE ? uop_reshape(x.t, n, arg) : uop_expand(x.t, n, arg);
      break;
    }
    case UOP_PERMUTE: {
      for (u32 i = 0; i < n; i++) {
        r.shape.dims[i] = x.shape.dims[arg[i]];
      }
      r.t = uop_permute(x.t, n, arg);
      break;
    }
    case UOP_PAD: {
      for (u32 i = 0; i < x.shape.ndim; i++) {
        r.shape.dims[i] = x.shape.dims[i] + arg[2 * i] + arg[2 * i + 1];
      }
      r.t = uop_pad(x.t, x.shape.ndim, arg);
      break;
    }
    case UOP_SHRINK: {
      for (u32 i = 0; i < x.shape.ndim; i++) {
        r.shape.dims[i] = arg[2 * i + 1] - arg[2 * i];
      }
      r.t = uop_shrink(x.t, x.shape.ndim, arg);
      break;
    }
    default: {
      r.t = 0;
      break;
    }
  }
  return r;
}

fn Tensor nn_reshape(Tensor x, u32 n, u32 const *dims) {
  return nn_move(UOP_RESHAPE, x, n, dims);
}

fn Tensor nn_permute(Tensor x, u32 n, u32 const *perm) {
  return nn_move(UOP_PERMUTE, x, n, perm);
}

fn Tensor nn_expand(Tensor x, u32 n, u32 const *dims) {
  return nn_move(UOP_EXPAND, x, n, dims);
}

// Tensor.shrink_to: every axis cut to [0, dims[i]).
fn Tensor nn_shrink_to(Tensor x, u32 const *dims) {
  u32 be[2 * MAX_DIM];
  for (u32 i = 0; i < x.shape.ndim; i++) {
    be[2 * i]     = 0;
    be[2 * i + 1] = dims[i];
  }
  return nn_move(UOP_SHRINK, x, x.shape.ndim, be);
}
