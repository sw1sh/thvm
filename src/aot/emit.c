// src/aot/emit.c -- C emitter: one def's book body becomes CPS
// (continuation-passing style) code for the ahead-of-time task runtime:
// FN_<name>, par_<name>_entry, a par_<name>_cont_K join per fork, and
// aot_program_<name>_dispatch / _register.  Only self-calls become tasks (two
// siblings under a CTR2 or OP2 fork; a tail call is a CALL); others are terms.

#ifndef THVM_AOT_EMIT_INCLUDED
#define THVM_AOT_EMIT_INCLUDED

#include <stdarg.h>

#define AOT_EMIT_INITIAL_CAP   (1u << 14)

typedef struct {
  char *buf;
  u32   len;
  u32   cap;
} AotEmit;

static void aot_emit_init(AotEmit *e) {
  e->cap = AOT_EMIT_INITIAL_CAP;
  e->len = 0;
  e->buf = (char *)malloc(e->cap);
  if (e->buf == NULL) thvm_fatal("aot emit: out of memory");
  e->buf[0] = '\0';
}

static void aot_emit_grow(AotEmit *e, u32 need) {
  while (e->cap - e->len < need) {
    u32 ncap = e->cap * 2;
    char *nb = (char *)realloc(e->buf, ncap);
    if (nb == NULL) thvm_fatal("aot emit: out of memory");
    e->buf = nb;
    e->cap = ncap;
  }
}

static void aot_emit_str(AotEmit *e, const char *s) {
  u32 n = (u32)strlen(s);
  aot_emit_grow(e, n + 1);
  memcpy(e->buf + e->len, s, n);
  e->len += n;
  e->buf[e->len] = '\0';
}

static void aot_emit_fmt(AotEmit *e, const char *fmt, ...) {
  // Sized by a first vsnprintf pass, so no fragment is truncated.
  va_list ap, ap2;
  va_start(ap, fmt);
  va_copy(ap2, ap);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (n < 0) { va_end(ap2); return; }
  char *tmp = (char *)malloc((size_t)n + 1);
  if (tmp == NULL) thvm_fatal("aot emit: out of memory");
  vsnprintf(tmp, (size_t)n + 1, fmt, ap2);
  va_end(ap2);
  aot_emit_str(e, tmp);
  free(tmp);
}

static void aot_cg_atomic_term(AotEmit *e, const char *out_var,
                                const char *indent,
                                const char *tag_macro, u32 ext, u64 val) {
  aot_emit_fmt(e,
    "%sTerm %s = term_new(0, %s, %u, %lluu);\n",
    indent, out_var, tag_macro, ext, (unsigned long long)val);
}

// One-cell node; the cell is declared as <out_var>_loc.
static void aot_cg_heap_term1(AotEmit *e, const char *out_var,
                               const char *indent,
                               const char *tag_macro, u32 ext,
                               const char *child0) {
  aot_emit_fmt(e,
    "%su64 %s_loc = aot_heap_alloc(1);\n"
    "%sheap_set(%s_loc, %s);\n"
    "%sTerm %s = term_new(0, %s, %u, %s_loc);\n",
    indent, out_var,
    indent, out_var, child0,
    indent, out_var, tag_macro, ext, out_var);
}

// Two-cell node, likewise at <out_var>_loc.
static void aot_cg_heap_term2(AotEmit *e, const char *out_var,
                               const char *indent,
                               const char *tag_macro, u32 ext,
                               const char *child0, const char *child1) {
  aot_emit_fmt(e,
    "%su64 %s_loc = aot_heap_alloc(2);\n"
    "%sheap_set(%s_loc + 0, %s);\n"
    "%sheap_set(%s_loc + 1, %s);\n"
    "%sTerm %s = term_new(0, %s, %u, %s_loc);\n",
    indent, out_var,
    indent, out_var, child0,
    indent, out_var, child1,
    indent, out_var, tag_macro, ext, out_var);
}

// DP projection of a dup cell already emitted (found in the dup memo).
static void aot_cg_dp_share(AotEmit *e, const char *out_var,
                             const char *indent,
                             const char *tag_macro, u32 label,
                             const char *cell_name) {
  aot_emit_fmt(e,
    "%sTerm %s = term_new(0, %s, %u, %s);\n",
    indent, out_var, tag_macro, label, cell_name);
}

// Source dup_loc -> C name of its runtime cell, so DP0 and DP1 of one dup share
// it.  Past the cap each projection gets its own cell and copy of the body.
#define AOT_DUP_MEMO_CAP  32

typedef struct {
  u64  src_loc;        // source dup_loc (book heap loc of the dup)
  char var_name[24];   // runtime C var name (e.g., "dup_3_loc")
} AotDupMemoEntry;

// Conts are emitted mid-walk as top-level functions, hence their own buffer,
// stitched in by thvm_aot_emit_program.
typedef struct {
  AotEmit         *body;         // entry function body
  AotEmit         *cont_defs;    // par_<name>_cont_K function defs
  u32              cont_count;   // next K to assign
  u32              self_id;
  u32              self_arity;
  const char      *self_name;
  AotDupMemoEntry  dup_memo[AOT_DUP_MEMO_CAP];
  u32              dup_memo_n;
  u32              dup_fresh;    // monotonic counter for dup_K names
} AotProgState;

