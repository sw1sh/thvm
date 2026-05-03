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

  if (v.op == UOP_FLIP) {
    if (term_tag(v.src[0]) != TAG_UOP || term_ext(v.src[0]) != UOP_FLIP) {
      return 0;
    }
    u64 inner_loc = term_val(v.src[0]);
    Term outer_axes = heap_read(v.loc + 1);
    Term inner_axes = heap_read(inner_loc + 1);
    if (term_tag(outer_axes) != TAG_NUM || term_tag(inner_axes) != TAG_NUM) {
      return 0;
    }
    u32 axes = (u32)term_val(outer_axes) ^ (u32)term_val(inner_axes);
    Term inner_src = heap_read(inner_loc);
    return axes == 0 ? inner_src : uop_flip(inner_src, axes);
  }

  u32 ndim = (u32)term_val(heap_read(v.loc + 1));
  if (ndim > MAX_DIM) {
    return 0;
  }

  if (v.op == UOP_RESHAPE || v.op == UOP_EXPAND) {
    Term collapsed = uop_rewrite_movement_src(v.op, v.src[0]);
    if (collapsed == 0) {
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

  if (v.op == UOP_PAD) {
    if (term_tag(v.src[0]) != TAG_UOP || term_ext(v.src[0]) != UOP_PAD) {
      return 0;
    }
    u64 inner_loc = term_val(v.src[0]);
    Term inner_ndim = heap_read(inner_loc + 1);
    if (term_tag(inner_ndim) != TAG_NUM || term_val(inner_ndim) != ndim) {
      return 0;
    }
    u32 widths[2 * MAX_DIM];
    for (u32 i = 0; i < 2 * ndim; i++) {
      widths[i] = (u32)term_val(heap_read(inner_loc + 2 + i))
                + (u32)term_val(heap_read(v.loc + 2 + i));
    }
    return uop_pad(heap_read(inner_loc), ndim, widths);
  }

  if (v.op == UOP_SHRINK) {
    if (term_tag(v.src[0]) != TAG_UOP || term_ext(v.src[0]) != UOP_SHRINK) {
      return 0;
    }
    u64 inner_loc = term_val(v.src[0]);
    Term inner_ndim = heap_read(inner_loc + 1);
    if (term_tag(inner_ndim) != TAG_NUM || term_val(inner_ndim) != ndim) {
      return 0;
    }
    u32 bounds[2 * MAX_DIM];
    for (u32 i = 0; i < ndim; i++) {
      u32 inner_begin = (u32)term_val(heap_read(inner_loc + 2 + 2 * i));
      u32 outer_begin = (u32)term_val(heap_read(v.loc + 2 + 2 * i));
      u32 outer_end   = (u32)term_val(heap_read(v.loc + 3 + 2 * i));
      bounds[2 * i]     = inner_begin + outer_begin;
      bounds[2 * i + 1] = inner_begin + outer_end;
    }
    return uop_shrink(heap_read(inner_loc), ndim, bounds);
  }

  return 0;
}

static int uop_graph_simplify_commutative_op(u8 op) {
  return op == UOP_ADD || op == UOP_MUL || op == UOP_CMPEQ;
}

static int uop_graph_simplify_sort_after(Term a, Term b) {
  int a_const = term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST;
  int b_const = term_tag(b) == TAG_UOP && term_ext(b) == UOP_CONST;
  if (a_const != b_const) {
    return a_const;
  }
  return a > b;
}

static Term uop_graph_simplify_commutative(Term t, void *user) {
  (void)user;
  UOpView v;
  if (!uop_view(t, &v) || !uop_graph_simplify_commutative_op(v.op)) {
    return 0;
  }
  if (!uop_graph_simplify_sort_after(v.src[0], v.src[1])) {
    return 0;
  }
  return uop_binary(v.op, v.src[1], v.src[0]);
}

static int uop_graph_simplify_const_f32(Term t, f32 *out) {
  if (out == NULL || term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) {
    return 0;
  }
  Term num = heap_read(term_val(t));
  if (term_tag(num) != TAG_NUM || term_ext(num) != DT_FP32) {
    return 0;
  }
  u32 bits = (u32)term_val(num);
  memcpy(out, &bits, sizeof(*out));
  return 1;
}

static int uop_graph_simplify_split_const(Term a,
                                           Term b,
                                           Term *expr,
                                           f32 *c) {
  if (uop_graph_simplify_const_f32(a, c)) {
    *expr = b;
    return 1;
  }
  if (uop_graph_simplify_const_f32(b, c)) {
    *expr = a;
    return 1;
  }
  return 0;
}

static Term uop_graph_simplify_reassociate_const(Term t, void *user) {
  (void)user;
  UOpView v;
  if (!uop_view(t, &v) || (v.op != UOP_ADD && v.op != UOP_MUL)) {
    return 0;
  }

  Term outer_expr = 0;
  f32 outer_const = 0.0f;
  if (!uop_graph_simplify_split_const(v.src[0], v.src[1],
                                      &outer_expr, &outer_const)) {
    return 0;
  }
  UOpView inner;
  if (!uop_view(outer_expr, &inner) || inner.op != v.op) {
    return 0;
  }
  Term inner_expr = 0;
  f32 inner_const = 0.0f;
  if (!uop_graph_simplify_split_const(inner.src[0], inner.src[1],
                                      &inner_expr, &inner_const)) {
    return 0;
  }

  f32 folded = v.op == UOP_ADD ? inner_const + outer_const
                               : inner_const * outer_const;
  return uop_binary(v.op, inner_expr, uop_const(DT_FP32, f32_bits(folded)));
}

static int uop_graph_simplify_dims_match_shape(u64 loc,
                                               u32 ndim,
                                               Shape const *shape) {
  if (ndim != shape->ndim || ndim > MAX_DIM) {
    return 0;
  }
  for (u32 i = 0; i < ndim; i++) {
    u32 dim = (u32)term_val(heap_read(loc + 2 + i));
    if (dim != shape->dims[i]) {
      return 0;
    }
  }
  return 1;
}

static Term uop_graph_simplify_movement_identity(Term t, void *user) {
  (void)user;
  UOpView v;
  if (!uop_view(t, &v) || !uop_is_movement(v.op)) {
    return 0;
  }

  if (v.op == UOP_FLIP) {
    Term axes = heap_read(v.loc + 1);
    if (term_tag(axes) == TAG_NUM && term_val(axes) == 0) {
      return v.src[0];
    }
    return 0;
  }

  Term ndim_term = heap_read(v.loc + 1);
  if (term_tag(ndim_term) != TAG_NUM) {
    return 0;
  }
  u32 ndim = (u32)term_val(ndim_term);
  if (ndim > MAX_DIM) {
    return 0;
  }

  if (v.op == UOP_PERMUTE) {
    for (u32 i = 0; i < ndim; i++) {
      u32 axis = (u32)term_val(heap_read(v.loc + 2 + i));
      if (axis != i) {
        return 0;
      }
    }
    return v.src[0];
  }

  if (v.op == UOP_PAD) {
    for (u32 i = 0; i < 2 * ndim; i++) {
      if (term_val(heap_read(v.loc + 2 + i)) != 0) {
        return 0;
      }
    }
    return v.src[0];
  }

  Shape src_shape;
  if (!term_shape_in(v.src[0], 0, &src_shape)) {
    return 0;
  }

  if (v.op == UOP_RESHAPE || v.op == UOP_EXPAND) {
    if (uop_graph_simplify_dims_match_shape(v.loc, ndim, &src_shape)) {
      return v.src[0];
    }
    return 0;
  }

  if (v.op == UOP_SHRINK) {
    if (ndim != src_shape.ndim) {
      return 0;
    }
    for (u32 i = 0; i < ndim; i++) {
      u32 begin = (u32)term_val(heap_read(v.loc + 2 + 2 * i));
      u32 end   = (u32)term_val(heap_read(v.loc + 3 + 2 * i));
      if (begin != 0 || end != src_shape.dims[i]) {
        return 0;
      }
    }
    return v.src[0];
  }

  return 0;
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
    {"commutative-canonicalize", uop_graph_simplify_commutative},
    {"symbolic-reassociate-const", uop_graph_simplify_reassociate_const},
    {"movement-identity",       uop_graph_simplify_movement_identity},
    {"movement-chain-collapse", uop_graph_simplify_movement_chain},
  };
  u32 n_rules = (u32)(sizeof(rules) / sizeof(rules[0]));
  return uop_graph_rewrite(root, rules, n_rules, NULL);
}

static Term uop_graph_simplify_materialize_subset(Term root) {
  UOpGraphRewriteRule rules[] = {
    {"symbolic-unary",          uop_graph_simplify_unary},
    {"symbolic-binary",         uop_graph_simplify_binary},
    {"symbolic-cast",           uop_graph_simplify_cast},
    {"symbolic-reassociate-const", uop_graph_simplify_reassociate_const},
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

static Term uop_graph_simplify_materialize_checked(Term root, u32 env_id) {
  Term out = uop_graph_simplify_materialize_subset(root);
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
  return uop_graph_simplify_materialize_checked(root, env_id);
}
