// uop/walk.c - visit every UOp node reachable from a root, once each.
//
// Hash-consed UOp graphs share subtrees, so a recursive search without a
// visited set revisits them exponentially, and a depth or stack budget
// returns a wrong answer on any graph past it.  UOpWalk keeps pending
// nodes on a growable heap stack and marks each node when it is queued,
// so a search is linear in the reachable nodes at any depth.  Mirrors
// tinygrad's UOp.toposort (tinygrad/uop/ops.py:295-306): an explicit
// stack plus a visited set, where the caller's choice of which sources
// to queue plays the role of toposort's `gate`.
//
// Usage: uop_walk_push the root, then pop nodes until 0, pushing the
// sources worth searching; uop_walk_free releases the storage.

typedef struct {
  Term *stack;
  u32   sp;
  u32   stack_cap;
  Term *seen;        // open-addressed; 0 marks an empty slot
  u32   seen_cap;    // power of two
  u32   seen_used;
} UOpWalk;

__attribute__((noreturn))
static void uop_walk_oom(void) {
  thvm_fatal("uop_walk: out of memory");
}

static u32 uop_walk_hash(Term t) {
  u64 x = t;
  x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return (u32)x;
}

static void uop_walk_insert(Term *slots, u32 cap, Term t) {
  u32 mask = cap - 1;
  for (u32 i = uop_walk_hash(t) & mask; ; i = (i + 1) & mask) {
    if (slots[i] == 0) {
      slots[i] = t;
      return;
    }
  }
}

static void uop_walk_grow_seen(UOpWalk *w) {
  u32   cap   = w->seen_cap ? w->seen_cap * 2 : 64;
  Term *slots = (Term *)calloc(cap, sizeof(Term));
  if (slots == NULL) {
    uop_walk_oom();
  }
  for (u32 i = 0; i < w->seen_cap; i++) {
    if (w->seen[i] != 0) {
      uop_walk_insert(slots, cap, w->seen[i]);
    }
  }
  free(w->seen);
  w->seen     = slots;
  w->seen_cap = cap;
}

// 1 when `t` was not marked before (and is now).  UOp Terms are never 0.
static int uop_walk_mark(UOpWalk *w, Term t) {
  if (2 * (w->seen_used + 1) > w->seen_cap) {
    uop_walk_grow_seen(w);
  }
  u32 mask = w->seen_cap - 1;
  for (u32 i = uop_walk_hash(t) & mask; ; i = (i + 1) & mask) {
    if (w->seen[i] == t) {
      return 0;
    }
    if (w->seen[i] == 0) {
      w->seen[i] = t;
      w->seen_used++;
      return 1;
    }
  }
}

// Queues `t` unless it is not a UOp node or was queued before.
fn void uop_walk_push(UOpWalk *w, Term t) {
  if (term_tag(t) != TAG_UOP || !uop_walk_mark(w, t)) {
    return;
  }
  if (w->sp == w->stack_cap) {
    u32   cap   = w->stack_cap ? w->stack_cap * 2 : 64;
    Term *stack = (Term *)realloc(w->stack, cap * sizeof(Term));
    if (stack == NULL) {
      uop_walk_oom();
    }
    w->stack     = stack;
    w->stack_cap = cap;
  }
  w->stack[w->sp++] = t;
}

// Next queued node, most recently pushed first; 0 when the walk is done.
fn Term uop_walk_pop(UOpWalk *w) {
  return w->sp > 0 ? w->stack[--w->sp] : 0;
}

fn void uop_walk_free(UOpWalk *w) {
  free(w->stack);
  free(w->seen);
  memset(w, 0, sizeof(*w));
}
