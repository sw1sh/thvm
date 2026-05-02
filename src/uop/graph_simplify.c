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

static Term uop_graph_simplify_cast(Term t, void *user) {
  (void)user;
  UOpView v;
  if (!uop_view(t, &v)) {
    return 0;
  }
  if (v.op != UOP_CAST && v.op != UOP_BITCAST) {
    return 0;
  }

  Term dtype_term = heap_read(v.loc + 1);
  if (term_tag(dtype_term) != TAG_NUM) {
    return 0;
  }
  u32 dtype = (u32)term_val(dtype_term);
  if (v.op == UOP_CAST) {
    u32 src_dtype = DT_FP32;
    if (term_dtype_in(v.src[0], 0, &src_dtype) && src_dtype == dtype) {
      return v.src[0];
    }
    if (term_tag(v.src[0]) == TAG_UOP && term_ext(v.src[0]) == UOP_CONST) {
      Term folded = uop_cast(v.src[0], dtype);
      if (folded != 0 && folded != t
          && !(term_tag(folded) == TAG_UOP && term_ext(folded) == UOP_CAST)) {
        return folded;
      }
    }
    return 0;
  }

  u32 src_dtype = DT_FP32;
  if (term_dtype_in(v.src[0], 0, &src_dtype) && src_dtype == dtype) {
    return v.src[0];
  }
  if (term_tag(v.src[0]) == TAG_UOP && term_ext(v.src[0]) == UOP_BITCAST) {
    Term inner = heap_read(term_val(v.src[0]));
    return uop_bitcast(inner, dtype);
  }
  if (term_tag(v.src[0]) == TAG_UOP && term_ext(v.src[0]) == UOP_CONST) {
    return uop_bitcast(v.src[0], dtype);
  }
  return 0;
}

fn Term uop_graph_simplify(Term root) {
  UOpGraphRewriteRule rules[] = {
    {"symbolic-unary",          uop_graph_simplify_unary},
    {"symbolic-binary",         uop_graph_simplify_binary},
    {"symbolic-cast",           uop_graph_simplify_cast},
    {"movement-chain-collapse", uop_graph_simplify_movement_chain},
  };
  u32 n_rules = (u32)(sizeof(rules) / sizeof(rules[0]));
  return uop_graph_rewrite(root, rules, n_rules, NULL);
}

static int uop_graph_shape_equal(Shape const *a, Shape const *b) {
  if (a->ndim != b->ndim) {
    return 0;
  }
  for (u32 i = 0; i < a->ndim; i++) {
    if (a->dims[i] != b->dims[i]) {
      return 0;
    }
  }
  return 1;
}

fn Term uop_graph_simplify_checked(Term root, u32 env_id) {
  Term out = uop_graph_simplify(root);
  if (out == root) {
    return out;
  }

  Shape root_shape;
  Shape out_shape;
  if (!term_shape_in(root, env_id, &root_shape)
      || !term_shape_in(out, env_id, &out_shape)
      || !uop_graph_shape_equal(&root_shape, &out_shape)) {
    return root;
  }

  u32 root_dtype;
  u32 out_dtype;
  if (!term_dtype_in(root, env_id, &root_dtype)
      || !term_dtype_in(out, env_id, &out_dtype)
      || root_dtype != out_dtype) {
    return root;
  }

  return out;
}

static int uop_graph_simplify_materialize_enabled(void) {
  static int known = 0;
  static int enabled = 0;
  if (!known) {
    char const *e = getenv("THVM_UOP_GRAPH_SIMPLIFY");
    enabled       = e != NULL && e[0] == '1';
    known         = 1;
  }
  return enabled;
}

fn Term uop_graph_simplify_materialize(Term root, u32 env_id) {
  if (!uop_graph_simplify_materialize_enabled()) {
    return root;
  }
  return uop_graph_simplify_checked(root, env_id);
}
