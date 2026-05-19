"""tinygrad-compatible helpers module (Phase 2A subset)."""
from __future__ import annotations

import os
from typing import Any


def getenv(key: str, default: Any = 0):
    """Read an env var and cast to the default's type (tinygrad-compatible)."""
    return type(default)(os.environ.get(key, default))


CI: bool = os.environ.get("CI", "") != ""
