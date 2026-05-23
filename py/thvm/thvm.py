"""ctypes loader + thin Python API for thvm UOp construction.

A `Term` is just a u64 handle (thvm's tagged-pointer scheme); we wrap it
in a NewType for type-checker friendliness but operate on the raw u64
values throughout.

Lifecycle: instantiate `Thvm()` once per process. It calls thvm_init()
on construction and thvm_free() on garbage collection.
"""
from __future__ import annotations

import ctypes
import os
from ctypes import c_char_p, c_float, c_int32, c_uint32, c_uint64, c_void_p
from pathlib import Path
from typing import NewType


Term = NewType("Term", int)


# The shared library is `.dylib` on macOS (Metal bridge) and `.so` on
# Linux (CUDA bridge); `make py` builds whichever the platform supports.
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

# Whether the loaded library carries the CUDA bridge (`py_cuda_*`).
# True on the Linux+CUDA build, False on the macOS+Metal build.  The
# `Cuda` class binds its entry points lazily so importing this module
# never fails on a host without one backend or the other.
_HAS_CUDA = hasattr(_lib, "py_cuda_init")
_HAS_METAL = hasattr(_lib, "py_metal_init")


def _bind(name: str, restype, *argtypes):
    f = getattr(_lib, name)
    f.restype = restype
    f.argtypes = list(argtypes)
    return f


# ---------------- runtime lifecycle ----------------
_thvm_init = _bind("py_thvm_init", None)
_thvm_free = _bind("py_thvm_free", None)

# ---------------- term inspection ----------------
_term_tag = _bind("py_term_tag", c_uint32, c_uint64)
_term_ext = _bind("py_term_ext", c_uint32, c_uint64)
_term_val = _bind("py_term_val", c_uint64, c_uint64)

# ---------------- atom constructors ----------------
_term_iconst = _bind("py_term_iconst", c_uint64, c_int32)
_term_fconst = _bind("py_term_fconst", c_uint64, c_float)

# ---------------- UOp graph constructors ----------------
_uop_buffer = _bind("py_uop_buffer", c_uint64,
                    c_uint32, c_uint32, c_uint32,
                    ctypes.POINTER(c_uint32), c_uint32)
_uop_range = _bind("py_uop_range", c_uint64, c_uint32, c_uint32, c_uint32)
_uop_index_e = _bind("py_uop_index_e", c_uint64, c_uint64, c_uint64)
_uop_int_binary = _bind("py_uop_int_binary", c_uint64, c_uint32, c_uint64, c_uint64)
_uop_iwhere = _bind("py_uop_iwhere", c_uint64, c_uint64, c_uint64, c_uint64)
_uop_invalid = _bind("py_uop_invalid", c_uint64)
_uop_binary = _bind("py_uop_binary", c_uint64, c_uint32, c_uint64, c_uint64)
_uop_reduce = _bind("py_uop_reduce", c_uint64, c_uint32, c_uint32, c_uint64)
_uop_opt = _bind("py_uop_opt", c_uint64, c_uint64, c_uint32, c_uint32)
_uop_store = _bind("py_uop_store", c_uint64, c_uint64, c_uint64, c_uint64)
_uop_after = _bind("py_uop_after", c_uint64, c_uint64, c_uint64)
_uop_load = _bind("py_uop_load", c_uint64, c_uint64)
_uop_detach = _bind("py_uop_detach", c_uint64, c_uint64)

# ---------------- high-level tensor surface (TAG_TEN) ----------------
_ten_create = _bind("py_ten_create", c_uint64,
                    c_uint32, c_uint32, ctypes.POINTER(c_uint32))
_ten_write = _bind("py_ten_write", c_int32, c_uint64, c_void_p, c_uint64)
_ten_read = _bind("py_ten_read", c_int32, c_uint64, c_void_p, c_uint64)
_ten_dtype = _bind("py_ten_dtype", c_uint32, c_uint64)
_ten_numel = _bind("py_ten_numel", c_uint64, c_uint64)
_ten_shape = _bind("py_ten_shape", c_uint32, c_uint64,
                   ctypes.POINTER(c_uint32))
_uop_unary = _bind("py_uop_unary", c_uint64, c_uint32, c_uint64)
_uop_cast = _bind("py_uop_cast", c_uint64, c_uint64, c_uint32)
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
_grad = _bind("py_grad", c_uint64, c_uint64, c_uint64)
_grad_with_target = _bind("py_grad_with_target", c_uint64,
                          c_uint64, c_uint64, c_uint64)
_fwd = _bind("py_fwd", c_uint64, c_uint64, c_uint64)
_wnf = _bind("py_wnf", c_uint64, c_uint64)
_nf = _bind("py_nf", c_uint64, c_uint64)
_realize = _bind("py_realize", c_uint64, c_uint64)
_realize_many = _bind("py_realize_many", c_uint64,
                      ctypes.POINTER(c_uint64), c_uint32)
_pin_set = _bind("py_pin_set", None, c_uint64, c_uint64)
_pin_drop = _bind("py_pin_drop", None, c_uint64)
_reclaim = _bind("py_reclaim", None)
_jit_begin = _bind("py_jit_begin", c_uint32)
_jit_end = _bind("py_jit_end", None)
_jit_replay = _bind("py_jit_replay", c_uint32, c_uint32)
_jit_op_count = _bind("py_jit_op_count", c_uint32, c_uint32)
_jit_drop = _bind("py_jit_drop", None, c_uint32)
_tens_count = _bind("py_tens_count", c_uint32)
_kernel_count = _bind("py_kernel_count", c_uint32)
_cpu_peak_bytes = _bind("py_cpu_peak_bytes", c_uint64)
_cpu_live_bytes = _bind("py_cpu_live_bytes", c_uint64)
_cpu_peak_reset = _bind("py_cpu_peak_reset", None)
_cuda_jit_compiles = _bind("py_cuda_jit_compiles", c_uint64)
_cuda_jit_evictions = _bind("py_cuda_jit_evictions", c_uint64)
_ten_set_requires_grad = _bind("py_ten_set_requires_grad", c_int32,
                               c_uint64, c_int32)