// The memoed cell name for src_loc, or NULL.
static const char *aot_dup_memo_find(AotProgState *st, u64 src_loc) {
  for (u32 i = 0; i < st->dup_memo_n; i++) {
    if (st->dup_memo[i].src_loc == src_loc) {
      return st->dup_memo[i].var_name;
    }
  }
  return NULL;
}

static void aot_dup_memo_push(AotProgState *st, u64 src_loc,
                              const char *var_name) {
  if (st->dup_memo_n >= AOT_DUP_MEMO_CAP) return;
  st->dup_memo[st->dup_memo_n].src_loc = src_loc;
  snprintf(st->dup_memo[st->dup_memo_n].var_name,
           sizeof st->dup_memo[st->dup_memo_n].var_name,
           "%s", var_name);
  st->dup_memo_n++;
}

// A binder's lam_loc -> the C expression a VAR to it inlines, such as
// t->args[K], term_ctr_at(dv, K) or a value LAM's term_new(0, TAG_VAR, ...).
#define AOT_BIND_CAP  32

typedef struct {
  u64  lam_loc;
  char name[64];
} AotBinding;

typedef struct {
  AotBinding entries[AOT_BIND_CAP];
  u32        n;
} AotBindings;

static void aot_bind_init(AotBindings *b) { b->n = 0; }

static void aot_bind_push(AotBindings *b, u64 lam_loc, const char *name) {
  if (b->n >= AOT_BIND_CAP) return;
  b->entries[b->n].lam_loc = lam_loc;
  snprintf(b->entries[b->n].name, sizeof(b->entries[b->n].name),
           "%s", name);
  b->n++;
}

// The innermost binding wins; NULL (an unbound loc) is an emit-time bug.
static const char *aot_bind_lookup(AotBindings *b, u64 lam_loc) {
  for (u32 i = b->n; i > 0; i--) {
    if (b->entries[i - 1].lam_loc == lam_loc) {
      return b->entries[i - 1].name;
    }
  }
  return NULL;
}

// Binds each outer LAM to t->args[K]; returns the body, the count in *out_argc.
static Term aot_peel_lams(Term root, u32 *out_argc, AotBindings *b) {
  Term cursor = root;
  u32 n = 0;
  while (term_tag(cursor) == TAG_LAM) {
    char nm[24];
    snprintf(nm, sizeof nm, "t->args[%u]", n);
    aot_bind_push(b, term_val(cursor), nm);
    cursor = book_read(term_val(cursor));
    n++;
  }
  *out_argc = n;
  return cursor;
}

static u32 aot_value_fresh  = 0;
static u32 aot_nested_fresh = 0;   // the dvN names of nested dispatches

// The program being emitted, for the DP case's dup memo: a static like
// aot_value_fresh rather than one more parameter on every value-expr call.
static AotProgState *aot_current_st = NULL;

static const char *aot_emit_value_expr(AotEmit *e, Term term,
                                       AotBindings *bind,
                                       const char *indent);

