# Backprop roadmap — what's missing for Adam-on-LeNet

> **Note:** This doc describes an earlier (target-in-cell, JVP-equivalent)
> grad design.  The current implementation uses dup-like cells with
> gy threading and is documented in [`grad.md`](grad.md).  This file
> is kept for historical reference.

## TL;DR

`interact_grad` (in `src/interact/uop_grad.c`) currently rewrites
`GRAD[y, gy, target]` for **8 of the 19 UOps**.  Every other opcode
falls through to a `fprintf(stderr, "interact_grad: unhandled UOp
opcode %u\n", ...)` + `grad_zero(target)` -- silently incorrect.

Adam-on-LeNet needs grad rules added for **8 more UOps**, in roughly
this order:

  1. `UOP_RESHAPE`
  2. `UOP_EXPAND`
  3. `UOP_CMPLT`
  4. `UOP_EXP2`
  5. `UOP_RECIP`
  6. `UOP_LOG2`
  7. `UOP_REDUCE` with `kind = MAX`
  8. `UOP_CONV2D`

The first six are 10-30 LOC each + one parity test apiece.  The last
two are bigger and warrant standalone task items.

## What's already in `interact_grad`

VJP semantics: `GRAD[y, gy, target] = (∂y/∂target) · gy`, evaluated
one structural layer per fire.

| Pattern                   | Rule                                                                   |
|---------------------------|------------------------------------------------------------------------|
| `y === target`            | `expand_to_target(gy, target)`                                         |
| `y` is `TAG_TEN`/`TAG_NUM`| `expand_to_target(CONST(0), target)`                                   |
| `UOP_KERNEL`              | recurse on `KernelEntry.source_uop` (kernels are transparent to grad)  |
| `UOP_CONST`               | zero                                                                   |
| `UOP_ADD[a, b]`           | `ADD[GRAD(a, gy, t), GRAD(b, gy, t)]`                                  |
| `UOP_MUL[a, b]`           | `ADD[GRAD(a, MUL[b, gy], t), GRAD(b, MUL[a, gy], t)]`                  |
| `UOP_NEG[a]`              | `GRAD(a, NEG[gy], t)`                                                  |
| `UOP_REDUCE[a, SUM, _]`   | `GRAD(a, expand_to_target(gy, t), t)`                                  |

`REDUCE` with `kind = MAX` falls into the default branch -- still a
gap.

## The forward chain LeNet uses

`TLeNet[]` materialises into this UOP sequence (per sample, ignoring
batches):

```
input
  -> UOP_CONV2D       (Conv 6@5x5)
  -> UOP_MUL,         (ReLU = MUL[x, CMPLT(0, x)])
     UOP_CMPLT,
     UOP_EXPAND       (CMPLT broadcasts a CONST(0))
  -> UOP_RESHAPE,     (PoolingLayer Max 2x2; pool decomposes into
     UOP_REDUCE/MAX    reshape-to-windows then REDUCE MAX over the
                       window axis)
  -> UOP_CONV2D       (Conv 16@5x5)
  -> ReLU again       (MUL/CMPLT/EXPAND)
  -> Pool again       (RESHAPE + REDUCE/MAX)
  -> UOP_RESHAPE      (FlattenLayer)
  -> UOP_MUL,         (LinearLayer 120: matmul = MUL+REDUCE_SUM,
     UOP_REDUCE/SUM    bias add = ADD)
  -> ReLU again
  -> Linear again     (LinearLayer 10)
  -> UOP_EXP2,        (SoftmaxLayer)
     UOP_REDUCE/SUM,
     UOP_RECIP,
     UOP_MUL,
     UOP_EXPAND
```

Loss (`TCrossEntropyLoss[probs, target]`) tacks on:

```
  -> UOP_LOG2,        (TLog = MUL[LOG2(x), CONST(ln 2)])
     UOP_MUL,
  -> UOP_MUL          (target * log(probs))
  -> UOP_REDUCE/SUM
  -> UOP_NEG
```

Every UOp in that chain needs a grad rule for the loss to actually
backprop into the weights.  Adam itself is a *gradient consumer* --
its own ops (ADD/MUL/SQRT/RECIP applied to the cotangent) don't get
differentiated, so no grad rules are required for the optimizer's
internals.

## Per-UOp rules to add

### 1. `UOP_RESHAPE` -- ~15 LOC

`GRAD[RESHAPE(a, new_shape), gy, t]`
  = `GRAD[a, RESHAPE(gy, a.shape), t]`

Reshape is a metadata-only view in the runtime (memcpy in the CPU
kernel; identity over data for grad purposes).  Cotangent has the
same numel as the input, just needs to be reshaped back.

Unblocks: Flatten, the reshape that pooling decomposes into.

### 2. `UOP_EXPAND` -- ~25 LOC

`GRAD[EXPAND(a, new_shape), gy, t]`
  = `GRAD[a, REDUCE_SUM along expanded axes (gy), t]`

Where "expanded axes" are the axes whose extent in `a.shape` is 1
but `new_shape` is >1.  Multiple expanded axes need successive
REDUCE_SUM ops (one per axis).

Unblocks: softmax (the per-row sum gets EXPAND'd back to per-element
shape so it can divide), CMPLT in ReLU (the CONST(0) is EXPAND'd to
input shape).

### 3. `UOP_CMPLT` -- ~5 LOC

`GRAD[CMPLT(a, b), gy, t]` = `grad_zero(t)`

CMPLT produces a boolean mask; neither input is "differentiable" wrt
the comparison.  In the ReLU pattern `MUL[x, CMPLT(0, x)]`, the
gradient flows correctly through the surrounding `MUL` rule:
`GRAD[MUL[x, mask], gy]` becomes `GRAD[x, MUL[mask, gy]]` +
`GRAD[mask, MUL[x, gy]]`, and that second term collapses to zero.

