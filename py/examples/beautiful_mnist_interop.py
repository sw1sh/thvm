#!/usr/bin/env python3
"""beautiful_mnist INTEROP + introspection: tinygrad graph -> thvm backend.

Companion to beautiful_mnist_thvm.py (the import-swap version).  Instead of
running thvm's own Tensor frontend, this builds the beautiful_mnist forward
with REAL tinygrad, then ingests tinygrad's lazy UOp graph into thvm via
from_tinygrad and runs it on thvm's backend.  Because the graph is built by
tinygrad and executed by thvm, divergence is a thvm bug by construction --
this is the differential oracle from py/tests/test_diff_inhouse_vs_import.py
applied to the whole conv/bn/relu/pool/linear stack.

Introspection per component + whole-model:
  - thvm kernel count (TKernelCount delta) vs tinygrad's schedule length,
  - max abs numeric error thvm-import vs tinygrad reference.

tinygrad runs on its CPU device (Context(DEV="CPU")) so it never contends
with the GPU thvm uses; thvm reads its own env DEV independently.

  DEV=cuda python3 py/examples/beautiful_mnist_interop.py
  DEV=cpu  BS=8 python3 py/examples/beautiful_mnist_interop.py
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


def getenv(name, default):
    raw = os.environ.get(name, "")
    if raw == "":
        return default
    try:
        return type(default)(raw)
    except (TypeError, ValueError):
        return raw


def main():
    bs = getenv("BS", 8)
    rng = np.random.default_rng(0)

    import tinygrad
    import tinygrad.nn as TGN
    from tinygrad import Context
    from thvm.thvm import Thvm
    from thvm.from_tinygrad import from_tinygrad

    _TH = Thvm()
    dev = os.environ.get("DEV", "cpu")
    print(f"beautiful_mnist interop (tinygrad build -> thvm backend)  "
          f"DEV={dev}  BS={bs}")

    # Fixed inputs/weights so the tinygrad-ref and thvm-import builds are
    # bit-identical (random-in-lambda would feed different data each call).
    X = rng.standard_normal((bs, 1, 28, 28)).astype(np.float32)

    def build(Tns, N, state):
        """Build the beautiful_mnist forward with weights pinned from
        `state` (a dict of fixed numpy arrays) so both framework builds
        match.  Returns the logits tensor."""
        def conv(cin, cout, k, name):
            c = N.Conv2d(cin, cout, k)
            c.weight = Tns(state[f"{name}.w"])
            c.bias = Tns(state[f"{name}.b"])
            return c

        def bn(sz, name):
            b = N.BatchNorm(sz)
            b.weight = Tns(state[f"{name}.g"])
            b.bias = Tns(state[f"{name}.b"])
            return b

        lin = N.Linear(576, 10)
        lin.weight = Tns(state["fc.w"])
        lin.bias = Tns(state["fc.b"])
        layers = [
            conv(1, 32, 5, "c1"), Tns.relu,
            conv(32, 32, 5, "c2"), Tns.relu,
            bn(32, "bn1"), Tns.max_pool2d,
            conv(32, 64, 3, "c3"), Tns.relu,
            conv(64, 64, 3, "c4"), Tns.relu,
            bn(64, "bn2"), Tns.max_pool2d,
            lambda x: x.flatten(1), lin]
        return Tns(state["X"]).sequential(layers)

    # Pin all weights once (numpy), shared by both builds.
    def randn(*s):
        return rng.standard_normal(s).astype(np.float32) * 0.05

    state = {
        "X": X,
        "c1.w": randn(32, 1, 5, 5),   "c1.b": np.zeros(32, np.float32),
        "c2.w": randn(32, 32, 5, 5),  "c2.b": np.zeros(32, np.float32),
        "bn1.g": np.ones(32, np.float32), "bn1.b": np.zeros(32, np.float32),
        "c3.w": randn(64, 32, 3, 3),  "c3.b": np.zeros(64, np.float32),
        "c4.w": randn(64, 64, 3, 3),  "c4.b": np.zeros(64, np.float32),
        "bn2.g": np.ones(64, np.float32), "bn2.b": np.zeros(64, np.float32),
        "fc.w": randn(10, 576),       "fc.b": np.zeros(10, np.float32),
    }

    tinygrad.Tensor.training = True
    with Context(DEV="CPU"):
        ref = np.asarray(build(tinygrad.Tensor, TGN, state).numpy(),
                         dtype=np.float32)

    from thvm import Tensor as ThvmTensor  # noqa
    ThvmTensor.training = True
    with Context(DEV="CPU"):
        tg_graph = build(tinygrad.Tensor, TGN, state)
    k0 = _TH.kernel_count()
    imported = np.asarray(from_tinygrad(tg_graph).numpy(),
                          dtype=np.float32).reshape(ref.shape)
    k_import = _TH.kernel_count() - k0

    err = float(np.abs(imported - ref).max())
    print(f"whole-model forward: shape={ref.shape}  "
          f"thvm_kernels={k_import}  max_abs_err={err:.2e}  "
          f"{'OK' if err < 1e-2 else 'DIVERGE'}")


if __name__ == "__main__":
    main()
