// (PRI(id) arg)
// ------------- APP-PRI
// prim_fun(id)(args) at prim_arity(id) args, else a PRI holding one more
//
// The registry follows HVM4's (HVM4 clang/prim/register.c).  val is 0
// (no args yet) or a block [NUM(n), arg_0, ..., arg_{n-1}], copied per arg.
fn Term interact_app_pri(Term pri, Term arg) {
  ITRS++;
  multi_emit(RULE_APP_PRI, MULTI_TERM, (u64)pri, (u64)arg, term_ext(pri));
  u32 prim_id = term_ext(pri);
  u32 arity   = prim_arity(prim_id);

  u64 old_loc = term_val(pri);
  u32 old_n   = 0;
  if (old_loc != 0) {
    Term cnt = heap_read(old_loc);
    old_n = (u32)term_val(cnt);
  }

  u32 new_n   = old_n + 1;
  u64 new_loc = heap_alloc(1 + new_n);
  heap_set(new_loc, term_new(0, TAG_NUM, DT_INT32, new_n));
  for (u32 i = 0; i < old_n; i++) {
    heap_set(new_loc + 1 + i, heap_read(old_loc + 1 + i));
  }
  heap_set(new_loc + 1 + old_n, arg);

  if (new_n >= arity) {
    PrimFn func = prim_fun(prim_id);
    if (func == NULL) return term_new(0, TAG_ERA, 0, 0);
    Term args[16];
    u32 n = (arity > 16) ? 16 : arity;
    for (u32 i = 0; i < n; i++) {
      args[i] = heap_read(new_loc + 1 + i);
    }
    return func(args);
  }

  return term_new(0, TAG_PRI, prim_id, new_loc);
}
