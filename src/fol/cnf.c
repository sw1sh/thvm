// FOL formula -> CNF preprocessing pipeline.
//
// thvm's FOL clausal layer (src/fol/_.c) consumes FolClauses directly.
// Real-world FOL inputs are formulas with quantifiers + connectives.
// The pipeline:
//   1. NNF -- push negations down to atoms; eliminate `->` and `<->`.
//   2. Skolemize -- replace ∃-bound variables with Skolem functions
//      whose arguments are the enclosing ∀-bound variables.  After
//      this step every variable is implicitly universal.
//   3. Distribute ∨ over ∧ to reach clausal form.
//   4. Extract the conjunction-of-disjunctions as FolClause*.
//
// Formulas are encoded as Term trees with these reserved CTR labels.
// Predicate / function symbols live OUTSIDE this range (callers pick
// freely from labels < FOL_LAB_NOT).  Equality keeps the existing
// FOL_LAB_EQ = 0 convention.
//
// This tick lands NNF + the formula-label conventions; the rest of
// the pipeline (Skolemize, distribute, extract) follows in tick 8.

// Reserved labels.  Chosen high so they don't collide with typical
// user-picked predicate / function labels in tests (which sit in the
// low double-digit range).
#define FOL_LAB_NOT 0xFFF0u   // unary: ¬p
#define FOL_LAB_AND 0xFFF1u   // n-ary: p1 ∧ p2 ∧ ... ∧ pn
#define FOL_LAB_OR  0xFFF2u   // n-ary: p1 ∨ p2 ∨ ... ∨ pn
#define FOL_LAB_IMP 0xFFF3u   // binary: p -> q
#define FOL_LAB_IFF 0xFFF4u   // binary: p <-> q
#define FOL_LAB_ALL 0xFFF5u   // binary: (var, body), var = TAG_FVR
#define FOL_LAB_EX  0xFFF6u   // binary: (var, body)

fn u8 fol_is_connective(u32 label) {
  return (label >= FOL_LAB_NOT && label <= FOL_LAB_EX) ? 1u : 0u;
}

// Constructors -- pure thin wrappers over term_new_ctr.
fn Term fol_mk_not(Term p) {
  Term k[1] = { p };
  return term_new_ctr(FOL_LAB_NOT, k, 1u);
}
fn Term fol_mk_and(Term p, Term q) {
  Term k[2] = { p, q };
  return term_new_ctr(FOL_LAB_AND, k, 2u);
}
fn Term fol_mk_or(Term p, Term q) {
  Term k[2] = { p, q };
  return term_new_ctr(FOL_LAB_OR, k, 2u);
}
fn Term fol_mk_imp(Term p, Term q) {
  Term k[2] = { p, q };
  return term_new_ctr(FOL_LAB_IMP, k, 2u);
}
fn Term fol_mk_iff(Term p, Term q) {
  Term k[2] = { p, q };
  return term_new_ctr(FOL_LAB_IFF, k, 2u);
}
fn Term fol_mk_all(Term var, Term body) {
  Term k[2] = { var, body };
  return term_new_ctr(FOL_LAB_ALL, k, 2u);
}
fn Term fol_mk_ex(Term var, Term body) {
  Term k[2] = { var, body };
  return term_new_ctr(FOL_LAB_EX, k, 2u);
}

// === explicit work stacks ===========================================
//
// Every walker below that descends a FORMULA keeps its pending work in
// heap arrays: a formula's depth is its input nesting -- HWV061+1 is a
// 89k-conjunct chain under a 28k-binder prefix, 117k levels -- and one C
// frame per level overflowed the 8 MB stack.  Each walker owns its
// arrays (grown on demand, kept across calls; no walker re-enters
// itself).  Children are pushed in reverse so they are taken left to
// right: every allocation, Skolem label and Tseitin name comes out in
// the order of the recursive walkers these replace, as clang built
// them (left-to-right argument evaluation).

static void *fol_grow(void *a, u32 *cap, u32 need, size_t elt) {
  u32 ncap = (*cap == 0u) ? 256u : *cap;
  while (ncap <= need) {
    if (ncap > 0x40000000u) {
      fprintf(stderr, "thvm: out of memory in clausification\n");
      abort();
    }
    ncap *= 2u;
  }
  void *na = realloc(a, (size_t)ncap * elt);
  if (na == NULL) {
    fprintf(stderr, "thvm: out of memory in clausification\n");
    abort();
  }
  *cap = ncap;
  return na;
}

#define FOL_RESERVE(arr, cap, need)                                  \
  do {                                                               \
    if ((need) >= (cap)) {                                           \
      (arr) = fol_grow((arr), &(cap), (need), sizeof *(arr));        \
    }                                                                \
  } while (0)

// A caller that bounds clausification by time installs a poll hook:
// the walkers call it once per 2^16 work items, and it abandons the
// clausification (it does not return) once its deadline has passed.
static void (*g_fol_poll)(void) = NULL;
static u32 g_fol_poll_tick = 0u;

fn void fol_set_poll(void (*hook)(void)) {
  g_fol_poll = hook;
}

static inline void fol_poll(void) {
  if (((++g_fol_poll_tick) & 0xFFFFu) == 0u && g_fol_poll != NULL) {
    g_fol_poll();
  }
}

// Nesting depth of `t` (a leaf is 1), counted up to `cap`: the result
// is min(depth, cap).
typedef struct { Term t; u32 d; } FolDepthItem;
static FolDepthItem *g_fol_dep = NULL;
static u32           g_fol_dep_cap = 0u;

fn u32 fol_depth(Term t, u32 cap) {
  u32 n = 0u, best = 0u;
  FOL_RESERVE(g_fol_dep, g_fol_dep_cap, n);
  g_fol_dep[n++] = (FolDepthItem){ t, 1u };
  while (n > 0u && best < cap) {
    FolDepthItem it = g_fol_dep[--n];
    if (it.d > best) {
      best = it.d;
    }
    if (term_tag(it.t) != TAG_CTR) {
      continue;
    }
    u32 nk = term_ctr_n(it.t);
    for (u32 i = 0; i < nk; i++) {
      FOL_RESERVE(g_fol_dep, g_fol_dep_cap, n);
      g_fol_dep[n++] = (FolDepthItem){ term_ctr_at(it.t, i), it.d + 1u };
    }
  }
  return (best < cap) ? best : cap;
}

// === NNF ============================================================
//
// Push negations down to atoms; eliminate `->` and `<->`.  Returns a
// fresh Term tree whose only `¬` occurrences are on predicate atoms.
//
// Rewrite rules (applied recursively):
//   ¬¬p          -> p
//   ¬(p ∧ q)     -> ¬p ∨ ¬q
//   ¬(p ∨ q)     -> ¬p ∧ ¬q
//   ¬(p -> q)    -> p ∧ ¬q
//   ¬(p <-> q)   -> (p ∧ ¬q) ∨ (¬p ∧ q)
//   ¬(∀x.p)      -> ∃x.¬p
//   ¬(∃x.p)      -> ∀x.¬p
//   p -> q       -> ¬p ∨ q
//   p <-> q      -> (¬p ∨ q) ∧ (¬q ∨ p)
//   p ∧ q        -> nnf(p) ∧ nnf(q)
//   p ∨ q        -> nnf(p) ∨ nnf(q)
//   ∀x.p         -> ∀x.nnf(p)
//   ∃x.p         -> ∃x.nnf(p)
//
// `negate` selects positive (0) or negated (1) descent.  Atoms get
// wrapped in `¬` only when `negate == 1`.
//
// Post-order over an explicit stack: an EVAL task rewrites one node
// under its polarity, pushing the constructor task that joins its
// children's results; the results wait on a value stack.

enum { FOL_NNF_EVAL, FOL_NNF_AND, FOL_NNF_OR, FOL_NNF_ALL, FOL_NNF_EX };

