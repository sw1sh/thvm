// term/new_eql.c - construct a TAG_EQL.
//
// Heap layout: [a, b].  The reducer (src/wnf/_.c) is strict on a and
// b: both reach WNF before any rule fires.  When both are TAG_NUM,
// the result is NUM(1) for equal values, NUM(0) otherwise.  TAG_ERA
// on either side propagates as ERA (failed branches collapse out).
// SUP commutation is handled in src/wnf/_.c so a SUP at either port
// pushes through and leaves a SUP at the head.

fn Term term_new_eql(Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_EQL, 0, loc);
}
