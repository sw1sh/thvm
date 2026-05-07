// aot/metal_emit.c -- Phase 7 iter D: MSL source emitter.
//
// Generates a Metal compute kernel from a TDef'd body.  The kernel
// signature mirrors the iter A-C tooling:
//
//   kernel void aot_def_<name>(
//       device   Term         *heap      [[buffer(0)]],
//       device   Term         *args      [[buffer(1)]],
//       device   Term         *result    [[buffer(2)]],
//       device   atomic_uint  *book_next [[buffer(3)]],
//       uint                   tid       [[thread_position_in_grid]])
//
// Iter D coverage: defs whose body, after peeling N outer TLam
// binders, is a value expression made of TVar (bound to args), TNum
// literals, and TOp2 folds (recursive).  The body folds inline to a
// single uint; the kernel writes msl_term_new(TAG_NUM, _, uint) into
// result[0].  Out of scope (later iters): TMat, TRef, TCtr, TDup.
//
// Reuses AotEmit + aot_emit_init/str/fmt from aot/emit.c (same TU
// after the #includes in src/thvm.c).

typedef struct {
    u64  lam_loc;
    char arg_var[24];
} AotMslBinding;

#define AOT_MSL_BIND_CAP 8192

typedef struct {
    AotMslBinding entries[AOT_MSL_BIND_CAP];
    u32           n;
} AotMslBindings;

static void aot_msl_bind_init(AotMslBindings *b) { b->n = 0; }

static void aot_msl_bind_push(AotMslBindings *b, u64 loc, const char *name) {
    if (b->n >= AOT_MSL_BIND_CAP) return;
    b->entries[b->n].lam_loc = loc;
    snprintf(b->entries[b->n].arg_var,
             sizeof b->entries[b->n].arg_var, "%s", name);
    b->n++;
}

static const char *aot_msl_bind_lookup(AotMslBindings *b, u64 loc) {
    for (u32 i = b->n; i > 0; i--) {
        if (b->entries[i - 1].lam_loc == loc)
            return b->entries[i - 1].arg_var;
    }
    return NULL;
}

// Per-emit fresh counter.  Reset at the top of thvm_aot_metal_emit.
static u32 g_msl_fresh = 0;

// Iter G: REF-call inlining context.  Set at the top of
// thvm_aot_metal_emit so the TAG_APP case can detect recursion
// (callee_id == self_id -> bail).  emit_failed flips to 1 on any
// unsupported shape inside a recursive emit; the top-level entry
// frees the buffer and returns NULL when set.
//
// Iter W: g_msl_emit_failure_reason carries a brief description so
// the host (compile_and_run in _.m) can print a useful diagnostic
// instead of just "emit failed for def_id N".
static u32  g_msl_self_def_id = 0;
static int  g_msl_emit_failed = 0;
static char g_msl_emit_failure_reason[256] = {0};

// Public accessor so the dual-TU Metal backend can read the latest
// failure reason after thvm_aot_metal_emit returns NULL.
const char *thvm_aot_metal_emit_failure_reason(void) {
    return g_msl_emit_failure_reason;
}

static void aot_msl_emit_fail(const char *fmt, ...) {
    g_msl_emit_failed = 1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_msl_emit_failure_reason,
              sizeof g_msl_emit_failure_reason, fmt, ap);
    va_end(ap);
}

// Iter V: DUP memo -- per-emit map from dup_loc -> emitted uint var
// name.  When a LAM binder is used multiple times, WL's auto_dup
// rewrites the body's TVar(lam_loc) cells into a chain of DP0/DP1
// projections of a shared DUP cell.  In our NUM-folding context we
// don't need real IC duplication semantics -- both projections just
// observe the same value -- so on first DP0/DP1 encounter we emit
// the body's value once and memo the var name for the sibling
// projection to reuse.
typedef struct {
    u64  dup_loc;
    char uint_var[24];
} AotMslDupEntry;
#define AOT_MSL_DUP_CAP 16
static AotMslDupEntry g_msl_dup_memo[AOT_MSL_DUP_CAP];
static u32            g_msl_dup_n = 0;

static const char *aot_msl_dup_lookup(u64 dup_loc) {
    for (u32 i = 0; i < g_msl_dup_n; i++) {
        if (g_msl_dup_memo[i].dup_loc == dup_loc)
            return g_msl_dup_memo[i].uint_var;
    }
    return NULL;
}

static void aot_msl_dup_push(u64 dup_loc, const char *uint_var) {
    if (g_msl_dup_n >= AOT_MSL_DUP_CAP) return;
    g_msl_dup_memo[g_msl_dup_n].dup_loc = dup_loc;
    snprintf(g_msl_dup_memo[g_msl_dup_n].uint_var,
             sizeof g_msl_dup_memo[g_msl_dup_n].uint_var,
             "%s", uint_var);
    g_msl_dup_n++;
}

// Iter Z: parallel Term-memo for DP0/DP1 with SUP/LAM bodies (the
// auto-dup uint memo above is only sound for NUM-typed bodies where
// both projections observe the same scalar).  In the IC-construction
// path used by Church-bool TDefs, both DP0 and DP1 of the same source
// dup_loc must point at the SAME freshly-allocated GPU dup-body loc
// so DUP-SUP annihilation fires at runtime.  Key: source book dup_loc;
// value: name of the `ulong dup_loc_K` MSL local holding the GPU-side
// body cell.  Reset alongside g_msl_dup_n at top of thvm_aot_metal_emit.
typedef struct {
    u64  dup_loc;
    char loc_var[24];
} AotMslDupTermEntry;
#define AOT_MSL_DUP_TERM_CAP 1024
static AotMslDupTermEntry g_msl_dup_term_memo[AOT_MSL_DUP_TERM_CAP];
static u32                g_msl_dup_term_n = 0;

static const char *aot_msl_dup_term_lookup(u64 dup_loc) {
    for (u32 i = 0; i < g_msl_dup_term_n; i++) {
        if (g_msl_dup_term_memo[i].dup_loc == dup_loc)
            return g_msl_dup_term_memo[i].loc_var;
    }
    return NULL;
}

static void aot_msl_dup_term_push(u64 dup_loc, const char *loc_var) {
    if (g_msl_dup_term_n >= AOT_MSL_DUP_TERM_CAP) return;
    g_msl_dup_term_memo[g_msl_dup_term_n].dup_loc = dup_loc;
    snprintf(g_msl_dup_term_memo[g_msl_dup_term_n].loc_var,
             sizeof g_msl_dup_term_memo[g_msl_dup_term_n].loc_var,
             "%s", loc_var);
    g_msl_dup_term_n++;
}

// Forward decls for the term/uint emit pair.
static const char *aot_msl_emit_uint(AotEmit *b, Term t,
                                     AotMslBindings *bind);

// Iter Z: book->dyn migration for AOT-Metal results.
//
// The kernel's IC reducer allocates compound terms in BOOK_HEAP via
// aot_book_alloc.  Host runtime cnf/collapse walk HEAP (the dyn
// heap), so the result Term needs to be relocated before WL can
// consume it via TCnf/TCollapse.  Mirrors the shape of alo_realize
// for the closed-term case: walk the book-heap subtree from the
// root, allocate equivalents in dyn heap, return a dyn-pointing
// Term.  Memo by source book_loc to handle shared subterms (e.g.,
// auto_dup'd dup-body cells where DP0 and DP1 reference the same
// loc).
//
// Iter Z+1's parallel collapse shader reads BOOK_HEAP directly and
// makes this migration unnecessary; for the single-thread bringup
// it's the cleanest way to bridge the two heap arenas.

#define AOT_MIGRATE_VISITED_CAP 4096
typedef struct {
    u64 book_loc;
    u64 dyn_loc;
} AotMigrateMemo;

static u32 aot_migrate_arity(u8 tag) {
    switch (tag) {
        case TAG_LAM: return 1;
        case TAG_APP: return 2;
        case TAG_SUP: return 2;
        case TAG_DUP: return 1;
        case TAG_OP2: return 2;
        case TAG_MAT: return 2;
        case TAG_DP0: case TAG_DP1: case TAG_BJ0: case TAG_BJ1: return 1;
        default: return 0;
    }
}

static Term aot_migrate_book_to_dyn_rec(Term t,
        AotMigrateMemo *memo, u32 *memo_n) {
    u8  tag = term_tag(t);
    u32 ext = term_ext(t);
    u64 val = term_val(t);
    if (term_sub_get(t)) {
        return aot_migrate_book_to_dyn_rec(term_sub_set(t, 0), memo, memo_n);
    }
    if (tag == TAG_NUM || tag == TAG_TEN || tag == TAG_ERA ||
        tag == TAG_REF || tag == TAG_ANY ||
        tag == TAG_FVR || tag == TAG_PRI) return t;
    if (tag == TAG_VAR) {
        // Translate VAR's binder loc through the memo so it points
        // at the migrated LAM's dyn-heap cell.
        for (u32 i = 0; i < *memo_n; i++) {
            if (memo[i].book_loc == val) {
                return term_new(0, TAG_VAR, ext, memo[i].dyn_loc);
            }
        }
        // Unmemoized binder -- the LAM hasn't been visited yet (forward
        // reference, e.g. body before LAM in walk).  Allocate a placeholder
        // LAM cell and memo it; the LAM case will reuse the memoed dyn_loc
        // when it's emitted.  The LAM-case visitor sees an existing memo,
        // skips alloc, and writes its body into the placeholder.
        if (*memo_n >= AOT_MIGRATE_VISITED_CAP) return t;
        u64 dyn_loc = heap_alloc(1);
        memo[*memo_n].book_loc = val;
        memo[*memo_n].dyn_loc  = dyn_loc;
        (*memo_n)++;
        return term_new(0, TAG_VAR, ext, dyn_loc);
    }
    u32 arity = aot_migrate_arity(tag);
    if (arity == 0) return t;
    for (u32 i = 0; i < *memo_n; i++) {
        if (memo[i].book_loc == val) {
            return term_new(0, tag, ext, memo[i].dyn_loc);
        }
    }
    if (*memo_n >= AOT_MIGRATE_VISITED_CAP) return t;
    u64 dyn_loc = heap_alloc(arity);
    memo[*memo_n].book_loc = val;
    memo[*memo_n].dyn_loc  = dyn_loc;
    (*memo_n)++;
    for (u32 i = 0; i < arity; i++) {
        Term child = book_read(val + i);
        heap_set(dyn_loc + i,
                 aot_migrate_book_to_dyn_rec(child, memo, memo_n));
    }
    return term_new(0, tag, ext, dyn_loc);
}

