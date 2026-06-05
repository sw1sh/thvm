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
from . import templates, nn, helpers
from .tensor import Tensor
from .dtypes import dtypes, DType
from .device import Device
from .context import Context
from ._misc import Variable, TinyJit, UOp
from .helpers import GlobalCounters, function
from .from_tinygrad import from_tinygrad

# tinygrad-shaped dotted submodules so `from thvm.uop.ops import UOp`,
# `from thvm.nn.state import torch_load`, `from thvm.llm.gguf import
# gguf_load` resolve (nn / uop / llm are not real packages -- register
# them in sys.modules the same way nn.optim / nn.datasets are).
import sys as _sys
import types as _types
from . import uop_ops as _uop_ops
from . import gguf as _gguf
from . import nn_state as _nn_state

_uop_pkg = _types.ModuleType(__name__ + ".uop")
_uop_pkg.ops = _uop_ops
_sys.modules[__name__ + ".uop"] = _uop_pkg
_sys.modules[__name__ + ".uop.ops"] = _uop_ops

_llm_pkg = _types.ModuleType(__name__ + ".llm")
_llm_pkg.gguf = _gguf
_sys.modules[__name__ + ".llm"] = _llm_pkg
_sys.modules[__name__ + ".llm.gguf"] = _gguf

_sys.modules[__name__ + ".nn.state"] = _nn_state

__all__ = [
    # tinygrad-shaped surface
    "Tensor", "dtypes", "DType", "Device", "Context",
    "Variable", "TinyJit", "UOp", "nn", "helpers", "GlobalCounters", "function",
    # cross-validation: ingest a tinygrad lazy UOp graph
    "from_tinygrad",
    # bridge handles
    "Thvm", "Metal", "Cuda", "K", "Term", "DEFAULT_HEADER", "templates",
]
