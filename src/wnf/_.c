// wnf -- reduce a term to weak normal form.
//
// Stack-machine reducer modeled on HVM4's clang/wnf/_.c.  Two phases:
//
//   enter:  walk into the head position, pushing eliminator frames
//           (APP, DP0, DP1) until we reach a WHNF root (LAM, ERA,
//           SUP, or an unsubstituted VAR).
//
//   apply:  pop frames in LIFO order and dispatch the active-pair
//           interaction with the WHNF root.  Some interactions
//           (APP-LAM, DUP-SUP) produce a new term to re-enter; others
//           (APP-ERA, DUP-ERA) produce a fresh WHNF directly.
//
// `WNF_STACK` and `WNF_S_POS` are the global stack and head used by
// every reducer call.  We snapshot `base = WNF_S_POS` at entry so a
// nested wnf() invocation would still terminate at its own base
// (the runtime is single-threaded today, so nesting is moot, but the
// pattern matches HVM4 and costs nothing).
//
// `wnf_n(t, max_steps)` is the step-bounded form: the budget is
// checked just *before* each interaction-firing site (every place
// the reducer would bump ITRS).  Free reductions -- VAR-SUB / DUP
// follows, APP/DP frame pushes -- always run, so a single step
// resolves to a meaningful WHNF rather than stopping mid-deref.
// On bail we snapshot the still-pending eliminator frames into
// `WNF_LAST_STACK` (innermost-first) and unwind via the same
// "stuck term" path the apply loop uses for non-redex pairs --
// heap mutations stick, so calling wnf again on the returned root
// resumes the reduction.
//
// `wnf(t)` is the unbounded form (max_steps = 0).
//
// WNF_LAST_STACK / WNF_LAST_STACK_LEN now live in TContext (see
// thvm.h); the macros in this file resolve to ctx fields.  Storage
// is heap-allocated in thvm_init / thvm_context_create.

