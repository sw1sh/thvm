# Changelog

High-level log of meaningful changes. Newest first. Consult `git log`
for the per-commit story.

## Unreleased

### Runtime
- The kernel content hash (Metal pipeline cache and autotune cache key) includes each buffer's input slot: `x*x+y` and `x*y+y` hashed equal, so on Metal one kernel could reuse the other's pipeline. Autotune caches on disk re-tune once.
- The garbage collector refuses to build when a UOp opcode is added without a heap size in `gc_node_size` (a missing size leaves live nodes of that op unevacuated), and the UOP_BUFFER constructor documents its instance cell.
- Fix: the copying collector keeps every UOp node whole.  A live RANGE lost
  its extent and a BUFFER its last dim (both were sized one cell short), and
  BUFFERIZE, END, STACK, GEP, UNROLL, CONTRACT, VCONST, PLACEHOLDER and
  DETACH nodes were never copied, so after a collection a kernel's content
  hash (the Metal PSO / tile-JIT / autotune cache key) and its ranges changed.
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
- `bin/test_kernel_fire_gen` builds again: it passes `kernel_bench_us` the
  early-stop budget that argument gained with the beam-search bounds (0
  disables it).
- `bin/test_uop` reads UOP_REDUCE's multi-axis heap layout
  `[src, kind, n_axes, axis_0, ...]` and checks the reduce accessors.
- `bin/test_render_uop` pins how a pre-stamped legacy KAX_GLOBAL output
  range renders: it joins the flat-grid decode and reads `tid` (no LOCAL
  axis), not `tg`.
- `bin/test_rangeify_unified` pins the default seed's diamond: a shared
  producer whose consumers index it identically stays fused; once one
  consumer reads it flipped, it realizes.
- `bin/test_rangeify_unified` pins what THVM_PCONTIG does to a softmax:
  unset and levels up to 2 leave the realize map byte-identical, flash mode
  (3) keeps both softmax reduces fused and realizes only the exp's reduced
  axis.
- `bin/test_bufferize` checks the kernel-input to bufferize-edge wiring on a
  shared producer that the default seed realizes (one consumer reads it
  flipped); with identical consumers it fuses and no input slot comes from
  it.
- `bin/test_materialize_v2`: a shared subexpression whose parents index it
  identically fuses into the sink (one boundary); read flipped by one
  parent, it becomes a second boundary ahead of the sink.
- `bin/test_materialize_v2`: a same-kind REDUCE chain is one multi-axis
  REDUCE (one boundary); a max-then-sum chain keeps two boundaries in fire
  order.  The old chain reduced axis 0 of an already fully reduced value.
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
- Fix: the linearized renderer keeps index math signed (`int` loop
  variables, signed integer literals, as tinygrad), so a padded window's
  `-1 < idx` guard no longer reads -1 as 4294967295.
- Fix: the linearized renderer spells float literals exactly (hex float on
  C, bit casts on Metal/CUDA, INFINITY/NAN), so a MAX reduce's -inf
  identity compiles (was `-inff`) and 1e-7 no longer renders as 0.
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
- `nn.state.safe_save` / `safe_load` / `safe_load_metadata` port tinygrad's safetensors format: files match tinygrad's byte for byte and load in either direction; other dtypes narrow to float32 / int32 as `torch_load` does. `nn.state.*` and `from thvm.nn.state import ...` now expose the same functions, including `get_parameters`.
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
- FindProof honours TPTP include selections: `include('F', [n1, ...])` splices only the listed formulas (it spliced the whole file, so a proof could use axioms the problem leaves out), `include('F', *)` splices them all, and includes inside an included file expand (up to 8 levels) instead of being dropped, which let a direct run answer Satisfiable on a problem whose nested axioms make it unsatisfiable.  A selection naming a formula the file lacks, a namespace argument or a malformed directive stops the run as an input error (exit 2), as an unresolvable include already did.  `tests/soundness/edge_include_select.p` is satisfiable only with its selection honoured.
- The FindProof and CheckProof source tarballs (`tools/casc/mk_tarball.sh`,
  `tools/proover/mk_tarball.sh`) ship `tools/atp_cli/`: since the CLI was split
  into modules, `tools/thvm_atp_cli.c` includes them, and a package built
  without them stopped at `atp_cli/knobs.c: file not found`.  The standalone
  Makefiles now rebuild when a module changes.
