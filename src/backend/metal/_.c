// backend/metal/_.c - Metal backend STUB.
//
// Placeholder while the real Objective-C backend (per docs/metal.md)
// gets built up kernel-by-kernel.  All entries return error sentinels
// so a runtime that gets switched here via `DEV=metal`
// fails LOUDLY at any compute-touching call -- much better than
// silently returning zeros.
//
// init() succeeds (no resources to allocate yet); shutdown() is a
// no-op.  buf_alloc returns 0 (the "no buffer" sentinel reserved by
// the CPU backend convention), so any subsequent buf_read/write/
// dispatch hits the error paths.
//
// Once src/backend/metal/init.m + the per-op .m files arrive, this
// file gets replaced with the .m glue (and CPU_BACKEND remains the
// other choice).

static int  metal_init(void)                                    { return 0; }
static void metal_shutdown(void)                                { /* nop */ }

static u32  metal_buf_alloc(u64 nbytes)                         { (void)nbytes;     return 0; }
static void metal_buf_free (u32 buf_id)                         { (void)buf_id;     /* nop */ }
static void metal_buf_incref(u32 buf_id)                        { (void)buf_id;     /* nop */ }
static void metal_buf_decref(u32 buf_id)                        { (void)buf_id;     /* nop */ }
static int  metal_buf_read (u32 buf_id, void *dst, u64 nbytes)  { (void)buf_id; (void)dst; (void)nbytes; return -1; }
static int  metal_buf_write(u32 buf_id, const void *src, u64 nbytes) { (void)buf_id; (void)src; (void)nbytes; return -1; }
static int  metal_buf_copy (u32 dst_buf_id, u32 src_buf_id, u64 nbytes) { (void)dst_buf_id; (void)src_buf_id; (void)nbytes; return -1; }

static int  metal_dispatch_kernel(struct KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  (void)ke; (void)in_buf_ids; (void)out_buf_id;
  fprintf(stderr, "thvm: metal backend stub -- dispatch_kernel not implemented\n");
  return -1;
}

u32  thvm_metal_buf_pool_begin(void)                         { return 1; }
void thvm_metal_buf_pool_rollback_with_preserve(u32 wm)      { (void)wm; }
void thvm_metal_buf_mark_preserved(u32 buf_id)               { (void)buf_id; }
void thvm_metal_buf_gpu_zero(u32 buf_id)                     { (void)buf_id; }
void thvm_metal_buf_clear_preserved(u32 wm)                  { (void)wm; }
u64  thvm_metal_buf_free_unpreserved_all(void)               { return 0; }
void thvm_metal_precompile_range(u32 kid_lo, u32 kid_hi)     { (void)kid_lo; (void)kid_hi; }
u32  thvm_metal_buf_wrap_external(void *page_base, u64 maplen, u64 minor) { (void)page_base; (void)maplen; (void)minor; return 0; }
u64  thvm_metal_buf_byte_offset(u32 buf_id)                   { (void)buf_id; return 0; }
u32  thvm_metal_buf_arena_alloc(u64 nbytes)                  { (void)nbytes; return 0; }
u32  thvm_metal_buf_arena_view(u32 a, u64 off, u64 nb)       { (void)a; (void)off; (void)nb; return 0; }
void thvm_metal_buf_arena_release(u32 a)                     { (void)a; }

// AOT-on-Metal entry points: real definitions live in backend/metal/_.m
// (Apple only).  The WL bridge (thvmlink.c) calls these unconditionally,
// so non-Metal builds need definitions to link -- a Linux .so tolerates
// the undefined refs but a Windows .dll does not.  These stubs return
// the error sentinel (Term/u64 0, int -1), matching the vtable stubs
// above: no GPU, the Metal AOT path is simply unavailable.
Term thvm_aot_metal_compile_and_run(const char *name, u32 def_id,
    Term *args, u32 n_args, Term *book_heap, u64 book_cells,
    u64 *book_next_inout) {
  (void)name; (void)def_id; (void)args; (void)n_args;
  (void)book_heap; (void)book_cells; (void)book_next_inout;
  return 0;
}
int thvm_aot_metal_op2_fold_batch(Term *book_heap, u64 book_cells,
    u64 *root_locs, u32 n_roots, Term *result_out) {
  (void)book_heap; (void)book_cells; (void)root_locs;
  (void)n_roots; (void)result_out;
  return -1;
}
Term thvm_aot_metal_ic_def_run(Term root, Term *args, u32 n_args,
    Term *book_heap, u64 book_cells, u64 *book_next_inout) {
  (void)root; (void)args; (void)n_args;
  (void)book_heap; (void)book_cells; (void)book_next_inout;
  return 0;
}
u64 thvm_aot_metal_ic_collapse(Term root, u32 depth,
    Term *book_heap, u64 book_cells, u64 *book_next_inout,
    Term *out, u64 out_cap) {
  (void)root; (void)depth; (void)book_heap; (void)book_cells;
  (void)book_next_inout; (void)out; (void)out_cap;
  return 0;
}
int thvm_aot_metal_sp_run(const uint32_t *edges_clause,
    const uint32_t *edges_var, const uint8_t *edges_sign,
    const uint32_t *clause_edges_off, const uint32_t *clause_edges_flat,
    const uint32_t *var_edges_off, const uint32_t *var_edges_flat,
    uint32_t n_edges, uint32_t n_clauses, uint32_t n_vars,
    uint32_t max_iters, float damping, float threshold, float *out_eta) {
  (void)edges_clause; (void)edges_var; (void)edges_sign;
  (void)clause_edges_off; (void)clause_edges_flat;
  (void)var_edges_off; (void)var_edges_flat;
  (void)n_edges; (void)n_clauses; (void)n_vars;
  (void)max_iters; (void)damping; (void)threshold; (void)out_eta;
  return -1;
}
int thvm_aot_metal_sp_solve(const int32_t *cnf_lits,
    const uint32_t *cnf_bounds, uint32_t n_clauses, uint32_t n_vars,
    uint32_t sp_max_iters, float damping, float threshold,
    int8_t *out_assignment) {
  (void)cnf_lits; (void)cnf_bounds; (void)n_clauses; (void)n_vars;
  (void)sp_max_iters; (void)damping; (void)threshold; (void)out_assignment;
  return -1;
}
u64 thvm_aot_metal_cnf_bitmask(const uint32_t *clauses_pos,
    const uint32_t *clauses_neg, uint32_t n_clauses, uint32_t n_vars,
    uint32_t *out, u64 out_cap) {
  (void)clauses_pos; (void)clauses_neg; (void)n_clauses;
  (void)n_vars; (void)out; (void)out_cap;
  return 0;
}

Backend METAL_BACKEND = {
  .id              = 2,
  .view_aware      = 0,   // stub: no Metal view-strided dispatch yet
  .init            = metal_init,
  .shutdown        = metal_shutdown,
  .buf_alloc       = metal_buf_alloc,
  .buf_free        = metal_buf_free,
  .buf_incref      = metal_buf_incref,
  .buf_decref      = metal_buf_decref,
  .buf_read        = metal_buf_read,
  .buf_write       = metal_buf_write,
  .buf_copy        = metal_buf_copy,
  .dispatch_begin  = NULL,
  .dispatch_flush  = NULL,
  .dispatch_end    = NULL,
  .dispatch_kernel = metal_dispatch_kernel,
};
