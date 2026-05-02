// uop/graph_simplify.c - first symbolic pass over uop_graph_rewrite.

static Term uop_graph_simplify_unary(Term t, void *user) {
  (void)user;
  UOpView v;
  if (!uop_view(t, &v) || !uop_is_unary_elementwise(v.op)) {
    return 0;
  }
  return uop_rewrite_unary(v.op, v.src[0]);
}

static Term uop_graph_simplify_binary(Term t, void *user) {
  (void)user;
  UOpView v;
  if (!uop_view(t, &v) || !uop_is_binary_elementwise(v.op)) {
    return 0;
  }
  return uop_rewrite_binary(v.op, v.src[0], v.src[1]);
}

static Term uop_graph_simplify_movement_chain(Term t, void *user) {
  (void)user;
  UOpView v;
  if (!uop_view(t, &v)) {
    return 0;
  }
  if (v.op != UOP_RESHAPE && v.op != UOP_EXPAND) {
    return 0;
  }

  Term collapsed = uop_rewrite_movement_src(v.op, v.src[0]);
  if (collapsed == 0) {
    return 0;
  }

  u32 ndim = (u32)term_val(heap_read(v.loc + 1));
  if (ndim > MAX_DIM) {
    return 0;
  }
  u32 dims[MAX_DIM];
  for (u32 i = 0; i < ndim; i++) {
    dims[i] = (u32)term_val(heap_read(v.loc + 2 + i));
  }

  if (v.op == UOP_RESHAPE) {
    return uop_reshape(collapsed, ndim, dims);
  }
  return uop_expand(collapsed, ndim, dims);
}

fn Term uop_graph_simplify(Term root) {
  UOpGraphRewriteRule rules[] = {
    {"symbolic-unary",          uop_graph_simplify_unary},
    {"symbolic-binary",         uop_graph_simplify_binary},
    {"movement-chain-collapse", uop_graph_simplify_movement_chain},
  };
  u32 n_rules = (u32)(sizeof(rules) / sizeof(rules[0]));
  return uop_graph_rewrite(root, rules, n_rules, NULL);
}
