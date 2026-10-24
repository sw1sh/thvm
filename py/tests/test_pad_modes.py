"""A replicate pad of a tensor with a size-1 axis, and reshapes that undo themselves, read right.

A realize inlined into a consumer that is itself inlined binds its closed ranges to the
consumer's index, which can be an expression over ranges (a PAD's gated index); the outer
substitution now rewrites inside it (src/schedule/materialize.c), where a range of the inner
consumer stayed in the kernel unbound and every store read its last value.  A reshape to its
source's own shape is the source (tinygrad mixin/movement.py:184), and a one-piece stack is the
unsqueezed piece (uop/ops.py:110), so an unsqueeze a flatten undoes, as a one-piece cat's, adds
no node.  Each case runs against numpy on both CPU routes (the C JIT and the UOp walker) in child
processes.

    make py && DEV=cpu PYTHONPATH=py python -m pytest py/tests/test_pad_modes.py -q
"""
import unittest

import numpy as np

import helpers
from thvm import Tensor

R = np.random.default_rng(0)


def _edge(a, flat):
    # torch's flat pads, last axis first, as np.pad's per-axis pairs; replicate is np's edge.
    pairs = [(0, 0)] * (a.ndim - len(flat) // 2) + [(flat[2 * i], flat[2 * i + 1]) for i in range(len(flat) // 2)][::-1]
    return np.pad(a, pairs, mode="edge")


def _close(got, want):
    np.testing.assert_allclose(got, want, rtol=0, atol=0)


def case_replicate_leading_one():
    for shape, pad in (((1, 3, 3, 3), (0, 2, 0, 2, 0, 2)), ((1, 5, 5, 5), (1, 2, 3, 4, 1, 2)),
                       ((2, 1, 5, 5, 5), (2, 0, 2, 0, 2, 0))):
        a = R.standard_normal(shape).astype(np.float32)
        _close(Tensor(a).pad(pad, mode="replicate").numpy(), _edge(a, pad))


def case_identity_reshapes_between_cats():
    a = R.standard_normal((1, 3, 3, 3)).astype(np.float32)
    x1 = Tensor(a).unsqueeze(0).flatten(0, 1)
    x2 = x1.shrink((None, (0, 1), None, None)).cat(x1, dim=1)
    x4 = x2.unsqueeze(2).flatten(2, 3)
    x4 = x4.shrink((None, None, None, (0, 1))).cat(x4, dim=3)
    w2 = np.concatenate([a[:, :1], a], 1)
    _close(x4.numpy(), np.concatenate([w2[..., :1], w2], 3))


def case_one_piece_cat_is_the_piece():
    a = R.standard_normal((1, 4, 3)).astype(np.float32)
    t = Tensor(a)
    assert t.cat(dim=0).term == t.term and t.cat(dim=2).term == t.term
    _close((t.cat(dim=1) + 1).numpy(), a + 1)


ROUTE_CASES = [(n[5:], f) for n, f in sorted(globals().items()) if n.startswith("case_")]


class TestBothRoutes(helpers.BothRoutes, unittest.TestCase):
    MODULE = "test_pad_modes"
    ROUTE_CASES = ROUTE_CASES


if __name__ == "__main__":
    unittest.main()
