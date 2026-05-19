"""thvm-py: ctypes wrapper around thvm's UOp construction + render API.

Build prereq:
  make py        # produces py/thvm/libthvm_py.dylib (macOS, Metal)
                 #       or py/thvm/libthvm_py.so    (Linux, CUDA)

Usage:
  from py.thvm import Thvm, K
  h = Thvm()
  out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
  a   = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, K_), instance=1)
  ...
  msl = h.render(root, name='my_kernel')        # Metal
  cu  = h.render_cuda(root, name='my_kernel')   # CUDA

The `Metal` class drives a GPU on macOS; `Cuda` does the same on a
Linux+CUDA host.  The `Thvm` UOp builder + `K` constants are backend
agnostic and shared by both.
"""
from .thvm import Thvm, Metal, Cuda, K, Term, DEFAULT_HEADER
from . import templates

__all__ = ["Thvm", "Metal", "Cuda", "K", "Term", "DEFAULT_HEADER", "templates"]
