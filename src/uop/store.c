// uop/store.c - construct UOP_STORE and UOP_AFTER.
//
// UOP_STORE is the symmetric counterpart to UOP_INDEX_E: where INDEX_E
// reads `buf[addr]`, STORE writes `value` to `buf[addr]`.  Both share
// the symbolic-address tree shape (UOP_RANGE / UOP_I* / UOP_IWHERE).
//
// UOP_AFTER is an ordering annotation: `AFTER(node, after_node)` means
// `node` is sequenced after `after_node`.  Backends emit a barrier
// when AFTER crosses a scope boundary (LOCAL/GLOBAL) and a warp
// shuffle when crossing REG.
//
// Both opcodes hash-cons via uop_mov_cache.

fn Term uop_store(Term buf, Term addr, Term value) {
  u32 args[6] = { (u32)buf,   (u32)(buf   >> 32),
                  (u32)addr,  (u32)(addr  >> 32),
                  (u32)value, (u32)(value >> 32) };
  u64 key = uop_mov_hash(UOP_STORE, 0, args, 6);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, buf);
  heap_set(loc + 1, addr);
  heap_set(loc + 2, value);
  Term t = term_new(0, TAG_UOP, UOP_STORE, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_store_buf(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STORE) return 0;
  return heap_read(term_val(t) + 0);
}

fn Term uop_store_addr(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STORE) return 0;
  return heap_read(term_val(t) + 1);
}

fn Term uop_store_value(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STORE) return 0;
  return heap_read(term_val(t) + 2);
}

fn Term uop_after(Term node, Term after_node) {
  u32 args[4] = { (u32)node,       (u32)(node       >> 32),
                  (u32)after_node, (u32)(after_node >> 32) };
  u64 key = uop_mov_hash(UOP_AFTER, 0, args, 4);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, node);
  heap_set(loc + 1, after_node);
  Term t = term_new(0, TAG_UOP, UOP_AFTER, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_after_node(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_AFTER) return 0;
  return heap_read(term_val(t) + 0);
}

fn Term uop_after_after_node(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_AFTER) return 0;
  return heap_read(term_val(t) + 1);
}
