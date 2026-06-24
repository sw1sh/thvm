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

// Per-outer-realize firing memo.  Within a single interact_kernel
// entry (one user-facing fire), the kernel DAG is a pure function of
// inputs -- a kernel referenced by multiple consumers should fire
// exactly once, not once per consumer.  Without this guard, w2's
// chain rule on LeNet (where forward intermediates are shared across
// dozens of partial-conv kernels) re-fires kernels exponentially:
// 640 kernels x ~1700 visits each = >1M dispatches per realize.
//
// Bumped at each top-level interact_kernel entry so subsequent
// user-facing realizes (which may include ASSIGN-driven mutation
// of input buffers) start fresh.  Reset on overflow so we never
// stale-skip after a u32 wrap.
static u32 KERNEL_FIRE_GEN = 0;
static u32 KERNEL_FIRE_SCOPE_DEPTH = 0;
fn void kernel_fire_gen_bump(void) {
  KERNEL_FIRE_GEN++;
  if (KERNEL_FIRE_GEN == 0) KERNEL_FIRE_GEN = 1;   // skip 0 sentinel
}

// --- precise fire memo (opt-in THVM_PRECISE_FIRE_MEMO=1) -------------
// The coarse memo (fire_gen) re-fires every kernel after each ASSIGN,
// because interact_assign bumps KERNEL_FIRE_GEN globally per assign.  In
// a BUNDLED optimizer step (one realize scope, so shared-upstream
// outputs stay live -- no per-assign rollback), that re-fires each
// shared grad/activation ~30x.  The precise memo records each ASSIGN's
// written (backend, buf_id) with a monotonic seq; a kernel may skip a
// re-fire iff NO input buffer was assigned since its last actual
// dispatch AND every input + the output buffer is still live.  Correct:
// a pure kernel's output depends only on its inputs.  Needs bundling to
// be effective (per-tensor assign scopes free the outputs -> nothing to
// skip to; see the freed-output finding in the speed memo).
static int precise_fire_memo_enabled(void) {
  static int known = 0, on = 0;
  if (!known) { char const *e = getenv("THVM_PRECISE_FIRE_MEMO");
                on = (e == NULL || e[0] == '\0') ? 1 : (e[0] != '0');  // default ON
                known = 1; }
  return on;
}
static u64 ASSIGN_SEQ = 0;
u64 PRECISE_SKIPS = 0, PRECISE_REFIRE_INPUT = 0, PRECISE_REFIRE_OUT = 0;
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
// Was (backend, buf_id) ASSIGNed at a seq > `since`?  Out-of-window ->
// conservative 1 (re-fire).
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

// One realize PASS (the whole scope from scope_begin to its matching
// scope_end -- thvm_realize / thvm_realize_many) gets a stable epoch.
// Unlike KERNEL_FIRE_GEN, this is NOT advanced by interact_assign_with's
// post-write gen bump, so the ASSIGN memo (assign_fire_claim) can tell
// "same cell, same pass" across the gen advances an in-place assign
// forces for downstream kernel re-fire.
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

// Open a fresh ASSIGN pass without a realize scope.  A wnf-driven
// recursive training loop forces a SHARED materialized step (one cell
// loc, reached via TRef/ALO) once per iteration via TPriForce; the
// per-pass assign_fire_claim memo keys on (cell loc, ASSIGN_PASS_EPOCH).
// Without a realize scope around the loop the epoch never advances, so
// the shared ASSIGN cell claims-and-fires only on the first iteration
// (every later force is a no-op) and the loop runs a single step
// regardless of n.  prim_pri calls this before forcing each step so
// every iteration is its own pass: multi-root assigns inside ONE step
// still dedup (same epoch within the force), but the next iteration's
// re-force gets a fresh epoch and re-fires.  The companion KERNEL_FIRE_GEN
// bump that lets the upstream kernel re-fire already happens inside
// interact_assign_with; this is the missing assign-side counterpart.
fn void assign_pass_epoch_bump(void) {
  ASSIGN_PASS_EPOCH++;
  if (ASSIGN_PASS_EPOCH == 0) ASSIGN_PASS_EPOCH = 1;
}

