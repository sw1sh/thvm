# thvm

An interaction-net runtime fused with a tinygrad-style tensor IR. Built
from scratch alongside the [TinyHVM](TinyHVM/) research prototype.

- Roadmap: [PLAN.md](PLAN.md)
- Architecture, one piece per page: [docs/README.md](docs/README.md)
- Working notes for contributors: [AGENTS.md](AGENTS.md)
- Changelog: [CHANGELOG.md](CHANGELOG.md)
- WL-side style rules: [wl/GUIDE.md](wl/GUIDE.md)

## Status

Steps **0-10** of the plan are landed: scaffolding, the Wolfram
Language paclet, a minimal but working reducer + interactions, the
architecture docs, and the IC-style heap graph renderer.

| Component                        | Status                          |
| -------------------------------- | ------------------------------- |
| Term bit layout + packing        | implemented + tested            |
| Flat heap (alloc/read/set/take)  | implemented + tested            |
| Variable substitution helper     | implemented                     |
| WNF stack-machine reducer        | implemented + tested            |
| `interact_app_lam` (beta)        | implemented + tested            |
| `interact_app_era`               | implemented + tested            |
| `interact_dup_sup` (same label)  | implemented + tested            |
| `interact_dup_sup` (commute)     | stuck (deferred)                |
| `interact_dup_era`               | implemented + tested            |
| `interact_dup_lam` (clone)       | implemented + tested            |
| WL paclet `wl/THVMLink/`         | implemented + tested            |
| `THeapGraph[]` heap renderer     | implemented + tested            |
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
> bin/test_app_lam    ok    4/4
> bin/test_era        ok    3/3
> bin/test_dup_sup    ok    3/3

$ make wl-test
wl tests: 14 passed, 0 failed
```

## Layout

```
src/
  thvm.h            public types, term layout, function decls
  thvm.c            single-TU hub
  term/             term packing/unpacking (one fn per file)
  heap/             flat allocator + read/set/take + subst_*
  interact/         one interaction rule per file
  wnf/              WNF stack-machine reducer
tests/
  test.h            tiny CHECK / PENDING harness
  test_*.c          one program per test, self-contained
docs/
  README.md         index
  term.md           bit layout + tag table
  heap.md           allocator + substitution model
  wnf.md            enter/apply state machine
  interact/         one page per active-pair rule
  wl.md             paclet design + usage
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

From the repo root:

```wolfram
PacletDirectoryLoad["wl/THVMLink"];
Needs["THVMLink`"];

TInit[];
id  = TLam[var, var];            (* identity lambda *)
app = TApp[id, TEra[]];          (* (id ERA) *)
TTermInfo[app]
(* <| "sub"->0, "tag"->0, "tagName"->"APP", "ext"->0, "val"->2, "raw"->...|> *)
THeap[]
(* snapshot: nextLoc + per-cell decoded info *)
TTagName[TTermTag[TWnf[app]]]    (* "ERA" -- one APP-LAM interaction *)
TItrs[]                          (* 1 *)
```

For a deeper tour read [docs/wl.md](docs/wl.md).
