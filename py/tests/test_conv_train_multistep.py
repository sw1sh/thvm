"""Multi-step conv training at the shape class of the conv-backward
second-step trigger.

On the second train step a backward conv contraction re-read a now-realized
pooled activation through the `_pool` col2im window, and the fused reduce
grew to ~6.8e12 iterations (6cc9b6c10).  This trains the two tools/bench_train
models, the 2-conv `simple` net and beautiful_mnist, for four eager Adam steps
on one batch and checks every loss is finite and that training on the fixed
batch lowers it.

    make py && python -m pytest py/tests/test_conv_train_multistep.py
"""
import unittest

import numpy as np

import helpers  # noqa: F401
from thvm import Tensor, nn
from thvm.helpers import Context

BS, STEPS = 2, 4


def _layers(model):
    if model == "simple":
        return [nn.Conv2d(1, 32, 5), Tensor.relu,
                nn.Conv2d(32, 32, 5), Tensor.relu,
                lambda x: x.flatten(1), nn.Linear(32 * 20 * 20, 10)]
    return [nn.Conv2d(1, 32, 5), Tensor.relu, nn.Conv2d(32, 32, 5), Tensor.relu,
            nn.BatchNorm(32, track_running_stats=False), Tensor.max_pool2d,
            nn.Conv2d(32, 64, 3), Tensor.relu, nn.Conv2d(64, 64, 3), Tensor.relu,
            nn.BatchNorm(64, track_running_stats=False), Tensor.max_pool2d,
            lambda x: x.flatten(1), nn.Linear(576, 10)]


def _train(model):
    np.random.seed(7)
    layers = _layers(model)
    opt = nn.optim.Adam(nn.state.get_parameters(layers), lr=1e-2)
    x = Tensor(np.random.randn(BS, 1, 28, 28).astype(np.float32))
    Y = Tensor(np.random.randint(0, 10, BS).astype(np.int32))
    losses = []
    with Context(TRAINING=1):
        for _ in range(STEPS):
            opt.zero_grad()
            loss = x.sequential(layers).sparse_categorical_crossentropy(Y)
            loss.backward()
            opt.step()
            losses.append(float(loss.numpy()))
    return losses


class TestConvTrainMultistep(unittest.TestCase):
    def _check(self, model):
        losses = _train(model)
        self.assertTrue(all(np.isfinite(losses)), f"{model} losses {losses}")
        self.assertLess(min(losses[1:]), losses[0], f"{model} losses {losses}")

    def test_simple(self):
        self._check("simple")

    def test_beautiful(self):
        self._check("beautiful")


if __name__ == "__main__":
    unittest.main()
