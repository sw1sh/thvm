// schedule/consumer_count.c - per-kernel consumer-count pass
// (sub-item a of the refcount-driven-free arc).
//
// kernel_compute_consumer_counts walks every KernelEntry and, for
// each input slot, traces back to the producing kernel via
// TENS[tid].producer_kid, then increments that producer's
// consumer_count.  Run after materialize but before kernel firing;
// the result feeds the refcount-driven free pass that frees a
// kernel's output buf when its last consumer has fired.
//
// Aliasing-aware: a view-only TenDesc (RESHAPE / EXPAND alias) keeps
// its source tensor's producer_kid via tensor_view_of, so an alias
// consumer is correctly attributed to the original producer.

fn void kernel_compute_consumer_counts(void) {
  for (u32 k = 1; k < KERNELS_NEXT; k++) {
    KERNELS[k].consumer_count = 0;
  }
  for (u32 k = 1; k < KERNELS_NEXT; k++) {
    KernelEntry *ke = &KERNELS[k];
    for (u32 i = 0; i < ke->n_inputs; i++) {
      u32 tid = ke->input_tids[i];
      if (tid == 0 || tid >= TENS_NEXT) continue;
      u32 producer = TENS[tid].producer_kid;
      if (producer == 0 || producer >= KERNELS_NEXT) continue;
      KERNELS[producer].consumer_count++;
    }
  }
}
