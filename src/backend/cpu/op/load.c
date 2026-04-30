// backend/cpu/op/load.c - identity LOAD kernel.
//
// LOAD is a structural marker (sub-item b of the UOP_LOAD arc):
// at runtime it's just a memcpy from the source buffer to the
// pre-allocated output buffer.  The value of having LOAD as its
// own opcode shows up later in sub-item (c) when the linearizer
// emits an explicit LOAD per kernel input -- runtime stays
// identity.

fn void cpu_op_load(void *out, void **srcs, u32 const *src_numels,
                    KProgOp const *p, u32 out_numel) {
  void *src     = srcs[0];
  u32   in_numel = src_numels[0];
  u32   n        = (in_numel < out_numel) ? in_numel : out_numel;
  if (dtype_is_packed(p->dtype)) {
    memcpy(out, src, (size_t)dtype_storage_bytes(p->dtype, n));
    return;
  }
  u32   esz      = dtype_itemsize(p->dtype);
  memcpy(out, src, (size_t)n * esz);
}
