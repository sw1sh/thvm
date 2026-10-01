// ===================================================================
// --emit-ifeq10: a Horn problem in TPTP's -10 unit-equality encoding
// ===================================================================
//
// TPTP's [CS18] problems (the -10 versions, "Converted from X to UEQ
// using [CS18]") were produced by jukebox's horn-to-unit tool
// (Smallbone 2018) with --no-conjunctive-conjectures
// --no-disjunctive-conjectures.  This file ports that pipeline -- the
// jukebox clausifier, the Horn encoding and the output renaming -- so
// that a CNF or FOF Horn problem comes out as the -10 file TPTP ships:
//   * predicates become boolean-valued functions: p(t) = true;
//   * a Horn clause C | ~G1 | ... | ~Gk becomes one equation
//       ifeqN(gk_l, gk_r, ... ifeqN(g1_l, g1_r, c_l, c_r) ..., c_r) = c_r
//     with G1 < ... < Gk in jukebox's atom order, and one dispatcher
//     axiom ifeqN(A,A,B,C) = B per (condition type, result type) pair,
//     named ifeq, ifeq2, ... from the last pair in that order;
//   * one negative clause is the goal (several literals become
//     tuple(...) != tuple(...)); with two or more, each gets the head
//     a = b and the goal is a != b over fresh tokens a and b;
//   * clause names, clause order, Skolem names sK<n>_<formula>_<var>
//     and variable names are jukebox's.
//
// Atom order depends on symbol order, and jukebox orders symbols by
// intern number.  The build that made TPTP's files interned LAZILY: a
// symbol got its number the first time a comparison forced it -- in
// containers-0.4 Data.Set operations, which compare the inserted
// element first and, in an intersection, split the larger set by the
// other set's root -- and the first fresh name the clausifier made (a
// Skolem function, a definition predicate, a renamed bound variable)
// interned every remaining symbol in post-order over the input.  The
// port replays exactly those comparisons: SEU323-10, PLA023-10 and
// SYN143-10 each pin a part of this order that first-occurrence order
// gets wrong.
//
// Deviations: jukebox's one-element-model check (it answers Satisfiable
// instead of encoding) runs here on Horn problems only, so a non-Horn
// problem is always rejected; tree rotations past five elements follow
// containers-0.4.2 (delta 3, ratio 2).

#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  I10_RC_OK      = 0,
  I10_RC_INPUT   = 2,
  I10_RC_NONHORN = 3,
  I10_RC_SAT1    = 4
};

#define I10_MAX_INCLUDE_DEPTH 8
#define I10_COST_CAP ((long long)1 << 60)

// --- arena, vectors, strings ------------------------------------------

typedef struct I10Chunk {
  struct I10Chunk *next;
  size_t used;
  size_t cap;
  unsigned char *data;
} I10Chunk;

typedef struct I10Vec {
  void **v;
  int n;
  int cap;
} I10Vec;

static I10Chunk *g_i10_arena = NULL;
static jmp_buf g_i10_jb;
static int g_i10_rc;
static char g_i10_msg[1024];

static _Noreturn void i10_fail(int rc, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(g_i10_msg, sizeof g_i10_msg, fmt, ap);
  va_end(ap);
  g_i10_rc = rc;
  longjmp(g_i10_jb, 1);
}

static void i10_arena_free(void) {
  while (g_i10_arena != NULL) {
    I10Chunk *next = g_i10_arena->next;
    free(g_i10_arena);
    g_i10_arena = next;
  }
}

static void *i10_alloc(size_t n) {
  n = (n + 15u) & ~(size_t)15u;
  if (n == 0) {
    n = 16;
  }
  I10Chunk *c = g_i10_arena;
  if (c == NULL || c->cap - c->used < n) {
    size_t cap = (size_t)1 << 20;
    if (n > cap) {
      cap = n;
    }
    c = (I10Chunk *)malloc(sizeof(I10Chunk) + cap + 16u);
    if (c == NULL) {
      i10_fail(I10_RC_INPUT, "out of memory");
    }
    c->next = g_i10_arena;
    c->used = 0;
    c->cap = cap;
    c->data = (unsigned char *)(((uintptr_t)(c + 1) + 15u) & ~(uintptr_t)15u);
    g_i10_arena = c;
  }
  void *p = c->data + c->used;
  c->used += n;
  memset(p, 0, n);
  return p;
}

static void i10_push(I10Vec *a, void *x) {
  if (a->n == a->cap) {
    int cap = (a->cap == 0) ? 8 : 2 * a->cap;
    void **nv = (void **)i10_alloc(sizeof(void *) * (size_t)cap);
    if (a->n > 0) {
      memcpy(nv, a->v, sizeof(void *) * (size_t)a->n);
    }
    a->v = nv;
    a->cap = cap;
  }
  a->v[a->n++] = x;
}

static char *i10_strn(const char *s, size_t n) {
  char *d = (char *)i10_alloc(n + 1u);
  memcpy(d, s, n);
  d[n] = '\0';
  return d;
}

static char *i10_fmt(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (n < 0) {
    i10_fail(I10_RC_INPUT, "cannot format a name");
  }
  char *s = (char *)i10_alloc((size_t)n + 1u);
  va_start(ap, fmt);
  vsnprintf(s, (size_t)n + 1u, fmt, ap);
  va_end(ap);
  return s;
}

typedef struct I10StrSet {
  const char **slot;
  int cap;
  int n;
} I10StrSet;

static unsigned long i10_hash(const char *s) {
  unsigned long h = 5381u;
  for (; *s != '\0'; s++) {
    h = h * 33u + (unsigned char)*s;
  }
  return h;
}

static int i10_ss_has(const I10StrSet *s, const char *k) {
  if (s->cap == 0) {
    return 0;
  }
  unsigned long m = (unsigned long)s->cap - 1u;
  for (unsigned long i = i10_hash(k) & m; s->slot[i] != NULL; i = (i + 1u) & m) {
    if (strcmp(s->slot[i], k) == 0) {
      return 1;
    }
  }
  return 0;
}

static void i10_ss_add(I10StrSet *s, const char *k) {
  if (i10_ss_has(s, k)) {
    return;
  }
  if (2 * (s->n + 1) > s->cap) {
    int cap = (s->cap == 0) ? 64 : 2 * s->cap;
    const char **slot = (const char **)i10_alloc(sizeof(char *) * (size_t)cap);
    for (int i = 0; i < s->cap; i++) {
      if (s->slot[i] != NULL) {
        unsigned long j = i10_hash(s->slot[i]) & (unsigned long)(cap - 1);
        while (slot[j] != NULL) {
          j = (j + 1u) & (unsigned long)(cap - 1);
        }
        slot[j] = s->slot[i];
      }
    }
    s->slot = slot;
    s->cap = cap;
  }
  unsigned long j = i10_hash(k) & (unsigned long)(s->cap - 1);
  while (s->slot[j] != NULL) {
    j = (j + 1u) & (unsigned long)(s->cap - 1);
  }
  s->slot[j] = k;
  s->n++;
}

typedef struct I10StrMap {
  const char **key;
  int *val;
  int cap;
  int n;
} I10StrMap;

// The value slot of k (0 when k is new).
static int *i10_sm_slot(I10StrMap *m, const char *k) {
  if (2 * (m->n + 1) > m->cap) {
    int cap = (m->cap == 0) ? 64 : 2 * m->cap;
    const char **key = (const char **)i10_alloc(sizeof(char *) * (size_t)cap);
    int *val = (int *)i10_alloc(sizeof(int) * (size_t)cap);
    for (int i = 0; i < m->cap; i++) {
      if (m->key[i] != NULL) {
        unsigned long j = i10_hash(m->key[i]) & (unsigned long)(cap - 1);
        while (key[j] != NULL) {
          j = (j + 1u) & (unsigned long)(cap - 1);
        }
        key[j] = m->key[i];
        val[j] = m->val[i];
      }
    }
    m->key = key;
    m->val = val;
    m->cap = cap;
  }
  unsigned long msk = (unsigned long)m->cap - 1u;
  unsigned long j = i10_hash(k) & msk;
  while (m->key[j] != NULL) {
    if (strcmp(m->key[j], k) == 0) {
      return &m->val[j];
    }
    j = (j + 1u) & msk;
  }
  m->key[j] = k;
  m->n++;
  return &m->val[j];
}

// --- names, symbols, terms, formulas ----------------------------------

// Jukebox's Name: Fixed (a parsed symbol; ordered by intern number,
// assigned on first use), Unique (a variable or a generated symbol;
// ordered by creation), Variant (a base name with argument names: the
// ifeq and tuple families).  Fixed < Unique < Variant.
enum { I10_FIXED = 0, I10_UNIQUE = 1, I10_VARIANT = 2 };

typedef struct I10Name I10Name;
struct I10Name {
  int kind;
  long long id;
  const char *base;
  I10Name *vbase;
  I10Name **vargs;
  int nvargs;
  const char *sk_prefix;
  const char *sk_tag;
  const char *pretty;
};

typedef struct I10Fun I10Fun;
struct I10Fun {
  I10Name *name;
  int arity;
  int pred;
  I10Name *res;
  I10Fun *boolfun;
  int truth;
};

typedef struct I10Term I10Term;
struct I10Term {
  I10Name *var;
  I10Fun *fun;
  I10Term **args;
  int nargs;
};

typedef struct I10Atom {
  int eq;
  I10Term *l;
  I10Term *r;
} I10Atom;

typedef struct I10Lit {
  int neg;
  I10Atom *a;
} I10Lit;

typedef struct I10VSet {
  I10Name **v;
  int n;
} I10VSet;

enum { I10F_LIT, I10F_NOT, I10F_AND, I10F_OR, I10F_EQUIV, I10F_ALL, I10F_EX, I10F_CONN };
enum { I10C_IMPLIES, I10C_FOLLOWS, I10C_XOR, I10C_NOR, I10C_NAND };

typedef struct I10Form I10Form;
struct I10Form {
  int kind;
  int conn;
  I10Lit *lit;
  I10Form **k;
  int n;
  I10VSet vs;
  int fv_ok;
  I10VSet fv;
};

typedef struct I10Input {
  const char *tag;
  const char *role;
  int conj;
  I10Form *f;
} I10Input;

// A clause: a literal list with its name and printed role.
typedef struct I10Clause {
  const char *tag;
  const char *role;
  I10Lit **l;
  int n;
} I10Clause;

typedef struct I10FunTab {
  I10Fun **slot;
  int cap;
  int n;
} I10FunTab;

static I10Name g_i10_ty_i = {I10_FIXED, -1, "$i", NULL, NULL, 0, NULL, NULL, NULL};
static long long g_i10_interned;
static long long g_i10_counter;
static int g_i10_bulk_done;
static int g_i10_ndefs;
static const char *g_i10_tag;
static I10Vec g_i10_inputs;
static I10FunTab g_i10_funs;

static void i10_force(I10Name *n) {
  if (n->kind == I10_FIXED && n->id == 0) {
    n->id = ++g_i10_interned;
  }
}

static I10Name *i10_name_fixed(const char *s) {
  I10Name *n = (I10Name *)i10_alloc(sizeof(I10Name));
  n->kind = I10_FIXED;
  n->base = s;
  return n;
}

static I10Name *i10_name_var(const char *base, long long id) {
  I10Name *n = (I10Name *)i10_alloc(sizeof(I10Name));
  n->kind = I10_UNIQUE;
  n->base = base;
  n->id = id;
  return n;
}

static I10Name *i10_name_variant(I10Name *vbase, I10Name **vargs, int nvargs) {
  I10Name *n = (I10Name *)i10_alloc(sizeof(I10Name));
  n->kind = I10_VARIANT;
  n->base = vbase->base;
  n->vbase = vbase;
  n->nvargs = nvargs;
  n->vargs = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(nvargs > 0 ? nvargs : 1));
  for (int i = 0; i < nvargs; i++) {
    n->vargs[i] = vargs[i];
  }
  return n;
}

static void i10_bulk_form(I10Form *f);

// NameM.newName.  Its first call forces the supply's initial value,
// which is computed from the names of the whole input: the moment the
// lazily built binary interned every symbol not yet compared.
static I10Name *i10_new_name(const char *base) {
  if (!g_i10_bulk_done) {
    g_i10_bulk_done = 1;
    for (int i = 0; i < g_i10_inputs.n; i++) {
      i10_bulk_form(((I10Input *)g_i10_inputs.v[i])->f);
    }
  }
  return i10_name_var(base, ++g_i10_counter);
}

static I10Fun *i10_fun_new(I10Name *name, int arity, int pred, I10Name *res) {
  I10Fun *f = (I10Fun *)i10_alloc(sizeof(I10Fun));
  f->name = name;
  f->arity = arity;
  f->pred = pred;
  f->res = res;
  return f;
}

static I10Fun *i10_symbol(const char *s, int arity, int pred) {
  I10FunTab *t = &g_i10_funs;
  if (2 * (t->n + 1) > t->cap) {
    int cap = (t->cap == 0) ? 256 : 2 * t->cap;
    I10Fun **slot = (I10Fun **)i10_alloc(sizeof(I10Fun *) * (size_t)cap);
    for (int i = 0; i < t->cap; i++) {
      if (t->slot[i] != NULL) {
        unsigned long j = i10_hash(t->slot[i]->name->base) & (unsigned long)(cap - 1);
        while (slot[j] != NULL) {
          j = (j + 1u) & (unsigned long)(cap - 1);
        }
        slot[j] = t->slot[i];
      }
    }
    t->slot = slot;
    t->cap = cap;
  }
  unsigned long m = (unsigned long)t->cap - 1u;
  unsigned long j = i10_hash(s) & m;
  while (t->slot[j] != NULL) {
    I10Fun *f = t->slot[j];
    if (strcmp(f->name->base, s) == 0) {
      if (f->arity != arity || f->pred != pred) {
        i10_fail(I10_RC_INPUT, "symbol '%s' is used with two arities or as both "
                 "predicate and function", s);
      }
      return f;
    }
    j = (j + 1u) & m;
  }
  I10Fun *f = i10_fun_new(i10_name_fixed(s), arity, pred, pred ? NULL : &g_i10_ty_i);
  t->slot[j] = f;
  t->n++;
  return f;
}

static I10Term *i10_var_term(I10Name *v) {
  I10Term *t = (I10Term *)i10_alloc(sizeof(I10Term));
  t->var = v;
  return t;
}

static I10Term *i10_app(I10Fun *f, I10Term **args, int n) {
  I10Term *t = (I10Term *)i10_alloc(sizeof(I10Term));
  t->fun = f;
  t->nargs = n;
  t->args = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)(n > 0 ? n : 1));
  for (int i = 0; i < n; i++) {
    t->args[i] = args[i];
  }
  return t;
}

