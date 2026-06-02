# Regression test for the requires_grad-in-constructor footgun.
#
# tinygrad semantics: `Tensor(data, requires_grad=True)` is sufficient to
# make a leaf trainable -- after a backward() the leaf's .grad is populated.
# thvm's Tensor.__init__ used to set self.requires_grad but never wired the
# C-side TenDesc.requires_grad flag or the tid->Tensor routing dict, so only
# the explicit .requires_grad_() path worked; the constructor flag left
# .grad == None after backward.  This poisoned several gap-discovery repros
# into false zero-grad blockers.
import unittest

import numpy as np

from thvm import Tensor


class TestRequiresGradConstructor(unittest.TestCase):
  def test_constructor_flag_populates_grad(self):
    # d/dx of sum(x*x) is 2x; x=[2,3] -> grad [4,6].
    x = Tensor(np.array([2.0, 3.0], dtype=np.float32), requires_grad=True)
    y = (x * x).sum()
    y.backward()
    self.assertIsNotNone(x.grad, ".grad is None -- constructor flag not wired")
    np.testing.assert_allclose(x.grad.numpy(), [4.0, 6.0], rtol=1e-5)

  def test_constructor_matches_requires_grad_method(self):
    # The constructor flag and the explicit .requires_grad_() must agree.
    data = np.array([1.5, -2.0, 0.5], dtype=np.float32)
    xc = Tensor(data.copy(), requires_grad=True)
    (xc * xc).sum().backward()
    xm = Tensor(data.copy()).requires_grad_(True)
    (xm * xm).sum().backward()
    self.assertIsNotNone(xc.grad)
    self.assertIsNotNone(xm.grad)
    np.testing.assert_allclose(xc.grad.numpy(), xm.grad.numpy(), rtol=1e-5)

  def test_graph_result_requires_grad_does_not_crash(self):
    # A non-leaf (graph-result) term carries a non-TAG_TEN term; asking for
    # requires_grad must safely no-op (C-side TAG_TEN guard + tid>0 guard),
    # not crash.  We pass requires_grad=True via the constructor copy-path.
    base = Tensor(np.array([1.0, 2.0], dtype=np.float32))
    gr = base + Tensor(np.array([3.0, 4.0], dtype=np.float32))
    # Re-wrap the graph-result into a fresh Tensor with the constructor flag.
    wrapped = Tensor(gr, requires_grad=True)  # must not raise
    self.assertTrue(wrapped.requires_grad)

  def test_none_and_false_preserved(self):
    # The None ("unset", optimizer promotes) vs explicit False (BatchNorm
    # running stats stay non-trained) distinction must survive the new wiring.
    a = Tensor(np.array([1.0], dtype=np.float32))
    b = Tensor(np.array([1.0], dtype=np.float32), requires_grad=False)
    self.assertIsNone(a.requires_grad)
    self.assertFalse(b.requires_grad)


if __name__ == "__main__":
  unittest.main()
