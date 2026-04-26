// wnf/redex.c -- redex enumeration + single-redex firing for the
// debugger / step interface.
//
// A "redex" is a Term value whose principal port currently faces an
// interaction partner.  Identification is by the packed Term value
// itself: the val field uniquely picks out the redex's data slot,
// the tag picks out the eliminator role.
//
// The core trio:
//   is_redex(t)       -- predicate; safe to call on any Term.
//   redex_fire(t)     -- dispatches the matching interact_*; returns
//                        the rewrite result (or 0 on validation
//                        failure -- the input wasn't a redex).  Also
//                        rewrites every heap cell still holding the
//                        old redex term to point at the result, so
//                        nested redexes get their parent slots
//                        patched without the caller's help.
//   redex_enumerate   -- scans the live heap for redex Terms,
//                        deduped by packed Term value.  Used both
//                        for TRedexes[t] reporting and for the
//                        pre/post diff that powers TInteract's
//                        "fresh" return.
//
// Cost is O(HEAP_NEXT) per scan -- linear over the live heap.  For
// the inspector workflow this is fine.

fn u8 is_redex(Term t) {
  u8  tag = term_tag(t);
  u64 val = term_val(t);
  switch (tag) {
    case TAG_APP: {
      Term fun = term_resolve(heap_read(val));
      u8 ft = term_tag(fun);
      return ft == TAG_LAM || ft == TAG_ERA || ft == TAG_MAT;
    }
    case TAG_DP0:
    case TAG_DP1: {
      Term cell = heap_read(val);
      // Skip if the dup slot already holds a SUB (a previous fire's
      // residue) -- that's a substitution shortcut, not a fresh
      // active pair.
      if (term_sub_get(cell)) return 0;
      Term body = term_resolve(cell);
      u8 bt = term_tag(body);
      return bt == TAG_SUP || bt == TAG_LAM || bt == TAG_ERA;
    }
    case TAG_REF: return 1;
    case TAG_ALO: return 1;
    case TAG_OP2: {
      Term x = term_resolve(heap_read(val));
      Term y = term_resolve(heap_read(val + 1));
      return term_tag(x) == TAG_NUM && term_tag(y) == TAG_NUM;
    }
    case TAG_UOP: {
      u8 op = term_ext(t);
      // KERNEL always reducible (one fire each, but enumeration
      // doesn't track fired-ness; the dispatcher is idempotent
      // enough for inspection).  GRAD always *eligible* -- the fire
      // is a no-op when y isn't structurally pattern-matchable.
      return op == UOP_KERNEL || op == UOP_GRAD;
    }
    default: return 0;
  }
}

// Patch every heap cell that still holds `old` to hold `new`.  The
// IC interactions logically substitute the redex's incoming wire
// with the result; in heap terms that means any parent slot whose
// content equals the redex term gets replaced by the result.
static void heap_replace(Term old, Term new_) {
  if (old == new_) return;
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    if (heap_read(i) == old) heap_set(i, new_);
  }
}

fn Term redex_fire(Term redex) {
  if (!is_redex(redex)) return 0;
  u8  tag = term_tag(redex);
  u64 val = term_val(redex);
  Term result = redex;

  switch (tag) {
    case TAG_APP: {
      Term fun = term_resolve(heap_read(val));
      Term arg = heap_read(val + 1);
      switch (term_tag(fun)) {
        case TAG_LAM: result = interact_app_lam(fun, arg); break;
        case TAG_ERA: result = interact_app_era();         break;
        case TAG_MAT: {
          u64 mat_loc = term_val(fun);
          u32 match   = term_ext(fun);
          Term arg_w  = wnf(arg);
          ITRS++;
          if (term_tag(arg_w) == TAG_NUM &&
              (u32)term_val(arg_w) == match) {
            result = heap_read(mat_loc);
          } else {
            Term fb = heap_read(mat_loc + 1);
            u64  a2 = heap_alloc(2);
            heap_set(a2 + 0, fb);
            heap_set(a2 + 1, arg_w);
            result = term_new(0, TAG_APP, 0, a2);
          }
          break;
        }
        default: return 0;  // validation race: not a redex anymore
      }
      break;
    }
    case TAG_DP0:
    case TAG_DP1: {
      u64  loc  = val;
      Term cell = heap_take(loc);
      Term body = term_resolve(cell);
      u8 side = (tag == TAG_DP0) ? 0 : 1;
      u32 lab = term_ext(redex);
      switch (term_tag(body)) {
        case TAG_SUP: result = interact_dup_sup(lab, loc, side, body); break;
        case TAG_ERA: result = interact_dup_era(side, loc, body);      break;
        case TAG_LAM: result = interact_dup_lam(lab, loc, side, body); break;
        default:
          heap_set(loc, cell);  // restore -- not a redex anymore
          return 0;
      }
      break;
    }
    case TAG_REF: {
      u32  name = term_ext(redex);
      Term book = (name < DEFS_CAP) ? DEFS[name] : 0;
      if (book == 0) return 0;
      ITRS++;
      result = alo_realize(book, 0);
      break;
    }
    case TAG_ALO: {
      ITRS++;
      result = alo_force(redex);
      break;
    }
    case TAG_OP2: {
      Term x = term_resolve(heap_read(val + 0));
      Term y = term_resolve(heap_read(val + 1));
      if (term_tag(x) != TAG_NUM || term_tag(y) != TAG_NUM) return 0;
      u32 op = term_ext(redex);
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
      result = term_new(0, TAG_NUM, term_ext(x), r);
      break;
    }
    case TAG_UOP: {
      u8 op = term_ext(redex);
      if (op == UOP_KERNEL) {
        result = interact_kernel(redex);   // ITRS++ inside
      } else if (op == UOP_GRAD) {
        Term g = interact_grad(redex);
        if (g == redex) return 0;          // stuck -- treat as non-redex
        ITRS++;
        result = g;
      } else {
        return 0;
      }
      break;
    }
    default: return 0;
  }

  heap_replace(redex, result);
  return result;
}

