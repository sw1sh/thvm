# AOT roadmap

Where we are, what's next, and what "all on device" actually requires.

## Current state (5 commits in)

The AOT layer specializes one IC `@def` to a C function that the WNF
machine dispatches through `TAG_REF` instead of the lazy ALO unfold.

```
runtime call site                     specialised C
  TWnf[term]                          aot_emitted_<id>(stack, sp, base)
   |                                    |
   v                                    v
  enter TAG_REF[name]   --AOT_FNS[name]-+
   |                                    |
   |  fallback (no AOT registered)      |  pops APP frames, runs
   v                                    |  case-tree dispatch in
  alo_realize -> ALO chain              |  native C, recurses, returns
                                        |  WHNF Term
```

Three hand-coded reference programs land under `src/aot/programs/`:

| program  | shape                          | speedup vs interp | ITRS-exact? |
|----------|--------------------------------|------------------:|:-----------:|
| fib_nat  | CTR + DUP-CTR + chained recur  | 14×               | yes         |
| gab_tak  | CTR labels T/F + 3-way recur   | mixed (small ✓, deep ✗) | no |
| u32_fib  | NUM + OP2 + tree recur         | 11×               | yes         |

A Phase-1 auto-emitter (`src/aot/emit.c`) walks a TDef'd body and
produces equivalent C source. Today it covers TNum, TLam, TVar, TApp,
TRef, TOp2, and TMatNum chains — enough to auto-emit the u32_fib body
faithfully. WL surface: `TAOTEmit[name]` returns the C source as a
string; users drop it under `src/aot/programs/<name>.c` and rebuild.

## What's missing

Two fronts.

### A. IC pattern coverage (Phase 2)

The emitter doesn't yet handle CTR construction, CTR-MAT destructure,
TDup, tail-recursion → goto loops, or the LAM_ERA_MASK fast-path.
fib_nat and gab_tak still need their hand-coded versions.

### B. The compile / load loop (gating real auto-AOT)

The runtime's helpers (`aot_register`, `aot_pop_app_arg`,
`term_new`, `heap_alloc`, ...) are `static inline` in the single-TU
build. A dlopen'd dylib produced by clang at runtime can't link to
them. Two ways to unblock:

1. **ABI split** — declare the small set of helpers the emitted code
   touches (~15 functions) as `extern` instead of `static inline`.
   Force the linker to emit them as real symbols in the host. ~50
   LOC of header churn, half a day.
2. **Re-include trick** — emitted .c does `#include "thvm.c"` so
   each AOT dylib carries its own private runtime instance. Works
   but the dylib's runtime state (HEAP, ITRS, ...) is *separate
   from* the host's, so the AOT can't actually share heap with the
   interpreter. Wrong semantics; rejected.

Doing (1) gates `TAOTCompile[name]` — the function that runs the
emitter, invokes `clang -shared`, dlopens, calls the registration
helper. Without it, the AOT path requires manual rebuild.

## Beyond IC: dispatching UOp kernels from AOT

Current arrangement: two parallel compile paths.

- **UOp kernel JIT** (`src/backend/cpu/jit.c` + `src/codegen/`):
  a UOp graph at the tensor layer rangeifies into KProgOp ops, which
  render to a fused inner-loop C function `void k(out, ins, n)`.
  Compiled by clang at first fire, dlopened, function pointer cached
  in `KernelEntry.func`.
- **AOT (this work)**: an IC body's `@def` at the term-machine layer
  specialises into a per-def case-tree. Today routes UOp encounters
  through the lazy ALO interpreter (which then fires kernel JIT
  separately).

The end state: the AOT emits **direct calls** to the kernel JIT's
function pointers, eliminating the runtime materialize+dispatch round
trip. When an AOT body sees a UOp subtree:

1. Trigger materialize on the subtree to produce a KernelEntry.
2. Trigger `cpu_jit_build` on the KernelEntry to get a function
   pointer (or fall back to the interpreter if the JIT can't render
   that pattern).
