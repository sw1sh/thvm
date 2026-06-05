# Regression test for auto-grad-all-float-leaves (tinygrad spec).
#
# tinygrad REMOVED requires_grad from Tensor.__init__ + the
# requires_grad_() method; backward() now auto-fills .grad for every
# in-scope non-CONST float leaf, with no flag anywhere.  thvm follows
# the same spec: a plain `Tensor(data)` leaf gets a .grad after a
# backward() that reaches it, and the OPTIMIZER's param list (not a
# flag) decides what updates.
import unittest

import numpy as np

from thvm import Tensor


class TestAutoGradAllFloatLeaves(unittest.TestCase):
  def test_plain_leaf_populates_grad(self):
    # d/dx of sum(x*x) is 2x; x=[2,3] -> grad [4,6].  No marking needed.
    x = Tensor(np.array([2.0, 3.0], dtype=np.float32))
    y = (x * x).sum()
    y.backward()
    self.assertIsNotNone(x.grad, ".grad is None -- auto-grad did not reach leaf")
    np.testing.assert_allclose(x.grad.numpy(), [4.0, 6.0], rtol=1e-5)

  def test_every_reached_leaf_gets_grad(self):
    # Both leaves of a*b are reached -> both get a gradient automatically.
    a = Tensor(np.array([1.5, -2.0, 0.5], dtype=np.float32))
    b = Tensor(np.array([3.0, 4.0, -1.0], dtype=np.float32))
    (a * b).sum().backward()
    self.assertIsNotNone(a.grad)
    self.assertIsNotNone(b.grad)
    # d(a*b)/da = b, d(a*b)/db = a.
    np.testing.assert_allclose(a.grad.numpy(), b.numpy(), rtol=1e-5)
    np.testing.assert_allclose(b.grad.numpy(), a.numpy(), rtol=1e-5)

  def test_out_of_scope_leaf_has_no_grad(self):
    # A leaf NOT reached by the loss term gets no gradient (backward only
    # fills .grad for in-scope leaves -- tinygrad's all_tensors-in-scope
    # filter over the term graph).
    x = Tensor(np.array([2.0, 3.0], dtype=np.float32))
    other = Tensor(np.array([9.0, 9.0], dtype=np.float32))
    (x * x).sum().backward()
    self.assertIsNotNone(x.grad)
    self.assertIsNone(other.grad, "an out-of-scope leaf must stay grad-free")

  def test_is_param_default_and_opt_out(self):
    # is_param defaults True (optimizer-parameter candidate); a buffer
    # opts out via .is_param_(False).  This is the param/buffer split,
    # NOT a grad gate.
    a = Tensor(np.array([1.0], dtype=np.float32))
    b = Tensor(np.array([1.0], dtype=np.float32)).is_param_(False)
    self.assertTrue(a.is_param)
    self.assertFalse(b.is_param)


if __name__ == "__main__":
  unittest.main()
