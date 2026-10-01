# Gather and scatter

Two native UOps index a tensor by the values of another:
`UOP_GATHER` reads, `UOP_SCATTER` writes.  They give tinygrad's
`Tensor.gather`, `scatter_reduce` and `scatter` the same values tinygrad
computes through one-hot masks, at O(|index|) cost instead of
O(|index| x |axis|): an embedding lookup over a 1e5-row table costs the
same as one over a 1e3-row table.

## Semantics

`UOP_GATHER(src, idx, dim)` ([src/uop/gather.c](../src/uop/gather.c)) has
idx's shape and src's dtype:

```
out[i] = src[i with i_dim := idx[i]]   if 0 <= idx[i] < src.shape[dim]
       = 0                            otherwise
```

This is tinygrad `Tensor.gather` (`mixin/op.py:1023-1041`): its one-hot
sum selects nothing for an index out of range.  `uop_gather` returns 0
unless the operands have equal rank, idx has an int dtype, `dim` is in
range and `idx.shape[d] <= src.shape[d]` for every `d != dim`.

Deviation: a kernel compares the index as an int32, the width of every
kernel address, and py's `Tensor` stores an int64 array as int32, so an
index outside the int32 range reads the row its low 32 bits name
(2^32 + 1 reads row 1) where tinygrad reads 0.  A list or ndarray index
to `x[...]` maps an out-of-range index to -1 on the host first, so it
reads 0.  A scatter reads its index the same way.

`UOP_SCATTER(base, idx, src, dim, kind, include_self)`
([src/uop/scatter.c](../src/uop/scatter.c)) has base's shape and dtype.
With `t(p)` = p with `p_dim := idx[p]` and a valid p one whose index is in
range:

- `SUM`, `PROD`, `MAX`, `MIN`: `out = S (op) T`, where `S[j]` folds the
  src values of every valid p with `t(p) = j` in row-major p order (the
  kind's identity when there is none) and `T` is base, or with
  `include_self = 0` the identity at every position some p writes.  This
  is `scatter_reduce` (`mixin/op.py:1126-1134`).
- `REPLACE`: the last valid write along `dim` wins, base elsewhere
  (`scatter`, `_masked_merge`, `mixin/op.py:1170-1177`).

`scatter_identity_bits` gives tinygrad's exact identities (0, 1, the dtype
minimum or maximum, `dtype.py:69-77`).

## Lowering

Both lower in `pm_apply_rangeify`; [lowering_passes.md](lowering_passes.md)
has the detail.

- A gather is a guarded indexed load: idx loads at the node's own ranges,
  src at those ranges with axis `dim` replaced by the loaded index, gated
  to 0 when it is out of range.  It creates no REDUCE and fuses into its
  consumer like an elementwise op (`gather * 2 + 1` is one kernel).  An
  index grid computed on the fly (`idx + 1`, a negative index wrapped by
  `x[idx]`) fuses too; a src that is not a view of storage realizes first.
- A scatter is always its own kernel: an AFTER chain of stores, with a
  serial read-modify-write over the index space.  An invalid index writes
  the old value back.  SUM and REPLACE lower today; PROD, MAX and MIN are
  declined with a stderr line until they get a lowering.
- Kernels holding either op are flagged `no_opts` and run on the CPU
  backend only: emit declines them on a GPU backend.
