// HVM4's eval/collapse.c without its work-stealing priority queue (Wspq): SUP
// tree leaves in key order; a SUP raises the key by one, an INC lowers it.

#define EVAL_COLLAPSE_QUEUE_CAP 65536u

typedef struct {
  Term term;
  u32  key;     // priority; lower = sooner
  u32  seq;     // FIFO tie-breaker within a key
} EvalCollapseTask;

static int eval_collapse_cmp(const void *a, const void *b) {
  const EvalCollapseTask *x = (const EvalCollapseTask *)a;
  const EvalCollapseTask *y = (const EvalCollapseTask *)b;
  if (x->key != y->key) return (x->key < y->key) ? -1 : 1;
  if (x->seq != y->seq) return (x->seq < y->seq) ? -1 : 1;
  return 0;
}

// Writes at most `cap` leaves to `out` and returns the count; popped slots are
// not reused, so the walk stops once it has pushed the queue's cap in all.
fn u64 eval_collapse(Term term, Term *out, u64 cap) {
  if (cap == 0) return 0;
  EvalCollapseTask *q = (EvalCollapseTask *)
      malloc(EVAL_COLLAPSE_QUEUE_CAP * sizeof(EvalCollapseTask));
  if (q == NULL) return 0;
  u32 q_head = 0;
  u32 q_tail = 0;
  u32 q_seq  = 0;
  u64 count  = 0;
  q[q_tail].term = term;
  q[q_tail].key  = 0;
  q[q_tail].seq  = q_seq++;
  q_tail++;

  while (q_head < q_tail && count < cap) {
    // A sort per pop stands in for HVM4's priority deque: simpler, at
    // O(n log n) a pop.
    qsort(&q[q_head], q_tail - q_head, sizeof(EvalCollapseTask),
          eval_collapse_cmp);
    EvalCollapseTask t = q[q_head++];
    Term resolved = cnf(t.term);
    switch (term_tag(resolved)) {
      case TAG_SUP: {
        if (q_tail + 2 > EVAL_COLLAPSE_QUEUE_CAP) {
          free(q);
          return count;
        }
        u64  loc = term_val(resolved);
        Term l   = heap_read(loc + 0);
        Term r   = heap_read(loc + 1);
        u32  k   = t.key + 1;
        q[q_tail].term = l; q[q_tail].key = k; q[q_tail].seq = q_seq++; q_tail++;
        q[q_tail].term = r; q[q_tail].key = k; q[q_tail].seq = q_seq++; q_tail++;
        break;
      }
      case TAG_ERA: {
        // a failed branch
        break;
      }
      case TAG_INC: {
        if (q_tail + 1 > EVAL_COLLAPSE_QUEUE_CAP) {
          free(q);
          return count;
        }
        u64  loc  = term_val(resolved);
        Term body = heap_read(loc);
        u32  k    = (t.key > 0) ? t.key - 1 : 0;
        q[q_tail].term = body; q[q_tail].key = k; q[q_tail].seq = q_seq++; q_tail++;
        break;
      }
      default: {
        out[count++] = resolved;
        break;
      }
    }
  }

  free(q);
  return count;
}
