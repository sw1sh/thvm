# THVMLink documentation guide

How to write the THVMLink documentation sources under `docs/` - the
`Template: TechNote` tutorials, the `Template: Symbol` reference pages, and the
`Template: Guide` guide. They are literate-markdown files that
[`build_docs.wls`](../../scripts/build_docs.wls) turns into evaluated Wolfram notebooks via
`MarkdownToNotebook` (MTN). This guide is the house style; it captures rules the
user has called out, and it overrides the upstream
[wolfram-symbol-page skill](https://github.com/sw1sh/MarkdownToNotebook/blob/main/skills/wolfram-symbol-page/SKILL.md)
where they disagree (noted inline). For Wolfram-code style inside cells, follow
[`wl/GUIDE.md`](../../GUIDE.md).

## Build and inspect, always

Every page is a *twin*: the `.md` source and the evaluated `.nb` MTN builds from
it. A page is not done until you have **built it and inspected every output
cell**. Run the build and read back the output cells; each one's value must be
correct (probe it against the live paclet first and paste the real result into
the `<!-- => ... -->` hint - a wrong hint is worse than none). `build_docs.wls`
auto-discovers every `docs/**/*.md`, so a new file builds with no wiring.

## Symbols are always autolinked, never bare backticks

Every symbol - **built-in (`Total`, `Normal`, `Plus`, `Dot`) and paclet
(`TRealize`, `TGrad`)** - is a link, never a backticked code word.

- A bare mention is the inferred-link form `[TGrad]()` (empty parens; the
  converter resolves it to the ref page). Built-ins take the same form:
  `[Total]()`, `[Normal]()`.
- An inline *call* is code-styled **and** autolinked: write
  `<code>[TRealize]()[expr]</code>`, not `` `TRealize[expr]` `` and not plain
  `[TRealize]()`. Markdown forbids nested formatting inside a backtick span but
  processes markdown inside an inline `<code>` element, so the link renders
  inside the code style.
- Backticks are only for things that are *not* symbols: a type or tag (`TTerm`,
  `TAG_TEN`, `UOP_KERNEL`), a context (`THVMLink``), an option value (`"SUM"`,
  `"f32"`), or a path (`wl/Examples/`).
- If you link a paclet symbol that has no `docs/Symbols/<Name>.md` page, **create
  the page** (same pass), so the link resolves.

## Argument names are italics, not math

In a `## Usage` signature and in prose, write argument names in *italics*:
`<code>[TUOpReduce]()[*t*, *axis*]</code>`. **Do not** use the `$x$` math form -
it renders as ugly inline LaTeX. (This overrides the skill, which uses `$x_i$`.)

## Cells: no ceremony, one output each

- **No `Needs`.** MTN loads the package from the frontmatter `Context:` (e.g.
  `Context: THVMLink``) before it evaluates the cells, so an example never needs
  `Needs["THVMLink`"]`.
- **No `TInit`.** The runtime initializes itself on first use, so init calls are
  noise. (A purposeful `TReset[]` / `TInit[]` to show something *in isolation* is
  rare and must earn its place; the kernel table survives `TReset`, only `TInit`
  fully resets it - prefer presenting the live accumulated state honestly.)
- **One output per cell.** Never show `{TTensorShape[x], TTensorDType[x]}` - split
  into two cells.
- **Readouts use [Normal]().** `Normal @ TRealize @ expr` returns a plain list;
  prefer it over `TTensorData @ TRealize @ expr` (the `Normal` UpValue means
  `Normal[TTensorData[...]]`).
- **Self-contained per section.** MTN resets cell state at every `##` heading, so
  a section's cells may not reference a variable bound in an earlier section -
  rebuild what you need at the top of each section.
- **Output type matters.** Lists, strings, numbers, graphics (`Graph`,
  `Legended`, `Column`), and **atomic objects with a summary box** (`TMemoryPlan`,
  `TKernel`, `TOpt`, `TKernelOpts`, `TJitClosure`, a `TTerm`, ...) serialize and
  render fine. Prefer showing the object itself over extracting from it: write
  <code>[TMemoryPlan]()[]</code>, not `Keys[First[TMemoryPlan[]]]`;
  <code>[TKernelProposed]()[*kid*]</code> (a list of `TOpt` boxes), not
  `ToString[..., InputForm]`. What does **not** round-trip is a *bare*
  `Association` (`<|...|>` with no wrapping head) or an `InputForm[...]` box - only
  for those project to a list/string (`Keys[...]`, `ToString[..., InputForm]`) or
  read a scalar property off the handle.

## Teach the sugar, not the UOPs

Show the low-level `TUOp*` constructors once, in the section that introduces the
graph. From then on use the **UpValue sugar** - a `TTerm` behaves like a packed
array, so `a + b`, `a*b`, `Total[a^2]`, `m1 . m2`, `Transpose[m]`,
`ArrayReduce[Total, m, axis]` build the same graph and read as ordinary
Mathematica. Do not fall back to `TUOp*` once the sugar is introduced. If a
common operation has no sugar yet, add the UpValue (see
[`wl/GUIDE.md`](../../GUIDE.md)) rather than writing the UOP in the doc.

This holds in **example code**, not just prose, and on **every page**: write
`xFeed * xFeed`, never `TUOpMul[xFeed, xFeed]`, when the product is incidental to
what you are showing. The mapping: `TUOpMul[a,b]` -> `a*b`, `TUOpAdd[a,b]` ->
`a + b`, `TUOpReduce[t, 0, "SUM"]` -> `Total[t]`, `TUOpReshape[t, sh]` ->
`ArrayReshape[t, sh]`, `TUOpPermute[t, p]` -> `Transpose[t, p]`, `TUOpNeg[t]` ->
`-t`, `TUOpConst[c]` -> the bare number `c` when it is an operand. The **only**
place a raw `TUOp*` constructor belongs is its own reference page, where it is
the subject (and even there you may show the sugar equivalent alongside).

The one unavoidable exception is a `TLam[*w*, ...]` **binder body**: the bound
`*w*` is a held symbol, not a `TTerm`, so `*w*^2` does not fire the UpValue
(`TApp[TLam[*w*, *w*^2], ...]` stays unevaluated) and the body must be written
with `TUOp*`. So `TLam` / `TApp` examples carry raw UOPs legitimately. But when a
lambda is not itself the point -- e.g. you only need a compiled kernel to inspect
-- build the graph on a concrete tensor with sugar instead
(`<code>[TRealize]()[[Total]()[*x*^2]]</code>` realizes to an inspectable kernel),
so no `TUOp*` appears at all.

## Show the runtime, not just numbers

Use the visualization surface so a reader sees the machine, not only outputs:
[THeapGraph]() (the interaction-net string diagram of a term), [TScheduleGraph]()
(the kernel-dependency DAG), [TMemoryPlanGantt]() (buffer alive-spans as a Gantt),
[TMemoryPlanReport]() (the plan summary). They render as real graphics in the
notebook.

Atomic THVMLink objects (`TTerm`, `TKernel`, `TOpt`, `TKernelOpts`, `TJitClosure`,
...) have `MakeBoxes` summary boxes - an icon plus key fields, expandable to the
rest (defined in [Format.wl](../Kernel/Format.wl)). They round-trip through MTN and
render in the `.nb`, so **display the object itself** in an output cell where the box
aids the reader, not only its extracted scalars - e.g. show a [TJit]() closure (its
box greens up and bumps the op count once it has captured) alongside the call result.

## Be mechanically accurate

Explain what actually happens, in the runtime's own terms:

- A kernel's `"jit"` `"DispatchKind"` is the CPU backend handing it to `clang -O3`
  **automatically on realize** - it is not [TJit](). [TJit]() is the separate
  *capture/replay* closure over a whole dispatch sequence.
- [TGrad]() **adds the backward branch to the heap** and fires one walk that
  auto-grads every reachable float leaf, accumulating each one's gradient into
  its grad slot (no `requires_grad` flag, matching tinygrad). It does not
  "run a forward pass" or keep a "tape" - there is no tape, it is all the heap.
- `TKernelSource` renders `"C"` / `"Metal"` only on `main` (no CUDA/PTX backend).

## Page shape

- Frontmatter: `Template`, `Name`, `Context`, `Paclet`, `URI` (the `ref/` or
  `tutorial/` path; the basename must match the URI tail), `Keywords`, and -
  for a Symbol page - `SeeAlso` and `RelatedGuides`.
- A tutorial (`Template: TechNote`) carries one running example deep across
  sections with real prose and narrative, like
  [ATP.md](Tutorials/ATP.md).
- A symbol page (`Template: Symbol`) has `## Usage` (the signature, one statement
  per paragraph), `## Details & Options` (bullets become Notes), then
  `## Basic Examples` / `## Scope` / `## Properties and Relations` /
  `## Possible Issues` as warranted. Model it on
  [Symbols/TUOpAdd.md](Symbols/TUOpAdd.md).
