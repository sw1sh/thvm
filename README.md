# thvm

An Interaction Calculus runtime in C with a tinygrad-style tensor compiler and
an equational theorem prover on the same flat heap. Python (`thvm`, tinygrad's
API) and the Wolfram Language (the THVMLink paclet) are the hosts.

- Architecture, one piece per page: [docs/README.md](docs/README.md)
- Conventions, build, code map: [AGENTS.md](AGENTS.md)
- Wolfram Language style rules: [wl/GUIDE.md](wl/GUIDE.md)

## What it does

- **Tensors (NN).** tinygrad's `Tensor` API from Python (`import thvm` in place
  of `import tinygrad`) and from Wolfram (`TTensor`, `TUOp*`, `TFromNet`,
  `NetTrain`). Graphs are scheduled by a port of tinygrad's rangeify, rendered
  by one UOp DAG renderer and dispatched to CPU (clang JIT, BLAS, a UOp
  walker), Metal or CUDA kernels. Autograd is an interaction rule
  (`UOP_GRAD`); `TinyJit` captures and replays kernel sequences; dimensions
  can be symbolic. Examples train LeNet and beautiful_mnist, decode GPT-2 and
  run FLUX.2 image generation.
- **Theorem proving (ATP).** Unfailing Knuth-Bendix completion, faithful to
  Waldmeister by default, with ports from Twee and Vampire, AC completion and a
  first-order clausal front end. `bin/FindProof` is the CASC entry (StarExec
  packaging in `tools/casc/`); `TFindProof` returns a Wolfram `ProofObject`;
  `bin/CheckProof` checks TSTP proofs (`tools/proover/`).
- **IC core.** 31 interaction rules, a weak-head stack machine, SUP-lift
  readback and collapsers, a parallel normal-form pool, a Cheney GC, heap
  visualizations (`THeapGraph`, `THeapDiagram`), an ahead-of-time compiler for
  `TDef` bodies, and a multicomputation trace (`TMultiTrace`).

## Build

```bash
make               # THVMLink library (needs Wolfram 13+ and its LibraryLink headers)
make py            # py/thvm/libthvm_py.{dylib,so}
make bin/FindProof # the ATP CLI
make test          # build and run every C test under tests/
make wl-test       # run every wl/THVMLink/Tests/*.wlt
```

Requires a C11 compiler (`clang` by default). The Metal backend builds on
macOS with Xcode's Metal toolchain (`make HAVE_METAL=` builds CPU-only), CUDA
on Linux with the toolkit; `DEV=cpu|metal|cuda` picks the device at
runtime. `make wl` finds the newest Wolfram install (`/Applications/Wolfram*.app`
on macOS, `/usr/local/Wolfram/*/*` on Linux); `WOLFRAM_APP=<install>` picks
another, and `WL_INCLUDE=<dir>` points at any directory holding
`WolframLibrary.h`. More targets in [AGENTS.md](AGENTS.md#build-and-test).

## Quick start

Python:

```python
# make py; then, from the repo root: PYTHONPATH=py python3
from thvm import Tensor
a = Tensor([[1., 2.], [3., 4.]])
print((a @ a).relu().sum().numpy())
```

Wolfram Language:

```wolfram
PacletDirectoryLoad["wl/THVMLink"];
Needs["WolframInstitute`THVMLink`"];

TInit[];
a = TTensorCreate @ NumericArray[{{1., 2.}, {3., 4.}}, "Real32"];
b = TTensorCreate @ NumericArray[{{5., 6.}, {7., 8.}}, "Real32"];
TTensorData @ TRealize @ TUOpAdd[a, b]
(* NumericArray[{{6., 8.}, {10., 12.}}, "Real32"] *)

g = TGrad[TUOpReduce[TUOpMul[a, a], 0, "SUM"], a];
TTensorData @ TRealize @ g
(* NumericArray[{{2., 4.}, {6., 8.}}, "Real32"] *)

TFindProof["InverseOfInverse", "AbelianGroupAxioms"]
```

`wl/Examples/` has runnable scripts for the IC primitives, the tensor and
autograd basics and the training pipelines
(`DEV=metal wolframscript -f wl/Examples/lenet-mnist/verify.wls`).

## Layout

```
src/            C runtime, one translation unit through src/thvm.c
py/             Python package thvm (py/thvm/) and its tests (py/tests/)
wl/             THVMLink paclet (wl/THVMLink/), examples, scripts, GUIDE.md
tests/          one self-contained C test per executable; the ATP track's in tests/atp/
tools/          FindProof CLI, CASC and ProoVer packaging, ATP and model tooling
bench/          benchmarks against tinygrad, PyTorch, MLX and Twee
docs/           architecture: core/, NN/, ATP/
```

The function `foo_bar()` lives at `src/foo/bar.c`, after
[HVM4](https://github.com/HigherOrderCO/HVM)'s style guide; tinygrad is the
spec the tensor track follows.