// External linkage so backend/metal/_.m can call this from a
// separate TU.  Iter Z+1 collapses this into the parallel readback
// shader.
Term thvm_aot_migrate_book_to_dyn(Term root) {
    AotMigrateMemo memo[AOT_MIGRATE_VISITED_CAP];
    u32 memo_n = 0;
    return aot_migrate_book_to_dyn_rec(root, memo, &memo_n);
}

// Iter Z: does this body need the IC-construction + GPU-side wnf
// state machine?  Returns 1 if any TAG_SUP appears at any depth, or
// any TAG_LAM appears (i.e., a Church value sitting inside the body
// rather than the outer-peeled binder structure).  The existing
// scalar/MAT/CTR fast paths handle everything else.
//
// Bounded walk: tracks a small visited-base set to avoid pathological
// recursion via shared sub-terms (rare in book templates but cheap to
// guard against).
#define AOT_MSL_NEEDS_IC_VISITED_CAP 8192
static int aot_msl_needs_ic_walk(Term t,
                                  u64 *visited, u32 *visited_n) {
    u8  tag = term_tag(t);
    if (tag == TAG_SUP || tag == TAG_LAM ||
        tag == TAG_BJ0 || tag == TAG_BJ1) return 1;
    u64 val = term_val(t);
    // Dedup by base loc to bound work.
    if (tag == TAG_APP || tag == TAG_DP0 || tag == TAG_DP1 ||
        tag == TAG_CTR || tag == TAG_OP2 || tag == TAG_MAT ||
        tag == TAG_EQL) {
        for (u32 i = 0; i < *visited_n; i++) {
            if (visited[i] == val) return 0;
        }
        if (*visited_n >= AOT_MSL_NEEDS_IC_VISITED_CAP) return 0;
        visited[(*visited_n)++] = val;
    }
    if (tag == TAG_APP) {
        return aot_msl_needs_ic_walk(book_read(val + 0), visited, visited_n) ||
               aot_msl_needs_ic_walk(book_read(val + 1), visited, visited_n);
    }
    if (tag == TAG_DP0 || tag == TAG_DP1) {
        // DP whose body is non-NUM forces the IC path.  NUM body
        // (auto-dup of NUM-binder) is handled by the existing memo.
        Term body = book_read(val);
        if (term_tag(body) == TAG_NUM) return 0;
        return aot_msl_needs_ic_walk(body, visited, visited_n);
    }
    if (tag == TAG_CTR) {
        Term n_cell = book_read(val);
        if (term_tag(n_cell) != TAG_NUM) return 0;
        u32 n = (u32)term_val(n_cell);
        for (u32 i = 0; i < n; i++) {
            if (aot_msl_needs_ic_walk(book_read(val + 1 + i),
                                       visited, visited_n)) return 1;
        }
        return 0;
    }
    if (tag == TAG_OP2 || tag == TAG_EQL) {
        // OP2/EQL with a SUP under either arg needs the IC state
        // machine (commutes).  Plain NUM-NUM still folds inline.
        return aot_msl_needs_ic_walk(book_read(val + 0), visited, visited_n) ||
               aot_msl_needs_ic_walk(book_read(val + 1), visited, visited_n);
    }
    if (tag == TAG_MAT) {
        // MAT cells are case-trees; if the handler or fallback
        // contains a SUP/LAM, the IC path is required.  Note the
        // scrutinee comes via the containing APP, so it's walked
        // separately above.
        return aot_msl_needs_ic_walk(book_read(val + 0), visited, visited_n) ||
               aot_msl_needs_ic_walk(book_read(val + 1), visited, visited_n);
    }
    return 0;
}

static int aot_msl_body_needs_ic(Term body) {
    /* Heap-allocate visited cache so 8K * sizeof(u64) = 64 KiB doesn't
       blow the stack on dispatchers that already have a deep frame. */
    u64 *visited = (u64 *)malloc(AOT_MSL_NEEDS_IC_VISITED_CAP * sizeof(u64));
    if (visited == NULL) return 0;
    u32 visited_n = 0;
    int r = aot_msl_needs_ic_walk(body, visited, &visited_n);
    free(visited);
    return r;
}

