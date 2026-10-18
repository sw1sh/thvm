"""Rebuilds a tinygrad Tensor's lazy UOp graph as thvm terms, cross-checking thvm's
backend on tinygrad's graphs; an unmapped op raises NotImplementedError."""
from __future__ import annotations

from .dtypes import DType, dtypes
from .tensor import Tensor, _TH
from .thvm import K, Term


# Imported lazily, so importing thvm does not need tinygrad.
def _ops():
    from tinygrad.uop.ops import Ops
    return Ops


def _bcast(term: int, src_shape, out_shape) -> int:
    """`term` right-aligned into `out_shape` by RESHAPE and EXPAND: tinygrad's ALU
    srcs keep their shapes (uop/ops.py:80-92), thvm's binops take equal ones."""
    src_shape, out_shape = tuple(src_shape), tuple(out_shape)
    if src_shape == out_shape or not out_shape:
        return term
    return _TH.broadcast2(term, src_shape, term, out_shape)[0]


def _emit_b(u, i: int, memo: dict[int, int]) -> int:
    """Emit src i of an elementwise node, broadcast to the node's shape."""
    return _bcast(_emit(u.src[i], memo), _shape_of(u.src[i]), _shape_of(u))


def _is_int_dtype(dt) -> bool:
    """Integer or bool, a CONST of which maps to iconst."""
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
    """thvm's flat [begin0, end0, ...] for a PAD or SHRINK, whose tinygrad marg is a
    per-dim (offset, size), size the output extent (uop/ops.py:419-428): PAD's end is
    the trailing width size - offset - in_dim, SHRINK's the bound offset + size."""
    in_shape = _shape_of(u.src[0])
    flat: list[int] = []
    for d, (offset, size) in enumerate(u.marg):
        offset, size = int(offset), int(size)
        in_dim = int(in_shape[d]) if d < len(in_shape) else 0
        flat.extend([offset, size - offset - in_dim if kind == "PAD" else offset + size])
    return flat


def _emit(u, memo: dict[int, int]) -> int:
    """`u` as a raw thvm Term; `memo` maps id(uop) to its Term, so a shared subgraph
    is built once, and None to the leaf Tensors the walk keeps alive."""
    key = id(u)
    if key not in memo:
        memo[key] = _emit_op(u, memo)
    return memo[key]


