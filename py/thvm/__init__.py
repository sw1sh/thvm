"""A tinygrad replacement (`import tinygrad` -> `import thvm`) over libthvm_py, which
`make py` builds; the bridge handles Thvm, K and Term build UOp graphs directly."""
from .thvm import Thvm, K, Term
from . import nn, helpers
from .tensor import Tensor
from .dtypes import dtypes, DType
from .device import Device
from .helpers import Context
from ._misc import Variable, TinyJit, UOp
from .helpers import GlobalCounters, function, getenv
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

# tinygrad's module names for the same objects: dtype.py and engine/jit.py.
_sys.modules[__name__ + ".dtype"] = _sys.modules[__name__ + ".dtypes"]
_engine_pkg = _types.ModuleType(__name__ + ".engine")
_engine_pkg.jit = _sys.modules[__name__ + "._misc"]
_sys.modules[__name__ + ".engine"] = _engine_pkg
_sys.modules[__name__ + ".engine.jit"] = _engine_pkg.jit

__all__ = [
    "Tensor", "dtypes", "DType", "Device", "Context",
    "Variable", "TinyJit", "UOp", "nn", "helpers", "GlobalCounters", "function", "getenv",
    "from_tinygrad",
    "Thvm", "K", "Term",
]
