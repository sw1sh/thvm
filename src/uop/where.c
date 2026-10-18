static int uop_where_const_truth(Term cond, int *truth) {
  if (term_tag(cond) != TAG_UOP || term_ext(cond) != UOP_CONST) {
    return 0;
  }
  Term num  = heap_read(term_val(cond));
  u32  bits = (u32)term_val(num);
  if (dtype_is_float(term_ext(num))) {
    f32 v;
    memcpy(&v, &bits, sizeof v);
    *truth = (v != 0.0f);
  } else {
    *truth = (bits != 0);
  }
  return 1;
}

// tinygrad's Ops.WHERE on values: an IWHERE folding only a constant cond or
// equal branches.  uop_iwhere's index folds would turn nested selects into
// IAND / IOR, which materialize cannot schedule.  Shares uop_iwhere's hash key.
fn Term uop_where(Term cond, Term then_v, Term else_v) {
  if (then_v == else_v) {
    return then_v;
  }
  int truth;
  if (uop_where_const_truth(cond, &truth)) {
    return truth ? then_v : else_v;
  }
  u32 args[6] = { (u32)cond,   (u32)(cond   >> 32),
                  (u32)then_v, (u32)(then_v >> 32),
                  (u32)else_v, (u32)(else_v >> 32) };
  u64 key = uop_mov_hash(UOP_IWHERE, 0, args, 6);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    return hit;
  }
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, cond);
  heap_set(loc + 1, then_v);
  heap_set(loc + 2, else_v);
  Term t = term_new(0, TAG_UOP, UOP_IWHERE, loc);
  uop_mov_insert(key, t);
  return t;
}
