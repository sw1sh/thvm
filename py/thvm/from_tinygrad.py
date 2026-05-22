"""Ingest a tinygrad lazy UOp graph and rebuild it in thvm's heap.

Cross-validation tool: tinygrad builds the lazy graph, thvm executes
it.  `from_tinygrad(tg_tensor)` walks the unrealized `tg_tensor.uop`
DAG node-by-node, emits the matching thvm UOp via the Phase-1 bridge
(`py/thvm/thvm.py`), and returns a thvm `Tensor` you can `.numpy()`.

Why the unrealized `.uop`: after a realize/view-merge tinygrad folds
movement into `Ops.VIEW`/`Ops.BUFFER_VIEW` and we lose the structural
RESHAPE/EXPAND/PERMUTE chain thvm wants.  We translate the pre-merge
graph; we DO call `.realize()` once on the source tensor first so the
leaf BUFFERs have host data we can read back.

Op coverage (extend by adding cases to `_emit`; unmapped ops raise
NotImplementedError on purpose so coverage grows explicitly):

  leaves     BUFFER, COPY (passthrough to the BUFFER under it),
             CONST, VCONST-as-value
  plumbing   DEVICE, UNIQUE, LUNIQUE          (never translated; only
             appear as srcs of BUFFER/COPY)
  binary     ADD, MUL, MAX, CMPLT, CMPNE, SUB
  unary      RECIPROCAL/RECIP, EXP2, LOG2, SQRT, NEG
  ternary    WHERE  (cond ? a : b, composed from CMP + mul/add)
  reduce     REDUCE (arg=(Ops.ADD|Ops.MAX, (axis, ...))) and the
             legacy REDUCE_AXIS spelling
  movement   RESHAPE, EXPAND, PERMUTE, PAD, SHRINK, FLIP
  cast       CAST, BITCAST
  noop       CONTIGUOUS, CONTIGUOUS_BACKWARD, DETACH, NOOP (passthrough)

Movement target shapes come from `u.shape` (NOT the symbolic VCONST
shape-spec srcs).  PAD/SHRINK begin/end vectors come from the trailing
VCONST srcs (modern tinygrad) with a fallback to `u.arg` (older).
"""
from __future__ import annotations

from typing import Any

from .dtypes import DType, dtypes
from .tensor import Tensor
from .thvm import (
    K,
    Term,
    _term_fconst,
    _term_iconst,
    _uop_binary,
    _uop_cast,
    _uop_expand,
    _uop_flip,
    _uop_iwhere,
    _uop_pad,
    _uop_permute,
    _uop_reduce,
    _uop_reshape,
    _uop_shrink,
    _uop_unary,
)

import ctypes as _ct


# Lazily import the tinygrad Ops enum so importing thvm never hard-depends
# on tinygrad being on the path.  Only `from_tinygrad()` needs it.
def _ops():
    from tinygrad.uop.ops import Ops
    return Ops


# ---- low-level bridge wrappers (raw u64 Term in/out) --------------------

def _binary(opcode: int, a: int, b: int) -> int:
    return int(_uop_binary(_ct.c_uint32(int(opcode)),
                           _ct.c_uint64(int(a)), _ct.c_uint64(int(b))))


def _iwhere(cond: int, a: int, b: int) -> int:
    return int(_uop_iwhere(_ct.c_uint64(int(cond)),
                           _ct.c_uint64(int(a)), _ct.c_uint64(int(b))))


def _unary(opcode: int, src: int) -> int:
    return int(_uop_unary(_ct.c_uint32(int(opcode)), _ct.c_uint64(int(src))))


def _reduce(kind: int, axis: int, src: int) -> int:
    return int(_uop_reduce(_ct.c_uint32(int(kind)), _ct.c_uint32(int(axis)),
                           _ct.c_uint64(int(src))))


def _cast(src: int, dtype_id: int) -> int:
    return int(_uop_cast(_ct.c_uint64(int(src)), _ct.c_uint32(int(dtype_id))))


def _fconst(value: float) -> int:
    return int(_term_fconst(_ct.c_float(float(value))))


def _iconst(value: int) -> int:
    return int(_term_iconst(_ct.c_int32(int(value))))