// Emits the declarations that rebuild a book term at run time and returns its
// C expression in out_buf (a v_K temp or an inline expression); unsupported
// tags become ERA with a marker comment.
static const char *aot_emit_value_expr_named(AotEmit *e, Term term,
                                             AotBindings *bind,
                                             const char *indent,
                                             char *out_buf, u32 out_cap) {
  u32 idx = aot_value_fresh++;
  snprintf(out_buf, out_cap, "v_%u", idx);

  u8 tag = term_tag(term);
  switch (tag) {
    case TAG_NUM: {
      aot_cg_atomic_term(e, out_buf, indent, "TAG_NUM",
                          term_ext(term), term_val(term));
      return out_buf;
    }
    case TAG_VAR: {
      // The bound name is itself a C expression: return it, no temp.
      u64 loc = term_val(term);
      const char *bound = aot_bind_lookup(bind, loc);
      if (bound != NULL) {
        snprintf(out_buf, out_cap, "%s", bound);
      } else {
        aot_emit_fmt(e,
          "%sTerm %s = term_new(0, TAG_ERA, 0, 0);"
                "  /* unbound TVar(%llu) -- emit-time bug */\n",
          indent, out_buf, (unsigned long long)loc);
      }
      return out_buf;
    }
    case TAG_REF: {
      aot_cg_atomic_term(e, out_buf, indent, "TAG_REF", term_ext(term), 0);
      return out_buf;
    }
    case TAG_ERA: {
      snprintf(out_buf, out_cap, "term_new(0, TAG_ERA, 0, 0)");
      return out_buf;
    }
    case TAG_DP0:
    case TAG_DP1: {
      u64 dup_loc = term_val(term);
      u32 label   = term_ext(term);
      const char *tag_macro = (tag == TAG_DP0) ? "TAG_DP0" : "TAG_DP1";
      const char *cell_name = (aot_current_st != NULL)
          ? aot_dup_memo_find(aot_current_st, dup_loc) : NULL;
      if (cell_name != NULL) {
        aot_cg_dp_share(e, out_buf, indent, tag_macro, label, cell_name);
        return out_buf;
      }
      // First projection: the cell is named dup_K_loc, not
      // aot_cg_heap_term1's <out_var>_loc, so the sibling can find it.
      Term body = book_read(dup_loc);
      char body_buf[64];
      const char *body_var = aot_emit_value_expr_named(
          e, body, bind, indent, body_buf, sizeof body_buf);
      char cell_buf[24];
      if (aot_current_st != NULL) {
        snprintf(cell_buf, sizeof cell_buf, "dup_%u_loc",
                 aot_current_st->dup_fresh++);
        aot_dup_memo_push(aot_current_st, dup_loc, cell_buf);
      } else {
        snprintf(cell_buf, sizeof cell_buf, "%s_loc", out_buf);
      }
      aot_emit_fmt(e,
        "%su64 %s = aot_heap_alloc(1);\n"
        "%sheap_set(%s, %s);\n"
        "%sTerm %s = term_new(0, %s, %u, %s);\n",
        indent, cell_buf,
        indent, cell_buf, body_var,
        indent, out_buf, tag_macro, label, cell_buf);
      return out_buf;
    }
    case TAG_CTR: {
      u32 label = term_ext(term);
      u64 loc   = term_val(term);
      Term n_cell = book_read(loc);
      u32  n      = (term_tag(n_cell) == TAG_NUM) ? (u32)term_val(n_cell) : 0;
      if (n == 0) {
        aot_emit_fmt(e, "%sTerm %s = aot_make_ctr0(%u);\n",
                     indent, out_buf, label);
        return out_buf;
      }
      // Each child first, as it may emit statements of its own; 64 bytes holds
      // any value expression, ERA's term_new included.
      char (*kid_buf)[64] = (char (*)[64])malloc((size_t)n * sizeof *kid_buf);
      const char **kid = (const char **)malloc((size_t)n * sizeof *kid);
      if (kid_buf == NULL || kid == NULL) thvm_fatal("aot emit: out of memory");
      for (u32 i = 0; i < n; i++) {
        kid[i] = aot_emit_value_expr_named(e, book_read(loc + 1 + i), bind,
                                           indent, kid_buf[i], sizeof kid_buf[i]);
      }
      if (n <= 4) {
        aot_emit_fmt(e, "%sTerm %s = aot_make_ctr%u(%u", indent, out_buf, n, label);
        for (u32 i = 0; i < n; i++) aot_emit_fmt(e, ", %s", kid[i]);
        aot_emit_fmt(e, ");\n");
      } else {
        u32 arr = aot_value_fresh++;
        aot_emit_fmt(e, "%sTerm __ctr_kids_%u[%u] = {", indent, arr, n);
        for (u32 i = 0; i < n; i++) aot_emit_fmt(e, "%s%s", i == 0 ? "" : ", ", kid[i]);
        aot_emit_fmt(e, "};\n%sTerm %s = aot_make_ctrn(%u, %u, __ctr_kids_%u);\n",
                     indent, out_buf, label, n, arr);
      }
      free(kid);
      free(kid_buf);
      return out_buf;
    }
    case TAG_OP2: {
      // Built unevaluated: an argument like fib's n - 1 is forced (cnf) by
      // the callee's dispatch before its tag tests.
      u32 op  = term_ext(term);
      u64 loc = term_val(term);
      char b0[64], b1[64];
      const char *lv = aot_emit_value_expr_named(
          e, book_read(loc + 0), bind, indent, b0, sizeof b0);
      const char *rv = aot_emit_value_expr_named(
          e, book_read(loc + 1), bind, indent, b1, sizeof b1);
      aot_cg_heap_term2(e, out_buf, indent, "TAG_OP2", op, lv, rv);
      return out_buf;
    }
    case TAG_PRI: {
      // The dylib's run entries register the core prims in its own PRIM_TABLE.
      aot_cg_atomic_term(e, out_buf, indent, "TAG_PRI", term_ext(term), 0);
      return out_buf;
    }
    case TAG_ANY:
    case TAG_FVR: {
      aot_cg_atomic_term(e, out_buf, indent,
                          tag == TAG_ANY ? "TAG_ANY" : "TAG_FVR",
                          term_ext(term), 0);
      return out_buf;
    }
    case TAG_INC: {
      u64 loc = term_val(term);
      char b0[64];
      const char *bv = aot_emit_value_expr_named(
          e, book_read(loc), bind, indent, b0, sizeof b0);
      aot_cg_heap_term1(e, out_buf, indent, "TAG_INC", 0, bv);
      return out_buf;
    }
    case TAG_AND:
    case TAG_OR:
    case TAG_EQL:
    case TAG_WHEN: {
      u64 loc = term_val(term);
      char b0[64], b1[64];
      const char *av = aot_emit_value_expr_named(
          e, book_read(loc + 0), bind, indent, b0, sizeof b0);
      const char *bv = aot_emit_value_expr_named(
          e, book_read(loc + 1), bind, indent, b1, sizeof b1);
      const char *tn = (tag == TAG_AND ? "TAG_AND"
                       : tag == TAG_OR  ? "TAG_OR"
                       : tag == TAG_EQL ? "TAG_EQL"
                       :                  "TAG_WHEN");
      aot_cg_heap_term2(e, out_buf, indent, tn, 0, av, bv);
      return out_buf;
    }
    case TAG_SUP: {
      u32 label = term_ext(term);
      u64 loc   = term_val(term);
      char b0[64], b1[64];
      const char *av = aot_emit_value_expr_named(
          e, book_read(loc + 0), bind, indent, b0, sizeof b0);
      const char *bv = aot_emit_value_expr_named(
          e, book_read(loc + 1), bind, indent, b1, sizeof b1);
      aot_cg_heap_term2(e, out_buf, indent, "TAG_SUP", label, av, bv);
      return out_buf;
    }
    case TAG_LAM: {
      // Value-position LAM: its cell is allocated and bound before the body
      // is emitted, so the body's VARs can name it; heap_set comes after.
      u64 lam_loc = term_val(term);
      Term body   = book_read(lam_loc);
      u32 lam_k   = aot_value_fresh++;
      char loc_name[24];
      snprintf(loc_name, sizeof loc_name, "lam_%u_loc", lam_k);
      char var_expr[64];
      snprintf(var_expr, sizeof var_expr,
               "term_new(0, TAG_VAR, 0, %s)", loc_name);
      aot_emit_fmt(e,
        "%su64 %s = aot_heap_alloc(1);\n",
        indent, loc_name);
      u32 saved_bind_n = bind->n;
      aot_bind_push(bind, lam_loc, var_expr);
      char body_buf[64];
      const char *bv = aot_emit_value_expr_named(
          e, body, bind, indent, body_buf, sizeof body_buf);
      bind->n = saved_bind_n;
      aot_emit_fmt(e,
        "%sheap_set(%s, %s);\n"
        "%sTerm %s = term_new(0, TAG_LAM, 0, %s);\n",
        indent, loc_name, bv,
        indent, out_buf, loc_name);
      return out_buf;
    }
    case TAG_APP: {
      // An APP no dispatch or self-call shape took: built for the runtime.
      u64 loc = term_val(term);
      char b0[64], b1[64];
      const char *fv = aot_emit_value_expr_named(
          e, book_read(loc + 0), bind, indent, b0, sizeof b0);
      const char *av = aot_emit_value_expr_named(
          e, book_read(loc + 1), bind, indent, b1, sizeof b1);
      aot_cg_heap_term2(e, out_buf, indent, "TAG_APP", 0, fv, av);
      return out_buf;
    }
    default: {
      aot_emit_fmt(e,
        "%sTerm %s = term_new(0, TAG_ERA, 0, 0);"
              "  /* unsupported tag %u in value position */\n",
        indent, out_buf, tag);
      return out_buf;
    }
  }
}

