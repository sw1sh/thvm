---
Template: Guide
Name: THVMLink
Title: THVMLink
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/guide/THVMLink
Description: Wolfram Language bridge to thvm - observe and drive the interaction-net runtime.
Keywords: [interaction net, HVM, tensor, JIT, autodiff, ATP, equational]
RelatedGuides: [ComputationalGraphs, NeuralNetworks, EquationalProofs]
Links: ["[thvm on GitHub](https://github.com/sw1sh/thvm)"]
---

## Abstract

THVMLink is the Wolfram-language driver for `thvm`, a tensor-aware interaction-
combinator runtime. One symbolic graph carries lambda terms, the tensor UOp
DAG, and the kernel schedule; the same paclet renders the live heap as an IC
string diagram, plans buffer lifetimes, fires autodiff over the heap, autotunes
the CPU/CUDA/Metal back ends, and proves equational theorems through the
C-side ATP. Bring a tensor with <code>[TTensorCreate]()</code>, build a
combinator term with <code>[TLam]()</code> or a UOp graph with the `TUOp*`
family, and drive it through <code>[TRealize]()</code> on whichever back end
the active <code>[TContext]()</code> selects.

## Functions

### Lifecycle

- `TInit` initializes the runtime
- `TFree` tears the runtime down
- `TReset` zeroes the heap, the WNF stack, and the interaction counter

### Term calculus

- `TLam` constructs a lambda; bodies that are UOp graphs JIT into a kernel on first application
- `TApp` constructs an application
- `TSup` constructs a SUP (superposition) with a fresh label
- `TDup` constructs a DUP and returns the projected pair `{dp0, dp1}`
- `TEra` constructs an eraser
- `TVarFor` constructs a VAR pointing at a binder loc
- `TFreshLabel` returns the next monotonic SUP/DUP label

### Reduction

- `TWnf` reduces to weak normal form
- `TNf` reduces to full normal form by firing every reachable redex
- `TCnf` runs the readback layer that lifts the first SUP to the top
- `TStep` fires exactly one interaction
- `TInteract` fires one named redex by its `TTerm` key
- `TRedexes` lists every redex in the live heap
- `TCollapse` enumerates the SUP-tree of a term as a list of leaves
- `TReduce` reduces in-place and returns the original root

### Inspection

- `TTermExpr` walks the heap and returns a tag-headed nested expression
- `TTermTree` renders `TTermExpr` as a Wolfram `Tree`
- `TTermSubexprs` returns every `path -> subterm` rule, pre-order DFS
- `TTermShape` runs the runtime's `term_shape_in` shape inference
- `THeap` returns an Association snapshot of the live heap
- `THeapGraph` renders the heap as an IC string-diagram Graph
- `THeapDiagram` renders the heap as a `DiagramNetwork`

### Tensors

- `TTensor` allocates a tensor with given shape and dtype
- `TTensorCreate` builds a tensor from a `NumericArray` or nested list, zero-copy on CPU
- `TTensorShape` returns the tensor's shape
- `TTensorData` reads the tensor's buffer as a `NumericArray`
- `TTensorDType` returns the dtype as a string
- `TSet` writes the bytes of one tensor into another's backing buffer in place

### UOp graph constructors

- `TUOpAdd` builds elementwise `+`
- `TUOpMul` builds elementwise `*`
- `TUOpNeg`, `TUOpRecip`, `TUOpExp2`, `TUOpLog2`, `TUOpSqrt` build the corresponding unary nodes
- `TUOpCmplt`, `TUOpCmpeq` build comparison masks
- `TUOpReshape`, `TUOpPermute`, `TUOpExpand`, `TUOpPad`, `TUOpShrink`, `TUOpFlip` build the movement primitives
- `TUOpReduce` builds a `SUM` or `MAX` reduction along one axis
- `TUOpCast` value-preservingly casts to a named dtype
- `TUOpBitcast` reinterprets bits at the same itemsize
- `TUOpGrad` builds the backward projection of a grad cell
- `TUOpLoad`, `TUOpStore`, `TUOpBuffer`, `TUOpRange`, `TUOpIndexE`, `TUOpAfter`, `TUOpOpt` build the Phase E index-layer primitives

### Materialization and dispatch

- `TMaterialize` schedules the UOp DAG into kernels without dispatching
- `TRealize` materializes then drives the result through `TWnf`
- `TAssign` lazily writes a source UOp's result into a destination tensor
- `TGrad` is `loss.backward()` - one walk that auto-grads every reachable float leaf
- `TGradOf` returns a leaf's lazy accumulated gradient
- `TClearGrad` zeroes a leaf's gradient (PyTorch `zero_grad` analogue)

### Neural-net building blocks

