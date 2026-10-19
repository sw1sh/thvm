// schedule/kernel_custom.c - raw custom-kernel injection.
//
// Registers a hand-written kernel source (the WL TKernel[<|"Program" -> ...|>]
// surface, or a C test) as a KernelEntry that fires through the normal backend
// dispatch, so JIT capture records and replays it like a scheduler kernel.
// tinygrad's analogue is a raw Ops.PROGRAM given to Tensor.custom_kernel
// (tensor.py:159).  A raw program must name the buffers it writes, else the
// planner may recycle one of them mid-replay; tinygrad assumes every buffer
// when it cannot infer the writes (uop/ops.py:1375).
//
// Contract:
//   - Every argument buffer (read or written) occupies one input slot
//     (input_tids[i]); custom_outs_mask marks the written slots and
//     output_tid is the tensor of out_slots[0].
//   - Arguments must be live, contiguous, offset-0 leaf tensors on one
//     backend; the WL layer realizes each one first.
//   - The source ABI is per backend:
//       CPU:   void <name>(void *const *bufs, unsigned const *grid,
//                          unsigned const *block);
//              one call per dispatch; the kernel loops itself, reading
//              launch geometry from grid[3] / block[3].
//       CUDA:  extern "C" __global__ void <name>(T *arg0, T *arg1, ...);
//              launched with grid[3] x block[3] blocks/threads and
//              custom_shared_bytes of dynamic shared memory.
//       Metal: kernel <name> with argument slot i at [[buffer(i)]],
//              dispatched as grid[3] threadgroups of block[3] threads.
//   - Firing is explicit (kernel_custom_fire): no producer_kid is set, so
//     a lazy graph never re-fires an injected kernel; downstream reads see
//     the written bytes as plain TAG_TEN leaves.

// The outs mask (custom_outs_mask) is a u64 bit set, so an injected
// kernel takes at most 64 buffer arguments.
#define KERNEL_CUSTOM_MAX_ARGS 64u

// One argument slot: realized, live, contiguous offset-0 leaf.
static int kernel_custom_arg_ok(u32 tid) {
  if (tid == 0 || tid >= TENS_NEXT) return 0;
  TenDesc const *td = &TENS[tid];
  if (td->backend == NULL || td->buf_id == 0) return 0;
  if (td->backend->buf_refcount != NULL
      && td->backend->buf_refcount(td->buf_id) == 0) return 0;
  return td->nviews == 0 && td->view.contiguous && td->view.offset == 0;
}

// Returns the new kid, or 0 when validation fails (stderr says why).
fn u32 kernel_emit_custom_program(char const *name, char const *source,
                                  u32 const grid[3], u32 const block[3],
                                  u32 const *arg_tids, u32 n_args,
                                  u32 const *out_slots, u32 n_outs,
                                  u32 shared_bytes) {
  if (name == NULL || name[0] == '\0' || source == NULL || source[0] == '\0') {
    fprintf(stderr, "thvm: kernel_emit_custom_program: empty name or source\n");
    return 0;
  }
  if (n_args == 0 || n_args > KERNEL_CUSTOM_MAX_ARGS) {
    fprintf(stderr, "thvm: kernel_emit_custom_program: n_args=%u out of "
                    "range [1,%u]\n", n_args, KERNEL_CUSTOM_MAX_ARGS);
    return 0;
  }
  if (n_outs == 0 || n_outs > n_args) {
    fprintf(stderr, "thvm: kernel_emit_custom_program: n_outs=%u out of "
                    "range [1,%u]\n", n_outs, n_args);
    return 0;
  }
  u64 outs_mask = 0;
  for (u32 i = 0; i < n_outs; i++) {
    if (out_slots[i] >= n_args) {
      fprintf(stderr, "thvm: kernel_emit_custom_program: out slot %u >= "
                      "n_args %u\n", out_slots[i], n_args);
      return 0;
    }
    outs_mask |= 1ull << out_slots[i];
  }
  Backend *b = NULL;
  for (u32 i = 0; i < n_args; i++) {
    if (!kernel_custom_arg_ok(arg_tids[i])) {
      fprintf(stderr, "thvm: kernel_emit_custom_program: arg %u (tid %u) is "
                      "not a live contiguous offset-0 leaf tensor\n",
              i, arg_tids[i]);
      return 0;
    }
    if (b == NULL) b = TENS[arg_tids[i]].backend;
    if (TENS[arg_tids[i]].backend != b) {
      fprintf(stderr, "thvm: kernel_emit_custom_program: arg %u is on a "
                      "different backend than arg 0\n", i);
      return 0;
    }
  }

  char *program = strdup(source);
  char *entry   = strdup(name);
  if (program == NULL || entry == NULL) {
    free(program);
    free(entry);
    return 0;
  }

  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  kernel_inputs_reserve(ke, n_args);
  for (u32 i = 0; i < n_args; i++) {
    u32 tid = arg_tids[i];
    ke->input_tids  [i] = tid;
    ke->input_dtypes[i] = TENS[tid].dtype;
    ke->input_numels[i] = TENS[tid].view.numel;
    ke->input_views [i] = TENS[tid].view;
    ke->input_terms [i] = 0;
  }
  ke->n_inputs = n_args;

  u32 primary = arg_tids[out_slots[0]];
  ke->output_tid   = primary;
  ke->output_dtype = TENS[primary].dtype;
  ke->output_shape = TENS[primary].view.shape;
  ke->output_numel = TENS[primary].view.numel;
  ke->schedule     = &ke->_local_schedule;

  ke->custom_program      = program;
  ke->custom_name         = entry;
  ke->custom_shared_bytes = shared_bytes;
  ke->custom_outs_mask    = outs_mask;
  for (u32 d = 0; d < 3; d++) {
    ke->custom_grid [d] = (grid  != NULL && grid [d] != 0) ? grid [d] : 1;
    ke->custom_block[d] = (block != NULL && block[d] != 0) ? block[d] : 1;
  }
  return kid;
}

