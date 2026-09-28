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

  thvm_free();
  TEST_REPORT();
}
