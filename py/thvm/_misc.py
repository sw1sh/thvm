"""TinyJit + Variable for the tinygrad-ported surface.

Variable: a placeholder for symbolic shapes (bind() resolves to concrete
value; Phase 2B for kernel-sharing-across-bound-values).

TinyJit: capture the kernel sequence on the capture call, replay on
subsequent calls.  Inputs are passed as fresh Tensor arguments each call
(tinygrad's contract); on replay the captured graph's input buffers are
rebound to the current call's fresh inputs before dispatch -- the port
of tinygrad's input_replace (engine/jit.py:124-126), implemented in
src/jit/capture.c (jit_capture_set_inputs / jit_replay_with_inputs).
The captured ops then dispatch against the fresh buffers; the result
buffer is shared across calls and refilled by each replay.

Non-Tensor (symbolic) arguments are baked into kernel structure at
capture time -- thvm has no symbolic kernels (Phase 2B) -- so a replay
is only valid for the same symbolic-argument signature; a changed value
(e.g. a different bound token) re-captures rather than returning a stale
result.

Disabled when THVM_JIT=0; falls back to pass-through (re-runs fn each
call).  Use this when you suspect a capture/replay correctness issue.
"""
from __future__ import annotations

import os


class BoundVar(int):
    """A Variable bound to a concrete value.  Subclasses int so it slots
    into shrink/slice bounds + arithmetic transparently (thvm recompiles
    per concrete value rather than sharing a symbolic kernel), while
    exposing the tinygrad `.val` attribute callers read (`start_pos.val`).

    Also serves as thvm's `UOp` for the gpt2 single-token decode path:
    `isinstance(tokens, UOp)` is the test gpt2.forward uses to switch
    between consuming a prompt Tensor and a bound single-token decode."""

    @property
    def val(self) -> int:
        return int(self)


# thvm's stand-in for tinygrad's symbolic UOp on the Tensor surface.
# `Variable.bind` returns a BoundVar, which IS a UOp here -- so gpt2's
# `isinstance(tokens, UOp)` correctly identifies the bound single-token
# decode step.
UOp = BoundVar


class Variable:
    """tinygrad Variable.  Full symbolic shapes (one kernel reused across
    bound values) are pending Phase 2B; bind() resolves to the concrete
    value so the math is correct -- we recompile per value instead of
    sharing one symbolic kernel."""

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
    """Collect the buf_ids of the Tensor arguments, in a stable order
    (positional args first, then kwargs sorted by key) -- the same order
    tinygrad's _prepare_jit_inputs uses (engine/jit.py:227).  These are
    the JIT's declared inputs whose fresh buffers must be rebound on each
    replay.  Lazy import to avoid a cycle with .tensor."""
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
    """The signature of the call's NON-Tensor arguments -- ints, floats,
    and BoundVars (bound symbolic Variables).  thvm has no symbolic
    kernels: a BoundVar/int gets baked into kernel structure at capture
    (e.g. an embedding-row `shrink` offset or a kv-cache slice extent that
    depends on `start_pos`).  So a captured graph is only valid for the
    SAME symbolic values it was captured with; if any differ, replay would
    return the captured token's result (stale).  We key the capture on
    this signature and fall through to a fresh forward when it changes --
    correct, if not yet faster (the proper fix is symbolic var_vals
    rebinding, tinygrad engine/jit.py:161-164; thvm Phase 2B / the
    schedule/kvar.c symbolic-shape registry, not yet wired through the
    Python shrink path)."""
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
            # An unrecognised non-Tensor arg (e.g. an object): treat its
            # identity as part of the key so we never replay across a
            # changed one.
            sig.append(("o", id(v)))
    return tuple(sig)


def _jit_result_tensors(result):
    """Flatten the wrapped fn's return into its Tensor leaves (single
    Tensor, list/tuple, or dict values)."""
    from .tensor import Tensor
    if isinstance(result, Tensor):
        return [result]
    if isinstance(result, (list, tuple)):
        return [t for t in result if isinstance(t, Tensor)]
    if isinstance(result, dict):
        return [t for t in result.values() if isinstance(t, Tensor)]
    return []


def _jit_realize_result(result):
    """Realize every Tensor leaf of the result so its dispatches fire
    inside the capture span (thvm tensors are lazy)."""
    for t in _jit_result_tensors(result):
        t.realize()


