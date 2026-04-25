// schedule/kernel_alloc.c - reserve a fresh KernelEntry slot.
//
// Bump-only allocation in KERNELS[] for step 12 (no freelist, no
// kernel-cache keyed by signature yet).  Each materialized kernel
// gets its own slot regardless of structural duplication; step 14
// adds content-addressed caching.

fn u32 kernel_alloc(void) {
  if (KERNELS_NEXT >= KERNELS_CAP) {
    fprintf(stderr, "kernel_alloc: out of slots (cap=%llu)\n",
            (unsigned long long)KERNELS_CAP);
    exit(1);
  }
  u32 id = KERNELS_NEXT++;
  memset(&KERNELS[id], 0, sizeof(KernelEntry));
  return id;
}

// Release the most-recently-allocated kernel slot when a tentative
// emit (e.g. materialize_kernel_inlined's bail path) decides not
// to use it.  No-op if `kid` isn't the head of the bump pointer
// (some other allocation happened in between).
fn void kernel_dealloc_last(u32 kid) {
  if (kid == 0) return;
  if (kid + 1 != KERNELS_NEXT) return;
  memset(&KERNELS[kid], 0, sizeof(KernelEntry));
  KERNELS_NEXT = kid;
}
