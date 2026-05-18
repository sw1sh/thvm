# Kernel engineering: a field guide

A research survey and tutorial on GPU compute kernels: what they are,
how they are written, why they are hard, and how the current tools
(thvm's UOp rewriting, tinygrad, TileLang, the HuggingFace Kernel Hub)
attack the problem.

This document is background reading.  For the operational agent loop
(write a kernel, score it, iterate) see [agent_brief.md](agent_brief.md)
and the rest of this directory.

## Contents

1. What a compute kernel is
2. The GPU execution model (the part you must internalize)
3. How kernels are written: the five levels
4. The core challenges
5. The HuggingFace `kernels` ecosystem
6. Autotuning: search, templates, and LLM generation
7. thvm's UOpt approach (KOpt rewriting)
8. tinygrad: the UOp graph and BEAM
9. TileLang: the tile DSL and carver
10. TPU kernels and JAX Pallas: a different machine
11. A worked example: softmax from naive to speed-of-light
12. The frontier
13. Further reading

---

## 1. What a compute kernel is

A *kernel* is a function that runs on an accelerator (GPU, NPU, or a
SIMD CPU core) across many data elements in parallel.  In machine
learning, every operation a model performs -- a matrix multiply, a
softmax, a layernorm, a convolution -- ultimately runs as one or more
kernels.

The same mathematical operation can be expressed by many different
kernels with the same output and wildly different speed.  A naive
matrix multiply and a tuned one compute identical numbers; the tuned
one can be 20-50x faster.  Kernel engineering is the discipline of
finding the fast one.

Why it matters now:

- **Inference cost.** Serving a large language model is dominated by a
  handful of kernels (attention, matmul, normalization).  A 1.5x
  kernel speedup is a 1.5x cost reduction at scale.
- **On-device models.** Running a model locally (on a laptop or phone)
  means the kernel must fit the consumer accelerator's limits.  A
  faster kernel is the difference between a model that runs and one
  that does not.
- **The hardware-software gap.** New accelerators ship faster than
  compilers learn to use them.  Hand-tuned and autotuned kernels
  close the gap that the vendor compiler leaves open.

## 2. The GPU execution model (the part you must internalize)

Every kernel optimization is a consequence of two facts about GPUs:
they have a strict thread hierarchy, and they have a steep memory
hierarchy.  If you understand these two, most optimizations become
obvious.

### The thread hierarchy

A GPU dispatch launches a *grid* of threads.  The grid is partitioned:

- **Thread**: runs one instance of the kernel body.
- **Simdgroup** (NVIDIA calls it a *warp*; Apple a *simdgroup*): 32
  threads that execute in lockstep -- one instruction, 32 lanes.  All
  32 lanes run the same instruction every cycle; a branch that some
  lanes take and others do not is run *both ways* with inactive lanes
  masked off (this is "divergence", and it is expensive).
- **Threadgroup** (NVIDIA: *thread block*): a set of simdgroups,
  typically 256-1024 threads, that share a fast scratchpad memory and
  can synchronize with a barrier.
- **Grid**: all threadgroups for the dispatch.

The single most important consequence: **the 32 lanes of a simdgroup
can cooperate for free.**  Apple and NVIDIA expose *simdgroup
collectives* -- `simd_sum`, `simd_max`, `simd_shuffle` -- that
combine values across all 32 lanes in a handful of cycles.  A
reduction (sum, max) that uses `simd_sum` instead of a scalar loop is
the first optimization you reach for.

### The memory hierarchy

Speed and size trade off, steeply:

| Level | Latency | Bandwidth | Size | Scope |
|---|---|---|---|---|
| Registers | ~1 cycle | enormous | ~256/thread | per thread |
| Threadgroup / shared memory | ~tens of cycles | high | ~32-64 KB | per threadgroup |
| L2 / system cache | ~hundreds of cycles | ~15-20 B/cycle | a few MB | device |
| Global / device memory | ~hundreds of cycles | ~32 B/cycle/core | gigabytes | device |

(Numbers from `philipturner/metal-benchmarks` for Apple M-series; the
*shape* is the same on every GPU.)

Two regimes follow:

- **Compute-bound**: the kernel does many arithmetic operations per
  byte of memory it touches (matmul of large matrices).  The limit is
  the arithmetic throughput.  You optimize by keeping the arithmetic
  units fed: tiling, register accumulators, the matrix-multiply
  hardware instruction.
- **Memory-bound**: the kernel does few operations per byte
  (elementwise add, normalization).  The limit is memory bandwidth.
  You optimize by touching memory as efficiently as possible:
  coalesced and vectorized loads, fusing operations so an
  intermediate never leaves a register.

The *roofline model* makes this precise: plot achievable FLOP/s
against *arithmetic intensity* (FLOPs per byte).  A kernel is either
under the diagonal (memory-bound) or under the flat ceiling
(compute-bound).  Knowing which tells you what to optimize and what
ceiling to expect.

### Apple-specific notes

The corpus in this repo targets Apple M-series GPUs.  Specifics that
shape the kernels:

- Simdgroup width is 32, same as an NVIDIA warp.
- `simdgroup_matrix<float, 8, 8>` is the matrix-multiply-accumulate
  (MMA) instruction: an 8x8x8 fragment multiply in ~17-18 cycles.  It
  is the closest Apple equivalent to NVIDIA tensor cores, but it is a
  simdgroup instruction, not a separate accelerator.
- Unified memory: the GPU and CPU share physical memory, so there is
  no copy-in / copy-out for small kernels.
- No `cp.async`, no Tensor Memory Accelerator (TMA), no warp
  specialization.  Software pipelining (overlapping loads with
  compute) is done by hand with multiple threadgroup-memory buffers.

## 3. How kernels are written: the five levels

Kernel code can be produced at five levels of abstraction.  Higher
levels are more productive; lower levels give more control.  Every
tool in this document sits at one or two of these levels.

### Level 1: hand-written shader source

You write the kernel directly in the GPU's shading language: Metal
Shading Language (MSL) for Apple, CUDA C++ for NVIDIA, HIP for AMD.
Maximum control, maximum effort.  This is what you fall back to when
the compiler cannot express the structure you need (online softmax,
fused attention, a hand-pipelined matmul).

The raw-MSL track in this repo is Level 1: you write `kernel.metal`.

### Level 2: a tile DSL

A domain-specific language one notch above the shader.  You describe
the kernel in terms of *tiles* -- blocks of the output computed by one
threadgroup -- and the DSL handles index arithmetic, memory movement,
and synchronization.  TileLang and Triton are Level 2.

You still choose the tiling strategy; the DSL spares you the
bookkeeping.

### Level 3: a graph compiler with an explicit rewrite IR

The kernel is not written at all.  You write a tensor program (the
math), and a compiler lowers it to an intermediate representation
(IR), applies rewrite rules, and emits shader source.  tinygrad's
UOp graph and thvm's UOp DAG are Level 3.  The "kernel" the user sees
is a few lines of array math; the compiler decides the loop structure.

### Level 4: an autotuner over Level 2 or 3

The compiler does not just lower the program -- it generates many
candidate kernels (different tile sizes, unroll factors, thread
counts), benchmarks them, and keeps the fastest.  tinygrad's BEAM,
TileLang's autotuner, and thvm's `kernel_opts_propose` loop are
Level 4.  Autotuning sits *on top of* a Level 2 or 3 system; it does
not replace it.

### Level 5: an LLM writes the kernel

A language model generates kernel source from a description, a
reference implementation, and hardware hints.  A harness compiles it,
checks correctness, times it, and feeds errors back for another
attempt.  Gimlet Labs' agentic swarm and the HuggingFace "Humanity's
Last Hackathon" are Level 5.  The model usually emits Level 1 (raw
shader) or Level 2 (a tile DSL).