def _jit_result_root(result):
    """A single result term to anchor the capture's liveness pass.  The
    common JIT case returns one Tensor; multi-tensor returns are all
    realized above (so their output buffers exist) and the first anchors
    the liveness root -- enough to keep the captured graph alive."""
    leaves = _jit_result_tensors(result)
    if leaves:
        return int(leaves[0].term)
    return 0


def TinyJit(fn):
    """Decorator: capture the wrapped function's kernel sequence on first
    call, replay on each subsequent call.  Inputs are passed as fresh
    Tensor arguments each call (tinygrad's contract); the captured graph's
    input buffers are rebound to the current call's fresh inputs before
    every replay (the input_replace port, jit/capture.c).  The returned
    Tensor is captured by buf_id and refilled by each replay.

    Disable via THVM_JIT=0 (returns the bare function for A/B
    comparison).
    """
    if os.environ.get("THVM_JIT", "1") == "0":
        return fn

    from .thvm import Thvm
    _TH = Thvm()

    # cnt-state machine mirroring tinygrad's TinyJit (engine/jit.py:268):
    #   cnt 0 -> run fn (warmup, no capture; first call stabilises the
    #            graph and lets any one-time init/alloc settle)
    #   cnt 1 -> run fn AND capture the dispatch sequence
    #   cnt>=2 -> replay the captured sequence with the fresh inputs
    # `sig` is the captured call's symbolic-argument signature; a replay is
    # only valid for the same signature (thvm bakes symbolic values into
    # kernels), otherwise we re-capture for the new signature.
    state = {"slot": 0, "result": None, "cnt": 0, "sig": None}

    def _capture(args, kwargs):
        # jit_begin arms the capture buffer; the wrapped fn's normal
        # realize/dispatch path records every dispatch through uop_kernel's
        # jit_capture_record.  Record the input tensors' buf_ids BEFORE the
        # fn runs so the rebind baseline matches the buffers the captured
        # ops will reference.
        if state["slot"]:
            _TH.jit_drop(state["slot"])
            state["slot"] = 0
        slot = _TH.jit_begin()
        if slot == 0:
            # Capture-slot exhaustion: stay in pass-through.
            result = fn(*args, **kwargs)
            _jit_realize_result(result)
            return result, False
        input_ids = _jit_input_buf_ids(_TH, args, kwargs)
        result = None
        try:
            result = fn(*args, **kwargs)
            # Force the result's dispatches to fire INSIDE the capture span
            # -- thvm tensors are lazy, so without this realize the
            # `+`/matmul/etc. only builds a term and nothing gets recorded
            # (tinygrad realizes params here too, jit.py:284).
            _jit_realize_result(result)
        finally:
            # End capture WITH the result root so the replay-skip liveness
            # pass (jit_capture_finalize) knows the result buffer is
            # observably needed -- otherwise EVERY dispatch gets marked
            # replay_skip=1 and replays do zero work.
            root = _jit_result_root(result)
            if root:
                _TH.jit_end_with_result(root)
            else:
                _TH.jit_end()
        # Declare the input baseline for replay rebinding.
        _TH.jit_set_inputs(slot, input_ids)
        state["slot"] = slot
        state["result"] = result
        state["sig"] = _jit_symbolic_sig(args, kwargs)
        return result, True

    def wrapped(*args, **kwargs):
        if state["cnt"] == 0:
            # Warmup: just run, no capture.
            result = fn(*args, **kwargs)
            _jit_realize_result(result)
            state["cnt"] = 1
            return result
        if state["cnt"] == 1:
            result, captured = _capture(args, kwargs)
            if captured:
                state["cnt"] = 2
            return result
        # cnt >= 2.  Replay only when the symbolic-argument signature
        # matches the capture; thvm bakes symbolic values into kernels, so
        # a changed value (e.g. GPT2's per-token embedding-shrink offset or
        # the growing kv-cache slice driven by start_pos) needs a fresh
        # graph.  Re-capture for the new signature -> correct, token-
        # identical to JIT=0 (the speedup case is a stable signature with
        # changing Tensor-buffer inputs, e.g. a fixed-shape training step).
        if state["sig"] != _jit_symbolic_sig(args, kwargs):
            result, _ = _capture(args, kwargs)
            return result
        # Rebind the captured input buffers to this call's fresh input
        # tensors, then replay.  No graph build, no materialize, no kernel
        # emit -- just re-dispatch.
        new_ids = _jit_input_buf_ids(_TH, args, kwargs)
        _TH.jit_replay_with_inputs(state["slot"], new_ids)
        return state["result"]

    return wrapped
