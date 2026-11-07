# AGENTS.md

Working notes for agents (LLM or human) contributing to **thvm**. Keep this
file current: when you change a convention, update it here in the same commit.

## What this project is

thvm is an Interaction Calculus (IC) runtime in C: one flat heap of 64-bit
terms, reduced by interaction rules. Two tracks run on it:

- **NN**: a tinygrad-style tensor compiler. Tensors are UOp graphs on the heap;
  scheduling (rangeify), codegen and dispatch to the CPU, Metal and CUDA
  backends, autograd as an interaction rule, JIT capture and replay. Front
  ends: the Python package `py/thvm` (tinygrad's API, `import thvm` in place
  of `import tinygrad`) and the Wolfram paclet THVMLink.
- **ATP** (automated theorem proving): an equational prover by unfailing
  Knuth-Bendix completion, faithful to Waldmeister by default, with the CASC
  (CADE ATP System Competition) CLI `bin/FindProof` and the Wolfram
  `TFindProof`.

Architecture docs, one piece per page: [docs/README.md](docs/README.md)
(`docs/core/`, `docs/NN/`, `docs/ATP/`).

## References

- **tinygrad is the spec** for the NN track: port its behavior, and cite the
  tinygrad file and lines a port follows (`codegen/opt/heuristic.py:27-189`)
  in the comment or commit message. Keep a tinygrad checkout next to this repo
  (`../tinygrad`); the importers and parity tests assume tinygrad commit
  `365f87a8a`.
