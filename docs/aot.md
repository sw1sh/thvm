# AOT (ahead-of-time compilation)

thvm's AOT compiles a `TDef`'d body to fork-friendly C source. The
output has the same shape as Bend2's compiler at
[TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c](../TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c).
The runtime is in `src/aot/`; the WL surface is `THVMLink`TAOTEmit`,
`TAOTCompile`, `TAOTRun`, `TAOTPath`.

---

## Model

The interpreter (`src/wnf/`) is canonical: REPL, debugging, hot reload.
AOT layers on top as the fast path for compute-heavy defs.

The compile target is **CPS-transformed, fork-emitting C**, not a
spine-based interpreter shim. When a def's body says
`node{count(p), count(p)}` (two independent recursive calls inside a
constructor), the emit produces:

```c
u64 dp = aot_alloc_cont(CONT_count_0, 0, t->ret);
return aot_make_split(
    aot_make_task(FN_count, aot_enc_ret((u32)dp, 0), p, 0, 0, 0),
    aot_make_task(FN_count, aot_enc_ret((u32)dp, 1), p, 0, 0, 0));
```

The runtime grabs both tasks off a shared queue, dispatches them on
separate workers, and the cont fires when both children land. **The
emit tells the runtime where the parallelism is** rather than relying
on a generic interpreter to discover it.

Three result shapes per `dispatch(task)` call:

- `R_VALUE(term)` - task is done, here's the answer.
- `R_CALL(task)` - tail-chain into the next task.
- `R_SPLIT(task_a, task_b)` - fork these two, fire the registered
  cont when both have values.

---

## File layout

| File | Role |
|---|---|
| `src/aot/task.h` | `AotTask` / `AotResult` types + `aot_enc_ret` + `aot_make_*` constructors |
| `src/aot/halloc.{h,c}` | thread-local heap chunks (Bend2's `tl_hp`/`tl_he`) so cont allocs don't contend on the global atomic |
| `src/aot/cont.c` | continuation cell layout in the thvm heap; `aot_alloc_cont` / `aot_write_slot` / `aot_fire_cont` |
| `src/aot/resolve.c` | `aot_resolve` walks values up the cont tree without C recursion |
| `src/aot/worker.c` | `AotBarrier` + `AotRun` + seed/work-phase pattern; `aot_run_serial` (T=1) + `aot_run_parallel` (T>1) |
| `src/aot/emit.c` | `thvm_aot_emit_program(def_id, name)` - walks a TDef'd body and produces compilable C source |
| `src/aot/build.c` | drives clang to compile the emitted C into a dylib for `TAOTCompile` / `TAOTRun` |
| `src/aot/metal_emit.c`, `src/aot/wnf_metal_shim.h` | Metal-targeted emit for `TAOTRun[..., Method -> "Metal"]` |
| `src/aot/_.c` | bundle that `src/thvm.c` includes |
| `tests/test_aot_emit.c` | unit tests on the emit string |
| `tests/test_aot_build.c` | end-to-end via the `TAOTCompile` path (emit -> clang -> dlopen) |
| `tests/test_aot_e2e.c` | end-to-end: emit -> wrap -> clang -> run -> match expected |
| `tests/test_aot_e2e_bench.c` | AOT-emit vs hand-coded `count` perf compare |
| `tests/test_aot_metal.c`, `tests/test_aot_metal_run.c` | Metal-target emit + run smoke + correctness |
| `tests/test_bend_tree_sum.c` | hand-coded reference program (the canonical fork-friendly shape) |

---

## What the emitter handles

| Pattern | -> emit |
|---|---|
| `TLam[v0, ..., TLam[vN, body]]` at def top | LAM peeling; binds each `vK` to `t->args[K]` |
| `TMatChain[<|...|>, default][arg]` | dispatch by tag/value, with dead-arm pruning when arms destructure CTRs |
| `TNum[v]` in value position | `term_new(0, TAG_NUM, ext, v)` |
| `TVar[bound_name]` in value position | inlined as `t->args[K]` or `term_ctr_at(arg, K)` (no named temp) |
| `TRef[name]` in value position | `term_new(0, TAG_REF, name, 0)` |
| `TCtr[label, ...]` (arity 0/1/2) | `aot_make_ctr0/1/2(label, children)` |
| `App^N(TRef[self], a_0, ..., a_{N-1})` (saturated self-call) | `aot_make_call(aot_make_task(FN_self, t->ret, ...))` |
| `TCtr[label, self_call, self_call]` (sibling-pair) | `aot_alloc_cont(CONT_K, 0, ret)` + `aot_make_split` + a `par_<self>_cont_K` that wraps results in `aot_make_ctr2(label, ...)` |
| `TOp2[op, self_call, self_call]` (sibling-pair) | same SPLIT pattern; cont folds via `lv <op> rv` (constant-folded at emit time) |

Open coverage gaps:

- **Cross-def saturated calls** when one def calls another AOT'd def
  by name (needs a registry analogous to the legacy per-dylib
  `AOT_FNS_DIRECT` table).
- **Sequential per-task descent** akin to Bend2's `eval()` managed-CPS
  stack that doesn't touch the global queue. T>1 still scales
  backwards for tree-sum at d>=16 because every cont alloc bounces a
  cache line.
- **`TUop` / tensor ops**: emitted through the Metal-target path in
  `metal_emit.c`; not yet by the CPU emitter.
- **CSE for repeated subexpressions**: the SPLIT pattern emits the
  same `term_ctr_at(n, 0)` twice (once per sibling task). Worth ~2x
  on count-shaped workloads.

---

## Driving from WL

```mathematica
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

