// term/new_bri.c - construct a TAG_BRI Bridge (ICC Val).
//
// θx.body, the dual of LAM.  Heap layout matches LAM:
//   heap[loc]   = body
//   bound x     = VAR(loc)  (the binder slot is the body slot, reused
//                            after substitution -- same trick as LAM)
//
// Caller is expected to have already wired the body so any internal
// VAR(loc) refers to the binder.  The mirror of `interact_app_lam`
// applies here: APP-BRI extracts the body, replaces heap[loc] with a
// SUB-flagged value, and walks the body.

fn Term term_new_bri(Term body) {
  u64 loc = heap_alloc(1);
  heap_set(loc, body);
  return term_new(0, TAG_BRI, 0, loc);
}
