// Auto-dup for a LAM whose binder occurs n > 1 times, done on the built heap
// where HVM4 does it on the parse tree (HVM4 clang/parse/auto_dup.c).
//
// n-1 DUPs split the binder: D_1 holds VAR(lam_loc), D_i holds DP1(D_{i-1});
// use i becomes DP0(D_i) and use n becomes DP1(D_{n-1}).
//
// Only the WL bridge (thvm_wl_lam_seal_ext) and tests seal here; the runtime's
// LAM rebuilders (clone_to_book_rec, ann_lam, app_bri) make linear bodies.

#define LAM_AUTODUP_MAX_USES 1024
#define LAM_AUTODUP_BAIL     ((u32)-1)

// Labels 0x10000..0x1FFFF sit above WL's TFreshLabel counter, which counts up
// from 1, and clear of bit 17 (DUP_GRAD_FLAG), which makes grad projections.
#define AUTO_DUP_LABEL_BASE 0x10000u
#define AUTO_DUP_LABEL_TOP  0x1FFFFu
static u32 AUTO_DUP_LABEL_NEXT = AUTO_DUP_LABEL_BASE;

static u32 auto_dup_fresh_label(void) {
  u32 lab = AUTO_DUP_LABEL_NEXT;
  AUTO_DUP_LABEL_NEXT = (AUTO_DUP_LABEL_NEXT >= AUTO_DUP_LABEL_TOP)
                            ? AUTO_DUP_LABEL_BASE
                            : (AUTO_DUP_LABEL_NEXT + 1);
  return lab;
}

// The body's VAR(lam_loc) cells into out_locs, left to right; returns their
// count, or LAM_AUTODUP_BAIL on a UOP_KERNEL or a body past the walk's caps.
static u32 auto_dup_collect(u64 lam_loc, u64 *out_locs, u32 max_count) {
  static u64  visited_bases[LAM_BODY_MAX_VISITS];
  static u64  stack_locs[LAM_BODY_MAX_VISITS];
  static Term stack_terms[LAM_BODY_MAX_VISITS];
  u32 v_count = 0;
  u32 s_pos   = 0;
  u32 found   = 0;

  stack_locs[s_pos]  = lam_loc;
  stack_terms[s_pos] = heap_read(lam_loc);
  s_pos++;

  while (s_pos > 0) {
    s_pos--;
    u64  cur_loc = stack_locs[s_pos];
    Term t       = stack_terms[s_pos];
    u8   tag     = term_tag(t);
    u64  val     = term_val(t);

    if (tag == TAG_VAR && val == lam_loc) {
      if (found >= max_count) return LAM_AUTODUP_BAIL;
      out_locs[found++] = cur_loc;
      continue;
    }
    if (lam_body_leaf(tag)) {
      continue;
    }

    // A REF's or ALO's body is a book template over its own binders, never
    // over the LAM being sealed here.
    if (tag == TAG_REF || tag == TAG_ALO) {
      continue;
    }

    // The walk cannot see kernel inputs (KernelEntry.input_terms).
    if (tag == TAG_UOP && term_ext(t) == UOP_KERNEL) {
      return LAM_AUTODUP_BAIL;
    }

    u32 ar = term_cells(t, HEAP);
    if (ar == 0) continue;

    u8 seen = 0;
    for (u32 i = 0; i < v_count; i++) {
      if (visited_bases[i] == val) { seen = 1; break; }
    }
    if (seen) continue;
    if (v_count >= LAM_BODY_MAX_VISITS) return LAM_AUTODUP_BAIL;
    visited_bases[v_count++] = val;

    // Pushed in reverse so pops run left to right, as HVM4's descent does; any
    // other order hands DP0/DP1 to the wrong uses, and DUP-SUP pairs the wrong
    // SUP arms.
    for (u32 i = ar; i > 0; i--) {
      if (s_pos >= LAM_BODY_MAX_VISITS) return LAM_AUTODUP_BAIL;
      u64 cloc = val + i - 1;
      stack_locs[s_pos]  = cloc;
      stack_terms[s_pos] = heap_read(cloc);
      s_pos++;
    }
  }

  return found;
}

// n >= 2: builds the header's DUP chain and rewrites the n VAR cells.
static void auto_dup_lam_at(u64 lam_loc, u64 *var_locs, u32 n) {
  u64 dup_bodies[LAM_AUTODUP_MAX_USES];
  u32 labels[LAM_AUTODUP_MAX_USES];

  for (u32 i = 0; i < n - 1; i++) {
    dup_bodies[i] = heap_alloc(1);
    labels[i]     = auto_dup_fresh_label();
    if (i == 0) {
      heap_set(dup_bodies[i], term_new(0, TAG_VAR, 0, lam_loc));
    } else {
      heap_set(dup_bodies[i],
               term_new(0, TAG_DP1, labels[i - 1], dup_bodies[i - 1]));
    }
  }

  for (u32 i = 0; i < n; i++) {
    Term repl;
    if (i + 1 < n) {
      repl = term_new(0, TAG_DP0, labels[i], dup_bodies[i]);
    } else {
      repl = term_new(0, TAG_DP1, labels[n - 2], dup_bodies[n - 2]);
    }
    heap_set(var_locs[i], repl);
  }
}

// A compute-UOP body gets no chain: APP-LAM compiles it to a kernel whose input
// slot reads the arg any number of times, and materialize cannot see past DPs.
fn u32 lam_seal_ext_with_auto_dup(u64 lam_loc, u32 base_ext) {
  Term body_t = heap_read(lam_loc);
  if (term_tag(body_t) == TAG_UOP && term_ext(body_t) != UOP_KERNEL) {
    return lam_seal_ext(lam_loc, base_ext);
  }
  u64 var_locs[LAM_AUTODUP_MAX_USES];
  u32 n = auto_dup_collect(lam_loc, var_locs, LAM_AUTODUP_MAX_USES);

  if (n == LAM_AUTODUP_BAIL) {
    return lam_seal_ext(lam_loc, base_ext);
  }
  if (n == 0) {
    return base_ext | LAM_ERA_MASK;
  }
  if (n > 1) {
    auto_dup_lam_at(lam_loc, var_locs, n);
  }
  return base_ext;
}
