"""thvm's `nn.state` surface: safetensors, torch_load, state dicts.

Faithful to tinygrad/nn/state.py's API:
  - safe_save(tensors, fn, metadata=None) / safe_load(fn) -> dict[str, Tensor]
  - safe_load_metadata(fn) -> (file bytes, data_start, header)
  - torch_load(path) -> dict[str, Tensor]   (reads a PyTorch .bin/.pth)
  - get_state_dict(obj) -> dict[str, Tensor]
  - load_state_dict(model, state_dict) -> list[Tensor]

safetensors files are byte-identical to tinygrad's safe_save.  thvm has no
disk device, so safe_load_metadata returns a read-only numpy memmap of the
file where tinygrad returns a disk Tensor.  py/thvm holds only float32 and
int32 tensors, so safe_load narrows other dtypes the way torch_load does:
float formats to float32 and integer formats to int32.

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
import json
import pathlib
import pickle
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


# safetensors dtype tag -> numpy storage dtype; BF16 and the fp8 tags have
# no numpy dtype and are widened by _safe_array (tinygrad nn/state.py:33-35).
_SAFE_NP = {
    "BOOL": np.bool_, "I8": np.int8, "U8": np.uint8, "I16": np.int16,
    "U16": np.uint16, "I32": np.int32, "U32": np.uint32, "I64": np.int64,
    "U64": np.uint64, "F16": np.float16, "BF16": np.uint16,
    "F32": np.float32, "F64": np.float64,
}


def _safe_array(raw, tag: str, shape) -> np.ndarray:
    if tag == "BF16":
        u16 = np.frombuffer(raw, dtype=np.uint16)
        flat = (u16.astype(np.uint32) << 16).view(np.float32)
    elif tag in _SAFE_NP:
        flat = np.frombuffer(raw, dtype=_SAFE_NP[tag])
    else:
        raise NotImplementedError(f"safetensors dtype {tag} is not supported by py/thvm")
    return flat.reshape(shape)


def safe_load_metadata(fn):
    """Loads a .safetensors file, returning the file bytes, the data start
    position, and the header (tinygrad nn/state.py:44-50)."""
    t = np.memmap(pathlib.Path(fn), dtype=np.uint8, mode="r")
    data_start = int.from_bytes(t[0:8].tobytes(), "little") + 8
    return t, data_start, json.loads(t[8:data_start].tobytes())


def safe_load(fn) -> dict[str, "Tensor"]:
    """Loads a .safetensors file, returning the `state_dict`
    (tinygrad nn/state.py:51-62)."""
    t, data_start, metadata = safe_load_metadata(fn)
    data = t[data_start:]
    out = {}
    for k, v in metadata.items():
        if k == "__metadata__":
            continue
        lo, hi = v["data_offsets"]
        out[k] = Tensor(np.array(_safe_array(data[lo:hi], v["dtype"], v["shape"])))
    return out


def safe_save(tensors: dict[str, "Tensor"], fn, metadata: dict | None = None):
    """Saves a `state_dict` to disk in a .safetensors file with optional
    metadata (tinygrad nn/state.py:64-86): a little-endian u64 header
    length, the JSON header padded with spaces to a multiple of 8, then the
    raw tensor bytes in dict order."""
    headers, offset, blobs = {}, 0, []
    if metadata:
        headers["__metadata__"] = metadata
    for k, v in tensors.items():
        arr = np.ascontiguousarray(v.numpy(), dtype=v.dtype.np_dtype)
        tag = {np.dtype(np.float32): "F32", np.dtype(np.int32): "I32"}[arr.dtype]
        headers[k] = {"dtype": tag, "shape": list(v.shape),
                      "data_offsets": [offset, offset + arr.nbytes]}
        offset += arr.nbytes
        blobs.append(arr.tobytes())
    j = json.dumps(headers, separators=(",", ":"))
    j += "\x20" * ((len(j) + 7) // 8 * 8 - len(j))
    with open(fn, "wb") as f:
        f.write(len(j).to_bytes(8, "little"))
        f.write(j.encode("utf-8"))
        for b in blobs:
            f.write(b)


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


def get_parameters(obj) -> list["Tensor"]:
    return list(_get_state_dict(obj).values())


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


__all__ = ["safe_save", "safe_load", "safe_load_metadata", "torch_load",
           "get_state_dict", "get_parameters", "load_state_dict"]
