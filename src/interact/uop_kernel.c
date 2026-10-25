// interact/uop_kernel.c - fire a UOP_KERNEL: fire each input's producer
// kernel first (TenDesc.producer_kid), dispatch through the output backend's
// dispatch_kernel, and return the output TAG_TEN at heap[loc+0].  One fire is
// one interaction (ITRS), as HVM4 counts an OP2-NUM-NUM collapse.

// Fire memo: within one fire generation a kernel reached by several consumers
// fires once (shared forward intermediates otherwise re-fire exponentially).
// A realize scope, a top-level interact_kernel and every ASSIGN start a new one.
static u32 KERNEL_FIRE_GEN = 0;
static u32 KERNEL_FIRE_SCOPE_DEPTH = 0;
fn void kernel_fire_gen_bump(void) {
  KERNEL_FIRE_GEN++;
  if (KERNEL_FIRE_GEN == 0) KERNEL_FIRE_GEN = 1;   // skip 0 sentinel
}

// Precise fire memo (THVM_PRECISE_FIRE_MEMO, default on): every ASSIGN bumps
// the generation, which re-fires each shared kernel of a bundled optimizer
// step.  Each write is logged with a sequence number; a kernel skips a re-fire
// when no input or output buffer was written since its last dispatch and all
// of them are still live (a pure kernel's output depends only on its inputs).
static int precise_fire_memo_enabled(void) {
  static int known = 0, on = 0;
  if (!known) { char const *e = getenv("THVM_PRECISE_FIRE_MEMO");
                on = (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');  // default ON
                known = 1; }
  return on;
}
static u64 ASSIGN_SEQ = 0;
#define ASSIGN_WRITE_RING 8192u
static u64   ASSIGN_W_SEQ    [ASSIGN_WRITE_RING];
static u32   ASSIGN_W_BUF    [ASSIGN_WRITE_RING];
static void *ASSIGN_W_BACKEND[ASSIGN_WRITE_RING];
fn void kernel_assign_write_record(void *backend, u32 buf_id) {
  if (!precise_fire_memo_enabled()) return;   // zero overhead when off
  ASSIGN_SEQ++;
  u32 slot = (u32)(ASSIGN_SEQ % ASSIGN_WRITE_RING);
  ASSIGN_W_SEQ[slot]     = ASSIGN_SEQ;
  ASSIGN_W_BUF[slot]     = buf_id;
  ASSIGN_W_BACKEND[slot] = backend;
}
// Was (backend, buf_id) written after seq `since`?  Past the ring: 1 (re-fire).
static int kernel_buf_assigned_since(void *backend, u32 buf_id, u64 since) {
  if (ASSIGN_SEQ == 0) return 0;
  u64 oldest = (ASSIGN_SEQ > ASSIGN_WRITE_RING) ? (ASSIGN_SEQ - ASSIGN_WRITE_RING + 1) : 1;
  if (since + 1 < oldest) return 1;
  u64 lo = since + 1;
  if (lo < oldest) lo = oldest;
  for (u64 s = lo; s <= ASSIGN_SEQ; s++) {
    u32 slot = (u32)(s % ASSIGN_WRITE_RING);
    if (ASSIGN_W_SEQ[slot] == s && ASSIGN_W_BUF[slot] == buf_id
        && ASSIGN_W_BACKEND[slot] == backend) return 1;
  }
  return 0;
}

// One epoch per realize pass (outermost scope_begin to scope_end).  An ASSIGN
// bumps the fire generation but not the epoch, so assign_fire_claim can tell
// "same cell, same pass" across those bumps.
static u32 ASSIGN_PASS_EPOCH = 0;

fn void kernel_fire_scope_begin(void) {
  if (KERNEL_FIRE_SCOPE_DEPTH == 0) {
    kernel_fire_gen_bump();
    ASSIGN_PASS_EPOCH++;
    if (ASSIGN_PASS_EPOCH == 0) ASSIGN_PASS_EPOCH = 1;
  }
  KERNEL_FIRE_SCOPE_DEPTH++;
}

fn void kernel_fire_scope_end(void) {
  if (KERNEL_FIRE_SCOPE_DEPTH > 0) KERNEL_FIRE_SCOPE_DEPTH--;
}

// A new ASSIGN pass without a realize scope.  A wnf-driven training loop
// (TPriForce) re-forces one shared materialized step per iteration; prim_pri
// calls this before each force, else its ASSIGN cells would fire only once.
fn void assign_pass_epoch_bump(void) {
  ASSIGN_PASS_EPOCH++;
  if (ASSIGN_PASS_EPOCH == 0) ASSIGN_PASS_EPOCH = 1;
}

// ASSIGN firing memo, keyed by the cell's heap loc and the pass epoch: an
// ASSIGN reachable from several roots (Adam's `m` update, also read by the
// param update) writes its buffer once per pass, not once per visit.
#define ASSIGN_FIRE_MEMO_CAP 8192u
static u64 ASSIGN_FIRE_LOC[ASSIGN_FIRE_MEMO_CAP];
static u32 ASSIGN_FIRE_EPOCH[ASSIGN_FIRE_MEMO_CAP];
// 1 the first time `loc` is claimed this pass, 0 on every re-visit.
fn int assign_fire_claim(u64 loc) {
  if (loc == 0) return 1;
  u32 h = (u32)((loc >> 4) % ASSIGN_FIRE_MEMO_CAP);
  for (u32 probe = 0; probe < 4; probe++) {
    u32 i = (h + probe) % ASSIGN_FIRE_MEMO_CAP;
    if (ASSIGN_FIRE_EPOCH[i] == ASSIGN_PASS_EPOCH && ASSIGN_FIRE_LOC[i] == loc)
      return 0;                                  // already fired this pass
    if (ASSIGN_FIRE_EPOCH[i] != ASSIGN_PASS_EPOCH) {  // free / stale slot
      ASSIGN_FIRE_EPOCH[i] = ASSIGN_PASS_EPOCH;
      ASSIGN_FIRE_LOC[i] = loc;
      return 1;                                  // claimed -- fire it
    }
  }
  // All four probes hold this pass's entries: overwrite the base slot.  A
  // mis-claim costs a re-fire, never a dropped assign.
  ASSIGN_FIRE_EPOCH[h] = ASSIGN_PASS_EPOCH;
  ASSIGN_FIRE_LOC[h] = loc;
  return 1;
}

// Cross-backend kernel inputs.  A kernel runs on its output's backend; an
// input on another backend (a CPU weight feeding a Metal net) is staged there
// once and reused across re-fires, as tinygrad moves operands to the kernel's
// device.  Keyed by (src_tid, backend, context): tids restart per context.
// Reset with the UOP_COPY upload cache.
#define KIN_UP_CAP (1u << 15)
static struct { u32 src_tid; u32 backend_id; u32 dst_tid; u32 ctx; } KIN_UP[KIN_UP_CAP];
fn void kernel_input_upload_reset(void) { memset(KIN_UP, 0, sizeof(KIN_UP)); }

// Drop the zero-copy Metal wrap cached for a disk-mmap weight.  Pool rollbacks
// skip borrowed buffers, so the WL streaming loader releases each block's
// wraps once its matmuls retire; the next use re-wraps the (maybe re-faulted)
// pages.  The mmap pages stay with the CPU-side DiskMap.
fn void thvm_kernel_input_drop_wrap(u32 src_tid) {
  if (src_tid == 0 || src_tid >= TENS_NEXT) return;
  u32 backend_id = METAL_BACKEND.id;
  u32 h = (src_tid * 0x9e3779b1u + backend_id) & (KIN_UP_CAP - 1);
  for (u32 p = 0; p < KIN_UP_CAP; p++) {
    u32 i = (h + p) & (KIN_UP_CAP - 1);
    if (KIN_UP[i].src_tid == 0) return;                    // not cached
    if (KIN_UP[i].src_tid == src_tid && KIN_UP[i].backend_id == backend_id
        && KIN_UP[i].ctx == thvm_context_current()) {
      u32 dt = KIN_UP[i].dst_tid;
      if (dt != 0 && dt < TENS_NEXT) {
        u32 wbid = TENS[dt].buf_id;
        if (wbid != 0 && thvm_metal_buf_is_borrowed(wbid)) {
          thvm_metal_buf_free_borrowed(wbid);
          TENS[dt].buf_id = 0;                             // wrap tid now dead
        }
      }
      // Clear the slot.  A linear-probe hash with tombstones would break the
      // run-of-set chain, so re-pack the run after i: lift any later entry whose
      // home bucket lies at or before i (the standard backward-shift delete).
      KIN_UP[i].src_tid = 0; KIN_UP[i].backend_id = 0; KIN_UP[i].dst_tid = 0;
      KIN_UP[i].ctx = 0;
      u32 j = i;
      for (u32 q = 1; q < KIN_UP_CAP; q++) {
        u32 k = (i + q) & (KIN_UP_CAP - 1);
        if (KIN_UP[k].src_tid == 0) break;
        u32 home = (KIN_UP[k].src_tid * 0x9e3779b1u + KIN_UP[k].backend_id)
                   & (KIN_UP_CAP - 1);
        // k can fill slot j iff its home bucket is not strictly between j+1..k
        // (cyclically) -- i.e. moving it back to j keeps it findable.
        u32 dist_kj = (k - j) & (KIN_UP_CAP - 1);
        u32 dist_kh = (k - home) & (KIN_UP_CAP - 1);
        if (dist_kh >= dist_kj) {
          KIN_UP[j] = KIN_UP[k];
          KIN_UP[k].src_tid = 0; KIN_UP[k].backend_id = 0; KIN_UP[k].dst_tid = 0;
          j = k;
        }
      }
      return;
    }
  }
}
// A tid whose buffer holds src_tid's bytes on dst_b; src_tid itself when it is
// already there or the move fails.  Leaves are contiguous, so the kernel's
// input view applies unchanged to the moved buffer.
static u32 kernel_input_on_backend(u32 src_tid, Backend *dst_b) {
  if (src_tid == 0 || src_tid >= TENS_NEXT || dst_b == NULL) return src_tid;
  Backend *sb = TENS[src_tid].backend;
  if (sb == NULL || sb == dst_b) return src_tid;            // already on dst
  if (sb->buf_read == NULL || dst_b->buf_write == NULL) return src_tid;
  u32 h = (src_tid * 0x9e3779b1u + dst_b->id) & (KIN_UP_CAP - 1);
  for (u32 p = 0; p < KIN_UP_CAP; p++) {
    u32 i = (h + p) & (KIN_UP_CAP - 1);
    if (KIN_UP[i].src_tid == 0) break;
    if (KIN_UP[i].src_tid == src_tid && KIN_UP[i].backend_id == dst_b->id
        && KIN_UP[i].ctx == thvm_context_current()) {
      u32 dt = KIN_UP[i].dst_tid;
      if (dt < TENS_NEXT && TENS[dt].buf_id != 0 && TENS[dt].backend == dst_b
          && (dst_b->buf_refcount == NULL
              || dst_b->buf_refcount(TENS[dt].buf_id) != 0))
        return dt;                                           // live cache hit
      break;                                                 // stale -> re-upload
    }
  }
  u64 numel  = TENS[src_tid].view.numel;
  u64 nbytes = dtype_storage_bytes(TENS[src_tid].dtype, numel);
  u32 dst_tid = 0;
  // A disk-mmap weight going to Metal needs no copy on unified memory: wrap the
  // page-aligned mmap in place, its `minor` byte offset carried by the wrapped
  // buffer.  Any decline falls through to the staged upload.
  if (dst_b == &METAL_BACKEND) {
    void *map_base = NULL;
    u64   maplen = 0, minor = 0;
    u32   isz = dtype_itemsize(TENS[src_tid].dtype);
    if (isz != 0
        && thvm_disk_buf_map_info(TENS[src_tid].buf_id, &map_base, &maplen, &minor)
        && (minor % isz) == 0) {
      if (TENS_NEXT < TENS_CAP) {
        u32 wbid = thvm_metal_buf_wrap_external(map_base, maplen, minor);
        if (wbid != 0) {
          u32 dwid = TENS_NEXT++;
          TenDesc *dw = &TENS[dwid];
          dw->dtype          = TENS[src_tid].dtype;
          dw->refcount       = 1;
          dw->view           = TENS[src_tid].view;     // contiguous offset-0 leaf
          // Offset-0 view: the wrapped buffer carries the byte offset (the
          // matmul codegen's leaf rebuild drops a view offset).
          dw->prior_views    = NULL;
          dw->nviews         = 0;
          dw->requires_grad  = 0;
          dw->grad           = 0;
          dw->assign_kvar_id = 0;
          dw->backend        = dst_b;
          dw->producer_kid   = 0;
          dw->buf_id         = wbid;
          dst_tid = dwid;
        }
      }
    }
  }
  if (dst_tid == 0) {
    // Host-stage + upload: read src bytes into a temp, write across.
    void *stage = malloc((size_t)nbytes);
    if (stage == NULL) return src_tid;
    if (sb->buf_read(TENS[src_tid].buf_id, stage, nbytes) != 0) { free(stage); return src_tid; }
    dst_tid = tensor_alloc(dst_b, TENS[src_tid].view.shape, TENS[src_tid].dtype);
    dst_b->buf_write(TENS[dst_tid].buf_id, stage, nbytes);
    free(stage);
  }
  for (u32 p = 0; p < KIN_UP_CAP; p++) {
    u32 i = (h + p) & (KIN_UP_CAP - 1);
    if (KIN_UP[i].src_tid == 0
        || (KIN_UP[i].src_tid == src_tid && KIN_UP[i].backend_id == dst_b->id
            && KIN_UP[i].ctx == thvm_context_current())) {
      KIN_UP[i].src_tid = src_tid; KIN_UP[i].backend_id = dst_b->id;
      KIN_UP[i].dst_tid = dst_tid; KIN_UP[i].ctx = thvm_context_current();
      break;
    }
  }
  return dst_tid;
}

// A kernel the kernel GC stripped has no inputs left: re-firing it ran
// nothing and left whatever its output buffer held, so a downstream read
// took stale bytes.  Reaching one is a stale reference somewhere upstream;
// the realize fails with its kid instead of computing on it.
static u32 KERNEL_STALE_FIRE_KID = 0;

fn u32 kernel_stale_fire_take(void) {
  u32 kid = KERNEL_STALE_FIRE_KID;
  KERNEL_STALE_FIRE_KID = 0;
  return kid;
}

fn void kernel_fire_by_id(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) return;
  KernelEntry *ke = &KERNELS[kid];
  if (ke->fire_gen == KERNEL_FIRE_GEN) return;     // already fired this pass
  if (ke->stripped) {
    if (KERNEL_FIRE_SCOPE_DEPTH == 0) {
      char msg[128];
      snprintf(msg, sizeof msg, "fired kernel kid=%u outside a realize, which the "
               "kernel GC stripped (its inputs were freed)", kid);
      thvm_fatal(msg);
    }
    if (KERNEL_STALE_FIRE_KID == 0) {
      KERNEL_STALE_FIRE_KID = kid;
    }
    return;
  }
  u32 prev_fire_gen = ke->fire_gen;                // 0 = never actually fired
  ke->fire_gen = KERNEL_FIRE_GEN;

  // A symbolic input slot (a free TAG_VAR at emission) resolves through APP-LAM's
  // substitution to a TEN by now; a slot that does not leaves the kernel unfired.
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

  // Precise memo: skip the dispatch and the upstream walk when nothing this
  // kernel reads or writes changed since it fired this pass.  Epoch 0 (no
  // scope) never skips: a never-fired kernel's fire_pass is 0 too.
  (void)prev_fire_gen;
  if (precise_fire_memo_enabled() && ASSIGN_PASS_EPOCH != 0
      && ke->fire_pass == ASSIGN_PASS_EPOCH) {
    Backend *ob   = TENS[ke->output_tid].backend;
    u32      obuf = TENS[ke->output_tid].buf_id;
    int out_live = (ob != NULL && obuf != 0
                    && (ob->buf_refcount == NULL || ob->buf_refcount(obuf) != 0));
    if (out_live && !kernel_buf_assigned_since(ob, obuf, ke->fire_assign_seq)) {
      int need = 0;
      for (u32 i = 0; i < ke->n_inputs; i++) {
        u32 tid = resolved_tids[i];
        if (tid == 0 || tid >= TENS_NEXT) { need = 1; break; }
        Backend *ib   = TENS[tid].backend;
        u32      ibuf = TENS[tid].buf_id;
        if (ib == NULL || ibuf == 0
            || (ib->buf_refcount != NULL && ib->buf_refcount(ibuf) == 0)
            || kernel_buf_assigned_since(ib, ibuf, ke->fire_assign_seq)) {
          need = 1; break;
        }
      }
      if (!need) { return; }
    }
  }

  // Depth-first: fire every input's producing kernel first.
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = resolved_tids[i];
    if (tid < TENS_NEXT && TENS[tid].producer_kid != 0) {
      kernel_fire_by_id(TENS[tid].producer_kid);
    }
  }

  // Resolve concrete buffer ids now that all upstream outputs are filled.
  // An input on a different backend than this kernel (a CPU weight feeding a
  // Metal-routed net) is uploaded to the kernel's backend first.
  Backend *out_b = TENS[ke->output_tid].backend;
  u32 in_buf_ids[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 on_b = kernel_input_on_backend(resolved_tids[i], out_b);
    in_buf_ids[i] = TENS[on_b].buf_id;
    // The kernel reads a slot's view from input_tids, so a moved plain leaf is
    // rebound to its moved tid (same contiguous view); composed slots keep the
    // INDEX the codegen lowered.
    if (on_b != resolved_tids[i] && ke->input_tids != NULL
        && i < ke->n_inputs && ke->input_tids[i] == resolved_tids[i]
        && (ke->input_chain_composed == NULL || !ke->input_chain_composed[i])) {
      ke->input_tids[i] = on_b;
    }
  }
  u32 out_buf_id = TENS[ke->output_tid].buf_id;

  // An output slot recycled since the last fire (refcount 0) gets a fresh
  // buffer, as tinygrad's Buffer.ensure_allocated: re-firing into it would
  // write a buffer another tensor now owns.
  if (out_b != NULL && out_b->buf_refcount != NULL && out_b->buf_alloc != NULL
      && (out_buf_id == 0 || out_b->buf_refcount(out_buf_id) == 0)) {
    u64 nbytes = dtype_storage_bytes(ke->output_dtype, (u64)ke->output_numel);
    u32 new_id = out_b->buf_alloc(nbytes);
    if (new_id != 0) {
      TENS[ke->output_tid].buf_id = new_id;
      out_buf_id = new_id;
    }
  }

  // First-fire opts, after the producers so the inputs are ready: the
  // hand-coded heuristic (NOOPT=1 skips it), then BEAM, whose winner must beat it.
  if (kernel_should_hand_code_opts(ke)) {
    kernel_hand_coded_opts(ke);
  }
  if (kernel_should_autotune(ke)) {
    kernel_autotune(kid);
  }

  Backend *b = TENS[ke->output_tid].backend;
  if (b && b->dispatch_kernel) {
    // A capturing TJit records the dispatch; replays re-dispatch the record.
    if (jit_is_capturing()) {
      jit_capture_record((u32)(ke - KERNELS),
                         in_buf_ids, ke->n_inputs, out_buf_id);
    }
    // Trace-capture (THVM_JIT_TRACE_CAPTURE, Metal): the record holds all the
    // ICB needs, so skip the eager submit; one ICB replay after finalize fills
    // the results.  An autotune bench fire is not trace-pending.
#ifdef THVM_HAS_METAL
    if (b == &METAL_BACKEND && jit_capture_trace_pending()) {
      // Stamp the route the eager pass would record: finalize decides ICB
      // eligibility from it.
      thvm_metal_kernel_route_classify(ke, in_buf_ids, out_buf_id);
      // An eager dispatch was skipped: arm the finalize replay.
      jit_capture_note_trace_skip();
      ke->fire_assign_seq = ASSIGN_SEQ;
      ke->dispatched      = 1;
      ke->fire_pass       = ASSIGN_PASS_EPOCH;
      ITRS++;
      multi_emit(RULE_UOP_KERNEL, MULTI_TERM, (u64)kid, 0, 0);
      return;
    }
#endif
    // THVM_DISPATCH_TRACE=1: a line per dispatch with its fire generation (one
    // kid under many generations: many scopes; under one: a memo miss).
    {
      static int dt_known = 0, dt_on = 0;
      if (!dt_known) { char const *e = getenv("THVM_DISPATCH_TRACE");
                       dt_on = (e != NULL && e[0] == '1'); dt_known = 1; }
      if (dt_on) {
        fprintf(stderr, "[dispatch] kid=%u gen=%u depth=%u cap=%d out_buf=%u in=[",
                (u32)(ke - KERNELS), KERNEL_FIRE_GEN, KERNEL_FIRE_SCOPE_DEPTH,
                jit_is_capturing() ? 1 : 0, out_buf_id);
        for (u32 ti = 0; ti < ke->n_inputs; ti++)
          fprintf(stderr, "%u ", in_buf_ids[ti]);
        fprintf(stderr, "]\n");
        fflush(stderr);
      }
    }
    int rc = b->dispatch_kernel(ke, in_buf_ids, out_buf_id);
    // A CPU dispatch that ran no rung wrote nothing: leave the memo unstamped
    // so a later fire tries again.
    if (!(rc < 0 && b == &CPU_BACKEND)) {
      cg_count_dispatch(kid);
      // Log the output write so a reader re-fires exactly when this kernel did;
      // logged before stamping, so the kernel's own write never dirties it.
      if (b == out_b && out_buf_id != 0) kernel_assign_write_record(b, out_buf_id);
      ke->fire_assign_seq = ASSIGN_SEQ;
      ke->dispatched      = 1;
      ke->fire_pass       = ASSIGN_PASS_EPOCH;
    }
  }

  ITRS++;
  multi_emit(RULE_UOP_KERNEL, MULTI_TERM, (u64)kid, 0, 0);
}