def _emit_op(u, memo: dict[int, int]) -> int:
    Ops = _ops()
    op = u.op

    if op == Ops.BUFFER:
        arr = u.buffer.numpy()
        dt = _to_dtype(u.dtype)
        target = _shape_of(u)
        if target and tuple(arr.shape) != target:
            arr = arr.reshape(target)
        leaf = Tensor._from_numpy(arr.astype(dt.np_dtype, copy=False), dt)
        # The walk holds the leaf's term as a raw integer, so the leaf is kept
        # alive: a reclaim on the next leaf's allocation would free its buffer.
        memo.setdefault(None, []).append(leaf)
        return int(leaf.term)

    # A single-device COPY is an identity (schedule/prepare.py:214-215), and
    # tinygrad walks through STAGE, DETACH and CONTIGUOUS_BACKWARD (tensor.py:178).
    if op in (Ops.COPY, Ops.STAGE, Ops.CONTIGUOUS_BACKWARD, Ops.DETACH, Ops.NOOP):
        return _emit(u.src[0], memo)

    if op == Ops.CONST:
        if _is_int_dtype(u.dtype):
            return _TH.iconst(int(u.val))
        return _TH.fconst(float(u.val))

    # AFTER(buf, STORE(buf, v)), as Tensor.assign builds it (tensor.py:259) and an
    # ALLOC'd parameter's init: the value is v.
    if op is Ops.AFTER:
        return _emit(_after_value(u, Ops), memo)

    unary = {Ops.RECIPROCAL: _TH.recip, Ops.EXP2: _TH.exp2,
             Ops.LOG2: _TH.log2, Ops.SQRT: _TH.sqrt, Ops.NEG: _TH.neg}
    if op in unary:
        return unary[op](_emit(u.src[0], memo))

    binary = {Ops.ADD: _TH.add, Ops.MUL: _TH.mul, Ops.CMPLT: _TH.cmplt,
              Ops.SUB: _sub, Ops.MAX: _max, Ops.CMPNE: _cmpne}
    if op in binary:
        return binary[op](_emit_b(u, 0, memo), _emit_b(u, 1, memo))

    # A true select, as an arithmetic blend makes 0 * -inf NaN on max_pool2d's -inf
    # masks; uop_where, as uop_iwhere's folds merge nested selects into an IAND that
    # materialize cannot schedule or differentiate.
    if op == Ops.WHERE:
        return _TH.where(_emit_b(u, 0, memo), _emit_b(u, 1, memo),
                         _emit_b(u, 2, memo))

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
        src = _TH.reduce(kind, range(int(n_axes)), _emit(u.src[0], memo))
        # Reshape to tinygrad's output shape so downstream broadcasts line up.
        return _TH.reshape(src, _shape_of(u))

    if op == Ops.RESHAPE:
        return _TH.reshape(_emit(u.src[0], memo), _shape_of(u))
    if op == Ops.EXPAND:
        # EXPAND prepends its arg to the shape (uop/ops.py:414-415).
        return _bcast(_emit(u.src[0], memo), _shape_of(u.src[0]), _shape_of(u))
    if op == Ops.PERMUTE:
        return _TH.permute(_emit(u.src[0], memo), tuple(int(a) for a in u.arg))
    if op == Ops.PAD:
        return _TH.pad(_emit(u.src[0], memo), _movement_begin_end(u, "PAD"))
    if op == Ops.SHRINK:
        return _TH.shrink(_emit(u.src[0], memo), _movement_begin_end(u, "SHRINK"))
    if op == Ops.FLIP:
        # tinygrad FLIP arg is a per-dim bool tuple; pack into a bitmask.
        mask = sum(1 << i for i, f in enumerate(u.arg) if f)
        return _TH.flip(_emit(u.src[0], memo), mask)

    if op in (Ops.CAST, Ops.BITCAST):
        src = _emit(u.src[0], memo)
        dt = _to_dtype(u.dtype)
        return (_TH.bitcast if op == Ops.BITCAST else _TH.cast)(src, dt.thvm_id)

    raise NotImplementedError(f"unmapped tinygrad op {op}")


def _after_value(u, Ops):
    target, deps = u.src[0], u.src[1:]
    if (len(deps) != 1 or deps[0].op is not Ops.STORE
            or deps[0].src[0] is not target
            or _shape_of(deps[0].src[1]) != _shape_of(u)):
        raise NotImplementedError(
            "AFTER other than one whole-buffer STORE into its own target")
    return deps[0].src[1]


def _sub(a: int, b: int) -> int:
    return _TH.add(a, _TH.neg(b))


def _max(a: int, b: int) -> int:
    """(a < b) ? b : a as a true select, like WHERE: the arithmetic blend is NaN
    whenever the losing operand is infinite."""
    return _TH.where(_TH.cmplt(a, b), b, a)


def _cmpne(a: int, b: int) -> int:
    eq = _TH.cmpeq(a, b)
    one = _TH.fconst(1.0)
    return _TH.add(one, _TH.neg(eq))


def from_tinygrad(tg_tensor) -> Tensor:
    """`tg_tensor`'s lazy UOp graph as a thvm Tensor: taken before the realize that
    replaces it with a buffer, walked after, once its leaf BUFFERs hold data."""
    root = tg_tensor.uop
    tg_tensor.realize()
    memo: dict[int, int] = {}
    term = _emit(root, memo)
    dt = _to_dtype(root.dtype)
    shape = _shape_of(root)
    return Tensor._from_term(Term(term), dt, shape)
