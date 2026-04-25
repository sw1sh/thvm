# Kernelization Boundaries: thvm v1 vs. tinygrad

## Status quo: one UOP, one kernel

thvm's materializer (`src/schedule/materialize{,_in_env}.c`) lowers
each UOP into its own `KernelEntry`:

- One output buffer allocated per UOP (`tensor_alloc` -> a fresh
  `TenDesc` slot).
- One `KernelEntry` slot allocated (`KERNELS[kid]`).
- One program op in the kernel (`ke->program[0]` -- now
  `program[ke->n_inputs]` after the LOAD-prefix changes from the
  UOP_LOAD arc).
- The CPU backend dispatches that single op, writing the result
  into the output buffer; consumers read it back via input slots.

The hot comment at materialize.c:9 documents the v1 stance: "for
v1 every kernel has exactly one program op". This is correct but
fundamentally wasteful at any non-trivial scale.

## Why it bites: lenet-mnist as the failure case

`TUOpConv2DLowered` (the kh*kw partial-sum chain that replaced
the bespoke UOP_CONV2D in the conv2d-removal arc) emits roughly
~200 UOPs per LeNet conv layer:

- 25 partials (5x5 kernel) x 7 ops each (SHRINK_x, RESHAPE,
  EXPAND_x, SHRINK_w, EXPAND_w, MUL, REDUCE_SUM)
- 24 ADDs to fold the partials
- 3 ops for bias broadcast (RESHAPE, EXPAND, ADD)
- ~ 200 ops total per conv

LeNet has two conv layers + ReLU + Pool + Linear chains. End-to-
end, a forward + backward through `lenet-mnist/verify.wls` blows
past:

- `KERNELS_CAP = 4096` (kernel slot table).
- Bumping that exposes a cascading `tensor_alloc` descriptor cap.

