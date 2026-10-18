// src/aot/task.h -- AOT (ahead-of-time) runtime types.  dispatch(task) runs
// fn_id on args and returns a VALUE for task->ret, a SPLIT (two independent
// child tasks joined by a continuation cell, cont.c) or a CALL (one task to
// run next).  Values are thvm Terms rather than Bend2's (uint8 tag, uint32
// loc) packing, so AOT code and the interpreter exchange terms directly.

#ifndef THVM_AOT_TASK_H
#define THVM_AOT_TASK_H

#include "../thvm.h"

#define AOT_MAX_ARGS  4    // Bend2's MAX_ARGS

// ret is an encoded return slot (aot_enc_ret).  The id field is fn_id
// because `fn` is the project's `static inline` macro.
typedef struct AotTask {
  u32  fn_id;
  u32  ret;
  Term args[AOT_MAX_ARGS];
} AotTask;

#define AOT_R_VALUE  0    // val is the answer
#define AOT_R_SPLIT  1    // t0, t1 are two independent child tasks
#define AOT_R_CALL   2    // t0 is the next task in a tail chain

typedef struct AotResult {
  u32     tag;
  Term    val;
  AotTask t0;
  AotTask t1;
} AotResult;

// A return slot packs a continuation cell's loc dp with child slot 0 or 1.
// AOT_RET_ROOT marks the final answer, which aot_resolve stores in
// program->result.
#define AOT_RET_ROOT  0xFFFFFFFFu

static inline u32 aot_enc_ret(u32 dp, u32 slot) {
  return (dp << 1) | (slot & 1u);
}

static inline u32 aot_dec_dp(u32 ret)   { return ret >> 1; }
static inline u32 aot_dec_slot(u32 ret) { return ret & 1u; }

static inline AotTask aot_make_task(u32 fn_id, u32 ret,
                                    Term a0, Term a1, Term a2, Term a3) {
  AotTask t;
  t.fn_id = fn_id; t.ret = ret;
  t.args[0] = a0; t.args[1] = a1; t.args[2] = a2; t.args[3] = a3;
  return t;
}

static inline AotResult aot_make_value(Term v) {
  AotResult r; r.tag = AOT_R_VALUE; r.val = v; return r;
}

static inline AotResult aot_make_split(AotTask a, AotTask b) {
  AotResult r; r.tag = AOT_R_SPLIT; r.t0 = a; r.t1 = b; return r;
}

static inline AotResult aot_make_call(AotTask a) {
  AotResult r; r.tag = AOT_R_CALL; r.t0 = a; return r;
}

#endif  // THVM_AOT_TASK_H
