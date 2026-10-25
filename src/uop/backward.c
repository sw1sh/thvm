// Tensor.backward's chain rule in one call: mark the `n` leaves `tids` for the
// single walk, fire the chain rule on y seeded by gy, and return each leaf's
// gradient in grads[] (0: none reaches it).  The walk adds into each leaf's
// TenDesc.grad, which starts and ends the call empty, as the caller
// accumulates .grad (tinygrad tensor.py:465-466).  A node the rule cannot
// differentiate fires to zero and leaves a message; then every grads[i] is 0
// and the message is returned.  NULL on success.  The leaves leave unmarked
// either way.
fn char const *uop_backward(Term y, Term gy, u32 const *tids, u32 n, Term *grads) {
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
  wnf(uop_grad(y, gy));
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
  return err;
}
