"""TinyJit and Variable.  thvm has no symbolic kernels: a bound Variable is its int
value, and a capture bakes in the call's non-Tensor arguments, so a call with others
captures again."""
from __future__ import annotations

from .helpers import getenv


class BoundVar(int):
    """A bound Variable: an int, so it serves as a shrink bound and in arithmetic, with
    tinygrad's `.val`; it is also thvm's UOp, which gpt2's forward tests tokens for."""

    @property
    def val(self) -> int:
        return int(self)


UOp = BoundVar


class Variable:
    """tinygrad's Variable (tinygrad/__init__.py:9); bind() returns the value as a
    BoundVar, so each value compiles its own kernels."""

    def __init__(self, name: str, lower: int, upper: int):
        self.name, self.lower, self.upper = name, lower, upper

    @property
    def val(self) -> int:
        raise RuntimeError(f"unbound Variable {self.name} has no value")

    def bind(self, value: int) -> "BoundVar":
        if not (self.lower <= value <= self.upper):
            raise ValueError(f"{value} out of [{self.lower},{self.upper}]")
        return BoundVar(value)


def _jit_input_buf_ids(_TH, args, kwargs):
    """The Tensor arguments' buf_ids, positional then by keyword as tinygrad orders JIT
    inputs (engine/jit.py:85); each is realized first, and each replay rebinds them."""
    from .tensor import Tensor
    items = list(enumerate(args)) + sorted(
        (k, v) for k, v in kwargs.items())
    ids = []
    for _, t in items:
        if isinstance(t, Tensor):
            t.realize()
            ids.append(int(_TH.ten_get_buf_id(t.term)))
    return ids


def _jit_symbolic_sig(args, kwargs):
    """The call's non-Tensor arguments, which a capture bakes into its kernels where
    tinygrad binds them per call as var_vals (engine/jit.py:73)."""
    from .tensor import Tensor
    sig = []
    for v in list(args) + [kwargs[k] for k in sorted(kwargs)]:
        if isinstance(v, Tensor):
            continue
        if isinstance(v, BoundVar):
            sig.append(("v", int(v)))
        elif isinstance(v, (int, float, bool, str)):
            sig.append(("c", v))
        else:
            # Any other object keys by identity, so a changed one never replays.
            sig.append(("o", id(v)))
    return tuple(sig)


def _jit_result_tensors(result):
    from .tensor import Tensor
    if isinstance(result, Tensor):
        return [result]
    if isinstance(result, (list, tuple)):
        return [t for t in result if isinstance(t, Tensor)]
    if isinstance(result, dict):
        return [t for t in result.values() if isinstance(t, Tensor)]
    return []


def _jit_realize_result(result):
    """Realizes the result's Tensors, so their dispatches fire during a capture."""
    for t in _jit_result_tensors(result):
        t.realize()


def TinyJit(fn):
    """tinygrad's TinyJit (engine/jit.py:126-178): the second call captures fn's
    dispatches, later ones replay them on their Tensor arguments; JIT=0 returns fn."""
    if getenv("JIT", 1) == 0:
        return fn

    from .thvm import Thvm
    _TH = Thvm()

    # tinygrad's cnt (engine/jit.py:128-177): 0 runs fn, 1 captures, 2+ replays.
    state = {"slot": 0, "result": None, "cnt": 0, "sig": None}

    def _capture(args, kwargs):
        # The input buf_ids are read before fn runs, as the buffers the captured
        # ops reference are the baseline a replay rebinds.
        if state["slot"]:
            _TH.jit_drop(state["slot"])
            state["slot"] = 0
        from .tensor import _realize_pending
        _realize_pending()
        slot = _TH.jit_begin()
        if slot == 0:
            # No capture slot is free: run fn uncaptured.
            result = fn(*args, **kwargs)
            _jit_realize_result(result)
            return result, False
        input_ids = _jit_input_buf_ids(_TH, args, kwargs)
        result = None
        try:
            result = fn(*args, **kwargs)
            # A lazy result records nothing until realized (tinygrad engine/jit.py:142).
            _jit_realize_result(result)
        finally:
            # Every result is a root: jit_capture_finalize replays only the
            # dispatches a root needs.
            _TH.jit_end([t.term for t in _jit_result_tensors(result)])
        _TH.jit_set_inputs(slot, input_ids)
        state["slot"] = slot
        state["result"] = result
        state["sig"] = _jit_symbolic_sig(args, kwargs)
        return result, True

    def wrapped(*args, **kwargs):
        if state["cnt"] == 0:
            result = fn(*args, **kwargs)
            _jit_realize_result(result)
            state["cnt"] = 1
            return result
        if state["cnt"] == 1:
            result, captured = _capture(args, kwargs)
            if captured:
                state["cnt"] = 2
            return result
        if state["sig"] != _jit_symbolic_sig(args, kwargs):
            result, _ = _capture(args, kwargs)
            return result
        new_ids = _jit_input_buf_ids(_TH, args, kwargs)
        _TH.jit_replay_with_inputs(state["slot"], new_ids)
        return state["result"]

    return wrapped