- **HVM4** ([HigherOrderCO/HVM](https://github.com/HigherOrderCO/HVM)) is the
  model for the IC runtime: the term layout, the WNF stack machine, one
  interaction per file, and the path = function name rule.
- Other checkouts this repo reads (reference kernels, Waldmeister, MLX, the
  research prototype) live under `external/`, which is gitignored. The repo
  has no submodules: `.gitmodules` stays empty.

## Conventions

### File layout: path = function name

The function `foo_bar_baz()` lives at `src/foo/bar/baz.c`. The directory
entry point is `_.c`:

```
src/term/tag.c            -> term_tag()
src/heap/alloc.c          -> heap_alloc()
src/wnf/_.c               -> wnf()
src/interact/app_lam.c    -> interact_app_lam()
```

A file may declare helper variants, but the primary function matches the
filename.

### One interaction per file

Each interaction rule (APP-LAM, DUP-SUP, OP2-SUP, ...) lives in its own file
under `src/interact/`. Its page, if it has one, lives at the same path under
`docs/interact/` ([docs/interact/_.md](docs/interact/_.md) is the index). The
C file's leading comment mirrors the sequent rule from the page; if they
disagree, the page wins and the comment is fixed.

### Single translation unit

`src/thvm.c` `#include`s every `.c` file in build order. Every artifact is one
TU over it: each C test, the THVMLink library
(`wl/THVMLink/CSource/thvmlink.c`), the Python library
(`py/csource/thvm_py.c`), `bin/FindProof` (`tools/thvm_atp_cli.c`) and the
standalone prover library `libthvm_atp` (`wl/ATP/CSource/libthvm_atp.c`). Add a
source file by including it from `src/thvm.c`, never by editing the Makefile.

### C style

- C11. `static inline` on small functions (the `fn` macro).
- Switch, not if-chains, for tag dispatch.
- Always braces: no single-line `if` / `while` / function bodies.
- Align declaration columns when adjacent.
- Trust internal invariants; validate only at boundaries.

### Comments and docs

- Code comments: default to none. Add a short comment only where behavior
  would surprise a careful reader; never restate the code.
- Docs and comments: no em dashes; plain ASCII in tables and diagrams;
  repo-relative paths; spell an abbreviation out on first use; no dated
  "as of" status notes.
- Wolfram Language sources follow [wl/GUIDE.md](wl/GUIDE.md); the paclet
  documentation follows
  [wl/THVMLink/docs/DOC_GUIDE.md](wl/THVMLink/docs/DOC_GUIDE.md).

## Build and test

The [Makefile](Makefile) is the source of truth; the ATP track's defines,
tests, CLI and benches are in [atp.mk](atp.mk), which it includes.

```bash
make                  # the THVMLink library (default goal; a full compile check)
make bin/test_foo     # one C test; make all builds every test, make test runs them
make wl-test          # run every wl/THVMLink/Tests/*.wlt against the built library
make py               # py/thvm/libthvm_py.{dylib,so}
make bin/FindProof    # the ATP CLI (the CASC entry)
make atp-lib          # libthvm_atp for the WolframInstitute`ATP` package (wl/ATP)
make wl-cross         # THVMLink libraries for all five Wolfram platforms
make clean            # remove bin/
```

- Run the tests that cover what you changed (`grep -rl <symbol> tests/`, then
  `make bin/test_<name> && bin/test_<name>`) rather than the whole suite.
- WL tests load the library `make wl` built; rebuild it after a runtime change.
- Python tests: `make py`, then `python -m pytest py/tests/test_<name>.py`
  (`py/tests/helpers.py` puts `py/` on the path).
- The Metal backend builds on macOS; CUDA builds on Linux when the toolkit
  headers are found (`CUDA_HOME`). `DEV=cpu|metal|cuda` picks the device at
  runtime.
- `THVM_*` environment knobs are indexed in
  [docs/NN/env_knobs.md](docs/NN/env_knobs.md) and
  [docs/ATP/env_knobs.md](docs/ATP/env_knobs.md); add or remove a row with the
  `getenv` that reads it.

## Workflow

1. **Tests describe behavior.** Write or update the test (`tests/test_*.c`,
   `wl/THVMLink/Tests/*.wlt`, `py/tests/test_*.py`) before changing behavior.
2. **Commit often, small commits.** Describe the change in the commit message:
   a short imperative summary line with a scope prefix (`fix(cpu): ...`,
   `docs: ...`), then what changed and why.
3. **Update docs alongside code.** A changed mechanism updates its page; a new
   piece gets a page under `docs/core/`, `docs/NN/` or `docs/ATP/` and a line
   in [docs/README.md](docs/README.md).
4. **Keep [README.md](README.md) true** to what ships.

## Code map

`src/` (C runtime, one TU through `src/thvm.c`; public types and the term bit
layout in `src/thvm.h`):

| Directory | What it holds |
| --- | --- |
| `term/`, `heap/` | term packing; the flat heap, allocator and Cheney GC |
| `wnf/`, `cnf/`, `collapse/`, `eval/` | the reducers: WNF stack machine, parallel `nf` pool, SUP-lift readback, collapsers |
| `interact/` | one interaction rule per file (31 rules) |
| `book/`, `alo/`, `lam/` | static definitions (`TDef`), their lazy realization, auto-dup of non-linear binders |
| `aot/` | ahead-of-time compiler for `TDef` bodies (C and Metal) |
| `instrument/` | hot counters and the multicomputation trace |
| `uop/`, `view/`, `dtype/`, `tensor/` | UOp constructors and rewrites, views, dtypes, tensor descriptors |
| `schedule/` | realize classification, rangeify, materialize, kernel GC, symbolic dims |
| `codegen/` | the UOp DAG renderer (C, Metal, CUDA), kernel opts, BEAM autotune |
| `backend/{cpu,metal,cuda,dispatch}/` | backends: clang JIT, BLAS and UOp walker; Metal; CUDA; batched dispatch |
| `jit/` | kernel-sequence capture and replay |
| `nn/` | tinygrad's NN-level ops in C behind one flat entry (`nn_call`) |
| `atp/` | the equational prover (completion loop, indexing, selection, proofs) |
| `kbo/`, `lpo/`, `rpo/`, `wpo/` | reduction orderings |
| `unify/`, `cp/`, `rewrite/` | unification, critical pairs, rewriting |
| `fol/`, `cc/`, `ffmep/`, `wmfpa/`, `wald/` | clausal resolution, congruence closure, finite-model enumeration, the Waldmeister normalization port, the `.pr` parser |
| `util/` | concurrency primitives of the parallel reducer |

Elsewhere:

- `py/thvm/`: the Python package (`tensor.py`, `mixin.py`, `nn.py`,
  `optim.py`, ...) over `libthvm_py` (`py/csource/thvm_py.c`); `py/tests/`.
- `wl/ATP/`: the WolframInstitute`ATP` package (completed equational systems
  for v2's FullSimplify) over its own library `libthvm_atp`. Temporary: it
  duplicates THVMLink's `Kernel/ATP/AtpSystem.wl` until v2 ships THVMLink
  whole, then it is deleted; change both copies together until then.
- `wl/THVMLink/`: the paclet: `CSource/` (bridge), `Kernel/` (one package per
  concern, `Kernel/README.md` maps them; `Kernel/NN/`, `Kernel/ATP/`),
  `Tests/`, `docs/` (paclet documentation source). `wl/Examples/`: runnable
  examples; `wl/scripts/`: docs build, lint and release scripts.
- `tests/`: one self-contained C test per executable; the ATP track's tests,
  shell gates, probes, problem data and soundness corpus are under `tests/atp/`.
- `tools/`: the FindProof CLI (`thvm_atp_cli.c`, `atp_cli/`), CASC and ProoVer
  packaging (`casc/`, `proover/`), ATP baselines and the ENIGMA tooling, the
  build helpers (`build/`), the DSV2 converters (`dsv2/`), the FLUX scripts
  (`flux/`) and the WolframKernel process guards (`guards/`).
- `bench/`: every benchmark, thvm against tinygrad, PyTorch and MLX and the
  ATP engine against Twee ([bench/README.md](bench/README.md)).
- `docs/`: architecture pages ([docs/README.md](docs/README.md)).

## When in doubt

- Match the existing scaffold; for the NN track, match tinygrad.
- Prefer deletion over flag or option proliferation.
- Ask before introducing a new top-level directory.