_ten_get_requires_grad = _bind("py_ten_get_requires_grad", c_int32, c_uint64)
_grad_memo_hits   = _bind("py_grad_memo_hits",   c_uint64)
_grad_memo_misses = _bind("py_grad_memo_misses", c_uint64)
_grad_fires       = _bind("py_grad_fires",       c_uint64)
_ten_get_grad     = _bind("py_ten_get_grad",     c_uint64, c_uint64)
_ten_clear_grad   = _bind("py_ten_clear_grad",   c_int32,  c_uint64)

# ---------------- buffer accessors ----------------
_uop_buffer_scope = _bind("py_uop_buffer_scope", c_uint32, c_uint64)
_uop_buffer_dtype = _bind("py_uop_buffer_dtype", c_uint32, c_uint64)
_uop_buffer_ndim = _bind("py_uop_buffer_ndim", c_uint32, c_uint64)
_uop_buffer_dim = _bind("py_uop_buffer_dim", c_uint32, c_uint64, c_uint32)

# ---------------- renderer ----------------
_render_uop_kernel = _bind("py_render_uop_kernel", c_void_p, c_uint64, c_char_p)
_render_uop_kernel_cuda = _bind(
    "py_render_uop_kernel_cuda", c_void_p, c_uint64, c_char_p)
_string_free = _bind("py_string_free", None, c_void_p)

# ---------------- BEAM / autotune surface ----------------
_kernel_alloc = _bind("py_kernel_alloc", c_uint32)
_kernel_set_cached_lift = _bind(
    "py_kernel_set_cached_lift", None,
    c_uint32, c_uint64, c_uint64,
    ctypes.POINTER(c_uint64), c_uint32)
_kernel_opts_propose = _bind(
    "py_kernel_opts_propose", c_uint32,
    c_uint32,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.POINTER(c_uint32),
    c_uint32)
_kernel_apply_opt = _bind(
    "py_kernel_apply_opt", c_uint64,
    c_uint32, ctypes.c_uint8, ctypes.c_uint8, c_uint32)
_uop_dag_apply_kopt = _bind(
    "py_uop_dag_apply_kopt", c_uint64,
    c_uint64, ctypes.c_uint8, ctypes.c_uint8, c_uint32)
_cuda_dag_dispatch_shape = _bind(
    "py_cuda_dag_dispatch_shape", c_uint32,
    c_uint64, ctypes.POINTER(c_uint32), ctypes.POINTER(c_uint32))


# ---------------- in-process Metal compile + dispatch ----------------
# Bound only on the macOS+Metal build; on the Linux+CUDA build the
# `py_metal_*` symbols are absent and these stay None (the `Metal`
# class raises a clear error if instantiated there).
if _HAS_METAL:
    _metal_init = _bind("py_metal_init", c_int32)
    _metal_shutdown = _bind("py_metal_shutdown", None)

    _metal_compile_msl = _bind(
        "py_metal_compile_msl", c_uint32,
        c_char_p, c_char_p, c_char_p, c_uint32)
    _metal_pso_max_threads = _bind("py_metal_pso_max_threads", c_uint32, c_uint32)
    _metal_pso_release = _bind("py_metal_pso_release", None, c_uint32)

    _metal_buf_alloc = _bind("py_metal_buf_alloc", c_uint32, c_uint64)
    _metal_buf_write = _bind("py_metal_buf_write", c_int32,
                             c_uint32, c_void_p, c_uint64)
    _metal_buf_read = _bind("py_metal_buf_read", c_int32,
                            c_uint32, c_void_p, c_uint64)
    _metal_buf_release = _bind("py_metal_buf_release", None, c_uint32)

    _metal_dispatch = _bind(
        "py_metal_dispatch", c_uint64,
        c_uint32, ctypes.POINTER(c_uint32), c_uint32,
        c_uint32, c_uint32, c_uint32,
        c_uint32, c_uint32, c_uint32,
        c_char_p, c_uint32)

    _metal_dispatch_timed = _bind(
        "py_metal_dispatch_timed", c_int32,
        c_uint32, ctypes.POINTER(c_uint32), c_uint32,
        c_uint32, c_uint32, c_uint32,
        c_uint32, c_uint32, c_uint32,
        ctypes.POINTER(c_uint64), ctypes.POINTER(c_uint64),
        c_char_p, c_uint32)


# ---------------- in-process CUDA compile + dispatch ----------------
# Bound only on the Linux+CUDA build; on the macOS+Metal build the
# `py_cuda_*` symbols are absent and these stay None.
if _HAS_CUDA:
    _cuda_init = _bind("py_cuda_init", c_int32)
    _cuda_shutdown = _bind("py_cuda_shutdown", None)
    _cuda_device_sm = _bind("py_cuda_device_sm", c_uint32)
    _cuda_last_error = _bind("py_cuda_last_error", c_char_p)

    _cuda_compile = _bind(
        "py_cuda_compile", c_uint32,
        c_char_p, c_char_p, c_char_p, c_uint32)
    _cuda_fn_release = _bind("py_cuda_fn_release", None, c_uint32)

    _cuda_buf_alloc = _bind("py_cuda_buf_alloc", c_uint32, c_uint64)
    _cuda_buf_write = _bind("py_cuda_buf_write", c_int32,
                            c_uint32, c_void_p, c_uint64)
    _cuda_buf_read = _bind("py_cuda_buf_read", c_int32,
                           c_uint32, c_void_p, c_uint64)
    _cuda_buf_release = _bind("py_cuda_buf_release", None, c_uint32)

    _cuda_dispatch = _bind(
        "py_cuda_dispatch", c_uint64,
        c_uint32, ctypes.POINTER(c_uint32), c_uint32,
        c_uint32, c_uint32,
        c_char_p, c_uint32)

    _cuda_dispatch_timed = _bind(
        "py_cuda_dispatch_timed", c_int32,
        c_uint32, ctypes.POINTER(c_uint32), c_uint32,
        c_uint32, c_uint32,
        ctypes.POINTER(c_uint64), ctypes.POINTER(c_uint64),
        c_char_p, c_uint32)


