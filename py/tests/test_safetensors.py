"""nn.state safe_save / safe_load: tinygrad's safetensors format.

safe_save writes a little-endian u64 header length, the JSON header padded
with spaces to a multiple of 8, then the raw bytes in dict order
(tinygrad nn/state.py:64-86).  Files must match tinygrad byte for byte and
load in either direction.

Run:
  cd <repo> && PYTHONPATH=py DEV=cpu python3 -m pytest py/tests/test_safetensors.py -q
"""
import json
import os
import tempfile
import unittest

import numpy as np

import helpers
from thvm import Tensor, dtypes, nn
from thvm.nn.state import safe_load, safe_load_metadata, safe_save


class TestSafetensors(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory(prefix="thvm_safetensors_")
        self.path = os.path.join(self.dir.name, "t.safetensors")
        rng = np.random.default_rng(0)
        self.w = rng.standard_normal((3, 4)).astype(np.float32)
        self.b = np.arange(-2, 3, dtype=np.int32)
        self.s = np.array(1.5, dtype=np.float32)

    def tearDown(self):
        self.dir.cleanup()

    def _tensors(self):
        return {"w": Tensor(self.w), "b": Tensor(self.b), "s": Tensor(self.s)}

    def test_round_trip(self):
        safe_save(self._tensors(), self.path, metadata={"format": "pt"})
        got = safe_load(self.path)
        self.assertEqual(list(got), ["w", "b", "s"])
        np.testing.assert_array_equal(got["w"].numpy(), self.w)
        np.testing.assert_array_equal(got["b"].numpy(), self.b)
        np.testing.assert_array_equal(got["s"].numpy(), self.s)
        self.assertEqual(got["w"].dtype, dtypes.float32)
        self.assertEqual(got["b"].dtype, dtypes.int32)
        self.assertEqual(got["s"].shape, ())

    def test_header_layout(self):
        safe_save(self._tensors(), self.path, metadata={"format": "pt"})
        raw = open(self.path, "rb").read()
        n = int.from_bytes(raw[:8], "little")
        self.assertEqual(n % 8, 0)
        header = json.loads(raw[8:8 + n])
        self.assertEqual(header["__metadata__"], {"format": "pt"})
        self.assertEqual(header["w"], {"dtype": "F32", "shape": [3, 4], "data_offsets": [0, 48]})
        self.assertEqual(header["b"], {"dtype": "I32", "shape": [5], "data_offsets": [48, 68]})
        self.assertEqual(len(raw), 8 + n + 72)
        _, data_start, meta = safe_load_metadata(self.path)
        self.assertEqual(data_start, 8 + n)
        self.assertEqual(meta, header)

    def test_nn_state_spelling(self):
        nn.state.safe_save(self._tensors(), self.path)
        got = nn.state.safe_load(self.path)
        np.testing.assert_array_equal(got["w"].numpy(), self.w)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_bytes_match_tinygrad(self):
        from tinygrad import Tensor as TgTensor
        from tinygrad.nn.state import safe_save as tg_safe_save
        tg_path = os.path.join(self.dir.name, "tg.safetensors")
        tg_safe_save({"w": TgTensor(self.w), "b": TgTensor(self.b), "s": TgTensor(self.s)},
                     tg_path, metadata={"format": "pt"})
        safe_save(self._tensors(), self.path, metadata={"format": "pt"})
        self.assertEqual(open(self.path, "rb").read(), open(tg_path, "rb").read())

    def test_loads_other_dtypes(self):
        # Hand-written file: py/thvm narrows float formats to float32 and
        # integer formats to int32, as torch_load does.
        h = np.array([0.5, -2.0, 65504.0], dtype=np.float16)
        bf = np.array([1.0, -3.5, 0.15625], dtype=np.float32)
        f64 = np.array([0.25, -8.0], dtype=np.float64)
        i64 = np.array([7, -9, 2**20], dtype=np.int64)
        u8 = np.array([0, 200, 255], dtype=np.uint8)
        blobs = {"h": ("F16", h.tobytes()), "bf": ("BF16", (bf.view(np.uint32) >> 16).astype(np.uint16).tobytes()),
                 "f64": ("F64", f64.tobytes()), "i64": ("I64", i64.tobytes()), "u8": ("U8", u8.tobytes())}
        shapes = {"h": [3], "bf": [3], "f64": [2], "i64": [3], "u8": [3]}
        header, offset = {}, 0
        for k, (tag, raw) in blobs.items():
            header[k] = {"dtype": tag, "shape": shapes[k], "data_offsets": [offset, offset + len(raw)]}
            offset += len(raw)
        j = json.dumps(header, separators=(",", ":"))
        j += " " * ((len(j) + 7) // 8 * 8 - len(j))
        with open(self.path, "wb") as f:
            f.write(len(j).to_bytes(8, "little") + j.encode() + b"".join(r for _, r in blobs.values()))
        got = safe_load(self.path)
        np.testing.assert_array_equal(got["h"].numpy(), h.astype(np.float32))
        np.testing.assert_array_equal(got["bf"].numpy(), bf)
        np.testing.assert_array_equal(got["f64"].numpy(), f64.astype(np.float32))
        np.testing.assert_array_equal(got["i64"].numpy(), i64.astype(np.int32))
        np.testing.assert_array_equal(got["u8"].numpy(), u8.astype(np.int32))
        self.assertEqual(got["h"].dtype, dtypes.float32)
        self.assertEqual(got["i64"].dtype, dtypes.int32)

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_loads_tinygrad_file_with_other_dtypes(self):
        from tinygrad import Tensor as TgTensor
        from tinygrad.nn.state import safe_save as tg_safe_save
        h = np.array([0.5, -2.0, 65504.0], dtype=np.float16)
        i64 = np.array([7, -9, 2**20], dtype=np.int64)
        u8 = np.array([0, 200, 255], dtype=np.uint8)
        tg_safe_save({"h": TgTensor(h), "i64": TgTensor(i64), "u8": TgTensor(u8)}, self.path)
        got = safe_load(self.path)
        np.testing.assert_array_equal(got["h"].numpy(), h.astype(np.float32))
        np.testing.assert_array_equal(got["i64"].numpy(), i64.astype(np.int32))
        np.testing.assert_array_equal(got["u8"].numpy(), u8.astype(np.int32))

    @unittest.skipUnless(helpers.have_tinygrad(), "tinygrad not on path")
    def test_tinygrad_loads_our_file(self):
        from tinygrad.nn.state import safe_load as tg_safe_load
        safe_save(self._tensors(), self.path)
        got = tg_safe_load(self.path)
        np.testing.assert_array_equal(got["w"].numpy(), self.w)
        np.testing.assert_array_equal(got["b"].numpy(), self.b)


if __name__ == "__main__":
    unittest.main()
