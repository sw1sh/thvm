"""A subset of tinygrad's helpers.py."""
from __future__ import annotations

import contextlib
import gzip
import os
import pathlib
import sys
import time
import urllib.request
from typing import Any

# tinygrad helpers.py:17
OSX, WIN = sys.platform == "darwin", sys.platform == "win32"


def argfix(*x):
    """A single tuple or list argument stands for the arguments themselves
    (tinygrad helpers.py:24-28)."""
    if x and x[0].__class__ in (tuple, list):
        if len(x) != 1:
            raise ValueError(f"bad arg {x}")
        return tuple(x[0])
    return x


def getenv(key: str, default: Any = 0):
    """The env var cast to the default's type (tinygrad helpers.py:162)."""
    return type(default)(os.environ.get(key, default))


class Context(contextlib.ContextDecorator):
    """tinygrad's Context (helpers.py:169-177), which also exports each key to the
    environment, where the C runtime reads its knobs."""

    def __init__(self, **kwargs):
        self.kwargs = kwargs

    def _recreate_cm(self):
        # ContextDecorator otherwise reuses self, so a recursive call would
        # overwrite the saved state.
        return Context(**self.kwargs)

    def __enter__(self):
        self.saved_vars = {k: ContextVar._cache[k].value
                           for k in self.kwargs if k in ContextVar._cache}
        self.saved_env = {k: os.environ.get(k) for k in self.kwargs}
        for k, v in self.kwargs.items():
            if k in ContextVar._cache:
                ContextVar._cache[k].value = v
            os.environ[k] = str(v)
        return self

    def __exit__(self, *exc):
        for k, v in self.saved_vars.items():
            ContextVar._cache[k].value = v
        for k, old in self.saved_env.items():
            if old is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = old


class ContextVar:
    """tinygrad's ContextVar (helpers.py:179-191): a knob seeded from the environment
    that Context overrides for a block."""

    _cache: dict[str, "ContextVar"] = {}

    def __init__(self, key: str, default_value: Any):
        if key in ContextVar._cache:
            raise RuntimeError(f"attempt to recreate ContextVar {key}")
        ContextVar._cache[key] = self
        self.value, self.key = getenv(key, default_value), key

    def __bool__(self):
        return bool(self.value)

    def __eq__(self, x):
        return self.value == x

    def __ge__(self, x):
        return self.value >= x

    def __gt__(self, x):
        return self.value > x

    def __lt__(self, x):
        return self.value < x


# tinygrad helpers.py:240: BatchNorm uses batch statistics and the optimizers
# step only while it is set.
TRAINING = ContextVar("TRAINING", 0)

# tinygrad helpers.py:288, accepted and ignored: nn.Embedding is the same either way.
USE_ATOMICS = ContextVar("USE_ATOMICS", 0)

# tinygrad helpers.py:236 and 238; bench/gpt2 reads DEBUG for its timing print and
# JIT to pick forward_jit.
DEBUG: int = getenv("DEBUG", 0)
JIT: int = getenv("JIT", 1)


class Timing:
    """tinygrad's Timing (helpers.py:313-318)."""

    def __init__(self, prefix: str = "", on_exit=None, enabled: bool = True):
        self.prefix, self.on_exit, self.enabled = prefix, on_exit, enabled

    def __enter__(self):
        self.st = time.perf_counter_ns()
        return self

    def __exit__(self, *exc):
        self.et = time.perf_counter_ns() - self.st
        if self.enabled:
            suffix = self.on_exit(self.et) if self.on_exit else ""
            print(f"{self.prefix}{self.et*1e-6:.2f} ms" + suffix)


def fetch(url: str, name: "pathlib.Path | str | None" = None,
          subdir: str | None = None, gunzip: bool = False,
          allow_caching: bool = True, **_) -> pathlib.Path:
    """tinygrad's fetch (helpers.py:469-507): the local path of `url`, downloaded once
    into tinygrad's own cache dir, so what tinygrad already fetched is reused."""
    import hashlib
    if name is not None and (isinstance(name, pathlib.Path) or "/" in str(name)):
        fp = pathlib.Path(name)
    else:
        cache = pathlib.Path(
            os.environ.get("XDG_CACHE_HOME",
                           pathlib.Path.home() / "Library" / "Caches"
                           if OSX
                           else pathlib.Path.home() / ".cache")) / "tinygrad" / "downloads"
        if subdir:
            cache = cache / subdir
        fname = name if name is not None else hashlib.md5(url.encode()).hexdigest()
        fp = cache / (fname + (".gunzip" if gunzip else ""))
    if not fp.is_file() or not allow_caching:
        fp.parent.mkdir(parents=True, exist_ok=True)
        tmp = fp.parent / (fp.name + ".tmp")
        print(f"fetching {url}")
        req = urllib.request.Request(url, headers={"User-Agent": "thvm"})
        with urllib.request.urlopen(req) as r, open(tmp, "wb") as f:
            src = gzip.GzipFile(fileobj=r) if gunzip else r
            total = 0 if gunzip else int(r.headers.get("content-length", 0))
            done = 0
            while chunk := src.read(16384):
                f.write(chunk)
                done += len(chunk)
                if total:
                    sys.stderr.write(f"\r  {done/1e6:.1f}/{total/1e6:.1f} MB")
                    sys.stderr.flush()
        sys.stderr.write("\n")
        tmp.rename(fp)
    return fp


_COLORS = {"red": 31, "green": 32, "yellow": 33, "blue": 34,
           "magenta": 35, "cyan": 36, "white": 37}


def colored(s, color: str | None = None, background=False):
    if color is None or not sys.stdout.isatty():
        return str(s)
    code = _COLORS.get(color, 37) + (10 if background else 0)
    return f"\033[{code}m{s}\033[0m"


class _Trange:
    """tqdm's iteration and set_description, the latter a status line on stderr."""
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
    """tinygrad's GlobalCounters (helpers.py:301-309), which thvm never increments."""
    global_ops = 0
    global_mem = 0
    time_sum_s = 0.0
    kernel_count = 0

    @classmethod
    def reset(cls):
        cls.global_ops = cls.global_mem = cls.kernel_count = 0
        cls.time_sum_s = 0.0
        # A step boundary frees what died since the last reclaim, tightening the peak
        # of a loop that calls it.  Imported here: tensor imports this module.
        from .tensor import _TH
        _TH.reclaim_if_due(force=True)


def function(fn):
    """tinygrad's @function (function.py:107-113), here a passthrough that captures
    no call graph."""
    return fn
