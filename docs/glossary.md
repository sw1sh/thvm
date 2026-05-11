# Glossary

Words that appear across [tensors.md](tensors.md), the future
fusion / scheduling / codegen docs, and PLAN.md steps 12–15. Pinned
down here so the rest of the docs can use them without parenthetical
re-definitions every time.

For the IC-side terminology (`term`, `agent`, `port`, `wire`, ...)
see [term.md](term.md).  For the multicomputation reading of the
SUP / DUP / INC machinery (slice, branch cylinder, observer,
foliation, the reduction trace) see
[multicomputation.md](multicomputation.md); the *Multicomputation*
section near the bottom of this file is the short version.

## Tensor and storage

| word | meaning |
|-----|-----|
| **tensor** | Logical n-dimensional array. In our runtime, a `TAG_TEN` term whose `VAL` indexes into `TENS[]` for a `TenDesc`. |
| **TenDesc** | Side-table entry describing one tensor: dtype, refcount, ShapeTracker, backing buffer id, host-side cache pointer, backend pointer. |
| **buffer** | A region of device memory holding raw bytes. Owned by a `Backend`; addressed by `buf_id`. Multiple `TenDesc`s can share one buffer (view aliasing). |
| **buf_id** | Backend-specific opaque handle for a buffer. CPU = index into a malloc table. Metal = `id<MTLBuffer>` slot. |
| **dtype** | Data type tag (`f32`, `i32`, `f16`, ...). Stored in `TenDesc.dtype` and in the `EXT` field of `TAG_TEN`. |
| **shape** | Tuple of dimension sizes. Lives inside `View.shape` (not on `TenDesc` directly). |
| **stride** | Per-axis step (in elements) used to compute a buffer offset from a logical index. `0` = broadcast; negative = flip. |
| **offset** | Starting element offset into the buffer. Lets multiple views slice into the same buffer. |
| **contiguous** | A view whose stride pattern matches the row-major layout of `shape` with no offset. Most kernels are faster on contiguous inputs. |
| **View** | One layer of (`shape`, `strides`, `offset`, optional mask). Movement ops modify a View; if they can't, a new View is pushed onto the ShapeTracker. |
| **ShapeTracker** | Stack of up to `ST_MAX_VIEWS` Views, oldest at the bottom. Indexing walks the stack innermost-first to derive a buffer offset. |
| **broadcast** | Simulating a larger shape by reusing values along axes that have stride `0`. Cheap; no copy. |
| **mask** | Per-axis `[begin, end)` validity range; positions outside read as 0. Used by `pad`, `shrink`, conv-backward edge handling. |
| **refcount** | `TenDesc.refcount`. Bumped by DUP rules on `TAG_TEN`, decremented by ERA. Backend buffers also have their own refcount tracked via `Backend.buf_incref` / `buf_decref`. |

## UOps and the lazy graph

| word | meaning |
|-----|-----|
| **UOp** | A single node in a computational graph. In our runtime, a `TAG_UOP` term whose `EXT` carries the opcode and `VAL` points to its operand cells in the heap. |
| **opcode** | The integer in a UOp's `EXT` field that picks which kind it is (`UOP_ADD`, `UOP_RESHAPE`, `UOP_KERNEL`, ...). |
| **arity** | Number of source operands a UOp consumes. Different per opcode; recorded in a `uop_arity[]` table for the graph walker. |
| **leaf** | A UOp with no compute sources of its own — `UOP_CONST`, or a `TAG_TEN` reference. The bottom of an AST. |
| **AST** | The compute subgraph rooted at a UOp; specifically, the subtree wrapped by a `UOP_KERNEL` and walked by the dispatcher. |
| **lazy** | The default state of a UOp graph: built but not executed. Stays lazy until a `UOP_REALIZE` is reduced. |

## The compile-and-execute pipeline

The full lifecycle of a UOp, in the order a single `TWnf[realize]`
call walks through it. Each row is one verb you'll see in commits,
file paths, and other docs.

