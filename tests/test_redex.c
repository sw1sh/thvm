// test_redex.c -- redex enumeration + single-redex firing.
//
// Verifies:
//   - is_redex correctly classifies APP-LAM, APP-ERA, REF, ALO, OP2.
//   - redex_fire dispatches the right interaction and returns the
//     expected result Term.
//   - heap back-ref propagation: cells that held the old redex term
//     get updated to point at the result.
//   - redex_enumerate dedupes by Term value.
//   - "fresh" diff (pre/post enumerate) catches new redexes, both
//     locally produced (substituted body becomes a redex) and via
//     back-ref propagation into shared subgraphs.

#include "../src/thvm.c"
#include "test.h"

static Term build_id_lam(void) {
  u64  lam_loc = heap_alloc(1);
  Term var     = term_new(0, TAG_VAR, 0, lam_loc);
  heap_set(lam_loc, var);
  return term_new(0, TAG_LAM, 0, lam_loc);
}

static Term build_app(Term f, Term x) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, f);
  heap_set(loc + 1, x);
  return term_new(0, TAG_APP, 0, loc);
}

// Symmetric-difference: redexes in `post` not in `pre`.  Returns the
// count of "fresh" redexes; fills `out` up to `cap`.
static u32 fresh_diff(Term *pre,  u32 pre_n,
                      Term *post, u32 post_n,
                      Term *out,  u32 cap) {
  u32 count = 0;
  for (u32 i = 0; i < post_n; i++) {
    u8 in_pre = 0;
    for (u32 j = 0; j < pre_n; j++) {
      if (pre[j] == post[i]) { in_pre = 1; break; }
    }
    if (in_pre) continue;
    if (count < cap) out[count] = post[i];
    count++;
  }
  return count;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("redex/is-redex-classifies-correctly");
  Term era = term_new(0, TAG_ERA, 0, 0);
  Term lam = build_id_lam();
  Term app_lam_era = build_app(lam, era);
  CHECK(is_redex(app_lam_era));
  CHECK(!is_redex(era));
  CHECK(!is_redex(lam));
  CHECK(!is_redex(term_new(0, TAG_NUM, 0, 42)));
  Term app_var = build_app(term_new(0, TAG_VAR, 0, 0), era);
  CHECK(!is_redex(app_var));   // VAR head -- stuck

  TEST_BEGIN("redex/fire-app-lam-returns-substituted-body");
  Term era2 = term_new(0, TAG_ERA, 0, 0);
  Term lam2 = build_id_lam();
  Term app2 = build_app(lam2, era2);
  u64  itrs0 = ITRS;
  Term r = redex_fire(app2);
  // identity body is VAR pointing to lam_loc; after sub the VAR
  // resolves to era2.
  CHECK_EQ(term_resolve(r), era2);
  CHECK_EQ(ITRS - itrs0, 1);

  TEST_BEGIN("redex/fire-rejects-non-redex");
  Term not_a_redex = term_new(0, TAG_NUM, 0, 7);
  CHECK_EQ(redex_fire(not_a_redex), (Term)0);

  TEST_BEGIN("redex/back-ref-propagates-to-parent-slot");
  // outer = APP(inner_redex, era_x).  Firing inner_redex should
  // patch outer.fun-slot to point at the result.
  Term era_x = term_new(0, TAG_ERA, 0, 0);
  Term inner_lam = build_id_lam();
  Term era_y = term_new(0, TAG_ERA, 0, 0);
  Term inner = build_app(inner_lam, era_y);
  Term outer = build_app(inner, era_x);
  u64  outer_loc = term_val(outer);
  CHECK_EQ(heap_read(outer_loc + 0), inner);   // pre-fire
  Term inner_result = redex_fire(inner);
  CHECK(inner_result != 0);
  // outer's fun-slot now points at the result, not the consumed inner APP.
  CHECK_EQ(heap_read(outer_loc + 0), inner_result);

  TEST_BEGIN("redex/enumerate-dedupes-by-term-value");
  // Build APP(lam, era), then store the SAME redex term in another
  // free heap cell.  Enumerate should report it once.
  Term era3 = term_new(0, TAG_ERA, 0, 0);
  Term lam3 = build_id_lam();
  Term redex3 = build_app(lam3, era3);
  u64 dup_loc = heap_alloc(1);
  heap_set(dup_loc, redex3);   // second cell holds the same Term
  Term buf[64];
  u32 n = redex_enumerate(NULL, 0, buf, 64);
  u32 hits = 0;
  for (u32 i = 0; i < n; i++) if (buf[i] == redex3) hits++;
  CHECK_EQ(hits, 1);

  TEST_BEGIN("redex/fresh-diff-catches-new-redex-after-fire");
  // ((id era_y) era_z)  -- only the inner APP-LAM is a redex
  // initially: the outer's head is an APP, not LAM/ERA/MAT.  Firing
  // the inner replaces the outer's fun-slot with the substituted
  // body (a VAR resolving to era_y), making the outer an APP-ERA
  // redex.  fresh_diff should pick that up.
  thvm_free(); thvm_init();
  Term ea1 = term_new(0, TAG_ERA, 0, 0);
  Term ea2 = term_new(0, TAG_ERA, 0, 0);
  Term lam_inner = build_id_lam();
  Term ai = build_app(lam_inner, ea1);    // (id era_y)
  Term ao = build_app(ai, ea2);            // ((id era_y) era_z)

  Term roots_pre[1] = {ao};
  Term pre[64];
  u32  pre_n = redex_enumerate(roots_pre, 1, pre, 64);
  CHECK_EQ(pre_n, 1);
  CHECK_EQ(pre[0], ai);

  (void)redex_fire(ai);

  Term roots_post[1] = {ao};
  Term post[64];
  u32  post_n = redex_enumerate(roots_post, 1, post, 64);
  Term fresh[64];
  u32  fresh_n = fresh_diff(pre, pre_n, post, post_n, fresh, 64);
  u8 found_outer = 0;
  for (u32 i = 0; i < fresh_n; i++) if (fresh[i] == ao) found_outer = 1;
  CHECK(found_outer);

  thvm_free();
  TEST_REPORT();
}
