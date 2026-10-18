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

from .dtypes import DType, dtypes
from .tensor import Tensor, _TH
from .thvm import K, Term


# Lazily import the tinygrad Ops enum so importing thvm never hard-depends
# on tinygrad being on the path.  Only `from_tinygrad()` needs it.
def _ops():
    from tinygrad.uop.ops import Ops
    return Ops


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
    return _TH.broadcast2(term, src_shape, term, out_shape)[0]


def _emit_b(u, i: int, memo: dict[int, int]) -> int:
    """Emit src i of an elementwise node, broadcast to the node's shape."""
    return _bcast(_emit(u.src[i], memo), _shape_of(u.src[i]), _shape_of(u))


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

def _emit(u, memo: dict[int, int]) -> int:
    """Translate one tinygrad UOp `u` to a thvm Term, recursing on srcs.

    `memo` maps id(uop) -> thvm Term so a shared subgraph (DAG) is built
    once, and None -> the leaf Tensors the walk keeps alive.  Returns the
    raw u64 Term.
    """
    key = id(u)
    if key not in memo:
        memo[key] = _emit_op(u, memo)
    return memo[key]


def _emit_op(u, memo: dict[int, int]) -> int:
    Ops = _ops()
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
        # The walk holds the leaf's term as a raw integer until the result
        # Tensor wraps it; keep the leaf alive so a reclaim triggered by the
        # next leaf's allocation does not free its buffer.
        memo.setdefault(None, []).append(leaf)
        return int(leaf.term)

    # A COPY's value is its src[0] (a single-device COPY is an identity,
    # schedule/prepare.py:214-215); src[1:] carries device plumbing.
    # contiguous() is `alu(Ops.STAGE)`, and tinygrad itself walks through
    # {STAGE, DETACH, CONTIGUOUS_BACKWARD} as value-identity (tensor.py:178).
    if op in (Ops.COPY, Ops.STAGE, Ops.CONTIGUOUS_BACKWARD, Ops.DETACH, Ops.NOOP):
        return _emit(u.src[0], memo)

    if op == Ops.CONST:
        if _is_int_dtype(u.dtype):
            return _TH.iconst(int(u.val))
        return _TH.fconst(float(u.val))

    # AFTER(buf, STORE(buf, v)): the buffer read after a whole-buffer write.
    # Contiguous allocations (Tensor.ones/zeros params, whose target is an
    # ALLOC) and Tensor.assign take this form, so the value is v.
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

    # WHERE(cond, a, b) -> thvm IWHERE, a true select: correct for
    # non-finite branches (tinygrad max_pool2d masks with -inf; an
    # arithmetic select cond*a+(1-cond)*b would hit 0*-inf=NaN).
    # uop_where, not uop_iwhere: the index-layer folds would merge nested
    # selects into a tensor-level IAND that materialize cannot schedule or
    # differentiate.
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
        # EXPAND PREPENDS len(arg) dims (uop/ops.py:430-431, :731):
        # out = arg + src.shape.  _bcast handles both that and the
        # same-rank expand-the-1s form.
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
    """max(a,b) = (a<b) ? b : a as a true select, like WHERE: the
    arithmetic form (a<b)*b + (1-(a<b))*a is 0*inf = NaN whenever the
    losing operand is infinite."""
    return _TH.where(_TH.cmplt(a, b), b, a)


def _cmpne(a: int, b: int) -> int:
    # a != b  ==  1 - (a == b)
    eq = _TH.cmpeq(a, b)
    one = _TH.fconst(1.0)
    return _TH.add(one, _TH.neg(eq))


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
