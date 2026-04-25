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
fn Term wnf(Term term) {
  Term *stack = WNF_STACK;
  u32   s_pos = WNF_S_POS;
  u32   base  = s_pos;
  Term  next  = term;
  Term  whnf;

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
      ITRS++;
      next = alo_realize(book, 0);
      goto enter;
    }
    case TAG_ALO: {
      ITRS++;
      next = alo_force(next);
      goto enter;
    }
    case TAG_UOP: {
      u32 op = term_ext(next);
      if (op == UOP_MATERIALIZE) {
        // Direct rewrite: schedule + kernelize + linearize, no firing.
        // Continue reduction on the scheduled DAG so any nested
        // UOP_KERNEL inside it fires on this same pass.
        next = thvm_materialize(next);
        goto enter;
      }
      if (op == UOP_KERNEL) {
        // Fire all upstream kernels then this one; return the output TAG_TEN.
        whnf = interact_kernel(next);
        goto apply;
      }
      if (op == UOP_GRAD) {
        // Pure rewrite: chain rule produces a fresh UOp graph that
        // may itself contain GRAD nodes (which fire on re-entry).
        next = interact_grad(next);
        goto enter;
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
    case TAG_LAM:
    case TAG_ERA:
    case TAG_SUP:
    case TAG_MAT:
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
            next = interact_app_lam(whnf, arg);
            goto enter;
          }
          case TAG_ERA: {
            whnf = interact_app_era();
            continue;
          }
          case TAG_MAT: {
            // APP-MAT-NUM: force the arg, dispatch on its NUM value.
            // Heap[mat_loc+0] = handler (used on match).
            // Heap[mat_loc+1] = fallback (applied to arg on miss).
            u64  mat_loc = term_val(whnf);
            u32  match   = term_ext(whnf);
            Term arg_w   = wnf(arg);
            ITRS++;
            if (term_tag(arg_w) == TAG_NUM &&
                (u32)term_val(arg_w) == match) {
              next = heap_read(mat_loc + 0);
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
        u8  side = (term_tag(frame) == TAG_DP0) ? 0 : 1;
        u64 loc  = term_val(frame);
        u32 lab  = term_ext(frame);
        switch (term_tag(whnf)) {
          case TAG_SUP: {
            next = interact_dup_sup(lab, loc, side, whnf);
            goto enter;
          }
          case TAG_ERA: {
            whnf = interact_dup_era(side, loc, whnf);
            continue;
          }
          case TAG_LAM: {
            next = interact_dup_lam(lab, loc, side, whnf);
            goto enter;
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

  WNF_S_POS = s_pos;
  return whnf;
}
