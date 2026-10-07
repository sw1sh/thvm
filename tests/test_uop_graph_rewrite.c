// test_uop_graph_rewrite.c - named bottom-up UOp graph rewrite pass.

#include "../src/thvm.c"
#include "test.h"

#include <sys/wait.h>
#include <unistd.h>

static u32 alloc_f32_tensor(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static Term strip_neg(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_NEG) {
    return 0;
  }
  return heap_read(term_val(t));
}

// UOp nodes along src[0] starting at `t`: the depth of a left-leaning chain.
static u32 spine_len(Term t) {
  u32 n = 0;
  while (term_tag(t) == TAG_UOP && uop_arity(term_ext(t)) > 0) {
    n++;
    t = heap_read(term_val(t) + 0);
  }
  return n;
}

// Occurrences of `op` on every path from `root`.  Explicit stack, so the
// check itself has no depth limit.
static u32 count_op(Term root, u32 op) {
  u32   cap   = 1024;
  u32   sp    = 0;
  u32   n     = 0;
  Term *stack = (Term *)malloc(cap * sizeof(Term));
  stack[sp++] = root;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) {
      continue;
    }
    if (term_ext(t) == op) {
      n++;
    }
    u8 ar = uop_arity(term_ext(t));
    for (u8 i = 0; i < ar; i++) {
      if (sp == cap) {
        cap *= 2;
        stack = (Term *)realloc(stack, cap * sizeof(Term));
      }
      stack[sp++] = heap_read(term_val(t) + i);
    }
  }
  free(stack);
  return n;
}

// IADD chain `depth` deep over `bottom`, one fresh LOOP range per level.
static Term iadd_chain(Term bottom, u32 depth, u32 first_axis) {
  Term t = bottom;
  for (u32 i = 0; i < depth; i++) {
    t = uop_int_binary(UOP_IADD, t, uop_range(first_axis + i, KAX_LOOP, 2));
  }
  return t;
}

// OPT(target, kind, factor) -> target.
static Term rule_strip_opt(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_OPT) {
    return 0;
  }
  return uop_opt_target(t);
}

static UOpGraphRewriteRule const STRIP_OPT[] = {{"strip-opt", rule_strip_opt}};

static Term strip_opt(Term root) {
  return uop_graph_rewrite(root, STRIP_OPT, 1, NULL);
}

static Term rule_range_7_to_8(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) {
    return 0;
  }
  if (uop_range_axis_id(t) != 7) {
    return 0;
  }
  return uop_range(8, KAX_LOOP, 4);
}

static Term rule_range_8_to_7(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) {
    return 0;
  }
  if (uop_range_axis_id(t) != 8) {
    return 0;
  }
  return uop_range(7, KAX_LOOP, 4);
}

static Term rule_grow_extent(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) {
    return 0;
  }
  if (uop_range_axis_id(t) != 9) {
    return 0;
  }
  return uop_range(9, KAX_LOOP, uop_range_extent(t) + 1);
}

static Term rule_wrap_in_iadd(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) {
    return 0;
  }
  if (uop_range_axis_id(t) != 10) {
    return 0;
  }
  return uop_int_binary(UOP_IADD, t, uop_range(11, KAX_LOOP, 4));
}

