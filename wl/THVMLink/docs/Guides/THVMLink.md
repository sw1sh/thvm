---
Template: Guide
Name: THVMLink
Title: THVMLink
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/guide/THVMLink
Description: Lambda terms and their reduction as interaction nets, lazy tensor graphs compiled to kernels, gradients, memory plans, runtime snapshots and equational theorem proving
Keywords: [interaction net, lambda calculus, superposition, reduction, tensor, lazy graph, kernel, automatic differentiation, gradient, training, memory plan, snapshot, theorem proving, equational logic]
RelatedTutorials: [Overview, Tensors, Train, Gpt2, ATP, AtpMethods, Disproof, FindReplacePath, TPTPImport, Waldmeister, SMT, GnnGuidance]
Links: ["[thvm source repository](https://github.com/sw1sh/thvm)", "[Neural Networks](https://reference.wolfram.com/language/guide/NeuralNetworks.html)", "[Theorem Proving](https://reference.wolfram.com/language/guide/TheoremProving.html)"]
---

## Abstract

THVMLink computes with `thvm`, a runtime that evaluates programs by rewriting an interaction net held in one heap. It covers lambda terms and superpositions and their reduction; the inspection of terms and the heap, and drawings of interaction nets and reductions; tensors, whose arithmetic builds a lazy graph of micro-operations; the kernels such a graph compiles to, with their schedules and memory plans; gradients, training and models; runtime contexts and their snapshots; and equational theorem proving, with ground decision procedures, external provers and learned proof guidance.

## Functions

### Lambda terms and superpositions

- `TLam` a lambda whose variable is a symbol
- `TApp` the application of a term to an argument
- `TNum` a number term
- `TOp2` an integer operation on two terms
- `TSup` a superposition of two terms, standing for both at once
- `TDup` the two copies of a duplicated term
- `TEra` the eraser, a term that discards what it interacts with
- `TCtr` a constructor with an integer label and children
- `TMatCtr` a matcher that dispatches on the label of a constructor
- `TIfZero` a conditional on whether a number term is 0
- `TEql` a test of structural equality that reduces to 1 or 0

- `TMatNum`, `TMatChain`, `TDsu`, `TDdu`, `TAny`, `TVarFor`, `TBookCtr`, `TFreshLabel`

### Reduction

- `TWnf` reduce a term to weak normal form
- `TNf` reduce a term to full normal form
- `TCollapse` the list of values a superposition stands for
- `TCnf` reduce a term to collapsed normal form, with superpositions lifted to the top
- `TStep` fire at most one interaction at the head of a term
- `TReduce` reduce a term to weak normal form in place
- `TRedexes` the redexes in the live heap
- `TInteract` fire the interaction at one redex
- `TItrs` the number of interactions fired so far

- `TStack`, `TTermEq`, `TTermSame`

### Inspecting terms and the heap

- `TTerm` a term of the runtime
- `Term` the nested canonical form of a term
- `TTermExpr` a nested expression of the tag names of the cells a term reaches
- `TTermTree` the tag names of the cells a term reaches as a tree
- `TTermSubexprs` the subterms of a term together with their positions
- `TTermVal` the value field of a term, such as the value of a number

- `TSubexprAt`, `TTermTag`, `TTagName`, `TTermExt`, `TTermSub`, `THeapRead`, `THeapSet`, `THeapAlloc`, `THeapPos`, `THeapBase`, `TBookRead`, `TGCCollect`, `TExternPinCount`

### Drawing interaction nets and reductions

- `THeapGraph` the interaction net of a term as a graph
- `THeapDiagram` the interaction net of a term as a string diagram
- `TMultiwayGraph` the multiway graph of the reduction of a term
- `TCausalGraph` the causal graph of the interactions that reduce a term
- `TMultiTrace` a record of each step of the reduction of a term
- `TMultiTraceQ` whether the runtime can record the events of a reduction

### Tensors

- `TTensorCreate` a tensor holding the numbers of a list or a numeric array
- `TTensorShape` the shape of a tensor
- `TTensorDType` the element type of a tensor
- `Plus` (WL), `Times` (WL), `Power` (WL), `Dot` (WL), `Total` (WL), `Transpose` (WL), `ArrayReshape` (WL), `ArrayReduce` (WL) arithmetic and array operations, which on tensors build a lazy graph
- `TRealize` compute a lazy tensor graph, giving a tensor of its values
- `Normal` (WL) the values of a computed tensor as a list
- `TTensorData` the values of a tensor as a numeric array
- `TSet` write new values into the buffer of a tensor in place
- `TMatMul` the matrix product of two tensors
- `TWhere` an elementwise choice between two tensors by a mask

- `TMaximum`, `TMinimum`, `TClip`, `TGather`, `TTakeAlongAxis`

