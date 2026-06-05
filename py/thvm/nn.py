"""Phase 3A of py_tensor_frontend.md -- tinygrad-shaped nn layers.

Each layer is a composition of Tensor ops + factories (weight init via
Tensor.uniform / .ones / .zeros).  Phase 3A scope:

  Linear     -- y = x @ W^T + b
  BatchNorm  -- (x - mean) * invstd; running stats in inference mode
  LayerNorm  -- normalize over last `normalized_shape` dims
  Conv2d     -- delegates to Tensor.conv2d (Phase-3A host-side cheat)

state.get_parameters(obj) walks lists/tuples/dicts/objects, collecting
every Tensor attribute (incl. requires_grad=False running stats, as
tinygrad does -- tests replace ALL params via .replace).

Phase 3B will:
  - move conv2d / max_pool2d onto the composed reshape+expand+mul+sum
    unfold path (so they fuse into thvm kernels);
  - wire backward() through requires_grad bookkeeping;
  - add optim.SGD + optim.Adam.
"""
from __future__ import annotations

import math
from typing import Any

from .tensor import Tensor


# ---------------- layers --------------------------------------------------


class Linear:
    """y = x @ W^T + b.  Mirrors tinygrad's nn.Linear (weight stored as
    (out, in); transposed at call time)."""

    def __init__(self, in_features: int, out_features: int,
                 bias: bool = True):
        bound = 1.0 / math.sqrt(in_features)
        self.weight = Tensor.uniform(out_features, in_features,
                                     low=-bound, high=bound)
        self.bias = (Tensor.uniform(out_features, low=-bound, high=bound)
                     if bias else None)

    def __call__(self, x: Tensor) -> Tensor:
        return x.linear(self.weight.transpose(), self.bias)


class Conv2d:
    """2D convolution over (B, C_in, H, W) -> (B, C_out, H', W').

    Phase 3A delegates to Tensor.conv2d, which currently runs on the
    host via numpy.  The forward results are correct; gradient flow
    waits on the composed Phase-3B path.
    """

    def __init__(self, in_channels: int, out_channels: int,
                 kernel_size, stride=1, padding=0, dilation=1,
                 groups: int = 1, bias: bool = True):
        if isinstance(kernel_size, int):
            kernel_size = (kernel_size, kernel_size)
        self.kernel_size = tuple(kernel_size)
        self.stride = stride
        self.padding = padding
        self.dilation = dilation
        self.groups = groups
        scale = 1.0 / math.sqrt(in_channels * self.kernel_size[0]
                                * self.kernel_size[1])
        self.weight = Tensor.uniform(out_channels, in_channels // groups,
                                     *self.kernel_size,
                                     low=-scale, high=scale)
        self.bias = (Tensor.uniform(out_channels, low=-scale, high=scale)
                     if bias else None)

    def __call__(self, x: Tensor) -> Tensor:
        return x.conv2d(self.weight, self.bias, self.groups,
                        self.stride, self.dilation, self.padding)


class BatchNorm:
    """Batch normalization (2D / 3D / 4D input)."""

    def __init__(self, sz: int, eps: float = 1e-5, affine: bool = True,
                 track_running_stats: bool = True,
                 momentum: float = 0.1):
        self.eps = eps
        self.track_running_stats = track_running_stats
        self.momentum = momentum
        self.weight = Tensor.ones(sz) if affine else None
        self.bias = Tensor.zeros(sz) if affine else None
        if track_running_stats:
            self.running_mean = Tensor.zeros(sz, requires_grad=False)
            self.running_var = Tensor.ones(sz, requires_grad=False)
        self.num_batches_tracked = Tensor.zeros(requires_grad=False)

    def calc_stats(self, x: Tensor) -> tuple[Tensor, Tensor]:
        if self.track_running_stats and not Tensor.training:
            return self.running_mean, self.running_var
        # batch mean over all axes except feature axis (axis 1).
        reduce_axes = tuple(i for i in range(x.ndim) if i != 1)
        batch_mean = x.mean(axis=reduce_axes)
        shape_mask = [1, -1] + [1] * (x.ndim - 2)
        # detach the mean here: d(var)/d(mean) == 0 at mean == E[x], so
        # detaching gives the identical gradient with a far smaller
        # backward graph (otherwise the variance grad flows back through
        # the mean, compounding through every upstream layer).  Mirrors
        # tinygrad BatchNorm.calc_stats.
        y = x - batch_mean.detach().reshape(*shape_mask)
        batch_var = (y * y).mean(axis=reduce_axes)
        return batch_mean, batch_var

    def __call__(self, x: Tensor) -> Tensor:
        mean, var = self.calc_stats(x)
        # Update running stats in training (mirrors tinygrad) so eval --
        # which reads running_mean/var with Tensor.training=False -- is
        # correct.  detach() keeps the update off the autograd path.
        if self.track_running_stats and Tensor.training:
            n = x.numel()
            corr = n / (n - x.shape[1]) if n > x.shape[1] else 1.0
            # Realize the running-stat assigns IMMEDIATELY rather than
            # letting them accumulate as an ASSIGN chain on .term.  The
            # bench doesn't include running_mean / running_var in its
            # per-step realize bundle (Adam.schedule_step skips them: no
            # grad means no opt update fires), so without an explicit
            # realize here each training step nests another ASSIGN UOp
            # over the prior term.  By eval time the chain references
            # the entire training-mode forward graph, which materialize
            # then tries to re-execute -- locks up in the eval-mode
            # forward read because the ASSIGN sub-DAG cross-references
            # the same buffers the eval reads.  Tinygrad sidesteps this
            # via its lazybuffer model (assign returns a tensor backed
            # by the same buffer; .realized state lets readers stop the
            # walk at the buffer), thvm's eager assign needs the
            # explicit realize.  Cost: 2 small assigns per BN per step.
            Tensor.realize(self.running_mean.assign(
                self.running_mean * (1.0 - self.momentum)
                + mean.detach() * self.momentum))
            Tensor.realize(self.running_var.assign(
                self.running_var * (1.0 - self.momentum)
                + var.detach() * (self.momentum * corr)))
        invstd = (var + self.eps).rsqrt()
        return x.batchnorm(self.weight, self.bias, mean, invstd)