# ---------------- enum binder ----------------
def _read_uint32_const(name: str) -> int:
    fn = getattr(_lib, name, None)
    if fn is None:
        return -1
    fn.restype = c_uint32
    fn.argtypes = []
    return int(fn())


class _Constants:
    """Symbolic constants exposed by the wrapper.

    The actual values match thvm.h and are pulled from the dylib at
    import time so the Python layer can never drift from the C layer.
    """
    # dtypes
    INT32 = _read_uint32_const("py_const_DT_INT32")
    FP32 = _read_uint32_const("py_const_DT_FP32")
    # tensor term tag + max rank
    TAG_TEN = _read_uint32_const("py_const_TAG_TEN")
    TAG_UOP = _read_uint32_const("py_const_TAG_UOP")
    UOP_SHRINK = _read_uint32_const("py_const_UOP_SHRINK")
    MAX_DIM = _read_uint32_const("py_const_MAX_DIM")
    # buffer scopes
    SCOPE_GLOBAL = _read_uint32_const("py_const_UOP_SCOPE_GLOBAL")
    SCOPE_LOCAL = _read_uint32_const("py_const_UOP_SCOPE_LOCAL")
    SCOPE_REG = _read_uint32_const("py_const_UOP_SCOPE_REG")
    # FP binary opcodes
    ADD = _read_uint32_const("py_const_UOP_ADD")
    MUL = _read_uint32_const("py_const_UOP_MUL")
    NEG = _read_uint32_const("py_const_UOP_NEG")
    ASSIGN = _read_uint32_const("py_const_UOP_ASSIGN")
    CMPLT = _read_uint32_const("py_const_UOP_CMPLT")
    CMPEQ = _read_uint32_const("py_const_UOP_CMPEQ")
    RECIP = _read_uint32_const("py_const_UOP_RECIP")
    EXP2 = _read_uint32_const("py_const_UOP_EXP2")
    LOG2 = _read_uint32_const("py_const_UOP_LOG2")
    SQRT = _read_uint32_const("py_const_UOP_SQRT")
    # Integer binary opcodes
    IADD = _read_uint32_const("py_const_UOP_IADD")
    ISUB = _read_uint32_const("py_const_UOP_ISUB")
    IMUL = _read_uint32_const("py_const_UOP_IMUL")
    IDIV = _read_uint32_const("py_const_UOP_IDIV")
    IMOD = _read_uint32_const("py_const_UOP_IMOD")
    ILT = _read_uint32_const("py_const_UOP_ILT")
    IAND = _read_uint32_const("py_const_UOP_IAND")
    IOR = _read_uint32_const("py_const_UOP_IOR")
    IXOR = _read_uint32_const("py_const_UOP_IXOR")
    # Reduce kinds
    REDUCE_SUM = _read_uint32_const("py_const_REDUCE_SUM")
    REDUCE_MAX = _read_uint32_const("py_const_REDUCE_MAX")
    # Axis types (KAX_*)
    AXIS_LOOP = _read_uint32_const("py_const_KAX_LOOP")
    AXIS_REDUCE = _read_uint32_const("py_const_KAX_REDUCE")
    AXIS_UPCAST = _read_uint32_const("py_const_KAX_UPCAST")
    AXIS_UNROLL = _read_uint32_const("py_const_KAX_UNROLL")
    AXIS_LOCAL = _read_uint32_const("py_const_KAX_LOCAL")
    AXIS_GLOBAL = _read_uint32_const("py_const_KAX_GLOBAL")
    AXIS_GROUP_REDUCE = _read_uint32_const("py_const_KAX_GROUP_REDUCE")
    # Opt kinds (UOP_OPT_*)
    OPT_UNROLL = _read_uint32_const("py_const_UOP_OPT_UNROLL")
    OPT_UPCAST = _read_uint32_const("py_const_UOP_OPT_UPCAST")
    OPT_TC = _read_uint32_const("py_const_UOP_OPT_TC")
    OPT_LOCAL = _read_uint32_const("py_const_UOP_OPT_LOCAL")
    OPT_GROUP_REDUCE = _read_uint32_const("py_const_UOP_OPT_GROUP_REDUCE")
    OPT_CONV = _read_uint32_const("py_const_UOP_OPT_CONV")
    OPT_FAST_MATH = _read_uint32_const("py_const_UOP_OPT_FAST_MATH")
    OPT_SIMD_REDUCE = _read_uint32_const("py_const_UOP_OPT_SIMD_REDUCE")
    OPT_VEC_LOAD = _read_uint32_const("py_const_UOP_OPT_VEC_LOAD")
    # KOpt opcodes (autotune-side; autotune.c BEAM uses these)
    KOP_NONE = _read_uint32_const("py_const_KOP_NONE")
    KOP_UPCAST = _read_uint32_const("py_const_KOP_UPCAST")
    KOP_UNROLL = _read_uint32_const("py_const_KOP_UNROLL")
    KOP_LOCAL = _read_uint32_const("py_const_KOP_LOCAL")
    KOP_GROUP = _read_uint32_const("py_const_KOP_GROUP")
    KOP_GROUPTOP = _read_uint32_const("py_const_KOP_GROUPTOP")
    KOP_SWAP = _read_uint32_const("py_const_KOP_SWAP")
    KOP_PADTO = _read_uint32_const("py_const_KOP_PADTO")
    KOP_NOLOCALS = _read_uint32_const("py_const_KOP_NOLOCALS")
    KOP_TC = _read_uint32_const("py_const_KOP_TC")
    KOP_GLOBAL = _read_uint32_const("py_const_KOP_GLOBAL")
    KOP_FAST_MATH = _read_uint32_const("py_const_KOP_FAST_MATH")
    KOP_SIMD_REDUCE = _read_uint32_const("py_const_KOP_SIMD_REDUCE")
    KOP_VEC_LOAD = _read_uint32_const("py_const_KOP_VEC_LOAD")