| stage | what it does | where it lives |
|-----|-----|-----|
| **build** | User constructs a UOp graph through WL surface (`TUOpAdd`, `TUOpReshape`, ...). No reduction yet. | wl/THVMLink/Kernel/Tensor.wl |
| **materialize** | Rewriting a raw UOp graph into a scheduled DAG of `UOP_KERNEL` nodes. Runs schedule + kernelize + compile in one rewrite. **Fires no kernels.** Reachable two ways: as a rule on `UOP_MATERIALIZE` under `TWnf`, or directly via the `TMaterialize` WL helper (which calls the rewrite without invoking `wnf`). | src/schedule/materialize.c |
| **schedule** | Decide which UOps become kernels and in what order. v1 = trivial (one kernel per materialize). Part of materialize. | src/schedule/schedule.c |
| **kernelize** | Rewrite the raw UOp graph into `UOP_KERNEL[output_buf, ast_root]` (and later `UOP_ASSIGN` / `UOP_SINK`) nodes. Part of materialize. | src/schedule/kernelize.c |
| **fusion** | Decide which compute ops run in one kernel without intermediate buffers. v1 = elementwise chains until a shape-changing op; step 14 = full producer-consumer fusion. Part of **schedule**. | src/schedule/schedule.c |
| **memory planning** | Assign physical buffers / reusable temporary slots to a kernel's intermediates. v1 = each `KProgOp` gets its own temp (no reuse); step 14 rewrites `program[]` with reused slots. | src/schedule/plan.c (step 14) |
| **linearization** | Flatten a kernel's AST into an SSA-over-indices list (`KernelEntry.program[]`). Each entry is `{opcode, dtype, src_indices, arg}` referencing earlier positions. Same representation tinygrad pickles to the PYTHON device. | src/schedule/linearize.c |
| **lowering** | Take a linearized kernel and turn it into a backend-specific IR (e.g. SSA over indices with register assignment). v1 = skipped (interpreter reads `program[]` directly). | src/lower/ (step 14) |
| **codegen** | Emit source code (C, Metal Shading Language) from the lowered IR. v1 = skipped; the CPU backend *is* the interpreter, analogous to tinygrad's PYTHON device. | src/codegen/ (step 14) |
| **render** | Synonym for codegen when the output is a string of source. (Tinygrad uses "render" specifically for source emission.) | src/codegen/ (step 14) |
| **compile** | Turn a linearized kernel into something `dispatch_fn` can invoke. v1 auto path = nop (store `program[]` + `dispatch_fn = cpu_interpret`). v1 custom path (`TCompileKernel`) = `cc -shared` + `dlsym`. Step 14 adds the codegen auto path. | src/schedule/compile.c |
| **kernel cache** | Content-addressed store keyed by the structural signature of a kernel AST (or by user-provided id for custom kernels). Compile once, reuse forever. | src/schedule/kernel_cache.c (step 14) |
| **dispatch** | Backend invocation of a compiled kernel with concrete input + output buffers. Triggered by `interact_kernel` once all of a `UOP_KERNEL`'s AST leaves are `TAG_TEN`. | backend->dispatch_kernel |
| **launch** | Synonym for dispatch when emphasising the GPU side. |  |
| **firing** | A `UOP_KERNEL` whose AST leaves are all `TAG_TEN` runs `interact_kernel`, which calls dispatch and returns its output `TAG_TEN`. The IC-level "interaction" event. Happens naturally under `TWnf`. | src/interact/uop_kernel.c |

The v1 (step 12) implementation does **build → realize → schedule
(trivial) → kernelize (one kernel per realize) → dispatch
(interpreter)**. Everything in between (fusion, memory planning,
linearization, lowering, codegen, render, compile, kernel cache)
arrives in step 14, by which point the upstream stages will have
been made richer to feed them.

## Backend and device

| word | meaning |
|-----|-----|
| **Backend** | A vtable of buffer + dispatch hooks. CPU and Metal each have one. Tensors carry a `Backend *`. |
| **device** | The hardware target a backend talks to: a CPU, a GPU, an accelerator. Sometimes used loosely as a synonym for Backend. |
| **device id** | Disambiguates multiple devices of the same kind (`cpu:0`, `metal:0`, `metal:1`). Encoded in `UOP_DEVICE`. |
| **dispatch_kernel** | Backend hook: given a compiled kernel id and a tuple of input buffer ids + an output buffer id, run the kernel. |
| **MPS** | Metal Performance Shaders — Apple's pre-baked kernel library. The Metal backend can route some ops (matmul, conv) through MPS instead of custom MSL. |