The literal symptom: `kernel_alloc: out of slots (cap=4096)` mid-
TGrad on the verify-step chain. The user-visible regression is
that `lenet-mnist/grad-check.wls` and `verify.wls` no longer run
to completion (documented under the conv2d-removal "Re-run all
CONV2D tests" task). `lenet-mnist/forward.wls` (no TGrad) still
works because forward alone fits within 4K kernels.

## How tinygrad fuses (summary)

Tinygrad's `tinygrad/engine/schedule.py` and
`tinygrad/codegen/kernel.py` (the `Kernel` class) group UOPs
into kernels using boundary classification. Roughly:

- **Elementwise** ops (UOps.ADD, MUL, NEG, etc.) DON'T break a
  kernel -- they fuse together into a single computation that
  reads inputs into registers and writes the final output once.
- **REDUCE** ops break a kernel because the reduction needs a
  shared accumulator; subsequent ops can't trivially fuse with
  the producer of the reduce's input. Practically, REDUCE marks
  the END of a fused chain.
- **Movement** ops (RESHAPE, EXPAND, PERMUTE, SHRINK, PAD, FLIP)
  are *view-only* and don't appear as separate kernels -- they
  modify the lazy-tensor's `ShapeTracker` instead. The actual
  reads from the underlying buffer use the tracker's
  stride/offset to pick out the right elements at op-fire time,
  no memory roundtrip.

The big idea: the `ShapeTracker` lets movement ops compose into a
single index expression that the eventual elementwise/reduce
kernel evaluates per output element. A 25-partial conv lowering
in tinygrad becomes ONE kernel that loops over output positions,
reads inputs through the composed index expression, accumulates
in a register, and writes the output once.

## What thvm leaves on the table

For the LeNet path specifically:

1. **Per-partial fusion.** Inside one conv-partial (ki, kj):
   `SHRINK_x`, `RESHAPE`, `EXPAND_x` produce intermediate buffers
   that immediately feed the next op. They're elementwise/movement
   only. Tinygrad would express these as a composed index
   expression and never materialize the intermediate buffers; thvm
   today materializes 4-5 buffers per partial. Across 25 partials:
   ~100 buffers per conv layer that exist for one `cpu_op_X` call
   and are then freed.

2. **Cross-partial fusion.** The 24-deep ADD chain summing the
   partials is binary-elementwise all the way down. In tinygrad
   the whole `(p00 + p01 + ... + p24)` becomes one accumulator
   inside the same kernel that emits the partials -- 25
   element-wise reads + 24 adds per output element, no
   intermediate buffers. thvm allocates 25 partial buffers + 24
   ADD-output buffers per conv.

3. **ReLU + Pool fusion.** ReLU = `MUL(x, CMPLT(0, x))` is
   pure elementwise; tinygrad fuses it into the producer of `x`
   (the conv's final ADD). thvm does it as 3 separate kernels
   (CMPLT, MUL, then the consumer reads). Pool's REDUCE_MAX is
   a natural boundary, but the RESHAPE feeding it is a movement-
   op view in tinygrad and a separate kernel in thvm.

4. **Bias broadcast.** `EXPAND(RESHAPE(bias, {C_out, 1, 1}),
   {C_out, H_out, W_out})` is two pure-movement ops; tinygrad
   resolves them into the conv's final-ADD index expression. thvm
   emits two separate memcpy-like kernels.

Rough kernel-count after fusion (LeNet conv layer):

- thvm v1 (today): ~200 kernels per conv.
- thvm + tinygrad-style fusion: ~3-5 kernels per conv (one fused
  body per REDUCE boundary).

That's a 40-60x reduction. Two convs at ~5 kernels each = ~10
kernels for the conv layers; the full LeNet forward + backward
through 4 Adam steps would comfortably fit in 4K, eliminating the
verify.wls regression.

## Measured progress (sub-items f3a-c landed)

LeNet-5 forward (input {1, 28, 28}; both convs end-to-end):

| Stage                                     | KernelEntries | Drop  |
| ----------------------------------------- | ------------- | ----- |
| Pre-fusion baseline                       | 466           | 0%    |
| f3b: RESHAPE view-only                    | 409           | 12%   |
| f3a + f3b + f3c: + EXPAND view-only       | 304           | **35%** |

The 35% drop with f3c was enough to unblock `lenet-mnist/grad-check.wls`
(forward + 1 backward through full LeNet), which previously hit
`kernel_alloc: out of slots (cap=4096)` mid-TGrad.  `verify.wls`
(forward + backward + 4 Adam steps) now also runs to completion.
Convergence is slower than the bespoke-CONV2D baseline (loss
2.61 -> 2.49 over 4 steps; prob[true] reaches 0.086 instead of
the bespoke ~0.7) -- needs investigation but is a separate
concern from the kernel-count blocker.

The remaining 304 kernels are dominated by SHRINK/PERMUTE/PAD/FLIP
movement ops (the next four ShapeTracker sub-items would target
each in turn, mirroring f3c's stride-rewrite approach).  Going
further would also unlock the elementwise-chain fusion (f1) by
eliminating much of the movement-op noise that confused f1b's
shared-subexpression detection.

## Design space for thvm fusion

Two complementary directions:

### A. Movement-op coalescing via a ShapeTracker

Mirror tinygrad's `ShapeTracker`: a per-tensor view descriptor
holding `(shape, strides, offset, mask)` that movement ops modify
in-place rather than allocating new buffers. RESHAPE / EXPAND /
PERMUTE / SHRINK / PAD / FLIP all become ShapeTracker mutations.
The producer kernel reads its inputs through the tracker; no
memcpy happens at op time.

This is the bigger architectural change but eliminates ~80% of
the kernel count and ~100% of the movement-op buffer allocations.

### B. Elementwise chain fusion

Even without ShapeTracker, the materializer could group
consecutive elementwise UOPs (ADD/MUL/NEG/...) into a single
KernelEntry whose `program[]` array runs all of them in sequence
on the same output buffer. The kernel runner already supports
multi-op programs (the `regs[]` infrastructure in
`cpu_interpret`); we just don't produce them today.

This is the smaller change -- can be done without touching
movement-op semantics. Wins ~30-50% of the kernel count for
LeNet.

## Queued follow-up work

Concrete sub-items that would land in subsequent fires (not yet
in TASKS.md -- this design note proposes them):

- [ ] **f1. Materializer groups elementwise UOPs into one kernel.**
      When walking a UOP tree, if the parent is elementwise AND its
      child is elementwise + has no other consumers (refcount = 1),
      append the child's op to the parent's `program[]` instead of
      emitting a separate kernel.  ~80 LOC of materializer changes
      + tests asserting the grouped program.

- [ ] **f2. Fuse the conv2d-lowered ADD-fold into one kernel.**
      Specifically detect the `Fold[TUOpAdd, partials, ...]` pattern
      in TUOpConv2DLowered and emit a single kernel with `n` ADDs in
      its program (rather than `n-1` separate ADD kernels).  Either
      a special-case in `TUOpConv2DLowered` (stage-2 helper
      `TUOpAddFold`?) or a general elementwise-chain pass on top of
      f1.  Should drop ~25 kernel slots per LeNet conv.

- [ ] **f3. ShapeTracker for movement ops.**  Replace RESHAPE /
      EXPAND / PERMUTE / SHRINK / PAD / FLIP runtime kernels with
      view-mutation on a `(shape, strides, offset, mask)` tuple
      attached to TenDesc.  Producer kernels read inputs via the
      tracker.  Big change (~300+ LOC across tracker, materialize,
      cpu/metal indexing) but unlocks the 80% kernel-count
      reduction.  Decompose further when it's the topmost task.

- [ ] **f4. Re-enable lenet-mnist/verify.wls.**  After f1-f3 land,
      verify.wls should fit within 4K kernels.  Re-run the 4-Adam-
      step training and confirm confidence climbs from ~0.07 to
      >~0.7.  ~10 LOC of test re-enablement; the real work is in
      f1-f3.

- [ ] **f5. Document the new fusion behavior.**  Update this file
      (docs/kernelization.md) with measured kernel counts before
      and after each of f1-f3.  Add a per-LeNet-layer breakdown.