typedef struct { Term t; u8 op; u8 neg; } FolNnfTask;
static FolNnfTask *g_nnf_task = NULL;
static u32         g_nnf_task_cap = 0u;
static Term       *g_nnf_val = NULL;
static u32         g_nnf_val_cap = 0u;

static void fol_nnf_push(u32 *nt, Term t, u8 op, u8 neg) {
  FOL_RESERVE(g_nnf_task, g_nnf_task_cap, *nt);
  g_nnf_task[*nt] = (FolNnfTask){ t, op, neg };
  (*nt)++;
}

static void fol_nnf_val(u32 *nv, Term t) {
  FOL_RESERVE(g_nnf_val, g_nnf_val_cap, *nv);
  g_nnf_val[(*nv)++] = t;
}

fn Term fol_nnf(Term f) {
  u32 nt = 0u, nv = 0u;
  fol_nnf_push(&nt, f, FOL_NNF_EVAL, 0u);
  while (nt > 0u) {
    fol_poll();
    FolNnfTask k = g_nnf_task[--nt];
    switch (k.op) {
      case FOL_NNF_EVAL: {
        Term g = k.t;
        u8 neg = k.neg;
        if (term_tag(g) != TAG_CTR) {
          fol_nnf_val(&nv, neg ? fol_mk_not(g) : g);
          break;
        }
        u32 lab = term_ext(g);
        switch (lab) {
          case FOL_LAB_NOT:
            // ~p under `negate`: flip and descend.
            fol_nnf_push(&nt, term_ctr_at(g, 0u), FOL_NNF_EVAL, (u8)(neg ^ 1u));
            break;
          case FOL_LAB_AND:
          case FOL_LAB_OR: {
            // ~(p & q) -> ~p | ~q ;  ~(p | q) -> ~p & ~q
            u8 is_and = (u8)((lab == FOL_LAB_AND) ^ neg);
            fol_nnf_push(&nt, 0, is_and ? FOL_NNF_AND : FOL_NNF_OR, 0u);
            fol_nnf_push(&nt, term_ctr_at(g, 1u), FOL_NNF_EVAL, neg);
            fol_nnf_push(&nt, term_ctr_at(g, 0u), FOL_NNF_EVAL, neg);
            break;
          }
          case FOL_LAB_IMP:
            // p -> q  <=>  ~p | q
            // ~(p -> q)  <=>  p & ~q
            fol_nnf_push(&nt, 0, neg ? FOL_NNF_AND : FOL_NNF_OR, 0u);
            fol_nnf_push(&nt, term_ctr_at(g, 1u), FOL_NNF_EVAL, neg);
            fol_nnf_push(&nt, term_ctr_at(g, 0u), FOL_NNF_EVAL, (u8)(neg ^ 1u));
            break;
          case FOL_LAB_IFF: {
            // p <-> q  <=>  (~p | q) & (p | ~q)
            // ~(p <-> q)  <=>  (p & ~q) | (~p & q)
            Term p = term_ctr_at(g, 0u);
            Term q = term_ctr_at(g, 1u);
            u8 outer = neg ? FOL_NNF_OR : FOL_NNF_AND;
            u8 inner = neg ? FOL_NNF_AND : FOL_NNF_OR;
            fol_nnf_push(&nt, 0, outer, 0u);
            fol_nnf_push(&nt, 0, inner, 0u);
            fol_nnf_push(&nt, q, FOL_NNF_EVAL, (u8)(neg ^ 1u));
            fol_nnf_push(&nt, p, FOL_NNF_EVAL, neg);
            fol_nnf_push(&nt, 0, inner, 0u);
            fol_nnf_push(&nt, q, FOL_NNF_EVAL, neg);
            fol_nnf_push(&nt, p, FOL_NNF_EVAL, (u8)(neg ^ 1u));
            break;
          }
          case FOL_LAB_ALL:
          case FOL_LAB_EX: {
            // ~(![x] p) -> ?[x] ~p ;  ~(?[x] p) -> ![x] ~p
            u8 is_all = (u8)((lab == FOL_LAB_ALL) ^ neg);
            fol_nnf_push(&nt, term_ctr_at(g, 0u), is_all ? FOL_NNF_ALL : FOL_NNF_EX, 0u);
            fol_nnf_push(&nt, term_ctr_at(g, 1u), FOL_NNF_EVAL, neg);
            break;
          }
          default:
            // Non-connective CTR: a predicate atom.  Wrap in ~ iff negate.
            fol_nnf_val(&nv, neg ? fol_mk_not(g) : g);
            break;
        }
        break;
      }
      case FOL_NNF_AND:
      case FOL_NNF_OR: {
        Term q = g_nnf_val[--nv];
        Term p = g_nnf_val[--nv];
        fol_nnf_val(&nv, (k.op == FOL_NNF_AND) ? fol_mk_and(p, q) : fol_mk_or(p, q));
        break;
      }
      default: {
        Term body = g_nnf_val[--nv];
        fol_nnf_val(&nv, (k.op == FOL_NNF_ALL) ? fol_mk_all(k.t, body)
                                                : fol_mk_ex(k.t, body));
        break;
      }
    }
  }
  return g_nnf_val[0];
}

// === Skolemization ===================================================
//
// Replace every `∃y` with the Skolem term `sk_n(x1, ..., xk)` where
// x1..xk are the enclosing ∀-bound variables in scope.  After this
// step every variable is implicitly universal, so the `∀` wrappers
// also drop -- the result is a quantifier-free formula whose free
// variables are universally quantified at the outermost level.
//
// Fresh Skolem function symbols live at `FOL_LAB_SKOLEM_BASE +
// counter`, where the BASE sits well above the connective labels so
// they don't collide.  A global counter assigns them in increasing
// order; reset with fol_reset_skolem before each independent run.
//
// Caller convention: run `fol_nnf` first so the input has no `->` /
// `<->` connectives.  This Skolemizer handles `->` / `<->` defensively
// as well (it recurses through their args) -- correctness then depends
// on those subforms being NNF-equivalent.

// thvm CTR labels are encoded in EXT_MASK = 0x3FFFF (18 bits, max
// 262143).  Connectives live at 0xFFF0..0xFFF6 (~65520); user
// predicate labels typically take low values.  Skolems fit safely
// above the connectives but below the 18-bit ceiling.
// The Skolem and Tseitin families sit above the user labels; a caller
// with a signature past the connectives (fol_set_label_floor) moves both
// up before allocating any.
static u32 g_fol_lab_tseitin = 0x10000u;
static u32 g_fol_lab_skolem  = 0x20000u;
#define FOL_LAB_SKOLEM_BASE g_fol_lab_skolem

static u32 g_fol_skolem_next = 0u;
// Set when a Skolem or Tseitin family runs out of labels (the CTR label
// field is 18 bits): a label past its range would alias an existing
// symbol, so the clausification must be abandoned, never wrapped.
static u8  g_fol_sk_out = 0u;
static u8  g_fol_ts_out = 0u;

fn void fol_reset_skolem(void) { g_fol_skolem_next = 0u; g_fol_sk_out = 0u; }
fn u8   fol_labels_exhausted(void) { return (u8)(g_fol_sk_out | g_fol_ts_out); }

// Substitute `var_id` in `t` with `repl`; structure-shares unchanged
// subterms.  Local to this file to keep the Skolem path free of any
// thvm_subst_apply dependency (whose REWRITE_MAX_VAR cap would force
// the user to fit all formula vars under 256).  Post-order: a node is
// rebuilt, after its children, only when one of them changed.
typedef struct { Term t; u8 build; } FolVsTask;
static FolVsTask *g_vs_task = NULL;
static u32        g_vs_task_cap = 0u;
static Term      *g_vs_val = NULL;
static u32        g_vs_val_cap = 0u;