fn Term wnf_n(Term term, u64 max_steps) {
  HOT_WNF_CALLS++;
  Term *stack = WNF_STACK;
  u32   s_pos = WNF_S_POS;
  u32   base  = s_pos;
  Term  next  = term;
  Term  whnf;
  u64   itrs0 = ITRS;

#define BUDGET_HIT (max_steps && (ITRS - itrs0) >= max_steps)
#define BAIL_AT(t) do { whnf = (t); goto bail; } while (0)

enter:
  switch (term_tag(next)) {
    case TAG_VAR: {
      u64  loc  = term_val(next);
      // Acquire-load: pairs with heap_subst_var's release-store so we
      // never observe the SUB bit without the matching value bits
      // (which would be UB at T>1 with plain heap_read).
      Term cell = heap_read_acq(loc);
      if (term_sub_get(cell)) {
        next = term_sub_set(cell, 0);
        goto enter;
      }
      whnf = next;
      goto apply;
    }
    case TAG_DP0:
    case TAG_DP1: {
      u64  loc  = term_val(next);
      // Grad-flavored projection: dispatch BEFORE touching the cell
      // so the other projection can still read y.
      if (term_ext(next) & DUP_GRAD_FLAG) {
        if (BUDGET_HIT) BAIL_AT(next);
        if (term_tag(next) == TAG_DP0) {
          // FWD passthrough: cell holds y, force it.
          ITRS++;
          multi_emit(RULE_GRAD_FWD, MULTI_TERM, 0, 0, 0);
          next = heap_read(loc);
          goto enter;
        }
        // BWD: HVM4-style stack-based descent.  Push the grad-DP1
        // frame, descend into cell[0] (= y) so the normal enter loop
        // drives any nested DPs / UOPs to head form.  The apply phase
        // will pop the frame and call interact_grad with the resolved
        // y.  This avoids the reentrant wnf() approach (which broke
        // with deep nesting via cell-consumption / shared-stack
        // surprises) and naturally handles arbitrarily deep DP-DP
        // structures from prior chain-rule rounds.
        stack[s_pos++] = next;
        next = heap_read(loc + 0);
        goto enter;
      }
      // Plain (non-grad) DP projection: HVM4-style dynamic projection.
      // Push the DP frame and descend into the cell body so the apply
      // phase can fire DUP-XXX (DUP-LAM, DUP-SUP, DUP-NUM, DUP-CTR, ...)
      // as soon as the body reaches WHNF.  Levy-opaque book-time
      // projection is TAG_BJ0/BJ1 below.
      //
      // SUB-bit shortcut: an earlier sibling fire already resolved this
      // projection; follow the SUB chain.  Acquire-load matches
      // heap_subst_var's release-store on the writer side.
      Term cell = heap_read_acq(loc);
      if (term_sub_get(cell)) {
        next = term_sub_set(cell, 0);
        goto enter;
      }
      stack[s_pos++] = next;
      next = cell;
      goto enter;
    }
    case TAG_BJ0:
    case TAG_BJ1: {
      // Book-time projection (HVM4 BJ).  Levy-opaque under wnf -- we
      // never fire DUP-XXX in place; alo_realize is what unfolds these
      // into fresh dyn-heap DP cells when their body is sub'd into a
      // book copy.  Identical SUB-chain follow as the DP arm above.
      u64  loc  = term_val(next);
      Term cell = heap_read_acq(loc);
      if (term_sub_get(cell)) {
        next = term_sub_set(cell, 0);
        goto enter;
      }
      whnf = next;
      goto apply;
    }
    case TAG_APP: {
      u64  loc = term_val(next);
      Term fun = heap_read(loc);
      stack[s_pos++] = next;
      next = fun;
      goto enter;
    }
    case TAG_DUP: {
      u64  loc  = term_val(next);
      Term body = heap_read(loc);
      next = body;
      goto enter;
    }
    case TAG_REF: {
      // Lazy ALO unfold.  ALO-VAR / ALO-LAM / ALO-NOD then walk one
      // layer per fire as wnf re-enters this term (no eager expansion
      // even for self-referential defs).
      //
      // (Legacy AOT had a fast path here that dispatched to AOT_FNS
      // for registered defs.  The Bend2-style AOT replacing it works
      // outside of this loop -- the runtime calls AOT entry points
      // directly with Tasks, not through wnf descent.)
      u32  name = term_ext(next);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0) {
        // Undefined ref -- treat as WHNF atom; won't reduce further.
        whnf = next;
        goto apply;
      }
      if (BUDGET_HIT) BAIL_AT(next);
      // REF -> ALO unfolding is structural, not a beta/dup/mat
      // reduction -- HVM4's wnf_alo_*.c never bumps ITRS for it
      // either.  Counting these inflated thvm's ITRS ~3x versus
      // HVM4 on REF-heavy benches like fib_nat.
      next = alo_realize(book, 0);
      goto enter;
    }
    case TAG_ALO: {
      if (BUDGET_HIT) BAIL_AT(next);
      next = alo_force(next);
      goto enter;
    }
    case TAG_UOP: {
      u32 op = term_ext(next);
      if (op == UOP_KERNEL) {
        // Fire all upstream kernels then this one; return the output TAG_TEN.
        if (BUDGET_HIT) BAIL_AT(next);
        whnf = interact_kernel(next);
        goto apply;
      }
      // (slots UOP_GRAD/UOP_FWD have moved to TAG_DP0/DP1+DUP_GRAD_FLAG;
      // see the TAG_DP{0,1} branch above.)
      if (op == UOP_ASSIGN) {
        // Force src (heap[loc+1]) first -- could be a kernel chain
        // that has to fire before we have a TEN to copy from.  dst
        // (heap[loc+0]) should already be a TEN handle but resolve
        // it for symmetry.  Once both are TEN, hand the resolved
        // values to interact_assign_with which memcpys src.buf ->
        // dst.buf and returns dst.
        //
        // Recursive training loops produce a FRESH ASSIGN cell each
        // iteration via alo_realize (REF/ALO unfold deep-copies into
        // dyn heap), so heap-mutating the cells per fire is fine --
        // next iter has its own copy and re-fires its own upstream
        // kernel chain.  We pass the resolved values directly rather
        // than heap_set'ing them so the original cell structure is
        // preserved for tooling (THeapDiagram, debug traces).
        u64  aloc   = term_val(next);
        if (BUDGET_HIT) BAIL_AT(next);
        // Sync WNF_S_POS with our local s_pos before reentering wnf:
        // the outer (this) call may have pushed frames (F_UOP_CHILD
        // ancestors).  Without this, the inner wnf snapshots a stale
        // WNF_S_POS and clobbers outer frames.  Re-read on return.
        WNF_S_POS = s_pos;
        Term src_w  = wnf(heap_read(aloc + 1));
        s_pos = WNF_S_POS;
        WNF_S_POS = s_pos;
        Term dst_w  = wnf(heap_read(aloc + 0));
        s_pos = WNF_S_POS;
        if (term_tag(src_w) == TAG_TEN && term_tag(dst_w) == TAG_TEN) {
          whnf = interact_assign_with(dst_w, src_w);
          goto apply;
        }
        // Either side stuck (e.g. dst still a UOP) -- leave as WHNF.
        whnf = next;
        goto apply;
      }
      // BUFFER / CONST / VIEW / movement / elementwise / REDUCE / ...
      // are WNF by themselves; they become active only inside a KERNEL AST
      // the interpreter walks after firing.
      //
      // BUT: a descendant slot may hold an ACTIVE term (chain-rule
      // DP1_GRAD; DUP-projection inside a UOP from TGrad's leafTids
      // nest; nested UOP_ASSIGN / UOP_KERNEL).  wnf would otherwise
      // leave it unfired and materialize would BAIL.  Push an
      // F_UOP_CHILD frame for the first active child and descend
      // into it; the apply phase resumes scanning for siblings (and
      // recursively drives the result if it's itself UOP-with-
      // actives) until the parent UOP is fully WHNF.  When all
      // children are passive (the common forward-only case), the
      // initial scan finds nothing and we fall through immediately.
      u64 uloc = term_val(next);
      u8  uar  = uop_arity((u8)op);
      u8  next_idx = uop_next_active_child(uloc, uar, 0);
      if (next_idx < uar) {
        if (BUDGET_HIT) BAIL_AT(next);
        // Frame ext layout:
        //   bits  0.. 7  child idx (slot we're driving)
        //   bits  8..15  parent UOP opcode (so apply rebuilds the
        //                parent Term without re-reading the heap)
        //   bit  16      rescan flag: when set, apply skips the
        //                heap_set (already done before recursive
        //                descent) and just resumes scanning from
        //                child_idx for the next active sibling.
        u32 packed = (u32)next_idx | ((u32)op << 8);
        stack[s_pos++] = term_new(0, TAG_F_UOP_CHILD, packed, uloc);
        next = heap_read(uloc + next_idx);
        goto enter;
      }
      whnf = next;
      goto apply;
    }
    case TAG_OP2: {
      // HVM4-style stack-based strict eval.  Push the OP2 frame and
      // descend into x.  When x reduces (apply phase), if x is NUM
      // and y is also NUM, fire op directly; otherwise push a
      // F_OP2_NUM frame with x's value baked in and descend into y.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_EQL: {
      // HVM4-style stack-based strict eval.  Push EQL frame, descend
      // into a.  Apply pops EQL: dispatches on a's WHNF (ERA/ANY/SUP
      // handle without forcing b; otherwise push F_EQL_R and descend
      // into b).  The full rule table:
      //   EQL-ERA-{L,R}: ERA on either side -> ERA
      //   EQL-ANY-{L,R}: ANY on either side -> NUM(1)
      //   EQL-SUP-L: EQL(&L{a0,a1}, b) -> &L{EQL(a0,B0), EQL(a1,B1)}
      //              with !&L{B0,B1}=b (DUP duplicates b)
      //   EQL-SUP-R: symmetric
      //   EQL-NUM-NUM: compare values, NUM(1) if equal else NUM(0)
      //   otherwise: stuck
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_AND: {
      // HVM4-style: push AND frame, descend into a (the strict slot).
      // Apply dispatches on a's WHNF; b stays lazy until needed.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_OR: {
      // HVM4-style: push OR frame, descend into a.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_WHEN: {
      // HVM4-style: push WHEN frame, descend into cond.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_ANN: {
      // HVM4-style: push ANN frame, descend into typ (the strict slot).
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 1);
      goto enter;
    }
    case TAG_DSU: {
      // Dynamic-label SUP: strict on the label term.  Push DSU frame
      // (the frame IS the DSU itself, since its loc is in val and we
      // know to descend into label at +0), descend into label.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_DDU: {
      // Dynamic-label DUP: same shape as DSU -- strict on label.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_LAM:
    case TAG_ERA:
    case TAG_SUP:
    case TAG_MAT:
    case TAG_BRI:
    default: {
      whnf = next;
      goto apply;
    }
  }

apply:
  while (s_pos > base) {
    Term frame = stack[--s_pos];
    switch (term_tag(frame)) {
      case TAG_APP: {
        u64  app_loc = term_val(frame);
        Term arg     = heap_read(app_loc + 1);
        switch (term_tag(whnf)) {
          case TAG_LAM: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = interact_app_lam(whnf, arg);
            goto enter;
          }
          case TAG_BRI: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = interact_app_bri(whnf, arg);
            goto enter;
          }
          case TAG_PRI: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            // 8.1b: accumulate arg into PRI's buffer; saturated
            // call returns the result Term, partial returns a
            // larger PRI -- both go through `next` for further
            // reduction.
            next = interact_app_pri(whnf, arg);
            goto enter;
          }
          case TAG_SUP: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            // 8.1d-i: APP-SUP commutation -- distribute the APP
            // across the SUP's children, sharing arg via a DUP.
            next = interact_app_sup(whnf, arg);
            goto enter;
          }
          case TAG_ERA: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_app_era();
            continue;
          }
          case TAG_MAT: {
            // APP-MAT: force the arg, dispatch by its tag.
            //   APP-MAT-NUM (arg is TAG_NUM, val == match):  return handler.
            //   APP-MAT-CTR (arg is TAG_CTR, ext == match):  destructure --
            //                  apply handler to each CTR child via APP-chain.
            //                  Mirrors HVM4's APP-MAT-CTR-MAT: a MAT lambda
            //                  matching constructor #K applied to a CTR with
            //                  the same #K extracts its fields and passes
            //                  them positionally to the handler.
            //   miss:           build APP(fallback, arg) and continue.
            // Heap[mat_loc+0] = handler.
            // Heap[mat_loc+1] = fallback.
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            u64  mat_loc = term_val(whnf);
            u32  match   = term_ext(whnf);
            // Sync WNF_S_POS with our local s_pos before reentering
            // wnf: the outer (this) call has already pushed APP frames
            // onto the shared stack.  Without the sync the inner wnf
            // would start at a stale base and overwrite our frames.
            // Drive a Levy-opaque DP head through cnf so DUP-CTR /
            // DUP-NUM fire before the case-tree dispatches; for non-DP
            // heads, plain wnf is enough and cheaper than a full cnf
            // walk (which would descend into compound children and
            // pay for SUP-lift even when there's no SUP).
            WNF_S_POS = s_pos;
            Term arg_w   = wnf(arg);
            if (term_tag(arg_w) == TAG_DP0 || term_tag(arg_w) == TAG_DP1) {
              arg_w = cnf(arg_w);
            }
            s_pos = WNF_S_POS;
            ITRS++;
            multi_emit(RULE_MAT_DISPATCH, MULTI_TERM, 0, 0, match);
            if (term_tag(arg_w) == TAG_NUM &&
                (u32)term_val(arg_w) == match) {
              next = heap_read(mat_loc + 0);
              goto enter;
            }
            if (term_tag(arg_w) == TAG_CTR && term_ext(arg_w) == match) {
              // Destructure: apply handler to each CTR child via an
              // APP chain.  Reuse the consumed MAT cell for the first
              // APP and the consumed CTR's child cells for the second
              // APP; only allocate fresh storage from APP #3 onward.
              // Mirrors HVM4's wnf_app_mat_ctr.c cell-reuse trick.
              //
              // Layout reminder:
              //   MAT cell:  [mat_loc+0 = handler, mat_loc+1 = fallback]
              //   CTR cell:  [ctr_loc+0 = NUM(arity),
              //               ctr_loc+1 = c_0, ctr_loc+2 = c_1, ...]
              // APP cell: [loc+0 = fun, loc+1 = arg].
              //   APP #1 fits at mat_loc (already 2 cells; slot 0
              //     already holds handler, write child0 to slot 1).
              //   APP #2 fits at ctr_loc+1..ctr_loc+2 (the slots that
              //     held c_0 and c_1; write APP#1 to ctr_loc+1 and
              //     keep c_1 at ctr_loc+2).
              Term handler = heap_read(mat_loc + 0);
              u32 n = term_ctr_n(arg_w);
              if (n == 0) {
                next = handler;
                goto enter;
              }
              u64 ctr_loc = term_val(arg_w);
              Term child0 = heap_read(ctr_loc + 1);
              heap_set(mat_loc + 1, child0);
              Term res = term_new(0, TAG_APP, 0, mat_loc);
              if (n == 1) { next = res; goto enter; }
              Term child1 = heap_read(ctr_loc + 2);
              heap_set(ctr_loc + 1, res);
              heap_set(ctr_loc + 2, child1);
              res = term_new(0, TAG_APP, 0, ctr_loc + 1);
              if (n == 2) { next = res; goto enter; }
              u64 apps = heap_alloc(2 * (u64)(n - 2));
              for (u32 i = 2; i < n; i++) {
                u64 a = apps + 2 * (u64)(i - 2);
                heap_set(a + 0, res);
                heap_set(a + 1, term_ctr_at(arg_w, i));
                res = term_new(0, TAG_APP, 0, a);
              }
              next = res;
              goto enter;
            }
            // arg is SUP: fire APP-MAT-SUP commute (clone the
            // case-tree at the SUP's label so each branch gets its
            // own copy).  Without this, MAT with a SUP scrutinee
            // would silently fall through to the fallback below,
            // collapsing the SUP enumeration.
            if (term_tag(arg_w) == TAG_SUP) {
              next = interact_app_mat_sup(whnf, arg_w);
              goto enter;
            }
            // Miss: build APP(fallback, arg_w).  Reuse the consumed
            // MAT cell -- it's already 2 words, slot 1 already holds
            // fallback; copy fallback to slot 0, write arg_w to slot 1.
            heap_set(mat_loc + 0, heap_read(mat_loc + 1));
            heap_set(mat_loc + 1, arg_w);
            next = term_new(0, TAG_APP, 0, mat_loc);
            goto enter;
          }
          default: {
            heap_set(app_loc + 0, whnf);
            whnf = frame;
            continue;
          }
        }
      }
      case TAG_DP0:
      case TAG_DP1: {
        // Grad-flavored DP1 frame: BWD chain-rule dispatch.
        if (term_tag(frame) == TAG_DP1 && (term_ext(frame) & DUP_GRAD_FLAG)) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          u64 gloc = term_val(frame);
          heap_set(gloc + 0, whnf);
          Term g = interact_grad(frame);
          if (g == frame) {
            // Stuck (chain rule can't pattern-match -- e.g. y is a
            // SUP that hasn't been routed yet).  Propagate frame as
            // WHNF; outer DUPs / etc. will retry.
            whnf = frame;
            continue;
          }
          ITRS++;
          multi_emit(RULE_GRAD_BWD, MULTI_TERM, 0, 0, 0);
          next = g;
          goto enter;
        }
        // Plain (non-grad) DP frame: HVM4-style dynamic projection
        // dispatch on body's WHNF tag.  Mirrors redex.c's DP firing.
        u64 loc  = term_val(frame);
        u32 lab  = term_ext(frame);
        u8  side = (term_tag(frame) == TAG_DP0) ? 0 : 1;
        switch (term_tag(whnf)) {
          case TAG_SUP: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = interact_dup_sup(lab, loc, side, whnf);
            goto enter;
          }
          case TAG_ERA: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_dup_era(side, loc, whnf);
            continue;
          }
          case TAG_LAM: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = interact_dup_lam(lab, loc, side, whnf);
            goto enter;
          }
          case TAG_BRI: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = interact_dup_bri(lab, loc, side, whnf);
            goto enter;
          }
          case TAG_NUM: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_dup_num(side, loc, whnf);
            continue;
          }
          case TAG_ANY: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_dup_any(side, loc, whnf);
            continue;
          }
          case TAG_TEN: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_dup_ten(side, loc, whnf);
            continue;
          }
          case TAG_CTR: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = interact_dup_ctr(lab, loc, side, whnf);
            goto enter;
          }
          case TAG_UOP: {
            Term r = interact_dup_uop(lab, loc, side, whnf);
            if (r == 0) {
              // Active UOP (KERNEL/ASSIGN) -- can't commute yet.  Restore
              // body to its slot and propagate the DP up as WHNF.
              heap_set(loc, whnf);
              whnf = frame;
              continue;
            }
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = r;
            goto enter;
          }
          default: {
            // Body cnfs to a non-dispatchable tag (REF/ALO/VAR/INC/
            // another DP/BJ).  Restore body and propagate the DP root
            // as WHNF.  Outer consumers (cnf, etc.) will retry.
            heap_set(loc, whnf);
            whnf = frame;
            continue;
          }
        }
      }
      case TAG_DSU: {
        // DSU frame: label reduced to whnf.  Dispatch:
        //   NUM(n) -> SUP^n{a, b}  (DSU-NUM)
        //   ERA    -> ERA          (DSU-ERA)
        //   SUP    -> nested SUP via cross-product on cloned (a,b)
        //                          (DSU-SUP)
        //   DP0/DP1 -> drive through cnf so any DUP-XXX fires first
        //   else   -> stuck: write whnf back to label slot; return frame
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_DP0 || term_tag(whnf) == TAG_DP1) {
          WNF_S_POS = s_pos;
          whnf = cnf(whnf);
          s_pos = WNF_S_POS;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          next = interact_dsu_num(loc, whnf);
          goto enter;
        }
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          whnf = interact_dsu_era();
          continue;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          next = interact_dsu_sup(loc, whnf);
          goto enter;
        }
        // Label stuck: rebuild DSU with reduced label.
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_DDU: {
        // DDU frame: label reduced to whnf.  Same dispatch shape as
        // DSU but with the DDU rules.
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_DP0 || term_tag(whnf) == TAG_DP1) {
          WNF_S_POS = s_pos;
          whnf = cnf(whnf);
          s_pos = WNF_S_POS;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          next = interact_ddu_num(loc, whnf);
          goto enter;
        }
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          whnf = interact_ddu_era();
          continue;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          next = interact_ddu_sup(loc, whnf);
          goto enter;
        }
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_OP2: {
        // OP2 frame: x reduced to whnf.  If x is NUM and y is also
        // NUM (cheap heap_read), fire op directly.  Otherwise push a
        // F_OP2_NUM frame with x's value baked in and descend into
        // y.  If x didn't reach NUM, OP2 is stuck.
        u64 loc = term_val(frame);
        u32 op  = term_ext(frame);
        // Drive a Levy-opaque DP through cnf so DUP-NUM / DUP-SUP fire
        // before dispatching this strict frame (Phase 1+2 readback).
        if (term_tag(whnf) == TAG_DP0 || term_tag(whnf) == TAG_DP1) {
          WNF_S_POS = s_pos;
          whnf = cnf(whnf);
          s_pos = WNF_S_POS;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          Term y = heap_read(loc + 1);
          if (term_tag(y) == TAG_NUM) {
            ITRS++;
            multi_emit(RULE_OP2_NUM_NUM, MULTI_TERM, 0, 0, op);
            u32 xv = (u32)term_val(whnf);
            u32 yv = (u32)term_val(y);
            u32 r;
            switch (op) {
              case OP_ADD: r = xv + yv; break;
              case OP_SUB: r = xv - yv; break;
              case OP_MUL: r = xv * yv; break;
              case OP_EQ:  r = (xv == yv) ? 1 : 0; break;
              case OP_LT:  r = (xv <  yv) ? 1 : 0; break;
              case OP_DIV: r = (yv == 0) ? 0 : xv / yv; break;
              case OP_MOD: r = (yv == 0) ? 0 : xv % yv; break;
              case OP_XOR: r = xv ^ yv; break;
              case OP_AND: r = xv & yv; break;
              case OP_OR:  r = xv | yv; break;
              case OP_SHL: r = xv << (yv & 31); break;
              case OP_SHR: r = xv >> (yv & 31); break;
              case OP_GT:  r = (xv >  yv) ? 1 : 0; break;
              case OP_LE:  r = (xv <= yv) ? 1 : 0; break;
              case OP_GE:  r = (xv >= yv) ? 1 : 0; break;
              case OP_NE:  r = (xv != yv) ? 1 : 0; break;
              default:     r = 0; break;
            }
            whnf = term_new(0, TAG_NUM, term_ext(whnf), r);
            continue;
          }
          // x is NUM; descend into y with F_OP2_NUM frame holding
          // x's NUM raw bits (val) + dtype (low 6 bits of ext, op
          // packed at upper bits via ext field directly = op).
          // Pack: ext = op | (dtype << 8); val = NUM raw bits.
          u32 dtype = term_ext(whnf);
          u32 packed_ext = (op & 0xFF) | ((dtype & 0xFF) << 8);
          stack[s_pos++] = term_new(0, TAG_F_OP2_NUM, packed_ext, term_val(whnf));
          next = y;
          goto enter;
        }
        // x is SUP: fire OP2-SUP commute (clone y at SUP's label).
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          Term y = heap_read(loc + 1);
          whnf = interact_op2_sup(op, whnf, y);
          continue;
        }
        // x stuck (not NUM, not SUP): rebuild OP2 with reduced x.
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_F_OP2_NUM: {
        // F_OP2_NUM frame: x is NUM (val=raw bits, ext low 8=op,
        // ext bits 8..15=dtype).  Whnf is y's reduction.  If NUM,
        // fire op; otherwise rebuild OP2 with reduced y.
        u32 packed_ext = term_ext(frame);
        u32 op    = packed_ext & 0xFF;
        u32 dtype = (packed_ext >> 8) & 0xFF;
        u32 xv    = (u32)term_val(frame);
        if (term_tag(whnf) == TAG_DP0 || term_tag(whnf) == TAG_DP1) {
          WNF_S_POS = s_pos;
          whnf = cnf(whnf);
          s_pos = WNF_S_POS;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_OP2_NUM_NUM, MULTI_TERM, 0, 0, op);
          u32 yv = (u32)term_val(whnf);
          u32 r;
          switch (op) {
            case OP_ADD: r = xv + yv; break;
            case OP_SUB: r = xv - yv; break;
            case OP_MUL: r = xv * yv; break;
            case OP_EQ:  r = (xv == yv) ? 1 : 0; break;
            case OP_LT:  r = (xv <  yv) ? 1 : 0; break;
            case OP_DIV: r = (yv == 0) ? 0 : xv / yv; break;
            case OP_MOD: r = (yv == 0) ? 0 : xv % yv; break;
            case OP_XOR: r = xv ^ yv; break;
            case OP_AND: r = xv & yv; break;
            case OP_OR:  r = xv | yv; break;
            case OP_SHL: r = xv << (yv & 31); break;
            case OP_SHR: r = xv >> (yv & 31); break;
            case OP_GT:  r = (xv >  yv) ? 1 : 0; break;
            case OP_LE:  r = (xv <= yv) ? 1 : 0; break;
            case OP_GE:  r = (xv >= yv) ? 1 : 0; break;
            case OP_NE:  r = (xv != yv) ? 1 : 0; break;
            default:     r = 0; break;
          }
          whnf = term_new(0, TAG_NUM, dtype, r);
          continue;
        }
        // y is SUP: fire OP2-NUM-SUP commute (no DUP needed; the
        // baked-in NUM is atomic and reusable across both branches).
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          Term x = term_new(0, TAG_NUM, dtype, xv);
          whnf = interact_op2_num_sup(op, x, whnf);
          continue;
        }
        // y stuck (not NUM, not SUP): rebuild OP2(NUM(xv), whnf).
        Term x = term_new(0, TAG_NUM, dtype, xv);
        u64 nloc = heap_alloc(2);
        heap_set(nloc + 0, x);
        heap_set(nloc + 1, whnf);
        whnf = term_new(0, TAG_OP2, op, nloc);
        continue;
      }
      case TAG_F_UOP_CHILD: {
        // Active UOP child finished.  Frame layout: ext low 8 bits
        // = child idx, bits 8..15 = parent UOP opcode (baked at
        // enter-site push so apply rebuilds the parent Term).
        // whnf may itself be a UOP whose children are now active
        // (e.g. DUP-UOP commute returns a UOP with fresh DUPs).
        // Recursively re-drive via reentrant wnf so any new actives
        // fire before we finalize the parent.
        u64 uloc      = term_val(frame);
        u32 packed    = term_ext(frame);
        u8  child_idx = (u8)(packed & 0xFF);
        u8  parent_op = (u8)((packed >> 8) & 0xFF);
        // Re-drive whnf to FULL WHNF if it's a UOP with active
        // descendants (e.g. DUP-UOP commute returns a UOP with
        // fresh DUPs around its children).  Sync WNF_S_POS so the
        // inner snapshots its base ABOVE this outer's frames.
        if (term_tag(whnf) == TAG_UOP) {
          u32 wop = term_ext(whnf);
          if (wop != UOP_KERNEL && wop != UOP_ASSIGN) {
            u64 wloc = term_val(whnf);
            u8  war  = uop_arity((u8)wop);
            if (uop_next_active_child(wloc, war, 0) < war) {
              WNF_S_POS = s_pos;
              whnf = wnf(whnf);
              s_pos = WNF_S_POS;
            }
          }
        }
        heap_set(uloc + child_idx, whnf);
        u8 ar       = uop_arity(parent_op);
        u8 next_idx = uop_next_active_child(uloc, ar, child_idx + 1);
        if (next_idx < ar) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          stack[s_pos++] = term_new(0, TAG_F_UOP_CHILD,
                                    (u32)next_idx | ((u32)parent_op << 8),
                                    uloc);
          next = heap_read(uloc + next_idx);
          goto enter;
        }
        whnf = term_new(0, TAG_UOP, parent_op, uloc);
        continue;
      }
      case TAG_EQL: {
        // EQL frame: a reduced to whnf.  ERA/ANY short-circuit;
        // SUP-L commutes; otherwise store a back, push F_EQL_R,
        // descend into b.
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_ERA, MULTI_PRUNE, 0, 0, 0);
          continue;   // whnf = ERA stays
        }
        if (term_tag(whnf) == TAG_ANY) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_ANY, MULTI_TERM, 0, 0, 0);
          whnf = term_new(0, TAG_NUM, 0, 1);
          continue;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          u32  lab  = term_ext(whnf);
          u64  sloc = term_val(whnf);
          Term a0   = heap_read(sloc + 0);
          Term a1   = heap_read(sloc + 1);
          Term b    = heap_read(loc + 1);
          u64  dup  = heap_alloc(1);
          heap_set(dup, b);
          Term b0   = term_new(0, TAG_DP0, lab, dup);
          Term b1   = term_new(0, TAG_DP1, lab, dup);
          Term e0   = term_new_eql(a0, b0);
          Term e1   = term_new_eql(a1, b1);
          u64  ns   = heap_alloc(2);
          heap_set(ns + 0, e0);
          heap_set(ns + 1, e1);
          ITRS++;
          multi_emit(RULE_EQL_SUP, MULTI_SLIDE, 0, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        // Store a's WHNF in heap[loc+0]; push F_EQL_R; descend into b.
        if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
        heap_set(loc + 0, whnf);
        stack[s_pos++] = term_new(0, TAG_F_EQL_R, 0, loc);
        next = heap_read(loc + 1);
        goto enter;
      }
      case TAG_F_EQL_R: {
        // F_EQL_R frame: b reduced to whnf, a's WHNF stored at
        // heap[loc+0].  Apply the right-side rules.
        u64  loc = term_val(frame);
        Term a   = heap_read(loc + 0);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_ERA, MULTI_PRUNE, 0, 0, 0);
          continue;   // whnf = ERA stays
        }
        if (term_tag(whnf) == TAG_ANY) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_ANY, MULTI_TERM, 0, 0, 0);
          whnf = term_new(0, TAG_NUM, 0, 1);
          continue;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          u32  lab  = term_ext(whnf);
          u64  sloc = term_val(whnf);
          Term b0   = heap_read(sloc + 0);
          Term b1   = heap_read(sloc + 1);
          u64  dup  = heap_alloc(1);
          heap_set(dup, a);
          Term a0   = term_new(0, TAG_DP0, lab, dup);
          Term a1   = term_new(0, TAG_DP1, lab, dup);
          Term e0   = term_new_eql(a0, b0);
          Term e1   = term_new_eql(a1, b1);
          u64  ns   = heap_alloc(2);
          heap_set(ns + 0, e0);
          heap_set(ns + 1, e1);
          ITRS++;
          multi_emit(RULE_EQL_SUP, MULTI_SLIDE, 0, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        if (term_tag(a) == TAG_NUM && term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_NUM, MULTI_TERM, 0, 0, 0);
          u32 r = ((u32)term_val(a) == (u32)term_val(whnf)) ? 1 : 0;
          whnf = term_new(0, TAG_NUM, term_ext(a), r);
          continue;
        }
        // Both stuck: rebuild EQL(a, b) with reduced values in heap.
        heap_set(loc + 1, whnf);
        whnf = term_new(0, TAG_EQL, 0, loc);
        continue;
      }
      case TAG_AND: {
        // AND frame: a reduced to whnf.
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_AND_ERA, MULTI_PRUNE, 0, 0, 0);
          continue;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_AND_NUM, MULTI_TERM, 0, 0, 0);
          if ((u32)term_val(whnf) == 0) {
            continue;     // whnf = NUM(0) stays
          }
          // a is NUM(non-zero): result is wnf(b).  Drop AND frame,
          // descend into b.
          next = heap_read(loc + 1);
          goto enter;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          u32  lab  = term_ext(whnf);
          u64  sloc = term_val(whnf);
          Term a0   = heap_read(sloc + 0);
          Term a1   = heap_read(sloc + 1);
          Term b    = heap_read(loc + 1);
          u64  dup  = heap_alloc(1);
          heap_set(dup, b);
          Term n0   = term_new_and(a0, term_new(0, TAG_DP0, lab, dup));
          Term n1   = term_new_and(a1, term_new(0, TAG_DP1, lab, dup));
          u64  ns   = heap_alloc(2);
          heap_set(ns + 0, n0);
          heap_set(ns + 1, n1);
          ITRS++;
          multi_emit(RULE_AND_SUP, MULTI_SLIDE, 0, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        // a stuck.
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_OR: {
        // OR frame: a reduced to whnf.
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_OR_ERA, MULTI_PRUNE, 0, 0, 0);
          continue;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_OR_NUM, MULTI_TERM, 0, 0, 0);
          if ((u32)term_val(whnf) == 0) {
            // a is NUM(0): result is wnf(b).
            next = heap_read(loc + 1);
            goto enter;
          }
          whnf = term_new(0, TAG_NUM, term_ext(whnf), 1);
          continue;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          u32  lab  = term_ext(whnf);
          u64  sloc = term_val(whnf);
          Term a0   = heap_read(sloc + 0);
          Term a1   = heap_read(sloc + 1);
          Term b    = heap_read(loc + 1);
          u64  dup  = heap_alloc(1);
          heap_set(dup, b);
          Term n0   = term_new_or(a0, term_new(0, TAG_DP0, lab, dup));
          Term n1   = term_new_or(a1, term_new(0, TAG_DP1, lab, dup));
          u64  ns   = heap_alloc(2);
          heap_set(ns + 0, n0);
          heap_set(ns + 1, n1);
          ITRS++;
          multi_emit(RULE_OR_SUP, MULTI_SLIDE, 0, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_WHEN: {
        // WHEN frame: cond reduced to whnf.
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_WHEN_ERA, MULTI_PRUNE, 0, 0, 0);
          continue;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_WHEN_NUM, MULTI_TERM, 0, 0, 0);
          if ((u32)term_val(whnf) == 0) {
            whnf = term_new(0, TAG_ERA, 0, 0);
            continue;
          }
          next = heap_read(loc + 1);
          goto enter;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          u32  lab  = term_ext(whnf);
          u64  sloc = term_val(whnf);
          Term c0   = heap_read(sloc + 0);
          Term c1   = heap_read(sloc + 1);
          Term body = heap_read(loc + 1);
          u64  dup  = heap_alloc(1);
          heap_set(dup, body);
          Term w0   = term_new_when(c0, term_new(0, TAG_DP0, lab, dup));
          Term w1   = term_new_when(c1, term_new(0, TAG_DP1, lab, dup));
          u64  ns   = heap_alloc(2);
          heap_set(ns + 0, w0);
          heap_set(ns + 1, w1);
          ITRS++;
          multi_emit(RULE_WHEN_SUP, MULTI_SLIDE, 0, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_ANN: {
        // ANN frame: typ reduced to whnf.  val is at heap[loc+0].
        u64 loc = term_val(frame);
        Term val = heap_read(loc + 0);
        if (term_tag(whnf) == TAG_LAM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          next = interact_ann_lam(val, whnf);
          goto enter;
        }
        if (term_tag(whnf) == TAG_BRI) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          next = interact_ann_bri(val, whnf);
          goto enter;
        }
        // typ stuck: rebuild ANN with reduced typ.
        heap_set(loc + 1, whnf);
        whnf = frame;
        continue;
      }
      default: {
        continue;
      }
    }
  }

  WNF_LAST_STACK_LEN = 0;
  WNF_S_POS = s_pos;
  return whnf;

