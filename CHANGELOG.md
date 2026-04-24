# Changelog

Human-readable log of meaningful changes. Newest first. Group entries
under `## Unreleased` until we cut a tagged version, then roll into a
dated section.

## Unreleased

### Changed: WL kernel split into per-concern files; shape inference centralised

`wl/THVMLink/Kernel/` now uses one BeginPackage["THVMLink`"] +
Begin["`Private`"] block per file, all sharing the same private
context.  Cross-file references resolve directly without
THVMLink`Private`-qualified calls.

Two new files separate concerns that used to be inlined in the
renderers:
- `Shape.wl` -- shape arithmetic (`broadcastShape`, `dropAxis`,
  `shapeText`), tensor-id shape lookup (`tenShapeOf`), and the
  manual IEEE 754 single-precision decoder (`bitsToReal32`,
  `bitsToInt32`, `scalarTextFromCell`).
- `Uop.wl` -- per-opcode metadata in one place: `uopArity`,
  `uopName`, plus an inferred-output-shape walker (`uopShapeOf`,
  `cellShape`, `uopSrcShape`) that mirrors the rules in
  `src/schedule/materialize.c`.

`Visualization.wl` and `Diagram.wl` now read these helpers
directly, dropping their duplicated tables.  UOP labels gained the
inferred output shape (e.g. "MUL\n@8\n{3}") and CONST keeps both
its heap base and its scalar value.

`THVMLink.wl` no longer hard-codes the load order -- after its
own EndPackage it Get's every other `*.wl` in the Kernel directory
in alphabetical order.  Adding a new sibling file means dropping
it in; no edits to the loader.

### Changed: shape-aware grad_rec drops the MUL(target, CONST(0)) wrapper

`interact_grad` no longer post-wraps the chain-rule output in
`ADD[raw, MUL(target, CONST(0))]` to coax materialize into producing
target-shaped gradients.  Instead, every leaf-level emission inside
`grad_rec` is wrapped in `EXPAND(_, target.shape)`:

- leaf match (`y === target`)        -> `EXPAND(gy, target.shape)`
- independent leaf / NUM             -> `EXPAND(CONST(0), target.shape)`
- `UOP_CONST`                        -> `EXPAND(CONST(0), target.shape)`
- `default`                          -> `EXPAND(CONST(0), target.shape)`

This required minimal materialize + interpret support for `UOP_EXPAND`
(previously a step-14 placeholder): `op_output_shape` now reads the
heap NUM cells for EXPAND's target dims (using the source view's rank
to know how many cells to read -- tinygrad EXPAND preserves rank), and
a new `cpu_op_expand` fans the source buffer out to the larger numel.
Sufficient for the autograd path (scalar -> 1-D); per-axis broadcast
in higher ranks lands with view tracking in step 14.

`tests/test_grad.c` was rewritten to expect the EXPAND wrapping
(replacing the old `unwrap` helper that stripped the dead `MUL` wrapper).
WL `grad.wlt` end-to-end numerics still pass (9/9).

### Added: shape on TEN labels, scalar value on CONST labels

`THeapDiagram`'s leaf labels now carry the data the user actually wants
to see:
- `TEN#<id>` -> reads `TENS[id].view.shape` and shows e.g. `{3}` on a
  third line.
- `CONST` -> decodes the NUM cell's bits via manual IEEE 754 (so a
  CONST(1.0) renders as `CONST\n1.` instead of a mystery `CONST\n@2`).

### Added: tensor-aware THeapDiagram (IC string-diagram path)

