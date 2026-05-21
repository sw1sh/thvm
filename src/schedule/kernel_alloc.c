// schedule/kernel_alloc.c - reserve a fresh KernelEntry slot.
//
// Bump-only allocation in KERNELS[] (no freelist, no kernel-cache
// keyed by signature yet).  Each materialized kernel gets its own
// slot regardless of structural duplication; a future content-
// addressed cache could dedup but doesn't yet.
//
// KernelEntry's input arrays are heap-allocated pointers that grow
// on demand via kernel_inputs_reserve.  This was originally a fixed
// [64] inline layout; deeply-nested chain-rule output (e.g.
// nth-order TGrad) blew past that cap and bailed materialize.
// Heap arrays grow geometrically, free on dealloc.

fn void kernel_inputs_reserve(KernelEntry *ke, u32 needed) {
  if (needed <= ke->inputs_cap) return;
  if (needed > KERNEL_MAX_INPUT) {
    fprintf(stderr, "kernel_inputs_reserve: needed=%u exceeds sanity bound %llu\n",
            needed, (unsigned long long)KERNEL_MAX_INPUT);
    exit(1);
  }
  u32 new_cap = ke->inputs_cap == 0 ? KERNEL_INIT_INPUT : ke->inputs_cap * 2;
  while (new_cap < needed) new_cap *= 2;
  ke->input_tids   = (u32  *)realloc(ke->input_tids,   (size_t)new_cap * sizeof(u32));
  ke->input_dtypes = (u32  *)realloc(ke->input_dtypes, (size_t)new_cap * sizeof(u32));
  ke->input_numels = (u32  *)realloc(ke->input_numels, (size_t)new_cap * sizeof(u32));
  ke->input_terms  = (Term *)realloc(ke->input_terms,  (size_t)new_cap * sizeof(Term));
  ke->input_views  = (View *)realloc(ke->input_views,  (size_t)new_cap * sizeof(View));
  ke->input_source_buffer_ids = (u32 *)realloc(
      ke->input_source_buffer_ids, (size_t)new_cap * sizeof(u32));
  // Per-slot ShapeTracker-chain composed flag (set by rangeify when it
  // folds the prior_views chain into the kernel INDEX; the CPU / Metal
  // backends gate the per-input pre-mat skip on it).
  ke->input_chain_composed = (u8 *)realloc(
      ke->input_chain_composed, (size_t)new_cap * sizeof(u8));
  // Zero new tail so unused slots stay clean (matters for input_tids=0
  // sentinel and input_terms=0 sentinel).
  for (u32 i = ke->inputs_cap; i < new_cap; i++) {
    ke->input_tids  [i] = 0;
    ke->input_dtypes[i] = 0;
    ke->input_numels[i] = 0;
    ke->input_terms [i] = 0;
    ke->input_source_buffer_ids[i] = 0;
    ke->input_chain_composed[i]    = 0;
    memset(&ke->input_views[i], 0, sizeof(View));
  }
  ke->inputs_cap = new_cap;
}

// Free the heap-allocated arrays in a KernelEntry; reset counts and
// caps to zero.  Call when releasing a kernel slot or resetting the
// runtime context.  Safe to call repeatedly (NULL-tolerant).
fn void kernel_free_arrays(KernelEntry *ke) {
  free(ke->input_tids);   ke->input_tids   = NULL;
  free(ke->input_dtypes); ke->input_dtypes = NULL;
  free(ke->input_numels); ke->input_numels = NULL;
  free(ke->input_terms);  ke->input_terms  = NULL;
  free(ke->input_views);  ke->input_views  = NULL;
  free(ke->input_source_buffer_ids); ke->input_source_buffer_ids = NULL;
  free(ke->input_chain_composed);    ke->input_chain_composed    = NULL;
  ke->n_inputs   = 0;
  ke->inputs_cap = 0;
  // Invalidate the cached lift so a later re-fire of this stripped slot
  // no-ops cleanly.  cpu_uop_walk keys its input count off
  // cached_lift.n_inputs (not ke->n_inputs) and declines when
  // store_root==0; without this a GC-stripped kernel reached via a
  // stale TenDesc.producer_kid would walk the dangling lift with
  // freed input arrays and read uninitialised buffer ids.
  ke->cached_lift.store_root = 0;
  ke->cached_lift.out_buf    = 0;
  ke->cached_lift.n_inputs   = 0;
}

fn u32 kernel_alloc(void) {
  if (KERNELS_NEXT >= KERNELS_CAP) {
    fprintf(stderr, "kernel_alloc: out of slots (cap=%llu)\n",
            (unsigned long long)KERNELS_CAP);
    exit(1);
  }
  u32 id = KERNELS_NEXT++;
  // Free any heap arrays from a previous owner of this slot (the
  // ctx free path zero's KERNELS, but a kernel_dealloc_last that
  // didn't run would leave them dangling otherwise).  Then zero
  // the entry; pointer fields become NULL, so reserve will alloc
  // fresh on first growth.
  kernel_free_arrays(&KERNELS[id]);
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
  kernel_free_arrays(&KERNELS[kid]);
  memset(&KERNELS[kid], 0, sizeof(KernelEntry));
  KERNELS_NEXT = kid;
}
