"""thvm drives one device of each kind: a device index past 0 raises, as a second device tinygrad
cannot open, so tinygrad's needs_second_gpu (test/helpers.py) skips its multi-device tests.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_device_index.py -q
"""
import unittest

import helpers  # noqa: F401
from thvm import Device, Tensor


class TestDeviceIndex(unittest.TestCase):
    def test_second_device_raises(self):
        for make in (lambda d: Tensor.zeros(10, device=d), lambda d: Tensor([1.0], device=d),
                     lambda d: Tensor.empty(2, device=d), lambda d: Tensor.ones(2).to(d)):
            with self.assertRaises(RuntimeError):
                make(f"{Device.DEFAULT}:1")

    def test_first_device_is_the_device(self):
        t = Tensor.zeros(3, device=f"{Device.DEFAULT}:0").to(Device.DEFAULT).contiguous().realize()
        self.assertEqual(t.tolist(), [0.0, 0.0, 0.0])
        self.assertEqual(Device.canonicalize(f"{Device.DEFAULT.lower()}:0"), Device.DEFAULT)


if __name__ == "__main__":
    unittest.main()
