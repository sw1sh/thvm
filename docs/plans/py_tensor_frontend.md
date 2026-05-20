# Python Tensor frontend -- a drop-in tinygrad replacement

Goal: a `thvm` Python package whose public surface mirrors tinygrad's,
so `import tinygrad` -> `import thvm` runs existing model code mostly
unchanged.  This makes cross-validation structural -- the *same*
network file runs on thvm-Metal, thvm-CUDA, and tinygrad; the diff
(numerics, kernel count, memory plan, autotune wins, rendered source)
is a one-import-line swap.

Today the `py.thvm` bridge stops at the low-level UOp-DAG builder
(`py_uop_*`) + the autotune surface (`py_kernel_*`).  Networks are
hand-built UOp DAGs -- a dead end past a couple of layers.  The WL
frontend (`wl/THVMLink/`) has the full Tensor/nn surface but is
Wolfram-idiomatic and not Python, so it can't be the cross-test base.

## The interface / engine boundary

This is a **port of tinygrad's `tensor.py` + `nn/`**, not of
`NN.wl`.  `NN.wl` is at most a crib for how thvm lowers a given op
(`tanh` = the EXP2 chain; `matmul` = RESHAPE+EXPAND+MUL+REDUCE).  The
API surface, names, and signatures come from tinygrad.

Draw the line cleanly:

- **Port (interface)**: the `Tensor` op surface -- `__add__`,
  `__matmul__`, `reshape`/`permute`/`pad`/`shrink`/`flip`, reductions,
  `softmax`, activations, `conv2d`; the `Tensor.backward()` /
  `realize()` / `numpy()` entry points; `nn.Linear`/`Conv2d`/
  `BatchNorm`/`LayerNorm`/...; `optim.SGD`/`Adam`.  Match tinygrad's
  signatures and semantics.
- **Skip (engine -- thvm C already has it)**: tinygrad's Python lazy
  graph and its Python autodiff.  thvm owns the lazy graph,
  `uop_grad`, materialize, kernelize, schedule, memory planner,
  autotune, and backend renderers -- all in C.  So each ported
  method's *body* builds thvm's `TAG_TEN` graph through the bridge;
  `.backward()` routes to C `uop_grad`.  We are not re-implementing
  an autodiff engine -- we are wrapping one.

