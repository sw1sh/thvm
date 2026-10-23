// x where cond is nonzero, else y (tinygrad mixin/elementwise.py:426-438): a true
// select, so a non-finite unselected value never leaks in.  x and y promote as a
// pair, then broadcast against cond in Tensor.where's order: cond with x, cond
// with y, and x against that cond, whose own rewrite is dropped.
fn Tensor nn_where(Tensor cond, Tensor x, Tensor y) {
  if (x.dtype != y.dtype) {
    x = nn_cast(x, DT_FP32);
    y = nn_cast(y, DT_FP32);
  }
  Tensor c = cond;
  int ok = nn_broadcast(&c, &x) && nn_broadcast(&c, &y);
  Tensor dropped = c;
  if (!ok || !nn_broadcast(&x, &dropped)) {
    x.t = 0;
    return x;
  }
  x.t = uop_where(c.t, x.t, y.t);
  return x;
}
