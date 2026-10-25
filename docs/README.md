# thvm docs

One self-contained page per piece. Pages link the source they describe, so the
prose and the implementation move together. The two tracks, the tensor
compiler (NN) and the theorem prover (ATP), share the core runtime.

## core/: the interaction-calculus runtime

Read in this order if you are new:

1. [core/term.md](core/term.md): how a 64-bit `Term` is packed.
2. [core/heap.md](core/heap.md): the flat heap, the allocator, the GC and the
   substitution model that makes interactions work.
3. [core/normal_form.md](core/normal_form.md): the reducers (`wnf`, `cnf`, the
   collapsers, the parallel `nf` pool) and tracing.
4. [interact/_.md](interact/_.md): the active-pair rules, one page per rule,
   paired with `src/interact/<name>.c`.
5. [core/wl.md](core/wl.md): the Wolfram LibraryLink bridge (THVMLink) and the
   IC constructors on top of it.
6. [core/heap_graph.md](core/heap_graph.md) and
   [core/diagrams.md](core/diagrams.md): `THeapGraph` and `THeapDiagram`, the
   two heap visualizations.

Also: [core/glossary.md](core/glossary.md) (vocabulary of both tracks),
[core/aot.md](core/aot.md) and [core/aot_metal.md](core/aot_metal.md) (the
ahead-of-time compiler for `TDef` bodies, C and Metal targets).

## NN/: tensors, compiler, backends

- [NN/lowering_passes.md](NN/lowering_passes.md): UOp DAG to dispatched kernel,
  pass by pass.
- [NN/cpu.md](NN/cpu.md), [NN/metal.md](NN/metal.md): the CPU backend with the
  shared codegen layer, and the Metal backend.
- [NN/tinygrad_late_passes.md](NN/tinygrad_late_passes.md): how UPCAST and
  UNROLL reach rendered source, against tinygrad's expander and devectorizer.
- [NN/dtypes.md](NN/dtypes.md), [NN/symbolic_shapes.md](NN/symbolic_shapes.md),
  [NN/memory.md](NN/memory.md): dtypes, runtime-variable dimensions, buffer
  lifetime.
- [NN/grad.md](NN/grad.md), [NN/gather_scatter.md](NN/gather_scatter.md),
  [NN/nn.md](NN/nn.md): autograd as an interaction, the native indexed ops,
  and the C NN layer behind the Python and Wolfram front ends.
- [NN/inert_loop_nettrain.md](NN/inert_loop_nettrain.md): a whole `NetTrain`
  run as one inert term.
- [NN/flux_forward_spec.md](NN/flux_forward_spec.md): the FLUX.2-klein-4B
  forward-pass spec the port follows.
- [NN/thvm_vs_tinygrad.md](NN/thvm_vs_tinygrad.md): pipeline comparison, knob
  reference and coverage gaps against tinygrad.
- [NN/env_knobs.md](NN/env_knobs.md): every `THVM_*` environment variable.

## ATP/: the equational theorem prover

Start at [ATP/README.md](ATP/README.md): `bin/FindProof` (the CASC entry) and
`TFindProof` (the Wolfram surface), the algorithms and engineering pages, the
CASC strategy, the tutorials (`atp_methods.md`, `tptp_import.md`, `smt.md`),
the ENIGMA plan and the knob table ([ATP/env_knobs.md](ATP/env_knobs.md)).

## Side tracks

Research and essays outside the two tracks:

- [multicomputation/](multicomputation/multicomputation.md): the interaction
  calculus read as multicomputation
  ([multicomputation.md](multicomputation/multicomputation.md)), the trace that
  records it ([multicomputation_trace.md](multicomputation/multicomputation_trace.md))
  and worked essays on superposition and the trace, run cell by cell
  ([essays/](multicomputation/essays/)).
- The primers on neuroscience for toy AI and on symbolic and neuro-symbolic AI
  live with the ARC work, in the brain-arc repo's `docs/neuroscience/` and
  `docs/symbolic/` (`external/brain/`).

## Elsewhere

- [../AGENTS.md](../AGENTS.md): conventions, build, code map.
- [../wl/GUIDE.md](../wl/GUIDE.md): Wolfram Language style rules.
- [../wl/THVMLink/docs/](../wl/THVMLink/docs/): the paclet's own documentation
  source (guides, tutorials, reference pages), with its rules in
  [DOC_GUIDE.md](../wl/THVMLink/docs/DOC_GUIDE.md).