Unblocks: ReLU.

### 4. `UOP_EXP2` -- ~15 LOC

`d/dx (2^x) = 2^x · ln(2)`, so:

`GRAD[EXP2(a), gy, t]`
  = `GRAD[a, MUL[gy, MUL[EXP2(a), CONST(ln 2)]], t]`

Unblocks: softmax, every elementwise exp.  TTanh too.

### 5. `UOP_RECIP` -- ~15 LOC

`d/dx (1/x) = -1/x²`, so:

`GRAD[RECIP(a), gy, t]`
  = `GRAD[a, MUL[gy, NEG[MUL[RECIP(a), RECIP(a)]]], t]`

Unblocks: softmax denominator.

### 6. `UOP_LOG2` -- ~15 LOC

`d/dx (log₂ x) = 1/(x · ln 2)`, so:

`GRAD[LOG2(a), gy, t]`
  = `GRAD[a, MUL[gy, MUL[RECIP(a), CONST(1/ln 2)]], t]`

Unblocks: TLog -> TCrossEntropyLoss.

### 7. `UOP_REDUCE` with `kind = MAX` -- ~40 LOC + one new primitive

Gradient is one-hot at the argmax position along the reduced axis:

`GRAD[REDUCE_MAX(a, axis), gy, t]`
  = `GRAD[a, MUL[gy_lifted, MASK], t]`

where `MASK` has 1.0 where `a == REDUCE_MAX(a, axis)` (broadcast
back) and 0.0 elsewhere.  Computing MASK needs an equality check
that doesn't currently exist as a UOp -- options:

  - Add `UOP_CMPEQ` (cheap; mirrors `UOP_CMPLT`).
  - Or compose: `1 - CMPLT(a, max) - CMPLT(max, a)` (works but
    materialises three intermediate tensors).

Prefer adding `UOP_CMPEQ` -- one shader, one CPU kernel, simpler
lowering.  Then the rule is straightforward.

Unblocks: Max-pooling backprop.

### 8. `UOP_CONV2D` -- the heaviest

Forward is `output[c_out, y, x] = sum_{c_in, ky, kx} input[c_in, y+ky, x+kx] · weights[c_out, c_in, ky, kx] + bias[c_out]`.

Three gradient terms:

  - **`grad_input`**: full convolution of `gy` with the *flipped*
    weights (transposed conv).  Same arithmetic as forward CONV2D
    but with padding = `kh-1, kw-1` and weights flipped along the
    spatial axes.
  - **`grad_weights`**: cross-correlation of `input` with `gy`,
    summed over the batch / spatial output positions.
  - **`grad_bias`**: `REDUCE_SUM[gy]` over batch + spatial axes.

Practical landing path:

  1. Add `UOP_FLIP` support (it exists as an opcode but currently
     no kernel + no grad rule) so the transposed-conv path can
     express weight flipping cleanly.
  2. Add `UOP_PAD` kernel (also opcode-only today) so transposed
     conv has an explicit zero-pad rather than open-coding.
  3. Implement the rule as `ADD` of three `GRAD` sub-terms, one
     per input slot, each lowered into existing CONV2D / REDUCE /
     PAD / FLIP primitives.

This is the only rule that justifies a multi-fire task arc.

## Why this order

  - **Steps 1-3 (RESHAPE, EXPAND, CMPLT)** are mechanical and
    unblock backprop through Flatten + ReLU + Softmax broadcast.
    Three small commits, one parity test each, no new primitives.

  - **Steps 4-6 (EXP2, RECIP, LOG2)** complete softmax +
    cross-entropy backprop on a Conv-free MLP.  At this point you
    can train a 2-layer fully-connected MNIST classifier
    end-to-end on CPU as the first real validation that the
    chain rule plumbing through `interact_grad` actually
    converges.  *That MLP is the right intermediate milestone --
    far simpler to debug than LeNet, but exercises every grad
    rule downstream of CONV2D.*

  - **Step 7 (REDUCE_MAX)** introduces one new primitive
    (`UOP_CMPEQ`) and unblocks max-pooling.  After this you can
    train an MLP-on-pooled-features network -- still no Conv but
    closer to LeNet's shape pipeline.

  - **Step 8 (CONV2D)** is the final unlock.  Best done last so
    that when the Conv backprop is wrong, every other grad rule
    on the chain has already been parity-tested individually and
    the bug is isolated to the Conv code.

## Testing strategy per rule

For each rule, add a parity test alongside the existing ones in
`tests/test_grad.c`:

  1. Build a tiny computation `y = f(x)` involving the new UOp.
  2. Compute `GRAD[y, ones, x]` via `interact_grad`.
  3. Compute the same gradient via numerical finite differences
     (`(f(x+ε) - f(x-ε)) / 2ε`, ε ≈ 1e-3).
  4. Assert max absolute error < 1e-3.

The existing `test_grad` tests for ADD/MUL/NEG/REDUCE_SUM follow
this pattern -- new tests should mirror them.

## Out of scope

  - **Higher-order gradients** (grad-of-grad).  The current
    `interact_grad` only emits forward-mode UOps, so a second
    GRAD over its output would in principle work, but no test
    covers it and Adam doesn't need it.
  - **Sparse / structured cotangents**.  All grads are dense
    tensors of the input shape; no sparsity tracking.
  - **Gradient checkpointing**.  Whole forward graph is kept in
    the heap; recomputation isn't a thing yet.

These are real concerns for production training but irrelevant
for getting Adam-on-LeNet-on-MNIST to converge.