static I10Atom *i10_atom(int eq, I10Term *l, I10Term *r) {
  I10Atom *a = (I10Atom *)i10_alloc(sizeof(I10Atom));
  a->eq = eq;
  a->l = l;
  a->r = r;
  return a;
}

static I10Lit *i10_lit(int neg, I10Atom *a) {
  I10Lit *l = (I10Lit *)i10_alloc(sizeof(I10Lit));
  l->neg = neg;
  l->a = a;
  return l;
}

static void i10_bulk_term(I10Term *t) {
  if (t->var != NULL) {
    return;
  }
  for (int i = 0; i < t->nargs; i++) {
    i10_bulk_term(t->args[i]);
  }
  i10_force(t->fun->name);
}

static void i10_bulk_form(I10Form *f) {
  if (f->kind == I10F_LIT) {
    i10_bulk_term(f->lit->a->l);
    if (f->lit->a->eq) {
      i10_bulk_term(f->lit->a->r);
    }
    return;
  }
  for (int i = 0; i < f->n; i++) {
    i10_bulk_form(f->k[i]);
  }
}

// --- variable sets (ascending by id) ------------------------------------

static int i10_idcmp(const void *a, const void *b) {
  long long x = (*(I10Name *const *)a)->id;
  long long y = (*(I10Name *const *)b)->id;
  return (x < y) ? -1 : (x > y) ? 1 : 0;
}

static I10VSet i10_vs_empty(void) {
  I10VSet s = {NULL, 0};
  return s;
}

static I10VSet i10_vs_of(I10Name **v, int n) {
  I10VSet s;
  s.v = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(n > 0 ? n : 1));
  s.n = 0;
  if (n == 0) {
    return s;
  }
  memcpy(s.v, v, sizeof(I10Name *) * (size_t)n);
  qsort(s.v, (size_t)n, sizeof(I10Name *), i10_idcmp);
  for (int i = 0; i < n; i++) {
    if (s.n == 0 || s.v[s.n - 1]->id != s.v[i]->id) {
      s.v[s.n++] = s.v[i];
    }
  }
  return s;
}

static int i10_vs_has(I10VSet s, const I10Name *x) {
  int lo = 0, hi = s.n - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (s.v[mid]->id == x->id) {
      return 1;
    }
    if (s.v[mid]->id < x->id) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return 0;
}

// op: 0 union, 1 intersection, 2 difference
static I10VSet i10_vs_merge(I10VSet a, I10VSet b, int op) {
  I10VSet s;
  s.v = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(a.n + b.n + 1));
  s.n = 0;
  int i = 0, j = 0;
  while (i < a.n || j < b.n) {
    if (j == b.n || (i < a.n && a.v[i]->id < b.v[j]->id)) {
      if (op != 1) {
        s.v[s.n++] = a.v[i];
      }
      i++;
    } else if (i == a.n || b.v[j]->id < a.v[i]->id) {
      if (op == 0) {
        s.v[s.n++] = b.v[j];
      }
      j++;
    } else {
      if (op != 2) {
        s.v[s.n++] = a.v[i];
      }
      i++;
      j++;
    }
  }
  return s;
}

static I10VSet i10_vs_union(I10VSet a, I10VSet b) {
  return i10_vs_merge(a, b, 0);
}

static I10VSet i10_vs_inter(I10VSet a, I10VSet b) {
  return i10_vs_merge(a, b, 1);
}

static I10VSet i10_vs_diff(I10VSet a, I10VSet b) {
  return i10_vs_merge(a, b, 2);
}

static void i10_term_vars(I10Term *t, I10Vec *acc) {
  if (t->var != NULL) {
    i10_push(acc, t->var);
    return;
  }
  for (int i = 0; i < t->nargs; i++) {
    i10_term_vars(t->args[i], acc);
  }
}

static void i10_lit_vars(I10Lit *l, I10Vec *acc) {
  i10_term_vars(l->a->l, acc);
  if (l->a->eq) {
    i10_term_vars(l->a->r, acc);
  }
}

static I10VSet i10_free(I10Form *f) {
  if (f->fv_ok) {
    return f->fv;
  }
  I10VSet s = i10_vs_empty();
  switch (f->kind) {
    case I10F_LIT: {
      I10Vec acc = {NULL, 0, 0};
      i10_lit_vars(f->lit, &acc);
      s = i10_vs_of((I10Name **)acc.v, acc.n);
      break;
    }
    case I10F_ALL:
    case I10F_EX:
      s = i10_vs_diff(i10_free(f->k[0]), f->vs);
      break;
    default:
      for (int i = 0; i < f->n; i++) {
        s = i10_vs_union(s, i10_free(f->k[i]));
      }
      break;
  }
  f->fv = s;
  f->fv_ok = 1;
  return s;
}

// --- formula constructors and Form.hs simplification --------------------

static I10Form *i10_fnew(int kind) {
  I10Form *f = (I10Form *)i10_alloc(sizeof(I10Form));
  f->kind = kind;
  return f;
}

static I10Form *i10_flit(I10Lit *l) {
  I10Form *f = i10_fnew(I10F_LIT);
  f->lit = l;
  return f;
}

static I10Form *i10_cat(int kind, I10Form **a, int na, I10Form **b, int nb) {
  I10Form *f = i10_fnew(kind);
  f->n = na + nb;
  f->k = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(na + nb + 1));
  for (int i = 0; i < na; i++) {
    f->k[i] = a[i];
  }
  for (int i = 0; i < nb; i++) {
    f->k[na + i] = b[i];
  }
  return f;
}

static I10Form *i10_flist(int kind, I10Form **k, int n) {
  return i10_cat(kind, k, n, NULL, 0);
}

static I10Form *i10_f1(int kind, I10Form *a) {
  return i10_cat(kind, &a, 1, NULL, 0);
}

static I10Form *i10_f2(int kind, int conn, I10Form *a, I10Form *b) {
  I10Form *f = i10_cat(kind, &a, 1, &b, 1);
  f->conn = conn;
  return f;
}

static I10Form *i10_fq(int kind, I10VSet vs, I10Form *body) {
  I10Form *f = i10_f1(kind, body);
  f->vs = vs;
  return f;
}

static I10Form *i10_ftrue(void) {
  return i10_flist(I10F_AND, NULL, 0);
}

static I10Form *i10_ffalse(void) {
  return i10_flist(I10F_OR, NULL, 0);
}

static int i10_is_true(const I10Form *f) {
  return f->kind == I10F_AND && f->n == 0;
}

static int i10_is_false(const I10Form *f) {
  return f->kind == I10F_OR && f->n == 0;
}

static I10Form *i10_and2(I10Form *a, I10Form *b) {
  if (a->kind == I10F_AND && b->kind == I10F_AND) {
    return i10_cat(I10F_AND, a->k, a->n, b->k, b->n);
  }
  if (i10_is_false(a) || i10_is_false(b)) {
    return i10_ffalse();
  }
  if (a->kind == I10F_AND) {
    return i10_cat(I10F_AND, &b, 1, a->k, a->n);
  }
  if (b->kind == I10F_AND) {
    return i10_cat(I10F_AND, &a, 1, b->k, b->n);
  }
  return i10_cat(I10F_AND, &a, 1, &b, 1);
}

static I10Form *i10_or2(I10Form *a, I10Form *b) {
  if (a->kind == I10F_OR && b->kind == I10F_OR) {
    return i10_cat(I10F_OR, a->k, a->n, b->k, b->n);
  }
  if (i10_is_true(a) || i10_is_true(b)) {
    return i10_ftrue();
  }
  if (a->kind == I10F_OR) {
    return i10_cat(I10F_OR, &b, 1, a->k, a->n);
  }
  if (b->kind == I10F_OR) {
    return i10_cat(I10F_OR, &a, 1, b->k, b->n);
  }
  return i10_cat(I10F_OR, &a, 1, &b, 1);
}

static I10Form *i10_nt(I10Form *a) {
  return (a->kind == I10F_NOT) ? a->k[0] : i10_f1(I10F_NOT, a);
}

static I10Form *i10_connective(int c, I10Form *t, I10Form *u) {
  switch (c) {
    case I10C_IMPLIES:
      return i10_or2(i10_nt(t), u);
    case I10C_FOLLOWS:
      return i10_or2(t, i10_nt(u));
    case I10C_XOR:
      return i10_nt(i10_f2(I10F_EQUIV, 0, t, u));
    case I10C_NOR:
      return i10_nt(i10_or2(t, u));
    default:
      return i10_nt(i10_and2(t, u));
  }
}

static I10Form *i10_not_inwards(I10Form *f);

static I10Form *i10_positive(I10Form *f) {
  switch (f->kind) {
    case I10F_NOT:
      return i10_not_inwards(f->k[0]);
    case I10F_CONN:
      return i10_positive(i10_connective(f->conn, f->k[0], f->k[1]));
    default:
      return f;
  }
}

static I10Form *i10_not_inwards(I10Form *f) {
  switch (f->kind) {
    case I10F_AND:
    case I10F_OR: {
      I10Form **k = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(f->n + 1));
      for (int i = 0; i < f->n; i++) {
        k[i] = i10_not_inwards(f->k[i]);
      }
      return i10_flist((f->kind == I10F_AND) ? I10F_OR : I10F_AND, k, f->n);
    }
    case I10F_EQUIV:
      return i10_f2(I10F_EQUIV, 0, i10_not_inwards(f->k[0]), f->k[1]);
    case I10F_NOT:
      return i10_positive(f->k[0]);
    case I10F_ALL:
      return i10_fq(I10F_EX, f->vs, i10_not_inwards(f->k[0]));
    case I10F_EX:
      return i10_fq(I10F_ALL, f->vs, i10_not_inwards(f->k[0]));
    case I10F_LIT:
      return i10_flit(i10_lit(!f->lit->neg, f->lit->a));
    default:
      return i10_not_inwards(i10_connective(f->conn, f->k[0], f->k[1]));
  }
}

static I10Form *i10_simple(I10Form *f) {
  switch (f->kind) {
    case I10F_OR: {
      I10Form **k = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(f->n + 1));
      for (int i = 0; i < f->n; i++) {
        k[i] = i10_nt(f->k[i]);
      }
      return i10_f1(I10F_NOT, i10_flist(I10F_AND, k, f->n));
    }
    case I10F_EX:
      return i10_f1(I10F_NOT, i10_fq(I10F_ALL, f->vs, i10_nt(f->k[0])));
    case I10F_CONN:
      return i10_simple(i10_connective(f->conn, f->k[0], f->k[1]));
    default:
      return f;
  }
}

static I10Form *i10_simplify(I10Form *f) {
  switch (f->kind) {
    case I10F_LIT:
      return f;
    case I10F_CONN:
      return i10_simplify(i10_connective(f->conn, f->k[0], f->k[1]));
    case I10F_NOT:
      return i10_simplify(i10_not_inwards(f->k[0]));
    case I10F_AND: {
      I10Form *acc = i10_ftrue();
      for (int i = f->n - 1; i >= 0; i--) {
        acc = i10_and2(i10_simplify(f->k[i]), acc);
      }
      return acc;
    }
    case I10F_OR: {
      I10Form *acc = i10_ffalse();
      for (int i = f->n - 1; i >= 0; i--) {
        acc = i10_or2(i10_simplify(f->k[i]), acc);
      }
      return acc;
    }
    case I10F_EQUIV: {
      I10Form *t = i10_simplify(f->k[0]);
      I10Form *u = i10_simplify(f->k[1]);
      if (i10_is_true(t)) {
        return u;
      }
      if (i10_is_true(u)) {
        return t;
      }
      if (i10_is_false(t)) {
        return i10_nt(u);
      }
      if (i10_is_false(u)) {
        return i10_nt(t);
      }
      return i10_f2(I10F_EQUIV, 0, t, u);
    }
    default: {
      I10Form *t = i10_simplify(f->k[0]);
      if (f->vs.n == 0) {
        return t;
      }
      if (t->kind == f->kind) {
        return i10_fq(f->kind, i10_vs_union(f->vs, t->vs), t->k[0]);
      }
      return i10_fq(f->kind, f->vs, t);
    }
  }
}

// --- substitution -------------------------------------------------------

typedef struct I10Subst {
  I10Name **from;
  I10Term **to;
  int n;
} I10Subst;

static I10Term *i10_subst_term(I10Term *t, const I10Subst *s) {
  if (t->var != NULL) {
    for (int i = s->n - 1; i >= 0; i--) {
      if (s->from[i]->id == t->var->id) {
        return s->to[i];
      }
    }
    return t;
  }
  I10Term **args = NULL;
  for (int i = 0; i < t->nargs; i++) {
    I10Term *a = i10_subst_term(t->args[i], s);
    if (a != t->args[i] && args == NULL) {
      args = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)t->nargs);
      for (int j = 0; j < i; j++) {
        args[j] = t->args[j];
      }
    }
    if (args != NULL) {
      args[i] = a;
    }
  }
  return (args == NULL) ? t : i10_app(t->fun, args, t->nargs);
}

static I10Lit *i10_subst_lit(I10Lit *l, const I10Subst *s) {
  I10Term *a = i10_subst_term(l->a->l, s);
  I10Term *b = l->a->eq ? i10_subst_term(l->a->r, s) : NULL;
  if (a == l->a->l && b == l->a->r) {
    return l;
  }
  return i10_lit(l->neg, i10_atom(l->a->eq, a, b));
}

static I10Form *i10_subst_form(I10Form *f, const I10Subst *s) {
  if (s->n == 0) {
    return f;
  }
  switch (f->kind) {
    case I10F_LIT: {
      I10Lit *l = i10_subst_lit(f->lit, s);
      return (l == f->lit) ? f : i10_flit(l);
    }
    case I10F_ALL:
    case I10F_EX: {
      I10Subst inner;
      inner.from = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(s->n + 1));
      inner.to = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)(s->n + 1));
      inner.n = 0;
      for (int i = 0; i < s->n; i++) {
        if (!i10_vs_has(f->vs, s->from[i])) {
          inner.from[inner.n] = s->from[i];
          inner.to[inner.n] = s->to[i];
          inner.n++;
        }
      }
      return i10_fq(f->kind, f->vs, i10_subst_form(f->k[0], &inner));
    }
    default: {
      I10Form *g = i10_flist(f->kind, f->k, f->n);
      g->conn = f->conn;
      for (int i = 0; i < f->n; i++) {
        g->k[i] = i10_subst_form(f->k[i], s);
      }
      return g;
    }
  }
}

// --- Haskell-faithful comparisons ---------------------------------------
//
// Each mirrors the derived (or explicit) Ord instance it ports,
// including the order in which it forces names: the left operand of
// every comparison is examined first, and a comparison stops at the
// first difference.

