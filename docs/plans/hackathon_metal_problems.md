# Hackathon Metal problems: predicted format + mockup

The hackathon ([huggingface.co/humanitys-last-hackathon](https://huggingface.co/humanitys-last-hackathon))
launches 2026-05-15. Submissions go through GPU MODE
([github.com/gpu-mode/reference-kernels](https://github.com/gpu-mode/reference-kernels),
vendored at [external/reference-kernels/](../../external/reference-kernels/)).

The repo has no Metal track today. This doc predicts what one will look
like, by analogy to the existing tracks (`pmpp_v2`, `nvidia`, `amd`),
so we can build tooling that matches before May 15.

## What the existing tracks look like

Every problem folder is the same shape:

```
problems/<track>/<problem>/
    reference.py    # generate_input(...) -> tuple of tensors
                    # ref_kernel(data) -> output (PyTorch on the target device)
                    # check_implementation = make_match_reference(ref_kernel)
    task.py         # input_t / output_t TypeVars, TestSpec TypedDict
    task.yml        # tests + benchmarks list of shape dicts; tolerances; timeouts
    submission.py   # the candidate -- exposes custom_kernel(data) -> output
    template.py     # blank template for participants
```

The harness ([eval.py](../../external/reference-kernels/problems/pmpp_v2/eval.py))
runs `submission.custom_kernel(data)`, calls `check_implementation`, and
times under three modes: `test` (correctness only), `benchmark` (correctness
+ timing with stats), `leaderboard` (correctness + timing on every shape).
On CUDA today: `torch.cuda.Event`, `torch.cuda.synchronize`, `clear_l2_cache`.

The whole contract is: **one Python file (`submission.py`) exposing
`custom_kernel(data: input_t) -> output_t`.** The kernel writer has
total freedom inside that function to do anything Python can do --
including loading a `.metal` source string, compiling it via
`[device newLibraryWithSource:]`, dispatching, and returning the output
buffer wrapped as an MPS tensor.

## Predicted Metal track

The 3 Metal kernels will almost certainly follow the same template,
with these changes:

1. **`device='mps'` everywhere** in `reference.py`. Already supported by
   the `utils.get_device()` helper. `torch.cuda.synchronize` becomes
   `torch.mps.synchronize`. `start_event`/`end_event` either fall back
   to `time.perf_counter_ns()` with explicit `mps.synchronize`, or use
   the new MPS event API (`torch.mps.Event` exists in recent PyTorch).
2. **`clear_l2_cache`** rewritten to allocate-and-fill a large MPS
   tensor (the existing CUDA version writes to a CUDA tensor; Apple
   GPUs have unified memory but a similar trick flushes the SLC).
3. **`task.yml`** unchanged in shape. Test/benchmark shapes will probably
   stay matmul-flavored (M, N, K) or attention-flavored (B, S, H, D), with
   sizes scaled down by ~2x because Apple GPUs have less compute than
   B200 / MI300X.
4. **The submission** is still a Python file, but the practical
   kernel-writing happens via one of:
   - `mlx.fast.metal_kernel` -- compiles a string of MSL source,
     wraps it as an MLX op. Cleanest "raw MSL from Python" path on Mac.
   - `torch.utils.cpp_extension.load_inline` with `is_python_module=False`
     plus a `.mm` glue file -- heavier but works.
   - PyTorch's `torch.mps.compile_shader` (if it lands in their MPS
     extension API) -- not yet stable as of 2026-05-08.
   - A custom runtime (us): the participant builds a small Python
     extension that wraps thvm's MSL compile + dispatch, exposes
     `kernel_compile(src, name) -> handle`, `kernel_run(handle, inputs, output, dispatch_shape)`.

The hackathon's "**judged on context, not code**" framing tells us
the finals are run on **held-out kernels** the participant has never
seen. So the submission isn't just one `submission.py` -- it's the
whole tooling around Codex that lets Codex *generate* a winning
`submission.py` on a new problem in minutes.

What we should build is:
1. A **substrate** that takes raw MSL + I/O bindings and runs it (this
   is `tools/thvm-kernel/score.m`, MVP today).
2. A **Codex tool** that wraps the substrate (a CLI Codex calls in a
   compile/run/diff/edit loop).
3. An **agent harness** (prompts, pattern library, retrieval) that
   produces good-quality MSL on first try and iterates against the
   substrate's signal.

## Mockup: `matmul_metal/`

Here is what we expect the simplest Metal problem to look like. The
shape mirrors `pmpp_v2/matmul_py/` exactly.

`reference.py`:
```python
import torch
from task import input_t, output_t
from utils import make_match_reference

def generate_input(m: int, n: int, k: int, seed: int) -> input_t:
    gen = torch.Generator(device='mps')
    gen.manual_seed(seed)
    a = torch.empty(m, k, device='mps', dtype=torch.float32)
    a.uniform_(0, 1, generator=gen)
    b = torch.empty(k, n, device='mps', dtype=torch.float32)
    b.uniform_(0, 1, generator=gen)
    c = torch.empty(m, n, device='mps', dtype=torch.float32)
    return a, b, c

def ref_kernel(data: input_t) -> output_t:
    a, b, c = data
    return a @ b

check_implementation = make_match_reference(ref_kernel, rtol=1e-3, atol=1e-3)
```

`task.yml` (representative; values are guesses):
```yaml
description: |
  Implement a custom matmul on Apple GPU via Metal Shading Language.
  Input is a tuple (A, B, C) with C the pre-allocated output buffer.
  All shapes are multiples of 16. dtype=float32.

tests:
  - {"m": 64, "n": 64, "k": 64, "seed": 53124}
  - {"m": 256, "n": 256, "k": 256, "seed": 1200}
  - {"m": 32, "n": 512, "k": 32, "seed": 32523}

benchmarks:
  - {"m": 512, "n": 512, "k": 512, "seed": 123456}
  - {"m": 1024, "n": 1024, "k": 1024, "seed": 1029}
  - {"m": 2048, "n": 2048, "k": 2048, "seed": 75342}

test_timeout: 180
benchmark_timeout: 180
```

`submission.py` (the slot Codex writes into):
```python
from task import input_t, output_t
import mlx.core as mx
import mlx.nn as nn

# Or: a custom Metal compile/dispatch harness loaded here.

KERNEL_SRC = r"""
#include <metal_stdlib>
using namespace metal;

kernel void k(device float *out [[ buffer(0) ]],
              device const float *a [[ buffer(1) ]],
              device const float *b [[ buffer(2) ]],
              constant uint *shape [[ buffer(3) ]],
              uint tid [[ thread_position_in_grid ]])
{
    uint M = shape[0], N = shape[1], K = shape[2];
    if (tid >= M*N) return;
    uint m = tid / N, n = tid % N;
    float acc = 0.0f;
    for (uint k = 0; k < K; k++) acc += a[m*K + k] * b[k*N + n];
    out[m*N + n] = acc;
}
"""

# Codex iterates on KERNEL_SRC + dispatch_shape, scoring each variant.
# This is the loop our `thvm-kernel` tool drives.

def custom_kernel(data: input_t) -> output_t:
    a, b, c = data
    M, K = a.shape
    _, N = b.shape
    # ... compile KERNEL_SRC, dispatch (M*N, 1, 1) threads, return c
    ...
```

The exact submission API will depend on which substrate the hackathon
mandates (MLX, torch+mm, custom). We can hedge by making
`tools/thvm-kernel` compile-and-dispatch standalone, then writing a
thin Python wrapper for whichever substrate appears May 15.

## How thvm benefits

Every artifact we build to support the hackathon is a thvm wedge:

| Hackathon need | thvm wedge advanced ([docs/plans/ideal_pipeline.md](ideal_pipeline.md)) |
|---|---|
| Standalone MSL compile + dispatch + bench | bench harness embryo (no thvm equivalent today; closest is `make test` correctness-only) |
| PyTorch reference -> UOp DAG | Torch front-end (none today; would unlock HuggingFace-model ingestion) |
| UOp DAG textual format | Tier-1 prereq for on-disk autotune cache ([docs/plans/tilelang_scout.md:443-459](tilelang_scout.md#L443-L459)) |
| Pattern fragment library (matmul, conv, softmax, layernorm, attention) | F3 / F4 UPatRule tables in `render_uop.c` |
| Roofline scorer over arch model | Carver-shape `propose.c` |
| Correctness oracle vs PyTorch | Regression seam for every plan-relevant test |

The hackathon is a 7-day forcing function. After May 15 the tooling
folds into thvm and stays.
