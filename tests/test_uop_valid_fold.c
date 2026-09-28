// test_uop_valid_fold.c - the opt-in THVM_VALID_FOLD index folds.
//
// uop_valid_fold_enabled() caches THVM_VALID_FOLD on its first call, so
// this binary sets it before building any UOp.  The default-off behaviour
// stays covered by test_uop_index_simplify, which never sets it.
//
// Each case evaluates the folded gate on every point where its valid
// holds, so a fold that changes the value fails even when its shape
// looks plausible.

#include "../src/thvm.c"
#include "test.h"

#define ENV_CAP 64
#define UNBOUND 1000003

typedef struct { u32 n; u32 aid[ENV_CAP]; i64 val[ENV_CAP]; } Env;

static void env_set(Env *env, u32 aid, i64 val) {
  for (u32 i = 0; i < env->n; i++) {
    if (env->aid[i] == aid) {
      env->val[i] = val;
      return;
    }
  }
  env->aid[env->n] = aid;
  env->val[env->n] = val;
  env->n++;
}

// Evaluate an index expression with RANGE leaves bound by axis id.  An
// unbound RANGE (a leaked fake) reads as UNBOUND so it cannot pass for 0.
static i64 ev(Term t, Env const *env) {
  u64 loc = term_val(t);
  switch (term_ext(t)) {
    case UOP_CONST: return (i32)term_val(heap_read(loc));
    case UOP_RANGE: {
      u32 aid = (u32)term_val(heap_read(loc));
      for (u32 i = 0; i < env->n; i++) {
        if (env->aid[i] == aid) {
          return env->val[i];
        }
      }
      return UNBOUND;
    }
    case UOP_IADD: return ev(heap_read(loc), env) + ev(heap_read(loc + 1), env);
    case UOP_ISUB: return ev(heap_read(loc), env) - ev(heap_read(loc + 1), env);
    case UOP_IMUL: return ev(heap_read(loc), env) * ev(heap_read(loc + 1), env);
    case UOP_IDIV: return ev(heap_read(loc), env) / ev(heap_read(loc + 1), env);
    case UOP_IMOD: return ev(heap_read(loc), env) % ev(heap_read(loc + 1), env);
    case UOP_ILT:  return ev(heap_read(loc), env) < ev(heap_read(loc + 1), env);
    case UOP_IAND: return ev(heap_read(loc), env) & ev(heap_read(loc + 1), env);
    case UOP_IOR:  return ev(heap_read(loc), env) | ev(heap_read(loc + 1), env);
    case UOP_IWHERE: {
      if (ev(heap_read(loc), env)) {
        return ev(heap_read(loc + 1), env);
      }
      return ev(heap_read(loc + 2), env);
    }
    default: return -UNBOUND;
  }
}

// 1 iff a RANGE minted by uop_make_fake_range occurs under `t`.
static int has_fake(Term t, u32 depth) {
  if (depth > 512 || term_tag(t) != TAG_UOP) {
    return 0;
  }
  u64 loc = term_val(t);
  switch (term_ext(t)) {
    case UOP_RANGE:
      return (u32)term_val(heap_read(loc)) >= UOP_VALID_FAKE_AXIS_BASE;
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR:
    case UOP_IXOR: case UOP_ISHR:
      return has_fake(heap_read(loc), depth + 1) || has_fake(heap_read(loc + 1), depth + 1);
    case UOP_IWHERE:
      return has_fake(heap_read(loc), depth + 1) || has_fake(heap_read(loc + 1), depth + 1)
          || has_fake(heap_read(loc + 2), depth + 1);
    default:
      return 0;
  }
}

static Term ic(i64 v) { return uop_const(DT_INT32, (u32)(i32)v); }

static Term then_of(Term w) { return heap_read(term_val(w) + 1); }

