// deep_stack.h - run a test block on a thread with a stack of a chosen
// size, for the deep-term tests (test_ft_deep, test_atp_deep,
// test_atp_cli_deep).
//
// The deep blocks run on DEEP_STACK_BYTES, so a walk that slips back to
// one C frame per level overflows there whatever `ulimit -s` the machine
// has (8 MB, or unlimited in many containers): a non-tail recursive call
// costs at least DEEP_MIN_FRAME bytes (return address and frame link),
// and DEEP_LEVELS of them need more than the deep stack holds.  The
// reference recursions the pins compare against run on REF_STACK_BYTES
// instead, which holds them at the depths the tests use.
//
// Include after ../src/thvm.c.

#ifndef THVM_DEEP_STACK_H
#define THVM_DEEP_STACK_H

#include <pthread.h>

#define DEEP_STACK_BYTES ((size_t)1u << 20)     // 1 MB
#define REF_STACK_BYTES  ((size_t)1u << 29)     // 512 MB, reserved lazily
#define DEEP_MIN_FRAME   16u
#define DEEP_LEVELS      100000u

typedef struct {
  void          (*body)(void *);
  void           *arg;
  WnfThreadState *wnf;
} DeepStackJob;

static void *deep_stack_main(void *p) {
  DeepStackJob *j = (DeepStackJob *)p;
  // The runtime's WNF state pointer is thread-local; the caller is
  // blocked in pthread_join, so the worker may borrow it.
  CURRENT_WNF_STATE = j->wnf;
  j->body(j->arg);
  return NULL;
}

// body(arg) on a fresh thread with a `bytes` stack; returns when it does.
static void deep_stack_run(size_t bytes, void (*body)(void *), void *arg) {
  DeepStackJob   job = { body, arg, CURRENT_WNF_STATE };
  pthread_attr_t attr;
  pthread_t      th;
  if (pthread_attr_init(&attr) != 0 ||
      pthread_attr_setstacksize(&attr, bytes) != 0 ||
      pthread_create(&th, &attr, deep_stack_main, &job) != 0) {
    fprintf(stderr, "deep_stack_run: cannot start a %zu-byte stack thread\n", bytes);
    exit(1);
  }
  pthread_join(th, NULL);
  pthread_attr_destroy(&attr);
}

typedef struct {
  void (*plain)(void);
  void (*seeded)(u32 *);
  u32  *seed;
} DeepCall;

static void deep_call_main(void *p) {
  DeepCall *c = (DeepCall *)p;
  if (c->plain != NULL) {
    c->plain();
  } else {
    c->seeded(c->seed);
  }
}

// A test's deep block, f() or f(seed), on the deep stack.
static void deep_stack_call(void (*f)(void)) {
  DeepCall c = { f, NULL, NULL };
  deep_stack_run(DEEP_STACK_BYTES, deep_call_main, &c);
}

static void deep_stack_call_seed(void (*f)(u32 *), u32 *seed) {
  DeepCall c = { NULL, f, seed };
  deep_stack_run(DEEP_STACK_BYTES, deep_call_main, &c);
}

// A block of reference comparisons, f(), on the reference stack.
static void ref_stack_call(void (*f)(void)) {
  DeepCall c = { f, NULL, NULL };
  deep_stack_run(REF_STACK_BYTES, deep_call_main, &c);
}

// The negative control: a regression to per-level recursion over
// DEEP_LEVELS levels cannot fit the deep stack.
static int deep_stack_catches_recursion(void) {
  return (u64)DEEP_LEVELS * DEEP_MIN_FRAME > (u64)DEEP_STACK_BYTES;
}

#endif // THVM_DEEP_STACK_H