The levels are not a ladder you climb and discard.  A production
stack uses several: a graph compiler (Level 3) for the common case, an
autotuner (Level 4) to tune the hot kernels, and hand-written shaders
(Level 1) for the few kernels no compiler handles well.

## 4. The core challenges

Why is this hard?  Six recurring difficulties.

### 4.1 The search space is enormous and discontinuous

For a single matmul, the choices include: threadgroup tile shape (M, N,
K), how the tile maps to simdgroups, register accumulator shape,
vector load width, whether to use the MMA instruction, threadgroup-
memory layout, loop unroll factors, software-pipeline depth.  That is
a combinatorial space of thousands of candidates, and the performance
surface is *not smooth*: a tile of 64x64 can be 2x faster than 63x63
because 64 divides the simdgroup geometry and 63 does not.  You cannot
gradient-descend it; you search.

### 4.2 Memory, not arithmetic, is usually the bottleneck

Beginners optimize the arithmetic.  Almost always the kernel is
waiting on memory.  The wins come from: coalescing (adjacent threads
read adjacent addresses, so one wide transaction serves the
simdgroup), vectorization (`float4` loads move 16 bytes per
instruction), staging through fast threadgroup memory, and *fusion*
(computing a chain of operations without writing the intermediate to
global memory).

### 4.3 Occupancy is a balancing act

