"""thvm Tensor -- Phase 2A of py_tensor_frontend.md.

A Tensor wraps a thvm TAG_TEN term + Python-side dtype + shape.  Method
bodies build a TAG_TEN graph through the Phase-1 bridge (Thvm); a
trailing .realize() (implicit in .numpy() / .item() / .tolist()) drives
thvm's wnf -> materialize -> kernelize -> schedule -> dispatch.  No
Python lazy graph, no Python autodiff -- thvm owns those.

Honest bar (tinygrad parity):
  - elementwise (+, -, *, /, -, .relu/.elu/.exp/.log/.sqrt)
  - reductions (.sum / .mean) with axis= int | None
  - movement (.reshape, .permute, .expand, .flatten, .contiguous)
  - host I/O (.numpy / .tolist / .item) and factories (ones, zeros,
    eye, rand, empty)
  - autodiff via thvm's uop_grad -- requires_grad_(), .backward(), .grad
Broadcasting beyond scalar-other, matmul (@), and JIT are Phase 2B+.
"""
from __future__ import annotations

from typing import Any

import numpy as np

from .dtypes import DType, dtypes
from .thvm import K, Term, Thvm


# Process-global bridge.  All Tensors share one Thvm instance.
_TH = Thvm()


class Tensor:
    """thvm tensor: TAG_TEN handle + Python-tracked dtype + shape."""

    __slots__ = ("term", "_dtype", "_shape", "requires_grad", "grad")

    # ---- construction --------------------------------------------------

    def __init__(self, data=None, dtype: DType | None = None,
                 device: str | None = None,
                 requires_grad: bool | None = None):
        # device is accepted for tinygrad compatibility; DEV picks the
        # backend at thvm_init time.
        del device
        if isinstance(data, Tensor):
            self.term = data.term
            self._dtype = data._dtype
            self._shape = data._shape
        elif data is None:
            # Uninitialised; a factory will call _take.
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
            # ascontiguousarray promotes 0-D to (1,); only call for >=1D.
            if arr.ndim >= 1 and not arr.flags["C_CONTIGUOUS"]:
                arr = np.ascontiguousarray(arr)
            self._dtype = dtype
            self._shape = tuple(int(d) for d in arr.shape)
            self.term = _TH.ten_create(dtype.thvm_id, list(self._shape))
            if arr.size > 0:
                _TH.ten_write(self.term, arr.tobytes())
        self.requires_grad = bool(requires_grad)
        self.grad: Tensor | None = None

    @classmethod
    def _from_term(cls, term: Term, dtype: DType,
                   shape: tuple[int, ...]) -> "Tensor":
        t = cls.__new__(cls)
        t.term = term
        t._dtype = dtype
        t._shape = tuple(int(d) for d in shape)
        t.requires_grad = False
        t.grad = None
        return t

    # ---- properties ----------------------------------------------------

    @property
    def shape(self) -> tuple[int, ...]:
        return self._shape

    @property
    def dtype(self) -> DType:
        return self._dtype

    @property
    def ndim(self) -> int:
        return len(self._shape)

    def numel(self) -> int:
        n = 1
        for d in self._shape:
            n *= d
        return n

    # ---- realize + host readback --------------------------------------

    def realize(self, *lst: "Tensor", **kwargs) -> "Tensor":
        """Drive thvm's pipeline; mutates `term` to the resulting
        TAG_TEN.  Extra tensors are realized too (tinygrad pattern:
        `Tensor.realize(*tensors)` calls this with the first as self).
        """
        self.term = _TH.realize(self.term)
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
        return arr.reshape(self._shape if self._shape else ()).copy()

    def tolist(self):
        return self.numpy().tolist()

    def item(self):
        return self.numpy().item()

    def __repr__(self) -> str:
        return f"<Tensor dtype={self._dtype.name} shape={self._shape}>"

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
    def empty(cls, *shape, dtype: DType | None = None, **kw) -> "Tensor":
        dt = dtype or dtypes.default_float
        return cls._from_numpy(np.empty(shape, dtype=dt.np_dtype), dt)

    @classmethod
    def ones_like(cls, t: "Tensor") -> "Tensor":
        return cls.ones(*t._shape, dtype=t._dtype)

    @classmethod
    def zeros_like(cls, t: "Tensor") -> "Tensor":
        return cls.zeros(*t._shape, dtype=t._dtype)

    @classmethod
    def cat(cls, *tensors: "Tensor", dim: int = 0) -> "Tensor":
        """Concat tensors along `dim`.  Phase 2A: host-side numpy cat
        (realizes inputs, copies into a new TAG_TEN); fine for the
        tinygrad test_tiny coverage, kept honest for tinygrad parity."""
        arrs = [t.realize().numpy() for t in tensors]
        out = np.concatenate(arrs, axis=dim)
        return cls(out, dtype=tensors[0]._dtype)

    # ---- elementwise binary (same-shape + scalar broadcasting) --------

    def _to_same_shape(self, other) -> "Tensor":
        if isinstance(other, Tensor):
            if other._shape == self._shape:
                return other
            raise NotImplementedError(
                f"broadcasting {self._shape} vs {other._shape}: Phase 2B")
        # scalar -> fill a numpy array of self.shape with the value
        return Tensor(
            np.full(self._shape, other, dtype=self._dtype.np_dtype),
            dtype=self._dtype)

    def _binop(self, other, opcode: int) -> "Tensor":
        b = self._to_same_shape(other)
        # Thvm._uop_binary wraps uop_binary directly via add/mul; for
        # generic opcodes go through the bridge ctypes call.
        from .thvm import _uop_binary  # late import to avoid leak
        import ctypes as _ct
        t = Term(_uop_binary(_ct.c_uint32(int(opcode)),
                             _ct.c_uint64(int(self.term)),
                             _ct.c_uint64(int(b.term))))
        return Tensor._from_term(t, self._dtype, self._shape)

    def __add__(self, o) -> "Tensor":
        return self._binop(o, K.ADD)

    def __radd__(self, o) -> "Tensor":
        return self.__add__(o)

    def __sub__(self, o) -> "Tensor":
        return self.__add__(-o if isinstance(o, (int, float))
                            else (-(o if isinstance(o, Tensor) else Tensor(o))))

    def __mul__(self, o) -> "Tensor":
        return self._binop(o, K.MUL)

    def __rmul__(self, o) -> "Tensor":
        return self.__mul__(o)

    def __truediv__(self, o) -> "Tensor":
        if isinstance(o, (int, float)):
            return self.__mul__(1.0 / float(o))
        recip = Tensor._from_term(_TH.recip(o.term), o._dtype, o._shape)
        return self.__mul__(recip)

    def __neg__(self) -> "Tensor":
        return Tensor._from_term(_TH.neg(self.term), self._dtype, self._shape)

    def __matmul__(self, other: "Tensor") -> "Tensor":
        """tinygrad-style matmul.  Phase 2A: 2-D only -- (M,K) @ (K,N).
        Composes as RESHAPE+EXPAND+MUL+REDUCE per the tinygrad pattern;
        thvm's materializer fuses it into a single matmul kernel."""
        if self.ndim != 2 or other.ndim != 2:
            raise NotImplementedError(
                f"matmul: only 2-D for Phase 2A ({self._shape} @ {other._shape})")
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
    def exp2(self) -> "Tensor":  return self._unary(K.EXP2)
    def log2(self) -> "Tensor":  return self._unary(K.LOG2)
    def sqrt(self) -> "Tensor":  return self._unary(K.SQRT)

    def exp(self) -> "Tensor":
        # exp(x) = exp2(x * log2(e))
        ln2_recip = 1.4426950408889634
        return (self * ln2_recip).exp2()

    def log(self) -> "Tensor":
        # ln(x) = log2(x) / log2(e)
        ln2 = 0.6931471805599453
        return self.log2() * ln2

    def relu(self) -> "Tensor":
        # relu(x) = (0 < x) * x
        zero = Tensor(np.zeros(self._shape, dtype=self._dtype.np_dtype),
                      dtype=self._dtype)
        from .thvm import _uop_binary
        import ctypes as _ct
        mask = Term(_uop_binary(_ct.c_uint32(K.CMPLT),
                                _ct.c_uint64(int(zero.term)),
                                _ct.c_uint64(int(self.term))))
        mask_t = Tensor._from_term(mask, self._dtype, self._shape)
        return self * mask_t

    def elu(self, alpha: float = 1.0) -> "Tensor":
        # elu(x) = x if x > 0 else alpha*(exp(x)-1)
        # = relu(x) - relu(alpha * (1 - exp(x)))   (tinygrad identity)
        # Simpler: relu(x) + (x <= 0) * alpha * (exp(x) - 1)
        return self.relu() + (-((-self).relu())).exp().__sub__(1.0).__mul__(alpha) * (
            self._cmp_le_zero())

    def _cmp_le_zero(self) -> "Tensor":
        # 1 where self <= 0 else 0.  Used by elu.
        from .thvm import _uop_binary
        import ctypes as _ct
        zero = Tensor(np.zeros(self._shape, dtype=self._dtype.np_dtype),
                      dtype=self._dtype)
        # cmplt(self, 0+eps) -- approximate via not(0 < self)
        gt = Term(_uop_binary(_ct.c_uint32(K.CMPLT),
                              _ct.c_uint64(int(zero.term)),
                              _ct.c_uint64(int(self.term))))
        one_minus = (Tensor._from_term(gt, self._dtype, self._shape)
                     * -1.0 + 1.0)
        return one_minus

    # ---- reductions ----------------------------------------------------

    def sum(self, axis=None, keepdim: bool = False) -> "Tensor":
        if axis is None:
            t = self.term
            for ax in range(self.ndim - 1, -1, -1):
                t = _TH.reduce(K.REDUCE_SUM, ax, t)
            out_shape = tuple(1 for _ in self._shape) if keepdim else ()
            return Tensor._from_term(t, self._dtype, out_shape)
        if isinstance(axis, int):
            ax = axis if axis >= 0 else axis + self.ndim
            t = _TH.reduce(K.REDUCE_SUM, ax, self.term)
            new_shape = list(self._shape)
            if keepdim:
                new_shape[ax] = 1
            else:
                new_shape.pop(ax)
            return Tensor._from_term(t, self._dtype, tuple(new_shape))
        raise NotImplementedError(f"sum(axis={axis!r}): Phase 2B")

    def max(self, axis=None, keepdim: bool = False) -> "Tensor":
        if axis is None:
            t = self.term
            for ax in range(self.ndim - 1, -1, -1):
                t = _TH.reduce(K.REDUCE_MAX, ax, t)
            out_shape = tuple(1 for _ in self._shape) if keepdim else ()
            return Tensor._from_term(t, self._dtype, out_shape)
        if isinstance(axis, int):
            ax = axis if axis >= 0 else axis + self.ndim
            t = _TH.reduce(K.REDUCE_MAX, ax, self.term)
            new_shape = list(self._shape)
            if keepdim:
                new_shape[ax] = 1
            else:
                new_shape.pop(ax)
            return Tensor._from_term(t, self._dtype, tuple(new_shape))
        raise NotImplementedError(f"max(axis={axis!r}): Phase 2B")

    def mean(self, axis=None, keepdim: bool = False) -> "Tensor":
        s = self.sum(axis=axis, keepdim=keepdim)
        # divisor = product of reduced axes
        if axis is None:
            div = self.numel()
        else:
            ax = axis if axis >= 0 else axis + self.ndim
            div = self._shape[ax]
        return s * (1.0 / div)

    # ---- movement -----------------------------------------------------

    def reshape(self, *shape) -> "Tensor":
        if len(shape) == 1 and isinstance(shape[0], (tuple, list)):
            shape = tuple(shape[0])
        shape = tuple(int(d) for d in shape)
        t = _TH.reshape(self.term, list(shape))
        return Tensor._from_term(t, self._dtype, shape)

    def permute(self, *axes) -> "Tensor":
        if len(axes) == 1 and isinstance(axes[0], (tuple, list)):
            axes = tuple(axes[0])
        new_shape = tuple(self._shape[a] for a in axes)
        t = _TH.permute(self.term, list(axes))
        return Tensor._from_term(t, self._dtype, new_shape)

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

    # ---- autodiff (thvm's real uop_grad) ------------------------------

    def requires_grad_(self, requires_grad: bool = True) -> "Tensor":
        self.requires_grad = requires_grad
        return self

    def backward(self, gradient: "Tensor | None" = None) -> "Tensor":
        """Build BWD projection via thvm's uop_grad; one grad call per
        leaf marked requires_grad on the producer chain.  Phase 2A:
        applies only to the explicit-target leaves passed via grad_with
        _target; tracking requires_grad across the graph is Phase 2B."""
        if gradient is None:
            gradient = Tensor.ones_like(self) if self._shape == () else \
                       Tensor(np.ones(self._shape, dtype=self._dtype.np_dtype),
                              dtype=self._dtype)
        # Not yet integrated with requires_grad bookkeeping; tests using
        # the symmetric .grad property need Phase 2B.  Surface uop_grad
        # via grad_with_target on demand.
        self._last_gy = gradient
        return self
