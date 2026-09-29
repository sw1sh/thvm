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
- Fix (Linux/gcc): a ~2000-deep elementwise chain overflowed the 8 MB stack
  in the unified rewrite (gcc hoisted a 1 KB substitution and the inlined
  bufferize helpers' arrays into every recursion level, 6 KB each); those
  now live out of line.  Headroom on the Linux pod: 4000 deep.
- Split opts are validated like tinygrad's apply_opt (#18112/#18332/#18346):
  split targets, REDUCE membership for UNROLL/GROUP, no group inside another
  reduce; invalid opts from py, WL or autotune are skipped, never mis-applied.
- UOp graph rewrites no longer stop silently at depth 256: past a structural
  depth of 8192, 1024 successive rule hits on one node, or a rewrite cycle,
  the pass reports the rule chain and fails (tinygrad uop/ops.py:1804, :1815).
  reduce_collapse no longer folds a value that uses the reduce axis deep in
  the graph (wrong numbers), and the expander finds UNROLLs at any depth.
- Fix: the linearizer keeps run counts exact (58 bits, was clipped at 2^24),
  so on a 2^24-element output a nested reduce's init no longer lands inside
  its own loop and resets the accumulator every iteration.
- Linearized reduce: the accumulator init runs inside the reduce input's
  live loops (tinygrad #18372), with the AFTER value emit it needs; PTX
  closes a serial loop at its END, not at the last use of its axis.
- Devectorize seeds the reduce accumulator counter above existing
  PLACEHOLDER ids (tinygrad #18373) and keys accumulator declarations by Term.
- IWHERE closure fold before the gate merges (tinygrad #18268), plus the
  else-branch (a|b) merge.
- Generic c0*x < c1 fold (tinygrad #17290/#17291) for every sign pair with
  |c0|>1 (was only two of the four).
- uop_given_valid follows tinygrad #18478: one simultaneous substitution
  over every valid bound replaces the per-clause pass, plus the simplex
  candidate behind try_simplex.
- Fix (THVM_VALID_FOLD=1): uop_given_valid's lo-offset inverse substituted
  the fake back as e instead of e+(-lo), and leaked its fake RANGE; now exact
  (tinygrad #18478).  New bin/test_uop_valid_fold covers the flag-on path.
- Fix: a reduce over an elementwise chain 63+ nodes deep summed wrong on
  the CPU path.  rangeify's "does the reduce body still use its axis"
  walk gave up at depth 64 and answered no, so the broadcast repair
  rewrote SUM(body) into body*extent.  That walk and its two siblings are
  now exact (memoized, no depth budget).
- CONST CAST/BITCAST folds keep the value under the f32-payload rule
  (tinygrad #18203/#18204): bf16/f16/fp8 consts truncate to the dtype, a
  float CAST(CONST) rebuilds the const at the destination (CAST(f32 1.0,
  bf16) used to realize as 0), BITCAST(CONST) reads the storage bits at
  the destination width, and PTX float immediates follow render_val.
- Materialize -> kernelize -> linearize pipeline.
- Bufferize / rangeify schedule IR -- unified pass, default on.
- Tile-level UOps + tile codegen for CPU and Metal.
- AOT compiler with C and Metal targets.
- A hand-opt UNROLL/UPCAST split of a reduce axis folds into exactly the
  reduce that owns it, in the renderer and the CPU UOp walker.  A mean
  broadcast back into a second sum no longer re-accumulates 9x, and
  walker-run reduce kernels (BatchNorm-train gradients, conv+BN blocks)
  no longer keep only the last unrolled lane.
- A store of a REDUCE of a REDUCE of another kind renders through the
  generic nested-reduce path.  The dedicated chain template read each
  link's axis count as its axis id, so a link over two axes (max over j of
  a sum over k,l) looped one of them outside the reduce and stored the
  last iteration.

### Backends
- Linux CPU BLAS: native Linux builds route matmul / matvec / dot kernels
  through OpenBLAS or BLIS (cblas) when installed, as macOS does through
  Accelerate; THVM_CBLAS=0 opts out.  Without it those kernels run as
  JIT-compiled loops (a 1024x80->64->1 MLP forward: 2.9 ms vs 46 us).
- CPU JIT compiles with any C compiler: THVM_CPU_CC, else $CC, else the first
  of clang / cc / gcc (clang-only flags only for clang; the float4 lane type
  is a GCC-compatible vector_size vector).  A failed compile warns once
  instead of silently running every kernel in the interpreter.
- CPU JIT: the C renderer compiles kernels whose lift keeps an unread input
  slot (conv+BatchNorm) instead of declining them to the slow walker; it
  still declines untyped slots and bodies that read an unclaimed slot
  (`THVM_TRACE_RENDER_DECLINE=1` says why).
- CPU interpreter + per-op kernels + UOp-graph JIT.
- Metal: shader compile, MTLDevice dispatch, PSO cache.
- Hand-optimized Metal conv tiling (multi-axis UPCAST + LOCAL).
- Metal autotune (GEMM beam search) in progress.

### Frontend
- py: `test_tiny.py` resynced to tinygrad 365f87a8a (test_eye, clone-based
  gemm/gemv, test_hgemm, test_call); `thvm.helpers` exports `OSX`/`WIN`.
- py: `Tensor.var`, `var_mean`, `std_mean`; `Tensor.std` takes `correction`
  with tinygrad's default of 1 (was the population estimator) (#18150).
- py: `Tensor.to(device, force=False)` accepts tinygrad's `force` flag
  (#18327); it stays the identity on thvm's single DEV device.
- py: `Tensor.clone()` copies the value into fresh storage (tinygrad
  tensor.py:344-351), clones `.grad` and keeps `is_param`; it realizes
  eagerly, so gradients do not flow back through it (tinygrad passes them).
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
- FindProof `--proof` no longer changes the search: a proof run takes the
  no-proof run's steps, rules and critical pairs (same winning lane) and
  recovers the proof beside or after it.  The treatment recorder and the
  unpacked trace are no longer armed (both changed the search: MVA011-1
  lost its 1.4s encompass proof, the narrowing lane ran 5-20x slower
  and lost COL003-1/COL044-1 at 180s); the treatment chain behind each
  printed orient/simplify record is re-derived at emission by replaying
  the engine's normalizers over the historical rule window, with a
  meet-in-the-middle fallback (`thvm_atp_rederive`); the emission is
  bounded to finish inside the wall (a schedule replays a slice only
  once it exits), so a slow re-derivation costs proof quality, never
  the status.  The critical-goal
  queue logs the splices of the engine's own normalizer instead of
  re-normalizing with the proof extractor.  Trace entries past the
  default cap (kept for deep proofs) live in a permanent heap tail the
  collector never copies, run off the clock-gated disciplines' work
  clock, and are discounted by the RSS guard; the proof settings are
  requested in-process so the environment is the no-proof one, and the
  Horn front-end's wall budgets leave the proof capture out.  A
  front-end decision (Horn goal reached, clausification or the clausal
  pre-pass refuting) prints the no-proof status first; the engine then
  only rebuilds the proof (30s at most), and nothing after the decision
  gives up (MGT080+1 and MGT077+1 printed no status or GaveUp).
- Vampire Otter-loop simplification set (`--otter`, `src/atp/otter.c`):
  orientable queued critical pairs rewrite as passive rules before
  selection.  With Vampire's symbol-precedence port (`--sym-prec`
  schemes + `--spb-prec` goal band) it forms the `otter-lpo` schedule
  lane, which proves the CASC-J13 ring misses RNG027-5/027-7/027-10/
  028-5/028-7 inside 180s on a 16-vCPU pod; the `otter-lpo-cmin` variant
  (Vampire `sp=const_min` plus Prover9's clause-weight penalties) proves
  RNG027-8, and the Prover9-shaped twin `otter-ladr` (`--sym-prec=ladr`,
  LADR's symbol order) proves RNG130-1.  RNG028-8 and RNG029-5/029-6/
  029-7 were proved only by the search `--proof` used to change; with
  proof runs taking the no-proof search they are open again.
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
