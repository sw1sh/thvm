# Changelog

High-level log of meaningful changes. Newest first. Consult `git log`
for the per-commit story.

## Unreleased

### Runtime
- Term packing, flat heap, WNF stack machine, 29 interaction rules.
- Parallel WNF: atomic heap primitives, work-stealing deque,
  persistent worker pthreads (`THVM_THREADS`).
- Multi-context API in progress: `TContextSnapshot`, slice trace,
  multiway and causal graphs.

### Tensors & Autograd
- `TTensor` + `TUOp` graph (const / unary / binary / reduce / movement / Conv2D).
- Autograd via `UOP_GRAD`; chain rule through Conv2D, Softmax,
  Pooling, ReLU, Tanh, Reshape, Flatten, ...
- `TOptim["SGD"]`, `TOptim["Adam"]` as recursive lambda terms.
- Einx-style verbs over TUOp (`wl/THVMLink/Kernel/Einx.wl`).

### Scheduling & Codegen
- Materialize -> kernelize -> linearize pipeline.
- Bufferize / rangeify schedule IR -- unified pass, default on.
- Tile-level UOps + tile codegen for CPU and Metal.
- AOT compiler with C and Metal targets.

### Backends
- CPU interpreter + per-op kernels + UOp-graph JIT.
- Metal: shader compile, MTLDevice dispatch, PSO cache.
- Hand-optimized Metal conv tiling (multi-axis UPCAST + LOCAL).
- Metal autotune (GEMM beam search) in progress.

### Frontend
- Wolfram LibraryLink paclet (`wl/THVMLink/`).
- Heap-graph and IC string-diagram renderers (dark + light themes).
- End-to-end LeNet + Adam on MNIST (Metal); beautiful-MNIST,
  MLP, GPT-2 example pipelines.
- Cross-framework benchmarks vs tinygrad / PyTorch MPS / MLX.
- Python cross-validation bridges (`py/thvm/from_tinygrad*.py`) track
  tinygrad master 365f87a8a (STAGE passthrough, copy-as-store schedules);
  the lowered-kernel bridge validates on CPU by default.

### Equational reasoning (parallel research)
- FindProof builds on modern Linux compilers: the TPTP UEQ reader no longer calls the GNU-only `memmem`, which gcc 14+ / clang 16+ reject as an implicit declaration under thvm's strict feature macros (older compilers truncated its pointer result to `int`).
- Waldmeister-style ATP layer: KBO / LPO, unification, critical pairs,
  rewrite engine, CNF, unfailing completion driver
  (`src/{wald,kbo,lpo,rewrite,unify,cp,cnf,atp}/`).
- CheckProof (ProoVer proof checker, `tools/proof_check.c`): the ProoVer
  2026 specified-rule obligations (skolemize shape / status / witness
  annotation), status-claim semantics (thm / esa / cth), binder-fresh
  parsing, conjecture-as-premise and cth-hypothesis detection; the
  published 2026 set is a DEV metric (+137/138), never a forecast.
- FindProof `--proof` output, judged by strict CheckProof (each step against
  its cited premises only): records print in the problem's own signature
  (quoted atoms, hashed long names and curried wide functors mapped back);
  fresh-symbol definitions declare their symbol, and tgt-folded
  definitions, goals and conjecture units print their raw form plus
  single-rewrite fold steps; the clausal pre-pass prints each forward
  demodulation as its own `demodulation` record citing its unit equation.