## Autograd

| word | meaning |
|-----|-----|
| **forward** | The original UOp graph the user built. |
| **backward** | The graph that computes gradients with respect to leaves marked `requires_grad`. We build this lazily by rewriting `UOP_GRAD` rather than constructing a separate IR. |
| **VJP** | Vector-Jacobian product: reverse-mode gradient of a function at a point, against a cotangent vector. What `UOP_GRAD` computes. |
| **JVP** | Jacobian-vector product: forward-mode gradient. Tinygrad's `UOP_GRAD_FWD` / TinyHVM's equivalent; not in our scope yet. |
| **tape** | An eager autograd implementation records every forward op into a list (the tape) and replays it backwards. *We don't have one*; `UOP_GRAD` is a rewrite rule, not a tape. |
| **GRAD_PIN** | A handle that keeps a forward-pass tensor alive long enough to be used by backward, even if the forward path otherwise erased it. TinyHVM's term for the same thing. |

## Multi-output and writes

| word | meaning |
|-----|-----|
| **output buffer** | The destination tensor a kernel writes into. Held in `Heap[loc]` of a `UOP_KERNEL`. |
| **input buffer** | A tensor a kernel reads from. Discovered by walking the kernel's AST for `TAG_TEN` (or in step 14, `UOP_BUFFER` / `UOP_LOAD`) leaves. |
| **assign** | Pin a kernel's result to a specific buffer. `UOP_ASSIGN[target, src_kernel]`. |
| **sink** | A root that aggregates multiple `UOP_ASSIGN` nodes for graphs that produce more than one output. `UOP_SINK[a0, a1, ...]`. |
| **in-place** | A kernel whose output buffer is an existing live tensor (rather than a fresh one). Common for optimizer steps and accumulation. |
| **epoch** | A monotonic counter on a buffer, bumped on every write. Lets the kernel cache invalidate stale results without recomputing the kernel signature. (TinyHVM concept; we don't need it in step 12.) |

## Symbolic shapes (deferred, step 14+)

| word | meaning |
|-----|-----|
| **symbolic shape** | A shape whose dimensions can be variables (e.g. `batch_size`) instead of constants. Lets one compiled kernel serve a range of shapes. |
| **bind** | Substitute a symbolic variable with a concrete value at dispatch time. |
| **simplify** | Constant-fold + range-bound a symbolic expression so kernel-cache lookups don't see equivalent forms as distinct. |

## Kernel cache vocabulary (step 14+)

| word | meaning |
|-----|-----|
| **structural signature** | Hash of a kernel's AST shape, dtypes, and reduction axes — independent of the specific buffers it operates on. Two kernels with the same signature share a compiled binary. |
| **CAM** | Content-addressed memory; here, the kernel cache keyed by structural signature. |
| **JIT** | Just-in-time compilation. The first dispatch of a kernel signature compiles it; subsequent dispatches reuse the binary. |

## Equational reasoning and the IC-as-ATP layer

> The word "superposition" means *two completely different things* on the
> two sides of this project.  HVM-SUP is a runtime data primitive (a
> labelled non-deterministic value); ATP-superposition is a logical
> inference rule.  The IC-native ATP plan
> ([docs/plans/waldmeister_ic_atp.md](plans/waldmeister_ic_atp.md))
> uses the former to represent the search-space of the latter -- but
> they live at different layers and reading the codebase requires
> keeping them apart.

| word | meaning | which layer |
|---|---|---|
| **HVM-SUP** | `&L{a, b}`: a labeled superposition node in the interaction net.  A *value* that "is both `a` and `b` at once," carrying a label `L`.  Reduces via DUP-SUP rules.  Lives in the heap as `TAG_SUP` (`src/thvm.h:62`). | runtime / IC |
| **DUP-SUP** | The interaction rule that fires when a duplicator meets a superposition.  Same label = annihilate (project both sides); different labels = commute (push the SUP up through a fresh DUP pair).  See [docs/interact/](interact/) and [src/interact/dup_sup.c](../src/interact/dup_sup.c). | runtime / IC |
| **collapse** | Enumerate all branches of an HVM-SUP-tree to a flat list of normal forms.  In thvm: `thvm_collapse` ([src/collapse/_.c](../src/collapse/_.c)).  ERA branches are dropped (failed candidates disappear).  *Not* related to wave-function collapse. | runtime / IC |
| **label (SUP/DUP)** | The 18-bit `EXT` field that pairs a SUP with the DUP that is allowed to annihilate it.  Same label = pairwise (correlated) choice; different labels = full cross product across collapse. | runtime / IC |
| **substitution** | Apply a variable assignment `σ` to a term `t`, producing `σ(t)`.  In thvm, lazy via the SUB bit on heap cells; in ATP, eager and explicit. | both |
| **cosubstitution** (Wolfram) | The dual of substitution: instead of specializing the rule's pattern variables to fit the expression, specialize the *expression's* pattern variables to fit the rule.  See [Wolfram, "Beyond Substitution"](https://www.wolframscience.com/metamathematics/beyond-substitution-cosubstitution-and-bisubstitution/). | logic / ATP |
| **bisubstitution** (Wolfram) | Substitution + cosubstitution mixed: both the rule pattern and the expression pattern can specialize.  Wolfram identifies this as the same operation that ATP literature calls **paramodulation**.  This *is* the basic equational inference step. | logic / ATP |
| **unification** | Find a most-general substitution `σ` such that `σ(s) = σ(t)`.  Underlies bisubstitution / paramodulation / CP generation.  In Waldmeister: [sources/INF/Unifikation1.c](../waldmeister/sources/INF/Unifikation1.c). | logic / ATP |
| **matching** | One-way unification: find `σ` such that `σ(pattern) = term`, with no constraints on `term`'s variables.  Used inside rewriting (find applicable rule) and subsumption. | logic / ATP |
| **paramodulation** | Inference rule: given equation `s = t` and a clause `C[s']` where `s` and `s'` are unifiable with mgu `σ`, derive `σ(C[t])`.  Replaces a term *up to unification* rather than only by literal match.  Wolfram: this *is* bisubstitution. | logic / ATP |
| **superposition (ATP)** | Refined paramodulation, central to modern ATPs (Vampire, E, Twee).  Fires only at *non-variable* positions and respects an ordering on terms.  Stronger redundancy criteria.  *Has nothing to do with HVM-SUP except sharing a name.* | logic / ATP |
| **critical pair (CP)** | Two rules `l1 -> r1` and `l2 -> r2` whose LHSs unify on a non-variable subterm.  Their CP is the pair of distinct rewrites of the unified overlap; they should join to the same normal form for the system to be confluent.  Generated by [waldmeister/sources/INF/Grundzusammenfuehrung.c](../waldmeister/sources/INF/Grundzusammenfuehrung.c). | logic / ATP |
| **Knuth-Bendix completion** | Algorithm that takes a set of equations and tries to produce a confluent terminating rewrite system by iterating: select CP -> normalize -> orient -> add to ruleset.  Semi-decision procedure for the word problem. | logic / ATP |
| **unfailing completion** | KB variant (Bachmair-Dershowitz-Plaisted) that handles unorientable equations by treating them as both-directional.  What Waldmeister implements. | logic / ATP |
| **reduction ordering** | A well-founded ordering on terms, total on ground terms, that decides which side of an equation becomes the LHS of a rule.  KBO (Knuth-Bendix) and LPO (Lexicographic Path) are the standard choices. | logic / ATP |
| **joinability** | Two terms `s, t` are joinable under a rewrite system iff they reduce to a common term.  In a confluent system, joinability ≡ provable equality. | logic / ATP |
| **saturation** | A clause/rule set is saturated under an inference calculus iff every conclusion of every applicable inference is already in the set (or redundant).  Saturation = proof if `false` is derived, refutation otherwise. | logic / ATP |
| **subsumption** | One clause/rule is subsumed by another iff the second is more general.  Subsumed clauses are deleted as redundant. | logic / ATP |
| **PCL** | Proof-Construction Language: Schulz/Waldmeister's stepwise proof object format.  One entry per rewrite, citing parent steps + applied substitution. | logic / ATP |

### How the layers connect (in the IC-native ATP plan)

The plan ([docs/plans/waldmeister_ic_atp.md](plans/waldmeister_ic_atp.md))
proposes encoding the ATP search-space inside HVM-SUPs:

- **The ruleset becomes one HVM-SUP**: `R = &L{r0, &L{r1, &L{r2, ...}}}`.
  Applying `R` to a term via APP-SUP commutes the SUP up, fanning out
  into per-rule rewrite candidates.  Failed candidates collapse to ERA.
- **CP generation becomes a superposed cross product**:
  `&pos{positions} x &rule{rules}` - distinct labels give the full
  combinatorial space.  A `unify` function applied to the superposed
  pair produces a SUP of substitutions; failed unifications become ERA.
- **Selection priority** uses `TAG_INC` wrappers on cost-weighted
  candidates; priority-aware collapse enumerates cheapest-first.

In short: HVM-SUP is the *search-space data structure*; ATP-superposition
is the *one inference step* we're trying to enumerate over.  The two
share a name purely by historical accident.  See sections 2 and 3 of
the plan memo for the full treatment.

## Multicomputation: slices, observers, and the reduction trace

> The *multiway* face of the SUP / DUP / INC machinery -- the same
> structure the ATP section above calls *entailment cones*.  Full
> treatment in [multicomputation.md](multicomputation.md); this is the
> vocabulary table.  Nothing in the "trace" rows is implemented yet --
> see [the trace plan](plans/multicomputation_trace.md).

| word | meaning | which layer |
|---|---|---|
| **multiway system** (Wolfram) | A rewriting system viewed as the graph of *all* possible derivations: states as nodes, rule applications as edges, with derivations that reach the same state identified.  See [Wolfram, "Multicomputation"](https://writings.stephenwolfram.com/2021/09/multicomputation-a-fourth-paradigm-for-theoretical-science/). | concept |
| **slice** | The multiway state-set at one moment of multiway time.  In thvm: **one IC term that contains SUPs** -- the slice carried in shared (compressed) form.  `collapse` / the CNF readback ([src/cnf/_.c](../src/cnf/_.c)) enumerates the states in a slice. | runtime / IC |
| **branch** | One state of the slice = one leaf of the term's SUP-label tree = one path "left/right at each label". | runtime / IC |
| **branch cylinder** | A *partial* `label -> side` map: picks out a *cone* of branches (all leaves below that point).  The compressed representative of "all branches that share this much ancestry."  A *shared sub-term* lives in the cylinder of all branches below it -- which is why one IC interaction can be many multiway events at once. | runtime / IC |
| **branchial space / branchial graph** | The metric on branches given by how recently they forked -- in thvm, *reified* as the live SUP-label tree plus literal heap-sharing.  Two branches are branchially close iff they still point at the same cells. | concept / IC |
| **term evolution** | A *within-branch* interaction (`APP-LAM`, `OP2-NUM-NUM`, `MAT-CTR`, `USE-VAL`, ...).  A genuine multiway event -- an edge of the states graph. | runtime / IC |
| **slice evolution** | A *re-foliation* interaction (`APP-SUP` / `OP2-SUP` / `MAT-SUP`, the `cnf` lift-first-SUP, every `INC` rule).  The state-set is unchanged; only the SUP boundary inside the term (or the observer's order) moves.  Not a states-graph edge. | runtime / IC |
| **fork** | A step that fans a state out 1 -> 2: a seeded `&L{a,b}`, or `DUP` commuting through a constructor (`DUP-LAM`, `DUP-CTR`, `DUP-NOD`) which manufactures a SUP.  Analogue of a string multiway system's "the rule matched in two places."  (May be sharing plumbing if a same-label `merge` follows.) | runtime / IC |
| **split** | `DUP-SUP` with *different* labels ([dup_sup.md](interact/dup_sup.md), commute case): the branchial cross product -- state count * 2.  Analogue of two disjoint regions of a string each branching independently. | runtime / IC |
| **merge** | `DUP-SUP` with the *same* label ([dup_sup.md](interact/dup_sup.md), annihilate case): two separately-tracked branches reconverge / are identified -- a diamond closes.  Analogue of a string system identifying two derivations that produce equal states (by direct equality, not hashing).  A *label collision* is a *spurious* merge -- a failure mode unique to IC's label-as-branch-identity scheme. | runtime / IC |
| **prune** | `ERA` absorbing a neighbour (`APP-ERA`, `DUP-ERA`, ... and ERA propagation): a dead-end branch dropped from the slice.  In `#SAT`, the unsatisfying assignment. | runtime / IC |
| **observer** | Whatever sequentialises a slice into a definite *stream* of states.  In thvm: the priority collapser, [src/collapse/ordered.c](../src/collapse/ordered.c).  A bounded computation; it cannot present a slice "all at once," it walks the branches in some order. | runtime / IC |
| **foliation / reference frame** | A choice of how to slice multiway time -- here, the order the collapser walks the branches in, set by the `INC` (`TAG_INC`) decoration: descending into a SUP raises the priority key, an `INC` wrapper lowers it.  Different `INC` schemes = different observers; by confluence they emit the same *set* of states, only the *order* differs. | runtime / IC |
| **causal invariance** | Wolfram's term for "any two derivations produce *isomorphic causal graphs*" -- a *strictly stronger* property than confluence (which only requires the final state to agree).  Confluence is about endpoint states; causal invariance is about the entire event structure (same events, same partial order, only interleaved differently in clock time).  Most WPP-relevant string-rewriting systems satisfy it only asymptotically; **interaction nets satisfy it strictly, by construction** -- the per-active-pair rule discipline makes every event determined by the net and every causal dependency determined by wire-provenance, neither of which depends on the schedule.  This is what makes both "the multiway states graph of a trace" a well-defined quotient *and* "the causal graph of a trace" a program invariant. | concept / IC |
| **reduction trace** | The recorded sequence of interactions of one run, the thing the multiway / branchial / causal / token-event graphs are projections of.  Proposed encoding: a **branch-cylinder-tagged token-event graph** -- see [the trace plan](plans/multicomputation_trace.md). | proposed |
| **token-event graph** (Wolfram) | Tokens (persistent atoms) as one kind of node, events (rule firings, consuming/producing tokens) as the other.  An interaction net *is* one: wires = tokens, interactions = events.  The states / branchial / causal graphs are quotients of it. | concept / IC |
| **multiway / branchial / causal / token-event view** | The four `Graph[]` objects derivable from a trace: states + `fork`/`merge`/`term` edges; the SUP-label tree at one antichain; events + token-provenance edges; the raw token-event graph.  Proposed WL surface `TMultiwayGraph` / `TBranchialGraph` / `TCausalGraph` / `TTokenEventGraph` -- see [plans/multicomputation_trace.md, §7](plans/multicomputation_trace.md#7-wl-surface). | proposed |

For why this matters (debugging SUP-heavy code: spurious merges =
label collisions, runaway splits = unintended branch dimensions; making
the observer a first-class object; the contrast with the Wolfram
Physics Project -- free confluence, branchial space as literal sharing,
`INC` as foliation) see [multicomputation.md, §5 and §6](multicomputation.md); the implementation sketch lives in [plans/multicomputation_trace.md](plans/multicomputation_trace.md).

## Visualization

| word | meaning |
|-----|-----|
| **THeapGraph** | The full heap-graph rendering ([heap_graph.md](heap_graph.md)). Shows every cell. |
| **THeapDiagram** | The IC string-diagram rendering ([diagrams.md](diagrams.md)). Shows agents and wires only. |
| **AST overlay** | (Future) A rendering mode that highlights the AST inside a `UOP_KERNEL` as a sub-region. |