static int i10_cmp_ll(long long a, long long b) {
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

static int i10_cmp_name(I10Name *x, I10Name *y) {
  i10_force(x);
  i10_force(y);
  if (x->kind != y->kind) {
    return (x->kind < y->kind) ? -1 : 1;
  }
  if (x->kind != I10_VARIANT) {
    return i10_cmp_ll(x->id, y->id);
  }
  int c = i10_cmp_name(x->vbase, y->vbase);
  if (c != 0) {
    return c;
  }
  for (int i = 0;; i++) {
    if (i == x->nvargs || i == y->nvargs) {
      return i10_cmp_ll(x->nvargs - i, y->nvargs - i);
    }
    c = i10_cmp_name(x->vargs[i], y->vargs[i]);
    if (c != 0) {
      return c;
    }
  }
}

static int i10_cmp_term(I10Term *a, I10Term *b) {
  if (a->var != NULL || b->var != NULL) {
    if (a->var != NULL && b->var != NULL) {
      return i10_cmp_name(a->var, b->var);
    }
    return (a->var != NULL) ? -1 : 1;
  }
  int c = i10_cmp_name(a->fun->name, b->fun->name);
  if (c != 0) {
    return c;
  }
  for (int i = 0;; i++) {
    if (i == a->nargs || i == b->nargs) {
      return i10_cmp_ll(a->nargs - i, b->nargs - i);
    }
    c = i10_cmp_term(a->args[i], b->args[i]);
    if (c != 0) {
      return c;
    }
  }
}

static int i10_eq_term(I10Term *a, I10Term *b) {
  if (a->var != NULL || b->var != NULL) {
    return a->var != NULL && b->var != NULL && i10_cmp_name(a->var, b->var) == 0;
  }
  if (i10_cmp_name(a->fun->name, b->fun->name) != 0) {
    return 0;
  }
  for (int i = 0; i < a->nargs && i < b->nargs; i++) {
    if (!i10_eq_term(a->args[i], b->args[i])) {
      return 0;
    }
  }
  return a->nargs == b->nargs;
}

// Form.normAtomic: an equation as its sides in ascending order (Left),
// before every predicate atom (Right).
typedef struct I10Norm {
  int left;
  I10Term *x;
  I10Term *y;
} I10Norm;

static I10Norm i10_norm(I10Atom *a) {
  I10Norm n;
  n.left = a->eq;
  n.x = a->l;
  n.y = a->r;
  if (a->eq && i10_cmp_term(a->l, a->r) > 0) {
    n.x = a->r;
    n.y = a->l;
  }
  return n;
}

static int i10_cmp_atom(I10Atom *a, I10Atom *b) {
  I10Norm na = i10_norm(a);
  I10Norm nb = i10_norm(b);
  if (na.left != nb.left) {
    return na.left ? -1 : 1;
  }
  int c = i10_cmp_term(na.x, nb.x);
  if (c != 0 || !na.left) {
    return c;
  }
  return i10_cmp_term(na.y, nb.y);
}

static int i10_cmp_lit(I10Lit *a, I10Lit *b) {
  if (a->neg != b->neg) {
    return a->neg ? 1 : -1;
  }
  return i10_cmp_atom(a->a, b->a);
}

typedef struct I10LL {
  I10Lit **l;
  int n;
} I10LL;

static int i10_cmp_clause(I10LL *a, I10LL *b) {
  for (int i = 0;; i++) {
    if (i == a->n || i == b->n) {
      return i10_cmp_ll(a->n - i, b->n - i);
    }
    int c = i10_cmp_lit(a->l[i], b->l[i]);
    if (c != 0) {
      return c;
    }
  }
}

static int i10_cmp_atom_v(void *a, void *b) {
  return i10_cmp_atom((I10Atom *)a, (I10Atom *)b);
}

static int i10_cmp_clause_v(void *a, void *b) {
  return i10_cmp_clause((I10LL *)a, (I10LL *)b);
}

static int i10_cmp_name_v(void *a, void *b) {
  return i10_cmp_name((I10Name *)a, (I10Name *)b);
}

// --- containers-0.4 Data.Set --------------------------------------------
//
// Only the comparisons matter, but which elements meet depends on the
// tree shapes, so the balancing is ported too.

typedef struct I10Set I10Set;
struct I10Set {
  I10Set *l;
  I10Set *r;
  void *x;
  int sz;
};

typedef int (*I10Cmp)(void *, void *);

static int i10_ssize(const I10Set *t) {
  return (t == NULL) ? 0 : t->sz;
}

static I10Set *i10_sbin(void *x, I10Set *l, I10Set *r) {
  I10Set *t = (I10Set *)i10_alloc(sizeof(I10Set));
  t->x = x;
  t->l = l;
  t->r = r;
  t->sz = 1 + i10_ssize(l) + i10_ssize(r);
  return t;
}

static I10Set *i10_balance_l(void *x, I10Set *l, I10Set *r) {
  if (r == NULL) {
    if (l == NULL) {
      return i10_sbin(x, NULL, NULL);
    }
    if (l->l == NULL && l->r == NULL) {
      return i10_sbin(x, l, NULL);
    }
    if (l->l == NULL) {
      return i10_sbin(l->r->x, i10_sbin(l->x, NULL, NULL), i10_sbin(x, NULL, NULL));
    }
    if (l->r == NULL) {
      return i10_sbin(l->x, l->l, i10_sbin(x, NULL, NULL));
    }
    if (l->r->sz < 2 * l->l->sz) {
      return i10_sbin(l->x, l->l, i10_sbin(x, l->r, NULL));
    }
    return i10_sbin(l->r->x, i10_sbin(l->x, l->l, l->r->l), i10_sbin(x, l->r->r, NULL));
  }
  if (l == NULL) {
    return i10_sbin(x, NULL, r);
  }
  if (l->sz > 3 * r->sz) {
    if (l->r->sz < 2 * l->l->sz) {
      return i10_sbin(l->x, l->l, i10_sbin(x, l->r, r));
    }
    return i10_sbin(l->r->x, i10_sbin(l->x, l->l, l->r->l), i10_sbin(x, l->r->r, r));
  }
  return i10_sbin(x, l, r);
}

static I10Set *i10_balance_r(void *x, I10Set *l, I10Set *r) {
  if (l == NULL) {
    if (r == NULL) {
      return i10_sbin(x, NULL, NULL);
    }
    if (r->l == NULL && r->r == NULL) {
      return i10_sbin(x, NULL, r);
    }
    if (r->l == NULL) {
      return i10_sbin(r->x, i10_sbin(x, NULL, NULL), r->r);
    }
    if (r->r == NULL) {
      return i10_sbin(r->l->x, i10_sbin(x, NULL, NULL), i10_sbin(r->x, NULL, NULL));
    }
    if (r->l->sz < 2 * r->r->sz) {
      return i10_sbin(r->x, i10_sbin(x, NULL, r->l), r->r);
    }
    return i10_sbin(r->l->x, i10_sbin(x, NULL, r->l->l), i10_sbin(r->x, r->l->r, r->r));
  }
  if (r == NULL) {
    return i10_sbin(x, l, NULL);
  }
  if (r->sz > 3 * l->sz) {
    if (r->l->sz < 2 * r->r->sz) {
      return i10_sbin(r->x, i10_sbin(x, l, r->l), r->r);
    }
    return i10_sbin(r->l->x, i10_sbin(x, l, r->l->l), i10_sbin(r->x, r->l->r, r->r));
  }
  return i10_sbin(x, l, r);
}

// An equal element is replaced by the new one, as in containers-0.4.
static I10Set *i10_sinsert(I10Cmp cmp, void *x, I10Set *t) {
  if (t == NULL) {
    return i10_sbin(x, NULL, NULL);
  }
  int c = cmp(x, t->x);
  if (c < 0) {
    return i10_balance_l(t->x, i10_sinsert(cmp, x, t->l), t->r);
  }
  if (c > 0) {
    return i10_balance_r(t->x, t->l, i10_sinsert(cmp, x, t->r));
  }
  return i10_sbin(x, t->l, t->r);
}

static I10Set *i10_sfromlist(I10Cmp cmp, void **xs, int n) {
  I10Set *t = NULL;
  for (int i = 0; i < n; i++) {
    t = i10_sinsert(cmp, xs[i], t);
  }
  return t;
}

static void i10_stolist(const I10Set *t, I10Vec *out) {
  if (t == NULL) {
    return;
  }
  i10_stolist(t->l, out);
  i10_push(out, t->x);
  i10_stolist(t->r, out);
}

static I10Set *i10_sinsert_max(void *x, I10Set *t) {
  if (t == NULL) {
    return i10_sbin(x, NULL, NULL);
  }
  return i10_balance_r(t->x, t->l, i10_sinsert_max(x, t->r));
}

static I10Set *i10_sinsert_min(void *x, I10Set *t) {
  if (t == NULL) {
    return i10_sbin(x, NULL, NULL);
  }
  return i10_balance_l(t->x, i10_sinsert_min(x, t->l), t->r);
}

static I10Set *i10_slink(void *x, I10Set *l, I10Set *r) {
  if (l == NULL) {
    return i10_sinsert_min(x, r);
  }
  if (r == NULL) {
    return i10_sinsert_max(x, l);
  }
  if (3 * l->sz < r->sz) {
    return i10_balance_l(r->x, i10_slink(x, l, r->l), r->r);
  }
  if (3 * r->sz < l->sz) {
    return i10_balance_r(l->x, l->l, i10_slink(x, l->r, r));
  }
  return i10_sbin(x, l, r);
}

static I10Set *i10_sdel_min(I10Set *t, void **out) {
  if (t->l == NULL) {
    *out = t->x;
    return t->r;
  }
  I10Set *l = i10_sdel_min(t->l, out);
  return i10_balance_r(t->x, l, t->r);
}

static I10Set *i10_sdel_max(I10Set *t, void **out) {
  if (t->r == NULL) {
    *out = t->x;
    return t->l;
  }
  I10Set *r = i10_sdel_max(t->r, out);
  return i10_balance_l(t->x, t->l, r);
}

static I10Set *i10_sglue(I10Set *l, I10Set *r) {
  void *m = NULL;
  if (l == NULL) {
    return r;
  }
  if (r == NULL) {
    return l;
  }
  if (l->sz > r->sz) {
    I10Set *l2 = i10_sdel_max(l, &m);
    return i10_balance_r(m, l2, r);
  }
  I10Set *r2 = i10_sdel_min(r, &m);
  return i10_balance_l(m, l, r2);
}

static I10Set *i10_smerge(I10Set *l, I10Set *r) {
  if (l == NULL) {
    return r;
  }
  if (r == NULL) {
    return l;
  }
  if (3 * l->sz < r->sz) {
    return i10_balance_l(r->x, i10_smerge(l, r->l), r->r);
  }
  if (3 * r->sz < l->sz) {
    return i10_balance_r(l->x, l->l, i10_smerge(l->r, r));
  }
  return i10_sglue(l, r);
}

// splitLookup / splitMember: compares x against t's nodes on one path.
static I10Set *i10_ssplit(I10Cmp cmp, void *x, I10Set *t, I10Set **gt, void **found) {
  if (t == NULL) {
    *gt = NULL;
    *found = NULL;
    return NULL;
  }
  int c = cmp(x, t->x);
  if (c < 0) {
    I10Set *g = NULL;
    I10Set *lt = i10_ssplit(cmp, x, t->l, &g, found);
    *gt = i10_slink(t->x, g, t->r);
    return lt;
  }
  if (c > 0) {
    I10Set *g = NULL;
    I10Set *lt = i10_ssplit(cmp, x, t->r, &g, found);
    *gt = g;
    return i10_slink(t->x, t->l, lt);
  }
  *found = t->x;
  *gt = t->r;
  return t->l;
}

static I10Set *i10_sinter(I10Cmp cmp, I10Set *t1, I10Set *t2) {
  if (t1 == NULL || t2 == NULL) {
    return NULL;
  }
  I10Set *gt = NULL;
  void *found = NULL;
  if (t1->sz >= t2->sz) {
    I10Set *lt = i10_ssplit(cmp, t2->x, t1, &gt, &found);
    I10Set *tl = i10_sinter(cmp, lt, t2->l);
    I10Set *tr = i10_sinter(cmp, gt, t2->r);
    return (found != NULL) ? i10_slink(found, tl, tr) : i10_smerge(tl, tr);
  }
  I10Set *lt = i10_ssplit(cmp, t1->x, t2, &gt, &found);
  I10Set *tl = i10_sinter(cmp, t1->l, lt);
  I10Set *tr = i10_sinter(cmp, t1->r, gt);
  return (found != NULL) ? i10_slink(t1->x, tl, tr) : i10_smerge(tl, tr);
}

// --- clausifier: miniscoping (Clausify.hs) -------------------------------

static I10Form *i10_forall(I10VSet xs, I10Form *a);

static I10Form *i10_forall_or(I10VSet xs, I10Form **fs, int n) {
  if (n == 0) {
    return i10_ffalse();
  }
  if (n == 1) {
    return i10_forall(xs, fs[0]);
  }
  if (xs.n == 0) {
    I10Form *acc = i10_ffalse();
    for (int i = n - 1; i >= 0; i--) {
      acc = i10_or2(fs[i], acc);
    }
    return acc;
  }
  I10Name *x = xs.v[0];
  I10VSet rest = {xs.v + 1, xs.n - 1};
  I10Form **b1 = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)n);
  I10Form **b2 = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)n);
  int n1 = 0, n2 = 0;
  I10VSet u1 = i10_vs_empty(), u2 = i10_vs_empty();
  for (int i = 0; i < n; i++) {
    I10VSet fv = i10_free(fs[i]);
    if (i10_vs_has(fv, x)) {
      b1[n1++] = fs[i];
      u1 = i10_vs_union(u1, fv);
    } else {
      b2[n2++] = fs[i];
      u2 = i10_vs_union(u2, fv);
    }
  }
  I10VSet common = i10_vs_inter(i10_vs_inter(u1, u2), xs);
  I10VSet inner = i10_vs_diff(rest, common);
  I10Form *f = i10_forall_or(inner, b1, n1);
  I10Form *g = i10_forall_or(inner, b2, n2);
  I10Name *one[1] = {x};
  I10VSet sx = i10_vs_of(one, 1);
  return i10_fq(I10F_ALL, common, i10_or2(i10_fq(I10F_ALL, sx, f), g));
}

