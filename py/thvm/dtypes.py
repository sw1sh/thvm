"""tinygrad-compatible dtypes namespace.

Phase 2A: float32 + int32 only.  Each DType knows its byte size, the
numpy dtype for host I/O, and the thvm dtype-id the bridge uses.
"""
from __future__ import annotations

import numpy as np

from .thvm import K as _K


class DType:
    __slots__ = ("name", "itemsize", "np_dtype", "thvm_id")

    def __init__(self, name: str, itemsize: int, np_dtype, thvm_id: int):
        self.name = name
        self.itemsize = itemsize
        self.np_dtype = np_dtype
        self.thvm_id = thvm_id

    def __repr__(self) -> str:
        return f"dtypes.{self.name}"

    def __eq__(self, other) -> bool:
        return isinstance(other, DType) and other.name == self.name

    def __hash__(self) -> int:
        return hash(self.name)


class dtypes:
    """tinygrad-compatible namespace: dtypes.float, dtypes.int, ..."""
    float32 = DType("float32", 4, np.float32, _K.FP32)
    int32   = DType("int32",   4, np.int32,   _K.INT32)

    # tinygrad-canonical aliases
    float = float32
    int   = int32
    default_float = float32
    default_int   = int32

    @staticmethod
    def is_float(dt: DType) -> bool:
        return dt.thvm_id == _K.FP32

    @staticmethod
    def is_int(dt: DType) -> bool:
        return dt.thvm_id == _K.INT32
