// The symbolic address layer: RANGE leaves, integer ops, IWHERE, the INVALID
// pad-mask sentinel, INDEX_E (buffer + address) and BUFFERIZE.  All hash-cons,
// as movement lowering rebuilds the same address trees for each consumer.

// Heap layout: [NUM(axis_id), NUM(axis_type), NUM(extent)], axis_type a KAX_*.
fn Term uop_range(u32 axis_id, u32 axis_type, u32 extent) {
  u32 args[3] = { axis_id, axis_type, extent };
  u64 key = uop_mov_hash(UOP_RANGE, 0, args, 3);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, axis_id));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, axis_type));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, extent));
  Term t = term_new(0, TAG_UOP, UOP_RANGE, loc);
  uop_mov_insert(key, t);
  return t;
}

fn u32 uop_range_axis_id(Term r) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  return (u32)term_val(heap_read(term_val(r) + 0));
}

fn u32 uop_range_axis_type(Term r) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  return (u32)term_val(heap_read(term_val(r) + 1));
}

fn u32 uop_range_extent(Term r) {
  if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) return 0;
  return (u32)term_val(heap_read(term_val(r) + 2));
}

// The literal extent, or a kvar range's upper bound kvar_hi (KVAR_FLAG, bit 31,
// marks one; the low 31 bits are the kvar id): buffer sizes and dispatch shapes
// must fit the worst case.
fn u32 uop_range_static_extent(Term r) {
  return kvar_extent_static(uop_range_extent(r));
}

// Heap layout: [buffer_src, addr_expr].
fn Term uop_index_e(Term buffer, Term addr) {
  u32 args[4] = { (u32)buffer, (u32)(buffer >> 32),
                  (u32)addr,   (u32)(addr >> 32) };
  u64 key = uop_mov_hash(UOP_INDEX_E, 0, args, 4);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, buffer);
  heap_set(loc + 1, addr);
  Term t = term_new(0, TAG_UOP, UOP_INDEX_E, loc);
  uop_mov_insert(key, t);
  return t;
}

// opcode is IADD, ISUB, IMUL, IDIV, IMOD, ILT, IAND, IOR, IXOR or ISHR.
fn Term uop_int_binary(u32 opcode, Term a, Term b) {
  Term folded = uop_simplify_int_binary(opcode, a, b);
  if (folded != 0) return folded;
  u32 args[4] = { (u32)a, (u32)(a >> 32), (u32)b, (u32)(b >> 32) };
  u64 key = uop_mov_hash(opcode, 0, args, 4);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  Term t = term_new(0, TAG_UOP, opcode, loc);
  uop_mov_insert(key, t);
  return t;
}

// Heap layout: [cond, then_v, else_v].
fn Term uop_iwhere(Term cond, Term then_v, Term else_v) {
  Term folded = uop_simplify_iwhere(cond, then_v, else_v);
  if (folded != 0) return folded;
  u32 args[6] = { (u32)cond,   (u32)(cond   >> 32),
                  (u32)then_v, (u32)(then_v >> 32),
                  (u32)else_v, (u32)(else_v >> 32) };
  u64 key = uop_mov_hash(UOP_IWHERE, 0, args, 6);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, cond);
  heap_set(loc + 1, then_v);
  heap_set(loc + 2, else_v);
  Term t = term_new(0, TAG_UOP, UOP_IWHERE, loc);
  uop_mov_insert(key, t);
  return t;
}

// The PAD-mask sentinel, a singleton; its NUM(0) cell is a placeholder.
fn Term uop_invalid(void) {
  u64 key = uop_mov_hash(UOP_INVALID, 0, NULL, 0);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(1);
  heap_set(loc, term_new(0, TAG_NUM, DT_INT32, 0));
  Term t = term_new(0, TAG_UOP, UOP_INVALID, loc);
  uop_mov_insert(key, t);
  return t;
}

// Heap layout: [value, NUM(addrspace), NUM(removable), NUM(n_ranges), range_0,
// ...]: tinygrad's Ops.STAGE (schedule/indexing.py:92), addrspace a UOP_SCOPE_*
// and removable its BufferizeOpts.removable.
fn Term uop_bufferize_new(Term value, u32 addrspace, u32 removable,
                          u32 n_ranges, Term const *ranges) {
  if (n_ranges > MAX_DIM) n_ranges = MAX_DIM;

  u32 args[3 + 2 + 2 * MAX_DIM];
  args[0] = (u32)value;
  args[1] = (u32)(value >> 32);
  args[2] = addrspace;
  args[3] = removable;
  args[4] = n_ranges;
  for (u32 i = 0; i < n_ranges; i++) {
    args[5 + 2 * i + 0] = (u32)ranges[i];
    args[5 + 2 * i + 1] = (u32)(ranges[i] >> 32);
  }
  u64 key = uop_mov_hash(UOP_BUFFERIZE, 0, args, 5 + 2 * n_ranges);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;

  u64 loc = heap_alloc(4 + n_ranges);
  heap_set(loc + 0, value);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, addrspace));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, removable));
  heap_set(loc + 3, term_new(0, TAG_NUM, DT_INT32, n_ranges));
  for (u32 i = 0; i < n_ranges; i++) {
    heap_set(loc + 4 + i, ranges[i]);
  }
  Term t = term_new(0, TAG_UOP, UOP_BUFFERIZE, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_bufferize_value(Term b) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  return heap_read(term_val(b) + 0);
}

fn u32 uop_bufferize_addrspace(Term b) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  return (u32)term_val(heap_read(term_val(b) + 1));
}

fn u32 uop_bufferize_n_ranges(Term b) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  return (u32)term_val(heap_read(term_val(b) + 3));
}

fn Term uop_bufferize_range_at(Term b, u32 i) {
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_BUFFERIZE) return 0;
  u32 n = uop_bufferize_n_ranges(b);
  if (i >= n) return 0;
  return heap_read(term_val(b) + 4 + i);
}