The GPU hides memory latency by having many simdgroups in flight: when
one stalls on a load, another runs.  This requires *occupancy* -- many
resident threadgroups.  But each thread has a fixed register budget
and each threadgroup a fixed scratchpad budget.  A kernel that uses
more registers per thread (a bigger accumulator tile) does more work
per thread but allows fewer threads resident, so it hides less
latency.  The optimum is a saddle point you find by measuring.

### 4.4 Dispatch overhead dominates small kernels

Launching a kernel costs the CPU something: encoding commands,
submitting to the GPU queue, waiting for completion.  On a sub-100-
microsecond kernel that fixed overhead -- often 100-200 microseconds
-- *exceeds the kernel's runtime*.  A model that runs hundreds of
tiny kernels per inference step is *dispatch-bound*: the GPU sits
idle.  The fix is fusion (fewer, bigger kernels) and command batching.
This also makes small kernels hard to *measure*: see
[profiling.md](profiling.md).

### 4.5 Numerical stability constrains the algorithm

You cannot freely reorder floating-point arithmetic.  Softmax must
subtract the row maximum before exponentiating or it overflows.  A
parallel reduction sums in a different order than a sequential one, so
the result differs in the last bits -- which is fine, until a test
checks bit-exactness.  Sentinel values for out-of-bounds lanes must be
chosen carefully: `-FLT_MAX` is safe in an online softmax recurrence
where `-INFINITY` produces `NaN`.  (See [pitfalls.md](pitfalls.md).)

### 4.6 Portability is mostly a myth

A kernel tuned for one GPU generation is rarely optimal on the next:
the simdgroup width, the scratchpad size, the cache hierarchy, the
matrix instruction all change.  CUDA kernels do not run on Apple GPUs
at all.  The realistic goal is a *parameterized* kernel plus a
per-architecture table or autotuner that picks parameters -- not one
kernel that is fast everywhere.

## 5. The HuggingFace `kernels` ecosystem

The challenges above are about *making* a fast kernel.  HuggingFace's
`kernels` project addresses a different problem: *distributing* one.

### The distribution problem

A compiled GPU kernel is not portable.  It is built against a specific
CUDA version, a specific PyTorch C++ ABI, a specific Python version, a
specific operating system and architecture.  Ship a kernel as a normal
Python package and a user on a slightly different stack must rebuild
it from source -- a process that can take hours and often fails.  When
the user later upgrades PyTorch, the locally built kernel breaks.

This is "dependency hell" for compute kernels, and it has held back
the adoption of fast custom kernels: a kernel that is 2x faster is
worthless if the user cannot install it.

### The Kernel Hub

HuggingFace's answer makes a *kernel* a first-class repository type on
the Hub, alongside models and datasets.  A kernel repository holds
pre-built binaries for a wide matrix of (backend, OS, architecture,
PyTorch version, Python version).  The `kernels` Python package
downloads the build that matches the running system -- no local
compile.

Hub kernels are designed to be:

- **Portable**: loadable from a path outside `PYTHONPATH`.
- **Unique**: multiple versions of the same kernel can coexist in one
  Python process, so two libraries depending on different versions do
  not collide.
- **Compatible**: builds cover all recent Python and PyTorch
  configurations.

### The API

Loading a kernel:

```python
import torch
from kernels import get_kernel

# fetch the build matching this machine; no local compile
activation = get_kernel("kernels-community/activation", version=1)

x = torch.randn((10, 10), dtype=torch.float16, device="cuda")
y = torch.empty_like(x)
activation.gelu_fast(y, x)
```

`version=1` pins the major version: within a `v1` branch the API and
the set of supported PyTorch builds may never break.  `has_kernel(repo,
version)` tests up-front whether the current machine is supported.
`get_loaded_kernels()` introspects what has been loaded.

A kernel repository can also ship *layers* -- drop-in replacements for
`torch.nn` modules -- so a model can be accelerated by swapping a
layer, not by rewriting call sites.

### kernel-builder

The other half of the project is `kernel-builder`: you provide kernel
source in a fixed directory layout plus a declarative build config,
and the builder produces the full matrix of compiled artifacts
reproducibly.  It uses Nix to pin the build environment exactly, so a
build is bit-reproducible and not contaminated by the host system.

For Metal specifically (`kernel-builder` Metal support), the build
machine needs Xcode 26.x, the separately-downloaded Metal Toolchain
(`xcodebuild -downloadComponent MetalToolchain`), and the Nix sandbox
relaxed so the derivation can reach Xcode.  Apple ships macOS versions
fast and the toolchain tracks the latest major version.

### Relation to this repo and the hackathon

