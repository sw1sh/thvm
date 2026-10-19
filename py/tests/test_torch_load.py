"""torch_load on a hand-built zip .pth (no torch needed): a saved transposed view
loads in logical order, as tinygrad permutes it (nn/state.py:232-241)."""
import io
import os
import pickle
import sys
import tempfile
import types
import unittest
import zipfile
from collections import OrderedDict

import numpy as np

import helpers  # noqa: F401 - puts py/ on the path
from thvm.nn.state import torch_load


class _Storage:
    def __init__(self, key, arr):
        self.key, self.arr = key, arr


class _Rebuild:
    def __init__(self, storage, size, stride):
        self.args = (storage, 0, size, stride, False, OrderedDict())

    def __reduce__(self):
        return (sys.modules["torch._utils"]._rebuild_tensor_v2, self.args)


class _Pickler(pickle.Pickler):
    def persistent_id(self, obj):
        if isinstance(obj, _Storage):
            return ("storage", sys.modules["torch"].FloatStorage, obj.key, "cpu", obj.arr.size)
        return None


def _write_pth(path, entries):
    """entries: name -> (storage array, size, stride in elements)."""
    torch, utils = types.ModuleType("torch"), types.ModuleType("torch._utils")
    storage_cls = type("FloatStorage", (), {"__module__": "torch"})

    def rebuild(*args):
        raise AssertionError("never called while pickling")
    rebuild.__module__, rebuild.__qualname__ = "torch._utils", "_rebuild_tensor_v2"
    torch.FloatStorage, utils._rebuild_tensor_v2, torch._utils = storage_cls, rebuild, utils
    saved = {k: sys.modules.get(k) for k in ("torch", "torch._utils")}
    sys.modules["torch"], sys.modules["torch._utils"] = torch, utils
    try:
        state, blobs = OrderedDict(), {}
        for i, (name, (arr, size, stride)) in enumerate(entries.items()):
            blobs[str(i)] = arr.astype(np.float32).tobytes()
            state[name] = _Rebuild(_Storage(str(i), arr), size, stride)
        buf = io.BytesIO()
        _Pickler(buf, protocol=2).dump(state)
    finally:
        for k, v in saved.items():
            if v is None:
                sys.modules.pop(k, None)
            else:
                sys.modules[k] = v
    with zipfile.ZipFile(path, "w") as zf:
        zf.writestr("archive/data.pkl", buf.getvalue())
        for key, raw in blobs.items():
            zf.writestr("archive/data/" + key, raw)


class TestTorchLoad(unittest.TestCase):
    def test_transposed_view_loads_in_logical_order(self):
        orig = np.arange(6, dtype=np.float32).reshape(3, 2)
        flat = np.arange(4, dtype=np.float32) + 10
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "t.pth")
            _write_pth(path, {"t": (orig, (2, 3), (1, 2)), "c": (flat, (2, 2), (2, 1))})
            got = torch_load(path)
        np.testing.assert_array_equal(got["t"].numpy(), orig.T)
        np.testing.assert_array_equal(got["c"].numpy(), flat.reshape(2, 2))


if __name__ == "__main__":
    unittest.main()
