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
