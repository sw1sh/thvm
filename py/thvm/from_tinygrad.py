"""Ingest a tinygrad lazy UOp graph and rebuild it in thvm's heap.

Cross-validation tool: tinygrad builds the lazy graph, thvm executes
it.  `from_tinygrad(tg_tensor)` walks the unrealized `tg_tensor.uop`
DAG node-by-node, emits the matching thvm UOp via the Phase-1 bridge
(`py/thvm/thvm.py`), and returns a thvm `Tensor` you can `.numpy()`.

Why the unrealized `.uop`: a realize replaces the tensor's graph with
its result buffer and we lose the structural RESHAPE/EXPAND/PERMUTE chain
thvm wants.  We translate the pre-realize graph; we DO call `.realize()`
once on the source tensor first so the leaf BUFFERs have host data we can
read back.

Op coverage (extend by adding cases to `_emit`; unmapped ops raise
NotImplementedError on purpose so coverage grows explicitly):

  leaves     BUFFER, COPY (passthrough to the BUFFER under it), CONST
  buffers    AFTER(buf, STORE(buf, v)) -> v  (whole-buffer write, e.g. an
             ALLOC'd Tensor.ones parameter or an assign)
  binary     ADD, MUL, MAX, CMPLT, CMPNE, SUB
  unary      RECIPROCAL, EXP2, LOG2, SQRT, NEG
  ternary    WHERE  (cond ? a : b, a true IWHERE select built by uop_where,
             the value-level constructor: no index-layer folds, so nested
             selects stay selects and the chain rule differentiates them;
             MAX uses the same)
  reduce     REDUCE in tinygrad's front normal form, arg=(Ops.ADD|Ops.MAX,
             n_front_axes): the reduced axes were PERMUTEd to the front and
             the output drops them (uop/ops.py:655-664, :433-437)
  movement   RESHAPE, EXPAND, PERMUTE, PAD, SHRINK, FLIP
  cast       CAST, BITCAST
  noop       STAGE, CONTIGUOUS_BACKWARD, DETACH, NOOP (passthrough)

Movement target shapes come from `u.shape`; PAD/SHRINK (offset, size)
pairs come from `u.marg`.
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
    _uop_bitcast,
    _uop_cast,
    _uop_expand,
    _uop_flip,
    _uop_pad,
    _uop_permute,
    _uop_reduce,
    _uop_reshape,
    _uop_shrink,
    _uop_unary,
    _uop_where,
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


def _where(cond: int, a: int, b: int) -> int:
    return int(_uop_where(_ct.c_uint64(int(cond)),
                          _ct.c_uint64(int(a)), _ct.c_uint64(int(b))))


def _unary(opcode: int, src: int) -> int:
    return int(_uop_unary(_ct.c_uint32(int(opcode)), _ct.c_uint64(int(src))))


def _reduce(kind: int, axis: int, src: int) -> int:
    return int(_uop_reduce(_ct.c_uint32(int(kind)), _ct.c_uint32(int(axis)),
                           _ct.c_uint64(int(src))))


def _cast(src: int, dtype_id: int) -> int:
    return int(_uop_cast(_ct.c_uint64(int(src)), _ct.c_uint32(int(dtype_id))))


def _bitcast(src: int, dtype_id: int) -> int:
    return int(_uop_bitcast(_ct.c_uint64(int(src)), _ct.c_uint32(int(dtype_id))))


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


def _bcast(term: int, src_shape, out_shape) -> int:
    """Right-align `src_shape` into `out_shape` and expand -- tinygrad's
    implicit broadcast (uop/ops.py:70-82 _broadcast_shape/broadcast_axes):
    elementwise srcs keep their own shapes and the ALU node's shape is the
    per-right-aligned-dim broadcast; codegen expands late.  thvm's binops
    take same-shape operands, so the bridge materialises the broadcast as
    RESHAPE (prepend 1s) + EXPAND."""
    src_shape, out_shape = tuple(src_shape), tuple(out_shape)
    if src_shape == out_shape or not out_shape:
        return term
    if len(src_shape) < len(out_shape):
        src_shape = (1,) * (len(out_shape) - len(src_shape)) + src_shape
        term = _reshape(term, src_shape)
    if src_shape != out_shape:
        term = _expand(term, out_shape)
    return term


def _emit_b(u, i: int, memo: dict[int, int]) -> int:
    """Emit src i of an elementwise node, broadcast to the node's shape."""
    return _bcast(_emit(u.src[i], memo), _shape_of(u.src[i]), _shape_of(u))


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


