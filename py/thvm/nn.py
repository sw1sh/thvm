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
    """tinygrad's nn.Conv2d (nn/__init__.py:80-109) over (B, C_in, *spatial), one spatial axis
    per kernel_size entry; padding="same" pads to the input's size at stride 1."""

    def __init__(self, in_channels: int, out_channels: int,
                 kernel_size, stride=1, padding=0, dilation=1,
                 groups: int = 1, bias: bool = True):
        self.kernel_size = ((kernel_size, kernel_size) if isinstance(kernel_size, int)
                            else tuple(kernel_size))
        if isinstance(padding, str):
            if padding.lower() != "same":
                raise ValueError(f"Invalid padding string {padding!r}, only 'same' is supported")
            if stride != 1:
                raise ValueError("padding='same' is not supported for strided convolutions")
            dils = ((dilation,) * len(self.kernel_size) if isinstance(dilation, int)
                    else tuple(dilation))
            padding = tuple(v for d, k in zip(dils[::-1], self.kernel_size[::-1])
                            for v in (d * (k - 1) // 2, d * (k - 1) - d * (k - 1) // 2))
        self.stride = stride
        self.padding = padding
        self.dilation = dilation
        self.groups = groups
        scale = 1.0 / math.sqrt(in_channels * math.prod(self.kernel_size))
        self.weight = Tensor.uniform(out_channels, in_channels // groups,
                                     *self.kernel_size,
                                     low=-scale, high=scale)
        self.bias = (Tensor.uniform(out_channels, low=-scale, high=scale)
                     if bias else None)

    def __call__(self, x: Tensor) -> Tensor:
        return x.conv2d(self.weight, self.bias, self.groups,
                        self.stride, self.dilation, self.padding)


def Conv1d(in_channels: int, out_channels: int, kernel_size: int, stride=1, padding=0,
           dilation=1, groups: int = 1, bias: bool = True) -> Conv2d:
    """tinygrad's nn.Conv1d (nn/__init__.py:62-79): a Conv2d with one spatial axis."""
    return Conv2d(in_channels, out_channels, (kernel_size,), stride, padding, dilation, groups,
                  bias)


class ConvTranspose2d(Conv2d):
    """tinygrad's nn.ConvTranspose2d (nn/__init__.py:130-155): weight (C_in, C_out / groups,
    *kernel)."""

    def __init__(self, in_channels: int, out_channels: int, kernel_size, stride=1, padding=0,
                 output_padding=0, dilation=1, groups: int = 1, bias: bool = True):
        super().__init__(in_channels, out_channels, kernel_size, stride, padding, dilation,
                         groups, bias)
        scale = 1.0 / math.sqrt(in_channels * math.prod(self.kernel_size))
        self.weight = Tensor.uniform(in_channels, out_channels // groups, *self.kernel_size,
                                     low=-scale, high=scale)
        self.output_padding = output_padding

    def __call__(self, x: Tensor) -> Tensor:
        return x.conv_transpose2d(self.weight, self.bias, self.groups, self.stride,
                                  self.dilation, self.padding, self.output_padding)


def ConvTranspose1d(in_channels: int, out_channels: int, kernel_size: int, stride=1, padding=0,
                    output_padding=0, dilation=1, groups: int = 1,
                    bias: bool = True) -> ConvTranspose2d:
    """tinygrad's nn.ConvTranspose1d (nn/__init__.py:111-129)."""
    return ConvTranspose2d(in_channels, out_channels, (kernel_size,), stride, padding,
                           output_padding, dilation, groups, bias)


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


class GroupNorm:
    """tinygrad's nn.GroupNorm (nn/__init__.py:179-208): layernorm over each of num_groups channel
    groups, then a per-channel affine."""

    def __init__(self, num_groups: int, num_channels: int, eps: float = 1e-5,
                 affine: bool = True):
        self.num_groups, self.num_channels, self.eps = num_groups, num_channels, eps
        self.weight = Tensor.ones(num_channels) if affine else None
        self.bias = Tensor.zeros(num_channels) if affine else None

    def __call__(self, x: Tensor) -> Tensor:
        x = x.reshape(x.shape[0], self.num_groups, -1).layernorm(eps=self.eps).reshape(x.shape)
        if self.weight is None or self.bias is None:
            return x
        ones = [1] * (x.ndim - 2)
        return x * self.weight.reshape(1, -1, *ones) + self.bias.reshape(1, -1, *ones)


class InstanceNorm:
    """tinygrad's nn.InstanceNorm (nn/__init__.py:209-234): layernorm over each channel's spatial
    axes, then a per-channel affine."""

    def __init__(self, num_features: int, eps: float = 1e-5, affine: bool = True):
        self.num_features, self.eps = num_features, eps
        self.weight = Tensor.ones(num_features) if affine else None
        self.bias = Tensor.zeros(num_features) if affine else None

    def __call__(self, x: Tensor) -> Tensor:
        x = x.reshape(x.shape[0], self.num_features, -1).layernorm(eps=self.eps).reshape(x.shape)
        if self.weight is None or self.bias is None:
            return x
        ones = [1] * (x.ndim - 2)
        return x * self.weight.reshape(1, -1, *ones) + self.bias.reshape(1, -1, *ones)


class RMSNorm:
    """tinygrad's nn.RMSNorm (nn/__init__.py:281-306): x * rsqrt(mean(x^2, -1) + eps) in f32."""

    def __init__(self, dim: int, eps: float = 1e-6, elementwise_affine: bool = True):
        self.eps = eps
        self.weight = Tensor.ones(dim) if elementwise_affine else None

    def _norm(self, x: Tensor) -> Tensor:
        return x * (x.square().mean(-1, keepdim=True) + self.eps).rsqrt()

    def __call__(self, x: Tensor) -> Tensor:
        x = self._norm(x.float()).cast(x.dtype)
        return x if self.weight is None else x * self.weight


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


class LSTMCell:
    """tinygrad's nn.LSTMCell (nn/__init__.py:398-420): one step from x and (h, c), zeros when
    hc is None."""

    def __init__(self, input_size: int, hidden_size: int, bias: bool = True):
        stdv = 1.0 / math.sqrt(hidden_size)
        self.weight_ih = Tensor.uniform(hidden_size * 4, input_size, low=-stdv, high=stdv)
        self.weight_hh = Tensor.uniform(hidden_size * 4, hidden_size, low=-stdv, high=stdv)
        self.bias_ih = Tensor.zeros(hidden_size * 4) if bias else None
        self.bias_hh = Tensor.zeros(hidden_size * 4) if bias else None

    def __call__(self, x: Tensor, hc=None):
        if hc is None:
            hc = (Tensor.zeros(x.size(0), self.weight_hh.size(1), dtype=x.dtype),) * 2
        gates = x.linear(self.weight_ih.T, self.bias_ih) + hc[0].linear(self.weight_hh.T,
                                                                        self.bias_hh)
        i, f, g, o = gates.chunk(4, dim=1)
        i, f, g, o = i.sigmoid(), f.sigmoid(), g.tanh(), o.sigmoid()
        new_c = f * hc[1] + i * g
        return o * new_c.tanh(), new_c


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
