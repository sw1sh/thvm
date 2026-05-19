"""tinygrad-compatible nn module.

Phase 3 deliverable: nn.Conv2d / Linear / BatchNorm / ... ported as
thvm Tensor compositions.  Phase-2A stub: importing works but every
layer raises NotImplementedError on construction so tests that need
nn fail loudly at the use site, not the import.
"""
from __future__ import annotations


def _missing(layer_name: str):
    def _raise(*args, **kwargs):
        raise NotImplementedError(
            f"thvm.nn.{layer_name}: Phase 3 (nn layers) not yet implemented")
    return _raise


Conv2d    = _missing("Conv2d")
Linear    = _missing("Linear")
BatchNorm = _missing("BatchNorm")
LayerNorm = _missing("LayerNorm")


class _State:
    @staticmethod
    def get_parameters(layers):
        raise NotImplementedError(
            "thvm.nn.state.get_parameters: Phase 3 pending")


state = _State()