TInit[];

(* count(SUC^N{ZER}) = 2^N *)
TDef["count",
  TLam[n,
    TMatChain[
      <|0 -> TNum[1],
        1 -> TLam[p, TOp2["+",
                          TApp[TRef["count"], p],
                          TApp[TRef["count"], p]]]
      |>,
      TLam[ig, TEra[]]
    ][n]]];

src = TAOTEmit["count"];
```

Output (excerpt):

```c
// auto-generated by thvm_aot_emit_program("count")
// def_id 0, arity 1, 1 cont(s)

#define FN_count  0u
#define CONT_count_0  1u

static AotResult par_count_entry(AotProgram *p, AotTask *t);
static AotResult par_count_cont_0(AotProgram *p, AotTask *t);

static AotResult par_count_cont_0(AotProgram *p, AotTask *t) {
  (void)p;
  /* OP2 fold (op id 0) -- assumes both child results are NUM */
  u32 lv = (u32)term_val(t->args[0]);
  u32 rv = (u32)term_val(t->args[1]);
  return aot_make_value(
      term_new(0, TAG_NUM, term_ext(t->args[0]), lv + rv));
}

static AotResult par_count_entry(AotProgram *p, AotTask *t) {
  (void)p; (void)t;
  Term dv = t->args[0];
  /* dead-arm pruned: chain is CTR-only (some arm destructures) */
  if (term_tag(dv) == TAG_CTR && term_ext(dv) == 0u) {
    return aot_make_value(term_new(0, TAG_NUM, 5, 1u));
  }
  if (term_tag(dv) == TAG_CTR && term_ext(dv) == 1u) {
    u64 dp = aot_alloc_cont(CONT_count_0, 0, t->ret);
    return aot_make_split(
        aot_make_task(FN_count, aot_enc_ret((u32)dp, 0),
                      term_ctr_at(dv, 0), 0, 0, 0),
        aot_make_task(FN_count, aot_enc_ret((u32)dp, 1),
                      term_ctr_at(dv, 0), 0, 0, 0));
  }
  ...
}
```

`TAOTCompile[name]` drives `src/aot/build.c`: it wraps the emit with a
runtime preamble, shells out to clang, and dlopens the resulting
dylib. `TAOTRun[name, input]` dispatches into that dylib. The
host-vs-dylib heap-globals issue is bridged by the runtime-OPS
function-pointer ABI that `build.c` injects into the wrapper.

---

## Compile + run via the test harness

[tests/test_aot_e2e.c](../tests/test_aot_e2e.c) is the canonical
external-process harness. Each test:

1. Builds a def in `BOOK_HEAP` directly (skipping the WL bridge for
   the C-only test path).
2. Calls `thvm_aot_emit_program(def_id, name)` to get the source.
3. Wraps it: prepends `#include "<repo>/src/thvm.c"` for the runtime,
   appends a `main()` that registers the program, builds the input
   Term, calls `aot_run_serial`, prints the result.
4. Invokes `clang -O2 ... /tmp/aot_e2e_<name>.c` to produce a
   standalone binary.
5. `popen`s the binary, parses `result tag=N val=N`, asserts.

```
$ bin/test_aot_e2e
  ok    17/17
```

The standalone-binary route sidesteps the host-vs-dylib heap-globals
issue by keeping all state inside the spawned process; the in-process
`TAOTRun` path takes the OPS-ABI shortcut instead.

---

## History

The earlier per-def-dylib AOT (`aot_register` / spine-driven dispatch
from `wnf`'s TAG_REF case) is gone. See `git log -- src/aot/` for
the iter-by-iter trail.
