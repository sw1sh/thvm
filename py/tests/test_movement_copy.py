"""A realized strided view compiles a copy kernel on the CPU, as tinygrad's.

The root of a realize that is a movement op over a buffer resolves to a view of it; a plain
contiguous view is the buffer itself, and any other (a permute, an offset shrink, a flip, a
chain of them) was gathered on the host, which read the whole source and decoded each element's
index (50 ms for tinygrad's array_packing, 4096^2 floats).  On the CPU it now takes the kernel
path, the copy kernel tinygrad schedules for `.contiguous()`, and a TinyJit replay re-runs that
kernel over the live input, where the gather was recorded as a replayable op.

The cases cover a transpose, tinygrad's array_packing, an offset shrink, a flip, a permuted
shrink, an expand and a copy of a copy against numpy on both CPU routes (the C JIT and the UOp
walker) in child processes, and a TinyJit replay over inputs that change between calls.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_movement_copy.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor, TinyJit

X = np.arange(64 * 96, dtype=np.float32).reshape(64, 96)


def _eq(got, want):
    np.testing.assert_array_equal(got, want)


def case_transpose():
    _eq(Tensor(X).T.contiguous().numpy(), X.T)


def case_array_packing():
    want = X.reshape(64, 3, 32).transpose(1, 0, 2)
    _eq(Tensor(X).reshape(64, 3, 32).permute(1, 0, 2).contiguous().numpy(), want)


def case_offset_shrink():
    _eq(Tensor(X)[5:20, 7:50].contiguous().numpy(), X[5:20, 7:50])


def case_flip():
    _eq(Tensor(X).flip(1).contiguous().numpy(), X[:, ::-1])


def case_permuted_shrink():
    _eq(Tensor(X)[3:40].T[10:30].contiguous().numpy(), X[3:40].T[10:30])


def case_expand():
    _eq(Tensor(X[:1]).expand(5, 96).contiguous().numpy(), np.broadcast_to(X[:1], (5, 96)))


def case_copy_of_copy():
    # Two copy kernels: the second reads the first's output through a flip.
    y = Tensor(X)[5:20].T.contiguous()
    _eq(y.flip(1).contiguous().numpy(), X[5:20].T[:, ::-1])


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_movement_copy"
    ROUTE_CASES = ROUTE_CASES


class TestJitReplay(unittest.TestCase):
    def test_replay_reads_live_input(self):
        @TinyJit
        def pack(x):
            return x.reshape(64, 3, 32).permute(1, 0, 2).contiguous().realize()

        for i in range(5):
            a = X + i
            _eq(pack(Tensor(a)).numpy(), a.reshape(64, 3, 32).transpose(1, 0, 2))


if __name__ == "__main__":
    unittest.main()