- CheckProof reads the axiom files a problem includes: every `include()` resolves exactly as FindProof resolves it (the problem's directory, `$TPTP`, then the current directory), `include('F', [n1, ...])` loads only the listed formulas, and an include that does not expand leaves the problem partly unread (a leaf citing a formula found nowhere abstains).  FindProof's ring proofs whose problems include `Axioms/RNG003-0.ax` or `RNG005-0.ax` (17 of the 24 J13 RNG proofs) were Unknown only because that include went unread; all 24 now check VerifiedGood.
- Derivation-history export for learned CP selection (`THVM_ATP_HISTORY_EXPORT=<path>`, `src/atp/history.c`): an opt-in, observation-only 64-byte record stream of queue pushes (seq-hash sampled at `THVM_ATP_HISTORY_RATE`, default 1/16), selections with pop outcomes, in-place reweights, deletions, parent deaths and the rule-side trace DAG (entries from proof emission flagged), with packed-byte structural features; its work runs off the work clock, so recorded and unrecorded runs select identically.  `tools/atp_enigma/np0_extract.py` labels a run from the emitted TSTP proof's citations; `tools/atp_enigma/history_net.py` trains a derivation-history CP scorer on the tinygrad Tensor API and runs the phase-0 offline gate.
- Learned CP selection, phase 0b (`tools/atp_enigma/listwise_net.py`): the derivation-history scorer trained with a listwise loss (at each sampled selection step, -log of the softmax mass of the pending proof pairs within the rebuilt queue, a segment log-sum-exp by `scatter_reduce`), with the engine's own signal as input (its rank and priority at the step; the untrained model ranks exactly as the engine), and the pre-registered phase-0b offline gate (median-percentile ratio and top-10 miss-rate ratio, both at most 0.5).  `history_net.py`'s gate takes a per-step scorer.
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
- FindProof answers FNT (first-order non-theorem) problems: when the
  one-element-model gate finds a model of the full clause set, every
  original record is re-evaluated in that model (the clausifier has
  silent-drop paths) and, if all hold, FindProof prints SZS Satisfiable
  (no conjecture) or CounterSatisfiable plus a Vampire-style FiniteModel
  in the problem's own symbol spelling -- previously it gave up.  The gate
  now reads `$true`/`$false` literals at their fixed value (as free
  predicates, `p => $false` with `p` read "satisfiable").
- FOF clausification never drops a clause: a record whose naive
  distribution would emit a clause wider than `FOL_MAX_LITS` (64) goes
  through Tseitin like one past the clause-count bound.  The extractor used
  to skip such a clause silently (the one-element gate read the Theorems
  LCL662+1.010 / LCL668+1.015 as satisfiable), or fail the record outright
  when it was the only clause ("clausification failed" on HWV051+1,
  PRD001+1 and six J13 FNT problems).
- `tools/casc/model_check.py`: an independent finite-model checker (own
  TPTP tokenizer, parser with include selections, evaluator) for
  FindProof's FiniteModel answers -- checks the status claim, symbol
  coverage and every record's truth; shares no code with the prover.
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
- Rule-set journal (`src/atp/rule_journal.c`): every rule birth, content
  stamp (its birth record, or an in-place RHS compose) and death, in slot
  order, plus the rule-set position each orient / simplify record was
  computed at.  Always on and the same with or without `--proof`; nothing
  in the search reads it.  It records the deaths no trace record states:
  joined interreduction victims, backward and E-set subsumption, the Twee
  full interreduction, ground-joinable deletion and the Otter loop's
  retired passive rules.  `THVM_ATP_RJ_CHECK=1` replays it against the
  live rule slots after every step (clean on the 108-pair battery and on
  the OA, DN and raw-descriptor pins).
- `--proof` re-derives each treatment chain over the journal's rule set
  for its record -- the rules alive then, in slot order, with the content
  and orientation they had -- and, inside an interreduction pass, over the
  pass's new-rule slice (the victim reduct and the slice-only RHS
  compose).  TOP052-1 t100893 (wm) and RNG027-8 t115364 (otter-lpo-cmin),
  printed underived and VerifiedBad before, re-derive: the trace-only
  model kept joined victims and retired passives in the window and had
  no slice replay.
