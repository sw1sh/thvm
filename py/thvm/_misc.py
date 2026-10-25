"""TinyJit and Variable.  thvm has no symbolic kernels: a bound Variable is its int
value, and a capture bakes in the call's non-Tensor arguments, so a call with others
captures again."""
from __future__ import annotations

import functools
import types

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


class JitError(Exception):
    """tinygrad's JitError (engine/jit.py:39)."""


# The TinyJit capturing now (tinygrad engine/realize.py's `capturing`).
capturing: list = []


def _ret_tensors(ret):
    from .tensor import Tensor
    if isinstance(ret, Tensor):
        return [ret]
    if isinstance(ret, (tuple, list, dict)):
        return [t for x in (ret.values() if isinstance(ret, dict) else ret) for t in _ret_tensors(x)]
    return []


def _ret_map(ret, f):
    from .tensor import Tensor
    if isinstance(ret, Tensor):
        return f(ret)
    if isinstance(ret, (tuple, list)):
        return type(ret)(_ret_map(x, f) for x in ret)
    if isinstance(ret, dict):
        return {k: _ret_map(v, f) for k, v in ret.items()}
    return ret


def _check_no_non_tensor_return(ret):
    """tinygrad engine/jit.py:41-46."""
    from .tensor import Tensor
    if ret is None or isinstance(ret, Tensor):
        return
    if isinstance(ret, (tuple, list, dict)):
        for item in (ret.values() if isinstance(ret, dict) else ret):
            _check_no_non_tensor_return(item)
        return
    raise JitError(f"JIT return contains non-Tensor value of type {type(ret).__name__}")


def _prepare_jit_inputs(args, kwargs):
    """tinygrad's _prepare_jit_inputs (engine/jit.py:82-100): the Tensor arguments, positional then
    by keyword, then those one container deep, realized unless a view of a realized buffer; their
    buf_ids, their names, and per input the (shape, dtype, views) a replay must match, as a capture
    bakes in each input's view.  Also the tensors themselves."""
    from .tensor import Tensor, _TH, _realize_now
    named = [(n, t) for n, t in list(enumerate(args)) + sorted(kwargs.items()) if t.__class__ is Tensor]
    names, tensors = [n for n, _ in named], [t for _, t in named]
    for x in args + tuple(kwargs.values()):
        it = x if isinstance(x, (tuple, list)) else x.values() if isinstance(x, dict) else []
        tensors += [t for t in it if t.__class__ is Tensor and not any(t is y for y in tensors)]
    views = [_TH.view_chain(t.term) for t in tensors]
    if lazy := [t for t, v in zip(tensors, views) if v is None]:
        _realize_now(*lazy, share=True)
    bases = [v[0] if v else t.term for t, v in zip(tensors, views)]
    ids = [_TH.ten_get_buf_id(b) for b in bases]
    if 0 in ids:
        raise JitError("JIT inputs must be real buffers; use .clone()")
    if len(set(ids)) != len(ids):
        raise JitError("duplicate inputs to JIT")
    info = [(t._shape, t._dtype, ("view", *v[1]) if v else _TH.ten_view_words(t.term)) for t, v in zip(tensors, views)]
    return ids, names, info, tensors


def _jit_symbolic_sig(args, kwargs):
    """The call's non-Tensor arguments, which a capture bakes into its kernels where
    tinygrad binds them per call as var_vals (engine/jit.py:97)."""
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


class _JitCall:
    """A captured op as test/helpers.py's assert_jit_cache_len reads a CALL: its export row."""

    def __init__(self, row):
        self.arg = row

    @property
    def without_after(self):
        return self