int main(void) {
  setenv("THVM_VALID_FOLD", "1", 1);
  thvm_init();
  CHECK(uop_valid_fold_enabled());

  Term inv = uop_invalid();
  Term r0  = uop_range(0, KAX_LOOP, 8);
  Term r1  = uop_range(1, KAX_LOOP, 8);
  Term e   = uop_int_binary(UOP_IADD, r0, r1);  // [0, 14]

  // A lower bound lo > 0 models e as fake+lo; the inverse must be
  // fake -> e-lo, or every surviving fake+lo comes back as e+lo.
  TEST_BEGIN("valid-fold/lo-offset-div");
  {
    Term valid = uop_int_binary(UOP_ILT, ic(3), e);  // e >= 4
    Term g = uop_iwhere(valid, uop_int_binary(UOP_IDIV, e, ic(8)), inv);
    CHECK(!has_fake(g, 0));
    int wrong = 0;
    for (i64 a = 0; a < 8; a++) {
      for (i64 b = 0; b < 8; b++) {
        Env env = { 0 };
        env_set(&env, 0, a);
        env_set(&env, 1, b);
        if (a + b >= 4 && ev(g, &env) != (a + b) / 8) {
          wrong++;
        }
      }
    }
    CHECK_EQ(wrong, 0);
    Env at22 = { 0 };
    env_set(&at22, 0, 2);
    env_set(&at22, 1, 2);
    CHECK_EQ(ev(g, &at22), 0);
  }

  TEST_BEGIN("valid-fold/lo-offset-mul-unchanged");
  {
    Term valid = uop_int_binary(UOP_ILT, ic(1), e);  // e >= 2
    Term body  = uop_int_binary(UOP_IMUL, e, ic(3));
    Term g = uop_iwhere(valid, body, inv);
    CHECK_EQ(term_ext(g), UOP_IWHERE);
    CHECK_EQ(then_of(g), body);
  }

  // e sits 44..54 levels under the gated body.  At 49 the forward pass
  // still reaches e, but the fake lands one level deeper after the +lo
  // hoists to the root, past uop_index_substitute's depth cap; the back
  // pass then leaves it in place.
  TEST_BEGIN("valid-fold/no-fake-leak-deep");
  {
    Term valid = uop_int_binary(UOP_ILT, ic(3), e);  // e >= 4
    int leaked = 0, changed = 0, wrong = 0;
    for (u32 k = 44; k <= 54; k++) {
      Term t = e;
      for (u32 i = 1; i <= k; i++) {
        t = uop_int_binary(UOP_IADD, t, uop_range(100 + i, KAX_LOOP, 2));
      }
      Term g = uop_iwhere(valid, t, inv);
      leaked  += has_fake(g, 0);
      changed += then_of(g) != t;
      Env env = { 0 };
      env_set(&env, 0, 3);
      env_set(&env, 1, 2);
      for (u32 i = 1; i <= k; i++) {
        env_set(&env, 100 + i, 1);
      }
      wrong += ev(g, &env) != 5 + (i64)k;
    }
    CHECK_EQ(leaked, 0);
    CHECK_EQ(changed, 0);
    CHECK_EQ(wrong, 0);
  }

  // tinygrad substitutes every bounds entry at once.  Alone, e0 -> [0,3]
  // leaves e1 at [0,14] and the sum's max at 17, so neither clause folds
  // the //8; together the max is 6.
  Term r2 = uop_range(2, KAX_LOOP, 8);
  Term r3 = uop_range(3, KAX_LOOP, 8);
  Term e1 = uop_int_binary(UOP_IADD, r2, r3);
  Term s  = uop_int_binary(UOP_IADD, e, e1);
  TEST_BEGIN("valid-fold/joint-two-clauses");
  {
    CHECK_EQ(heap_read(term_val(s) + 0), e);
    CHECK_EQ(heap_read(term_val(s) + 1), e1);
    Term valid = uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, e, ic(4)),
                                          uop_int_binary(UOP_ILT, e1, ic(4)));
    Term g = uop_iwhere(valid, uop_int_binary(UOP_IDIV, s, ic(8)), inv);
    CHECK_EQ(g, uop_iwhere(valid, ic(0), inv));
  }

  // A lower bound on one clause: e >= 8 and e1 <= 1 put the sum in
  // [8, 15], so the //8 is 1.
  TEST_BEGIN("valid-fold/joint-lo-offset");
  {
    Term valid = uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, ic(7), e),
                                          uop_int_binary(UOP_ILT, e1, ic(2)));
    Term g = uop_iwhere(valid, uop_int_binary(UOP_IDIV, s, ic(8)), inv);
    CHECK_EQ(g, uop_iwhere(valid, ic(1), inv));
  }

  // The #18478 shape: rb < 7 and ra*7+rb < 8.  The joint map holds both
  // x = ra*7+rb and its own addend rb; x must be replaced as a unit, with
  // the bare rb elsewhere bound separately.
  TEST_BEGIN("valid-fold/joint-nested-clauses");
  {
    Term ra = uop_range(20, KAX_LOOP, 2);
    Term rb = uop_range(21, KAX_LOOP, 8);
    Term x  = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, ra, ic(7)), rb);
    Term valid = uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, rb, ic(7)),
                                          uop_int_binary(UOP_ILT, x, ic(8)));
    Term body = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IDIV, x, ic(8)),
                                         uop_int_binary(UOP_IMOD, rb, ic(7)));
    Term g = uop_iwhere(valid, body, inv);
    CHECK_EQ(g, uop_iwhere(valid, rb, inv));
    CHECK(!has_fake(g, 0));
    int wrong = 0;
    for (i64 a = 0; a < 2; a++) {
      for (i64 b = 0; b < 8; b++) {
        Env env = { 0 };
        env_set(&env, 20, a);
        env_set(&env, 21, b);
        if (b < 7 && a * 7 + b < 8 && ev(g, &env) != (a * 7 + b) / 8 + b % 7) {
          wrong++;
        }
      }
    }
    CHECK_EQ(wrong, 0);
  }

  // The PAD lowering gates ISUB(r, s) on s-1 < r < hi.  The bare RANGE r
  // is a joint candidate too; r - s round-trips as r + (-s), tinygrad's
  // spelling of it, and stays put on the next pass.
  TEST_BEGIN("valid-fold/pad-gate-body");
  {
    Term r = uop_range(30, KAX_LOOP, 10);
    Term valid = uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, ic(2), r),
                                          uop_int_binary(UOP_ILT, r, ic(8)));
    Term g = uop_iwhere(valid, uop_int_binary(UOP_ISUB, r, ic(3)), inv);
    CHECK_EQ(then_of(g), uop_int_binary(UOP_IADD, r, ic(-3)));
    CHECK_EQ(uop_iwhere(valid, then_of(g), inv), g);
    int wrong = 0;
    for (i64 v = 3; v < 8; v++) {
      Env env = { 0 };
      env_set(&env, 30, v);
      wrong += ev(g, &env) != v - 3;
    }
    CHECK_EQ(wrong, 0);
  }

  // Simplex: r0 + r1 >= 1 means r0 >= 1 or r1 >= 1, and 2*r0 + r1 >= 1
  // holds under either.  Only try_simplex tries the per-addend branches;
  // the gated path passes 0 (tinygrad symbolic.py:432).
  TEST_BEGIN("valid-fold/simplex");
  {
    Term valid = uop_int_binary(UOP_ILT, ic(0), e);
    Term both  = uop_int_binary(UOP_ILT, ic(0),
                   uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r0, ic(2)), r1));
    CHECK_EQ(uop_given_valid(valid, both, 1), ic(1));
    CHECK_EQ(uop_given_valid(valid, both, 0), both);
    Term one_side = uop_int_binary(UOP_ILT, ic(0), r0);
    CHECK_EQ(uop_given_valid(valid, one_side, 1), one_side);
  }

  thvm_free();
  TEST_REPORT();
}