3. Emit C that:
   - Allocates an output buffer of the known dtype/shape.
   - Calls the kernel function with the input buffer ids resolved
     from the bound tensor variables.
   - Wraps the result in a TenDesc + TAG_TEN term.
   - Returns it.

The AOT becomes a thin **orchestrator** that wires together
pre-compiled SIMD kernels with native control flow, instead of going
through wnf for every sub-step.

Concrete blocker: **shape polymorphism**. The kernel JIT today is
shape-specialized — a `void k(out, ins, n)` produced for one shape
isn't reusable on another. AOT bodies can't always know shapes at
emit time (a `@forward` def takes a tensor whose shape depends on
the user's input). Three options:

- **Specialize per call site shape**, recompile on shape change. The
  JIT cache already does this — extend it to be reachable from the
  AOT.
- **Emit shape-polymorphic code** that takes shape arrays. Slower per
  call but fewer compiled artifacts. Doable for elementwise; harder
  for matmul/reduce.
- **Defer to runtime** for shape-dependent subtrees. The AOT punts
  to the existing JIT cache; future calls hit cache.

Option 1 is what HVM4 does for its kernels (one shape per call
site). Probably what we want.

## Beyond CPU: Metal-side AOT

The dream end state: the AOT-emitted code runs on the GPU. Two
sub-questions.

### B1. Metal-side numeric kernels
Already partly here — `src/backend/metal/` dispatches MSL shaders.
The kernel JIT could grow a Metal renderer (`render_metal.c`
exists) so kernels compile via `xcrun metal` to .metallib, dlopen'd
the same way.

### B2. Metal-side control flow

The harder problem. Metal's compute model is "compute kernel that
runs N times in parallel" — there's no native support for IC's
case-tree dispatch, lazy ALO unfolds, or DUP-CTR commutes.

Two options:

- **Stay CPU-side for control flow**, dispatch kernels to GPU. The
  AOT emits CPU C that issues `MTLCommandBuffer.commit()` calls. The
  IC reduction proceeds on CPU; only the SIMD-heavy bits run on GPU.
  This is what most ML frameworks do (PyTorch, etc.). Easiest and
  probably the right answer.
- **Device-side scheduler** — emit MSL that runs an interpreter
  loop on the GPU's command queue. Hard: requires a small VM in MSL,
  has to handle dynamic memory, and has poor branching performance
  on SIMT hardware. Probably not worth it.

So "all on device" really means "all the **compute** on device,
control flow stays on CPU". Achievable; aligns with how production
ML frameworks ship.

## Concrete next steps, in dependency order

1. **AOT Phase 2** — emitter coverage for TCtr + DUP-CTR + tail-loop
   detection. ~400 LOC. Unblocks auto-emit of fib_nat and gab_tak.
2. **AOT ABI split** — refactor the ~15 runtime helpers from
   `static inline` to `extern`. ~50 LOC + header churn. Unblocks…
3. **AOT build pipeline** (`src/aot/build.c`) — write emitted .c to
   disk, invoke clang, dlopen, register. Mirror existing
   `src/backend/cpu/jit.c` pattern. ~250 LOC.
4. **Shared `clang_compile.c`** — extract the dlopen + cache-by-hash
   bits common to UOp JIT and AOT build. ~100 LOC refactor.
5. **AOT calls UOp JIT** — when emitter sees a UOp subtree, emit a
   call to the kernel JIT's function pointer. Includes the
   materialize-at-emit-time orchestration. ~600 LOC.
6. **Metal kernel renderer** — extend `src/codegen/render_metal.c`
   so the JIT can produce `.metallib` dylibs. ~400 LOC.
7. **AOT dispatches GPU kernels** — same hooks as (5) but routes to
   the Metal-built kernel function. Touches lifecycle (command buffer,
   completion handlers). ~300 LOC.

Total: ~2100 LOC of careful work. Maybe 2 weeks focused.

The win: an `@forward` net definition compiled once becomes a single
C function that orchestrates a precompiled sequence of GPU
dispatches with native control flow. Same shape as a TorchScript
deployable, but with IC-level lambda calculus underneath.