### Graphs of micro-operations

- `TUOpAdd` the node for an elementwise sum
- `TUOpMul` the node for an elementwise product
- `TUOpNeg` the node for an elementwise negation
- `TUOpConst` the node for a constant
- `TUOpReduce` the node for a sum or a maximum along an axis
- `TUOpReshape` the node for a view of a tensor in a new shape
- `TUOpPermute` the node for a reordering of the axes of a tensor

### Kernels

- `TMaterialize` schedule a lazy tensor graph into kernels without running them
- `TKernel` a scheduled kernel as an object with properties
- `TKernelSource` the C or Metal source code of a kernel
- `TKernelFlops` an estimate of the arithmetic operations in one dispatch of a kernel
- `TKernelProposed` candidate optimizations of the loop schedule of a kernel
- `TKernelApplyOpt` apply an optimization to the loop schedule of a kernel
- `TKernelCount` the number of entries in the kernel table of the runtime

### Schedules and memory

- `TScheduleGraph` the graph of the kernels of the current schedule
- `TMemoryPlan` the kernels, tensors and buffers of the current schedule
- `TMemoryPlanReport` a text summary of a memory plan
- `TMemoryPlanGantt` the lifetimes of the buffers of a memory plan as a Gantt chart

### Gradients

- `TGrad` differentiate a scalar with respect to every floating-point tensor it depends on
- `TGradOf` the gradient held for a tensor
- `TClearGrad` clear the gradient held for a tensor

### Training and models

- `TGlorot` random weights for a layer, scaled by its number of inputs
- `TJit` a closure that records the kernels a function dispatches and replays them
- `TBench` timing and memory measurements of a training step
- `FluxGenerate` an image generated from a text prompt by a text-to-image model

### Runtime contexts and snapshots

- `TInit` initialize the runtime
- `TReset` empty the heap of the runtime and restart its counters
- `TContext` a runtime context, or a snapshot of one
- `TContextSnapshot` a snapshot of the runtime as an expression
- `TInitialize` restore a snapshot into the runtime
- `TContextStrip` a snapshot with the data of each tensor replaced by its shape

### Equational theorem proving

- `TFindProof` prove a theorem from equational axioms, giving a proof object
- `ProofObject` (WL) a proof as an object with properties
- `FindEquationalProof` (WL) the built-in prover for equational theorems
- `TProofObject` a proof object in the native form of the runtime
- `TToProofObject` convert a native proof object to a built-in proof object
- `TFindEquationalPath` the rewrite path from one side of a theorem to the other
- `TFindStringProof` prove a theorem about rewriting strings
- `TStringPath` the rewrite path of a string theorem as a list of words
- `CounterexampleObject` a disproof of a conjecture by a counterexample
- `TFindFiniteModels` finite models of a set of relations as multiplication tables
- `TRelevantAxioms` the axioms a relevance filter keeps for a conjecture
- `TAtpSchedule` the list of configurations a proving method runs
- `TAtpDescribeMethod` the settings a proving method resolves to
- `TATP` run the saturation engine on axioms and a conjecture
- `TFindEquationalProof` an earlier name of TFindProof

### Ground decision procedures

- `TSatEUF` decide equalities and disequalities between terms built from uninterpreted functions
- `TSmtDecide` decide the satisfiability of a Boolean combination of equalities

### External provers

- `TVampireProof` run the Vampire prover on a problem file
- `TTweeProof` run the Twee prover on a problem file
- `TEproverProof` run the E prover on a problem file
- `TWaldmeisterProof` run the Waldmeister prover on a problem file
- `TVampireProofObject`, `TTweeProofObject`, `TEproverProofObject`, `TWaldmeisterProofObject` a proof from an external prover in the form of the runtime's proofs
- `TSZSDerivationToProofObject` a proof from a derivation in the SZS output format

### Learned proof guidance

- `TAtpCpDataset` labeled features of the critical pairs a prover processes
- `TAtpTrainScorer` train a model that scores critical pairs
- `TAtpSetLearnedScorer` make the prover select critical pairs with a trained model
- `TAtpCpGraph` an equation as a graph for a graph neural network
- `TAtpCpGraphEquation` the equation a graph encodes
- `TAtpGraphDataset` a labeled dataset of equation graphs from proofs
- `TAtpTrainGnn` train a graph convolutional network on equation graphs
- `TAtpGnnScore` the scores a trained network gives equation graphs
- `TFindProofReranked` prove a theorem, reordering the critical pairs with a trained network
- `TFindProofGnnReranked` prove a theorem with a trained network scoring critical pairs inside the prover

- `TAtpSetGnnScorer`, `TAtpSaveGnnScorer`, `TAtpLoadGnnScorer`, `TAtpGnnScorerAsset`