bail:
  // Step budget exhausted just before an interaction.  Snapshot the
  // pending frames (innermost-first), then unwind by writing whnf
  // back through each frame's primary slot -- the same logic the
  // apply loop uses for stuck terms.  Heap mutations from already-
  // fired interactions stick; calling wnf again on the returned
  // root resumes from there.
  WNF_LAST_STACK_LEN = s_pos - base;
  for (u32 i = 0; i < WNF_LAST_STACK_LEN; i++) {
    WNF_LAST_STACK[i] = stack[base + WNF_LAST_STACK_LEN - 1 - i];
  }
  while (s_pos > base) {
    Term frame = stack[--s_pos];
    u8 ftag = term_tag(frame);
    if (ftag == TAG_APP || ftag == TAG_DP0 || ftag == TAG_DP1) {
      heap_set(term_val(frame), whnf);
      whnf = frame;
    } else if (ftag == TAG_OP2 || ftag == TAG_EQL || ftag == TAG_AND
               || ftag == TAG_OR  || ftag == TAG_WHEN
               || ftag == TAG_DSU || ftag == TAG_DDU) {
      // Strict-eval frames: store the in-flight WHNF back at slot 0
      // so subsequent wnf re-entry sees the partially-reduced state.
      // DSU/DDU strict slot is the label term at +0; same shape.
      heap_set(term_val(frame) + 0, whnf);
      whnf = frame;
    } else if (ftag == TAG_ANN) {
      // ANN's strict slot is typ at slot 1.
      heap_set(term_val(frame) + 1, whnf);
      whnf = frame;
    } else if (ftag == TAG_F_OP2_NUM) {
      // x is NUM (baked in frame), in-flight whnf is partial y.
      // Rebuild OP2(NUM, whnf) on a fresh cell so the bail snapshot
      // points to a valid term we can resume.
      u32 packed_ext = term_ext(frame);
      u32 op    = packed_ext & 0xFF;
      u32 dtype = (packed_ext >> 8) & 0xFF;
      u32 xv    = (u32)term_val(frame);
      Term x = term_new(0, TAG_NUM, dtype, xv);
      u64 nloc = heap_alloc(2);
      heap_set(nloc + 0, x);
      heap_set(nloc + 1, whnf);
      whnf = term_new(0, TAG_OP2, op, nloc);
    } else if (ftag == TAG_F_EQL_R) {
      // a stored at heap[loc+0]; in-flight whnf is partial b.
      u64 loc = term_val(frame);
      heap_set(loc + 1, whnf);
      whnf = term_new(0, TAG_EQL, 0, loc);
    }
  }
  WNF_S_POS = s_pos;
  return whnf;

#undef BUDGET_HIT
#undef BAIL_AT
}

fn Term wnf(Term term) { return wnf_n(term, 0); }
