"""A live detached tensor keeps the storage it reads across reclaim().

GlobalCounters.reset() runs reclaim(), which frees every backend buffer that
no live tensor reaches: thvm's stand-in for tinygrad's reference-counted
buffers, where a live Tensor keeps the buffers its UOp graph reads.  A
detached tensor reads its source's
storage (tinygrad's detach is a DETACH UOp over the source, mixin/
elementwise.py:43-47), so holding only `Tensor(x).realize().detach()` across
a training step, as one holds data or targets, must keep x's buffer, and a
loss built from it after the reclaim must still differentiate (the detached
operand itself gets no gradient).  It did not: the realized source was
unpinned when its Python object died, reclaim() freed the buffer and the next
read returned garbage, then crashed (a 1 MB buffer is unmapped when freed).

Each case runs in a child process, so a crash fails the test, not the run.
"""
import unittest

import helpers

_CHILD = r"""
import numpy as np
from thvm import GlobalCounters, Tensor
x = np.arange(1 << 18, dtype=np.float32)
y = np.linspace(-1, 1, 1 << 18, dtype=np.float32)
xd = Tensor(x).realize().detach()
yd = Tensor(y).realize().detach()
w = Tensor(y)
GlobalCounters.reset()
junk = [Tensor(np.full(1 << 18, 7.0, np.float32)).realize() for _ in range(4)]
print("READ", bool(np.array_equal(xd.numpy(), x)), flush=True)
print("SUM", bool(np.array_equal((xd * 2.0).numpy(), 2.0 * x)), flush=True)
(w * yd).sum().backward()
# tinygrad fills .grad on the detached tensor too: the cotangent its DETACH node gathers.
print("GRAD", bool(np.array_equal(w.grad.numpy(), y)), bool(np.array_equal(yd.grad.numpy(), y)), flush=True)
"""


class TestReclaimPins(unittest.TestCase):
    def test_detached_tensor_keeps_its_storage(self):
        out = helpers.run_child(_CHILD, 600, DEV="cpu")
        self.assertEqual(out.returncode, 0, f"child died:\n{out.stdout}\n{out.stderr[-3000:]}")
        self.assertEqual(out.stdout.split(),
                         ["READ", "True", "SUM", "True", "GRAD", "True", "True"])


if __name__ == "__main__":
    unittest.main()
