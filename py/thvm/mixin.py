"""tinygrad's composite Tensor ops, ported from its mixins at master (mixin/elementwise.py,
movement.py, reduce.py and op.py, cited by line) over the primitives of py/thvm/tensor.py.
Tensor inherits OpMixin as tinygrad's Tensor inherits its mixins."""
from __future__ import annotations

import functools
import math
import operator
import string

from .dtypes import dtypes, least_upper_dtype, to_dtype
from .helpers import argfix, argsort, ceildiv, flatten, make_tuple, polyN, prod, round_up
from .thvm import K


def flat_to_grouped(padding):
    """helpers.py:58: torch's flat pads, last axis first, as per-axis (before, after) pairs."""
    return tuple(zip(padding[-2::-2], padding[::-2]))


def resolve_pool_pads(padding, dims: int):
    """helpers.py:59-62."""
    if not isinstance(padding, int) and not (len(padding) == 2 * dims or len(padding) == dims):
        raise ValueError(f"Padding must be an int or a sequence of length {dims} or {2*dims}, "
                         f"but got {padding=} with {dims=}.")
    if isinstance(padding, int):
        return [padding] * 2 * dims
    return padding if len(padding) == 2 * dims else [p for p in padding for _ in range(2)][::-1]


def _align_left(*shapes):
    # uop/ops.py:77-79.
    n = max(len(s) for s in shapes)
    return tuple((1,) * (n - len(s)) + tuple(s) for s in shapes)


def _broadcast_shape(*shapes):
    """uop/ops.py:80-88: the shape all of `shapes` broadcast to."""
    ret = []
    for sizes in zip(*_align_left(*shapes)):
        if len(rest := set(s for s in sizes if s != 1)) > 1:
            raise IndexError(f"shape mismatch: objects cannot be broadcast to a single shape {shapes}")
        ret.append(rest.pop() if rest else 1)
    return tuple(ret)


def _get_shape(x) -> tuple[int, ...]:
    # helpers.py:30-34.
    if not hasattr(x, "__len__") or isinstance(x, str):
        return ()
    subs = [_get_shape(xi) for xi in x]
    if any(s != subs[0] for s in subs):
        raise ValueError(f"inhomogeneous shape from {x}")
    return (len(subs),) + (subs[0] if subs else ())


def _fully_flatten(x):
    # helpers.py:54-56.
    return [x] if not isinstance(x, (list, tuple)) else [y for xi in x for y in _fully_flatten(xi)]


