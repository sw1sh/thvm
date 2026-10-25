// Tensor.backward's chain rule in one call: mark the `n` leaves `tids` for the
// single walk, fire the chain rule on y seeded by gy, and return each leaf's
// gradient in grads[] (0: none reaches it) and each of the `n_nodes` interior
// `nodes`' in node_grads[] (grad_node_note).  The walk adds into each leaf's
// TenDesc.grad, which starts and ends the call empty, as the caller
// accumulates .grad (tinygrad tensor.py:465-466).  A node the rule cannot
// differentiate fires to zero and leaves a message; then every gradient is 0
// and the message is returned.  NULL on success.  The leaves leave unmarked
// either way.
fn char const *uop_backward(Term y, Term gy, u32 const *tids, u32 n, Term *grads,
                            Term const *nodes, u32 n_nodes, Term *node_grads) {
  grad_err_take();
  for (u32 i = 0; i < n; i++) {
    TenDesc *td = (tids[i] != 0 && tids[i] < TENS_NEXT) ? &TENS[tids[i]] : NULL;
    if (td == NULL) {
      continue;
    }
    td->grad = 0;
    if (!td->requires_grad) {
      td->requires_grad = 1;
      grad_req_ncount_inc();
    }
  }
  for (u32 i = 0; i < n_nodes; i++) {
    node_grads[i] = 0;
  }
  GRAD_NODES      = nodes;
  GRAD_NODE_GRADS = node_grads;
  GRAD_NODES_N    = n_nodes;
  wnf(uop_grad(y, gy));
  GRAD_NODES_N    = 0;
  char const *err = grad_err_take();
  for (u32 i = 0; i < n; i++) {
    TenDesc *td = (tids[i] != 0 && tids[i] < TENS_NEXT) ? &TENS[tids[i]] : NULL;
    grads[i] = td != NULL && err == NULL ? td->grad : 0;
    if (td == NULL) {
      continue;
    }
    td->grad = 0;
    if (td->requires_grad) {
      td->requires_grad = 0;
      grad_req_ncount_dec();
    }
  }
  if (err != NULL) {
    for (u32 i = 0; i < n_nodes; i++) {
      node_grads[i] = 0;
    }
  }
  return err;
}
