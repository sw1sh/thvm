// tensor/assign.c - write one tensor's elements into another's buffer.
//
// tensor_assign(dst, src, off) writes src's view.numel elements into
// dst's buffer from element `off` on: a weight update writes the whole
// buffer at 0, a KV-cache append writes one row of the cache.  Matching
// dtypes copy the storage bytes, so every dtype's bit patterns survive
// (NaN payloads, subnormals, packed nibbles).  Differing dtypes store
// each value at dst's dtype as the CPU walker's CAST does (uwalk_load_* /
// uwalk_store_* in backend/cpu/uop_walk.c, so an fp8 dst saturates like
// a kernel store).  tinygrad casts a weak (Python-scalar) value to the
// target dtype before it assigns and raises on any other mismatch
// (tensor.py:243-244 at 365f87a8a); a fired ASSIGN cannot tell the two
// apart, so it converts both.
//
// Same backend and dtype: the backend's native copy.  Otherwise a host
// round trip that reads through src's backend and writes through dst's,
// the memcpy a cross-device COPY lowers to (tinygrad engine/realize.py
// exec_copy).  Returns 0 once dst is written, -1 if a host buffer could
// not be allocated.

// Packed nibbles have no walker load or store; their values are small
// integers, which f32 holds exactly.
static int tensor_assign_convert(void *dst, u32 ddt, void *src, u32 sdt, u32 n) {
  if (dtype_is_packed(ddt) || dtype_is_packed(sdt)) {
    f32 *tmp = (f32 *)malloc((size_t)n * sizeof(f32));
    if (tmp == NULL) {
      return -1;
    }
    to_fp32_lane(tmp, src, sdt, n);
    from_fp32_lane(dst, ddt, tmp, n);
    free(tmp);
    return 0;
  }
  int to_float = dtype_is_float(ddt);
  for (u32 i = 0; i < n; i++) {
    if (to_float) {
      uwalk_store_f64(dst, i, ddt, uwalk_load_f64(src, i, sdt));
    } else {
      uwalk_store_i64(dst, i, ddt, uwalk_load_i64(src, i, sdt));
    }
  }
  return 0;
}

fn int tensor_assign(u32 dst_id, u32 src_id, u64 dst_off) {
  TenDesc *dd       = &TENS[dst_id];
  TenDesc *sd       = &TENS[src_id];
  u32      numel    = dd->view.numel;
  u64      nbytes   = dtype_storage_bytes(dd->dtype, numel);
  u64      byte_off = dtype_storage_bytes(dd->dtype, dst_off);
  int      same     = (dd->dtype == sd->dtype);
  if (same && dd->backend == sd->backend) {
    if (dd->backend->buf_copy_at != NULL
        && dd->backend->buf_copy_at(dd->buf_id, byte_off, sd->buf_id, nbytes) == 0) {
      return 0;
    }
    if (byte_off == 0 && dd->backend->buf_copy != NULL
        && dd->backend->buf_copy(dd->buf_id, sd->buf_id, nbytes) == 0) {
      return 0;
    }
  }
  u64   sbytes = dtype_storage_bytes(sd->dtype, numel);
  void *in     = malloc((size_t)sbytes);
  void *out    = same ? in : malloc((size_t)nbytes);
  int   rc     = -1;
  if (in != NULL && out != NULL) {
    sd->backend->buf_read(sd->buf_id, in, sbytes);
    if (same || tensor_assign_convert(out, dd->dtype, in, sd->dtype, numel) == 0) {
      if (dd->backend->buf_write_at != NULL) {
        dd->backend->buf_write_at(dd->buf_id, byte_off, out, nbytes);
      } else {
        dd->backend->buf_write(dd->buf_id, out, nbytes);   // whole-buffer (byte_off == 0)
      }
      rc = 0;
    }
  }
  if (out != in) {
    free(out);
  }
  free(in);
  return rc;
}
