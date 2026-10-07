"""Ingest a tinygrad LOWERED kernel and rebuild it as thvm ranged IR.

Companion to from_tinygrad.py (which ports the tensor-level lazy graph and
runs thvm's full backend incl. rangeify).  This module ports the OTHER
level: tinygrad's post-lower kernel uops -- RANGE / INDEX / STORE / ALU
over PARAM buffers, i.e. the form tinygrad's own renderer consumes -- and
rebuilds them as thvm's ranged IR (UOP_BUFFER / UOP_RANGE / UOP_INDEX_E /
UOP_STORE + integer address arithmetic), which thvm renders DIRECTLY
(no rangeify -- the IR is already ranged) as C99 and runs on the CPU.
This cross-validates thvm's codegen against tinygrad's lowered kernels.

A tinygrad lowered kernel is the STORE subtree of the compute CALL (the one
whose body is a SINK) in `tensor.schedule_linear()`:

    STORE(INDEX(PARAM_out, addr), value)

with RANGE leaves (axis loops, arg=(axis_id, AxisType)), INDEX(buf, addr)
loads, integer ALU on the addresses, and float ALU / WHERE / REDUCE on the
values.  A PARAM's ParamArg slot n is kernel-arg slot n (n=0 is the output
by convention).

The other CALLs in the schedule are copies that stage the host inputs;
they are followed back to the source buffers to find each input's data.

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


def _axis_type(at) -> int:
    s = str(at)
    if "GLOBAL" in s:
        return K.AXIS_GLOBAL
    if "LOCAL" in s:
        return K.AXIS_LOCAL
    if "REDUCE" in s:
        return K.AXIS_REDUCE
    if "DEVICE" in s:
        # a multi-device (UNSHARD) sharding axis; this bridge ingests
        # single-device kernels only -- refuse rather than misroute.
        raise NotImplementedError("AxisType.DEVICE range in a single-device import")
    # AxisType.WEAK (placement uncommitted; tinygrad #17283 renamed the old
    # LOOP to WEAK and STRONGLOOP to LOOP) and AxisType.LOOP (committed
    # serial loop) both render as thvm's serial AXIS_LOOP.
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

    def emit(self, u) -> int:
        key = id(u)
        if key in self.memo:
            return self.memo[key]
        Ops = _ops()
        op = u.op
        h = self.h
        if op is Ops.PARAM:
            # The PARAM arg is a ParamArg (uop/ops.py:23): its size is the
            # buffer's element count, None for a scalar.
            if "ALU" in str(u.arg.addrspace):
                # A bound Variable is an ALU PARAM (slot -1, tinygrad
                # #18444): a scalar argument, not a buffer to bind.
                raise NotImplementedError("ALU PARAM (bound Variable) in a lowered kernel")
            slot = int(u.arg.slot)
            n = 1 if u.arg.size is None else int(u.arg.size)
            self.params[slot] = n
            t = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(n,),
                         instance=slot)
        elif op is Ops.CONST:
            v = u.val
            t = (h.iconst(int(v)) if _is_int_dtype(u.dtype)
                 else h.fconst(float(v)))
        elif op is Ops.RANGE:
            extent = int(u.src[0].val)
            # RANGE arg = (axis_id, ..., axis_type): tinygrad reads the type
            # as arg[-1] (uop/ops.py:89, :611, :1172); a multi-part range
            # carries sub-ids between the two.
            at = _axis_type(u.arg[-1])
            t = h.range(axis_id=int(u.arg[0]), axis_type=at, extent=extent)
        elif op is Ops.INDEX:
            t = h.index_e(self.emit(u.src[0]), self.emit(u.src[1]))
        elif op is Ops.REDUCE:
            # REDUCE(value, reduce_range...) -- arg[0] is the reduce op.
            # thvm reduces by a single AXIS_REDUCE range id; nest for
            # multi-axis reduces (src[1:] are the reduce RANGEs).
            kind = K.REDUCE_MAX if u.arg[0] is Ops.MAX else K.REDUCE_SUM
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
            t = h.cmplt(self.emit(u.src[0]), self.emit(u.src[1]))
        elif op is Ops.WHERE:
            t = h.iwhere(self.emit(u.src[0]), self.emit(u.src[1]),
                         self.emit(u.src[2]))
        elif op is Ops.RECIPROCAL:
            t = h.unary(K.RECIP, self.emit(u.src[0]))
        elif op is Ops.EXP2:
            t = h.unary(K.EXP2, self.emit(u.src[0]))
        elif op is Ops.LOG2:
            t = h.unary(K.LOG2, self.emit(u.src[0]))
        elif op is Ops.SQRT:
            t = h.unary(K.SQRT, self.emit(u.src[0]))
        elif op is Ops.NEG:
            t = h.unary(K.NEG, self.emit(u.src[0]))
        elif op is Ops.CAST:
            dst = K.INT32 if _is_int_dtype(u.dtype) else K.FP32
            t = h.cast(self.emit(u.src[0]), dst)
        elif op is Ops.BITCAST:
            dst = K.INT32 if _is_int_dtype(u.dtype) else K.FP32
            t = h.bitcast(self.emit(u.src[0]), dst)
        elif op is Ops.MAX:
            # Binary MAX survives into the lowered IR now (relu = MAX(x, 0)).
            # thvm's kernel IR has no max opcode; a true select via IWHERE
            # (not an arithmetic blend) stays correct when a branch is
            # non-finite, e.g. -inf pool masks.  Float only: the kernel
            # CMPLT is the float compare.
            if _is_int_dtype(u.dtype):
                raise NotImplementedError("lowered int MAX (no int CMPLT in the kernel IR)")
            a, b = self.emit(u.src[0]), self.emit(u.src[1])
            t = h.iwhere(h.cmplt(a, b), b, a)
        else:
            raise NotImplementedError(f"lowered op {op}")
        self.memo[key] = t
        return t

    def _binop(self, iop: int, fop: int, u) -> int:
        a, b = self.emit(u.src[0]), self.emit(u.src[1])
        if _is_int_dtype(u.dtype):
            return (self.h.iadd(a, b) if iop == K.IADD else self.h.imul(a, b))
        return (self.h.add(a, b) if fop == K.ADD else self.h.mul(a, b))


def _param_slot(p) -> int:
    return int(p.arg.slot)


def _uniq(buf_uop):
    """A BUFFER's stable identity (its ParamArg slot, uop/ops.py:23), or
    None for any other UOp."""
    return int(buf_uop.arg.slot) if buf_uop.op is _ops().BUFFER else None


def _copy_links(calls) -> dict:
    """Map each copy CALL's destination arg to its source arg.

    tinygrad schedules a copy as a STORE-bodied CALL (copy-is-store,
    schedule/prepare.py:218-222): the body is STORE(PARAM_dst, PARAM_src)
    and the PARAM slots index the CALL's own args.  The destination is an
    arena view (BITCAST of a SHRINK of the arena BUFFER,
    engine/memory.py:55-57) that the kernel CALL takes as an input -- the
    SAME hash-consed UOp, so the view itself is the join key.
    """
    Ops = _ops()
    links = {}
    for c in calls:
        body, args = c.src[0], c.src[1:]
        if (body.op is Ops.STORE and len(body.src) >= 2
                and all(x.op is Ops.PARAM for x in body.src[:2])):
            links[args[_param_slot(body.src[0])]] = args[_param_slot(body.src[1])]
    return links


def _kernel_call(sink):
    """The schedule's one compute CALL (its body is the kernel SINK)."""
    Ops = _ops()
    calls = [u for u in sink.toposort() if u.op is Ops.CALL and u.src]
    kernels = [c for c in calls if c.src[0].op is Ops.SINK]
    if len(kernels) != 1:
        raise NotImplementedError(
            f"{len(kernels)} kernels; single-kernel only for now")
    return kernels[0], calls


