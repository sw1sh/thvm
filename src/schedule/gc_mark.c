// schedule/gc_mark.c - tracing-GC mark walk.
//
// gc_mark_term traverses `t`'s heap children based on its tag
// and marks every reachable buf preserved.  Recursion
// terminates via the heap_visited[HEAP_CAP] bitmap so cycles
// in the dyn heap (DUP / SUP / re-entrant lambda bodies)
// don't infinite-loop.
//
// Tag dispatch covers the live runtime set per src/thvm.h.
// term_resolve unwraps SUB-tagged variables before recursing
// so the graph view matches what the runtime would see.
fn void gc_mark_term(Term t, u8 *heap_visited) {
  if (t == 0) return;
  Term r = term_resolve(t);
  u8 tag = term_tag(r);
  u64 v  = term_val(r);

  switch (tag) {
    case TAG_TEN: {
      u32 tid = (u32)v;
      tensor_mark_buf_preserved(tid);
      return;
    }
    case TAG_UOP: {
      u32 op = term_ext(r);
      u8 arity = uop_arity((u8)op);
      // UOP_KERNEL stores [output_buf, kid_num]; uop_arity
      // returns 0 for it but we still want to mark the output.
      if (op == UOP_KERNEL) arity = 2;
      for (u8 i = 0; i < arity; i++) {
        if (v + i >= HEAP_NEXT) break;
        if (heap_visited[v + i]) continue;
        heap_visited[v + i] = 1;
        gc_mark_term(heap_read(v + i), heap_visited);
      }
      return;
    }
    case TAG_LAM:
    case TAG_DUP: {
      if (v >= HEAP_NEXT) return;
      if (heap_visited[v]) return;
      heap_visited[v] = 1;
      gc_mark_term(heap_read(v), heap_visited);
      return;
    }
    case TAG_APP:
    case TAG_SUP:
    case TAG_OP2:
    case TAG_MAT: {
      for (u8 i = 0; i < 2; i++) {
        if (v + i >= HEAP_NEXT) break;
        if (heap_visited[v + i]) continue;
        heap_visited[v + i] = 1;
        gc_mark_term(heap_read(v + i), heap_visited);
      }
      return;
    }
    case TAG_REF: {
      if (v < DEFS_CAP && DEFS[v] != 0) {
        gc_mark_term(DEFS[v], heap_visited);
      }
      return;
    }
    case TAG_ALO: {
      if (v >= HEAP_NEXT) return;
      if (heap_visited[v]) return;
      heap_visited[v] = 1;
      gc_mark_term(heap_read(v), heap_visited);
      return;
    }
    default:
      return;   // TAG_ERA / TAG_VAR / TAG_NUM: leaves.
  }
}

#define GC_ROOTS_CAP 4096
fn void mark_gc_preserve(Term result) {
  Term roots[GC_ROOTS_CAP];
  u32  n_roots = 0;
  gc_collect_roots(result, roots, GC_ROOTS_CAP, &n_roots);
  u8 *visited = (u8 *)calloc(HEAP_CAP, 1);
  if (visited != NULL) {
    for (u32 i = 0; i < n_roots; i++) {
      gc_mark_term(roots[i], visited);
    }
    free(visited);
  }
}