// Runs the rewrite in a forked child with stderr captured into `err`.
// Returns the wait status.  The alarm turns a hang into a failure.
static int rewrite_in_child(Term root, UOpGraphRewriteRule const *rules,
                            u32 n_rules, char *err, size_t cap) {
  int fds[2];
  if (pipe(fds) != 0) {
    return -1;
  }
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    dup2(fds[1], 2);
    close(fds[1]);
    alarm(10);
    uop_graph_rewrite(root, rules, n_rules, NULL);
    _exit(0);
  }
  close(fds[1]);
  char   sink[256];
  size_t len = 0;
  for (;;) {
    char   *dst  = (len + 1 < cap) ? err + len : sink;
    size_t  room = (len + 1 < cap) ? cap - 1 - len : sizeof(sink);
    ssize_t got  = read(fds[0], dst, room);
    if (got <= 0) {
      break;
    }
    if (dst != sink) {
      len += (size_t)got;
    }
  }
  err[len] = 0;
  close(fds[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  return status;
}

// thvm_fatal exits 1 when no LibraryLink entry armed its recovery jump.
static int child_failed_loudly(int status, char const *err) {
  return WIFEXITED(status) && WEXITSTATUS(status) == 1
      && strstr(err, "thvm fatal: uop_graph_rewrite: ") != NULL;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("uop-graph-rewrite/bottom-up-rebuilds-parent");
  u32 ta = alloc_f32_tensor(4);
  u32 tb = alloc_f32_tensor(4);
  Term a = term_new(0, TAG_TEN, DT_FP32, ta);
  Term b = term_new(0, TAG_TEN, DT_FP32, tb);
  Term neg = uop_unary(UOP_NEG, a);
  Term root = uop_binary(UOP_MUL, neg, b);
  UOpGraphRewriteRule rules[] = {{"strip-neg", strip_neg}};
  Term out = uop_graph_rewrite(root, rules, 1, NULL);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_MUL);
  CHECK_EQ(heap_read(term_val(out) + 0), a);
  CHECK_EQ(heap_read(term_val(out) + 1), b);
  CHECK_EQ(uop_graph_rewrite_stat_hits("strip-neg"), 1);

  TEST_BEGIN("uop-graph-rewrite/memoizes-shared-subgraph");
  u32 tc = alloc_f32_tensor(4);
  Term c = term_new(0, TAG_TEN, DT_FP32, tc);
  Term shared_neg = uop_unary(UOP_NEG, a);
  Term left = uop_binary(UOP_MUL, shared_neg, b);
  Term right = uop_binary(UOP_MUL, shared_neg, c);
  Term sum = uop_binary(UOP_ADD, left, right);
  Term sum_out = uop_graph_rewrite(sum, rules, 1, NULL);
  CHECK_EQ(term_ext(sum_out), UOP_ADD);
  Term left_out = heap_read(term_val(sum_out) + 0);
  Term right_out = heap_read(term_val(sum_out) + 1);
  CHECK_EQ(heap_read(term_val(left_out) + 0), a);
  CHECK_EQ(heap_read(term_val(right_out) + 0), a);
  CHECK_EQ(uop_graph_rewrite_stat_hits("strip-neg"), 1);

  TEST_BEGIN("uop-graph-rewrite/preserves-movement-metadata");
  u32 widths[2] = {1, 2};
  Term pad = uop_pad(uop_unary(UOP_NEG, a), 1, widths);
  Term pad_out = uop_graph_rewrite(pad, rules, 1, NULL);
  CHECK_EQ(term_tag(pad_out), TAG_UOP);
  CHECK_EQ(term_ext(pad_out), UOP_PAD);
  u64 ploc = term_val(pad_out);
  CHECK_EQ(heap_read(ploc + 0), a);
  CHECK_EQ(term_val(heap_read(ploc + 1)), 1);
  CHECK_EQ(term_val(heap_read(ploc + 2)), 1);
  CHECK_EQ(term_val(heap_read(ploc + 3)), 2);
  CHECK_EQ(uop_graph_rewrite_stat_hits("strip-neg"), 1);

  TEST_BEGIN("uop-is-movement/classifies-ops");
  CHECK(uop_is_movement(UOP_PAD));
  CHECK(!uop_is_movement(UOP_ADD));

  TEST_BEGIN("uop-graph-rewrite/strips-opt-300-deep");
  Term opt_leaf = uop_opt(uop_range(0, KAX_LOOP, 2), UOP_OPT_UNROLL, 2);
  Term deep300 = iadd_chain(opt_leaf, 300, 1);
  CHECK(spine_len(deep300) >= 300);
  CHECK_EQ(count_op(deep300, UOP_OPT), 1);
  Term deep300_out = strip_opt(deep300);
  CHECK_EQ(count_op(deep300_out, UOP_OPT), 0);
  CHECK(spine_len(deep300_out) >= 300);

  TEST_BEGIN("uop-graph-rewrite/strips-opt-2000-deep");
  Term deep2000 = iadd_chain(opt_leaf, 2000, 1);
  CHECK(spine_len(deep2000) >= 2000);
  Term deep2000_out = strip_opt(deep2000);
  CHECK_EQ(count_op(deep2000_out, UOP_OPT), 0);
  CHECK(spine_len(deep2000_out) >= 2000);

  TEST_BEGIN("uop-graph-rewrite/shared-node-shallow-then-deep");
  // The shallow occurrence is visited first; the deep one must reuse its
  // rewrite rather than come back raw.
  Term shallow = uop_int_binary(UOP_IADD, opt_leaf,
                                uop_range(5000, KAX_LOOP, 2));
  Term deep_side = iadd_chain(opt_leaf, 300, 1);
  Term both = uop_int_binary(UOP_IADD, shallow, deep_side);
  CHECK_EQ(heap_read(term_val(both) + 0), shallow);
  CHECK_EQ(count_op(both, UOP_OPT), 2);
  Term both_out = strip_opt(both);
  CHECK_EQ(count_op(both_out, UOP_OPT), 0);
  CHECK_EQ(uop_graph_rewrite_stat_hits("strip-opt"), 1);

  char child_err[4096];

  TEST_BEGIN("uop-graph-rewrite/ping-pong-fails-loudly");
  UOpGraphRewriteRule ping_pong[] = {
    {"range-7-to-8", rule_range_7_to_8},
    {"range-8-to-7", rule_range_8_to_7},
  };
  Term pp_root = uop_int_binary(UOP_IADD, uop_range(7, KAX_LOOP, 4),
                                uop_range(12, KAX_LOOP, 4));
  int pp_status = rewrite_in_child(pp_root, ping_pong, 2,
                                   child_err, sizeof(child_err));
  CHECK(child_failed_loudly(pp_status, child_err));
  CHECK(strstr(child_err, "revisits a node") != NULL);
  CHECK(strstr(child_err, "range-7-to-8") != NULL);
  CHECK(strstr(child_err, "range-8-to-7") != NULL);

  TEST_BEGIN("uop-graph-rewrite/runaway-chain-fails-loudly");
  UOpGraphRewriteRule runaway[] = {{"grow-extent", rule_grow_extent}};
  int ra_status = rewrite_in_child(uop_range(9, KAX_LOOP, 4), runaway, 1,
                                   child_err, sizeof(child_err));
  CHECK(child_failed_loudly(ra_status, child_err));
  CHECK(strstr(child_err, "chain budget exceeded") != NULL);
  CHECK(strstr(child_err, "grow-extent") != NULL);

  TEST_BEGIN("uop-graph-rewrite/self-containing-replacement-fails-loudly");
  UOpGraphRewriteRule self_wrap[] = {{"wrap-in-iadd", rule_wrap_in_iadd}};
  int sw_status = rewrite_in_child(uop_range(10, KAX_LOOP, 4), self_wrap, 1,
                                   child_err, sizeof(child_err));
  CHECK(child_failed_loudly(sw_status, child_err));
  CHECK(strstr(child_err, "structural depth budget exceeded") != NULL);
  CHECK(strstr(child_err, "wrap-in-iadd") != NULL);

  thvm_free();
  TEST_REPORT();
}
