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
import warnings
import weakref
from typing import Any, Sequence

import numpy as np

from .dtypes import DType, dtypes
from .thvm import K, Term, Thvm, _uop_binary


# Process-global bridge.  All Tensors share one Thvm instance.
_TH = Thvm()
_TAG_TEN = K.TAG_TEN
# Bundle ASSIGN-bearing realizes through realize_many (one fire scope) by
# default -- with the precise fire memo (default-on in uop_kernel.c) this
# kills the ~8x dispatch redundancy of the per-tensor optimizer step
# (loss + eval-acc byte-identical, validated BS=8/32/128).  Opt out with
# THVM_BUNDLE_ASSIGNS=0 for the cross-dependent-assign edge case (a
# sibling's src reading a sibling's freshly-written dst in one bundle).
import os as _os
_BUNDLE_ASSIGNS = _os.environ.get('THVM_BUNDLE_ASSIGNS', '1') != '0'

# Routes a realized TAG_TEN's tid back to its originating Python Tensor.
# backward() walks the in-scope leaf tids of the loss term, looks each
# up here, and (for the float ones) auto-fills .grad -- the tinygrad
# spec, where backward() fills .grad for every in-scope non-CONST float
# leaf with no requires_grad flag anywhere.  WeakValueDictionary so dead
# tensors auto-drop; nn parameters stay alive via their layer.  Every
# realized leaf registers here in _pin(), not just "parameters".
_LEAF_TENSORS: "weakref.WeakValueDictionary[int, Tensor]" = \
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


def _term_is_assign(t: int) -> bool:
    """True iff term `t` is a TAG_UOP whose opcode is UOP_ASSIGN.  Used
    by Tensor.realize to gate the realize_many bundle: pure-elementwise
    bundles share forward intermediates via the cross-realize loc->tid
    cache (schedule/materialize.c), but ASSIGN bundles must run
    one-at-a-time to avoid in-place buffer aliasing across siblings."""
    if _TH.term_tag(t) != K.TAG_UOP:
        return False
    return _TH.term_ext(t) == K.ASSIGN


# thvm's `where`/`maximum`/`minimum` compose through a 0/1 mask MULTIPLY
# (no float WHERE/select ALU op), so a literal +/-inf operand poisons the
# UNSELECTED lanes with 0*inf = NaN.  Substituting a large finite f32
# sentinel (+/-3.0e38, the f32 max magnitude) at construction keeps every
# downstream use mathematically identical -- softmax(qk + -3e38) underflows
# to exactly 0, the same as a -inf additive causal mask -- while staying
# NaN-safe.  This is the standard finite-large-negative causal-mask idiom.
_F32_SENTINEL = 3.0e38


def _finite_sentinel(value, dt: "DType"):
    """Clamp a non-finite scalar const to a large finite f32 sentinel for
    float dtypes (see _F32_SENTINEL note).  Pass non-float / finite values
    through untouched."""
    if dt.thvm_id != dtypes.float32.thvm_id:
        return value
    try:
        fv = float(value)
    except (TypeError, ValueError):
        return value
    if math.isinf(fv):
        return _F32_SENTINEL if fv > 0 else -_F32_SENTINEL
    return value


def _normalize_pairs(arg, shape, full):
    """Normalize a shrink/pad argument to a flat [b0,e0,b1,e1,...] list.

    Accepts the tinygrad tuple-of-pairs form (each element is a
    `(begin, end)` pair or `None` -> `full(axis_size)`) OR the already-flat
    int list.  `full(n)` supplies the per-axis default (begin,end) for a
    `None` element (full-range for shrink, no-pad for pad)."""
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
    """Coerce a Python scalar (or Tensor) into a Tensor matching self's
    dtype.  Broadcasts to self.shape when other is a scalar."""
    if isinstance(other, Tensor):
        return other
    arr = np.asarray(other, dtype=self_t._dtype.np_dtype)
    return Tensor(arr, dtype=self_t._dtype)


