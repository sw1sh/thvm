# Changelog

Human-readable log of meaningful changes. Newest first. Group entries
under `## Unreleased` until we cut a tagged version, then roll into a
dated section.

## Unreleased

### Added
- `AGENTS.md` with conventions (path-is-the-function-name, single TU,
  one-interaction-per-file), build/test instructions, and a code map.
- `.gitignore` covering `bin/`, `*.o`, `*.dylib`, and macOS `.DS_Store`.
- `Makefile` with `make` (build all), `make test` (build + run tests),
  `make clean`. Tests are independent C programs that include `src/thvm.c`.
- `src/thvm.h` declaring the term bit layout (SUB:1 / TAG:7 / EXT:18 /
  VAL:38), the minimal tag set (APP, LAM, VAR, ERA, DP0, DP1, SUP, SUB),
  heap globals, and function signatures for the term/heap/wnf/interact
  modules.
- `src/thvm.c` single-TU hub that `#include`s all `.c` files in build order.
- `src/term/{new,tag,ext,val}.c` and `src/term/sub/{get,set}.c` —
  full implementations of term packing/unpacking. Trivial bit-twiddling.
- `src/heap/{alloc,read,set,take,subst_var}.c` — flat single-threaded
  bump-allocated heap with substitution helper.
- `src/wnf/_.c` — WNF stack machine **stub** that returns its input
  unchanged. Step 6 will replace this with the real reducer.
- `src/interact/app_lam.c` — APP-LAM beta reduction **stub**. Step 6
  fills it in.
- `tests/test_term.c` — round-trip test for `term_new` ↔
  `term_tag/ext/val/sub_get`.
- `tests/test_heap.c` — alloc-then-read-back, set-then-read-back.
- `tests/test_app_lam.c` — beta reduction spec (`(λx.x) y → y`,
  `(λx.x) (λy.y) → (λy.y)`). **Currently fails** — pending step 6.
- `tests/test_era.c` — APP-ERA, DUP-ERA propagation spec.
  **Currently fails** — pending step 6.
- `tests/test_dup_sup.c` — DUP-SUP same-label collapse and
  different-label commutation spec. **Currently fails** — pending step 6.
- `README.md` describing what works today and what is stubbed.
