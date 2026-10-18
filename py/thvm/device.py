"""tinygrad's Device: DEFAULT is the device DEV names at import, CPU when unset."""
from __future__ import annotations

import os


class Device:
    DEFAULT: str = (os.environ.get("DEV", "") or "cpu").split(":", 1)[0].upper()
