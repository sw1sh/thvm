// ffmep/_.c - finite-model "ExpressionPrune" hot enumeration (C port).
//
// The C half of the hybrid "ExpressionPruneC" method of WL
// TFindFiniteModels (wl/THVMLink/Kernel/ATP/FindFiniteModels.wl).  The
// symbolic clausification stays in WL (epClauseDB); this module runs the
// hot enumeration that the WL "ExpressionPrune" branch performs with
// pruneSelectTuples + extend + FromDigits.  It is a faithful 1:1 port of
// that WL code, so "ExpressionPruneC" is differential-identical to
// "ExpressionPrune".
//
// === clause DB encoding ===
//
// The relation is grounded + clausified into a list of clause-sets; each
// clause-set is a list of clauses; each clause is a list of literals.  A
// literal "cell c holds value v" (the WL `op[pos] == value`) is encoded
// as the integer  c * k + v,  where the cell id  c = offset[op] + pos
// numbers every operation-table cell across all operators (offset[op] =
// cumulative sum of preceding operators' table sizes, table size =
// k^arity).  ncells = sum of all table sizes.
//
// === algorithm (mirrors pruneSelectTuples / MapTake / extend) ===
//
//   - assign[ncells], each cell UNSET or a value 0..k-1, with an undo
//     trail.
//   - ffmep_prune: among the remaining (not-yet-consumed) clause-sets
//     pick First @ SortBy[remaining, Length] -- the smallest by current
//     clause count, ties broken by Mathematica's canonical Order
//     (ffmep_cs_cmp), so the MapTake enumeration order matches the WL.
//     Iterate that clause-set's clauses in order; for each clause
//     compatible with `assign` (no cell already bound to a different
//     value) bind its literals (trailed), forward-check the remaining
//     clause-sets to the clauses still compatible with `assign`, recurse,
//     and untrail on backtrack.  The MapTake cap (UpTo[maxItems])
//     truncates the solution-tuple stream at every level, exactly as the
//     WL MapTake[rec, first, MaxItems].
//   - at a leaf (no remaining clause-sets): the assignment is one
//     solution tuple; the cells left UNSET are free -- emit the cartesian
//     product over 0..k-1 for them (the WL `extend`); for each completed
//     assignment read each operator's cells assign[off..off+size-1] and
//     FromDigits base k to that operator's index, appending the
//     per-operator index list (one int per operator, operator order) to
//     the output.
//
// maxItems caps the number of SOLUTION TUPLES generated (the WL MapTake
// truncates pre-extend); each tuple can still expand to several models
// via the free-cell cartesian product.  The WL side then Union-sorts the
// emitted model list and applies the shared Take[UpTo[MaxItems]] tail, so
// "ExpressionPruneC" reproduces "ExpressionPrune" byte-for-byte.

// The clause-DB layout (FfmepDb) is declared in src/thvm.h so the
// LibraryLink bridge + the C test can populate it.  Literals: lit[] holds
// every literal-int, contiguous per clause; clause_off[c]..clause_off[c+1]
// index lit[] for clause c; set_off[s]..set_off[s+1] index set_clause[]
// (the clause ids) for clause-set s.

// === solver state ===
#define FFMEP_UNSET 0xffffffffu

// One remaining clause-set during the search: its set id plus its
// currently-compatible clause ids (forward-checked).  Each recursion
// level owns a fresh RemSet array (the WL Map[Select[...]] builds new
// lists), so a parent's clause lists are never mutated by a child.
typedef struct {
  u32  set;          // clause-set id
  u32 *clauses;      // owned: currently-compatible clause ids
  u32  n;            // clause count
} FfmepRemSet;

typedef struct {
  const FfmepDb *db;
  u32  *assign;      // [ncells] value 0..k-1 or FFMEP_UNSET
  u32  *trail;       // cells bound since the start (for undo)
  u32   trail_len;
  i64   max_items;   // -1 == Infinity (no cap)

  // output model list: nmodels rows of `nops` ints, row-major.
  i64  *out;
  u32   out_cap;     // capacity in MODELS (rows)
  u32   out_len;     // models emitted so far

  // scratch for FromDigits during a free-cell cartesian expansion
  u32  *free_cells;  // [ncells] ids of currently-free cells
} FfmepSolver;

// === literal helpers ===
static inline u32 ffmep_lit_cell(const FfmepDb *db, i64 lit) {
  return (u32)(lit / (i64)db->k);
}
static inline u32 ffmep_lit_val(const FfmepDb *db, i64 lit) {
  return (u32)(lit % (i64)db->k);
}

