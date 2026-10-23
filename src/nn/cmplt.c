// a < b as Tensor.__lt__ builds it: broadcast without promoting, a 0/1 mask in
// a's dtype (thvm has no bool dtype).
fn Tensor nn_cmplt(Tensor a, Tensor b) {
  if (!nn_broadcast(&a, &b)) {
    a.t = 0;
    return a;
  }
  a.t = uop_binary(UOP_CMPLT, a.t, b.t);
  return a;
}
