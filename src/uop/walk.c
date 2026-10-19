// UOpWalk marks a node as it is queued: linear at any depth, where shared
// subtrees make recursion exponential and a depth budget wrong.  tinygrad's
// UOp.toposort (uop/ops.py:295-306), with the caller's pushes as its `gate`.

typedef struct {
  Term  *stack;
  u32    sp;
  u32    stack_cap;
  UOpMap seen;
} UOpWalk;

fn void uop_walk_push(UOpWalk *w, Term t) {
  if (term_tag(t) != TAG_UOP || !uop_map_put(&w->seen, t, 0)) {
    return;
  }
  if (w->sp == w->stack_cap) {
    u32   cap   = w->stack_cap ? w->stack_cap * 2 : 64;
    Term *stack = (Term *)realloc(w->stack, cap * sizeof(Term));
    if (stack == NULL) {
      thvm_fatal("uop_walk: out of memory");
    }
    w->stack     = stack;
    w->stack_cap = cap;
  }
  w->stack[w->sp++] = t;
}

fn Term uop_walk_pop(UOpWalk *w) {
  return w->sp > 0 ? w->stack[--w->sp] : 0;
}

fn void uop_walk_free(UOpWalk *w) {
  free(w->stack);
  uop_map_free(&w->seen);
  memset(w, 0, sizeof(*w));
}
