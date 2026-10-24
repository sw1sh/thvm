"""tinygrad's Tensor surface (365f87a8a is the spec) over a thvm term.

A Tensor is a thvm term plus its dtype and shape.  Methods build the UOp graph
through the Thvm bridge; realize() (implicit in numpy(), item() and tolist())
runs thvm's materialize, schedule and dispatch, and backward() its chain rule.
"""
from __future__ import annotations

import ctypes
import functools
import itertools
import math
import warnings
import weakref

import numpy as np

from .device import Device
from .dtypes import (DType, dtypes, from_thvm_id, least_upper_dtype, sum_acc_dtype, to_dtype,
                     _from_np_dtype, _to_np_dtype)
from .helpers import TRAINING, argfix, make_tuple
from .mixin import OpMixin, _align_left
from .thvm import K, Term, Thvm
from ._misc import jit_record_random


_TH = Thvm()
_TAG_TEN = K.TAG_TEN

# Each TAG_TEN tid -> its Tensor, so backward() can fill .grad on the in-scope float leaves
# (tinygrad tensor.py:485-494); weak values, so a dead Tensor drops out.
_LEAF_TENSORS: "weakref.WeakValueDictionary[int, Tensor]" = \
    weakref.WeakValueDictionary()

# The next pin handle (_pin).  Tensor allocation and realize call reclaim_if_due(), which bounds
# memory without GlobalCounters.reset(); both are safe points, where every term still needed is
# held by a live Tensor, never only as a raw integer in a local.
_PIN_NEXT = 1

# Every Tensor's raw term is a root of the deferred heap collection, which
# rebinds it when its cells move (Thvm._collect).
_LIVE_ADD = Thvm.live_terms.add


def _host_scatter_extreme(base, flat, vals, is_max: bool, include_self: bool):
    """amax / amin scatter on the host, folded as the native kernel does: S starts at the
    identity and takes x when S < x (amax) or x < S (amin), in index order; out = S (op) T,
    T = base, or the identity where something lands without include_self."""
    if base.dtype.kind == "f":
        ident = base.dtype.type(-np.inf if is_max else np.inf)
    else:
        info = np.iinfo(base.dtype)
        ident = base.dtype.type(info.min if is_max else info.max)
    acc = np.full(base.size, ident, base.dtype)
    for j, x in zip(flat.tolist(), vals.tolist()):
        if (acc[j] < x) if is_max else (x < acc[j]):
            acc[j] = x
    T = base.reshape(-1).copy()
    if not include_self:
        T[np.unique(flat)] = ident
    take = (acc < T) if is_max else (T < acc)
    return np.where(take, T, acc).reshape(base.shape)


def _native_indexing() -> bool:
    """Gather / scatter kernels run on the CPU backend only (emit declines them on a GPU), so the
    native UOP_GATHER / UOP_SCATTER are built only when DEV picked the CPU."""
    return Device.DEFAULT == "CPU"


def _host_bytes(arr: np.ndarray, dt: DType) -> bytes:
    """arr's elements in dt's storage; bf16 and fp8 go through the C f32 conversion."""
    if dt.np_dtype is not None:
        return arr.astype(dt.np_dtype, copy=False).tobytes()
    src = np.ascontiguousarray(arr, dtype=np.float32)
    out = (ctypes.c_uint8 * (src.size * dt.itemsize))()
    _TH.dtype_from_f32(out, dt.thvm_id, src, src.size)
    return bytes(out)


def _const_broadcast(value, shape, dtype: DType) -> "Tensor":
    """A lazy constant of `shape`, EXPAND(RESHAPE(CONST)) with no host buffer, like tinygrad's
    const_like (uop/ops.py:603-605)."""
    shape = tuple(int(d) for d in shape)
    if isinstance(value, int) and not isinstance(value, bool) and dtypes.is_int(dtype):
        # An int wraps to the dtype, as tinygrad's const renders in C: 0xFFFFFFFF in int32 is -1.
        bits = 8 * dtype.itemsize
        value &= (1 << bits) - 1
        if not dtypes.is_unsigned(dtype) and value >> (bits - 1):
            value -= 1 << bits
    return Tensor._from_nn(_TH.nn("full", [], [dtype.thvm_id, *shape], [float(value)]), "full")


def _promote(a: "Tensor", b: "Tensor") -> "tuple[Tensor, Tensor]":
    """A mixed pair in its least_upper_dtype (tinygrad mixin/elementwise.py:24-34)."""
    if a._dtype != b._dtype:
        dt = least_upper_dtype(a._dtype, b._dtype)
        return a.cast(dt), b.cast(dt)
    return a, b


def _conv_opts(n: int, groups, stride, dilation, padding, output_padding=None) -> list[int]:
    """A conv's py_nn_* options over n spatial axes: groups, stride[n], dilation[n], then
    (begin, end) per axis in axis order from tinygrad's int, per-axis or per-side (last axis
    first) padding (helpers.py:57-62), then output_padding[n]."""
    tup = (lambda v: (int(v),) * n if isinstance(v, int) else tuple(int(x) for x in v))
    if isinstance(padding, int):
        pads = [padding] * (2 * n)
    elif len(padding) == n:
        pads = [int(q) for q in padding for _ in (0, 1)]
    elif len(padding) == 2 * n:
        pads = [int(q) for pair in zip(padding[-2::-2], padding[::-2]) for q in pair]
    else:
        raise ValueError(f"padding {padding} does not fit {n} spatial axes")
    opts = [groups, *tup(stride), *tup(dilation), *pads]
    return opts if output_padding is None else opts + list(tup(output_padding))


def _normalize_pairs(arg, shape, full):
    """A shrink / pad argument as a flat [b0, e0, b1, e1, ...] list, from tinygrad's per-axis pairs
    (a None pair is full(axis size)) or an already-flat int list."""
    seq = list(arg)
    is_pairs = any(x is None or isinstance(x, (tuple, list)) for x in seq)
    if not is_pairs:
        return [int(x) for x in seq]
    out: list[int] = []
    for i, x in enumerate(seq):
        if x is None:
            b, e = full(shape[i])
        else:
            b, e = x
        out += [int(b), int(e)]
    return out


def _wrap_other(self_t: "Tensor", other) -> "Tensor":
    """A Python scalar as a weak rank-0 CONST (tinygrad mixin/elementwise.py:21-34): it takes the
    tensor's dtype, except that a float scalar makes an int operation float32."""
    if isinstance(other, Tensor):
        return other
    dtype = self_t._dtype
    if isinstance(other, float) and not dtypes.is_float(self_t._dtype):
        dtype = dtypes.float32
    elif isinstance(other, int) and not isinstance(other, bool) and dtype == dtypes.bool:
        dtype = dtypes.int32
    if np.ndim(other) > 0:
        return Tensor(np.asarray(other, dtype=_to_np_dtype(dtype)), dtype=dtype)
    return _const_broadcast(other, (), dtype)


class _TensorMeta(type):
    # tinygrad replaced Tensor.training with the TRAINING ContextVar (#16774); without this,
    # `Tensor.training = True` would set a class attribute nothing reads.
    @property
    def training(cls):
        raise AttributeError(_TRAINING_MOVED)

    @training.setter
    def training(cls, mode):
        raise AttributeError(_TRAINING_MOVED)


_TRAINING_MOVED = ("Tensor.training was removed (tinygrad #16774): use "
                   "`with Context(TRAINING=1):` or `@Context(TRAINING=1)`")