static Term fol_var_subst(Term t, u32 var_id, Term repl) {
  u32 nt = 0u, nv = 0u;
  FOL_RESERVE(g_vs_task, g_vs_task_cap, nt);
  g_vs_task[nt++] = (FolVsTask){ t, 0u };
  while (nt > 0u) {
    fol_poll();
    FolVsTask k = g_vs_task[--nt];
    Term x = k.t;
    if (k.build) {
      u32 n = term_ctr_n(x);
      u32 base = nv - n;
      u8 changed = 0u;
      for (u32 i = 0; i < n; i++) {
        if (g_vs_val[base + i] != term_ctr_at(x, i)) {
          changed = 1u;
        }
      }
      Term r = changed ? term_new_ctr(term_ext(x), g_vs_val + base, n) : x;
      nv = base;
      g_vs_val[nv++] = r;
      continue;
    }
    Term r = x;
    if (term_tag(x) == TAG_FVR) {
      if (term_ext(x) == var_id) {
        r = repl;
      }
    } else if (term_tag(x) == TAG_CTR) {
      // An inner quantifier over the SAME variable shadows it: the
      // substitution must not cross the binder (it would replace the
      // binder's own var slot with a non-variable term, wedging the
      // Skolemizer and leaving the quantifier in the "CNF").
      u32 lab = term_ext(x);
      u8 shadow = 0u;
      if (lab == FOL_LAB_ALL || lab == FOL_LAB_EX) {
        Term var = term_ctr_at(x, 0u);
        shadow = (term_tag(var) == TAG_FVR && term_ext(var) == var_id);
      }
      u32 n = term_ctr_n(x);
      if (!shadow && n > 0u) {
        FOL_RESERVE(g_vs_task, g_vs_task_cap, nt + n);
        g_vs_task[nt++] = (FolVsTask){ x, 1u };
        for (u32 i = n; i-- > 0u;) {
          g_vs_task[nt++] = (FolVsTask){ term_ctr_at(x, i), 0u };
        }
        continue;
      }
    }
    FOL_RESERVE(g_vs_val, g_vs_val_cap, nv);
    g_vs_val[nv++] = r;
  }
  return g_vs_val[0];
}

// Variable-id occurrence stamps shared by the Skolemizer and the
// Tseitin/chunking passes: stamp[id] == epoch marks id as seen in the
// current walk.  Grown on demand (a Coq-exported record binds 1300
// variables; a fixed table silently dropped the rest).
static u32 *g_fol_vstamp = NULL;
static u32  g_fol_vstamp_cap = 0u;
static u32  g_fol_vepoch = 0u;

static void fol_vstamp_fit(u32 id) {
  if (id < g_fol_vstamp_cap) return;
  u32 ncap = (g_fol_vstamp_cap == 0u) ? 1024u : g_fol_vstamp_cap;
  while (ncap <= id) ncap *= 2u;
  u32 *ns = (u32 *)realloc(g_fol_vstamp, (size_t)ncap * sizeof(u32));
  if (ns == NULL) {
    fprintf(stderr, "thvm: out of memory in clausification\n");
    abort();
  }
  memset(ns + g_fol_vstamp_cap, 0, (size_t)(ncap - g_fol_vstamp_cap) * sizeof(u32));
  g_fol_vstamp = ns;
  g_fol_vstamp_cap = ncap;
}

static void fol_vepoch_next(void) {
  if (++g_fol_vepoch == 0u) {
    if (g_fol_vstamp != NULL) memset(g_fol_vstamp, 0, (size_t)g_fol_vstamp_cap * sizeof(u32));
    g_fol_vepoch = 1u;
  }
}

// Stamp every variable id occurring in `t` (binders included).
static Term *g_fol_walk = NULL;
static u32   g_fol_walk_cap = 0u;

static void fol_stamp_vars(Term t) {
  u32 n = 0u;
  FOL_RESERVE(g_fol_walk, g_fol_walk_cap, n);
  g_fol_walk[n++] = t;
  while (n > 0u) {
    fol_poll();
    Term x = g_fol_walk[--n];
    if (term_tag(x) == TAG_FVR) {
      u32 id = term_ext(x);
      fol_vstamp_fit(id);
      g_fol_vstamp[id] = g_fol_vepoch;
    } else if (term_tag(x) == TAG_CTR) {
      u32 nk = term_ctr_n(x);
      FOL_RESERVE(g_fol_walk, g_fol_walk_cap, n + nk);
      for (u32 i = 0; i < nk; i++) {
        g_fol_walk[n++] = term_ctr_at(x, i);
      }
    }
  }
}

static u8 fol_var_stamped(u32 id) {
  return (id < g_fol_vstamp_cap && g_fol_vstamp[id] == g_fol_vepoch) ? 1u : 0u;
}

// The universal scope of the Skolemizer: one shared stack (a scope
// copy per binder was O(depth^2), and a fixed 256-slot copy returned
// the formula with its quantifiers still inside past that depth).
static Term *g_fol_sk_univ = NULL;
static u32   g_fol_sk_univ_cap = 0u;
static Term *g_fol_sk_args = NULL;
static u32   g_fol_sk_args_cap = 0u;

static void fol_sk_fit(Term **a, u32 *cap, u32 need) {
  if (need < *cap) return;
  u32 ncap = (*cap == 0u) ? 256u : *cap;
  while (ncap <= need) ncap *= 2u;
  Term *na = (Term *)realloc(*a, (size_t)ncap * sizeof(Term));
  if (na == NULL) {
    fprintf(stderr, "thvm: out of memory in Skolemization\n");
    abort();
  }
  *a = na;
  *cap = ncap;
}

// Pre-order over an explicit stack: an EVAL task carries its node and
// the depth of its universal scope (the prefix of g_fol_sk_univ it may
// read -- entries past it belong to finished siblings); the connective
// it keeps is rebuilt by a MK task over its children's results.
enum { FOL_SK_EVAL, FOL_SK_MK };

typedef struct { Term t; u32 n_univ; u32 lab; u8 op; } FolSkTask;
static FolSkTask *g_sk_task = NULL;
static u32        g_sk_task_cap = 0u;
static Term      *g_sk_val = NULL;
static u32        g_sk_val_cap = 0u;

static void fol_sk_push(u32 *nt, Term t, u32 n_univ, u32 lab, u8 op) {
  FOL_RESERVE(g_sk_task, g_sk_task_cap, *nt);
  g_sk_task[*nt] = (FolSkTask){ t, n_univ, lab, op };
  (*nt)++;
}

static void fol_sk_val(u32 *nv, Term t) {
  FOL_RESERVE(g_sk_val, g_sk_val_cap, *nv);
  g_sk_val[(*nv)++] = t;
}

// The Skolem term replacing an existentially bound variable whose body
// is `body`, over the first `n_univ` entries of the universal scope; 0
// once the Skolem label family is spent.
static Term fol_skolem_term(Term body, u32 n_univ) {
  // Fresh Skolem function symbol over the universal scope.  A scope wider
  // than the engine's native arity keeps only the universals the
  // body mentions (dedup'd, scope order) -- the same Skolemization
  // semantically (a witness cannot depend on a variable its
  // formula never mentions), without 50-argument Skolem terms from
  // the deep binder nests of the Coq/Mizar exports.  Narrow scopes
  // keep every universal, as the solved FOF set was validated with.
  if (FOL_LAB_SKOLEM_BASE + g_fol_skolem_next > (u32)EXT_MASK) {
    g_fol_sk_out = 1u;
    return 0;
  }
  u32 sk_label = FOL_LAB_SKOLEM_BASE + g_fol_skolem_next;
  g_fol_skolem_next++;
  if (n_univ == 0u) {
    return term_new_ctr(sk_label, NULL, 0u);
  }
  if (n_univ <= REWRITE_MAX_ARITY) {
    return term_new_ctr(sk_label, g_fol_sk_univ, n_univ);
  }
  fol_vepoch_next();
  fol_stamp_vars(body);
  u32 na = 0u;
  for (u32 i = 0; i < n_univ; i++) {
    Term u = g_fol_sk_univ[i];
    if (term_tag(u) != TAG_FVR || !fol_var_stamped(term_ext(u))) {
      continue;
    }
    u32 dup = 0u;
    for (u32 j = 0; j < na && !dup; j++) {
      dup = (g_fol_sk_args[j] == u);
    }
    if (dup) {
      continue;
    }
    fol_sk_fit(&g_fol_sk_args, &g_fol_sk_args_cap, na);
    g_fol_sk_args[na++] = u;
  }
  return term_new_ctr(sk_label, na ? g_fol_sk_args : NULL, na);
}

