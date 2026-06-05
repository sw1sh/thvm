"""thvm's `nn.state` surface: torch_load / load_state_dict / get_state_dict.

Faithful to tinygrad/nn/state.py's API:
  - torch_load(path) -> dict[str, Tensor]   (reads a PyTorch .bin/.pth)
  - get_state_dict(obj) -> dict[str, Tensor]
  - load_state_dict(model, state_dict) -> list[Tensor]

torch_load reads BOTH PyTorch save formats directly into numpy (no
torch / safetensors dependency):

  * zip format (newer torch.save): a ZIP whose `*/data.pkl` holds the
    state-dict pickle, with storages persisted as `*/data/<id>` members.
  * legacy format (older torch.save, e.g. HF gpt2): a raw pickle stream
    `magic, protocol, sys_info, <main pickle>, <storage-id list>,
    <storage bytes>`; we two-pass it exactly like tinygrad
    (torch_load else-branch): first pass populates per-storage byte
    lengths via the _rebuild_tensor_v2 reduce, then the storage bytes are
    sliced in id order and the main pickle is replayed.

Re-exported under the dotted `thvm.nn.state` name in thvm/__init__.py.
"""
from __future__ import annotations

import io
import pickle
import struct
import zipfile

import numpy as np

from .tensor import Tensor
from .nn import _get_state_dict

# PyTorch storage dtype tag -> (numpy dtype, itemsize, is_bf16).
_TORCH_DTYPES = {
    "FloatStorage": (np.float32, 4, False),
    "DoubleStorage": (np.float64, 8, False),
    "HalfStorage": (np.float16, 2, False),
    "BFloat16Storage": (np.uint16, 2, True),   # widen to fp32 on read
    "LongStorage": (np.int64, 8, False),
    "IntStorage": (np.int32, 4, False),
    "ShortStorage": (np.int16, 2, False),
    "CharStorage": (np.int8, 1, False),
    "ByteStorage": (np.uint8, 1, False),
    "BoolStorage": (np.bool_, 1, False),
}


def _prod(xs):
    n = 1
    for x in xs:
        n *= x
    return n


def _bytes_to_array(buf: bytes, np_dtype, is_bf16, size):
    if is_bf16:
        u16 = np.frombuffer(buf, dtype=np.uint16)
        flat = (u16.astype(np.uint32) << 16).view(np.float32)
    else:
        flat = np.frombuffer(buf, dtype=np_dtype)
    n = _prod(size) if size else 1
    view = flat[:n]
    arr = np.ascontiguousarray(view.reshape(size) if size else view.reshape(()))
    if arr.dtype == np.float64:
        arr = arr.astype(np.float32)
    return arr