// Per-pass ASSIGN firing memo.  An UOP_ASSIGN cell reachable from more
// than one realize root (e.g. Adam's `m` assign is BOTH a top-level
// schedule_step output AND embedded in the param update that reads
// `m`) must fire its in-place buffer write exactly ONCE per pass --
// otherwise the second visit re-applies `m = m*b1 + g*(1-b1)` against
// the already-updated buffer (m -> 1.9x).  Keyed by the cell's heap loc
// + the realize-pass epoch (NOT the fire gen, which an in-place assign
// bumps mid-pass); recursive training loops get a FRESH cell loc each
// iter (alo_realize deep-copies), so no cross-iter false skip.
#define ASSIGN_FIRE_MEMO_CAP 8192u
static u64 ASSIGN_FIRE_LOC[ASSIGN_FIRE_MEMO_CAP];
static u32 ASSIGN_FIRE_EPOCH[ASSIGN_FIRE_MEMO_CAP];
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
  // 4-way slot full of this-pass entries: overwrite the base slot.  A
  // mis-claim only costs a re-fire (the prior buggy behavior), never a
  // dropped distinct assign.
  ASSIGN_FIRE_EPOCH[h] = ASSIGN_PASS_EPOCH;
  ASSIGN_FIRE_LOC[h] = loc;
  return 1;
}

// === Cross-backend kernel-input upload ===============================
// A kernel runs on its OUTPUT's backend.  When one of its inputs lives on a
// DIFFERENT backend -- a CPU weight leaf feeding a Metal-routed net -- the
// dispatch would otherwise hand the input's CPU buf id straight to the Metal
// queue, which reads its own buffer pool at that id (garbage / zeros).  Host-
// stage the input src->dst once and reuse across re-fires (a weight leaf is
// static within a realize), keyed by (src_tid, dst_backend_id).  Mirrors
// tinygrad moving an op's operands to the kernel's device before dispatch.
// Reset on the same lifecycle as the UOP_COPY upload cache
// (copy_upload_cache_reset sites).
#define KIN_UP_CAP (1u << 15)
static struct { u32 src_tid; u32 backend_id; u32 dst_tid; } KIN_UP[KIN_UP_CAP];
fn void kernel_input_upload_reset(void) { memset(KIN_UP, 0, sizeof(KIN_UP)); }