// === canonical (Mathematica Order) comparison of clause-sets ===
//
// The WL pruneSelectTuples picks First @ SortBy[remaining, Length]; on
// equal Length, SortBy breaks ties by the canonical Order of the
// clause-sets.  To pick the SAME clause-set (which determines the MapTake
// enumeration order, and thus the MaxItems prefix), C must reproduce that
// canonical order for our restricted expression class (clause-sets of
// And-of-Equals).  Mathematica's Order here is:
//   literal  op[pos] == val : by (op, pos, val).  Encoded as the integer
//     cellId*k + val with cellId in CANONICAL OPERATOR ORDER (the WL side
//     assigns offsets in Sort[ops] order), so a plain int compare matches.
//   clause   : a multi-literal And sorts BEFORE a single-literal Equal
//     (head And < head Equal); among Ands, fewer literals first then the
//     literal lists lexicographically; among Equals, by the lone literal.
//   clause-set (a List): Length ascending, then elementwise clause Order.
// The clause literals are stored canonically-sorted within each clause by
// the WL clausifier, so a positional compare suffices.

// Compare two clauses (given by their literal slices).  Returns <0 if A
// orders before B, >0 if after, 0 if equal.
static int ffmep_clause_cmp(const FfmepDb *db, u32 ca, u32 cb) {
  u32 a0 = db->clause_off[ca], a1 = db->clause_off[ca + 1];
  u32 b0 = db->clause_off[cb], b1 = db->clause_off[cb + 1];
  u32 na = a1 - a0, nb = b1 - b0;
  int a_eq = (na == 1), b_eq = (nb == 1);   // single-literal == bare Equal
  // And (na>1) before Equal (na==1).
  if (a_eq != b_eq) return a_eq ? 1 : -1;
  if (!a_eq) {
    // both And: fewer literals first.
    if (na != nb) return (na < nb) ? -1 : 1;
  }
  // same length: lexicographic by literal-int.
  for (u32 i = 0; i < na; i++) {
    i64 la = db->lit[a0 + i], lb = db->lit[b0 + i];
    if (la != lb) return (la < lb) ? -1 : 1;
  }
  return 0;
}

// Compare two (filtered) clause-sets by Length, then elementwise clause
// Order over their current clause lists.  Returns <0 / 0 / >0.
static int ffmep_cs_cmp(const FfmepDb *db, const u32 *acl, u32 an,
                        const u32 *bcl, u32 bn) {
  if (an != bn) return (an < bn) ? -1 : 1;
  for (u32 i = 0; i < an; i++) {
    int c = ffmep_clause_cmp(db, acl[i], bcl[i]);
    if (c != 0) return c;
  }
  return 0;
}

// A clause is compatible with the current assignment iff its literals,
// merged with the assignment, never give a cell two different values.
// This covers BOTH conflicts against already-bound cells AND internal
// contradictions (two literals of the SAME clause binding one cell to
// different values, e.g. the WL `cd[1]==0 && cd[1]==1` self-inconsistent
// clause).  Mirrors WL compatibleQ: group merged literals by cell, require
// SameQ values.  Detection here scans the clause and remembers each cell's
// committed value through the binding pass so the second literal of a
// contradictory pair is caught even when the first left the cell UNSET.
static int ffmep_clause_compatible(FfmepSolver *sv, u32 clause) {
  const FfmepDb *db = sv->db;
  u32 mark = sv->trail_len;
  int ok = 1;
  for (u32 i = db->clause_off[clause]; i < db->clause_off[clause + 1]; i++) {
    u32 cell = ffmep_lit_cell(db, db->lit[i]);
    u32 val  = ffmep_lit_val(db, db->lit[i]);
    if (sv->assign[cell] != FFMEP_UNSET) {
      if (sv->assign[cell] != val) { ok = 0; break; }
    } else {
      sv->assign[cell] = val;
      sv->trail[sv->trail_len++] = cell;
    }
  }
  // undo the tentative bindings made during the scan
  while (sv->trail_len > mark) {
    u32 cell = sv->trail[--sv->trail_len];
    sv->assign[cell] = FFMEP_UNSET;
  }
  return ok;
}

// Bind a clause's literals into the assignment, recording each newly-set
// cell on the trail.  The clause must already be compatible (so a literal
// that finds its cell already set agrees with it).  Returns the trail
// length before binding (the restore mark).
static u32 ffmep_bind_clause(FfmepSolver *sv, u32 clause) {
  const FfmepDb *db = sv->db;
  u32 mark = sv->trail_len;
  for (u32 i = db->clause_off[clause]; i < db->clause_off[clause + 1]; i++) {
    u32 cell = ffmep_lit_cell(db, db->lit[i]);
    u32 val  = ffmep_lit_val(db, db->lit[i]);
    if (sv->assign[cell] == FFMEP_UNSET) {
      sv->assign[cell] = val;
      sv->trail[sv->trail_len++] = cell;
    }
  }
  return mark;
}