// The result lives in a 256-slot static ring: valid for the next 255 calls.
static const char *aot_emit_value_expr(AotEmit *e, Term term,
                                       AotBindings *bind,
                                       const char *indent) {
  static char ring[256][64];
  static u32  ring_idx = 0;
  char *slot = ring[(ring_idx++) & 0xFF];
  return aot_emit_value_expr_named(e, term, bind, indent, slot, 64);
}

#define AOT_MAX_CALL_ARGS  AOT_MAX_ARGS

// Matches a saturated self-call App(..App(REF self, a_0).., a_{N-1}) with
// N == self_arity: returns N with out_args in call order, else 0.
static u32 aot_match_self_call(Term term, u32 self_id, u32 self_arity,
                               Term *out_args) {
  if (self_arity == 0 || self_arity > AOT_MAX_CALL_ARGS) return 0;
  Term cursor = term;
  Term args_rev[AOT_MAX_CALL_ARGS];
  u32 n = 0;
  while (term_tag(cursor) == TAG_APP) {
    if (n >= self_arity) return 0;
    u64 loc = term_val(cursor);
    args_rev[n++] = book_read(loc + 1);
    cursor = book_read(loc + 0);
  }
  if (n != self_arity) return 0;
  if (term_tag(cursor) != TAG_REF) return 0;
  if (term_ext(cursor) != self_id) return 0;
  for (u32 i = 0; i < n; i++) out_args[i] = args_rev[n - 1 - i];
  return n;
}

// Fork shapes: CTR2(L, call, call) and OP2(op, call, call) with both children
// saturated self-calls.  Each matcher returns 1 and fills the label or op and
// both calls' args.
static u32 aot_match_split_ctr2(Term term, AotProgState *st,
                                u32 *out_label,
                                Term *out_call_0_args,
                                Term *out_call_1_args) {
  if (term_tag(term) != TAG_CTR) return 0;
  u32 label = term_ext(term);
  u64 loc   = term_val(term);
  Term n_cell = book_read(loc);
  if (term_tag(n_cell) != TAG_NUM) return 0;
  if ((u32)term_val(n_cell) != 2) return 0;

  Term c0 = book_read(loc + 1);
  Term c1 = book_read(loc + 2);

  if (aot_match_self_call(c0, st->self_id, st->self_arity,
                          out_call_0_args) != st->self_arity) return 0;
  if (aot_match_self_call(c1, st->self_id, st->self_arity,
                          out_call_1_args) != st->self_arity) return 0;

  *out_label = label;
  return 1;
}

// A cont rebuilding CTR2(label, child0, child1); returns its K.
static u32 aot_emit_ctr2_cont(AotProgState *st, u32 label) {
  u32 k = st->cont_count++;
  aot_emit_fmt(st->cont_defs,
    "static AotResult par_%s_cont_%u(AotProgram *p, AotTask *t) {\n"
    "  (void)p;\n"
    "  return aot_make_value(aot_make_ctr2(%uu, t->args[0], t->args[1]));\n"
    "}\n\n",
    st->self_name, k, label);
  return k;
}