// The slot of the lowest set outs bit.  Backends bind out_buf_id there in
// place of the slot's own buffer: at JIT replay, input rebinding and planner
// redirects rewrite out_buf_id, not the mirrored input slot.
fn u32 kernel_custom_primary_out_slot(KernelEntry const *ke) {
  u64 m = ke->custom_outs_mask;
  u32 slot = 0;
  while (m != 0 && (m & 1) == 0) { m >>= 1; slot++; }
  return slot;
}

// The dispatch core of kernel_fire_by_id (interact/uop_kernel.c) without
// its fire-generation memos, so an explicit custom fire always executes.
// Returns 0 on success, -1 on any failure.
fn int kernel_custom_fire(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) return -1;
  KernelEntry *ke = &KERNELS[kid];
  if (!kernel_is_custom(ke)) {
    fprintf(stderr, "thvm: kernel_custom_fire: kid=%u carries no custom "
                    "program\n", kid);
    return -1;
  }

  // Fill lazy inputs first, as kernel_fire_by_id's producer walk does; the
  // scope gives the walk its own fire generation.
  kernel_fire_scope_begin();
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid != 0 && tid < TENS_NEXT && TENS[tid].producer_kid != 0) {
      kernel_fire_by_id(TENS[tid].producer_kid);
    }
  }
  kernel_fire_scope_end();

  Backend *b = TENS[ke->output_tid].backend;
  if (b == NULL || b->dispatch_kernel == NULL) return -1;
  u32 in_buf_ids[KERNEL_CUSTOM_MAX_ARGS];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid == 0 || tid >= TENS_NEXT || TENS[tid].buf_id == 0) {
      fprintf(stderr, "thvm: kernel_custom_fire: kid=%u arg %u has no live "
                      "buffer\n", kid, i);
      return -1;
    }
    in_buf_ids[i] = TENS[tid].buf_id;
  }
  u32 out_buf_id = TENS[ke->output_tid].buf_id;

  // Record before the dispatch, as kernel_fire_by_id does; the capture keeps
  // every argument of a custom op live (jit_dispatch_op_custom, jit/capture.c).
  if (jit_is_capturing()) {
    jit_capture_record(kid, in_buf_ids, ke->n_inputs, out_buf_id);
  }
#ifdef THVM_HAS_METAL
  // Metal trace capture (THVM_JIT_TRACE_CAPTURE), as in kernel_fire_by_id:
  // record the writes but skip the submit, since jit_capture_end_common's
  // finalize replay dispatches this kernel; an in-place one would run twice.
  if (b == &METAL_BACKEND && jit_capture_trace_pending()) {
    jit_capture_note_trace_skip();
    ke->dispatched = 1;
    for (u32 i = 0; i < ke->n_inputs; i++) {
      if (ke->custom_outs_mask & (1ull << i)) {
        kernel_assign_write_record(b, in_buf_ids[i]);
      }
    }
    ITRS++;
    multi_emit(RULE_UOP_KERNEL, MULTI_TERM, (u64)kid, 0, 0);
    return 0;
  }
#endif
  int rc = b->dispatch_kernel(ke, in_buf_ids, out_buf_id);
  if (rc != 0) return -1;
  ke->dispatched = 1;

  // Each written argument voids downstream kernels' precise-fire-memo skip,
  // as a scheduler kernel's output write does.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->custom_outs_mask & (1ull << i)) {
      kernel_assign_write_record(b, in_buf_ids[i]);
    }
  }
  // Drain (cuCtxSynchronize on CUDA) so an eager caller can read the
  // written buffers at once.
  if (b->dispatch_end != NULL) b->dispatch_end();

  ITRS++;
  multi_emit(RULE_UOP_KERNEL, MULTI_TERM, (u64)kid, 0, 0);
  return 0;
}
