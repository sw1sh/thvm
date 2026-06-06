// test_ffmep.c - finite-model "ExpressionPrune" hot enumeration (C port).
//
// Unit coverage of src/ffmep/_.c against small hand-checked clause DBs:
// the integer-encoded literal DB (cell c, value v -> c*k + v) is solved
// and the produced per-operator model index lists are compared (as a
// sorted set, like the WL Union) to indices computed by hand / verified
// against TFindFiniteModels[..., Method -> "ExpressionPrune"].

#include "../src/thvm.c"
#include "test.h"

#include <string.h>

// === expected-set comparison ==========================================
//
// The solver emits models in EP enumeration order (with duplicates,
// before the WL Union).  Compare the Union (sorted, deduped) of the
// emitted rows to the expected sorted set.

static int row_cmp(const i64 *a, const i64 *b, u32 nops) {
  for (u32 i = 0; i < nops; i++) {
    if (a[i] < b[i]) return -1;
    if (a[i] > b[i]) return 1;
  }
  return 0;
}

static void sort_rows(i64 *rows, u32 n, u32 nops) {
  // simple insertion sort (test sets are tiny)
  for (u32 i = 1; i < n; i++) {
    i64 tmp[8];
    memcpy(tmp, &rows[(size_t)i * nops], nops * sizeof(i64));
    u32 j = i;
    while (j > 0 && row_cmp(&rows[(size_t)(j - 1) * nops], tmp, nops) > 0) {
      memcpy(&rows[(size_t)j * nops], &rows[(size_t)(j - 1) * nops],
             nops * sizeof(i64));
      j--;
    }
    memcpy(&rows[(size_t)j * nops], tmp, nops * sizeof(i64));
  }
}

// Union the solver output, then compare to the expected sorted rows.
static int union_equals(i64 *out, u32 nmodels, u32 nops,
                        const i64 *expect, u32 nexpect) {
  sort_rows(out, nmodels, nops);
  // dedup in place
  u32 u = 0;
  for (u32 i = 0; i < nmodels; i++) {
    if (u == 0 || row_cmp(&out[(size_t)(u - 1) * nops],
                          &out[(size_t)i * nops], nops) != 0) {
      memcpy(&out[(size_t)u * nops], &out[(size_t)i * nops],
             nops * sizeof(i64));
      u++;
    }
  }
  if (u != nexpect) return 0;
  for (u32 i = 0; i < u; i++) {
    if (row_cmp(&out[(size_t)i * nops], &expect[(size_t)i * nops], nops) != 0)
      return 0;
  }
  return 1;
}

// === fixtures =========================================================

// f[a,b] == f[b,a], k = 2.  One op (arity 2, table size 4, offset 0).
// One clause-set with two clauses:
//   {f[1]==0 && f[2]==0,  f[1]==1 && f[2]==1}
// literal c*k+v:  f[1]==0 -> 2, f[2]==0 -> 4 ; f[1]==1 -> 3, f[2]==1 -> 5.
// Free cells f[0],f[3] -> 4 models per clause; Union = {0,1,6,7,8,9,14,15}.
static void test_commut_k2(void) {
  TEST_BEGIN("commutativity k=2");
  u32 op_off[2]  = {0, 4};
  u32 op_size[1] = {4};
  i64 lit[4]     = {2, 4, 3, 5};     // clause0 {2,4}, clause1 {3,5}
  u32 clause_off[3] = {0, 2, 4};     // two clauses
  u32 set_off[2]    = {0, 2};        // one set with clauses {0,1}
  u32 set_clause[2] = {0, 1};

  FfmepDb db;
  memset(&db, 0, sizeof(db));
  db.k = 2; db.ncells = 4; db.nops = 1;
  db.op_off = op_off; db.op_size = op_size;
  db.lit = lit; db.clause_off = clause_off;
  db.nsets = 1; db.set_off = set_off; db.set_clause = set_clause;

  u32 n = 0;
  i64 *out = ffmep_solve(&db, -1, &n);
  i64 expect[8] = {0, 1, 6, 7, 8, 9, 14, 15};
  CHECK(out != NULL);
  CHECK(union_equals(out, n, 1, expect, 8));
  free(out);
}

