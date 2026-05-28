"""TinyJit + Variable for the tinygrad-ported surface.

Variable: a placeholder for symbolic shapes (bind() resolves to concrete
value; Phase 2B for kernel-sharing-across-bound-values).

TinyJit: capture the kernel sequence on first call, replay on subsequent.
Mirrors tinygrad's pattern -- inputs must be PRE-ALLOCATED stable
Tensors that the wrapped function reads through, with .assign() used to
write fresh data between calls.  The captured ops dispatch the same
KernelEntries against the same buf_ids each replay; the input buffer's
new contents drive the new outputs.

Disabled when THVM_JIT=0; falls back to pass-through (re-runs fn each
call).  Use this when you suspect a capture/replay correctness issue.
"""
from __future__ import annotations

import os


class Variable:
    """tinygrad Variable.  Full symbolic shapes (one kernel reused across
    bound values) are pending Phase 2B; bind() resolves to the concrete
    value so the math is correct -- we recompile per value instead of
    sharing one symbolic kernel."""

    def __init__(self, name: str, lower: int, upper: int):
        self.name, self.lower, self.upper = name, lower, upper

    def bind(self, value: int) -> int:
        if not (self.lower <= value <= self.upper):
            raise ValueError(f"{value} out of [{self.lower},{self.upper}]")
        return int(value)


def TinyJit(fn):
    """Decorator: capture the wrapped function's kernel sequence on first
    call, replay on each subsequent call.  The wrapped function must read
    its inputs through PRE-ALLOCATED stable Tensors (e.g. via .assign of
    fresh data between calls); the returned Tensor is captured by buf_id
    and refilled by each replay.

    Disable via THVM_JIT=0 (returns the bare function for A/B
    comparison).
    """
    if os.environ.get("THVM_JIT", "1") == "0":
        return fn

    from .thvm import Thvm
    _TH = Thvm()

    state = {"slot": 0, "result": None, "captured": False}

    def wrapped(*args, **kwargs):
        if not state["captured"]:
            # First call: capture.  jit_begin arms the capture buffer;
            # the wrapped fn's normal realize/dispatch path records every
            # dispatch through uop_kernel's jit_capture_record.
            slot = _TH.jit_begin()
            if slot == 0:
                # Capture-slot exhaustion: just run pass-through.
                return fn(*args, **kwargs)
            result = None
            try:
                result = fn(*args, **kwargs)
            finally:
                # End capture WITH the result root so the replay-skip
                # liveness pass (jit_capture_finalize) knows the result
                # buffer is observably needed -- otherwise EVERY dispatch
                # gets marked replay_skip=1 and replays do zero work.
                if result is not None and hasattr(result, "term"):
                    _TH.jit_end_with_result(int(result.term))
                else:
                    _TH.jit_end()
            state["slot"] = slot
            state["result"] = result
            state["captured"] = True
            return result
        # Subsequent calls: replay the captured sequence.  No graph build,
        # no materialize, no kernel emit -- just re-dispatch.
        _TH.jit_replay(state["slot"])
        return state["result"]

    return wrapped
