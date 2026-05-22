"""Ingest a tinygrad LOWERED kernel and rebuild it as thvm ranged IR.

Companion to from_tinygrad.py (which ports the tensor-level lazy graph and
runs thvm's full backend incl. rangeify).  This module ports the OTHER
level: tinygrad's post-lower kernel uops -- RANGE / INDEX / STORE / ALU
over PARAM buffers, i.e. the form tinygrad's own renderer consumes -- and
rebuilds them as thvm's ranged IR (UOP_BUFFER / UOP_RANGE / UOP_INDEX_E /
UOP_STORE + integer address arithmetic), which thvm renders DIRECTLY
(no rangeify -- the IR is already ranged).  This cross-validates thvm's
codegen against tinygrad's lowered kernels.

A tinygrad lowered kernel is the STORE subtree of `tensor.schedule_linear()`:

    STORE(INDEX(PARAM_out, addr), value)

with RANGE leaves (axis loops, arg=(axis_id, AxisType)), INDEX(buf, addr)
loads, integer ALU on the addresses, and float ALU / WHERE / REDUCE on the
values.  PARAM(arg=n) is kernel-arg slot n (n=0 is the output by
convention).  thvm's renderer promotes the LOOP/GLOBAL axes to GPU threads,
so the dispatch grid is the product of those axis extents (== output numel
for a pure pointwise kernel).

Scope: single-kernel pointwise / simple-index kernels (elementwise, casts,
selects, affine indexing).  Multi-kernel programs (conv, anything with an
intermediate buffer) and accumulator-loop reductions are the next step.
"""
from __future__ import annotations

import ctypes as _ct

import numpy as np

from .thvm import Thvm, K


def _ops():
    from tinygrad.uop.ops import Ops
    return Ops


def _const(u):
    return getattr(u.arg, "x", u.arg)


def _ptr_numel(dt) -> int | None:
    for attr in ("size", "_size"):
        if hasattr(dt, attr):
            try:
                return int(getattr(dt, attr))
            except Exception:
                pass
    return None


def _axis_type(at) -> int:
    s = str(at)
    if "GLOBAL" in s:
        return K.AXIS_GLOBAL
    if "LOCAL" in s:
        return K.AXIS_LOCAL
    if "REDUCE" in s:
        return K.AXIS_REDUCE
    return K.AXIS_LOOP


def _is_int_dtype(dt) -> bool:
    s = str(dt)
    return any(k in s for k in ("int", "uint", "long", "bool", "weakint"))


