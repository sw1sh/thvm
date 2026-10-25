"""ctypes bindings to libthvm_py: `Thvm` builds and realizes terms, each a raw
u64 `Term` (thvm.h's tag / ext / val word)."""
from __future__ import annotations

import ctypes
import weakref
from ctypes import c_char_p, c_float, c_int32, c_uint32, c_uint64, c_void_p
from pathlib import Path
from typing import NewType


Term = NewType("Term", int)


_LIB_DIR = Path(__file__).parent
_DYLIB_PATH = next(
    (p for p in (_LIB_DIR / "libthvm_py.dylib", _LIB_DIR / "libthvm_py.so")
     if p.exists()),
    None,
)
if _DYLIB_PATH is None:
    raise ImportError(
        f"libthvm_py.{{dylib,so}} not found in {_LIB_DIR}. "
        "Run `make py` from repo root."
    )

_lib = ctypes.CDLL(str(_DYLIB_PATH))


def _bind(name: str, restype, *argtypes):
    f = getattr(_lib, name)
    f.restype = restype
    f.argtypes = list(argtypes)
    return f


_thvm_init = _bind("py_thvm_init", None)
_thvm_free = _bind("py_thvm_free", None)

_term_tag = _bind("py_term_tag", c_uint32, c_uint64)
_term_val = _bind("py_term_val", c_uint64, c_uint64)
_term_ext = _bind("py_term_ext", c_uint32, c_uint64)

_term_iconst = _bind("py_term_iconst", c_uint64, c_int32)
_term_fconst = _bind("py_term_fconst", c_uint64, c_float)

_uop_buffer = _bind("py_uop_buffer", c_uint64,
                    c_uint32, c_uint32, c_uint32,
                    ctypes.POINTER(c_uint32), c_uint32)
_uop_range = _bind("py_uop_range", c_uint64, c_uint32, c_uint32, c_uint32)
_uop_index_e = _bind("py_uop_index_e", c_uint64, c_uint64, c_uint64)
_uop_int_binary = _bind("py_uop_int_binary", c_uint64, c_uint32, c_uint64, c_uint64)
_uop_iwhere = _bind("py_uop_iwhere", c_uint64, c_uint64, c_uint64, c_uint64)
_uop_where = _bind("py_uop_where", c_uint64, c_uint64, c_uint64, c_uint64)
_uop_binary = _bind("py_uop_binary", c_uint64, c_uint32, c_uint64, c_uint64)
_uop_reduce = _bind("py_uop_reduce", c_uint64,
                    c_uint32, c_uint32, ctypes.POINTER(c_uint32), c_uint64)
_uop_store = _bind("py_uop_store", c_uint64, c_uint64, c_uint64, c_uint64)
_uop_detach = _bind("py_uop_detach", c_uint64, c_uint64)
_uop_gather = _bind("py_uop_gather", c_uint64, c_uint64, c_uint64, c_uint32)
_uop_scatter = _bind("py_uop_scatter", c_uint64, c_uint64, c_uint64, c_uint64,
                     c_uint32, c_uint32, c_uint32)

_ten_create = _bind("py_ten_create", c_uint64,
                    c_uint32, c_uint32, ctypes.POINTER(c_uint32))
_ten_write = _bind("py_ten_write", c_int32, c_uint64, c_void_p, c_uint64)
_ten_read = _bind("py_ten_read", c_int32, c_uint64, c_void_p, c_uint64)
_dtype_from_f32 = _bind("py_dtype_from_f32", None, c_void_p, c_uint32, c_void_p, c_uint32)
_dtype_to_f32 = _bind("py_dtype_to_f32", None, c_void_p, c_char_p, c_uint32, c_uint32)
_ten_dtype = _bind("py_ten_dtype", c_uint32, c_uint64)
_ten_numel = _bind("py_ten_numel", c_uint64, c_uint64)
_ten_shape = _bind("py_ten_shape", c_uint32, c_uint64,
                   ctypes.POINTER(c_uint32))
