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
- A hand-opt UNROLL/UPCAST split of a reduce axis folds into exactly the
  reduce that owns it, in the renderer and the CPU UOp walker.  A mean
  broadcast back into a second sum no longer re-accumulates 9x, and
  walker-run reduce kernels (BatchNorm-train gradients, conv+BN blocks)
  no longer keep only the last unrolled lane.

### Backends
- CPU JIT: the C renderer compiles kernels whose lift keeps an unread input
  slot (conv+BatchNorm) instead of declining them to the slow walker; it
  still declines untyped slots and bodies that read an unclaimed slot
  (`THVM_TRACE_RENDER_DECLINE=1` says why).
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
  tinygrad master 365f87a8a (STAGE passthrough, copy-as-store schedules,
  AFTER writes, COPY as a value identity, BITCAST as a real reinterpret);
  the lowered-kernel bridge validates on CPU by default.
- py frontend follows tinygrad's training switch: the `TRAINING` ContextVar
  and a decorator-capable `Context` replace `Tensor.training`, which now
  raises instead of silently doing nothing.  Muon applies its post-momentum
  weight decay (tinygrad #17709).  New `Tensor.softmin`, `max_shape`,
  `max_numel`.

### Equational reasoning (parallel research)
- FindProof builds on modern Linux compilers: the TPTP UEQ reader no longer calls the GNU-only `memmem`, which gcc 14+ / clang 16+ reject as an implicit declaration under thvm's strict feature macros (older compilers truncated its pointer result to `int`).
- Waldmeister-style ATP layer: KBO / LPO, unification, critical pairs,
  rewrite engine, CNF, unfailing completion driver
  (`src/{wald,kbo,lpo,rewrite,unify,cp,cnf,atp}/`).
- CheckProof (ProoVer proof checker, `tools/proof_check.c`): the ProoVer
  2026 specified-rule obligations (skolemize shape / status / witness
  annotation), status-claim semantics (thm / esa / cth), binder-fresh
  parsing, conjecture-as-premise and cth-hypothesis detection; the
  published 2026 set is a DEV metric (+137/138), never a forecast.  A
  counter-model needs an ORDERED saturation (the unordered pass's naive
  demodulation false-rejected a one-step superposition); a one-rewrite
  route confirms variable-bearing disequation steps (narrowing proofs).
- FindProof `--proof` output, judged by strict CheckProof (each step against
  its cited premises only): records print in the problem's own signature
  (quoted atoms, hashed long names and curried wide functors mapped back);
  fresh-symbol definitions declare their symbol, and tgt-folded
  definitions, goals and conjecture units print their raw form plus
  single-rewrite fold steps; the clausal pre-pass prints each forward
  demodulation as its own `demodulation` record citing its unit equation.
- Vampire Otter-loop simplification set (`--otter`, `src/atp/otter.c`):
  orientable queued critical pairs rewrite as passive rules before
  selection.  With Vampire's symbol-precedence port (`--sym-prec`
  schemes + `--spb-prec` goal band) it forms the `otter-lpo` schedule
  lane, which proves the CASC-J13 ring misses RNG027-5/027-7/027-10/
  028-5/028-7/029-5/029-6/029-7 with verified proofs; the
  `otter-lpo-cmin` variant (Vampire `sp=const_min` plus Prover9's
  clause-weight penalties) proves RNG027-8 and RNG028-8, and the
  Prover9-shaped twin `otter-ladr` (`--sym-prec=ladr`, LADR's symbol
  order) proves RNG130-1.
- Prover9's clause-weight penalties (`--nest-penalty` / `--depth-penalty`)
  now weigh every critical pair: the FT-treated push route, which forms
  almost all of them, scored its pairs without the penalty.  With them on,
  `--max-weight` bounds the penalized weight, as Prover9's `max_weight`
  does.
- Prover9 lex-dependent demodulation (`--lex-dep-demod=N`, LADR
  `lex_dep_demod_lim`): unorientable equations with equal side sizes
  (commutativity, left-commutativity) demodulate in every normalization
  under the run's ordering, the LADR AC-normalization the Prover9 route
  of the ring problems runs on.
- In-step collections under heap pressure no longer thrash: once a
  collection leaves the live set above half the semi-space (the on-heap
  proof trace), the next waits for an eighth of the space, and a live
  set above the abort fraction ends the run as ResourceOut instead of a
  fatal from-space exhaustion.
