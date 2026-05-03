// lam/auto_dup.c -- C-side auto-dup for LAM bodies whose binder is
// referenced more than once.  Mirrors HVM4's parse-time
// parse/auto_dup.c (TinyHVM/HVM4/clang/parse/auto_dup.c) but operates
// on the post-construction heap (the body has been installed at
// HEAP[lam_loc] before this fires).
//
// Shape of the rewrite for n VAR(lam_loc) cells in the body:
//
//   D_1.body  = VAR(lam_loc)
//   D_i.body  = DP1(D_{i-1})            for i = 2..n-1
//
//   use_i     = DP0(D_i)                for i = 1..n-1
//   use_n     = DP1(D_{n-1})
//
// Each VAR(lam_loc) cell is rewritten in place to its DP0/DP1
// replacement.  When APP-LAM later fires, heap_subst_var(lam_loc, arg)
// drops `arg` into D_1.body; the chain then projects n independent
// copies via the existing DUP-XXX rules (dup_num, dup_ctr, dup_lam,
// dup_sup, ...).
//
// Each DUP gets a distinct fresh label drawn from the second-highest
// quartile of the 18-bit ext field (0x10000 .. 0x1FFFF) so it (a)
// cannot collide with WL's TFreshLabel (which counts up from 1 and
// stays in the low range in practice) and (b) does NOT set bit 17
// (DUP_GRAD_FLAG, src/thvm.h:70), which would mark the projection as
// a grad-flavored cell and route wnf through the chain-rule path.
//
// Walker is dedup'd by base loc, mirrors lam_body_uses_var.  Bails
// conservatively (returning LAM_AUTODUP_BAIL) on TAG_REF / TAG_ALO /
// UOP_KERNEL or oversize bodies; callers fall back to the plain
// lam_seal_ext path which leaves any non-linear body broken under
// reduction -- the same state thvm was in before this file landed.
//
// Auto-dup fires exclusively from `lam_seal_ext_with_auto_dup`, which
// is what the WL bridge (thvm_wl_lam_seal_ext) calls.  In-runtime LAM
// rebuilders (clone_to_book_rec, ann_lam, app_bri) keep using
// lam_seal_ext directly: their bodies are already auto-dup'd from the
// original construction, so re-running auto-dup would just re-walk a
// linear body.

#define LAM_AUTODUP_MAX_USES 1024
#define LAM_AUTODUP_BAIL     ((u32)-1)

// Fresh label counter for auto-dup chains.  Range chosen to avoid
// (a) collision with WL's TFreshLabel (low values) and (b) bit 17
// which is the DUP_GRAD_FLAG -- using that bit here would route
// every projection through the gradient chain-rule path in wnf.
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

// Walk HEAP[lam_loc] (a LAM's body) and collect heap locations of
// every VAR cell whose val == lam_loc.  Returns the count, or
// LAM_AUTODUP_BAIL if the walker had to give up.
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

    // TAG_REF / TAG_ALO live in book/cloned regions and never
    // reference our local lam_loc.  Safe to skip.
    if (tag == TAG_REF || tag == TAG_ALO) continue;

    // UOP_KERNEL has side-table inputs (KernelEntry.input_terms) we
    // can't see from a heap walk; bail conservatively.
    if (tag == TAG_UOP && term_ext(t) == UOP_KERNEL) {
      return LAM_AUTODUP_BAIL;
    }

    // CTR: arity stored at val, children at val+1..val+n.
    if (tag == TAG_CTR) {
      u32 n = term_ctr_n(t);
      if (n == 0) continue;
      u8 seen = 0;
      for (u32 i = 0; i < v_count; i++) {
        if (visited_bases[i] == val) { seen = 1; break; }
      }
      if (seen) continue;
      if (v_count >= LAM_BODY_MAX_VISITS) return LAM_AUTODUP_BAIL;
      visited_bases[v_count++] = val;
      for (u32 i = 0; i < n; i++) {
        if (s_pos >= LAM_BODY_MAX_VISITS) return LAM_AUTODUP_BAIL;
        u64 cloc = val + 1 + i;
        stack_locs[s_pos]  = cloc;
        stack_terms[s_pos] = heap_read(cloc);
        s_pos++;
      }
      continue;
    }

    // Generic compound: arity from lam_body_arity (defined in
    // lam/body_uses_var.c, included earlier in src/thvm.c).
    u32 ar = lam_body_arity(t);
    if (ar == 0) continue;

    u8 seen = 0;
    for (u32 i = 0; i < v_count; i++) {
      if (visited_bases[i] == val) { seen = 1; break; }
    }
    if (seen) continue;
    if (v_count >= LAM_BODY_MAX_VISITS) return LAM_AUTODUP_BAIL;
    visited_bases[v_count++] = val;

    for (u32 i = 0; i < ar; i++) {
      if (s_pos >= LAM_BODY_MAX_VISITS) return LAM_AUTODUP_BAIL;
      u64 cloc = val + i;
      stack_locs[s_pos]  = cloc;
      stack_terms[s_pos] = heap_read(cloc);
      s_pos++;
    }
  }

  return found;
}

// Build the n-1 DUP chain and rewrite the n VAR cells.  Caller has
// verified n >= 2.
static void auto_dup_lam_at(u64 lam_loc, u64 *var_locs, u32 n) {
  // Allocate dup body cells.  Each is a single-cell heap loc; the
  // initial body is VAR(lam_loc) for D_1, DP1(D_{i-1}) for D_i (i>1).
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

  // Rewrite each VAR(lam_loc) site to its DPx replacement.
  for (u32 i = 0; i < n; i++) {
    Term repl;
    if (i + 1 < n) {
      repl = term_new(0, TAG_DP0, labels[i], dup_bodies[i]);
    } else {
      // Last use takes DP1 of the deepest DUP.
      repl = term_new(0, TAG_DP1, labels[n - 2], dup_bodies[n - 2]);
    }
    heap_set(var_locs[i], repl);
  }
}

// Auto-dup-aware sealing.  Walks the body, sets LAM_ERA_MASK for 0
// uses, leaves a single use untouched, builds a DUP chain for n > 1.
// On walker bail, falls back to plain lam_seal_ext (which keeps the
// ERA_MASK semantics).
fn u32 lam_seal_ext_with_auto_dup(u64 lam_loc, u32 base_ext) {
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
