# Metal kernels knowledge base

Everything an agent needs to write, autotune, and profile Metal compute
kernels in the thvm tree.  Read [agent_brief.md](agent_brief.md) first
if you were spawned with a task; the rest of this directory is
reference material that brief points to.

## Contents

| File | What it covers |
|---|---|
| [agent_brief.md](agent_brief.md) | Mission, success criteria, iteration loop, where to write the kernel, where to write the report |
| [setup.md](setup.md) | Cloning thvm, building the C + Python artefacts, deps for Mac M-series |
| [python_api.md](python_api.md) | The `py.thvm` ctypes surface: `Thvm`, `Metal`, `K.*` constants |
| [autotuning.md](autotuning.md) | KOpt vocabulary, BEAM loop, `kernel_apply_opt` semantics, `kernel_opts_propose` |
| [msl_writing.md](msl_writing.md) | Writing raw MSL, `Metal.compile_msl` API, `mx.fast.metal_kernel`, AIR / metallib offline path |
| [mlx_reference.md](mlx_reference.md) | What MLX's softmax / matmul / reduce kernels do that's hard to beat -- `simd_max`, `fast::exp`, N_READS, two-stage TG reduce, `-FLT_MAX` sentinel |
| [profiling.md](profiling.md) | `Metal.dispatch_timed`, the score harnesses, GPU vs wall clock, jitter floor |
| [pitfalls.md](pitfalls.md) | NaN traps, alignment, axis_size==C convention, `mx.eval` synchronization, common compile errors |

## Quickstart for an agent landing here cold

1. Read **agent_brief.md** -- it names your problem, your workspace
   directory, and your success criteria.
2. Read **mlx_reference.md** for the exact MLX kernel you're trying to
   beat (line refs into `external/mlx/...`).
3. Edit `kernel.metal` (raw MSL track) or write a Python kernel.py
   (UOp-DAG track) in your workspace.
4. Run `./score.sh` (or `python3 score_one.py kernel.py`) -- 5-line
   machine-parseable output.  Iterate.
5. Stop on first of: success threshold met, iteration cap, time cap.
   Write `RESULTS.md` in your workspace.

## How this is organized

- **The agent's workspace** is `bench/metal-problems/<problem>/runs/<run-id>/`
  or `py/examples/agent_<problem>_<variant>/`.  All edits go there.
- **Reference docs** (this directory) are read-only background; don't
  rewrite them, link to them.
- **The score harnesses** live in `bench/metal-problems/runner/` and
  `py/examples/agent_softmax_msl/score.sh`.  They emit five lines:
  ```
  status=ok|compile_err|correctness_err|runtime_err
  correctness=max_abs:X max_rel:Y
  candidate=p50:Xus p10:Yus
  mlx_baseline=p50:Xus p10:Yus
  speedup_vs_mlx=Kx
  ```

## What this repo already knows about

- **Metal backend** (`docs/metal.md`): Objective-C bridge in
  `src/backend/metal/_.m`, offline `.metal` -> `.air` -> `.metallib`
  via `xcrun`.  No JIT compilation; all MSL is rendered, then offline-
  compiled, then loaded.
- **UOp DAG renderer** (`src/codegen/render_uop.c`): emits MSL for
  `STORE(buf, addr, value)` graphs.  Three KOpts ported from MLX so
  far -- `KOP_FAST_MATH`, `KOP_SIMD_REDUCE`, `KOP_VEC_LOAD` -- via
  `UOP_OPT(_, kind, factor)` annotations on the DAG.
- **Autotune surface** (`src/codegen/{propose, apply_opt}.c`):
  `kernel_opts_propose` enumerates candidates, `kernel_apply_opt`
  mutates the DAG one KOpt at a time.  No iterative search loop in
  C; the loop is in Python (`py/examples/matmul_beam_loop.py`).
- **Reference kernels**: `external/mlx/mlx/backend/metal/kernels/*.h`
  for ground truth on what Apple-tuned MSL looks like.
  `external/reference-kernels/` is the upstream PMPP-style problem
  set.

See `docs/plans/mlx_features_to_port.md` for the architectural backlog
(the 6 MLX features and how they map to thvm UOp annotations).