class _Lowerer:
    """Walks one tinygrad lowered STORE subtree, building thvm ranged IR.

    Records per-PARAM buffer info (slot -> numel) so the caller can size and
    order the dispatch buffers, and the GLOBAL/LOOP axis extents so it can
    size the dispatch grid.
    """

    def __init__(self, h: Thvm):
        self.h = h
        self.memo: dict[int, int] = {}
        self.params: dict[int, int] = {}      # PARAM slot -> numel
        self.grid_axes: list[int] = []        # extents of LOOP/GLOBAL axes

    def emit(self, u) -> int:
        key = id(u)
        if key in self.memo:
            return self.memo[key]
        Ops = _ops()
        op = u.op
        h = self.h
        if op is Ops.PARAM:
            n = _ptr_numel(u.dtype) or 1
            slot = int(u.arg)
            self.params[slot] = n
            t = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n,),
                         instance=slot)
        elif op is Ops.CONST:
            v = _const(u)
            t = (h.iconst(int(v)) if _is_int_dtype(u.dtype)
                 else h.fconst(float(v)))
        elif op is Ops.RANGE:
            extent = int(_const(u.src[0]))
            at = _axis_type(u.arg[1])
            if at in (K.AXIS_LOOP, K.AXIS_GLOBAL):
                self.grid_axes.append(extent)
            t = h.range(axis_id=int(u.arg[0]), axis_type=at, extent=extent)
        elif op is Ops.INDEX:
            t = h.index_e(self.emit(u.src[0]), self.emit(u.src[1]))
        elif op is Ops.REDUCE:
            # REDUCE(value, reduce_range...) -- arg[0] is the reduce op.
            # thvm reduces by a single AXIS_REDUCE range id; nest for
            # multi-axis reduces (src[1:] are the reduce RANGEs).
            kind = (K.REDUCE_MAX if u.arg[0] is getattr(Ops, "MAX", None)
                    else K.REDUCE_SUM)
            t = self.emit(u.src[0])
            for rng in u.src[1:]:
                self.emit(rng)               # build the AXIS_REDUCE range
                t = h.reduce(kind, int(rng.arg[0]), t)
        elif op is Ops.STORE:
            idx = u.src[0]                    # INDEX(buf, addr)
            buf = self.emit(idx.src[0])
            addr = self.emit(idx.src[1])
            t = h.store(buf, addr, self.emit(u.src[1]))
        elif op is Ops.ADD:
            t = self._binop(K.IADD, K.ADD, u)
        elif op is Ops.MUL:
            t = self._binop(K.IMUL, K.MUL, u)
        elif op is Ops.CMPLT:
            t = self._cmp(u)
        elif op is getattr(Ops, "WHERE", None):
            t = int(_uop_iwhere(_ct.c_uint64(int(self.emit(u.src[0]))),
                                _ct.c_uint64(int(self.emit(u.src[1]))),
                                _ct.c_uint64(int(self.emit(u.src[2])))))
        elif op in (getattr(Ops, "RECIP", None), getattr(Ops, "RECIPROCAL", None)):
            t = h.unary(K.RECIP, self.emit(u.src[0]))
        elif op is getattr(Ops, "EXP2", None):
            t = h.unary(K.EXP2, self.emit(u.src[0]))
        elif op is getattr(Ops, "LOG2", None):
            t = h.unary(K.LOG2, self.emit(u.src[0]))
        elif op is getattr(Ops, "SQRT", None):
            t = h.unary(K.SQRT, self.emit(u.src[0]))
        elif op is getattr(Ops, "NEG", None):
            t = h.unary(K.NEG, self.emit(u.src[0]))
        elif op is getattr(Ops, "CAST", None) or op is getattr(Ops, "BITCAST", None):
            # value-preserving cast; thvm cast to the node's dtype
            dst = K.INT32 if _is_int_dtype(u.dtype) else K.FP32
            t = h.cast(self.emit(u.src[0]), dst)
        else:
            raise NotImplementedError(f"lowered op {op}")
        self.memo[key] = t
        return t

    def _binop(self, iop: int, fop: int, u) -> int:
        a, b = self.emit(u.src[0]), self.emit(u.src[1])
        if _is_int_dtype(u.dtype):
            return (self.h.iadd(a, b) if iop == K.IADD else self.h.imul(a, b))
        return (self.h.add(a, b) if fop == K.ADD else self.h.mul(a, b))

    def _cmp(self, u) -> int:
        # CMPLT -> thvm float CMPLT (0/1 mask)
        a, b = self.emit(u.src[0]), self.emit(u.src[1])
        return int(_uop_binary(_ct.c_uint32(K.CMPLT),
                               _ct.c_uint64(int(a)), _ct.c_uint64(int(b))))


# Pull the raw bridge for ops the Thvm handle doesn't wrap directly.
from .thvm import _uop_binary, _uop_iwhere  # noqa: E402


def _kernel_stores(tg_tensor):
    """Return the lowered STORE uops of `tg_tensor`'s schedule, in order."""
    Ops = _ops()
    sink = tg_tensor.schedule_linear()
    return [u for u in sink.toposort() if u.op is Ops.STORE]


def _uniq(buf_uop):
    """The UNIQUE id of a BUFFER uop (its stable buffer identity), or None."""
    Ops = _ops()
    if (buf_uop.op is Ops.BUFFER and buf_uop.src
            and buf_uop.src[0].op is Ops.UNIQUE):
        return buf_uop.src[0].arg
    return None


def _slot_inputs(sink, params, py_data):
    """Map each input PARAM slot (>=1) to its host data array.

    The kernel CALL (the one whose arg0 is the SINK ast) lists its buffers in
    PARAM-slot order in src[1:].  COPY calls (arg0 is a COPY) bind a device
    buffer's uniq (src[1]) to its source python uniq (src[2]); py_data is
    keyed on the python uniqs.  Resolve slot -> device uniq -> python uniq ->
    data.
    """
    Ops = _ops()
    calls = [u for u in sink.toposort() if u.op is Ops.CALL]
    kernel_bufs = None
    copy_map = {}
    for c in calls:
        if not c.src:
            continue
        if c.src[0].op is Ops.SINK:
            kernel_bufs = list(c.src[1:])         # slot 0,1,2,...
        elif c.src[0].op is Ops.COPY and len(c.src) >= 3:
            dst, src = _uniq(c.src[1]), _uniq(c.src[2])
            if dst is not None and src is not None:
                copy_map[dst] = src
    inputs = {}
    if kernel_bufs is None:
        return inputs
    for slot in (s for s in sorted(params) if s != 0):
        if slot >= len(kernel_bufs):
            continue
        dev = _uniq(kernel_bufs[slot])
        py = copy_map.get(dev, dev)
        if py in py_data:
            inputs[slot] = py_data[py]
    return inputs