// A run of consecutive existential binders is Skolemized at once: one
// substitution walk for the whole run instead of one per binder
// (HWV061+1: a 28k-binder prefix over a 327k-atom matrix, each binder's
// own walk over the rest -- quadratic; HWV086+1's 2.7k binders under 107
// universals stamped and substituted the growing body per binder).  The
// run is the longest chain whose variables are distinct and none a
// universal of the scope.  Then no binder's substitution reaches
// another's Skolem term (their arguments are universals of the scope),
// a binder inside the body shadows only its own variable, and past
// REWRITE_MAX_ARITY universals every binder of the run keeps the same
// arguments -- the universals the innermost body mentions (the chain's
// own binders add none, and an earlier Skolem term's arguments are
// among them) -- so the simultaneous substitution is the
// one-at-a-time one.  Its variables go to g_fol_run_vids.
static u32 *g_fol_run_vids = NULL;
static u32  g_fol_run_vids_cap = 0u;

static u32 fol_sk_run(Term g, u32 n_univ) {
  fol_vepoch_next();
  for (u32 i = 0; i < n_univ; i++) {
    Term u = g_fol_sk_univ[i];
    if (term_tag(u) == TAG_FVR) {
      fol_vstamp_fit(term_ext(u));
      g_fol_vstamp[term_ext(u)] = g_fol_vepoch;
    }
  }
  u32 k = 0u;
  while (term_tag(g) == TAG_CTR && term_ext(g) == FOL_LAB_EX) {
    Term var = term_ctr_at(g, 0u);
    if (term_tag(var) != TAG_FVR || fol_var_stamped(term_ext(var))) {
      break;
    }
    u32 id = term_ext(var);
    fol_vstamp_fit(id);
    g_fol_vstamp[id] = g_fol_vepoch;
    FOL_RESERVE(g_fol_run_vids, g_fol_run_vids_cap, k);
    g_fol_run_vids[k++] = id;
    g = term_ctr_at(g, 1u);
  }
  return k;
}

// The run's substitution: g_fol_run_map[id] is the Skolem term replacing
// variable `id` (0 = none).  Post-order like fol_var_subst; a binder of
// a run variable clears its entry for its subtree (restored after it).
static Term *g_fol_run_map = NULL;
static u32   g_fol_run_map_cap = 0u;

enum { FOL_VR_EVAL, FOL_VR_BUILD, FOL_VR_RESTORE };
typedef struct { Term t; u32 vid; u8 op; } FolVrTask;
static FolVrTask *g_vr_task = NULL;
static u32        g_vr_task_cap = 0u;

static void fol_run_map_fit(u32 id) {
  if (id < g_fol_run_map_cap) {
    return;
  }
  u32 old = g_fol_run_map_cap;
  g_fol_run_map = fol_grow(g_fol_run_map, &g_fol_run_map_cap, id, sizeof(Term));
  memset(g_fol_run_map + old, 0, (size_t)(g_fol_run_map_cap - old) * sizeof(Term));
}

static Term fol_var_subst_run(Term t) {
  u32 nt = 0u, nv = 0u;
  FOL_RESERVE(g_vr_task, g_vr_task_cap, nt);
  g_vr_task[nt++] = (FolVrTask){ t, 0u, FOL_VR_EVAL };
  while (nt > 0u) {
    fol_poll();
    FolVrTask k = g_vr_task[--nt];
    Term x = k.t;
    if (k.op == FOL_VR_RESTORE) {
      g_fol_run_map[k.vid] = x;
      continue;
    }
    if (k.op == FOL_VR_BUILD) {
      u32 n = term_ctr_n(x);
      u32 base = nv - n;
      u8 changed = 0u;
      for (u32 i = 0; i < n; i++) {
        if (g_vs_val[base + i] != term_ctr_at(x, i)) {
          changed = 1u;
        }
      }
      Term r = changed ? term_new_ctr(term_ext(x), g_vs_val + base, n) : x;
      nv = base;
      g_vs_val[nv++] = r;
      continue;
    }
    Term r = x;
    if (term_tag(x) == TAG_FVR) {
      u32 id = term_ext(x);
      if (id < g_fol_run_map_cap && g_fol_run_map[id] != 0) {
        r = g_fol_run_map[id];
      }
    } else if (term_tag(x) == TAG_CTR && term_ctr_n(x) > 0u) {
      u32 lab = term_ext(x);
      u32 n = term_ctr_n(x);
      FOL_RESERVE(g_vr_task, g_vr_task_cap, nt + n + 1u);
      if (lab == FOL_LAB_ALL || lab == FOL_LAB_EX) {
        Term var = term_ctr_at(x, 0u);
        u32 id = term_ext(var);
        if (term_tag(var) == TAG_FVR && id < g_fol_run_map_cap && g_fol_run_map[id] != 0) {
          g_vr_task[nt++] = (FolVrTask){ g_fol_run_map[id], id, FOL_VR_RESTORE };
          g_fol_run_map[id] = 0;
        }
      }
      g_vr_task[nt++] = (FolVrTask){ x, 0u, FOL_VR_BUILD };
      for (u32 i = n; i-- > 0u;) {
        g_vr_task[nt++] = (FolVrTask){ term_ctr_at(x, i), 0u, FOL_VR_EVAL };
      }
      continue;
    }
    FOL_RESERVE(g_vs_val, g_vs_val_cap, nv);
    g_vs_val[nv++] = r;
  }
  return g_vs_val[0];
}