_uop_unary = _bind("py_uop_unary", c_uint64, c_uint32, c_uint64)
_uop_cast = _bind("py_uop_cast", c_uint64, c_uint64, c_uint32)
_uop_bitcast = _bind("py_uop_bitcast", c_uint64, c_uint64, c_uint32)
_uop_reshape = _bind("py_uop_reshape", c_uint64,
                     c_uint64, c_uint32, ctypes.POINTER(c_uint32))
_uop_permute = _bind("py_uop_permute", c_uint64,
                     c_uint64, c_uint32, ctypes.POINTER(c_uint32))
_uop_expand = _bind("py_uop_expand", c_uint64,
                    c_uint64, c_uint32, ctypes.POINTER(c_uint32))
_uop_pad = _bind("py_uop_pad", c_uint64,
                 c_uint64, c_uint32, ctypes.POINTER(c_uint32))
_uop_shrink = _bind("py_uop_shrink", c_uint64,
                    c_uint64, c_uint32, ctypes.POINTER(c_uint32))
_uop_flip = _bind("py_uop_flip", c_uint64, c_uint64, c_uint32)
_uop_broadcast2 = _bind("py_uop_broadcast2", c_int32, ctypes.POINTER(c_uint64),
                        c_uint32, ctypes.POINTER(c_uint32), c_uint32, ctypes.POINTER(c_uint32),
                        ctypes.POINTER(c_uint32))
_nn = _bind("py_nn", c_uint64, c_uint32, ctypes.POINTER(c_uint64), ctypes.POINTER(c_uint32),
            c_uint32, ctypes.POINTER(c_int32), c_uint32, ctypes.POINTER(ctypes.c_double), c_uint32)
_nn_op = _bind("py_nn_op", c_uint32, c_char_p)
_grad_with_target = _bind("py_grad_with_target", c_uint64,
                          c_uint64, c_uint64, c_uint64)
_realize = _bind("py_realize", c_uint64, c_uint64)
_realize_many = _bind("py_realize_many", c_uint64,
                      ctypes.POINTER(c_uint64), c_uint32)
_realize_error = _bind("py_realize_error", c_char_p)
_gc_pending = _bind("py_gc_pending", c_int32)
_gc_collect = _bind("py_gc_collect", None, ctypes.POINTER(c_uint64), c_uint32)
_pin_set = _bind("py_pin_set", None, c_uint64, c_uint64)
_pin_drop = _bind("py_pin_drop", None, c_uint64)
_reclaim = _bind("py_reclaim", None, ctypes.POINTER(c_uint64), c_uint32)
_reclaim_due = _bind("py_reclaim_due", c_int32, c_int32)
_buf_live_count = _bind("py_buf_live_count", c_uint32)
_jit_begin = _bind("py_jit_begin", c_uint32)
_jit_end = _bind("py_jit_end", None, ctypes.POINTER(c_uint64), c_uint32)
_jit_replay = _bind("py_jit_replay", c_uint32, c_uint32)
_jit_set_inputs = _bind("py_jit_set_inputs", None, c_uint32,
                        ctypes.POINTER(c_uint32), c_uint32)
_jit_replay_with_inputs = _bind("py_jit_replay_with_inputs", c_uint32,
                                c_uint32, ctypes.POINTER(c_uint32), c_uint32)
_jit_drop = _bind("py_jit_drop", None, c_uint32)
_tens_count = _bind("py_tens_count", c_uint32)
_kernel_count = _bind("py_kernel_count", c_uint32)
_cpu_peak_bytes = _bind("py_cpu_peak_bytes", c_uint64)
_cpu_live_bytes = _bind("py_cpu_live_bytes", c_uint64)
_cpu_peak_reset = _bind("py_cpu_peak_reset", None)
_cuda_jit_compiles = _bind("py_cuda_jit_compiles", c_uint64)
_cuda_jit_evictions = _bind("py_cuda_jit_evictions", c_uint64)
_cg_profile_dump = _bind("py_cg_profile_dump", None, c_uint32)
_ten_set_grad_leaf = _bind("py_ten_set_grad_leaf", c_int32,
                           c_uint64, c_int32)