static void ffmep_unbind(FfmepSolver *sv, u32 mark) {
  while (sv->trail_len > mark) {
    u32 cell = sv->trail[--sv->trail_len];
    sv->assign[cell] = FFMEP_UNSET;
  }
}

// === output ===
static void ffmep_out_grow(FfmepSolver *sv, u32 need_rows) {
  if (need_rows <= sv->out_cap) return;
  u32 cap = sv->out_cap ? sv->out_cap : 16u;
  while (cap < need_rows) cap *= 2u;
  sv->out = (i64 *)realloc(sv->out, (size_t)cap * sv->db->nops * sizeof(i64));
  sv->out_cap = cap;
}

// Emit every model of the current (leaf) assignment: the WL `extend`
// cartesian product over the free (UNSET) cells, then FromDigits base k
// per operator.  The WL EP encodes a model index as
// FromDigits[Reverse @ Table[op[pos], {pos, 0, size-1}], k], i.e. cell
// pos 0 is the LEAST-significant digit:  idx = sum_pos cell[pos] * k^pos.
static void ffmep_emit_models(FfmepSolver *sv) {
  const FfmepDb *db = sv->db;
  u32 k = db->k;
  // collect the free cells
  u32 nfree = 0;
  for (u32 c = 0; c < db->ncells; c++) {
    if (sv->assign[c] == FFMEP_UNSET) sv->free_cells[nfree++] = c;
  }
  // cartesian product over nfree cells in base k: total = k^nfree
  // (guarded against overflow by the caller's small-domain regime).
  u64 total = 1;
  for (u32 i = 0; i < nfree; i++) total *= (u64)k;
  for (u64 combo = 0; combo < total; combo++) {
    // set each free cell from the mixed-radix digits of `combo`
    u64 rest = combo;
    for (u32 i = 0; i < nfree; i++) {
      sv->assign[sv->free_cells[i]] = (u32)(rest % k);
      rest /= k;
    }
    ffmep_out_grow(sv, sv->out_len + 1u);
    i64 *row = &sv->out[(size_t)sv->out_len * db->nops];
    for (u32 op = 0; op < db->nops; op++) {
      i64 idx = 0;
      i64 place = 1;
      u32 base = db->op_off[op];
      u32 size = db->op_size[op];
      for (u32 cell = 0; cell < size; cell++) {
        idx += (i64)sv->assign[base + cell] * place;
        place *= (i64)k;
      }
      row[op] = idx;
    }
    sv->out_len++;
  }
  // leave free cells UNSET again for the caller
  for (u32 i = 0; i < nfree; i++) sv->assign[sv->free_cells[i]] = FFMEP_UNSET;
}

