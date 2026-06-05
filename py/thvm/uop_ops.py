"""thvm's `uop.ops` surface.

tinygrad exposes a `UOp` symbolic-graph type here; on the thvm Tensor
surface a bound symbolic Variable is a `BoundVar` (an int subclass with
`.val`), which doubles as the `UOp` gpt2 checks for the single-token
decode path.  Re-exported under the dotted `thvm.uop.ops` name in
__init__.py."""
from ._misc import UOp

__all__ = ["UOp"]
