# thvm

An interaction-net runtime fused with a tinygrad-style tensor IR. Built
from scratch alongside the [TinyHVM](TinyHVM/) research prototype.

The roadmap lives in [PLAN.md](PLAN.md). Working notes for contributors
(human or LLM) live in [AGENTS.md](AGENTS.md). Every meaningful change
is recorded in [CHANGELOG.md](CHANGELOG.md). WL-side style rules live
in [wl/GUIDE.md](wl/GUIDE.md).

## Status

Steps **0-4** of the plan are landed: scaffolding, spec tests, and the
Wolfram Language paclet. The runtime itself is still mostly stubs.

| Component                        | Status                          |
| -------------------------------- | ------------------------------- |
| Term bit layout + packing        | implemented + tested            |
| Flat heap (alloc/read/set/take)  | implemented + tested            |
| Variable substitution helper     | implemented                     |
| WNF stack-machine reducer        | **stub**, lands in step 6       |
| `interact_app_lam` (beta)        | **stub**, lands in step 6       |
| `interact_dup_sup`               | not yet declared                |
| WL paclet `wl/THVMLink/`         | implemented + tested            |
| Tensor / TUOp                    | step 12                         |
| Autograd via interactions        | step 13                         |
| Kernel fusion / codegen / Metal  | step 14                         |

## Build & test

```bash
make            # compile every test under tests/
make test       # compile + run; passing tests print ok, pending print pend
make wl         # build the Wolfram LibraryLink dylib
make wl-test    # build the dylib and run the WL VerificationTest suite
make clean      # remove bin/
```

Requires a C11 compiler (`clang` by default). Single translation unit:
`src/thvm.c` `#include`s every other `.c` in dependency order. Each
C test is an independent program that itself `#include`s `src/thvm.c`.

The WL paclet auto-detects the newest `/Applications/Wolfram*.app`;
override with `make WOLFRAM_APP="/Applications/Wolfram 14.0.app" wl`.

Today's output:

```
$ make test
> bin/test_term       ok    73/73
> bin/test_heap       ok    9/9
> bin/test_app_lam    pend  APP-LAM ... land in step 6
> bin/test_era        pend  ERA propagation ... step 6
> bin/test_dup_sup    pend  DUP-SUP ... step 6

$ make wl-test
wl tests: 11 passed, 0 failed
```

`pend` C tests carry the spec assertions; they fire unchanged once the
implementation lands and the `PENDING(...)` line at the top of each
test file is removed.

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
wl/
  GUIDE.md          WL style rules
  THVMLink/         the Wolfram paclet
    PacletInfo.wl
    Kernel/THVMLink.wl       package source
    CSource/thvmlink.c       LibraryLink bridge (single-TU build)
    Tests/core.wlt           VerificationTest specs
    Tests/run.wls            test runner script
    LibraryResources/        compiled dylib lives here (gitignored)
TinyHVM/            symlink to the research prototype (read-only)
```

The path-is-the-function-name rule from
[HVM4](TinyHVM/HVM4/clang/STYLEGUIDE.md) is enforced everywhere under
`src/`: `src/heap/alloc.c` defines `heap_alloc()`, `src/wnf/_.c`
defines `wnf()`.

## Using the WL paclet

```wolfram
PacletDirectoryLoad["/Users/you/src/thvm/wl/THVMLink"];
Needs["THVMLink`"];

TInit[];
id  = TLam[var |-> var];        (* identity lambda *)
app = TApp[id, TEra[]];          (* (id ERA) *)
TTermInfo[app]
(* <| "sub"->0, "tag"->0, "tagName"->"APP", "ext"->0, "val"->2, "raw"->...|> *)
THeap[]
(* snapshot: nextLoc + per-cell decoded info *)
TWnf[app]                        (* stub today, real reducer in step 6 *)
```
