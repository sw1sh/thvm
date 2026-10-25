""".grad accumulates as tinygrad's backward does (tensor.py:465-466): a second backward adds onto
.grad, and setting .grad to None (or opt.zero_grad()) starts over.  thvm accumulated into the C-side
TenDesc.grad instead, which `.grad = None` never reached, so tinygrad's test_jit_lazy_grad_after_replay
step (backward, conv.weight.grad = None, backward again) grew its gradient by one step's every call.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_grad_accumulate.py -q
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, nn
from thvm.nn import optim


class TestGradAccumulate(unittest.TestCase):
    def test_backward_adds_until_reset(self):
        x = Tensor([2.0, 3.0]).realize()
        (x * x).sum().backward()
        (x * x).sum().backward()
        np.testing.assert_allclose(x.grad.numpy(), [8.0, 12.0])
        x.grad = None
        (x * 5).sum().backward()
        np.testing.assert_allclose(x.grad.numpy(), [5.0, 5.0])

    def test_zero_grad(self):
        w = Tensor([1.0, -1.0]).realize()
        opt = optim.SGD([w], lr=0.0)
        for _ in range(3):
            opt.zero_grad()
            (w * 3).sum().backward()
            np.testing.assert_allclose(w.grad.numpy(), [3.0, 3.0])

    def test_reset_between_two_backwards_each_step(self):
        Tensor.manual_seed(42)
        conv = nn.Conv2d(3, 4, kernel_size=3, padding=1)
        x, y = Tensor.randn(2, 6, 6, 3).realize(), Tensor.randn(2, 4 * 6 * 6).realize()
        got = []
        for _ in range(3):
            loss = (conv(x.permute(0, 3, 1, 2).contiguous()).relu().flatten(1) * y).sum(axis=1)
            loss.sum().backward()
            conv.weight.grad = None
            (loss * 0.5).sum().backward()
            got.append(conv.weight.grad.numpy())
        np.testing.assert_allclose(got[1], got[0], rtol=1e-5, atol=1e-5)
        np.testing.assert_allclose(got[2], got[0], rtol=1e-5, atol=1e-5)


if __name__ == "__main__":
    unittest.main()