def torch_load(path) -> dict[str, "Tensor"]:
    """Load a PyTorch .bin/.pth into a {name: thvm Tensor} state dict."""
    path = str(path)

    # ---- shared pickle machinery (storage ids -> raw byte buffers) ----
    storage_source: dict = {}     # storage key -> bytes
    lens: dict = {}               # storage key -> byte length

    def _rebuild_tensor_v2(storage, storage_offset, size, stride,
                           requires_grad=None, backward_hooks=None,
                           metadata=None, *_):
        # storage = (typename, dtype_tag, key, location, numel)
        dtype_tag, key, numel = storage[1], storage[2], storage[4]
        np_dtype, itemsize, is_bf16 = _TORCH_DTYPES.get(
            dtype_tag, (np.float32, 4, False))
        lens[key] = (numel if numel >= 0 else _prod(size)) * itemsize
        if key not in storage_source:
            return None
        byte_start = storage_offset * itemsize
        byte_end = (storage_offset + _prod(size)) * itemsize
        return _bytes_to_array(storage_source[key][byte_start:byte_end],
                               np_dtype, is_bf16, tuple(size))

    class _Param:
        def __setstate__(self, state):
            self.tensor = state[0]

    intercept = {
        "_rebuild_tensor_v2": _rebuild_tensor_v2,
        "_rebuild_tensor": _rebuild_tensor_v2,
        "_rebuild_parameter": lambda data, *_: data,
        "Parameter": _Param,
        "device": (lambda *a, **k: "cpu"),
    }
    intercept.update({tag: tag for tag in _TORCH_DTYPES})

    class _Dummy:
        pass

    class _TorchPickle(pickle.Unpickler):
        def find_class(self, module, name):
            root = module.split(".")[0]
            if name in intercept:
                return intercept[name]
            if root not in ("torch", "collections", "numpy", "_codecs"):
                return _Dummy
            try:
                return super().find_class(module, name)
            except Exception:
                return _Dummy

        def persistent_load(self, pid):
            # legacy pid is the storage key itself (or a tuple whose [2] is)
            if isinstance(pid, (tuple, list)):
                return pid
            return pid

    def _to_tensors(d):
        out: dict[str, Tensor] = {}
        for k, v in d.items():
            if isinstance(v, _Param):
                v = v.tensor
            if isinstance(v, np.ndarray):
                out[k] = Tensor(v.copy())
            elif isinstance(v, Tensor):
                out[k] = v
        return out

    raw = open(path, "rb").read()
    fobj = io.BytesIO(raw)

    if zipfile.is_zipfile(fobj):
        # ---- zip format -----------------------------------------------
        with zipfile.ZipFile(io.BytesIO(raw), "r") as zf:
            names = zf.namelist()
            pkl_name = next(n for n in names if n.endswith("data.pkl"))
            base = pkl_name.split("/", 1)[0]
            for n in names:
                if n.startswith(f"{base}/data/") and not n.endswith(".pkl"):
                    storage_source[n.split("/")[-1]] = zf.read(n)
            pkl_bytes = zf.read(pkl_name)

        # legacy reduce keys storages by str(key); zip keys by the same id
        # string in persistent_id -- patch persistent_load to map a zip pid
        # tuple (typename, storage_tag, key, ...) to its key.
        class _ZipPickle(_TorchPickle):
            def persistent_load(self, pid):
                return pid  # pid is the (typename, tag, key, loc, numel) tuple
        d = _ZipPickle(io.BytesIO(pkl_bytes)).load()
        return _to_tensors(d)

    # ---- legacy format (magic, proto, sys_info, pickle, ids, data) ----
    fobj.seek(0)
    pkl = _TorchPickle(fobj)
    pkl.load()                 # magic number
    pkl.load()                 # protocol version
    pkl.load()                 # sys_info
    rwd = fobj.tell()          # rewind point = start of main state pickle
    pkl.load()                 # main pickle (populates lens[] via reduce)
    ids = pkl.load()           # ordered list of storage keys
    base_offset = fobj.tell()
    for i in ids:
        # each storage is prefixed with an 8-byte element count.
        base_offset += 8
        storage_source[i] = raw[base_offset:base_offset + lens[i]]
        base_offset += lens[i]
    fobj.seek(rwd)
    d = _TorchPickle(fobj).load()
    return _to_tensors(d)


def get_state_dict(obj, prefix: str = "") -> dict[str, "Tensor"]:
    sd = _get_state_dict(obj)
    if prefix:
        return {prefix + k: v for k, v in sd.items()}
    return sd


def load_state_dict(model, state_dict: dict[str, "Tensor"],
                    strict: bool = True, verbose: bool = True,
                    consume: bool = False, realize: bool = True):
    """Copy `state_dict` into `model`'s parameters by dotted name; returns
    the loaded Tensors.  Mirrors tinygrad's load_state_dict (replace +
    realize), so the model's leaf buffers carry the checkpoint weights."""
    model_sd = get_state_dict(model)
    ret = []
    for k, v in model_sd.items():
        if k not in state_dict:
            if strict:
                raise KeyError(f"missing key in state_dict: {k}")
            continue
        src = state_dict[k]
        if v.shape != src.shape:
            if {(), (1,)} == {tuple(src.shape), tuple(v.shape)}:
                src = src.reshape(*v.shape)
            else:
                raise ValueError(
                    f"shape mismatch in `{k}`: model {v.shape} vs ckpt {src.shape}")
        v.replace(src)
        if realize:
            v.realize()
        if consume:
            del state_dict[k]
        ret.append(v)
    return ret


__all__ = ["torch_load", "get_state_dict", "load_state_dict"]
