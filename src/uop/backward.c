// Tensor.backward's leaf protocol (tinygrad tensor.py:475-494) in one call:
// mark the `n` leaves `tids` for the single walk, fire the chain rule on y
// seeded by gy, and return each leaf's .grad in grads[], with had_grad[i] set
// when it held one before.  A node the rule cannot differentiate fires to zero
// and leaves a message; then every .grad is rolled back and the message is
// returned.  NULL on success.  The leaves leave unmarked either way.
fn char const *uop_backward(Term y, Term gy, u32 const *tids, u32 n, Term *grads,
                            u8 *had_grad) {
  grad_err_take();
  for (u32 i = 0; i < n; i++) {
    TenDesc *td = (tids[i] != 0 && tids[i] < TENS_NEXT) ? &TENS[tids[i]] : NULL;
    grads[i]    = td != NULL ? td->grad : 0;
    had_grad[i] = grads[i] != 0;
    if (td != NULL && !td->requires_grad) {
      td->requires_grad = 1;
      grad_req_ncount_inc();
    }
  }
  wnf(uop_grad(y, gy));
  char const *err = grad_err_take();
  for (u32 i = 0; i < n; i++) {
    TenDesc *td = (tids[i] != 0 && tids[i] < TENS_NEXT) ? &TENS[tids[i]] : NULL;
    if (td == NULL) {
      continue;
    }
    if (err != NULL) {
      td->grad = grads[i];
    }
    grads[i] = td->grad;
    if (td->requires_grad) {
      td->requires_grad = 0;
      grad_req_ncount_dec();
    }
  }
  return err;
}
