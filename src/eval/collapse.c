// eval_collapse -- enumerate all pure terms reachable through SUP
// branches in a term.  Mirrors HVM4 clang/eval/collapse.c stripped
// of the parallel work-stealing queue: single-threaded FIFO walk
// over a key-priority queue with INC reducing the key (higher
// priority) and SUP increasing it (lower priority).
//
// At each step:
//   1. cnf the term to surface a SUP/ERA/INC/(pure) head.
//   2. SUP -> push both branches with key+1.
//   3. ERA -> drop the branch.
//   4. INC -> unwrap and re-push at key-1 (higher priority).
//   5. otherwise -> append to the output list.
//
// The output array has at most `cap` slots; further leaves are
// dropped silently.  Returns the count actually written.
//
// Loaded after wnf/_.c and cnf/_.c (single-TU build) so cnf() and
// the heap helpers are visible.

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
    // Sort the slice [q_head, q_tail) so the smallest-key task pops
    // first.  Sorting per round is O(n log n); for the workloads we
    // care about (single hundreds of branches) this is fine.  HVM4
    // uses a Wspq deque keyed by priority; we trade simpler code for
    // O(n log n) per round.
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
        // Drop the branch -- failed search.
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
