// schedule/consumer_count.c - per-kernel consumer counts.
//
// A kernel's consumer_count is the number of input slots, over all
// kernels, whose tensor traces back to it through TENS[tid].producer_kid.
// A view alias inherits its source's producer_kid (tensor_view_of), so a
// read through a reshape or expand counts against the real producer.
// TKernelTable reports the count; nothing in the runtime reads it.

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
