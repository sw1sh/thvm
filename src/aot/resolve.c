// src/aot/resolve.c -- carry a finished value up the continuation tree, as
// Bend2's resolve (external/TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c:491).

#include "task.h"

// The program's switch on fn_id: emit.c's aot_program_<name>_dispatch, or a
// hand-written one in tests.
struct AotProgram;
typedef AotResult (*AotDispatchFn)(struct AotProgram *p, AotTask *task);

typedef struct AotProgram {
  AotDispatchFn  dispatch;
  // Set by aot_resolve at AOT_RET_ROOT; the runners stop once done is 1.
  Term           result;
  _Atomic u32    done;
} AotProgram;

// Tail-chains values through fired conts until the root, a cont still waiting
// for its other child, or a cont returning SPLIT / CALL, whose children (at
// most 2) go to out[*out_n] (a caller-private count).
fn void aot_resolve(AotProgram *program,
                    u32 ret, Term val,
                    AotTask *out, u32 *out_n) {
  while (1) {
    if (ret == AOT_RET_ROOT) {
      program->result = val;
      atomic_store_explicit(&program->done, 1, memory_order_release);
      return;
    }
    if (!aot_write_slot(ret, val)) return;

    u32 dp = aot_dec_dp(ret);
    AotTask t = aot_fire_cont(dp);
    AotResult r = program->dispatch(program, &t);

    if (r.tag == AOT_R_VALUE) {
      val = r.val;
      ret = t.ret;
      continue;
    }
    if (r.tag == AOT_R_SPLIT) {
      u32 i = *out_n;
      out[i + 0] = r.t0;
      out[i + 1] = r.t1;
      *out_n = i + 2;
      return;
    }
    // AOT_R_CALL
    u32 i = *out_n;
    out[i] = r.t0;
    *out_n = i + 1;
    return;
  }
}
