// tu_probes.h - test-only accessors over the runtime's internals.
//
// Every test #includes the single-TU runtime, so these read its file-scope
// tables directly; they live here, not in src/, because only tests ask.
// Include after "../src/thvm.c".

#ifndef THVM_TU_PROBES_H
#define THVM_TU_PROBES_H

// --- bufferize_classify: per-node classification -------------------------
static u8 bufferize_is_realized(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) return 0;
  if (term_ext(uop_term) == UOP_KERNEL) return 1;   // already realized
  u32 idx = bufferize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) return 0;
  return BUFFERIZE_NODES[idx].realized;
}

static u32 bufferize_consumer_count(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) return 0;
  u32 idx = bufferize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) return 0;
  return BUFFERIZE_NODES[idx].consumer_count;
}

static u32 bufferize_reasons(Term uop_term) {
  if (term_tag(uop_term) != TAG_UOP) {
    return 0;
  }
  u32 idx = bufferize_info_find(term_val(uop_term));
  if (idx == 0xFFFFFFFFu) {
    return 0;
  }
  return BUFFERIZE_NODES[idx].reasons;
}

// --- rangeify_unified: the last unified pass ------------------------------
static u32 rangeify_unified_last_nodes_walked(void) { return RU_LAST_NODES_WALKED; }
static u32 rangeify_unified_range_idx_counter(void) { return RU_RANGE_IDX_COUNTER; }

static int rangeify_unified_has_ranges_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_RANGE_MAP[node_idx].has_ranges ? 1 : 0;
}

static u32 rangeify_unified_reduce_n_ranges_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_REDUCE_RANGES[node_idx].n;
}

static Term rangeify_unified_reduce_range_at(u32 node_idx, u32 i) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  if (i >= RU_REDUCE_RANGES[node_idx].n) return 0;
  return RU_REDUCE_RANGES[node_idx].ranges[i];
}

static Term rangeify_unified_subst_at(u32 node_idx) {
  if (node_idx >= BUFFERIZE_NODES_LEN) return 0;
  return RU_SUBST[node_idx];
}

static u32 rangeify_unified_last_bufferizes_emitted(void) {
  return RU_LAST_BUFFERIZES_EMITTED;
}

// --- materialize: a view op as its alias TenDesc --------------------------
// For a view-only movement op, the alias TenDesc as a TAG_TEN; anything
// else (PAD, elementwise, reduce, kernel) through thvm_materialize.
static Term materialize_uop_in_env(Term t, u32 env_id) {
  (void)env_id;
  if (term_tag(t) == TAG_UOP) {
    u8 op = term_ext(t);
    if (op_is_view_movement(op)) {
      u32 alias_tid = view_resolve(t);
      if (alias_tid != 0) {
        return term_new(0, TAG_TEN, TENS[alias_tid].dtype, alias_tid);
      }
    }
  }
  return thvm_materialize(t);
}

// --- extern_pin: every pinned term ----------------------------------------
static void extern_pinned_for_each(void (*cb)(Term)) {
  if (cb == NULL) return;
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_LEN; i++) {
    if (EXTERN_PINNED_TERMS[i] != 0) cb(EXTERN_PINNED_TERMS[i]);
  }
}

#endif
