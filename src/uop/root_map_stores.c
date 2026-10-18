// A kernel root is a STORE or an AFTER chain of STOREs (ordered passes, as a
// scatter's init, accumulate and finish).  uop_arity(AFTER) is 0, so these hand
// a pass every store, in execution order (an AFTER's after_node first).

fn Term uop_root_map_stores(Term root, UopStoreMapFn pass, void *ctx) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_AFTER) {
    return pass(root, ctx);
  }
  Term node  = uop_after_node(root);
  Term after = uop_after_after_node(root);
  Term na = uop_root_map_stores(after, pass, ctx);
  Term nn = uop_root_map_stores(node, pass, ctx);
  if (na == 0 || nn == 0) {
    return 0;
  }
  if (na == after && nn == node) {
    return root;
  }
  return uop_after(nn, na);
}

// Visits every store: no short-circuit.
fn int uop_root_each_store(Term root, UopStorePredFn pass, void *ctx) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_AFTER) {
    return pass(root, ctx);
  }
  int a = uop_root_each_store(uop_after_after_node(root), pass, ctx);
  int b = uop_root_each_store(uop_after_node(root), pass, ctx);
  return a | b;
}

// Every store of a kernel root writes the one output buffer.
fn Term uop_root_store_buf(Term root) {
  while (term_tag(root) == TAG_UOP && term_ext(root) == UOP_AFTER) {
    root = uop_after_node(root);
  }
  return uop_store_buf(root);
}