- `--proof` fails closed: the proof block is printed only when every
  record in it is derived from what it cites -- an input leaf, a critical
  pair of its two parents, an orient / simplify record that restates its
  parent or is reached from it by the re-derived chain or a verified
  bridge, a goal rewrite whose equation replays -- and some goal reaches
  `$false`.  Otherwise the SZS status stands alone and one stderr line
  names the first underived record.
- The pop path reads `THVM_ATP_LAZY_DBG` and `THVM_ATP_FORCE_DEF_ORIENT`
  once, as do the requeue (`THVM_ATP_ENQ_DEBUG`), the AC unifier's debug
  switch, the WM interleave (`THVM_ATP_WM_ITL_TOP`) and the goal drive:
  an uncached getenv per selection costs work in proportion to the
  environment's size, which the instruction work clock turns into
  moved clock-gated decisions.
- The bench's `THVM_ATP_NO_TRACE_PACK` switch is gone: the on-heap trace
  takes the classic Term routes of critical-pair formation and requeue,
  its own search (COL004-1 narrowq forks at pick 83; COL006-5, COL049-1,
  GRP697-1 too), and nothing needs it since `--proof` stopped arming it.
- No saturation reads a term node wider than 16 arguments any more: the
  unifier and renamer pass such a node through unchanged, and
  clausification builds them (a Skolem term over more than 16
  universals, a Tseitin name over as many free variables).  CheckProof
  leaves those clauses out of its refutations, where a saturation of the
  rest is no countermodel; FindProof's clausal pre-pass leaves them out
  (its equational lanes read them curried).  A $false step from
  satisfiable axioms with an existential under 17 universals was
  VerifiedGood, and FindProof refuted the satisfiable
  tests/soundness/wide_skolem17_pp.p.
- CheckProof reads functors wider than 16 arguments by currying them into
  a binary application symbol, records and problem alike; every proof of
  TOP052-1, whose goal is a 144-argument tuple, was Unknown.  A model of
  the original gives one of the curried form only when it can host the
  partial applications (a satisfiable Horn set always has such a model),
  so a refutation through the encoding confirms a Horn step only (a
  countermodel still refutes any step); the curried heads carry the
  arity, quoted atoms cannot spell the encoding's names, wide predicates
  and wide Skolem symbols abstain, and FindProof's FOF front end still
  gives up on such a functor (its clausal pre-pass would saturate the
  curried non-Horn set).
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
- `--otter-raw` (with `--otter`): the passive-demodulator replay and the
  selection normalize start from each critical pair's raw overlap, kept
  beside its push-time treated queue pair, instead of from the treated
  pair.  Vampire demodulates the generating inference's conclusion
  itself, once, against the simplification set as it grows during the
  batch, and activates the retained clause as it stands; normalizing the
  treated pair again hid the raw overlap from the batch's own passive
  rules.  This is the half of the old `--proof` search (its treatment
  recorder read the raw pair off the trace) that proved RNG029-5/029-7.
- `--ir-run-order`: interreduction checks each instance of an
  unorientable new equation with the run's ordering, as Vampire's
  backward demodulation does, walking leftmost-outermost from the root.
  The FT walk it replaces gates those instances with KBO even in an LPO
  run, so an LPO lane kept rules its own ordering reduces (commutativity
  swaps KBO calls increasing).  The other half of the old `--proof`
  search on the ring lanes: its treatment recorder interreduced through
  the Term-side walk.
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