Diagram.wl now renders TAG_UOP / TAG_TEN terms via Wolfram`Diagrammatic`-
`Computation`, so `THeapDiagram[term]` produces a proper IC string
diagram for tensor compute graphs (it previously returned an empty
network for anything that wasn't pure IC).

UOP rendering uses an opcode-driven shape/style:
- Plain compute UOPs (ADD/MUL/...) -- apex-down blue triangle, N
  inputs at top (one per `uopArity[opcode]`), 1 output at bottom.
- GRAD -- DUP-shaped (apex-up orange triangle), 1 input at top
  apex (the y branch), 2 outputs at the flat bottom (forward
  passthrough + backward gradient).  `gy` and `target` cells are
  hidden; the target tensor id is surfaced as `#<tid>` in the
  GRAD label.

TEN handles render as cyan apex-down triangles, one leaf per
referencing slot (no DUP needed for multi-reference -- each ref
gets its own `TEN#<id>` triangle).

CONST UOPs (zero-arity) are rendered the same way: per-reference
leaves labeled `CONST@<base>`, so a constant referenced from N
slots draws N triangles instead of forcing a shared agent (which
would require DUPs to fan out).

Reachability filter walks UOPs/TENs forward from the seed term
so post-`TWnf` heaps don't surface their pre-rewrite cells.
`principalCellOf` was tightened to consider only cells inside
reachable agents' slot ranges, so dead heap can't grab a UOP's
output wire.

`run.wls` no longer skips IC diagrams for `grad-` examples; both
pre-reduce (`diagram.png`) and post-WNF (`diagram-wnf.png`) IC
diagrams are now rendered alongside the heap graphs.

New plain-UOP examples (no grad rewrite):
- `wl/Examples/uop-add` -- `TUOpAdd[a, b]`
- `wl/Examples/uop-mul` -- `TUOpMul[a, b]`
- `wl/Examples/uop-mul-add` -- `(a*b)+c`

Existing grad-`*` examples now use lazy `TTensor[{3}]` allocations
instead of `TTensorCreate @ NumericArray[...]`; the visualization
doesn't need real numerics and the lazy form is shorter.

### Added: tensor-aware heap graph + grad- visualization examples

`Visualization.wl` got a major extension to render tensor compute
graphs (it previously only knew about IC tags LAM/APP/SUP/DUP/ERA,
so any `TAG_UOP` / `TAG_TEN` term came out blank).

New vertex-id convention prefixes the kind:
- `a<base>` -- IC compound at args base `<base>`
- `e<loc>`  -- ERA cell at heap loc
- `u<loc>`  -- TAG_UOP at heap loc
- `t<id>`   -- TAG_TEN at tensor id

Per-tag rendering:
- `TAG_TEN` -- cyan square labeled `TEN\n#<id>`
- `TAG_UOP` -- blue rectangle labeled `<OPCODE>\n@<loc>`
- Edge labels follow `src<N>` using a per-opcode `uopComputeArity`
  table (NUM-only cells stay implicit).

Single-vertex default size bumped (0.18 -> 0.45) so identity-only
terms don't render as a pinhead.

Three new `wl/Examples/grad-*` folders, each with `term.wl` plus
pre-reduce (`term.png`) and post-`TWnf` (`term-wnf.png`) heap
renderings:
- `grad-add` -- gradient of `a + b` w.r.t. `a` -> ones_like(a)
- `grad-mul` -- product rule `d(ab)/da` -> b
- `grad-x-times-x` -- `d(a*a)/da` -> 2a

`run.wls` detects `grad-`-prefixed folders, skips the IC string
diagram (tensor graphs aren't IC nets), and renders both the
pre-reduce graph and the post-`TWnf` rewritten graph using the
`TWnf` result as the discovery seed.

### Added: PLAN.md step 13 (partial) -- UOP_GRAD reverse-mode autograd

`UOP_GRAD` is the 18th UOp opcode and a pure rewrite rule (not a
graph node that survives reduction).  Reducing
`UOP_GRAD[y, gy_seed, target]` under `TWnf` recursively applies the
chain rule until no `UOP_GRAD` nodes remain, then wraps the result
in a `target * 0` summand so the broadcast machinery in materialize
projects it onto target's shape.

Step-13 chain-rule coverage: leaf cases (target match, other tensor,
NUM, CONST), `UOP_ADD`, `UOP_MUL` (product rule), `UOP_NEG`, and
`UOP_REDUCE` (SUM only -- MAX needs an indicator one-hot, deferred
to step 14).  Anything else returns `CONST(0)` and warns.

WL surface:
- `TUOpGrad[y, gy, target]` -- explicit cotangent.
- `TGrad[y, target]` -- top-level VJP shortcut with `gy = CONST(1)`.

`materialize_expr` recognises `UOP_GRAD` and reduces it inline before
kernelizing, so `TMaterialize[TGrad[...]]` works without a separate
TWnf pass.

Tests:
- `tests/test_grad.c` (16 checks): structural pin-downs of the
  rewrite output for each handled opcode.
- `wl/THVMLink/Tests/grad.wlt` (9 checks): end-to-end f32 numerics
  including identity, independent leaf, ADD, MUL product rule,
  NEG, REDUCE_SUM broadcast-back, `x*x = 2x`, and `2x + 3 = 2`.

### Removed: `Function[t_TTerm]` UpValue

The `(f_Function)[t_TTerm] -> TApp[TLam[f], t]` IC sugar was a
footgun -- it silently rewrote any pure-function map over TTerms
into a beta-redex (which surfaced as a crash when our numeric
Plus/Times UpValues used `& /@`).  Removed alongside the
`$inTLamBinder` guard that only existed to break the resulting
recursion.  `TTerm[id_Integer][arg]` sugar (forming
TApp[TTerm[id], arg]) stays.

### Added: PLAN.md step 12 -- TTensor + TUOp + materialize + dispatch

End-to-end tensor pipeline.  WL-built UOp graphs reduce naturally
through schedule + kernelize + linearize + interpreter dispatch to
concrete `TAG_TEN` results, all under one `TWnf` call.  See
`docs/tensors.md` and `docs/glossary.md`.

Six commits across the step:

- **tensor foundation** (139af93)
  - Three new term tags: `TAG_TEN` (8) atom for tensor handles,
    `TAG_UOP` (9) heap-backed for graph nodes, `TAG_NUM` (10) atom
    for inline scalars.
  - `TenDesc` side table (`TENS[]`) with refcount, View
    (shape/strides/offset), buffer id, and Backend pointer.
  - CPU `Backend` vtable: alloc/free/incref/decref + buf_read/write,
    parallel `CPU_BUFS[]` table with its own refcount for view
    aliasing.
  - View aliasing (`tensor_view_of`) bumps the buffer refcount so
    reshape/permute can share storage zero-copy in step 14.

- **UOp vocabulary + WL surface** (719ac4a)
  - 18 opcodes covering CONST, six movement ops
    (RESHAPE/PERMUTE/EXPAND/PAD/SHRINK/FLIP), eight elementwise ops
    (ADD/MUL/NEG/RECIP/EXP2/LOG2/SQRT/CMPLT), REDUCE, plus the
    rewrite triggers MATERIALIZE and KERNEL.
  - One `src/uop/<op>.c` per opcode emitting the documented heap
    layout.
  - WL surface in the new `Tensor.wl` sibling: `TTensor`,
    `TUOpAdd/Mul/.../Reduce`, `TUOpMaterialize`, plus inspection
    helpers.

- **TRealize + TTensorCreate + zero-copy NumericArray I/O** (862887e)
  - `TRealize[expr] := TWnf[TUOpMaterialize[expr]]`.
  - `TTensorCreate[data]` shares a `NumericArray`'s buffer on the
    CPU backend (Shared passing mode + per-buffer cleanup
    callback).  PackedArrays / nested Lists lift to NumericArray
    first.
  - `TTensorData` returns a `NumericArray` whose type matches the
    dtype (single memcpy in the f32 fast path; no f32 -> f64
    conversion).
  - CpuBuf gains `owns_data` + `on_release` callback so the same
    slot can hold either malloc'd or borrowed bytes.

- **materialize pipeline** (8ffd333)
  - New `KERNELS[]` side table with linearized `KProgOp` programs;
    the same SSA-over-indices shape tinygrad's PYTHON device
    consumes.
  - `src/schedule/materialize.c` rewrites a UOp graph into a tree
    of `UOP_KERNEL[output_buf, NUM(kid)]` terms; recursively
    materializes children, dedups identical inputs.
  - `TMaterialize` WL helper for inspecting the scheduled DAG
    *before* kernel firing; `TKernelInfo[kid]` returns the
    linearized program as an Association.

- **CPU interpreter + interact_kernel** (3e071bd)
  - Per-op CPU files under `src/backend/cpu/op/` (one per opcode,
    matching the project's file = function name convention).
  - `cpu_interpret` walks `KernelEntry.program[]`, allocates one
    scratch per intermediate, dispatches via switch on opcode.
  - `interact_kernel` recursively fires producer kernels first
    (via the new `TenDesc.producer_kid` field), then invokes
    `Backend.dispatch_kernel` for the current kernel.  Increments
    `ITRS` once per firing, the same way HVM4 counts an OP2-NUM-NUM
    collapse.
  - `wnf` extension: `TAG_UOP/UOP_MATERIALIZE` -> direct rewrite,
    `TAG_UOP/UOP_KERNEL` -> fire, anything else -> WNF.

- **PLAN.md** (9b5a4db)
  - Step 12 marked done.

Numerical UpValues on `TTerm` (Plus / Times / Minus / Power[1/2] /
Less) rewrite ordinary WL arithmetic against tensor-shaped TTerms
into UOp graphs.  Scalars lift to UOP_CONST with the seed tensor's
dtype.

Removed: the `Function[t_TTerm]` UpValue that converted `f[t]` to
`TApp[TLam[f], t]`.  It was dumb, surprised the Plus/Times rewrite
that maps over tensors, and the matching `TLam[$inTLamBinder] guard`
went with it.

End-to-end:
```mathematica
a   = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
b   = TTensor[{4}, {10.0, 20.0, 30.0, 40.0}];
out = TRealize[2.0 * (a + b) + 1.0];
Normal @ TTensorData[out]
(* {23.0, 45.0, 67.0, 89.0} *)
```

### Added: THeapDiagram (Wolfram`DiagrammaticComputation` backend)

- New `wl/THVMLink/Kernel/Diagram.wl` subpackage at context
  `THVMLink`Diagram` exporting `THeapDiagram[term]`, which builds a
  `DiagramNetwork` from the current heap using the
  `Wolfram/DiagrammaticComputation` paclet (assumed installed).
- The subpackage lives in its own context so its `BeginPackage`
  imports can pull in `Wolfram`DiagrammaticComputation` and its
  `Diagram` subcontext without shadowing names in the main
  `THVMLink` context.
- Wire-name strings are unique per heap location: `w<loc>` for
  cells, with `VAR` cells collapsed to their binder's wire and
  `DP0`/`DP1` cells expanded to `dup<base>_dp{0,1}_lab<ext>`.
- `wl/Examples/run.wls` now writes `diagram.png` next to `term.png`
  for each example's input `term.wl` (skipped for reference
  variants like `term-reduced.wl`).

### Added: TTermExpr / TTermTree, TReduce, .hvm refs, restructured examples

- `wl/Examples/` folders no longer carry numeric prefixes; the
  reduced variants merge into their parent (`02-id-app-era` +
  `03-id-app-era-reduced` -> `id-app-era/` with both `term.wl` and
  `term-reduced.wl`).  `13-church-2-applied` becomes its own
  top-level `church-2-applied/` (the lambda lives in `church-2/`).
- `term.wl` is the input term construction.
- `term-reduced.wl` (optional) constructs the *expected* WHNF
  directly -- no `TWnf` / `TReduce` inside it.  The reduction test
  runner compares `TTermExpr[TWnf[term.wl]]` against
  `TTermExpr[term-reduced.wl]`.
- `term.hvm` (optional) carries the HVM4 surface-syntax reference
  with the expected output as a `//` comment line.  Documentation
  only; we have no parser yet.
- `wl/Examples/run.wls` now scans `term*.wl` per folder but only
  renders the input `term.wl` (skips `term-reduced.wl`, which is
  reference data).
- `wl/Examples/test_reductions.wls` is the reduction-comparison test
  driver, wired up as `make wl-examples-test`.
- New WL helpers in the paclet:
  - `TReduce[t]` = `(TWnf[t]; t)` -- reduces in place and returns the
    original root.  Useful as a `THeapGraph` seed when you want to
    visualise the post-reduction state.
  - `TTermExpr[t]` walks the heap from `t` and returns a nested
    expression with tag-name string heads (`"LAM"`, `"APP"`, `"SUP"`,
    `"DUP"`, `"DP0"`, `"DP1"`, `"VAR"`, `"ERA"`).  Cycles produce
    `"Cycle"[loc]` leaves.
  - `TTermTree[t]` = `ExpressionTree[TTermExpr[t]]` for visual
    rendering as a Wolfram `Tree`.
- README catalogue rewritten with the new folder names + an
  "Expected WHNF" column pointing at `term-reduced.wl`.

### Added: dark export + auto-fit labels + sugar

- WL `THeapGraph` accepts trailing `Graph` options via
  `OptionsPattern[]` (per the GUIDE) so callers can override
  `GraphLayout`, `VertexSize`, `PlotRange`, `Background`, etc.
- Vertex labels now render INSIDE each shape via `Inset[Pane[label,
  {pixelW, pixelH}, ImageSizeAction -> "ShrinkToFit"]]`.  Labels
  auto-shrink so the same `LAM @0` text fits cleanly in any vertex
  size.
- `VertexShapeFunction` honours the `size` argument throughout,
  including the ERA stroked Circle, so `VertexSize -> Tiny | Small |
  Large | Scaled[...]` all behave.  Removed the manual
  `singleVertexLoopFn` hack -- the default Wolfram self-loop renderer
  works once the shape sizes are scaled correctly and the plot range
  has room for the loop (single-vertex case explicitly widens
  `PlotRange` and shrinks the vertex).
- Examples export onto a dark `GrayLevel[0.12]` background with
  `Style[..., "DarkScheme"]` so `LightDarkSwitched` picks the
  dark-mode arm (white labels, darker fills, white outlines).
  Generated PNGs now read cleanly on dark READMEs and notebooks.

### Added: TTerm sugar (call as function, lambda literal)

- `TTerm[id_Integer][arg_]` desugars to `TApp[TTerm[id], arg]` so
  users can write `id[era]` instead of `TApp[id, era]`.
- `(var |-> body)[t_TTerm]` desugars to `TApp[TLam[var |-> body], t]`
  via a tagged UpValue on `TTerm`.  Lets you write a literal
  beta-redex without spelling out `TLam` / `TApp`.
- The `Function` UpValue is guarded by `$inTLamBinder` so `TLam`'s
  own internal call `builder[TVarFor[loc]]` does not trigger it
  (which would recurse infinitely).
- Two new VerificationTests cover both forms.

### Added: DUP-LAM + church-numeral examples

- `src/interact/dup_lam.c`: real DUP-LAM rule.  Allocates one
  five-cell block holding the new pair of bound vars (as a SUP
  inside the original binder) and the new pair of body projections
  (as a fresh DUP over the original body).  No body cloning happens
  eagerly -- only when a future projection inspects part of the
  body does it descend lazily.  This is the rule that gives Church
  numerals (and similarly Lamping / optimal-reduction style
  workloads) their non-exponential cloning behaviour.
- `tests/test_dup_lam.c`: two C tests; clone an identity lambda and
  confirm DUP-LAM fires once, then end-to-end apply one of the
  cloned copies to ERA.
- `wl/Examples/10-k-combinator/`, `11-church-1/`, `12-church-2/`,
  `13-church-2-applied/`: four new runnable examples.  The Church 2
  family exercises the DUP machinery; the applied form reduces
  end-to-end and the resulting graph (in `13-...-applied/graph.png`)
  shows the post-firing heap including the cloned lambdas and the
  substituted DUP cell.
- Two new VerificationTests in `wl/THVMLink/Tests/core.wlt`: a
  direct DUP-LAM clone, and Church-2-applied reducing to the
  identity-applied result.
- `docs/interact/dup_lam.md` documents the rule, the C, the cost,
  and why the lazy-cloning shape matters.

### Added: visualization renderer split + theme-aware colors

- `wl/THVMLink/Kernel/Visualization.wl`: extracts the heap-graph
  renderer into a dedicated kernel sibling.  THVMLink.wl now `Get`s
  it after declaring public symbols.
- Theme-aware colors throughout: `LightDarkSwitched[Black, White]`
  for foreground; `Lighter[StandardX, 0.55]` / `Darker[StandardX,
  0.45]` per-tag agent fills (green LAM, blue APP, orange SUP,
  purple DUP); ERA stays as a plain foreground-stroked Circle.
- Vertex labels now render in column form: `TAG\n@<base>` for
  arity-1 agents, `TAG\n@<base>..<base+1>` for arity-2.
- Triangles are real triangles via `Triangle[]` (not trapezoids)
  with apex orientation matching IC convention: LAM/DUP point down,
  APP/SUP point up.
- VertexShapeFunction now respects the size argument so
  `VertexSize -> Tiny | Small | Large | Scaled[...]` actually take
  effect.
- Single-vertex self-loop is drawn explicitly via
  EdgeShapeFunction; the identity lambda's loop is now visible.
- Pink "background" mystery solved: `Dashing[{Small, Small}]` was
  invalid (Small is not a numeric Dashing arg) which silently put
  Wolfram into an error-overlay state.  Replaced with the proper
  `Dashed` directive.
- Context-shadowing fix: switched `wl/Examples/run.wls` and
  `wl/THVMLink/Tests/run.wls` from `Needs["THVMLink`"]` to
  `Get["THVMLink`"]` so user code resolves to package symbols
  rather than auto-created `Global`*` placeholders.
- `wl/GUIDE.md` gains a Dark-mode + Standard colors section and an
  OptionsPattern[] section.

### Added: TTerm atomic wrapper + ensureInit

- TTerm[id_Integer] is the canonical wrapper around a packed Term;
  TLam / TApp / TSup / TDup / TEra / TVarFor return TTerm-wrapped
  values; TTermTag/Ext/Val/Sub accept either a TTerm or a raw
  Integer.  Old TTermInfo is gone (folded into TTerm[id]["info"]);
  TTermNew is no longer in the public API (private packTerm helper).
- TTerm[id]["tag" | "ext" | "val" | "sub" | "tagName" | "raw" |
  "info"] forwards to the bridge.  Format.wl gives TTerm a summary
  box keyed off the structural pattern (QuantumFramework style).
- ensureInit[]: heap-touching ops auto-call TInit if the runtime is
  not initialised yet.  TFree clears the flag.

### Added: wl/Examples/ runnable example database

- New `wl/Examples/` directory: one folder per example term, each
  holding a minimal `term.wl` (no `Needs`, no `TInit`, just the
  expression to construct the term) plus the rendered
  `graph.png` produced by the runner.
- 9 examples covering every interaction we currently fire: identity
  lambda, (id ERA) before / after `TWnf`, (ERA lam) before / after
  `TWnf`, bare `TSup[ERA, ERA]`, DUP-SUP same-label annihilation
  before / after, and nested APPs.
- `wl/Examples/run.wls`: single CLI for both bulk and per-example
  runs.  Loads the paclet, calls `TInit` per example, evaluates the
  `term.wl`, exports the resulting `THeapGraph[term]` as a PNG
  alongside the source.  Supports a positional example id and a
  `--eval` flag to skip the PNG export.
- `wl/Examples/README.md` catalogues every example and documents how
  to add new ones.
- `make wl-examples` (regenerate every PNG) and
  `make wl-examples EXAMPLE=<id>` (just one).
- `docs/heap_graph.md` now embeds two of those PNGs directly from
  `wl/Examples/<id>/graph.png` so the doc and the runnable example
  stay in sync.  The previous one-off `docs/images/` directory is
  removed.
- `wl/GUIDE.md` gains a rule for multi-line `If`: leading space after
  the bracket so the test argument lines up with the branches
  (`If[ cond, then, else]`).

### Added: heap graph rendering (PLAN.md step 10)

- `THeapGraph[]` and `THeapGraph[term]` (or `THeapGraph[{t1, t2,
  ...}]`) render the runtime as an IC string-diagram Wolfram
  `Graph[]`: compound terms (LAM/APP/SUP/DUP) are agent vertices
  keyed by their args base, VAR cells collapse into wires labelled
  `var`, and ERA cells render as small black dots.  Optional seed
  terms add agents that are heapless (held only as WL return values).
- `THeap[]` now returns an atomic `THeap[<|nextLoc, cells, Graph|>]`
  with the rendered graph at the `"Graph"` key (capitalized).
- `TTermInfo[t]` now returns an atomic `TTermInfo[<|...|>]` with the
  same payload shape.
- Both atomic objects expose Association-style indexing via DownValues
  and forward `KeyExistsQ`/`Keys`/`Values`/`Normal` via UpValues so
  callers see the same access shape as before.
- `wl/THVMLink/Kernel/Format.wl` defines the `MakeBoxes` UpValues
  (QuantumFramework-style: structural Q-test guarded by `Unevaluated`,
  `BoxForm`ArrangeSummaryBox` for the visual).  Loaded from
  `THVMLink.wl` after the public symbols are declared.
- `TFreshLabel[]` returns a fresh integer from a monotonic counter
  (reset by `TReset[]`).  `TSup[a, b]` and `TDup[body, k]` now
  auto-label via `TFreshLabel[]`; the existing 3-arg
  `TSup[label, a, b]` / `TDup[label, body, k]` forms remain for
  tests that need explicit label matching.
- `wl/THVMLink/Tests/core.wlt` gains six new VerificationTests:
  fresh-label monotonicity + TReset rewind, auto-label distinctness
  for both SUP and DUP, identity-lambda `THeapGraph` shape, seeded
  vs unseeded graph for `TApp[id, ERA]` and `TDup[TSup[ERA, ERA], k]`.
- `docs/heap_graph.md` is now the permanent reference for the model
  (agent-as-vertex, VAR-as-wire, ERA-as-dot) with six worked
  snapshots, mermaid diagrams, and live `THeapGraph` PNGs for
  examples 2 and 4 (regenerated by `docs/images/generate.wls`).
- `docs/term.md` gains a glossary table pinning down term / cell /
  loc / slot / agent / args base / port / node / wire and links
  forward to `docs/heap_graph.md`.
- `docs/wl.md` documents the `Format.wl` summary-box layer and the
  layout convention.
- `docs/images/generate.wls` produces the PNGs embedded in the doc;
  `docs/images/` is the canonical location for generated diagrams.

`make test`    -> 91 C checks pass.
`make wl-test` -> 23 WL VerificationTests pass.

### Added: architecture docs (PLAN.md steps 8-9)

- `docs/` with a self-contained markdown per piece, indexed by
  `docs/README.md`:
  - `docs/term.md`: bit layout + tag table + worked examples.
  - `docs/heap.md`: bump allocator + the substitution model
    (`heap_subst_var`, `heap_subst_cop`).
  - `docs/wnf.md`: enter/apply state machine, frame protocol, and
    the dispatch table for current interactions.
  - `docs/interact/_.md`: index of active-pair rules + tracking of
    which active pairs are stuck (deferred).
  - `docs/interact/{app_lam,app_era,dup_sup,dup_era}.md`: one page
    per interaction with the sequent rule, the C, a worked example,
    and a cost summary.
  - `docs/wl.md`: WL paclet design (scalar bridge + WL-side
    constructors) and usage.
- `README.md`: top of the file points at `docs/README.md` and the
  layout block now includes `docs/`.
- `AGENTS.md`: workflow step 4 clarifies that the `docs/interact/`
  page is the source of truth when it disagrees with the C file's
  header comment.

### Added: minimal reducer + interactions (PLAN.md steps 5-6)

- `src/wnf/_.c`: real two-phase stack-machine reducer (enter/apply)
  modeled on HVM4's clang/wnf/_.c.  Pushes APP / DP0 / DP1 frames at
  enter, dispatches active-pair interactions at apply, rebuilds stuck
  nodes by writing the reduced head back into the heap cell.
- `src/interact/app_lam.c`: real APP-LAM beta (`(lam x.body) arg`
  substitutes `arg` at the binder loc and continues into `body`).
- `src/interact/app_era.c`: APP-ERA (erased function yields ERA).
- `src/interact/dup_sup.c`: DUP-SUP same-label annihilation.  The
  commuting (different-label) case is left stuck for now; a test will
  drive the implementation when needed.
- `src/interact/dup_era.c`: DUP-ERA (both projections receive ERA via
  `heap_subst_cop`).
- `src/heap/subst_cop.c`: pair-substitution helper used by both
  DUP-style interactions; substitutes one side and returns the other.
- `src/thvm.h`: declares the new `interact_*` and `heap_subst_cop`
  signatures.
- `tests/test_app_lam.c`, `tests/test_era.c`, `tests/test_dup_sup.c`:
  removed the `PENDING(...)` gates and added ITRS-counter assertions
  so each test verifies the specific interaction fires (and only
  fires once).
- `wl/THVMLink/Tests/core.wlt`: three new VerificationTests covering
  APP-LAM, APP-ERA, and same-label DUP-SUP through the LibraryLink
  bridge.
- `Makefile`: moved `SRC :=` definition above the `$(WL_LIB)` rule so
  `make wl` correctly retriggers when any C runtime file changes.

`make test` -> 91 C checks pass.  `make wl-test` -> 14 WL tests pass.

### Added: WL paclet (PLAN.md step 4)

- `wl/THVMLink/` paclet that exposes the C runtime to Wolfram
  Language, with the LibraryLink bridge in `wl/THVMLink/CSource/`,
  the package in `wl/THVMLink/Kernel/THVMLink.wl`, and tests in
  `wl/THVMLink/Tests/`.
- `wl/THVMLink/CSource/thvmlink.c` exports 14 scalar `EXTERN_C
  DLLEXPORT` functions covering lifecycle (init/free/reset), term
  packing/unpacking (`thvm_wl_term_*`), heap access
  (`thvm_wl_heap_pos/alloc/read/set`), the WNF entry point, and the
  interaction counter. Every function is scalar-in / scalar-out (no
  arrays, no opaque handles).
- `wl/THVMLink/Kernel/THVMLink.wl` synthesizes higher-level term
  constructors (`TLam`, `TApp`, `TSup`, `TDup`) from the scalar
  primitives via shared `heapWith` / `heapTerm` helpers, plus the
  inspector `TTermInfo` and the heap snapshot `THeap[]`.
- `wl/THVMLink/Tests/core.wlt` defines 11 `VerificationTest` specs
  covering term packing roundtrip, heap primitives, the four
  high-level constructors, the heap snapshot, and the WNF stub
  passthrough.
- `wl/THVMLink/Tests/run.wls` is the test runner. It loads the
  paclet, invokes `TestReport` on every `*.wlt` file, prints
  `wl tests: N passed, M failed` to stdout, lists each failed test,
  and exits non-zero on any failure.
- `wl/GUIDE.md` records WL style rules: no `Print` (use a local
  `debugPrint` wrapping `WriteString`), no em dashes, no Unicode
  box-drawing characters, no decorative arrows in source.
- `Makefile` gains `make wl` (build the dylib at
  `wl/THVMLink/LibraryResources/$(WL_PLATFORM)/THVMLink.dylib`) and
  `make wl-test` (run `run.wls`). Auto-detects the newest
  `/Applications/Wolfram*.app`; override with
  `WOLFRAM_APP=/Applications/Wolfram\ X.Y.app`.

### Added: scaffold (PLAN.md steps 0-3)

- `AGENTS.md` with conventions (path-is-the-function-name, single TU,
  one-interaction-per-file), build/test instructions, and a code map.
- `.gitignore` covering `bin/`, `*.o`, `*.dylib`, macOS `.DS_Store`,
  the local-only `.claude/` settings dir, and the `TinyHVM` reference
  symlink.
- `Makefile` with `make` (build all), `make test` (build + run tests),
  `make clean`. Tests are independent C programs that include
  `src/thvm.c`.
- `src/thvm.h` declaring the term bit layout (SUB:1 / TAG:7 / EXT:18 /
  VAL:38), the minimal tag set (APP, LAM, VAR, ERA, DP0, DP1, SUP,
  DUP), heap globals, and function signatures for the
  term/heap/wnf/interact modules.
- `src/thvm.c` single-TU hub that `#include`s all `.c` files in build
  order.
- `src/term/{new,tag,ext,val}.c` and `src/term/sub/{get,set}.c`:
  full implementations of term packing/unpacking. Trivial
  bit-twiddling.
- `src/heap/{alloc,read,set,take,subst_var}.c`: flat single-threaded
  bump-allocated heap with substitution helper.
- `src/wnf/_.c`: WNF stack machine **stub** that returns its input
  unchanged. Step 6 will replace this with the real reducer.
- `src/interact/app_lam.c`: APP-LAM beta reduction **stub**. Step 6
  fills it in.
- `tests/test_term.c`: round-trip test for `term_new` and
  `term_tag/ext/val/sub_get`. 73 checks pass.
- `tests/test_heap.c`: alloc-then-read-back, set-then-read-back. 9
  checks pass.
- `tests/test_app_lam.c`, `tests/test_era.c`, `tests/test_dup_sup.c`:
  carry the spec for APP-LAM beta, ERA propagation, and DUP-SUP
  collapse/commute. Bodies are gated by `PENDING(...)` until step 6
  lands `wnf` and the interactions, so they exit 0 today and report
  `pend` in `make test` output.
- `README.md` describing what works today and what is stubbed.
