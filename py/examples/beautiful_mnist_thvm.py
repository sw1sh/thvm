#!/usr/bin/env python3
"""beautiful_mnist on thvm -- import-swapped from tinygrad's beautiful_mnist.py.

The Model and training loop mirror tinygrad/examples/beautiful_mnist.py.
The thvm py frontend is a drop-in `Tensor`/`nn` but lacks a few tinygrad
conveniences (optim.Adam, Tensor.sparse_categorical_crossentropy,
Tensor.argmax, randint / fancy-index, the nn.datasets.mnist loader), so
this file supplies thin thvm-native equivalents up top -- everything below
"=== Model ===" reads the same as the tinygrad original.

Introspection per step: kernel count (TKernelCount delta), wall time, peak
GPU memory (nvidia-smi when DEV=cuda), loss, and periodic test accuracy.
BatchNorm is evaluated in train-mode (batch statistics) for both train and
eval -- thvm's nn.BatchNorm does not update running stats, matching the
validated WL harness wl/Examples/beautiful-mnist/train.wls.

  DEV=cuda STEPS=70 BS=512 python3 py/examples/beautiful_mnist_thvm.py
  DEV=cpu  STEPS=3  BS=64  MNIST_LIMIT=2048 \
      python3 py/examples/beautiful_mnist_thvm.py        # quick smoke
"""
from __future__ import annotations

import math
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from thvm import Tensor, nn          # noqa: E402  (drop-in for tinygrad)
from thvm.thvm import Thvm           # noqa: E402

_TH = Thvm()


def getenv(name: str, default):
    raw = os.environ.get(name, "")
    if raw == "":
        return default
    try:
        return type(default)(raw)
    except (TypeError, ValueError):
        return raw


# === thvm-native fills for the tinygrad surface this script uses ======

def log_softmax(x: Tensor, axis: int = 1) -> Tensor:
    # max-subtract for numerical stability (REDUCE_MAX backward is the
    # mask-at-argmax rule in uop_grad, so grad flows correctly).
    m = x.max(axis=axis, keepdim=True)
    shifted = x - m
    lse = shifted.exp().sum(axis=axis, keepdim=True).log()
    return shifted - lse


def sparse_categorical_crossentropy(logits: Tensor, labels_np: np.ndarray,
                                    n_classes: int) -> Tensor:
    # one-hot the int labels host-side (constant, no grad), then
    # loss = mean( -sum_c onehot * log_softmax(logits) ).
    bs = logits.shape[0]
    oh = np.zeros((bs, n_classes), dtype=np.float32)
    oh[np.arange(bs), labels_np.astype(np.int64)] = 1.0
    ls = log_softmax(logits, axis=1)
    return (ls * Tensor(oh)).sum(axis=1).mean() * -1.0


class Adam:
    """Adam optimizer composed from thvm Tensor ops + assign (eager)."""

    def __init__(self, params, lr=1e-3, b1=0.9, b2=0.999, eps=1e-8):
        self.params = list(params)
        self.lr, self.b1, self.b2, self.eps = lr, b1, b2, eps
        # Params must be realized TAG_TEN leaves for requires_grad/backward
        # to register them (Tensor.uniform inits are unrealized UOP graphs;
        # ten_set_requires_grad no-ops on a non-leaf term).
        for p in self.params:
            p.realize()
            p.requires_grad_()
        self.m = [Tensor.zeros(*p.shape) if p.shape else Tensor.zeros(1)
                  for p in self.params]
        self.v = [Tensor.zeros(*p.shape) if p.shape else Tensor.zeros(1)
                  for p in self.params]
        self.t = 0

    def step(self):
        self.t += 1
        bc1 = 1.0 - self.b1 ** self.t
        bc2 = 1.0 - self.b2 ** self.t
        for i, p in enumerate(self.params):
            g = p.grad
            if g is None:
                continue
            g = g.reshape(*p.shape) if p.shape else g
            m = self.m[i] * self.b1 + g * (1.0 - self.b1)
            v = self.v[i] * self.b2 + (g * g) * (1.0 - self.b2)
            m.realize(); v.realize()
            self.m[i], self.v[i] = m, v
            mhat = m * (1.0 / bc1)
            vhat = v * (1.0 / bc2)
            upd = p - mhat * self.lr * (vhat.sqrt() + self.eps).reciprocal()
            p.assign(upd)        # realizes upd into p.term (fresh TAG_TEN)
            p.grad = None        # clear stale cotangent
            p.requires_grad_()   # re-register the new leaf for next backward


def argmax_np(logits: Tensor) -> np.ndarray:
    return np.argmax(logits.numpy(), axis=1)


