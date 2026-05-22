"""thvm-py: drop-in tinygrad replacement (`import tinygrad` -> `import thvm`).

Build prereq:
  make py        # produces py/thvm/libthvm_py.dylib (macOS, Metal)
                 #       or py/thvm/libthvm_py.so    (Linux, CUDA)

Two layers:
  - The Phase-2 Tensor API (Tensor, dtypes, nn, ...) mirrors tinygrad's;
    method bodies build a thvm TAG_TEN graph through the Phase-1 bridge
    and route .backward() to thvm's real uop_grad.
  - The bridge handles (Thvm, Metal, Cuda, K, render) stay exposed for
    low-level autotune / kernel work.
"""
from .thvm import Thvm, Metal, Cuda, K, Term, DEFAULT_HEADER
from . import templates, nn
from .tensor import Tensor
from .dtypes import dtypes, DType
from .device import Device
from .context import Context
from ._misc import Variable, TinyJit
from .from_tinygrad import from_tinygrad

__all__ = [
    # tinygrad-shaped surface
    "Tensor", "dtypes", "DType", "Device", "Context",
    "Variable", "TinyJit", "nn",
    # cross-validation: ingest a tinygrad lazy UOp graph
    "from_tinygrad",
    # bridge handles
    "Thvm", "Metal", "Cuda", "K", "Term", "DEFAULT_HEADER", "templates",
]
