# Memory footprint during training

What this doc answers: "how much memory does thvm allocate per
LeNet training step today, and what would proper buffer reuse
gain us?"

## Status quo: per-op allocation, no reuse

`tensor_alloc` is called per-op during materialize.  Each
`KernelEntry`'s output tensor gets its own freshly-allocated
buffer.  Buffers are never freed mid-session; they live until
`thvm_free()` (full reset).

Concretely, for one LeNet forward + 1 backward (no Adam update):

| Phase                             | TenDescs added | Buf bytes added |
| --------------------------------- | -------------- | --------------- |
| `TInit[]`                         | 0              | 0               |
| MNIST 1-sample batch              | 2              | 3.1 KiB         |
| Glorot host-init (8 weights)      | 0              | 0 (host-only)   |
| Tensor upload (1 input + 8 wts)   | 9              | 1683.9 KiB      |
| Forward + loss materialize        | 480            | 14292.6 KiB     |
| `TGrad[loss, x]` materialize      | 20             | 39.8 KiB        |
| **Total**                         | **511**        | **15.65 MiB**   |

(Numbers from `wl/Examples/lenet-mnist/memory-probe.wls` on a
fresh `TInit` run.  `TenDescs` = `TENS_NEXT - 1`; `Buf bytes` =
sum of live `CPU_BUFS[i].nbytes` where `refcount > 0`.)

The forward pass dominates: 480 TenDescs and 14.3 MiB of
intermediate buffers.  `TGrad[loss, x]` only adds 20 / 40 KiB
because it primarily emits new compute graphs that get walked at
the next materialize -- backward IS lazy here.  A full Adam step
adds the backward materialization PLUS 8 new tensors for the
weight gradients PLUS 16 host-side moment estimates (m, v) PLUS
8 weight-update buffers, so a full training step probably
reaches 25-30 MiB.

## Where the bytes go

The 14.3 MiB of forward-pass intermediates breaks down roughly:

- **Conv1 output** (20 x 24 x 24 f32 = 45 KiB) materialized
  through ~125 partial / fold buffers averaging ~kilobyte each
  -- conv-lowered's kh*kw partial-sum chain allocates one
  intermediate per (ki, kj) pair.
- **Conv2 output** (50 x 8 x 8 f32 = 12.5 KiB) materialized
  through ~150 partial / fold buffers, larger because LeNet's
  inner conv is 20->50 channels (the partials are
  {50, 8, 8} = 12.5 KiB each, and there's 25 of them = 312 KiB
  per partial slice).
- **Pool / ReLU / Linear** outputs are small (KiB-level each).
- **Softmax + CE-loss intermediates** are ~10 elements each
  (negligible).

The biggest absolute consumer is **conv2's per-partial buffers**:
25 partials at 12.5 KiB plus 24 ADD-fold outputs at 12.5 KiB =
~600 KiB.  Conv1's per-partial buffers are smaller (~1 KiB each
* 50 = ~50 KiB).

## What tinygrad would do

Tinygrad's runtime + scheduler give back most of this memory
through three mechanisms:

