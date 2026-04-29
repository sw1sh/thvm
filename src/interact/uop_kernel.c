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
  // Sized to ke->n_inputs (KERNEL_MAX_INPUT is now a 1M sanity bound,
  // not a typical size, so a static [KERNEL_MAX_INPUT] would blow
  // the stack).
  u32 resolved_tids[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid == 0 && ke->input_terms[i] != 0) {
      Term r = term_resolve(ke->input_terms[i]);
      if (term_tag(r) != TAG_TEN) return;
      tid = (u32)term_val(r);
    }
    resolved_tids[i] = tid;
  }

  // No `fired` memoization.  A kernel re-fires on every interact_kernel
  // entry, the same way OP2 re-collapses on every NUM-NUM redex.  IC's
  // structural sharing (DUP/SUP) decides how many distinct redex
  // instances exist; the dispatcher just runs the program for each.
  // ASSIGN UOPs in optimizer loops mutate input buffers between
  // re-fires so the same kid produces different output values.

  // Depth-first: fire every input's producing kernel first.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = resolved_tids[i];
    if (tid < TENS_NEXT && TENS[tid].producer_kid != 0) {
      kernel_fire_by_id(TENS[tid].producer_kid);
    }
  }

  // Resolve concrete buffer ids now that all upstream outputs are filled.
  u32 in_buf_ids[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    in_buf_ids[i] = TENS[resolved_tids[i]].buf_id;
  }
  u32 out_buf_id = TENS[ke->output_tid].buf_id;

  Backend *b = TENS[ke->output_tid].backend;
  if (b && b->dispatch_kernel) {
    // JIT capture hook: when a TJit closure is mid-record, push the
    // (kid, in_buf_ids, out_buf_id) tuple onto its capture buffer
    // BEFORE dispatch fires.  The dispatch happens normally on the
    // capture pass; subsequent replays skip materialize entirely
    // and just re-dispatch the recorded sequence.
    if (jit_is_capturing()) {
      jit_capture_record((u32)(ke - KERNELS),
                         in_buf_ids, ke->n_inputs, out_buf_id);
    }
    b->dispatch_kernel(ke, in_buf_ids, out_buf_id);
  }

  // Per-fire consumer-count decref removed alongside `fired`: the
  // assumption "one fire = one full read of every input" only holds
  // for the static one-shot case, and re-firing kernels in optimizer
  // loops would over-decrement.  Future work: structural lifetime
  // analysis on the kernel DAG, not per-fire accounting.

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
