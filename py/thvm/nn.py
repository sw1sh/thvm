"""tinygrad's nn layers (tinygrad nn/__init__.py), with nn.state, nn.optim and
nn.datasets."""
from __future__ import annotations

import math

from .dtypes import dtypes
from .helpers import TRAINING
from .tensor import Tensor


class Linear:
    """tinygrad's nn.Linear (nn/__init__.py:156-177): y = x @ W^T + b, W stored
    (out, in)."""

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
    """tinygrad's nn.Conv2d (nn/__init__.py:80-109) over (B, C_in, H, W)."""

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
    """tinygrad's nn.BatchNorm (nn/__init__.py:8-59)."""

    def __init__(self, sz: int, eps: float = 1e-5, affine: bool = True,
                 track_running_stats: bool = True,
                 momentum: float = 0.1):
        self.eps = eps
        self.track_running_stats = track_running_stats
        self.momentum = momentum
        self.weight = Tensor.ones(sz) if affine else None
        self.bias = Tensor.zeros(sz) if affine else None
        if track_running_stats:
            self.running_mean = Tensor.zeros(sz).is_param_(False)
            self.running_var = Tensor.ones(sz).is_param_(False)
        self.num_batches_tracked = Tensor.zeros().is_param_(False)

    def calc_stats(self, x: Tensor) -> tuple[Tensor, Tensor]:
        if self.track_running_stats and not TRAINING:
            return self.running_mean, self.running_var
        reduce_axes = tuple(i for i in range(x.ndim) if i != 1)
        batch_mean = x.mean(axis=reduce_axes)
        shape_mask = [1, -1] + [1] * (x.ndim - 2)
        # d(var)/d(mean) is 0 at the batch mean, so detaching it keeps the gradient
        # and shrinks the backward graph (tinygrad nn/__init__.py:48).
        y = x - batch_mean.detach().reshape(*shape_mask)
        batch_var = (y * y).mean(axis=reduce_axes)
        return batch_mean, batch_var

    def __call__(self, x: Tensor) -> Tensor:
        mean, var = self.calc_stats(x)
        if self.track_running_stats and TRAINING:
            n = x.numel()
            corr = n / (n - x.shape[1]) if n > x.shape[1] else 1.0
            # Lazy, as tinygrad's (nn/__init__.py:56-57): the optimizer's
            # schedule_step realizes these buffers with the step.
            self.running_mean.assign(self.running_mean * (1.0 - self.momentum)
                                     + mean.detach() * self.momentum)
            self.running_var.assign(self.running_var * (1.0 - self.momentum)
                                    + var.detach() * (self.momentum * corr))
            self.num_batches_tracked.assign(self.num_batches_tracked + 1)
        invstd = (var + self.eps).rsqrt()
        return x.batchnorm(self.weight, self.bias, mean, invstd)


BatchNorm2d = BatchNorm
BatchNorm3d = BatchNorm


class LayerNorm:
    """tinygrad's nn.LayerNorm (nn/__init__.py:235-261) over the last
    `normalized_shape` dims."""

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
    """tinygrad's nn.LayerNorm2d (nn/__init__.py:263-279): LayerNorm over the C
    of (B, C, H, W)."""
    def __call__(self, x: Tensor) -> Tensor:
        return super().__call__(x.permute(0, 2, 3, 1)).permute(0, 3, 1, 2)


class Embedding:
    """tinygrad's nn.Embedding.  On the CPU backend an out-of-range id reads a zero row
    and gets no gradient; tinygrad's atomic backward clips it (nn/__init__.py:356)."""

    def __init__(self, vocab_size: int, embed_size: int):
        self.vocab_size = vocab_size
        self.embed_size = embed_size
        # tinygrad's glorot_uniform init (nn/__init__.py:389).
        bound = math.sqrt(6.0 / (vocab_size + embed_size))
        self.weight = Tensor.uniform(vocab_size, embed_size,
                                     low=-bound, high=bound)

    def __call__(self, idx: Tensor) -> Tensor:
        # tinygrad nn/__init__.py:392.
        if not dtypes.is_int(idx.dtype):
            raise TypeError(f"Expected integer dtype for index in embedding, got {idx.dtype}")
        return self.weight.embedding(idx)


def _get_state_dict(obj, prefix: str = "") -> dict[str, Tensor]:
    """Every Tensor under `obj`, keyed by dotted path (tinygrad nn/state.py:89-112)."""
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
        """Every Tensor under `obj`, is_param False ones included (tinygrad
        nn/state.py:114-126)."""
        return list(_get_state_dict(obj).values())

    @staticmethod
    def get_state_dict(obj) -> dict[str, Tensor]:
        return _get_state_dict(obj)

    # The rest of nn.state lives in nn_state, which imports this module.
    def __getattr__(self, name):
        from . import nn_state
        return getattr(nn_state, name)


state = _State()

# nn is a module, not a package, so the submodules are registered under nn.*
# for `from thvm.nn.datasets import mnist`.
import sys as _sys   # noqa: E402
from . import optim   # noqa: E402
from . import datasets   # noqa: E402

_sys.modules[__name__ + ".optim"] = optim
_sys.modules[__name__ + ".datasets"] = datasets
