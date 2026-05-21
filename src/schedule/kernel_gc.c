// schedule/kernel_gc.c -- buffer-liveness sweep over the KernelEntry
// arena.  Frees the per-kernel input_* heap arrays for every kernel
// whose output buffer was released by cpu_buf_pool's rollback at end
// of realize.  Bounds the per-kernel input memory growth across long
// training loops -- without this each Adam step adds ~2.5K kernels'
// worth of input arrays which never shrink.
//
// We do NOT release the output TenDesc itself (no tensor_release)
// and we do NOT recycle slot ids (no freelist pop in kernel_alloc):
// kid references survive in heap UOP_KERNEL terms, and a future
// realize that touches a still-pinned TenDesc could DFS-fire its
// producer kid.  Freeing only the input arrays keeps that path
// safe -- a re-fire of a stripped kernel just no-ops (n_inputs=0)
// and doesn't corrupt buffers.  See M4 in
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

fn u32 kernel_gc_sweep(Term result) {
  (void)result;
  if (KERNELS_NEXT <= 1) return 0;
  // Metal-active builds have CPU_BUFS == NULL; nothing to sweep.
  if (CPU_BUFS == NULL) return 0;

  u32 freed = 0;
  for (u32 kid = 1; kid < KERNELS_NEXT; kid++) {
    KernelEntry *ke = &KERNELS[kid];
    // Already-stripped slot (idempotent across multiple sweeps).
    if (ke->n_inputs == 0 && ke->input_tids == NULL) continue;
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
    if (alive) {
      // The output buffer survived this realize's pool rollback, so it
      // is a committed leaf for subsequent passes.  Detach its producer
      // kid: a later reuse must read the preserved buffer directly, not
      // re-fire (recompute) the producer chain.  The chain's dead
      // intermediates are stripped by this same sweep, so a re-fire
      // would recompute from freed buffers and yield wrong values.
      // Mirrors tinygrad replacing a realized tensor's lazydata with a
      // BUFFER leaf.
      TENS[ke->output_tid].producer_kid = 0;
      continue;
    }
    // This sweep keys liveness off CPU_BUFS; non-CPU backends have
    // their own buffer tables, so leave their kernels inspectable.
    if (TENS[ke->output_tid].backend != &CPU_BACKEND) {
      continue;
    }
    // Buffer is dead.  Strip the input arrays; leave output_tid
    // pointing at the (now-empty) TenDesc so existing kid
    // references on the heap can resolve without faulting.
    kernel_free_arrays(ke);
    freed++;
  }
  return freed;
}
