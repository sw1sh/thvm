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
  u32 in_buf_ids[ke->n_inputs ? ke->n_inputs : 1];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    in_buf_ids[i] = TENS[resolved_tids[i]].buf_id;
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
  Backend *out_b = TENS[ke->output_tid].backend;
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
        fprintf(stderr, "[dispatch] kid=%u gen=%u depth=%u cap=%d out_buf=%u\n",
                (u32)(ke - KERNELS), KERNEL_FIRE_GEN, KERNEL_FIRE_SCOPE_DEPTH,
                jit_is_capturing() ? 1 : 0, out_buf_id);
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
