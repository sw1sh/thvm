"""tinygrad's dtypes (dtype.py) over thvm's DT_* table: each DType carries its thvm id, and
promotion is the C runtime's least_upper_dtype (src/nn/promote.c)."""
from __future__ import annotations

import ctypes
import math

import numpy as np

from .thvm import _lib

_lib.py_dtype_id.restype = ctypes.c_uint32
_lib.py_dtype_id.argtypes = [ctypes.c_char_p]
_lib.py_dtype_promote.restype = ctypes.c_uint32
_lib.py_dtype_promote.argtypes = [ctypes.c_uint32, ctypes.c_uint32]


class DType:
    """A dtype by tinygrad's name and (priority, bitsize) order; np_dtype is None where numpy
    has no type (bf16, fp8), whose host values go through f32."""
    __slots__ = ("priority", "bitsize", "name", "fmt", "np_dtype", "thvm_id")

    def __init__(self, priority: int, bitsize: int, name: str, fmt: str | None, np_dtype):
        self.priority, self.bitsize, self.name, self.fmt = priority, bitsize, name, fmt
        self.np_dtype = np_dtype
        self.thvm_id = int(_lib.py_dtype_id(name.encode()))

    @property
    def itemsize(self) -> int: return (self.bitsize + 7) // 8
    @property
    def scalar(self) -> "DType": return self
    @property
    def min(self): return dtypes.min(self)
    @property
    def max(self): return dtypes.max(self)

    def __repr__(self) -> str: return f"dtypes.{self.name}"
    def __eq__(self, other) -> bool: return isinstance(other, DType) and other.name == self.name
    def __hash__(self) -> int: return hash(self.name)
    def __lt__(self, o: "DType") -> bool: return (self.priority, self.bitsize) < (o.priority, o.bitsize)


class dtypes:
    bool = DType(0, 1, "bool", "?", np.bool_)
    i8 = DType(1, 8, "i8", "b", np.int8)
    u8 = DType(2, 8, "u8", "B", np.uint8)
    i16 = DType(3, 16, "i16", "h", np.int16)
    u16 = DType(4, 16, "u16", "H", np.uint16)
    i32 = DType(5, 32, "i32", "i", np.int32)
    u32 = DType(6, 32, "u32", "I", np.uint32)
    i64 = DType(7, 64, "i64", "q", np.int64)
    u64 = DType(8, 64, "u64", "Q", np.uint64)
    fp8e4m3 = DType(10, 8, "fp8e4m3", None, None)
    fp8e5m2 = DType(11, 8, "fp8e5m2", None, None)
    f16 = DType(12, 16, "f16", "e", np.float16)
    bf16 = DType(13, 16, "bf16", None, None)
    f32 = DType(14, 32, "f32", "f", np.float32)
    f64 = DType(15, 64, "f64", "d", np.float64)

    float16 = half = f16; bfloat16 = bf16; float32 = float = f32; float64 = double = f64  # noqa: E702
    uint8 = uchar = u8; uint16 = ushort = u16; uint32 = uint = u32; uint64 = ulong = u64  # noqa: E702
    int8 = char = i8; int16 = short = i16; int32 = int = i32; int64 = long = i64  # noqa: E702
    default_float = f32
    default_int = i32

    fp8s = (fp8e4m3, fp8e5m2)
    floats = fp8s + (f16, bf16, f32, f64)
    uints = (u8, u16, u32, u64)
    sints = (i8, i16, i32, i64)
    ints = uints + sints
    all = floats + ints + (bool,)  # noqa: A003

    @staticmethod
    def is_float(x: DType) -> bool: return x in dtypes.floats
    @staticmethod
    def is_int(x: DType) -> bool: return x in dtypes.ints
    @staticmethod
    def is_unsigned(x: DType) -> bool: return x in dtypes.uints
    @staticmethod
    def is_bool(x: DType) -> bool: return x == dtypes.bool

    @staticmethod
    def finfo(dt: DType) -> tuple[int, int]:
        """(exponent, mantissa) bits of a float dtype."""
        return {dtypes.f16: (5, 10), dtypes.bf16: (8, 7), dtypes.f32: (8, 23), dtypes.f64: (11, 52),
                dtypes.fp8e4m3: (4, 3), dtypes.fp8e5m2: (5, 2)}[dt]

    @staticmethod
    def min(dt: DType):
        if dtypes.is_int(dt): return 0 if dtypes.is_unsigned(dt) else -2**(dt.bitsize - 1)
        return False if dt == dtypes.bool else -math.inf

    @staticmethod
    def max(dt: DType):
        if dtypes.is_int(dt): return 2**dt.bitsize - 1 if dtypes.is_unsigned(dt) else 2**(dt.bitsize - 1) - 1
        return True if dt == dtypes.bool else math.inf

    @staticmethod
    def fields() -> dict[str, DType]: return {d.name: d for d in dtypes.all}

    @staticmethod
    def from_py(x) -> DType:
        """tinygrad's from_py with its weak dtypes made concrete: a bool is bool, an int the
        default int (i64 past its range), a float the default float."""
        if isinstance(x, bool): return dtypes.bool
        if isinstance(x, float): return dtypes.default_float
        if isinstance(x, int): return dtypes.default_int if -2**31 <= x < 2**31 else dtypes.i64
        if isinstance(x, (list, tuple)):
            return max((dtypes.from_py(xi) for xi in x), default=dtypes.default_float)
        raise RuntimeError(f"Could not infer dtype of {x} with type {type(x)}")


DTYPES_DICT = {d.name: d for d in dtypes.all}
_BY_ID = {d.thvm_id: d for d in dtypes.all}


def least_upper_dtype(*ds: DType) -> DType:
    """tinygrad's least_upper_dtype (dtype.py:193-194), through the C lattice."""
    out = ds[0]
    for d in ds[1:]:
        out = _BY_ID[int(_lib.py_dtype_promote(out.thvm_id, d.thvm_id))]
    return out


def sum_acc_dtype(dt: DType) -> DType:
    """A sum's accumulator dtype (tinygrad dtype.py:214-218)."""
    if dtypes.is_unsigned(dt):
        return least_upper_dtype(dt, dtypes.uint32)
    if dtypes.is_int(dt) or dt == dtypes.bool:
        return least_upper_dtype(dt, dtypes.int32)
    return least_upper_dtype(dt, dtypes.float32)


def to_dtype(dt) -> DType:
    """A DType or tinygrad's name for one ("float32", "half", ...)."""
    return dt if isinstance(dt, DType) else getattr(dtypes, dt.lower())


def from_thvm_id(i: int) -> DType: return _BY_ID[i]


def _from_np_dtype(npdtype) -> DType:
    return {np.dtype(d.np_dtype): d for d in dtypes.all if d.np_dtype is not None}[np.dtype(npdtype)]


def _to_np_dtype(dt: DType):
    """numpy's type for dt; bf16 and fp8 hold their host values as f32 (tinygrad
    dtype.py:304-307)."""
    return np.float32 if dt.np_dtype is None else dt.np_dtype
