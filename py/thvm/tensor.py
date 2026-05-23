"""thvm Tensor -- Phase 2 + 3A of py_tensor_frontend.md.

A Tensor wraps a thvm TAG_TEN term + Python-side dtype + shape.  Method
bodies build a TAG_TEN graph through the Phase-1 bridge (Thvm); a
trailing .realize() (implicit in .numpy() / .item() / .tolist()) drives
thvm's wnf -> materialize -> kernelize -> schedule -> dispatch.  No
Python lazy graph, no Python autodiff -- thvm owns those.

Honest bar (tinygrad parity, "mostly works"):
  - elementwise (+, -, *, /, -, activations) with tinygrad broadcast
  - reductions (.sum / .mean / .max) with axis=int|tuple|None
  - movement (.reshape, .permute, .expand, .flatten, .transpose)
  - host I/O + factories (ones, zeros, eye, rand, empty, uniform, ...)
  - matmul (__matmul__) and .linear / .batchnorm / .layernorm methods
  - Phase-3A host-side max_pool2d / conv2d (numpy bridge: correct
    numerically, but bypasses thvm's fusion; proper composed paths
    are Phase 3B)
Symbolic dims, real JIT, and grad-flow through backward() are Phase
3B; .detach() / .assign() are stubs sufficient for forward inference.
"""
from __future__ import annotations

import ctypes as _ct
import math
import weakref
from typing import Any, Sequence

import numpy as np

from .dtypes import DType, dtypes
from .thvm import K, Term, Thvm, _uop_binary


# Process-global bridge.  All Tensors share one Thvm instance.
_TH = Thvm()
_TAG_TEN = K.TAG_TEN

# Phase 3B: TenDesc.requires_grad is the canonical "this is a
# parameter" flag (set via py_ten_set_requires_grad).  This dict
# routes a TAG_TEN's tid back to its originating Python Tensor so
# backward() can assign each leaf's .grad.  WeakValueDictionary so
# dead tensors auto-drop -- nn parameters stay alive via their layer.
_GRAD_TENSORS: "weakref.WeakValueDictionary[int, Tensor]" = \
    weakref.WeakValueDictionary()

# Monotonic handle ids for live-tensor pinning.  Each realized Tensor
# pins its term via _TH.pin_set(handle, term) so it is a GC/preserve
# root; __del__ drops the handle.  _TH.reclaim() (driven once per
# training step via GlobalCounters.reset) then frees every backend
# buffer NOT reachable from a pinned tensor -- the cross-step buffer
# reclaim that keeps device memory flat in the eager (no-JIT) loop.
# Requires THVM_GC=0 (no copying heap GC) so the raw term integers
# Python caches never move.
_PIN_NEXT = 1


def _wrap_other(self_t: "Tensor", other) -> "Tensor":
    """Coerce a Python scalar (or Tensor) into a Tensor matching self's
    dtype.  Broadcasts to self.shape when other is a scalar."""
    if isinstance(other, Tensor):
        return other
    arr = np.asarray(other, dtype=self_t._dtype.np_dtype)
    return Tensor(arr, dtype=self_t._dtype)


