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
    case UOP_RECIP: case UOP_EXP2: case UOP_LOG2: case UOP_SQRT: case UOP_SIN: case UOP_CMPLT:
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

fn Term uop_substitute(Term root, Term const *from, Term const *to, u32 n) {
  if (n == 0 || root == 0) {
    return root;
  }
  UOpMap m     = {0};
  u32    scap  = 256, sp = 0;
  Term  *stack = malloc(scap * sizeof(Term));
  if (stack == NULL) {
    thvm_fatal("uop_substitute: out of memory");
  }
  // Seeding the memo with the map makes a key's value final: it is never
  // descended into.
  for (u32 i = 0; i < n; i++) {
    (void)uop_map_put(&m, from[i], to[i]);
  }
  stack[sp++] = root;
  Term v;
  while (sp > 0) {
    Term t = stack[sp - 1];
    if (uop_map_get(&m, t, &v)) {
      sp--;
      continue;
    }
    u32 ns      = uop_src_count(t);
    int pending = 0;
    for (u32 i = 0; i < ns; i++) {
      Term s = uop_src_at(t, i);
      if (s != 0 && !uop_map_get(&m, s, &v)) {
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
      if (s != 0 && uop_map_get(&m, s, &v)) {
        srcs[i] = v;
      }
      changed |= srcs[i] != s;
    }
    (void)uop_map_put(&m, t, changed ? uop_substitute_rebuild(t, srcs) : t);
  }
  Term out = root;
  uop_map_get(&m, root, &out);
  free(stack);
  uop_map_free(&m);
  return out;
}
