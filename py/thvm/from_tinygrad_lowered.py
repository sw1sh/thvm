"""Rebuilds the kernel tinygrad lowers a Tensor to (the STORE under schedule_linear()'s
compute CALL) as thvm ranged IR, which thvm renders as C, without rangeify, and runs on
the CPU: a cross-check of thvm's codegen.  A PARAM's slot is its kernel argument, 0 the
output; the copy CALLs lead back to the inputs' host data."""
from __future__ import annotations

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
        # A multi-device sharding axis, refused rather than misrouted.
        raise NotImplementedError("AxisType.DEVICE range in a single-device import")
    # WEAK (placement uncommitted) and LOOP (a committed serial loop) both render
    # as thvm's serial AXIS_LOOP.
    return K.AXIS_LOOP


def _is_int_dtype(dt) -> bool:
    s = str(dt)
    return any(k in s for k in ("int", "uint", "long", "bool", "weakint"))


class _Lowerer:
    """Builds thvm ranged IR from one lowered STORE subtree, recording each PARAM's
    element count for the caller's buffers."""

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
                # A Variable is an ALU PARAM, a scalar argument rather than a
                # buffer (slot -1, uop/ops.py:1018-1021).
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
            # RANGE arg = (axis_id, ..., axis_type): the type is arg[-1], a
            # multi-part id the parts before it (uop/ops.py:501-509).
            at = _axis_type(u.arg[-1])
            t = h.range(axis_id=int(u.arg[0]), axis_type=at, extent=extent)
        elif op is Ops.INDEX:
            t = h.index_e(self.emit(u.src[0]), self.emit(u.src[1]))
        elif op is Ops.REDUCE:
            # REDUCE(value, range...), arg[0] the op: one reduce per range,
            # nested in src order.
            kind = K.REDUCE_MAX if u.arg[0] is Ops.MAX else K.REDUCE_SUM
            t = self.emit(u.src[0])
            for rng in u.src[1:]:
                self.emit(rng)               # build the AXIS_REDUCE range
                t = h.reduce(kind, (int(rng.arg[0]),), t)
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
            # The kernel IR has no max: a true IWHERE select stays right on a
            # non-finite branch (-inf pool masks).  Float only: CMPLT compares floats.
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
    """Each copy CALL's destination arg -> its source arg.  A copy CALL's body is
    STORE(PARAM_dst, PARAM_src) (schedule/prepare.py:217-222), and its destination, an
    arena view (schedule/memory.py:55-57), is the very UOp the kernel CALL reads."""
    Ops = _ops()
    links = {}
    for c in calls:
        body, args = c.src[0], c.src[1:]
        if (body.op is Ops.STORE and len(body.src) >= 2
                and all(x.op is Ops.PARAM for x in body.src[:2])):
            links[args[_param_slot(body.src[0])]] = args[_param_slot(body.src[1])]
    return links


def _kernel_call(sink):
    """The schedule's one compute CALL (its body a SINK), and every CALL."""
    Ops = _ops()
    calls = [u for u in sink.toposort() if u.op is Ops.CALL and u.src]
    kernels = [c for c in calls if c.src[0].op is Ops.SINK]
    if len(kernels) != 1:
        raise NotImplementedError(
            f"{len(kernels)} kernels; single-kernel only for now")
    return kernels[0], calls


def _slot_inputs(kernel, calls, params, py_data):
    """Each input PARAM slot's host data, through the copy links from the kernel CALL's
    src[1:] (slot order).  A slot without data raises: the renderer packs inputs densely
    by slot, so skipping one would shift the later ones."""
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
    """(got, ref, max_abs_err): thvm's C render of the single kernel a fresh build_fn()
    lowers to, against tinygrad's result.  build_fn runs twice, as schedule_linear()
    consumes its inputs; `opts` are (op, axis, arg) KOpts, such as (K.KOP_UPCAST, 0, 4),
    applied to the imported root first."""
    from tinygrad.helpers import Context
    # tinygrad's lowered IR is target-dependent (a REDUCE lowers differently
    # for CPU vs GPU), so it must lower for the CPU thvm renders for.
    with Context(DEV="CPU"):
        ref = np.asarray(build_fn().numpy(), dtype=np.float32)

        # The input data is read before schedule_linear zeroes the buffers.
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

    for opt in (opts or []):
        new_root = h.uop_dag_apply_kopt(root, tuple(int(x) for x in opt))
        if new_root is None:
            raise RuntimeError(f"KOpt {opt} bailed")
        root = new_root

    inputs = _slot_inputs(kernel, calls, low.params, py_data)

    return _run_cpu(h, root, ref, inputs)


def _run_cpu(h, root, ref, inputs):
    """Runs the kernel once through cpu_jit_run_root, which loops over its own axes."""
    shape = ref.shape if ref.shape else (1,)
    out = np.zeros(int(np.prod(shape)), dtype=np.float32)
    ins = [np.ascontiguousarray(inputs[s], dtype=np.float32) for s in sorted(inputs)]
    rc = h.run_kernel_c(root, out, ins)
    if rc == -1:
        raise RuntimeError("thvm's C renderer declined the kernel signature")
    if rc != 0:
        raise RuntimeError("compiling the rendered kernel failed "
                           "(THVM_CPU_JIT_VERBOSE=1 shows the compiler's errors)")
    got = out.reshape(shape)
    return got, ref, float(np.abs(got - ref).max())