static u32 aot_match_split_op2(Term term, AotProgState *st,
                               u32 *out_op,
                               Term *out_call_0_args,
                               Term *out_call_1_args) {
  if (term_tag(term) != TAG_OP2) return 0;
  u32 op  = term_ext(term);
  u64 loc = term_val(term);
  Term c0 = book_read(loc + 0);
  Term c1 = book_read(loc + 1);

  if (aot_match_self_call(c0, st->self_id, st->self_arity,
                          out_call_0_args) != st->self_arity) return 0;
  if (aot_match_self_call(c1, st->self_id, st->self_arity,
                          out_call_1_args) != st->self_arity) return 0;

  *out_op = op;
  return 1;
}

// The fold for op over lv / rv, chosen at emit time.
static const char *aot_op2_c_expr(u32 op) {
  switch (op) {
    case OP_ADD: return "lv + rv";
    case OP_SUB: return "lv - rv";
    case OP_MUL: return "lv * rv";
    case OP_EQ:  return "(lv == rv) ? 1u : 0u";
    case OP_LT:  return "(lv <  rv) ? 1u : 0u";
    default:     return "0u  /* unsupported OP2 op */";
  }
}

// A cont folding the two NUM results with op; returns its K.
static u32 aot_emit_op2_cont(AotProgState *st, u32 op) {
  u32 k = st->cont_count++;
  aot_emit_fmt(st->cont_defs,
    "static AotResult par_%s_cont_%u(AotProgram *p, AotTask *t) {\n"
    "  (void)p;\n"
    "  /* OP2 fold (op id %u) -- assumes both child results are NUM */\n"
    "  u32 lv = (u32)term_val(t->args[0]);\n"
    "  u32 rv = (u32)term_val(t->args[1]);\n"
    "  return aot_make_value(\n"
    "      term_new(0, TAG_NUM, term_ext(t->args[0]), %s));\n"
    "}\n\n",
    st->self_name, k, op, aot_op2_c_expr(op));
  return k;
}

// SPLIT call site: allocate the cont cell, then two FN_<self> tasks returning
// into its slots 0 and 1.
static void aot_emit_split_call(AotProgState *st, u32 cont_k,
                                Term *call_0_args, Term *call_1_args,
                                AotBindings *bind, const char *indent) {
  // An arg Term both calls share is emitted once.
  const char *a0_names[AOT_MAX_CALL_ARGS];
  const char *a1_names[AOT_MAX_CALL_ARGS];
  for (u32 i = 0; i < st->self_arity; i++) {
    a0_names[i] = aot_emit_value_expr(st->body, call_0_args[i],
                                       bind, indent);
  }
  for (u32 i = 0; i < st->self_arity; i++) {
    if (call_0_args[i] == call_1_args[i]) {
      a1_names[i] = a0_names[i];
      continue;
    }
    a1_names[i] = aot_emit_value_expr(st->body, call_1_args[i],
                                       bind, indent);
  }

  aot_emit_fmt(st->body,
    "%su64 dp = aot_alloc_cont(CONT_%s_%u, 0, t->ret);\n",
    indent, st->self_name, cont_k);
  aot_emit_fmt(st->body,
    "%sreturn aot_make_split(\n"
    "%s    aot_make_task(FN_%s, aot_enc_ret((u32)dp, 0)",
    indent,
    indent, st->self_name);
  for (u32 i = 0; i < st->self_arity; i++) {
    aot_emit_fmt(st->body, ", %s", a0_names[i]);
  }
  for (u32 i = st->self_arity; i < AOT_MAX_ARGS; i++) {
    aot_emit_fmt(st->body, ", 0");
  }
  aot_emit_fmt(st->body, "),\n%s    aot_make_task(FN_%s, aot_enc_ret((u32)dp, 1)",
               indent, st->self_name);
  for (u32 i = 0; i < st->self_arity; i++) {
    aot_emit_fmt(st->body, ", %s", a1_names[i]);
  }
  for (u32 i = st->self_arity; i < AOT_MAX_ARGS; i++) {
    aot_emit_fmt(st->body, ", 0");
  }
  aot_emit_fmt(st->body, "));\n");
}

static u32 aot_emit_app_of_mat_named(AotProgState *st, Term cursor,
                                      AotBindings *bind,
                                      const char *dv_name,
                                      const char *indent);

