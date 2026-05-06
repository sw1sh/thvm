// src/aot/task.h
//
// Bend2-style fork-emitting AOT runtime: types.
//
// This is the data layer that the CPS-emitted code (Phase 2+) and
// the worker pool (Phase 1) speak in.  Two top-level types:
//
//   Task    -- "evaluate function `fn` with these args; when you
//              have a value, write it to `ret`".  Workers grab
//              tasks off the global queue and dispatch via the
//              program's emitted `dispatch(task)` switch.
//
//   Result  -- what dispatch() returns: either a Value (this task
//              finished, here's the answer), a Split (this task
//              wants two child tasks evaluated -- the runtime
//              forks them and registers the continuation), or a
//              Call (this task wants ONE child evaluated then a
//              continuation fired -- the runtime tail-chains it).
//
// The continuation lives in a heap cell allocated via thvm's
// existing heap_alloc.  See src/aot/cont.c for the layout.
//
// We piggy-back on thvm's Term type rather than rolling Bend2's
// own (uint8 tag | uint32 loc) packing.  This keeps interop with
// the interpreter intact: AOT'd code consumes/produces Terms that
// the interpreter understands, and an AOT call can call back into
// the interpreter (e.g., to force a non-AOT'd dependency) without
// any conversion.

#ifndef THVM_AOT_TASK_H
#define THVM_AOT_TASK_H

#include "../thvm.h"

#define AOT_MAX_ARGS  4    // matches Bend2's MAX_ARGS; bump if a
                           // workload needs more positional args

// A Task is a queued unit of work.  fn_id is an opaque function id
// (the program's emit assigns FN_* / CONT_* constants), ret is an
// encoded "return slot" address (see enc_ret below), args are
// positional Term parameters.  Stays POD so the worker queue can
// memcpy them around.  The field is `fn_id`, not `fn`, because
// `fn` is a project-wide macro for `static inline`.
typedef struct AotTask {
  u32  fn_id;
  u32  ret;
  Term args[AOT_MAX_ARGS];
} AotTask;

// Tagged result of dispatching one task.  Mirrors Bend2's three
// shapes (R_VALUE / R_SPLIT / R_CALL).  Stored in a struct rather
// than a tagged union so a worker can spill it to a register pair
// without indirection.
#define AOT_R_VALUE  0    // val is the answer
#define AOT_R_SPLIT  1    // t0, t1 are two independent child tasks
#define AOT_R_CALL   2    // t0 is the next task in a tail chain

typedef struct AotResult {
  u32     tag;
  Term    val;
  AotTask t0;
  AotTask t1;
} AotResult;

// Return-slot encoding.  A continuation cell at heap loc `dp`
// reserves two slots for child results.  `enc_ret(dp, slot)` packs
// (dp, slot in {0, 1}) into a single u32 the resolve loop decodes.
//
//     ret = (dp << 1) | (slot & 1)
//
// The reserved sentinel `AOT_RET_ROOT` means "this task's value is
// the program's final answer; write to g_result and signal done".
#define AOT_RET_ROOT  0xFFFFFFFFu

static inline u32 aot_enc_ret(u32 dp, u32 slot) {
  return (dp << 1) | (slot & 1u);
}

static inline u32 aot_dec_dp(u32 ret)   { return ret >> 1; }
static inline u32 aot_dec_slot(u32 ret) { return ret & 1u; }

// Constructors.  The runtime never builds these directly except in
// `aot_dispatch`; CPS-emitted bodies call them.
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