class OpMixin:
    __slots__ = ()

    # ---- elementwise (mixin/elementwise.py) ---------------------------

    def _inverse(self):
        # :390, an order-reversing map, so min is max of the inverse.
        return -self if dtypes.is_float(self.dtype) else self.bitwise_not()

    def floor(self):  return (self < (b := self.trunc())).where(b - 1, b)                 # :662-670
    def ceil(self):   return (self > (b := self.trunc())).where(b + 1, b)                 # :652-660

    def round(self):
        # :888-897, half to even.
        if not dtypes.is_float(self.dtype):
            return self
        b = self.trunc() / 2.0
        return (self > 0).eq(b.trunc().eq(b)).where((self - 0.5).ceil(), (self + 0.5).floor())

    def mod(self, x, reverse: bool = False):
        # :202-213; an int pair is FLOORMOD as floormod_to_mod (codegen/decomp/op.py:58-62).
        a, b = self._broadcasted(x, reverse)
        if dtypes.is_int(a.dtype):
            r = a._int_op(b, K.IMOD)
            return r + (r.ne(0) & (a < 0).ne(b < 0)).where(b, 0)
        return a - a.div(b, rounding_mode="floor") * b

    def fmod(self, x):
        # :214-227; an int pair is CMOD.
        a, b = self._broadcasted(x)
        if dtypes.is_int(a.dtype):
            return a._int_op(b, K.IMOD)
        return a - a.div(b, rounding_mode="trunc") * b

    def __floordiv__(self, x):  return self.div(x, rounding_mode="floor")
    def __rfloordiv__(self, x): return self.div(x, reverse=True, rounding_mode="floor")
    def __mod__(self, x):       return self.mod(x)
    def __rmod__(self, x):      return self.mod(x, True)

    def clamp(self, min_=None, max_=None):
        # :584-595.
        if min_ is None and max_ is None:
            raise RuntimeError("at least one of 'min_' or 'max_' must not be None")
        ret = (self < min_).where(min_, self) if min_ is not None else self
        return (ret > max_).where(max_, ret) if max_ is not None else ret

    def clip(self, min_=None, max_=None): return self.clamp(min_, max_)
    def isnan(self):    return self != self
    def isfinite(self): return (self.isinf() | self.isnan()).logical_not()

    def isinf(self, detect_positive: bool = True, detect_negative: bool = True):
        # :611-619, with | where tinygrad adds the bools.
        ret = self.const_like(False, dtypes.bool)
        if detect_positive:
            ret = ret | self.eq(math.inf)
        if detect_negative:
            ret = ret | self.eq(-math.inf)
        return ret

    def isclose(self, other, rtol: float = 1e-05, atol: float = 1e-08, equal_nan=False):
        # :631-650.
        other = self._ufix(other)
        is_finite_close = self.isfinite() & other.isfinite() & ((self - other).abs() <= atol + rtol * other.abs())
        is_infinite_close = (self.isinf() | other.isinf()) & self.eq(other)
        is_nan_close = (self.isnan() & other.isnan()) & equal_nan
        return is_finite_close | is_infinite_close | is_nan_close

    def allclose(self, other, rtol: float = 1e-05, atol: float = 1e-08, equal_nan=False):
        return self.isclose(other, rtol=rtol, atol=atol, equal_nan=equal_nan).all()      # op.py:1021-1025

    def masked_fill(self, mask, value):  return mask.where(value, self)                    # :438-454

    def copysign(self, other):
        # :409-415.
        a, b = self._broadcasted(other)
        return ((b < 0) | (b.reciprocal() < 0)).where(-(mag := a.abs()), mag)

    def logaddexp(self, other):
        # :417-423.
        a, b = self._broadcasted(other)
        m = (mx := a.maximum(b)).isfinite().where(mx, 0)
        return ((a - m).exp() + (b - m).exp()).log() + m

    def lerp(self, end, weight):
        # :1080-1091, uint8 in 7-bit fixed point.
        if self.dtype == dtypes.uint8 and isinstance(weight, type(self)):
            w_i = (weight * (1 << (W_PREC := 7)) + 0.5).cast(dtypes.int16)
            step = ((end - self).cast(dtypes.int8) * w_i + (1 << W_PREC - 1)).cast(dtypes.uint16) >> W_PREC
            return (self + step).cast(dtypes.uint8)
        return self + (end - self) * weight

    def cos(self):
        # :499-508, sin(pi/2 - x) in at least float32.
        x = self if dtypes.is_float(self.dtype) else self.float()
        return ((math.pi / 2) - x.cast(least_upper_dtype(x.dtype, dtypes.float32))).sin().cast(x.dtype)

    def tan(self):  return self.sin() / self.cos()                                         # :919-927

    def relu6(self):       return ((r := self.relu()) < 6).where(r, 6)                       # :695-705
    def hardswish(self):   return self * (self + 3).relu6() * (1 / 6)                        # :707-717
    def hardtanh(self, min_val=-1, max_val=1): return self.clip(min_val, max_val)            # :732-740
    def leaky_relu(self, neg_slope=0.01): return (self < 0).where(neg_slope * self, self)   # :742-753
    def quick_gelu(self):  return self * (self * 1.702).sigmoid()                            # :767-775
    def swish(self):       return self * self.sigmoid()                                      # :794-804
    def silu(self):        return self.swish()                                               # :806-816
    def log10(self):       return self.log2() * math.log10(2)                                # :840-850
    def atanh(self):       return ((1 + self) / (1 - self)).log() / 2                        # :852-862
    def acosh(self):       return (self + (self.square() - 1).sqrt()).log()                  # :876-886
    def acos(self):        return math.pi / 2 - self.asin()                                  # :942-950
    def atan(self):        return (self / (1 + self * self).sqrt()).asin()                   # :952-960
    def celu(self, alpha=1.0): return alpha * (self / alpha).elu()                           # :974-984
    def softplus(self, beta=1.0): return (1 / beta) * (self * beta).logaddexp(0.0)           # :998-1006
    def mish(self):        return self * self.softplus().tanh()                              # :1008-1018
    def logsigmoid(self):  return -(-self).softplus()                                        # :1020-1030
    def sinh(self):        return (self.exp() - self.neg().exp()) / 2                        # :1032-1042
    def cosh(self):        return (self.exp() + self.neg().exp()) / 2                        # :1044-1054
    def softsign(self):    return self / (1 + self.abs())                                    # :1070-1078

    def hardsigmoid(self, alpha: float = 1 / 6, beta: float = 0.5):
        return ((y := (alpha * self + beta).relu()) < 1).where(y, 1)                         # :719-730

    def selu(self, alpha=1.6732632423543772, gamma=1.0507009873554805):
        return gamma * self.elu(alpha)                                                       # :986-996

    def asinh(self):
        # :864-874.
        return (sg := (self < 0).where(-1.0, 1.0)) * (self * sg + (self.square() + 1).sqrt()).log()

    def asin(self):
        # :929-940, Abramowitz and Stegun 4.4.46 with a0 = pi/2, so asin(0) is exactly 0.
        coefficients = [-0.0012624911, 0.0066700901, -0.0170881256, 0.0308918810, -0.0501743046,
                        0.0889789874, -0.2145988016, math.pi / 2]
        a = (s := (self >= 0).where(1.0, -1.0)) * self
        return s * (math.pi / 2 - (1.0 - a).sqrt() * polyN(a, coefficients))

    # ---- reduce (mixin/reduce.py, op.py) -------------------------------

    def min(self, axis=None, keepdim=False):
        return self._inverse().max(axis=axis, keepdim=keepdim)._inverse()                   # op.py:496-517

    def any(self, axis=None, keepdim=False):  return self.bool().max(axis, keepdim)          # reduce.py:96-116

    def all(self, axis=None, keepdim=False):  return self.bool().prod(axis, keepdim)         # reduce.py:118-138

    def argmin(self, axis=None, keepdim=False):
        return self._inverse().argmax(axis=axis, keepdim=keepdim)                           # op.py:913-934

    def logsumexp(self, axis=None, keepdim=False):
        # op.py:653-678.
        m = (mx := self.max(axis=axis, keepdim=True).detach()).isfinite().where(mx, 0)
        return (self - m).exp().sum(axis=axis, keepdim=keepdim).log() + (m if keepdim else m.squeeze(axis))

    def normalize(self, p: float = 2.0, dim: int = 1, eps: float = 1e-12):
        # op.py:632-651.
        if p == 0:
            den = self.ne(0).sum(dim, keepdim=True)
        else:
            den = self.abs().pow(p).sum(dim, keepdim=True).pow(1 / p)
        return self / den.maximum(eps)

    def rmsnorm(self, axis=-1, eps: float = 1e-6):
        return self.mul(self.square().mean(axis, keepdim=True).add(eps).rsqrt())             # op.py:1667-1682

    def _cumalu(self, axis: int, op: str):
        # op.py:775-779: a window over the axis padded by op's identity, then op over it.
        assert self.shape[axis] != 0
        pads = (None,) * (self.ndim - 1) + ((self.shape[axis] - 1, 0),)
        ident = {"sum": 0, "prod": 1}.get(op, self.dtype.min)
        pooled = self.transpose(axis, -1)._pad_constant(pads, ident)._pool((self.shape[axis],))
        return getattr(pooled, op)(-1).transpose(axis, -1)

    def _split_cumalu(self, axis: int, op: str):
        # op.py:781-791: past 2 * SPLIT, a cumulative op per chunk, then over the chunk ends.
        axis = self._resolve_dim(axis)
        if self.ndim == 0 or 0 in self.shape:
            return self.cast(self.sum().dtype) if op == "sum" else self
        SPLIT, ident = 256, {"sum": 0, "prod": 1}.get(op, self.dtype.min)
        if (s := self.shape[axis]) <= SPLIT * 2:
            return self._cumalu(axis, op)
        pads = (None,) * (self.ndim - 1) + ((round_up(s, SPLIT) - s, 0),)
        chunks = self.transpose(axis, -1)._pad_constant(pads, ident).unflatten(-1, (-1, SPLIT))._cumalu(-1, op)
        base = chunks[..., -1]._cumalu(-1, op)._pad_constant((None,) * (chunks.ndim - 2) + ((1, -1),), ident)
        merged = {"sum": chunks.add, "prod": chunks.mul, "max": chunks.maximum}[op](base.unsqueeze(-1))
        return merged.flatten(start_dim=-2)[..., -s:].transpose(axis, -1)

    def cumsum(self, axis: int = 0):  return self._split_cumalu(axis, "sum")                 # op.py:770-782

    def cumprod(self, axis: int):  return self._split_cumalu(axis, "prod")                   # op.py:784-796

    def cummax(self, axis: int = 0):
        # op.py:821-837: the values, and the last index each running max is reached at.
        if self.ndim == 0:
            return self._split_cumalu(axis, "max"), type(self).zeros(self.shape, dtype=dtypes.int32)
        values, n = self._split_cumalu(axis, "max"), int(self.shape[axis])
        x, values_t = self.transpose(axis, -1), values.transpose(axis, -1)
        match = x.unsqueeze(-1).eq(values_t.unsqueeze(-2)) * self._tri(n, n)
        idx = (match * type(self).arange(n).reshape(n, 1)).max(-2).cast(dtypes.int32)
        return values, idx.transpose(-1, axis)

    def cummin(self, axis: int = 0):
        values, indices = self._inverse().cummax(axis)                                       # op.py:839-851
        return values._inverse(), indices

    def logcumsumexp(self, axis=0):
        # op.py:853-884.
        if self.ndim == 0:
            return self
        x = self.transpose(axis, -1)
        n = x.shape[-1]
        x_cummax = (mx := x.cummax(-1)[0].detach()).isfinite().where(mx, 0)
        mask = self._tri(n, n, 1).logical_not()
        ret = mask.where(x.unsqueeze(-2) - x_cummax.unsqueeze(-1), self.dtype.min).exp().sum(-1).log() + x_cummax
        return ret.transpose(-1, axis)

    @classmethod
    def linspace(cls, start, stop, steps: int, dtype=None):
        # op.py:221-235.
        if steps < 0:
            raise ValueError("number of steps must be non-negative")
        if (dtype := to_dtype(dtype or dtypes.default_float)) == dtypes.bool:
            raise ValueError("linspace with bool dtype is not supported")
        if steps == 1:
            return cls.full((1,), start, dtype=dtype)
        return (start + cls.arange(steps, dtype=dtypes.default_float) * ((stop - start) / (steps - 1))).cast(dtype)

    def masked_select(self, mask, size=None, fill_value=0):
        # op.py:1202-1229: each selected element's rank by a cumsum, a count per rank by a
        # scatter-add, and the gather at the counts' cumsum.
        if not dtypes.is_bool(mask.dtype):
            raise RuntimeError(f"masked_select expects bool mask tensor, got {mask.dtype}")
        x, mask = self.flatten(), mask._broadcast_to(self.shape).flatten()
        mask_cumsum = mask.cumsum()
        if size is None:
            counts = type(self).zeros(mask_cumsum[-1].item() if mask.numel() else 0, dtype=dtypes.int32)
            return x[counts.scatter(0, mask_cumsum, 1, reduce='add').cumsum()]
        counts = type(self).zeros(size, dtype=dtypes.int32).scatter(0, mask_cumsum, 1, reduce='add')
        return (type(self).arange(size) < mask.sum()).where(x[counts.cumsum()], fill_value).cast(self.dtype)

    def nonzero(self, size=None, fill_value=0):
        # op.py:1231-1262.
        if self.ndim == 0:
            return type(self).zeros(size if size is not None else int(self.ne(0).item()), 0, dtype=dtypes.int32)
        mask = self.ne(0).flatten()
        indices = type(self).stack(*[type(self).arange(s).reshape(*[1] * i, s, *[1] * (self.ndim - i - 1)).expand(self.shape).flatten()
                                     for i, s in enumerate(self.shape)], dim=-1)
        return indices.masked_select(mask.unsqueeze(-1).expand(*mask.shape, self.ndim),
                                     size=size * self.ndim if size is not None else None,
                                     fill_value=fill_value).reshape(-1, self.ndim)

    @classmethod
    def _tri(cls, r: int, c: int, diagonal=0):
        return cls.arange(r).unsqueeze(-1) + diagonal <= cls.arange(c)                       # op.py:256-257

    # ---- movement (mixin/movement.py) ----------------------------------

    def view(self, shape, *args):  return self.reshape(shape, *args)                         # :297-299

    def squeeze(self, dim=None):
        # :301-321.
        dims = range(self.ndim) if dim is None else [self._resolve_dim(d) for d in make_tuple(dim, 1)]
        return self.reshape(tuple(s for i, s in enumerate(self.shape) if i not in dims or s != 1))

    def flatten(self, start_dim=0, end_dim=-1):
        # :360-374.
        start_dim, end_dim = self._resolve_dim(start_dim), self._resolve_dim(end_dim)
        return self.reshape(self.shape[:start_dim] + (prod(self.shape[start_dim:end_dim + 1]),)
                            + self.shape[end_dim + 1:])

    def unflatten(self, dim: int, sizes):
        dim = self._resolve_dim(dim)                                                         # :376-391
        return self.reshape(self.shape[:dim] + tuple(sizes) + self.shape[dim + 1:])

    def pad_to(self, shape, *args):
        # :293-295, zeros at the end of each axis up to shape (None keeps it).
        pads = tuple(None if ns is None else (0, ns - s) for s, ns in zip(self.shape, argfix(shape, *args)))
        return self._pad_zero(tuple((0, 0) if p is None else p for p in pads))

    def meshgrid(self, *args, indexing: str = "ij"):
        # :489-514.
        if indexing not in ("ij", "xy"):
            raise RuntimeError(f'indexing must be in ("ij", "xy"), got {indexing}')
        if len(tensors := (self, *args)) == 1:
            return tensors
        basis = tuple(range(len(tensors))) if indexing == "ij" else (1, 0) + tuple(range(2, len(tensors)))
        tensors = tuple(t.reshape((-1,) + (1,) * (len(args) - i)) for i, t in zip(basis, tensors))
        output_shape = _broadcast_shape(*(t.shape for t in tensors))
        return tuple(t._broadcast_to(output_shape) for t in tensors)

    def diag(self):
        # :516-525.
        if self.ndim != 1:
            raise ValueError(f"expect input to be 1-D, getting {self.ndim}-D")
        n = self.shape[0]
        return self.unsqueeze(-1).pad_to((None, 1 + n)).flatten().shrink_to((n * n,)).reshape(n, n)

    def diagonal(self, offset: int = 0, dim1: int = 0, dim2: int = 1):
        # :527-549.
        if (dim1 := self._resolve_dim(dim1)) == (dim2 := self._resolve_dim(dim2)):
            raise RuntimeError("dim1 and dim2 cannot be the same dimension")
        x = self.permute(*[i for i in range(self.ndim) if i != dim1 and i != dim2], dim1, dim2)
        if offset >= 0:
            x = x.shrink(tuple(None for _ in x.shape[:-1]) + ((offset, x.shape[-1]),))
        else:
            x = x.shrink(tuple(None for _ in x.shape[:-2]) + ((-offset, x.shape[-2]), None))
        if (d := min(int(x.shape[-2]), int(x.shape[-1]))) <= 0:
            return x.reshape(*x.shape[:-2], 0)
        nones, x = tuple(None for _ in x.shape[:-2]), x.shrink_to(tuple(None for _ in x.shape[:-2]) + (d, d))
        return x.flatten(-2).pad_to(nones + (d * (d + 1),)).unflatten(-1, (d, d + 1)).shrink_to(nones + (None, 1)).squeeze(-1)

    def roll(self, shifts, dims=None):
        # :551-571.
        if dims is None:
            return self.flatten().roll(shifts, 0).reshape(self.shape)
        dims, shifts = tuple(self._resolve_dim(d) for d in make_tuple(dims, 1)), make_tuple(shifts, 1)
        if len(dims) != len(shifts):
            raise RuntimeError(f"{len(dims)=} != {len(shifts)=}")
        if 0 in self.shape:
            return self
        shift = [0] * self.ndim
        for d, s in zip(dims, shifts):
            shift[d] += s
        shrink_arg = tuple((delta := n - s % n, delta + n) if i in dims else None
                           for i, (n, s) in enumerate(zip(self.shape, shift)))
        return self.repeat(*tuple(2 if i in dims else 1 for i in range(self.ndim))).shrink(shrink_arg)

    def repeat(self, repeats, *args):
        # :576-595; fewer repeats than axes fail the reshape, as tinygrad's.
        repeats = argfix(repeats, *args)
        base = _align_left(self.shape, repeats)[0]
        unsq = flatten([[s] if r == 1 else [1, s] for r, s in zip(repeats, base)])
        expd = flatten([[s] if r == 1 else [r, s] for r, s in zip(repeats, base)])
        return self.reshape(unsq).expand(expd).reshape([r * s for r, s in zip(repeats, base)])

    def repeat_interleave(self, repeats: int, dim=None):
        # :575-589.
        x, dim = (self.flatten(), 0) if dim is None else (self, self._resolve_dim(dim))
        shp = x.shape
        x = x.reshape(*shp[:dim + 1], 1, *shp[dim + 1:]).expand(*shp[:dim + 1], repeats, *shp[dim + 1:])
        return x.reshape(*shp[:dim], shp[dim] * repeats, *shp[dim + 1:])

    def unfold(self, dim: int, size, step: int):
        # :634-656.
        if size < 0:
            raise RuntimeError(f'size must be >= 0 but got {size=}')
        if step <= 0:
            raise RuntimeError(f'step must be > 0 but got {step=}')
        if size > self.shape[dim]:
            raise RuntimeError(f'maximum size for tensor at dimension {dim} is {self.shape[dim]} but size is {size}')
        dim = self._resolve_dim(dim)
        perm_to_last = tuple(i for i in range(self.ndim) if i != dim) + (dim,)
        return self.permute(perm_to_last)._pool((size,), step).permute(argsort(perm_to_last) + (self.ndim,))

    # ---- pad (op.py:305-358) -------------------------------------------

    def _pad_constant(self, pX, value):
        # shrink first for negative pads, then pad with only non-negative values
        pX = tuple((0, 0) if p is None else p for p in pX)
        has_neg = not all(p >= 0 for p in flatten(pX))
        X = self.shrink(tuple((-min(pB, 0), min(pA + s, s)) for (pB, pA), s in zip(pX, self.shape))) if has_neg else self
        pads = tuple((max(pB, 0), max(pA, 0)) for pB, pA in pX) if has_neg else pX
        base = X._pad_zero(pads)
        # tinygrad folds where(True, base, value) when nothing is padded.
        if value == 0 or base is X:
            return base
        return X.const_like(True, dtypes.bool)._pad_zero(pads).where(base, value)

    def _pad_circular(self, pX):
        X = self.shrink(tuple((-min(pB, 0), min(pA + sh, sh)) for (pB, pA), sh in zip(pX, self.shape)))
        pX = tuple((max(pB, 0), max(pA, 0)) for pB, pA in pX)
        if any(pB > sh or pA > sh for (pB, pA), sh in zip(pX, X.shape)):
            raise ValueError('Padding value causes wrapping around more than once.')
        orig_shape, X = X.shape, X.repeat(tuple(1 + bool(pB) + bool(pA) for pB, pA in pX))
        return X.shrink(tuple((0 if pB == 0 else osh - pB, xsh if pA == 0 else xsh - osh + pA)
                              for (pB, pA), osh, xsh in zip(pX, orig_shape, X.shape)))

    def _pad_reflect_replicate(self, pX, mode: str):
        X, pads = self, tuple((max(pB, 0), max(pA, 0)) for pB, pA in pX)
        for d, (pB, pA) in enumerate(pads):
            if mode == "reflect":
                if pB >= (s := X.shape[d]) or pA >= s:
                    raise ValueError(f"Padding ({pB}, {pA}) should be less than the input size={s} for dim={d}.")
                slcB, slcA = slice(pB, 0, -1), slice(s - 2, s - 2 - pA if s - 2 - pA >= 0 else None, -1)
                xB, xA = (X[[slc if i == d else slice(None) for i in range(X.ndim)]] if p > 0 else None
                          for slc, p in ((slcB, pB), (slcA, pA)))
            else:
                shrB = tuple((0, 1) if i == d else None for i in range(X.ndim))
                shrA = tuple((X.shape[i] - 1, X.shape[i]) if i == d else None for i in range(X.ndim))
                xB, xA = (X.shrink(shr).expand(tuple(p if i == d else None for i in range(X.ndim))) if p > 0 else None
                          for shr, p in ((shrB, pB), (shrA, pA)))
            pieces = [X_ for X_ in (xB, X, xA) if X_ is not None]
            X = pieces[0].cat(*pieces[1:], dim=d)
        # shrink after for negative pads (reflection/replication must see full data first)
        return X.shrink(tuple((-min(pB, 0), min(pA + s, s)) for (pB, pA), s in zip(pX, X.shape)))

    def pad(self, padding, mode: str = "constant", value=0.0):
        """Per-axis (before, after) pairs (None: none) or torch's flat pads, last axis first."""
        if not any(isinstance(p, (tuple, list)) or p is None for p in padding):
            if len(padding) % 2 != 0:
                raise ValueError("Flat padding must have even number of pads")
            pX = ((0, 0),) * (self.ndim - len(padding) // 2) + flat_to_grouped(padding)
        else:
            pX = tuple((0, 0) if p is None else tuple(p) for p in padding)
        if len(pX) != self.ndim:
            raise ValueError(f"padding length is improper, {padding=} {self.ndim=}")
        if mode == "constant":
            return self._pad_constant(pX, value)
        if mode == "circular":
            return self._pad_circular(pX)
        if mode in {"reflect", "replicate"}:
            return self._pad_reflect_replicate(pX, mode)
        raise NotImplementedError(f"{mode=} is not supported")

    # ---- getitem (op.py:57-146, movement.py:73-126) ---------------------

    def _normalize_indices(self, indices: list) -> list:
        if len(ell := [i for i, x in enumerate(indices) if x is Ellipsis]) > 1:
            raise IndexError("indices can only have a single ellipsis")
        num_real = len(indices) - len(ell) - sum(1 for i in indices if i is None)
        if num_real > self.ndim:
            raise IndexError(f"too many indices ({num_real}) for {self.ndim}D")
        fill_idx = ell[0] if ell else len(indices)
        indices[fill_idx:fill_idx + 1] = [slice(None)] * (self.ndim - num_real)
        return indices

    @staticmethod
    def _parse_view_index(index, size: int) -> dict:
        if index is None:
            return {"size": 1, "boundary": (0, 1), "stride": 1, "collapse_dim": False}
        if isinstance(index, int):
            if index >= size or index < -size:
                raise IndexError(f"{index=} is out of bounds with {size=}")
            b = index if index >= 0 else index + size
            return {"size": size, "boundary": (b, b + 1), "stride": 1, "collapse_dim": True}
        if isinstance(index, slice):
            if index.step == 0:
                raise ValueError(f"{index=} cannot have 0 as step")
            *bound, stride = index.indices(size)
            bound = [0, 0] if stride * (bound[1] - bound[0]) < 0 else ([bound[1] + 1, bound[0] + 1] if stride < 0 else bound)
            return {"size": ceildiv(bound[1] - bound[0], abs(stride)), "boundary": tuple(bound),
                    "stride": stride, "collapse_dim": False}
        raise IndexError(f"{type(index).__name__} indexing is not supported")

    def _apply_view_ops(self, mops: list):
        # shrink, flip the negative strides, then take every stride-th element
        x = self.shrink(tuple(m["boundary"] for m in mops)).flip(tuple(i for i, m in enumerate(mops) if m["stride"] < 0))
        strides = tuple(abs(m["stride"]) for m in mops)
        if any(st != 1 for st in strides):
            x = x.pad_to(tuple(round_up(s, st) for s, st in zip(x.shape, strides)))
            x = x.reshape(tuple(flatten((s // st, st) for s, st in zip(x.shape, strides))))
            x = x.shrink_to(tuple(flatten((s, 1) for s in x.shape[::2]))).reshape(x.shape[::2])
        return x

    def _getitem(self, indices):
        """Views for slices, ints, None and an Ellipsis; an int Tensor or list index selects through
        a one-hot mask, consecutive ones through one linear index."""
        def is_adv(i): return isinstance(i, (list, tuple, type(self)))
        indices_parsed, dim = [], 0
        for index in self._normalize_indices(list(indices)):
            size = 1 if index is None else self.shape[dim]
            parsed = {"size": size, "boundary": (0, size), "stride": 1, "collapse_dim": False}
            if isinstance(index, (list, tuple)):
                flat = _fully_flatten(index)
                if not dtypes.is_int(inferred := dtypes.from_py(flat)):
                    raise IndexError(f"{index=} contains non-int element")
                index = type(self)([i + size if i < 0 else i for i in flat], dtype=inferred).reshape(_get_shape(index))
            elif isinstance(index, type(self)):
                if not dtypes.is_int(index.dtype):
                    raise IndexError(f"index dtype {index.dtype} is not supported")
                index = (index < 0).where(index + size, index)
            else:
                parsed = self._parse_view_index(index, size)
            indices_parsed.append({**parsed, "index": index})
            if index is not None:
                dim += 1
        x = self._apply_view_ops([p for p in indices_parsed if p["index"] is not None])
        x_dims = [p for p in indices_parsed if not p["collapse_dim"]]
        x = x.reshape(tuple(p["size"] for p in x_dims))
        if not (tops := [(d, p) for d, p in enumerate(x_dims) if is_adv(p["index"])]):
            return x
        dims, tensors = [d for d, _ in tops], [p["index"] for _, p in tops]
        big_shape = _broadcast_shape(*(t.shape for t in tensors))
        consecutive = dims == list(range(dims[0], dims[0] + len(dims)))
        if len(dims) > 1 and consecutive:
            ishp = tuple(x.shape[d] for d in dims)
            strides = tuple(prod(ishp[i + 1:]) for i in range(len(dims)))
            linear_idx = functools.reduce(operator.add, [t * s for t, s in zip(tensors, strides)])
            valid = functools.reduce(operator.and_, [(t >= 0) & (t < s) for t, s in zip(tensors, ishp)])
            pre, post = x.shape[:dims[0]], x.shape[dims[-1] + 1:]
            x = x.reshape(pre + (prod(ishp),) + post)[tuple([slice(None)] * len(pre)) + (valid.where(linear_idx, 0),)]
            return valid.reshape((1,) * len(pre) + big_shape + (1,) * len(post)).where(x, 0)
        pre_reduce_shape = x.shape[:dims[0]] + big_shape + x.shape[dims[0]:]
        masks = []
        for d, t in zip(dims, tensors):
            try:
                i = t.reshape(t.shape + (1,) * (x.ndim - dims[0])).expand(pre_reduce_shape)
            except ValueError as err:
                raise IndexError(f"cannot broadcast indices: {err}") from err
            masks.append(i._one_hot_along_dim(num_classes=x.shape[d], dim=(d - x.ndim)))
        mask = functools.reduce(operator.and_, masks)
        reshape_arg = x.shape[:dims[0]] + (1,) * len(big_shape) + x.shape[dims[0]:]
        x = mask.where(x.reshape(reshape_arg), 0).sum(tuple(d + len(big_shape) for d in dims), dtype=x.dtype)
        if dims[0] != 0 and not consecutive:
            x = x.permute(*range(dims[0], dims[0] + len(big_shape)), *range(0, dims[0]),
                          *range(dims[0] + len(big_shape), x.ndim))
        return x

    def _one_hot_along_dim(self, num_classes: int, dim: int = -1):
        # op.py:1028-1031.
        if not dtypes.is_int(self.dtype):
            raise RuntimeError(f"_one_hot_along_dim expects int index tensor, getting {self.dtype}")
        offset = self.ndim - self._resolve_dim(dim) - 1
        return self.eq(type(self).arange(num_classes).reshape((num_classes,) + (1,) * offset))

    def one_hot(self, num_classes: int):
        # op.py:1033-1044.
        if not dtypes.is_int(self.dtype):
            raise RuntimeError(f"expect integer dtype, getting {self.dtype=}")
        if num_classes < 0:
            raise ValueError(f"num_classes must be non-negative, got {num_classes}")
        return self[..., None]._one_hot_along_dim(num_classes).where(1, 0)

    # ---- sort (op.py:936-1019) -----------------------------------------

    def sort(self, dim: int = -1, descending: bool = False):
        """A bitonic sort network, then each sorted value's index from its rank among equals."""
        x, dim = self, self._resolve_dim(dim)
        if (orig_len := int(x.shape[dim])) <= 1:
            return x, x.const_like(0, dtypes.int32)
        n_stages = (orig_len - 1).bit_length()
        pads = tuple((0, 2 ** n_stages - orig_len) if i == dim else None for i in range(x.ndim))
        x = x._pad_constant(pads, x.dtype.min if descending else x.dtype.max).unflatten(dim, (2,) * n_stages)
        for stage in range(1, n_stages + 1):
            if stage != n_stages:
                crossover_dim = dim + n_stages - stage - 1
                blue_box, green_box = x.split(1, crossover_dim)
                flip_dims = tuple(-i for i in range(1, stage + 1 + (self.ndim - dim)))
                x = (blue_box.cat(green_box.flip(flip_dims), dim=crossover_dim)).contiguous()
            for substage in range(stage - 1, -1, -1):
                partner_dim = dim + n_stages - substage - 1
                x_top, x_bottom = x.split(1, partner_dim)
                x_larger, x_smaller = x_top.maximum(x_bottom), x_top.minimum(x_bottom)
                x = (x_larger.cat(x_smaller, dim=partner_dim) if descending else x_smaller.cat(x_larger, dim=partner_dim)).contiguous()
            if stage != n_stages:
                blue_box, flipped_green_box = x.split(1, crossover_dim)
                x = blue_box.cat(flipped_green_box.flip(flip_dims), dim=crossover_dim)
        x = x.flatten(dim, dim + n_stages - 1).shrink_to(self.shape)
        mask = self._tri(orig_len, orig_len, 1).logical_not()
        mask = mask.reshape((None, None) + (1,) * (self.ndim - dim - 1))
        def compute_counts(t): return (mask & t.unsqueeze(dim).eq(t.unsqueeze(dim + 1))).sum(dim + 1)
        count_orig, count_sorted = compute_counts(self), compute_counts(x)
        cond = self.unsqueeze(dim + 1).eq(x.unsqueeze(dim)) & count_orig.unsqueeze(dim + 1).eq(count_sorted.unsqueeze(dim))
        idx = type(self).arange(orig_len).reshape(tuple(orig_len if i == dim else 1 for i in range(x.ndim)))
        return x, (cond * idx.unsqueeze(dim + 1)).sum(dim)

    def argsort(self, dim: int = -1, descending: bool = False):
        return self.sort(dim, descending)[1]

    def topk(self, k: int, dim: int = -1, largest: bool = True, sorted_: bool = True):
        if not sorted_:
            raise NotImplementedError("topk with sorted_=False is not supported")
        if k > self.shape[dim := self._resolve_dim(dim)]:
            raise ValueError(f"selected index {k=} is out of range")
        x, idx = self.sort(dim, descending=largest)
        topk_shape = tuple(k if i == dim else None for i in range(self.ndim))
        return x.shrink_to(topk_shape), idx.shrink_to(topk_shape)

    # ---- einsum (op.py:436-471) ----------------------------------------

    @classmethod
    def einsum(cls, formula: str, *operands, dtype=None):
        xs, formula = list(argfix(*operands)), formula.replace(" ", "")
        # implicit output is the ellipsis, then the letters that appear once, sorted
        lhs, rhs = formula.split("->") if "->" in formula else \
            (formula, "..." * ("..." in formula) + "".join(sorted(c for c in formula if formula.count(c) == 1 and c.isalpha())))
        inputs = lhs.split(",")
        if len(xs) != len(inputs):
            raise ValueError(f"number of operands doesn't match, expected {len(inputs)}, got {len(xs)}")
        # expand each ellipsis to a suffix of the unused letters, so ellipsis dims align from the right
        ell = "".join(c for c in string.ascii_letters if c not in formula)
        ells = [ell[len(ell) - (x.ndim - len(s) + 3):] if "..." in s else "" for s, x in zip(inputs, xs)]
        inputs, rhs = [s.replace("...", e) for s, e in zip(inputs, ells)], rhs.replace("...", max(ells, key=len))
        sz = {}
        for s, x in zip(inputs, xs):
            for c, n in zip(s, x.shape, strict=True):
                if sz.setdefault(c, n) != n:
                    raise RuntimeError(f"size of {c} differs: {sz[c]} vs {n}")
        if not set(rhs) <= set(sz):
            raise ValueError(f"output letters {rhs} must appear in the inputs {inputs}")
        # trace: take diagonal when letter repeats in single input, the diagonal is the last axis
        for i, (s, x) in enumerate(zip(inputs, xs)):
            for c in dict.fromkeys(s):
                while s.count(c) > 1:
                    j = s.index(c)
                    x, s = x.diagonal(dim1=j, dim2=s.index(c, j + 1)), s.replace(c, "", 2) + c
            inputs[i], xs[i] = s, x
        # align all tensors to output letters then summed letters, multiply, sum
        alpha = rhs + "".join(sorted(c for c in sz if c not in rhs))
        xs = [x.permute([s.index(c) for c in alpha if c in s]).reshape([sz[c] if c in s else 1 for c in alpha])
              for s, x in zip(inputs, xs)]
        return functools.reduce(operator.mul, xs[1:], xs[0]).sum(list(range(len(rhs), len(alpha))), dtype=dtype)

    # ---- pool (op.py:1294-1431) ----------------------------------------

    def _apply_ceil_mode(self, pads, k_, s_, d_):
        (d_, s_), i_ = (make_tuple(x, len(k_)) for x in (d_, s_)), self.shape[-len(k_):]
        grouped_pads = list(flat_to_grouped(pads))
        # https://arxiv.org/pdf/1603.07285 section 5.1, relationship 15.
        o_ = [ceildiv(i + pB + pA - (d * (k - 1) + 1), s) + 1 for i, d, k, s, (pB, pA) in zip(i_, d_, k_, s_, grouped_pads)]
        for dim, (o, i, s, k, d, (pB, pA)) in enumerate(zip(o_, i_, s_, k_, d_, grouped_pads)):
            # trailing pad for the last window, less where that window would start in the pad
            grouped_pads[dim] = (pB, pA + s * (o - 1) + (d * (k - 1) + 1) - (i + pB + pA) - max(s * (o - 1) - (pB + i - 1), 0))
        return flatten(reversed(grouped_pads))

    def avg_pool2d(self, kernel_size=(2, 2), stride=None, dilation=1, padding=0, ceil_mode=False,
                   count_include_pad=True):
        axis = tuple(range(-len(k_ := make_tuple(kernel_size, 2)), 0))
        s_ = stride if stride is not None else k_
        def pool(x, padding_):
            return x._pad_constant(((0, 0),) * (x.ndim - len(k_)) + flat_to_grouped(padding_), 0.0)._pool(k_, s_, dilation)
        reg_pads = resolve_pool_pads(padding, len(k_))
        pads = self._apply_ceil_mode(reg_pads, k_, s_, dilation) if ceil_mode else reg_pads
        if not count_include_pad:
            return pool(self, pads).sum(axis) / pool(self.const_like(1), pads).sum(axis)
        if not ceil_mode:
            return pool(self, pads).mean(axis)
        ones = self._pad_constant(((0, 0),) * (self.ndim - len(k_)) + flat_to_grouped(reg_pads), 0.0).const_like(1)
        return pool(self, pads).sum(axis) / pool(ones, tuple(cp - rp for cp, rp in zip(pads, reg_pads))).sum(axis)

    def max_pool2d(self, kernel_size=(2, 2), stride=None, dilation=1, padding=0, ceil_mode=False,
                   return_indices=False):
        axis = tuple(range(-len(k_ := make_tuple(kernel_size, 2)), 0))
        s_ = stride if stride is not None else k_
        pads = resolve_pool_pads(padding, len(k_))
        if ceil_mode:
            pads = self._apply_ceil_mode(pads, k_, s_, dilation)
        padded = self._pad_constant(((0, 0),) * (self.ndim - len(k_)) + flat_to_grouped(pads), self.dtype.min)
        if not return_indices:
            return padded._max_pool(k_, s_, dilation)
        pooled = padded._pool(k_, s_, dilation)
        spatial_sz = int(prod(spatial_shape := self.shape[-len(k_):]))
        idx = type(self).arange(spatial_sz, 0, -1).reshape(spatial_shape)
        m = pooled.eq(pooled.max(axis, keepdim=True))
        idx = m * idx._pad_constant(((0, 0),) * (idx.ndim - len(k_)) + flat_to_grouped(pads), idx.dtype.min)._pool(k_, s_, dilation)
        return pooled.max(axis), spatial_sz - idx.max(axis)

    def max_unpool2d(self, indices, kernel_size=(2, 2), stride=None, dilation=1, padding=0, output_size=None):
        bs, c, *spatial_shape = self.shape
        if output_size is None:
            k_, d_, s_ = (make_tuple(x, len(spatial_shape)) for x in (kernel_size, dilation, stride if stride is not None else kernel_size))
            p_ = flat_to_grouped(resolve_pool_pads(padding, len(spatial_shape)))
            # https://arxiv.org/pdf/1603.07285 inverse of relationship 15 in section 5.1.
            output_size = tuple((i - 1) * s - (pB + pA) + (d * (k - 1) + 1) for i, k, d, s, (pA, pB) in zip(spatial_shape, k_, d_, s_, p_))
        else:
            output_size = output_size[-len(spatial_shape):]
        ret = (indices.reshape(bs, c, 1, -1)._one_hot_along_dim(prod(output_size), 2).where(self.reshape(bs, c, 1, -1), 0)).sum(3)
        return ret.reshape(bs, c, *output_size)

    def interpolate(self, size, mode: str = "linear", align_corners: bool = False):
        # op.py:1066-1098.
        assert isinstance(size, (tuple, list)) and all(isinstance(s, int) for s in size) and 0 < len(size) <= self.ndim, f"invalid {size=}"
        assert mode in ("linear", "nearest", "nearest-exact"), "only supports linear, nearest or nearest-exact interpolate"
        assert not (align_corners and mode != "linear"), "align_corners option can only be set with the interpolating mode linear"
        x, expand = self, list(self.shape)
        for i in range(-1, -len(size) - 1, -1):
            in_sz, reshape = int(self.shape[i]), [1] * self.ndim
            reshape[i] = expand[i] = size[i]
            if mode == "linear":
                arr = type(self).arange(size[i])
                num, den = (arr * (in_sz - 1), max(size[i] - 1, 1)) if align_corners else ((arr * 2 + 1) * in_sz - size[i], size[i] * 2)
                num = num.clip(0, (in_sz - 1) * den)
                low, high, perc = [y.reshape(reshape).expand(expand)
                                   for y in (num // den, (num + den - 1) // den, (num % den).cast(dtypes.float32) / den)]
                x = x.gather(i, low).lerp(x.gather(i, high), perc)
            else:
                scale, arr = in_sz / size[i], type(self).arange(size[i], dtype=dtypes.float32)
                index = (scale * (arr + 0.5) if mode == "nearest-exact" else scale * arr).cast(dtypes.int32).reshape(reshape).expand(expand)
                x = x.gather(i, index)
        return x.cast(self.dtype)

    # ---- loss (op.py:1725-1834) ----------------------------------------

    def _do_reduction(self, reduction: str = "mean"):
        if reduction == "none":
            return self
        if reduction == "sum":
            return self.sum()
        if reduction == "mean":
            return self.mean()
        raise ValueError(f"{reduction=} must be one of ('mean', 'sum', 'none')")

    def binary_crossentropy(self, Y, reduction: str = "mean"):
        return (-Y * self.log() - (1 - Y) * (1 - self).log())._do_reduction(reduction)

    def binary_crossentropy_logits(self, Y, reduction: str = "mean", pos_weight=None):
        log_p, log_1_minus_p = self.logsigmoid(), (-self).logsigmoid()
        return (-((1 if pos_weight is None else pos_weight) * Y * log_p + (1 - Y) * log_1_minus_p))._do_reduction(reduction)

    def _sparse_categorical_crossentropy(self, Y, ignore_index=-1, label_smoothing=0.0, reduction="mean"):
        # op.py:1760-1783, the form with an ignore_index, smoothing or reduction.
        assert 0.0 <= label_smoothing <= 1.0, "label_smoothing must be in [0.0, 1.0]"
        log_probs = self.log_softmax()
        loss_mask = Y.ne(ignore_index) if ignore_index != -1 else Y.const_like(True, dtypes.bool)
        y = Y.unsqueeze(-1)._one_hot_along_dim(self.shape[-1], dim=-1) * loss_mask.unsqueeze(-1)
        smoothing = label_smoothing * (log_probs.mean(-1) * loss_mask)
        unreduced = ((1 - label_smoothing) * (log_probs * y).sum(-1) + smoothing)
        return -unreduced.sum() / loss_mask.sum() if reduction == "mean" else -unreduced._do_reduction(reduction)

    def cross_entropy(self, Y, reduction: str = "mean", label_smoothing: float = 0.0):
        assert 0.0 <= label_smoothing <= 1.0, "label_smoothing must be in [0.0, 1.0]"
        classes_dim = 0 if self.ndim == 1 else 1
        if self.shape != Y.shape:
            if self.max(classes_dim).shape != Y.shape:
                raise RuntimeError(f"shape mismatch: {self.shape=}, {Y.shape=}")
            Y = Y.unsqueeze(classes_dim)._one_hot_along_dim(num_classes=self.shape[classes_dim], dim=classes_dim)
        Y = (1 - label_smoothing) * Y + label_smoothing / int(Y.shape[classes_dim])
        return -self.log_softmax(classes_dim).mul(Y).sum(classes_dim)._do_reduction(reduction)

    def nll_loss(self, Y, weight=None, ignore_index=None, reduction: str = "mean"):
        weight = Y.const_like(1) if weight is None else weight.gather(0, Y.flatten()).reshape(Y.shape)
        masked_weight = weight if ignore_index is None else weight * Y.ne(ignore_index)
        nll = -self.gather(1, Y.unsqueeze(1)).squeeze(1) * masked_weight
        return nll.sum() / masked_weight.sum() if reduction == "mean" else nll._do_reduction(reduction)