- The index is read from its buffer at run time, so a JIT capture replays
  with the index rewritten in place or rebound (TinyJit's fresh inputs).
  An index view's offset, by contrast, is a constant of the kernel: a
  decode loop that slices `allpos[:, p:p+1]` gets one kernel per position,
  like every other shape or offset thvm cannot keep symbolic.

## Gradients

The chain rule ([grad.md](grad.md)) follows the one-hot formulations:

- `GATHER`: src gets the cotangent scatter-added into zeros of its shape,
  accumulated in float32 for 16- and 8-bit floats (`sum_acc_dtype`).
- `SCATTER(SUM)`: src gets the cotangent gathered back; base gets it as
  is, or without `include_self` with every written position zeroed (a
  `REPLACE` scatter, tinygrad's `_inv_mask`).
- The index gets no gradient.
- `PROD`, `MAX`, `MIN` and `REPLACE` scatters have no rule yet: their cell
  fires to zero and `grad_err_take()` returns the message, which py's
  `backward()` raises as a RuntimeError.

## Python

On the CPU backend (`Device.DEFAULT == "CPU"`):

- `Tensor.gather(dim, index)` is one `UOP_GATHER`, with tinygrad's
  checks and exception types (RuntimeError for a rank mismatch or a
  non-int index, IndexError for `dim`, AssertionError for the shape rule).
- `Tensor.embedding(idx)` and `nn.Embedding` gather rows: shape
  `(*idx.shape, embed)`, a zero row for an id outside `[0, vocab)`,
  negative ids included, as tinygrad's one-hot embedding reads them.
  `nn.Embedding` raises TypeError for a non-int index.
- `x[idx]` with one Tensor, list or ndarray index selects rows along axis
  0: a negative index counts from the end, one still out of range reads
  a zero row, a non-int index raises IndexError.  Other advanced forms
  still raise NotImplementedError.
- All three are differentiable: the weight's gradient is a scatter-add.
- `Tensor.scatter_reduce(dim, index, src, reduce, include_self=True)`
  with reduce `"sum"` is one `SCATTER(SUM)`, after `_pre_scatter`'s checks
  and exception types (`mixin/op.py:1077-1093`; src is shrunk to index's
  shape first).  `"mean"` divides it by a second one that counts, as
  tinygrad's `mask.where(1, 0).sum(-1)` plus 1 (or plus 1 only where
  nothing lands, without include_self) does (`:1131-1133`): a SUM scatter
  of ones onto ones.  sum keeps self's dtype (tinygrad's `sum_acc_dtype`
  on float32 and int32); a mean is float32, an int one included.
  `"prod"`, `"amax"` and `"amin"` raise NotImplementedError naming the
  kind until their scatter lowerings land.  Both reductions are
  differentiable (a mean's gradient goes through the division); an index
  outside `[0, self.shape[dim])` is dropped, so -1 padding is inert.
- `Tensor.scatter(dim, index, src, reduce=None)` is a `REPLACE` scatter:
  the last write along `dim` wins (`:1136-1168`).  A scalar src fills
  index's shape in self's dtype; `reduce="add"` is scatter_reduce `"sum"`
  and `"multiply"` is `"prod"`.  `backward()` through the reduce=None form
  raises until `REPLACE` has a gradient rule.

On another device gather, embedding and `x[idx]` keep the host numpy path
they had before, and scatter_reduce and scatter compute on the host; none
of them carries a gradient there.

## Deviations from tinygrad

- `USE_ATOMICS` is accepted and changes nothing.  tinygrad's atomic
  embedding backward clips an out-of-range id into `[0, vocab)`
  (`nn/__init__.py:356`), so padding with -1 would dump the padded
  positions' gradient into row 0; thvm drops it, as the forward reads it
  as 0 and tinygrad's default path does.
- Native gather and scatter run on the CPU backend only.
- A position or offset in an index view is compiled into the kernel (see
  Lowering); tinygrad keeps a bound Variable symbolic.
- A scatter_reduce mean divides as every thvm division does, `sum *
  (1/count)`.  tinygrad's CPU renderer turns `a * (1/b)` into a true
  division (`codegen/decomp/op.py:122-125`), so a mean can land one ulp
  away: 59 / 3 is 19.666668 in thvm and 19.666666 in tinygrad.

## Performance

Measured on the Linux CPU pod (C JIT):

| Case | Time |
|---|---|
| gather, V = 1e4, N = 1024, D = 64 | ~1.2 ms (the one-hot formulation: 0.89 s) |
| embedding forward + backward, N = 1e5, D = 64, V = 1e3 | 17 ms |
| same, V = 1e4 | 17 ms |
| same, V = 1e5 | 32 ms (the extra V x D gradient init) |

## Tests

- [tests/test_uop_gather_scatter_ir.c](../tests/test_uop_gather_scatter_ir.c):
  constructors, checks, hash-consing, rewrite and GC round trips.
- [tests/test_uop_gather.c](../tests/test_uop_gather.c),
  [tests/test_uop_scatter.c](../tests/test_uop_scatter.c): forward values
  against host loops on both CPU backends, operand forms and views,
  fusion, loop bounds, JIT replay.
- [tests/test_grad_gather_scatter.c](../tests/test_grad_gather_scatter.c):
  gradients in every chain-rule mode, finite differences, second order.
- [py/tests/test_gather_parity.py](../py/tests/test_gather_parity.py):
  ports of tinygrad's gather, embedding and index tests.
- [py/tests/test_gather_scale.py](../py/tests/test_gather_scale.py): the
  table size barely moves an embedding step.
- [py/tests/test_scatter_reduce_parity.py](../py/tests/test_scatter_reduce_parity.py):
  ports of tinygrad's scatter, scatter_add and scatter_reduce (sum, mean)
  tests, with gradients against tinygrad's autograd, numpy and finite
  differences; -1 padded indices; the ATP aggregation; TinyJit replays
  with fresh index buffers; both CPU routes.
