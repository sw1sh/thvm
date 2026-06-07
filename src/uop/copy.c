// uop/copy.c - construct a UOP_COPY node (lazy device transfer).
//
// Heap layout: [src, NUM(device+1)] -- the source term plus a trailing
// NUM payload holding the EXPLICIT target device (mirroring the variable
// payload of REDUCE/RESHAPE; arity stays 1, src is the only recursable
// child).  The stored value is `device + 1` so the generic sentinel
// (device == -1, "the realize backend") is a non-negative 0 and CPU=1,
// METAL=2 read back cleanly.  At materialize time the COPY uploads src to
// its target device: an explicit device names the backend directly; the
// generic sentinel falls back to CURRENT_BACKEND (the legacy behaviour).
// When the target already holds src it is an identity (no kernel, no copy).
//
// Mirrors tinygrad's Ops.COPY, which carries the target as src[1]
// (uop/ops.py:660 copy_to_device -> UOp(Ops.COPY, dt, (src, DEVICE));
// ops.py:756 COPY.device == src[1].device; engine/realize.py:158
// exec_copy host-staged upload).  thvm packs the device as a NUM cell
// rather than a child DEVICE uop, but the meaning is identical.
//
// Hash-cons by (UOP_COPY, device, src): COPY nodes are immutable and a
// distinct target device is a distinct node, so the same src copied to
// two devices does not collide.

fn Term uop_copy_dev(Term src, i32 device) {
  u32 args[3] = { (u32)src, (u32)(src >> 32), (u32)device };
  u64 key = uop_mov_hash(UOP_COPY, (u32)device, args, 3);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, (u64)(u32)(device + 1)));
  Term t = term_new(0, TAG_UOP, UOP_COPY, loc);
  uop_mov_insert(key, t);
  return t;
}

// Generic transfer to the realize backend (device == -1), the legacy
// device-agnostic COPY.
fn Term uop_copy(Term src) { return uop_copy_dev(src, -1); }

// Read a COPY node's explicit target device, or -1 for the generic
// (realize-backend) sentinel.  `loc` is the COPY heap loc (term_val).
fn i32 uop_copy_device(u64 loc) {
  Term d = heap_read(loc + 1);
  if (term_tag(d) != TAG_NUM) return -1;
  return (i32)(u32)term_val(d) - 1;
}
