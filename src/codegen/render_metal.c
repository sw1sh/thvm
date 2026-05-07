
// Bridge for tests: render an arbitrary KernelEntry to MSL and return
// the source.  Lets the WL-side test grid sanity-check that the same
// KProgOp[] emits valid Metal source -- proves the Renderer
// abstraction holds without requiring a Metal-side compile/dispatch
// path.
// Both Metal MSL emit paths (metal_jit_encode and
// metal_tile_jit_encode in backend/metal/_.m) now route through the
// same UOp-DAG renderer.  cg_emit_metal exists for the dispatch
// ladder's metal_jit slot; it just forwards to cg_emit_tile_metal.
char *cg_emit_metal(KernelEntry const *ke) {
  return cg_emit_tile_metal((KernelEntry *)ke);
}


static int rmt_collect_conv2d_info(KernelEntry const *ke,
                                   TileConv2DInfo *out) {
  {
    u32 n_app = tile_anno_applied_opts_count(ke);
    KOpt const *opts = tile_anno_applied_opts(ke);
    for (u32 i = 0; i < n_app; i++) {
      u8 op = opts[i].op;
      if (op == KOP_GROUP || op == KOP_GROUPTOP) {
        return 0;
      }
    }
  }
  if (!tile_analyze_conv2d_flat(ke, out)) {
    return 0;
  }
  return out->threads > 0 && out->threads <= 256;
}

static int rmt_kprog_has_opcode(KernelEntry const *ke, u8 opcode) {
  if (ke == NULL || ke->program == NULL) {
    return 0;
  }
  for (u32 i = 0; i < ke->n_ops; i++) {
    if (ke->program[i].opcode == opcode) {
      return 1;
    }
  }
  return 0;
}

int cg_tile_metal_dispatch_shape(KernelEntry *ke, u32 *groups_x,
                                 u32 *threads_x) {
  if (ke == NULL) return 0;
  // Metal hardware caps buffer attributes at index 30; reject
  // kernels that won't render (matches the renderer's reject).
  if (ke->n_inputs > 30) return 0;
  // Conv2D flat: dispatch (c_out * patches / outputs_per_thread)
  // total threads divided into `threads` per group.
  TileConv2DInfo conv;
  if (rmt_collect_conv2d_info(ke, &conv)) {
    u64 total = (u64)conv.c_out * (u64)conv.patches;
    if (total == 0 || total > 0xFFFFFFFFu) return 0;
    u32 outputs = conv.outputs_per_thread ? conv.outputs_per_thread : 1;
    u64 threads_total = (total + (u64)outputs - 1) / (u64)outputs;
    u32 threads = conv.threads;
    u32 groups  = (u32)((threads_total + (u64)threads - 1) / (u64)threads);
    if (groups == 0) return 0;
    if (groups_x  != NULL) *groups_x  = groups;
    if (threads_x != NULL) *threads_x = threads;
    return 1;
  }
  if (tile_rejects_conv2d_flat_cin1(ke)) return 0;
  // Generic path: walk the tile_uops graph (built from the kernel's
  // axis structure) and compute (groups, threads).  Handles
  // FLAT_GRID / LOCAL_GLOBAL / GROUP_REDUCE modes from KAX_* axis
  // types directly.
  if (tile_sync_from_scalar(ke)) {
    return tile_compute_dispatch_shape(ke, groups_x, threads_x);
  }
  // Lifter-based fallback: tile_sync_from_scalar declines for kernels
  // that don't have a clean gemm-shape AND don't have scalar_uops
  // (e.g. TMatMul-equivalent UOp DAGs that go straight to materialize
  // without rangeify, multi-output kernels, or kernels whose
  // ScalarUop arena has shapes tile_build_from_scalar doesn't handle).
  // For those, if the kernel_lift would succeed, render_uop can emit
  // a valid kernel; dispatch shape is just (output_numel, 256-default
  // threadgroup).  The kernel's outer for-loops handle work
  // distribution within each thread (each thread runs the full body
  // redundantly; last-writer-wins on the output buffer gives correct
  // results).  Future wedges can specialize when M/N-axes get bound
  // to thread positions via UOP_OPT_LOCAL annotations.
  KernelUopLift lift = {0};
  if (!kernel_lift_to_uop(ke, &lift)) return 0;
  u64 total = ke->output_numel ? (u64)ke->output_numel : 1;
  if (total == 0 || total > 0xFFFFFFFFu) return 0;
  u32 threads = total < 256 ? (u32)total : 256u;
  u32 groups  = (u32)((total + (u64)threads - 1) / (u64)threads);
  if (groups == 0 || threads == 0) return 0;
  if (groups_x  != NULL) *groups_x  = groups;
  if (threads_x != NULL) *threads_x = threads;
  return 1;
}