_uop_leaf_tids = _bind("py_uop_leaf_tids", c_uint32,
                       c_uint64, ctypes.POINTER(c_uint32), c_uint32)
_grad_err_take     = _bind("py_grad_err_take",     c_char_p)
_uop_substitute = _bind("py_uop_substitute", c_uint64, c_uint64, ctypes.POINTER(c_uint64),
                        ctypes.POINTER(c_uint64), c_uint32)
_backward = _bind("py_backward", c_char_p, c_uint64, c_uint64, ctypes.POINTER(c_uint32),
                  c_uint32, ctypes.POINTER(c_uint64), ctypes.POINTER(ctypes.c_uint8))
_ten_get_buf_id   = _bind("py_ten_get_buf_id",   c_uint32, c_uint64)
_ten_clear_grad   = _bind("py_ten_clear_grad",   c_int32,  c_uint64)

_run_uop_kernel_c = _bind("py_run_uop_kernel_c", c_int32, c_uint64, c_void_p,
                          ctypes.POINTER(c_void_p), c_uint32, ctypes.POINTER(c_uint32))

_uop_dag_apply_kopt = _bind(
    "py_uop_dag_apply_kopt", c_uint64,
    c_uint64, ctypes.c_uint8, c_uint32, c_uint32)


def _read_uint32_const(name: str) -> int:
    fn = getattr(_lib, name, None)
    if fn is None:
        return -1
    fn.restype = c_uint32
    fn.argtypes = []
    return int(fn())


class _Constants:
    """thvm.h's constants, read from the library at import so Python cannot
    drift from C; a missing export reads as -1."""
    INT32 = _read_uint32_const("py_const_DT_INT32")
    FP32 = _read_uint32_const("py_const_DT_FP32")
    TAG_TEN = _read_uint32_const("py_const_TAG_TEN")
    TAG_UOP = _read_uint32_const("py_const_TAG_UOP")
    MAX_DIM = _read_uint32_const("py_const_MAX_DIM")
    SCOPE_GLOBAL = _read_uint32_const("py_const_UOP_SCOPE_GLOBAL")
    ADD = _read_uint32_const("py_const_UOP_ADD")
    MUL = _read_uint32_const("py_const_UOP_MUL")
    NEG = _read_uint32_const("py_const_UOP_NEG")
    ASSIGN = _read_uint32_const("py_const_UOP_ASSIGN")
    CMPLT = _read_uint32_const("py_const_UOP_CMPLT")
    CMPEQ = _read_uint32_const("py_const_UOP_CMPEQ")
    RECIP = _read_uint32_const("py_const_UOP_RECIP")
    EXP2 = _read_uint32_const("py_const_UOP_EXP2")
    SIN = _read_uint32_const("py_const_UOP_SIN")
    LOG2 = _read_uint32_const("py_const_UOP_LOG2")
    SQRT = _read_uint32_const("py_const_UOP_SQRT")
    IADD = _read_uint32_const("py_const_UOP_IADD")
    IMUL = _read_uint32_const("py_const_UOP_IMUL")
    IDIV = _read_uint32_const("py_const_UOP_IDIV")
    IMOD = _read_uint32_const("py_const_UOP_IMOD")
    IAND = _read_uint32_const("py_const_UOP_IAND")
    IOR = _read_uint32_const("py_const_UOP_IOR")
    IXOR = _read_uint32_const("py_const_UOP_IXOR")
    ISHL = _read_uint32_const("py_const_UOP_ISHL")
    THREEFRY = _read_uint32_const("py_const_UOP_THREEFRY")
    ISHR = _read_uint32_const("py_const_UOP_ISHR")
    REDUCE_SUM = _read_uint32_const("py_const_REDUCE_SUM")
    REDUCE_MAX = _read_uint32_const("py_const_REDUCE_MAX")
    REDUCE_PROD = _read_uint32_const("py_const_REDUCE_PROD")
    SCATTER_SUM = _read_uint32_const("py_const_SCATTER_SUM")
    SCATTER_PROD = _read_uint32_const("py_const_SCATTER_PROD")
    SCATTER_MAX = _read_uint32_const("py_const_SCATTER_MAX")
    SCATTER_MIN = _read_uint32_const("py_const_SCATTER_MIN")
    SCATTER_REPLACE = _read_uint32_const("py_const_SCATTER_REPLACE")
    AXIS_LOOP = _read_uint32_const("py_const_KAX_LOOP")
    AXIS_REDUCE = _read_uint32_const("py_const_KAX_REDUCE")
    AXIS_LOCAL = _read_uint32_const("py_const_KAX_LOCAL")
    AXIS_GLOBAL = _read_uint32_const("py_const_KAX_GLOBAL")
    KOP_UPCAST = _read_uint32_const("py_const_KOP_UPCAST")