class Tensor:
    """thvm tensor: TAG_TEN handle + Python-tracked dtype + shape."""

    __slots__ = ("term", "_dtype", "_shape", "requires_grad", "grad",
                 "_pin_id", "__weakref__")

    # tinygrad's class-level `Tensor.training` flag; BatchNorm reads it.
    training: bool = False

    # Identity hashing: __eq__ is overridden to build an elementwise mask
    # (tinygrad semantics), which would otherwise make Tensor unhashable.
    def __hash__(self):
        return id(self)

    class _TrainCtx:
        """Context manager + decorator: `with Tensor.train():` or
        `@Tensor.train()` flips Tensor.training for the dynamic extent."""
        def __init__(self, mode: bool):
            self.mode = mode
            self.prev = None

        def __enter__(self):
            self.prev = Tensor.training
            Tensor.training = self.mode
            return self

        def __exit__(self, *exc):
            Tensor.training = self.prev
            return False

        def __call__(self, fn):
            import functools

            @functools.wraps(fn)
            def wrapped(*a, **k):
                with Tensor._TrainCtx(self.mode):
                    return fn(*a, **k)
            return wrapped

    @classmethod
    def train(cls, mode: bool = True) -> "Tensor._TrainCtx":
        return cls._TrainCtx(mode)

    # ---- construction --------------------------------------------------

    def __init__(self, data=None, dtype: DType | None = None,
                 device: str | None = None,
                 requires_grad: bool | None = None):
        del device  # accepted for tinygrad compat; DEV picks at init
        if isinstance(data, Tensor):
            self.term = data.term
            self._dtype = data._dtype
            self._shape = data._shape
        elif data is None:
            self.term = Term(0)
            self._dtype = dtype or dtypes.default_float
            self._shape = ()
        else:
            arr = np.asarray(data)
            if dtype is None:
                dtype = (dtypes.default_int
                         if np.issubdtype(arr.dtype, np.integer)
                         else dtypes.default_float)
            arr = arr.astype(dtype.np_dtype, copy=False)
            # ascontiguousarray promotes 0-D -> (1,); only call for >=1D
            if arr.ndim >= 1 and not arr.flags["C_CONTIGUOUS"]:
                arr = np.ascontiguousarray(arr)
            self._dtype = dtype
            self._shape = tuple(int(d) for d in arr.shape)
            self.term = _TH.ten_create(dtype.thvm_id, list(self._shape))
            if arr.size > 0:
                _TH.ten_write(self.term, arr.tobytes())
        # None == "unset" (tinygrad semantics): an optimizer promotes it
        # to True; an explicit False (BatchNorm running stats) stays a
        # non-trained buffer.  Distinguishing the two is what lets
        # nn.optim pick exactly the trainable parameters.
        self.requires_grad = requires_grad
        self.grad: Tensor | None = None
        self._pin_id = 0
        self._pin()

    @classmethod
    def _from_term(cls, term: Term, dtype: DType,
                   shape: tuple[int, ...]) -> "Tensor":
        t = cls.__new__(cls)
        t.term = term
        t._dtype = dtype
        t._shape = tuple(int(d) for d in shape)
        t.requires_grad = None
        t.grad = None
        t._pin_id = 0
        t._pin()
        return t

    # ---- live-tensor pinning (cross-step buffer reclaim) --------------

    def _pin(self) -> None:
        """Pin the current term if it is a realized TAG_TEN, so reclaim()
        keeps its buffer.  Re-pinning (term changed via realize/assign)
        drops the stale handle entry first to avoid leaking pin slots."""
        global _PIN_NEXT
        if int(_TH.term_tag(self.term)) != _TAG_TEN:
            return
        if self._pin_id == 0:
            self._pin_id = _PIN_NEXT
            _PIN_NEXT += 1
        else:
            _TH.pin_drop(self._pin_id)
        _TH.pin_set(self._pin_id, self.term)

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
    def ndim(self) -> int: return len(self._shape)

    def numel(self) -> int:
        n = 1
        for d in self._shape: n *= d
        return n

    # ---- realize + host readback --------------------------------------

    def realize(self, *lst: "Tensor", **kwargs) -> "Tensor":
        """Drive thvm's pipeline; mutates `term` to the resulting
        TAG_TEN.  Extra tensors realize too (tinygrad pattern), each in
        its own pass.

        NOTE: do NOT bundle self+lst into one realize_many pass here.
        Bundling fixed a CPU Adam corruption (an in-place ASSIGN read by
        another assign re-firing) but REGRESSED CUDA to NaN (CUDA's
        default-on per-realize buffer reuse races the bundled in-place
        assigns).  The Adam corruption is instead fixed at the source in
        optim.Adam._step (m,v realized before the param update reads
        them), so per-tensor realize is correct on every backend."""
        self.term = _TH.realize(self.term)
        self._pin()
        for x in lst:
            x.realize()
        return self

    def contiguous(self) -> "Tensor":
        return self.realize()

    def numpy(self) -> np.ndarray:
        self.realize()
        n = self.numel()
        if n == 0:
            return np.zeros(self._shape, dtype=self._dtype.np_dtype)
        raw = _TH.ten_read(self.term, n * self._dtype.itemsize)
        arr = np.frombuffer(raw, dtype=self._dtype.np_dtype)
        return arr.reshape(self._shape).copy()

    def tolist(self): return self.numpy().tolist()
    def item(self):   return self.numpy().item()

    def __repr__(self) -> str:
        return f"<Tensor dtype={self._dtype.name} shape={self._shape}>"

    # ---- bookkeeping helpers (forward-only stubs for Phase 3A) --------

    def detach(self) -> "Tensor":
        """tinygrad detach -- stops gradient.  Phase 3A: no autograd
        graph yet, so it's just a shallow alias (same term)."""
        return Tensor._from_term(self.term, self._dtype, self._shape)

    def assign(self, x: "Tensor") -> "Tensor":
        """tinygrad assign -- lazy IN-PLACE update.  Builds UOP_ASSIGN(self,
        x); realizing it copies x's buffer into self's (the dst tid/buffer
        stay stable) and returns self.  In-place + lazy is what makes the
        optimizer step JIT-capturable (jit_capture_record_assign) and keeps
        param buffers stable across replays -- exactly tinygrad/WL TAssign."""
        x = x if isinstance(x, Tensor) else Tensor(x, dtype=self._dtype)
        self.term = Term(_uop_binary(K.ASSIGN, self.term, x.term))
        return self

    def replace(self, x: "Tensor") -> "Tensor":
        """tinygrad replace -- swap underlying data; returns self."""
        x = x if isinstance(x, Tensor) else Tensor(x, dtype=self._dtype)
        self.term = x.term
        self._dtype = x._dtype
        self._shape = x._shape
        self._pin()
        return self

    def requires_grad_(self, requires_grad: bool = True) -> "Tensor":
        """Set the canonical TenDesc.requires_grad flag (C-side) AND
        register the Python tensor in the tid->Tensor routing dict so
        backward() can assign .grad back to this object."""
        self.requires_grad = requires_grad
        # Sync TenDesc; for graph-result Tensors with non-TAG_TEN term
        # the C call no-ops (returns 0), which is fine -- backward is
        # only meaningful for leaves anyway.
        _TH.ten_set_requires_grad(self.term, requires_grad)
        tid = int(_TH.term_val(self.term))
        if requires_grad and tid > 0:
            _GRAD_TENSORS[tid] = self
        else:
            _GRAD_TENSORS.pop(tid, None)
        return self

    def backward(self, gradient: "Tensor | None" = None) -> "Tensor":
        """Walk-once backward (no grad_with_target).  Builds ONE
        uop_grad(y, gy) root and drives wnf so the chain rule visits
        every requires_grad leaf once and accumulates its cotangent
        into TENS[tid].grad (the canonical TenDesc.grad accumulator,
        populated by grad_leaf_sup's target==0 path).  We then wrap
        each leaf's TenDesc.grad into a Tensor for caller consumption.

        wnf only drives the term-level chain-rule rewrite -- no
        kernels, no materialize, no dispatch.  The actual gradient
        values stay lazy inside TenDesc.grad; Tensor.realize(*grads)
        drives the pipeline downstream just like tinygrad."""
        if gradient is None:
            if self.numel() != 1:
                raise RuntimeError(
                    "backward(): implicit gradient only for scalar outputs")
            gradient = (Tensor(1.0, dtype=self._dtype) if not self._shape
                        else Tensor.ones(*self._shape, dtype=self._dtype))
        # Build the walk-once BWD root and drive wnf to fire the
        # chain rule (interact_grad -> grad_leaf_sup accumulator).
        bwd = _TH.grad(self.term, gradient.term)
        _TH.wnf(bwd)
        # Wrap each leaf's TenDesc.grad term.
        for tid, leaf in list(_GRAD_TENSORS.items()):
            if not _TH.ten_get_requires_grad(leaf.term):
                continue
            g_term = _TH.ten_get_grad(leaf.term)
            if g_term != 0:
                leaf.grad = Tensor._from_term(Term(g_term),
                                              leaf._dtype, leaf._shape)
        return self

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
    def ones(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        dt = dtype or dtypes.default_float
        return cls._from_numpy(np.ones(shape, dtype=dt.np_dtype), dt)

    @classmethod
    def zeros(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        dt = dtype or dtypes.default_float
        return cls._from_numpy(np.zeros(shape, dtype=dt.np_dtype), dt)

    @classmethod
    def eye(cls, n: int, m: int | None = None,
            dtype: DType | None = None, **kw) -> "Tensor":
        dt = dtype or dtypes.default_float
        return cls._from_numpy(np.eye(n, m or n, dtype=dt.np_dtype), dt)

    @classmethod
    def rand(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        dt = dtype or dtypes.default_float
        return cls._from_numpy(
            np.random.rand(*shape).astype(dt.np_dtype, copy=False), dt)

    @classmethod
    def uniform(cls, *shape, low: float = 0.0, high: float = 1.0,
                dtype: DType | None = None, **kw) -> "Tensor":
        dt = dtype or dtypes.default_float
        arr = np.random.uniform(low, high, shape).astype(dt.np_dtype,
                                                          copy=False)
        return cls._from_numpy(arr, dt)

    @classmethod
    def randint(cls, *shape, low: int = 0, high: int = 10,
                dtype: DType | None = None, **kw) -> "Tensor":
        dt = dtype or dtypes.default_int
        arr = np.random.randint(low, high, size=shape or (1,))
        return cls._from_numpy(arr.astype(dt.np_dtype, copy=False), dt)

    @classmethod
    def empty(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        dt = dtype or dtypes.default_float
        return cls._from_numpy(np.empty(shape, dtype=dt.np_dtype), dt)

    @classmethod
    def full(cls, shape, value, dtype: DType | None = None,
             **kw) -> "Tensor":
        dt = dtype or dtypes.default_float
        return cls._from_numpy(np.full(shape, value, dtype=dt.np_dtype),
                               dt)

    @classmethod
    def ones_like(cls, t: "Tensor") -> "Tensor":
        return cls.ones(*t._shape, dtype=t._dtype)

    @classmethod
    def zeros_like(cls, t: "Tensor") -> "Tensor":
        return cls.zeros(*t._shape, dtype=t._dtype)

    @classmethod
    def cat(cls, *tensors: "Tensor", dim: int = 0) -> "Tensor":
        """Concat along `dim` (Phase 3A: host-side numpy)."""
        arrs = [t.realize().numpy() for t in tensors]
        out = np.concatenate(arrs, axis=dim)
        return cls(out, dtype=tensors[0]._dtype)

    # ---- broadcasting (tinygrad rule: align trailing, expand 1s) ------

    def _broadcast(self, other) -> tuple["Tensor", "Tensor"]:
        b = _wrap_other(self, other)
        sa, sb = self._shape, b._shape
        if sa == sb:
            return self, b
        nd = max(len(sa), len(sb))
        sa_pad = (1,) * (nd - len(sa)) + sa
        sb_pad = (1,) * (nd - len(sb)) + sb
        out_shape = []
        for da, db in zip(sa_pad, sb_pad):
            if da != db and da != 1 and db != 1:
                raise ValueError(
                    f"cannot broadcast {sa} vs {sb}")
            out_shape.append(max(da, db))
        out = tuple(out_shape)
        a = self if sa_pad == sa else self.reshape(*sa_pad)
        b2 = b if sb_pad == sb else b.reshape(*sb_pad)
        if sa_pad != out:
            a = a.expand(*out)
        if sb_pad != out:
            b2 = b2.expand(*out)
        return a, b2

    # ---- elementwise binary -------------------------------------------

    def _binop(self, other, opcode: int) -> "Tensor":
        a, b = self._broadcast(other)
        t = Term(_uop_binary(_ct.c_uint32(int(opcode)),
                             _ct.c_uint64(int(a.term)),
                             _ct.c_uint64(int(b.term))))
        return Tensor._from_term(t, a._dtype, a._shape)

    def __add__(self, o):  return self._binop(o, K.ADD)
    def __radd__(self, o): return self.__add__(o)
    def __sub__(self, o):
        # a - b = a + (-b)
        if isinstance(o, (int, float)):
            return self.__add__(-o)
        return self.__add__(-(o if isinstance(o, Tensor) else Tensor(o)))
    def __rsub__(self, o): return (-self).__add__(o)
    def __mul__(self, o):  return self._binop(o, K.MUL)
    def __rmul__(self, o): return self.__mul__(o)

    def __truediv__(self, o):
        if isinstance(o, (int, float)):
            return self.__mul__(1.0 / float(o))
        other = o if isinstance(o, Tensor) else Tensor(o)
        return self.__mul__(other.recip())
    def __rtruediv__(self, o):
        return self.recip().__mul__(o)

    def __neg__(self) -> "Tensor":
        return Tensor._from_term(_TH.neg(self.term), self._dtype,
                                 self._shape)

    # tinygrad alias: `t.add(other)` == `t + other`
    def add(self, o): return self.__add__(o)
    def sub(self, o): return self.__sub__(o)
    def mul(self, o): return self.__mul__(o)
    def div(self, o): return self.__truediv__(o)

    def __matmul__(self, other: "Tensor") -> "Tensor":
        """Matmul -- composes RESHAPE+EXPAND+MUL+REDUCE; thvm fuses."""
        if self.ndim != 2 or other.ndim != 2:
            raise NotImplementedError(
                f"matmul: only 2-D for Phase 2A "
                f"({self._shape} @ {other._shape})")
        M, K_in = self._shape
        K2, N = other._shape
        if K_in != K2:
            raise ValueError(
                f"matmul shape mismatch: {self._shape} @ {other._shape}")
        a = self.reshape(M, K_in, 1).expand(M, K_in, N)
        b = other.reshape(1, K_in, N).expand(M, K_in, N)
        return (a * b).sum(axis=1)

    # ---- elementwise unary --------------------------------------------

    def _unary(self, opcode: int) -> "Tensor":
        return Tensor._from_term(_TH.unary(opcode, self.term),
                                 self._dtype, self._shape)

    def neg(self) -> "Tensor":   return self.__neg__()
    def recip(self) -> "Tensor": return self._unary(K.RECIP)
    def reciprocal(self) -> "Tensor": return self._unary(K.RECIP)  # tinygrad alias
    def exp2(self) -> "Tensor":  return self._unary(K.EXP2)
    def log2(self) -> "Tensor":  return self._unary(K.LOG2)
    def sqrt(self) -> "Tensor":  return self._unary(K.SQRT)
    def rsqrt(self) -> "Tensor": return self.sqrt().recip()

    def exp(self) -> "Tensor":
        return (self * 1.4426950408889634).exp2()  # exp2(x*log2(e))
    def log(self) -> "Tensor":
        return self.log2() * 0.6931471805599453  # ln(x) = log2(x)*ln(2)
    def abs(self) -> "Tensor":
        return (self * self).sqrt()
    def sigmoid(self) -> "Tensor":
        return (1.0 + self.neg().exp()).reciprocal()
    def tanh(self) -> "Tensor":
        return 2.0 * (2.0 * self).sigmoid() - 1.0
    def softmax(self, axis: int = -1) -> "Tensor":
        # numerically-stable: exp(x - max) / sum(exp(x - max)) along axis.
        m = self.max(axis=axis, keepdim=True)
        e = (self - m).exp()
        return e * e.sum(axis=axis, keepdim=True).reciprocal()

    def log_softmax(self, axis: int = -1) -> "Tensor":
        # x - max, then subtract logsumexp; REDUCE_MAX backward (mask at
        # argmax) makes the max-subtract differentiable.
        m = self.max(axis=axis, keepdim=True)
        shifted = self - m
        return shifted - shifted.exp().sum(axis=axis, keepdim=True).log()

    def sparse_categorical_crossentropy(self, Y: "Tensor",
                                        axis: int = -1) -> "Tensor":
        """Mean NLL of log_softmax(self) at the integer class labels Y.

        Y holds class indices (a gathered batch / dataset slice -- data,
        not on the autograd path), so it is one-hot'd host-side and the
        gradient flows only through `self` (the logits)."""
        ax = axis if axis >= 0 else axis + self.ndim
        n_classes = self._shape[ax]
        labels = Y.numpy().astype(np.int64).reshape(-1)
        oh = np.zeros((labels.shape[0], n_classes), dtype=np.float32)
        oh[np.arange(labels.shape[0]), labels] = 1.0
        ls = self.log_softmax(axis=ax)
        return (ls * Tensor(oh.reshape(self._shape))).sum(axis=ax).mean() * -1.0

    def argmax(self, axis: int = -1) -> "Tensor":
        """Index of the max along `axis`.  Host-side (used for eval
        accuracy, off the autograd path); returns an int Tensor."""
        ax = axis if axis >= 0 else axis + self.ndim
        return Tensor(np.argmax(self.numpy(), axis=ax).astype(np.int32),
                      dtype=dtypes.default_int)

    def __eq__(self, other) -> "Tensor":
        """Elementwise equality mask (1.0/0.0) -- tinygrad semantics, so
        `(pred == labels).mean()` reads as an accuracy."""
        a = self.numpy()
        b = other.numpy() if isinstance(other, Tensor) else np.asarray(other)
        return Tensor((a == b).astype(np.float32))

    def __ne__(self, other) -> "Tensor":
        return Tensor((self.numpy() != (
            other.numpy() if isinstance(other, Tensor) else np.asarray(other))
        ).astype(np.float32))

    def relu(self) -> "Tensor":
        # relu(x) = (0 < x) * x
        zero = Tensor(np.zeros(self._shape, dtype=self._dtype.np_dtype),
                      dtype=self._dtype)
        mask = Term(_uop_binary(_ct.c_uint32(K.CMPLT),
                                _ct.c_uint64(int(zero.term)),
                                _ct.c_uint64(int(self.term))))
        return self * Tensor._from_term(mask, self._dtype, self._shape)

    def elu(self, alpha: float = 1.0) -> "Tensor":
        # elu(x) = max(0, x) + min(0, alpha*(exp(x)-1))
        # Composed as: relu(x) + (-relu(-x)).exp() - 1, scaled by alpha.
        # Simpler: relu(x) + (relu(-x) > 0).where(alpha*(exp(x)-1), 0)
        # For Phase 3A use the (1 - x.relu()/(x.relu()+eps))*alpha
        # path is brittle; use the safe identity:
        #   elu(x) = relu(x) + alpha * (exp(min(x,0)) - 1)
        # min(x,0) = -relu(-x)
        neg_relu_neg = -((-self).relu())
        return self.relu() + (neg_relu_neg.exp() - 1.0) * alpha

    def maximum(self, other) -> "Tensor":
        a, b = self._broadcast(other)
        # max(a,b) = (a<b) * b + (a>=b) * a  -- via CMPLT
        less = Term(_uop_binary(_ct.c_uint32(K.CMPLT),
                                _ct.c_uint64(int(a.term)),
                                _ct.c_uint64(int(b.term))))
        less_t = Tensor._from_term(less, a._dtype, a._shape)
        return less_t * b + (1.0 - less_t) * a

    def minimum(self, other) -> "Tensor":
        return -(-self).maximum(-_wrap_other(self, other))

    # ---- reductions ---------------------------------------------------

    def _reduce(self, kind: int, axis, keepdim: bool) -> "Tensor":
        if axis is None:
            axes = list(range(self.ndim))
        elif isinstance(axis, int):
            axes = [axis if axis >= 0 else axis + self.ndim]
        else:
            axes = sorted(set(a if a >= 0 else a + self.ndim for a in axis))
        # Reduce innermost-first so the outer-axis indices stay valid.
        t = self.term
        new_shape = list(self._shape)
        for ax in reversed(axes):
            t = _TH.reduce(kind, ax, t)
            if keepdim:
                new_shape[ax] = 1
            else:
                new_shape.pop(ax)
        # thvm's UOP_REDUCE DROPS the reduced axis (output ndim = src-1).
        # For keepdim we must reshape the term to re-insert the size-1 axes,
        # else the Python shape says (.,1,.) while the term is rank-reduced --
        # and any later broadcast against it reads out of bounds (garbage).
        if keepdim:
            t = _TH.reshape(t, list(new_shape))
        return Tensor._from_term(t, self._dtype, tuple(new_shape))

    def sum(self, axis=None, keepdim: bool = False) -> "Tensor":
        return self._reduce(K.REDUCE_SUM, axis, keepdim)

    def max(self, axis=None, keepdim: bool = False) -> "Tensor":
        return self._reduce(K.REDUCE_MAX, axis, keepdim)

    def mean(self, axis=None, keepdim: bool = False) -> "Tensor":
        s = self.sum(axis=axis, keepdim=keepdim)
        if axis is None:
            div = self.numel()
        elif isinstance(axis, int):
            ax = axis if axis >= 0 else axis + self.ndim
            div = self._shape[ax]
        else:
            div = 1
            for ax in axis:
                a = ax if ax >= 0 else ax + self.ndim
                div *= self._shape[a]
        return s * (1.0 / div) if div else s

    def std(self, axis=None, keepdim: bool = False) -> "Tensor":
        m = self.mean(axis=axis, keepdim=True)
        diff = self - m
        var = (diff * diff).mean(axis=axis, keepdim=keepdim)
        return var.sqrt()

    # ---- movement -----------------------------------------------------

    def reshape(self, *shape) -> "Tensor":
        if len(shape) == 1 and isinstance(shape[0], (tuple, list)):
            shape = tuple(shape[0])
        s = list(int(d) for d in shape)
        if -1 in s:
            if s.count(-1) > 1:
                raise ValueError("only one -1 allowed in reshape")
            known = 1
            for d in s:
                if d != -1:
                    known *= d
            if known == 0:
                raise ValueError(f"cannot infer reshape: {s}")
            s[s.index(-1)] = self.numel() // known
        shape = tuple(s)
        t = _TH.reshape(self.term, list(shape))
        return Tensor._from_term(t, self._dtype, shape)

    def permute(self, *axes) -> "Tensor":
        if len(axes) == 1 and isinstance(axes[0], (tuple, list)):
            axes = tuple(axes[0])
        new_shape = tuple(self._shape[a if a >= 0 else a + self.ndim]
                          for a in axes)
        t = _TH.permute(self.term, list(axes))
        return Tensor._from_term(t, self._dtype, new_shape)

    def transpose(self, dim0: int = -2, dim1: int = -1) -> "Tensor":
        if self.ndim < 2:
            return self
        perm = list(range(self.ndim))
        d0 = dim0 if dim0 >= 0 else dim0 + self.ndim
        d1 = dim1 if dim1 >= 0 else dim1 + self.ndim
        perm[d0], perm[d1] = perm[d1], perm[d0]
        return self.permute(*perm)

    def expand(self, *shape) -> "Tensor":
        if len(shape) == 1 and isinstance(shape[0], (tuple, list)):
            shape = tuple(shape[0])
        shape = tuple(int(d) for d in shape)
        t = _TH.expand(self.term, list(shape))
        return Tensor._from_term(t, self._dtype, shape)

    def flatten(self, start_dim: int = 0) -> "Tensor":
        before = self._shape[:start_dim]
        flat = 1
        for d in self._shape[start_dim:]:
            flat *= d
        return self.reshape(*before, flat)

    def repeat(self, *repeats) -> "Tensor":
        """Repeat each axis by `repeats[i]` (tinygrad-style: pads shape
        left with 1s when repeats is longer than ndim)."""
        if len(repeats) == 1 and isinstance(repeats[0], (tuple, list)):
            repeats = tuple(repeats[0])
        repeats = tuple(int(r) for r in repeats)
        base = list(self._shape)
        if len(repeats) > len(base):
            base = [1] * (len(repeats) - len(base)) + base
        if len(repeats) < len(base):
            repeats = (1,) * (len(base) - len(repeats)) + repeats
        unsqueezed: list[int] = []
        expanded: list[int] = []
        for r, s in zip(repeats, base):
            if r == 1:
                unsqueezed.append(s); expanded.append(s)
            else:
                unsqueezed.extend([1, s]); expanded.extend([r, s])
        final = [r * s for r, s in zip(repeats, base)]
        t = self if list(self._shape) == base else self.reshape(*base)
        return t.reshape(*unsqueezed).expand(*expanded).reshape(*final)

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

    def _pool(self, k_, stride=1, dilation=1) -> "Tensor":
        """tinygrad's _pool unfold via repeat+shrink+reshape (port of
        tinygrad/mixin/movement.py:_pool).  Produces a single tensor
        of shape (..., *o_, *k_) where each (o, k) slice is the
        sliding window at output position o.  Powers conv2d / pool
        without N separate shrink iterations."""
        if isinstance(k_, int):
            k_ = (k_,)
        k_ = tuple(int(k) for k in k_)
        s_ = (stride,) * len(k_) if isinstance(stride, int) else tuple(stride)
        d_ = (dilation,) * len(k_) if isinstance(dilation, int) else tuple(dilation)
        assert len(k_) == len(s_) == len(d_)
        noop_ndim = self.ndim - len(k_)
        i_ = self._shape[-len(k_):]
        o_ = tuple((i - d * (k - 1) + s - 1) // s
                   for i, d, k, s in zip(i_, d_, k_, s_))   # ceildiv
        f_ = tuple(max(1, (o * s - d + i - 1) // i)
                   for o, s, i, d in zip(o_, s_, i_, d_))
        reps = [1] * noop_ndim + [(k * (i * f + d) + i - 1) // i
                                  for k, i, d, f in zip(k_, i_, d_, f_)]
        x = self.repeat(reps)
        prefix = list(self._shape[:noop_ndim])
        x = x.shrink_to(prefix + [k * (i * f + d)
                                  for k, i, d, f in zip(k_, i_, d_, f_)])
        reshape1 = list(prefix)
        for k, i, d, f in zip(k_, i_, d_, f_):
            reshape1.extend([k, i * f + d])
        x = x.reshape(*reshape1)
        target2 = list(prefix)
        for k, o, s in zip(k_, o_, s_):
            target2.extend([k, o * s])
        x = x.shrink_to(target2)
        reshape2 = list(prefix)
        for k, o, s in zip(k_, o_, s_):
            reshape2.extend([k, o, s])
        x = x.reshape(*reshape2)
        target3 = list(prefix)
        for k, o in zip(k_, o_):
            target3.extend([k, o, 1])
        x = x.shrink_to(target3)
        reshape3 = list(prefix)
        for k, o in zip(k_, o_):
            reshape3.extend([k, o])
        x = x.reshape(*reshape3)
        # permute: move k axes after o axes.
        # current after reshape3: prefix, k_0, o_0, k_1, o_1, ...
        # want: prefix, o_0, o_1, ..., k_0, k_1, ...
        perm = list(range(noop_ndim))
        for i in range(len(i_)):
            perm.append(noop_ndim + i * 2 + 1)   # o_i
        for i in range(len(i_)):
            perm.append(noop_ndim + i * 2)       # k_i
        return x.permute(*perm)

    def pad(self, begin_end) -> "Tensor":
        """Pad each axis with zeros.  begin_end has 2*ndim entries:
        [b0, e0, b1, e1, ...]."""
        new_shape = tuple(self._shape[i] + begin_end[2*i] + begin_end[2*i+1]
                          for i in range(self.ndim))
        t = _TH.pad(self.term, list(begin_end))
        return Tensor._from_term(t, self._dtype, new_shape)

    def shrink(self, begin_end) -> "Tensor":
        """Take a sub-range per axis.  begin_end has 2*ndim entries:
        [b0, e0, b1, e1, ...] -- begin inclusive, end exclusive."""
        new_shape = tuple(begin_end[2*i+1] - begin_end[2*i]
                          for i in range(self.ndim))
        t = _TH.shrink(self.term, list(begin_end))
        return Tensor._from_term(t, self._dtype, new_shape)

    def __getitem__(self, idx):
        """Basic indexing: int / slice (per leading dim), via shrink.

        An int index shrinks the axis to width 1 then drops it (squeeze);
        a slice shrinks to [start:stop].  Trailing dims default to full.
        Tensor / fancy (gather) indexing is not handled here -- that lives
        at the lowered/gather level."""
        # Fancy / gather indexing: a Tensor or array of indices selects
        # rows along axis 0.  Done host-side -- the index source is data
        # (the dataset / a randint batch), never on the autograd path.
        if isinstance(idx, (Tensor, np.ndarray, list)):
            sel = idx.numpy() if isinstance(idx, Tensor) else np.asarray(idx)
            gathered = self.numpy()[sel.astype(np.int64)]
            return Tensor(gathered, dtype=self._dtype)
        if not isinstance(idx, tuple):
            idx = (idx,)
        if len(idx) > self.ndim:
            raise IndexError(f"too many indices for ndim {self.ndim}")
        begin_end = []
        squeeze = []
        for d in range(self.ndim):
            n = self._shape[d]
            ix = idx[d] if d < len(idx) else slice(None)
            if isinstance(ix, slice):
                start, stop, step = ix.indices(n)
                if step != 1:
                    raise NotImplementedError("strided slice")
                begin_end += [start, stop]
            elif isinstance(ix, int):
                i = ix if ix >= 0 else ix + n
                begin_end += [i, i + 1]
                squeeze.append(d)
            else:
                raise NotImplementedError(f"index type {type(ix).__name__}")
        out = self.shrink(begin_end)
        if squeeze:
            keep = [out._shape[d] for d in range(out.ndim) if d not in squeeze]
            out = out.reshape(tuple(keep))
        return out

    # ---- nn-flavoured helpers (compose Tensor ops) --------------------

    def linear(self, weight: "Tensor", bias: "Tensor | None" = None
               ) -> "Tensor":
        """y = x @ weight + bias.  Broadcasts bias over leading dims."""
        out = self @ weight
        if bias is not None:
            out = out + bias
        return out

    def batchnorm(self, weight: "Tensor | None", bias: "Tensor | None",
                  mean: "Tensor", invstd: "Tensor") -> "Tensor":
        """Apply pre-computed batchnorm given (mean, invstd=1/sqrt(var+eps))."""
        shape = [1, -1] + [1] * (self.ndim - 2)
        x_hat = (self - mean.reshape(*shape)) * invstd.reshape(*shape)
        if weight is not None:
            x_hat = x_hat * weight.reshape(*shape)
        if bias is not None:
            x_hat = x_hat + bias.reshape(*shape)
        return x_hat

    def layernorm(self, axis=-1, eps: float = 1e-5) -> "Tensor":
        m = self.mean(axis=axis, keepdim=True)
        y = self - m
        var = (y * y).mean(axis=axis, keepdim=True)
        return y * (var + eps).rsqrt()

    # --- Phase-3A host-side convolution + pooling ---
    # These realize their inputs and run via numpy.  They bypass thvm's
    # kernel fusion -- correctness over perf for Phase 3A; the proper
    # composed paths (reshape+expand+mul+sum unfold pattern) are
    # Phase 3B.

    def max_pool2d(self, kernel_size=(2, 2), stride=None, dilation=1,
                   padding=0, ceil_mode=False) -> "Tensor":
        """tinygrad-faithful max_pool2d: _pool unfold then a max-reduce
        over the trailing window axes.  Stays on the autograd graph (the
        old Phase-3A path realized to numpy and returned a fresh tensor,
        which severed backward -- so every conv UPSTREAM of a pool got
        no gradient).  Port of tinygrad/mixin/__init__.py:max_pool2d."""
        del ceil_mode  # ceildiv output sizing is a Phase-later path
        if isinstance(kernel_size, int):
            kernel_size = (kernel_size, kernel_size)
        k_ = tuple(int(k) for k in kernel_size)
        if stride is None:
            stride = k_
        if padding:
            # max-pool padding must fill with -inf so a padded cell never
            # wins the window max; thvm's pad is zero-only, so refuse
            # rather than silently zero-pad (which would corrupt the max).
            raise NotImplementedError(
                "max_pool2d padding != 0 needs a -inf pad (not yet wired)")
        s_ = (stride,) * len(k_) if isinstance(stride, int) else tuple(stride)
        d_ = (dilation,) * len(k_) if isinstance(dilation, int) else tuple(dilation)
        spatial = self._shape[-len(k_):]
        # Fast path: non-overlapping window (stride == kernel, no dilation,
        # divides evenly).  Pooling is then a pure CONTIGUOUS reshape that
        # splits each spatial axis into (out, k) -- a stride view that fuses
        # straight into the max-reduce.  The general _pool below builds an
        # overlap unfold via repeat/expand whose reshape-of-broadcast can't
        # stay a view in thvm yet, so it MATERIALIZES a huge intermediate
        # (a (B,C,20,20) pool blows up multi-GB at BS=128); the reshape path
        # sidesteps that entirely for the common stride==kernel pool.
        if (all(s == k for s, k in zip(s_, k_)) and all(d == 1 for d in d_)
                and all(i % k == 0 for i, k in zip(spatial, k_))):
            prefix = list(self._shape[:-len(k_)])
            split = []
            for i, k in zip(spatial, k_):
                split.extend([i // k, k])
            x = self.reshape(*prefix, *split)
            # window (k) axes are the odd trailing positions; reduce them.
            base = len(prefix)
            kaxes = tuple(base + 2 * j + 1 for j in range(len(k_)))
            return x.max(axis=kaxes)
        # General overlapping/strided/dilated case: _pool unfold + window max.
        x = self._pool(k_, stride=stride, dilation=dilation)
        return x.max(axis=tuple(range(-len(k_), 0)))

    def conv2d(self, weight: "Tensor",
               bias: "Tensor | None" = None,
               groups: int = 1, stride=1, dilation=1, padding=0) -> "Tensor":
        """tinygrad-faithful conv2d: pad + _pool unfold + single
        broadcast-mul + sum over (C_in, kH, kW).  Produces a small
        graph (O(1) ops vs the previous O(kH*kW*C_in) per-(h,w,c)
        composition) so thvm's hand-coded opts can recognize it as
        the matmul-shaped conv-reduce kernel.

        Port of tinygrad/mixin/__init__.py:conv2d (line ~1235).
        """
        if groups != 1:
            raise NotImplementedError("conv2d groups != 1: Phase 3C")
        sH, sW = (stride, stride) if isinstance(stride, int) else stride
        dH, dW = (dilation, dilation) if isinstance(dilation, int) else dilation
        if isinstance(padding, int):
            pH = pW = padding
        else:
            pH, pW = padding[0], padding[1]

        x = self
        if pH or pW:
            x = x.pad([0, 0, 0, 0, pH, pH, pW, pW])

        B, C_in = x._shape[:2]
        kH, kW = weight._shape[2:]
        C_out = weight._shape[0]
        # _pool: (B, C_in, H, W) -> (B, C_in, H_out, W_out, kH, kW)
        x = x._pool((kH, kW), stride=(sH, sW), dilation=(dH, dW))
        H_out, W_out = x._shape[2:4]

        # Broadcast x and weight over (B, C_out, H_out, W_out), placing
        # the reduce axes (C_in, kH, kW) as the TRAILING three so
        # reduce_chain_collect in src/schedule/uop_meta.c fuses the
        # three .sum() calls into one contiguous-axis REDUCE (it only
        # fuses contiguous trailing axes).  Without this the walker
        # hits triply-nested REDUCEs in the backward and explodes by
        # ~25x per output element.
        # Move x's C_in axis from position 1 to position 3 (between
        # W_out and kH).  Then add the broadcast C_out axis at 1.
        # x:      (B, C_in, H_out, W_out, kH, kW)
        #      -> (B, H_out, W_out, C_in, kH, kW)         via permute
        #      -> (B, 1, H_out, W_out, C_in, kH, kW)      via reshape
        #      -> (B, C_out, H_out, W_out, C_in, kH, kW)  via expand
        x_b = (x.permute(0, 2, 3, 1, 4, 5)
                .reshape(B, 1, H_out, W_out, C_in, kH, kW)
                .expand(B, C_out, H_out, W_out, C_in, kH, kW))
        w_b = (weight.reshape(1, C_out, 1, 1, C_in, kH, kW)
                     .expand(B, C_out, H_out, W_out, C_in, kH, kW))
        # Sum over the TRAILING three (C_in, kH, kW).  reduce_chain_collect
        # fuses these into a single REDUCE of extent C_in*kH*kW.
        out = (x_b * w_b).sum(axis=(4, 5, 6))
        if bias is not None:
            out = out + bias.reshape(1, -1, 1, 1)
        return out