- `TLinear` is the `nn.Linear` analogue, `x @ W + b` with broadcasted bias
- `TConv2D` is a stride-1, no-pad 2-D convolution, dispatched via im2col + `cblas_sgemm`
- `TMatMul`, `TMatVec`, `TDot` are the matrix / vector / inner-product surface
- `TReLU`, `TTanh`, `TGELU`, `TSoftmax`, `TLog` are the activations
- `TLayerNorm`, `TLayerNormAffine`, `TBatchNorm`, `TBatchNormTrain` are the normalization layers
- `TAttention`, `TMultiHeadAttention`, `TCausalMask`, `TEmbedding`, `TEmbeddingMatrix` are the transformer primitives
- `TCrossEntropyLoss`, `TCategoricalCrossEntropy`, `TSparseCategoricalCrossEntropy`, `TMSELoss`, `TL2Loss` are the losses
- `TGlorot`, `TZeros`, `TOnes`, `TZerosLike`, `TOneHot` are the initializers
- `TFromNet`, `TFromLayer`, `TLayerWeights`, `TLayerToTensors` import a Wolfram NN expression into a UOp graph

### Optimization

- `TAdam` is the Adam optimizer wrapper
- The single-step SGD update is conventionally expressed as `TSet[w, w - lr * grad]`

### Kernels

- `TKernel` wraps a UOP_KERNEL term as a typed object with a property surface
- `TKernelSource` returns the C99 or Metal source of a lifted kernel
- `TKernelInfo` returns the input/output shape header
- `TKernelStoreRoot` returns the lifted UOp DAG as a `TTerm`
- `TKernelDispatch` dispatches the kernel; same as calling `k[]`
- `TOpt` builds a typed kernel-optimization action (UPCAST, UNROLL, LOCAL, GLOBAL, TC, ...)
- `TKernelOpts` returns the axis-typed scheduling plan
- `TKernelApplyOpt` mutates the plan with one `TOpt`
- `TKernelProposed` lists the C-side proposer's candidate `TOpt`s
- `TKernelAutotune`, `TKernelAutotuneAll`, `TKernelAutotuneUnique`, `TKernelAutotuneTop` benchmark proposed candidates and apply the winner
- `TKernelVariants` returns each proposed candidate's wallclock without committing
- `TKernelTable`, `TKernelCount`, `TKernelInputs` are the side-table accessors

### Memory and profiling

- `TMemoryPlan` returns a per-buffer alive-span snapshot, alias-aware
- `TMemoryPlanReport` formats it as a `Column` with top-k by bytes and span
- `TMemoryPlanGantt` renders the buffer life cycle as a Gantt chart
- `TCpuBufTable`, `TMetalBufTable`, `TMetalBufSummary`, `TMetalMemoryProfile` are the backend buffer tables
- `TMetalGpuTime`, `TMetalPerOpProfile` are the Metal GPU-time accessors
- `TProfile` is the headline-metric snapshot (cells by tag, tensors, kernels, interactions)
- `TProfileTable`, `TProfileGrowth`, `TProfilePlot`, `TProfileReport` are the multi-snapshot views
- `THotCounters`, `THotCountersDelta`, `THotCountersReport` expose the per-context hot-path counters

### Visualization

- `THeapGraph` renders the IC string diagram
- `THeapDiagram` renders the diagram-network form via `DiagrammaticComputation`
- `TScheduleGraph` renders the kernel DAG
- `TMemoryPlanGantt` renders the buffer Gantt

### Snapshots

- `TContextSnapshot` captures every live heap cell, book cell, tensor, definition, and ALO state
- `TInitialize` restores a snapshot into a fresh runtime
- `TContextStrip` drops the tensor buffers for a small portable copy
- `TContextToTermTree` renders the snapshot's term tree without touching the runtime

### Contexts (back-end + heap partitioning)

- `TContext` is the typed handle to a runtime context
- `TContextNew` allocates a fresh context with a given default device
- `TContextDestroy` frees a context's C-side state
- `TContextCurrent` returns the active context
- `TContextList` lists every allocated context
- `TInContext` evaluates a body with `$TContext` rebound

### Benchmarking

- `TBench` runs a training step under controlled timing and snapshots the memory plan
- `TBenchReport` formats a `TBench` result for stdout
- `TBenchExport` writes the report to a file for diffing

### Multiway and pattern surface

- `TMultiwayGraph` builds the multiway reduction graph
- `TPattern`, `TMatchQ`, and the `Rewrite.wl` surface drive WL-side pattern-rewriting against `TTerm`
- `TLazy` exposes lazy WL expressions to the runtime

### Theorem proving (`THVMLink`ATP``)

- `TFindProof` runs the C-engine equational saturator and returns a Wolfram `ProofObject`
- `TFindEquationalProof` is a deprecated alias for `TFindProof`
- `TRelevantAxioms` reports the relevance filter's `Kept` / `Dropped` partition without running a proof
- `TAtpSchedule` returns the per-config schedule a `Method` would expand to
- `TAtpDescribeMethod` describes a `Method` preset's defaults
- `TATP` is the lower-level entry point that consumes a Waldmeister `.pr` file or raw equational axioms

### Tutorials

- [Overview tutorial](paclet:WolframInstitute/THVMLink/tutorial/Overview) walks an end-to-end forward + backward pass and prints the resulting heap, schedule, and Metal source.
