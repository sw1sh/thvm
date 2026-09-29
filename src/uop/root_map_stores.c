// uop/root_map_stores.c - run a per-store pass over every STORE of a kernel
// root.
//
// A kernel root is one STORE, or an AFTER chain of STOREs for a kernel that
// writes its output in several ordered passes (a scatter's init, accumulate
// and finish stores).  uop_arity(UOP_AFTER) stays 0 because the
// devectorizer's AFTER chains rely on it, so a pass that walks by arity
// never looks inside an AFTER root.  These helpers hand such a pass each
// STORE instead, in execution order (an AFTER's after_node first).  A root
// that is not an AFTER is its own single store.
//
//   uop_root_map_stores  rebuild the root with `pass` applied to each store
//   uop_root_each_store  OR of `pass` over the stores (every store is visited)
//   uop_root_store_buf   the buffer the root's stores write

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
