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

#define AOT_MSL_BIND_CAP 16

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
static u32 g_msl_self_def_id = 0;
static int g_msl_emit_failed = 0;

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
          snprintf(out, 80, "0u /* unbound */");
          return out;
        }
        snprintf(out, 80, "uint(msl_term_val(%s))", bound);
        return out;
      }
      case TAG_OP2: {
        u32 op = term_ext(t);
        u64 loc = term_val(t);
        Term x = book_read(loc + 0);
        Term y = book_read(loc + 1);
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
          g_msl_emit_failed = 1;
          snprintf(out, 80, "0u /* APP head not REF (tag=%u) */",
                   term_tag(cursor));
          return out;
        }
        u32 callee_id = term_ext(cursor);
        if (callee_id == g_msl_self_def_id) {
          g_msl_emit_failed = 1;
          snprintf(out, 80, "0u /* recursive REF -- not supported */");
          return out;
        }
        if (callee_id >= DEFS_CAP || DEFS[callee_id] == 0) {
          g_msl_emit_failed = 1;
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
          g_msl_emit_failed = 1;
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

    // Preamble: Term encoding mirrored from src/thvm.h, helpers.
    aot_emit_fmt(&b,
        "// auto-generated by thvm_aot_metal_emit(\"%s\") def_id=%u arity=%u\n"
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "#define TAG_SHIFT  56\n"
        "#define EXT_SHIFT  38\n"
        "#define TAG_MASK   0x7FUL\n"
        "#define EXT_MASK   0x3FFFFUL\n"
        "#define VAL_MASK   0x3FFFFFFFFFUL\n"
        "#define TAG_NUM    10u\n"
        "\n"
        "typedef ulong Term;\n"
        "static inline ulong msl_term_val(Term t) { return t & VAL_MASK; }\n"
        "static inline uint  msl_term_ext(Term t) {\n"
        "  return uint((t >> EXT_SHIFT) & EXT_MASK);\n"
        "}\n"
        "static inline Term  msl_term_new(uint tag, uint ext, ulong val) {\n"
        "  return ((ulong(tag) & TAG_MASK) << TAG_SHIFT)\n"
        "       | ((ulong(ext) & EXT_MASK) << EXT_SHIFT)\n"
        "       | ( val        & VAL_MASK);\n"
        "}\n"
        "\n"
        "kernel void aot_def_%s(\n"
        "    device   Term         *heap      [[buffer(0)]],\n"
        "    device   Term         *args      [[buffer(1)]],\n"
        "    device   Term         *result    [[buffer(2)]],\n"
        "    device   atomic_uint  *book_next [[buffer(3)]],\n"
        "    uint                   tid       [[thread_position_in_grid]])\n"
        "{\n"
        "  (void)heap; (void)book_next;\n"
        "  if (tid != 0) return;\n",
        name, def_id, argc, name);

    g_msl_fresh = 0;
    g_msl_self_def_id = def_id;
    g_msl_emit_failed = 0;

    // Iter F: detect App(MAT-chain, TVar) shape -- numeric switch
    // dispatch.  After TLam-peel, the body might be
    //   App(MAT[v0, [h0, MAT[v1, [h1, ... fallback]]]], TVar(arg))
    // We walk the MAT chain, collect (match_val, handler) pairs, and
    // emit MSL chained conditionals.  Each handler + the fallback is
    // emitted via aot_msl_emit_uint (TNum / TVar / TOp2 fold).
    int emitted_mat = 0;
    if (term_tag(cursor) == TAG_APP) {
        u64  app_loc  = term_val(cursor);
        Term mat_head = book_read(app_loc + 0);
        Term arg      = book_read(app_loc + 1);
        if (term_tag(mat_head) == TAG_MAT && term_tag(arg) == TAG_VAR) {
            const char *arg_name = aot_msl_bind_lookup(&bind, term_val(arg));
            if (arg_name != NULL) {
                aot_emit_fmt(&b,
                    "  uint scrutinee = uint(msl_term_val(%s));\n"
                    "  uint result_val;\n",
                    arg_name);
                Term mc = mat_head;
                int  first = 1;
                while (term_tag(mc) == TAG_MAT) {
                    u32  match_val = term_ext(mc);
                    u64  ml        = term_val(mc);
                    Term handler   = book_read(ml + 0);
                    Term fallback  = book_read(ml + 1);
                    const char *hv = aot_msl_emit_uint(&b, handler, &bind);
                    aot_emit_fmt(&b,
                        "  %sif (scrutinee == %uu) result_val = %s;\n",
                        first ? "" : "else ", match_val, hv);
                    first = 0;
                    mc = fallback;
                }
                // Default arm: emit the fallback expression (after the
                // MAT chain runs out).  Same TNum/TVar/TOp2 vocabulary.
                const char *fv = aot_msl_emit_uint(&b, mc, &bind);
                aot_emit_fmt(&b,
                    "  else result_val = %s;\n",
                    fv);
                aot_emit_fmt(&b,
                    "  result[0] = msl_term_new(TAG_NUM, 0, ulong(result_val));\n"
                    "}\n");
                emitted_mat = 1;
            }
        }
    }

    if (!emitted_mat) {
        // Plain value-expression body: emit directly + wrap as NUM.
        const char *rv = aot_msl_emit_uint(&b, cursor, &bind);
        aot_emit_fmt(&b,
            "  result[0] = msl_term_new(TAG_NUM, 0, ulong(%s));\n"
            "}\n",
            rv);
    }

    if (g_msl_emit_failed) {
        // Iter G: an unsupported shape (recursion, partial app, etc.)
        // tripped the failure flag during recursive emit.  Discard the
        // partial source so the caller can fall back / report failure.
        free(b.buf);
        return NULL;
    }

    return b.buf;
}