fn Term fol_skolemize(Term f) {
  u32 nt = 0u, nv = 0u;
  fol_sk_push(&nt, f, 0u, 0u, FOL_SK_EVAL);
  while (nt > 0u) {
    fol_poll();
    FolSkTask k = g_sk_task[--nt];
    if (k.op == FOL_SK_MK) {
      if (k.lab == FOL_LAB_NOT) {
        Term a = g_sk_val[--nv];
        fol_sk_val(&nv, fol_mk_not(a));
      } else {
        Term kids[2];
        kids[1] = g_sk_val[--nv];
        kids[0] = g_sk_val[--nv];
        fol_sk_val(&nv, term_new_ctr(k.lab, kids, 2u));
      }
      continue;
    }
    Term g = k.t;
    u32 n_univ = k.n_univ;
    if (term_tag(g) != TAG_CTR) {
      fol_sk_val(&nv, g);
      continue;
    }
    u32 lab = term_ext(g);
    switch (lab) {
      case FOL_LAB_NOT:
        fol_sk_push(&nt, 0, 0u, lab, FOL_SK_MK);
        fol_sk_push(&nt, term_ctr_at(g, 0u), n_univ, 0u, FOL_SK_EVAL);
        break;
      case FOL_LAB_AND:
      case FOL_LAB_OR:
      case FOL_LAB_IMP:
      case FOL_LAB_IFF:
        fol_sk_push(&nt, 0, 0u, lab, FOL_SK_MK);
        fol_sk_push(&nt, term_ctr_at(g, 1u), n_univ, 0u, FOL_SK_EVAL);
        fol_sk_push(&nt, term_ctr_at(g, 0u), n_univ, 0u, FOL_SK_EVAL);
        break;
      case FOL_LAB_ALL:
        // Extend univ scope; drop the `!` -- everything stays implicit.
        fol_sk_fit(&g_fol_sk_univ, &g_fol_sk_univ_cap, n_univ);
        g_fol_sk_univ[n_univ] = term_ctr_at(g, 0u);
        fol_sk_push(&nt, term_ctr_at(g, 1u), n_univ + 1u, 0u, FOL_SK_EVAL);
        break;
      case FOL_LAB_EX: {
        u32 run = fol_sk_run(g, n_univ);
        if (run >= 2u &&
            FOL_LAB_SKOLEM_BASE + g_fol_skolem_next + (run - 1u) <= (u32)EXT_MASK) {
          Term inner = g;
          for (u32 i = 0; i < run; i++) {
            inner = term_ctr_at(inner, 1u);
          }
          const Term *args = g_fol_sk_univ;
          u32 na = n_univ;
          if (n_univ > REWRITE_MAX_ARITY) {
            fol_vepoch_next();
            fol_stamp_vars(inner);
            na = 0u;
            for (u32 i = 0; i < n_univ; i++) {
              Term u = g_fol_sk_univ[i];
              if (term_tag(u) != TAG_FVR || !fol_var_stamped(term_ext(u))) {
                continue;
              }
              u32 dup = 0u;
              for (u32 j = 0; j < na && !dup; j++) {
                dup = (g_fol_sk_args[j] == u);
              }
              if (!dup) {
                fol_sk_fit(&g_fol_sk_args, &g_fol_sk_args_cap, na);
                g_fol_sk_args[na++] = u;
              }
            }
            args = g_fol_sk_args;
          }
          for (u32 i = 0; i < run; i++) {
            u32 id = g_fol_run_vids[i];
            u32 sk_label = FOL_LAB_SKOLEM_BASE + g_fol_skolem_next;
            g_fol_skolem_next++;
            fol_run_map_fit(id);
            g_fol_run_map[id] = term_new_ctr(sk_label, na ? args : NULL, na);
          }
          Term substituted = fol_var_subst_run(inner);
          for (u32 i = 0; i < run; i++) {
            g_fol_run_map[g_fol_run_vids[i]] = 0;
          }
          fol_sk_push(&nt, substituted, n_univ, 0u, FOL_SK_EVAL);
          break;
        }
        Term var  = term_ctr_at(g, 0u);
        Term body = term_ctr_at(g, 1u);
        Term sk_term = (term_tag(var) == TAG_FVR) ? fol_skolem_term(body, n_univ) : 0;
        if (sk_term == 0) {
          fol_sk_val(&nv, g);   // not a variable binder, or labels spent
          break;
        }
        Term substituted = fol_var_subst(body, term_ext(var), sk_term);
        fol_sk_push(&nt, substituted, n_univ, 0u, FOL_SK_EVAL);
        break;
      }
      default:
        // Atom / non-connective: returned unchanged.
        fol_sk_val(&nv, g);
        break;
    }
  }
  return g_sk_val[0];
}

// True iff `label` is a Skolem function symbol generated by the
// pipeline.  Useful for trace printers / proof reconstruction.
fn u8 fol_is_skolem(u32 label) {
  return (label >= FOL_LAB_SKOLEM_BASE) ? 1u : 0u;
}

// === CNF distribution ===============================================
//
// Take an NNF + skolemized formula (whose only connectives are AND,
// OR, NOT-on-atom) and rewrite it into a conjunction-of-disjunctions:
//
//   distribute(p ∧ q) = distribute(p) ∧ distribute(q)
//   distribute(p ∨ q) = or_cnf(distribute(p), distribute(q))
//   distribute(literal) = literal
//
// or_cnf joins two CNFs: (a ∧ b) ∨ y = (a ∨ y) ∧ (b ∨ y), then
// x ∨ (a ∧ b) = (x ∨ a) ∧ (x ∨ b), else x ∨ y.  Both sides are already
// distributed, so every result node is built once: distributing a
// joined side again rebuilds its AND spine per conjunct of the other,
// quadratic in the clause count (LCL681+1.015's 1590-clause, 109k-
// literal CNF exhausted a 2^30-cell term heap that way).
//
// Worst-case exponential in nested-OR depth -- the classical Tseitin
// alternative (intro auxiliary atoms) avoids that blowup but enlarges
// the signature; the FOF front end routes such records through it.

//
// One explicit stack runs both: DIST distributes a node, OR_CNF joins
// two distributed sides, JOIN pops the two sides a DIST of an OR left
// and schedules their OR_CNF, AND builds a conjunction of two results.
enum { FOL_DI_DIST, FOL_DI_OR_CNF, FOL_DI_JOIN, FOL_DI_AND };

typedef struct { Term x, y; u8 op; } FolDiTask;
static FolDiTask *g_di_task = NULL;
static u32        g_di_task_cap = 0u;
static Term      *g_di_val = NULL;
static u32        g_di_val_cap = 0u;

static void fol_di_push(u32 *nt, u8 op, Term x, Term y) {
  FOL_RESERVE(g_di_task, g_di_task_cap, *nt);
  g_di_task[*nt] = (FolDiTask){ x, y, op };
  (*nt)++;
}

static void fol_di_val(u32 *nv, Term t) {
  FOL_RESERVE(g_di_val, g_di_val_cap, *nv);
  g_di_val[(*nv)++] = t;
}

static u8 fol_is_and(Term t) {
  return (term_tag(t) == TAG_CTR && term_ext(t) == FOL_LAB_AND) ? 1u : 0u;
}

fn Term fol_distribute(Term f) {
  u32 nt = 0u, nv = 0u;
  fol_di_push(&nt, FOL_DI_DIST, f, 0);
  while (nt > 0u) {
    fol_poll();
    FolDiTask k = g_di_task[--nt];
    switch (k.op) {
      case FOL_DI_DIST: {
        u32 lab = (term_tag(k.x) == TAG_CTR) ? term_ext(k.x) : 0u;
        if (lab == FOL_LAB_AND || lab == FOL_LAB_OR) {
          fol_di_push(&nt, (lab == FOL_LAB_AND) ? FOL_DI_AND : FOL_DI_JOIN, 0, 0);
          fol_di_push(&nt, FOL_DI_DIST, term_ctr_at(k.x, 1u), 0);
          fol_di_push(&nt, FOL_DI_DIST, term_ctr_at(k.x, 0u), 0);
        } else {
          // NOT-on-atom or literal atom -- leave alone.
          fol_di_val(&nv, k.x);
        }
        break;
      }
      case FOL_DI_JOIN: {
        Term y = g_di_val[--nv];
        Term x = g_di_val[--nv];
        fol_di_push(&nt, FOL_DI_OR_CNF, x, y);
        break;
      }
      case FOL_DI_OR_CNF:
        if (fol_is_and(k.x)) {
          fol_di_push(&nt, FOL_DI_AND, 0, 0);
          fol_di_push(&nt, FOL_DI_OR_CNF, term_ctr_at(k.x, 1u), k.y);
          fol_di_push(&nt, FOL_DI_OR_CNF, term_ctr_at(k.x, 0u), k.y);
        } else if (fol_is_and(k.y)) {
          fol_di_push(&nt, FOL_DI_AND, 0, 0);
          fol_di_push(&nt, FOL_DI_OR_CNF, k.x, term_ctr_at(k.y, 1u));
          fol_di_push(&nt, FOL_DI_OR_CNF, k.x, term_ctr_at(k.y, 0u));
        } else {
          fol_di_val(&nv, fol_mk_or(k.x, k.y));
        }
        break;
      default: {
        Term q = g_di_val[--nv];
        Term p = g_di_val[--nv];
        fol_di_val(&nv, fol_mk_and(p, q));
        break;
      }
    }
  }
  return g_di_val[0];
}

// === clause extraction ==============================================
//
// Walk a fully-distributed (NNF + skolemized + distributed) formula
// and emit one FolClause per top-level disjunct.