// 1 iff `t` is a kernel that ran and that the kernel GC then stripped: the
// GC strips only a dead output, so an ASSIGN reading it copied it already.
fn int kernel_term_spent(Term t) {
  Term s = term_resolve(t);
  if (term_tag(s) != TAG_UOP || term_ext(s) != UOP_KERNEL) return 0;
  u32 kid = (u32)term_val(heap_read(term_val(s) + 1));
  return kid != 0 && kid < KERNELS_NEXT && KERNELS[kid].stripped && KERNELS[kid].dispatched;
}

fn Term interact_kernel(Term kernel) {
  HOT_KERNEL_FIRES++;
  u64  loc    = term_val(kernel);
  Term outbuf = heap_read(loc + 0);
  Term kidnum = heap_read(loc + 1);
  u32  kid    = (u32)term_val(kidnum);
  if (KERNEL_FIRE_SCOPE_DEPTH == 0) {
    kernel_fire_gen_bump();
  }
  kernel_fire_by_id(kid);
  return outbuf;
}

// The first-fire opts for the Metal TU's parallel precompile (it sees only
// thvm.h), so a pre-rendered kernel hashes like the fire path's.  0 when BEAM
// will rewrite the kernel at fire time: leave it to the lazy compile.
int thvm_kernel_apply_first_fire_opts(struct KernelEntry *ke) {
  if (ke == NULL) return 0;
  if (kernel_should_hand_code_opts(ke)) kernel_hand_coded_opts(ke);
  if (kernel_should_autotune(ke)) return 0;
  return 1;
}