static I10Form *i10_forall(I10VSet xs, I10Form *a) {
  I10VSet ys = i10_vs_inter(xs, i10_free(a));
  if (ys.n == 0) {
    return a;
  }
  I10Form *p = i10_positive(a);
  switch (p->kind) {
    case I10F_AND: {
      I10Form **k = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(p->n + 1));
      for (int i = 0; i < p->n; i++) {
        k[i] = i10_forall(ys, p->k[i]);
      }
      return i10_flist(I10F_AND, k, p->n);
    }
    case I10F_ALL:
      return i10_forall(i10_vs_union(ys, p->vs), p->k[0]);
    case I10F_OR:
      return i10_forall_or(ys, p->k, p->n);
    default:
      // Clausify.forAll quantifies the original formula, not positive's.
      return i10_fq(I10F_ALL, ys, a);
  }
}

static I10Form *i10_miniscope(I10Form *f) {
  switch (f->kind) {
    case I10F_LIT:
      return f;
    case I10F_NOT:
      return i10_f1(I10F_NOT, i10_miniscope(f->k[0]));
    case I10F_AND:
    case I10F_OR:
    case I10F_EQUIV: {
      I10Form *g = i10_flist(f->kind, f->k, f->n);
      for (int i = 0; i < f->n; i++) {
        g->k[i] = i10_miniscope(f->k[i]);
      }
      return g;
    }
    case I10F_ALL:
      return i10_forall(f->vs, i10_miniscope(f->k[0]));
    case I10F_EX:
      return i10_nt(i10_forall(f->vs, i10_nt(i10_miniscope(f->k[0]))));
    default:
      i10_fail(I10_RC_INPUT, "internal: a connective survived simplification");
  }
}

// --- clausifier: Skolem functions and definitions ------------------------

static I10Name *i10_skolem_name(const char *prefix, const char *base) {
  I10Name *n = i10_new_name(base);
  n->sk_prefix = prefix;
  n->sk_tag = g_i10_tag;
  return n;
}

static I10Term *i10_skolem(I10Name *v, I10VSet vs) {
  I10Name *n = i10_skolem_name("sK", v->base);
  I10Fun *f = i10_fun_new(n, vs.n, 0, &g_i10_ty_i);
  I10Term **args = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)(vs.n + 1));
  for (int i = 0; i < vs.n; i++) {
    args[i] = i10_var_term(vs.v[i]);
  }
  return i10_app(f, args, vs.n);
}

static I10Atom *i10_definition(const char *w, I10VSet vs) {
  I10Name *n = i10_skolem_name("sP", w);
  g_i10_ndefs++;
  I10Fun *f = i10_fun_new(n, vs.n, 1, NULL);
  I10Term **args = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)(vs.n + 1));
  for (int i = 0; i < vs.n; i++) {
    args[i] = i10_var_term(vs.v[i]);
  }
  return i10_atom(0, i10_app(f, args, vs.n), NULL);
}

static I10Form *i10_remove_exists(I10Form *f) {
  switch (f->kind) {
    case I10F_AND:
    case I10F_OR: {
      I10Form *g = i10_flist(f->kind, f->k, f->n);
      for (int i = 0; i < f->n; i++) {
        g->k[i] = i10_remove_exists(f->k[i]);
      }
      return g;
    }
    case I10F_ALL:
      return i10_fq(I10F_ALL, f->vs, i10_remove_exists(f->k[0]));
    case I10F_EX: {
      I10VSet fv = i10_free(f);
      I10Subst s;
      s.n = f->vs.n;
      s.from = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(s.n + 1));
      s.to = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)(s.n + 1));
      for (int i = 0; i < f->vs.n; i++) {
        s.from[i] = f->vs.v[i];
        s.to[i] = i10_skolem(f->vs.v[i], fv);
      }
      return i10_remove_exists(i10_subst_form(f->k[0], &s));
    }
    default:
      return f;
  }
}

// --- clausifier: equivalences (removeEquiv) ------------------------------

typedef struct I10Req {
  I10Vec defs;
  I10Form *pos;
  I10Form *neg;
} I10Req;

static void i10_append(I10Vec *dst, const I10Vec *src) {
  for (int i = 0; i < src->n; i++) {
    i10_push(dst, src->v[i]);
  }
}

static int i10_is_small(const I10Form *f) {
  switch (f->kind) {
    case I10F_LIT:
      return 1;
    case I10F_NOT:
    case I10F_ALL:
    case I10F_EX:
      return i10_is_small(f->k[0]);
    default:
      return 0;
  }
}

static I10Req i10_make_copyable(int in_equiv, I10Form *pos, I10Form *neg) {
  I10Req r;
  memset(&r, 0, sizeof r);
  if (i10_is_small(pos) || !in_equiv) {
    r.pos = i10_remove_exists(pos);
    r.neg = i10_remove_exists(neg);
    return r;
  }
  I10Atom *dp = i10_definition("equiv", i10_free(pos));
  i10_push(&r.defs, i10_or2(i10_flit(i10_lit(1, dp)), pos));
  i10_push(&r.defs, i10_or2(i10_flit(i10_lit(0, dp)), neg));
  r.pos = i10_flit(i10_lit(0, dp));
  r.neg = i10_flit(i10_lit(1, dp));
  return r;
}

static I10Req i10_remove_equiv_aux(int in_equiv, I10Form *p) {
  I10Form *s = i10_simple(p);
  I10Req r;
  memset(&r, 0, sizeof r);
  switch (s->kind) {
    case I10F_NOT: {
      I10Req q = i10_remove_equiv_aux(in_equiv, s->k[0]);
      r.defs = q.defs;
      r.pos = q.neg;
      r.neg = q.pos;
      return r;
    }
    case I10F_AND: {
      I10Form **poss = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(s->n + 1));
      I10Form **negs = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(s->n + 1));
      for (int i = 0; i < s->n; i++) {
        I10Req q = i10_remove_equiv_aux(in_equiv, s->k[i]);
        i10_append(&r.defs, &q.defs);
        poss[i] = q.pos;
        negs[i] = q.neg;
      }
      r.pos = i10_flist(I10F_AND, poss, s->n);
      r.neg = i10_flist(I10F_OR, negs, s->n);
      return r;
    }
    case I10F_ALL: {
      I10Req q = i10_remove_equiv_aux(in_equiv, s->k[0]);
      r.defs = q.defs;
      r.pos = i10_fq(I10F_ALL, s->vs, q.pos);
      r.neg = i10_fq(I10F_EX, s->vs, q.neg);
      return r;
    }
    case I10F_EQUIV: {
      I10Req qp = i10_remove_equiv_aux(1, s->k[0]);
      I10Req qq = i10_remove_equiv_aux(1, s->k[1]);
      I10Req cp = i10_make_copyable(in_equiv, qp.pos, qp.neg);
      I10Req cq = i10_make_copyable(in_equiv, qq.pos, qq.neg);
      i10_append(&r.defs, &qp.defs);
      i10_append(&r.defs, &qq.defs);
      i10_append(&r.defs, &cp.defs);
      i10_append(&r.defs, &cq.defs);
      r.pos = i10_and2(i10_or2(cp.neg, cq.pos), i10_or2(cp.pos, cq.neg));
      r.neg = i10_and2(i10_or2(cp.neg, cq.neg), i10_or2(cp.pos, cq.pos));
      return r;
    }
    case I10F_LIT:
      r.pos = s;
      r.neg = i10_flit(i10_lit(!s->lit->neg, s->lit->a));
      return r;
    default:
      i10_fail(I10_RC_INPUT, "internal: unexpected formula in removeEquiv");
  }
}

// --- clausifier: cheap disjunctions (removeExpensiveOr) ------------------

typedef struct I10Cost {
  long long c;
  long long l;
} I10Cost;

typedef struct I10Reo {
  I10Vec defs;
  I10Form *p;
  I10Cost cost;
} I10Reo;

static long long i10_cap_add(long long a, long long b) {
  return (a > I10_COST_CAP - b) ? I10_COST_CAP : a + b;
}

static long long i10_cap_mul(long long a, long long b) {
  if (a != 0 && b > I10_COST_CAP / a) {
    return I10_COST_CAP;
  }
  return a * b;
}

static I10Cost i10_or_cost(const I10Cost *cs, int n) {
  I10Cost r = {1, 0};
  if (n == 0) {
    return r;
  }
  r = cs[n - 1];
  for (int i = n - 2; i >= 0; i--) {
    I10Cost t;
    t.c = i10_cap_mul(cs[i].c, r.c);
    t.l = i10_cap_add(i10_cap_mul(cs[i].c, r.l), i10_cap_mul(r.c, cs[i].l));
    r = t;
  }
  return r;
}

static I10Reo i10_make_or(I10Form **fs, I10Cost *cs, int n) {
  I10Reo r;
  memset(&r, 0, sizeof r);
  if (n == 0) {
    r.p = i10_ffalse();
    r.cost = i10_or_cost(cs, 0);
    return r;
  }
  if (n == 1) {
    r.p = fs[0];
    r.cost = cs[0];
    return r;
  }
  // split: cheap disjuncts move to fcs1 (reversed); fcs2 is the rest.
  I10Form **f1 = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(n + 1));
  I10Cost *c1 = (I10Cost *)i10_alloc(sizeof(I10Cost) * (size_t)(n + 1));
  int n1 = 0, i = 0, start2 = n;
  for (; i < n && cs[i].c <= 1; i++) {
    f1[n1] = fs[i];
    c1[n1] = cs[i];
    n1++;
  }
  for (int a = 0, b = n1 - 1; a < b; a++, b--) {
    I10Form *tf = f1[a];
    f1[a] = f1[b];
    f1[b] = tf;
    I10Cost tc = c1[a];
    c1[a] = c1[b];
    c1[b] = tc;
  }
  if (i < n) {
    int take = (cs[i].c <= 2) ? 2 : 1;
    if (i + take > n) {
      take = n - i;
    }
    for (int j = n1 - 1; j >= 0; j--) {
      f1[j + take] = f1[j];
      c1[j + take] = c1[j];
    }
    for (int j = 0; j < take; j++) {
      f1[j] = fs[i + j];
      c1[j] = cs[i + j];
    }
    n1 += take;
    start2 = i + take;
  }
  int n2 = n - start2;
  if (n2 == 0) {
    r.p = i10_flist(I10F_OR, f1, n1);
    r.cost = i10_or_cost(c1, n1);
    return r;
  }
  I10VSet fv = i10_vs_empty();
  for (int j = start2; j < n; j++) {
    fv = i10_vs_union(fv, i10_free(fs[j]));
  }
  I10Atom *d = i10_definition("or", fv);
  I10Form **g = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(n2 + 1));
  I10Cost *gc = (I10Cost *)i10_alloc(sizeof(I10Cost) * (size_t)(n2 + 1));
  I10Cost unit = {1, 1};
  g[0] = i10_flit(i10_lit(1, d));
  gc[0] = unit;
  for (int j = 0; j < n2; j++) {
    g[j + 1] = fs[start2 + j];
    gc[j + 1] = cs[start2 + j];
  }
  I10Reo q = i10_make_or(g, gc, n2 + 1);
  i10_push(&r.defs, q.p);
  i10_append(&r.defs, &q.defs);
  I10Form **h = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(n1 + 1));
  I10Cost *hc = (I10Cost *)i10_alloc(sizeof(I10Cost) * (size_t)(n1 + 1));
  h[0] = i10_flit(i10_lit(0, d));
  hc[0] = unit;
  for (int j = 0; j < n1; j++) {
    h[j + 1] = f1[j];
    hc[j + 1] = c1[j];
  }
  r.p = i10_flist(I10F_OR, h, n1 + 1);
  r.cost = i10_or_cost(hc, n1 + 1);
  return r;
}

static I10Reo i10_reo(I10Form *f) {
  I10Reo r;
  memset(&r, 0, sizeof r);
  switch (f->kind) {
    case I10F_AND: {
      I10Form **ps = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(f->n + 1));
      I10Cost sum = {0, 0};
      for (int i = 0; i < f->n; i++) {
        I10Reo q = i10_reo(f->k[i]);
        i10_append(&r.defs, &q.defs);
        ps[i] = q.p;
        sum.c = i10_cap_add(sum.c, q.cost.c);
        sum.l = i10_cap_add(sum.l, q.cost.l);
      }
      r.p = i10_flist(I10F_AND, ps, f->n);
      r.cost = sum;
      return r;
    }
    case I10F_OR: {
      I10Form **ps = (I10Form **)i10_alloc(sizeof(I10Form *) * (size_t)(f->n + 1));
      I10Cost *cs = (I10Cost *)i10_alloc(sizeof(I10Cost) * (size_t)(f->n + 1));
      I10Vec kid_defs = {NULL, 0, 0};
      for (int i = 0; i < f->n; i++) {
        I10Reo q = i10_reo(f->k[i]);
        i10_append(&kid_defs, &q.defs);
        ps[i] = q.p;
        cs[i] = q.cost;
      }
      // sortBy (comparing snd): a stable insertion sort on the cost pair
      for (int i = 1; i < f->n; i++) {
        I10Form *pf = ps[i];
        I10Cost pc = cs[i];
        int j = i - 1;
        while (j >= 0 && (cs[j].c > pc.c || (cs[j].c == pc.c && cs[j].l > pc.l))) {
          ps[j + 1] = ps[j];
          cs[j + 1] = cs[j];
          j--;
        }
        ps[j + 1] = pf;
        cs[j + 1] = pc;
      }
      I10Reo m = i10_make_or(ps, cs, f->n);
      r.defs = m.defs;
      i10_append(&r.defs, &kid_defs);
      r.p = m.p;
      r.cost = m.cost;
      return r;
    }
    case I10F_ALL: {
      I10Reo q = i10_reo(f->k[0]);
      for (int i = 0; i < q.defs.n; i++) {
        i10_push(&r.defs, i10_fq(I10F_ALL, f->vs, (I10Form *)q.defs.v[i]));
      }
      r.p = i10_fq(I10F_ALL, f->vs, q.p);
      r.cost = q.cost;
      return r;
    }
    default:
      r.p = f;
      r.cost.c = 1;
      r.cost.l = 1;
      return r;
  }
}

// --- clausifier: Form.uniqueNames -----------------------------------------

typedef struct I10IdSet {
  long long *slot;
  int cap;
  int n;
} I10IdSet;

static int i10_ids_has(const I10IdSet *s, long long id) {
  if (s->cap == 0) {
    return 0;
  }
  unsigned long m = (unsigned long)s->cap - 1u;
  for (unsigned long i = (unsigned long)id * 2654435761u & m; s->slot[i] != 0; i = (i + 1u) & m) {
    if (s->slot[i] == id + 1) {
      return 1;
    }
  }
  return 0;
}

