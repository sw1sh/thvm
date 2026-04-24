# AGENTS.md

Working notes for agents (LLM or human) contributing to **thvm**. Keep this
file *current* — when you change conventions, update here in the same commit.

## What this project is

`thvm` is a from-scratch implementation of an Interaction Calculus runtime
unified with a tinygrad-style tensor IR. The goal is to express deep-learning
computation (forward, autograd, kernel fusion, codegen, dispatch) as a single
interaction-net rewrite system over a flat heap.

It is being built from the ground up alongside a reference research project
located at `./TinyHVM` (symlink). That repo is sloppy by design — it was the
prototype. We mine it for ideas and snippets but do not depend on it.

The roadmap is in [PLAN.md](PLAN.md). Read it before making structural changes.

## Reference repos (read-only siblings)

- [`TinyHVM/`](TinyHVM/) — research prototype. Whole pipeline exists here in
  rough form: heap, interactions, tensor ops, autograd via interactions,
  Metal codegen, Wolfram paclet.
- [`TinyHVM/HVM4/clang/`](TinyHVM/HVM4/clang/) — canonical HVM4 C runtime.
  Term layout, WNF stack machine, and per-interaction file split are all
  copied/adapted from here.
- [`TinyHVM/tinygrad/`](TinyHVM/tinygrad/) — tinygrad source for the UOp /
  pattern-rewrite / kernelization machinery we will mirror in C.

## Conventions

### File layout: path = function name

Inherited from HVM4's STYLEGUIDE.md. The function `foo_bar_baz()` lives at
`src/foo/bar/baz.c`. The directory entry-point is `_.c`:

```
src/term/tag.c            → term_tag()
src/term/new/lam.c        → term_new_lam()
src/heap/alloc.c          → heap_alloc()
src/wnf/_.c               → wnf()
src/interact/app_lam.c    → interact_app_lam()
```

A file may declare helper variants, but the primary function must match the
filename.

### One interaction per file

Each WNF interaction rule (APP-LAM, DUP-SUP, ERA-LAM, etc.) lives in its
own file under `src/interact/`. Documentation for each interaction lives at
the same path under `docs/interact/`. The C file's leading comment block
mirrors the sequent-calculus rule from the doc.

### Single translation unit

`src/thvm.c` is the umbrella — it `#include`s every `.c` file in build
order. We compile one TU. This matches HVM4 and gives the optimizer maximum
inlining freedom. It also keeps the build trivial (one `clang` invocation).

### C style

- C11. `static inline` on small functions (`fn` macro = `static inline`).
- Switch (not if-chains) for tag dispatch.
- No single-line `if`/`while`/function bodies — always braces.
- Align declaration columns when adjacent.
- Trust internal invariants; validate only at boundaries.

### Comments

Default to none. Add a short `WHY` comment only when behavior would surprise
a careful reader. Do **not** restate what the code already says.

## Build and test

```bash
make           # build src/thvm.c + tests/* → bin/
make test      # build then run every bin/test_*
make clean
```

The Makefile is the source of truth. Each test is an independent C program
that `#include`s `src/thvm.c` (single-TU, recompiled per test — fast at this
size).

## Workflow

1. **Read PLAN.md first.** It defines the staged build order.
2. **Tests describe behavior.** Write or update `tests/test_*.c` *before*
   touching implementation when adding a feature.
3. **Commit often, small commits.** Update `CHANGELOG.md` in the same commit
   with a human-readable line under `## Unreleased`. Commit messages are
   short imperative summaries.
4. **Update docs alongside code.** `docs/interact/<name>.md` and the matching
   C file's header comment must agree.
5. **Keep README.md synchronized with the current shippable state.**

## Code map (current)

Skeleton landed in the initial commit:

- `src/thvm.h` — public types, term bit layout, tag constants, function decls
- `src/thvm.c` — single-TU hub
- `src/term/{new,tag,ext,val}.c` + `src/term/sub/{get,set}.c` — term packing
- `src/heap/{alloc,read,set,take,subst_var}.c` — flat heap primitives
- `src/wnf/_.c` — WNF stack machine (**stub**: returns input unchanged)
- `src/interact/app_lam.c` — APP-LAM beta (**stub**)
- `tests/test_term.c` — term packing roundtrip (passes)
- `tests/test_heap.c` — heap alloc/read/set (passes)
- `tests/test_app_lam.c` — APP-LAM beta spec (**fails** until WNF lands)
- `tests/test_era.c` — ERA propagation (**fails** until WNF lands)
- `tests/test_dup_sup.c` — DUP-SUP same/different label (**fails** until WNF)

## What's deliberately not here yet

Tracked in PLAN.md. Do not pre-build for them.

- WL paclet (`wl/`) — step 4
- Tensor ops (`src/tensor/`, `src/ops/`) — step 12+
- Autograd interactions (`src/grad/`) — step 13
- Kernel fusion / codegen / Metal backend — step 14
- Multi-threading — not in the initial roadmap; runtime is single-threaded

## When in doubt

- Match HVM4's style and the existing scaffold.
- Prefer deletion over flag/option proliferation.
- Ask before introducing a new top-level directory.
- If a step in PLAN.md is ambiguous, surface it before guessing.