// An arm body, tried in order: a CTR2 / OP2 fork (SPLIT and a cont), a
// saturated self-call (CALL), a nested App-of-Mat dispatch, else a value.
static void aot_emit_arm_handler(AotProgState *st, Term handler,
                                 const char *arg_var, u32 is_ctr_arm,
                                 AotBindings *bind, const char *indent) {
  Term cursor = handler;
  u32 saved_n = bind->n;
  // Each arm is its own C block, so a sibling arm's dup cells are out of
  // scope: start with an empty memo.  dup_fresh keeps cell names unique.
  u32 saved_dup_memo_n = st->dup_memo_n;
  st->dup_memo_n = 0;

  // CTR arm (1): APP-MAT-CTR applies the handler to the fields, so each LAM
  // binds term_ctr_at(arg_var, i).  NUM arm (0): APP-MAT-NUM returns it as is.
  // Default (2): APP-MAT-MIS applies it to the scrutinee; deeper LAMs are body.
  u32 peel_idx = 0;
  if (is_ctr_arm == 1) {
    while (term_tag(cursor) == TAG_LAM) {
      char destr[64];
      snprintf(destr, sizeof destr,
               "term_ctr_at(%s, %u)", arg_var, peel_idx);
      aot_bind_push(bind, term_val(cursor), destr);
      cursor = book_read(term_val(cursor));
      peel_idx++;
    }
  } else if (is_ctr_arm == 2 && term_tag(cursor) == TAG_LAM) {
    aot_bind_push(bind, term_val(cursor), arg_var);
    cursor = book_read(term_val(cursor));
  }

  u32 ctr2_label;
  u32 op2_op;
  Term call_0_args[AOT_MAX_CALL_ARGS];
  Term call_1_args[AOT_MAX_CALL_ARGS];

  if (aot_match_split_ctr2(cursor, st, &ctr2_label,
                            call_0_args, call_1_args)) {
    u32 cont_k = aot_emit_ctr2_cont(st, ctr2_label);
    aot_emit_split_call(st, cont_k, call_0_args, call_1_args,
                         bind, indent);
    bind->n = saved_n;
    st->dup_memo_n = saved_dup_memo_n;
    return;
  }

  if (aot_match_split_op2(cursor, st, &op2_op,
                           call_0_args, call_1_args)) {
    u32 cont_k = aot_emit_op2_cont(st, op2_op);
    aot_emit_split_call(st, cont_k, call_0_args, call_1_args,
                         bind, indent);
    bind->n = saved_n;
    st->dup_memo_n = saved_dup_memo_n;
    return;
  }

  Term call_args[AOT_MAX_CALL_ARGS];
  u32 n = aot_match_self_call(cursor, st->self_id, st->self_arity,
                              call_args);
  if (n > 0) {
    const char *arg_names[AOT_MAX_CALL_ARGS];
    for (u32 i = 0; i < n; i++) {
      arg_names[i] = aot_emit_value_expr(st->body, call_args[i],
                                          bind, indent);
    }
    aot_emit_fmt(st->body,
      "%sreturn aot_make_call(aot_make_task(\n"
      "%s    FN_%s, t->ret",
      indent, indent, st->self_name);
    for (u32 i = 0; i < n; i++) {
      aot_emit_fmt(st->body, ", %s", arg_names[i]);
    }
    for (u32 i = n; i < AOT_MAX_ARGS; i++) {
      aot_emit_fmt(st->body, ", 0");
    }
    aot_emit_fmt(st->body, "));\n");
    bind->n = saved_n;
    st->dup_memo_n = saved_dup_memo_n;
    return;
  }

  // Nested dispatch, as in ack's default arm matching on n; a fresh dvN keeps
  // the outer dv visible.
  char dv2_name[16];
  snprintf(dv2_name, sizeof dv2_name, "dv%u", ++aot_nested_fresh);
  if (aot_emit_app_of_mat_named(st, cursor, bind, dv2_name, indent)) {
    bind->n = saved_n;
    st->dup_memo_n = saved_dup_memo_n;
    return;
  }

  // A value that may still reduce (a cross-def call, arithmetic, a REF) is
  // forced, so the parent cont or caller gets a NUM or CTR.
  const char *rv = aot_emit_value_expr(st->body, cursor, bind, indent);
  u8 cursor_tag = term_tag(cursor);
  _Bool needs_force = (cursor_tag == TAG_APP || cursor_tag == TAG_OP2 ||
                       cursor_tag == TAG_REF || cursor_tag == TAG_MAT ||
                       cursor_tag == TAG_DP0 || cursor_tag == TAG_DP1);
  if (needs_force) {
    aot_emit_fmt(st->body,
      "%sTerm rv = %s;\n"
      "%sif (term_tag(rv) != TAG_CTR && term_tag(rv) != TAG_NUM) rv = cnf(rv);\n"
      "%sreturn aot_make_value(rv);\n",
      indent, rv, indent, indent);
  } else {
    aot_emit_fmt(st->body, "%sreturn aot_make_value(%s);\n", indent, rv);
  }

  bind->n = saved_n;
  st->dup_memo_n = saved_dup_memo_n;
}

// 1 when some arm's handler is two or more nested LAMs, which only a CTR with
// several fields destructures; assuming a match never mixes NUM and CTR arms,
// the NUM tests are then skipped.  One LAM may be a returned function.
static u32 aot_chain_is_ctr_only(Term mat) {
  Term cursor = mat;
  while (term_tag(cursor) == TAG_MAT) {
    Term handler = book_read(term_val(cursor) + 0);
    if (term_tag(handler) == TAG_LAM) {
      Term inner = book_read(term_val(handler));
      if (term_tag(inner) == TAG_LAM) return 1;
    }
    cursor = book_read(term_val(cursor) + 1);
  }
  return 0;
}

