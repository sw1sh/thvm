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
  // Spliced kernels (sub-item f1a of the kernel-fusion arc) had
  // their program ops absorbed into a consumer; the consumer
  // produces this kernel's output buffer too, so dispatching here
  // would just compute the same thing twice.
  if (ke->spliced) return;

  // Resolve any symbolic input slots first (input_tids[i] == 0 +
  // input_terms[i] != 0).  These come from materialize_uop_in_env
  // when a child was a free TAG_VAR; by fire time, APP-LAM beta
  // should have substituted the binder and term_resolve walks the
  // SUB-bit chain to reach a TAG_TEN.  If we still don't see a
  // concrete TEN, the kernel can't fire -- bail.
  u32 resolved_tids[KERNEL_MAX_INPUT];
  int has_symbolic = 0;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid == 0 && ke->input_terms[i] != 0) {
      Term r = term_resolve(ke->input_terms[i]);
      if (term_tag(r) != TAG_TEN) return;
      tid = (u32)term_val(r);
      has_symbolic = 1;
    }
    resolved_tids[i] = tid;
  }

  // Concrete-only kernels fire once; symbolic-input kernels can be
  // re-fired with different bindings each time.
  if (!has_symbolic && ke->fired) return;

  // Depth-first: fire every input's producing kernel first.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = resolved_tids[i];
    if (tid < TENS_NEXT && TENS[tid].producer_kid != 0) {
      kernel_fire_by_id(TENS[tid].producer_kid);
    }
  }

  // Resolve concrete buffer ids now that all upstream outputs are filled.
  u32 in_buf_ids[KERNEL_MAX_INPUT];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    in_buf_ids[i] = TENS[resolved_tids[i]].buf_id;
  }
  u32 out_buf_id = TENS[ke->output_tid].buf_id;

  Backend *b = TENS[ke->output_tid].backend;
  if (b && b->dispatch_kernel) {
    b->dispatch_kernel(ke, in_buf_ids, out_buf_id);
  }

  // Decref hook (sub-item b of the refcount-driven free arc).
  // Inputs have been read; tell each producer kernel "one more
  // consumer is done with you."  When a producer's count hits zero,
  // its output buf is no longer needed -- mark it freeable so the
  // next pool rollback can reclaim it.  Skipped for symbolic
  // kernels because they may re-fire with different bindings, where
  // the static consumer_count from kernel_compute_consumer_counts
  // doesn't correspond 1:1 to actual reads.
  if (!has_symbolic) {
    for (u32 i = 0; i < ke->n_inputs; i++) {
      u32 tid = ke->input_tids[i];
      if (tid == 0 || tid >= TENS_NEXT) continue;
      u32 producer = TENS[tid].producer_kid;
      if (producer == 0 || producer >= KERNELS_NEXT) continue;
      KernelEntry *pe = &KERNELS[producer];
      if (pe->consumer_count > 0) pe->consumer_count--;
      if (pe->consumer_count == 0) {
        u32 prod_buf = TENS[pe->output_tid].buf_id;
        cpu_buf_mark_freeable(prod_buf);
      }
    }
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
