"""Phase-2A stubs for tinygrad surface area not yet implemented.

Kept here so `from thvm import Variable, TinyJit` succeeds for the
tinygrad-ported tests; tests that actually exercise these features
will fail at use-time, not import-time.
"""
from __future__ import annotations


class Variable:
    """tinygrad Variable stub -- symbolic shapes pending Phase 2B."""

    def __init__(self, name: str, lower: int, upper: int):
        self.name, self.lower, self.upper = name, lower, upper

    def bind(self, value: int):
        raise NotImplementedError("Variable.bind: symbolic dims pending Phase 2B")


def TinyJit(fn):
    """Phase-2A no-op JIT decorator -- calls through every time.

    tinygrad's TinyJit caches a kernel sequence after one warmup; the
    pass-through stub means the wrapped function runs every call.
    """
    return fn