// An if-chain on arg_var over a MAT chain (src/term/new_mat.c: ext the match
// value, cells [handler, fallback]); the last fallback is the default arm.
static void aot_emit_mat_chain(AotProgState *st, Term mat,
                               const char *arg_var,
                               AotBindings *bind, const char *indent) {
  char inner_indent[64];
  snprintf(inner_indent, sizeof inner_indent, "%s  ", indent);

  u32 ctr_only = aot_chain_is_ctr_only(mat);
  if (ctr_only) {
    aot_emit_fmt(st->body,
      "%s/* dead-arm pruned: chain is CTR-only (some arm destructures) */\n",
      indent);
  }

  Term cursor = mat;
  while (term_tag(cursor) == TAG_MAT) {
    u32 match    = term_ext(cursor);
    u64 loc      = term_val(cursor);
    Term handler = book_read(loc + 0);
    Term fb      = book_read(loc + 1);

    if (!ctr_only) {
      aot_emit_fmt(st->body,
        "%sif (term_tag(%s) == TAG_NUM && (u32)term_val(%s) == %uu) {\n",
        indent, arg_var, arg_var, match);
      aot_emit_arm_handler(st, handler, arg_var, /*is_ctr_arm=*/0,
                            bind, inner_indent);
      aot_emit_fmt(st->body, "%s}\n", indent);
    }

    aot_emit_fmt(st->body,
      "%sif (term_tag(%s) == TAG_CTR && term_ext(%s) == %uu) {\n",
      indent, arg_var, arg_var, match);
    aot_emit_arm_handler(st, handler, arg_var, /*is_ctr_arm=*/1,
                          bind, inner_indent);
    aot_emit_fmt(st->body, "%s}\n", indent);

    cursor = fb;
  }

  aot_emit_fmt(st->body, "%s/* default arm */\n", indent);
  aot_emit_arm_handler(st, cursor, arg_var, /*kind=default*/2,
                        bind, indent);
}

// Dispatch on App(MAT chain, arg); returns 0, having emitted nothing, for any
// other shape.  A nested dispatch passes a fresh dv_name.
static u32 aot_emit_app_of_mat_named(AotProgState *st, Term cursor,
                                      AotBindings *bind,
                                      const char *dv_name,
                                      const char *indent) {
  if (term_tag(cursor) != TAG_APP) return 0;
  u64 app_loc = term_val(cursor);
  Term fun = book_read(app_loc + 0);
  Term arg = book_read(app_loc + 1);
  if (term_tag(fun) != TAG_MAT) return 0;

  // A bound VAR is used by name; any other arg is emitted as a value first.
  char arg_name_buf[64];
  const char *arg_name = NULL;
  if (term_tag(arg) == TAG_VAR) {
    arg_name = aot_bind_lookup(bind, term_val(arg));
  }
  if (arg_name == NULL) {
    arg_name = aot_emit_value_expr_named(
        st->body, arg, bind, indent,
        arg_name_buf, sizeof arg_name_buf);
  }

  aot_emit_fmt(st->body, "%s/* dispatch on %s */\n", indent, arg_name);
  // The arms test only NUM and CTR tags, so anything else is forced (cnf).
  aot_emit_fmt(st->body,
    "%sTerm %s = %s;\n"
    "%sif (term_tag(%s) != TAG_CTR && term_tag(%s) != TAG_NUM) %s = cnf(%s);\n",
    indent, dv_name, arg_name,
    indent, dv_name, dv_name, dv_name, dv_name);
  aot_emit_mat_chain(st, fun, dv_name, bind, indent);
  return 1;
}

static u32 aot_emit_app_of_mat(AotProgState *st, Term cursor,
                               AotBindings *bind) {
  return aot_emit_app_of_mat_named(st, cursor, bind, "dv", "  ");
}

// Head tags the value fallback treats as reduced, emitting no wnf.
static _Bool aot_tag_is_whnf(u8 tag) {
  switch (tag) {
    case TAG_NUM:
    case TAG_ERA:
    case TAG_LAM:
    case TAG_SUP:
    case TAG_CTR:
    case TAG_TEN:
    case TAG_FVR:
    case TAG_ANY:
    case TAG_VAR:
    case TAG_DP0:
    case TAG_DP1:
      return 1;
    default:
      return 0;
  }
}

#define AOT_PRI_SCAN_MAX_DEPTH  32

// 1 if a PRI occurs within AOT_PRI_SCAN_MAX_DEPTH levels, or past what the scan
// can see.  Callers fire PRIs via TWnf when they choose, so the fallback leaves
// such a body unreduced.
static _Bool aot_body_contains_pri(Term term, u32 depth) {
  if (depth > AOT_PRI_SCAN_MAX_DEPTH) return 1;
  u8  tag = term_tag(term);
  u64 loc = term_val(term);
  switch (tag) {
    case TAG_PRI:
      return 1;

    // Heap[loc..loc+1] = [a, b]
    case TAG_APP:
    case TAG_OP2:
    case TAG_MAT:
    case TAG_EQL:
    case TAG_AND:
    case TAG_OR:
    case TAG_ANN:
    case TAG_WHEN:
    case TAG_SUP: {
      Term a = book_read(loc + 0);
      Term b = book_read(loc + 1);
      return aot_body_contains_pri(a, depth + 1) ||
             aot_body_contains_pri(b, depth + 1);
    }

    // Heap[loc] = body
    case TAG_LAM:
    case TAG_INC:
    case TAG_DUP:
    case TAG_BRI:
      return aot_body_contains_pri(book_read(loc), depth + 1);

    // Heap[loc] = NUM(n); Heap[loc+1..loc+n] = children
    case TAG_CTR: {
      Term n_cell = book_read(loc);
      if (term_tag(n_cell) != TAG_NUM) return 0;
      u32 n = (u32)term_val(n_cell);
      if (n > 64) return 1;  // past the scan's cap
      for (u32 i = 0; i < n; i++) {
        if (aot_body_contains_pri(book_read(loc + 1 + i), depth + 1))
          return 1;
      }
      return 0;
    }

    // Atoms, and tags not scanned (DP0 / DP1, ALO, UOP, ...).
    default:
      return 0;
  }
}