// f[a,b] == a, k = 2.  Four singleton clause-sets, each one clause:
//   {{f[3]==0}, {f[2]==0}, {f[1]==1}, {f[0]==1}}  (from EP).
// All cells fixed: f[0]=1,f[1]=1,f[2]=0,f[3]=0.  EP index =
// FromDigits[Reverse@{f0,f1,f2,f3}, 2] = f0 + 2 f1 + 4 f2 + 8 f3
//   = 1 + 2 + 0 + 0 = 3.  (TFindFiniteModels returns {3}.)
static void test_fab_eq_a_k2(void) {
  TEST_BEGIN("f[a,b]==a k=2");
  // EP clauses: f[3]==0, f[2]==0, f[1]==1, f[0]==1.
  // cell*k+v: f[3]==0 -> 6, f[2]==0 -> 4, f[1]==1 -> 3, f[0]==1 -> 1.
  u32 op_off[2]  = {0, 4};
  u32 op_size[1] = {4};
  i64 lit[4]     = {6, 4, 3, 1};
  u32 clause_off[5] = {0, 1, 2, 3, 4};   // four singleton clauses
  u32 set_off[5]    = {0, 1, 2, 3, 4};   // four singleton sets
  u32 set_clause[4] = {0, 1, 2, 3};

  FfmepDb db;
  memset(&db, 0, sizeof(db));
  db.k = 2; db.ncells = 4; db.nops = 1;
  db.op_off = op_off; db.op_size = op_size;
  db.lit = lit; db.clause_off = clause_off;
  db.nsets = 4; db.set_off = set_off; db.set_clause = set_clause;

  u32 n = 0;
  i64 *out = ffmep_solve(&db, -1, &n);
  i64 expect[1] = {3};
  CHECK(out != NULL);
  CHECK(union_equals(out, n, 1, expect, 1));
  free(out);
}

// f[a] == a, k = 2.  Two singleton clause-sets: {f[1]==0},{f[0]==1}.
// cell*k+v: f[1]==0 -> 2, f[0]==1 -> 1.  All cells fixed: f0=1,f1=0.
// EP index = FromDigits[Reverse@{f0,f1}, 2] = f0 + 2 f1 = 1.  (EP {1}.)
static void test_fa_eq_a_k2(void) {
  TEST_BEGIN("f[a]==a k=2");
  u32 op_off[2]  = {0, 2};
  u32 op_size[1] = {2};
  i64 lit[2]     = {2, 1};            // f[1]==0 -> 2, f[0]==1 -> 1
  u32 clause_off[3] = {0, 1, 2};
  u32 set_off[3]    = {0, 1, 2};
  u32 set_clause[2] = {0, 1};

  FfmepDb db;
  memset(&db, 0, sizeof(db));
  db.k = 2; db.ncells = 2; db.nops = 1;
  db.op_off = op_off; db.op_size = op_size;
  db.lit = lit; db.clause_off = clause_off;
  db.nsets = 2; db.set_off = set_off; db.set_clause = set_clause;

  u32 n = 0;
  i64 *out = ffmep_solve(&db, -1, &n);
  i64 expect[1] = {1};               // f0 + 2 f1 = 1
  CHECK(out != NULL);
  CHECK(union_equals(out, n, 1, expect, 1));
  free(out);
}

