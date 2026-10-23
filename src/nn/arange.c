// Tensor.arange: [start, stop) by step as a host-written leaf on the current
// backend, numpy's arange computed in double and cast to dtype.  tinygrad builds
// it lazily (mixin/op.py:166-195); the leaf keeps tensor.py's graph.
fn Tensor nn_arange(double start, double stop, double step, u32 dtype) {
  double len = step != 0 ? ceil((stop - start) / step) : 0;
  u32 n = len > 0 ? (u32)len : 0;
  Shape s = {1, {n}};
  Tensor r = {0, dtype, s};
  if (len > (double)UINT32_MAX) {
    return r;
  }
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, dtype);
  r.t = term_new(0, TAG_TEN, dtype, tid);
  if (n == 0) {
    return r;
  }
  u64 nbytes = dtype_storage_bytes(dtype, n);
  void *buf = malloc(nbytes);
  f32 *lane = dtype == DT_INT32 || dtype == DT_INT64 || dtype == DT_FP64 ? NULL
                                                                         : malloc(n * sizeof *lane);
  for (u32 i = 0; i < n; i++) {
    double v = start + (double)i * step;
    switch (dtype) {
      case DT_INT32: {
        ((i32 *)buf)[i] = (i32)v;
        break;
      }
      case DT_INT64: {
        ((i64 *)buf)[i] = (i64)v;
        break;
      }
      case DT_FP64: {
        ((f64 *)buf)[i] = v;
        break;
      }
      default: {
        lane[i] = (f32)v;
        break;
      }
    }
  }
  if (lane != NULL) {
    from_fp32_lane(buf, dtype, lane, n);
    free(lane);
  }
  TenDesc *d = &TENS[tid];
  d->backend->buf_write(d->buf_id, buf, nbytes);
  kernel_assign_write_record(d->backend, d->buf_id);
  free(buf);
  return r;
}
