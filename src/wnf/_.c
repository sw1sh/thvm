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
      Term cell = heap_read(loc);
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
      Term cell = heap_take(loc);
      if (term_sub_get(cell)) {
        next = term_sub_set(cell, 0);
        goto enter;
      }
      stack[s_pos++] = next;
      next = cell;
      goto enter;
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
      // Look up the static template and wrap it in an empty-state ALO.
      // ALO-VAR / ALO-LAM / ALO-NOD then walk one layer per fire as
      // wnf re-enters this term (lazy unfolding -- no eager expansion
      // even for self-referential defs).
      u32  name = term_ext(next);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0) {
        // Undefined ref -- treat as WHNF atom; won't reduce further.
        whnf = next;
        goto apply;
      }
      if (BUDGET_HIT) BAIL_AT(next);
      ITRS++;
      next = alo_realize(book, 0);
      goto enter;
    }
    case TAG_ALO: {
      if (BUDGET_HIT) BAIL_AT(next);
      ITRS++;
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
        Term src_w  = wnf(heap_read(aloc + 1));
        Term dst_w  = wnf(heap_read(aloc + 0));
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
      whnf = next;
      goto apply;
    }
    case TAG_OP2: {
      // Strict on x then y; both must reduce to TAG_NUM for the op
      // to fire.  Inline the inner reductions via a recursive wnf()
      // call (single-threaded; the saved base keeps the stack
      // clean).  If either operand stays non-NUM the OP2 is stuck
      // and we return it as WHNF.
      u64  loc = term_val(next);
      u32  op  = term_ext(next);
      Term x   = wnf(heap_read(loc + 0));
      Term y   = wnf(heap_read(loc + 1));
      if (term_tag(x) == TAG_NUM && term_tag(y) == TAG_NUM) {
        if (BUDGET_HIT) BAIL_AT(next);
        u32 xv = (u32)term_val(x);
        u32 yv = (u32)term_val(y);
        u32 r;
        switch (op) {
          case OP_ADD: r = xv + yv; break;
          case OP_SUB: r = xv - yv; break;
          case OP_MUL: r = xv * yv; break;
          case OP_EQ:  r = (xv == yv) ? 1 : 0; break;
          case OP_LT:  r = (xv <  yv) ? 1 : 0; break;
          default:     r = 0; break;
        }
        ITRS++;
        whnf = term_new(0, TAG_NUM, term_ext(x), r);
        goto apply;
      }
      heap_set(loc + 0, x);
      heap_set(loc + 1, y);
      whnf = next;
      goto apply;
    }
    case TAG_EQL: {
      // Strict on a then b.  Rules:
      //   EQL-ERA-{L,R}: ERA on either side -> ERA  (failed branches
      //                  collapse out)
      //   EQL-SUP-L:     EQL(&L{a0,a1}, b) -> &L{EQL(a0,B0), EQL(a1,B1)}
      //                  with !&L{B0,B1}=b  (DUP duplicates b across
      //                  the two new EQLs)
      //   EQL-SUP-R:     EQL(a, &L{b0,b1}) -> &L{EQL(A0,b0), EQL(A1,b1)}
      //                  with !&L{A0,A1}=a  (DUP duplicates a;
      //                  DUP-NUM annihilates cleanly when a is atomic)
      //   EQL-NUM-NUM:   compare values, NUM(1) if equal else NUM(0)
      //   otherwise:     stuck
      u64  loc = term_val(next);
      Term a   = wnf(heap_read(loc + 0));
      if (term_tag(a) == TAG_ERA) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        whnf = a;
        goto apply;
      }
      if (term_tag(a) == TAG_ANY) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        whnf = term_new(0, TAG_NUM, 0, 1);
        goto apply;
      }
      if (term_tag(a) == TAG_SUP) {
        if (BUDGET_HIT) BAIL_AT(next);
        u32  lab  = term_ext(a);
        u64  sloc = term_val(a);
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
        next = term_new(0, TAG_SUP, lab, ns);
        goto enter;
      }
      Term b = wnf(heap_read(loc + 1));
      if (term_tag(b) == TAG_ERA) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        whnf = b;
        goto apply;
      }
      if (term_tag(b) == TAG_ANY) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        whnf = term_new(0, TAG_NUM, 0, 1);
        goto apply;
      }
      if (term_tag(b) == TAG_SUP) {
        if (BUDGET_HIT) BAIL_AT(next);
        u32  lab  = term_ext(b);
        u64  sloc = term_val(b);
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
        next = term_new(0, TAG_SUP, lab, ns);
        goto enter;
      }
      if (term_tag(a) == TAG_NUM && term_tag(b) == TAG_NUM) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        u32 r = ((u32)term_val(a) == (u32)term_val(b)) ? 1 : 0;
        whnf = term_new(0, TAG_NUM, term_ext(a), r);
        goto apply;
      }
      heap_set(loc + 0, a);
      heap_set(loc + 1, b);
      whnf = next;
      goto apply;
    }
    case TAG_AND: {
      // Short-circuit boolean AND.  Strict on a only:
      //   AND(NUM(0), _)     -> NUM(0)        (b stays unreduced)
      //   AND(NUM(n!=0), b)  -> wnf(b)
      //   AND(ERA, _)        -> ERA
      //   AND(&L{a0,a1}, b)  -> &L{AND(a0,B0), AND(a1,B1)}, !&L{B0,B1}=b
      //   otherwise          -> stuck
      u64  loc = term_val(next);
      Term a   = wnf(heap_read(loc + 0));
      if (term_tag(a) == TAG_ERA) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        whnf = a;
        goto apply;
      }
      if (term_tag(a) == TAG_NUM) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        if ((u32)term_val(a) == 0) {
          whnf = a;
          goto apply;
        }
        next = heap_read(loc + 1);
        goto enter;
      }
      if (term_tag(a) == TAG_SUP) {
        if (BUDGET_HIT) BAIL_AT(next);
        u32  lab  = term_ext(a);
        u64  sloc = term_val(a);
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
        next = term_new(0, TAG_SUP, lab, ns);
        goto enter;
      }
      heap_set(loc + 0, a);
      whnf = next;
      goto apply;
    }
    case TAG_OR: {
      // Short-circuit boolean OR.  Strict on a only:
      //   OR(NUM(0), b)      -> wnf(b)
      //   OR(NUM(n!=0), _)   -> NUM(1)        (b stays unreduced)
      //   OR(ERA, _)         -> ERA
      //   OR(&L{a0,a1}, b)   -> &L{OR(a0,B0), OR(a1,B1)}, !&L{B0,B1}=b
      //   otherwise          -> stuck
      u64  loc = term_val(next);
      Term a   = wnf(heap_read(loc + 0));
      if (term_tag(a) == TAG_ERA) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        whnf = a;
        goto apply;
      }
      if (term_tag(a) == TAG_NUM) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        if ((u32)term_val(a) == 0) {
          next = heap_read(loc + 1);
          goto enter;
        }
        whnf = term_new(0, TAG_NUM, term_ext(a), 1);
        goto apply;
      }
      if (term_tag(a) == TAG_SUP) {
        if (BUDGET_HIT) BAIL_AT(next);
        u32  lab  = term_ext(a);
        u64  sloc = term_val(a);
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
        next = term_new(0, TAG_SUP, lab, ns);
        goto enter;
      }
      heap_set(loc + 0, a);
      whnf = next;
      goto apply;
    }
    case TAG_WHEN: {
      // Boolean filter, strict on cond:
      //   WHEN(NUM(0), _)        -> ERA
      //   WHEN(NUM(n != 0), b)   -> wnf(b)
      //   WHEN(ERA, _)           -> ERA
      //   WHEN(&L{c0,c1}, b)     -> &L{WHEN(c0,B0), WHEN(c1,B1)}, !&L{B0,B1}=b
      //   otherwise              -> stuck
      u64  loc  = term_val(next);
      Term cond = wnf(heap_read(loc + 0));
      if (term_tag(cond) == TAG_ERA) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        whnf = cond;
        goto apply;
      }
      if (term_tag(cond) == TAG_NUM) {
        if (BUDGET_HIT) BAIL_AT(next);
        ITRS++;
        if ((u32)term_val(cond) == 0) {
          whnf = term_new(0, TAG_ERA, 0, 0);
          goto apply;
        }
        next = heap_read(loc + 1);
        goto enter;
      }
      if (term_tag(cond) == TAG_SUP) {
        if (BUDGET_HIT) BAIL_AT(next);
        u32  lab  = term_ext(cond);
        u64  sloc = term_val(cond);
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
        next = term_new(0, TAG_SUP, lab, ns);
        goto enter;
      }
      heap_set(loc + 0, cond);
      whnf = next;
      goto apply;
    }
    case TAG_ANN: {
      // ICC annotation {val : typ}.  Strict on typ; dispatch on its tag:
      //   ANN val (λx.body) -> ann_lam (type-forward-flow)
      //   ANN val (θx.body) -> ann_bri (type erasure)
      //   ANN val var       -> stuck (rebuild with reduced typ)
      u64  loc = term_val(next);
      Term val = heap_read(loc + 0);
      Term typ = wnf(heap_read(loc + 1));
      if (term_tag(typ) == TAG_LAM) {
        if (BUDGET_HIT) BAIL_AT(next);
        next = interact_ann_lam(val, typ);
        goto enter;
      }
      if (term_tag(typ) == TAG_BRI) {
        if (BUDGET_HIT) BAIL_AT(next);
        next = interact_ann_bri(val, typ);
        goto enter;
      }
      heap_set(loc + 1, typ);
      whnf = next;
      goto apply;
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
            Term arg_w   = wnf(arg);
            ITRS++;
            if (term_tag(arg_w) == TAG_NUM &&
                (u32)term_val(arg_w) == match) {
              next = heap_read(mat_loc + 0);
              goto enter;
            }
            if (term_tag(arg_w) == TAG_CTR && term_ext(arg_w) == match) {
              // Destructure: apply handler to each child via fresh APP cells.
              Term handler = heap_read(mat_loc + 0);
              u32 n = term_ctr_n(arg_w);
              Term res = handler;
              for (u32 i = 0; i < n; i++) {
                Term child = term_ctr_at(arg_w, i);
                u64 a = heap_alloc(2);
                heap_set(a + 0, res);
                heap_set(a + 1, child);
                res = term_new(0, TAG_APP, 0, a);
              }
              next = res;
              goto enter;
            }
            // Miss: build APP(fallback, arg_w) and continue.
            Term fallback = heap_read(mat_loc + 1);
            u64  app2     = heap_alloc(2);
            heap_set(app2 + 0, fallback);
            heap_set(app2 + 1, arg_w);
            next = term_new(0, TAG_APP, 0, app2);
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
        // Grad-flavored DP1 frame (pushed in enter when we encountered
        // TAG_DP1 + DUP_GRAD_FLAG): whnf is now the resolved cell[0]
        // (= y in head form).  Re-stash it back so interact_grad
        // sees the resolved value, then dispatch the chain rule.
        // Grad-flag DP0 never gets pushed (FWD passthrough is
        // immediate in enter), so we only handle DP1 here.
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
          next = g;
          goto enter;
        }
        u8  side = (term_tag(frame) == TAG_DP0) ? 0 : 1;
        u64 loc  = term_val(frame);
        u32 lab  = term_ext(frame);
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
            // NUM is atomic: copy the Term value into both projections.
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_dup_num(side, loc, whnf);
            continue;
          }
          case TAG_ANY: {
            // ANY is atomic: copy into both projections.
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_dup_any(side, loc, whnf);
            continue;
          }
          case TAG_TEN: {
            // TEN is atomic (just a tid handle): copy into both.
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_dup_ten(side, loc, whnf);
            continue;
          }
          case TAG_UOP: {
            // DUP-UOP commute: structurally replicate the UOP at both
            // projection sides, pushing DUPs into the compute slots.
            // Returns 0 when the UOP is "active" (KERNEL/GRAD/FWD/
            // ASSIGN) or otherwise unsupported -- in that case stay
            // stuck so the UOP gets a chance to evolve via its own
            // interaction first (chain rule emits SUPs, etc.).
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            Term r = interact_dup_uop(lab, loc, side, whnf);
            if (r == 0) {
              // Stuck: restore body, return DP frame as WHNF.
              heap_set(loc, whnf);
              whnf = frame;
              continue;
            }
            whnf = r;
            continue;
          }
          default: {
            heap_set(loc, whnf);
            whnf = frame;
            continue;
          }
        }
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
    }
  }
  WNF_S_POS = s_pos;
  return whnf;

#undef BUDGET_HIT
#undef BAIL_AT
}

fn Term wnf(Term term) { return wnf_n(term, 0); }