// The program's C source (caller frees), or NULL.  It has no #includes: it is
// compiled after the runtime (src/thvm.c, or build.c's embedded copy).
fn char *thvm_aot_emit_program(u32 def_id, const char *name) {
  if (def_id >= DEFS_CAP) return NULL;
  Term root = DEFS[def_id];
  if (root == 0) return NULL;

  // v_K and dvN names restart per program, so equal defs emit equal source and
  // hit build.c's content-hash cache.
  aot_value_fresh  = 0;
  aot_nested_fresh = 0;

  AotEmit body_buf, cont_buf, final_buf;
  aot_emit_init(&body_buf);
  aot_emit_init(&cont_buf);
  aot_emit_init(&final_buf);
  if (!body_buf.buf || !cont_buf.buf || !final_buf.buf) return NULL;

  AotBindings bind;
  aot_bind_init(&bind);

  AotProgState st = {
    .body       = &body_buf,
    .cont_defs  = &cont_buf,
    .cont_count = 0,
    .self_id    = def_id,
    .self_arity = 0,
    .self_name  = name,
  };
  aot_current_st = &st;

  u32  argc;
  Term body = aot_peel_lams(root, &argc, &bind);
  st.self_arity = argc;

  if (!aot_emit_app_of_mat(&st, body, &bind)) {
    // Not a dispatch (a PRI redex, a CTR, arithmetic, ...): return the value,
    // in weak head normal form for TAOTRun unless it holds a PRI or already is.
    const char *rv = aot_emit_value_expr(st.body, body, &bind, "  ");
    _Bool needs_force = !aot_body_contains_pri(body, 0) &&
                        !aot_tag_is_whnf(term_tag(body));
    if (needs_force) {
      aot_emit_fmt(st.body,
        "  /* fallback: value expression body, auto-reduce */\n"
        "  return aot_make_value(wnf(%s));\n", rv);
    } else {
      aot_emit_fmt(st.body,
        "  /* fallback: value expression body */\n"
        "  return aot_make_value(%s);\n", rv);
    }
  }

  aot_emit_fmt(&final_buf,
    "// auto-generated by thvm_aot_emit_program(\"%s\")\n"
    "// def_id %u, arity %u, %u cont(s)\n"
    "//\n"
    "// This source is meant to be #include'd into a TU that has\n"
    "// already brought the thvm AOT runtime into scope (typically\n"
    "// via `#include \"src/thvm.c\"` -- which transitively pulls in\n"
    "// task.h / halloc.h / cont.c / resolve.c / worker.c).  The\n"
    "// emit deliberately omits #includes so the same string works\n"
    "// for both the in-tree path (test files) and the dylib path\n"
    "// (a generated wrapper file that includes thvm.c at the top).\n"
    "\n"
    "#define FN_%s  0u\n",
    name, def_id, argc, st.cont_count,
    name);

  // CONT ids start at 1; FN_<name> is 0.
  for (u32 k = 0; k < st.cont_count; k++) {
    aot_emit_fmt(&final_buf,
      "#define CONT_%s_%u  %uu\n", name, k, k + 1);
  }
  aot_emit_str(&final_buf, "\n");

  aot_emit_fmt(&final_buf,
    "static AotResult par_%s_entry(AotProgram *p, AotTask *t);\n",
    name);
  for (u32 k = 0; k < st.cont_count; k++) {
    aot_emit_fmt(&final_buf,
      "static AotResult par_%s_cont_%u(AotProgram *p, AotTask *t);\n",
      name, k);
  }
  aot_emit_str(&final_buf, "\n");

  if (st.cont_count > 0) {
    aot_emit_str(&final_buf, cont_buf.buf);
  }

  aot_emit_fmt(&final_buf,
    "static AotResult par_%s_entry(AotProgram *p, AotTask *t) {\n"
    "  (void)p; (void)t;\n",
    name);
  aot_emit_str(&final_buf, body_buf.buf);
  aot_emit_str(&final_buf, "}\n\n");

  aot_emit_fmt(&final_buf,
    "static AotResult aot_program_%s_dispatch(AotProgram *p, AotTask *t) {\n"
    "  switch (t->fn_id) {\n"
    "    case FN_%s:  return par_%s_entry(p, t);\n",
    name, name, name);
  for (u32 k = 0; k < st.cont_count; k++) {
    aot_emit_fmt(&final_buf,
      "    case CONT_%s_%u:  return par_%s_cont_%u(p, t);\n",
      name, k, name, k);
  }
  aot_emit_fmt(&final_buf,
    "    default:     return aot_make_value(0);\n"
    "  }\n"
    "}\n\n"
    "fn void aot_program_%s_register(AotProgram *p) {\n"
    "  p->dispatch = aot_program_%s_dispatch;\n"
    "}\n",
    name, name);

  free(body_buf.buf);
  free(cont_buf.buf);
  aot_current_st = NULL;
  return final_buf.buf;
}

#endif  // THVM_AOT_EMIT_INCLUDED
