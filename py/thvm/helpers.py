"""tinygrad-compatible helpers module (Phase 2A subset)."""
from __future__ import annotations

import os
import pathlib
import sys
import time
import urllib.request
from typing import Any


def getenv(key: str, default: Any = 0):
    """Read an env var and cast to the default's type (tinygrad-compatible)."""
    return type(default)(os.environ.get(key, default))


CI: bool = os.environ.get("CI", "") != ""

# tinygrad debug/JIT env knobs (helpers.DEBUG / helpers.JIT).  GPT2 reads
# both: DEBUG gates the per-step timing print, JIT gates the symbolic
# single-token forward_jit path.
DEBUG: int = getenv("DEBUG", 0)
JIT: int = getenv("JIT", 1)


class Timing:
    """tinygrad's Timing context manager: prints `prefix` + elapsed (and an
    optional on_exit suffix) on __exit__ when enabled."""

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
    """tinygrad's fetch: download `url` to a content-addressed cache file
    and return its local path (cached on subsequent calls).  Shares
    tinygrad's cache dir so already-downloaded weights are reused."""
    import hashlib
    if name is not None and (isinstance(name, pathlib.Path) or "/" in str(name)):
        fp = pathlib.Path(name)
    else:
        cache = pathlib.Path(
            os.environ.get("XDG_CACHE_HOME",
                           pathlib.Path.home() / "Library" / "Caches"
                           if sys.platform == "darwin"
                           else pathlib.Path.home() / ".cache")) / "tinygrad" / "downloads"
        if subdir:
            cache = cache / subdir
        fname = name if name is not None else hashlib.md5(url.encode()).hexdigest()
        fp = cache / fname
    if not fp.is_file() or not allow_caching:
        fp.parent.mkdir(parents=True, exist_ok=True)
        tmp = fp.parent / (fp.name + ".tmp")
        print(f"fetching {url}")
        req = urllib.request.Request(url, headers={"User-Agent": "thvm"})
        with urllib.request.urlopen(req) as r, open(tmp, "wb") as f:
            total = int(r.headers.get("content-length", 0))
            done = 0
            while chunk := r.read(16384):
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
        # Cross-step buffer reclaim: free every backend buffer not
        # reachable from a live (pinned) Tensor.  beautiful_mnist's loop
        # calls GlobalCounters.reset() once per step, so this keeps the
        # eager no-JIT device memory flat.  Lazy import avoids a cycle.
        try:
            from .tensor import _TH
            _TH.reclaim()
        except Exception:
            pass


def function(fn):
    """tinygrad's @function marks a call-graph boundary; a transparent
    passthrough here (no capture)."""
    return fn