def load_mnist(limit):
    """Return (Xtr, Ytr, Xte, Yte) as numpy.  Uses tinygrad's loader for
    the fetch only (wrapped to its CPU device so it never touches the GPU
    thvm is using), then pulls straight to host arrays."""
    try:
        from tinygrad import Context
        from tinygrad.nn.datasets import mnist
        with Context(DEV="CPU"):
            Xtr, Ytr, Xte, Yte = mnist()
            Xtr, Ytr = Xtr.numpy(), Ytr.numpy()
            Xte, Yte = Xte.numpy(), Yte.numpy()
    except Exception as e:  # pragma: no cover - fallback path
        raise SystemExit(f"could not load MNIST via tinygrad: {e}")
    Xtr = Xtr.astype(np.float32).reshape(-1, 1, 28, 28) / 255.0
    Xte = Xte.astype(np.float32).reshape(-1, 1, 28, 28) / 255.0
    Ytr = Ytr.astype(np.int64).reshape(-1)
    Yte = Yte.astype(np.int64).reshape(-1)
    if limit not in ("All", "all", None):
        n = int(limit)
        Xtr, Ytr, Xte, Yte = Xtr[:n], Ytr[:n], Xte[:n], Yte[:n]
    return Xtr, Ytr, Xte, Yte


def gpu_mem_mb():
    if not _is_cuda():
        return 0.0
    try:
        out = subprocess.check_output(
            ["nvidia-smi", "--query-gpu=memory.used",
             "--format=csv,noheader,nounits"], timeout=10)
        return float(out.decode().splitlines()[0].strip())
    except Exception:
        return 0.0


def _is_cuda():
    return os.environ.get("DEV", "").lower() in ("cuda", "gpu")


# === Model (identical in spirit to tinygrad's beautiful_mnist.py) =====

class Model:
    def __init__(self):
        self.layers = [
            nn.Conv2d(1, 32, 5), Tensor.relu,
            nn.Conv2d(32, 32, 5), Tensor.relu,
            nn.BatchNorm(32), Tensor.max_pool2d,
            nn.Conv2d(32, 64, 3), Tensor.relu,
            nn.Conv2d(64, 64, 3), Tensor.relu,
            nn.BatchNorm(64), Tensor.max_pool2d,
            lambda x: x.flatten(1), nn.Linear(576, 10)]

    def __call__(self, x: Tensor) -> Tensor:
        return x.sequential(self.layers)


def main():
    bs = getenv("BS", 512)
    steps = getenv("STEPS", 70)
    test_every = getenv("TEST_EVERY", 10)
    mnist_limit = os.environ.get("MNIST_LIMIT", "All")
    np.random.seed(42)

    dev = os.environ.get("DEV", "cpu")
    print(f"beautiful_mnist on thvm  DEV={dev}  BS={bs}  STEPS={steps}  "
          f"TEST_EVERY={test_every}  MNIST_LIMIT={mnist_limit}")

    t0 = time.time()
    Xtr, Ytr, Xte, Yte = load_mnist(mnist_limit)
    print(f"MNIST: train={len(Xtr)} test={len(Xte)} "
          f"({(time.time() - t0) * 1e3:.0f}ms)")

    model = Model()
    params = nn.state.get_parameters(model.layers)
    opt = Adam(params, lr=getenv("LR", 1e-3))
    nparams = sum(int(np.prod(p.shape)) if p.shape else 1 for p in params)
    print(f"params: {len(params)} tensors, {nparams} scalars")

    Tensor.training = True

    eval_limit = getenv("EVAL_LIMIT", len(Xte))

    def test_acc():
        correct, total = 0, 0
        for b0 in range(0, min(len(Xte), eval_limit), bs):
            xb = Xte[b0:b0 + bs]
            yb = Yte[b0:b0 + bs]
            if len(xb) < bs:  # pad trailing partial batch
                pad = bs - len(xb)
                xb = np.concatenate([xb, np.zeros((pad, 1, 28, 28), np.float32)])
            pred = argmax_np(model(Tensor(xb)))[:len(yb)]
            correct += int((pred == yb).sum())
            total += len(yb)
        return 100.0 * correct / total

    wall = []
    peak_mem = 0.0
    acc = float("nan")
    for step in range(1, steps + 1):
        k_before = _TH.kernel_count()
        ts = time.time()
        idx = np.random.randint(0, len(Xtr), size=bs)
        xb = Tensor(Xtr[idx])
        yb = Ytr[idx]
        logits = model(xb)
        loss = sparse_categorical_crossentropy(logits, yb, 10)
        # backward() must see the lazy graph -- realizing first collapses
        # loss to a leaf and there is nothing left to differentiate.
        loss.backward()
        loss.realize()
        opt.step()
        dt = (time.time() - ts) * 1e3
        wall.append(dt)
        k_new = _TH.kernel_count() - k_before
        mem = gpu_mem_mb()
        peak_mem = max(peak_mem, mem)
        tag = "cold" if step == 1 else "warm"
        msg = (f"{tag} step {step:3d}: loss={loss.item():7.3f}  "
               f"wall={dt:8.1f}ms  kernels(new)={k_new}  mem={mem:.0f}MB")
        if test_every and step % test_every == 0:
            acc = test_acc()
            msg += f"  test_acc={acc:5.2f}%"
        print(msg)

    if not test_every:
        acc = test_acc()
    warm = wall[1:] if len(wall) > 1 else wall
    print(f"\nSUMMARY  final_test_acc={acc:.2f}%  "
          f"cold={wall[0]:.1f}ms  warm_mean={np.mean(warm):.1f}ms  "
          f"warm_min={np.min(warm):.1f}ms  peak_gpu_mem={peak_mem:.0f}MB  "
          f"throughput={bs * 1000.0 / np.mean(warm):.0f} img/s")


if __name__ == "__main__":
    main()