This is why the port is thinner than tinygrad's `tensor.py`: that
file is heavy *because* it carries the lazy graph + autograd.  Strip
those (thvm's C owns them) and what is left is the surface.  This is
`feedback_tinygrad_is_spec` taken to its end -- the frontend literally
is tinygrad's; the implementation is thvm's C.  (tinygrad is MIT --
port with attribution + cite file:line.)

Honest bar: **"mostly works"**, not 100%.  Common model-code paths
(elementwise, matmul, conv, norms, attention, the optimizers) work;
tinygrad internals and exotic ops will not.

## Phase 0 -- env-var rename (prep refactor)

For a drop-in replacement, the knobs a user sets must share tinygrad's
namespace.  ~60 `THVM_*` env vars exist; the **comparative** subset --
user-facing, tinygrad-paralleling -- drops the `THVM_` prefix.  The
~50 internal dump/trace/bisection/cap knobs KEEP `THVM_` (they are not
comparison surface, and bare `GC`/`TILE`/`THREADS` would collide).

Comparative subset (signed off):

| current | new | tinygrad analogue |
|---|---|---|
| `THVM_BACKEND` | `DEV` | `DEV` -- exact (`DEV=CUDA`/`METAL`/`CPU`) |
| `THVM_AUTOTUNE` | `AUTOTUNE` | -- (tinygrad folds on/off into `BEAM`) |
| `THVM_AUTOTUNE_BEAM` | `BEAM` | `BEAM` -- exact |
| `THVM_AUTOTUNE_RUNS` | `BEAM_RUNS` | -- |
| `THVM_AUTOTUNE_DEPTH` | `AUTOTUNE_DEPTH` | -- |
| `THVM_AUTOTUNE_CACHE` / `_DIR` / `_DISABLE_CACHE` | `AUTOTUNE_CACHE` / `_DIR` / `_DISABLE` | `CACHELEVEL` / `CACHEDB` (loose) |
| `THVM_HAND_CODED_OPTS` | `NOOPT` + `HAND_CODED_OPTS` alias | `NOOPT` -- exact (inverse sense) |

Two resolved decisions:

- **Device selection** is `DEV` (tinygrad's `_DEV` ContextVar, `helpers.py:231`).
  Current tinygrad deprecated the per-device `CUDA=1` flags
  (`device.py:49`); `DEV=CUDA` is the live form, so thvm matches that
  exactly -- a tinygrad `DEV=CUDA python model.py` invocation carries
  over verbatim.  Values map straight onto today's `THVM_BACKEND`
  strings (`cuda`/`metal`/`cpu`); the `DEV=CUDA:renderer` suffix form
  is parsed-but-ignored for now.
- **Opt-disable** exposes both senses: `NOOPT=1` in tinygrad's exact
  inverse sense (disables hand-coded opts; `helpers.py:231`) for
  drop-in, and `HAND_CODED_OPTS` as the positive-sense alias for thvm
  scripts.  `NOOPT` wins if both are set.

Everything else (`THVM_DUMP_*`, `THVM_*_TRACE`, `THVM_CPU_UOP_WALK`,
`THVM_BUFFERIZE_*`, `THVM_METAL_*`, `THVM_GC*`, `THVM_*_BYTES`, ...)
stays prefixed.  Rename touches `getenv` sites in `src/`, the docs,
the pod scripts, and the `reference_tinygrad_cuda_pod` memory note.

## Phase 1 -- C bridge

`py.thvm` (`py/csource/thvm_py.c`) today exposes only the *low-level*
UOp-DAG layer: `py_uop_*` builds renderer-facing DAG nodes (BUFFER,
RANGE, INDEX_E, STORE), `py_kernel_*` drives autotune, `py_render_*`
emits MSL/CUDA.  Hand-building UOp DAGs is the dead end.

The high-level tensor-term layer (`TAG_TEN`) is NOT in the py bridge
-- but it already exists in C, and is already wrapped, by the WL
LibraryLink glue `wl/THVMLink/CSource/thvmlink.c` (`thvm_wl_*`, ~198
exports).  Phase 1 is therefore a *port*: add `extern "C"` `py_*`
wrappers over the SAME underlying C entry points -- `thvm_py.c` is a
single-TU `#include "src/thvm.c"`, so the wrappers call `tensor_alloc`
/ `uop_grad` / `thvm_realize` directly, exactly as `py_uop_binary`
already calls `uop_binary`.  No new engine logic; the WL path proves
these functions work.

The surface to mirror:

- tensor create / host I/O -- `tensor_alloc`, host->tensor and
  tensor->host copies, `tensor_shape` -> `py_ten_*`.
- tensor algebra -- the high-level term builders `uop_const`,
  `uop_unary`, `uop_binary`, `uop_reduce`, `uop_cast` and the
  movement ops `uop_reshape`/`permute`/`expand`/`pad`/`shrink`/`flip`.
  These are the `TAG_TEN` builders `NN.wl`'s `TUOp*` wrap -- distinct
  from the existing low-level `py_uop_*` (which build the post-lift
  DAG).
- autodiff -- `uop_grad`, `uop_grad_with_target`, `uop_fwd` ->
  `py_grad`.  Routes `Tensor.backward()` to thvm's real `uop_grad`.
- reduce + dispatch -- `wnf` (reduce to weak normal form) and
  `realize` -> `py_wnf` / `py_realize`.  (`realize_many` was
  briefly exposed as a Python bridge for a dedup experiment;
  removed -- see `project_thvm_realize_many_dead`.  The C-side
  `thvm_realize_many` stays as the multi-root pool-boundary
  primitive used by `TAdam`'s batched ASSIGNs.)
- introspection -- `tens_count`, `tens_table`, `uop_leaf_tids`, plus
  the kernel list/count, memory plan, schedule, and rendered
  MSL/CUDA, so the Phase-4 cross-check list is observable from
  Python.

## Phase 2 -- `thvm/tensor.py`

tinygrad's `Tensor` API surface, ported, with each method's body
routed to the thvm `TAG_TEN` graph via the Phase-1 bridge.  A
`Tensor` is a C-graph handle + operator overloads + ctypes.
`.backward()` -> `py_grad`; `.realize()`/`.numpy()` -> the pipeline.
Broadcasting/shape route to thvm's C shape inference + EXPAND.

## Phase 3 -- `thvm/nn.py` + `thvm/optim.py`

tinygrad's `nn/__init__.py` + `nn/optim.py` ported.  Near drop-in
once Phase 2's `Tensor` surface matches -- the layer classes are
compositions of Tensor ops.  Curriculum-first: the NN-arc layer set
(Linear, Conv2d, LayerNorm, BatchNorm, attention, activations,
losses) before the long tail.

## Phase 4 -- cross-test harness

One network definition; run it thvm-Metal / thvm-CUDA / tinygrad by
swapping the import.  Per layer and end to end, diff: roundtrip
numerics, kernel count + kernelization, memory plan, autotune
efficiency, rendered backend source.  This is the deliverable the
whole CUDA cross-validation arc was building toward.

## Non-goals / risks

- **Not** porting tinygrad's lazy graph or Python autodiff -- thvm's
  C owns those.
- **Not** porting `TFromNet` (Wolfram-`NeuralNetworks` interop -- no
  Python analogue).
- **Decision**: Python becomes the primary high-level frontend; the
  WL frontend goes secondary.  Otherwise every layer is
  double-maintained.  The project's gravity is already all-Python
  (CUDA backend, `brain/`, every cross-val) -- this makes it official.
- Drop-in is "mostly", not total.  The cross-test harness should
  report what diverges, not assume parity.