def _movement_begin_end(u, kind: str):
    """Decode a PAD / SHRINK movement-arg into thvm's flat
    [begin0,end0,begin1,end1,...] per-axis bounds.

    IMPORTANT SEMANTIC CONVERSION.  tinygrad's movement-arg
    (`UOp.marg`) for PAD and SHRINK is a per-dim `(offset, size)` tuple,
    NOT `(before_width, after_width)` / `(begin, end)` (tinygrad
    `uop/ops.py` shape rule: PAD/SHRINK output dim == `size`, with the
    input of length `s` placed at `offset` so that `offset + s <= size`
    for PAD and `offset + size <= s` for SHRINK).  thvm's UOP_PAD takes
    `(begin_width, end_width)` and UOP_SHRINK takes a `[begin, end)`
    slice, so we MUST translate:

      PAD:    begin = offset
              end   = size - offset - in_dim      (trailing pad width)
      SHRINK: begin = offset
              end   = offset + size               (slice upper bound)

    Reading tinygrad's `(offset, size)` straight through as thvm
    `(begin, end)` is what corrupted the arange/cumsum construction: the
    PAD `((3, 7),)` over a length-4 input is offset=3 / out_size=7 (thvm
    begin=3, end=0), but the literal read gave thvm a (3, 7) before/after
    pad -> a length-14 buffer whose downstream rank-changing RESHAPE then
    failed to decompose and zeroed the whole result.
    """
    in_shape = _shape_of(u.src[0])
    flat: list[int] = []
    for d, (offset, size) in enumerate(u.marg):
        offset, size = int(offset), int(size)
        in_dim = int(in_shape[d]) if d < len(in_shape) else 0
        flat.extend([offset, size - offset - in_dim if kind == "PAD" else offset + size])
    return flat


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
    once, and None -> the leaf Tensors the walk keeps alive.  Returns the
    raw u64 Term.
    """
    Ops = _ops()
    key = id(u)
    if key in memo:
        return memo[key]

    op = u.op

    # ---- leaves -------------------------------------------------------
    if op == Ops.BUFFER:
        # Read the realized host data into a fresh thvm leaf tensor.
        arr = u.buffer.numpy()
        dt = _to_dtype(u.dtype)
        target = _shape_of(u)
        if target and tuple(arr.shape) != target:
            arr = arr.reshape(target)
        leaf = Tensor._from_numpy(arr.astype(dt.np_dtype, copy=False), dt)
        term = int(leaf.term)
        memo[key] = term
        # The walk holds the leaf's term as a raw integer until the result
        # Tensor wraps it; keep the leaf alive so a reclaim triggered by the
        # next leaf's allocation does not free its buffer.
        memo.setdefault(None, []).append(leaf)
        return term

    # A COPY's value is its src[0] (a single-device COPY is an identity,
    # schedule/prepare.py:214-215); src[1:] carries device plumbing.
    # Descending to "the first BUFFER below" dropped the computation and any
    # movement between the COPY and its buffer.
    if op == Ops.COPY:
        term = _emit(u.src[0], memo)
        memo[key] = term
        return term

    if op == Ops.CONST:
        val = u.val
        if _is_int_dtype(u.dtype):
            term = _iconst(int(val))
        else:
            term = _fconst(float(val))
        memo[key] = term
        return term

    # ---- materialized buffers -----------------------------------------
    # AFTER(buf, STORE(buf, v)): the buffer read after a whole-buffer write.
    # Contiguous allocations (Tensor.ones/zeros params, whose target is an
    # ALLOC) and Tensor.assign take this form, so the value is v.
    if op is Ops.AFTER:
        term = _emit(_after_value(u, Ops), memo)
        memo[key] = term
        return term

    # ---- passthrough plumbing / no-ops --------------------------------
    # contiguous() is `alu(Ops.STAGE)`, and tinygrad itself walks through
    # {STAGE, DETACH, CONTIGUOUS_BACKWARD} as value-identity (tensor.py:178).
    if op in (Ops.STAGE, Ops.CONTIGUOUS_BACKWARD, Ops.DETACH, Ops.NOOP):
        term = _emit(u.src[0], memo)
        memo[key] = term
        return term

    # ---- unary --------------------------------------------------------
    unary_table = {
        Ops.RECIPROCAL: K.RECIP,
        Ops.EXP2: K.EXP2,
        Ops.LOG2: K.LOG2,
        Ops.SQRT: K.SQRT,
        Ops.NEG: K.NEG,
    }
    if op in unary_table:
        src = _emit(u.src[0], memo)
        term = _unary(unary_table[op], src)
        memo[key] = term
        return term

    # ---- binary -------------------------------------------------------
    if op == Ops.SUB:
        a = _emit_b(u, 0, memo)
        b = _emit_b(u, 1, memo)
        term = _binary(K.ADD, a, _unary(K.NEG, b))
        memo[key] = term
        return term

    if op == Ops.MAX:
        a = _emit_b(u, 0, memo)
        b = _emit_b(u, 1, memo)
        term = _max(a, b)
        memo[key] = term
        return term

    if op == Ops.CMPNE:
        # a != b  ==  1 - (a == b)
        a = _emit_b(u, 0, memo)
        b = _emit_b(u, 1, memo)
        eq = _binary(K.CMPEQ, a, b)
        one = _fconst(1.0)
        term = _binary(K.ADD, one, _unary(K.NEG, eq))
        memo[key] = term
        return term

    if op in (Ops.ADD, Ops.MUL, Ops.CMPLT):
        a = _emit_b(u, 0, memo)
        b = _emit_b(u, 1, memo)
        term = _binary(_binary_opcode(op), a, b)
        memo[key] = term
        return term

    # ---- ternary: WHERE(cond, a, b) -> thvm IWHERE (true select) ------
    # IWHERE renders/realizes as `cond ? a : b`, correct for non-finite
    # branches (tinygrad max_pool2d masks with -inf; an arithmetic select
    # cond*a+(1-cond)*b would hit 0*-inf=NaN).  uop_where, not uop_iwhere:
    # the index-layer folds would merge nested selects into a tensor-level
    # IAND that materialize cannot schedule or differentiate.
    if op == Ops.WHERE:
        cond = _emit_b(u, 0, memo)
        a = _emit_b(u, 1, memo)
        b = _emit_b(u, 2, memo)
        term = _where(cond, a, b)
        memo[key] = term
        return term

    # ---- reduce -------------------------------------------------------
    if op == Ops.REDUCE:
        rop, n_axes = u.arg
        if rop == Ops.ADD:
            kind = K.REDUCE_SUM
        elif rop == Ops.MAX:
            kind = K.REDUCE_MAX
        else:
            raise NotImplementedError(f"unmapped REDUCE op {rop}")
        # Front normal form (uop/ops.py:655-664): the reduced axes are the
        # first n_axes of the (already permuted) src.
        src = _emit(u.src[0], memo)
        # thvm reduces one axis at a time; reduce innermost-first so the
        # outer axis indices stay valid.
        for ax in reversed(range(int(n_axes))):
            src = _reduce(kind, ax, src)
        # Reshape to tinygrad's output shape so downstream broadcasts line up.
        src = _reshape(src, _shape_of(u))
        memo[key] = src
        return src

    # ---- movement -----------------------------------------------------
    if op == Ops.RESHAPE:
        src = _emit(u.src[0], memo)
        term = _reshape(src, _shape_of(u))
        memo[key] = term
        return term

    if op == Ops.EXPAND:
        # EXPAND now PREPENDS len(arg) dims (uop/ops.py:430-431, :731):
        # out = arg + src.shape.  _bcast handles both that and the legacy
        # same-rank expand-the-1s form.
        src = _emit(u.src[0], memo)
        term = _bcast(src, _shape_of(u.src[0]), _shape_of(u))
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
        term = (_bitcast if op == Ops.BITCAST else _cast)(src, dt.thvm_id)
        memo[key] = term
        return term

    raise NotImplementedError(f"unmapped tinygrad op {op}")


def _after_value(u, Ops):
    target, deps = u.src[0], u.src[1:]
    if (len(deps) != 1 or deps[0].op is not Ops.STORE
            or deps[0].src[0] is not target
            or _shape_of(deps[0].src[1]) != _shape_of(u)):
        raise NotImplementedError(
            "AFTER other than one whole-buffer STORE into its own target")
    return deps[0].src[1]


def _max(a: int, b: int) -> int:
    """max(a,b) = (a<b) ? b : a as a true select, like WHERE: the
    arithmetic form (a<b)*b + (1-(a<b))*a is 0*inf = NaN whenever the
    losing operand is infinite."""
    return _where(_binary(K.CMPLT, a, b), b, a)


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
