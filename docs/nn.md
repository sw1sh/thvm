# The NN layer (`src/nn/`)

tinygrad's NN-level Tensor ops ported once in C over the UOp constructors
(`src/uop/`), so the py package, THVMLink and other hosts build one graph from
one implementation.

## Values

An op takes and returns `Tensor` (src/thvm.h): the UOp term with its dtype and
shape, as tinygrad's Tensor carries them.  Ops compute shapes from their
arguments and never walk the graph under them.  A result with `t == 0` is a
failed op (bad rank, mismatched shapes, an empty window, a groups mismatch).

Each op replays py/thvm/tensor.py's construction order call for call, so a front
end that switches to an `nn_*` op keeps its kernel keys:

| C | tensor.py | tinygrad |
|---|---|---|
| `nn_move`, `nn_reshape`, `nn_permute`, `nn_expand`, `nn_shrink_to` | `reshape`, `permute`, `expand`, `pad`, `shrink`, `shrink_to` | mixin/movement.py |
| `nn_cast`, `nn_unary` | `cast`, `_unary` | |
| `nn_broadcast`, `nn_binop`, `nn_sub` | `_broadcast`, `_binop` (a mixed pair in f32), `__sub__` | mixin/elementwise.py:24-34 |
| `nn_full`, `nn_scalar`, `nn_binop_f` | `_const_broadcast`, `_wrap_other`, an op with a float | uop/ops.py:603-605 |
| `nn_cmplt`, `nn_where` | `__lt__`, `where` | mixin/elementwise.py:426-438 |
| `nn_reduce`, `nn_mean` | `_reduce`, `mean` | |
| `nn_elementwise` (`NN_EXP` .. `NN_SQUARE`) | `exp`, `log`, `sigmoid`, `tanh`, `relu`, `gelu`, `erf`, `rsqrt`, `square` | mixin/elementwise.py |
| `nn_maximum`, `nn_minimum` | `maximum`, `minimum` | mixin/gradient.py:78-79 |
| `nn_softmax` | `softmax`, `log_softmax` | mixin/op.py:657-700 |
| `nn_layernorm`, `nn_batchnorm` | `layernorm`, `batchnorm` | mixin/op.py:1646-1683 |
| `nn_dot`, `nn_linear` | `__matmul__`, `linear` | mixin/op.py:387-392, 1254-1271 |
| `nn_pool`, `nn_conv2d` | `_pool`, `conv2d` | mixin/movement.py:598-616, mixin/op.py:1564-1605 |

Where tensor.py differs from tinygrad (softmax's max is not detached,
batchnorm scales by invstd before weight), the C keeps tensor.py's graph for
now; `nn_linear` follows tinygrad (a 1-D weight multiplies).

## Flat ABI

`py_nn_*` in py/csource/thvm_py.c export the ops with plain integers only, so
any FFI can call them: a tensor is its term plus a descriptor
`desc[2 + MAX_DIM] = [dtype, ndim, dims...]`, and the result's descriptor is
written over the first input's.  An op over several tensors takes their terms
as an array (0 for an absent bias or weight) and their descriptors back to
back.  A failed op returns 0.

| Export | Arguments after the tensors |
|---|---|
| `py_nn_move(op, x, desc, n, arg)` | a `UOP_RESHAPE`/`PERMUTE`/`EXPAND` (n dims) or `PAD`/`SHRINK` (2 * ndim) |
| `py_nn_binop(op, ab, desc)` | |
| `py_nn_elementwise(op, x, desc)` | op from `py_const_NN_*` |
| `py_nn_reduce(kind, x, desc, n, axes, keepdim, mean)` | `REDUCE_SUM` / `REDUCE_MAX`, i32 axes |
| `py_nn_where(cxy, desc)` | |
| `py_nn_maximum(ab, desc, min)` | |
| `py_nn_softmax(x, desc, axis, log)` | |
| `py_nn_layernorm(x, desc, n, axes, eps)` | i32 axes, a double eps |
| `py_nn_batchnorm(xwbmi, desc)` | x, weight, bias, mean, invstd |
| `py_nn_dot(xw, desc)`, `py_nn_linear(xwb, desc)` | |
| `py_nn_pool(x, desc, nk, ksd)` | kernel[nk], stride[nk], dilation[nk] |
| `py_nn_conv2d(xwb, desc, opts)` | groups, stride[2], dilation[2], padding[2] |

A host other than Python (an FFI such as WolframKernel's) needs three more
pieces, all in the same library:

- **Lifetime.** A term is a heap word that a collection moves.  The host
  holds a pin handle instead: `py_pin_set(id, term)` roots the term under an id
  the host picks, `py_pin_get(id)` reads its current term (read it right before
  an op, then pin the result), `py_pin_drop(id)` releases it.  Collections run
  only in `py_gc_collect(roots, n)`, which the host calls after a realize when
  `py_gc_pending()` is nonzero; with every live term pinned, `n` is 0.
- **Buffers in.** `py_ten_external(dtype, ndim, dims, data, nbytes, release,
  handle)` makes a CPU tensor over the host's bytes without a copy; the
  buffer's last release calls `release(handle)`.
- **Buffers out.** `py_ten_data(t, &offset, strides)` gives a CPU tensor's
  buffer, its view's element offset and its strides (in elements, 0
  broadcasts); the pointer stays valid while the tensor is pinned.

Tests: tests/test_nn_ops.c and tests/test_nn_conv.c (values against host loops),
py/tests/test_flat_abi.py (the host-facing exports through ctypes).
