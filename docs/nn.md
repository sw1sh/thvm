# The NN layer (`src/nn/`)

tinygrad's NN-level Tensor ops (the `_pool` unfold, conv2d, and the layers
built on them) ported once in C over the UOp constructors (`src/uop/`), so the
py package, THVMLink and other hosts build one graph from one implementation.

## Values

An op takes and returns `Tensor` (src/thvm.h): the UOp term with its dtype and
shape, as tinygrad's Tensor carries them.  Ops compute shapes from their
arguments and never walk the graph under them.  A result with `t == 0` is a
failed op (bad rank, an empty window, a groups mismatch).

The helpers mirror py/thvm/tensor.py's Tensor methods call for call, so a
front end that switches to an `nn_*` op keeps its kernel keys:

| C | tensor.py |
|---|---|
| `nn_move`, `nn_reshape`, `nn_permute`, `nn_expand`, `nn_shrink_to` | `reshape`, `permute`, `expand`, `pad`, `shrink`, `shrink_to` |
| `nn_cast` | `cast` |
| `nn_binop` | `_binop` (broadcast, then a mixed pair in f32) |
| `nn_reduce` | `_reduce` without keepdim |
| `nn_pool` | `_pool` (tinygrad mixin/movement.py:598-616) |
| `nn_conv2d` | `conv2d` (tinygrad mixin/op.py:1564-1605) |

## Flat ABI

`py_nn_*` in py/csource/thvm_py.c export the ops with plain integers only, so
any FFI can call them: a tensor is its term plus a descriptor
`desc[2 + MAX_DIM] = [dtype, ndim, dims...]`, and the result's descriptor is
written over the first input's.

```c
uint64_t py_nn_pool(uint64_t x, uint32_t *desc, uint32_t nk,
                    uint32_t const *ksd);           // kernel, stride, dilation
uint64_t py_nn_conv2d(uint64_t const *xwb,          // x, w, b (0: no bias)
                      uint32_t *desc,               // three descriptors
                      uint32_t const *opts);        // groups, stride[2],
                                                    // dilation[2], padding[2]
```

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

Tests: tests/test_nn_conv.c (values against a direct loop),
py/tests/test_flat_abi.py (the host-facing exports through ctypes).
