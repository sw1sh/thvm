---
Template: Paclet
ResourceType: Paclet
Name: WolframInstitute/THVMLink
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
Description: Wolfram Language bridge to thvm - observe and drive the interaction-net runtime.
ContributedBy: Wolfram Institute
Keywords: [interaction net, HVM, tensor, JIT, autodiff, ATP, equational]
MainGuide: Documentation/English/Guides/THVMLink.nb
License: MIT
WolframVersion: 13.0+
Categories: [Machine Learning, Symbolic & Numeric Computation]
SourceControlURL: https://github.com/sw1sh/thvm
---

## Details & Options

- THVMLink is the Wolfram-language driver for `thvm`, a tensor-aware interaction-net runtime that compiles to CPU, CUDA, and Metal back ends from one symbolic graph.
- Construction is purely symbolic. <code>[TLam]()</code>, <code>[TApp]()</code>, <code>[TSup]()</code>, and <code>[TDup]()</code> build interaction-combinator terms; <code>[TUOpAdd]()</code> and the rest of the `TUOp*` family build the tensor UOp graph; <code>[TTensorCreate]()</code> ingests a [NumericArray]() zero-copy on CPU.
- Reduction is staged. <code>[TWnf]()</code> drives a term to weak normal form, <code>[TMaterialize]()</code> schedules the UOp DAG into kernels, and <code>[TRealize]()</code> fires the whole pipeline (schedule + dispatch).
- Differentiation lives in the runtime. <code>[TGrad]()</code> seeds a single backward walk that auto-grads every reachable float leaf; per-leaf adjoints are readable via <code>[TGradOf]()</code>.
- Introspection is first class. <code>[THeapGraph]()</code> renders the live heap as an IC string diagram, <code>[TMemoryPlan]()</code> projects per-buffer alive spans, and <code>[TKernel]()</code> exposes per-kernel timing, source, and autotune candidates.
- The ATP submodule wraps `thvm`'s equational saturation engine via <code>[TFindProof]()</code> (a drop-in [FindEquationalProof]() replacement that returns a full `ProofObject`) and the lower-level <code>[TATP]()</code>.

## Usage

THVMLink exposes <code>[TLam]()</code>, <code>[TApp]()</code>, <code>[TTensorCreate]()</code>, <code>[TUOpAdd]()</code>, <code>[TUOpMul]()</code>, <code>[TUOpReduce]()</code>, <code>[TRealize]()</code>, <code>[TGrad]()</code>, <code>[TKernel]()</code>, <code>[TMemoryPlan]()</code>, <code>[TFindProof]()</code>, and many more.

## Basic Examples

Build and reduce a tiny term:

```wl
Needs["WolframInstitute`THVMLink`"];
TInit[];
TWnf[TApp[TLam[x, x], TLam[y, y]]]
```
<!-- => TTerm[<LAM identity>] -->

---

Build a UOp graph and realize it on the active backend:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
y = TTensorCreate[{10., 20., 30., 40.}];
TTensorData @ TRealize[TUOpAdd[x, y]]
```
<!-- => NumericArray[{11., 22., 33., 44.}, "Real32"] -->

---

Take a gradient through a small expression (every float leaf is auto-graded, no marking):

```wl
w = TTensorCreate[{1., 2., 3.}];
x = TTensorCreate[{4., 5., 6.}];
TRealize @ TGrad[TUOpReduce[TUOpMul[w, x], 0, "SUM"]];
TTensorData @ TRealize @ TGradOf[w]
```
<!-- => NumericArray[{4., 5., 6.}, "Real32"] -->

## Scope

Render the live interaction-net heap as an IC string diagram, seeded at a particular term:

```wl
THeapGraph @ TApp[TLam[w, TUOpAdd[w, w]], TTensorCreate[{1., 2.}]]
```
<!-- => a Graph of the heap reachable from the seed term, with LAM / APP / TEN / UOP nodes coloured per Style.wl -->

---

Snapshot the live runtime to a transportable Wolfram expression, restore it across a fresh kernel:

```wl
#| eval: False
snap = TContextSnapshot[TLam[x, TUOpAdd[x, x]]];
TFree[]; TInit[];
TInitialize[snap]
```
<!-- => the original lambda, live again in the freshly initialized runtime -->

## Applications

Project the schedule the materializer produces from a small neural-net forward pass and gate it through the memory planner:

```wl
W = TGlorot[{4, 10}]; b = TZeros[{10}]; xs = TTensorCreate[ConstantArray[1., {1, 4}]];
loss = TL2Loss @ TLinear[xs, W, b];
TMaterialize[loss];
TMemoryPlanReport @ TMemoryPlan[]
```
<!-- => a Column of top-5 largest buffers + alive-span summary for the scheduled DAG -->

---

Prove a small equational theorem with the C-engine ATP and reconstruct it as a Wolfram `ProofObject`:

```wl
#| eval: False
Needs["WolframInstitute`THVMLink`ATP`"];
TFindProof[
    Inactive[Equal][x \[CircleTimes] y \[CircleTimes] z, z \[CircleTimes] y \[CircleTimes] x],
    "AbelianGroupAxioms",
    TimeConstraint -> 10
]
```
<!-- => ProofObject[...] supporting ["ProofDataset"], ["ProofGraph"], ["ProofLength"], etc. -->

## Properties and Relations

Per-kernel introspection - source, autotune candidates, and dispatch counters live on <code>[TKernel]()</code>:

```wl
TRealize @ TUOpAdd[TTensorCreate[Range[1., 8.]], TTensorCreate[Range[1., 8.]]];
k = TKernel[1];
{k["DispatchKind"], k["DispatchCount"], TKernelProposed[1]}
```
<!-- => {"jit" or "blas-...", n_dispatches, {TOpt[op, axis, arg], ...}} -->

## Possible Issues

The default `make` target does not rebuild the WL paclet dynamic library; the host `.wlt` tests and notebooks dispatch through the stale `LibraryResources/THVMLink.dylib`. Always run `make && make wl` from the repository root after touching C sources.

## Hero Image

A single luminous symbolic graph morphs from an interaction-net string diagram on the left, through a stack of translucent tensor planes in the middle, into an equational rewrite proof graph on the right - the three computational surfaces THVMLink unifies behind one symbolic graph:

```wl
ImageResize[
    Import[PacletObject["WolframInstitute/THVMLink"]["AssetLocation", "Hero"]],
    {1500, Automatic}
]
```

## Author Notes

Drafted with assistance from Claude (Anthropic, model `claude-opus-4-7`) under
human supervision. Frontmatter, section structure, and prose were machine-
generated from the in-source `::usage` strings; the Wolfram Language examples
were reviewed by a human contributor against the live paclet before publishing.
The C runtime, the LibraryLink bridge, and the `Kernel/*.wl` sources THVMLink
documents are hand-written and pre-date this documentation pass.