// Drop the Metal zero-copy disk-mmap WRAP cached for src_tid (a disk-mmap CPU
// weight that fed a Metal matmul).  A streaming per-block forward wraps each
// block's weights zero-copy (kernel_input_on_backend's borrowed-wrap branch),
// caches the wrap tid in KIN_UP, and never sees the per-realize pool rollback
// reclaim the borrowed MTLBuffer (those rollbacks skip borrowed slots).  After
// a block's matmuls retire, the WL streaming loader calls this per weight to
// release that block's wrap so wraps don't accumulate (one per block) for the
// whole model.  Frees the borrowed MTLBuffer wrapper (the underlying mmap
// pages stay owned by the CPU-side DiskMap, dropped separately via
// thvm_disk_buf_dontneed) and clears the KIN_UP slot so the NEXT use of this
// weight re-wraps its (possibly re-faulted) pages fresh.  No-op when src_tid
// has no Metal wrap cached.
fn void thvm_kernel_input_drop_wrap(u32 src_tid) {
  if (src_tid == 0 || src_tid >= TENS_NEXT) return;
  u32 backend_id = METAL_BACKEND.id;
  u32 h = (src_tid * 0x9e3779b1u + backend_id) & (KIN_UP_CAP - 1);
  for (u32 p = 0; p < KIN_UP_CAP; p++) {
    u32 i = (h + p) & (KIN_UP_CAP - 1);
    if (KIN_UP[i].src_tid == 0) return;                    // not cached
    if (KIN_UP[i].src_tid == src_tid && KIN_UP[i].backend_id == backend_id) {
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
// Return a tid whose buffer holds src_tid's bytes on dst_b (src_tid itself if it
// is already on dst_b, or on any failure -- the dispatch then proceeds as before
// rather than aborting).  Leaves are contiguous, so the kernel's input view
// (strides/offset) applies unchanged over the uploaded contiguous buffer.
static u32 kernel_input_on_backend(u32 src_tid, Backend *dst_b) {
  if (src_tid == 0 || src_tid >= TENS_NEXT || dst_b == NULL) return src_tid;
  Backend *sb = TENS[src_tid].backend;
  if (sb == NULL || sb == dst_b) return src_tid;            // already on dst
  if (sb->buf_read == NULL || dst_b->buf_write == NULL) return src_tid;
  u32 h = (src_tid * 0x9e3779b1u + dst_b->id) & (KIN_UP_CAP - 1);
  for (u32 p = 0; p < KIN_UP_CAP; p++) {
    u32 i = (h + p) & (KIN_UP_CAP - 1);
    if (KIN_UP[i].src_tid == 0) break;
    if (KIN_UP[i].src_tid == src_tid && KIN_UP[i].backend_id == dst_b->id) {
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
  // === Zero-copy wrap: disk-mmap weight -> Metal =======================
  // On Apple unified memory, a CPU disk-mmap source (a safetensors weight,
  // on_release == disk_map_release) staged to Metal needs no copy:
  // newBufferWithBytesNoCopy wraps the mmap pages in place so the GPU reads
  // the SAME physical bytes (no upload, no extra RSS).  We wrap the page-
  // aligned DiskMap base; the weight's first byte sits `minor` bytes in, which
  // the wrapped buffer's byte_offset carries (applied at every kernel input
  // bind, so the kernel's contiguous index 0 lands on the weight).  The dst
  // TenDesc keeps the src's contiguous offset-0 leaf view unchanged.  Declines
  // (dst_tid stays 0) on non-Metal target, non-disk src, a non-element-multiple
  // minor, or a wrap failure -> falls through to the staged host upload below.
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
          // The weight's within-map byte offset is carried by the wrapped
          // buffer's byte_offset (applied at the kernel input bind), so the
          // view stays offset-0 -- a nonzero view offset would be dropped by
          // the matmul codegen's fresh offset-0 leaf rebuild.
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
        || (KIN_UP[i].src_tid == src_tid && KIN_UP[i].backend_id == dst_b->id)) {
      KIN_UP[i].src_tid = src_tid; KIN_UP[i].backend_id = dst_b->id; KIN_UP[i].dst_tid = dst_tid;
      break;
    }
  }
  return dst_tid;
}

fn void kernel_fire_by_id(u32 kid) {
  if (kid == 0 || kid >= KERNELS_NEXT) return;
  KernelEntry *ke = &KERNELS[kid];
  if (ke->fire_gen == KERNEL_FIRE_GEN) return;     // already fired this pass
  u32 prev_fire_gen = ke->fire_gen;                // 0 = never actually fired
  ke->fire_gen = KERNEL_FIRE_GEN;

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

  // Precise fire memo (opt-in): when the coarse memo would re-fire (an
  // intervening ASSIGN bumped fire_gen), skip the dispatch + upstream
  // re-walk iff this kernel fired before AND neither its inputs nor its
  // output were ASSIGNed since that fire AND every input + the output
  // buffer is still live.  Conservative on any uncertainty -> re-fire.
  (void)prev_fire_gen;
  // ASSIGN_PASS_EPOCH != 0 guards the no-scope path (direct kernel_fire_by_id
  // with no scope_begin leaves epoch 0; a never-fired kernel also has
  // fire_pass 0, so 0==0 would falsely skip the FIRST fire).  Real realizes
  // always bump the epoch >= 1.
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
      if (!need) { PRECISE_SKIPS++; return; }
      PRECISE_REFIRE_INPUT++;
    } else {
      PRECISE_REFIRE_OUT++;
    }
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
  // An input on a different backend than this kernel (a CPU weight feeding a
  // Metal-routed net) is uploaded to the kernel's backend first.
  Backend *out_b = TENS[ke->output_tid].backend;
  u32 in_buf_ids[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 on_b = kernel_input_on_backend(resolved_tids[i], out_b);
    in_buf_ids[i] = TENS[on_b].buf_id;
    // kernel_input_on_backend moved a cross-backend leaf (a CPU weight feeding
    // a Metal-routed net) onto out_b -- either a staged upload or a zero-copy
    // disk-mmap wrap.  The kernel reads each input's strides/offset from
    // ke->input_tids[slot], not from in_buf_ids; keep them consistent by
    // rebinding the recorded tid to the moved one (both the staged copy and
    // the wrap carry a contiguous offset-0 same-shape view, so this is
    // semantically a no-op, but it keeps the recorded view's buffer identity
    // matching the bound buffer).  Only rebind the plain-leaf case the move
    // applied to; leave composed/strided slots the codegen already lowered
    // into the kernel INDEX untouched.
    if (on_b != resolved_tids[i] && ke->input_tids != NULL
        && i < ke->n_inputs && ke->input_tids[i] == resolved_tids[i]
        && (ke->input_chain_composed == NULL || !ke->input_chain_composed[i])) {
      ke->input_tids[i] = on_b;
    }
  }
  u32 out_buf_id = TENS[ke->output_tid].buf_id;

  // Re-alloc the output buf if its slot was recycled out from under us
  // (refcount==0 == "donor slot drained by buf_freelist_try_pop" or
  // "post-rollback dead slot").  Without this, a re-fire would dispatch
  // to a stale dptr -- which for CUDA is either a 0 dptr (-1 hard fail)
  // or, worse, a dptr now owned by ANOTHER live tensor (silent loss
  // overwrite -- the per-batch CE kernel re-firing into the scalar mean
  // buf slot was the documented BS=128 CUDA mnist bug).  The fresh
  // buf_alloc gets a new slot id with a fresh storage region (or one
  // recycled cleanly by the same freelist machinery), and we re-bind
  // TENS[output_tid].buf_id so subsequent fires see the live slot.
  // Tinygrad parity: their Buffer.ensure_allocated() at fire time.
  if (out_b != NULL && out_b->buf_refcount != NULL && out_b->buf_alloc != NULL
      && (out_buf_id == 0 || out_b->buf_refcount(out_buf_id) == 0)) {
    u64 nbytes = dtype_storage_bytes(ke->output_dtype, (u64)ke->output_numel);
    u32 new_id = out_b->buf_alloc(nbytes);
    if (new_id != 0) {
      TENS[ke->output_tid].buf_id = new_id;
      out_buf_id = new_id;
    }
  }

  // First-fire opt decisions.  Two layers:
  //   - hand-coded heuristic (HAND_CODED_OPTS, default ON; NOOPT=1 off):
  //     apply tinygrad-style UPCAST/LOCAL/GROUP/UNROLL/TC by default,
  //     no benchmarking.  Runs first so BEAM (if enabled) can search
  //     beyond the heuristic baseline.
  //   - BEAM autotune (AUTOTUNE=1 or BEAM>0): benchmark proposer candidates.
  // Both run after producers are populated so a direct dispatch sees
  // ready input buffers; neither perturbs the fire-generation memo or
  // TJit capture.
  if (kernel_should_hand_code_opts(ke)) {
    kernel_hand_coded_opts(ke);
  }
  if (kernel_should_autotune(ke)) {
    kernel_autotune(kid);
  }

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
    // THVM_DISPATCH_TRACE=1: one line per actual dispatch, with the
    // fire-gen + scope depth + capturing flag.  Used to localize
    // redundant re-dispatch (e.g. a kernel firing N x per step): if the
    // same kid logs many lines with DIFFERENT fire_gen, the redundancy
    // is multiple fire scopes; with the SAME fire_gen it's a memo miss.
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
    b->dispatch_kernel(ke, in_buf_ids, out_buf_id);
    // Precise memo bookkeeping: record this dispatch's OUTPUT write so a
    // downstream kernel reading it re-fires iff WE re-fired (propagates
    // re-fires through the kernel chain -- a fresh forward correctly
    // re-runs when an input changed; an unchanged upstream still lets
    // downstream skip, preserving the dedup).  Record BEFORE stamping
    // fire_assign_seq so our own output write (seq == fire_assign_seq)
    // doesn't make US look dirty on a later re-walk.
    if (b == out_b && out_buf_id != 0) kernel_assign_write_record(b, out_buf_id);
    ke->fire_assign_seq = ASSIGN_SEQ;
    ke->fire_pass       = ASSIGN_PASS_EPOCH;
  }

  // Per-fire consumer-count decref removed alongside `fired`: the
  // assumption "one fire = one full read of every input" only holds
  // for the static one-shot case, and re-firing kernels in optimizer
  // loops would over-decrement.  Future work: structural lifetime
  // analysis on the kernel DAG, not per-fire accounting.

  ITRS++;
  multi_emit(RULE_UOP_KERNEL, MULTI_TERM, (u64)kid, 0, 0);
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
