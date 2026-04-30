// backend/cpu/op/bitcast.c -- bit-level reinterpret kernel.
//
// memcpy of `numel * itemsize` bytes.  src and dst must share
// itemsize; the constructor enforces this so reaching this kernel
// with a mismatch is a runtime invariant violation.

fn void cpu_op_bitcast(void *out, void **srcs, u32 const *src_numels,
                       KProgOp const *p, u32 out_numel) {
  (void)src_numels;
  u32 dst_dtype = p->dtype;
  if (dtype_is_packed(dst_dtype)) {
    fprintf(stderr, "cpu_op_bitcast: packed dtype %u not supported\n", dst_dtype);
    abort();
  }
  memcpy(out, srcs[0], dtype_storage_bytes(dst_dtype, out_numel));
}