# tinygrad-style aliases
BatchNorm2d = BatchNorm
BatchNorm3d = BatchNorm


class LayerNorm:
    """Normalize over the last `normalized_shape` dims."""

    def __init__(self, normalized_shape,
                 eps: float = 1e-5,
                 elementwise_affine: bool = True):
        if isinstance(normalized_shape, int):
            normalized_shape = (normalized_shape,)
        self.normalized_shape = tuple(normalized_shape)
        self.axis = tuple(-1 - i for i in range(len(self.normalized_shape)))
        self.eps = eps
        if elementwise_affine:
            self.weight = Tensor.ones(*self.normalized_shape)
            self.bias = Tensor.zeros(*self.normalized_shape)
        else:
            self.weight = None
            self.bias = None

    def __call__(self, x: Tensor) -> Tensor:
        x = x.layernorm(axis=self.axis, eps=self.eps)
        if self.weight is None or self.bias is None:
            return x
        return x * self.weight + self.bias


class LayerNorm2d(LayerNorm):
    """LayerNorm over channel-last with channels-first storage."""
    def __call__(self, x: Tensor) -> Tensor:
        # (B, C, H, W) -> permute to (B, H, W, C) for layernorm, then back
        return super().__call__(x.permute(0, 2, 3, 1)).permute(0, 3, 1, 2)


class Embedding:
    """Lookup table: maps integer indices to rows of a (vocab, embed)
    weight.  Mirrors tinygrad's nn.Embedding -- forward gathers the rows
    selected by `idx` and reshapes to (*idx.shape, embed_size)."""

    def __init__(self, vocab_size: int, embed_size: int):
        self.vocab_size = vocab_size
        self.embed_size = embed_size
        # tinygrad uses glorot_uniform; the checkpoint replaces this anyway.
        bound = math.sqrt(6.0 / (vocab_size + embed_size))
        self.weight = Tensor.uniform(vocab_size, embed_size,
                                     low=-bound, high=bound)

    def __call__(self, idx: Tensor) -> Tensor:
        return self.weight.embedding(idx)


# ---------------- state helpers (port of tinygrad/nn/state.get_parameters)


def _get_state_dict(obj, prefix: str = "") -> dict[str, Tensor]:
    """Recursively walk an object, collecting Tensors keyed by dotted path."""
    out: dict[str, Tensor] = {}
    if isinstance(obj, Tensor):
        out[prefix.rstrip(".")] = obj
    elif isinstance(obj, (list, tuple)):
        for i, x in enumerate(obj):
            out.update(_get_state_dict(x, f"{prefix}{i}."))
    elif isinstance(obj, dict):
        for k, v in obj.items():
            out.update(_get_state_dict(v, f"{prefix}{k}."))
    elif hasattr(obj, "__dict__"):
        for k, v in obj.__dict__.items():
            out.update(_get_state_dict(v, f"{prefix}{k}."))
    return out


class _State:
    @staticmethod
    def get_parameters(obj) -> list[Tensor]:
        """tinygrad-compatible: returns ALL Tensors in the object tree
        (incl. requires_grad=False BN running stats)."""
        return list(_get_state_dict(obj).values())

    @staticmethod
    def get_state_dict(obj) -> dict[str, Tensor]:
        return _get_state_dict(obj)


state = _State()

# tinygrad-shaped submodules: nn.optim.Adam / nn.datasets.mnist.
# Register under the dotted nn.* names too so `from thvm.nn.datasets
# import mnist` resolves even though nn is a module (not a package).
import sys as _sys   # noqa: E402
from . import optim   # noqa: E402
from . import datasets   # noqa: E402

_sys.modules[__name__ + ".optim"] = optim
_sys.modules[__name__ + ".datasets"] = datasets
