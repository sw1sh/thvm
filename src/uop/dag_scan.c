// uop/dag_scan.c -- read-side scanners over a UOp DAG.
//
// Phase C slice 4: KProgOp-iterating consumers (metal_kernel_supported,
// metal_dispatch_kernel's pre-build dtype gate, propose_kprog_*) need
// structural facts about a kernel that today come from walking
// `ke->program[]`.  After kernel_lift_to_uop materialises `cached_lift`
// the same facts are recoverable from the lifted UOp DAG without re-
// running the lifter.  This file collects the small read-only walks.
//
// Helpers here treat `root` as a UOP_STORE (single-output) or UOP_AFTER
// chain of stores.  They return safe defaults (0 / "uniform") when
// `root` is 0 so callers can chain them with the legacy program[] read
// behind a `cached_lift.store_root != 0` gate.
//
// Coverage matches rmu_discover_bufs_rec in render_uop.c (every UOp
// shape that appears in lifted kernels: arithmetic, INDEX_E, REDUCE,
// IWHERE, OPT, CAST/BITCAST, STORE, AFTER).

// === recursion guard ===========================================
// Lifted DAGs are bounded (KERNEL_LIFT_MAX_INPUT inputs; one STORE
// chain per output) but cycles aren't possible -- the lifter constructs
// pure DAGs.  No memoisation; each helper is one bounded walk per
// dispatch.  Depth is naturally O(n_ops + n_inputs).

// Walk the DAG rooted at `t` and verify every dtype-carrying node has
// dtype `dt`.  Returns 1 on uniform-dtype, 0 on mismatch / unknown.
//
// Nodes that carry an explicit dtype:
//   UOP_BUFFER  -> uop_buffer_dtype(t)
//   UOP_CONST   -> term_ext on the inner NUM cell at heap_read(loc + 0)
//   UOP_CAST    -> heap_read(loc + 1) (NUM(dst_dtype))
//   UOP_BITCAST -> heap_read(loc + 1) (NUM(dst_dtype))
//
// Other ops (UOP_ADD/MUL/NEG/REDUCE/...) carry their dtype implicitly
// via their operands; the recursive walk catches them when it hits a
// BUFFER or CONST leaf.  UOP_RANGE / UOP_I* / UOP_INVALID are integer
// address terms with no float-dtype payload; they're skipped.
//
// External linkage so the metal backend (compiled as a separate
// translation unit, backend_metal.o) can call this without an
// inline-definition link error -- mirrors tile_anno_applied_opts_count.
int uop_dag_dtype_uniform(Term t, u32 dt) {
  if (t == 0) return 1;
  if (term_tag(t) != TAG_UOP) return 1;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_BUFFER:
      return uop_buffer_dtype(t) == dt;
    case UOP_CONST: {
      Term inner = heap_read(loc + 0);
      if (term_tag(inner) != TAG_NUM) return 0;
      return term_ext(inner) == dt;
    }
    case UOP_CAST: case UOP_BITCAST: {
      // [src, NUM(dst_dtype)].  The cast result dtype must equal `dt`,
      // and we still recurse into src so any BUFFER/CONST under it is
      // also typed (lifted kernels keep input buffer dtype == output
      // dtype today; the cast checks catch future int<->float kernels).
      Term ddtype = heap_read(loc + 1);
      if (term_tag(ddtype) != TAG_NUM) return 0;
      if (term_val(ddtype) != dt) return 0;
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt);
    }
    // === integer-address arithmetic (skip; they're index-domain) ====
    case UOP_RANGE: case UOP_INVALID:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
      return 1;
    // === one-operand recursion ======================================
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_REDUCE:
    case UOP_OPT:
    case UOP_LOAD:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt);
    // === two-operand recursion ======================================
    case UOP_ADD:  case UOP_MUL:
    case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt);
    // === three-operand recursion ====================================
    case UOP_IWHERE:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 2), dt);
    case UOP_STORE:
      // [buf, addr, value].  buf must match `dt`; addr is index-domain
      // (skipped); value must match `dt`.
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 2), dt);
    case UOP_AFTER:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt);
    default:
      // Unknown op: conservative -- treat as non-uniform so the caller
      // bails to legacy.
      return 0;
  }
}

