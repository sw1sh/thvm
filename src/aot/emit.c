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

typedef struct {
    AotBinding entries[AOT_BIND_CAP];
    u32 n;
    u32 fresh;           // monotonic counter for fresh variable names
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

        // Numeric arm: `if (tag == NUM && val == match)`.  We don't
        // emit a CTR arm here because the WL setup uses TMatNum
        // throughout; CTR matching reuses the same MAT atom.
        aot_emit_fmt(b,
            "%sif (term_tag(%s) == TAG_NUM && (u32)term_val(%s) == %u) {\n",
            indent, arg_var, arg_var, match);
        aot_emit_fmt(b, "%s  ITRS += 1;\n", indent);
        // For arity-0 handlers (no APP), return handler directly.
        // (TMatNum's "handler" is whatever WL passed to TMatNum.)
        char inner_indent[64];
        snprintf(inner_indent, sizeof inner_indent, "%s  ", indent);
        const char *hv = aot_emit_term(b, handler, bind, inner_indent);
        aot_emit_fmt(b, "%s  return %s;\n%s}\n", indent, hv, indent);

        // CTR arm: same as NUM arm but uses term_ext + destructure
        // (Phase 2: handle CTR destructure).  For now skip.

        // Step into fallback for the next arm.  If fb is itself a
        // TMatNum, continue the chain; otherwise it's the default
        // handler -- emit `APP(fb, arg)` and recurse.
        mat = fb;
    }

    // Default handler.  If it's a TLam, peel the binder and recurse
    // on the body; otherwise emit `APP(handler, arg)` for the runtime
    // to dispatch.
    if (term_tag(mat) == TAG_LAM) {
        u64 lam_loc = term_val(mat);
        Term body = book_read(lam_loc);
        aot_emit_fmt(b, "%sITRS += 2;  /* MIS chain depth + APP-LAM */\n", indent);
        aot_bind_push(bind, lam_loc, arg_var);
        const char *rv = aot_emit_term(b, body, bind, indent);
        aot_emit_fmt(b, "%sreturn %s;\n", indent, rv);
        aot_bind_pop(bind);
        return rv;
    }
    // Non-LAM default: emit APP(default, arg) and let wnf dispatch.
    const char *fbv = aot_emit_term(b, mat, bind, indent);
    aot_emit_fmt(b,
        "%sITRS += 2;  /* MIS chain depth */\n"
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
            "  ITRS += 1;  /* APP-LAM */\n",
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
