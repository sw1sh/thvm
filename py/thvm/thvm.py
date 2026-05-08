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


_DYLIB_PATH = Path(__file__).parent / "libthvm_py.dylib"
if not _DYLIB_PATH.exists():
    raise ImportError(
        f"libthvm_py.dylib not found at {_DYLIB_PATH}. "
        "Run `make py` from repo root."
    )

_lib = ctypes.CDLL(str(_DYLIB_PATH))


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

# ---------------- buffer accessors ----------------
_uop_buffer_scope = _bind("py_uop_buffer_scope", c_uint32, c_uint64)
_uop_buffer_dtype = _bind("py_uop_buffer_dtype", c_uint32, c_uint64)
_uop_buffer_ndim = _bind("py_uop_buffer_ndim", c_uint32, c_uint64)
_uop_buffer_dim = _bind("py_uop_buffer_dim", c_uint32, c_uint64, c_uint32)

# ---------------- renderer ----------------
_render_uop_kernel = _bind("py_render_uop_kernel", c_void_p, c_uint64, c_char_p)
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


# ---------------- in-process Metal compile + dispatch ----------------
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
    # buffer scopes
    SCOPE_GLOBAL = _read_uint32_const("py_const_UOP_SCOPE_GLOBAL")
    SCOPE_LOCAL = _read_uint32_const("py_const_UOP_SCOPE_LOCAL")
    SCOPE_REG = _read_uint32_const("py_const_UOP_SCOPE_REG")
    # FP binary opcodes
    ADD = _read_uint32_const("py_const_UOP_ADD")
    MUL = _read_uint32_const("py_const_UOP_MUL")
    NEG = _read_uint32_const("py_const_UOP_NEG")
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

    # ---------------- renderer ----------------
    def render(self, root: Term, name: str = "k") -> str:
        ptr = _render_uop_kernel(c_uint64(int(root)), name.encode("utf-8"))
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