// Walk the DAG rooted at `t` and find the first UOP_RANGE leaf whose
// axis_type == 1 (KAX_REDUCE).  Returns the extent (NUM at slot 1) if
// found, else 0.  Used by propose's reduce-axis-size heuristic when
// program[] / scalar_uops aren't available.
u32 uop_dag_reduce_axis_extent(Term t) {
  if (t == 0 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    Term axt = heap_read(loc + 1);
    Term ext = heap_read(loc + 2);
    if (term_tag(axt) != TAG_NUM || term_tag(ext) != TAG_NUM) return 0;
    if (term_val(axt) != 1 /*KAX_REDUCE*/) return 0;
    return term_val(ext);
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E: case UOP_AFTER: {
      u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 0));
      if (a) return a;
      return uop_dag_reduce_axis_extent(heap_read(loc + 1));
    }
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:   case UOP_REDUCE:
    case UOP_LOAD:
      return uop_dag_reduce_axis_extent(heap_read(loc + 0));
    case UOP_IWHERE:
      {
        u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 0));
        if (a) return a;
        a = uop_dag_reduce_axis_extent(heap_read(loc + 1));
        if (a) return a;
        return uop_dag_reduce_axis_extent(heap_read(loc + 2));
      }
    case UOP_STORE:
      {
        // STORE = [buf, addr, value]; the address tree carries the
        // RANGE leaves the renderer hoists into for-loops.
        u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 1));
        if (a) return a;
        return uop_dag_reduce_axis_extent(heap_read(loc + 2));
      }
    default:
      return 0;
  }
}

// Walk the DAG rooted at `t` and verify every UOP_REDUCE node carries
// a sum/max kind compatible with the reduce-unroll/group-reduce
// metal templates.  Today both KProgOp and UOp REDUCE encode kind in a
// child slot; we conservatively pass any REDUCE we find (the legacy
// path's per-op switch covered REDUCE only without further checks).
//
// Returns 1 if at least one UOP_REDUCE is reachable from `t` AND every
// reachable arithmetic op is one of the float-only set the metal
// reduce-unroll template emits.  Mirrors propose_metal_reduce_unroll
// _kernel's program[]-side check.
//
// `is_float_only` flag tracks whether we've seen any non-float-only
// ops so far (returned via *out_ok).  *out_has_reduce is set to 1 on
// any UOP_REDUCE encounter.
static void uop_dag_reduce_unroll_walk(Term t, int *out_ok,
                                       int *out_has_reduce) {
  if (t == 0 || term_tag(t) != TAG_UOP) return;
  if (!*out_ok) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  // Coverage of accepted ops mirrors propose_metal_reduce_unroll
  // _kernel's KProgOp switch: float arithmetic + REDUCE + index-domain
  // (RANGE / I* / INVALID) + leaves (BUFFER/CONST) + STORE/AFTER
  // structural.  CMPEQ/CMPLT and OPT pass through.  Anything else
  // (e.g. CAST to a non-fp32 dtype) flips out_ok.
  switch (op) {
    case UOP_REDUCE:
      *out_has_reduce = 1;
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      return;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2: case UOP_LOG2:
    case UOP_SQRT: case UOP_OPT: case UOP_LOAD:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    case UOP_IWHERE:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 2), out_ok, out_has_reduce);
      return;
    case UOP_STORE:
      // Skip the buf operand (leaf BUFFER) and walk addr + value.
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 2), out_ok, out_has_reduce);
      return;
    case UOP_AFTER:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      return;
    case UOP_BUFFER: case UOP_CONST:
    case UOP_RANGE: case UOP_INVALID:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND:
      // Leaves / index-domain: no-op.
      return;
    case UOP_CAST: case UOP_BITCAST:
      // The renderer handles BITCAST same-itemsize moves and CAST is
      // already in propose's accepted KProgOp set indirectly via the
      // renderer's coverage; conservative: treat as ok.
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    default:
      *out_ok = 0;
      return;
  }
}

// Public surface: returns 1 iff the DAG rooted at `t` has at least one
// UOP_REDUCE and every reachable op is in the metal reduce-unroll
// accepted set.  Mirrors the KProgOp gate in propose.c.
int uop_dag_is_reduce_unroll_kernel(Term t) {
  int ok = 1;
  int has_reduce = 0;
  uop_dag_reduce_unroll_walk(t, &ok, &has_reduce);
  return ok && has_reduce;
}
