// test.h - minimal test harness used by every tests/test_*.c
//
// Each test is an independent C program. It #includes the single-TU
// build of the runtime (../src/thvm.c) and uses these macros to assert
// invariants and report results in a uniform format.

#ifndef THVM_TEST_H
#define THVM_TEST_H

#include <stdio.h>
#include <stdlib.h>

static int    test_failures = 0;
static int    test_total    = 0;
static const char *test_name = "<unnamed>";

#define TEST_BEGIN(name) do { test_name = (name); } while (0)

#define CHECK(cond) do {                                                    \
  test_total++;                                                              \
  if (!(cond)) {                                                             \
    test_failures++;                                                         \
    fprintf(stderr, "  FAIL %s:%d  %s  (%s)\n",                              \
            __FILE__, __LINE__, test_name, #cond);                           \
  }                                                                          \
} while (0)

#define CHECK_EQ(a, b) do {                                                  \
  test_total++;                                                              \
  unsigned long long _a = (unsigned long long)(a);                           \
  unsigned long long _b = (unsigned long long)(b);                           \
  if (_a != _b) {                                                            \
    test_failures++;                                                         \
    fprintf(stderr, "  FAIL %s:%d  %s  expected %s == %s, got 0x%llx vs 0x%llx\n", \
            __FILE__, __LINE__, test_name, #a, #b, _a, _b);                  \
  }                                                                          \
} while (0)

#define TEST_REPORT() do {                                                   \
  int _f = test_failures;                                                    \
  int _t = test_total;                                                       \
  if (_f == 0) {                                                             \
    printf("  ok    %d/%d\n", _t, _t);                                       \
    return 0;                                                                \
  } else {                                                                   \
    printf("  FAIL  %d/%d\n", _t - _f, _t);                                  \
    return 1;                                                                \
  }                                                                          \
} while (0)

// PENDING - used at the top of a test whose implementation arrives in a
// later PLAN.md step. Prints a notice and exits 0 so the suite stays green.
// Delete the call when the implementation lands and the body should run.
#define PENDING(reason) do {                                                 \
  printf("  pend  %s\n", (reason));                                          \
  return 0;                                                                  \
} while (0)

// Shared term builders used by multiple tests/test_*.c files.  The
// runtime types (`Term`, `u32`, `u64`) and helpers (`term_new_fvr`,
// `term_new`, `heap_alloc`, `heap_set`, `TAG_NUM`, `TAG_SUP`) come from
// ../src/thvm.c, which every test #includes before this header.

static inline Term mk_v(u32 id) { return term_new_fvr(id); }

static inline Term build_num(u32 v) { return term_new(0, TAG_NUM, 0, v); }

static inline Term build_sup(u32 lab, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_SUP, lab, loc);
}

#endif // THVM_TEST_H
