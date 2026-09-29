// uop/where.c - value-level select (tinygrad Ops.WHERE on tensors).
//
// Builds the same UOP_IWHERE node as uop_iwhere, but folds only a constant
// condition and equal branches.  uop_iwhere runs the index-layer folds (the
// IWHERE-of-IWHERE collapse to IAND, the where-closure substitution, the
// valid folds), which suit address math but turn nested value selects into
// tensor-level IAND/IOR nodes that materialize cannot schedule.  Value
// selects -- the chain rule's masks, a WHERE imported from tinygrad -- go
// through here.  Hash-consed under uop_iwhere's key, so an unfolded select
// is one node whichever constructor built it.

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
