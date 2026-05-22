"""tinygrad-compatible helpers module (Phase 2A subset)."""
from __future__ import annotations

import os
import sys
import time
from typing import Any


def getenv(key: str, default: Any = 0):
    """Read an env var and cast to the default's type (tinygrad-compatible)."""
    return type(default)(os.environ.get(key, default))


CI: bool = os.environ.get("CI", "") != ""


_COLORS = {"red": 31, "green": 32, "yellow": 33, "blue": 34,
           "magenta": 35, "cyan": 36, "white": 37}


def colored(s, color: str | None = None, background=False):
    if color is None or not sys.stdout.isatty():
        return str(s)
    code = _COLORS.get(color, 37) + (10 if background else 0)
    return f"\033[{code}m{s}\033[0m"


class _Trange:
    """Minimal tqdm-like wrapper: iterable + set_description (writes a
    carriage-return status line to stderr)."""
    def __init__(self, iterable):
        self._it = iterable
        self._t0 = time.time()

    def __iter__(self):
        for x in self._it:
            yield x

    def set_description(self, desc: str):
        sys.stderr.write("\r" + desc + " " * 4)
        sys.stderr.flush()


def trange(n, **_):
    return _Trange(range(n))


def tqdm(iterable, **_):
    return _Trange(iterable)


class GlobalCounters:
    """tinygrad's global op/mem counters.  thvm doesn't track FLOPs the
    same way; reset() zeroes them so the training loop's per-step
    bookkeeping has somewhere to write."""
    global_ops = 0
    global_mem = 0
    time_sum_s = 0.0
    kernel_count = 0

    @classmethod
    def reset(cls):
        cls.global_ops = cls.global_mem = cls.kernel_count = 0
        cls.time_sum_s = 0.0


def function(fn):
    """tinygrad's @function marks a call-graph boundary; a transparent
    passthrough here (no capture)."""
    return fn