K = _Constants


DEFAULT_HEADER = """\
#include <metal_stdlib>
using namespace metal;
"""


class Thvm:
    """Singleton handle to the thvm runtime.

    Holds no state of its own; thvm's runtime is process-global. Multiple
    instances share the same runtime; call `close()` once at the end.
    """

    _initialized = False

    def __init__(self):
        if not Thvm._initialized:
            _thvm_init()
            Thvm._initialized = True

    @staticmethod
    def close():
        if Thvm._initialized:
            _thvm_free()
            Thvm._initialized = False

    # ---------------- atoms ----------------
    def iconst(self, value: int) -> Term:
        return Term(_term_iconst(c_int32(int(value))))

    def fconst(self, value: float) -> Term:
        return Term(_term_fconst(c_float(float(value))))

    # ---------------- buffer ----------------
    def buffer(self, *, scope: int, dtype: int, dims, instance: int = 0) -> Term:
        ndim = len(dims)
        arr = (c_uint32 * ndim)(*[c_uint32(int(d)) for d in dims])
        return Term(_uop_buffer(c_uint32(scope), c_uint32(dtype),
                                c_uint32(ndim), arr, c_uint32(instance)))

    # ---------------- range ----------------
    def range(self, axis_id: int, axis_type: int, extent: int) -> Term:
        return Term(_uop_range(c_uint32(axis_id), c_uint32(axis_type),
                               c_uint32(extent)))

    # ---------------- INDEX layer ----------------
    def index_e(self, buf: Term, addr: Term) -> Term:
        return Term(_uop_index_e(c_uint64(int(buf)), c_uint64(int(addr))))

    def iadd(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.IADD), c_uint64(int(a)), c_uint64(int(b))))

    def isub(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.ISUB), c_uint64(int(a)), c_uint64(int(b))))

    def imul(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.IMUL), c_uint64(int(a)), c_uint64(int(b))))

    def idiv(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.IDIV), c_uint64(int(a)), c_uint64(int(b))))

    def imod(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.IMOD), c_uint64(int(a)), c_uint64(int(b))))

    def ilt(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.ILT), c_uint64(int(a)), c_uint64(int(b))))

    def iand(self, a: Term, b: Term) -> Term:
        return Term(_uop_int_binary(c_uint32(K.IAND), c_uint64(int(a)), c_uint64(int(b))))

    def iwhere(self, cond: Term, t: Term, e: Term) -> Term:
        return Term(_uop_iwhere(c_uint64(int(cond)),
                                c_uint64(int(t)),
                                c_uint64(int(e))))

    def invalid(self) -> Term:
        return Term(_uop_invalid())

    # ---------------- FP arithmetic ----------------
    def add(self, a: Term, b: Term) -> Term:
        return Term(_uop_binary(c_uint32(K.ADD), c_uint64(int(a)), c_uint64(int(b))))

    def mul(self, a: Term, b: Term) -> Term:
        return Term(_uop_binary(c_uint32(K.MUL), c_uint64(int(a)), c_uint64(int(b))))

    def cmplt(self, a: Term, b: Term) -> Term:
        return Term(_uop_binary(c_uint32(K.CMPLT), c_uint64(int(a)), c_uint64(int(b))))

    def cmpeq(self, a: Term, b: Term) -> Term:
        return Term(_uop_binary(c_uint32(K.CMPEQ), c_uint64(int(a)), c_uint64(int(b))))

    # ---------------- FP unary ----------------
    # Builds UOP_NEG / RECIP / EXP2 / LOG2 / SQRT nodes.
    def _unary(self, opcode: int, x: Term) -> Term:
        return Term(_uop_unary(c_uint32(opcode), c_uint64(int(x))))

    def neg(self, x: Term) -> Term:    return self._unary(K.NEG, x)
    def recip(self, x: Term) -> Term:  return self._unary(K.RECIP, x)
    def exp2(self, x: Term) -> Term:   return self._unary(K.EXP2, x)
    def log2(self, x: Term) -> Term:   return self._unary(K.LOG2, x)
    def sqrt(self, x: Term) -> Term:   return self._unary(K.SQRT, x)

    # Synthetic exp(x) = exp2(x * log2(e)) since UOP_EXP isn't in the
    # opcode set.  log2(e) is hash-consed via uop_const(FP32, ...) so
    # repeated calls share heap loc.
    def exp(self, x: Term) -> Term:
        log2_e = self.fconst(1.4426950408889634)
        return self.exp2(self.mul(x, log2_e))

    # ---------------- reduce / opt / store / after / load ----------------
    def reduce(self, kind: int, axis: int, src: Term) -> Term:
        return Term(_uop_reduce(c_uint32(kind), c_uint32(axis), c_uint64(int(src))))

    def opt(self, target: Term, kind: int, factor: int = 0) -> Term:
        return Term(_uop_opt(c_uint64(int(target)), c_uint32(kind), c_uint32(factor)))

    def store(self, buf: Term, addr: Term, value: Term) -> Term:
        return Term(_uop_store(c_uint64(int(buf)),
                               c_uint64(int(addr)),
                               c_uint64(int(value))))

    def after(self, node: Term, after_node: Term) -> Term:
        return Term(_uop_after(c_uint64(int(node)), c_uint64(int(after_node))))

    def load(self, src: Term) -> Term:
        return Term(_uop_load(c_uint64(int(src))))

    def detach(self, src: Term) -> Term:
        return Term(_uop_detach(c_uint64(int(src))))

    # ============ high-level tensor surface (TAG_TEN) ============
    # Phase 1 of the Python Tensor frontend.  add / mul / cmplt /
    # cmpeq / reduce above already compose TAG_TEN operands; these add
    # tensor create, host I/O, unary/cast, movement, autodiff, and the
    # realize entry point.

    def ten_create(self, dtype: int, dims) -> Term:
        """Allocate a tensor on the DEV-selected backend; returns TAG_TEN."""
        ndim = len(dims)
        arr = (c_uint32 * max(ndim, 1))(*[c_uint32(int(d)) for d in dims])
        return Term(_ten_create(c_uint32(int(dtype)), c_uint32(ndim), arr))

    def ten_write(self, t: Term, data: bytes) -> bool:
        """Copy raw host bytes into a tensor's backend buffer."""
        return bool(_ten_write(c_uint64(int(t)), data, c_uint64(len(data))))

    def ten_read(self, t: Term, nbytes: int) -> bytes:
        """Copy a (realized) tensor's backend buffer out to host bytes."""
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

    # ---- tensor algebra (unary / cast) ----
    def unary(self, opcode: int, x: Term) -> Term:
        return Term(_uop_unary(c_uint32(int(opcode)), c_uint64(int(x))))

    def cast(self, x: Term, dst_dtype: int) -> Term:
        return Term(_uop_cast(c_uint64(int(x)), c_uint32(int(dst_dtype))))

    # ---- movement ops ----
    def _move(self, fn, src: Term, dims) -> Term:
        arr = (c_uint32 * max(len(dims), 1))(*[c_uint32(int(d)) for d in dims])
        return Term(fn(c_uint64(int(src)), c_uint32(len(dims)), arr))

    def reshape(self, src: Term, dims) -> Term:
        return self._move(_uop_reshape, src, dims)

    def permute(self, src: Term, perm) -> Term:
        return self._move(_uop_permute, src, perm)

    def expand(self, src: Term, dims) -> Term:
        return self._move(_uop_expand, src, dims)

    def pad(self, src: Term, begin_end) -> Term:
        """begin_end has 2*ndim entries: (begin, end) pad per axis."""
        arr = (c_uint32 * len(begin_end))(*[c_uint32(int(d)) for d in begin_end])
        return Term(_uop_pad(c_uint64(int(src)),
                             c_uint32(len(begin_end) // 2), arr))

    def shrink(self, src: Term, begin_end) -> Term:
        arr = (c_uint32 * len(begin_end))(*[c_uint32(int(d)) for d in begin_end])
        return Term(_uop_shrink(c_uint64(int(src)),
                                c_uint32(len(begin_end) // 2), arr))

    def flip(self, src: Term, axes_bitmask: int) -> Term:
        return Term(_uop_flip(c_uint64(int(src)), c_uint32(int(axes_bitmask))))

    # ---- autodiff (thvm's real uop_grad chain-rule rewrite) ----
    def grad(self, y: Term, gy: Term) -> Term:
        return Term(_grad(c_uint64(int(y)), c_uint64(int(gy))))

    def grad_with_target(self, y: Term, gy: Term, target: Term) -> Term:
        return Term(_grad_with_target(c_uint64(int(y)), c_uint64(int(gy)),
                                      c_uint64(int(target))))

    def fwd(self, y: Term, gy: Term) -> Term:
        return Term(_fwd(c_uint64(int(y)), c_uint64(int(gy))))

    # ---- reduce to normal form + dispatch ----
    def wnf(self, t: Term) -> Term:
        return Term(_wnf(c_uint64(int(t))))

    def nf(self, t: Term) -> Term:
        return Term(_nf(c_uint64(int(t))))

    def realize(self, t: Term) -> Term:
        """Drive wnf -> materialize -> kernelize -> schedule -> dispatch."""
        return Term(_realize(c_uint64(int(t))))

    def realize_many(self, terms: "list[Term]") -> "list[Term]":
        """Realize several terms in ONE scheduler pass (tinygrad
        `loss.realize(*sched)`).  Returns the resolved terms in order."""
        n = len(terms)
        if n == 0:
            return []
        arr = (c_uint64 * n)(*[int(t) for t in terms])
        _realize_many(arr, c_uint32(n))
        return [Term(arr[i]) for i in range(n)]

    # ---- live-tensor pinning + cross-step buffer reclaim ----
    def pin_set(self, handle: int, t: Term) -> None:
        _pin_set(c_uint64(handle), c_uint64(int(t)))

    def pin_drop(self, handle: int) -> None:
        _pin_drop(c_uint64(handle))

    def reclaim(self) -> None:
        """Free every backend buffer not reachable from a pinned tensor.
        Call between training steps to keep device memory flat."""
        _reclaim()

    # ---- JIT capture / replay (TinyJit) ----
    def jit_begin(self) -> int:       return int(_jit_begin())
    def jit_end(self) -> None:        _jit_end()
    def jit_replay(self, s: int) -> int: return int(_jit_replay(c_uint32(s)))
    def jit_op_count(self, s: int) -> int: return int(_jit_op_count(c_uint32(s)))
    def jit_drop(self, s: int) -> None: _jit_drop(c_uint32(s))

    # ---- introspection (Phase-4 cross-check surface) ----
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

    # ---- requires_grad (canonical on TenDesc.requires_grad) ----
    def ten_set_requires_grad(self, t: Term, on: bool) -> bool:
        return bool(_ten_set_requires_grad(c_uint64(int(t)),
                                           c_int32(1 if on else 0)))

    def ten_get_requires_grad(self, t: Term) -> bool:
        return bool(_ten_get_requires_grad(c_uint64(int(t))))

    def grad_memo_hits   (self) -> int: return int(_grad_memo_hits())
    def grad_memo_misses (self) -> int: return int(_grad_memo_misses())
    def grad_fires       (self) -> int: return int(_grad_fires())

    def ten_get_grad(self, t: Term) -> int:
        """Read TENS[tid].grad (the chain-rule accumulator).
        Returns 0 if no grad has been accumulated yet."""
        return int(_ten_get_grad(c_uint64(int(t))))

    def ten_clear_grad(self, t: Term) -> bool:
        """Zero out TENS[tid].grad (PyTorch's zero_grad analogue)."""
        return bool(_ten_clear_grad(c_uint64(int(t))))

    # ---------------- buffer accessors ----------------
    def buffer_scope(self, t: Term) -> int:
        return int(_uop_buffer_scope(c_uint64(int(t))))

    def buffer_dtype(self, t: Term) -> int:
        return int(_uop_buffer_dtype(c_uint64(int(t))))

    def buffer_ndim(self, t: Term) -> int:
        return int(_uop_buffer_ndim(c_uint64(int(t))))

    def buffer_dim(self, t: Term, d: int) -> int:
        return int(_uop_buffer_dim(c_uint64(int(t)), c_uint32(d)))

    # ---------------- term inspection ----------------
    def term_tag(self, t: Term) -> int: return int(_term_tag(c_uint64(int(t))))
    def term_ext(self, t: Term) -> int: return int(_term_ext(c_uint64(int(t))))
    def term_val(self, t: Term) -> int: return int(_term_val(c_uint64(int(t))))

    # ---------------- BEAM / autotune surface ----------------
    def kernel_alloc(self) -> int:
        """Allocate a synthetic KernelEntry slot. Returns kid."""
        return int(_kernel_alloc())

    def kernel_set_cached_lift(self, kid: int, store_root: Term,
                               out_buf: Term, in_bufs: list[Term]) -> None:
        """Populate `KERNELS[kid].cached_lift` from a Python-built UOp DAG.
        Required before `kernel_opts_propose`.
        """
        n = len(in_bufs)
        arr = (c_uint64 * n)(*[c_uint64(int(b)) for b in in_bufs])
        _kernel_set_cached_lift(c_uint32(kid),
                                c_uint64(int(store_root)),
                                c_uint64(int(out_buf)),
                                arr, c_uint32(n))

    def kernel_opts_propose(self, kid: int, cap: int = 64) -> list[tuple[int, int, int]]:
        """Returns list of (op, axis, arg) candidate KOpts from autotune's
        propose. op values: K.KOP_TC / KOP_LOCAL / KOP_GLOBAL / etc.
        """
        ops = (ctypes.c_uint8 * cap)()
        axes = (ctypes.c_uint8 * cap)()
        args = (c_uint32 * cap)()
        n = _kernel_opts_propose(c_uint32(kid), ops, axes, args, c_uint32(cap))
        return [(int(ops[i]), int(axes[i]), int(args[i])) for i in range(int(n))]

    def kernel_apply_opt(self, kid: int,
                         opt: tuple[int, int, int]) -> Term | None:
        """Apply a KOpt to KERNELS[kid]'s UOp DAG. Mutates cached_lift.store_root
        in place and returns the new root term (so the caller can pass it to
        render). Returns None on bail (unsupported opt, axis out of range, etc.).

        opt is the (op, axis, arg) triple returned by kernel_opts_propose.
        """
        op, axis, arg = opt
        new_root = _kernel_apply_opt(c_uint32(kid),
                                     ctypes.c_uint8(int(op)),
                                     ctypes.c_uint8(int(axis)),
                                     c_uint32(int(arg)))
        if new_root == 0:
            return None
        return Term(int(new_root))

    def uop_dag_apply_kopt(self, root: Term,
                           opt: tuple[int, int, int]) -> Term | None:
        """DAG-only variant: apply a KOpt to a free-standing root term,
        without going through a KernelEntry slot.  Returns the new root
        (or None on bail).
        """
        op, axis, arg = opt
        new_root = _uop_dag_apply_kopt(c_uint64(int(root)),
                                       ctypes.c_uint8(int(op)),
                                       ctypes.c_uint8(int(axis)),
                                       c_uint32(int(arg)))
        if new_root == 0:
            return None
        return Term(int(new_root))

    def cuda_dag_dispatch_shape(self, root: Term) -> tuple[int, int] | None:
        """DAG-derived CUDA launch geometry for `root`.

        Returns (grid, block) -- the 1-D launch shape the structural
        CUDA renderer decodes from the DAG's axis types (KAX_LOOP ->
        grid via `tg`, KAX_LOCAL -> block via `tt`).  The autotune
        sweep needs this for a KOP_LOCAL-split kernel: a flat
        one-thread-per-output grid cannot express the threadblock
        tile.  Returns None if the DAG has no axes / overflows / a
        block dim exceeds the 1024-thread hardware cap.
        """
        grid = c_uint32(0)
        block = c_uint32(0)
        ok = _cuda_dag_dispatch_shape(c_uint64(int(root)),
                                      ctypes.byref(grid),
                                      ctypes.byref(block))
        if not ok:
            return None
        return int(grid.value), int(block.value)

    # ---------------- renderer ----------------
    def render(self, root: Term, name: str = "k") -> str:
        """Render the UOp DAG to a Metal Shading Language kernel string."""
        ptr = _render_uop_kernel(c_uint64(int(root)), name.encode("utf-8"))
        if not ptr:
            return ""
        try:
            return ctypes.string_at(ptr).decode("utf-8")
        finally:
            _string_free(ptr)

    def render_cuda(self, root: Term, name: str = "k") -> str:
        """Render the UOp DAG to a CUDA `.cu` kernel string
        (`extern "C" __global__ void k(...)`).
        """
        ptr = _render_uop_kernel_cuda(c_uint64(int(root)), name.encode("utf-8"))
        if not ptr:
            return ""
        try:
            return ctypes.string_at(ptr).decode("utf-8")
        finally:
            _string_free(ptr)


class Metal:
    """In-process Metal: compile MSL, alloc/write/read buffers, dispatch+time.

    Avoids subprocess overhead per agent iteration. Lifecycle:
      m = Metal(); m.init()
      pso = m.compile_msl(src, fn='k')
      buf = m.buf_alloc(nbytes); m.buf_write(buf, host_bytes)
      ns = m.dispatch(pso, [buf, ...], grid=(...), threadgroup=(...))
      m.buf_read(buf, host_bytes)
      m.shutdown()
    """

    _initialized = False

    def __init__(self):
        if not Metal._initialized:
            ok = _metal_init()
            if not ok:
                raise RuntimeError("py_metal_init failed (no Metal device)")
            Metal._initialized = True

    @staticmethod
    def shutdown():
        if Metal._initialized:
            _metal_shutdown()
            Metal._initialized = False

    # ---------------- compile ----------------
    def compile_msl(self, src: str, fn: str = "k") -> int:
        """Returns a 1-based PSO handle. Raises RuntimeError on compile error."""
        err = ctypes.create_string_buffer(512)
        h = _metal_compile_msl(src.encode("utf-8"), fn.encode("utf-8"),
                               err, c_uint32(512))
        if h == 0:
            raise RuntimeError(f"compile_msl: {err.value.decode('utf-8', 'replace')}")
        return int(h)

    def pso_max_threads(self, pso: int) -> int:
        return int(_metal_pso_max_threads(c_uint32(pso)))

    def pso_release(self, pso: int) -> None:
        _metal_pso_release(c_uint32(pso))

    # ---------------- buffers ----------------
    def buf_alloc(self, nbytes: int) -> int:
        h = _metal_buf_alloc(c_uint64(int(nbytes)))
        if h == 0:
            raise RuntimeError(f"buf_alloc({nbytes}) failed")
        return int(h)

    def buf_write(self, handle: int, data: bytes) -> None:
        ptr = ctypes.c_char_p(bytes(data))
        ok = _metal_buf_write(c_uint32(handle), ptr, c_uint64(len(data)))
        if not ok:
            raise RuntimeError(f"buf_write({handle}, {len(data)}b) failed")

    def buf_write_array(self, handle: int, np_array) -> None:
        """numpy array -> buffer (zero-copy where possible via ctypes)."""
        import numpy as np
        arr = np.ascontiguousarray(np_array)
        nbytes = arr.nbytes
        ok = _metal_buf_write(c_uint32(handle),
                              arr.ctypes.data_as(c_void_p),
                              c_uint64(nbytes))
        if not ok:
            raise RuntimeError(f"buf_write_array({handle}, {nbytes}b) failed")

    def buf_read_array(self, handle: int, shape, dtype):
        """Returns a numpy array of given shape+dtype filled from the buffer."""
        import numpy as np
        arr = np.empty(shape, dtype=dtype)
        ok = _metal_buf_read(c_uint32(handle),
                             arr.ctypes.data_as(c_void_p),
                             c_uint64(arr.nbytes))
        if not ok:
            raise RuntimeError(f"buf_read_array({handle}, {arr.nbytes}b) failed")
        return arr

    def buf_release(self, handle: int) -> None:
        _metal_buf_release(c_uint32(handle))

    # ---------------- dispatch ----------------
    def dispatch(self, pso: int, bufs, *, grid, threadgroup) -> int:
        """Run one kernel; return wall ns (0 on error)."""
        n = len(bufs)
        arr = (c_uint32 * n)(*[c_uint32(int(h)) for h in bufs])
        gx, gy, gz = grid
        tx, ty, tz = threadgroup
        err = ctypes.create_string_buffer(256)
        ns = _metal_dispatch(c_uint32(pso), arr, c_uint32(n),
                             c_uint32(gx), c_uint32(gy), c_uint32(gz),
                             c_uint32(tx), c_uint32(ty), c_uint32(tz),
                             err, c_uint32(256))
        if ns == 0:
            raise RuntimeError(f"dispatch: {err.value.decode('utf-8', 'replace')}")
        return int(ns)

    def dispatch_timed(self, pso: int, bufs, *, grid, threadgroup):
        """Run one kernel; return (wall_ns, gpu_ns)."""
        n = len(bufs)
        arr = (c_uint32 * n)(*[c_uint32(int(h)) for h in bufs])
        gx, gy, gz = grid
        tx, ty, tz = threadgroup
        wall = c_uint64(0)
        gpu = c_uint64(0)
        err = ctypes.create_string_buffer(256)
        ok = _metal_dispatch_timed(
            c_uint32(pso), arr, c_uint32(n),
            c_uint32(gx), c_uint32(gy), c_uint32(gz),
            c_uint32(tx), c_uint32(ty), c_uint32(tz),
            ctypes.byref(wall), ctypes.byref(gpu),
            err, c_uint32(256))
        if not ok:
            raise RuntimeError(f"dispatch_timed: {err.value.decode('utf-8', 'replace')}")
        return int(wall.value), int(gpu.value)


class Cuda:
    """In-process CUDA: nvrtc-compile a .cu string, alloc/write/read
    device buffers, dispatch + time.  The CUDA counterpart of `Metal`.

    Lifecycle:
      c = Cuda()                       # cuInit + context
      fn = c.compile(cu_src, fn='k')   # nvrtc -> module -> CUfunction
      buf = c.buf_alloc(nbytes); c.buf_write_array(buf, np_array)
      ns = c.dispatch(fn, [buf, ...], grid=N, block=B)
      out = c.buf_read_array(buf, shape, dtype)
      c.shutdown()

    The dispatch grid is 1-D: the CUDA structural renderer flattens
    every promoted output LOOP axis onto `tid` (blockIdx.x*blockDim.x +
    threadIdx.x).  `grid` is the block count and `block` the threads
    per block; total threads must cover the output element count.
    """

    _initialized = False

    def __init__(self):
        if not _HAS_CUDA:
            raise RuntimeError(
                "this libthvm_py build has no CUDA bridge "
                "(rebuild `make py` on a Linux+CUDA host)")
        if not Cuda._initialized:
            ok = _cuda_init()
            if not ok:
                raise RuntimeError(
                    f"py_cuda_init failed: "
                    f"{_cuda_last_error().decode('utf-8', 'replace')}")
            Cuda._initialized = True

    @staticmethod
    def available() -> bool:
        """True iff the loaded library carries the CUDA bridge."""
        return _HAS_CUDA

    @staticmethod
    def shutdown():
        if Cuda._initialized:
            _cuda_shutdown()
            Cuda._initialized = False

    def device_sm(self) -> int:
        """Compute capability as a 2-digit int (V100 -> 70)."""
        return int(_cuda_device_sm())

    # ---------------- compile ----------------
    def compile(self, src: str, fn: str = "k") -> int:
        """nvrtc-compile a .cu source string; return a 1-based function
        handle.  Raises RuntimeError on compile / load error.
        """
        err = ctypes.create_string_buffer(512)
        h = _cuda_compile(src.encode("utf-8"), fn.encode("utf-8"),
                          err, c_uint32(512))
        if h == 0:
            raise RuntimeError(f"compile: {err.value.decode('utf-8', 'replace')}")
        return int(h)

    def fn_release(self, fn: int) -> None:
        _cuda_fn_release(c_uint32(fn))

    # ---------------- buffers ----------------
    def buf_alloc(self, nbytes: int) -> int:
        h = _cuda_buf_alloc(c_uint64(int(nbytes)))
        if h == 0:
            raise RuntimeError(
                f"buf_alloc({nbytes}) failed: "
                f"{_cuda_last_error().decode('utf-8', 'replace')}")
        return int(h)

    def buf_write(self, handle: int, data: bytes) -> None:
        ptr = ctypes.c_char_p(bytes(data))
        ok = _cuda_buf_write(c_uint32(handle), ptr, c_uint64(len(data)))
        if not ok:
            raise RuntimeError(f"buf_write({handle}, {len(data)}b) failed")

    def buf_write_array(self, handle: int, np_array) -> None:
        """numpy array -> device buffer (cuMemcpyHtoD)."""
        import numpy as np
        arr = np.ascontiguousarray(np_array)
        ok = _cuda_buf_write(c_uint32(handle),
                             arr.ctypes.data_as(c_void_p),
                             c_uint64(arr.nbytes))
        if not ok:
            raise RuntimeError(f"buf_write_array({handle}, {arr.nbytes}b) failed")

    def buf_read_array(self, handle: int, shape, dtype):
        """Returns a numpy array of given shape+dtype filled from the
        device buffer (cuMemcpyDtoH).
        """
        import numpy as np
        arr = np.empty(shape, dtype=dtype)
        ok = _cuda_buf_read(c_uint32(handle),
                            arr.ctypes.data_as(c_void_p),
                            c_uint64(arr.nbytes))
        if not ok:
            raise RuntimeError(f"buf_read_array({handle}, {arr.nbytes}b) failed")
        return arr

    def buf_release(self, handle: int) -> None:
        _cuda_buf_release(c_uint32(handle))

    # ---------------- dispatch ----------------
    def dispatch(self, fn: int, bufs, *, grid: int, block: int) -> int:
        """Launch one kernel on a 1-D grid; return wall ns.

        `grid` is the block count (gridDim.x), `block` the threads per
        block (blockDim.x).  Buffers bind to the kernel parameters in
        order: out first, then inputs.
        """
        n = len(bufs)
        arr = (c_uint32 * n)(*[c_uint32(int(h)) for h in bufs])
        err = ctypes.create_string_buffer(256)
        ns = _cuda_dispatch(c_uint32(fn), arr, c_uint32(n),
                            c_uint32(int(grid)), c_uint32(int(block)),
                            err, c_uint32(256))
        if ns == 0:
            raise RuntimeError(f"dispatch: {err.value.decode('utf-8', 'replace')}")
        return int(ns)

    def dispatch_timed(self, fn: int, bufs, *, grid: int, block: int):
        """Launch one kernel; return (wall_ns, gpu_ns).

        gpu_ns is true GPU execution time from CUDA events bracketing
        the launch -- the honest analogue of Metal's GPUEndTime timing.
        """
        n = len(bufs)
        arr = (c_uint32 * n)(*[c_uint32(int(h)) for h in bufs])
        wall = c_uint64(0)
        gpu = c_uint64(0)
        err = ctypes.create_string_buffer(256)
        ok = _cuda_dispatch_timed(
            c_uint32(fn), arr, c_uint32(n),
            c_uint32(int(grid)), c_uint32(int(block)),
            ctypes.byref(wall), ctypes.byref(gpu),
            err, c_uint32(256))
        if not ok:
            raise RuntimeError(
                f"dispatch_timed: {err.value.decode('utf-8', 'replace')}")
        return int(wall.value), int(gpu.value)
