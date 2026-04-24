// interact/uop_kernel.c - fire a UOP_KERNEL once all inputs are TAG_TEN.
//
// Called by wnf when it enters a UOP_KERNEL term.  Recursively fires
// any upstream kernels first (the producer_kid on each input TenDesc
// points at the kernel that produces it) so every input buffer is
// populated by the time this kernel dispatches.  Then pulls input
// buffer ids from KernelEntry.input_tids[], dispatches through
// Backend->dispatch_kernel (CPU interpreter in step 12), and returns
// the output TAG_TEN from heap[loc+0].
//
// Increments ITRS: one kernel firing is one interaction (matches
// how HVM4 counts an OP2-NUM-NUM collapse).

fn void kernel_fire_by_id(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) return;
  KernelEntry *ke = &KERNELS[kid];
  if (ke->fired) return;

  // Depth-first: fire every input's producing kernel first.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid < TENS_NEXT && TENS[tid].producer_kid != 0) {
      kernel_fire_by_id(TENS[tid].producer_kid);
    }
  }

  // Resolve concrete buffer ids now that all upstream outputs are filled.
  u32 in_buf_ids[KERNEL_MAX_INPUT];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    in_buf_ids[i] = TENS[ke->input_tids[i]].buf_id;
  }
  u32 out_buf_id = TENS[ke->output_tid].buf_id;

  Backend *b = TENS[ke->output_tid].backend;
  if (b && b->dispatch_kernel) {
    b->dispatch_kernel(ke, in_buf_ids, out_buf_id);
  }
  ke->fired = 1;
  ITRS++;
}

fn Term interact_kernel(Term kernel) {
  u64  loc    = term_val(kernel);
  Term outbuf = heap_read(loc + 0);
  Term kidnum = heap_read(loc + 1);
  u32  kid    = (u32)term_val(kidnum);
  kernel_fire_by_id(kid);
  return outbuf;
}