// Iter H helper: emit a Term-valued MSL expression for use in
// value-position contexts (e.g., CTR child slots).  Falls through
// to aot_msl_emit_uint + msl_term_new wrap for numeric atoms; for
// TVar bound to args[K], the binding name IS already a Term, so
// just return that name directly without re-wrapping.
static const char *aot_msl_emit_term(AotEmit *b, Term t,
                                     AotMslBindings *bind) {
    static char ring[64][96];
    static u32  ring_idx = 0;
    char *out = ring[(ring_idx++) & 63];

    u8 tag = term_tag(t);
    if (tag == TAG_VAR) {
        u64 loc = term_val(t);
        const char *bound = aot_msl_bind_lookup(bind, loc);
        if (bound) {
            // Binding name is a Term-typed local (args[K] or var_K).
            snprintf(out, 96, "%s", bound);
            return out;
        }
    }

    // Iter Z: IC-construction path for Church-encoded bodies.  Each
    // case allocates the GPU-heap cells, recursively emits children,
    // and returns a Term-typed local.  Mirrors alo_realize's per-tag
    // shape (src/alo/realize.c).
    if (tag == TAG_LAM) {
        u32  lam_ext = term_ext(t);
        u64  src_loc = term_val(t);
        u32  idx     = g_msl_fresh++;
        char loc_var[24], var_var[24], lam_var[24];
        snprintf(loc_var, sizeof loc_var, "lam_loc_%u", idx);
        snprintf(var_var, sizeof var_var, "var_%u",     idx);
        snprintf(lam_var, sizeof lam_var, "lam_%u",     idx);
        // Allocate body cell + introduce VAR(loc_var) binding for the
        // body's TVar(src_loc) references.  The VAR resolves through
        // heap[loc_var]: initially holds the body Term (pre-APP-LAM);
        // after aot_app_lam fires it holds arg | SUB_BIT.
        aot_emit_fmt(b,
            "  ulong %s = aot_book_alloc(book_next, 1u);\n"
            "  Term  %s = msl_term_new(TAG_VAR, 0u, %s);\n",
            loc_var, var_var, loc_var);
        aot_msl_bind_push(bind, src_loc, var_var);
        Term body = book_read(src_loc);
        const char *bt = aot_msl_emit_term(b, body, bind);
        char body_local[96];
        snprintf(body_local, sizeof body_local, "%s", bt);
        aot_emit_fmt(b,
            "  heap[%s] = %s;\n"
            "  Term %s = msl_term_new(TAG_LAM, %uu, %s);\n",
            loc_var, body_local, lam_var, lam_ext, loc_var);
        snprintf(out, 96, "%s", lam_var);
        return out;
    }
    if (tag == TAG_APP) {
        u64  src_loc = term_val(t);
        u32  idx     = g_msl_fresh++;
        char loc_var[24], app_var[24], f_local[96], a_local[96];
        snprintf(loc_var, sizeof loc_var, "app_loc_%u", idx);
        snprintf(app_var, sizeof app_var, "app_%u",     idx);
        // Materialize children before allocation so deeper allocs
        // don't interleave with our cell layout (mirrors CTR pattern).
        Term f = book_read(src_loc + 0);
        Term a = book_read(src_loc + 1);
        const char *fe = aot_msl_emit_term(b, f, bind);
        snprintf(f_local, sizeof f_local, "%s", fe);
        const char *ae = aot_msl_emit_term(b, a, bind);
        snprintf(a_local, sizeof a_local, "%s", ae);
        aot_emit_fmt(b,
            "  ulong %s = aot_book_alloc(book_next, 2u);\n"
            "  heap[%s + 0] = %s;\n"
            "  heap[%s + 1] = %s;\n"
            "  Term %s = msl_term_new(TAG_APP, 0u, %s);\n",
            loc_var, loc_var, f_local, loc_var, a_local, app_var, loc_var);
        snprintf(out, 96, "%s", app_var);
        return out;
    }
    if (tag == TAG_SUP) {
        u32  label   = term_ext(t);
        u64  src_loc = term_val(t);
        u32  idx     = g_msl_fresh++;
        char loc_var[24], sup_var[24], a_local[96], b_local[96];
        snprintf(loc_var, sizeof loc_var, "sup_loc_%u", idx);
        snprintf(sup_var, sizeof sup_var, "sup_%u",     idx);
        Term ca = book_read(src_loc + 0);
        Term cb = book_read(src_loc + 1);
        const char *ae = aot_msl_emit_term(b, ca, bind);
        snprintf(a_local, sizeof a_local, "%s", ae);
        const char *be = aot_msl_emit_term(b, cb, bind);
        snprintf(b_local, sizeof b_local, "%s", be);
        aot_emit_fmt(b,
            "  ulong %s = aot_book_alloc(book_next, 2u);\n"
            "  heap[%s + 0] = %s;\n"
            "  heap[%s + 1] = %s;\n"
            "  Term %s = msl_term_new(TAG_SUP, %uu, %s);\n",
            loc_var, loc_var, a_local, loc_var, b_local,
            sup_var, label, loc_var);
        snprintf(out, 96, "%s", sup_var);
        return out;
    }
    if (tag == TAG_DP0 || tag == TAG_DP1 ||
        tag == TAG_BJ0 || tag == TAG_BJ1) {
        u32  label   = term_ext(t);
        u64  src_loc = term_val(t);
        u8   side    = (tag == TAG_DP0 || tag == TAG_BJ0) ? 0 : 1;
        // Memo: both DP0 and DP1 (or BJ0/BJ1) of the same source dup
        // share one fresh GPU dup-body cell.  Without sharing,
        // DUP-SUP annihilation at runtime mis-fires.
        const char *memo_loc = aot_msl_dup_term_lookup(src_loc);
        char loc_var[24];
        if (memo_loc != NULL) {
            snprintf(loc_var, sizeof loc_var, "%s", memo_loc);
        } else {
            u32 idx = g_msl_fresh++;
            snprintf(loc_var, sizeof loc_var, "dup_loc_%u", idx);
            Term body = book_read(src_loc);
            const char *be = aot_msl_emit_term(b, body, bind);
            char body_local[96];
            snprintf(body_local, sizeof body_local, "%s", be);
            aot_emit_fmt(b,
                "  ulong %s = aot_book_alloc(book_next, 1u);\n"
                "  heap[%s] = %s;\n",
                loc_var, loc_var, body_local);
            aot_msl_dup_term_push(src_loc, loc_var);
        }
        u32  dp_idx = g_msl_fresh++;
        char dp_var[24];
        snprintf(dp_var, sizeof dp_var, "dp_%u", dp_idx);
        // BJ tags become DP at GPU construction time -- we're emitting
        // dyn-equivalent state directly (alo_realize would do the same
        // BJ->DP unfold on a CPU realize).
        aot_emit_fmt(b,
            "  Term %s = msl_term_new(%s, %uu, %s);\n",
            dp_var,
            (side == 0) ? "TAG_DP0" : "TAG_DP1",
            label, loc_var);
        snprintf(out, 96, "%s", dp_var);
        return out;
    }
    if (tag == TAG_REF) {
        // Inline the def's body directly -- equivalent to alo_realize
        // for the closed-term case.  Each TAG_REF use gets its own
        // copy of the LAM tree (no per-call sharing via alo_dup_share,
        // but correct for Church-bool SAT where T_/F_ are tiny).  Bail
        // on recursive REFs (which would loop forever in this inliner).
        u32 ref_id = term_ext(t);
        if (ref_id >= DEFS_CAP || ref_id == g_msl_self_def_id) {
            aot_msl_emit_fail("recursive or out-of-range REF id %u in IC body", ref_id);
            snprintf(out, 96, "0u /* recursive REF */");
            return out;
        }
        Term ref_body = DEFS[ref_id];
        if (ref_body == 0) {
            aot_msl_emit_fail("undefined REF id %u in IC body", ref_id);
            snprintf(out, 96, "0u /* undefined REF */");
            return out;
        }
        // Recurse on the def's body.  The returned MSL local is a Term.
        const char *bt = aot_msl_emit_term(b, ref_body, bind);
        snprintf(out, 96, "%s", bt);
        return out;
    }

    if (tag == TAG_OP2) {
        // Iter Z+ATP: build OP2 cell on the book heap.  The wnf state
        // machine fires NUM-NUM directly and dispatches to op2_sup /
        // op2_num_sup commutes when an arg is SUP.
        u64  src_loc = term_val(t);
        u32  op      = term_ext(t);
        u32  idx     = g_msl_fresh++;
        char loc_var[24], op2_var[24], x_local[96], y_local[96];
        snprintf(loc_var, sizeof loc_var, "op2_loc_%u", idx);
        snprintf(op2_var, sizeof op2_var, "op2_%u",     idx);
        Term x = book_read(src_loc + 0);
        Term y = book_read(src_loc + 1);
        const char *xe = aot_msl_emit_term(b, x, bind);
        snprintf(x_local, sizeof x_local, "%s", xe);
        const char *ye = aot_msl_emit_term(b, y, bind);
        snprintf(y_local, sizeof y_local, "%s", ye);
        aot_emit_fmt(b,
            "  ulong %s = aot_book_alloc(book_next, 2u);\n"
            "  heap[%s + 0] = %s;\n"
            "  heap[%s + 1] = %s;\n"
            "  Term %s = msl_term_new(TAG_OP2, %uu, %s);\n",
            loc_var, loc_var, x_local, loc_var, y_local,
            op2_var, op, loc_var);
        snprintf(out, 96, "%s", op2_var);
        return out;
    }
    if (tag == TAG_MAT) {
        // Iter Z+ATP: build MAT cell on the book heap.  The wnf state
        // machine fires APP-MAT-NUM / APP-MAT-CTR (case dispatch) and
        // app_mat_sup commute when the scrutinee is SUP.
        u64  src_loc = term_val(t);
        u32  match   = term_ext(t);
        u32  idx     = g_msl_fresh++;
        char loc_var[24], mat_var[24], h_local[96], f_local[96];
        snprintf(loc_var, sizeof loc_var, "mat_loc_%u", idx);
        snprintf(mat_var, sizeof mat_var, "mat_%u",     idx);
        Term handler  = book_read(src_loc + 0);
        Term fallback = book_read(src_loc + 1);
        const char *he = aot_msl_emit_term(b, handler, bind);
        snprintf(h_local, sizeof h_local, "%s", he);
        const char *fe = aot_msl_emit_term(b, fallback, bind);
        snprintf(f_local, sizeof f_local, "%s", fe);
        aot_emit_fmt(b,
            "  ulong %s = aot_book_alloc(book_next, 2u);\n"
            "  heap[%s + 0] = %s;\n"
            "  heap[%s + 1] = %s;\n"
            "  Term %s = msl_term_new(TAG_MAT, %uu, %s);\n",
            loc_var, loc_var, h_local, loc_var, f_local,
            mat_var, match, loc_var);
        snprintf(out, 96, "%s", mat_var);
        return out;
    }
    if (tag == TAG_EQL) {
        // Iter ATP: build EQL cell on the book heap.  The wnf state
        // machine fires NUM-NUM directly, ERA/ANY shorts, and
        // eql_sup_l / eql_sup_r commutes when an arg is SUP.
        u64  src_loc = term_val(t);
        u32  idx     = g_msl_fresh++;
        char loc_var[24], eql_var[24], a_local[96], b_local[96];
        snprintf(loc_var, sizeof loc_var, "eql_loc_%u", idx);
        snprintf(eql_var, sizeof eql_var, "eql_%u",     idx);
        Term a = book_read(src_loc + 0);
        Term b_t = book_read(src_loc + 1);
        const char *ae = aot_msl_emit_term(b, a, bind);
        snprintf(a_local, sizeof a_local, "%s", ae);
        const char *be = aot_msl_emit_term(b, b_t, bind);
        snprintf(b_local, sizeof b_local, "%s", be);
        aot_emit_fmt(b,
            "  ulong %s = aot_book_alloc(book_next, 2u);\n"
            "  heap[%s + 0] = %s;\n"
            "  heap[%s + 1] = %s;\n"
            "  Term %s = msl_term_new(TAG_EQL, 0u, %s);\n",
            loc_var, loc_var, a_local, loc_var, b_local,
            eql_var, loc_var);
        snprintf(out, 96, "%s", eql_var);
        return out;
    }
    if (tag == TAG_CTR) {
        // Iter OO: nested CTR -- allocate child cell sequence on the
        // GPU heap, materialize each grandchild via the same emit_term
        // path (recursing into deeper nesting), return a Term-valued
        // local that holds the new TAG_CTR.
        u32  label  = term_ext(t);
        u64  ctr_loc = term_val(t);
        Term n_cell  = book_read(ctr_loc);
        u32  n       = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0;
        if (n > 8) {
            aot_msl_emit_fail("nested CTR arity %u exceeds 8 cap", n);
            snprintf(out, 96, "0u /* nested CTR arity too large */");
            return out;
        }
        // Materialize each grandchild first (their emit may use the
        // same ring slots, so copy each result into a local before
        // emitting the next).
        char child_exprs[8][96];
        for (u32 i = 0; i < n; i++) {
            Term child = book_read(ctr_loc + 1 + i);
            const char *ce = aot_msl_emit_term(b, child, bind);
            snprintf(child_exprs[i], sizeof child_exprs[0], "%s", ce);
        }
        u32 idx = g_msl_fresh++;
        char ctr_var[24], loc_var[24];
        snprintf(loc_var, sizeof loc_var, "ctr_loc_%u", idx);
        snprintf(ctr_var, sizeof ctr_var, "ctr_%u",     idx);
        aot_emit_fmt(b,
            "  ulong %s = aot_book_alloc(book_next, %uu);\n"
            "  heap[%s + 0] = msl_term_new(TAG_NUM, DT_INT32, ulong(%uu));\n",
            loc_var, 1u + n, loc_var, n);
        for (u32 i = 0; i < n; i++) {
            aot_emit_fmt(b,
                "  heap[%s + %uu] = %s;\n",
                loc_var, i + 1, child_exprs[i]);
        }
        aot_emit_fmt(b,
            "  Term %s = msl_term_new(TAG_CTR, %uu, %s);\n",
            ctr_var, label, loc_var);
        snprintf(out, 96, "%s", ctr_var);
        return out;
    }
    // Generic path: emit as uint (NUM payload), wrap as Term.
    // Iter II: preserve TNum's literal dtype on the wrap; for non-TNum
    // (e.g., TOp2 fold), default to 0u (caller's responsibility to
    // know the result dtype, mirroring iter HH).
    u32 dtype_ext = (tag == TAG_NUM) ? term_ext(t) : 0u;
    const char *uv = aot_msl_emit_uint(b, t, bind);
    snprintf(out, 96, "msl_term_new(TAG_NUM, %uu, ulong(%s))",
             dtype_ext, uv);
    return out;
}

