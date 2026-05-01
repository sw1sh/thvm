// aot/emit.c -- walk a TDef'd body and emit equivalent C source for
// the AOT layer.
//
// Phase 1 coverage:
//   - TNum constants
//   - TLam[x, body] -- pops one APP arg and binds x in the body
//   - TVar(loc) -- references a bound variable
//   - TApp[fun, arg] -- builds APP via term_new_app on arbitrary
//                       fun/arg, OR for the spine pattern
//                       App(App(... App(Ref, a), b), c) emits a
//                       direct fallback that pushes args + calls
//                       through wnf so AOT_FNS dispatches.
//   - TRef[name] -- emits term_new_ref(name)
//   - TOp2[op, x, y] -- inline OP2 cell construction (op fold
//                       happens at runtime via wnf)
//   - TAG_MAT chain dispatch -- emits if-tree on tag/value
//
// What's still NOT emitted:
//   - TCtr / TDup / TSup / TBri / TAnn / TUop -- fall through to
//     "/* unsupported */ aot_fallback".
//   - Tail-recursion -> goto loop (always uses C recursion).
//   - LAM_ERA_MASK fast-path for unused binders.
//
// The output is meant to #include into the same TU as the runtime
// (matching src/aot/programs/<name>.c).  The compile + dlopen path
// (src/aot/build.c) needs an ABI split first.

#define AOT_EMIT_CAP    (1u << 16)   // 64 KB initial output buffer
#define AOT_BIND_CAP    32

typedef struct {
    char *buf;
    u32   len;
    u32   cap;
} AotEmitBuf;

typedef struct {
    u64  lam_loc;        // book-heap loc of the LAM binder
    char name[24];       // C variable name, e.g. "arg_0"
} AotBinding;

// Tracks DP0/DP1 sharing.  A book-side dup body loc maps to a pair
// of C variable names (dp0_n, dp1_n).  First encounter of either
// projection emits an aot_make_dup() and registers both names;
// subsequent encounters reuse the bound names.
typedef struct {
    u64  dup_loc;        // book heap loc of the shared dup body
    char dp0[24];        // emitted C var for DP0
    char dp1[24];        // emitted C var for DP1
} AotDupBinding;

typedef struct {
    AotBinding entries[AOT_BIND_CAP];
    u32 n;
    u32 fresh;           // monotonic counter for fresh variable names

    AotDupBinding dups[AOT_BIND_CAP];
    u32 n_dups;
} AotBindings;

// === buffer helpers ==================================================

static void aot_emit_init(AotEmitBuf *b) {
    b->buf = (char *)malloc(AOT_EMIT_CAP);
    b->len = 0;
    b->cap = AOT_EMIT_CAP;
    if (b->buf) b->buf[0] = '\0';
}

static void aot_emit_grow(AotEmitBuf *b, u32 need) {
    while (b->cap - b->len < need) {
        u32 new_cap = b->cap * 2;
        char *nb = (char *)realloc(b->buf, new_cap);
        if (!nb) return;
        b->buf = nb;
        b->cap = new_cap;
    }
}