// No clause-sets at all (a relation that grounds to True): the single
// empty assignment expands every cell freely.  With one op arity 1 (k=2,
// table size 2) -> 4 models {0,1,2,3}.
static void test_all_free(void) {
  TEST_BEGIN("no clause-sets -> all models");
  u32 op_off[2]  = {0, 2};
  u32 op_size[1] = {2};
  FfmepDb db;
  memset(&db, 0, sizeof(db));
  db.k = 2; db.ncells = 2; db.nops = 1;
  db.op_off = op_off; db.op_size = op_size;
  db.lit = NULL; db.clause_off = NULL;
  db.nsets = 0; db.set_off = (u32[]){0}; db.set_clause = NULL;

  u32 n = 0;
  i64 *out = ffmep_solve(&db, -1, &n);
  i64 expect[4] = {0, 1, 2, 3};
  CHECK(out != NULL);
  CHECK(union_equals(out, n, 1, expect, 4));
  free(out);
}

// MaxItems cap: the commutativity k=2 DB with max_items = 1 caps the
// solution tuples to one (clause 0), which expands to 4 models {0,1,8,9}
// (the free cells f0,f3).  The WL side then Union-sorts + Take[1] -> {0}.
// Here we assert the cap limits the tuple count: only clause-0 models.
static void test_max_items_cap(void) {
  TEST_BEGIN("max_items tuple cap");
  u32 op_off[2]  = {0, 4};
  u32 op_size[1] = {4};
  i64 lit[4]     = {2, 4, 3, 5};
  u32 clause_off[3] = {0, 2, 4};
  u32 set_off[2]    = {0, 2};
  u32 set_clause[2] = {0, 1};

  FfmepDb db;
  memset(&db, 0, sizeof(db));
  db.k = 2; db.ncells = 4; db.nops = 1;
  db.op_off = op_off; db.op_size = op_size;
  db.lit = lit; db.clause_off = clause_off;
  db.nsets = 1; db.set_off = set_off; db.set_clause = set_clause;

  u32 n = 0;
  i64 *out = ffmep_solve(&db, 1, &n);     // cap = 1 solution tuple
  // one tuple (clause 0: f1=0,f2=0) -> free f0,f3 -> {0,1,8,9}
  i64 expect[4] = {0, 1, 8, 9};
  CHECK(out != NULL);
  CHECK(n == 4);                          // 1 tuple * 4 free combos
  CHECK(union_equals(out, n, 1, expect, 4));
  free(out);
}

// Two operators: f arity 1 (cells 0,1) + g arity 0 (cell 2), k = 2.
// Clause-sets fix f[0]=1,f[1]=1 (so f index = [1,1] -> 3) and g[0]=1
// (g index = 1).  One model {3, 1}.
static void test_two_ops(void) {
  TEST_BEGIN("two operators f + g");
  u32 op_off[3]  = {0, 2, 3};        // f cells [0,2), g cell [2,3)
  u32 op_size[2] = {2, 1};
  // f[0]==1 -> cell0 val1 -> 1 ; f[1]==1 -> cell1 val1 -> 3 ;
  // g[0]==1 -> cell2 val1 -> 5.
  i64 lit[3]     = {1, 3, 5};
  u32 clause_off[4] = {0, 1, 2, 3};
  u32 set_off[4]    = {0, 1, 2, 3};
  u32 set_clause[3] = {0, 1, 2};

  FfmepDb db;
  memset(&db, 0, sizeof(db));
  db.k = 2; db.ncells = 3; db.nops = 2;
  db.op_off = op_off; db.op_size = op_size;
  db.lit = lit; db.clause_off = clause_off;
  db.nsets = 3; db.set_off = set_off; db.set_clause = set_clause;

  u32 n = 0;
  i64 *out = ffmep_solve(&db, -1, &n);
  i64 expect[2] = {3, 1};
  CHECK(out != NULL);
  CHECK(union_equals(out, n, 2, expect, 1));
  free(out);
}

int main(void) {
  test_commut_k2();
  test_fab_eq_a_k2();
  test_fa_eq_a_k2();
  test_all_free();
  test_max_items_cap();
  test_two_ops();
  TEST_REPORT();
}