def cross_validate(build_fn, *, backend: str = "metal"):
    """Cross-validate thvm's codegen against tinygrad's lowered kernel.

    `build_fn` is a 0-arg callable returning a FRESH tinygrad Tensor (the
    computation under test).  It is called twice: once to get tinygrad's
    own result (the reference), and once to extract + lower the kernel --
    tinygrad's schedule_linear() is destructive (it consumes the input
    buffers), so the two graphs must be independent and the input data
    must be read BEFORE scheduling.

    Single-kernel only.  Returns (got, ref, max_abs_err).
    """
    Ops = _ops()

    # NOTE: tinygrad's lowered IR is target-dependent (a REDUCE lowers
    # differently for CPU vs GPU), so tinygrad must lower for the SAME
    # backend thvm renders -- do NOT force a different tinygrad device here.

    # 1) Reference, on an independent graph.
    ref = np.asarray(build_fn().numpy(), dtype=np.float32)

    # 2) Lowering graph: read python-buffer data by UNIQUE id BEFORE
    #    schedule_linear consumes it (it zeroes the input buffers).
    t_low = build_fn()
    py_data = {}
    for u in t_low.uop.toposort():
        uq = _uniq(u)
        if uq is not None and uq not in py_data:
            try:
                py_data[uq] = np.asarray(u.buffer.numpy(),
                                         dtype=np.float32).ravel()
            except Exception:
                pass

    sink = t_low.schedule_linear()
    stores = [u for u in sink.toposort() if u.op is Ops.STORE]
    if len(stores) != 1:
        raise NotImplementedError(
            f"{len(stores)} kernels; single-kernel only for now")

    h = Thvm()
    low = _Lowerer(h)
    root = low.emit(stores[0])

    # Authoritative PARAM-slot -> buffer mapping: the kernel CALL (arg0 is the
    # SINK ast) lists its buffers in slot order; COPY calls link a device
    # buffer's uniq to its source python uniq (which py_data is keyed on).
    inputs = _slot_inputs(sink, low.params, py_data)

    grid_n = 1
    for e in low.grid_axes:
        grid_n *= e
    if grid_n == 0:
        grid_n = max(1, int(np.prod(ref.shape)))

    if backend == "metal":
        return _run_metal(h, root, low, grid_n, ref, inputs)
    if backend == "cuda":
        return _run_cuda(h, root, low, grid_n, ref, inputs)
    raise ValueError(f"backend {backend!r}")


def _run_metal(h, root, low, grid_n, ref, inputs):
    from .thvm import Metal
    m = Metal()
    msl = h.render(root, name="k")
    pso = m.compile_msl(msl, fn="k")
    out_n = int(np.prod(ref.shape)) or 1
    out_buf = m.buf_alloc(out_n * 4)
    bufs = [out_buf]
    handles = {0: out_buf}
    for slot in sorted(inputs):
        b = m.buf_alloc(inputs[slot].nbytes)
        m.buf_write_array(b, inputs[slot].ravel())
        handles[slot] = b
    ordered = [handles[s] for s in sorted(handles)]
    tg = min(grid_n, 256)
    m.dispatch(pso, ordered, grid=(grid_n, 1, 1), threadgroup=(tg, 1, 1))
    got = m.buf_read_array(out_buf, ref.shape if ref.shape else (1,), np.float32)
    return got, ref, float(np.abs(got - ref).max())


def _run_cuda(h, root, low, grid_n, ref, inputs):
    from .thvm import Cuda
    c = Cuda()
    src = h.render_cuda(root, name="k")
    fn = c.compile(src, fn="k")
    out_n = int(np.prod(ref.shape)) or 1
    out_buf = c.buf_alloc(out_n * 4)
    handles = {0: out_buf}
    for slot in sorted(inputs):
        b = c.buf_alloc(inputs[slot].nbytes)
        c.buf_write_array(b, inputs[slot].ravel())
        handles[slot] = b
    ordered = [handles[s] for s in sorted(handles)]
    block = min(grid_n, 256) or 1
    blocks = (grid_n + block - 1) // block
    c.dispatch(fn, ordered, grid=blocks, block=block)
    got = c.buf_read_array(out_buf, ref.shape if ref.shape else (1,), np.float32)
    return got, ref, float(np.abs(got - ref).max())