def _slot_inputs(kernel, calls, params, py_data):
    """Map each input PARAM slot (>=1) to its host data array.

    The kernel CALL lists its buffers in PARAM-slot order in src[1:].  Walk
    each through the copy links back to the source buffer, whose uniq keys
    py_data.  A slot with no host data is an error, not a skip: the renderer
    packs its inputs densely by slot, so a dropped slot would shift every
    later input onto the wrong kernel argument.
    """
    kernel_bufs = kernel.src[1:]
    links = _copy_links(calls)
    inputs, missing = {}, []
    for slot in (s for s in sorted(params) if s != 0):
        buf = kernel_bufs[slot] if 0 < slot < len(kernel_bufs) else None
        while buf is not None and buf in links:
            buf = links[buf]
        uq = _uniq(buf) if buf is not None else None
        if uq in py_data:
            inputs[slot] = py_data[uq]
        else:
            missing.append(slot)
    if missing:
        raise NotImplementedError(f"no host data for input PARAM slots {missing}")
    return inputs


def cross_validate(build_fn, *, opts=None):
    """Cross-validate thvm's codegen against tinygrad's lowered kernel.

    `build_fn` is a 0-arg callable returning a FRESH tinygrad Tensor (the
    computation under test).  It is called twice: once to get tinygrad's
    own result (the reference), and once to extract + lower the kernel --
    tinygrad's schedule_linear() is destructive (it consumes the input
    buffers), so the two graphs must be independent and the input data
    must be read BEFORE scheduling.

    thvm renders the kernel as C99 and runs it in-process; tinygrad is
    pinned to the CPU for the reference and the lowering.

    `opts`, if given, is a list of (KOP, axis, factor) thvm KOpts (e.g.
    [(K.KOP_UPCAST, 0, 4)]) applied to the imported ranged-IR root before
    render -- this cross-validates thvm's KOpt transforms (UPCAST / UNROLL
    / LOCAL / ...) on a real tinygrad kernel: the OPTIMIZED kernel must
    still match tinygrad's (unoptimized) result.

    Single-kernel only.  Returns (got, ref, max_abs_err).
    """
    from tinygrad.helpers import Context
    # tinygrad's lowered IR is target-dependent (a REDUCE lowers differently
    # for CPU vs GPU), so it must lower for the CPU thvm renders for.
    with Context(DEV="CPU"):
        # 1) Reference, on an independent graph.
        ref = np.asarray(build_fn().numpy(), dtype=np.float32)

        # 2) Lowering graph: read python-buffer data by uniq BEFORE
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

    Ops = _ops()
    kernel, calls = _kernel_call(sink)
    stores = [u for u in kernel.src[0].toposort() if u.op is Ops.STORE]
    if len(stores) != 1:
        raise NotImplementedError(f"{len(stores)} stores; single-output only for now")

    h = Thvm()
    low = _Lowerer(h)
    root = low.emit(stores[0])

    # Apply any thvm KOpts to the ranged-IR root (UPCAST/UNROLL/LOCAL/...).
    for opt in (opts or []):
        new_root = h.uop_dag_apply_kopt(root, tuple(int(x) for x in opt))
        if new_root is None:
            raise RuntimeError(f"KOpt {opt} bailed")
        root = new_root

    inputs = _slot_inputs(kernel, calls, low.params, py_data)

    return _run_cpu(h, root, ref, inputs)