static void aot_emit_str(AotEmitBuf *b, const char *s) {
    u32 n = (u32)strlen(s);
    aot_emit_grow(b, n + 1);
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void aot_emit_fmt(AotEmitBuf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[1024];
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    aot_emit_str(b, tmp);
}

// === bindings ========================================================

static void aot_bind_init(AotBindings *bind) {
    bind->n = 0;
    bind->fresh = 0;
    bind->n_dups = 0;
}

// Look up the DP0/DP1 names for the given dup body loc.  Returns the
// AotDupBinding if found, NULL otherwise.
static AotDupBinding *aot_dup_lookup(AotBindings *bind, u64 dup_loc) {
    for (u32 i = 0; i < bind->n_dups; i++) {
        if (bind->dups[i].dup_loc == dup_loc) return &bind->dups[i];
    }
    return NULL;
}

// Register a fresh dup pair for the given book loc.  Caller has
// already emitted the `aot_make_dup` call that initialises the
// returned C variable names.
static AotDupBinding *aot_dup_push(AotBindings *bind, u64 dup_loc,
                                   const char *dp0, const char *dp1) {
    if (bind->n_dups >= AOT_BIND_CAP) return NULL;
    AotDupBinding *e = &bind->dups[bind->n_dups++];
    e->dup_loc = dup_loc;
    snprintf(e->dp0, sizeof e->dp0, "%s", dp0);
    snprintf(e->dp1, sizeof e->dp1, "%s", dp1);
    return e;
}

static void aot_bind_push(AotBindings *bind, u64 lam_loc, const char *name) {
    if (bind->n >= AOT_BIND_CAP) return;
    bind->entries[bind->n].lam_loc = lam_loc;
    snprintf(bind->entries[bind->n].name, sizeof(bind->entries[bind->n].name),
             "%s", name);
    bind->n++;
}

static void aot_bind_pop(AotBindings *bind) {
    if (bind->n > 0) bind->n--;
}

static const char *aot_bind_lookup(AotBindings *bind, u64 lam_loc) {
    for (u32 i = bind->n; i > 0; i--) {
        if (bind->entries[i - 1].lam_loc == lam_loc) {
            return bind->entries[i - 1].name;
        }
    }
    return NULL;
}

static u32 aot_fresh(AotBindings *bind) {
    return bind->fresh++;
}

// === Book-aware CTR accessors ========================================
//
// `term_ctr_n` / `term_ctr_at` (in src/term/new_ctr.c) use HEAP[]
// directly -- right for runtime CTRs, wrong for book CTRs.  The
// emitter walks the book template (the snapshot TDef'd into
// BOOK_HEAP[]), so it needs versions that read through book_read.

static u32 aot_book_ctr_n(Term ctr_term) {
    if (term_tag(ctr_term) != TAG_CTR) return 0;
    Term n_cell = book_read(term_val(ctr_term));
    if (term_tag(n_cell) != TAG_NUM)   return 0;
    return (u32)term_val(n_cell);
}

static Term aot_book_ctr_at(Term ctr_term, u32 i) {
    u32 n = aot_book_ctr_n(ctr_term);
    if (i >= n) return 0;
    return book_read(term_val(ctr_term) + 1 + i);
}

// === Op2 opcode -> C identifier =======================================

static const char *aot_op_id(u32 op) {
    switch (op) {
      case OP_ADD: return "OP_ADD";
      case OP_SUB: return "OP_SUB";
      case OP_MUL: return "OP_MUL";
      case OP_EQ:  return "OP_EQ";
      case OP_LT:  return "OP_LT";
      default:     return "0";
    }
}

// === Term emission (forward decl) ====================================

// Emit C statements that bind a fresh `Term` C variable to the value
// of `term` and return the name of that variable.  Output goes to b;
// the returned pointer is into a per-call static buffer (caller must
// not retain across recursive aot_emit_term calls).
//
// `indent` is the C indent prefix for emitted statements.
static const char *aot_emit_term(AotEmitBuf *b, Term term,
                                 AotBindings *bind, const char *indent);

// Emit a if/else-if tree that walks a TAG_MAT chain.  `arg_var` is
// the C variable holding the (already-forced) input arg.  Returns
// the C variable that holds the chain's result (only meaningful for
// the ZER/ARITY-0 arms; otherwise we emit `return ...;` directly).
static const char *aot_emit_mat_chain(AotEmitBuf *b, Term mat,
                                      const char *arg_var,
                                      AotBindings *bind, const char *indent);

// === Implementations =================================================

static const char *aot_emit_term(AotEmitBuf *b, Term term,
                                 AotBindings *bind, const char *indent) {
    static char name_buf[256][32];   // ring of static-name slots
    static u32  name_idx = 0;
    char *out = name_buf[(name_idx++) & 0xFF];

    u32 fresh = aot_fresh(bind);
    snprintf(out, 32, "v_%u", fresh);

    u8 tag = term_tag(term);
    switch (tag) {
      case TAG_NUM: {
        u32 ext = term_ext(term);
        u32 val = (u32)term_val(term);
        aot_emit_fmt(b, "%sTerm %s = term_new(0, TAG_NUM, %u, %u);\n",
                     indent, out, ext, val);
        return out;
      }

      case TAG_ERA: {
        aot_emit_fmt(b, "%sTerm %s = term_new(0, TAG_ERA, 0, 0);\n",
                     indent, out);
        return out;
      }

      case TAG_VAR: {
        u64 loc = term_val(term);
        const char *bound = aot_bind_lookup(bind, loc);
        if (bound) {
            aot_emit_fmt(b, "%sTerm %s = %s;\n", indent, out, bound);
        } else {
            // Free var: emit a placeholder VAR.  Should never happen
            // in well-formed bodies; signals an emitter bug.
            aot_emit_fmt(b,
                "%sTerm %s = term_new(0, TAG_VAR, 0, 0);  /* unbound %llu */\n",
                indent, out, (unsigned long long)loc);
        }
        return out;
      }

      case TAG_REF: {
        u32 name = term_ext(term);
        aot_emit_fmt(b, "%sTerm %s = term_new(0, TAG_REF, %u, 0);\n",
                     indent, out, name);
        return out;
      }

      case TAG_CTR: {
        // term_new_ctr(label, children, n).  Walk the children
        // (which may themselves contain DPs / OPs / nested CTRs)
        // recursively, then emit the construction.
        u32 label = term_ext(term);
        u32 n = aot_book_ctr_n(term);
        if (n > 16) {
            aot_emit_fmt(b,
                "%s/* CTR arity %u exceeds emitter cap */\n"
                "%sTerm %s = aot_fallback(def_slot);\n",
                indent, n, indent, out);
            return out;
        }
        if (n == 0) {
            aot_emit_fmt(b,
                "%sTerm %s = term_new_ctr(%u, NULL, 0);\n",
                indent, out, label);
            return out;
        }
        // Materialise each child's expression first; the names
        // returned by aot_emit_term are stable for the duration
        // of THIS call (the static name ring has 256 slots).
        const char *child_vars[16];
        for (u32 i = 0; i < n; i++) {
            child_vars[i] = aot_emit_term(b, aot_book_ctr_at(term, i),
                                          bind, indent);
        }
        // Stage children into a Term[] array so term_new_ctr can
        // copy them in.
        aot_emit_fmt(b, "%sTerm %s_ch[%u] = {", indent, out, n);
        for (u32 i = 0; i < n; i++) {
            aot_emit_fmt(b, "%s%s", i ? ", " : "", child_vars[i]);
        }
        aot_emit_fmt(b, "};\n%sTerm %s = term_new_ctr(%u, %s_ch, %u);\n",
                     indent, out, label, out, n);
        return out;
      }

      case TAG_DP0:
      case TAG_DP1: {
        // Look up or allocate the shared dup pair for this book
        // dup body loc.  First encounter of either projection
        // emits aot_make_dup() and binds both names; subsequent
        // encounters reuse the bound names.
        u64 dup_loc = term_val(term);
        u32 label = term_ext(term);
        AotDupBinding *db = aot_dup_lookup(bind, dup_loc);
        if (db == NULL) {
            char dp0_name[24], dp1_name[24];
            u32 idx = aot_fresh(bind);
            snprintf(dp0_name, sizeof dp0_name, "dp0_%u", idx);
            snprintf(dp1_name, sizeof dp1_name, "dp1_%u", idx);
            // Recursively emit the body.
            Term body = book_read(dup_loc);
            const char *body_var = aot_emit_term(b, body, bind, indent);
            aot_emit_fmt(b,
                "%sTerm %s, %s;\n"
                "%saot_make_dup(%u, %s, &%s, &%s);\n",
                indent, dp0_name, dp1_name,
                indent, label, body_var, dp0_name, dp1_name);
            db = aot_dup_push(bind, dup_loc, dp0_name, dp1_name);
        }
        const char *picked = (tag == TAG_DP0) ? db->dp0 : db->dp1;
        aot_emit_fmt(b, "%sTerm %s = %s;\n", indent, out, picked);
        return out;
      }

      case TAG_OP2: {
        u32 op = term_ext(term);
        u64 loc = term_val(term);
        Term x = book_read(loc + 0);
        Term y = book_read(loc + 1);
        const char *xv = aot_emit_term(b, x, bind, indent);
        const char *yv = aot_emit_term(b, y, bind, indent);
        // Build OP2 lazily; the wnf machine fires OP2-NUM-NUM when both
        // sides reduce.  Inlining the constant fold here would cut one
        // ITRS per op but adds emit complexity; defer to Phase 2.
        aot_emit_fmt(b,
            "%su64 %s_loc = heap_alloc(2);\n"
            "%sheap_set(%s_loc + 0, %s);\n"
            "%sheap_set(%s_loc + 1, %s);\n"
            "%sTerm %s = term_new(0, TAG_OP2, %s, %s_loc);\n",
            indent, out,
            indent, out, xv,
            indent, out, yv,
            indent, out, aot_op_id(op), out);
        return out;
      }

      case TAG_APP: {
        u64 loc = term_val(term);
        Term fun = book_read(loc + 0);
        Term arg = book_read(loc + 1);
        const char *fv = aot_emit_term(b, fun, bind, indent);
        const char *av = aot_emit_term(b, arg, bind, indent);
        aot_emit_fmt(b,
            "%sTerm %s = aot_new_app(%s, %s);\n",
            indent, out, fv, av);
        return out;
      }

      case TAG_LAM: {
        // Bare LAM (not eta-applied at this level).  Emit as a
        // partial-application fallback -- the runtime will see the
        // unconsumed LAM and figure it out.  This shouldn't appear in
        // practice for well-formed defs whose body is a function;
        // the outer LAMs get peeled at the def-entry, and inner LAMs
        // here are bodies of MAT arms which already get APP'd by
        // MAT-CTR-MAT destructure.
        aot_emit_fmt(b,
            "%s/* unsupported: bare TLam in expression position */\n"
            "%sTerm %s = aot_fallback(def_slot);\n",
            indent, indent, out);
        return out;
      }

      default: {
        aot_emit_fmt(b,
            "%s/* unsupported tag %u in expression position */\n"
            "%sTerm %s = aot_fallback(def_slot);\n",
            indent, tag, indent, out);
        return out;
      }
    }
}

// Walk a MAT chain `MAT[ext0, h0, MAT[ext1, h1, fb]]` applied to
// `arg_var` (already forced to head form).  Each arm emits an `if`
// that returns the handler's result.  The bottom of the chain falls
// back via `aot_fallback`.
static const char *aot_emit_mat_chain(AotEmitBuf *b, Term mat,
                                      const char *arg_var,
                                      AotBindings *bind, const char *indent) {
    while (term_tag(mat) == TAG_MAT) {
        u32 match = term_ext(mat);
        u64 loc   = term_val(mat);
        Term handler = book_read(loc + 0);
        Term fb      = book_read(loc + 1);

        char inner_indent[64];
        snprintf(inner_indent, sizeof inner_indent, "%s  ", indent);

        // NUM arm: `if (tag == NUM && val == match) return handler;`
        aot_emit_fmt(b,
            "%sif (term_tag(%s) == TAG_NUM && (u32)term_val(%s) == %u) {\n",
            indent, arg_var, arg_var, match);
        aot_emit_fmt(b, "%s  aot_itrs_inc(1);\n", indent);
        const char *hv_num = aot_emit_term(b, handler, bind, inner_indent);
        aot_emit_fmt(b, "%s  return %s;\n%s}\n", indent, hv_num, indent);

        // CTR arm: `if (tag == CTR && ext == match)`.  Three
        // shapes for the handler:
        //   1. TLam-chain (the common case): peel each binder
        //      and bind it to term_ctr_at(arg, i).  After peeling,
        //      emit the body inline.
        //   2. TMat-chain (nested case-split, e.g. fib's outer
        //      SUC arm whose handler is itself a MAT on the SUC's
        //      destructured field): recurse into emit_mat_chain
        //      with the destructured field as the new dispatch
        //      target.  Skips one APP-MAT entry the runtime would
        //      have done.
        //   3. Other (e.g. a constant CTR like SUC{ZER}): emit
        //      handler then wrap remaining children in APPs.
        aot_emit_fmt(b,
            "%sif (term_tag(%s) == TAG_CTR && term_ext(%s) == %u) {\n",
            indent, arg_var, arg_var, match);
        aot_emit_fmt(b, "%s  aot_itrs_inc(1);\n", indent);

        // Peel as many TLam binders as the handler offers, binding
        // each one to a `term_ctr_at(arg, i)` lookup.  Stop when we
        // either run out of LAMs or reach a non-LAM body.
        Term cursor = handler;
        u32 peel_count = 0;
        while (term_tag(cursor) == TAG_LAM) {
            u64 lam_loc = term_val(cursor);
            char child_name[24];
            u32 idx = aot_fresh(bind);
            snprintf(child_name, sizeof child_name, "ctr_%u_c%u", idx, peel_count);
            aot_emit_fmt(b,
                "%s  Term %s = term_ctr_at(%s, %u);\n",
                indent, child_name, arg_var, peel_count);
            aot_bind_push(bind, lam_loc, child_name);
            cursor = book_read(lam_loc);
            peel_count++;
            if (peel_count >= 16) break;
        }

        // Body is a NESTED MAT? Recurse into the MAT-chain
        // dispatcher with a destructured field as the new arg.
        if (term_tag(cursor) == TAG_MAT && peel_count == 0) {
            // The handler IS a MAT and we haven't consumed any
            // CTR children for it yet.  Pop one child as its
            // dispatch arg.  (This matches APP-MAT-CTR-MAT's
            // "apply handler to first child via APP-chain" rule
            // -- here we directly recurse on child[0].)
            u32 idx = aot_fresh(bind);
            char inner_arg[24];
            snprintf(inner_arg, sizeof inner_arg, "mat_arg_%u", idx);
            aot_emit_fmt(b,
                "%s  Term %s = aot_force(term_ctr_at(%s, 0));\n",
                indent, inner_arg, arg_var);
            // Remaining CTR children get wrapped in APPs after
            // the inner MAT's result lands.
            char nested_indent[64];
            snprintf(nested_indent, sizeof nested_indent, "%s  ", indent);
            aot_emit_mat_chain(b, cursor, inner_arg, bind, nested_indent);
            // emit_mat_chain emits its own `return`s, so we
            // close the if-block here.  Children beyond index 0
            // are dropped -- arity-2+ CTRs with MAT handlers
            // aren't handled in Phase 2.
            aot_emit_fmt(b, "%s}\n", indent);
            for (u32 i = 0; i < peel_count; i++) aot_bind_pop(bind);
            mat = fb;
            continue;
        }

        // Emit the (possibly LAM-peeled) body.
        const char *hv_ctr = aot_emit_term(b, cursor, bind, inner_indent);

        // Pop our peeled bindings before continuing the chain.
        for (u32 i = 0; i < peel_count; i++) aot_bind_pop(bind);

        // If we couldn't peel for every CTR child, wrap the
        // remaining children in APPs the runtime will fire as
        // APP-LAM.  (Common when the def declared FEWER LAMs than
        // the matched constructor's arity.)
        aot_emit_fmt(b,
            "%s  Term ctr_res = %s;\n"
            "%s  u32 ctr_n = term_ctr_n(%s);\n"
            "%s  for (u32 i = %u; i < ctr_n; i++) {\n"
            "%s    ctr_res = aot_new_app(ctr_res, term_ctr_at(%s, i));\n"
            "%s  }\n"
            "%s  return ctr_res;\n"
            "%s}\n",
            indent, hv_ctr,
            indent, arg_var,
            indent, peel_count,
            indent, arg_var,
            indent,
            indent,
            indent);

        mat = fb;
    }

    // Default handler.  If it's a TLam, peel the binder and recurse
    // on the body; otherwise emit `APP(handler, arg)` for the runtime
    // to dispatch.
    if (term_tag(mat) == TAG_LAM) {
        u64 lam_loc = term_val(mat);
        Term body = book_read(lam_loc);
        aot_emit_fmt(b, "%saot_itrs_inc(2);  /* MIS chain depth + APP-LAM */\n", indent);
        aot_bind_push(bind, lam_loc, arg_var);
        const char *rv = aot_emit_term(b, body, bind, indent);
        aot_emit_fmt(b, "%sreturn %s;\n", indent, rv);
        aot_bind_pop(bind);
        return rv;
    }
    // Non-LAM default: emit APP(default, arg) and let wnf dispatch.
    const char *fbv = aot_emit_term(b, mat, bind, indent);
    aot_emit_fmt(b,
        "%saot_itrs_inc(2);  /* MIS chain depth */\n"
        "%sreturn aot_new_app(%s, %s);\n",
        indent, indent, fbv, arg_var);
    return arg_var;
}

// === Top-level def emit ==============================================

// `arity` = number of leading TLam binders in the def body.  We pop
// that many APP frames from the spine before evaluating the body.

static int aot_emit_def_body(AotEmitBuf *b, Term root, AotBindings *bind) {
    // Peel outer LAMs first to get the def's natural arity.  For
    // TMatNum'd defs with NO outer LAM (the body is itself a MAT
    // chain), the MAT atom takes its dispatch input as an implicit
    // single APP arg -- treat it the same as a single-LAM def.
    Term cursor = root;
    while (term_tag(cursor) == TAG_LAM) {
        u64 lam_loc = term_val(cursor);
        u32 idx = aot_fresh(bind);
        char arg_name[24];
        snprintf(arg_name, sizeof arg_name, "arg_%u", idx);
        aot_emit_fmt(b,
            "  Term %s = aot_pop_app_arg(stack, sp, base);\n"
            "  if (%s == 0) return aot_fallback(def_slot);\n"
            "  aot_itrs_inc(1);  /* APP-LAM */\n",
            arg_name, arg_name);
        aot_bind_push(bind, lam_loc, arg_name);
        cursor = book_read(lam_loc);
    }

    if (term_tag(cursor) == TAG_MAT) {
        // The MAT dispatches on its applied arg.  If we already
        // peeled a LAM, the first binding is the MAT input.
        // Otherwise pop a fresh arg here -- the def is a "naked MAT"
        // (the common case for numeric u32_fib-style defs).
        const char *arg_var;
        char fresh_name[24];
        if (bind->n > 0) {
            arg_var = bind->entries[0].name;
        } else {
            u32 idx = aot_fresh(bind);
            snprintf(fresh_name, sizeof fresh_name, "arg_%u", idx);
            aot_emit_fmt(b,
                "  Term %s = aot_pop_app_arg(stack, sp, base);\n"
                "  if (%s == 0) return aot_fallback(def_slot);\n",
                fresh_name, fresh_name);
            arg_var = fresh_name;
        }
        aot_emit_fmt(b, "  Term %s_w = aot_force(%s);\n",
                     arg_var, arg_var);
        char w_var[32];
        snprintf(w_var, sizeof w_var, "%s_w", arg_var);
        aot_emit_mat_chain(b, cursor, w_var, bind, "  ");
        return 1;
    }

    // Plain expression body.
    const char *rv = aot_emit_term(b, cursor, bind, "  ");
    aot_emit_fmt(b, "  return %s;\n", rv);
    return 1;
}

// === Public entry ====================================================

fn char *thvm_aot_emit_def(u32 def_id) {
    if (def_id >= DEFS_CAP) return NULL;
    Term root = DEFS[def_id];
    if (root == 0) return NULL;

    AotEmitBuf b;
    aot_emit_init(&b);
    if (!b.buf) return NULL;

    AotBindings bind;
    aot_bind_init(&bind);

    aot_emit_str(&b,
        "// aot/emitted/<def_id>.c -- generated by thvm_aot_emit_def.\n"
        "// Phase 1: TNum / TVar / TLam / TApp / TRef / TOp2 / TMat\n"
        "// chain.  Drop under src/aot/programs/ and #include in src/thvm.c.\n"
        "\n");

    aot_emit_fmt(&b, "static u32 G_EMIT_DEF_SLOT_%u = (u32)-1;\n", def_id);
    aot_emit_fmt(&b, "#define def_slot G_EMIT_DEF_SLOT_%u\n\n", def_id);

    aot_emit_fmt(&b,
        "static Term aot_emitted_%u(Term *stack, u32 *sp, u32 base) {\n",
        def_id);
    aot_emit_def_body(&b, root, &bind);
    aot_emit_str(&b, "}\n\n");

    aot_emit_fmt(&b,
        "fn void aot_program_emit_%u_register(u32 slot) {\n"
        "  def_slot = slot;\n"
        "  aot_register(slot, aot_emitted_%u);\n"
        "}\n"
        "#undef def_slot\n",
        def_id, def_id);

    return b.buf;
}