// Append `lit` to the literals array, growing if needed.
static void fol_extract_lit(FolLit **lits, u32 *n, u32 *cap, FolLit lit) {
  if (*n >= *cap) {
    u32 ncap = (*cap == 0u) ? 8u : (*cap) * 2u;
    FolLit *grow = (FolLit *)realloc(*lits, (size_t)ncap * sizeof(FolLit));
    if (grow == NULL) return;
    *lits = grow;
    *cap = ncap;
  }
  (*lits)[*n] = lit;
  *n += 1u;
}

// Walk an OR subtree and append every literal into `*lits`, left to
// right.
static Term *g_eo_stk = NULL;
static u32   g_eo_stk_cap = 0u;

static void fol_extract_or(Term f, FolLit **lits, u32 *n, u32 *cap) {
  u32 ns = 0u;
  FOL_RESERVE(g_eo_stk, g_eo_stk_cap, ns);
  g_eo_stk[ns++] = f;
  while (ns > 0u) {
    fol_poll();
    Term g = g_eo_stk[--ns];
    if (term_tag(g) == TAG_CTR && term_ext(g) == FOL_LAB_OR) {
      FOL_RESERVE(g_eo_stk, g_eo_stk_cap, ns + 2u);
      g_eo_stk[ns++] = term_ctr_at(g, 1u);
      g_eo_stk[ns++] = term_ctr_at(g, 0u);
      continue;
    }
    FolLit lit;
    if (term_tag(g) == TAG_CTR && term_ext(g) == FOL_LAB_NOT) {
      lit.atom = term_ctr_at(g, 0u);
      lit.sign = 1u;
    } else {
      lit.atom = g;
      lit.sign = 0u;
    }
    fol_extract_lit(lits, n, cap, lit);
  }
}

static void fol_push_clause(FolClause ***out, u32 *n, u32 *cap, FolClause *c) {
  if (*n >= *cap) {
    u32 ncap = (*cap == 0u) ? 8u : (*cap) * 2u;
    FolClause **grow = (FolClause **)realloc(*out, (size_t)ncap * sizeof(FolClause *));
    if (grow == NULL) { fol_clause_free(c); return; }
    *out = grow;
    *cap = ncap;
  }
  (*out)[*n] = c;
  *n += 1u;
}

static void fol_push_lits(FolClause ***out, u32 *n, u32 *cap,
                          const FolLit *lits, u32 nl) {
  FolClause *c = fol_clause_new(nl);
  if (c == NULL) return;
  for (u32 i = 0; i < nl; i++) c->lits[i] = lits[i];
  fol_push_clause(out, n, cap, c);
}

static Term fol_fresh_aux(const Term *vars, u32 n_vars);   // Tseitin, below

// Distinct variables of a subformula (or of a chunked clause's shared
// part), first-occurrence order, in one shared growable list.
static Term *g_fol_ts_vars = NULL;
static u32   g_fol_ts_vars_cap = 0u;

// Append (once, first occurrence) every variable of `t` stamped in the
// current epoch; a taken variable's stamp is cleared.
static void fol_collect_shared(Term t, u32 *n) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (!fol_var_stamped(id)) return;
      g_fol_vstamp[id] = 0u;
      fol_sk_fit(&g_fol_ts_vars, &g_fol_ts_vars_cap, *n);
      g_fol_ts_vars[(*n)++] = t;
      return;
    }
    case TAG_CTR: {
      u32 nk = term_ctr_n(t);
      for (u32 i = 0; i < nk; i++) fol_collect_shared(term_ctr_at(t, i), n);
      return;
    }
    default: return;
  }
}

// Literals per piece of a chunked wide clause.  WHY 8: an all-negative
// (goal) clause then chunks into Horn rules ~L1..~L7 | a1,
// ~a1 | ~L8..~L13 | a2, ... whose bodies the Horn release can join (it
// takes at most 8 conditions); 63-literal pieces were rules no Horn lane
// could use.
#define FOL_WIDE_PIECE 8u

// Chunk a disjunction into pieces of at most FOL_WIDE_PIECE literals,
// naming each remainder:
//   A | B  ~~>  { A | a(x),  ~a(x) | B },  x = vars(A) & vars(B).
// Sound: any model of A | B satisfies both with a(x) := !y B(x,y)
// (y = the rest of B's variables): where A fails, B holds for every
// y.  Resolving on a(x) gives A | B back.  The names come from the
// Tseitin family, so the proof layer declares them as definitions.
static void fol_append_pieces(FolClause ***out, u32 *n, u32 *cap,
                              const FolLit *lits, u32 nl) {
  FolLit buf[FOL_WIDE_PIECE];
  FolLit prev = { 0, 0u };
  u32 have_prev = 0u;
  u32 i = 0u;
  while (nl - i > FOL_WIDE_PIECE - have_prev) {
    u32 take = FOL_WIDE_PIECE - 1u - have_prev;
    u32 k = 0u;
    if (have_prev) buf[k++] = prev;
    for (u32 j = 0; j < take; j++) buf[k++] = lits[i + j];
    i += take;
    // Shared variables: those of everything so far that the rest uses.
    fol_vepoch_next();
    for (u32 j = i; j < nl; j++) fol_stamp_vars(lits[j].atom);
    u32 nv = 0u;
    for (u32 j = 0; j < i; j++) fol_collect_shared(lits[j].atom, &nv);
    Term aux = fol_fresh_aux(g_fol_ts_vars, nv);
    buf[k++] = (FolLit){ .atom = aux, .sign = 0u };
    fol_push_lits(out, n, cap, buf, k);
    prev = (FolLit){ .atom = aux, .sign = 1u };
    have_prev = 1u;
  }
  u32 k = 0u;
  if (have_prev) buf[k++] = prev;
  for (; i < nl; i++) buf[k++] = lits[i];
  fol_push_lits(out, n, cap, buf, k);
}

// Variable-disjoint components of a clause: union-find over literal
// indices, a variable's first literal kept in g_fol_vowner (valid where
// g_fol_vstamp carries the current epoch).
static u32 *g_fol_vowner = NULL;
static u32  g_fol_vowner_cap = 0u;

static u32 fol_uf_find(u32 *par, u32 x) {
  while (par[x] != x) {
    par[x] = par[par[x]];
    x = par[x];
  }
  return x;
}

static void fol_comp_walk(Term t, u32 lit, u32 *par) {
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      if (fol_var_stamped(id)) {
        u32 a = fol_uf_find(par, lit), b = fol_uf_find(par, g_fol_vowner[id]);
        if (a != b) par[a > b ? a : b] = (a > b) ? b : a;
        return;
      }
      fol_vstamp_fit(id);
      if (g_fol_vowner_cap < g_fol_vstamp_cap) {
        u32 *no = (u32 *)realloc(g_fol_vowner, (size_t)g_fol_vstamp_cap * sizeof(u32));
        if (no == NULL) {
          fprintf(stderr, "thvm: out of memory in clausification\n");
          abort();
        }
        g_fol_vowner = no;
        g_fol_vowner_cap = g_fol_vstamp_cap;
      }
      g_fol_vstamp[id] = g_fol_vepoch;
      g_fol_vowner[id] = lit;
      return;
    }
    case TAG_CTR: {
      u32 nk = term_ctr_n(t);
      for (u32 i = 0; i < nk; i++) fol_comp_walk(term_ctr_at(t, i), lit, par);
      return;
    }
    default: return;
  }
}

// A disjunction wider than FOL_MAX_LITS (a 201-literal domain closure
// X = c1 | ... | X = c201; an 85-way negated existential conjecture)
// used to fail clause allocation and vanish -- the record then
// "clausified" to nothing.  Its variable-disjoint components C1..Cm
// (m >= 2) are named first, each by a fresh propositional atom:
//   C1 | ... | Cm  ~~>  { Cj | dj  (Cj all negative),  Cj | ~nj (else),
//                         ~d.. | n.. }
// Sound: with components sharing no variable, C1 | ... | Cm holds iff
// some !Cj holds; a model of the clause satisfies the named set with
// dj := ~!Cj and nj := !Cj, and resolving the top clause against the
// namings gives the clause back.  An all-negative component keeps its
// naming clause Horn (the body of dj), so a goal such as
// ~(?[X]:p1(X) & ... & ?[X]:p85(X)) becomes 85 one-condition rules and
// a propositional goal instead of one 85-condition join.  Every clause
// wider than FOL_WIDE_PIECE is then chunked (fol_append_pieces).
static u32 g_fol_wide_clauses = 0u;   // clauses chunked (FOFSTATS)

