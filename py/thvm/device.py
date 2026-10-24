"""tinygrad's Device (device.py): DEFAULT is the device DEV names at import, CPU when unset, and
Device[name] that device, whose renderer lists the dtypes it computes."""
from __future__ import annotations

import os


class _Renderer:
    def __init__(self, device: str):
        self.device = device

    def supported_dtypes(self) -> list:
        from .dtypes import dtypes
        # Metal has no f64 (tinygrad renderer/cstyle.py MetalRenderer).
        return [d for d in dtypes.all if not (self.device == "METAL" and d == dtypes.float64)]


class _Dev:
    def __init__(self, device: str):
        self.device, self.renderer = device, _Renderer(device)

    def synchronize(self) -> None:
        pass


class _Device:
    DEFAULT: str = (os.environ.get("DEV", "") or "cpu").split(":", 1)[0].upper()

    def canonicalize(self, device: str | None) -> str:
        return (device or self.DEFAULT).split(":", 1)[0].upper()

    def __getitem__(self, device: str) -> _Dev:
        return _Dev(self.canonicalize(device))


Device = _Device()