// Coverage telemetry: each cg_emit_tile_metal call increments
// KERNEL_LIFT_ATTEMPTS; on lift success bumps KERNEL_LIFT_SUCCESSES.
// THVM_RENDER_UOP_SHADOW_COMPILE=1 also renders the lift result and
// shells out to xcrun metal, populating KERNEL_LIFT_COMPILES /
// _COMPILE_FAILS.  Read via kernel_lift_attempts() / etc.
static void cg_shadow_lift_metal(KernelEntry const *ke) {
  KernelUopLift lift = {0};
  kernel_lift_count_attempt();
  // Metal hardware caps buffer attributes at index 30; kernels with
  // > 30 inputs can't be rendered through buffer-arg signatures.
  if (ke->n_inputs > 30) return;
  if (!kernel_lift_to_uop(ke, &lift)) return;
  kernel_lift_count_success();
  static int shadow_compile_inited = 0;
  static int shadow_compile_on     = 0;
  if (!shadow_compile_inited) {
    char const *e = getenv("THVM_RENDER_UOP_SHADOW_COMPILE");
    shadow_compile_on    = (e != NULL && e[0] == '1');
    shadow_compile_inited = 1;
  }
  if (!shadow_compile_on) return;
  char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  if (fp == NULL) return;
  cg_render_uop_kernel(lift.store_root, "shadow", lift.out_buf,
                       lift.in_bufs, lift.n_inputs, fp);
  fclose(fp);
  // Write to a temp file and shell out to xcrun metal.  Slow but
  // gives concrete signal on rendered-MSL compilability for real
  // kernels.  Caller gates via THVM_RENDER_UOP_SHADOW_COMPILE=1.
  extern int system(const char *);
  extern int unlink(const char *);
  extern int getpid(void);
  char path[64];
  snprintf(path, sizeof(path), "/tmp/thvm_shadow_%d.metal", getpid());
  FILE *out = fopen(path, "w");
  if (out == NULL) return;
  fputs(buf, out);
  fclose(out);
  char cmd[256];
  // When THVM_DUMP_LIFT_COMPILE_FAIL=1, leave the failing .metal file
  // in /tmp and dump the compiler stderr so we can see what's wrong.
  static int dump_fail_inited = 0;
  static int dump_fail_on     = 0;
  if (!dump_fail_inited) {
    char const *e = getenv("THVM_DUMP_LIFT_COMPILE_FAIL");
    dump_fail_on    = (e != NULL && e[0] == '1');
    dump_fail_inited = 1;
  }
  snprintf(cmd, sizeof(cmd),
           "xcrun metal -x metal -c %s -o /dev/null 2>/dev/null", path);
  int rc = system(cmd);
  if (WEXITSTATUS(rc) != 0 && dump_fail_on) {
    // Copy to a stable path that survives the subsequent unlink so a
    // follow-up shell can inspect the final failing rendering.
    char saved[64];
    snprintf(saved, sizeof(saved), "/tmp/thvm_shadow_last_fail.metal");
    char cp_cmd[256];
    snprintf(cp_cmd, sizeof(cp_cmd), "cp %s %s", path, saved);
    system(cp_cmd);
    fprintf(stderr, "=== compile-fail (saved as %s): ", saved);
    char err_cmd[256];
    snprintf(err_cmd, sizeof(err_cmd),
             "xcrun metal -x metal -c %s -o /dev/null 2>&1 | head -5",
             path);
    system(err_cmd);
  }
  unlink(path);
  kernel_lift_count_compile(WEXITSTATUS(rc) == 0);
}

// Render through kernel_lift_to_uop + cg_render_uop_kernel.  This is
// the primary (and only) Metal MSL emit path: the lifter handles
// every kernel shape (matmul, conv2d_flat including multi-input X
// im2col, elementwise, reduce, movement-fused subtrees).
static char *cg_emit_via_uop(KernelEntry const *ke) {
  // Metal hardware caps buffer attributes at index 30 (31 slots total
  // including output).  Reject kernels with too many inputs.
  if (ke->n_inputs > 30) return NULL;
  KernelUopLift lift = {0};
  if (!kernel_lift_to_uop(ke, &lift)) return NULL;
  // Render to a malloc'd string, matching cg_emit_tile_metal's
  // contract.  Use kernel name "k" so MTLLibrary lookup behaves like
  // the existing path.
  // F3.4: matmul-shape kernels with K not divisible by 8 don't fit
  // render_uop's simdgroup_matrix template; the fallback there is a
  // per-thread scalar accumulator which loses to metal_try_gemm's
  // tile-shared-mem GEMM. Decline the kernel so the dispatch ladder
  // falls through to metal_try_gemm. K%8==0 (or K-extent unknown)
  // proceeds and gets the simdgroup template via uop_recognise_tc.
  u32 mm_k = 0;
  if (uop_classify_matmul(lift.store_root, &mm_k) && mm_k != 0
      && (mm_k % 8) != 0) {
    return NULL;
  }
  // F3.1: pre-render pass installs UOP_OPT(_, TC, 0) on matmul-shaped
  // STORE roots so render_uop's simdgroup_matrix template fires.
  // No-op for non-matmul kernels.
  Term store_root = uop_recognise_tc(lift.store_root);
  char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  if (fp == NULL) return NULL;
  cg_render_uop_kernel(store_root, "k", lift.out_buf,
                       lift.in_bufs, lift.n_inputs, fp);
  long n = ftell(fp);
  fclose(fp);
  if (n <= 0) return NULL;
  char *out = (char *)malloc((size_t)n + 1);
  if (out == NULL) return NULL;
  memcpy(out, buf, (size_t)n);
  out[n] = '\0';
  return out;
}

char *cg_emit_tile_metal(KernelEntry const *ke) {
  // Multi-output kernels are not yet renderable through the tile
  // metal path (single `device float *out` arg + single S_STORE).
  // Bail until step 4 wires the multi-output dispatch.
  if (cg_kernel_has_extra_outputs(ke)) {
    return NULL;
  }
  cg_shadow_lift_metal(ke);
  // Render through the UOp-DAG renderer.  The lifter handles every
  // kernel shape (matmul / conv2d / elementwise / reduce / movement-
  // fused / im2col multi-input).  Returns NULL when the lifter
  // declines (n_inputs > 30, malformed shape, etc.) so the dispatch
  // ladder can route the kernel through a non-tile path if any.
  return cg_emit_via_uop(ke);
}

