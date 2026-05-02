// uop/view.c - small UOp inspection helper for graph rewrite rules.

fn u8 uop_is_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_PERMUTE
      || op == UOP_EXPAND  || op == UOP_PAD
      || op == UOP_SHRINK  || op == UOP_FLIP;
}

fn int uop_view(Term t, UOpView *out) {
  if (out == NULL) {
    return 0;
  }
  memset(out, 0, sizeof(*out));

  Term resolved = term_resolve(t);
  if (term_tag(resolved) != TAG_UOP) {
    return 0;
  }

  out->term  = resolved;
  out->op    = term_ext(resolved);
  out->arity = uop_arity(out->op);
  out->loc   = term_val(resolved);
  u8 n_src = out->arity < MAX_UOP_SRC ? out->arity : MAX_UOP_SRC;
  for (u8 i = 0; i < n_src; i++) {
    out->src[i] = heap_read(out->loc + i);
  }
  return 1;
}

fn int uop_view_op(Term t, u8 op, UOpView *out) {
  UOpView view;
  UOpView *dst = out != NULL ? out : &view;
  if (!uop_view(t, dst)) {
    return 0;
  }
  return dst->op == op;
}

fn Term uop_view_src(UOpView const *view, u8 idx) {
  if (view == NULL || idx >= view->arity || idx >= MAX_UOP_SRC) {
    return 0;
  }
  return view->src[idx];
}