1. **Lazy buffer reuse** -- intermediate buffers that are read
   exactly once are immediately freed (or never allocated:
   the producer writes directly into the consumer's input slot).
   thvm has no buffer-lifetime tracking; intermediates live
   forever within a `TInit` session.

2. **Movement-op view-only** (we landed RESHAPE + EXPAND in the
   f3 arc; SHRINK / PERMUTE / PAD / FLIP still allocate).
   View-only movement ops share buffers with their source --
   no new allocation.

3. **Kernel fusion** (the f1 arc, currently blocked) -- when
   N elementwise ops fuse into one kernel, they share ONE
   output buffer instead of N.  For our 24-deep ADD-fold per
   conv, that would drop ~24 buffers down to 1.

## Concrete reuse-pass opportunities

In rough order of impact:

- [ ] **Per-step buffer pool**.  Add a high-water-mark allocator
      that keeps track of bufs allocated during one materialize
      call and frees them when the materialize root's wnf
      finishes.  Eliminates the "bufs live until thvm_free"
      problem; ~50% memory reduction per step (we don't keep
      forward intermediates alive after the backward pass
      reads them).  ~150 LOC across `cpu_buf_alloc.c` +
      `kernel_fire_by_id`.

- [ ] **Refcount-driven free**.  When the last consumer reads a
      kernel's output buffer, decref + free.  Requires a
      consumer-count pass during materialize (similar to f1b's
      `count_kernel_consumers` helper).  ~80 LOC plus the
      consumer-count infrastructure.  Drops conv-partial
      memory by ~5x (each partial is consumed by exactly one
      ADD-fold position).

- [ ] **Movement-op view-only for SHRINK / PERMUTE / PAD / FLIP**.
      Mirror f3b/c.  Mostly orthogonal to the above two; once
      these land, the per-conv buffer count drops 50-70 buffers.

- [ ] **Adam-state arena**.  Today Adam's `m` and `v` host
      arrays are allocated per-step (`TAdamHostStep` re-allocs).
      Keep them alive across steps in a session-scoped store.
      ~40 LOC; small bytes-per-step gain but reduces host-side
      pressure.

The cron-loop should pick up these as queued sub-items in
TASKS.md when it gets to the memory work.

## Per-step pool boundary lands (sub-items a + b)

Status: infrastructure complete, ZERO measured savings yet.

- `cpu_buf_pool_begin() -> wm` + `cpu_buf_pool_rollback_with_preserve(wm)`
  in src/backend/cpu/buf_pool.c (sub-item a).
- `thvm_realize` (src/schedule/realize.c) wraps materialize +
  wnf with the pool boundary; `mark_preserved_chain` walks the
  result tensor's producer_kid -> input_tids tree marking every
  reachable buf as preserved (sub-item b).
- WL TRealize calls the new `thvm_wl_realize` bridge as one C
  round-trip.

Memory-probe.wls before/after:

| Stage              | Before | After  | Delta |
| ------------------ | ------ | ------ | ----- |
| TenDescs           | 511    | 511    | 0     |
| Buf bytes (KiB)    | 16 022 | 16 022 | 0     |
| KernelEntries      | 330    | 330    | 0     |

The conservative whole-producer-chain preserve is the source of
the zero delta: every forward intermediate is reachable from the
loss tensor (transitively, via the chain of producer_kids), so
EVERY intermediate gets marked preserved and rollback frees
nothing.

We tried the aggressive "preserve only the result.buf_id"
variant.  It broke nn.wlt (5 failures) and segfaulted; some
downstream paths (TGrad chain rule, view-only alias chains)
read intermediate bufs after wnf returns, before the next
TRealize starts.

The right next step is **refcount-driven free** (the next item
in the reuse-pass arc): track per-buf consumer count during
materialize, decref when each consumer kernel fires, free at
refcount = 0.  That cleanly separates "result bufs to keep" from
"intermediate bufs that have been READ by all their consumers"
without the producer-chain-preservation paradox.

The pool boundary infrastructure is correct + covered by tests;
it'll provide actual savings once the refcount work lands and
swaps the preserve-walk for "free what hit refcount=0".

## Refcount-driven free arc (sub-items a + b + c)

Status: infrastructure complete end-to-end; STILL ZERO measured
savings.

What landed:

- `KernelEntry.consumer_count` (src/thvm.h) + a single-pass
  `kernel_compute_consumer_counts` (src/schedule/consumer_count.c)
  attributes each input read back to its producing kernel (via
  `TENS[tid].producer_kid`), correctly handling view-aliased
  TenDescs (RESHAPE/EXPAND aliases inherit `producer_kid`).
- A post-dispatch decref hook in `kernel_fire_by_id` decrements
  the producer's count and, on a real 1->0 transition, marks the
  producer's output buf via `cpu_buf_mark_freeable`.
- `thvm_realize` calls `kernel_compute_consumer_counts` between
  materialize + wnf so the freeable signal is computed correctly
  per realize, and clears the freeable bits on the way out.
- `cpu_buf_mark_freeable` / `cpu_buf_clear_freeable` /
  `CpuBuf.freeable` flag (sibling to `preserved`).

What did NOT land: the rollback swap.  `thvm_realize` still uses
`cpu_buf_pool_rollback_with_preserve`, which conservatively keeps
every buf reachable from the result's producer chain.  The
freeable bits are computed correctly but ignored by the rollback.

Why: the aggressive variant (drop preserve walk; free
`freeable && !preserved`) segfaults `nn.wlt` patterns of the form
`{TRealize[loss], TRealize[TGrad[loss, x]]}`.  The second TRealize
materializes a backward graph that emits new kernels referencing
forward TenDescs whose bufs were freed by the first realize.
The kernel `fired` flag short-circuits re-firing, so the buf
can't be reconstructed.  This is the same dead-end the prior
"preserve only result.buf" attempt hit; the two paths converge
because both lack a heap-rooted preserve mechanism.

Memory-probe.wls (post sub-item c integration):

| Stage              | Value         |
| ------------------ | ------------- |
| TenDescs           | 511           |
| Buf bytes          | 16 022.5 KiB  |
| KernelEntries      | 330           |

Same as the pre-refcount baseline -- preserve walk dominates.

What unblocks the savings: a heap-rooted preserve pass that walks
HEAP[0..HEAP_NEXT) for live `TAG_TEN`/`UOP_*` cells, collects
their referenced TenDescs, and pins those bufs.  This replaces
the producer-chain conservative walk with a precise live-reference
walk; intermediate forward kernels whose outputs are no longer
referenced anywhere in the heap (and whose count hit zero) get
reclaimed.  Sized at one new schedule/heap_rooted_preserve.c plus
swapping the rollback variant in thvm_realize -- queued as the
next reuse-pass arc item.

## Probe script

`wl/Examples/lenet-mnist/memory-probe.wls` reports per-phase
buffer / TenDesc / KernelEntry counts.  Re-run after any of the
above lands to measure regression / progress.

C surface:

- `TTensCount[]`     -- `TENS_NEXT - 1`
- `TTotalBufBytes[]` -- sum of live `CPU_BUFS[i].nbytes`
- `TKernelCount[]`   -- `KERNELS_NEXT`
