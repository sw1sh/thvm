// uop/substitute.c - tinygrad UOp.substitute in walk mode (uop/ops.py:532):
// every from[i] -> to[i] at once over the whole DAG, the topmost match first,
// a replacement value never re-traversed.  Memoized and driven by an explicit
// stack, so a shared subtree is rewritten once and depth is unbounded.

// The Term sources a rewrite reaches: uop_arity's children, plus a BUFFERIZE's
// closed ranges and an AFTER's two cells.  Fatal on an opcode with no case, so
// a new one cannot be skipped silently.
fn u32 uop_src_count(Term t) {
  if (term_tag(t) != TAG_UOP) {
    return 0;
  }
  u32 op = term_ext(t);
  switch (op) {
    case UOP_KERNEL: case UOP_CONST: case UOP_RANGE: case UOP_INVALID:
    case UOP_DEFINE_VAR: case UOP_BUFFER:
      return 0;
    case UOP_BUFFERIZE:
      return 1 + (u32)term_val(heap_read(term_val(t) + 3));
    case UOP_AFTER:
      return 2;
    case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND: case UOP_PAD:
    case UOP_SHRINK: case UOP_FLIP: case UOP_ADD: case UOP_MUL: case UOP_NEG:
    case UOP_RECIP: case UOP_EXP2: case UOP_LOG2: case UOP_SQRT: case UOP_CMPLT:
    case UOP_REDUCE: case UOP_CMPEQ: case UOP_LOAD: case UOP_ASSIGN: case UOP_CAST:
    case UOP_BITCAST: case UOP_INDEX_E: case UOP_IADD: case UOP_ISUB: case UOP_IMUL:
    case UOP_IDIV: case UOP_IMOD: case UOP_ILT: case UOP_IAND: case UOP_IWHERE:
    case UOP_STORE: case UOP_OPT: case UOP_IOR: case UOP_IXOR: case UOP_DETACH:
    case UOP_ISHR: case UOP_COPY: case UOP_GATHER: case UOP_SCATTER:
      return uop_arity((u8)op);
    default: {
      char msg[80];
      snprintf(msg, sizeof msg, "uop_src_count: no case for UOp opcode %u", op);
      thvm_fatal(msg);
    }
  }
}

// Source i of t, in uop_src_count's numbering.
fn Term uop_src_at(Term t, u32 i) {
  u64 loc = term_val(t);
  if (term_ext(t) == UOP_BUFFERIZE && i > 0) {
    return heap_read(loc + 3 + i);
  }
  return heap_read(loc + i);
}

static Term uop_substitute_rebuild(Term t, Term const *srcs) {
  u64 loc = term_val(t);
  switch (term_ext(t)) {
    case UOP_BUFFERIZE: {
      u32 n = (u32)term_val(heap_read(loc + 3));
      return uop_bufferize_new(srcs[0], (u32)term_val(heap_read(loc + 1)),
                               (u32)term_val(heap_read(loc + 2)), n, srcs + 1);
    }
    case UOP_AFTER:
      return uop_after(srcs[0], srcs[1]);
    case UOP_DETACH:
      return uop_detach(srcs[0]);
    case UOP_COPY:
      return uop_copy_dev(srcs[0], uop_copy_device(loc));
    default:
      return uop_graph_rebuild_with_srcs(t, srcs);
  }
}

// Open-addressed Term -> Term memo; keys are never 0 (a TAG_UOP or atom Term).
typedef struct {
  Term *keys;
  Term *vals;
  u32   cap;
  u32   len;
} UopSubMemo;

static u32 uop_sub_slot(UopSubMemo const *m, Term k) {
  u32 i = (u32)((k * 0x9e3779b97f4a7c15ull) >> 32) & (m->cap - 1);
  while (m->keys[i] != 0 && m->keys[i] != k) {
    i = (i + 1) & (m->cap - 1);
  }
  return i;
}

static void uop_sub_put(UopSubMemo *m, Term k, Term v) {
  if (2 * (m->len + 1) > m->cap) {
    UopSubMemo g = { calloc(2 * (size_t)m->cap, sizeof(Term)),
                     calloc(2 * (size_t)m->cap, sizeof(Term)), 2 * m->cap, 0 };
    if (g.keys == NULL || g.vals == NULL) {
      thvm_fatal("uop_substitute: out of memory");
    }
    for (u32 i = 0; i < m->cap; i++) {
      if (m->keys[i] != 0) {
        u32 j = uop_sub_slot(&g, m->keys[i]);
        g.keys[j] = m->keys[i];
        g.vals[j] = m->vals[i];
        g.len++;
      }
    }
    free(m->keys);
    free(m->vals);
    *m = g;
  }
  u32 i = uop_sub_slot(m, k);
  if (m->keys[i] == 0) {
    m->keys[i] = k;
    m->len++;
  }
  m->vals[i] = v;
}

// 1, with its value in *v, when k is done.
static int uop_sub_find(UopSubMemo const *m, Term k, Term *v) {
  u32 i = uop_sub_slot(m, k);
  if (m->keys[i] != k) {
    return 0;
  }
  *v = m->vals[i];
  return 1;
}

fn Term uop_substitute(Term root, Term const *from, Term const *to, u32 n) {
  if (n == 0 || root == 0) {
    return root;
  }
  UopSubMemo m = { calloc(64, sizeof(Term)), calloc(64, sizeof(Term)), 64, 0 };
  u32   scap  = 256, sp = 0;
  Term *stack = malloc(scap * sizeof(Term));
  if (m.keys == NULL || m.vals == NULL || stack == NULL) {
    thvm_fatal("uop_substitute: out of memory");
  }
  // Seeding the memo with the map makes a key's value final: it is never
  // descended into.
  for (u32 i = 0; i < n; i++) {
    uop_sub_put(&m, from[i], to[i]);
  }
  stack[sp++] = root;
  Term v;
  while (sp > 0) {
    Term t = stack[sp - 1];
    if (uop_sub_find(&m, t, &v)) {
      sp--;
      continue;
    }
    u32 ns      = uop_src_count(t);
    int pending = 0;
    for (u32 i = 0; i < ns; i++) {
      Term s = uop_src_at(t, i);
      if (s != 0 && !uop_sub_find(&m, s, &v)) {
        if (sp == scap) {
          scap *= 2;
          stack = realloc(stack, scap * sizeof(Term));
          if (stack == NULL) {
            thvm_fatal("uop_substitute: out of memory");
          }
        }
        stack[sp++] = s;
        pending = 1;
      }
    }
    if (pending) {
      continue;
    }
    sp--;
    Term srcs[1 + MAX_UOP_SRC + MAX_DIM];
    int  changed = 0;
    for (u32 i = 0; i < ns; i++) {
      Term s  = uop_src_at(t, i);
      srcs[i] = s;
      if (s != 0 && uop_sub_find(&m, s, &v)) {
        srcs[i] = v;
      }
      changed |= srcs[i] != s;
    }
    uop_sub_put(&m, t, changed ? uop_substitute_rebuild(t, srcs) : t);
  }
  Term out = root;
  uop_sub_find(&m, root, &out);
  free(stack);
  free(m.keys);
  free(m.vals);
  return out;
}