static void fol_append_wide(FolClause ***out, u32 *n, u32 *cap,
                            const FolLit *lits, u32 nl) {
  g_fol_wide_clauses++;
  u32 *par = (u32 *)malloc((size_t)nl * sizeof(u32));
  u32 *start = (u32 *)calloc((size_t)nl + 1u, sizeof(u32));
  u32 *order = (u32 *)malloc((size_t)nl * sizeof(u32));
  FolLit *top = (FolLit *)malloc((size_t)nl * sizeof(FolLit));
  FolLit *comp = (FolLit *)malloc((size_t)(nl + 1u) * sizeof(FolLit));
  if (par == NULL || start == NULL || order == NULL || top == NULL || comp == NULL) {
    fprintf(stderr, "thvm: out of memory in clausification\n");
    abort();
  }
  for (u32 i = 0; i < nl; i++) par[i] = i;
  fol_vepoch_next();
  for (u32 i = 0; i < nl; i++) fol_comp_walk(lits[i].atom, i, par);
  // Literals grouped by component root (its smallest literal index),
  // input order kept within one.
  for (u32 i = 0; i < nl; i++) par[i] = fol_uf_find(par, i);
  for (u32 i = 0; i < nl; i++) start[par[i] + 1u]++;
  for (u32 r = 0; r < nl; r++) start[r + 1u] += start[r];
  for (u32 i = 0; i < nl; i++) order[start[par[i]]++] = i;
  for (u32 r = nl; r > 0; r--) start[r] = start[r - 1u];
  start[0] = 0u;
  u32 m = 0u;
  for (u32 r = 0; r < nl; r++) {
    if (start[r + 1u] > start[r]) m++;
  }
  if (m < 2u) {
    fol_append_pieces(out, n, cap, lits, nl);
  } else {
    u32 nt = 0u;
    for (u32 r = 0; r < nl; r++) {
      u32 b = start[r], e = start[r + 1u];
      if (b == e) continue;
      u32 k = 0u;
      u8 all_neg = 1u;
      for (u32 q = b; q < e; q++) {
        comp[k++] = lits[order[q]];
        if (lits[order[q]].sign == 0u) all_neg = 0u;
      }
      Term aux = fol_fresh_aux(NULL, 0u);
      comp[k++] = (FolLit){ .atom = aux, .sign = all_neg ? 0u : 1u };
      top[nt++] = (FolLit){ .atom = aux, .sign = all_neg ? 1u : 0u };
      fol_append_pieces(out, n, cap, comp, k);
    }
    fol_append_pieces(out, n, cap, top, nt);
  }
  free(comp);
  free(top);
  free(order);
  free(start);
  free(par);
}

// Append a clause built from `or_root` to `*out`, growing as needed.
static void fol_append_clause(FolClause ***out, u32 *n, u32 *cap, Term or_root) {
  FolLit *lits = NULL;
  u32 nl = 0u, capl = 0u;
  fol_extract_or(or_root, &lits, &nl, &capl);
  if (nl > FOL_MAX_LITS) fol_append_wide(out, n, cap, lits, nl);
  else fol_push_lits(out, n, cap, lits, nl);
  free(lits);
}

// Walk the AND-of-... tree and emit a clause per top-level disjunct,
// left to right.
static Term *g_ea_stk = NULL;
static u32   g_ea_stk_cap = 0u;

static void fol_extract_and(Term f, FolClause ***out, u32 *n, u32 *cap) {
  u32 ns = 0u;
  FOL_RESERVE(g_ea_stk, g_ea_stk_cap, ns);
  g_ea_stk[ns++] = f;
  while (ns > 0u) {
    fol_poll();
    Term g = g_ea_stk[--ns];
    if (term_tag(g) == TAG_CTR && term_ext(g) == FOL_LAB_AND) {
      FOL_RESERVE(g_ea_stk, g_ea_stk_cap, ns + 2u);
      g_ea_stk[ns++] = term_ctr_at(g, 1u);
      g_ea_stk[ns++] = term_ctr_at(g, 0u);
      continue;
    }
    fol_append_clause(out, n, cap, g);
  }
}

fn FolClause **fol_extract_clauses(Term cnf_formula, u32 *n_out) {
  if (n_out == NULL) return NULL;
  FolClause **out = NULL;
  u32 n = 0u, cap = 0u;
  fol_extract_and(cnf_formula, &out, &n, &cap);
  *n_out = n;
  return out;
}

// === end-to-end formula -> clauses ==================================
//
// One-shot: f -> NNF -> Skolem -> distribute -> extract.  Returns a
// heap-allocated array of `*n_out` FolClause*; caller owns each
// clause (free via fol_clause_free) and the array (free via free()).
// Resets the Skolem counter so each call is independent.
fn FolClause **fol_formula_to_clauses(Term f, u32 *n_out) {
  if (n_out == NULL) return NULL;
  fol_reset_skolem();
  Term nnf  = fol_nnf(f);
  Term sk   = fol_skolemize(nnf);
  Term cnf_ = fol_distribute(sk);
  return fol_extract_clauses(cnf_, n_out);
}

// === Tseitin structural CNF =========================================
//
// Naïve `fol_distribute` is O(exp) in nested-OR depth.  The Tseitin
// transformation produces a clause set whose size is linear in the
// input formula by introducing a fresh auxiliary atom for every
// compound subformula and encoding the equivalence `aux ↔ G` as a
// small set of clauses.
//
// Auxiliaries are predicates whose arguments are the FREE variables
// of the subformula they name -- so the aux properly captures the
// subformula's truth value at a particular variable binding.
//
// Caller must run fol_nnf + fol_skolemize first; this pass only
// handles ∧, ∨, ¬-on-atom, and atoms.

#define FOL_LAB_TSEITIN_BASE g_fol_lab_tseitin

// Place the Tseitin family at `floor` (>= 0x10000, rounded up to 4096)
// and the Skolem family 0x10000 above it.  0 when no Skolem room would
// remain.  Only before the first Tseitin/Skolem label of a problem.
fn u8 fol_set_label_floor(u32 floor) {
  if (floor < 0x10000u) floor = 0x10000u;
  floor = (floor + 0xFFFu) & ~0xFFFu;
  if (floor + 0x10000u + 0x8000u > (u32)EXT_MASK) return 0u;
  g_fol_lab_tseitin = floor;
  g_fol_lab_skolem  = floor + 0x10000u;
  return 1u;
}

static u32 g_fol_tseitin_next = 0u;

fn void fol_reset_tseitin(void) { g_fol_tseitin_next = 0u; g_fol_ts_out = 0u; }

fn u8 fol_is_tseitin_aux(u32 label) {
  return (label >= FOL_LAB_TSEITIN_BASE && label < FOL_LAB_SKOLEM_BASE) ? 1u : 0u;
}

// The auxiliary predicate naming a subformula carries EVERY variable
// of it: a truncated argument list (the old 128-variable cap) makes
// `aux(v1..v128) <-> G(v1..v200)` assert that G ignores v129..v200 --
// a strengthening, i.e. unsound.