class CapturedJit:
    """tinygrad's CapturedJit (engine/jit.py:49-80) over a capture slot: `_linear.src` holds an
    entry per op a replay runs, and a call replays them on that call's input buffers.  Where fxn
    returned one of its inputs, a replay returns that call's tensor, whose buffer it wrote."""

    def __init__(self, ret, slot: int, expected_names, expected_input_info, inputs=()):
        self.ret, self.slot = ret, slot
        self.expected_names, self.expected_input_info = expected_names, expected_input_info
        self._ret_inputs = {id(t): k for t in _ret_tensors(ret) for k, x in enumerate(inputs) if t is x}

    @property
    def _linear(self):
        from .tensor import _TH
        return types.SimpleNamespace(src=[_JitCall(r) for r in _TH.jit_ops(self.slot) if not r[14]])

    def __call__(self, input_ids, inputs=()):
        from .tensor import _TH
        _TH.jit_replay_with_inputs(self.slot, input_ids)
        if not self._ret_inputs:
            return self.ret
        return _ret_map(self.ret, lambda t: inputs[self._ret_inputs[id(t)]] if id(t) in self._ret_inputs else t)

    def free_intermediates(self):
        """tinygrad's free_intermediates (engine/jit.py:80): the capture's buffers are released,
        and the next call captures again."""
        from .tensor import _TH
        _TH.jit_drop(self.slot)
        self.slot = 0
        _TH.reclaim()

    def __del__(self):
        try:
            from .tensor import _TH
            _TH.jit_drop(self.slot)
        except Exception:
            pass


class _TinyJit:
    """tinygrad's TinyJit (engine/jit.py:102-176): the first call runs fxn, the second captures
    its dispatches, later ones replay them on their Tensor arguments.  A capture bakes in the
    call's non-Tensor arguments, so a call with others captures again."""

    def __init__(self, fxn, captured=None, prune=False):
        assert fxn or captured, "need either a function or a CapturedJit"
        self.fxn, self.captured, self.prune = fxn, captured, prune
        self.cnt = 2 if fxn is None else 0
        self._sig = None

    def reset(self):
        assert self.fxn is not None, "can't reset without function"
        self.cnt, self.captured = 0, None

    def __get__(self, obj, objtype):
        return functools.partial(self.__call__, obj)

    def __call__(self, *args, **kwargs):
        from .tensor import Tensor, _TH, _realize_now, _realize_pending
        ids, names, info, inputs = _prepare_jit_inputs(args, kwargs)
        sig = _jit_symbolic_sig(args, kwargs)
        if not getenv("JIT", 1) or self.cnt == 0:
            ret = self.fxn(*args, **kwargs)
            if ts := _ret_tensors(ret):
                _realize_now(*ts, share=True)
        elif self.cnt == 1 or (self.fxn is not None and (sig != self._sig or self.captured.slot == 0)):
            if capturing:
                raise RuntimeError(f"having TinyJit inside another TinyJit is not supported {len(capturing)=} {capturing=}")
            # An assign from outside fn realizes now, so the capture never records it.
            _realize_pending()
            self.captured = None
            slot = _TH.jit_begin()
            if slot == 0:
                # No capture slot is free: run fxn uncaptured, and capture on a later call.
                ret = self.fxn(*args, **kwargs)
                if ts := _ret_tensors(ret):
                    _realize_now(*ts, share=True)
                return ret
            ret = None
            capturing.append(self)
            try:
                ret = self.fxn(*args, **kwargs)
                # A lazy result records nothing until realized (tinygrad engine/jit.py:140).
                if ts := _ret_tensors(ret):
                    _realize_now(*ts, share=True)
            finally:
                capturing.clear()
                # Every result is a root, whose buffer the replay's packing never reuses.
                _TH.jit_end([t.term for t in _ret_tensors(ret)])
            captured = CapturedJit(ret, slot, names, info, inputs)
            if _TH.jit_op_count(slot) == 0:
                raise JitError("didn't JIT anything!")
            _check_no_non_tensor_return(ret)
            _TH.jit_set_inputs(slot, ids)
            if self.prune:
                _TH.jit_prune(slot)
            self.captured, self._sig = captured, sig
        else:
            if self.captured.expected_names != names:
                raise JitError(f"args mismatch in JIT: {self.captured.expected_names=} != {names}")
            if self.captured.expected_input_info != info:
                raise JitError(f"args mismatch in JIT: {self.captured.expected_input_info=} != {info=}")
            ret = self.captured(ids, inputs)
        self.cnt += 1
        return ret


def TinyJit(fxn=None, **kwargs):
    """tinygrad's TinyJit (engine/jit.py:178-183), as @TinyJit and as @TinyJit(prune=True)."""
    return (lambda f: _TinyJit(f, **kwargs)) if fxn is None else _TinyJit(fxn, **kwargs)