// === recursion: pruneSelectTuples + MapTake ===
//
// ffmep_prune mirrors the WL pruneSelectTuples: among the remaining
// clause-sets `rem` (n_rem of them) pick the smallest (MRV = WL
// SortBy[list, Length]); iterate that set's clauses in order; for each
// clause compatible with the assignment, bind it, build a FRESH list of
// remaining clause-sets forward-checked against the new assignment (the WL
// Map[Select[...]] makes new lists -- a child never mutates the parent's
// clause lists), recurse, and unbind.  `budget` is the MapTake cap for
// THIS level's solution-tuple stream (-1 == Infinity); the residual
// budget threads down via `budget - produced`, so the prefix of the
// fixed-order tuple stream is identical to the WL MapTake's.  Returns the
// number of solution tuples emitted by this call.
static i64 ffmep_prune(FfmepSolver *sv, FfmepRemSet *rem, u32 n_rem,
                       i64 budget) {
  if (budget == 0) return 0;
  // leaf: no remaining clause-sets -> the assignment is one solution
  // tuple; expand its free cells into models.
  if (n_rem == 0) {
    ffmep_emit_models(sv);
    return 1;
  }

  // MRV: First @ SortBy[remaining, Length] -- the smallest clause-set by
  // (Length, canonical Order), reproducing the WL tie-break so the MapTake
  // enumeration order (and thus the MaxItems prefix) matches.
  u32 pick = 0;
  for (u32 i = 1; i < n_rem; i++) {
    if (ffmep_cs_cmp(sv->db, rem[i].clauses, rem[i].n,
                     rem[pick].clauses, rem[pick].n) < 0) pick = i;
  }
  FfmepRemSet first = rem[pick];

  // the `rest` = the other clause-sets (set ids + their current clause
  // lists), packed into a contiguous array for the recursion.
  u32 n_rest = n_rem - 1u;
  FfmepRemSet *rest = NULL;
  if (n_rest > 0) {
    rest = (FfmepRemSet *)malloc(n_rest * sizeof(FfmepRemSet));
    u32 w = 0;
    for (u32 i = 0; i < n_rem; i++) {
      if (i != pick) rest[w++] = rem[i];
    }
  }

  i64 produced = 0;
  // iterate first's clauses IN ORDER (the WL MapTake walks them L-to-R).
  for (u32 ci = 0; ci < first.n; ci++) {
    u32 clause = first.clauses[ci];
    if (!ffmep_clause_compatible(sv, clause)) continue;
    u32 mark = ffmep_bind_clause(sv, clause);

    // forward-check: build a FRESH rest' list, each set's clauses filtered
    // to those still compatible with the extended assignment.
    FfmepRemSet *rest2 = NULL;
    int alloc_ok = 1;
    if (n_rest > 0) {
      rest2 = (FfmepRemSet *)malloc(n_rest * sizeof(FfmepRemSet));
      if (rest2 == NULL) { alloc_ok = 0; }
      for (u32 r = 0; alloc_ok && r < n_rest; r++) {
        rest2[r].set = rest[r].set;
        rest2[r].clauses = (u32 *)malloc((rest[r].n ? rest[r].n : 1u) * sizeof(u32));
        if (rest2[r].clauses == NULL) { rest2[r].n = 0; alloc_ok = 0; break; }
        u32 keep = 0;
        for (u32 j = 0; j < rest[r].n; j++) {
          if (ffmep_clause_compatible(sv, rest[r].clauses[j]))
            rest2[r].clauses[keep++] = rest[r].clauses[j];
        }
        rest2[r].n = keep;
      }
    }

    if (alloc_ok) {
      i64 child_budget = (budget < 0) ? -1 : (budget - produced);
      produced += ffmep_prune(sv, rest2, n_rest, child_budget);
    }

    if (rest2 != NULL) {
      for (u32 r = 0; r < n_rest; r++) free(rest2[r].clauses);
      free(rest2);
    }
    ffmep_unbind(sv, mark);

    if (budget >= 0 && produced >= budget) break;
  }

  free(rest);
  return produced;
}

// === entry point ===
//
// Solve the clause DB and return the per-operator model index lists.
// Output is a freshly malloc'd i64 buffer of (*out_nmodels) * nops ints
// (row-major), one row per emitted model; the caller owns + frees it.
// `max_items` < 0 means Infinity.  Returns NULL on allocation failure
// (with *out_nmodels == 0).
fn i64 *ffmep_solve(const FfmepDb *db, i64 max_items, u32 *out_nmodels) {
  *out_nmodels = 0;

  FfmepSolver sv;
  memset(&sv, 0, sizeof(sv));
  sv.db = db;
  sv.max_items = max_items;
  sv.assign     = (u32 *)malloc((db->ncells ? db->ncells : 1u) * sizeof(u32));
  sv.trail      = (u32 *)malloc((db->ncells ? db->ncells : 1u) * sizeof(u32));
  sv.free_cells = (u32 *)malloc((db->ncells ? db->ncells : 1u) * sizeof(u32));
  if (!sv.assign || !sv.trail || !sv.free_cells) {
    free(sv.assign); free(sv.trail); free(sv.free_cells);
    return NULL;
  }
  for (u32 c = 0; c < db->ncells; c++) sv.assign[c] = FFMEP_UNSET;

  // root remaining clause-sets: each set with its full clause list.
  FfmepRemSet *rem = NULL;
  if (db->nsets > 0) {
    rem = (FfmepRemSet *)malloc(db->nsets * sizeof(FfmepRemSet));
    if (rem == NULL) {
      free(sv.assign); free(sv.trail); free(sv.free_cells);
      return NULL;
    }
    for (u32 s = 0; s < db->nsets; s++) {
      u32 n = db->set_off[s + 1] - db->set_off[s];
      rem[s].set = s;
      rem[s].n = n;
      rem[s].clauses = (u32 *)malloc((n ? n : 1u) * sizeof(u32));
      for (u32 j = 0; j < n; j++) {
        rem[s].clauses[j] = db->set_clause[db->set_off[s] + j];
      }
    }
  }

  ffmep_prune(&sv, rem, db->nsets, max_items);

  if (rem != NULL) {
    for (u32 s = 0; s < db->nsets; s++) free(rem[s].clauses);
    free(rem);
  }

  *out_nmodels = sv.out_len;
  i64 *out = sv.out;   // hand ownership to the caller
  free(sv.assign); free(sv.trail); free(sv.free_cells);
  return out;
}
