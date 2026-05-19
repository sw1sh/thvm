"""tinygrad-compatible Context env-var context manager.

`with Context(BEAM=1): ...` sets BEAM=1 for the block and restores on
exit.  Phase 0 aligned thvm on tinygrad's env-var names, so the same
knobs (BEAM, AUTOTUNE, NOOPT, ...) work.
"""
from __future__ import annotations

import os


class Context:
    def __init__(self, **kwargs):
        self.overrides = {k: str(v) for k, v in kwargs.items()}
        self.saved: dict[str, str | None] = {}

    def __enter__(self):
        for k, v in self.overrides.items():
            self.saved[k] = os.environ.get(k)
            os.environ[k] = v
        return self

    def __exit__(self, *args):
        for k, old in self.saved.items():
            if old is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = old
