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
- A realized fp8 constant fills through the kernel-store rule, as tinygrad realizes a constant in a kernel: an fp8e5m2 +/-inf CONST (broadcast, 0-d, reshaped, or a CAST that folds to one) fills as 0x7b / 0xfb (maxnorm), not 0x7c / 0xfc. The CONST payload and BITCAST / CAST folds keep inf.
- JIT capture: sinking a dispatch into an assign's buffer now checks replayable gathers too, so a realized view of that dispatch's output keeps reading fresh bytes on replay (the sink only runs for Metal tile kernels). Gather endpoints are retained with the capture like other ops' buffers, and the profiling export shows gather rows.
- fp8 conversion follows tinygrad's two rules. Host conversion (CONST payloads, CAST/BITCAST folds, WL writes) is `float_to_fp8`: every e4m3 non-finite is NaN, and e5m2 keeps inf. CPU kernel stores follow the emulated kernels' saturating narrow, so a runtime cast of -inf to fp8e4m3 stays -448. The MAX identity is `dt.min` (-448 for e4m3), as in `identity_element`.
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
- The tensor-core recogniser's UPat tables are spelled as constant expressions, so the single-TU build (FindProof's CASC source package included) compiles with clang 14, which rejects a `const` object's value in a static initializer (C11 6.7.9p4); gcc accepted it as an extension.
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
- The devectorized MAX reduce of an int or bool starts at `dt.min`, as tinygrad's `identity_element` does: -128 / -32768 / -2^31 for signed ints, 0 for unsigned ints, False for bool. It started at the f32 -inf bits, which the linearized C renders as -8388608 for an int32 accumulator and which truncate to 0 in an int8 or int16 one. An i64 MAX reduce stays unlowered, since -2^63 does not fit the 32-bit CONST payload, so the linearized renderers decline it.
- `test_fp8_nonfinite` finds a lowered MAX reduce's init CONST through the whole `reduce_ranges_to_acc` chain (the result LOAD, the END body, the update STORE's buffer). It had stopped at the result LOAD, so the fp8e4m3 identity check failed and the fp8e5m2 / f32 checks passed by reading heap[0].
- The BEAM isolated bench builds a scratch's fill from one 34-element period of its pattern, doubled by memcpy, and on CPU writes a ~1 MB block of whole periods across the buffer instead of staging the whole buffer on the host: a 256 MB scratch fills in 15 ms instead of 214 ms, without a second 256 MB on the host. The bytes are unchanged.
- `THVM_AUTOTUNE_TRACE` names every opt sequence the BEAM search rejects (`[seq] ... NAN-REJECT` or `APPLY-FAIL`), the ops of a longer one joined by `+`. It named only one-op sequences, so a pair the depth-2 beam step rejected left no line.
- The BEAM isolated bench also holds the whole buffer for an input bound as a contiguous view at offset 0, not only for a chain-composed one: loads of another view of the buffer land on the first slot sharing it either way. In `x[0] + x[1]` the `x[0]` slot (128 of 256 elements) was read past its scratch, as were `(x[0] * x[1]).sum()` and `x[:32].reshape(4, 8) - x[1:].reshape(4, 8)`.
- The BEAM isolated bench holds a chain-composed input's whole buffer in its scratch, as tinygrad benches each PARAM on its whole buffer (it held the extent of the slot's own view). A kernel that reads two views of one buffer (`x[:, :64] + x[:, 64:]`, `x[:, :-1] - x[:, 1:]`, a GLU split) loads both through the first view's slot, so BEAM=2 read past that slot's scratch. Backends report a buffer's logical size (`Backend.buf_nbytes` on CPU, CUDA and Metal) and the search declines where it is unknown; `THVM_AUTOTUNE_TRACE` prints each scratch (`[iso]`).
- `test_kernel_fire_gen` builds again: it still called `kernel_bench_us` with two arguments after 0da146814 added the early-stop budget.
- materialize runs the three per-kernel bypass diagnostics (residual BUFFERIZE, stranded RANGE, broadcast input) only under the knobs that read them, `THVM_DUMP_LIFT_COVERAGE` and `THVM_DEBUG_BYPASS_LAST`: since every kernel runs the rewritten store they select nothing, and their DAG walks were about 2.5% of kernel emission time (3.6 of 145 ms over 1549 kernels in test_transformer_train).
- The `(x // d) < c` index rewrite handles negative divisors where truncating and floor division agree (x provably <= 0, or d == -1), and both signs decline when `c*d` leaves int32 (it folded `x // 65536 < 65536` to false).
- `fast_idiv` retries with the divisor's powers of two shifted out of both operands when the magic multiply overflows int32, as tinygrad does: `x // 448` over `[0, 2^20)` renders `((x >> 6) * 18725) >> 17` instead of a divide.
- `test_limit_bufs` covers the per-kernel buffer cap on CPU (`THVM_MAX_KERNEL_BUFFERS`, now in docs/ENV_KNOBS.md). The device table cites tinygrad master, which caps only WebGPU, and says why thvm keeps Metal at 31.
- A reduce over `reshape(shrink(x))` that merges a shrunk axis into its neighbour reads the right elements again on the CPU JIT and the UOp walker (`x.shrink(...).reshape(2,6).sum(1)` was off by 1.8, and the walker applied every offset shrink twice). materialize commits the chain-composed input flags with every rewritten store, and the BEAM isolated bench sizes a composed slot's scratch buffer to the extent its view spans in the raw buffer (it was the view numel, and the bench read past it).
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
- docs/dtypes.md, the lane.c and materialize.c comments and dtype_int4.wlt no longer route int4 / uint4 through the deleted `cpu_op_run_via_i8` / `cpu_op_run_via_f32` drivers. They say what runs today: no CPU kernel handles packed nibbles, so an i4 / u4 elementwise or movement op aborts in `bufferize_compute_costs` and a reduce or CAST to f32 returns wrong values.
- A CPU or CUDA freelist pop that finds the slot table full misses before touching the parked donor. It used to take the donor apart and put back only its data and size, so a rollback that revived the donor reported `buf_nbytes` 0 and the BEAM isolated bench declined it.
- `Backend.buf_nbytes` reports a buffer's logical size, the bytes its allocation requested: CPU and CUDA buffers record the request (`req_nbytes`), since a best-fit freelist recycle serves it from a larger parked block; `test_cpu_free_list` pins that.
- A CPU kernel with more than 64 inputs computes instead of returning zeros: the kernel lift, the UOp walker, the C renderer and the chain-fold marks size their input tables per kernel. A CPU dispatch that finds no rung returns -1, counts itself (`cpu_dispatch_no_rung_count`) and prints one line, instead of silently keeping the zero fill.
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
- `test_param_zero_init_aliasing` pins that zero-initialized parameters (`zeros`, `zeros().contiguous()`, `full`, `zeros_like`) own distinct buffers and train independently under Adam; tinygrad 0.11 hash-consed two `zeros().contiguous()` parameters into one buffer.
- `nn.state.safe_save` / `safe_load` / `safe_load_metadata` port tinygrad's safetensors format: files match tinygrad's byte for byte and load in either direction; other dtypes narrow to float32 / int32 as `torch_load` does. `nn.state.*` and `from thvm.nn.state import ...` now expose the same functions, including `get_parameters`.
- The lowered tinygrad bridge (`from_tinygrad_lowered`) compiles its rendered kernels through the CPU JIT's own `cpu_jit_compile_shared` (THVM_CPU_CC, else $CC, else clang / cc / gcc; `-fbracket-depth` only for clang) instead of a hardcoded `clang -O2`, so its cross-checks run on gcc-only Linux hosts.
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
- FOF clause distribution builds each result clause once: joining two already-distributed sides distributed them again, rebuilding one side's AND spine per conjunct of the other (quadratic in the clause count before a clause is emitted).  LCL681+1.015's 1590-clause conjecture exhausted a 2^30-cell term heap in every lane and lost its one-element model (CounterSatisfiable) once the BIO-scale routing distributed it instead of naming it; the same distributions now fit.  The clause sets are unchanged (`tests/test_fol.c` pins them against the old recursion).
- A FOF clause wider than 64 literals names its variable-disjoint components first (an all-negative component by a Horn rule `Cj | dj`, any other by `Cj | ~nj`) and is chunked into pieces of at most 8 literals: PRD001+1's 85-way existential conjecture became a 63-condition rule over 63 unrelated variables, which no Horn lane can join; it is now 85 one-condition rules and a propositional goal.  `THVM_ATP_FOF_STATS` reports `chunked=<n>`.
- CheckProof reads every record of a problem whose formula names carry escaped quotes (`'Zneg\''`, the Coq-exported SEV family): its record and argument scanners toggled on every `'`, so an escaped quote inverted the quote parity, merged the records after it into one and dropped their names without counting them unread.  A leaf citing a dropped name then read as a demonstrated absence, a false VerifiedBad (FindProof's own SEV646+1 proof, once the parser read the SEV formulas).  The scanners skip quoted tokens with escapes honoured (distinct objects too), and a record name is unescaped exactly as a citation of it is read.  Battery cases H34-H35 (H29-H33 are the wide-functor cases).
- A FOF knowledge base at BIO scale (the 27MB BIO001+0 axiom file: ~81k symbols, 363k clauses) clausifies: a signature past the connective labels continues above them with the Skolem/definition families moved up, the term heap is reserved in proportion to the input, and a record is clausified definitionally only when distribution grows super-linearly (a conjunction under one implication distributes into as many clauses as it has conjuncts; naming its every node ran out of definition labels).
- `THVM_ATP_SCHED_ERRDIR=<dir>` keeps each schedule slice's stderr in `<dir>/<slice>.err` (the per-lane exit reason of an all-lanes-gave-up run).
- FOF lanes no longer end when their equational stage cannot run: an oversized curried axiom unit drops instead of ending the lane (SEV606+1: every lane gave up at 106s on "curried term exceeds side buffer"), and the selection fallback keeps the first `PR_MAX_EQNS` hypothesis units of a conjecture that clausifies to more (LCL646+1.010 at 106s on "axiom selection cannot fit").  The clausal pre-pass is deferred until the encoding is built and curried (still ahead of the Horn release), so a give-up before that point -- no negative clause, an unrepresentable goal, no negated conjecture -- hands the pre-pass the whole remaining wall.
- The one-element-model gate (a clause set satisfiable in the one-element interpretation is satisfiable; no lane may refute it) is sized by the input and decides with unit propagation over occurrence lists (every occurrence counted even past a conflict, so backtracking restores exact counters): it used to stop at 4096 predicates / 65536 literals and report "not shown satisfiable" past them -- skipping the soundness gate exactly on the large problems -- and its per-node rescan of every clause was quadratic.  Same verdicts wherever the old search decided.  Its satisfying assignment is the one the one-element model answer (FNT) claims and re-checks against every original record; `$true`/`$false` literals keep their fixed value and a bare-variable or connective atom still means no verdict.
- The FOF loader scans records in one pass (a per-record `strstr` for both `fof(` and `cnf(` rescanned the rest of the text: 96s on the 27MB BIO knowledge base before its first parse error) and grows its clause arrays with the input (no 65536-record / 65536-clause bound; the one-element claim's record table grows too); past the equational unit table's bound an axiom-side unit drops instead of ending the lane, since the clausal pre-pass keeps the whole set.
- TPTP preprocessing blanks a `%` comment that starts mid-line and a `/* */` block (outside `'atoms'` and `"distinct objects"`, backslash escapes honoured): the record scanner read them as formula text.  Quoted atoms are renamed through one hash table, linear in the input (a 512-name cap left the rest quoted and its per-name `strstr` over the whole text was quadratic): a quoted lower_word is the plain atom, and distinct objects and non-identifier numbers get fresh constants of their own families.  The one-element model claim reads their original spelling back from the rename table and still refuses a distinct object or number (`tests/soundness/model/trap_signed_number.p`); `sat_quoted_word.p` is answered with `'p'` read as `p`.
- FindProof honours TPTP include selections: `include('F', [n1, ...])` splices only the listed formulas (it spliced the whole file, so a proof could use axioms the problem leaves out), `include('F', *)` splices them all, and includes inside an included file expand (up to 8 levels) instead of being dropped, which let a direct run answer Satisfiable on a problem whose nested axioms make it unsatisfiable.  A selection naming a formula the file lacks, a namespace argument or a malformed directive stops the run as an input error (exit 2), as an unresolvable include already did.  `tests/soundness/edge_include_select.p` is satisfiable only with its selection honoured.
- The FindProof and CheckProof source tarballs (`tools/casc/mk_tarball.sh`,
  `tools/proover/mk_tarball.sh`) ship `tools/atp_cli/`: since the CLI was split
  into modules, `tools/thvm_atp_cli.c` includes them, and a package built
  without them stopped at `atp_cli/knobs.c: file not found`.  The standalone
  Makefiles now rebuild when a module changes.
- CheckProof reads the axiom files a problem includes: every `include()` resolves exactly as FindProof resolves it (the problem's directory, `$TPTP`, then the current directory), `include('F', [n1, ...])` loads only the listed formulas, and an include that does not expand leaves the problem partly unread (a leaf citing a formula found nowhere abstains).  FindProof's ring proofs whose problems include `Axioms/RNG003-0.ax` or `RNG005-0.ax` (17 of the 24 J13 RNG proofs) were Unknown only because that include went unread; all 24 now check VerifiedGood.
- Derivation-history export for learned CP selection (`THVM_ATP_HISTORY_EXPORT=<path>`, `src/atp/history.c`): an opt-in, observation-only 64-byte record stream of queue pushes (seq-hash sampled at `THVM_ATP_HISTORY_RATE`, default 1/16), selections with pop outcomes, in-place reweights, deletions, parent deaths and the rule-side trace DAG (entries from proof emission flagged), with packed-byte structural features; its work runs off the work clock, so recorded and unrecorded runs select identically.  `tools/atp_enigma/np0_extract.py` labels a run from the emitted TSTP proof's citations; `tools/atp_enigma/history_net.py` trains a derivation-history CP scorer on the tinygrad Tensor API and runs the phase-0 offline gate.
- Learned CP selection, phase 0b (`tools/atp_enigma/listwise_net.py`): the derivation-history scorer trained with a listwise loss (at each sampled selection step, -log of the softmax mass of the pending proof pairs within the rebuilt queue, a segment log-sum-exp by `scatter_reduce`), with the engine's own signal as input (its rank and priority at the step; the untrained model ranks exactly as the engine), and the pre-registered phase-0b offline gate (median-percentile ratio and top-10 miss-rate ratio, both at most 0.5).  `history_net.py`'s gate takes a per-step scorer.
- Learned CP selection, phase 0c: the history export's opt-in `F` record (`THVM_ATP_HISTORY_CPFEAT=1`) carries each pushed CP's 14 engine features (`thvm_atp_cp_features` on the queued pair decoded from the slot, age = its seq), the vector a push-time scorer computes; `tools/atp_enigma/np0_extract.py` keeps them (`cpfeat`), and `tools/atp_enigma/static_net.py` trains a static listwise head over them plus log1p of the push priority (scored once at push, starting as the engine's ranking) and runs the pre-registered phase-0c gate.  FindProof runs without a heap collector, so the record's decoded Terms are never reclaimed: a long export can exhaust the heap.
- The TPTP formula parser FindProof and CheckProof share has no token-count, name-length, arity or (name-keyed) variable cap: tokens are spans of the input (a fixed 65536-token buffer rejected the 150k-token ITP/SEV formulas; names truncated at 191 chars split into two tokens), argument lists and variable tables grow on demand (the Coq-exported SEV family: arity-23 functions, 1300 variables in one record; SYO525: arity-30 predicates; past 16 arguments CheckProof curries the application and FindProof keeps it as one node), quoted atoms honour backslash escapes, and a parse failure names its record and cause.  Free-variable closure covers every variable id (an id past its fixed table stayed free, so a conjecture's variable escaped the negation).
- FOF clausification without silent caps: the Skolemizer keeps its universal scope on one shared stack (a 256-deep copy per binder used to hand back the formula with its quantifiers still inside) and, past `REWRITE_MAX_ARITY` universals, gives a Skolem term only the variables its body mentions; Tseitin definitions carry every free variable of the subformula they name (a 128-variable truncation made the definition a strengthening); a disjunction wider than `FOL_MAX_LITS` is chunked with named remainders instead of vanishing (HWV051+1's 201-literal domain closure, PRD001+1's 85-way conjecture); a Skolem or definition family that runs out of labels abandons the clause set instead of wrapping onto live symbols.
- The term rebuilds the clausal engine and CheckProof share (substitution, renaming apart, variable canonicalisation, KBO hashing, demodulation) handle any arity: past `REWRITE_MAX_ARITY` they used to return the subterm untouched -- a renaming or substitution that silently skipped part of a clause.
- Learned CP selection, phase 0d (`tools/atp_enigma/topk_net.py`): an offline selection-time re-ranker over the engine's top K.  At each weight selection it rebuilds the queue from the history export, takes the engine's K best by its own key, scores them with a listwise head over the engine rank, queue size, priority, the 14 push features and the age in selections (starting as the engine's pick), and runs the pre-registered phase-0d gate on the top-1 hit rate against the engine's pick; exact (unsubsampled) problems only.
- FindProof `--rerank-topk=<K> --rerank-model=<file>` (`src/atp/topk.c`): an opt-in selection-time re-ranker.  At every weight pick it walks the CP heap for the root's K best, scores them with the phase-0d head (weights exported by `tools/atp_enigma/topk_net.py export`; `tools/atp_enigma/models/topk_rerank_R32h32.safetensors`) and extracts the argmax, leaving the other K-1 queued.  The inputs reproduce the offline columns exactly: the 14 CP features against the goal normal form as it stood at the CP's push (a log of goal versions keyed by seq), the age from a log of `cp_seq_next` per pick, and the pairs decoded into heap scratch popped after the scoring.  `topk_net.py parity` and `featparity` check the engine's scores and inputs against the Python head and the queue rebuilt from the same run's history export.  Unarmed, selection is byte-identical.
- Learned CP selection, phase 0e (`tools/atp_enigma/topk_net.py`): DAgger for the top-K re-ranker.  `prep ... rerank:<K>:<model>` rebuilds a trajectory the armed re-ranker generated (a weight pick is any of the engine's top K, at its rank) and replays the generating head on the rebuilt inputs; `train2` fits one head on several trajectories at once with early stopping on their joint validation loss (no epoch cap); `rrcheck` compares the offline pick counts with the engine's own counters, which `np0_harvest.sh` now keeps per run.  The prep stores the top 64, so K 64 heads and ceilings are measurable.
- `tools/atp_enigma/models/topk_rerank_D32h64_dagger1.safetensors`: the phase-0e top-K re-ranker head (`--rerank-topk=32 --rerank-model=<it>`), trained on the engine's and the armed re-ranker's own trajectories.  It picks a proof CP more often than the phase-0d head on both, and online still solves the twee lane's set at 60 s (one more of its misses at 300 s), so it stays opt-in and out of the schedule.
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
- FOF clausification never drops a clause: the extractor chunks a clause
  wider than `FOL_MAX_LITS` (64) with named remainders, on the
  distribution and the Tseitin route alike (see "FOF clausification
  without silent caps").  It used to skip such a clause silently (the
  one-element gate read the Theorems LCL662+1.010 / LCL668+1.015 as
  satisfiable), or fail the record outright when it was the only clause
  ("clausification failed" on HWV051+1, PRD001+1 and six J13 FNT
  problems).
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
  unifier and renamer passed such a node through unchanged (every term
  rebuild handles any arity now; the exclusion stays as a guard), and
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
  arity, quoted atoms cannot spell the encoding's names, and wide
  predicates and wide Skolem symbols abstain.  FindProof's FOF front end
  does not curry at parse time (its clausal pre-pass would saturate the
  curried non-Horn set): it reads such a functor as one node.
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
- LPO compares run on a DAG of their operands (`src/lpo/_.c`): both flat
  encodings are hash-consed into one DAG of distinct subterms, and the
  Waldmeister-port recursion runs over subterm pairs with a lossless memo
  and O(1) per-pair tests, returning the flat recursion's verdict on
  every pair (pairs of at most 32 nodes stay on the flat recursion, where
  hash-consing costs more than it saves).  The position-keyed recursion
  recomputed pairs its direct-mapped memo evicted and rescanned slices at
  every level; over 16.4M compares from RNG otter-lane runs the DAG is 3x
  faster, 4.3x on the 240K hardest, 95x on the worst (576 ms), and
  `otter-ladr --ir-run-order` proves RNG025-4/025-6 instead of stalling
  near step 230.  `-DTHVM_LPO_SELFCHECK` runs both evaluators on every
  compare (0 mismatches) and dumps hard pairs that `bin/test_lpo FILE`
  rechecks.
- Otter interreduction reads each older rule side's symbol mask from a
  cache keyed on the side's Term and FT span (gc-epoch and session
  stamped) instead of rescanning every rule on every pass, and the
  run-order walk skips rules whose pattern head differs from the node's
  before the orientation lookup.  Both leave the pass unchanged
  (clock-free capped otter runs match main step for step); the rescan
  was half of an `otter-lpo-cmin --ir-run-order` run once its rules
  reached thousands of symbols, and RNG029-5 on `otter-lpo` with both
  options proves in 45s instead of 80s (instruction clock).
- The `otter-lpo` schedule lane arms `--otter-raw --ir-run-order`.
  Single-lane runs (instruction clock, 177s) over the 27 J13 ring
  problems and 31 held-out RNG UEQ problems: with both options the lane
  proves RNG029-5/029-6 besides its RNG027-5/7/10, 028-5/7 and 029-7,
  and the same held-out problems.  `otter-lpo-cmin` stays unarmed (with
  `--ir-run-order` its rule set explodes on RNG027-5/028-5 and it keeps 3
  of 8; `--otter-raw` alone trades RNG027-8 for problems `otter-lpo`
  proves), as does `otter-ladr` (no change either way).  CASC-J13
  protocol (16 vCPU, `--schedule --wall=180`, one problem at a time)
  against main: 25 of the 27 J13 ring problems instead of 23 (RNG029-5
  and 029-6, with and without `--proof`, proofs VerifiedGood), the 7
  recent flips and 21 of the 31 held-out problems as before, no loss.
- LPO is an ordering (`src/lpo/_.c`).  When the first differing argument
  pair of two terms with the same head is incomparable, the terms are
  incomparable unless an argument of one reaches the other, as
  Waldmeister (`LPO.c:300-306`) and Vampire 5.0.1 (`Kernel/LPO.cpp`
  cLMA -> cAA) decide it; the flat recursion, the DAG evaluator and the
  Term-tree recursion took the less-than leg there, so `f(x,y)` and
  `f(y,x)` each compared below the other.  Distinct symbols of equal
  precedence are incomparable heads: the flat and DAG paths compared
  their arguments lexicographically (`f(x)` and `g(x)` came out EQ) and
  the constant pretest counted a tied symbol as a greater one.  The
  Term-tree memo returned a cached less-than as 255.  RPO with every
  symbol LEX now agrees with LPO on tied precedences.  `bin/test_lpo`
  checks every evaluator against a textbook LPO (Baader and Nipkow, Def.
  5.4.12) on random terms with variables under strict and tied
  precedences: 0 mismatches over 65871 pairs, and antisymmetry,
  irreflexivity, transitivity (145886 chains), stability under
  substitution, monotonicity and the subterm property hold; the previous
  evaluators fail it (4693 mismatches, 1535 antisymmetry violations).
  The `-DTHVM_LPO_SELFCHECK` probe counts antisymmetry violations of the
  real compares (0 in 1.43M compares from six otter-lane runs; the old
  lex step decided 6.1% of 1.20M).  On Waldmeister's own LPO problem
  `andassoc.pr` (WM preset, the file's precedence) the run proves in
  260201 steps instead of stopping at the memory guard: the WM preset's
  MixWeight gave a lex-incomparable pair one side's weight instead of
  the sum, and correcting that term alone lets the old build prove it
  in 755097 steps.  The LPO schedule lanes, single lane (instruction
  clock, 177s) over every J13 UEQ problem and every non-satisfiable TPTP
  UEQ problem outside J13 where a lane does not exit fast: J13 49 -> 48
  solved by some LPO lane, the 31 held-out ring problems 21 -> 21, the
  other 151 held-out problems 126 -> 127; across five LRS clock scales
  the robust changes are RNG026-6 (`otter-lpo-cmin`, 4-8s instead of
  unsolved) and REL031-1 (`otter-ladr`) gained, REL026-3 and RNG027-9
  (both `otter-lpo-cmin`) lost, the rest wall-edge noise.  CASC-J13
  protocol (16 vCPU, `--schedule --proof --wall=180`, strict
  CheckProof): the 27 J13 ring problems and 7 flips go from 32 to 33
  (RNG027-9 in 46s via `otter-lpo-cmin` in four runs of four,
  VerifiedGood; main gives up in all four; the single-lane runs lose it,
  so the lane's outcome on it depends on the clock), and the 8 problems
  with a changed lane are solved by both, by other lanes.
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
