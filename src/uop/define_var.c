// tinygrad's UOp.variable (uop/ops.py:1018): the runtime value of a symbolic
// extent, heap [NUM(packed_extent)].  The walker reads kvar_extent_runtime and
// the renderers the V_<name> parameter kvar_collect_from_dag declares for it.

fn Term uop_define_var(u32 packed_extent) {
  u32 args[1] = { packed_extent };
  u64 key = uop_mov_hash(UOP_DEFINE_VAR, 0, args, 1);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    return hit;
  }
  u64 loc = heap_alloc(1);
  heap_set(loc, term_new(0, TAG_NUM, DT_INT32, packed_extent));
  Term t = term_new(0, TAG_UOP, UOP_DEFINE_VAR, loc);
  uop_mov_insert(key, t);
  return t;
}

fn u32 uop_define_var_extent(Term t) {
  return (u32)term_val(heap_read(term_val(t)));
}
