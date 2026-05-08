// uop/apply_opt_dag.c -- DAG-side apply_opt, the Phase E counterpart
// to apply_opt.c's legacy KpSchedule.applied_opts[] mutation.  Each
// helper takes a UOp DAG root + opt parameters and returns a new
// root with the equivalent transformation applied.  Operates purely
// on the UOp DAG -- no KpSchedule, no tile_uops side-channel.
//
// Initial slice covers the two opts that matter for the matmul
// autotune loop: KOP_TC (tile-size selection for the simdgroup_matrix
// template) and KOP_GLOBAL (axis-type swap to drop the
// `if(sgi==0u && tg==0u)` guard in render_uop's TC emission).
// KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP/SWAP land in subsequent
// slices.
//
// Wired into kernel_apply_opt's outer dispatcher: when
// `ke->cached_lift.store_root != 0` we take the DAG path here;
// otherwise we fall through to the legacy schedule path.

// ---------- KOP_TC ---------------------------------------------------
// Wrap (or update) the OPT(_, TC, factor) marker around the inner
// REDUCE that lives in STORE.value.  Three layouts to handle:
//   (a) STORE(buf, addr, REDUCE(...))        -- bare matmul, no OPT.
//                                               Wrap REDUCE with OPT.
//   (b) STORE(buf, addr, OPT(REDUCE, TC, _)) -- already TC-marked.
//                                               Replace factor.
//   (c) anything else                        -- not a matmul we
//                                               recognise; bail.
//
// Returns the new STORE root, or 0 on bail.

fn Term uop_dag_apply_tc(Term root, u32 factor) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  u64 sloc = term_val(root);
  Term buf   = heap_read(sloc + 0);
  Term addr  = heap_read(sloc + 1);
  Term value = heap_read(sloc + 2);

  Term new_value = 0;
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_REDUCE) {
    // Layout (a): bare REDUCE.
    new_value = uop_opt(value, UOP_OPT_TC, factor);
  } else if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
             && uop_opt_kind(value) == UOP_OPT_TC) {
    // Layout (b): already wrapped; rebuild with new factor.
    Term inner = uop_opt_target(value);
    if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_REDUCE) return 0;
    new_value = uop_opt(inner, UOP_OPT_TC, factor);
  } else {
    return 0;
  }

  if (new_value == value) return root;
  return uop_store(buf, addr, new_value);
}

// ---------- KOP_GLOBAL ----------------------------------------------
// Find every UOP_RANGE leaf with `axis_id == target_axis_id` and
// replace its axis_type with `KAX_GLOBAL` (5).  Hash-cons makes the
// new range a fresh term distinct from the old one; uop_graph_rewrite
// then walks the DAG bottom-up, rebuilding parents whose children
// changed (memoised so each parent rebuilds at most once).
//
// Validity gates:
//   - target axis must currently be axis_type == KAX_LOOP (0).  Don't
//     overwrite REDUCE/UPCAST/UNROLL/LOCAL/GROUP/etc. -- those carry
//     semantic meaning and a GLOBAL re-stamp would break the kernel.
//   - The renderer's TC emission with both m & n bound to KAX_GLOBAL
//     drops the sgi==0 guard (see render_uop.c rmu_emit_matmul_tc
//     parallel_tc branch); applying GLOBAL to ONE axis emits the
//     half-bound form (one for-loop survives).
//
// Returns the new STORE root, or `root` unchanged if no RANGE matched.

typedef struct {
  u32 target_axis_id;
} ApplyOptDagGlobalCtx;

static Term apply_opt_dag_global_rewrite(Term t, void *user) {
  ApplyOptDagGlobalCtx const *ctx = (ApplyOptDagGlobalCtx const *)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return t;
  u64 loc = term_val(t);
  u32 axis_id   = (u32)term_val(heap_read(loc + 0));
  u32 axis_type = (u32)term_val(heap_read(loc + 1));
  u32 extent    = (u32)term_val(heap_read(loc + 2));
  if (axis_id != ctx->target_axis_id) return t;
  if (axis_type != KAX_LOOP) return t;
  return uop_range(axis_id, KAX_GLOBAL, extent);
}

fn Term uop_dag_apply_global(Term root, u32 axis_id) {
  ApplyOptDagGlobalCtx ctx = { axis_id };
  UOpGraphRewriteRule rules[] = {
    { "apply-opt-dag-global", apply_opt_dag_global_rewrite },
  };
  return uop_graph_rewrite(root, rules, 1, &ctx);
}

// ---------- top-level dispatcher ------------------------------------

fn Term uop_dag_apply_kopt(Term root, KOpt opt) {
  switch (opt.op) {
    case KOP_TC:
      return uop_dag_apply_tc(root, opt.arg);
    case KOP_GLOBAL:
      return uop_dag_apply_global(root, opt.axis);
    default:
      return 0;  // unsupported in this slice
  }
}