class Tensor(OpMixin, metaclass=_TensorMeta):
    """A thvm term (a lazy UOp graph or a realized TAG_TEN) plus its dtype and shape."""

    __slots__ = ("term", "_dtype", "_shape", "is_param", "grad",
                 "_pin_id", "_view_base", "_view_begin_end", "__weakref__")

    # Identity hashing: __eq__ is overridden to build an elementwise mask
    # (tinygrad semantics), which would otherwise make Tensor unhashable.
    def __hash__(self):
        return id(self)

    # ---- construction --------------------------------------------------

    def __init__(self, data=None, dtype: DType | None = None,
                 device: str | None = None):
        del device  # accepted for tinygrad compat; DEV picks at init
        if isinstance(data, Tensor):
            self.term = data.term
            self._dtype = data._dtype
            self._shape = data._shape
        elif data is None:
            self.term = Term(0)
            self._dtype = to_dtype(dtype) if dtype is not None else dtypes.default_float
            self._shape = ()
        else:
            # tinygrad's inference (tensor.py:72-86): a numpy array keeps its dtype, bytes are u8,
            # a Python value takes from_py's.
            if isinstance(data, (np.ndarray, np.generic)):
                dt = to_dtype(dtype) if dtype is not None else _from_np_dtype(data.dtype)
            elif isinstance(data, bytes):
                data, dt = np.frombuffer(data, np.uint8), to_dtype(dtype or dtypes.uint8)
            else:
                dt = to_dtype(dtype) if dtype is not None else dtypes.from_py(data)
            arr = np.asarray(data).astype(_to_np_dtype(dt), copy=False)
            if arr.ndim >= 1 and not arr.flags["C_CONTIGUOUS"]:   # 0-D would become (1,)
                arr = np.ascontiguousarray(arr)
            self._dtype = dt
            self._shape = tuple(int(d) for d in arr.shape)
            _TH.reclaim_if_due()
            self.term = _TH.ten_create(dt.thvm_id, list(self._shape))
            if arr.size > 0:
                _TH.ten_write(self.term, _host_bytes(arr, dt))
        # is_param only splits an optimizer's list into params and buffers (BatchNorm running
        # stats opt out); backward() fills .grad on every in-scope float leaf regardless.
        self.is_param: bool = True
        self.grad: Tensor | None = None
        self._pin_id = 0
        self._view_base = None
        self._view_begin_end = None
        _LIVE_ADD(self)
        self._pin()

    @classmethod
    def _from_term(cls, term: Term, dtype: DType,
                   shape: tuple[int, ...]) -> "Tensor":
        t = cls.__new__(cls)
        t.term = term
        t._dtype = dtype
        t._shape = tuple(int(d) for d in shape)
        t.is_param = True
        t.grad = None
        t._pin_id = 0
        t._view_base = None
        t._view_begin_end = None
        _LIVE_ADD(t)
        t._pin()
        return t

    # ---- live-tensor pinning (cross-step buffer reclaim) --------------

    def _pin(self, lazy: bool = False) -> None:
        """Pin the term if it is a TAG_TEN (any term with `lazy`), a root for reclaim() and a
        realize's pool rollback, and map a TAG_TEN's tid to self for backward()."""
        global _PIN_NEXT
        is_ten = int(_TH.term_tag(self.term)) == _TAG_TEN
        if not is_ten and not lazy:
            return
        if self._pin_id == 0:
            self._pin_id = _PIN_NEXT
            _PIN_NEXT += 1
        else:
            _TH.pin_drop(self._pin_id)
        _TH.pin_set(self._pin_id, self.term)
        tid = int(_TH.term_val(self.term)) if is_ten else 0
        # An alias of a live leaf (a no-op reshape or broadcast of it) leaves the
        # tid with that leaf, or the alias's death would drop the leaf's grad.
        if tid > 0 and _LEAF_TENSORS.get(tid) is None:
            _LEAF_TENSORS[tid] = self

    def __del__(self):
        pid = getattr(self, "_pin_id", 0)
        if pid:
            try:
                _TH.pin_drop(pid)
            except Exception:
                pass

    # ---- properties ----------------------------------------------------

    @property
    def shape(self) -> tuple[int, ...]: return self._shape
    @property
    def dtype(self) -> DType: return self._dtype
    @property
    def device(self) -> str: return Device.DEFAULT
    @property
    def ndim(self) -> int: return len(self._shape)

    def numel(self) -> int:
        n = 1
        for d in self._shape: n *= d
        return n

    @property
    def max_shape(self) -> tuple[int, ...]:
        # tinygrad takes each symbolic dim's maximum (mixin/movement.py:49-53); thvm shapes are
        # concrete (a bound Variable is its value), so that is the shape itself.
        return tuple(int(d) for d in self._shape)

    def max_numel(self) -> int:
        return self.numel()

    # ---- realize + host readback --------------------------------------

    def realize(self, *lst: "Tensor", **kwargs) -> "Tensor":
        """Run thvm's pipeline and swap each term for its TAG_TEN; several tensors realize in one
        realize_many pass (tinygrad tensor.py:219-225), so shared kernels emit once."""
        if not lst:
            self.term = _TH.realize(self.term)
            self._pin()
            _TH.reclaim_if_due()
            return self
        # Assigns follow the data: a tensor reading a buffer's prior state realizes before another
        # tensor's assign overwrites it (tinygrad's WAR edges, schedule/__init__.py:57-65); within
        # one tensor the graph order holds, and a cycle raises RuntimeError.
        terms_all = [self.term] + [x.term for x in lst]
        out = _TH.realize_many(terms_all)
        self.term = out[0]
        self._pin()
        for x, r in zip(lst, out[1:]):
            x.term = r
            x._pin()
        _TH.reclaim_if_due()
        return self

    def contiguous(self) -> "Tensor":
        return self.realize()

    def clone(self, device=None) -> "Tensor":
        """Fresh storage holding self's value (tinygrad tensor.py:344-351, uop/ops.py:887-893);
        `device` is ignored, DEV picks thvm's one device."""
        del device
        _TH.reclaim_if_due()
        dst = Tensor._from_term(
            _TH.ten_create(self._dtype.thvm_id, list(self._shape)),
            self._dtype, self._shape)
        # Realized eagerly, so the clone is a new leaf: backward() does not reach self through it,
        # where tinygrad passes the gradient to the source (mixin/gradient.py:100-101).
        ret = dst.assign(self).realize()
        if self.grad is not None:
            ret.grad = self.grad.clone()
        return ret.is_param_(self.is_param)

    def numpy(self) -> np.ndarray:
        """The value on the host; bf16 and fp8 read back as f32, as tinygrad's do."""
        self.realize()
        n = self.numel()
        if n == 0:
            return np.zeros(self._shape, dtype=_to_np_dtype(self._dtype))
        raw = _TH.ten_read(self.term, n * self._dtype.itemsize)
        if self._dtype.np_dtype is None:
            out = np.empty(n, np.float32)
            _TH.dtype_to_f32(out, raw, self._dtype.thvm_id, n)
            return out.reshape(self._shape)
        return np.frombuffer(raw, dtype=self._dtype.np_dtype).reshape(self._shape).copy()

    def tolist(self): return self.numpy().tolist()
    def item(self):   return self.numpy().item()

    def __repr__(self) -> str:
        return f"<Tensor dtype={self._dtype.name} shape={self._shape}>"

    # ---- graph helpers --------

    def detach(self) -> "Tensor":
        """Stop-gradient (tinygrad mixin/elementwise.py:43-47): UOP_DETACH, the identity forward
        and no gradient backward."""
        t = Tensor._from_term(_TH.detach(self.term), self._dtype, self._shape)
        # The result reads self's storage, as tinygrad's DETACH does, so it pins its term to keep
        # that storage after self is gone.
        t._pin(lazy=True)
        return t

    def assign(self, x: "Tensor") -> "Tensor":
        """Lazy in-place update (tinygrad tensor.py:236-274): UOP_ASSIGN(self, x) writes x into
        self's buffer when realized, keeping the buffer, so a JIT capture can replay it."""
        x = x if isinstance(x, Tensor) else Tensor(x, dtype=self._dtype)
        # A shrink-view dst (the kv-cache write `cache[:, :, p:p+n].assign(...)`): UOP_ASSIGN
        # writes one contiguous run at the view's offset, ignoring strides, so scatter on the host.
        if self._view_base is not None and self._view_begin_end is not None:
            base = self._view_base
            base.realize()
            arr = base.numpy()
            be = self._view_begin_end
            sl = tuple(slice(be[2*i], be[2*i+1]) for i in range(base.ndim))
            arr[sl] = x.numpy().reshape(arr[sl].shape)
            new_base = Tensor(arr, dtype=base._dtype).realize()
            # Re-point base at the new buffer so later reads through it see the write.
            base.term = new_base.term
            base._pin()
            self.term = new_base.term
            self._dtype = new_base._dtype
            self._shape = new_base._shape
            self._view_base = None
            self._view_begin_end = None
            self._pin()
            return self
        self.term = _TH.binary(K.ASSIGN, self.term, x.term)
        return self

    def replace(self, x: "Tensor") -> "Tensor":
        """Take x's data, dtype and shape (tinygrad tensor.py:227-234)."""
        x = x if isinstance(x, Tensor) else Tensor(x, dtype=self._dtype)
        self.term = x.term
        self._dtype = x._dtype
        self._shape = x._shape
        self._pin()
        return self

    def is_param_(self, is_param: bool = True) -> "Tensor":
        """Mark whether this is an optimizer parameter (tinygrad tensor.py:124-126); only the
        optimizer's param / buffer split reads it, never backward()."""
        self.is_param = is_param
        return self

    def backward(self, gradient: "Tensor | None" = None) -> "Tensor":
        """Fill .grad on every live in-scope float leaf, with no requires_grad flag (tinygrad
        tensor.py:475-495); the grads stay lazy until realized."""
        if gradient is None:
            if self.numel() != 1:
                raise RuntimeError(
                    "backward(): implicit gradient only for scalar outputs")
            gradient = self.const_like(1.0)
        # tinygrad keeps the float leaves with a device (tensor.py:486-488); a TEN leaf is a
        # buffer, so the float test is all that is left.
        tids = [tid for tid in _TH.uop_leaf_tids(self.term)
                if (leaf := _LEAF_TENSORS.get(tid)) is not None and dtypes.is_float(leaf._dtype)]
        grads, had_grad, err = _TH.backward(self.term, gradient.term, tids)
        if err is not None:
            # tinygrad's compute_gradient raises for a node without a rule
            # (mixin/gradient.py:127); every .grad is rolled back.
            raise RuntimeError(f"backward(): {err}")
        if any(had_grad):
            warnings.warn(
                "backward(): a leaf entered with a non-zero gradient from a prior "
                "backward(); thvm accumulates into TenDesc.grad, so call "
                "opt.zero_grad() between backward passes to avoid stale accumulation "
                "(intentional accumulation can filter this warning).",
                stacklevel=2)
        for tid, g in zip(tids, grads):
            if g != 0:
                leaf = _LEAF_TENSORS[tid]
                leaf.grad = Tensor._from_term(Term(g), leaf._dtype, leaf._shape)
        return self

    def gradient(self, *targets: "Tensor", gradient: "Tensor | None" = None) -> "list[Tensor]":
        """d self / d target for each realized target, leaving every .grad alone (tinygrad
        mixin/op.py:473-488); a target self does not depend on gets zeros."""
        if gradient is None:
            if self._shape != ():
                raise AssertionError("when no gradient is provided, backward must be called on a scalar tensor")
            gradient = self.const_like(1.0)
        if not (dtypes.is_float(self._dtype) and all(dtypes.is_float(t._dtype) for t in targets)):
            raise RuntimeError("only float Tensors have gradient")
        for t in targets:
            t.realize()
        tids = [int(_TH.term_val(t.term)) for t in targets]
        # The C rule accumulates into each leaf's grad; clear it on both sides so this call
        # neither reads nor leaves one.
        for t in targets:
            _TH.ten_clear_grad(t.term)
        grads, _, err = _TH.backward(self.term, gradient.term, tids)
        for t in targets:
            _TH.ten_clear_grad(t.term)
        if err is not None:
            raise RuntimeError(f"gradient(): {err}")
        return [Tensor._from_term(Term(g), t._dtype, t._shape) if g != 0 else t.zeros_like()
                for g, t in zip(grads, targets)]

    def matmul(self, x: "Tensor", dtype: DType | None = None) -> "Tensor":
        out = self @ x
        return out if dtype is None else out.cast(dtype)

    def dot(self, w: "Tensor", dtype: DType | None = None) -> "Tensor":
        return self.matmul(w, dtype)

    def sequential(self, layers) -> "Tensor":
        """Apply a list of callables to self in sequence."""
        out = self
        for layer in layers:
            out = layer(out)
        return out

    # ---- factories ----------------------------------------------------

    @classmethod
    def _from_numpy(cls, arr: np.ndarray, dtype: DType) -> "Tensor":
        return cls(arr, dtype=dtype)

    @classmethod
    def full(cls, shape, fill_value, dtype: DType | None = None, **kw) -> "Tensor":
        """fill_value everywhere, in dtype or the one fill_value implies (tinygrad
        mixin/creation.py full)."""
        dt = to_dtype(dtype) if dtype is not None else dtypes.from_py(fill_value)
        return cls(np.full(argfix(shape), fill_value, dtype=_to_np_dtype(dt)), dtype=dt)

    @classmethod
    def ones(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        return cls.full(argfix(*shape), 1.0, dtype=dtype)

    @classmethod
    def zeros(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        return cls.full(argfix(*shape), 0.0, dtype=dtype)

    @classmethod
    def eye(cls, n: int, m: int | None = None, dtype: DType | None = None, **kw) -> "Tensor":
        dt = to_dtype(dtype) if dtype is not None else dtypes.default_float
        return cls(np.eye(n, m or n, dtype=_to_np_dtype(dt)), dtype=dt)

    @classmethod
    def empty(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        """An unfilled buffer (tinygrad empty): allocated, never written."""
        dt = to_dtype(dtype) if dtype is not None else dtypes.default_float
        shape = tuple(int(d) for d in argfix(*shape))
        _TH.reclaim_if_due()
        return cls._from_term(_TH.ten_create(dt.thvm_id, list(shape)), dt, shape)

    # Random factories draw from numpy's global RNG (manual_seed seeds it); tinygrad draws
    # in-graph (mixin/rand.py), with the same distributions and init formulas.

    @classmethod
    def _random(cls, draw, dtype) -> "Tensor":
        """A host draw; under a TinyJit capture each replay redraws it into the same buffer
        (_misc.jit_record_random), as tinygrad's in-graph RNG gives fresh values per call."""
        dt = to_dtype(dtype) if dtype is not None else dtypes.default_float
        t = cls(np.asarray(draw()).astype(_to_np_dtype(dt)), dtype=dt)
        jit_record_random(t, draw)
        return t

    def _redraw(self, draw) -> None:
        _TH.ten_write(self.term, _host_bytes(np.asarray(draw()).astype(_to_np_dtype(self._dtype)), self._dtype))

    @classmethod
    def rand(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        return cls._random(lambda: np.random.rand(*argfix(*shape)), dtype)

    @classmethod
    def randn(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        return cls._random(lambda: np.random.standard_normal(argfix(*shape)), dtype)

    @classmethod
    def normal(cls, *shape, mean: float = 0.0, std: float = 1.0, dtype=None, **kw) -> "Tensor":
        if std < 0:
            raise ValueError(f"Tensor.normal requires std >= 0, got {std=}")
        return cls._random(lambda: np.random.normal(mean, std, argfix(*shape)), dtype)

    @classmethod
    def uniform(cls, *shape, low: float = 0.0, high: float = 1.0, dtype: DType | None = None,
                **kw) -> "Tensor":
        if low >= high:
            raise ValueError(f"Tensor.uniform requires low < high, got {low=}, {high=}")
        return cls._random(lambda: np.random.uniform(low, high, argfix(*shape)), dtype)

    @classmethod
    def randint(cls, *shape, low: int = 0, high: int = 10, dtype: DType | None = None,
                **kw) -> "Tensor":
        dt = to_dtype(dtype) if dtype is not None else dtypes.int32
        if not dtypes.is_int(dt):
            raise TypeError(f"{dt=} must be int")
        if low >= high:
            raise ValueError(f"Tensor.randint requires low < high, got {low=}, {high=}")
        return cls._random(lambda: np.random.randint(low, high, size=argfix(*shape)), dt)

    @classmethod
    def scaled_uniform(cls, *shape, **kw) -> "Tensor":
        return cls.uniform(*shape, low=-1.0, high=1.0, **kw).mul(math.prod(argfix(*shape)) ** -0.5)

    @classmethod
    def glorot_uniform(cls, *shape, **kw) -> "Tensor":
        s = argfix(*shape)
        bound = (6 / (s[0] + math.prod(s[1:]))) ** 0.5
        return cls.uniform(*shape, low=-bound, high=bound, **kw)

    @classmethod
    def kaiming_uniform(cls, *shape, a: float = 0.01, **kw) -> "Tensor":
        bound = (6 / (1 + a ** 2) / math.prod(argfix(*shape)[1:])) ** 0.5
        return cls.uniform(*shape, low=-bound, high=bound, **kw)

    @classmethod
    def kaiming_normal(cls, *shape, a: float = 0.01, **kw) -> "Tensor":
        std = (2 / (1 + a ** 2) / math.prod(argfix(*shape)[1:])) ** 0.5
        return cls.normal(*shape, mean=0.0, std=std, **kw)

    def rand_like(self, **kw) -> "Tensor":
        return Tensor.rand(*self._shape, dtype=kw.pop("dtype", self._dtype))

    def randn_like(self, dtype: DType | None = None, **kw) -> "Tensor":
        return Tensor.randn(*self._shape, dtype=dtype or self._dtype)

    def full_like(self, fill_value, dtype: DType | None = None, **kw) -> "Tensor":
        return Tensor.full(self._shape, fill_value, dtype=dtype or self._dtype)

    def ones_like(self, **kw) -> "Tensor": return self.full_like(1, **kw)
    def zeros_like(self, **kw) -> "Tensor": return self.full_like(0, **kw)

    # ---- broadcasting (tinygrad rule: align trailing, expand 1s) ------

    def _broadcast(self, other) -> tuple["Tensor", "Tensor"]:
        b = _wrap_other(self, other)
        if self._shape == b._shape:
            return self, b
        r = _TH.broadcast2(self.term, self._shape, b.term, b._shape)
        if r is None:
            raise ValueError(f"cannot broadcast {self._shape} vs {b._shape}")
        ta, tb, out = r
        return Tensor._from_term(ta, self._dtype, out), Tensor._from_term(tb, b._dtype, out)

    # ---- elementwise binary -------------------------------------------

    def _broadcasted(self, y, reverse: bool = False) -> tuple["Tensor", "Tensor"]:
        """self and y broadcast and promoted, y first when reverse (tinygrad
        mixin/elementwise.py:24-34)."""
        a, b = _promote(*self._broadcast(y))
        return (b, a) if reverse else (a, b)

    def _ufix(self, x) -> "Tensor":
        return x if isinstance(x, Tensor) else _wrap_other(self, x)

    def _binop(self, other, opcode: int) -> "Tensor":
        a, b = self._broadcasted(other)
        if opcode == K.ADD and a._dtype == dtypes.bool:
            # A bool sum saturates, as tinygrad's rendered bool does.
            return a | b
        return Tensor._from_term(_TH.binary(opcode, a.term, b.term), a._dtype, a._shape)

    def __add__(self, o):  return self._binop(o, K.ADD)
    def __radd__(self, o): return self.__add__(o)
    def sub(self, x, reverse: bool = False) -> "Tensor":
        """a + (-b) after the broadcast and promotion (tinygrad mixin/elementwise.py:102-122), so
        a bool b is negated as a number, not by logical_not."""
        if isinstance(x, (int, float)) and not isinstance(x, bool) and not reverse:
            return self.__add__(-x)
        a, b = self._broadcasted(x, reverse)
        return a.__add__(-b)

    def __sub__(self, o):  return self.sub(o)
    def __rsub__(self, o): return self.sub(o, True)
    def __mul__(self, o):  return self._binop(o, K.MUL)
    def __rmul__(self, o): return self.__mul__(o)

    def div(self, x, reverse: bool = False, rounding_mode=None) -> "Tensor":
        """a * (1 / b), an int or bool pair in float32 (tinygrad mixin/elementwise.py:229-259); an
        int pair with a rounding_mode divides as ints, a zero divisor giving 0."""
        if rounding_mode not in (None, "trunc", "floor"):
            raise RuntimeError(f"{rounding_mode=} is not supported")
        if (isinstance(x, (int, float)) and not reverse and rounding_mode is None
                and dtypes.is_float(self._dtype)):
            # tinygrad folds 1 / b for a constant b.
            return self * (1.0 / x if x else math.copysign(math.inf, x))
        a, b = self._broadcasted(x, reverse)
        if dtypes.is_int(a._dtype) and rounding_mode is not None:
            # tinygrad's CDIV, and FLOORDIV as its floordiv_to_idiv (codegen/decomp/op.py:54-56).
            q = a._int_op(b, K.IDIV)
            if rounding_mode == "trunc":
                return q
            return q - (a._int_op(b, K.IMOD).ne(0) & (a < 0).ne(b < 0)).cast(a._dtype)
        if not dtypes.is_float(a._dtype):
            a, b = a.float(), b.float()
        d = a * b.reciprocal()
        if rounding_mode is None:
            return d
        if rounding_mode == "trunc":
            return d.trunc()
        return d.floor()

    def __truediv__(self, o):  return self.div(o)
    def __rtruediv__(self, o): return self.div(o, True)

    def __neg__(self) -> "Tensor":
        if self._dtype == dtypes.bool:
            return self.logical_not()
        return Tensor._from_term(_TH.neg(self.term), self._dtype,
                                 self._shape)

    def add(self, o): return self.__add__(o)
    def mul(self, o): return self.__mul__(o)

    def trunc(self) -> "Tensor":
        """Toward zero through an int cast, thvm having no TRUNC (tinygrad
        mixin/elementwise.py:469-477): a float past 2**mantissa (or inf, nan) is its own."""
        if not dtypes.is_float(self._dtype):
            return self
        mant = dtypes.finfo(self._dtype)[1]
        cut = self.cast(dtypes.int64 if mant > 30 else dtypes.int32).cast(self._dtype)
        return (self.abs() < 2.0 ** mant).where(cut, self)

    # ---- compare and logic (tinygrad mixin/elementwise.py:144-375) ----

    def _cmp(self, x, opcode: int, reverse: bool = False) -> "Tensor":
        # thvm's compare yields 0 / 1 in the operands' dtype, so the mask casts to bool.
        a, b = self._broadcasted(x, reverse)
        return Tensor._from_term(_TH.binary(opcode, a.term, b.term), a._dtype, a._shape).cast(dtypes.bool)

    def __lt__(self, x): return self._cmp(x, K.CMPLT)
    def __gt__(self, x): return self._cmp(x, K.CMPLT, True)
    def __ge__(self, x): return (self < x).logical_not()
    def __le__(self, x): return (self > x).logical_not()
    def eq(self, x):     return self._cmp(x, K.CMPEQ)
    def ne(self, x):     return self.eq(x).logical_not()
    def __eq__(self, x): return self.eq(x)
    def __ne__(self, x): return self.ne(x)
    def logical_not(self) -> "Tensor": return self.cast(dtypes.bool).eq(False)

    def _int_op(self, b: "Tensor", opcode: int) -> "Tensor":
        # An int binop on operands _broadcasted already promoted.
        return Tensor._from_term(_TH.binary(opcode, self.term, b.term), self._dtype, self._shape)

    def _bit_op(self, x, reverse: bool, opcode: int) -> "Tensor":
        # tinygrad's Ops.AND / OR / XOR / SHL / SHR binops (mixin/elementwise.py:159-200, 344-364),
        # in the operands' promoted int dtype; a bool and / or / xor is a select.
        a, b = self._broadcasted(x, reverse)
        if a._dtype == dtypes.bool and opcode in (K.IAND, K.IOR, K.IXOR):
            if opcode == K.IAND:
                return a.where(b, False)
            return a.where(True, b) if opcode == K.IOR else a.where(b.logical_not(), b)
        if not dtypes.is_int(a._dtype):
            raise RuntimeError(f"{a._dtype} is not supported")
        return a._int_op(b, opcode)

    def bitwise_and(self, x, reverse: bool = False): return self._bit_op(x, reverse, K.IAND)
    def bitwise_or(self, x, reverse: bool = False):  return self._bit_op(x, reverse, K.IOR)
    def bitwise_xor(self, x, reverse: bool = False): return self._bit_op(x, reverse, K.IXOR)
    def __and__(self, x):  return self.bitwise_and(x)
    def __or__(self, x):   return self.bitwise_or(x)
    def __xor__(self, x):  return self.bitwise_xor(x)
    def __rand__(self, x): return self.bitwise_and(x, True)
    def __ror__(self, x):  return self.bitwise_or(x, True)
    def __rxor__(self, x): return self.bitwise_xor(x, True)

    def bitwise_not(self) -> "Tensor":
        # ~x as -x - 1, or max - x unsigned, which agree with tinygrad's x ^ -1 / x ^ max.
        if self._dtype == dtypes.bool:
            return self.logical_not()
        if not dtypes.is_int(self._dtype):
            raise RuntimeError(f"{self._dtype} has no bitwise_not")
        return self._dtype.max - self if dtypes.is_unsigned(self._dtype) else -self - 1

    def __invert__(self): return self.bitwise_not()

    def lshift(self, x, reverse: bool = False) -> "Tensor": return self._bit_op(x, reverse, K.ISHL)
    def rshift(self, x, reverse: bool = False) -> "Tensor": return self._bit_op(x, reverse, K.ISHR)
    def __lshift__(self, x):  return self.lshift(x)
    def __rshift__(self, x):  return self.rshift(x)
    def __rlshift__(self, x): return self.lshift(x, True)
    def __rrshift__(self, x): return self.rshift(x, True)

    def pow(self, x, reverse: bool = False) -> "Tensor":
        """self ** x, or x ** self when reverse (tinygrad mixin/elementwise.py:547-570)."""
        if not reverse and isinstance(x, int) and x < 0 and not dtypes.is_float(self._dtype):
            raise RuntimeError("integers to negative integer powers are not allowed")
        o = _wrap_other(self, x)
        b, e = (o, self) if reverse else (self, o)
        return Tensor._from_nn(_TH.nn("pow", [b._nn(), e._nn()]), "pow")

    def __pow__(self, x): return self.pow(x)
    def __rpow__(self, x): return self.pow(x, True)

    def __matmul__(self, other: "Tensor") -> "Tensor":
        """Matmul as RESHAPE, EXPAND, MUL and REDUCE, which thvm fuses; N-D operands take
        tinygrad's dot (src/nn/dot.c)."""
        r = _TH.nn("dot", [self._nn(), other._nn()])
        if r is None:
            raise RuntimeError(f"cannot dot {self._shape} and {other._shape}")
        return Tensor._from_nn(r, "dot")

    # ---- elementwise unary --------------------------------------------

    def _unary(self, opcode: int) -> "Tensor":
        return Tensor._from_term(_TH.unary(opcode, self.term),
                                 self._dtype, self._shape)

    def neg(self) -> "Tensor":   return self.__neg__()
    def recip(self) -> "Tensor": return self._unary(K.RECIP)
    def reciprocal(self) -> "Tensor": return self._unary(K.RECIP)
    def exp2(self) -> "Tensor":  return self._unary(K.EXP2)
    def log2(self) -> "Tensor":  return self._unary(K.LOG2)
    def sqrt(self) -> "Tensor":  return self._unary(K.SQRT)

    def sin(self) -> "Tensor":
        # tinygrad mixin/elementwise.py:489-497, an int in float32.
        x = self if dtypes.is_float(self._dtype) else self.float()
        return x._unary(K.SIN)
    def _elementwise(self, op: str) -> "Tensor":
        """A composite elementwise op of src/nn/elementwise.c, by name."""
        return Tensor._from_nn(_TH.nn(op, [self._nn()]), op)

    def rsqrt(self) -> "Tensor":   return self._elementwise("rsqrt")
    def exp(self) -> "Tensor":     return self._elementwise("exp")
    def log(self) -> "Tensor":     return self._elementwise("log")
    def sign(self) -> "Tensor":    return self._elementwise("sign")
    def abs(self) -> "Tensor":     return self._elementwise("abs")
    def sigmoid(self) -> "Tensor": return self._elementwise("sigmoid")
    def tanh(self) -> "Tensor":    return self._elementwise("tanh")

    def softmax(self, axis: int = -1) -> "Tensor":
        if self.ndim == 0:
            return self.reshape(1).softmax(self._resolve_dim(axis)).reshape(())
        return Tensor._from_nn(_TH.nn("softmax", [self._nn()], [axis, 0]), "softmax")

    def softmin(self, axis: int = -1) -> "Tensor":
        # tinygrad mixin/op.py:709-729.
        return (-self).softmax(axis=axis)

    def log_softmax(self, axis: int = -1) -> "Tensor":
        if self.ndim == 0:
            return self.reshape(1).log_softmax(self._resolve_dim(axis)).reshape(())
        return Tensor._from_nn(_TH.nn("softmax", [self._nn()], [axis, 1]), "log_softmax")

    def sparse_categorical_crossentropy(self, Y: "Tensor", ignore_index: int = -1,
                                        label_smoothing: float = 0.0, reduction: str = "mean",
                                        axis: int = -1) -> "Tensor":
        """Mean NLL of log_softmax(self) at the int labels Y (src/nn/
        sparse_categorical_crossentropy.c), the one-hot an in-graph compare, so a TinyJit replay
        reads each step's labels; other arguments take tinygrad's composition."""
        if ignore_index != -1 or label_smoothing != 0.0 or reduction != "mean":
            return self._sparse_categorical_crossentropy(Y, ignore_index, label_smoothing, reduction)
        r = _TH.nn("sparse_categorical_crossentropy", [self._nn(), Y._nn()], [axis])
        return Tensor._from_nn(r, f"sparse_categorical_crossentropy {self._shape} {Y._shape}")

    def argmax(self, axis=None, keepdim: bool = False) -> "Tensor":
        """Index of the max along `axis` (the flattened tensor for None):
        tinygrad mixin/op.py:863-888, ties to the first index."""
        if axis is None:
            return self.flatten().argmax(0)
        ax = axis if axis >= 0 else axis + self.ndim
        n = self._shape[ax]
        m = self == self.max(axis=ax, keepdim=True)
        ramp = Tensor.arange(n, 0, -1, dtype=dtypes.float32)
        idx = m * ramp.reshape(n, *[1] * (self.ndim - ax - 1))
        return (n - idx.max(axis=ax, keepdim=keepdim)).cast(dtypes.int32)

    def relu(self) -> "Tensor":
        return self._elementwise("relu")

    def elu(self, alpha: float = 1.0) -> "Tensor":
        # tinygrad mixin/elementwise.py:962-972: its gradient at 0 is alpha.
        return (self > 0).where(self, alpha * ((self - self.relu()).exp() - 1))

    def maximum(self, other) -> "Tensor":
        r = _TH.nn("maximum", [self._nn(), _wrap_other(self, other)._nn()], [0])
        return Tensor._from_nn(r, f"maximum {self._shape}")

    def minimum(self, other) -> "Tensor":
        r = _TH.nn("maximum", [self._nn(), _wrap_other(self, other)._nn()], [1])
        return Tensor._from_nn(r, f"minimum {self._shape}")

    def square(self) -> "Tensor":
        return self._elementwise("square")

    def where(self, x, y) -> "Tensor":
        """x where self is nonzero, else y (src/nn/where.c): a true select, so a non-finite
        unselected value never leaks in; two Python values take their own dtypes."""
        if not isinstance(x, Tensor) and not isinstance(y, Tensor):
            dt = least_upper_dtype(dtypes.from_py(x), dtypes.from_py(y))
            x, y = _const_broadcast(x, (), dt), _const_broadcast(y, (), dt)
        ref = x if isinstance(x, Tensor) else y
        args = [self._nn(), _wrap_other(ref, x)._nn(), _wrap_other(ref, y)._nn()]
        return Tensor._from_nn(_TH.nn("where", args), "where")

    def newton_schulz(self, steps: int, params, eps: float = 1.0e-7) -> "Tensor":
        """Newton-Schulz for an odd polynomial: each step G <- sum_i params[i] * (G G^T)^i G, from
        the Frobenius-normalized G (tinygrad mixin/op.py:1875-1890)."""
        assert self.ndim > 1, "NS only works for two or more dims"
        if self._shape[-2] > self._shape[-1]:
            return self.transpose(-2, -1).newton_schulz(
                steps, params, eps).transpose(-2, -1)
        G = self / (self.square().sum(axis=(-2, -1), keepdim=True).sqrt() + eps)
        # tinygrad's reduce over [G]*i forms every (y @ y.T) factor from this step's G, so
        # G G^T is built once per step.
        for _ in range(steps):
            ggt = G @ G.transpose(-2, -1)
            acc = None
            for i, p in enumerate(params):
                x = G
                for _j in range(i):
                    x = ggt @ x
                term = x * p
                acc = term if acc is None else acc + term
            G = acc
        return G

    # ---- reductions ---------------------------------------------------

    def _reduce_axes(self, axis) -> list[int]:
        # tinygrad mixin/reduce.py:13-18: each axis resolved, none for a 0-D tensor.
        axes = range(self.ndim) if axis is None else [self._resolve_dim(a) for a in make_tuple(axis, 1)]
        return [] if self.ndim == 0 else sorted(set(axes))

    def _reduce_count(self, axis) -> int:
        return math.prod(self._shape[a] for a in self._reduce_axes(axis))

    def _reduce(self, kind: int, axis, keepdim: bool) -> "Tensor":
        axes = self._reduce_axes(axis)
        t = _TH.reduce(kind, axes, self.term)
        # UOP_REDUCE drops the reduced axes; keepdim reshapes them back as 1s.
        if keepdim:
            shape = tuple(1 if i in axes else s for i, s in enumerate(self._shape))
            return Tensor._from_term(_TH.reshape(t, list(shape)), self._dtype, shape)
        shape = tuple(s for i, s in enumerate(self._shape) if i not in axes)
        return Tensor._from_term(t, self._dtype, shape)

    def sum(self, axis=None, keepdim: bool = False, dtype=None) -> "Tensor":
        """In sum_acc_dtype, or dtype, a half sum cast back (tinygrad mixin/reduce.py:20-45)."""
        acc = sum_acc_dtype(self._dtype) if dtype is None else to_dtype(dtype)
        ret = self.cast(acc)._reduce(K.REDUCE_SUM, axis, keepdim)
        halves = (dtypes.f16, dtypes.bf16, *dtypes.fp8s)
        return ret.cast(self._dtype) if dtype is None and self._dtype in halves else ret

    def max(self, axis=None, keepdim: bool = False) -> "Tensor":
        return self._reduce(K.REDUCE_MAX, axis, keepdim)

    def prod(self, axis=None, keepdim: bool = False, dtype=None) -> "Tensor":
        """In dtype, else self's (tinygrad mixin/reduce.py:47-71)."""
        return (self if dtype is None else self.cast(to_dtype(dtype)))._reduce(K.REDUCE_PROD, axis, keepdim)

    def mean(self, axis=None, keepdim: bool = False) -> "Tensor":
        """tinygrad mixin/op.py:519-540, 0 / 0 over an empty axis."""
        out = self._dtype if dtypes.is_float(self._dtype) else dtypes.float32
        div = self._reduce_count(axis)
        numerator = self.cast(sum_acc_dtype(self._dtype)).sum(axis=axis, keepdim=keepdim)
        return (numerator * (1.0 / div if div else math.nan)).cast(out)

    def var(self, axis=None, keepdim: bool = False,
            correction=1) -> "Tensor":
        """Variance with Bessel's `correction` (tinygrad mixin/op.py:523-549); as with tinygrad's
        smax(n - correction, 0) divisor, n <= correction divides by zero instead of raising."""
        output_dtype = (self._dtype if dtypes.is_float(self._dtype)
                        else dtypes.float32)
        squares = (self - self.mean(axis=axis, keepdim=True)).square()
        # tinygrad sums in sum_acc_dtype(squares.dtype) (mixin/op.py:548); the
        # squares are already float32, since mean promotes int input.
        numerator = squares.sum(axis=axis, keepdim=keepdim)
        n = self._reduce_count(axis)
        return (numerator / Tensor(float(max(n - correction, 0)))).cast(output_dtype)

    def var_mean(self, axis=None, keepdim: bool = False,
                 correction=1) -> "tuple[Tensor, Tensor]":
        return self.var(axis, keepdim, correction), self.mean(axis, keepdim)

    def std(self, axis=None, keepdim: bool = False,
            correction=1) -> "Tensor":
        """Standard deviation (tinygrad mixin/op.py:568-590)."""
        return self.var(axis, keepdim, correction).sqrt()

    def std_mean(self, axis=None, keepdim: bool = False,
                 correction=1) -> "tuple[Tensor, Tensor]":
        return self.std(axis, keepdim, correction), self.mean(axis, keepdim)

    # ---- movement -----------------------------------------------------

    def reshape(self, *shape) -> "Tensor":
        if len(shape) == 1 and isinstance(shape[0], (tuple, list)):
            shape = tuple(shape[0])
        # tinygrad allows None at a position to mean "keep this dim".
        shape = tuple(self._shape[i] if d is None else d
                      for i, d in enumerate(shape))
        s = list(int(d) for d in shape)
        if -1 in s:
            if s.count(-1) > 1:
                # tinygrad mixin/movement.py:177-178.
                raise RuntimeError(f"only one dimension can be inferred using -1, getting {tuple(s)}")
            known = 1
            for d in s:
                if d != -1:
                    known *= d
            if known == 0:
                raise ValueError(f"cannot infer reshape: {s}")
            s[s.index(-1)] = self.numel() // known
        shape = tuple(s)
        if math.prod(shape) != self.numel():
            raise ValueError(f"size mismatch, can't reshape ({self._shape}) -> ({shape})")
        if shape == self._shape:
            # tinygrad's reshape returns self (mixin/movement.py:184), so a view keeps its base.
            return self
        t = _TH.reshape(self.term, list(shape))
        return Tensor._from_term(t, self._dtype, shape)

    def permute(self, *axes) -> "Tensor":
        """tinygrad mixin/movement.py permute: a negative axis counts from the end."""
        order = [self._resolve_dim(a) for a in argfix(*axes)]
        if sorted(order) != list(range(self.ndim)):
            raise RuntimeError(f"order is not a valid permutation, getting {order}")
        t = _TH.permute(self.term, order)
        return Tensor._from_term(t, self._dtype, tuple(self._shape[a] for a in order))

    def transpose(self, dim0: int = 1, dim1: int = 0) -> "Tensor":
        """tinygrad mixin/movement.py:330-343."""
        order = list(range(self.ndim))
        order[dim0], order[dim1] = order[dim1], order[dim0]
        return self.permute(*order)

    def expand(self, shape, *args) -> "Tensor":
        """tinygrad mixin/movement.py:149-162: -1 or None keeps an axis, a shorter shape aligns
        right."""
        new_shape = tuple(f if t == -1 or t is None else t
                          for f, t in zip(*_align_left(self._shape, argfix(shape, *args))))
        return self._broadcast_to(new_shape)

    def _broadcast_to(self, new_shape) -> "Tensor":
        # The new leading axes as 1s, then one EXPAND (tinygrad mixin/movement.py:128-147).
        new_shape = tuple(int(d) for d in new_shape)
        if self._shape == new_shape:
            return self
        shape = (1,) * (len(new_shape) - self.ndim) + self._shape
        if self.ndim > len(new_shape) or not all(s == ns or s == 1 for s, ns in zip(shape, new_shape)):
            raise ValueError(f"cannot broadcast {self._shape} to {new_shape=}")
        x = self if shape == self._shape else self.reshape(shape)
        return Tensor._from_term(_TH.expand(x.term, list(new_shape)), self._dtype, new_shape)

    def shrink_to(self, *shape) -> "Tensor":
        """tinygrad-style: shrink to target dims from offset 0."""
        if len(shape) == 1 and isinstance(shape[0], (tuple, list)):
            shape = tuple(shape[0])
        begin_end: list[int] = []
        for i, s in enumerate(shape):
            if s is None:
                begin_end.extend([0, self._shape[i]])
            else:
                begin_end.extend([0, int(s)])
        return self.shrink(begin_end)

    def _nn(self):
        """This tensor as the src/nn ops take it: (term, dtype id, shape)."""
        return (self.term, self._dtype.thvm_id, self._shape)

    @staticmethod
    def _from_nn(r, what: str) -> "Tensor":
        if r is None:
            raise ValueError(f"{what}: invalid shapes or arguments")
        t, dt, shape = r
        return Tensor._from_term(t, from_thvm_id(dt), shape)

    def _pool(self, k_, stride=1, dilation=1) -> "Tensor":
        """tinygrad's _pool unfold (src/nn/pool.c): shape (..., *o_, *k_), each (o, k) slice the
        window at output position o."""
        k_ = (int(k_),) if isinstance(k_, int) else tuple(int(k) for k in k_)
        s_ = (stride,) * len(k_) if isinstance(stride, int) else tuple(stride)
        d_ = (dilation,) * len(k_) if isinstance(dilation, int) else tuple(dilation)
        assert len(k_) == len(s_) == len(d_)
        if 0 in k_:
            # An empty window: tinygrad's output extents ceildiv(i + d, s), each window empty.
            i_ = self._shape[-len(k_):]
            o_ = [-(-(i - d * (k - 1)) // s) for i, d, k, s in zip(i_, d_, k_, s_)]
            return Tensor.zeros(*self._shape[:-len(k_)], *o_, *k_, dtype=self._dtype)
        return Tensor._from_nn(_TH.nn("pool", [self._nn()], [*k_, *s_, *d_]), "_pool")

    def _pad_zero(self, pX) -> "Tensor":
        """Zeros by non-negative per-axis (before, after) pairs, self for none (tinygrad
        mixin/movement.py:186-190)."""
        if all(b == 0 and e == 0 for b, e in pX):
            return self
        new_shape = tuple(s + int(b) + int(e) for s, (b, e) in zip(self._shape, pX))
        t = _TH.pad(self.term, [int(v) for p in pX for v in p])
        return Tensor._from_term(t, self._dtype, new_shape)

    def shrink(self, begin_end) -> "Tensor":
        """A sub-range per axis, by tinygrad's per-axis (begin, end) pairs (None: the whole axis) or
        a flat [b0, e0, b1, e1, ...]."""
        begin_end = _normalize_pairs(begin_end, self._shape,
                                     full=lambda n: (0, n))
        new_shape = tuple(int(begin_end[2*i+1]) - int(begin_end[2*i])
                          for i in range(self.ndim))
        if new_shape == self._shape:
            # tinygrad's shrink returns self (mixin/movement.py:192-212).
            return self
        t = _TH.shrink(self.term, [int(x) for x in begin_end])
        out = Tensor._from_term(t, self._dtype, new_shape)
        # Kept so assign() into this view can scatter into the base (see assign()).
        out._view_base = self
        out._view_begin_end = [int(x) for x in begin_end]
        return out

    def size(self, dim: int | None = None):
        """The shape, or one axis of it (tinygrad mixin/movement.py:59-72)."""
        return self._shape if dim is None else self._shape[self._resolve_dim(dim)]

    def flip(self, axis, *args) -> "Tensor":
        """The axes reversed (tinygrad mixin/movement.py:233-255)."""
        axes = [self._resolve_dim(a) for a in argfix(axis, *args)]
        if len(axes) != len(set(axes)):
            raise RuntimeError(f"dim can appear at most once, getting {axes}")
        if not axes:
            return self
        mask = sum(1 << a for a in set(axes))
        return Tensor._from_term(_TH.flip(self.term, mask), self._dtype, self._shape)

    def split(self, sizes, dim: int = 0) -> "tuple[Tensor, ...]":
        """Pieces of `sizes` along dim, an int size leaving a shorter last piece (tinygrad
        mixin/movement.py:423-448)."""
        dim = self._resolve_dim(dim)
        n = self._shape[dim]
        if isinstance(sizes, int):
            sizes = [min(sizes, n - i) for i in range(0, max(1, n), max(1, sizes))]
        out, at = [], 0
        for sz in sizes:
            out.append(self.shrink([(at, at + sz) if a == dim else None for a in range(self.ndim)]))
            at += sz
        return tuple(out)

    def chunk(self, chunks: int, dim: int = 0) -> "list[Tensor]":
        """chunks pieces of ceil(size / chunks) along dim (tinygrad mixin/movement.py:449-473)."""
        n = self._shape[self._resolve_dim(dim)]
        return list(self.split(-(-n // chunks) if n else [0] * chunks, dim=dim))

    def unsqueeze(self, dim: int) -> "Tensor":
        """A new axis of size 1 at `dim` (tinygrad mixin/movement.py:308-321)."""
        dim = self._resolve_dim(dim, extra=1)
        return self.reshape(*self._shape[:dim], 1, *self._shape[dim:])

    def cat(self, *args: "Tensor", dim: int = 0) -> "Tensor":
        """Concatenate self and `args` along `dim` (tinygrad mixin/op.py:754-773): equal pieces
        stack and flatten, others are zero-padded to the output extent and the pads summed."""
        dim = self._resolve_dim(dim)
        for arg in args:
            assert arg.ndim == self.ndim and all(
                ti == ai for i, (ti, ai) in enumerate(zip(self._shape, arg._shape)) if i != dim)
        if all(t._shape[dim] == self._shape[dim] for t in args):
            return self.stack(*args, dim=dim).flatten(dim, dim + 1)
        return Tensor._cat_padded([self, *args], dim)

    @staticmethod
    def _cat_padded(tensors: list, dim: int) -> "Tensor":
        # The pads sum, so a -0.0 comes out as +0.0, where tinygrad's STACK keeps it.
        dim_cumsum = list(itertools.accumulate([t._shape[dim] for t in tensors], initial=0))
        padded = [t.pad(tuple((dim_cumsum[i], dim_cumsum[-1] - dim_cumsum[i + 1]) if j == dim else None
                              for j in range(t.ndim)))
                  for i, t in enumerate(tensors)]
        return functools.reduce(Tensor.__add__, padded[1:], padded[0])

    def stack(self, *args: "Tensor", dim: int = 0) -> "Tensor":
        """Stack self and `args` along a new axis `dim` (tinygrad mixin/movement.py:256-272); with
        no STACK op the unsqueezed pieces concatenate padded."""
        tensors = argfix(self, *args)
        dim = tensors[0]._resolve_dim(dim, extra=1)
        assert all(t._shape == tensors[0]._shape for t in tensors), \
            f"all shapes must match for stack, got {[t._shape for t in tensors]}"
        # tinygrad's STACK of one source is the source (uop/ops.py:110).
        if len(tensors) == 1:
            return tensors[0].unsqueeze(dim)
        return Tensor._cat_padded([t.unsqueeze(dim) for t in tensors], dim)

    # ---- indexed ops (native UOP_GATHER on the CPU backend) -----------

    def _resolve_dim(self, dim: int, *, extra: int = 0) -> int:
        # tinygrad mixin/movement.py:81-84.
        total = self.ndim + extra
        if not -max(1, total) <= dim <= max(1, total) - 1:
            raise IndexError(
                f"{dim=} out of range {[-max(1, total), max(1, total) - 1]}")
        return dim + total if dim < 0 else dim

    def gather(self, dim: int, index: "Tensor") -> "Tensor":
        """Values of self along `dim` at `index`, 0 out of range (tinygrad mixin/op.py:1023-1041):
        a differentiable UOP_GATHER on the CPU, elsewhere a host gather without gradient."""
        if index.ndim != self.ndim:
            raise RuntimeError(
                f"self.ndim must equal index.ndim, {self.ndim=}, {index.ndim=}")
        dim = self._resolve_dim(dim)
        assert all(s >= i for d, (s, i) in enumerate(zip(self._shape, index._shape))
                   if d != dim), \
            "requires self.shape[d] >= index.shape[d] for all d != dim"
        if not dtypes.is_int(index._dtype):
            raise RuntimeError(
                f"_one_hot_along_dim expects int index tensor, getting {index._dtype}")
        if index.numel() == 0 or self._shape[dim] == 0 or not _native_indexing():
            return self._gather_host(dim, index)
        t = _TH.gather(self.term, index.term, dim)
        if int(t) == 0:
            raise RuntimeError(
                f"gather: cannot index {self._shape} along {dim} by {index._shape}")
        return Tensor._from_term(t, self._dtype, index._shape)

    def _gather_host(self, dim: int, index: "Tensor") -> "Tensor":
        """gather on the host: an empty index or axis (no kernel to run), or
        a device without native gather kernels."""
        out = np.zeros(index._shape, dtype=_to_np_dtype(self._dtype))
        if out.size and self._shape[dim]:
            idx = index.numpy().astype(np.int64)
            sl = tuple(slice(None) if d == dim else slice(0, s)
                       for d, s in enumerate(index._shape))
            valid = (idx >= 0) & (idx < self._shape[dim])
            vals = np.take_along_axis(self.numpy()[sl], np.where(valid, idx, 0), axis=dim)
            out = np.where(valid, vals, out)
        return Tensor(out, dtype=self._dtype)

    def _pre_scatter(self, dim: int, index: "Tensor", src: "Tensor"
                     ) -> "tuple[int, Tensor]":
        """tinygrad's _pre_scatter checks in its order and exception types (mixin/op.py:1077-1093,
        then _one_hot_along_dim's int check, :1006); returns dim and src shrunk to index (:1088)."""
        dim = self._resolve_dim(dim)
        assert index.ndim == self.ndim == src.ndim, (
            f"self.ndim, index.ndim and src.ndim must all equal, "
            f"{self.ndim=} {index.ndim=} {src.ndim=}")
        assert all((d == dim or s >= i) and r >= i for d, (s, i, r) in
                   enumerate(zip(self._shape, index._shape, src._shape))), (
            f"All dimensions of {index.shape=} should be <= to all dimensions of "
            f"{src.shape=} and all dimensions except dimension {dim} of {self.shape=}")
        if self._dtype != src._dtype:
            raise RuntimeError(f"expect {self.dtype=} to be equal to {src.dtype=}")
        if src._shape != index._shape:
            src = src.shrink_to(index._shape)
        if not dtypes.is_int(index._dtype):
            raise RuntimeError(
                f"_one_hot_along_dim expects int index tensor, getting {index._dtype}")
        return dim, src

    def _scatter_native(self, dim: int, index: "Tensor", src: "Tensor", kind: int,
                        include_self: bool) -> "Tensor":
        t = _TH.scatter(self.term, index.term, src.term, dim, kind, include_self)
        if int(t) == 0:
            raise RuntimeError(f"scatter: cannot scatter {src._shape} into "
                               f"{self._shape} along {dim} by {index._shape}")
        return Tensor._from_term(t, self._dtype, self._shape)

    def _scatter_reduced(self, dim: int, index: "Tensor", src: "Tensor",
                         reduce: "str | None", include_self: bool) -> "Tensor":
        """Scatter for reduce "sum", "prod", "mean", "amax", "amin" or None (the last write wins): native
        UOP_SCATTERs on the CPU (two for mean), else the host; empty operands run no kernel."""
        if self.numel() == 0:
            dt = dtypes.float32 if reduce == "mean" else self._dtype
            return Tensor(np.zeros(self._shape, dtype=_to_np_dtype(dt)), dtype=dt)
        if index.numel() == 0:
            # Nothing lands: tinygrad keeps self exactly (mean in float32), but its amax / amin turn
            # a NaN self into the identity (mixin/op.py:1129-1130) where this keeps the NaN; its
            # reduce=None merge cannot broadcast the empty one-hot axis and raises (:1170-1177).
            if reduce is None:
                moved = list(index._shape) + [self._shape[dim]]
                moved[dim], moved[-1] = moved[-1], moved[dim]
                raise IndexError("shape mismatch: objects cannot be broadcast to a single shape "
                                 f"({tuple(moved)}, {tuple(moved)}, {tuple(self._shape)})")
            return (self.cast(dtypes.float32) if reduce == "mean" else self) * 1
        if not _native_indexing():
            return self._scatter_host(dim, index, src, reduce, include_self)
        if reduce is None:
            return self._scatter_native(dim, index, src, K.SCATTER_REPLACE, False)
        if reduce in ("amax", "amin", "prod"):
            kind = {"amax": K.SCATTER_MAX, "amin": K.SCATTER_MIN, "prod": K.SCATTER_PROD}[reduce]
            return self._scatter_native(dim, index, src, kind, include_self)
        total = self._scatter_native(dim, index, src, K.SCATTER_SUM, include_self)
        if reduce == "sum":
            return total
        # tinygrad's count (mixin/op.py:1132): the hits, plus 1 where self
        # takes part.  A SUM scatter of ones onto ones adds exactly that.
        count = _const_broadcast(1, self._shape, dtypes.float32)._scatter_native(
            dim, index, _const_broadcast(1, index._shape, dtypes.float32),
            K.SCATTER_SUM, include_self)
        return total.cast(dtypes.float32) / count

    def _scatter_host(self, dim: int, index: "Tensor", src: "Tensor",
                      reduce: "str | None", include_self: bool) -> "Tensor":
        """_scatter_reduced on the host, for a device without native scatter
        kernels: the same values, no gradient."""
        base = self.numpy()
        idx = index.numpy().astype(np.int64)
        vals = src.numpy()
        valid = (idx >= 0) & (idx < self._shape[dim])
        pos = list(np.indices(idx.shape))
        pos[dim] = np.where(valid, idx, 0)
        flat = np.ravel_multi_index(tuple(pos), self._shape)[valid]
        if reduce in ("amax", "amin"):
            return Tensor(_host_scatter_extreme(base, flat, vals[valid], reduce == "amax",
                                                include_self), dtype=self._dtype)
        if reduce is None:
            # Positions that meet differ only along dim, so the last write
            # along dim is the last one in row-major order.
            out = base.copy().reshape(-1)
            targets, first_rev = np.unique(flat[::-1], return_index=True)
            out[targets] = vals[valid][len(flat) - 1 - first_rev]
            return Tensor(out.reshape(self._shape), dtype=self._dtype)
        acc = np.float32 if dtypes.is_float(self._dtype) else np.int32
        fold, ident = (np.multiply, 1) if reduce == "prod" else (np.add, 0)
        S = np.full(base.size, ident, acc)
        fold.at(S, flat, vals[valid].astype(acc))
        hits = np.bincount(flat, minlength=base.size)
        keep = base.reshape(-1) if include_self else np.where(hits == 0, base.reshape(-1), ident)
        total = fold(S, keep.astype(acc)).reshape(self._shape)
        if reduce in ("sum", "prod"):
            return Tensor(total, dtype=self._dtype)
        count = (hits + (1 if include_self else (hits == 0))).astype(np.float32)
        out = total.astype(np.float32) * (np.float32(1) / count.reshape(self._shape))
        return Tensor(out, dtype=dtypes.float32)

    def scatter_reduce(self, dim: int, index: "Tensor", src: "Tensor", reduce: str,
                       include_self: bool = True) -> "Tensor":
        """src folded into self at `index` along `dim` by "sum", "prod", "mean", "amax" or "amin"
        (tinygrad mixin/op.py:1095-1134); an out-of-range index is dropped."""
        # amax / amin fold from the dtype's min / max (mixin/op.py:1129-1130) by tinygrad's CPU
        # compare-select: the first of equal values stays, and a NaN, src or self, never wins.
        dim, src = self._pre_scatter(dim, index, src)
        if reduce not in ("sum", "prod", "mean", "amax", "amin"):
            raise RuntimeError(f"{reduce=} must be one of 'sum', 'prod', 'mean', 'amax', 'amin'")
        return self._scatter_reduced(dim, index, src, reduce, include_self)

    def scatter(self, dim: int, index: "Tensor", src, reduce: "str | None" = None
                ) -> "Tensor":
        """src written into self at `index` along `dim`, the last write winning (tinygrad
        mixin/op.py:1136-1168); the reduce=None form has no gradient rule, so backward() raises."""
        if reduce not in {None, "add", "multiply"}:
            raise TypeError(f"{reduce=} must be one of None, 'multiply', or 'add'")
        if isinstance(src, (int, float, bool)):
            if index.numel() == 0:
                src = Tensor(np.zeros(index._shape, _to_np_dtype(self._dtype)), dtype=self._dtype)
            else:
                src = _const_broadcast(src, index._shape, self._dtype)
        elif reduce:
            raise TypeError("non-scalar src is not supported with reduce arg. use scatter_reduce")
        if reduce == "add":
            return self.scatter_reduce(dim, index, src, "sum", include_self=True)
        if reduce == "multiply":
            return self.scatter_reduce(dim, index, src, "prod", include_self=True)
        dim, src = self._pre_scatter(dim, index, src)
        return self._scatter_reduced(dim, index, src, None, False)

    def _gather_rows(self, idx: "Tensor", wrap: bool) -> "Tensor":
        """self[idx] along axis 0, a zero row out of range; `wrap` first counts a negative index
        from the end (tinygrad mixin/op.py:95), on the gather's index grid so it fuses in."""
        rest = tuple(self._shape[1:])
        n = idx.numel()
        if n == 0:
            return Tensor(np.zeros((*idx._shape, *rest), dtype=_to_np_dtype(self._dtype)),
                          dtype=self._dtype)
        ix = idx.reshape(n, *([1] * len(rest))).expand(n, *rest)
        if wrap:
            s = ix._shape
            neg = ix < _const_broadcast(0, s, ix._dtype)
            ix = ix + neg * _const_broadcast(self._shape[0], s, ix._dtype)
        return self.gather(0, ix).reshape(*idx._shape, *rest)

    def _index_rows(self, idx) -> "Tensor":
        """Advanced getitem by one int Tensor / list / ndarray index on axis 0 (tinygrad
        mixin/op.py:84-95): a negative index counts from the end, one still out of range reads 0."""
        if isinstance(idx, Tensor):
            if not dtypes.is_int(idx._dtype):
                raise IndexError(f"index dtype {idx._dtype} is not supported")
            return self._gather_rows(idx, wrap=True)
        arr = np.asarray(idx)
        if arr.size and not np.issubdtype(arr.dtype, np.integer):
            raise IndexError(f"{idx=} contains non-int element")
        size = self._shape[0]
        arr = arr.astype(np.int64)
        arr = np.where(arr < 0, arr + size, arr)
        # An index still out of range reads a zero row whatever its value,
        # so -1 stands for every one of them and all indices fit int32.
        arr = np.where((arr >= 0) & (arr < size), arr, -1).astype(np.int32)
        return self._gather_rows(Tensor(arr, dtype=dtypes.int32), wrap=False)

    def __getitem__(self, indices):
        """tinygrad's getitem (mixin/op.py:57-146, OpMixin._getitem); on the CPU an int Tensor,
        list or ndarray index alone takes the native row gather (_index_rows)."""
        if isinstance(indices, list) and all(isinstance(i, int) for i in indices) or \
                not isinstance(indices, (tuple, list)):
            indices = [indices]
        if len(indices) == 1 and isinstance(indices[0], (Tensor, np.ndarray, list)) and _native_indexing():
            return self._index_rows(indices[0])
        return self._getitem([i.tolist() if isinstance(i, np.ndarray) else i for i in indices])

    # ---- nn-flavoured helpers (compose Tensor ops) --------------------

    def linear(self, weight: "Tensor", bias: "Tensor | None" = None
               ) -> "Tensor":
        """x * weight for a 1-D weight, else x @ weight, then + bias (src/nn/linear.c)."""
        args = [self._nn(), weight._nn(), None if bias is None else bias._nn()]
        r = _TH.nn("linear", args)
        if r is None:
            raise ValueError(f"cannot linear {self._shape} and {weight._shape}")
        return Tensor._from_nn(r, "linear")

    def batchnorm(self, weight: "Tensor | None", bias: "Tensor | None",
                  mean: "Tensor", invstd: "Tensor", axis=1) -> "Tensor":
        """Batchnorm over `axis` with a precomputed mean and invstd = 1 / sqrt(var + eps)
        (src/nn/batchnorm.c)."""
        args = [self._nn(), None if weight is None else weight._nn(),
                None if bias is None else bias._nn(), mean._nn(), invstd._nn()]
        axes = [axis] if isinstance(axis, int) else list(axis)
        return Tensor._from_nn(_TH.nn("batchnorm", args, axes), "batchnorm")

    def layernorm(self, axis=-1, eps: float = 1e-5) -> "Tensor":
        axes = [axis] if isinstance(axis, int) else list(axis)
        return Tensor._from_nn(_TH.nn("layernorm", [self._nn()], axes, [eps]), "layernorm")

    def _max_pool(self, k_, s_, d_) -> "Tensor":
        """_pool(k_, s_, d_).max over the window axes (src/nn/max_pool2d.c)."""
        s_, d_ = make_tuple(s_, len(k_)), make_tuple(d_, len(k_))
        return Tensor._from_nn(_TH.nn("max_pool2d", [self._nn()], [*k_, *s_, *d_]), "max_pool2d")

    def conv2d(self, weight: "Tensor",
               bias: "Tensor | None" = None,
               groups: int = 1, stride=1, dilation=1, padding=0) -> "Tensor":
        """tinygrad's conv2d over weight.ndim - 2 spatial axes (src/nn/conv2d.c): a pad, the
        _pool unfold, one broadcast MUL and a sum over (cin, *kernel)."""
        opts = _conv_opts(weight.ndim - 2, groups, stride, dilation, padding)
        args = [self._nn(), weight._nn(), None if bias is None else bias._nn()]
        r = _TH.nn("conv2d", args, opts)
        return Tensor._from_nn(r, f"conv2d {self._shape} * {weight._shape} groups={groups}")

    def conv_transpose2d(self, weight: "Tensor", bias: "Tensor | None" = None, groups: int = 1,
                         stride=1, dilation=1, padding=0, output_padding=0) -> "Tensor":
        """tinygrad's conv_transpose2d (src/nn/conv_transpose2d.c), weight (C_in, C_out / groups,
        *kernel)."""
        opts = _conv_opts(weight.ndim - 2, groups, stride, dilation, padding, output_padding)
        args = [self._nn(), weight._nn(), None if bias is None else bias._nn()]
        r = _TH.nn("conv_transpose2d", args, opts)
        return Tensor._from_nn(r, f"conv_transpose2d {self._shape} * {weight._shape}")

    # ---- transformer / GPT2 ops ---------------------------------------

    @property
    def T(self) -> "Tensor":
        """transpose(): dims 0 and 1 swapped (tinygrad mixin/movement.py:323-328)."""
        return self.transpose()

    @classmethod
    def arange(cls, start, stop=None, step=1,
               dtype: DType | None = None) -> "Tensor":
        """1-D ramp [start, stop) by `step` (tinygrad mixin/op.py:166-195)."""
        if stop is None:
            stop, start = start, 0
        lo, hi = (start, stop - step) if step > 0 else (stop - step, start)
        if dtype is None:
            # An int range takes the first of tinygrad's commit_int ladder that holds it (dtype.py:171-174).
            ladder = (dtypes.default_int, dtypes.int32, dtypes.int64, dtypes.uint64)
            dtype = (dtypes.default_float if any(isinstance(x, float) for x in (start, stop, step))
                     else next((d for d in ladder if d.min <= lo and hi <= d.max), dtypes.int64))
        dtype = to_dtype(dtype)
        if lo < dtype.min or dtype.max < hi:
            raise OverflowError(f"arange [{start}, {stop}) is not representable in dtype {dtype}")
        return Tensor._from_nn(_TH.nn("arange", [], [dtype.thvm_id], [start, stop, step]), "arange")

    def cast(self, dtype: DType) -> "Tensor":
        """Cast to `dtype` (thvm UOP_CAST)."""
        dtype = to_dtype(dtype)
        if dtype == self._dtype:
            return self
        t = _TH.cast(self.term, dtype.thvm_id)
        return Tensor._from_term(t, dtype, self._shape)

    def bitcast(self, dtype: DType) -> "Tensor":
        """self's bits read as `dtype` of the same itemsize (tinygrad mixin/dtype.py:38-53)."""
        dtype = to_dtype(dtype)
        if dtype == self._dtype:
            return self
        if dtype.itemsize != self._dtype.itemsize:
            raise RuntimeError(f"bitcast from {self._dtype} to {dtype} needs equal itemsizes")
        return Tensor._from_term(_TH.bitcast(self.term, dtype.thvm_id), dtype, self._shape)

    # tinygrad mixin/dtype.py:55-142.
    def float(self) -> "Tensor":    return self.cast(dtypes.float32)
    def half(self) -> "Tensor":     return self.cast(dtypes.float16)
    def int(self) -> "Tensor":      return self.cast(dtypes.int32)
    def bool(self) -> "Tensor":     return self.cast(dtypes.bool)
    def bfloat16(self) -> "Tensor": return self.cast(dtypes.bfloat16)
    def double(self) -> "Tensor":   return self.cast(dtypes.double)
    def long(self) -> "Tensor":     return self.cast(dtypes.long)
    def short(self) -> "Tensor":    return self.cast(dtypes.short)
    def is_floating_point(self) -> bool: return dtypes.is_float(self._dtype)
    def element_size(self) -> int:  return self._dtype.itemsize

    def to(self, device, force: bool = False) -> "Tensor":
        """Device move (tinygrad tensor.py:353-367): the identity, as DEV picks thvm's one device at
        process start; `force` has nothing to transfer, thvm's leaves being backend buffers."""
        del device, force
        return self

    def to_(self, device) -> "Tensor":
        """In-place device move (tinygrad tensor.py:345-356): the identity, as for to()."""
        del device
        return self

    def const_like(self, value, dtype: DType | None = None) -> "Tensor":
        """A lazy constant of self's shape, in self's dtype or dtype (tinygrad const_like)."""
        return _const_broadcast(value, self._shape, self._dtype if dtype is None else to_dtype(dtype))

    def gelu(self, approximate: str = "tanh") -> "Tensor":
        """tinygrad mixin/elementwise.py:780-795 (src/nn/elementwise.c)."""
        if approximate not in ("tanh", "none"):
            raise RuntimeError(f"{approximate=} is not supported")
        return self._elementwise("gelu" if approximate == "tanh" else "gelu_erf")

    def erf(self) -> "Tensor":
        """Abramowitz and Stegun 7.1.26, as tinygrad (mixin/elementwise.py:1059-1071)."""
        return self._elementwise("erf")

    def triu(self, diagonal: int = 0) -> "Tensor":
        """The upper-triangular part, other elements 0 (src/nn/tri.c)."""
        return Tensor._from_nn(_TH.nn("tri", [self._nn()], [diagonal, 0]), "triu")

    def tril(self, diagonal: int = 0) -> "Tensor":
        """The lower-triangular part, other elements 0 (src/nn/tri.c)."""
        return Tensor._from_nn(_TH.nn("tri", [self._nn()], [diagonal, 1]), "tril")

    def embedding(self, idx: "Tensor") -> "Tensor":
        """Rows of self (vocab, embed) at the int ids `idx`, a zero row outside [0, vocab) (tinygrad
        nn/__init__.py:369-371): on the CPU one native gather, differentiable into self."""
        if _native_indexing():
            return self._gather_rows(idx, wrap=False)
        # Host gather off the CPU backend: no native gather kernels there.
        sel = idx.numpy().astype(np.int64)
        rows = self.numpy()[sel.reshape(-1)]
        out_shape = tuple(sel.shape) + (self._shape[-1],)
        return Tensor(rows.reshape(out_shape), dtype=self._dtype)

    def scaled_dot_product_attention(self, key: "Tensor", value: "Tensor",
                                     attn_mask: "Tensor | None" = None,
                                     dropout_p: float = 0.0,
                                     is_causal: bool = False, enable_gqa: bool = False) -> "Tensor":
        """softmax((q @ k^T) / sqrt(d_k) + mask) @ value, the mask causal or additive (tinygrad
        mixin/rand.py:298-327); enable_gqa repeats each key / value head over its query group."""
        q = self
        if enable_gqa:
            key = key.repeat_interleave(int(self._shape[-3] // key.shape[-3]), dim=-3)
            value = value.repeat_interleave(int(self._shape[-3] // value.shape[-3]), dim=-3)
        if dropout_p and TRAINING:
            # tinygrad drops out softmax(qk) under TRAINING (mixin/rand.py:294, 327);
            # thvm has no dropout.
            raise NotImplementedError("scaled_dot_product_attention dropout_p "
                                      "under TRAINING (thvm has no dropout)")
        if is_causal and attn_mask is not None:
            raise RuntimeError("cannot set attn_mask when is_causal=True")
        args = [q._nn(), key._nn(), value._nn(), None if attn_mask is None else attn_mask._nn()]
        r = _TH.nn("scaled_dot_product_attention", args, [1 if is_causal else 0])
        return Tensor._from_nn(r, "scaled_dot_product_attention")

    @staticmethod
    def manual_seed(seed: int = 0) -> None:
        """Seed numpy's global RNG, which thvm's random factories and multinomial draw from
        (tinygrad tensor.py:441-457)."""
        np.random.seed(seed)

    def multinomial(self, num_samples: int = 1,
                    replacement: bool = False) -> "Tensor":
        """`num_samples` indices per row, drawn by the weights in self on the host from numpy's
        seeded RNG, with no gradient (tinygrad mixin/rand.py:251-276)."""
        w = self.numpy().astype(np.float64)
        if w.ndim == 1:
            w = w[None, :]
            squeeze = True
        else:
            squeeze = False
        out = np.empty((w.shape[0], num_samples), dtype=np.int32)
        for i in range(w.shape[0]):
            p = w[i] / w[i].sum()
            out[i] = np.random.choice(w.shape[1], size=num_samples,
                                      replace=replacement, p=p)
        res = out[0] if squeeze else out
        return Tensor(res.astype(np.int32), dtype=dtypes.default_int)