The HuggingFace "Humanity's Last Hackathon" (launched May 2026) is a
Level-5 competition: participants use an LLM agent to write the fastest
Mac Metal kernel for a benchmark task, and submissions are scored on a
leaderboard.  The framing -- "judged on context, not code" -- is the
thesis that the *context you give the agent* (hardware facts, the
reference kernel, profiling feedback) matters more than the agent.

The `kernels` ecosystem is the distribution channel for the kernels
such an effort produces: write a fast Metal kernel, package it with
`kernel-builder`, publish it to the Hub, and any `transformers` user
on a Mac gets it with `get_kernel`.  This repo's `docs/auto_metal_kernels/`
knowledge base and score harness are the *production* side of the same
loop -- write and verify the kernel before it is published.

## 6. Autotuning: search, templates, and LLM generation

Once you can express a kernel, you still must pick its parameters.
Three families of autotuner, with different cost/quality tradeoffs.

### 6.1 Search-based

Enumerate candidates, benchmark each, keep the fastest.  tinygrad's
BEAM and TileLang's autotuner are search-based.

- **Strength**: makes no assumptions; finds non-obvious winners.
- **Weakness**: a benchmark per candidate.  On Metal, where compiling
  one candidate costs ~100 ms, a beam of width 4 over 30 candidates
  per kernel runs into minutes.  The search is also *local*: it does
  not propose tile shapes from hardware knowledge, so it spends
  measurements on candidates that a roofline calculation would have
  rejected.

### 6.2 Template-based

A parameterized template per pattern (matmul, reduction, attention)
plus a hardware model.  Given a problem shape and an architecture, the
template enumerates plausible configurations and a roofline cost model
ranks them; only the top few are benchmarked.  TileLang's *carver* is
the reference design.

- **Strength**: shape-aware.  Returns "the 5 configs most likely to be
  good" without benchmarking 50.
- **Weakness**: the cost model is hand-built and can be 30-50% wrong
  for fused or unusual shapes; a template must be written per pattern.

### 6.3 Shape tables (the MLX pattern)

No search at all.  Hard-code a small table of (shape bucket -> config)
per operation; switch on it at runtime.  MLX does this -- its
`matmul.cpp` picks between `gemm`, `gemm_steel`, and `gemv` from a
table keyed on (M, N, K) and divisibility.

- **Strength**: zero runtime overhead; trivially maintainable; ships
  in a day.  For a handful of high-value kernels this gets ~80% of the
  achievable performance for ~1% of the engineering.
- **Weakness**: a shape the table does not cover hits a slow fallback;
  a new hardware generation needs a re-tuned table.

### 6.4 LLM-based (Level 5)

A language model writes the kernel; a harness compiles, correctness-
checks, and times it, then feeds structured errors back for a retry.
Gimlet Labs' published agentic swarm runs *multiple* frontier models
in parallel, has a deterministic supervisor pick the fastest correct
candidate, and gives failed candidates structured feedback (compile
error, correctness diff, latency) for up to 5 retries.  Reported
result: a geomean 1.87x speedup over PyTorch MPS across the
KernelBench v0 suite of 215 modules.

- **Strength**: not limited to a fixed search space; the model can
  restructure the algorithm (fuse two ops, switch to an online
  formulation).
- **Weakness**: one-shot generation rarely succeeds -- the error-
  feedback loop is essential; correctness must be machine-checked,
  never trusted; the published baselines show that without the loop,
  ~80% of LLM-generated kernels are *slower* than the framework.

The four families compose.  An LLM (Level 5) can emit a parameterized
template and hand the parameters to a search (Level 4); a search can
be seeded by a template's roofline ranking instead of a flat
enumeration.

## 7. thvm's UOpt approach (KOpt rewriting)

thvm is a Level-3 system: the user writes tensor math, and a compiler
lowers it to a *UOp DAG* -- a directed acyclic graph of micro-
operations -- which is then rewritten and rendered to shader source.

### The UOp DAG

A kernel is a DAG rooted at a `STORE(buffer, address, value)` node.
The interesting node types:

- `BUFFER(scope, dtype, dims)` -- a memory region (global, threadgroup,
  or register).
- `RANGE(axis_id, axis_type, extent)` -- a loop axis.  `axis_type` is
  `LOOP` (a plain loop), `REDUCE` (a reduction axis), or one of the
  parallel types (`GLOBAL`, `LOCAL`, `UPCAST`, `UNROLL`,
  `GROUP_REDUCE`).
- `INDEX_E(buffer, address)` -- a load.
- `IADD`, `IMUL`, ... -- integer address arithmetic.
- `ADD`, `MUL`, `EXP2`, `RECIP`, ... -- floating-point compute.
- `REDUCE(value, axis, kind)` -- a sum or max reduction.
- `OPT(target, kind, factor)` -- an *annotation* attached to a node
  that tells the renderer to emit a specialized form.

