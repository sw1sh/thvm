// schedule/gc_mark.c - gc2 of the tracing-GC arc.
//
// `gc_mark_term(t, heap_visited)` traverses `t`'s heap children
// based on its tag and marks every reachable buf preserved.
// Recursion terminates via the `heap_visited[HEAP_CAP]` bitmap;
// cycles in the dyn heap (from DUP / SUP / re-entrant lambda
// bodies) won't infinite-loop.
//
// Tag dispatch covers the live runtime set per src/thvm.h:
//
//   TAG_TEN              -- mark TENS[tid].buf_id preserved.  Leaf.
//   TAG_UOP              -- val = expr_loc; arity = uop_arity(ext).
//                            heap[expr_loc + i] = child Term for
//                            i in [0, arity).
//   TAG_LAM              -- val = body_loc; heap[body_loc] = body.
//   TAG_APP              -- val = loc; heap[loc..loc+1] = [f, x].
//   TAG_SUP              -- val = loc; heap[loc..loc+1] = [a, b].
//   TAG_DUP              -- val = dup_loc; heap[dup_loc] = source.
//   TAG_OP2 / TAG_MAT    -- val = loc; heap[loc..loc+1] = [x, y]
//                            (or [handler, fallback]).
//   TAG_REF              -- val = name id; follow DEFS[name].
//   TAG_ALO              -- val = dyn heap loc holding
//                            [book_term, NUM(state_id)]; recurse
//                            into the book_term cell.
//   TAG_ERA / TAG_VAR /  -- leaves; no children.
//   TAG_NUM
//
// term_resolve unwraps SUB-tagged variables (substituted vars
// hold a SUB=1 marker followed by the actual term they were
// bound to); applied to each child before recursing so the
// graph view matches what the runtime would see.
fn void gc_mark_term(Term t, u8 *heap_visited) {
  if (t == 0) return;
  Term r = term_resolve(t);
  u8 tag = term_tag(r);
  u64 v  = term_val(r);

  switch (tag) {
    case TAG_TEN: {
      u32 tid = (u32)v;
      if (tid > 0 && tid < TENS_NEXT) {
        u32 bid = TENS[tid].buf_id;
        if (bid != 0) cpu_buf_mark_preserved(bid);
      }
      return;
    }
    case TAG_UOP: {
      u32 op = term_ext(r);
      u8 arity = uop_arity((u8)op);
      // UOP_KERNEL stores [output_buf, kid_num]; treat as
      // arity 2 for marking (the output_buf is TAG_TEN, the
      // kid_num is TAG_NUM).
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
      // val = dyn heap loc holding [book_term, NUM(state_id)].
      if (v >= HEAP_NEXT) return;
      if (heap_visited[v]) return;
      heap_visited[v] = 1;
      gc_mark_term(heap_read(v), heap_visited);
      return;
    }
    default:
      // TAG_ERA / TAG_VAR / TAG_NUM: leaves.
      return;
  }
}

// gc3: tracing-GC preserve.  Composes gc1 (root collection) +
// gc2 (recursive mark) into the preserve interface
// thvm_realize calls.  Allocates a throwaway u8[HEAP_CAP]
// bitmap; walks each root with gc_mark_term.
//
// wpt3: with WL_PINNED_TERMS now folded into gc_collect_roots,
// the cross-realize TGrad pattern is covered by the pin set
// directly -- no more defensive heap-rooted overlay.  This is
// what bm4 / hrp / gc were missing: the WL caller's TTerm
// handles are now live roots, so the trace reaches forward
// intermediate kernel outputs without touching every heap cell.
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
