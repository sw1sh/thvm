// schedule/kernel_alloc.c - KernelEntry slot allocation.
//
// KERNELS[] is bump-allocated: every materialized kernel gets its own
// slot, with no freelist and no dedup of structurally equal kernels.
// The per-input arrays live on the C heap and grow geometrically, since
// nested chain-rule output can feed one kernel many inputs.

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
  ke->input_chain_composed = (u8 *)realloc(
      ke->input_chain_composed, (size_t)new_cap * sizeof(u8));
  if (ke->input_tids == NULL || ke->input_dtypes == NULL || ke->input_numels == NULL
      || ke->input_terms == NULL || ke->input_views == NULL || ke->input_chain_composed == NULL) {
    thvm_fatal("kernel_inputs_reserve: out of memory");
  }
  // Zero the new tail: 0 is the no-tensor sentinel of input_tids and
  // input_terms.
  for (u32 i = ke->inputs_cap; i < new_cap; i++) {
    ke->input_tids  [i] = 0;
    ke->input_dtypes[i] = 0;
    ke->input_numels[i] = 0;
    ke->input_terms [i] = 0;
    ke->input_chain_composed[i] = 0;
    memset(&ke->input_views[i], 0, sizeof(View));
  }
  ke->inputs_cap = new_cap;
  // Keep the lift's table as long as the slot arrays, so a slot added
  // after the lift (the unified rewrite's extend path) has a lift slot.
  kernel_lift_reserve(&ke->cached_lift, new_cap);
}

// Frees the entry's heap arrays and resets its counts and caps; safe to
// call repeatedly.
fn void kernel_free_arrays(KernelEntry *ke) {
  free(ke->input_tids);   ke->input_tids   = NULL;
  free(ke->input_dtypes); ke->input_dtypes = NULL;
  free(ke->input_numels); ke->input_numels = NULL;
  free(ke->input_terms);  ke->input_terms  = NULL;
  free(ke->input_views);  ke->input_views  = NULL;
  free(ke->input_chain_composed); ke->input_chain_composed = NULL;
  free(ke->custom_program); ke->custom_program = NULL;
  free(ke->custom_name);    ke->custom_name    = NULL;
  ke->n_inputs   = 0;
  ke->inputs_cap = 0;
  // A GC-stripped kernel can still be reached through a stale
  // producer_kid.  cpu_uop_walk sizes its inputs by cached_lift.n_inputs
  // and declines on store_root == 0, so clearing the lift makes that
  // re-fire a no-op instead of a read of freed arrays.
  kernel_lift_release(&ke->cached_lift);
}

fn u32 kernel_alloc(void) {
  if (KERNELS_NEXT >= KERNELS_CAP) {
    fprintf(stderr, "kernel_alloc: out of slots (cap=%llu)\n",
            (unsigned long long)KERNELS_CAP);
    exit(1);
  }
  u32 id = KERNELS_NEXT++;
  // A rewound slot keeps its previous owner's fields (thvm_reset frees
  // the arrays but does not zero the entry), so free and zero it.
  kernel_free_arrays(&KERNELS[id]);
  memset(&KERNELS[id], 0, sizeof(KernelEntry));
  return id;
}

// Gives back the newest slot when a tentative emit (emit_kernel_for_boundary
// bailing) does not use it; a no-op unless `kid` is the last slot.
fn void kernel_dealloc_last(u32 kid) {
  if (kid == 0) return;
  if (kid + 1 != KERNELS_NEXT) return;
  kernel_free_arrays(&KERNELS[kid]);
  memset(&KERNELS[kid], 0, sizeof(KernelEntry));
  KERNELS_NEXT = kid;
}
