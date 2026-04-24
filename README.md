# thvm

An interaction-net runtime fused with a tinygrad-style tensor IR. Built
from scratch alongside the [TinyHVM](TinyHVM/) research prototype.

The roadmap lives in [PLAN.md](PLAN.md). Working notes for contributors
(human or LLM) live in [AGENTS.md](AGENTS.md). Every meaningful change is
recorded in [CHANGELOG.md](CHANGELOG.md).

## Status

This is **step 0–3** of the plan: scaffolding and spec tests. Not yet a
working runtime.

| Component                        | Status                          |
| -------------------------------- | ------------------------------- |
| Term bit layout + packing        | implemented + tested            |
| Flat heap (alloc/read/set/take)  | implemented + tested            |
| Variable substitution helper     | implemented                     |
| WNF stack-machine reducer        | **stub** — lands in step 6      |
| `interact_app_lam` (β-reduction) | **stub** — lands in step 6      |
| `interact_dup_sup`               | not yet declared                |
| Wolfram Language paclet (`wl/`)  | step 4                          |
| Tensor / TUOp                    | step 12                         |
| Autograd via interactions        | step 13                         |
| Kernel fusion / codegen / Metal  | step 14                         |

## Build & test

```bash
make            # compile every test under tests/
make test       # compile + run; passing tests print ok, pending print pend
make clean
```

Requires a C11 compiler (`clang` by default). Single translation unit:
`src/thvm.c` `#include`s every other `.c` in dependency order. Each test is
an independent program that itself `#include`s `src/thvm.c`.

Today's `make test` output:

```
→ bin/test_term       ok    73/73
→ bin/test_heap       ok    9/9
→ bin/test_app_lam    pend  APP-LAM — wnf stack machine + interact_app_lam land in step 6
→ bin/test_era        pend  ERA propagation — needs wnf + interact_app_era / interact_dup_era (step 6)
→ bin/test_dup_sup    pend  DUP-SUP — needs wnf + interact_dup_sup (step 6)
```

`pend` tests carry the spec; the assertions inside them run unchanged
once the implementation lands and the `PENDING(...)` line at the top is
removed.

## Layout

```
src/
  thvm.h            public types, term layout, function decls
  thvm.c            single-TU hub
  term/             term packing/unpacking (one fn per file)
  heap/             flat allocator + read/set/take + subst_var
  interact/         one interaction rule per file
  wnf/              WNF stack-machine reducer
tests/
  test.h            tiny CHECK / PENDING harness
  test_*.c          one program per test, self-contained
TinyHVM/            symlink to the research prototype (read-only)
```

The path-is-the-function-name rule from
[HVM4](TinyHVM/HVM4/clang/STYLEGUIDE.md) is enforced everywhere:
`src/heap/alloc.c` defines `heap_alloc()`, `src/wnf/_.c` defines `wnf()`.
