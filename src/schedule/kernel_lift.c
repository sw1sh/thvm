// Coverage counters for kernel_lift_to_uop.  Read via
// kernel_lift_attempts() / kernel_lift_successes().  Counters are
// global (single-threaded scheduling).  Reset by thvm_init / thvm_free.
static u64 KERNEL_LIFT_ATTEMPTS;
static u64 KERNEL_LIFT_SUCCESSES;

fn u64 kernel_lift_attempts(void)        { return KERNEL_LIFT_ATTEMPTS; }
fn u64 kernel_lift_successes(void)       { return KERNEL_LIFT_SUCCESSES; }

fn void kernel_lift_counters_reset(void) {
  KERNEL_LIFT_ATTEMPTS = 0;
  KERNEL_LIFT_SUCCESSES = 0;
}

// Increment helpers for callers that precede this TU in the unity
// build (codegen/render_metal.c is #include'd first and can't reach
// the static globals directly).
fn void kernel_lift_count_attempt (void) { KERNEL_LIFT_ATTEMPTS++; }
fn void kernel_lift_count_success (void) { KERNEL_LIFT_SUCCESSES++; }

// schedule/kernel_lift.c -- hand a scheduled kernel back to the
// renderer as a UOp DAG root suitable for cg_render_uop_kernel.
//
// kernel_lift_to_uop looks up the UOP_STORE root the unified rangeify
// pass emitted for this kernel's boundary (via bufferize_info_find +
// rangeify_unified_store_root_at) and packages it as KernelUopLift
// for the renderer / dispatcher.  KernelUopLift is declared in thvm.h.

static void lift_reject_log(KernelEntry const *ke, u32 sid,
                            const char *where) {
  static int reject_log_inited = 0;
  static int reject_log_on     = 0;
  if (!reject_log_inited) {
    char const *e = getenv("THVM_DUMP_LIFT_REJECT");
    reject_log_on    = (e != NULL && e[0] == '1');
    reject_log_inited = 1;
  }
  if (!reject_log_on) return;
  (void)ke;
  fprintf(stderr, "lift reject: %s sid=%u\n", where, sid);
}

// Build a UOP_BUFFER for kernel input slot `slot` from the TenDesc
// shape.  Falls back to a 1D dummy when the slot has no TenDesc
// (test fixtures or paths where TenDesc isn't wired).
//
// The `instance` field on UOP_BUFFER ensures distinct slots get
// distinct Terms even when shape collides; we use slot+1 (since
// instance=0 is the output's reserved value).
static Term lift_input_buffer(KernelEntry const *ke, u32 slot) {
  if (slot >= ke->n_inputs) return 0;
  u32 dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
  u32 tid = (ke->input_tids != NULL) ? ke->input_tids[slot] : 0;
  u32 inst = slot + 1;
  if (tid != 0 && tid < TENS_NEXT) {
    TenDesc const *td = &TENS[tid];
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                           td->view.shape.ndim, td->view.shape.dims, inst);
  }
  u32 dummy[1] = { 1 };
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, 1, dummy, inst);
}

fn int kernel_lift_to_uop(KernelEntry const *ke, KernelUopLift *out) {
  if (ke == NULL || out == NULL) return 0;
  // Unified-pass short-circuit: when the rangeify_unified pass emitted
  // a UOP_STORE for this kernel's boundary, build only the buffer
  // fields the bypass-substitution site reads (in_bufs[], n_inputs,
  // out_buf) and hand back the unified root as store_root.
  if (ke->source_uop == 0) {
    lift_reject_log(ke, 0, "entry/no-source-uop");
    return 0;
  }
  u32 ru_idx = bufferize_info_find(term_val(ke->source_uop));
  if (ru_idx == 0xFFFFFFFFu) {
    lift_reject_log(ke, 0, "entry/no-bufferize-info");
    return 0;
  }
  Term ru_root = rangeify_unified_store_root_at(ru_idx);
  if (ru_root == 0) {
    lift_reject_log(ke, 0, "entry/no-unified-store-root");
    return 0;
  }
  if (ke->n_inputs > KERNEL_LIFT_MAX_INPUT) {
    lift_reject_log(ke, 0, "entry/n-inputs-over-cap");
    return 0;
  }
  out->n_inputs = ke->n_inputs;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    Term in_buf = lift_input_buffer(ke, i);
    if (in_buf == 0) {
      lift_reject_log(ke, 0, "entry/in-buf-build-fail");
      return 0;
    }
    out->in_bufs[i] = in_buf;
  }
  out->out_buf     = uop_store_buf(ru_root);
  out->store_root  = ru_root;
  out->n_outputs   = 1;
  out->out_bufs[0] = out->out_buf;
  return 1;
}