A canonical matrix multiply DAG, in this form:

```
STORE(C, m*N + n,
      REDUCE(MUL(INDEX_E(A, m*K + k),
                 INDEX_E(B, k*N + n)),
             axis=k, SUM))
```

### KOpt: kernel optimizations as DAG rewrites

A *KOpt* is an atomic rewrite of the DAG, identified by a triple
`(opcode, axis, argument)`.  Each KOpt is a structural transformation
in `src/uop/apply_opt_dag.c`.  The vocabulary:

| KOpt | Effect |
|---|---|
| `TC` | wrap a matmul-shaped REDUCE with `OPT(_, TC, tile)` -- the renderer emits the `simdgroup_matrix` MMA instruction |
| `GLOBAL` | flip a `LOOP` axis to `GLOBAL` -- one threadgroup per index of that axis |
| `UPCAST` / `UNROLL` / `LOCAL` / `GROUP` / `GROUPTOP` | split an axis into an outer loop and an inner axis of the named type (vectorization, unrolling, thread-parallelism, reduction splitting) |
| `SWAP` | exchange two axis ids -- loop reordering |
| `FAST_MATH` | wrap `EXP2`/`LOG2`/`SQRT` with `OPT(_, FAST_MATH)` -- emit the fast approximate intrinsic |
| `SIMD_REDUCE` | wrap a `REDUCE` with `OPT(_, SIMD_REDUCE)` -- emit a `simd_sum`/`simd_max` collective |
| `VEC_LOAD` | wrap a contiguous `INDEX_E` with `OPT(_, VEC_LOAD, width)` -- emit a vectorized `float4`/`float8` load |

This design has a specific virtue: a KOpt is a *value*, not code.  The
sequence of KOpts applied to a kernel is data that can be enumerated,
searched, serialized, cross-validated, and replayed.  The autotuner is
then just: `kernel_opts_propose` enumerates candidate KOpt triples,
`kernel_apply_opt` applies one to the DAG, the renderer emits MSL, and
a benchmark loop picks the winner (`py/examples/matmul_beam_loop.py`).

Because the KOpt set is closed and small, the same rewrites can be
expressed in two independent engines -- the C `apply_opt_dag.c` and a
symbolic rule set -- and cross-validated against each other: apply a
KOpt both ways, snapshot both DAGs, assert structural equality.  A
divergence is a bug in one engine.

### What thvm's renderer can and cannot emit

The KOpt vocabulary is exactly the set of transformations thvm's
renderer knows how to lower.  Three were ported from MLX
(`FAST_MATH`, `SIMD_REDUCE`, `VEC_LOAD`).  What it does *not* yet
emit: two-stage threadgroup reductions for long rows, online softmax,
a multi-fragment MMA accumulator, software-pipelined matmul, Welford-
style fused mean/variance, cross-operation fusion.  For those, you
drop to Level 1 (raw MSL), beat the baseline, and -- optionally --
port the technique back as a new `OPT_*` annotation.  See
[mlx_reference.md](mlx_reference.md) for the gap list.

### Where thvm sits

thvm autotune is at rough parity with tinygrad BEAM on per-kernel
speed for the feed-forward network ladder that has been measured
(both land in the 1.0-1.6x range over their own naive baselines).
The remaining gap is structural: thvm currently emits more kernels
for the same network because it fuses fewer operation boundaries.
The autotune campaign notes (`docs/plans/autotune_beam_profile.md`)
quantify this with an empirical kernel-count model.

## 8. tinygrad: the UOp graph and BEAM

tinygrad is the most direct point of comparison: it is also a Level-3
graph compiler with a Level-4 search, and it targets Metal as a
first-class backend.

### The UOp graph

tinygrad lowers every tensor program to a graph of `UOp` nodes -- a
small, uniform IR.  The entire compiler is, in essence, a *graph
rewrite engine*: a large set of rewrite rules (`PatternMatcher`)
repeatedly fires on the graph until it reaches a fixed point.
Lowering, optimization, and even constant folding are all rewrite
rules over the same IR.  This is an unusually pure design -- there is
one IR and one mechanism -- and it is why tinygrad is small enough to
read end to end.

### The Opt vocabulary and BEAM

tinygrad's autotuner searches over `Opt` primitives -- the direct
ancestor of thvm's KOpt vocabulary:

- `OptOps.UPCAST` -- unroll a loop into vector-width registers.
- `OptOps.UNROLL` -- unroll a reduction axis.
- `OptOps.LOCAL` -- map an axis to threadgroup-parallel threads.
- `OptOps.GROUP` -- split a reduction across a threadgroup.
- `OptOps.TC` -- use the matrix-multiply instruction.

