// aot/_.c -- AOT (ahead-of-time) compiled definition support.
//
// Mirrors HVM4's clang/aot/_.c.  Each registered AOT function is a
// specialised C version of one TDef'd body, taking the current WNF
// stack and consuming the spine APP frames directly via native C
// control flow (no per-arm MAT-MIS dispatch, no per-LAM APP-LAM
// entry into the wnf interpreter).  When the wnf machine encounters
// TAG_REF[name] and AOT_FNS[name] is non-NULL, it dispatches to the
// AOT function instead of REF -> ALO unfolding.
//
// AOT functions return either a continuation Term that wnf re-enters
// (`next = ...; goto enter`) or, for partial applications and unknown
// tag patterns, the result of aot_fallback() which rebuilds the
// original lazy ALO so the interpreter can finish the work.
//
// Function pointers are registered via aot_register(name, fn) -- the
// caller (the emitter or a hand-coded program) is responsible for
// matching the name slot to the corresponding TDef'd body.

typedef Term (*AotFn)(Term *stack, u32 *sp, u32 base);

static AotFn AOT_FNS[DEFS_CAP] = {0};
static u64   AOT_CALLS = 0;

fn void aot_register(u32 name, AotFn aot_fn) {
  if (name >= DEFS_CAP) {
    fprintf(stderr, "aot_register: name id %u out of range\n", name);
    return;
  }
  AOT_FNS[name] = aot_fn;
}

fn AotFn aot_lookup(u32 name) {
  if (name >= DEFS_CAP) return NULL;
  return AOT_FNS[name];
}

fn u64 aot_calls(void) {
  return AOT_CALLS;
}

fn void aot_calls_reset(void) {
  AOT_CALLS = 0;
}

// --- helpers callable from emitted / hand-coded AOT functions ---

// Pop one APP frame from the spine; return its arg, or 0 if none.
// Caller checks the return value -- 0 means "partial application,
// fall back".  The caller is responsible for ITRS++ on a successful
// pop (one APP-LAM / APP-MAT-equivalent step).
static inline Term aot_pop_app_arg(Term *stack, u32 *sp, u32 base) {
  if (*sp <= base) return 0;
  Term frame = stack[*sp - 1];
  if (term_tag(frame) != TAG_APP) return 0;
  (*sp)--;
  return heap_read(term_val(frame) + 1);
}

// Push an APP frame onto the spine so a deeper AOT call can consume
// it.  Used when an emitted body builds nested calls (e.g. add(x, y)
// = APP(APP(REF[add], x), y) -- push y, then call into add with x
// already on top).
static inline void aot_push_app_arg(Term *stack, u32 *sp, u32 base, Term arg) {
  (void)base;
  u64 loc = heap_alloc(2);
  // The fun slot stays uninitialised until the AOT function returns
  // -- our pop walks slot+1 directly so the caller doesn't need a
  // valid fun.  Set to ERA defensively so debug walks see a known
  // tag instead of a stale heap word.
  heap_set(loc + 0, term_new(0, TAG_ERA, 0, 0));
  heap_set(loc + 1, arg);
  stack[(*sp)++] = term_new(0, TAG_APP, 0, loc);
}

// Force a term to head form by re-entering the wnf interpreter.
// Used to drive an AOT function's input args to a CTR/NUM head
// before the case-tree dispatch switches on tag.
static inline Term aot_force(Term t) {
  return wnf(t);
}

// Build a CTR with `n` children (children passed via varargs is
// awkward for emitted code -- callers stage the children in a stack
// array and pass the pointer).  Mirrors term_new_ctr() with a tighter
// signature for the common arity-1 case.
static inline Term aot_wrap_ctr1(u32 label, Term child) {
  Term children[1] = { child };
  return term_new_ctr(label, children, 1);
}

// Build a TAG_NUM atom holding the given u32 value with the given dtype.
static inline Term aot_num_i32(u32 v) {
  return term_new(0, TAG_NUM, DT_INT32, v);
}

// Build APP(fun, arg) on a fresh 2-cell.  Used when the AOT can't
// emit a call_direct (e.g. when the fun is itself a non-REF term).
static inline Term aot_new_app(Term fun, Term arg) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, fun);
  heap_set(loc + 1, arg);
  return term_new(0, TAG_APP, 0, loc);
}

// Allocate a shared dup body cell holding `body`, then return the two
// projection Terms (DP0, DP1 with the given label).  The body cell
// participates in the SUB-resolution mechanism: when the first
// projection fires DUP-* via interact_dup_*, heap_subst_cop writes
// SUB(other_side) into the cell so the second projection resolves
// via VAR-style SUB without a second interaction.
static inline void aot_make_dup(u32 label, Term body, Term *out_dp0, Term *out_dp1) {
  u64 dup_loc = heap_alloc(1);
  heap_set(dup_loc, body);
  *out_dp0 = term_new(0, TAG_DP0, label, dup_loc);
  *out_dp1 = term_new(0, TAG_DP1, label, dup_loc);
}

// Fall back to the interpreter: realize the def's book template via
// ALO and return -- the wnf machine re-enters and processes via the
// existing ALO chain.  Called when the AOT function meets a partial
// application or an unexpected tag pattern it didn't compile cases
// for.  No ITRS bump (REF/ALO are structural in thvm).
fn Term aot_fallback(u32 def_id) {
  Term book = (def_id < DEFS_CAP) ? DEFS[def_id] : 0;
  if (book == 0) {
    return term_new(0, TAG_REF, def_id, 0);
  }
  return alo_realize(book, 0);
}

fn void aot_reset(void) {
  // Clear AOT_FNS so a TReset between contexts doesn't keep stale
  // pointers to dlopen'd code that may have been freed.  The build
  // layer (src/aot/build.c) re-registers on demand.
  for (u32 i = 0; i < DEFS_CAP; i++) AOT_FNS[i] = NULL;
  AOT_CALLS = 0;
}