def _run_cpu(h, root, ref, inputs):
    """Render C99 and run it in-process.  The kernel loops over its own
    axes, so there is no grid; the signature is the CPU-JIT contract that
    src/backend/cpu/jit.c dlsyms, compiled by the JIT's own compiler and
    flags (cpu_jit_compile_shared)."""
    import os
    import tempfile
    src = h.render_c(root, name="k")
    if not src:
        raise RuntimeError("thvm's C renderer declined the kernel signature")
    shape = ref.shape if ref.shape else (1,)
    out = np.zeros(int(np.prod(shape)), dtype=np.float32)
    ins = [np.ascontiguousarray(inputs[s], dtype=np.float32) for s in sorted(inputs)]
    with tempfile.TemporaryDirectory(prefix="thvm_lowered_") as d:
        c_path, so_path = os.path.join(d, "k.c"), os.path.join(d, "k.so")
        with open(c_path, "w") as f:
            f.write(src)
        if not h.compile_c_shared(c_path, so_path):
            raise RuntimeError("compiling the rendered kernel failed "
                               "(THVM_CPU_JIT_VERBOSE=1 shows the compiler's errors)")
        k = _ct.CDLL(so_path).k
    k.restype = None
    k.argtypes = [_ct.c_void_p, _ct.POINTER(_ct.c_void_p), _ct.c_uint,
                  _ct.POINTER(_ct.c_uint), _ct.POINTER(_ct.c_uint)]
    n_in = max(1, len(ins))
    ptrs = (_ct.c_void_p * n_in)(*[a.ctypes.data for a in ins])
    numels = (_ct.c_uint * n_in)(*[a.size for a in ins])
    kvars = (_ct.c_uint * 1)()
    k(out.ctypes.data, ptrs, out.size, numels, kvars)
    got = out.reshape(shape)
    return got, ref, float(np.abs(got - ref).max())