`Kernel.apply_opt` applies one to the kernel's loop structure.  The
search, `beam_search` in `engine/search.py`, keeps a beam of the best
`N` partial optimization sequences, extends each by one `Opt`,
benchmarks, and prunes.  `BEAM=4` is a typical width; `MCTS=1`
switches to Monte Carlo Tree Search for wider exploration.

### Strengths and limits on Metal

tinygrad's METAL backend is mature and `OptOps.TC` reaches
`simdgroup_matrix`.  The limits are the generic ones of search-based
autotuning (Section 6.1): the search is local, Metal compile cost is
~100 ms per candidate, and at modest beam widths the search can
*overfit* to a single warmup measurement -- in this repo's matmul
ablation, `BEAM=4` was occasionally *slower* than no search because
the chosen candidate was tuned to noise.  The lesson: a search is only
as good as its benchmark, and Metal's measurement noise floor is real
(see [profiling.md](profiling.md)).

Where to read: `tinygrad/codegen/kernel.py` (the `Opt` vocabulary),
`tinygrad/engine/search.py` (BEAM and MCTS),
`tinygrad/runtime/ops_metal.py` (the Metal backend).

## 9. TileLang: the tile DSL and carver

TileLang is a Level-2 system: a Python DSL for tile-level kernels,
built on Apache TVM's Tensor IR.  It is the reference design for
template-based autotuning.

### The DSL

TileLang exposes a compact vocabulary, all under `T.*`: allocation
(`T.alloc_shared`, `T.alloc_fragment`), control flow
(`T.Kernel(grid, threads=N)`, `T.Pipelined(num_stages=K)`,
`T.Parallel`), and tile operations (`T.copy`, `T.gemm`, `T.reduce`,
`T.fill`).  You write a kernel as a sequence of tile operations over
explicitly allocated shared-memory and register tiles; TileLang's
~60 IR passes lower that to MMA / WGMMA / `cp.async` intrinsics with
shared-memory layout swizzles and software pipelining.

The semantic split is worth absorbing even if you never use TileLang:
*allocation, kernel-launch shape, loop, tile-operation, annotation*
is a clean taxonomy of what a tile kernel needs.

### Carver

`carver` is TileLang's template-based config generator.  Given a
problem (a matmul of given shape and dtype) and a hardware model (core
count, shared-memory cap, simd width, register budget), it enumerates
plausible (tile, warp, stage, vector-width) configurations and ranks
them by a roofline estimate.  `recommend_hints(problem, arch, top_k=5)`
returns the five most promising configurations -- which a search then
benchmarks, instead of benchmarking fifty.

This is the highest-leverage idea to take from TileLang: candidate
configurations should come from hardware knowledge, not a flat
constant list.  thvm's `kernel_opts_propose` is the place a carver-
shaped generator would slot in.

### What does not port

TileLang's value for an Apple-GPU effort is in the *algorithms*
(carver's roofline enumeration, the autotuner's disk-cache key recipe)
and the *partitioning*, not the code.  TileLang's Metal codegen is
bare -- it emits scalar SIMT MSL with no `simdgroup_matrix` wired up,
and there is no Metal runtime-template directory.  Its strength is the
NVIDIA Hopper/Blackwell path (TMA, warp specialization, WGMMA), none
of which exists on Apple hardware.  See `docs/plans/tilelang_scout.md`
for the full port analysis.

## 10. TPU kernels and JAX Pallas: a different machine

Everything so far assumed a GPU: a grid of parallel threads, simdgroups,
a scratchpad, occupancy.  Google's Tensor Processing Unit (TPU) is a
different machine, and the contrast sharpens what is GPU-specific and
what is fundamental.

### Pallas: JAX's kernel language

Pallas is JAX's extension for writing custom kernels.  In this
document's taxonomy it is a Level-2 tile DSL, the JAX-native peer of
Triton and TileLang.  You write a kernel body that operates on `Ref`s
(mutable references to memory) with `pl.load` / `pl.store`, declare a
`grid` ("kernels in a loop"), and give a `BlockSpec` per input that
says how to chunk it into blocks.  One front-end, two lowerings:
**Mosaic GPU** for the GPU backend, and **Mosaic** -- Google's TPU
compiler -- for the TPU backend.

### The TPU is a systolic-array machine

A GPU is a throughput machine made of many parallel threads.  A TPU
is built around the **MXU** (Matrix Unit), a systolic array that
performs on the order of 65,000 multiply-accumulates per cycle in
current generations.  Matrix multiply is the *native* primitive --
not an instruction you reach for, the thing the chip is.  There are
three compute units:

