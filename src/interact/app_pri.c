// (PRI(id) arg)
// -------------- APP-PRI
// (accumulate `arg` into PRI's argument buffer; if buffer is now
// full to arity, call PRIM_TABLE[id].fn with the args and return
// its result.  Otherwise return a new PRI pointing at the larger
// buffer.)
//
// 8.1b: thin port of HVM4's primitive-call mechanism (see
// `TinyHVM/HVM4/clang/prim/register.c`).  Used by 8.1c onwards to
// invoke `thvm_unify` and friends from inside SUP-encoded CP
// enumeration.
//
// Heap layout for a partially-applied PRI:
//   val = heap_loc to a cell sequence
//     heap_loc[0]   = NUM(count_so_far)
//     heap_loc[1+i] = arg_i           for i in 0..count_so_far
//
// A freshly-constructed PRI (term_new_pri) has val=0 and no heap
// cells; we treat that as count_so_far == 0.
fn Term interact_app_pri(Term pri, Term arg) {
  ITRS++;
  u32 prim_id = term_ext(pri);
  u32 arity   = prim_arity(prim_id);

  u64 old_loc = term_val(pri);
  u32 old_n   = 0;
  if (old_loc != 0) {
    Term cnt = heap_read(old_loc);
    old_n = (u32)term_val(cnt);
  }

  // Build new accumulator with one more arg.
  u32 new_n   = old_n + 1;
  u64 new_loc = heap_alloc(1 + new_n);
  heap_set(new_loc, term_new(0, TAG_NUM, DT_I32, new_n));
  for (u32 i = 0; i < old_n; i++) {
    heap_set(new_loc + 1 + i, heap_read(old_loc + 1 + i));
  }
  heap_set(new_loc + 1 + old_n, arg);

  if (new_n >= arity) {
    // Saturated -- call the primitive.  Defensively: if no fn
    // registered, fall through to ERA so the caller doesn't
    // hang.
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
