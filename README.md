# thvm

An interaction-net runtime fused with a tinygrad-style tensor IR.
Forward, autograd, kernel scheduling, and CPU + Metal dispatch
all live as rewrite rules over a single flat heap. Wolfram
Language is the host; C is the runtime.

Clean-room rewrite of [sw1sh/TinyHVM](https://github.com/sw1sh/TinyHVM),
the earlier research project that explored the same HVM + tinygrad
fusion (single-TU C runtime, paclet, Python, Metal, autograd as
interaction rules). thvm rebuilds that design from scratch with a
stricter software-engineering workflow now that the mechanisms are
clear -- small files, path-is-the-function-name, one interaction per
file, tests-first, and per-piece architecture docs.

- Architecture, one piece per page: [docs/README.md](docs/README.md)
- WL-side style rules: [wl/GUIDE.md](wl/GUIDE.md)
- Working notes for contributors: [AGENTS.md](AGENTS.md)
- Roadmap: [PLAN.md](PLAN.md)

## Status

End-to-end working today: `TOptim["Adam"]` training
`NetModel["LeNet"]` on MNIST, dispatched to Metal on Apple
Silicon. Loss converges 2.61 -> 0.025 in 4 Adam steps on a
single sample; predicted class flips from 0 to 4 (correct).

| Layer                                        | Status                  |
| -------------------------------------------- | ----------------------- |
| Term packing + flat heap                     | implemented + tested    |
| WNF stack machine + 29 interactions          | implemented + tested    |
| Tensors / TUOp graph                         | implemented + tested    |
| Materialize + kernelize + linearize          | implemented + tested    |
| Bufferize / rangeify schedule IR (default on)| implemented + tested    |
| CPU backend (interpreter + UOp JIT)          | implemented + tested    |
| Metal backend (kernel dispatch)              | implemented + tested    |
| Tile-level path (CPU + Metal, opt-in)        | implemented + tested    |
| AOT compiler (C + Metal targets)             | implemented + tested    |
| Autograd via UOP_GRAD interaction            | implemented + tested    |
| TGrad through Conv2D / softmax / etc.        | implemented + tested    |
| TOptim["Adam"], TOptim["SGD"]                | implemented + tested    |
| Einx-style verbs over TUOp                   | implemented + tested    |
| Multi-context API (TContextSnapshot, slices) | in progress             |
| Metal autotune (GEMM beam search)            | in progress             |
| Lifetime-aware schedule (mem savings)        | open                    |

A separate equational-reasoning / ATP research layer
(Waldmeister-style unfailing Knuth-Bendix completion built on the
same interaction-net heap) lives under `src/wald/`, `src/kbo/`,
`src/rewrite/`, `src/unify/`, `src/cp/`, `src/cnf/`, `src/atp/`;
see [docs/plans/waldmeister_ic_atp.md](docs/plans/waldmeister_ic_atp.md).

`make test` builds and runs every C test under `tests/`;
`make wl-test` runs every `.wlt` under `wl/THVMLink/Tests/`
plus the example-helper tests under `wl/Examples/_lib/Tests/`.

## Build

```bash
make            # compile every C test under tests/
make test       # compile + run; passing tests print ok
make wl         # build the Wolfram LibraryLink dylib
make wl-test    # build the dylib and run every wl/THVMLink/Tests/*.wlt
make clean      # remove bin/
```

Requires a C11 compiler (`clang` by default) and Wolfram 13+.
On macOS the Metal backend is built automatically; pass
`DEV=metal` at runtime to dispatch through it.

The WL paclet auto-detects the newest `/Applications/Wolfram*.app`;
override with `make WOLFRAM_APP="/Applications/Wolfram 14.0.app" wl`.

## Quick start

```wolfram
PacletDirectoryLoad["wl/THVMLink"];
Needs["THVMLink`"];

TInit[];
a = TTensorCreate @ NumericArray[{{1., 2.}, {3., 4.}}, "Real32"];
b = TTensorCreate @ NumericArray[{{5., 6.}, {7., 8.}}, "Real32"];
TTensorData @ TRealize @ TUOpAdd[a, b]
(* NumericArray[{{6., 8.}, {10., 12.}}, "Real32"] *)

(* autograd through the chain rule *)
g = TGrad[TUOpReduce[TUOpMul[a, a], 0, "SUM"], a]
TTensorData @ TRealize @ g
(* NumericArray[{{2., 4.}, {6., 8.}}, "Real32"] *)
```

End-to-end LeNet training (Metal):

```bash
DEV=metal wolframscript -f wl/Examples/lenet-mnist/verify.wls
```

## Examples

`wl/Examples/` ships runnable scripts for both core IC patterns
and full training pipelines:

- Training pipelines:
  - `lenet-mnist/`     - LeCun LeNet inference + Adam training step
  - `beautiful-mnist/` - tinygrad-style 32->64 conv arch (forward only)
  - `mlp-mnist/`       - dense MLP forward
- IC primitives: `church-1/`, `dup-sup-annihilate/`, `era-app/`,
  `id-app-era/`, `identity/`, `k-combinator/`, `nested-apps/`,
  `sup-of-eras/`
- Tensor / autograd: `uop-add/`, `uop-mul/`, `uop-mul-add/`,
  `grad-add/`, `grad-mul/`, `grad-x-times-x/`
- `_bench/` - per-step wall-time + memory baselines

`wolframscript -f wl/Examples/run.wls` renders every example.

Cross-framework benchmarks live at the repo root in `bench/`:

- `bench/bench_tinygrad.py` - tinygrad TinyJit beautiful_mnist train step
- `bench/bench_torch.py`    - PyTorch MPS equivalent (eager + torch.compile)
- `bench/bench_mlx.py`      - MLX equivalent

See `bench/README.md` for invocation; results in
`docs/plans/profiling_methodology.md` §4.6.

## Layout

```
src/
  thvm.h          public types, term layout, function decls
  thvm.c          single-TU hub - #includes every other .c
  term/           term packing/unpacking
  heap/           flat allocator + read/set/take + subst + snapshots
  book/           static term store (REF / ALO support)
  alo/            named-definition realize / force chain
  view/           Shape + View descriptors
  dtype/          dtype machinery (f32/i32/...)
  uop/            UOp constructors (CONST / unary / binary / ...)
  tensor/         TenDesc lifecycle + refcount
  schedule/       materialize, kernelize, linearize, bufferize/rangeify
  codegen/        kernel codegen (scalar + tile, CPU C + Metal MSL)
  jit/            CPU UOp-graph JIT
  aot/            ahead-of-time compiler (C + Metal targets)
  interact/       one interaction rule per file (29 rules)
  wnf/            stack-machine reducer + redex enumeration
  eval/           full normal-form sweep
  collapse/       SUP collapser / observer
  backend/cpu/    interpreter + per-op kernels
  backend/metal/  MTLDevice + .metal shader dispatch
  instrument/     trace hooks, multi-trace, profiling
  lam/            lambda-form helpers
  kbo/  lpo/      Knuth-Bendix / lex-path orderings  (ATP)
  unify/  cp/     unification + critical pairs        (ATP)
  rewrite/  cnf/  rewrite engine + CNF                 (ATP)
  wald/  atp/     Waldmeister-style ATP driver         (ATP)
  util/           shared helpers
tests/            one self-contained C test per executable
docs/             architecture pages, glossary, plans
wl/
  GUIDE.md        WL style rules
  THVMLink/       the Wolfram paclet (LibraryLink bridge + WL)
  Examples/       runnable end-to-end examples
```

The path-is-the-function-name rule from
[HVM4](https://github.com/HigherOrderCO/HVM)'s STYLEGUIDE is
enforced everywhere: `src/heap/alloc.c` defines `heap_alloc()`,
`src/wnf/_.c` defines `wnf()`, `src/interact/app_lam.c` defines
`interact_app_lam()`. Single-TU build: `src/thvm.c` `#include`s
every other `.c` in dependency order; each test does the same.