- **Scalar unit** -- control flow and scalar arithmetic, on its own
  scalar memory (SMEM).
- **Vector unit (VPU)** -- the bulk of elementwise and reduction work,
  on 2D vector registers (8x128 for 32-bit values).
- **Matrix unit (MXU)** -- matrix multiplies, executed *asynchronously*
  to the main instruction stream.

There are no warps, no simdgroups, and no thread divergence.

### The grid is a sequential loop, not parallel threads

This is the deepest difference.  A GPU grid is thousands of threads
running at once.  A Pallas-TPU grid executes **sequentially, in
lexicographic order**, on one core.  You think sequentially.
Consecutive grid iterations writing the same output slice are
race-free by construction -- but ordering them correctly is now your
job, not the hardware's.

### Memory is explicit, DMA-staged, and compiler-pipelined

The hierarchy is HBM (high-bandwidth main memory, high latency) ->
VMEM (vector memory, a 16-95 MB scratchpad) -> vector registers.  The
kernel body receives `Ref`s that already point into VMEM or SMEM; the
Mosaic compiler inserts the HBM<->VMEM DMA transfers.  Crucially it
also **pipelines** them automatically: the HBM->VMEM copy for grid
iteration N+1 overlaps the compute of iteration N, and when
consecutive iterations share an input slice the transfer is skipped.

### Tiling and alignment are rigid

The last two array dimensions are mapped onto the 8x128 vector
register, so block shapes must be divisible by 8 and 128 on those
dimensions.  Reshapes and reductions that touch them are expensive or
unsupported.  Patterns "unnatural to the hardware" fall back to slow
software emulation.

### What this teaches

| | GPU | TPU |
|---|---|---|
| Parallelism | many concurrent simdgroups | a systolic array + wide vectors |
| The grid | thousands of parallel threads | a sequential loop, one core |
| Latency hiding | occupancy (swap in another simdgroup) | explicit software pipelining |
| Hazards | thread divergence | tiling/alignment to 8x128 |
| Fast memory | threadgroup scratchpad you manage | VMEM, DMA managed by the compiler |
| Native primitive | fused multiply-add lanes | the matmul itself |

A GPU kernel writer's skill is occupancy balancing and avoiding
divergence.  A TPU kernel writer's skill is pipelining and keeping the
MXU fed.  Both are answers to the same question -- how do you hide
memory latency -- with opposite mechanisms: the GPU overlaps *many
threads*, the TPU overlaps *stages of one loop*.

Relevant to thvm: its UOp `RANGE` axis types (`GLOBAL`, `LOCAL`, and
the rest) encode a GPU thread-grid model.  A TPU target would not
reuse them -- it would need a sequential-grid plus DMA-pipeline model,
which is why a "portable" kernel IR is so hard: the machines do not
agree on what a kernel *is*.

## 11. A worked example: softmax from naive to speed-of-light

Softmax over the rows of an `R x C` matrix:
`out[r,c] = exp(x[r,c] - max_c x[r,:]) / sum_c exp(x[r,:] - max_c)`.
It is memory-bound (two reads and a write of the matrix, little
arithmetic) and it is a reduction followed by a broadcast.  Watch the
techniques accumulate.

**Stage 0 -- naive.** One thread per row; the thread loops over the
row's `C` elements three times (find max, sum the exponentials,
normalize).  Correct, and roughly 5x slower than the framework: only
`R` threads run, so for a 32-row input only 32 threads are busy on a
40-core GPU, and each does `3C` scalar global-memory reads.

**Stage 1 -- one threadgroup per row, simdgroup reduce.** Assign a
whole threadgroup to a row.  Each thread reads a strided slice of the
row, finds its local maximum, then `simd_max` combines the 32 lanes
of each simdgroup in ~6-9 cycles.  The reduction is now parallel, not
a scalar loop.  This is the single biggest step.

**Stage 2 -- two-stage threadgroup reduction.** A row longer than one
simdgroup's worth of data needs a second stage: each simdgroup writes
its partial maximum to a small threadgroup-memory array, a barrier,
then the first simdgroup `simd_max`-reduces the partials.  Now an
arbitrarily long row reduces correctly with full threadgroup
parallelism.

**Stage 3 -- vectorized loads.** Each thread reads `N_READS` (4 or 8)
consecutive floats with a single `float4` vector load instead of
scalar loads.  Fewer load instructions, fewer threads needed, more
register budget freed.

**Stage 4 -- fast intrinsic.** Replace `exp` with `fast::exp`: ~10x
faster, ~2 ULP error -- free for softmax.