static void i10_ids_add(I10IdSet *s, long long id) {
  if (i10_ids_has(s, id)) {
    return;
  }
  if (2 * (s->n + 1) > s->cap) {
    int cap = (s->cap == 0) ? 64 : 2 * s->cap;
    long long *slot = (long long *)i10_alloc(sizeof(long long) * (size_t)cap);
    for (int i = 0; i < s->cap; i++) {
      if (s->slot[i] != 0) {
        unsigned long j = (unsigned long)(s->slot[i] - 1) * 2654435761u & (unsigned long)(cap - 1);
        while (slot[j] != 0) {
          j = (j + 1u) & (unsigned long)(cap - 1);
        }
        slot[j] = s->slot[i];
      }
    }
    s->slot = slot;
    s->cap = cap;
  }
  unsigned long j = (unsigned long)id * 2654435761u & (unsigned long)(s->cap - 1);
  while (s->slot[j] != 0) {
    j = (j + 1u) & (unsigned long)(s->cap - 1);
  }
  s->slot[j] = id + 1;
  s->n++;
}

static I10Form *i10_un_form(I10Form *f, I10IdSet *used, const I10Subst *s);

// A binder: a variable already bound earlier on the walk is stale and
// gets a fresh name, substituted in the body.
static I10Form *i10_un_bind(I10Form *f, I10IdSet *used, const I10Subst *s) {
  int nst = 0;
  I10Name **st = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(f->vs.n + 1));
  I10Name **fresh = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(f->vs.n + 1));
  int nfr = 0;
  for (int i = 0; i < f->vs.n; i++) {
    if (i10_ids_has(used, f->vs.v[i]->id)) {
      st[nst++] = f->vs.v[i];
    } else {
      fresh[nfr++] = f->vs.v[i];
    }
  }
  I10Name **st2 = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(nst + 1));
  for (int i = 0; i < nst; i++) {
    st2[i] = i10_new_name(st[i]->base);
  }
  for (int i = 0; i < nfr; i++) {
    i10_ids_add(used, fresh[i]->id);
  }
  for (int i = 0; i < nst; i++) {
    i10_ids_add(used, st2[i]->id);
  }
  if (nst == 0) {
    return i10_fq(f->kind, f->vs, i10_un_form(f->k[0], used, s));
  }
  I10Subst s2;
  s2.n = s->n + nst;
  s2.from = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(s2.n + 1));
  s2.to = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)(s2.n + 1));
  for (int i = 0; i < s->n; i++) {
    s2.from[i] = s->from[i];
    s2.to[i] = s->to[i];
  }
  for (int i = 0; i < nst; i++) {
    s2.from[s->n + i] = st[i];
    s2.to[s->n + i] = i10_var_term(st2[i]);
  }
  I10Name **all = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(f->vs.n + 1));
  for (int i = 0; i < nst; i++) {
    all[i] = st2[i];
  }
  for (int i = 0; i < nfr; i++) {
    all[nst + i] = fresh[i];
  }
  return i10_fq(f->kind, i10_vs_of(all, f->vs.n), i10_un_form(f->k[0], used, &s2));
}

static I10Form *i10_un_form(I10Form *f, I10IdSet *used, const I10Subst *s) {
  switch (f->kind) {
    case I10F_LIT: {
      if (s->n == 0) {
        return f;
      }
      I10Lit *l = i10_subst_lit(f->lit, s);
      return (l == f->lit) ? f : i10_flit(l);
    }
    case I10F_ALL:
    case I10F_EX:
      return i10_un_bind(f, used, s);
    default: {
      I10Form *g = i10_flist(f->kind, f->k, f->n);
      g->conn = f->conn;
      for (int i = 0; i < f->n; i++) {
        g->k[i] = i10_un_form(f->k[i], used, s);
      }
      return g;
    }
  }
}

static I10Form *i10_unique_names(I10Form *f) {
  I10IdSet used = {NULL, 0, 0};
  I10VSet fv = i10_free(f);
  for (int i = 0; i < fv.n; i++) {
    i10_ids_add(&used, fv.v[i]->id);
  }
  I10Subst none = {NULL, NULL, 0};
  return i10_un_form(f, &used, &none);
}

// --- clausifier: CNF and its simplification ------------------------------

static I10LL *i10_ll(int n) {
  I10LL *c = (I10LL *)i10_alloc(sizeof(I10LL));
  c->n = n;
  c->l = (I10Lit **)i10_alloc(sizeof(I10Lit *) * (size_t)(n > 0 ? n : 1));
  return c;
}

static void i10_cnf(I10Form *f, I10Vec *out) {
  switch (f->kind) {
    case I10F_ALL:
      i10_cnf(f->k[0], out);
      return;
    case I10F_AND:
      for (int i = 0; i < f->n; i++) {
        i10_cnf(f->k[i], out);
      }
      return;
    case I10F_LIT: {
      I10LL *c = i10_ll(1);
      c->l[0] = f->lit;
      i10_push(out, c);
      return;
    }
    case I10F_OR: {
      I10Vec acc = {NULL, 0, 0};
      i10_push(&acc, i10_ll(0));
      for (int i = f->n - 1; i >= 0; i--) {
        I10Vec cs = {NULL, 0, 0};
        i10_cnf(f->k[i], &cs);
        I10Vec next = {NULL, 0, 0};
        for (int a = 0; a < cs.n; a++) {
          I10LL *c = (I10LL *)cs.v[a];
          for (int b = 0; b < acc.n; b++) {
            I10LL *d = (I10LL *)acc.v[b];
            I10LL *cd = i10_ll(c->n + d->n);
            for (int j = 0; j < c->n; j++) {
              cd->l[j] = c->l[j];
            }
            for (int j = 0; j < d->n; j++) {
              cd->l[c->n + j] = d->l[j];
            }
            i10_push(&next, cd);
          }
        }
        acc = next;
      }
      i10_append(out, &acc);
      return;
    }
    default:
      i10_fail(I10_RC_INPUT, "internal: unexpected formula in cnf");
  }
}

static I10LL *i10_unify(I10LL *c) {
  I10Lit **rem = (I10Lit **)i10_alloc(sizeof(I10Lit *) * (size_t)(c->n + 1));
  for (int i = 0; i < c->n; i++) {
    rem[i] = c->l[i];
  }
  I10Lit **acc = (I10Lit **)i10_alloc(sizeof(I10Lit *) * (size_t)(c->n + 1));
  int nacc = 0;
  for (int i = 0; i < c->n; i++) {
    I10Lit *l = rem[i];
    if (l->neg && l->a->eq && l->a->l->var != NULL && l->a->r->var != NULL) {
      I10Name *from = l->a->l->var;
      I10Term *to = l->a->r;
      I10Subst s = {&from, &to, 1};
      for (int j = 0; j < nacc; j++) {
        acc[j] = i10_subst_lit(acc[j], &s);
      }
      for (int j = i + 1; j < c->n; j++) {
        rem[j] = i10_subst_lit(rem[j], &s);
      }
      continue;
    }
    acc[nacc++] = l;
  }
  I10LL *r = i10_ll(nacc);
  for (int i = 0; i < nacc; i++) {
    r->l[i] = acc[nacc - 1 - i];
  }
  return r;
}

// Clausify.tautElim, with the evaluation order of the lazy original:
// the positive set, then (only if non-empty) the negative set and the
// intersection, then the trivial-equation test, then the negative set
// if the guard did not need it.  NULL: the clause is a tautology.
static I10LL *i10_taut_elim(I10LL *c) {
  I10Vec posv = {NULL, 0, 0}, negv = {NULL, 0, 0};
  for (int i = 0; i < c->n; i++) {
    i10_push(c->l[i]->neg ? &negv : &posv, c->l[i]->a);
  }
  I10Set *pos = i10_sfromlist(i10_cmp_atom_v, posv.v, posv.n);
  I10Set *neg = NULL;
  int neg_built = 0;
  if (pos != NULL) {
    neg = i10_sfromlist(i10_cmp_atom_v, negv.v, negv.n);
    neg_built = 1;
    if (i10_sinter(i10_cmp_atom_v, pos, neg) != NULL) {
      return NULL;
    }
  }
  for (int i = 0; i < c->n; i++) {
    I10Lit *l = c->l[i];
    if (!l->neg && l->a->eq && i10_eq_term(l->a->l, l->a->r)) {
      return NULL;
    }
  }
  if (!neg_built) {
    neg = i10_sfromlist(i10_cmp_atom_v, negv.v, negv.n);
  }
  I10Vec na = {NULL, 0, 0}, pa = {NULL, 0, 0};
  i10_stolist(neg, &na);
  i10_stolist(pos, &pa);
  I10LL *r = i10_ll(na.n + pa.n);
  for (int i = 0; i < na.n; i++) {
    r->l[i] = i10_lit(1, (I10Atom *)na.v[i]);
  }
  for (int i = 0; i < pa.n; i++) {
    r->l[na.n + i] = i10_lit(0, (I10Atom *)pa.v[i]);
  }
  return r;
}

// simplifyCNF = usort . concatMap (tautElim . unify []): each clause is
// tested, then inserted into the (insertion-built) set of clauses.
static void i10_simplify_cnf(I10Vec *cnf, I10Vec *out) {
  I10Set *t = NULL;
  for (int i = 0; i < cnf->n; i++) {
    I10LL *c = i10_taut_elim(i10_unify((I10LL *)cnf->v[i]));
    if (c != NULL) {
      t = i10_sinsert(i10_cmp_clause_v, c, t);
    }
  }
  i10_stolist(t, out);
}

static void i10_clause_form(const char *tag, const char *role, I10Form *f, I10Vec *out) {
  g_i10_tag = tag;
  I10Form *m = i10_miniscope(i10_simplify(f));
  I10Req q = i10_remove_equiv_aux(0, m);
  I10Vec stage = {NULL, 0, 0};
  i10_push(&stage, q.pos);
  i10_append(&stage, &q.defs);
  for (int i = 0; i < stage.n; i++) {
    stage.v[i] = i10_remove_exists((I10Form *)stage.v[i]);
  }
  I10Vec noor = {NULL, 0, 0};
  for (int i = 0; i < stage.n; i++) {
    I10Reo r = i10_reo((I10Form *)stage.v[i]);
    i10_push(&noor, r.p);
    i10_append(&noor, &r.defs);
  }
  for (int i = 0; i < noor.n; i++) {
    noor.v[i] = i10_unique_names((I10Form *)noor.v[i]);
  }
  I10Vec cnf = {NULL, 0, 0};
  for (int i = 0; i < noor.n; i++) {
    i10_cnf((I10Form *)noor.v[i], &cnf);
  }
  I10Vec simp = {NULL, 0, 0};
  i10_simplify_cnf(&cnf, &simp);
  for (int i = 0; i < simp.n; i++) {
    I10LL *c = (I10LL *)simp.v[i];
    I10Clause *cl = (I10Clause *)i10_alloc(sizeof(I10Clause));
    cl->tag = (i == 0) ? tag : i10_fmt("%s_%d", tag, i);
    cl->role = role;
    cl->l = c->l;
    cl->n = c->n;
    i10_push(out, cl);
  }
}

// Clausify.clausify then Toolbox.oneConjecture: every axiom's clauses
// (in reverse, the accumulator's order), then the conjecture's.
static void i10_clausify(I10Vec *prob) {
  I10Vec conj = {NULL, 0, 0};
  int nconj = 0;
  for (int i = 0; i < g_i10_inputs.n; i++) {
    I10Input *in = (I10Input *)g_i10_inputs.v[i];
    if (in->conj) {
      if (++nconj > 1) {
        i10_fail(I10_RC_INPUT, "more than one conjecture");
      }
      i10_clause_form(in->tag, "negated_conjecture", i10_nt(in->f), &conj);
      continue;
    }
    I10Vec cs = {NULL, 0, 0};
    i10_clause_form(in->tag, in->role, in->f, &cs);
    for (int j = cs.n - 1; j >= 0; j--) {
      i10_push(prob, cs.v[j]);
    }
  }
  i10_append(prob, &conj);
}

// --- horn-to-unit (HornToUnit.hs) ---------------------------------------

typedef struct I10Horn {
  I10Name *bool_ty;
  I10Fun *truef;
  I10Name *tuple_ty;
  I10Name *tuple_nm;
  I10Name *token_ty;
  I10Fun *af;
  I10Fun *bf;
  I10Name *ifeq_nm;
  I10Name *xvar;
  I10Name *yvar;
  I10Name *zvar;
  I10Vec tuples;
  I10Vec ifeqs;
  int npreds;
} I10Horn;

static I10Horn g_i10_h;

static I10Name *i10_type_of(I10Term *t) {
  return (t->var != NULL) ? &g_i10_ty_i : t->fun->res;
}

static int i10_size(I10Term *t) {
  int n = 1;
  if (t->var == NULL) {
    for (int i = 0; i < t->nargs; i++) {
      n += i10_size(t->args[i]);
    }
  }
  return n;
}

static int i10_npos(const I10Clause *c) {
  int n = 0;
  for (int i = 0; i < c->n; i++) {
    n += !c->l[i]->neg;
  }
  return n;
}

static I10Clause *i10_clause(const char *tag, const char *role, I10Lit **l, int n) {
  I10Clause *c = (I10Clause *)i10_alloc(sizeof(I10Clause));
  c->tag = tag;
  c->role = role;
  c->n = n;
  c->l = (I10Lit **)i10_alloc(sizeof(I10Lit *) * (size_t)(n > 0 ? n : 1));
  for (int i = 0; i < n; i++) {
    c->l[i] = l[i];
  }
  return c;
}

typedef struct I10Buf {
  char s[512];
  size_t n;
} I10Buf;

static void i10_bput(I10Buf *b, const char *s) {
  while (*s != '\0' && b->n + 1u < sizeof b->s) {
    b->s[b->n++] = *s++;
  }
  b->s[b->n] = '\0';
}

static void i10_bput_term(I10Buf *b, I10Term *t) {
  if (t->var != NULL) {
    i10_bput(b, t->var->base);
    return;
  }
  i10_bput(b, t->fun->name->base);
  if (t->nargs > 0) {
    i10_bput(b, "(");
    for (int i = 0; i < t->nargs; i++) {
      i10_bput(b, (i > 0) ? "," : "");
      i10_bput_term(b, t->args[i]);
    }
    i10_bput(b, ")");
  }
}

static _Noreturn void i10_nonhorn(const I10Clause *c) {
  I10Buf b;
  b.n = 0;
  b.s[0] = '\0';
  for (int i = 0; i < c->n; i++) {
    I10Atom *a = c->l[i]->a;
    i10_bput(&b, (i > 0) ? " | " : "");
    if (!a->eq) {
      i10_bput(&b, c->l[i]->neg ? "~" : "");
      i10_bput_term(&b, a->l);
    } else {
      i10_bput_term(&b, a->l);
      i10_bput(&b, c->l[i]->neg ? " != " : " = ");
      i10_bput_term(&b, a->r);
    }
  }
  i10_fail(I10_RC_NONHORN, "not Horn: clause %s has %d positive literals: %s",
           c->tag, i10_npos(c), b.s);
}