def _move(fn, src: int, dims) -> int:
    dims = list(int(d) for d in dims)
    arr = (_ct.c_uint32 * max(len(dims), 1))(*[_ct.c_uint32(d) for d in dims])
    return int(fn(_ct.c_uint64(int(src)), _ct.c_uint32(len(dims)), arr))


def _reshape(src: int, shape) -> int:
    return _move(_uop_reshape, src, shape)


def _expand(src: int, shape) -> int:
    return _move(_uop_expand, src, shape)


def _permute(src: int, perm) -> int:
    return _move(_uop_permute, src, perm)


def _pad(src: int, begin_end) -> int:
    be = list(int(d) for d in begin_end)
    arr = (_ct.c_uint32 * len(be))(*[_ct.c_uint32(d) for d in be])
    return int(_uop_pad(_ct.c_uint64(int(src)), _ct.c_uint32(len(be) // 2), arr))


def _shrink(src: int, begin_end) -> int:
    be = list(int(d) for d in begin_end)
    arr = (_ct.c_uint32 * len(be))(*[_ct.c_uint32(d) for d in be])
    return int(_uop_shrink(_ct.c_uint64(int(src)), _ct.c_uint32(len(be) // 2),
                           arr))


def _flip(src: int, axes_bitmask: int) -> int:
    return int(_uop_flip(_ct.c_uint64(int(src)), _ct.c_uint32(int(axes_bitmask))))


# ---- helpers ------------------------------------------------------------

def _is_int_dtype(dt) -> bool:
    """True if a tinygrad dtype is integral (so a CONST maps to iconst)."""
    name = getattr(dt, "name", str(dt))
    return any(k in name for k in ("int", "bool", "uint", "long"))


def _shape_of(u) -> tuple[int, ...]:
    try:
        return tuple(int(d) for d in u.shape)
    except Exception:
        return ()


def _to_dtype(tg_dtype) -> DType:
    """Map a tinygrad dtype to a thvm DType (float32 / int32 only)."""
    if _is_int_dtype(tg_dtype):
        return dtypes.int32
    return dtypes.float32


def _const_value(u):
    """Extract the scalar payload from a CONST node.

    tinygrad CONST.arg may be a plain python number or a wrapper like
    ConstFloat(0.0) / ConstInt(2) exposing `.x`.
    """
    arg = u.arg
    if hasattr(arg, "x"):
        return arg.x
    return arg


def _find_buffer(u):
    """Descend through COPY / RESHAPE / DEVICE plumbing to the BUFFER leaf."""
    Ops = _ops()
    stack = [u]
    while stack:
        node = stack.pop()
        if node.op == Ops.BUFFER:
            return node
        stack.extend(node.src)
    return None


def _movement_begin_end(u, kind: str):
    """Decode PAD / SHRINK lo+hi vectors into a flat [lo0,hi0,lo1,hi1,...].

    Modern tinygrad encodes the params as the two trailing scalar srcs
    (everything after the data src[0]): lo and hi.  Each is either a
    per-dim VCONST (arg is a tuple) or a scalar CONST broadcast across
    every dim (e.g. SHRINK begins of all-zero is one CONST(0)).  Older
    tinygrad puts a ((lo,hi),...) tuple in u.arg, used as a fallback.
    For SHRINK lo/hi are begin/end ranges; for PAD they are the
    before/after widths -- the same two-vector layout either way.
    """
    Ops = _ops()
    in_ndim = len(_shape_of(u.src[0]))

    def _vec(s, n):
        if s.op == Ops.VCONST:
            return tuple(int(x) for x in s.arg)
        if s.op == Ops.CONST:
            v = getattr(s.arg, "x", s.arg)   # unwrap ConstInt/ConstFloat
            return tuple(int(v) for _ in range(n))
        raise NotImplementedError(
            f"{kind} param src is {s.op}, expected CONST/VCONST")

    param_srcs = [s for s in u.src[1:] if s.op in (Ops.VCONST, Ops.CONST)]
    if len(param_srcs) >= 2:
        lo = _vec(param_srcs[-2], in_ndim)
        hi = _vec(param_srcs[-1], in_ndim)
        flat: list[int] = []
        for d in range(in_ndim):
            flat.extend([lo[d], hi[d]])
        return flat
    arg = u.arg
    if arg is not None:
        flat = []
        for pair in arg:
            flat.extend([int(pair[0]), int(pair[1])])
        return flat
    raise NotImplementedError(
        f"cannot decode {kind} params (no CONST/VCONST srcs, no arg) for {u.op}")


# ---- core walker --------------------------------------------------------

# tinygrad binary Ops -> thvm FP opcode.
def _binary_opcode(op) -> int:
    Ops = _ops()
    table = {
        Ops.ADD: K.ADD,
        Ops.MUL: K.MUL,
        Ops.CMPLT: K.CMPLT,
        Ops.CMPNE: K.CMPEQ,   # thvm has CMPEQ; CMPNE handled below via neg
    }
    return table[op]


def _emit(u, memo: dict[int, int]) -> int:
    """Translate one tinygrad UOp `u` to a thvm Term, recursing on srcs.

    `memo` maps id(uop) -> thvm Term so a shared subgraph (DAG) is built
    once.  Returns the raw u64 Term.
    """
    Ops = _ops()
    key = id(u)
    if key in memo:
        return memo[key]

    op = u.op

    # ---- leaves -------------------------------------------------------
    if op in (Ops.BUFFER, Ops.COPY):
        # Read the realized host data of the BUFFER beneath this node and
        # build a fresh thvm leaf tensor from it.
        buf = _find_buffer(u)
        if buf is None:
            raise NotImplementedError(f"no BUFFER under {op}")
        arr = buf.buffer.numpy()
        dt = _to_dtype(buf.dtype)
        # tinygrad's leaf BUFFER is flat; reshape to this node's shape.
        target = _shape_of(u)
        if target and tuple(arr.shape) != target:
            arr = arr.reshape(target)
        leaf = Tensor._from_numpy(arr.astype(dt.np_dtype, copy=False), dt)
        term = int(leaf.term)
        memo[key] = term
        return term

    if op == Ops.CONST:
        val = _const_value(u)
        if _is_int_dtype(u.dtype):
            term = _iconst(int(val))
        else:
            term = _fconst(float(val))
        memo[key] = term
        return term

    # ---- passthrough plumbing / no-ops --------------------------------
    if op in (Ops.CONTIGUOUS, Ops.CONTIGUOUS_BACKWARD, Ops.DETACH, Ops.NOOP):
        term = _emit(u.src[0], memo)
        memo[key] = term
        return term

    # ---- unary --------------------------------------------------------
    _RECIP = getattr(Ops, "RECIPROCAL", getattr(Ops, "RECIP", None))
    unary_table = {
        _RECIP: K.RECIP,
        Ops.EXP2: K.EXP2,
        Ops.LOG2: K.LOG2,
        Ops.SQRT: K.SQRT,
        Ops.NEG: K.NEG,
    }
    if op in unary_table and op is not None:
        src = _emit(u.src[0], memo)
        term = _unary(unary_table[op], src)
        memo[key] = term
        return term

    # ---- binary -------------------------------------------------------
    if op == Ops.SUB:
        a = _emit(u.src[0], memo)
        b = _emit(u.src[1], memo)
        term = _binary(K.ADD, a, _unary(K.NEG, b))
        memo[key] = term
        return term

    if op == Ops.MAX:
        # max(a,b) = (a<b) ? b : a  =  (a<b)*b + (1-(a<b))*a
        a = _emit(u.src[0], memo)
        b = _emit(u.src[1], memo)
        term = _max(a, b)
        memo[key] = term
        return term

    if op == Ops.CMPNE:
        # a != b  ==  1 - (a == b)
        a = _emit(u.src[0], memo)
        b = _emit(u.src[1], memo)
        eq = _binary(K.CMPEQ, a, b)
        one = _fconst(1.0)
        term = _binary(K.ADD, one, _unary(K.NEG, eq))
        memo[key] = term
        return term

    if op in (Ops.ADD, Ops.MUL, Ops.CMPLT):
        a = _emit(u.src[0], memo)
        b = _emit(u.src[1], memo)
        term = _binary(_binary_opcode(op), a, b)
        memo[key] = term
        return term

    # ---- ternary: WHERE(cond, a, b) -> thvm IWHERE (true select) ------
    # IWHERE renders/realizes as `cond ? a : b`, correct for non-finite
    # branches (tinygrad max_pool2d masks with -inf; an arithmetic select
    # cond*a+(1-cond)*b would hit 0*-inf=NaN).
    if op == Ops.WHERE:
        cond = _emit(u.src[0], memo)
        a = _emit(u.src[1], memo)
        b = _emit(u.src[2], memo)
        term = _iwhere(cond, a, b)
        memo[key] = term
        return term

    # ---- reduce -------------------------------------------------------
    if op == Ops.REDUCE or getattr(Ops, "REDUCE_AXIS", None) == op:
        rop, axes = u.arg
        if rop == Ops.ADD:
            kind = K.REDUCE_SUM
        elif rop == Ops.MAX:
            kind = K.REDUCE_MAX
        else:
            raise NotImplementedError(f"unmapped REDUCE op {rop}")
        src = _emit(u.src[0], memo)
        # thvm reduces one axis at a time; reduce innermost-first so the
        # outer axis indices stay valid (thvm REDUCE keeps the axis as a
        # length-1 dim, matching tinygrad's REDUCE output shape).
        for ax in sorted((int(a) for a in axes), reverse=True):
            src = _reduce(kind, ax, src)
        memo[key] = src
        return src

    # ---- movement -----------------------------------------------------
    if op == Ops.RESHAPE:
        src = _emit(u.src[0], memo)
        term = _reshape(src, _shape_of(u))
        memo[key] = term
        return term

    if op == Ops.EXPAND:
        src = _emit(u.src[0], memo)
        term = _expand(src, _shape_of(u))
        memo[key] = term
        return term

    if op == Ops.PERMUTE:
        src = _emit(u.src[0], memo)
        term = _permute(src, tuple(int(a) for a in u.arg))
        memo[key] = term
        return term

    if op == Ops.PAD:
        src = _emit(u.src[0], memo)
        term = _pad(src, _movement_begin_end(u, "PAD"))
        memo[key] = term
        return term

    if op == Ops.SHRINK:
        src = _emit(u.src[0], memo)
        term = _shrink(src, _movement_begin_end(u, "SHRINK"))
        memo[key] = term
        return term

    if op == Ops.FLIP:
        src = _emit(u.src[0], memo)
        # tinygrad FLIP arg is a per-dim bool tuple; pack into a bitmask.
        flips = u.arg
        mask = 0
        for i, f in enumerate(flips):
            if f:
                mask |= (1 << i)
        term = _flip(src, mask)
        memo[key] = term
        return term

    # ---- cast ---------------------------------------------------------
    if op in (Ops.CAST, Ops.BITCAST):
        src = _emit(u.src[0], memo)
        dt = _to_dtype(u.dtype)
        term = _cast(src, dt.thvm_id)
        memo[key] = term
        return term

    if op is getattr(Ops, "VIEW", None) or op is getattr(Ops, "BUFFER_VIEW", None):
        raise NotImplementedError(
            "Ops.VIEW found: this is a post-view-merge graph. Pass the "
            "unrealized tensor's .uop (do not realize before from_tinygrad).")

    raise NotImplementedError(f"unmapped tinygrad op {op}")


def _max(a: int, b: int) -> int:
    """max(a,b) = (a<b)*b + (1-(a<b))*a, all on raw thvm Terms."""
    less = _binary(K.CMPLT, a, b)
    one = _fconst(1.0)
    not_less = _binary(K.ADD, one, _unary(K.NEG, less))
    return _binary(K.ADD, _binary(K.MUL, less, b), _binary(K.MUL, not_less, a))


# ---- public entry point -------------------------------------------------

def from_tinygrad(tg_tensor) -> Tensor:
    """Rebuild a tinygrad Tensor's lazy UOp graph in thvm; return a thvm
    Tensor.

    Realizes the source tensor first (so leaf BUFFERs hold host data we
    can read back), then walks the *original* `.uop` DAG -- which still
    carries the structural movement chain -- and emits the matching thvm
    UOp per node, memoizing id(uop) -> thvm Term to preserve sharing.
    """
    root = tg_tensor.uop
    # Allocate leaf buffers so `.buffer.numpy()` works on BUFFER leaves.
    tg_tensor.realize()
    memo: dict[int, int] = {}
    term = _emit(root, memo)
    dt = _to_dtype(root.dtype)
    shape = _shape_of(root)
    return Tensor._from_term(Term(term), dt, shape)