class Tensor:
    """thvm tensor: TAG_TEN handle + Python-tracked dtype + shape."""

    __slots__ = ("term", "_dtype", "_shape", "is_param", "grad",
                 "_pin_id", "_view_base", "_view_begin_end", "__weakref__")

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
                 device: str | None = None):
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
        # is_param (tinygrad nn.optim semantics): every Tensor is a
        # candidate optimizer parameter by default; a buffer opts out via
        # .is_param_(False) (BatchNorm running stats, Adam bias-correction
        # state).  This drives ONLY the optimizer's param/buffer split --
        # backward() auto-fills .grad for every in-scope float leaf
        # regardless of is_param (tinygrad has no requires_grad).
        self.is_param: bool = True
        self.grad: Tensor | None = None
        self._pin_id = 0
        self._view_base = None
        self._view_begin_end = None
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
        t._pin()
        return t

    # ---- live-tensor pinning (cross-step buffer reclaim) --------------

    def _pin(self) -> None:
        """Pin the current term if it is a realized TAG_TEN, so reclaim()
        keeps its buffer, AND register it in _LEAF_TENSORS so backward()
        can route the leaf's tid back to this Tensor.  Re-pinning (term
        changed via realize/assign) drops the stale handle entry first to
        avoid leaking pin slots."""
        global _PIN_NEXT
        if int(_TH.term_tag(self.term)) != _TAG_TEN:
            return
        if self._pin_id == 0:
            self._pin_id = _PIN_NEXT
            _PIN_NEXT += 1
        else:
            _TH.pin_drop(self._pin_id)
        _TH.pin_set(self._pin_id, self.term)
        tid = int(_TH.term_val(self.term))
        if tid > 0:
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
    def ndim(self) -> int: return len(self._shape)

    def numel(self) -> int:
        n = 1
        for d in self._shape: n *= d
        return n

    # ---- realize + host readback --------------------------------------

    def realize(self, *lst: "Tensor", **kwargs) -> "Tensor":
        """Drive thvm's pipeline; mutates `term` to the resulting
        TAG_TEN.  When the input set is ASSIGN-free we bundle through
        realize_many so the shared forward intermediates inside
        backward grad chains get emitted once per pass (the
        cross-realize loc -> tid cache in schedule/materialize.c lives
        for one realize call's scope -- mirrors tinygrad's
        per-schedule _realize_cache).

        ASSIGN-bearing bundles still go per-tensor: a bundled
        realize_many of multiple in-place ASSIGNs lets the buffer
        planner alias across siblings, which racing reads can corrupt
        (the previous "CUDA NaN under bundling" failure mode -- still
        latent on CPU when an ASSIGN's src reads a sibling's already-
        written buffer).  Adam/SGD _step build pure-ASSIGN bundles, so
        their realize(*mv) and realize(*out) paths stay one-at-a-time;
        loss.realize(*grads) is ASSIGN-free and bundles."""
        if not lst:
            self.term = _TH.realize(self.term)
            self._pin()
            return self
        terms_all = [self.term] + [x.term for x in lst]
        has_assign = any(_term_is_assign(t) for t in terms_all)
        if has_assign and not _BUNDLE_ASSIGNS:
            self.term = _TH.realize(self.term)
            self._pin()
            for x in lst:
                x.realize()
            return self
        out = _TH.realize_many(terms_all)
        self.term = out[0]
        self._pin()
        for x, r in zip(lst, out[1:]):
            x.term = r
            x._pin()
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
        """tinygrad detach -- stop-gradient.  Wraps in UOP_DETACH:
        identity forward (unwrapped before materialize), zero backward
        (the cotangent dies at the detach).  Used e.g. by BatchNorm to
        keep the variance gradient from flowing through the mean."""
        return Tensor._from_term(_TH.detach(self.term), self._dtype,
                                 self._shape)

    def assign(self, x: "Tensor") -> "Tensor":
        """tinygrad assign -- lazy IN-PLACE update.  Builds UOP_ASSIGN(self,
        x); realizing it copies x's buffer into self's (the dst tid/buffer
        stay stable) and returns self.  In-place + lazy is what makes the
        optimizer step JIT-capturable (jit_capture_record_assign) and keeps
        param buffers stable across replays -- exactly tinygrad/WL TAssign."""
        x = x if isinstance(x, Tensor) else Tensor(x, dtype=self._dtype)
        # View dst (a shrink slice, e.g. the kv-cache write
        # `cache[:, :, p:p+n].assign(...)`): thvm's eager UOP_ASSIGN copies
        # whole buffers and ignores the view offset/strides, so it would
        # never reach the base buffer.  Scatter host-side into the base
        # tensor's buffer instead, then realize the base in place.
        if self._view_base is not None and self._view_begin_end is not None:
            base = self._view_base
            base.realize()
            arr = base.numpy()
            be = self._view_begin_end
            sl = tuple(slice(be[2*i], be[2*i+1]) for i in range(base.ndim))
            arr[sl] = x.numpy().reshape(arr[sl].shape)
            new_base = Tensor(arr, dtype=base._dtype).realize()
            # Re-point the base tensor at the freshly written buffer so any
            # later read through `base` (the kv-cache) sees the update.
            base.term = new_base.term
            base._pin()
            self.term = new_base.term
            self._dtype = new_base._dtype
            self._shape = new_base._shape
            self._view_base = None
            self._view_begin_end = None
            self._pin()
            return self
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

    def is_param_(self, is_param: bool = True) -> "Tensor":
        """Mark whether this Tensor is an optimizer parameter (tinygrad
        Tensor.is_param_).  Buffers (BatchNorm running stats, Adam bias-
        correction state) call .is_param_(False) so the optimizer's
        param/buffer split skips them.  Has NO effect on backward() --
        gradients flow to every in-scope float leaf regardless."""
        self.is_param = is_param
        return self

    def backward(self, gradient: "Tensor | None" = None) -> "Tensor":
        """Auto-grad-all-float-leaves backward (tinygrad spec): build ONE
        uop_grad(y, gy) root and drive wnf so the chain rule visits every
        in-scope leaf once, accumulating each one's cotangent into
        TENS[tid].grad (grad_leaf_sup's target==0 path).  There is no
        requires_grad flag: .grad is filled for EVERY in-scope non-CONST
        float leaf, and the OPTIMIZER's param list (not a flag) decides
        what updates.

        In-scope leaves are the distinct TAG_TEN leaf tids reachable from
        the loss term (uop_leaf_tids -- tinygrad's all_tensors-in-scope
        filter over thvm's term graph).  We transiently mark the float
        ones for the accumulation walk, fire wnf, read back each leaf's
        TenDesc.grad, then unmark to leave global grad state untouched.

        wnf only drives the term-level chain-rule rewrite -- no kernels,
        no materialize, no dispatch.  The gradient values stay lazy inside
        TenDesc.grad; Tensor.realize(*grads) drives the pipeline downstream
        just like tinygrad."""
        if gradient is None:
            if self.numel() != 1:
                raise RuntimeError(
                    "backward(): implicit gradient only for scalar outputs")
            gradient = (Tensor(1.0, dtype=self._dtype) if not self._shape
                        else Tensor.ones(*self._shape, dtype=self._dtype))
        # In-scope leaves -> their live Tensors.  Mark every FLOAT leaf for
        # the accumulation walk (thvm TEN leaves are buffers, never CONST,
        # so the tinygrad CONST exclusion is the float filter here).
        scope_tids = _TH.uop_leaf_tids(self.term)
        leaves: "list[Tensor]" = []
        for tid in scope_tids:
            leaf = _LEAF_TENSORS.get(tid)
            if leaf is None or not dtypes.is_float(leaf._dtype):
                continue
            _TH.ten_set_grad_leaf(leaf.term, True)
            leaves.append(leaf)
        # Guard: warn if any leaf entered with a stale gradient from a
        # prior backward().  thvm ACCUMULATES into TenDesc.grad (tinygrad
        # semantics), so a forgotten opt.zero_grad() silently piles
        # gradients across steps -- a footgun, since PyTorch users expect a
        # fresh .grad each backward.  Soft, de-duped warning; intentional
        # gradient accumulation can filter it.
        for leaf in leaves:
            if _TH.ten_get_grad(leaf.term) != 0:
                warnings.warn(
                    "backward(): a leaf entered with a non-zero gradient "
                    "from a prior backward(); thvm accumulates into "
                    "TenDesc.grad, so call opt.zero_grad() between backward "
                    "passes to avoid stale accumulation (intentional "
                    "accumulation can filter this warning).",
                    stacklevel=2)
                break
        try:
            # Build the walk-once BWD root and drive wnf to fire the chain
            # rule (interact_grad -> grad_leaf_sup accumulator).
            bwd = _TH.grad(self.term, gradient.term)
            _TH.wnf(bwd)
            # Wrap each in-scope leaf's TenDesc.grad term.
            for leaf in leaves:
                g_term = _TH.ten_get_grad(leaf.term)
                if g_term != 0:
                    leaf.grad = Tensor._from_term(Term(g_term),
                                                  leaf._dtype, leaf._shape)
        finally:
            # Restore global grad state so a later backward() over a
            # different loss starts from a clean mark set.
            for leaf in leaves:
                _TH.ten_set_grad_leaf(leaf.term, False)
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
        value = _finite_sentinel(value, dt)
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
        """Matmul -- composes RESHAPE+EXPAND+MUL+REDUCE; thvm fuses.

        2-D @ 2-D takes the byte-identical specialized path below.
        N-D (batched / broadcast) matmul ports tinygrad's dot rule
        (tinygrad/mixin/__init__.py:341-346) with thvm's EXPLICIT
        broadcast (thvm mul does not implicit-broadcast): insert a
        broadcast-1 slot, transpose w's contraction axis to trailing,
        expand the leading batch dims to the numpy-broadcast batch,
        then the same MUL + trailing-reduce as the 2-D path.
        """
        if self.ndim == 2 and other.ndim == 2:
            M, K_in = self._shape
            K2, N = other._shape
            if K_in != K2:
                raise ValueError(
                    f"matmul shape mismatch: {self._shape} @ {other._shape}")
            a = self.reshape(M, K_in, 1).expand(M, K_in, N)
            b = other.reshape(1, K_in, N).expand(M, K_in, N)
            return (a * b).sum(axis=1)
        return self._dot_nd(other)

    def _dot_nd(self, w: "Tensor") -> "Tensor":
        """N-D dot, faithful to tinygrad/mixin/__init__.py:341-346.

        tinygrad:
            axis_w = -min(w.ndim, 2)
            x = x.reshape(*x.shape[:-1], *[1]*min(dx-1,dw-1,1), x.shape[-1])
            w = w.reshape(*w.shape[:-2], *[1]*min(dx-1,dw-1,1),
                          *w.shape[axis_w:]).transpose(-1, axis_w)
            return (x*w).sum(-1)
        tinygrad's mul implicitly broadcasts the leading batch dims;
        thvm's does not, so after the reshape/transpose we compute the
        numpy-broadcast common shape and .expand() both operands to it.
        """
        x = self
        dx, dw = x.ndim, w.ndim
        if not (dx > 0 and dw > 0):
            raise RuntimeError(
                f"both tensors need to be at least 1D, got {dx}D and {dw}D")
        axis_w = -min(dw, 2)
        if x._shape[-1] != w._shape[axis_w]:
            raise RuntimeError(f"cannot dot {x._shape} and {w._shape}")

        pad = (1,) * min(dx - 1, dw - 1, 1)
        # x: (..., M, 1?, K)
        xs = (*x._shape[:-1], *pad, x._shape[-1])
        x = x.reshape(*xs)
        # w: (..., 1?, *w.shape[axis_w:]) then move contraction axis -> last.
        ws = (*w._shape[:-2], *pad, *w._shape[axis_w:])
        w = w.reshape(*ws).transpose(-1, axis_w)

        # Now the trailing-2 of x is (.., 1?, K) and of w is (.., K, N?).
        # The contraction axis K is x's last and w's axis -2 (after the
        # transpose).  Broadcast every remaining dim numpy-style, then
        # let the elementwise MUL line up + trailing-reduce over K.
        xs, ws = x._shape, w._shape
        nd = max(len(xs), len(ws))
        xs_f = (1,) * (nd - len(xs)) + tuple(xs)
        ws_f = (1,) * (nd - len(ws)) + tuple(ws)
        if xs_f != xs:
            x = x.reshape(*xs_f)
        if ws_f != ws:
            w = w.reshape(*ws_f)
        bshape = []
        for a, b in zip(xs_f, ws_f):
            if a == b or a == 1 or b == 1:
                bshape.append(max(a, b))
            else:
                raise RuntimeError(
                    f"cannot dot {self._shape} and {w._shape}: "
                    f"non-broadcastable {a} vs {b}")
        bshape = tuple(bshape)
        if x._shape != bshape:
            x = x.expand(*bshape)
        if w._shape != bshape:
            w = w.expand(*bshape)
        return (x * w).sum(axis=-1)

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
        gradient flows only through `self` (the logits).

        Under TinyJit:  Y.numpy() does a host-side read, which means the
        one-hot buffer baked into the captured graph is step-1's labels;
        replays reuse those frozen labels instead of refilling.  For a
        JIT-capturable train step, pre-allocate a one-hot Tensor outside
        the JIT and pass it as the `oh` kwarg of the lower-level call --
        the caller writes fresh one-hots into oh's buffer each iter."""
        ax = axis if axis >= 0 else axis + self.ndim
        n_classes = self._shape[ax]
        labels = Y.numpy().astype(np.int64).reshape(-1)
        oh = np.zeros((labels.shape[0], n_classes), dtype=np.float32)
        oh[np.arange(labels.shape[0]), labels] = 1.0
        ls = self.log_softmax(axis=ax)
        return (ls * Tensor(oh.reshape(self._shape))).sum(axis=ax).mean() * -1.0

    def cross_entropy_from_onehot(self, OH: "Tensor",
                                  axis: int = -1) -> "Tensor":
        """JIT-capturable cross-entropy: caller pre-allocates a one-hot
        Tensor OH (same shape as `self`'s logits, BUT with the class
        dim being n_classes) and refills it between calls.  The graph
        reads OH's stable buf_id, so replay sees fresh one-hots."""
        ax = axis if axis >= 0 else axis + self.ndim
        ls = self.log_softmax(axis=ax)
        return (ls * OH).sum(axis=ax).mean() * -1.0

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
        # JIT-friendly: cache the zero tensor PER SHAPE so the captured
        # graph references a stable buf_id.  An earlier impl allocated a
        # fresh zero each call (`Tensor(np.zeros(...))`), which broke
        # TinyJit -- the captured op held the temp zero's buf_id but
        # the Python wrapper went out of scope so subsequent replays
        # saw a freed/recycled buffer.  Memoizing keeps the zero alive.
        if not hasattr(Tensor, "_zero_cache"):
            Tensor._zero_cache = {}
        key = (self._shape, self._dtype.name)
        zero = Tensor._zero_cache.get(key)
        if zero is None:
            zero = Tensor(np.zeros(self._shape, dtype=self._dtype.np_dtype),
                          dtype=self._dtype)
            zero.realize()
            Tensor._zero_cache[key] = zero
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

    def square(self) -> "Tensor":
        # tinygrad mixin/elementwise.py:498 -- square(x) = x*x.
        return self * self

    def __lt__(self, other) -> "Tensor":
        # CMPLT lowers to a 0.0/1.0 mask (same op relu/maximum use).
        a, b = self._broadcast(other)
        less = Term(_uop_binary(_ct.c_uint32(K.CMPLT),
                                _ct.c_uint64(int(a.term)),
                                _ct.c_uint64(int(b.term))))
        return Tensor._from_term(less, a._dtype, a._shape)

    def __gt__(self, other) -> "Tensor":
        return _wrap_other(self, other).__lt__(self)

    def where(self, a, b) -> "Tensor":
        """tinygrad Tensor.where -- self is a 0/1 (or boolean) mask;
        returns mask*a + (1-mask)*b.  thvm has no ternary WHERE ALU op,
        so it is composed from the 0/1 CMPLT mask exactly like maximum."""
        mask = self
        a_t = a if isinstance(a, Tensor) else None
        b_t = b if isinstance(b, Tensor) else None
        out_shape = mask._shape
        if a_t is not None:
            mask, a_t = mask._broadcast(a_t)
            out_shape = mask._shape
        if b_t is not None:
            mask2, b_t = mask._broadcast(b_t)
            mask = mask2
            out_shape = mask._shape
        sel_a = (mask * a) if a_t is not None else (mask * float(a))
        inv = 1.0 - mask
        sel_b = (inv * b) if b_t is not None else (inv * float(b))
        return sel_a + sel_b

    def newton_schulz(self, steps: int, params, eps: float = 1.0e-7) -> "Tensor":
        """Newton-Schulz orthogonalization of an odd polynomial, port of
        tinygrad/mixin/__init__.py:1444-1459.

        For coefficients ``params = (a, b, c)`` (the Muon quintic) each
        step computes ``G <- a*G + b*(GG^T)G + c*(GG^T)^2 G`` where the
        left factor ``GG^T`` is always formed from the CURRENT step's G
        (tinygrad's ``functools.reduce(lambda x,y:(y@y.T)@x,[G]*i,G)``
        feeds the original ``y=G`` into every ``(y@y.T)`` factor, only the
        rightmost ``@x`` chains).  G is Frobenius-normalized first; a
        tall matrix (shape[-2] > shape[-1]) is transposed in/out so the
        contraction stays on the smaller axis."""
        assert self.ndim > 1, "NS only works for two or more dims"
        if self._shape[-2] > self._shape[-1]:
            return self.transpose(-2, -1).newton_schulz(
                steps, params, eps).transpose(-2, -1)
        G = self / (self.square().sum(axis=(-2, -1), keepdim=True).sqrt() + eps)
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
        # tinygrad allows None at a position to mean "keep this dim".
        shape = tuple(self._shape[i] if d is None else d
                      for i, d in enumerate(shape))
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
        """Pad each axis with zeros.  Accepts either the tinygrad
        tuple-of-pairs form (one `(before, after)` pair or `None` per
        axis) or the flat form [b0, e0, b1, e1, ...]."""
        begin_end = _normalize_pairs(begin_end, self._shape,
                                     full=lambda n: (0, 0))
        new_shape = tuple(self._shape[i] + int(begin_end[2*i]) + int(begin_end[2*i+1])
                          for i in range(self.ndim))
        t = _TH.pad(self.term, [int(x) for x in begin_end])
        return Tensor._from_term(t, self._dtype, new_shape)

    def shrink(self, begin_end) -> "Tensor":
        """Take a sub-range per axis.  Accepts either:
          * the tinygrad tuple-of-pairs form: one `(begin, end)` pair (or
            `None` for the full axis) PER axis -- e.g.
            `shrink(((tok, tok+1), None))`; or
          * the flat form: 2*ndim ints [b0, e0, b1, e1, ...].
        Both normalize to the flat (begin, end) the bridge expects."""
        begin_end = _normalize_pairs(begin_end, self._shape,
                                     full=lambda n: (0, n))
        new_shape = tuple(int(begin_end[2*i+1]) - int(begin_end[2*i])
                          for i in range(self.ndim))
        t = _TH.shrink(self.term, [int(x) for x in begin_end])
        out = Tensor._from_term(t, self._dtype, new_shape)
        # Remember the source + slice so assign() into this view can
        # scatter back into the base buffer (thvm's eager UOP_ASSIGN does
        # a flat full-buffer copy that ignores the view offset/strides;
        # the kv-cache write `cache[:, :, p:p+n].assign(...)` relies on a
        # real strided scatter).  See assign().
        out._view_base = self
        out._view_begin_end = [int(x) for x in begin_end]
        return out

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

        if groups == 1:
            # Broadcast x and weight over (B, C_out, H_out, W_out),
            # placing the reduce axes (C_in, kH, kW) as the TRAILING
            # three so reduce_chain_collect in src/schedule/uop_meta.c
            # fuses the three .sum() calls into one contiguous-axis
            # REDUCE (it only fuses contiguous trailing axes).  Without
            # this the walker hits triply-nested REDUCEs in the backward
            # and explodes by ~25x per output element.
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
            # Sum over the TRAILING three (C_in, kH, kW).
            # reduce_chain_collect fuses these into a single REDUCE of
            # extent C_in*kH*kW.
            out = (x_b * w_b).sum(axis=(4, 5, 6))
            if bias is not None:
                out = out + bias.reshape(1, -1, 1, 1)
            return out

        # Grouped / depthwise: port of tinygrad/mixin/__init__.py:1244-1249
        # specialized to 2 spatial dims (HW=(kH,kW), oyx=(H_out,W_out)).
        # The channel dim C_in == groups*cin is split into (groups, cin);
        # the weight's C_out == groups*rcout splits into (groups, rcout).
        # The reduce axes (cin, kH, kW) stay TRAILING-three for fusion.
        cin = weight._shape[1]
        if groups * cin != C_in:
            raise ValueError(
                f"conv2d groups mismatch: groups({groups})*cin({cin}) "
                f"!= C_in({C_in})")
        rcout = C_out // groups
        # x:  (B, C_in, H_out, W_out, kH, kW)
        #  -> (B, groups, cin, 1, H_out, W_out, kH, kW)        reshape
        #  -> (B, groups, cin, rcout, H_out, W_out, kH, kW)    expand
        #  -> (B, groups, rcout, H_out, W_out, cin, kH, kW)    permute
        x_b = (x.reshape(B, groups, cin, 1, H_out, W_out, kH, kW)
                .expand(B, groups, cin, rcout, H_out, W_out, kH, kW)
                .permute(0, 1, 3, 4, 5, 2, 6, 7))
        w_b = (weight.reshape(1, groups, rcout, 1, 1, cin, kH, kW)
                     .expand(B, groups, rcout, H_out, W_out, cin, kH, kW))
        # Sum over the TRAILING three (cin, kH, kW); merge groups*rcout
        # back into C_out.
        out = (x_b * w_b).sum(axis=(5, 6, 7)).reshape(B, C_out, H_out, W_out)
        if bias is not None:
            out = out + bias.reshape(1, -1, 1, 1)
        return out

    # ---- transformer / GPT2 ops ---------------------------------------

    @property
    def T(self) -> "Tensor":
        """Transpose of the last two dims (tinygrad's `.T`)."""
        return self.transpose()

    @classmethod
    def arange(cls, start, stop=None, step=1,
               dtype: DType | None = None) -> "Tensor":
        """1-D ramp [start, stop) by `step` (tinygrad/mixin arange)."""
        if stop is None:
            stop, start = start, 0
        if dtype is None:
            dtype = (dtypes.default_float
                     if any(isinstance(x, float) for x in (start, stop, step))
                     else dtypes.default_int)
        return cls._from_numpy(
            np.arange(start, stop, step).astype(dtype.np_dtype, copy=False),
            dtype)

    def cast(self, dtype: DType) -> "Tensor":
        """Cast to `dtype` (thvm UOP_CAST)."""
        if dtype == self._dtype:
            return self
        t = _TH.cast(self.term, dtype.thvm_id)
        return Tensor._from_term(t, dtype, self._shape)

    def float(self) -> "Tensor":
        return self.cast(dtypes.float32)

    def half(self) -> "Tensor":
        # thvm's default Tensor surface is fp32-only; half() is a no-op
        # passthrough (gpt2 only takes the half path under HALF=1, which
        # the thvm bench does not set).
        return self

    def to(self, device) -> "Tensor":
        """Device move (tinygrad `.to`); thvm picks the device from DEV at
        process start, so this is an identity that realizes the data."""
        del device
        return self

    def const_like(self, value) -> "Tensor":
        """A tensor of `value` with self's shape + dtype (tinygrad
        const_like)."""
        value = _finite_sentinel(value, self._dtype)
        return Tensor(np.full(self._shape, value, dtype=self._dtype.np_dtype),
                      dtype=self._dtype)

    def gelu(self, approximate: str = "tanh") -> "Tensor":
        """Gaussian Error Linear Unit (tanh approximation), port of
        tinygrad/mixin/elementwise.py:gelu."""
        return 0.5 * self * (
            1.0 + (math.sqrt(2.0 / math.pi)
                   * (self + 0.044715 * (self * self * self))).tanh())

    def _tri(self, r: int, c: int, diagonal: int = 0) -> "Tensor":
        """Boolean (0/1) triangular mask: arange(r)[:,None]+diag <=
        arange(c).  Port of tinygrad/mixin/__init__.py:_tri."""
        row = Tensor.arange(r, dtype=dtypes.default_int).reshape(r, 1)
        col = Tensor.arange(c, dtype=dtypes.default_int).reshape(1, c)
        # (row + diagonal) <= col  -> 1.0 where True
        return (row + diagonal) < (col + 1)

    def triu(self, diagonal: int = 0) -> "Tensor":
        """Upper-triangular part (other elements 0).  Port of
        tinygrad/mixin/__init__.py:triu."""
        mask = self._tri(self._shape[-2], self._shape[-1], diagonal)
        return mask.where(self, self.const_like(0))

    def tril(self, diagonal: int = 0) -> "Tensor":
        """Lower-triangular part (other elements 0).  Port of
        tinygrad/mixin/__init__.py:tril."""
        mask = self._tri(self._shape[-2], self._shape[-1], diagonal + 1)
        return mask.where(self.const_like(0), self)

    @classmethod
    def stack(cls, *tensors: "Tensor", dim: int = 0) -> "Tensor":
        """Concatenate along a NEW dim (tinygrad Tensor.stack)."""
        unsq = [t.reshape(*t._shape[:dim if dim >= 0 else dim + t.ndim + 1],
                          1,
                          *t._shape[dim if dim >= 0 else dim + t.ndim + 1:])
                for t in tensors]
        return cls.cat(*unsq, dim=dim)

    def embedding(self, idx: "Tensor") -> "Tensor":
        """Gather rows of self (vocab, embed) by integer indices `idx`;
        result shape (*idx.shape, embed).  Powers nn.Embedding."""
        sel = idx.numpy().astype(np.int64)
        rows = self.numpy()[sel.reshape(-1)]
        out_shape = tuple(sel.shape) + (self._shape[-1],)
        return Tensor(rows.reshape(out_shape), dtype=self._dtype)

    def scaled_dot_product_attention(self, key: "Tensor", value: "Tensor",
                                     attn_mask: "Tensor | None" = None,
                                     dropout_p: float = 0.0,
                                     is_causal: bool = False) -> "Tensor":
        """Scaled dot-product attention, port of
        tinygrad/tensor.py:scaled_dot_product_attention.

        qk = (q @ k^T)/sqrt(d_k); optional causal/additive mask; softmax
        over the last axis; @ value."""
        q = self
        qk = (q @ key.transpose(-2, -1)) / math.sqrt(q._shape[-1])
        if is_causal:
            if attn_mask is not None:
                raise RuntimeError("cannot set attn_mask when is_causal=True")
            attn_mask = qk.const_like(1).tril()
            attn_mask = attn_mask.where(qk.const_like(0),
                                        qk.const_like(float("-inf")))
        if attn_mask is not None:
            qk = qk + attn_mask
        return qk.softmax(axis=-1) @ value

    @staticmethod
    def manual_seed(seed: int = 0) -> None:
        """Seed thvm's host RNG (tinygrad Tensor.manual_seed).  thvm's
        factories + multinomial sampling read numpy's global RNG."""
        np.random.seed(seed)

    def multinomial(self, num_samples: int = 1,
                    replacement: bool = False) -> "Tensor":
        """Sample `num_samples` indices from the multinomial distribution
        weighted by self (rows are independent).  Host-side draw against
        numpy's seeded RNG; off the autograd path (sampling is data, not
        a differentiable op) -- matches tinygrad's semantics."""
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
