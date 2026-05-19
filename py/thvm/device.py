"""Device selection -- mirrors tinygrad's Device namespace.

Phase 0 of the env-var rename aligned thvm on tinygrad's `DEV` var,
so `Device.DEFAULT` just reflects what `DEV` picked at process start.
"""
from __future__ import annotations

import os


class Device:
    DEFAULT: str = (os.environ.get("DEV", "") or "cpu").split(":", 1)[0].upper()
