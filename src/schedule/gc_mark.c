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

// Untruncated preserve mark for TABLE-WIDE consumers (the capture-time
// mark-park sweep).  gc_collect_roots caps its root array at 4096 and
// SILENTLY DROPS the excess -- tolerable for the watermark-scoped rollback
// (a dropped root's buffers are usually below the watermark), fatal for a
// table-wide sweep: a Krea-scale capture holds tens of thousands of
// extern-pinned wrappers, and a dropped WEIGHT wrapper's buffer would be
// parked + adopted (the round-3 gen symptom: replay_dispatches == 0, peak
// 10GB because the resident bf16 weights had been recycled out from under
// the WL cache).  Walks the root tables directly, no cap.  Returns 1 on
// success; 0 when the visited bitmap can't be allocated -- the caller MUST
// then skip its sweep (marks would be incomplete).
fn int mark_gc_preserve_all_roots(Term result) {
  u64 cells = HEAP_NEXT;
  if (cells == 0) cells = 1;
  u8 *visited = (u8 *)calloc(cells, 1);
  if (visited == NULL) return 0;
  if (result != 0) gc_mark_term(result, visited);
  for (u32 i = 0; i < WNF_LAST_STACK_LEN; i++) {
    if (WNF_LAST_STACK[i] != 0) gc_mark_term(WNF_LAST_STACK[i], visited);
  }
  for (u32 i = 0; i < DEFS_CAP; i++) {
    if (DEFS[i] != 0) gc_mark_term(DEFS[i], visited);
  }
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_LEN; i++) {
    if (EXTERN_PINNED_TERMS[i] != 0) {
      gc_mark_term(EXTERN_PINNED_TERMS[i], visited);
    }
  }
  free(visited);
  return 1;
}

// Preserve every buffer reachable from the live root set.  Delegates to the
// UNTRUNCATED walk (mark_gc_preserve_all_roots): the old fixed-4096 root array
// silently dropped roots past 4096, and a cold JIT capture holds well over
// 4096 live wrappers (cached weights + realized velocity-net intermediates) --
// a dropped root left its buffers unmarked, so the capture-recycle mark-park
// sweep parked a still-live buffer and the replay read garbage.  The all-roots
// walk marks directly from WNF_LAST_STACK + DEFS + EXTERN_PINNED with no cap
// (the visited bitmap is still sized to HEAP_NEXT, not full capacity).
fn void mark_gc_preserve(Term result) {
  (void)mark_gc_preserve_all_roots(result);
}
