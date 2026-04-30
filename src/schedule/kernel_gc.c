// schedule/kernel_gc.c -- buffer-liveness sweep over the KernelEntry
// arena.  Frees the per-kernel heap arrays (input_*, program) for
// every kernel whose output buffer was released by cpu_buf_pool's
// rollback at end of realize.  Bounds the per-kernel program/input
// memory growth across long training loops -- without this each
// Adam step adds ~2.5K kernels' worth of program/input arrays
// (KProgOp[]) which never shrink.
//
// We do NOT release the output TenDesc itself (no tensor_release)
// and we do NOT recycle slot ids (no freelist pop in kernel_alloc):
// kid references survive in heap UOP_KERNEL terms, and a future
// realize that touches a still-pinned TenDesc could DFS-fire its
// producer kid.  Freeing only the program arrays keeps that path
// safe -- a re-fire of a stripped kernel just no-ops (n_inputs=0,
// n_ops=0) and doesn't corrupt buffers.  See M4 in
// docs/plans/beautiful_mnist_parity.md for the architectural
// trade-offs and the orphan-kernel-emission issue that blocks the
// more ambitious slot-reuse design.
//
// Liveness signal: CPU_BUFS[buf_id].refcount.  After
// cpu_buf_pool_rollback_with_preserve runs (in thvm_realize), a
// non-preserved owning buf has been pushed to the cpu_buf freelist
// with refcount=0, and a non-preserved external buf has been
// cpu_buf_free'd.  Both produce refcount==0 -- a robust "this
// buffer is no longer reachable from any preserved root" signal
// regardless of whether the surface UOP graph happens to mention
// the kernel's output.
//
// Wired into thvm_realize after each user-facing realize so the
// arena stays bounded across N_STEPS.  Default-on; THVM_KGC=0
// disables for benchmarks that want the old "leak everything"
// behaviour.

fn u32 kernel_gc_freelist_pop(void) {
  // Slot reuse is intentionally disabled: surface UOP_KERNEL terms
  // and TENS[].producer_kid references retain pointers to old kid
  // slots after sweep, and a later realize can DFS-fire them via
  // kernel_fire_by_id.  Stripping the program arrays makes the
  // re-fire a no-op; reusing the slot for a different kernel would
  // dispatch the new program against the old caller's input/output
  // buffers.  Keep slots monotonic until a future fix tightens the
  // re-fire path.
  return 0;
}

fn void kernel_gc_reset(void) {
  // No persistent state to reset (mark bitmap and freelist were
  // removed alongside the slot-reuse path).  Kept as a public hook
  // so thvm_init / thvm_free can call it without conditional code.
}

fn u32 kernel_gc_sweep(Term result) {
  (void)result;
  if (KERNELS_NEXT <= 1) return 0;
  // Metal-active builds have CPU_BUFS == NULL; nothing to sweep.
  if (CPU_BUFS == NULL) return 0;

  u32 freed = 0;
  for (u32 kid = 1; kid < KERNELS_NEXT; kid++) {
    KernelEntry *ke = &KERNELS[kid];
    // Already-stripped slot (idempotent across multiple sweeps).
    if (ke->n_inputs == 0 && ke->n_ops == 0 && ke->program == NULL
        && ke->input_tids == NULL) continue;
    if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) continue;
    u32 buf_id = TENS[ke->output_tid].buf_id;
    // buf_id == 0 means the TenDesc was already released; treat as
    // dead.  Otherwise the buffer is alive iff its CPU_BUFS slot has
    // refcount > 0 -- after pool rollback, freed bufs sit at
    // refcount=0 (freelist push or cpu_buf_free both leave it there).
    int alive = 0;
    if (buf_id != 0 && buf_id < CPU_BUFS_NEXT) {
      alive = (CPU_BUFS[buf_id].refcount > 0);
    }
    if (alive) continue;
    // Buffer is dead.  Strip the program/input arrays; leave
    // output_tid pointing at the (now-empty) TenDesc so existing
    // kid references on the heap can resolve without faulting.
    kernel_free_arrays(ke);
    freed++;
  }
  return freed;
}
