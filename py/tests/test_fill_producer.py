"""A realized lazy constant is a buffer, not its fill kernel's output: the kernel GC took a kernel without
inputs for an already stripped one and never detached it, so on CUDA every later pass re-fired the fill
over each assign into the buffer, and SGD's momentum (Tensor.zeros realized in __init__, assigned each
step) and Adam's moments reset to zero every step.

    make py && PYTHONPATH=py python -m pytest py/tests/test_fill_producer.py -q   # DEV=cpu or DEV=cuda
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Context, GlobalCounters, Tensor
from thvm.nn import optim


class TestFillProducer(unittest.TestCase):
    def test_assign_into_a_realized_zeros_survives_a_joint_realize(self):
        b = Tensor.zeros(3)
        b.realize()
        g, p = Tensor([0.1, 0.2, 0.3]).realize(), Tensor([1.0, -1.0, 2.0]).realize()
        GlobalCounters.reset()
        b.assign(b * 0.9 + g)
        p.assign(p - b * 0.1)
        p.realize(b)
        np.testing.assert_allclose(b.numpy(), [0.1, 0.2, 0.3], rtol=1e-6)
        np.testing.assert_allclose(p.numpy(), [0.99, -1.02, 1.97], rtol=1e-6)
        self.assertLessEqual(GlobalCounters.kernel_count, 3)

    def test_sgd_momentum_moves(self):
        p = Tensor([1.0, -1.0, 2.0]).realize()
        opt = optim.SGD([p], lr=0.1, momentum=0.9)
        with Context(TRAINING=1):
            for _ in range(2):
                opt.zero_grad()
                (p * p).sum().backward()
                opt.step()
        np.testing.assert_allclose(p.numpy(), [0.46, -0.46, 0.92], rtol=1e-5)


if __name__ == "__main__":
    unittest.main()