// Recursively emit MSL for a value expression, returning a C-string
// MSL expression that evaluates to a `uint` (the folded NUM payload).
// For nested OP2s we materialize each sub-fold into a `uint v_K = ...`
// declaration so the final expression remains compact.
static const char *aot_msl_emit_uint(AotEmit *b, Term t,
                                     AotMslBindings *bind) {
    static char ring[256][80];
    char *out = ring[g_msl_fresh & 0xFF];
    g_msl_fresh++;

    u8 tag = term_tag(t);
    switch (tag) {
      case TAG_NUM: {
        snprintf(out, 80, "%uu", (u32)term_val(t));
        return out;
      }
      case TAG_VAR: {
        u64 loc = term_val(t);
        const char *bound = aot_msl_bind_lookup(bind, loc);
        if (!bound) {
          // Iter LL: signal failure -- silent fallthrough to "0u"
          // produced wrong-but-running kernels.
          aot_msl_emit_fail(
              "TVar(loc=%llu) has no binding -- def shape uses a "
              "free var the emit can't resolve",
              (unsigned long long)loc);
          snprintf(out, 80, "0u /* unbound TVar(loc=%llu) */",
                   (unsigned long long)loc);
          return out;
        }
        snprintf(out, 80, "uint(msl_term_val(%s))", bound);
        return out;
      }
      case TAG_DP0:
      case TAG_DP1:
      case TAG_BJ0:
      case TAG_BJ1: {
        // Iter V: auto-dup of multi-use binders.  Both DP0 and DP1
        // (or BJ0/BJ1, after Phase C's BJ rewrite at clone_to_book)
        // of the same dup_loc reference the SAME body; we emit the
        // body's uint value once and memo the var name so the
        // sibling projection reuses it.  Equivalent to firing
        // DUP-NUM (both projections become NUM(v)) but materialized
        // statically at emit time.
        u64 dup_loc = term_val(t);
        const char *memoed = aot_msl_dup_lookup(dup_loc);
        if (memoed != NULL) {
          snprintf(out, 80, "%s", memoed);
          return out;
        }
        Term body = book_read(dup_loc);
        const char *bv = aot_msl_emit_uint(b, body, bind);
        u32 idx = g_msl_fresh++;
        char dup_name[24];
        snprintf(dup_name, sizeof dup_name, "dup_%u", idx);
        aot_emit_fmt(b, "  uint %s = %s;\n", dup_name, bv);
        aot_msl_dup_push(dup_loc, dup_name);
        snprintf(out, 80, "%s", dup_name);
        return out;
      }
      case TAG_OP2: {
        u32 op = term_ext(t);
        u64 loc = term_val(t);
        Term x = book_read(loc + 0);
        Term y = book_read(loc + 1);
        // Iter XX: const-fold OP2(NUM, NUM) at emit time -- skip the
        // intermediate v_K decl entirely.
        if (term_tag(x) == TAG_NUM && term_tag(y) == TAG_NUM) {
          u32 xv32 = (u32)term_val(x);
          u32 yv32 = (u32)term_val(y);
          u32 r;
          switch (op) {
            case OP_ADD: r = xv32 + yv32; break;
            case OP_SUB: r = xv32 - yv32; break;
            case OP_MUL: r = xv32 * yv32; break;
            case OP_EQ:  r = (xv32 == yv32) ? 1u : 0u; break;
            case OP_LT:  r = (xv32 <  yv32) ? 1u : 0u; break;
            default:     r = 0u; break;
          }
          snprintf(out, 80, "%uu", r);
          return out;
        }
        const char *xv = aot_msl_emit_uint(b, x, bind);
        const char *yv = aot_msl_emit_uint(b, y, bind);
        u32 idx = g_msl_fresh++;
        snprintf(out, 80, "v_%u", idx);
        const char *opc;
        int   cmp = 0;
        switch (op) {
          case OP_ADD: opc = "+";  break;
          case OP_SUB: opc = "-";  break;
          case OP_MUL: opc = "*";  break;
          case OP_EQ:  opc = "=="; cmp = 1; break;
          case OP_LT:  opc = "<";  cmp = 1; break;
          default:     opc = "+";  break;  /* unsupported op */
        }
        if (cmp) {
          aot_emit_fmt(b, "  uint %s = (%s %s %s) ? 1u : 0u;\n",
                       out, xv, opc, yv);
        } else {
          aot_emit_fmt(b, "  uint %s = (%s) %s (%s);\n",
                       out, xv, opc, yv);
        }
        return out;
      }
      case TAG_APP: {
        // Iter G: cross-def static call.  Walk APP spine inward to
        // collect args + find the function head.  Shape:
        //   App(App(...App(REF, a_n-1), ...), a_1), a_0)
        // (innermost APP is the outermost call: it carries arg 0).
        Term call_args[8];
        u32  n_call_args = 0;
        Term cursor = t;
        while (term_tag(cursor) == TAG_APP && n_call_args < 8) {
          u64 al = term_val(cursor);
          call_args[n_call_args++] = book_read(al + 1);  // arg
          cursor = book_read(al + 0);                    // function
        }
        // Reverse so call_args[0] is the first call arg.
        for (u32 i = 0; i < n_call_args / 2; i++) {
          Term tmp = call_args[i];
          call_args[i] = call_args[n_call_args - 1 - i];
          call_args[n_call_args - 1 - i] = tmp;
        }
        if (term_tag(cursor) != TAG_REF) {
          aot_msl_emit_fail("APP head is tag=%u (expected TAG_REF)",
                            term_tag(cursor));
          snprintf(out, 80, "0u /* APP head not REF (tag=%u) */",
                   term_tag(cursor));
          return out;
        }
        u32 callee_id = term_ext(cursor);
        if (callee_id == g_msl_self_def_id) {
          aot_msl_emit_fail("recursive REF (def_id %u calls itself)"
                            " -- Metal emit doesn't support recursion",
                            callee_id);
          snprintf(out, 80, "0u /* recursive REF -- not supported */");
          return out;
        }
        if (callee_id >= DEFS_CAP || DEFS[callee_id] == 0) {
          aot_msl_emit_fail("REF id %u out of range or unset", callee_id);
          snprintf(out, 80, "0u /* REF id %u out of range */", callee_id);
          return out;
        }
        // Materialize each call arg as a Term, bind callee LAM[K] -> term name.
        u32 saved_n = bind->n;
        Term callee_cursor = DEFS[callee_id];
        u32 callee_arity = 0;
        char term_names[8][24];
        while (term_tag(callee_cursor) == TAG_LAM
               && callee_arity < n_call_args) {
          const char *uv = aot_msl_emit_uint(b, call_args[callee_arity], bind);
          u32 idx = g_msl_fresh++;
          snprintf(term_names[callee_arity], sizeof term_names[0],
                   "vt_%u", idx);
          aot_emit_fmt(b,
            "  Term %s = msl_term_new(TAG_NUM, 0, ulong(%s));\n",
            term_names[callee_arity], uv);
          u64 lam_loc = term_val(callee_cursor);
          aot_msl_bind_push(bind, lam_loc, term_names[callee_arity]);
          callee_cursor = book_read(lam_loc);
          callee_arity++;
        }
        if (callee_arity != n_call_args) {
          aot_msl_emit_fail(
              "REF call arity mismatch: %u call args vs %u callee LAMs"
              " (callee def_id %u)",
              n_call_args, callee_arity, callee_id);
          bind->n = saved_n;
          snprintf(out, 80,
                   "0u /* arity mismatch: %u call args vs %u callee LAMs */",
                   n_call_args, callee_arity);
          return out;
        }
        // Recursively emit the callee body with bindings in place.
        // The result is a uint expression we can pass back to the
        // caller's evaluator (TOp2, MAT, etc.) directly.
        const char *result_expr = aot_msl_emit_uint(b, callee_cursor, bind);
        bind->n = saved_n;
        snprintf(out, 80, "%s", result_expr);
        return out;
      }
      default: {
        // Iter UU: any tag we don't have a value-fold case for
        // (TPri / TSup / TUOp / TAlo / TBri / TInc / ...) signals
        // an emit failure with the offending tag.  Silent 0u
        // returns produced wrong-but-running kernels.
        aot_msl_emit_fail(
            "unsupported tag %u in value position -- def shape "
            "isn't yet supported by the Metal emit",
            tag);
        snprintf(out, 80, "0u /* unsupported tag %u */", tag);
        return out;
      }
    }
}