// HornToUnit.hasSizeOneModel for a Horn problem.  In a one-element model
// every equation holds, so each clause is a propositional Horn clause
// over the predicate symbols, and their least model (unit propagation)
// decides.  A clause with a positive equation is always true; a negative
// equation is always false.
static int i10_size_one_model(I10Vec *prob) {
  int nc = prob->n;
  int *count = (int *)i10_alloc(sizeof(int) * (size_t)(nc + 1));
  I10Fun **head = (I10Fun **)i10_alloc(sizeof(I10Fun *) * (size_t)(nc + 1));
  char *trivial = (char *)i10_alloc((size_t)nc + 1u);
  I10Vec preds = {NULL, 0, 0};
  for (int i = 0; i < nc; i++) {
    I10Clause *c = (I10Clause *)prob->v[i];
    for (int j = 0; j < c->n; j++) {
      I10Atom *a = c->l[j]->a;
      if (!a->eq && a->l->fun->truth >= 0) {
        a->l->fun->truth = -1 - preds.n;
        i10_push(&preds, a->l->fun);
      }
    }
  }
  // occ[p]: the clauses with a negative occurrence of predicate p
  I10Vec *occ = (I10Vec *)i10_alloc(sizeof(I10Vec) * (size_t)(preds.n + 1));
  char *value = (char *)i10_alloc((size_t)preds.n + 1u);
  I10Vec queue = {NULL, 0, 0};
  for (int i = 0; i < nc; i++) {
    I10Clause *c = (I10Clause *)prob->v[i];
    for (int j = 0; j < c->n; j++) {
      I10Atom *a = c->l[j]->a;
      if (!c->l[j]->neg) {
        if (a->eq) {
          trivial[i] = 1;
        } else {
          head[i] = a->l->fun;
        }
      } else if (!a->eq) {
        count[i]++;
        i10_push(&occ[-1 - a->l->fun->truth], (void *)(intptr_t)i);
      }
    }
    if (!trivial[i] && count[i] == 0 && head[i] != NULL) {
      i10_push(&queue, (void *)(intptr_t)i);
    }
  }
  for (int q = 0; q < queue.n; q++) {
    int i = (int)(intptr_t)queue.v[q];
    int p = -1 - head[i]->truth;
    if (value[p]) {
      continue;
    }
    value[p] = 1;
    for (int k = 0; k < occ[p].n; k++) {
      int j = (int)(intptr_t)occ[p].v[k];
      if (--count[j] == 0 && !trivial[j] && head[j] != NULL) {
        i10_push(&queue, (void *)(intptr_t)j);
      }
    }
  }
  int model = 1;
  for (int i = 0; i < nc && model; i++) {
    model = trivial[i] || head[i] != NULL || count[i] > 0;
  }
  for (int p = 0; p < preds.n; p++) {
    ((I10Fun *)preds.v[p])->truth = 0;
  }
  return model;
}

static I10Term *i10_true_term(void) {
  return i10_app(g_i10_h.truef, NULL, 0);
}

static I10Fun *i10_bool_fun(I10Fun *p) {
  if (p->boolfun == NULL) {
    p->boolfun = i10_fun_new(p->name, p->arity, 0, g_i10_h.bool_ty);
    g_i10_h.npreds++;
  }
  return p->boolfun;
}

static I10Fun *i10_tuple_fun(I10Name **tys, int n) {
  for (int i = 0; i < g_i10_h.tuples.n; i++) {
    I10Fun *f = (I10Fun *)g_i10_h.tuples.v[i];
    int same = (f->arity == n);
    for (int j = 0; same && j < n; j++) {
      same = (i10_cmp_name(f->name->vargs[j], tys[j]) == 0);
    }
    if (same) {
      return f;
    }
  }
  I10Name *res = i10_name_variant(g_i10_h.tuple_ty, tys, n);
  I10Fun *f = i10_fun_new(i10_name_variant(g_i10_h.tuple_nm, tys, n), n, 0, res);
  i10_push(&g_i10_h.tuples, f);
  return f;
}

static I10Fun *i10_ifeq_fun(I10Name *ty1, I10Name *ty2) {
  for (int i = 0; i < g_i10_h.ifeqs.n; i++) {
    I10Fun *f = (I10Fun *)g_i10_h.ifeqs.v[i];
    if (i10_cmp_name(f->name->vargs[0], ty1) == 0 &&
        i10_cmp_name(f->name->vargs[1], ty2) == 0) {
      return f;
    }
  }
  I10Name *tys[2] = {ty1, ty2};
  I10Fun *f = i10_fun_new(i10_name_variant(g_i10_h.ifeq_nm, tys, 2), 4, 0, ty2);
  i10_push(&g_i10_h.ifeqs, f);
  return f;
}

// encode (c :=: d) (Neg (a :=: b)) under the if-then-else encoding.
static I10Atom *i10_encode(I10Atom *head, I10Atom *cond) {
  I10Fun *ifeq = i10_ifeq_fun(i10_type_of(cond->l), i10_type_of(head->l));
  I10Term *c = head->l, *d = head->r;
  if (i10_size(c) < i10_size(d)) {
    I10Term *t = c;
    c = d;
    d = t;
  }
  I10Term *args[4] = {cond->l, cond->r, c, d};
  return i10_atom(1, i10_app(ifeq, args, 4), d);
}

static void i10_horn_to_unit(I10Vec *prob, I10Vec *out) {
  for (int i = 0; i < prob->n; i++) {
    if (i10_npos((I10Clause *)prob->v[i]) > 1) {
      i10_nonhorn((I10Clause *)prob->v[i]);
    }
  }
  if (i10_size_one_model(prob)) {
    i10_fail(I10_RC_SAT1, "a one-element model exists (jukebox answers Satisfiable)");
  }
  I10Horn *h = &g_i10_h;
  memset(h, 0, sizeof *h);

  // eliminatePredicates
  h->bool_ty = i10_name_var("bool", ++g_i10_counter);
  h->truef = i10_fun_new(i10_name_var("true", ++g_i10_counter), 0, 0, h->bool_ty);
  for (int i = 0; i < prob->n; i++) {
    I10Clause *c = (I10Clause *)prob->v[i];
    for (int j = 0; j < c->n; j++) {
      I10Atom *a = c->l[j]->a;
      if (!a->eq) {
        I10Term *p = a->l;
        I10Term *q = i10_app(i10_bool_fun(p->fun), p->args, p->nargs);
        c->l[j] = i10_lit(c->l[j]->neg, i10_atom(1, q, i10_true_term()));
      }
    }
  }

  // eliminateMultiplePreconditions (conjunctive conjectures not allowed)
  h->tuple_ty = i10_name_var("tuple", ++g_i10_counter);
  h->tuple_nm = i10_name_var("tuple", ++g_i10_counter);
  for (int i = 0; i < prob->n; i++) {
    I10Clause *c = (I10Clause *)prob->v[i];
    if (i10_npos(c) == 0 && c->n != 1) {
      I10Term **ts = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)(c->n + 1));
      I10Term **us = (I10Term **)i10_alloc(sizeof(I10Term *) * (size_t)(c->n + 1));
      I10Name **tys = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(c->n + 1));
      for (int j = 0; j < c->n; j++) {
        ts[j] = c->l[j]->a->l;
        us[j] = c->l[j]->a->r;
        tys[j] = i10_type_of(ts[j]);
      }
      I10Fun *tf = i10_tuple_fun(tys, c->n);
      I10Lit *l = i10_lit(1, i10_atom(1, i10_app(tf, ts, c->n), i10_app(tf, us, c->n)));
      prob->v[i] = i10_clause(c->tag, c->role, &l, 1);
    }
  }

  // eliminateUnsuitableConjectures (disjunctive conjectures not allowed)
  int ngoals = 0;
  for (int i = 0; i < prob->n; i++) {
    ngoals += (i10_npos((I10Clause *)prob->v[i]) == 0);
  }
  if (ngoals > 1) {
    h->token_ty = i10_name_var("token", ++g_i10_counter);
    h->af = i10_fun_new(i10_name_var("a", ++g_i10_counter), 0, 0, h->token_ty);
    h->bf = i10_fun_new(i10_name_var("b", ++g_i10_counter), 0, 0, h->token_ty);
    I10Atom *ab = i10_atom(1, i10_app(h->af, NULL, 0), i10_app(h->bf, NULL, 0));
    I10Vec good = {NULL, 0, 0}, bad = {NULL, 0, 0};
    for (int i = 0; i < prob->n; i++) {
      I10Clause *c = (I10Clause *)prob->v[i];
      if (i10_npos(c) > 0) {
        i10_push(&good, c);
        continue;
      }
      I10Lit **l = (I10Lit **)i10_alloc(sizeof(I10Lit *) * (size_t)(c->n + 1));
      l[0] = i10_lit(0, ab);
      for (int j = 0; j < c->n; j++) {
        l[j + 1] = c->l[j];
      }
      i10_push(&bad, i10_clause(c->tag, c->role, l, c->n + 1));
    }
    I10Lit *g = i10_lit(1, ab);
    prob->n = 0;
    i10_append(prob, &good);
    i10_append(prob, &bad);
    i10_push(prob, i10_clause("goal", "negated_conjecture", &g, 1));
  }

  // eliminateHornClauses
  h->ifeq_nm = i10_name_var("ifeq", ++g_i10_counter);
  g_i10_counter += 2;  // freshName, passiveName
  h->xvar = i10_name_var("A", ++g_i10_counter);
  h->yvar = i10_name_var("B", ++g_i10_counter);
  h->zvar = i10_name_var("C", ++g_i10_counter);
  I10Vec body = {NULL, 0, 0};
  for (int i = 0; i < prob->n; i++) {
    I10Clause *c = (I10Clause *)prob->v[i];
    I10Atom *head = NULL;
    for (int j = 0; j < c->n; j++) {
      if (!c->l[j]->neg) {
        head = c->l[j]->a;
      }
    }
    if (head == NULL) {
      i10_push(&body, c);
      continue;
    }
    for (int j = 0; j < c->n; j++) {
      if (c->l[j]->neg) {
        head = i10_encode(head, c->l[j]->a);
      }
    }
    I10Lit *l = i10_lit(0, head);
    i10_push(&body, i10_clause(c->tag, c->role, &l, 1));
  }
  I10Set *ds = NULL;
  for (int i = 0; i < h->ifeqs.n; i++) {
    ds = i10_sinsert(i10_cmp_name_v, ((I10Fun *)h->ifeqs.v[i])->name, ds);
  }
  I10Vec dn = {NULL, 0, 0};
  i10_stolist(ds, &dn);
  for (int i = 0; i < dn.n; i++) {
    I10Fun *f = NULL;
    for (int j = 0; j < h->ifeqs.n && f == NULL; j++) {
      if (((I10Fun *)h->ifeqs.v[j])->name == dn.v[i]) {
        f = (I10Fun *)h->ifeqs.v[j];
      }
    }
    I10Term *x = i10_var_term(h->xvar);
    I10Term *y = i10_var_term(h->yvar);
    I10Term *z = i10_var_term(h->zvar);
    I10Term *args[4] = {x, x, y, z};
    I10Lit *l = i10_lit(0, i10_atom(1, i10_app(f, args, 4), y));
    i10_push(out, i10_clause("ifeq_axiom", "axiom", &l, 1));
  }
  i10_append(out, &body);
}

// --- output: TPTP.Print.prettyNames and the TPTP layout -----------------

static void i10_collect_names(I10Term *t, I10Vec *acc) {
  if (t->var != NULL) {
    return;
  }
  if (t->fun->name->pretty == NULL) {
    t->fun->name->pretty = "";
    i10_push(acc, t->fun->name);
  }
  for (int i = 0; i < t->nargs; i++) {
    i10_collect_names(t->args[i], acc);
  }
}

static const char *i10_candidate(const I10Name *n, int k, const char **taken) {
  *taken = NULL;
  if (n->sk_prefix != NULL) {
    *taken = i10_fmt("%s%d", n->sk_prefix, k + 1);
    const char *tag = (n->sk_tag != NULL) ? n->sk_tag : "";
    const char *s = n->base;
    return i10_fmt("%s%d%s%s%s%s", n->sk_prefix, k + 1, (*tag != '\0') ? "_" : "", tag,
                   (*s != '\0') ? "_" : "", s);
  }
  if (k == 0) {
    return n->base;
  }
  size_t len = strlen(n->base);
  int digit = (len > 0 && isdigit((unsigned char)n->base[len - 1]));
  return i10_fmt("%s%s%d", n->base, digit ? "_" : "", k + 1);
}

static const char *i10_choose(const I10Name *n, const I10StrSet *used, const I10Vec *local) {
  for (int k = 0;; k++) {
    const char *taken = NULL;
    const char *w = i10_candidate(n, k, &taken);
    int clash = i10_ss_has(used, w) || (taken != NULL && i10_ss_has(used, taken));
    for (int i = 0; local != NULL && i < local->n && !clash; i++) {
      clash = (strcmp((const char *)local->v[i], w) == 0);
    }
    if (!clash) {
      return w;
    }
  }
}

static void i10_put_atom(FILE *out, const char *s) {
  const char *t = s;
  while (*t == '$') {
    t++;
  }
  int plain = (*t != '\0') && islower((unsigned char)*t);
  for (const char *q = t; plain && *q != '\0'; q++) {
    plain = isalnum((unsigned char)*q) || *q == '_';
  }
  if (plain) {
    fputs(s, out);
    return;
  }
  fputc('\'', out);
  for (const char *q = s; *q != '\0'; q++) {
    if (*q == '\'' || *q == '\\') {
      fputc('\\', out);
    }
    fputc(*q, out);
  }
  fputc('\'', out);
}

typedef struct I10VarNames {
  I10Name **var;
  const char **name;
  int n;
} I10VarNames;

static void i10_put_term(FILE *out, I10Term *t, const I10VarNames *vn) {
  if (t->var != NULL) {
    for (int i = 0; i < vn->n; i++) {
      if (vn->var[i]->id == t->var->id) {
        fputs(vn->name[i], out);
        return;
      }
    }
    fputs(t->var->base, out);
    return;
  }
  const I10Name *n = t->fun->name;
  i10_put_atom(out, (n->kind == I10_FIXED) ? n->base : n->pretty);
  if (t->nargs > 0) {
    fputc('(', out);
    for (int i = 0; i < t->nargs; i++) {
      if (i > 0) {
        fputc(',', out);
      }
      i10_put_term(out, t->args[i], vn);
    }
    fputc(')', out);
  }
}

