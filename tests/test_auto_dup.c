// test_auto_dup.c - C-side auto-dup at LAM seal.
//
// Verifies that lam_seal_ext_with_auto_dup builds a DUP chain when
// the LAM body references its binder more than once, leaves a
// linear body untouched, and sets LAM_ERA_MASK on a body that
// doesn't reference the binder at all.

#include "../src/thvm.c"
#include "test.h"

// Build APP(f, x).
static Term ad_app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

// Build OP2(op, x, y).
static Term ad_op2(u32 op, Term x, Term y) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, x);
  heap_set(loc + 1, y);
  return term_new(0, TAG_OP2, op, loc);
}

static Term ad_num(u32 v) { return term_new(0, TAG_NUM, DT_INT32, v); }

// Build (lam x. body) sealed via the auto-dup-aware path.  Caller
// passes a builder that, given lam_loc, returns the body term.
typedef Term (*ad_body_fn)(u64 lam_loc);

static Term ad_lam(ad_body_fn body_fn) {
  u64  lam_loc = heap_alloc(1);
  Term body    = body_fn(lam_loc);
  heap_set(lam_loc, body);
  u32  ext     = lam_seal_ext_with_auto_dup(lam_loc, 0);
  return term_new(0, TAG_LAM, ext, lam_loc);
}

// Body builders.

// (\x -> x)  -- linear, ERA_MASK off, no auto-dup.
static Term body_id(u64 lam_loc) {
  return term_new(0, TAG_VAR, 0, lam_loc);
}

// (\x -> 42)  -- unused, ERA_MASK on.
static Term body_const_42(u64 lam_loc) {
  (void)lam_loc;
  return ad_num(42);
}

// (\x -> x + x)  -- two uses; auto-dup inserts a single DUP.
static Term body_x_plus_x(u64 lam_loc) {
  Term v0 = term_new(0, TAG_VAR, 0, lam_loc);
  Term v1 = term_new(0, TAG_VAR, 0, lam_loc);
  return ad_op2(OP_ADD, v0, v1);
}

// (\x -> x + (x + x))  -- three uses; auto-dup chains two DUPs.
static Term body_x_plus_x_plus_x(u64 lam_loc) {
  Term v0    = term_new(0, TAG_VAR, 0, lam_loc);
  Term v1    = term_new(0, TAG_VAR, 0, lam_loc);
  Term v2    = term_new(0, TAG_VAR, 0, lam_loc);
  Term inner = ad_op2(OP_ADD, v1, v2);
  return ad_op2(OP_ADD, v0, inner);
}

int main(void) {
  thvm_init();

  // === Linear body: identity LAM applied to NUM(7) returns NUM(7). ===
  TEST_BEGIN("auto-dup/linear-identity");
  Term lam_id   = ad_lam(body_id);
  CHECK_EQ(term_tag(lam_id), TAG_LAM);
  // No ERA_MASK, no DUP chain rewrite.
  CHECK_EQ(term_ext(lam_id) & LAM_ERA_MASK, 0);
  Term r1 = wnf(ad_app(lam_id, ad_num(7)));
  CHECK_EQ(term_tag(r1), TAG_NUM);
  CHECK_EQ(term_val(r1), 7);

  // === Unused binder: LAM with body = NUM(42), ERA_MASK on. ===
  TEST_BEGIN("auto-dup/unused-binder-era-mask");
  Term lam_const = ad_lam(body_const_42);
  CHECK_EQ(term_tag(lam_const), TAG_LAM);
  CHECK(term_ext(lam_const) & LAM_ERA_MASK);
  Term r2 = wnf(ad_app(lam_const, ad_num(99)));
  CHECK_EQ(term_tag(r2), TAG_NUM);
  CHECK_EQ(term_val(r2), 42);

  // === Two uses: x + x applied to NUM(5) -> NUM(10). ===
  TEST_BEGIN("auto-dup/two-uses-x-plus-x");
  Term lam_2 = ad_lam(body_x_plus_x);
  CHECK_EQ(term_tag(lam_2), TAG_LAM);
  CHECK_EQ(term_ext(lam_2) & LAM_ERA_MASK, 0);
  // After auto-dup, the body cell at lam_loc should still be an OP2
  // root (we only rewrote VAR cells, not the body root).
  Term body_2 = heap_read(term_val(lam_2));
  CHECK_EQ(term_tag(body_2), TAG_OP2);
  Term r3 = wnf(ad_app(lam_2, ad_num(5)));
  CHECK_EQ(term_tag(r3), TAG_NUM);
  CHECK_EQ(term_val(r3), 10);

  // === Three uses: x + (x + x) applied to NUM(4) -> NUM(12). ===
  TEST_BEGIN("auto-dup/three-uses-x-plus-x-plus-x");
  Term lam_3 = ad_lam(body_x_plus_x_plus_x);
  CHECK_EQ(term_tag(lam_3), TAG_LAM);
  Term r4 = wnf(ad_app(lam_3, ad_num(4)));
  CHECK_EQ(term_tag(r4), TAG_NUM);
  CHECK_EQ(term_val(r4), 12);

  // === Five uses: still works (linear chain depth 4). ===
  TEST_BEGIN("auto-dup/five-uses");
  // Build (\x -> x + x + x + x + x) via repeated ad_op2.
  // body builder isn't a typedef'd fn here; inline the construction.
  u64  lam_loc_5 = heap_alloc(1);
  Term v[5];
  for (int i = 0; i < 5; i++) {
    v[i] = term_new(0, TAG_VAR, 0, lam_loc_5);
  }
  // Right-fold: v0 + (v1 + (v2 + (v3 + v4)))
  Term acc = v[4];
  for (int i = 3; i >= 0; i--) {
    acc = ad_op2(OP_ADD, v[i], acc);
  }
  heap_set(lam_loc_5, acc);
  u32  ext_5 = lam_seal_ext_with_auto_dup(lam_loc_5, 0);
  Term lam_5 = term_new(0, TAG_LAM, ext_5, lam_loc_5);
  Term r5    = wnf(ad_app(lam_5, ad_num(3)));
  CHECK_EQ(term_tag(r5), TAG_NUM);
  CHECK_EQ(term_val(r5), 15);  // 5 * 3

  thvm_free();
  TEST_REPORT();
}