K = _Constants


def _check_rank(ndim: int) -> None:
    if ndim > K.MAX_DIM:
        raise NotImplementedError(f"thvm tensors have at most {K.MAX_DIM} dims, got {ndim}")


class Thvm:
    """Handle to the process-global runtime: the first instance initializes it,
    close() frees it."""

    _initialized = False

    # Every live Tensor: a heap collection relocates each one's term to the
    # moved cell, so no lazy graph Python keeps points into the dead space.
    live_terms: "weakref.WeakSet" = weakref.WeakSet()

    def __init__(self):
        if not Thvm._initialized:
            _thvm_init()
            Thvm._initialized = True

    @staticmethod
    def close():
        if Thvm._initialized:
            _thvm_free()
            Thvm._initialized = False

    def iconst(self, value: int) -> Term:
        return Term(_term_iconst(c_int32(int(value))))

    def fconst(self, value: float) -> Term:
        return Term(_term_fconst(c_float(float(value))))

    def buffer(self, *, scope: int, dtype: int, dims, instance: int = 0) -> Term:
        ndim = len(dims)
        arr = (c_uint32 * ndim)(*[c_uint32(int(d)) for d in dims])
        return Term(_uop_buffer(c_uint32(scope), c_uint32(dtype),
                                c_uint32(ndim), arr, c_uint32(instance)))

    def range(self, axis_id: int, axis_type: int, extent: int) -> Term:
        return Term(_uop_range(c_uint32(axis_id), c_uint32(axis_type),
                               c_uint32(extent)))

    def index_e(self, buf: Term, addr: Term) -> Term:
        return Term(_uop_index_e(c_uint64(int(buf)), c_uint64(int(addr))))

    def iadd(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.IADD), c_uint64(int(a)), c_uint64(int(b))))

    def imul(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.IMUL), c_uint64(int(a)), c_uint64(int(b))))

    def iwhere(self, cond: Term, t: Term, e: Term) -> Term:
        return Term(_uop_iwhere(c_uint64(int(cond)),
                                c_uint64(int(t)),
                                c_uint64(int(e))))

    def where(self, cond: Term, t: Term, e: Term) -> Term:
        """Value-level select: IWHERE without the index-layer folds."""
        return Term(_uop_where(c_uint64(int(cond)),
                               c_uint64(int(t)),
                               c_uint64(int(e))))

    def binary(self, opcode: int, a: Term, b: Term) -> Term:
        return Term(_uop_binary(c_uint32(int(opcode)), c_uint64(int(a)), c_uint64(int(b))))

    def add(self, a: Term, b: Term) -> Term:   return self.binary(K.ADD, a, b)
    def mul(self, a: Term, b: Term) -> Term:   return self.binary(K.MUL, a, b)
    def cmplt(self, a: Term, b: Term) -> Term: return self.binary(K.CMPLT, a, b)
    def cmpeq(self, a: Term, b: Term) -> Term: return self.binary(K.CMPEQ, a, b)

    def neg(self, x: Term) -> Term:    return self.unary(K.NEG, x)
    def recip(self, x: Term) -> Term:  return self.unary(K.RECIP, x)
    def exp2(self, x: Term) -> Term:   return self.unary(K.EXP2, x)
    def log2(self, x: Term) -> Term:   return self.unary(K.LOG2, x)
    def sqrt(self, x: Term) -> Term:   return self.unary(K.SQRT, x)

    def reduce(self, kind: int, axes, src: Term) -> Term:
        arr = (c_uint32 * max(len(axes), 1))(*[c_uint32(int(a)) for a in axes])
        return Term(_uop_reduce(c_uint32(kind), c_uint32(len(axes)), arr, c_uint64(int(src))))

    def store(self, buf: Term, addr: Term, value: Term) -> Term:
        return Term(_uop_store(c_uint64(int(buf)),
                               c_uint64(int(addr)),
                               c_uint64(int(value))))

    def detach(self, src: Term) -> Term:
        return Term(_uop_detach(c_uint64(int(src))))

    def gather(self, src: Term, idx: Term, dim: int) -> Term:
        """UOP_GATHER, or Term(0) when the operands fail uop_gather's checks."""
        return Term(_uop_gather(c_uint64(int(src)), c_uint64(int(idx)),
                                c_uint32(int(dim))))

    def scatter(self, base: Term, idx: Term, src: Term, dim: int, kind: int,
                include_self: bool) -> Term:
        """UOP_SCATTER of a SCATTER_* kind, or Term(0) when the operands fail
        uop_scatter's checks."""
        return Term(_uop_scatter(c_uint64(int(base)), c_uint64(int(idx)),
                                 c_uint64(int(src)), c_uint32(int(dim)),
                                 c_uint32(int(kind)),
                                 c_uint32(1 if include_self else 0)))

    def ten_create(self, dtype: int, dims) -> Term:
        """A fresh TAG_TEN tensor on the DEV-selected backend."""
        ndim = len(dims)
        _check_rank(ndim)
        arr = (c_uint32 * max(ndim, 1))(*[c_uint32(int(d)) for d in dims])
        return Term(_ten_create(c_uint32(int(dtype)), c_uint32(ndim), arr))

    def ten_write(self, t: Term, data: bytes) -> bool:
        return bool(_ten_write(c_uint64(int(t)), data, c_uint64(len(data))))

    def dtype_from_f32(self, dst, dtype: int, src, n: int) -> None:
        """n f32 values of the numpy array src into dst in dtype's storage (bf16, fp8)."""
        _dtype_from_f32(dst, c_uint32(dtype), src.ctypes.data, c_uint32(n))

    def dtype_to_f32(self, dst, src: bytes, dtype: int, n: int) -> None:
        """n values of dtype's storage (bytes) into the f32 numpy array dst."""
        _dtype_to_f32(dst.ctypes.data, src, c_uint32(dtype), c_uint32(n))

    def ten_read(self, t: Term, nbytes: int) -> bytes:
        buf = ctypes.create_string_buffer(nbytes)
        if not _ten_read(c_uint64(int(t)), buf, c_uint64(nbytes)):
            raise RuntimeError(f"ten_read failed for term {int(t):#x}")
        return buf.raw

    def ten_dtype(self, t: Term) -> int:
        return int(_ten_dtype(c_uint64(int(t))))

    def ten_numel(self, t: Term) -> int:
        return int(_ten_numel(c_uint64(int(t))))

    def ten_shape(self, t: Term) -> tuple[int, ...]:
        out = (c_uint32 * K.MAX_DIM)()
        ndim = int(_ten_shape(c_uint64(int(t)), out))
        return tuple(int(out[i]) for i in range(ndim))

    def unary(self, opcode: int, x: Term) -> Term:
        return Term(_uop_unary(c_uint32(int(opcode)), c_uint64(int(x))))

    def cast(self, x: Term, dst_dtype: int) -> Term:
        return Term(_uop_cast(c_uint64(int(x)), c_uint32(int(dst_dtype))))

    def bitcast(self, x: Term, dst_dtype: int) -> Term:
        """Reinterprets the bits; another itemsize returns `x` unchanged."""
        return Term(_uop_bitcast(c_uint64(int(x)), c_uint32(int(dst_dtype))))

    def _move(self, fn, src: Term, dims) -> Term:
        _check_rank(len(dims))
        arr = (c_uint32 * max(len(dims), 1))(*[c_uint32(int(d)) for d in dims])
        return Term(fn(c_uint64(int(src)), c_uint32(len(dims)), arr))

    def reshape(self, src: Term, dims) -> Term:
        return self._move(_uop_reshape, src, dims)

    def broadcast2(self, a: Term, sa, b: Term, sb):
        """(a', b', shape): a (shape sa) and b (shape sb) broadcast to one shape
        (uop_broadcast2), or None when an axis is neither equal nor 1."""
        ab = (c_uint64 * 2)(int(a), int(b))
        arr_a = (c_uint32 * max(len(sa), 1))(*[int(d) for d in sa])
        arr_b = (c_uint32 * max(len(sb), 1))(*[int(d) for d in sb])
        out = (c_uint32 * K.MAX_DIM)()
        nd = int(_uop_broadcast2(ab, c_uint32(len(sa)), arr_a, c_uint32(len(sb)), arr_b, out))
        if nd < 0:
            return None
        return Term(ab[0]), Term(ab[1]), tuple(int(d) for d in out[:nd])

    _NN_OPS: dict = {}

    def nn(self, op: str, args, ints=(), reals=()):
        """nn_call (src/nn/call.c): the op named `op` over (term, dtype id, shape) triples (None:
        no tensor) with its integer and real arguments, as (term, dtype id, shape), or None when
        the op fails."""
        code = Thvm._NN_OPS.get(op)
        if code is None:
            code = Thvm._NN_OPS[op] = int(_nn_op(op.encode()))
        n = 2 + K.MAX_DIM
        desc = (c_uint32 * (n * max(len(args), 1)))()
        terms = (c_uint64 * max(len(args), 1))()
        for i, a in enumerate(args):
            if a is not None:
                terms[i] = int(a[0])
                desc[i * n], desc[i * n + 1] = a[1], len(a[2])
                for j, d in enumerate(a[2]):
                    desc[i * n + 2 + j] = int(d)
        ia = (c_int32 * max(len(ints), 1))(*[int(v) for v in ints])
        ra = (ctypes.c_double * max(len(reals), 1))(*[float(v) for v in reals])
        t = _nn(c_uint32(code), terms, desc, c_uint32(len(args)), ia, c_uint32(len(ints)), ra,
                c_uint32(len(reals)))
        if t == 0:
            return None
        return Term(t), int(desc[0]), tuple(int(d) for d in desc[2:2 + desc[1]])

    def permute(self, src: Term, perm) -> Term:
        return self._move(_uop_permute, src, perm)

    def expand(self, src: Term, dims) -> Term:
        return self._move(_uop_expand, src, dims)

    def pad(self, src: Term, begin_end) -> Term:
        """begin_end has 2*ndim entries: (begin, end) pad per axis."""
        _check_rank(len(begin_end) // 2)
        arr = (c_uint32 * len(begin_end))(*[c_uint32(int(d)) for d in begin_end])
        return Term(_uop_pad(c_uint64(int(src)),
                             c_uint32(len(begin_end) // 2), arr))

    def shrink(self, src: Term, begin_end) -> Term:
        _check_rank(len(begin_end) // 2)
        arr = (c_uint32 * len(begin_end))(*[c_uint32(int(d)) for d in begin_end])
        return Term(_uop_shrink(c_uint64(int(src)),
                                c_uint32(len(begin_end) // 2), arr))

    def flip(self, src: Term, axes_bitmask: int) -> Term:
        return Term(_uop_flip(c_uint64(int(src)), c_uint32(int(axes_bitmask))))

    def grad_with_target(self, y: Term, gy: Term, target: Term) -> Term:
        return Term(_grad_with_target(c_uint64(int(y)), c_uint64(int(gy)),
                                      c_uint64(int(target))))

    def realize(self, t: Term) -> Term:
        r = _realize(c_uint64(int(t)))
        if r == 0 and _realize_error():
            raise RuntimeError(_realize_error().decode())
        return self._collect([Term(r)])[0]

    def _collect(self, held: "list[Term]") -> "list[Term]":
        """Run a pending heap collection with every live Tensor's term and
        `held` as roots; returns `held` relocated."""
        if not _gc_pending():
            return held
        live = list(Thvm.live_terms)
        n = len(live) + len(held)
        arr = (c_uint64 * n)(*[int(x.term) for x in live], *[int(h) for h in held])
        _gc_collect(arr, c_uint32(n))
        for i, x in enumerate(live):
            x.term = Term(arr[i])
        return [Term(arr[len(live) + j]) for j in range(len(held))]

    def realize_many(self, terms: "list[Term]") -> "list[Term]":
        """Realizes `terms` in one pass (tinygrad Tensor.realize(*lst)); RuntimeError
        when that fails, as on an assign cycle (tinygrad schedule/__init__.py:82)."""
        n = len(terms)
        if n == 0:
            return []
        arr = (c_uint64 * n)(*[int(t) for t in terms])
        if _realize_many(arr, c_uint32(n)) == 0 and _realize_error():
            raise RuntimeError(_realize_error().decode())
        return self._collect([Term(arr[i]) for i in range(n)])

    def pin_set(self, handle: int, t: Term) -> None:
        _pin_set(c_uint64(handle), c_uint64(int(t)))

    def pin_drop(self, handle: int) -> None:
        _pin_drop(c_uint64(handle))

    def reclaim(self) -> None:
        """Frees each CPU or CUDA buffer no live Tensor, pin or JIT capture reaches
        (py_reclaim): safe only where live Tensors hold every term still needed."""
        live = list(Thvm.live_terms)
        arr = (c_uint64 * max(len(live), 1))(*[int(x.term) for x in live])
        _reclaim(arr, c_uint32(len(live)))

    def reclaim_if_due(self, force: bool = False) -> None:
        """reclaim() once resident bytes outgrew the last reclaim's survivors
        (py_reclaim_due); `force` reclaims after any growth."""
        if _reclaim_due(c_int32(1 if force else 0)):
            self.reclaim()

    def buf_live_count(self) -> int:
        return int(_buf_live_count())

    def jit_begin(self) -> int:       return int(_jit_begin())
    def jit_end(self, roots=()) -> None:
        n = len(roots)
        _jit_end((c_uint64 * n)(*[int(r) for r in roots]) if n else None, c_uint32(n))
    def jit_replay(self, s: int) -> int: return int(_jit_replay(c_uint32(s)))
    def jit_set_inputs(self, s: int, ids: list[int]) -> None:
        n = len(ids)
        arr = (c_uint32 * n)(*[c_uint32(int(i)) for i in ids]) if n else None
        _jit_set_inputs(c_uint32(s), arr, c_uint32(n))
    def jit_replay_with_inputs(self, s: int, ids: list[int]) -> int:
        n = len(ids)
        arr = (c_uint32 * n)(*[c_uint32(int(i)) for i in ids]) if n else None
        return int(_jit_replay_with_inputs(c_uint32(s), arr, c_uint32(n)))
    def jit_drop(self, s: int) -> None: _jit_drop(c_uint32(s))

    def tens_count(self) -> int:
        return int(_tens_count())

    def kernel_count(self) -> int:
        return int(_kernel_count())

    def cpu_peak_bytes(self) -> int:
        return int(_cpu_peak_bytes())

    def cpu_live_bytes(self) -> int:
        return int(_cpu_live_bytes())

    def cpu_peak_reset(self) -> None:
        _cpu_peak_reset()

    def cuda_jit_compiles(self) -> int:
        return int(_cuda_jit_compiles())

    def cuda_jit_evictions(self) -> int:
        return int(_cuda_jit_evictions())

    def cg_profile_dump(self, top_n: int = 20) -> None:
        _cg_profile_dump(c_uint32(top_n))

    def ten_set_grad_leaf(self, t: Term, on: bool) -> bool:
        """Marks or unmarks leaf `t` for the single walk's .grad accumulation;
        backward() marks its leaves through uop_backward instead."""
        return bool(_ten_set_grad_leaf(c_uint64(int(t)),
                                       c_int32(1 if on else 0)))

    def uop_leaf_tids(self, root: Term) -> "list[int]":
        """The distinct TAG_TEN tids reachable from `root`, backward()'s in-scope
        leaves (tinygrad tensor.py:487-488) before its live-float filter."""
        cap = max(1, int(_tens_count()))   # every leaf is a distinct tid
        buf = (c_uint32 * cap)()
        n = int(_uop_leaf_tids(c_uint64(int(root)), buf, c_uint32(cap)))
        return [int(buf[i]) for i in range(n)]

    def uop_substitute(self, root: Term, frm, to) -> Term:
        """root with each frm[i] replaced by to[i]."""
        n = len(frm)
        a, b = (c_uint64 * max(1, n))(*map(int, frm)), (c_uint64 * max(1, n))(*map(int, to))
        return Term(int(_uop_substitute(c_uint64(int(root)), a, b, c_uint32(n))))

    def ten_get_buf_id(self, t: Term) -> int:
        return int(_ten_get_buf_id(c_uint64(int(t))))

    def backward(self, y: Term, gy: Term, tids) -> "tuple[list[int], list[bool], str | None]":
        """uop_backward: each leaf's grad term, whether it held one before, and the
        chain rule's error (every grad rolled back) or None."""
        n = len(tids)
        arr = (c_uint32 * max(n, 1))(*tids)
        grads = (c_uint64 * max(n, 1))()
        had = (ctypes.c_uint8 * max(n, 1))()
        err = _backward(c_uint64(int(y)), c_uint64(int(gy)), arr, c_uint32(n), grads, had)
        return [int(g) for g in grads[:n]], [bool(h) for h in had[:n]], (
            None if err is None else err.decode())

    def grad_err_take(self) -> "str | None":
        """The chain rule's pending error (a node it has no gradient rule
        for), cleared by the call; None when there is none."""
        msg = _grad_err_take()
        return None if msg is None else msg.decode()

    def ten_clear_grad(self, t: Term) -> bool:
        return bool(_ten_clear_grad(c_uint64(int(t))))

    def term_tag(self, t: Term) -> int: return int(_term_tag(c_uint64(int(t))))
    def term_val(self, t: Term) -> int: return int(_term_val(c_uint64(int(t))))
    def term_ext(self, t: Term) -> int: return int(_term_ext(c_uint64(int(t))))

    def uop_dag_apply_kopt(self, root: Term,
                           opt: tuple[int, int, int]) -> Term | None:
        """`root` with the KOpt (op, axis, arg) applied, outside any KernelEntry;
        None when the opt bails."""
        op, axis, arg = opt
        new_root = _uop_dag_apply_kopt(int(root), int(op), int(axis), int(arg))
        if new_root == 0:
            return None
        return Term(int(new_root))

    def run_kernel_c(self, root: Term, out, ins) -> int:
        """Runs `root` once on contiguous numpy arrays (cpu_jit_run_root): 0, or -1 when
        the renderer declines, -2 when the compile or load fails."""
        n_in = max(1, len(ins))
        ptrs = (c_void_p * n_in)(*[a.ctypes.data for a in ins])
        numels = (c_uint32 * n_in)(*[a.size for a in ins])
        return int(_run_uop_kernel_c(c_uint64(int(root)), c_void_p(out.ctypes.data),
                                     ptrs, c_uint32(out.size), numels))