static void i10_print(FILE *out, I10Vec *clauses, const char *source, const char *status,
                      const char *spc) {
  // Global names: every function symbol of the output, generated ones
  // named in descending Name order against the input's own symbols.
  I10Vec names = {NULL, 0, 0};
  for (int i = 0; i < clauses->n; i++) {
    I10Clause *c = (I10Clause *)clauses->v[i];
    for (int j = 0; j < c->n; j++) {
      i10_collect_names(c->l[j]->a->l, &names);
      i10_collect_names(c->l[j]->a->r, &names);
    }
  }
  I10StrSet used = {NULL, 0, 0};
  i10_ss_add(&used, "$i");
  for (int i = 0; i < names.n; i++) {
    I10Name *n = (I10Name *)names.v[i];
    if (n->kind == I10_FIXED) {
      i10_ss_add(&used, n->base);
    }
  }
  I10Set *sorted = i10_sfromlist(i10_cmp_name_v, names.v, names.n);
  I10Vec order = {NULL, 0, 0};
  i10_stolist(sorted, &order);
  for (int i = order.n - 1; i >= 0; i--) {
    I10Name *n = (I10Name *)order.v[i];
    if (n->kind == I10_FIXED) {
      continue;
    }
    const char *taken = NULL;
    const char *w = NULL;
    for (int k = 0;; k++) {
      w = i10_candidate(n, k, &taken);
      if (!i10_ss_has(&used, w) && (taken == NULL || !i10_ss_has(&used, taken))) {
        break;
      }
    }
    n->pretty = w;
    i10_ss_add(&used, w);
    if (taken != NULL) {
      i10_ss_add(&used, taken);
    }
  }

  fprintf(out, "%%------------------------------------------------------------------------------\n");
  fprintf(out, "%% File     : %s : FindProof --emit-ifeq10\n", source);
  if (status != NULL) {
    fprintf(out, "%% Status   : %s\n", status);
    fprintf(out, "%% SPC      : CNF_%s_RFO_PEQ_UEQ\n", spc);
  }
  // No dispatcher, tuple or predicate: the source is unit equality
  // already and the encoding adds nothing (make_ifeq10_corpus.sh does
  // not emit it).  Definitions are the clausifier's sP<n> predicates.
  fprintf(out, "%% Encoding : dispatchers %d, tuples %d, predicates %d, definitions %d\n",
          g_i10_h.ifeqs.n, g_i10_h.tuples.n, g_i10_h.npreds, g_i10_ndefs);
  fprintf(out, "%% Comments : Converted to UEQ with TPTP's -10 scheme [CS18] (a port of\n");
  fprintf(out, "%%            jukebox horn-to-unit, no conjunctive or disjunctive goals).\n");
  fprintf(out, "%%------------------------------------------------------------------------------\n");

  // Variables: Form.uniqueNames over the whole problem renames a variable
  // already bound by an earlier clause; within a clause the newest
  // variable claims its base name first.
  I10IdSet seen = {NULL, 0, 0};
  I10StrMap counts = {NULL, NULL, 0, 0};
  for (int i = 0; i < clauses->n; i++) {
    I10Clause *c = (I10Clause *)clauses->v[i];
    I10Vec acc = {NULL, 0, 0};
    for (int j = 0; j < c->n; j++) {
      i10_lit_vars(c->l[j], &acc);
    }
    I10VSet vs = i10_vs_of((I10Name **)acc.v, acc.n);
    I10Name **byage = (I10Name **)i10_alloc(sizeof(I10Name *) * (size_t)(vs.n + 1));
    int nb = 0;
    for (int j = vs.n - 1; j >= 0; j--) {
      if (i10_ids_has(&seen, vs.v[j]->id)) {
        byage[nb++] = vs.v[j];
      }
    }
    for (int j = vs.n - 1; j >= 0; j--) {
      if (!i10_ids_has(&seen, vs.v[j]->id)) {
        byage[nb++] = vs.v[j];
      }
    }
    for (int j = 0; j < vs.n; j++) {
      i10_ids_add(&seen, vs.v[j]->id);
    }
    I10VarNames vn;
    vn.var = byage;
    vn.n = nb;
    vn.name = (const char **)i10_alloc(sizeof(char *) * (size_t)(nb + 1));
    I10Vec local = {NULL, 0, 0};
    for (int j = 0; j < nb; j++) {
      vn.name[j] = i10_choose(byage[j], &used, &local);
      i10_push(&local, (void *)vn.name[j]);
    }

    // TPTP's own renaming of repeated names (the dispatchers are all
    // ifeq_axiom): the k-th repeat gets the suffix _00k.
    int *dup = i10_sm_slot(&counts, c->tag);
    const char *name = (*dup > 0) ? i10_fmt("%s_%03d", c->tag, *dup) : c->tag;
    (*dup)++;

    fputs("cnf(", out);
    i10_put_atom(out, name);
    fprintf(out, ",%s,\n    ", c->role);
    if (c->n == 0) {
      fputs("$false", out);
    }
    for (int j = 0; j < c->n; j++) {
      if (j > 0) {
        fputs(" | ", out);
      }
      i10_put_term(out, c->l[j]->a->l, &vn);
      fputs(c->l[j]->neg ? " != " : " = ", out);
      i10_put_term(out, c->l[j]->a->r, &vn);
    }
    fputs(" ).\n\n", out);
  }
  fprintf(out, "%%------------------------------------------------------------------------------\n");
}

// --- parser (TPTP.Parse.Core: cnf, fof, include) ---------------------------

enum {
  I10T_EOF, I10T_ATOM, I10T_VAR, I10T_NUM, I10T_DOLLAR, I10T_DQ,
  I10T_LP, I10T_RP, I10T_LB, I10T_RB, I10T_COMMA, I10T_DOT, I10T_COLON,
  I10T_OR, I10T_AND, I10T_NOT, I10T_IFF, I10T_IMP, I10T_FOL, I10T_XOR,
  I10T_NOR, I10T_NAND, I10T_EQ, I10T_NEQ, I10T_ALL, I10T_EX, I10T_STAR,
  I10T_OTHER
};

typedef struct I10Parser {
  const char *p;
  const char *file;
  int line;
  int tok;
  const char *text;
} I10Parser;

typedef struct I10Scope {
  int cnf;
  I10Vec names;
  I10Vec vars;
} I10Scope;

typedef struct I10Sel {
  const char **names;
  int n;
} I10Sel;

static const char *g_i10_path;

static _Noreturn void i10_syntax(const I10Parser *ps, const char *what) {
  i10_fail(I10_RC_INPUT, "%s:%d: %s", ps->file, ps->line, what);
}

static int i10_word_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
}

static void i10_lex(I10Parser *ps) {
  const char *p = ps->p;
  for (;;) {
    while (*p != '\0' && isspace((unsigned char)*p)) {
      if (*p == '\n') {
        ps->line++;
      }
      p++;
    }
    if (*p == '%') {
      while (*p != '\0' && *p != '\n') {
        p++;
      }
      continue;
    }
    if (p[0] == '/' && p[1] == '*') {
      p += 2;
      while (*p != '\0' && !(p[0] == '*' && p[1] == '/')) {
        if (*p == '\n') {
          ps->line++;
        }
        p++;
      }
      if (*p != '\0') {
        p += 2;
      }
      continue;
    }
    break;
  }
  ps->text = NULL;
  const char *s = p;
  char c = *p;
  if (c == '\0') {
    ps->tok = I10T_EOF;
  } else if (islower((unsigned char)c) || isupper((unsigned char)c) || isdigit((unsigned char)c)) {
    while (i10_word_char(*p)) {
      p++;
    }
    ps->text = i10_strn(s, (size_t)(p - s));
    ps->tok = islower((unsigned char)c) ? I10T_ATOM : isupper((unsigned char)c) ? I10T_VAR : I10T_NUM;
  } else if (c == '$') {
    while (*p == '$') {
      p++;
    }
    while (i10_word_char(*p)) {
      p++;
    }
    ps->text = i10_strn(s, (size_t)(p - s));
    ps->tok = I10T_DOLLAR;
  } else if (c == '\'' || c == '"') {
    const char *e = p + 1;
    while (*e != '\0' && *e != c) {
      e += (*e == '\\' && e[1] != '\0') ? 2 : 1;
    }
    if (*e != c) {
      ps->p = e;
      i10_syntax(ps, "unterminated quoted token");
    }
    char *buf = (char *)i10_alloc((size_t)(e - p));
    size_t n = 0;
    for (p++; p < e; p++) {
      if (*p == '\\' && p + 1 < e) {
        p++;
      }
      if (*p == '\n') {
        ps->line++;
      }
      buf[n++] = *p;
    }
    p = e + 1;
    buf[n] = '\0';
    ps->text = buf;
    ps->tok = (c == '\'') ? I10T_ATOM : I10T_DQ;
  } else if (strncmp(p, "<=>", 3) == 0) {
    p += 3;
    ps->tok = I10T_IFF;
  } else if (strncmp(p, "<~>", 3) == 0) {
    p += 3;
    ps->tok = I10T_XOR;
  } else if (strncmp(p, "=>", 2) == 0) {
    p += 2;
    ps->tok = I10T_IMP;
  } else if (strncmp(p, "<=", 2) == 0) {
    p += 2;
    ps->tok = I10T_FOL;
  } else if (strncmp(p, "~|", 2) == 0) {
    p += 2;
    ps->tok = I10T_NOR;
  } else if (strncmp(p, "~&", 2) == 0) {
    p += 2;
    ps->tok = I10T_NAND;
  } else if (strncmp(p, "!=", 2) == 0) {
    p += 2;
    ps->tok = I10T_NEQ;
  } else {
    p++;
    switch (c) {
      case '(': ps->tok = I10T_LP; break;
      case ')': ps->tok = I10T_RP; break;
      case '[': ps->tok = I10T_LB; break;
      case ']': ps->tok = I10T_RB; break;
      case ',': ps->tok = I10T_COMMA; break;
      case '.': ps->tok = I10T_DOT; break;
      case ':': ps->tok = I10T_COLON; break;
      case '|': ps->tok = I10T_OR; break;
      case '&': ps->tok = I10T_AND; break;
      case '~': ps->tok = I10T_NOT; break;
      case '=': ps->tok = I10T_EQ; break;
      case '!': ps->tok = I10T_ALL; break;
      case '?': ps->tok = I10T_EX; break;
      case '*': ps->tok = I10T_STAR; break;
      default: ps->tok = I10T_OTHER; break;
    }
  }
  ps->p = p;
}

static void i10_expect(I10Parser *ps, int tok, const char *what) {
  if (ps->tok != tok) {
    i10_syntax(ps, i10_fmt("expected %s", what));
  }
  i10_lex(ps);
}

static I10Name *i10_scope_find(I10Scope *sc, const char *name) {
  for (int i = sc->names.n - 1; i >= 0; i--) {
    if (strcmp((const char *)sc->names.v[i], name) == 0) {
      return (I10Name *)sc->vars.v[i];
    }
  }
  return NULL;
}

static I10Term *i10_p_term(I10Parser *ps, I10Scope *sc);

static I10Term *i10_p_var(I10Parser *ps, I10Scope *sc) {
  I10Name *v = i10_scope_find(sc, ps->text);
  if (v == NULL) {
    if (!sc->cnf) {
      i10_syntax(ps, i10_fmt("unbound variable %s", ps->text));
    }
    v = i10_name_var(ps->text, ++g_i10_counter);
    i10_push(&sc->names, (void *)ps->text);
    i10_push(&sc->vars, v);
  }
  i10_lex(ps);
  return i10_var_term(v);
}

// An application's name and arguments; the caller decides whether it
// is a term or a predicate atom.
static int i10_p_args(I10Parser *ps, I10Scope *sc, I10Vec *args) {
  if (ps->tok != I10T_LP) {
    return 0;
  }
  i10_lex(ps);
  for (;;) {
    i10_push(args, i10_p_term(ps, sc));
    if (ps->tok == I10T_COMMA) {
      i10_lex(ps);
      continue;
    }
    i10_expect(ps, I10T_RP, "')' after arguments");
    return args->n;
  }
}

static I10Term *i10_p_term(I10Parser *ps, I10Scope *sc) {
  switch (ps->tok) {
    case I10T_VAR:
      return i10_p_var(ps, sc);
    case I10T_ATOM: {
      const char *name = ps->text;
      i10_lex(ps);
      I10Vec args = {NULL, 0, 0};
      int n = i10_p_args(ps, sc, &args);
      return i10_app(i10_symbol(name, n, 0), (I10Term **)args.v, n);
    }
    case I10T_NUM:
    case I10T_DQ:
    case I10T_DOLLAR:
      i10_syntax(ps, "numbers, distinct objects and $-symbols are not supported");
    default:
      i10_syntax(ps, "expected a term");
  }
}

static I10Form *i10_p_formula(I10Parser *ps, I10Scope *sc);
static I10Form *i10_p_unitary(I10Parser *ps, I10Scope *sc);

static I10Form *i10_p_equation(I10Parser *ps, I10Scope *sc, I10Term *lhs) {
  int neg = (ps->tok == I10T_NEQ);
  i10_lex(ps);
  I10Term *rhs = i10_p_term(ps, sc);
  return i10_flit(i10_lit(neg, i10_atom(1, lhs, rhs)));
}

static I10Form *i10_p_literal(I10Parser *ps, I10Scope *sc) {
  switch (ps->tok) {
    case I10T_DOLLAR: {
      int truth = (strcmp(ps->text, "$true") == 0);
      if (!truth && strcmp(ps->text, "$false") != 0) {
        i10_syntax(ps, i10_fmt("unsupported symbol %s", ps->text));
      }
      i10_lex(ps);
      return truth ? i10_ftrue() : i10_ffalse();
    }
    case I10T_LP: {
      i10_lex(ps);
      I10Form *f = i10_p_formula(ps, sc);
      i10_expect(ps, I10T_RP, "')'");
      if (ps->tok == I10T_EQ || ps->tok == I10T_NEQ) {
        i10_syntax(ps, "a formula cannot be an equation side");
      }
      return f;
    }
    case I10T_VAR: {
      I10Term *t = i10_p_var(ps, sc);
      if (ps->tok != I10T_EQ && ps->tok != I10T_NEQ) {
        i10_syntax(ps, "a variable is not a formula");
      }
      return i10_p_equation(ps, sc, t);
    }
    case I10T_ATOM: {
      const char *name = ps->text;
      i10_lex(ps);
      I10Vec args = {NULL, 0, 0};
      int n = i10_p_args(ps, sc, &args);
      if (ps->tok == I10T_EQ || ps->tok == I10T_NEQ) {
        return i10_p_equation(ps, sc, i10_app(i10_symbol(name, n, 0), (I10Term **)args.v, n));
      }
      I10Term *p = i10_app(i10_symbol(name, n, 1), (I10Term **)args.v, n);
      return i10_flit(i10_lit(0, i10_atom(0, p, NULL)));
    }
    default:
      i10_syntax(ps, "expected a literal");
  }
}

