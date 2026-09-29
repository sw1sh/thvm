"""Chain rule through the tinygrad importer's WHERE and MAX.

from_tinygrad maps tinygrad's WHERE (and MAX, a select on a < b) to thvm's
value-level uop_where.  The gradient follows tinygrad mixin/gradient.py:81:
the selected branch gets the cotangent, the condition none.  The graphs are
built with the importer's own helpers, so this runs without tinygrad.
"""
import numpy as np

from thvm import Tensor
from thvm.dtypes import dtypes
from thvm.from_tinygrad import _binary, _expand, _fconst, _max, _unary, _where
from thvm.thvm import K, Term

XS = np.array([-2.5, -1.25, -0.75, -0.125, 0.25, 0.5, 1.5, 3.0], dtype=np.float32)
N = len(XS)


def _c(v):
    return _expand(_fconst(v), (N,))


def _wrap(term):
    return Tensor._from_term(Term(term), dtypes.float32, (N,))


def test_where_grad():
    x = Tensor(XS)
    xt = int(x.term)
    y = _wrap(_where(_binary(K.CMPLT, xt, _c(0.0)),
                     _binary(K.MUL, xt, xt),
                     _binary(K.MUL, _c(3.0), xt)))
    y.sum().backward()
    np.testing.assert_allclose(x.grad.numpy(), np.where(XS < 0, 2 * XS, 3.0),
                               rtol=1e-6)
    np.testing.assert_allclose(y.numpy(), np.where(XS < 0, XS * XS, 3 * XS),
                               rtol=1e-6)


def test_nested_where_grad():
    # The two selects share their else branch, which the index-layer folds
    # of uop_iwhere would merge into a tensor-level IAND.
    x = Tensor(XS)
    xt = int(x.term)
    three_x = _binary(K.MUL, _c(3.0), xt)
    inner = _where(_binary(K.CMPLT, xt, _c(-1.0)), _unary(K.NEG, xt), three_x)
    y = _wrap(_where(_binary(K.CMPLT, xt, _c(0.0)), inner, three_x))
    y.sum().backward()
    np.testing.assert_allclose(x.grad.numpy(), np.where(XS < -1, -1.0, 3.0),
                               rtol=1e-6)
    want = np.where(XS < 0, np.where(XS < -1, -XS, 3 * XS), 3 * XS)
    np.testing.assert_allclose(y.numpy(), want, rtol=1e-6)


def test_max_grad():
    # max(a, b) = where(a < b, b, a): the larger operand gets the cotangent.
    a = Tensor(XS)
    b = Tensor(np.full_like(XS, 0.4))
    y = _wrap(_max(int(a.term), int(b.term)))
    y.sum().backward()
    np.testing.assert_allclose(a.grad.numpy(), np.where(XS < 0.4, 0.0, 1.0))
    np.testing.assert_allclose(b.grad.numpy(), np.where(XS < 0.4, 1.0, 0.0))
    np.testing.assert_allclose(y.numpy(), np.maximum(XS, np.float32(0.4)),
                               rtol=1e-6)
