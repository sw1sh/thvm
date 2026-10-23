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

Tests: tests/test_nn_conv.c (values against a direct loop).
