// wnf - weak head normal form on the stack machine of HVM4's clang/wnf/_.c.
// enter walks into the head and pushes each eliminator as a frame (APP, DP,
// a strict OP2, EQL, AND, OR, WHEN, ANN, DSU or DDU, an F_* frame for a later
// slot); apply pops them and fires.  Nested wnf / cnf calls share the stack: a
// caller stores s_pos in WNF_S_POS and reloads it; each stops at its own base.

// wnf_n stops once max_steps interactions have fired (0: no bound), before the
// next fire, unfold or strict push: pending frames go to WNF_LAST_STACK
// (innermost first) and unwind as stuck terms, so wnf on the result resumes.
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
      // Acquire: pairs with heap_subst_var's release store (SUB bit and value).
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
      // A grad projection leaves the cell alone: the sibling still reads y.
      if (term_ext(next) & DUP_GRAD_FLAG) {
        if (BUDGET_HIT) BAIL_AT(next);
        if (term_tag(next) == TAG_DP0) {
          // FWD: the cell's y.
          ITRS++;
          multi_emit(RULE_GRAD_FWD, MULTI_TERM, loc, 0, 0);
          next = heap_read(loc);
          goto enter;
        }
        // A UOP y but a KERNEL goes to its rule as it stands, as tinygrad's
        // compute_gradient never evaluates the graph: the descent below would
        // unwrap a DETACH, which stops the gradient (mixin/gradient.py:113-114),
        // and fire a copy ASSIGN under y into its dst, cutting the cotangent to
        // the value it writes (mixin/gradient.py:98-99).
        {
          Term y_raw = term_resolve(heap_read(loc + 0));
          if (term_tag(y_raw) == TAG_UOP && term_ext(y_raw) != UOP_KERNEL) {
            if (BUDGET_HIT) BAIL_AT(next);
            heap_set(loc + 0, y_raw);
            Term g = interact_grad(next);
            if (g == next) { whnf = next; goto apply; }
            ITRS++;
            multi_emit(RULE_GRAD_BWD, MULTI_TERM, loc, 0, 0);
            next = g;
            goto enter;
          }
        }
        // BWD: reduce y (cell[0]) on this stack, not in a nested wnf, so deep
        // DP chains from earlier chain-rule fires need no recursion.
        stack[s_pos++] = next;
        next = heap_read(loc + 0);
        goto enter;
      }
      // A plain DP fires its DUP once the body is WHNF, as in HVM4 (the
      // Levy-opaque projections are BJ0 / BJ1).  A SUB cell is the sibling's
      // finished fire.
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
      // A book-time projection (HVM4's BJ) never fires here: alo_realize turns
      // it into a DP per book copy.
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
      // A REF unfolds one layer per entry (alo_realize wraps the children in
      // ALOs), so self-referential defs stay lazy.
      u32  name = term_ext(next);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0) {
        // An undefined name is a WHNF atom.
        whnf = next;
        goto apply;
      }
      if (BUDGET_HIT) BAIL_AT(next);
      // No ITRS: unfolds are structural (HVM4's REF and ALO rules count none).
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
      if (op == UOP_DETACH) {
        // Forward, DETACH is the identity: unwrap to its child.
        next = heap_read(term_val(next) + 0);
        goto enter;
      }
      if (op == UOP_ASSIGN) {
        // Reduce src (slot 1, perhaps a kernel chain) and dst without writing
        // them back: the cell keeps its src kernel, so a training loop re-fires
        // it each pass (uop_assign.c).
        u64  aloc   = term_val(next);
        if (BUDGET_HIT) BAIL_AT(next);
        // A spent src kernel: an earlier pass wrote dst, so the assign reads as
        // dst, as tinygrad's AFTER once realized, where a re-fire is stale.
        if (kernel_term_spent(heap_read(aloc + 1))) {
          next = heap_read(aloc + 0);
          goto enter;
        }
        WNF_S_POS = s_pos;
        Term src_w  = wnf(heap_read(aloc + 1));
        s_pos = WNF_S_POS;
        WNF_S_POS = s_pos;
        Term dst_w  = wnf(heap_read(aloc + 0));
        s_pos = WNF_S_POS;
        if (term_tag(src_w) == TAG_TEN && term_tag(dst_w) == TAG_TEN) {
          // One write per cell and pass: a cell two roots reach (Adam's m, a
          // step output the param update reads) would apply twice.
          if (assign_fire_claim(aloc))
            whnf = interact_assign_with(dst_w, src_w);
          else
            whnf = dst_w;
          goto apply;
        }
        whnf = next;
        goto apply;
      }
      // Any other UOP is WHNF once no child is active (uop_child_is_active: a
      // DP, a KERNEL or ASSIGN, or a UOP over one); F_UOP_CHILD frames drive
      // each active child in turn.
      u64 uloc = term_val(next);
      u8  uar  = uop_arity((u8)op);
      u8  next_idx = uop_next_active_child(uloc, uar, 0);
      if (next_idx < uar) {
        if (BUDGET_HIT) BAIL_AT(next);
        // Frame ext: the child's index in bits 0-7, the UOP's opcode in 8-15.
        u32 packed = (u32)next_idx | ((u32)op << 8);
        stack[s_pos++] = term_new(0, TAG_F_UOP_CHILD, packed, uloc);
        next = heap_read(uloc + next_idx);
        goto enter;
      }
      whnf = next;
      goto apply;
    }
    case TAG_OP2: {
      // A strict eliminator is its own frame; enter reduces its strict slot.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_EQL: {
      // EQL rules, a first (EQL frame) then b (F_EQL_R): ERA on a side -> ERA,
      // ANY -> NUM(1), a SUP side commutes with a DUP of the other, NUM == NUM
      // -> NUM(1) or NUM(0); anything else is stuck.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_AND: {
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_OR: {
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_WHEN: {
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_ANN: {
      // ANN's strict slot is typ, slot 1.
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 1);
      goto enter;
    }
    case TAG_DSU: {
      if (BUDGET_HIT) BAIL_AT(next);
      u64 loc = term_val(next);
      stack[s_pos++] = next;
      next = heap_read(loc + 0);
      goto enter;
    }
    case TAG_DDU: {
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
            next = interact_app_pri(whnf, arg);
            goto enter;
          }
          case TAG_SUP: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = interact_app_sup(whnf, arg);
            goto enter;
          }
          case TAG_ERA: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            whnf = interact_app_era();
            continue;
          }
          case TAG_MAT: {
            // Force the scrutinee as app_mat.md says: wnf, then cnf for a DP
            // head (a DUP wnf left stuck, which cnf may still fire).
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            WNF_S_POS = s_pos;
            Term arg_w = wnf(arg);
            if (term_tag(arg_w) == TAG_DP0 || term_tag(arg_w) == TAG_DP1) {
              arg_w = cnf(arg_w);
            }
            s_pos = WNF_S_POS;
            next = interact_app_mat(whnf, arg_w);
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
        // DP1 + GRAD: the chain rule on the reduced y.
        if (term_tag(frame) == TAG_DP1 && (term_ext(frame) & DUP_GRAD_FLAG)) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          u64 gloc = term_val(frame);
          heap_set(gloc + 0, whnf);
          Term g = interact_grad(frame);
          if (g == frame) {
            // No rule matches y yet (an unrouted SUP, say): the DP1 is WHNF.
            whnf = frame;
            continue;
          }
          ITRS++;
          multi_emit(RULE_GRAD_BWD, MULTI_TERM, gloc, 0, 0);
          next = g;
          goto enter;
        }
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
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            Term r = interact_dup_uop(lab, loc, side, whnf);
            if (r == 0) {
              // An active UOP: restore the body; the DP is WHNF.
              heap_set(loc, whnf);
              whnf = frame;
              continue;
            }
            next = r;
            goto enter;
          }
          // DUP-NOD commutes the DUP into these compounds, as HVM4's DP frame
          // does; never APP (its "DO NOT ADD"), which would copy a beta redex.
          case TAG_OP2:
          case TAG_MAT:
          case TAG_EQL:
          case TAG_AND:
          case TAG_OR:
          case TAG_WHEN:
          case TAG_ANN:
          case TAG_DSU:
          case TAG_DDU:
          case TAG_INC: {
            if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
            next = interact_dup_nod(lab, loc, side, whnf);
            goto enter;
          }
          default: {
            // No DUP rule for the body (VAR, APP, DP, BJ, ...): the DP is WHNF.
            heap_set(loc, whnf);
            whnf = frame;
            continue;
          }
        }
      }
      case TAG_DSU: {
        // A DP label is one wnf left stuck; cnf may still fire its DUP.
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
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_DDU: {
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
        u64 loc = term_val(frame);
        u32 op  = term_ext(frame);
        // A DP operand gets the cnf drive a DSU label gets.
        if (term_tag(whnf) == TAG_DP0 || term_tag(whnf) == TAG_DP1) {
          WNF_S_POS = s_pos;
          whnf = cnf(whnf);
          s_pos = WNF_S_POS;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          Term y = heap_read(loc + 1);
          if (term_tag(y) == TAG_NUM) {
            whnf = interact_op2_num_num(op, term_ext(whnf), (u32)term_val(whnf),
                                        (u32)term_val(y), loc);
            continue;
          }
          // Stash x in slot 0 for F_OP2_NUM, whose val is `loc`.  A raw store:
          // the stash is no event; a wire_prov bump would hide x's producer.
          u32 dtype = term_ext(whnf);
          u32 packed_ext = (op & 0xFF) | ((dtype & 0xFF) << 8);
          HEAP[loc + 0] = whnf;
          stack[s_pos++] = term_new(0, TAG_F_OP2_NUM, packed_ext, loc);
          next = y;
          goto enter;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          Term y = heap_read(loc + 1);
          whnf = interact_op2_sup(op, whnf, y, loc);
          continue;
        }
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_F_OP2_NUM: {
        // val is the OP2 cell, with x stashed in slot 0 so the trace can use
        // `loc` as the carrier; ext is op | dtype << 8.
        u64 loc        = term_val(frame);
        u32 packed_ext = term_ext(frame);
        u32 op         = packed_ext & 0xFF;
        u32 dtype      = (packed_ext >> 8) & 0xFF;
        u32 xv         = (u32)term_val(heap_read(loc + 0));
        if (term_tag(whnf) == TAG_DP0 || term_tag(whnf) == TAG_DP1) {
          WNF_S_POS = s_pos;
          whnf = cnf(whnf);
          s_pos = WNF_S_POS;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          whnf = interact_op2_num_num(op, dtype, xv, (u32)term_val(whnf), loc);
          continue;
        }
        if (term_tag(whnf) == TAG_SUP) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          Term x = term_new(0, TAG_NUM, dtype, xv);
          whnf = interact_op2_num_sup(op, x, whnf, loc);
          continue;
        }
        // Stuck: x is in slot 0 already, so the OP2 is rebuilt in place.
        heap_set(loc + 1, whnf);
        whnf = term_new(0, TAG_OP2, op, loc);
        continue;
      }
      case TAG_F_UOP_CHILD: {
        u64 uloc      = term_val(frame);
        u32 packed    = term_ext(frame);
        u8  child_idx = (u8)(packed & 0xFF);
        u8  parent_op = (u8)((packed >> 8) & 0xFF);
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
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_ERA, MULTI_PRUNE, loc, 0, 0);
          continue;   // whnf = ERA stays
        }
        if (term_tag(whnf) == TAG_ANY) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_ANY, MULTI_TERM, loc, 0, 0);
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
          multi_emit(RULE_EQL_SUP, MULTI_SLIDE, loc, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
        heap_set(loc + 0, whnf);
        stack[s_pos++] = term_new(0, TAG_F_EQL_R, 0, loc);
        next = heap_read(loc + 1);
        goto enter;
      }
      case TAG_F_EQL_R: {
        u64  loc = term_val(frame);
        Term a   = heap_read(loc + 0);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_ERA, MULTI_PRUNE, loc, 0, 0);
          continue;   // whnf = ERA stays
        }
        if (term_tag(whnf) == TAG_ANY) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_ANY, MULTI_TERM, loc, 0, 0);
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
          multi_emit(RULE_EQL_SUP, MULTI_SLIDE, loc, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        if (term_tag(a) == TAG_NUM && term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_EQL_NUM, MULTI_TERM, loc, 0, 0);
          u32 r = ((u32)term_val(a) == (u32)term_val(whnf)) ? 1 : 0;
          whnf = term_new(0, TAG_NUM, term_ext(a), r);
          continue;
        }
        heap_set(loc + 1, whnf);
        whnf = term_new(0, TAG_EQL, 0, loc);
        continue;
      }
      case TAG_AND: {
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_AND_ERA, MULTI_PRUNE, loc, 0, 0);
          continue;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_AND_NUM, MULTI_TERM, loc, 0, 0);
          if ((u32)term_val(whnf) == 0) {
            continue;     // whnf = NUM(0) stays
          }
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
          multi_emit(RULE_AND_SUP, MULTI_SLIDE, loc, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_OR: {
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_OR_ERA, MULTI_PRUNE, loc, 0, 0);
          continue;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_OR_NUM, MULTI_TERM, loc, 0, 0);
          if ((u32)term_val(whnf) == 0) {
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
          multi_emit(RULE_OR_SUP, MULTI_SLIDE, loc, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_WHEN: {
        u64 loc = term_val(frame);
        if (term_tag(whnf) == TAG_ERA) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_WHEN_ERA, MULTI_PRUNE, loc, 0, 0);
          continue;
        }
        if (term_tag(whnf) == TAG_NUM) {
          if (BUDGET_HIT) { stack[s_pos++] = frame; BAIL_AT(whnf); }
          ITRS++;
          multi_emit(RULE_WHEN_NUM, MULTI_TERM, loc, 0, 0);
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
          multi_emit(RULE_WHEN_SUP, MULTI_SLIDE, loc, 0, lab);
          next = term_new(0, TAG_SUP, lab, ns);
          goto enter;
        }
        heap_set(loc + 0, whnf);
        whnf = frame;
        continue;
      }
      case TAG_ANN: {
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
  // Budget spent: snapshot the frames, then unwind each as a stuck term would.
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
      heap_set(term_val(frame) + 0, whnf);
      whnf = frame;
    } else if (ftag == TAG_ANN) {
      heap_set(term_val(frame) + 1, whnf);
      whnf = frame;
    } else if (ftag == TAG_F_OP2_NUM) {
      // x is stashed in slot 0 and slot 1 still holds y: rebuild the OP2.
      u32 packed_ext = term_ext(frame);
      u32 op         = packed_ext & 0xFF;
      u64 loc        = term_val(frame);
      whnf = term_new(0, TAG_OP2, op, loc);
    } else if (ftag == TAG_F_EQL_R) {
      u64 loc = term_val(frame);
      heap_set(loc + 1, whnf);
      whnf = term_new(0, TAG_EQL, 0, loc);
    } else if (ftag == TAG_F_UOP_CHILD) {
      // The partly reduced child goes back into its parent UOP's cell.
      u32 packed = term_ext(frame);
      u64 uloc   = term_val(frame);
      heap_set(uloc + (packed & 0xFF), whnf);
      whnf = term_new(0, TAG_UOP, (packed >> 8) & 0xFF, uloc);
    }
  }
  WNF_S_POS = s_pos;
  return whnf;

#undef BUDGET_HIT
#undef BAIL_AT
}

fn Term wnf(Term term) { return wnf_n(term, 0); }