static I10Form *i10_p_unitary(I10Parser *ps, I10Scope *sc) {
  switch (ps->tok) {
    case I10T_NOT:
      i10_lex(ps);
      return i10_f1(I10F_NOT, i10_p_unitary(ps, sc));
    case I10T_ALL:
    case I10T_EX: {
      int kind = (ps->tok == I10T_ALL) ? I10F_ALL : I10F_EX;
      if (sc->cnf) {
        i10_syntax(ps, "a quantifier in a CNF clause");
      }
      i10_lex(ps);
      i10_expect(ps, I10T_LB, "'['");
      I10Vec vars = {NULL, 0, 0};
      int mark = sc->names.n;
      for (;;) {
        if (ps->tok != I10T_VAR) {
          i10_syntax(ps, "expected a bound variable");
        }
        I10Name *v = i10_name_var(ps->text, g_i10_counter++);
        i10_push(&vars, v);
        i10_lex(ps);
        if (ps->tok == I10T_COLON) {
          i10_syntax(ps, "typed quantification is not supported");
        }
        if (ps->tok != I10T_COMMA) {
          break;
        }
        i10_lex(ps);
      }
      i10_expect(ps, I10T_RB, "']'");
      i10_expect(ps, I10T_COLON, "':'");
      for (int i = 0; i < vars.n; i++) {
        i10_push(&sc->names, (void *)((I10Name *)vars.v[i])->base);
        i10_push(&sc->vars, vars.v[i]);
      }
      I10Form *body = i10_p_unitary(ps, sc);
      sc->names.n = mark;
      sc->vars.n = mark;
      return i10_fq(kind, i10_vs_of((I10Name **)vars.v, vars.n), body);
    }
    default:
      return i10_p_literal(ps, sc);
  }
}

static I10Form *i10_p_formula(I10Parser *ps, I10Scope *sc) {
  I10Form *x = i10_p_unitary(ps, sc);
  int tok = ps->tok;
  switch (tok) {
    case I10T_AND:
    case I10T_OR: {
      i10_lex(ps);
      I10Form *y = i10_p_formula(ps, sc);
      return i10_f2((tok == I10T_AND) ? I10F_AND : I10F_OR, 0, x, y);
    }
    case I10T_IFF:
      i10_lex(ps);
      return i10_f2(I10F_EQUIV, 0, x, i10_p_formula(ps, sc));
    case I10T_IMP:
    case I10T_FOL:
    case I10T_XOR:
    case I10T_NOR:
    case I10T_NAND: {
      int conn = (tok == I10T_IMP) ? I10C_IMPLIES : (tok == I10T_FOL) ? I10C_FOLLOWS
                 : (tok == I10T_XOR) ? I10C_XOR : (tok == I10T_NOR) ? I10C_NOR : I10C_NAND;
      i10_lex(ps);
      return i10_f2(I10F_CONN, conn, x, i10_p_formula(ps, sc));
    }
    default:
      return x;
  }
}

static void i10_skip_balanced(I10Parser *ps) {
  int depth = 0;
  while (ps->tok != I10T_EOF && !(depth == 0 && ps->tok == I10T_RP)) {
    if (ps->tok == I10T_LP) {
      depth++;
    } else if (ps->tok == I10T_RP) {
      depth--;
    }
    i10_lex(ps);
  }
}

static int i10_selected(const I10Sel *sel, const char *name) {
  if (sel == NULL) {
    return 1;
  }
  for (int i = 0; i < sel->n; i++) {
    if (strcmp(sel->names[i], name) == 0) {
      return 1;
    }
  }
  return 0;
}

static char *i10_read_file(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    return NULL;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }
  long n = ftell(fp);
  if (n < 0 || fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return NULL;
  }
  char *buf = (char *)malloc((size_t)n + 1u);
  if (buf == NULL) {
    fclose(fp);
    return NULL;
  }
  size_t rd = fread(buf, 1, (size_t)n, fp);
  fclose(fp);
  buf[rd] = '\0';
  return buf;
}

// include('F') resolves against dirname(problem), $TPTP, then the CWD,
// as FindProof's own front end does (tptp_include.c).
static char *i10_read_include(const char *inc) {
  if (g_i10_path == NULL) {
    return NULL;
  }
  const char *slash = strrchr(g_i10_path, '/');
  char *buf = NULL;
  if (slash != NULL) {
    buf = i10_read_file(i10_fmt("%.*s/%s", (int)(slash - g_i10_path), g_i10_path, inc));
  }
  const char *tptp = getenv("TPTP");
  if (buf == NULL && tptp != NULL) {
    buf = i10_read_file(i10_fmt("%s/%s", tptp, inc));
  }
  if (buf == NULL) {
    buf = i10_read_file(inc);
  }
  return buf;
}

static void i10_parse_text(const char *text, const char *file, const I10Sel *sel, int depth);

static void i10_p_include(I10Parser *ps, const I10Sel *sel, int depth) {
  i10_expect(ps, I10T_LP, "'(' after include");
  if (ps->tok != I10T_ATOM) {
    i10_syntax(ps, "expected a quoted file name");
  }
  const char *inc = ps->text;
  i10_lex(ps);
  I10Sel *sub = NULL;
  if (ps->tok == I10T_COMMA) {
    i10_lex(ps);
    if (ps->tok == I10T_STAR) {
      i10_lex(ps);
    } else {
      i10_expect(ps, I10T_LB, "'[' or '*' after the file name");
      I10Vec names = {NULL, 0, 0};
      while (ps->tok != I10T_RB) {
        if (ps->tok != I10T_ATOM && ps->tok != I10T_NUM) {
          i10_syntax(ps, "expected a formula name");
        }
        i10_push(&names, (void *)ps->text);
        i10_lex(ps);
        if (ps->tok == I10T_COMMA) {
          i10_lex(ps);
        }
      }
      i10_lex(ps);
      sub = (I10Sel *)i10_alloc(sizeof(I10Sel));
      sub->names = (const char **)names.v;
      sub->n = names.n;
    }
  }
  i10_expect(ps, I10T_RP, "')'");
  i10_expect(ps, I10T_DOT, "'.'");
  const I10Sel *merged = sel;
  if (sub != NULL && sel != NULL) {
    I10Sel *m = (I10Sel *)i10_alloc(sizeof(I10Sel));
    m->names = (const char **)i10_alloc(sizeof(char *) * (size_t)(sel->n + 1));
    for (int i = 0; i < sel->n; i++) {
      if (i10_selected(sub, sel->names[i])) {
        m->names[m->n++] = sel->names[i];
      }
    }
    merged = m;
  } else if (sub != NULL) {
    merged = sub;
  }
  if (depth >= I10_MAX_INCLUDE_DEPTH) {
    i10_syntax(ps, "includes nested too deep");
  }
  char *buf = i10_read_include(inc);
  if (buf == NULL) {
    i10_syntax(ps, i10_fmt("cannot read include '%s'", inc));
  }
  char *text = i10_strn(buf, strlen(buf));
  free(buf);
  i10_parse_text(text, inc, merged, depth + 1);
}

static void i10_parse_text(const char *text, const char *file, const I10Sel *sel, int depth) {
  I10Parser ps;
  ps.p = text;
  ps.file = file;
  ps.line = 1;
  ps.text = NULL;
  i10_lex(&ps);
  while (ps.tok != I10T_EOF) {
    if (ps.tok != I10T_ATOM) {
      i10_syntax(&ps, "expected cnf, fof or include");
    }
    const char *kw = ps.text;
    i10_lex(&ps);
    if (strcmp(kw, "include") == 0) {
      i10_p_include(&ps, sel, depth);
      continue;
    }
    int cnf = (strcmp(kw, "cnf") == 0);
    if (!cnf && strcmp(kw, "fof") != 0) {
      i10_syntax(&ps, i10_fmt("unsupported record '%s' (CNF and FOF only)", kw));
    }
    i10_expect(&ps, I10T_LP, "'('");
    if (ps.tok != I10T_ATOM && ps.tok != I10T_NUM) {
      i10_syntax(&ps, "expected a formula name");
    }
    const char *tag = ps.text;
    i10_lex(&ps);
    i10_expect(&ps, I10T_COMMA, "','");
    if (!i10_selected(sel, tag)) {
      i10_skip_balanced(&ps);
      i10_expect(&ps, I10T_RP, "')'");
      i10_expect(&ps, I10T_DOT, "'.'");
      continue;
    }
    if (ps.tok != I10T_ATOM) {
      i10_syntax(&ps, "expected a role");
    }
    const char *role = ps.text;
    int conj = 0;
    if (strcmp(role, "conjecture") == 0 || strcmp(role, "question") == 0) {
      conj = 1;
    } else if (strcmp(role, "axiom") != 0 && strcmp(role, "hypothesis") != 0 &&
               strcmp(role, "definition") != 0 && strcmp(role, "assumption") != 0 &&
               strcmp(role, "lemma") != 0 && strcmp(role, "theorem") != 0 &&
               strcmp(role, "negated_conjecture") != 0) {
      i10_syntax(&ps, i10_fmt("unsupported role '%s'", role));
    }
    i10_lex(&ps);
    i10_expect(&ps, I10T_COMMA, "','");
    I10Scope sc;
    memset(&sc, 0, sizeof sc);
    sc.cnf = cnf;
    I10Form *f = i10_p_formula(&ps, &sc);
    if (cnf) {
      I10VSet fv = i10_free(f);
      if (fv.n > 0) {
        f = i10_fq(I10F_ALL, fv, f);
      }
    }
    if (ps.tok == I10T_COMMA) {
      i10_lex(&ps);
      i10_skip_balanced(&ps);
    }
    i10_expect(&ps, I10T_RP, "')'");
    i10_expect(&ps, I10T_DOT, "'.'");
    I10Input *in = (I10Input *)i10_alloc(sizeof(I10Input));
    in->tag = tag;
    in->role = role;
    in->conj = conj;
    in->f = f;
    i10_push(&g_i10_inputs, in);
  }
}

// --- entry points ---------------------------------------------------------

static const char *g_i10_text;
static FILE *g_i10_out;

static void i10_reset(void) {
  i10_arena_free();
  g_i10_interned = 0;
  g_i10_counter = 0;
  g_i10_bulk_done = 0;
  g_i10_ndefs = 0;
  g_i10_tag = "";
  memset(&g_i10_inputs, 0, sizeof g_i10_inputs);
  memset(&g_i10_funs, 0, sizeof g_i10_funs);
  memset(&g_i10_h, 0, sizeof g_i10_h);
  g_i10_msg[0] = '\0';
}

// The status of the encoded problem, from the Status line of the source's
// header (NULL when it has none): the encoding is equisatisfiable, and a
// theorem's negated conjecture makes it unsatisfiable.  *spc gets the
// matching TPTP SPC code, which pool tools (np0_pool.py) read.
static const char *i10_status(const char *text, const char **spc) {
  static const char *const k_map[][3] = {
    {"Theorem", "Unsatisfiable", "UNS"},
    {"Unsatisfiable", "Unsatisfiable", "UNS"},
    {"ContradictoryAxioms", "Unsatisfiable", "UNS"},
    {"CounterSatisfiable", "Satisfiable", "SAT"},
    {"Satisfiable", "Satisfiable", "SAT"},
    {"Unknown", "Unknown", "UNK"},
    {"Open", "Open", "OPN"},
  };
  const char *p = text;
  while (*p != '\0') {
    const char *q = p;
    while (*q == ' ' || *q == '\t' || *q == '\r') {
      q++;
    }
    if (*q != '%' && *q != '\n' && *q != '\0') {
      return NULL;
    }
    if (*q == '%') {
      for (q++; *q == ' ' || *q == '\t'; q++) {
      }
      if (strncmp(q, "Status", 6) == 0) {
        for (q += 6; *q == ' ' || *q == '\t' || *q == ':'; q++) {
        }
        size_t n = 0;
        while (isalpha((unsigned char)q[n])) {
          n++;
        }
        for (size_t i = 0; i < sizeof k_map / sizeof k_map[0]; i++) {
          if (strlen(k_map[i][0]) == n && strncmp(q, k_map[i][0], n) == 0) {
            *spc = k_map[i][2];
            return k_map[i][1];
          }
        }
        return NULL;
      }
    }
    const char *eol = strchr(q, '\n');
    if (eol == NULL) {
      break;
    }
    p = eol + 1;
  }
  return NULL;
}

// The pipeline over g_i10_text; every failure longjmps to ifeq10_encode,
// which is why its inputs travel in globals rather than through locals
// live across the setjmp.
static void i10_run(void) {
  i10_reset();
  const char *source = "problem";
  if (g_i10_path != NULL) {
    const char *slash = strrchr(g_i10_path, '/');
    source = (slash != NULL) ? slash + 1 : g_i10_path;
  }
  i10_parse_text(i10_strn(g_i10_text, strlen(g_i10_text)),
                 (g_i10_path != NULL) ? g_i10_path : "<input>", NULL, 0);
  I10Vec prob = {NULL, 0, 0};
  i10_clausify(&prob);
  I10Vec out_clauses = {NULL, 0, 0};
  i10_horn_to_unit(&prob, &out_clauses);
  const char *spc = NULL;
  const char *status = i10_status(g_i10_text, &spc);
  i10_print(g_i10_out, &out_clauses, source, status, spc);
}

// Encodes the problem text (path: where includes resolve from, or NULL
// for none) and writes the -10 file to out.  Returns an I10_RC_* code;
// on failure g_i10_msg says why, and every failure but running out of
// memory while printing happens before the first byte is written.
static int ifeq10_encode(const char *text, const char *path, FILE *out) {
  g_i10_text = text;
  g_i10_path = path;
  g_i10_out = out;
  if (setjmp(g_i10_jb) != 0) {
    i10_arena_free();
    return g_i10_rc;
  }
  i10_run();
  i10_arena_free();
  return I10_RC_OK;
}

static int ifeq10_main(const char *path) {
  char *text = i10_read_file(path);
  if (text == NULL) {
    fprintf(stderr, "FindProof --emit-ifeq10: cannot read %s\n", path);
    return I10_RC_INPUT;
  }
  int rc = ifeq10_encode(text, path, stdout);
  free(text);
  if (rc == I10_RC_SAT1) {
    printf("%% SZS status Satisfiable for %s\n", path);
  } else if (rc != I10_RC_OK) {
    fprintf(stderr, "FindProof --emit-ifeq10: %s\n", g_i10_msg);
  }
  if (fflush(stdout) != 0 || ferror(stdout)) {
    fprintf(stderr, "FindProof --emit-ifeq10: write error on standard output\n");
    return I10_RC_INPUT;
  }
  return rc;
}
