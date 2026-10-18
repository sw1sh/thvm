// Heap layout: [src, NUM(device + 1)], arity 1; the + 1 stores the generic
// device -1 (the realize backend) as 0.  tinygrad's Ops.COPY holds the target
// device as its arg (uop/ops.py:766-773 copy_to_device).

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

fn Term uop_copy(Term src) { return uop_copy_dev(src, -1); }

// Takes the COPY's heap loc (term_val), not its Term; -1 is the generic device.
fn i32 uop_copy_device(u64 loc) {
  Term d = heap_read(loc + 1);
  if (term_tag(d) != TAG_NUM) return -1;
  return (i32)(u32)term_val(d) - 1;
}
