# Verifies the backward() stale-grad guard (ideal_pipeline_v3 M0).
#
# thvm's backward() ACCUMULATES each leaf's cotangent into the canonical
# C-side TenDesc.grad (tinygrad semantics), unlike PyTorch's fresh-each-
# backward .grad.  So a forgotten opt.zero_grad() silently piles gradients
# across steps and diverges/NaNs.  backward() now soft-warns when a float
# leaf enters still carrying a gradient from a prior pass.
import unittest
import warnings

import numpy as np

from thvm import Tensor
from thvm.tensor import _TH


def _leaf():
    return Tensor(np.array([2.0, 3.0], dtype=np.float32))


class TestZeroGradGuard(unittest.TestCase):
    def test_first_backward_no_stale_warn(self):
        # A fresh leaf's first backward has nothing stale to warn about.
        x = _leaf()
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always")
            (x * x).sum().backward()
        self.assertFalse(
            any("zero_grad" in str(wi.message) for wi in w),
            "first backward on a fresh leaf must not warn")
        self.assertIsNotNone(x.grad)

    def test_second_backward_without_clear_warns(self):
        # Second backward without clearing -> the leaf grad is stale ->
        # warn (the user is about to accumulate onto last pass's grad).
        x = _leaf()
        (x * x).sum().backward()
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always")
            (x * x).sum().backward()
        self.assertTrue(
            any("zero_grad" in str(wi.message) for wi in w),
            "second backward without zero_grad must warn about stale grad")

    def test_clear_between_suppresses_warn(self):
        # Clearing the C-side accumulator (what opt.zero_grad() does)
        # between passes restores the no-warn state.
        x = _leaf()
        (x * x).sum().backward()
        _TH.ten_clear_grad(x.term)
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always")
            (x * x).sum().backward()
        self.assertFalse(
            any("zero_grad" in str(wi.message) for wi in w),
            "a cleared leaf must not warn on the next backward")


if __name__ == "__main__":
    unittest.main()