**Stage 5 -- the sentinel.** Out-of-bounds lanes in the final partial
must contribute a value that does not corrupt the reduction.  Use
`-FLT_MAX`, not `-INFINITY`: the online-softmax recurrence
`normalizer *= exp(prev_max - new_max)` computes `exp(NaN)` if both
are `-INFINITY`, and `NaN` poisons the whole row.

**Stage 6 -- structural.** Beyond copying the framework: pack multiple
rows into one threadgroup so the dispatched threadgroup count halves
(less per-launch overhead), or, for short rows, use a single
simdgroup per row with no threadgroup memory and no barrier at all,
packing several simdgroups per threadgroup to spread across cores.

Stages 1-5 reproduce what MLX's softmax kernel does; Stage 6 is where
a custom kernel can pull ahead of the framework, because the
framework's kernel is templated on a fixed `N_READS` and cannot
restructure its threadgroup mapping per shape.  The agent runs in this
repo found Stage-6 multi-row packing worth a further 5-9%.

The general shape of the climb -- parallelize the reduction, stage
through fast memory, vectorize, use the fast intrinsic, then
restructure -- is the same for layernorm, for reductions, for
attention.  Only the arithmetic in the middle changes.

## 12. The frontier

What is still hard, and where the field is moving.

- **Fusion across operations.** The largest remaining wins are not
  inside a kernel but *between* kernels: computing softmax-then-matmul
  (attention) or layernorm-then-linear without the intermediate ever
  reaching global memory.  Graph compilers fuse elementwise chains
  well and reduction-bearing chains poorly.  Flash Attention is the
  famous hand-fused example; making a compiler discover it is open.
- **Attention is its own subfield.** Block-wise tiling, online
  softmax, no full-score-matrix materialization, and now variants for
  long context, sparsity, and paged key-value caches.  It is the
  single most optimized kernel in modern ML and a moving target.
- **The measurement problem.** Autotuning and LLM scoring are only as
  good as the benchmark.  Sub-100-microsecond kernels on a GPU with
  dynamic clocking are genuinely hard to measure: dispatch overhead,
  thermal state, and clock ramp can swing a measurement 2-4x with the
  kernel unchanged.  An autotuner that searches on noisy measurements
  overfits to noise.  Honest GPU-timestamp measurement on a quiesced
  machine is a prerequisite, not a detail.
- **LLM kernel generation is improving fast but unreliable.** The
  KernelBench results show frontier models produce a *slower*-than-
  framework kernel the majority of the time on the first try.  The
  open work is in the harness: structured error feedback, multi-model
  ensembles, retrieval of a matching reference kernel, and good
  hardware context -- the "judged on context, not code" thesis.
- **Distribution is being solved.** The HuggingFace Kernel Hub is a
  genuine answer to the "I cannot install your fast kernel" problem.
  As it matures, the incentive to produce fast kernels rises, because
  a fast kernel can now actually reach users.

The throughline: producing a fast kernel, measuring it honestly, and
shipping it are three separate hard problems, and a complete system
needs an answer to all three.

## 13. Further reading

In this repository:

- [agent_brief.md](agent_brief.md) -- the operational loop: write a
  kernel, score it, iterate.
- [mlx_reference.md](mlx_reference.md) -- the techniques MLX's kernels
  use, with source line references.
- [autotuning.md](autotuning.md) -- the KOpt vocabulary and the
  `kernel_apply_opt` mechanics in detail.
- [profiling.md](profiling.md) -- honest GPU measurement and the
  noise floor.
- [pitfalls.md](pitfalls.md) -- the recurring traps.
- `docs/plans/tilelang_scout.md` -- the full TileLang port analysis.
- `docs/plans/autotune_beam_profile.md` -- thvm vs tinygrad autotune,
  the empirical kernel-count model.
- `bench/metal-problems/AUTOTUNE.md` and `STANDARD_TOOLS.md` -- the
  autotuning-tool landscape and the standard-tool baselines.

External:

- The HuggingFace `kernels` documentation and the Kernel Hub at
  `huggingface.co/kernels`.
- tinygrad source: `codegen/kernel.py`, `engine/search.py`.
- TileLang and its `carver` subdirectory.
- JAX Pallas docs (`docs.jax.dev/en/latest/pallas`) -- the kernel
  language; the TPU-details and pipelining pages for the TPU model.
- `philipturner/metal-benchmarks` -- Apple GPU microarchitecture
  cycle counts, the ground truth for any roofline estimate.
- The Flash Attention papers -- the canonical fused-kernel case study.
- KernelBench (`ScalingIntelligence/KernelBench`) -- the LLM-kernel-
  generation benchmark and its sobering baselines.
