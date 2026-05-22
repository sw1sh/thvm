"""Phase-2A stubs for tinygrad surface area not yet implemented.

Kept here so `from thvm import Variable, TinyJit` succeeds for the
tinygrad-ported tests; tests that actually exercise these features
will fail at use-time, not import-time.
"""
from __future__ import annotations


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
    """Phase-2A no-op JIT decorator -- calls through every time.

    tinygrad's TinyJit caches a kernel sequence after one warmup; the
    pass-through stub means the wrapped function runs every call.
    """
    return fn