// Add `t` to `out` if it's a redex we haven't already recorded.
static void redex_collect_one(Term t, Term *out, u32 cap, u32 *count) {
  if (!is_redex(t)) return;
  for (u32 j = 0; j < *count && j < cap; j++) {
    if (out[j] == t) return;
  }
  if (*count < cap) out[*count] = t;
  (*count)++;
}

// How many heap cells does a compound term own?  Atoms and known-
// non-compound tags return 0 (no recursion).  Mirrors the layout
// arities used by alo/realize.c and book/from_dynamic.c, but
// includes UOP variants.
static u32 term_arity(Term t) {
  u8 tag = term_tag(t);
  switch (tag) {
    case TAG_LAM: return 1;
    case TAG_APP: return 2;
    case TAG_SUP: return 2;
    case TAG_DUP: return 1;
    case TAG_OP2: return 2;
    case TAG_MAT: return 2;
    case TAG_ALO: return 2;
    case TAG_DP0: case TAG_DP1: return 1;
    case TAG_UOP: {
      u8 op = term_ext(t);
      if (op == UOP_KERNEL) return 2;
      if (op == UOP_GRAD) {
        // k0b: variable arity 3+n (heap = [y, gy, NUM(n), x_1..x_n]).
        Term n_cell = heap_read(term_val(t) + 2);
        u32  n = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 1;
        return 3 + n;
      }
      return uop_arity(op);
    }
    default: return 0;
  }
}

// DFS walk from each root, collecting redex Terms into `out`.  Also
// scans every live heap cell -- so redexes held inside a duplicated
// shared subgraph are caught even if the caller didn't pass the
// containing root.  Dedup is by packed Term value.
fn u32 redex_enumerate(Term *roots, u32 n_roots, Term *out, u32 cap) {
  u32 count = 0;

  // Worklist DFS for caller-provided roots (which may not live in
  // any heap cell -- e.g. a TTerm the user is holding directly).
  Term stack[256];
  u32  s_pos = 0;
  Term seen[256];
  u32  seen_n = 0;
  for (u32 i = 0; i < n_roots && s_pos < 256; i++) stack[s_pos++] = roots[i];
  while (s_pos > 0) {
    Term t = stack[--s_pos];
    u8 dup = 0;
    for (u32 j = 0; j < seen_n; j++) if (seen[j] == t) { dup = 1; break; }
    if (dup) continue;
    if (seen_n < 256) seen[seen_n++] = t;

    redex_collect_one(t, out, cap, &count);

    u32 ar = term_arity(t);
    if (ar == 0) continue;
    u64 base = term_val(t);
    for (u32 i = 0; i < ar && s_pos < 256; i++) {
      stack[s_pos++] = heap_read(base + i);
    }
  }

  // Plus the global heap scan so distant cells (e.g. shared via DUP,
  // or just other roots not passed in) get covered too.
  for (u64 i = 0; i < HEAP_NEXT; i++) {
    redex_collect_one(heap_read(i), out, cap, &count);
  }
  return count;
}
