"""thvm-py: ctypes wrapper around thvm's UOp construction + render API.

Build prereq:
  make py        # produces py/thvm/libthvm_py.dylib

Usage:
  from py.thvm import Thvm, K
  h = Thvm()
  out = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, N), instance=0)
  a   = h.buffer(scope=K.SCOPE_GLOBAL, dtype=K.FP32, dims=(M, K_), instance=1)
  ...
  msl = h.render(root, name='my_kernel')
"""
from .thvm import Thvm, Metal, K, Term, DEFAULT_HEADER
from . import templates

__all__ = ["Thvm", "Metal", "K", "Term", "DEFAULT_HEADER", "templates"]