// First-occurrence order: pre-order, children left to right.
static void fol_collect_vars_rec(Term t, u32 *n) {
  u32 ns = 0u;
  FOL_RESERVE(g_fol_walk, g_fol_walk_cap, ns);
  g_fol_walk[ns++] = t;
  while (ns > 0u) {
    fol_poll();
    Term x = g_fol_walk[--ns];
    if (term_tag(x) == TAG_FVR) {
      u32 id = term_ext(x);
      if (fol_var_stamped(id)) {
        continue;
      }
      fol_vstamp_fit(id);
      g_fol_vstamp[id] = g_fol_vepoch;
      fol_sk_fit(&g_fol_ts_vars, &g_fol_ts_vars_cap, *n);
      g_fol_ts_vars[(*n)++] = x;
    } else if (term_tag(x) == TAG_CTR) {
      u32 nk = term_ctr_n(x);
      FOL_RESERVE(g_fol_walk, g_fol_walk_cap, ns + nk);
      for (u32 i = nk; i-- > 0u;) {
        g_fol_walk[ns++] = term_ctr_at(x, i);
      }
    }
  }
}

static u32 fol_collect_vars(Term t) {
  u32 n = 0u;
  fol_vepoch_next();
  fol_collect_vars_rec(t, &n);
  return n;
}

// Build a fresh auxiliary atom A_n(v1, ..., vk) from the var list.
static Term fol_fresh_aux(const Term *vars, u32 n_vars) {
  if (FOL_LAB_TSEITIN_BASE + g_fol_tseitin_next >= FOL_LAB_SKOLEM_BASE) {
    g_fol_ts_out = 1u;   // callers abandon the clause set
    return term_new_ctr(FOL_LAB_TSEITIN_BASE, NULL, 0u);
  }
  u32 label = FOL_LAB_TSEITIN_BASE + g_fol_tseitin_next;
  g_fol_tseitin_next++;
  return term_new_ctr(label, n_vars ? vars : NULL, n_vars);
}

// Append a clause from a literal list (literals are copied).
static void fol_tseitin_emit(FolClause ***out, u32 *n, u32 *cap,
                             const FolLit *lits, u32 nl) {
  FolClause *c = fol_clause_new(nl);
  if (c == NULL) return;
  for (u32 i = 0; i < nl; i++) c->lits[i] = lits[i];
  if (*n >= *cap) {
    u32 ncap = (*cap == 0u) ? 8u : (*cap) * 2u;
    FolClause **grow = (FolClause **)realloc(*out, (size_t)ncap * sizeof(FolClause *));
    if (grow == NULL) { fol_clause_free(c); return; }
    *out = grow;
    *cap = ncap;
  }
  (*out)[*n] = c;
  *n += 1u;
}

// Decompose `f` into a "name" literal that's equisatisfiable with f.
// For an atom or ¬-atom, the literal IS the name (no aux needed).
// For a compound (∧ or ∨), introduce an auxiliary atom, emit the
// encoding clauses for aux <-> f, and return aux as the name.  The
// compounds are named in post-order (left subformula, right
// subformula, then the node), over an explicit stack.
typedef struct { Term f; u8 name; } FolTsTask;
static FolTsTask *g_ts_task = NULL;
static u32        g_ts_task_cap = 0u;
static FolLit    *g_ts_val = NULL;
static u32        g_ts_val_cap = 0u;

static FolLit fol_tseitin_name(Term f, FolLit name_b, FolLit name_c,
                               FolClause ***out, u32 *n, u32 *cap);

static FolLit fol_tseitin_rec(Term f, FolClause ***out, u32 *n, u32 *cap) {
  u32 nt = 0u, nv = 0u;
  FOL_RESERVE(g_ts_task, g_ts_task_cap, nt);
  g_ts_task[nt++] = (FolTsTask){ f, 0u };
  while (nt > 0u) {
    fol_poll();
    FolTsTask k = g_ts_task[--nt];
    Term g = k.f;
    FolLit r;
    if (k.name) {
      FolLit name_c = g_ts_val[--nv];
      FolLit name_b = g_ts_val[--nv];
      r = fol_tseitin_name(g, name_b, name_c, out, n, cap);
    } else if (term_tag(g) == TAG_CTR && term_ext(g) == FOL_LAB_NOT) {
      // ~atom case.
      r = (FolLit){ .atom = term_ctr_at(g, 0u), .sign = 1u };
    } else if (term_tag(g) != TAG_CTR || !fol_is_connective(term_ext(g))) {
      // Pure atom (any non-connective CTR or FVR).
      r = (FolLit){ .atom = g, .sign = 0u };
    } else {
      FOL_RESERVE(g_ts_task, g_ts_task_cap, nt + 2u);
      g_ts_task[nt++] = (FolTsTask){ g, 1u };
      g_ts_task[nt++] = (FolTsTask){ term_ctr_at(g, 1u), 0u };
      g_ts_task[nt++] = (FolTsTask){ term_ctr_at(g, 0u), 0u };
      continue;
    }
    FOL_RESERVE(g_ts_val, g_ts_val_cap, nv);
    g_ts_val[nv++] = r;
  }
  return g_ts_val[0];
}

// Name the compound `f` whose children are named `name_b`, `name_c`.
static FolLit fol_tseitin_name(Term f, FolLit name_b, FolLit name_c,
                               FolClause ***out, u32 *n, u32 *cap) {
  u32 lab = term_ext(f);
  u32 n_vars = fol_collect_vars(f);
  Term aux = fol_fresh_aux(g_fol_ts_vars, n_vars);

  FolLit neg_aux = { .atom = aux, .sign = 1u };
  FolLit pos_aux = { .atom = aux, .sign = 0u };
  FolLit neg_b   = { .atom = name_b.atom, .sign = (u8)(name_b.sign ^ 1u) };
  FolLit neg_c   = { .atom = name_c.atom, .sign = (u8)(name_c.sign ^ 1u) };

  if (lab == FOL_LAB_AND) {
    // aux ↔ B ∧ C
    //   ¬aux ∨ B            ¬aux ∨ C            ¬B ∨ ¬C ∨ aux
    FolLit c1[2] = { neg_aux, name_b };
    FolLit c2[2] = { neg_aux, name_c };
    FolLit c3[3] = { neg_b,  neg_c, pos_aux };
    fol_tseitin_emit(out, n, cap, c1, 2);
    fol_tseitin_emit(out, n, cap, c2, 2);
    fol_tseitin_emit(out, n, cap, c3, 3);
  } else {                     // FOL_LAB_OR
    // aux ↔ B ∨ C
    //   ¬aux ∨ B ∨ C        ¬B ∨ aux            ¬C ∨ aux
    FolLit c1[3] = { neg_aux, name_b, name_c };
    FolLit c2[2] = { neg_b, pos_aux };
    FolLit c3[2] = { neg_c, pos_aux };
    fol_tseitin_emit(out, n, cap, c1, 3);
    fol_tseitin_emit(out, n, cap, c2, 2);
    fol_tseitin_emit(out, n, cap, c3, 2);
  }
  return pos_aux;
}

fn FolClause **fol_tseitin_extract_clauses(Term nnf_skolemized, u32 *n_out) {
  if (n_out == NULL) return NULL;
  // Counter reset is the CALLER's job (the thvm.h contract): a
  // multi-record problem resets once and lets the counter run across
  // records so aux labels never collide between formulas.
  FolClause **out = NULL;
  u32 n = 0u, cap = 0u;
  FolLit root = fol_tseitin_rec(nnf_skolemized, &out, &n, &cap);
  // Assert the root: emit `{ root }` as a unit clause.
  fol_tseitin_emit(&out, &n, &cap, &root, 1u);
  *n_out = n;
  return out;
}

// End-to-end Tseitin pipeline: f -> NNF -> Skolem -> Tseitin CNF.
// Drop-in replacement for fol_formula_to_clauses on
// deeply-nested formulas where naïve distribute blows up.
fn FolClause **fol_formula_to_clauses_tseitin(Term f, u32 *n_out) {
  if (n_out == NULL) return NULL;
  fol_reset_skolem();
  fol_reset_tseitin();
  Term nnf = fol_nnf(f);
  Term sk  = fol_skolemize(nnf);
  return fol_tseitin_extract_clauses(sk, n_out);
}