// Public entry: generate MSL source for the named def.  Returns
// malloc'd buffer (caller frees).  NULL on bad def_id or missing root.
// External linkage so the dual-TU Metal backend (_.m) can call it.
char *thvm_aot_metal_emit(u32 def_id, const char *name) {
    if (def_id >= DEFS_CAP) return NULL;
    Term root = DEFS[def_id];
    if (root == 0) return NULL;

    AotEmit b;
    aot_emit_init(&b);
    if (!b.buf) return NULL;

    AotMslBindings bind;
    aot_msl_bind_init(&bind);

    // Peel outer LAMs and bind each to args[K].
    Term cursor = root;
    u32  argc = 0;
    while (term_tag(cursor) == TAG_LAM) {
        u64  loc = term_val(cursor);
        char arg_var[24];
        snprintf(arg_var, sizeof arg_var, "args[%u]", argc);
        aot_msl_bind_push(&bind, loc, arg_var);
        cursor = book_read(loc);
        argc++;
    }

    // Preamble split into multiple aot_emit_fmt calls so we stay
    // under the shared 1024-byte tmp buffer.  Order: header +
    // pragma + tag/ext defs (small), then helpers (medium), then
    // kernel signature (medium).
    aot_emit_fmt(&b,
        "// auto-generated by thvm_aot_metal_emit(\"%s\") def_id=%u arity=%u\n"
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "// Helpers below are emitted unconditionally; some defs use\n"
        "// only a subset (e.g., pure-NUM bodies skip aot_book_alloc).\n"
        "// Suppress -Wunused-function in xcrun metal output.\n"
        "#pragma clang diagnostic ignored \"-Wunused-function\"\n"
        "\n"
        "#define TAG_SHIFT  56\n"
        "#define EXT_SHIFT  38\n"
        "#define TAG_MASK   0x7FUL\n"
        "#define EXT_MASK   0x3FFFFUL\n"
        "#define VAL_MASK   0x3FFFFFFFFFUL\n"
        "#define TAG_APP    0u\n"
        "#define TAG_LAM    1u\n"
        "#define TAG_VAR    2u\n"
        "#define TAG_ERA    3u\n"
        "#define TAG_DP0    4u\n"
        "#define TAG_DP1    5u\n"
        "#define TAG_SUP    6u\n"
        "#define TAG_NUM    10u\n"
        "#define TAG_REF    11u\n"
        "#define TAG_OP2    13u\n"
        "#define TAG_MAT    14u\n"
        "#define TAG_CTR    20u\n"
        "#define TAG_BJ0    33u\n"
        "#define TAG_BJ1    34u\n"
        "#define TAG_F_OP2_NUM 26u\n"
        "#define TAG_EQL    15u\n"
        "#define TAG_F_EQL_R 27u\n"
        "#define DT_INT32   5u\n"
        "// OP_* opcodes mirror src/thvm.h.\n"
        "#define OP_ADD 0u\n"
        "#define OP_SUB 1u\n"
        "#define OP_MUL 2u\n"
        "#define OP_EQ  3u\n"
        "#define OP_LT  4u\n"
        "#define OP_DIV 5u\n"
        "#define OP_MOD 6u\n"
        "#define OP_XOR 7u\n"
        "#define OP_AND 8u\n"
        "#define OP_OR  9u\n"
        "#define OP_SHL 10u\n"
        "#define OP_SHR 11u\n"
        "#define OP_GT  12u\n"
        "#define OP_LE  13u\n"
        "#define OP_GE  14u\n"
        "#define OP_NE  15u\n"
        "#define SUB_SHIFT  63\n"
        "#define SUB_BIT    (1ULL << 63)\n"
        "#define LAM_ERA_MASK   (1u << 17)\n"
        "#define DUP_LABEL_MASK 0x1FFFFu\n"
        "\n",
        name, def_id, argc);
    aot_emit_str(&b,
        "typedef ulong Term;\n"
        "static inline ulong msl_term_val(Term t) { return t & VAL_MASK; }\n"
        "static inline uint  msl_term_ext(Term t) {\n"
        "  return uint((t >> EXT_SHIFT) & EXT_MASK);\n"
        "}\n"
        "static inline uint  msl_term_tag(Term t) {\n"
        "  return uint((t >> TAG_SHIFT) & TAG_MASK);\n"
        "}\n"
        "static inline uint  msl_sub_get(Term t) {\n"
        "  return uint((t >> SUB_SHIFT) & 1u);\n"
        "}\n"
        "static inline Term  msl_sub_clr(Term t) { return t & ~SUB_BIT; }\n"
        "static inline Term  msl_term_new(uint tag, uint ext, ulong val) {\n"
        "  return ((ulong(tag) & TAG_MASK) << TAG_SHIFT)\n"
        "       | ((ulong(ext) & EXT_MASK) << EXT_SHIFT)\n"
        "       | ( val        & VAL_MASK);\n"
        "}\n"
        "static inline ulong aot_book_alloc(\n"
        "    device atomic_uint *book_next, uint n) {\n"
        "  return ulong(atomic_fetch_add_explicit(book_next, n,\n"
        "                                          memory_order_relaxed));\n"
        "}\n"
        "// Single-thread grid: plain stores OK; SUB-bit substitution\n"
        "// just writes the marked Term.  Mirrors src/heap/subst_var.c\n"
        "// + src/heap/subst_cop.c on the CPU side.\n"
        "static inline void aot_subst_var(device Term *heap, ulong loc, Term v) {\n"
        "  heap[loc] = v | SUB_BIT;\n"
        "}\n"
        "static inline Term aot_subst_cop(uint side, ulong loc,\n"
        "    Term r0, Term r1, device Term *heap) {\n"
        "  if (side == 0u) { aot_subst_var(heap, loc, r1); return r0; }\n"
        "  aot_subst_var(heap, loc, r0); return r1;\n"
        "}\n"
        "\n"
        "// === IC interaction inlines (mirror src/interact/*.c) ===\n"
        "// Single-thread grid; no atomics required for correctness.\n"
        "\n"
        "// app_lam (src/interact/app_lam.c): LAM_ERA_MASK fast path\n"
        "// skips the binder substitution.  No JIT shape annotation.\n"
        "static Term aot_app_lam(Term lam, Term arg, device Term *heap) {\n"
        "  uint  lam_ext = msl_term_ext(lam);\n"
        "  ulong loc     = msl_term_val(lam);\n"
        "  Term  body    = heap[loc];\n"
        "  if ((lam_ext & LAM_ERA_MASK) == 0u) aot_subst_var(heap, loc, arg);\n"
        "  return body;\n"
        "}\n"
        "\n"
        "// app_sup (src/interact/app_sup.c): 7-cell allocation.\n"
        "static Term aot_app_sup(Term sup, Term arg,\n"
        "    device Term *heap, device atomic_uint *book_next) {\n"
        "  ulong sup_loc = msl_term_val(sup);\n"
        "  uint  lab     = msl_term_ext(sup);\n"
        "  Term f = heap[sup_loc + 0];\n"
        "  Term g = heap[sup_loc + 1];\n"
        "  ulong c = aot_book_alloc(book_next, 7u);\n"
        "  heap[c + 0] = arg;\n"
        "  heap[c + 1] = f;\n"
        "  heap[c + 2] = msl_term_new(TAG_DP0, lab, c + 0);\n"
        "  heap[c + 3] = g;\n"
        "  heap[c + 4] = msl_term_new(TAG_DP1, lab, c + 0);\n"
        "  heap[c + 5] = msl_term_new(TAG_APP, 0u, c + 1);\n"
        "  heap[c + 6] = msl_term_new(TAG_APP, 0u, c + 3);\n"
        "  return msl_term_new(TAG_SUP, lab, c + 5);\n"
        "}\n"
        "\n"
        "// dup_sup (src/interact/dup_sup.c): annihilate vs commute.\n"
        "static Term aot_dup_sup(uint lab, ulong loc, uint side, Term sup,\n"
        "    device Term *heap, device atomic_uint *book_next) {\n"
        "  ulong sup_loc = msl_term_val(sup);\n"
        "  uint  sup_lab = msl_term_ext(sup);\n"
        "  if (lab == sup_lab) {\n"
        "    Term tm0 = heap[sup_loc + 0];\n"
        "    Term tm1 = heap[sup_loc + 1];\n"
        "    return aot_subst_cop(side, loc, tm0, tm1, heap);\n"
        "  }\n"
        "  Term a = heap[sup_loc + 0];\n"
        "  Term b = heap[sup_loc + 1];\n"
        "  ulong c = aot_book_alloc(book_next, 6u);\n"
        "  heap[c + 0] = a;\n"
        "  heap[c + 1] = b;\n"
        "  heap[c + 2] = msl_term_new(TAG_DP0, lab, c + 0);\n"
        "  heap[c + 3] = msl_term_new(TAG_DP0, lab, c + 1);\n"
        "  heap[c + 4] = msl_term_new(TAG_DP1, lab, c + 0);\n"
        "  heap[c + 5] = msl_term_new(TAG_DP1, lab, c + 1);\n"
        "  Term x0 = msl_term_new(TAG_SUP, sup_lab, c + 2);\n"
        "  Term x1 = msl_term_new(TAG_SUP, sup_lab, c + 4);\n"
        "  return aot_subst_cop(side, loc, x0, x1, heap);\n"
        "}\n"
        "\n"
        "// dup_lam (src/interact/dup_lam.c): 5-cell allocation;\n"
        "// LAM_ERA_MASK fast path skips the binder subst_var.\n"
        "static Term aot_dup_lam(uint lab, ulong loc, uint side, Term lam,\n"
        "    device Term *heap, device atomic_uint *book_next) {\n"
        "  uint  lam_ext = msl_term_ext(lam);\n"
        "  ulong lam_loc = msl_term_val(lam);\n"
        "  Term  body    = heap[lam_loc];\n"
        "  ulong a = aot_book_alloc(book_next, 5u);\n"
        "  heap[a + 4] = body;\n"
        "  heap[a + 0] = msl_term_new(TAG_DP0, lab, a + 4);\n"
        "  heap[a + 1] = msl_term_new(TAG_DP1, lab, a + 4);\n"
        "  heap[a + 2] = msl_term_new(TAG_VAR, 0u, a + 0);\n"
        "  heap[a + 3] = msl_term_new(TAG_VAR, 0u, a + 1);\n"
        "  Term sup = msl_term_new(TAG_SUP, lab,     a + 2);\n"
        "  Term l0  = msl_term_new(TAG_LAM, lam_ext, a + 0);\n"
        "  Term l1  = msl_term_new(TAG_LAM, lam_ext, a + 1);\n"
        "  if ((lam_ext & LAM_ERA_MASK) == 0u) aot_subst_var(heap, lam_loc, sup);\n"
        "  return aot_subst_cop(side, loc, l0, l1, heap);\n"
        "}\n"
        "\n"
        "// dup_num / dup_era: zero-alloc; both projections see same atom.\n"
        "static Term aot_dup_num(uint side, ulong loc, Term num, device Term *heap) {\n"
        "  return aot_subst_cop(side, loc, num, num, heap);\n"
        "}\n"
        "static Term aot_dup_era(uint side, ulong loc, Term era, device Term *heap) {\n"
        "  return aot_subst_cop(side, loc, era, era, heap);\n"
        "}\n"
        "\n"
        "// op2_fire: NUM-NUM op application (mirrors wnf/_.c's TAG_F_OP2_NUM).\n"
        "static uint aot_op2_fire(uint op, uint xv, uint yv) {\n"
        "  switch (op) {\n"
        "    case OP_ADD: return xv + yv;\n"
        "    case OP_SUB: return xv - yv;\n"
        "    case OP_MUL: return xv * yv;\n"
        "    case OP_EQ:  return (xv == yv) ? 1u : 0u;\n"
        "    case OP_LT:  return (xv <  yv) ? 1u : 0u;\n"
        "    case OP_DIV: return (yv == 0u) ? 0u : xv / yv;\n"
        "    case OP_MOD: return (yv == 0u) ? 0u : xv % yv;\n"
        "    case OP_XOR: return xv ^ yv;\n"
        "    case OP_AND: return xv & yv;\n"
        "    case OP_OR:  return xv | yv;\n"
        "    case OP_SHL: return xv << (yv & 31u);\n"
        "    case OP_SHR: return xv >> (yv & 31u);\n"
        "    case OP_GT:  return (xv >  yv) ? 1u : 0u;\n"
        "    case OP_LE:  return (xv <= yv) ? 1u : 0u;\n"
        "    case OP_GE:  return (xv >= yv) ? 1u : 0u;\n"
        "    case OP_NE:  return (xv != yv) ? 1u : 0u;\n"
        "  }\n"
        "  return 0u;\n"
        "}\n"
        "\n"
        "// op2_sup (src/interact/op2_sup.c): SUP on left arg of OP2.\n"
        "// 7-cell allocation (DUP body for y + 2 OP2 layouts + SUP).\n"
        "static Term aot_op2_sup(uint op, Term sup, Term y,\n"
        "    device Term *heap, device atomic_uint *book_next) {\n"
        "  ulong sup_loc = msl_term_val(sup);\n"
        "  uint  lab     = msl_term_ext(sup);\n"
        "  Term a = heap[sup_loc + 0];\n"
        "  Term b = heap[sup_loc + 1];\n"
        "  ulong c = aot_book_alloc(book_next, 7u);\n"
        "  heap[c + 0] = y;\n"
        "  heap[c + 1] = a;\n"
        "  heap[c + 2] = msl_term_new(TAG_DP0, lab, c + 0);\n"
        "  heap[c + 3] = b;\n"
        "  heap[c + 4] = msl_term_new(TAG_DP1, lab, c + 0);\n"
        "  heap[c + 5] = msl_term_new(TAG_OP2, op, c + 1);\n"
        "  heap[c + 6] = msl_term_new(TAG_OP2, op, c + 3);\n"
        "  return msl_term_new(TAG_SUP, lab, c + 5);\n"
        "}\n"
        "\n"
        "// op2_num_sup (src/interact/op2_num_sup.c): NUM left, SUP right.\n"
        "// Atomic NUM is reused in both branches -- no DUP, 6 cells.\n"
        "static Term aot_op2_num_sup(uint op, Term num, Term sup,\n"
        "    device Term *heap, device atomic_uint *book_next) {\n"
        "  ulong sup_loc = msl_term_val(sup);\n"
        "  uint  lab     = msl_term_ext(sup);\n"
        "  Term a = heap[sup_loc + 0];\n"
        "  Term b = heap[sup_loc + 1];\n"
        "  ulong c = aot_book_alloc(book_next, 6u);\n"
        "  heap[c + 0] = num;\n"
        "  heap[c + 1] = a;\n"
        "  heap[c + 2] = num;\n"
        "  heap[c + 3] = b;\n"
        "  heap[c + 4] = msl_term_new(TAG_OP2, op, c + 0);\n"
        "  heap[c + 5] = msl_term_new(TAG_OP2, op, c + 2);\n"
        "  return msl_term_new(TAG_SUP, lab, c + 4);\n"
        "}\n"
        "\n"
        "// eql_sup_l (src/wnf/_.c TAG_EQL SUP-L): SUP on left arg.\n"
        "// 7-cell allocation (DUP body for b + 2 EQL layouts + SUP).\n"
        "static Term aot_eql_sup_l(Term sup, Term b,\n"
        "    device Term *heap, device atomic_uint *book_next) {\n"
        "  ulong sup_loc = msl_term_val(sup);\n"
        "  uint  lab     = msl_term_ext(sup);\n"
        "  Term a0 = heap[sup_loc + 0];\n"
        "  Term a1 = heap[sup_loc + 1];\n"
        "  ulong c = aot_book_alloc(book_next, 7u);\n"
        "  heap[c + 0] = b;\n"
        "  heap[c + 1] = a0;\n"
        "  heap[c + 2] = msl_term_new(TAG_DP0, lab, c + 0);\n"
        "  heap[c + 3] = a1;\n"
        "  heap[c + 4] = msl_term_new(TAG_DP1, lab, c + 0);\n"
        "  heap[c + 5] = msl_term_new(TAG_EQL, 0u, c + 1);\n"
        "  heap[c + 6] = msl_term_new(TAG_EQL, 0u, c + 3);\n"
        "  return msl_term_new(TAG_SUP, lab, c + 5);\n"
        "}\n"
        "\n"
        "// eql_sup_r (src/wnf/_.c TAG_EQL SUP-R): SUP on right arg.\n"
        "// 7-cell allocation (DUP body for a + 2 EQL layouts + SUP).\n"
        "static Term aot_eql_sup_r(Term a, Term sup,\n"
        "    device Term *heap, device atomic_uint *book_next) {\n"
        "  ulong sup_loc = msl_term_val(sup);\n"
        "  uint  lab     = msl_term_ext(sup);\n"
        "  Term b0 = heap[sup_loc + 0];\n"
        "  Term b1 = heap[sup_loc + 1];\n"
        "  ulong c = aot_book_alloc(book_next, 7u);\n"
        "  heap[c + 0] = a;\n"
        "  heap[c + 1] = msl_term_new(TAG_DP0, lab, c + 0);\n"
        "  heap[c + 2] = b0;\n"
        "  heap[c + 3] = msl_term_new(TAG_DP1, lab, c + 0);\n"
        "  heap[c + 4] = b1;\n"
        "  heap[c + 5] = msl_term_new(TAG_EQL, 0u, c + 1);\n"
        "  heap[c + 6] = msl_term_new(TAG_EQL, 0u, c + 3);\n"
        "  return msl_term_new(TAG_SUP, lab, c + 5);\n"
        "}\n"
        "\n"
        "// app_mat_sup (src/interact/app_mat_sup.c): MAT scrutinee SUP.\n"
        "// 12-cell: 2 DUP bodies + 2 MATs + 2 APPs + SUP wrapper.\n"
        "static Term aot_app_mat_sup(Term mat, Term sup,\n"
        "    device Term *heap, device atomic_uint *book_next) {\n"
        "  ulong sup_loc = msl_term_val(sup);\n"
        "  uint  lab     = msl_term_ext(sup);\n"
        "  ulong mat_loc = msl_term_val(mat);\n"
        "  uint  match   = msl_term_ext(mat);\n"
        "  Term handler  = heap[mat_loc + 0];\n"
        "  Term fallback = heap[mat_loc + 1];\n"
        "  Term a = heap[sup_loc + 0];\n"
        "  Term b = heap[sup_loc + 1];\n"
        "  ulong c = aot_book_alloc(book_next, 12u);\n"
        "  heap[c + 0] = handler;\n"
        "  heap[c + 1] = fallback;\n"
        "  heap[c + 2] = msl_term_new(TAG_DP0, lab, c + 0);\n"
        "  heap[c + 3] = msl_term_new(TAG_DP0, lab, c + 1);\n"
        "  heap[c + 4] = msl_term_new(TAG_DP1, lab, c + 0);\n"
        "  heap[c + 5] = msl_term_new(TAG_DP1, lab, c + 1);\n"
        "  heap[c + 6] = msl_term_new(TAG_MAT, match, c + 2);\n"
        "  heap[c + 7] = a;\n"
        "  heap[c + 8] = msl_term_new(TAG_MAT, match, c + 4);\n"
        "  heap[c + 9] = b;\n"
        "  heap[c + 10] = msl_term_new(TAG_APP, 0u, c + 6);\n"
        "  heap[c + 11] = msl_term_new(TAG_APP, 0u, c + 8);\n"
        "  return msl_term_new(TAG_SUP, lab, c + 10);\n"
        "}\n"
        "\n");
    aot_emit_fmt(&b,
        "kernel void aot_def_%s(\n"
        "    device   Term         *heap      [[buffer(0)]],\n"
        "    device   Term         *args      [[buffer(1)]],\n"
        "    device   Term         *result    [[buffer(2)]],\n"
        "    device   atomic_uint  *book_next [[buffer(3)]],\n"
        "    uint                   tid       [[thread_position_in_grid]])\n"
        "{\n"
        "  (void)heap; (void)book_next;\n"
        "  if (tid != 0) return;\n",
        name);

    g_msl_fresh = 0;
    g_msl_self_def_id = def_id;
    g_msl_emit_failed = 0;
    g_msl_emit_failure_reason[0] = '\0';
    g_msl_dup_n = 0;   // iter V: reset dup memo for this emit
    g_msl_dup_term_n = 0; // iter Z: reset Term-memo for IC construction

    // Iter Z: Church-style bodies (containing TAG_SUP / TAG_LAM /
    // TAG_BJ at any depth, or TAG_DP whose body isn't NUM) need the
    // GPU-side wnf state machine + IC-interaction inlines.  Construct
    // the term tree on the shared book heap, then drive wnf to WHNF
    // and write the root Term to result[0].
    int emitted_ic = 0;
    if (aot_msl_body_needs_ic(cursor)) {
        const char *root_var = aot_msl_emit_term(&b, cursor, &bind);
        // Stack cap chosen to fit Apple GPU thread-private memory
        // budget at 8 bytes per Term (4096 * 8 = 32 KiB).  Iter cap
        // bounds runaway loops; future iter can grow / make data-driven.
        const u32 stack_cap = 4096u;
        const u32 iter_cap  = 1u << 20;
        aot_emit_str(&b,
            "  // === Iter Z: GPU-side wnf state machine ===\n");
        aot_emit_fmt(&b,
            "  thread Term  ic_stk[%uu];\n"
            "  uint  ic_spos  = 0u;\n"
            "  Term  next     = %s;\n"
            "  Term  whnf     = 0u;\n"
            "  uint  state    = 0u;\n"
            "  uint  ic_iters = 0u;\n",
            stack_cap, root_var);
        aot_emit_fmt(&b,
            "  while (state != 2u && ic_iters < %uu) {\n"
            "    ic_iters++;\n",
            iter_cap);
        // ENTER phase
        aot_emit_str(&b,
            "    if (state == 0u) {\n"
            "      uint t = msl_term_tag(next);\n"
            "      if (t == TAG_VAR) {\n"
            "        ulong loc = msl_term_val(next);\n"
            "        Term  cell = heap[loc];\n"
            "        if (msl_sub_get(cell)) { next = msl_sub_clr(cell); continue; }\n"
            "        whnf = next; state = 1u; continue;\n"
            "      }\n");
        aot_emit_fmt(&b,
            "      if (t == TAG_DP0 || t == TAG_DP1) {\n"
            "        ulong loc = msl_term_val(next);\n"
            "        Term  cell = heap[loc];\n"
            "        if (msl_sub_get(cell)) { next = msl_sub_clr(cell); continue; }\n"
            "        if (ic_spos >= %uu) {\n"
            "          result[0] = msl_term_new(TAG_ERA, 0xFFFFFu, 0u); return;\n"
            "        }\n"
            "        ic_stk[ic_spos++] = next;\n"
            "        next = cell; continue;\n"
            "      }\n",
            stack_cap);
        aot_emit_fmt(&b,
            "      if (t == TAG_APP) {\n"
            "        ulong loc = msl_term_val(next);\n"
            "        if (ic_spos >= %uu) {\n"
            "          result[0] = msl_term_new(TAG_ERA, 0xFFFFFu, 0u); return;\n"
            "        }\n"
            "        ic_stk[ic_spos++] = next;\n"
            "        next = heap[loc]; continue;\n"
            "      }\n",
            stack_cap);
        // OP2 ENTER: push frame, descend into left arg.
        aot_emit_fmt(&b,
            "      if (t == TAG_OP2) {\n"
            "        ulong loc = msl_term_val(next);\n"
            "        if (ic_spos >= %uu) {\n"
            "          result[0] = msl_term_new(TAG_ERA, 0xFFFFFu, 0u); return;\n"
            "        }\n"
            "        ic_stk[ic_spos++] = next;\n"
            "        next = heap[loc + 0]; continue;\n"
            "      }\n",
            stack_cap);
        // EQL ENTER: push frame, descend into left arg (mirrors OP2).
        aot_emit_fmt(&b,
            "      if (t == TAG_EQL) {\n"
            "        ulong loc = msl_term_val(next);\n"
            "        if (ic_spos >= %uu) {\n"
            "          result[0] = msl_term_new(TAG_ERA, 0xFFFFFu, 0u); return;\n"
            "        }\n"
            "        ic_stk[ic_spos++] = next;\n"
            "        next = heap[loc + 0]; continue;\n"
            "      }\n",
            stack_cap);
        aot_emit_str(&b,
            "      // LAM / SUP / ERA / NUM / REF / CTR / MAT: WHNF root.\n"
            "      whnf = next; state = 1u; continue;\n"
            "    } else {\n");
        // APPLY phase
        aot_emit_str(&b,
            "      if (ic_spos == 0u) { state = 2u; continue; }\n"
            "      Term  frame = ic_stk[--ic_spos];\n"
            "      uint  ft    = msl_term_tag(frame);\n");
        aot_emit_fmt(&b,
            "      if (ft == TAG_APP) {\n"
            "        ulong app_loc = msl_term_val(frame);\n"
            "        Term  arg     = heap[app_loc + 1];\n"
            "        uint  wt = msl_term_tag(whnf);\n"
            "        if (wt == TAG_LAM) {\n"
            "          next = aot_app_lam(whnf, arg, heap);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        if (wt == TAG_SUP) {\n"
            "          next = aot_app_sup(whnf, arg, heap, book_next);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        if (wt == TAG_ERA) {\n"
            "          whnf = msl_term_new(TAG_ERA, 0u, 0u); continue;\n"
            "        }\n"
            "        if (wt == TAG_MAT) {\n"
            "          // APP-MAT: dispatch on the scrutinee.  Push the\n"
            "          // MAT cell back onto the stack as a frame and\n"
            "          // descend into the scrutinee (heap[app_loc+1]).\n"
            "          if (ic_spos >= %uu) {\n"
            "            result[0] = msl_term_new(TAG_ERA, 0xFFFFFu, 0u); return;\n"
            "          }\n"
            "          ic_stk[ic_spos++] = whnf;\n"
            "          next = arg; state = 0u; continue;\n"
            "        }\n"
            "        heap[app_loc + 0] = whnf;\n"
            "        whnf = frame; continue;\n"
            "      }\n",
            stack_cap);
        aot_emit_str(&b,
            "      if (ft == TAG_DP0 || ft == TAG_DP1) {\n"
            "        ulong loc  = msl_term_val(frame);\n"
            "        uint  lab  = msl_term_ext(frame);\n"
            "        uint  side = (ft == TAG_DP0) ? 0u : 1u;\n"
            "        uint  wt   = msl_term_tag(whnf);\n"
            "        if (wt == TAG_SUP) {\n"
            "          next = aot_dup_sup(lab, loc, side, whnf, heap, book_next);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        if (wt == TAG_LAM) {\n"
            "          next = aot_dup_lam(lab, loc, side, whnf, heap, book_next);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        if (wt == TAG_NUM) {\n"
            "          whnf = aot_dup_num(side, loc, whnf, heap); continue;\n"
            "        }\n"
            "        if (wt == TAG_ERA) {\n"
            "          whnf = aot_dup_era(side, loc, whnf, heap); continue;\n"
            "        }\n"
            "        heap[loc] = whnf;\n"
            "        whnf = frame; continue;\n"
            "      }\n");
        // OP2 frame: whnf is reduced left arg.
        aot_emit_fmt(&b,
            "      if (ft == TAG_OP2) {\n"
            "        ulong loc = msl_term_val(frame);\n"
            "        uint  op  = msl_term_ext(frame);\n"
            "        uint  wt  = msl_term_tag(whnf);\n"
            "        if (wt == TAG_NUM) {\n"
            "          // x is NUM.  Push F_OP2_NUM (op + xv baked in)\n"
            "          // and descend into right arg.  ext = op (8 bits);\n"
            "          // val = xv (NUM raw).\n"
            "          if (ic_spos >= %uu) {\n"
            "            result[0] = msl_term_new(TAG_ERA, 0xFFFFFu, 0u); return;\n"
            "          }\n"
            "          ic_stk[ic_spos++] = msl_term_new(TAG_F_OP2_NUM, op,\n"
            "                                          msl_term_val(whnf));\n"
            "          next = heap[loc + 1]; state = 0u; continue;\n"
            "        }\n"
            "        if (wt == TAG_SUP) {\n"
            "          Term y = heap[loc + 1];\n"
            "          next = aot_op2_sup(op, whnf, y, heap, book_next);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        // Stuck: rebuild OP2 and propagate.\n"
            "        heap[loc + 0] = whnf;\n"
            "        whnf = frame; continue;\n"
            "      }\n",
            stack_cap);
        // F_OP2_NUM frame: whnf is reduced right arg; left NUM baked in.
        aot_emit_str(&b,
            "      if (ft == TAG_F_OP2_NUM) {\n"
            "        uint op = msl_term_ext(frame);\n"
            "        uint xv = uint(msl_term_val(frame));\n"
            "        uint wt = msl_term_tag(whnf);\n"
            "        if (wt == TAG_NUM) {\n"
            "          uint yv = uint(msl_term_val(whnf));\n"
            "          uint r  = aot_op2_fire(op, xv, yv);\n"
            "          whnf = msl_term_new(TAG_NUM, msl_term_ext(whnf), ulong(r));\n"
            "          continue;\n"
            "        }\n"
            "        if (wt == TAG_SUP) {\n"
            "          Term x = msl_term_new(TAG_NUM, 0u, ulong(xv));\n"
            "          next = aot_op2_num_sup(op, x, whnf, heap, book_next);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        // Stuck: rebuild OP2(NUM(xv), whnf) on a fresh cell.\n"
            "        ulong nloc = aot_book_alloc(book_next, 2u);\n"
            "        heap[nloc + 0] = msl_term_new(TAG_NUM, 0u, ulong(xv));\n"
            "        heap[nloc + 1] = whnf;\n"
            "        whnf = msl_term_new(TAG_OP2, op, nloc); continue;\n"
            "      }\n");
        // EQL frame: whnf is reduced left arg.
        aot_emit_fmt(&b,
            "      if (ft == TAG_EQL) {\n"
            "        ulong loc = msl_term_val(frame);\n"
            "        uint  wt  = msl_term_tag(whnf);\n"
            "        if (wt == TAG_SUP) {\n"
            "          Term b = heap[loc + 1];\n"
            "          next = aot_eql_sup_l(whnf, b, heap, book_next);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        if (wt == TAG_NUM) {\n"
            "          // a is NUM.  Push F_EQL_R holding loc; descend\n"
            "          // right arg.  We store the reduced a back into\n"
            "          // heap[loc+0] so F_EQL_R can read it.\n"
            "          heap[loc + 0] = whnf;\n"
            "          if (ic_spos >= %uu) {\n"
            "            result[0] = msl_term_new(TAG_ERA, 0xFFFFFu, 0u); return;\n"
            "          }\n"
            "          ic_stk[ic_spos++] = msl_term_new(TAG_F_EQL_R, 0u, loc);\n"
            "          next = heap[loc + 1]; state = 0u; continue;\n"
            "        }\n"
            "        // Stuck: rebuild EQL.\n"
            "        heap[loc + 0] = whnf;\n"
            "        whnf = frame; continue;\n"
            "      }\n",
            stack_cap);
        // F_EQL_R frame: whnf is reduced right arg; left stored at heap[loc+0].
        aot_emit_str(&b,
            "      if (ft == TAG_F_EQL_R) {\n"
            "        ulong loc = msl_term_val(frame);\n"
            "        Term  a   = heap[loc + 0];\n"
            "        uint  wt  = msl_term_tag(whnf);\n"
            "        if (wt == TAG_SUP) {\n"
            "          next = aot_eql_sup_r(a, whnf, heap, book_next);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        if (msl_term_tag(a) == TAG_NUM && wt == TAG_NUM) {\n"
            "          uint av = uint(msl_term_val(a));\n"
            "          uint bv = uint(msl_term_val(whnf));\n"
            "          uint r  = (av == bv) ? 1u : 0u;\n"
            "          whnf = msl_term_new(TAG_NUM, msl_term_ext(a), ulong(r));\n"
            "          continue;\n"
            "        }\n"
            "        // Stuck: rebuild EQL.\n"
            "        heap[loc + 1] = whnf;\n"
            "        whnf = msl_term_new(TAG_EQL, 0u, loc); continue;\n"
            "      }\n");
        // MAT-dispatch frame: whnf is reduced scrutinee, frame is the MAT cell.
        aot_emit_str(&b,
            "      if (ft == TAG_MAT) {\n"
            "        ulong mat_loc = msl_term_val(frame);\n"
            "        uint  match   = msl_term_ext(frame);\n"
            "        uint  wt      = msl_term_tag(whnf);\n"
            "        if (wt == TAG_NUM && uint(msl_term_val(whnf)) == match) {\n"
            "          next = heap[mat_loc + 0]; state = 0u; continue;\n"
            "        }\n"
            "        if (wt == TAG_SUP) {\n"
            "          next = aot_app_mat_sup(frame, whnf, heap, book_next);\n"
            "          state = 0u; continue;\n"
            "        }\n"
            "        // Miss (NUM != match, CTR, etc.): build APP(fallback,\n"
            "        // whnf) on a fresh 2-cell layout.  CTR-arm dispatch is\n"
            "        // not yet emitted -- the substitutivity toy doesn't\n"
            "        // need it (scrutinees are NUM-or-SUP).\n"
            "        ulong app_loc = aot_book_alloc(book_next, 2u);\n"
            "        heap[app_loc + 0] = heap[mat_loc + 1];\n"
            "        heap[app_loc + 1] = whnf;\n"
            "        next = msl_term_new(TAG_APP, 0u, app_loc); state = 0u;\n"
            "        continue;\n"
            "      }\n");
        aot_emit_str(&b,
            "      whnf = frame; continue;\n"
            "    }\n"   /* close else (APPLY) */
            "  }\n"     /* close while */);
        aot_emit_fmt(&b,
            "  if (ic_iters >= %uu) {\n"
            "    result[0] = msl_term_new(TAG_ERA, 0xFFFFEu, 0u);\n"
            "  } else {\n"
            "    result[0] = whnf;\n"
            "  }\n"
            "}\n",
            iter_cap);
        emitted_ic = 1;
    }

    // Iter F + L: detect App(MAT-chain, TVar) shape.  After TLam-peel
    // the body might be:
    //   App(MAT[v0, [h0, MAT[v1, [h1, ... fallback]]]], TVar(arg))
    // For each MAT cell, the handler shape decides the arm:
    //   * value handler (TNum/TVar/TOp2/...) -> NUM-arm:
    //       if (scrutinee_tag == NUM && scrutinee_val == match) ...
    //   * TLam-wrapped handler (one or more lambdas) -> CTR-arm:
    //       if (scrutinee_tag == CTR && scrutinee_ext == match) {
    //         Term x = heap[scrutinee_val + 1];   (per peeled LAM)
    //         ...
    //         result_val = <body uint>;
    //       }
    // Mixed-shape chains are allowed (e.g., 0 -> NUM(42), #1 x -> x).
    int emitted_mat = 0;
    if (!emitted_ic && term_tag(cursor) == TAG_APP) {
        u64  app_loc  = term_val(cursor);
        Term mat_head = book_read(app_loc + 0);
        Term arg      = book_read(app_loc + 1);
        if (term_tag(mat_head) == TAG_MAT && term_tag(arg) == TAG_VAR) {
            const char *arg_name = aot_msl_bind_lookup(&bind, term_val(arg));
            if (arg_name != NULL) {
                aot_emit_fmt(&b,
                    "  Term scrutinee_term = %s;\n"
                    "  uint scrutinee_tag = uint((scrutinee_term >> TAG_SHIFT) & TAG_MASK);\n"
                    "  uint scrutinee_val = uint(scrutinee_term & VAL_MASK);\n"
                    "  uint scrutinee_ext = uint((scrutinee_term >> EXT_SHIFT) & EXT_MASK);\n"
                    "  (void)scrutinee_ext;\n"
                    "  uint result_val = 0u;\n",
                    arg_name);
                Term mc = mat_head;
                int  first = 1;
                while (term_tag(mc) == TAG_MAT) {
                    u32  match_val = term_ext(mc);
                    u64  ml        = term_val(mc);
                    Term handler   = book_read(ml + 0);
                    Term fallback  = book_read(ml + 1);
                    if (term_tag(handler) == TAG_LAM) {
                        // CTR-arm: peel LAMs, bind each to heap[val+1+i].
                        u32 saved_n = bind.n;
                        Term hcursor = handler;
                        u32 lam_idx = 0;
                        char binder_names[8][32];
                        while (term_tag(hcursor) == TAG_LAM && lam_idx < 8) {
                            u32 fr = g_msl_fresh++;
                            snprintf(binder_names[lam_idx], sizeof binder_names[0],
                                     "ctr_%u", fr);
                            u64 lam_loc = term_val(hcursor);
                            aot_msl_bind_push(&bind, lam_loc, binder_names[lam_idx]);
                            hcursor = book_read(lam_loc);
                            lam_idx++;
                        }
                        aot_emit_fmt(&b,
                            "  %sif (scrutinee_tag == 20u && scrutinee_ext == %uu) {\n",
                            first ? "" : "else ", match_val);
                        for (u32 i = 0; i < lam_idx; i++) {
                            aot_emit_fmt(&b,
                                "    Term %s = heap[scrutinee_val + %uu];\n",
                                binder_names[i], i + 1);
                        }
                        const char *hv = aot_msl_emit_uint(&b, hcursor, &bind);
                        aot_emit_fmt(&b,
                            "    result_val = %s;\n"
                            "  }\n", hv);
                        bind.n = saved_n;
                    } else {
                        // NUM-arm (iter F path).
                        const char *hv = aot_msl_emit_uint(&b, handler, &bind);
                        aot_emit_fmt(&b,
                            "  %sif (scrutinee_tag == %uu && scrutinee_val == %uu) result_val = %s;\n",
                            first ? "" : "else ",
                            (u32)TAG_NUM, match_val, hv);
                    }
                    first = 0;
                    mc = fallback;
                }
                // Default arm: fallback expression after the MAT chain.
                // If the fallback is TLam (catch-all binding the
                // scrutinee), peel + bind; else emit as value expr.
                if (term_tag(mc) == TAG_LAM) {
                    u32 saved_n = bind.n;
                    u32 fr = g_msl_fresh++;
                    char def_binder[32];
                    snprintf(def_binder, sizeof def_binder, "def_arg_%u", fr);
                    u64 def_lam_loc = term_val(mc);
                    aot_msl_bind_push(&bind, def_lam_loc, def_binder);
                    Term def_body = book_read(def_lam_loc);
                    aot_emit_fmt(&b,
                        "  else {\n"
                        "    Term %s = scrutinee_term;\n",
                        def_binder);
                    const char *fv = aot_msl_emit_uint(&b, def_body, &bind);
                    aot_emit_fmt(&b,
                        "    result_val = %s;\n"
                        "  }\n", fv);
                    bind.n = saved_n;
                } else {
                    const char *fv = aot_msl_emit_uint(&b, mc, &bind);
                    aot_emit_fmt(&b, "  else result_val = %s;\n", fv);
                }
                // Iter HH: preserve input dtype on result.  Mirrors
                // the C runtime's NUM ops (ADD/SUB/MUL keep LHS dtype).
                aot_emit_fmt(&b,
                    "  result[0] = msl_term_new(TAG_NUM,"
                    " %s, ulong(result_val));\n"
                    "}\n",
                    argc > 0 ? "msl_term_ext(args[0])" : "0u");
                emitted_mat = 1;
            }
        }
    }

    int emitted_ctr = 0;
    if (!emitted_ic && !emitted_mat && term_tag(cursor) == TAG_CTR) {
        // Iter H: build a fresh CTR cell on the GPU heap.  Layout:
        //   heap[loc]      = NUM(n) with DT_INT32 ext
        //   heap[loc+1..n] = children (each as a Term)
        // Result: term_new(0, TAG_CTR, label, loc).
        u32  label  = term_ext(cursor);
        u64  ctr_loc = term_val(cursor);
        Term n_cell  = book_read(ctr_loc);
        u32  n       = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0;
        if (n <= 8) {
            // Materialize each child Term first (uint exprs may live
            // in the static ring; the emit below has its own fresh
            // counter per call so we copy each into a local).
            char child_exprs[8][96];
            for (u32 i = 0; i < n; i++) {
                Term child = book_read(ctr_loc + 1 + i);
                const char *ce = aot_msl_emit_term(&b, child, &bind);
                snprintf(child_exprs[i], sizeof child_exprs[0], "%s", ce);
            }
            aot_emit_fmt(&b,
                "  ulong ctr_loc = aot_book_alloc(book_next, %uu);\n"
                "  heap[ctr_loc + 0] = msl_term_new(TAG_NUM, DT_INT32, ulong(%uu));\n",
                1u + n, n);
            for (u32 i = 0; i < n; i++) {
                aot_emit_fmt(&b,
                    "  heap[ctr_loc + %uu] = %s;\n",
                    i + 1, child_exprs[i]);
            }
            aot_emit_fmt(&b,
                "  result[0] = msl_term_new(TAG_CTR, %uu, ctr_loc);\n"
                "}\n",
                label);
            emitted_ctr = 1;
        }
    }

    if (!emitted_ic && !emitted_mat && !emitted_ctr) {
        // Plain value-expression body: emit directly + wrap as NUM.
        // Iter HH: preserve input dtype on result.
        const char *rv = aot_msl_emit_uint(&b, cursor, &bind);
        aot_emit_fmt(&b,
            "  result[0] = msl_term_new(TAG_NUM,"
            " %s, ulong(%s));\n"
            "}\n",
            argc > 0 ? "msl_term_ext(args[0])" : "0u",
            rv);
    }

    if (g_msl_emit_failed) {
        // Iter G: an unsupported shape (recursion, partial app, etc.)
        // tripped the failure flag during recursive emit.  Discard the
        // partial source so the caller can fall back / report failure.
        // Iter Z: when env THVM_AOT_METAL_DUMP=1 also print the partial
        // source so failures during the IC path are diagnosable.
        const char *e = getenv("THVM_AOT_METAL_DUMP");
        if (e != NULL && e[0] == '1' && b.buf != NULL) {
            fprintf(stderr,
                "// === thvm aot-metal: PARTIAL emit (FAILED) for \"%s\" ===\n%s"
                "// === end PARTIAL \"%s\" reason=\"%s\" ===\n",
                name, b.buf, name, g_msl_emit_failure_reason);
        }
        free(b.buf);
        return NULL;
    }

    return b.buf;
}
