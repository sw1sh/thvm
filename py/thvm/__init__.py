"""A tinygrad replacement (`import tinygrad` -> `import thvm`) over libthvm_py, which
`make py` builds; the bridge handles Thvm, K and Term build UOp graphs directly."""
from .thvm import Thvm, K, Term
from . import nn, helpers
from .tensor import Tensor
from .dtypes import dtypes, DType
from .device import Device
from .helpers import Context
from ._misc import Variable, TinyJit, UOp
from .helpers import GlobalCounters, function
from .from_tinygrad import from_tinygrad

# nn, uop and llm are not packages, so their dotted names are registered for
# `from thvm.uop.ops import UOp`, `from thvm.nn.state import torch_load` and the like.
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
    "Tensor", "dtypes", "DType", "Device", "Context",
    "Variable", "TinyJit", "UOp", "nn", "helpers", "GlobalCounters", "function",
    "from_tinygrad",
    "Thvm", "K", "Term",
]
